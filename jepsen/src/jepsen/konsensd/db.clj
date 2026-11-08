(ns jepsen.konsensd.db
  (:require [clojure.string :as str]
            [clojure.tools.logging :refer [info]]
            [jepsen [control :as c]
                    [db :as db]
                    [lazyfs :as lazyfs]
                    [util :as util :refer [meh]]]
            [jepsen.control.util :as cu])
  (:import (java.net InetAddress)))

(def user "konsens")
(def binary "/usr/local/bin/konsensd")
(def home (str "/home/" user))
(def data-directory (str home "/data"))
(def runtime-directory (str home "/run"))
(def config-file (str home "/konsensd.toml"))
(def log-file (str home "/konsensd.log"))
(def pid-file (str home "/konsensd.pid"))
(def peer-port 7151)
(def client-port 7161)

(defn pid-of
  [test node]
  (inc (.indexOf ^java.util.List (vec (:nodes test)) node)))

(defn address-of
  [node]
  (.getHostAddress (InetAddress/getByName node)))

(defn config-of
  [test node]
  (str "[cluster]\nname = \"jepsen\"\nconfiguration_id = 1\n\n"
       "[node]\n"
       "pid = " (pid-of test node) "\n"
       "data_directory = \"" data-directory "\"\n"
       "runtime_directory = \"" runtime-directory "\"\n"
       "peer_address = \"0.0.0.0:" peer-port "\"\n"
       "client_address = \"0.0.0.0:" client-port "\"\n"
       (str/join (for [peer (:nodes test)
                       :when (not= peer node)]
                   (str "\n[[peers]]\npid = " (pid-of test peer)
                        "\naddress = \"" (address-of peer) ":" peer-port "\"\n")))))

(defn start-konsensd!
  [first-start?]
  (c/sudo user
    (cu/start-daemon! {:logfile log-file
                       :pidfile pid-file
                       :chdir   home}
                      binary
                      :--config config-file
                      (when first-start? :--first-start))))

(defn signal-konsensd!
  [signal]
  (c/su (meh (c/exec :pkill (str "-" signal) :-x :konsensd))))

(defn konsensd-runs?
  []
  (try (c/exec :pgrep :-x :konsensd) true
       (catch Exception _ false)))

(def seconds-for-a-clean-stop 60)

(defn await-the-end-of-konsensd
  [seconds]
  (util/await-fn (fn [] (when (konsensd-runs?) (throw (ex-info "konsensd runs" {}))))
                 {:timeout        (* 1000 seconds)
                  :retry-interval 100
                  :log-message    "Waiting for konsensd to end"}))

(defn kill-konsensd!
  []
  (signal-konsensd! "KILL")
  (await-the-end-of-konsensd seconds-for-a-clean-stop)
  :killed)

(def threads-file (str home "/konsensd-threads.log"))

(defn record-the-threads-of-konsensd!
  []
  (c/su (c/exec :sh :-c (str "{ date -u; for t in /proc/$(pgrep -x konsensd)/task/*; do "
                             "echo \"== $t $(cat $t/comm) $(cat $t/wchan)\"; grep State $t/status; "
                             "echo \"syscall $(cat $t/syscall)\"; done; "
                             "gdb -p $(pgrep -x konsensd) -batch -ex 'thread apply all bt'; "
                             "ss -tnpi; } >> " threads-file " 2>&1"))))

(defn stop-konsensd!
  []
  (signal-konsensd! "CONT")
  (signal-konsensd! "TERM")
  (try (await-the-end-of-konsensd seconds-for-a-clean-stop)
       :stopped
       (catch clojure.lang.ExceptionInfo e
         (if (= :timeout (:type (ex-data e)))
           (do (record-the-threads-of-konsensd!)
               (kill-konsensd!))
           (throw e)))))

(defn starts-in-the-log
  []
  (parse-long (str/trim (c/exec :sh :-c (str "grep -c 'konsensd: node [0-9]* started' " log-file " || true")))))

(def database-file (str data-directory "/log/konsens.db"))

(defn repairs-in-the-log
  []
  (parse-long (str/trim (c/exec :sh :-c (str "grep -c 'konsensd: node [0-9]* repairs its store' " log-file " || true")))))

(defn start-repairs?
  [repairs-before]
  (try (util/await-fn (fn []
                        (when (<= (repairs-in-the-log) repairs-before)
                          (throw (ex-info "konsensd does not repair" {})))
                        true)
                      {:timeout        10000
                       :retry-interval 200
                       :log-message    "Waiting for konsensd to repair"})
       (catch clojure.lang.ExceptionInfo e
         (if (= :timeout (:type (ex-data e)))
           false
           (throw e)))))

