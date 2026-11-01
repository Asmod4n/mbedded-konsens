(ns jepsen.konsensd.nemesis
  (:require [jepsen [control :as c]
                    [generator :as gen]
                    [lazyfs :as lazyfs]
                    [nemesis :as nemesis]]
            [jepsen.konsensd.db :as kdb]))

(def faults-that-retire #{:crash :lose-after-crash})

(def faults #{:restart :crash :lose-after-stop :lose-after-crash})

(defn act!
  [db f]
  (case f
    :restart         (kdb/stop-konsensd!)
    :crash           (kdb/kill-konsensd!)
    :lose-after-stop (let [outcome (kdb/stop-konsensd!)]
                       (lazyfs/lose-unfsynced-writes! (:lazyfs-map db))
                       outcome)
    :lose-after-crash (let [outcome (kdb/kill-konsensd!)]
                        (lazyfs/lose-unfsynced-writes! (:lazyfs-map db))
                        outcome)))

(defrecord ProcessNemesis [db retired]
  nemesis/Nemesis
  (setup! [this test] this)
  (invoke! [this test op]
    (let [nodes (:value op)
          outcomes (c/on-nodes test nodes
                               (fn [_ _]
                                 (let [outcome (act! db (:f op))
                                       starts (kdb/starts-in-the-log)]
                                   (kdb/start-konsensd! false)
                                   [outcome (if (kdb/start-retired? starts) :retired :votes)])))]
      (doseq [[node [_ start]] outcomes
              :when (= :retired start)]
        (swap! retired conj node))
      (assoc op :type :info :value outcomes)))
  (teardown! [this test])
  nemesis/Reflection
  (fs [this] faults))

(defn counts-of-targets
  [n]
  (distinct [1 (inc (quot n 2)) n]))

(defn process-generator
  [enabled retired most-retired-nodes]
  (fn [test ctx]
    (let [budget (- most-retired-nodes (count @retired))
          nodes (remove @retired (:nodes test))
          choices (for [f (filter enabled faults)
                        k (counts-of-targets (count nodes))
                        :when (or (not (faults-that-retire f)) (<= k budget))]
                    [f k])]
      (when (seq choices)
        (let [[f k] (rand-nth choices)]
          {:type :info :f f :value (vec (take k (shuffle nodes)))})))))

(defn package
  [{:keys [db faults interval most-retired-nodes]}]
  (let [enabled (filter faults jepsen.konsensd.nemesis/faults)
        retired (atom #{})]
    {:nemesis   (ProcessNemesis. db retired)
     :generator (when (seq enabled)
                  (gen/stagger interval (process-generator (set enabled) retired most-retired-nodes)))
     :perf      #{{:name "process" :fs (set enabled) :color "#E9A4A0"}}}))
