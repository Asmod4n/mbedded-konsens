(ns jepsen.konsensd.db
  (:require [clojure.string :as str]
            [clojure.tools.logging :refer [info]]
            [jepsen [control :as c]
                    [db :as db]
                    [lazyfs :as lazyfs]
                    [util :refer [meh]]]
            [jepsen.control.util :as cu])
  (:import (java.net InetAddress)))

(def user "konsens")
(def binary "/usr/local/bin/konsensd")
(def home (str "/home/" user))
(def data-directory (str home "/data"))
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

(defrecord DB [lazyfs-map]
  db/DB
  (setup! [_ test node]
    (c/su (c/upload (:konsensd test) binary)
          (c/exec :chmod 755 binary))
    (lazyfs/mount! lazyfs-map)
    (c/sudo user (cu/write-file! (config-of test node) config-file))
    (start-konsensd! true))

  (teardown! [_ test node]
    (signal-konsensd! "KILL")
    (lazyfs/umount! lazyfs-map)
    (c/su (c/exec :rm :-rf data-directory config-file pid-file log-file)))

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
     (:log-file lazyfs-map) "lazyfs.log"}))

(defn konsensd-db
  []
  (DB. (lazyfs/lazyfs {:dir data-directory :user user})))
