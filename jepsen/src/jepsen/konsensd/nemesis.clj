(ns jepsen.konsensd.nemesis
  (:require [clojure.string :as str]
            [jepsen [checker :as checker]
                    [control :as c]
                    [generator :as gen]
                    [lazyfs :as lazyfs]
                    [nemesis :as nemesis]]
            [jepsen.konsensd.db :as kdb]))

(def faults-that-retire #{:crash :lose-after-crash})

(def faults #{:restart :crash :lose-after-stop :lose-after-crash :damage-log :damage-data})

(def faults-on-one-node #{:damage-log :damage-data})

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
                        outcome)
    :damage-log      [(kdb/stop-konsensd!) (kdb/damage-a-log-record!)]
    :damage-data     [(kdb/stop-konsensd!) (kdb/damage-a-value!)]))

(defrecord ProcessNemesis [db retired repaired]
  nemesis/Nemesis
  (setup! [this test] this)
  (invoke! [this test op]
    (let [nodes (:value op)
          outcomes (c/on-nodes test nodes
                               (fn [_ _]
                                 (let [starts (kdb/starts-in-the-log)
                                       repairs (kdb/repairs-in-the-log)
                                       outcome (act! db (:f op))]
                                   (kdb/start-konsensd! false)
                                   [(if (and (= :damage-data (:f op)) (map? (second outcome)))
                                      (update outcome 1 assoc :read (kdb/local-value (:key (second outcome))))
                                      outcome)
                                    (cond (kdb/start-retired? starts) :retired
                                          (and (= :damage-log (:f op)) (kdb/start-repairs? repairs)) :repairs
                                          :else :votes)])))]
      (doseq [[node [_ start]] outcomes]
        (case start
          :retired (swap! retired conj node)
          :repairs (swap! repaired conj node)
          nil))
      (assoc op :type :info :value outcomes)))
  (teardown! [this test])
  nemesis/Reflection
  (fs [this] faults))

(defn counts-of-targets
  [n]
  (distinct [1 (inc (quot n 2)) n]))

(defn process-generator
  [enabled retired repaired most-retired-nodes]
  (fn [test ctx]
    (let [budget (- most-retired-nodes (count @retired))
          nodes (remove @retired (:nodes test))
          choices (for [f (filter enabled faults)
                        k (counts-of-targets (count nodes))
                        :when (or (not (faults-that-retire f)) (<= k budget))
                        :when (or (not (faults-on-one-node f)) (= k 1))
                        :when (or (not= f :damage-log) (seq (remove @repaired nodes)))]
                    [f k])]
      (when (seq choices)
        (let [[f k] (rand-nth choices)
              targets (if (= f :damage-log) (remove @repaired nodes) nodes)]
          {:type :info :f f :value (vec (take k (shuffle targets)))})))))

(defn package
  [{:keys [db faults interval most-retired-nodes]}]
  (let [enabled (filter faults jepsen.konsensd.nemesis/faults)
        retired (atom #{})
        repaired (atom #{})]
    {:nemesis   (ProcessNemesis. db retired repaired)
     :generator (when (seq enabled)
                  (gen/stagger interval (process-generator (set enabled) retired repaired most-retired-nodes)))
     :perf      #{{:name "process" :fs (set enabled) :color "#E9A4A0"}}}))

(defn damages-without-a-repair
  [history]
  (for [op history
        :when (= :damage-log (:f op))
        :when (map? (:value op))
        [node [[stop damaged] start]] (:value op)
        :when (= :stopped stop)
        :when (not (str/blank? damaged))
        :when (not= :repairs start)]
    {:node node :index (str/trim damaged) :start start}))

(defn damaged-values-served
  [history]
  (for [op history
        :when (= :damage-data (:f op))
        :when (map? (:value op))
        [node [[_ damaged] _]] (:value op)
        :when (map? damaged)
        :when (= (:to damaged) (:read damaged))]
    (assoc damaged :node node)))

(defn damage-checker
  []
  (reify checker/Checker
    (check [_ test history opts]
      (let [missed (vec (damages-without-a-repair history))
            served (vec (damaged-values-served history))]
        {:valid?  (and (empty? missed) (empty? served))
         :damages (count (filter #(and (#{:damage-log :damage-data} (:f %)) (map? (:value %))) history))
         :missed  missed
         :served  served}))))
