(ns jepsen.konsensd.register
  (:require [jepsen [checker :as checker]
                    [generator :as gen]
                    [independent :as independent]]
            [jepsen.checker.timeline :as timeline]
            [jepsen.konsensd.client :as kc]
            [knossos.model :as model]))

(defn key-of
  [k]
  (str "register-" k))

(defrecord Register []
  kc/Operation
  (perform [_ http node op]
    (let [[k v] (:value op)]
      (case (:f op)
        :read  (let [r (kc/read-of http node (key-of k))]
                 (cond-> r (= :ok (:type r)) (assoc :value (independent/tuple k (:value r)))))
        :write (kc/write-of http node (key-of k) v)))))

(defn r [_ _] {:type :invoke :f :read})

(defn w [_ _] {:type :invoke :f :write :value (rand-int 5)})

(defn workload
  [opts]
  (let [n (count (:nodes opts))]
    {:client    (kc/client (Register.))
     :generator (independent/concurrent-generator
                  (* 2 n)
                  (range)
                  (fn [_]
                    (->> (gen/reserve n r w)
                         (gen/limit (:per-key-limit opts)))))
     :checker   (independent/checker
                  (checker/compose
                    {:linearizable (checker/linearizable {:model     (model/register)
                                                          :algorithm :linear})
                     :timeline     (timeline/html)}))}))
