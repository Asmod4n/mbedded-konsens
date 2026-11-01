(ns jepsen.konsensd.nemesis
  (:require [jepsen [control :as c]
                    [generator :as gen]
                    [lazyfs :as lazyfs]
                    [nemesis :as nemesis]]
            [jepsen.konsensd.db :as kdb]))

(def most-retired-nodes 2)

(def faults-that-retire #{:crash :lose-after-crash})

(def faults #{:restart :crash :lose-after-stop :lose-after-crash})

(defn retires?
  [f outcome]
  (or (faults-that-retire f) (= :killed outcome)))

(defn act!
  [db f]
  (case f
    :restart         (kdb/stop-konsensd!)
    :crash           (do (kdb/signal-konsensd! "KILL") :killed)
    :lose-after-stop (let [outcome (kdb/stop-konsensd!)]
                       (lazyfs/lose-unfsynced-writes! (:lazyfs-map db))
                       outcome)
    :lose-after-crash (do (kdb/signal-konsensd! "KILL")
                         (lazyfs/lose-unfsynced-writes! (:lazyfs-map db))
                         :killed)))

(defrecord ProcessNemesis [db retired]
  nemesis/Nemesis
  (setup! [this test] this)
  (invoke! [this test op]
    (let [node (:value op)
          outcome (c/on-nodes test [node]
                              (fn [_ _]
                                (let [outcome (act! db (:f op))]
                                  (kdb/start-konsensd! false)
                                  outcome)))
          outcome (get outcome node)]
      (when (retires? (:f op) outcome)
        (swap! retired conj node))
      (assoc op :type :info :value [node outcome])))
  (teardown! [this test])
  nemesis/Reflection
  (fs [this] faults))

(defn process-generator
  [enabled retired]
  (fn [test ctx]
    (let [retiring? (< (count @retired) most-retired-nodes)
          choices (cond->> (filter enabled faults)
                    (not retiring?) (remove faults-that-retire))
          nodes (remove @retired (:nodes test))]
      (when (and (seq choices) (seq nodes))
        {:type :info :f (rand-nth (vec choices)) :value (rand-nth (vec nodes))}))))

(defn package
  [{:keys [db faults interval]}]
  (let [enabled (filter faults jepsen.konsensd.nemesis/faults)
        retired (atom #{})]
    {:nemesis   (ProcessNemesis. db retired)
     :generator (when (seq enabled)
                  (gen/stagger interval (process-generator (set enabled) retired)))
     :perf      #{{:name "process" :fs (set enabled) :color "#E9A4A0"}}}))