(defn sqlite!
  [& sql]
  (c/sudo user (c/exec :sqlite3 database-file (str/join " " sql))))

(defn damage-a-log-record!
  []
  (sqlite! "UPDATE log SET record = unhex(CASE substr(hex(record),1,2) WHEN 'FF' THEN 'FE' ELSE 'FF' END"
           "|| substr(hex(record),3))"
           "WHERE idx = (SELECT idx FROM log ORDER BY random() LIMIT 1)"
           "RETURNING idx"))

(defn damage-a-value!
  []
  (let [row (str/trim (sqlite! "SELECT hex(key) || ' ' || hex(value) FROM data ORDER BY random() LIMIT 1"))]
    (when-not (str/blank? row)
      (let [[key value] (str/split row #" ")
            at (max 0 (- (count value) 10))
            flipped (Integer/parseInt (subs value at (+ at 2)) 16)
            changed (str (subs value 0 at) (format "%02X" (bit-xor flipped 1)) (subs value (+ at 2)))]
        (sqlite! (str "UPDATE data SET value = X'" changed "' WHERE key = X'" key "'"))
        {:key key :from value :to changed}))))

(defn text-of-hex
  [hex]
  (String. (byte-array (map #(unchecked-byte (Integer/parseInt (apply str %) 16)) (partition 2 hex)))
           "UTF-8"))

(defn hex-of-text
  [text]
  (apply str (map #(format "%02X" %) (.getBytes ^String text "UTF-8"))))

(defn local-value
  [key-hex]
  (try (util/await-fn (fn []
                        (let [r (c/exec :curl :-s :-w "\n%{http_code}"
                                        (str "http://127.0.0.1:" client-port "/kv/" (text-of-hex key-hex)))
                              [body status] (str/split r #"\n" 2)]
                          (case status
                            "200" (hex-of-text body)
                            "404" :absent
                            (throw (ex-info "no answer" {:status status})))))
                      {:timeout        10000
                       :retry-interval 200
                       :log-message    "Waiting for a local read"})
       (catch clojure.lang.ExceptionInfo e
         (if (= :timeout (:type (ex-data e)))
           :no-answer
           (throw e)))))

(defn start-retired?
  [starts-before]
  (util/await-fn (fn []
                   (when (<= (starts-in-the-log) starts-before)
                     (throw (ex-info "konsensd has not started" {})))
                   (boolean (re-find #"started retired"
                                     (c/exec :sh :-c (str "grep 'konsensd: node [0-9]* started' " log-file " | tail -1")))))
                 {:timeout        30000
                  :retry-interval 200
                  :log-message    "Waiting for konsensd to start"}))

(defn io-uring-descriptors
  []
  (parse-long (str/trim (c/su (c/exec :sh :-c (str "ls -l /proc/$(cat " pid-file ")/fd | grep -c io_uring || true"))))))

(defn check-the-engine!
  [engine]
  (util/await-fn (fn []
                   (when-not (konsensd-runs?) (throw (ex-info "konsensd does not run" {})))
                   (let [rings (io-uring-descriptors)]
                     (when (not= (= :kernel engine) (pos? rings))
                       (throw (ex-info "konsensd does not use the engine of the test"
                                       {:engine engine :io-uring-descriptors rings})))))
                 {:timeout 10000 :retry-interval 200 :log-message "Waiting for the engine of konsensd"}))

(defrecord DB [lazyfs-map]
  db/DB
  (setup! [_ test node]
    (c/su (c/upload (:konsensd test) binary)
          (c/exec :chmod 755 binary))
    (lazyfs/mount! lazyfs-map)
    (c/sudo user (cu/write-file! (config-of test node) config-file))
    (start-konsensd! true)
    (check-the-engine! (:engine test)))

  (teardown! [_ test node]
    (signal-konsensd! "KILL")
    (lazyfs/umount! lazyfs-map)
    (c/su (c/exec :rm :-rf data-directory runtime-directory config-file pid-file log-file threads-file)))

  db/Kill
  (kill! [_ test node]
    (signal-konsensd! "KILL"))
  (start! [_ test node]
    (start-konsensd! false))

  db/Pause
  (pause! [_ test node]
    (signal-konsensd! "STOP"))
  (resume! [_ test node]
    (signal-konsensd! "CONT"))

  db/LogFiles
  (log-files [_ test node]
    {log-file "konsensd.log"
     threads-file "konsensd-threads.log"
     (:log-file lazyfs-map) "lazyfs.log"}))

(defn konsensd-db
  []
  (DB. (lazyfs/lazyfs {:dir data-directory :user user})))
