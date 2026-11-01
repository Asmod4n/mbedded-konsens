(ns jepsen.konsensd.rw-register
  (:require [jepsen.konsensd.client :as kc]
            [jepsen.tests.cycle.wr :as wr]))

(defn key-of
  [k]
  (str "rw-register-" k))

(defrecord RwRegister []
  kc/Operation
  (perform [_ http node op]
    (let [txn (:value op)]
      (if (every? (comp #{:r} first) txn)
        (reduce (fn [done [_ k _]]
                  (let [r (kc/read-of http node (key-of k))]
                    (if (= :ok (:type r))
                      (update done :value conj [:r k (:value r)])
                      (reduced r))))
                {:type :ok :value []}
                txn)
        (let [[[_ k v]] txn]
          (kc/write-of http node (key-of k) v))))))

(defn workload
  [opts]
  (merge (wr/test {:key-count          10
                   :min-txn-length     1
                   :max-txn-length     1
                   :max-writes-per-key (:per-key-limit opts)
                   :consistency-models [:strict-serializable]
                   :linearizable-keys? true})
         {:client (kc/client (RwRegister.))}))
