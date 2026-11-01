(ns jepsen.konsensd
  (:require [jepsen.cli :as cli]
            [jepsen.tests :as tests]))

(defn konsensd-test
  [opts]
  (merge tests/noop-test opts {:name "konsensd-noop"}))

(defn -main
  [& args]
  (cli/run! (cli/single-test-cmd {:test-fn konsensd-test}) args))
