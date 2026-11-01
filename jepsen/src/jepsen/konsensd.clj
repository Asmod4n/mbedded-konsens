(ns jepsen.konsensd
  (:require [jepsen [checker :as checker]
                    [cli :as cli]
                    [generator :as gen]
                    [os :as os]
                    [tests :as tests]]
            [jepsen.konsensd [db :as kdb]
                             [register :as register]
                             [rw-register :as rw-register]]))

(def workloads
  {:register    register/workload
   :rw-register rw-register/workload})

(def patterns-of-a-fault
  ["AddressSanitizer" "node_fault" "konsensd: konsens:"])

(defn konsensd-test
  [opts]
  (let [workload ((workloads (:workload opts)) opts)
        generator (->> (gen/clients (:generator workload))
                       (gen/stagger (/ (:rate opts)))
                       (gen/time-limit (:time-limit opts)))]
    (merge tests/noop-test
           opts
           {:name      (str "konsensd-" (name (:workload opts)))
            :os        os/noop
            :db        (kdb/konsensd-db)
            :client    (:client workload)
            :generator ((:wrap-generator workload identity)
                        (if-let [final (:final-generator workload)]
                          (gen/phases generator (gen/clients final))
                          generator))
            :checker   (checker/compose
                         (merge {:workload   (:checker workload)
                                 :perf       (checker/perf)
                                 :stats      (checker/stats)
                                 :exceptions (checker/unhandled-exceptions)}
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
   [nil "--konsensd PATH" "The konsensd binary on the control node"
    :default "/jepsen/build/konsensd"]])

(defn -main
  [& args]
  (cli/run! (merge (cli/single-test-cmd {:test-fn  konsensd-test
                                         :opt-spec cli-opts})
                   (cli/serve-cmd))
            args))
