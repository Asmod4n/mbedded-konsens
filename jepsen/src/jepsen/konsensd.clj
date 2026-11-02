(ns jepsen.konsensd
  (:require [clojure.string :as str]
            [jepsen [checker :as checker]
                    [cli :as cli]
                    [generator :as gen]
                    [os :as os]
                    [tests :as tests]]
            [jepsen.nemesis.combined :as nc]
            [jepsen.konsensd [db :as kdb]
                             [nemesis :as knemesis]
                             [register :as register]
                             [rw-register :as rw-register]]))

(def workloads
  {:register    register/workload
   :rw-register rw-register/workload})

(def patterns-of-a-fault
  ["AddressSanitizer" "node_fault" "konsensd: konsens:" "Assertion" "terminate called"])

(def known-faults (into #{:partition :pause} knemesis/faults))

(def quiet-seconds-after-the-heal 10)

(defn binary-suffix
  [path]
  (let [file (last (str/split path #"/"))]
    (when (str/starts-with? file "konsensd-")
      [(subs file (count "konsensd-"))])))

(defn konsensd-test
  [opts]
  (let [workload ((workloads (:workload opts)) opts)
        db (kdb/konsensd-db)
        package-opts {:db        db
                      :interval  (:nemesis-interval opts)
                      :faults    (:faults opts)
                      :most-retired-nodes (:most-retired-nodes opts)
                      :partition {:targets [:one :majority :majorities-ring]}
                      :pause     {:targets [:one :minority]}}
        package (nc/compose-packages [(nc/partition-package package-opts)
                                      (nc/db-package package-opts)
                                      (knemesis/package package-opts)])
        generator (->> (gen/any (gen/clients (:generator workload))
                                (gen/nemesis (:generator package)))
                       (gen/stagger (/ (:rate opts)))
                       (gen/time-limit (:time-limit opts)))]
    (merge tests/noop-test
           opts
           {:name      (str/join "-" (concat ["konsensd" (name (:workload opts))]
                                         (map name (sort (:faults opts)))
                                         (binary-suffix (:konsensd opts))
                                         [(name (:engine opts))]))
            :os        os/noop
            :db        db
            :client    (:client workload)
            :nemesis   (:nemesis package)
            :generator ((:wrap-generator workload identity)
                        (gen/phases generator
                                    (gen/nemesis (:final-generator package))
                                    (gen/sleep quiet-seconds-after-the-heal)
                                    (gen/clients (:final-generator workload))))
            :checker   (checker/compose
                         (merge {:workload   (:checker workload)
                                 :perf       (checker/perf {:nemeses (:perf package)})
                                 :stats      (checker/stats)
                                 :exceptions (checker/unhandled-exceptions)
                                 :damage     (knemesis/damage-checker)}
                                (into {} (for [p patterns-of-a-fault]
                                           [(keyword (str "log " p))
                                            (checker/log-file-pattern p "konsensd.log")]))))})))

(def cli-opts
  [[nil "--workload NAME" "register or rw-register"
    :default  :register
    :parse-fn keyword
    :validate [workloads "unknown workload"]]
   [nil "--rate HZ" "Operations per second"
    :default  20
    :parse-fn parse-double]
   [nil "--per-key-limit N" "Writes per key"
    :default  100
    :parse-fn parse-long]
   [nil "--faults LIST" "Comma-separated faults: partition, pause, restart, crash, lose-after-stop, lose-after-crash, damage-log, damage-data"
    :default  #{}
    :parse-fn (fn [s] (into #{} (map keyword) (remove empty? (str/split s #","))))
    :validate [(partial every? known-faults) "unknown fault"]]
   [nil "--most-retired-nodes N" "Crashes that the nemesis may cause in a run; a crash retires a node"
    :default  2
    :parse-fn parse-long]
   [nil "--nemesis-interval SECONDS" "Seconds between faults"
    :default  10
    :parse-fn parse-double]
   [nil "--engine NAME" "kernel: konsensd uses io_uring of the kernel; emulation: the engine of slipstreamIO"
    :default  :kernel
    :parse-fn keyword
    :validate [#{:kernel :emulation} "kernel or emulation"]]
   [nil "--konsensd PATH" "The konsensd binary on the control node"
    :default "/jepsen/build/konsensd"]])

(defn -main
  [& args]
  (cli/run! (merge (cli/single-test-cmd {:test-fn  konsensd-test
                                         :opt-spec cli-opts})
                   (cli/serve-cmd))
            args))
