(defproject jepsen.konsensd "0.1.0"
  :description "A Jepsen test of konsensd"
  :license {:name "Apache-2.0"}
  :dependencies [[org.clojure/clojure "1.12.0"]
                 [jepsen "0.3.15"]]
  :jvm-opts ["-Djava.awt.headless=true"]
  :main jepsen.konsensd)
