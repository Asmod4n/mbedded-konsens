(ns jepsen.konsensd.client
  (:require [jepsen.client :as client]
            [jepsen.konsensd.db :as kdb])
  (:import (java.net ConnectException URI)
           (java.net.http HttpClient HttpClient$Version HttpRequest HttpRequest$BodyPublishers
                          HttpResponse$BodyHandlers HttpTimeoutException)
           (java.time Duration)))

(def request-timeout (Duration/ofSeconds 5))

(def statuses-of-a-refusal-before-the-node #{400 404 405 411 413 431 501 505})

(def problem-types-without-a-proposal
  #{"https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#retired"
    "https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#building"})

(defn problem-type-of
  [body]
  (second (re-find #"\"type\":\"([^\"]*)\"" (or body ""))))

(defn http-client
  []
  (-> (HttpClient/newBuilder)
      (.version HttpClient$Version/HTTP_1_1)
      (.connectTimeout request-timeout)
      (.build)))

(defn uri-of
  [node k]
  (URI. (str "http://" node ":" kdb/client-port "/kv/" k)))

(defn send-request
  [^HttpClient http ^HttpRequest request]
  (let [response (.send http request (HttpResponse$BodyHandlers/ofString))]
    {:status (.statusCode response)
     :body   (.body response)}))

(defn get-request
  [node k]
  (-> (HttpRequest/newBuilder (uri-of node k)) (.timeout request-timeout) (.GET) (.build)))

(defn put-request
  [node k v]
  (-> (HttpRequest/newBuilder (uri-of node k))
      (.timeout request-timeout)
      (.PUT (HttpRequest$BodyPublishers/ofString (str v)))
      (.build)))

(defn read-of
  [http node k]
  (let [{:keys [status body]} (send-request http (get-request node k))]
    (case (long status)
      200 {:type :ok :value (parse-long body)}
      404 {:type :ok :value nil}
      {:type :fail :error [:status status body]})))

(defn write-of
  [http node k v]
  (let [{:keys [status body]} (send-request http (put-request node k v))]
    (cond (= 204 status) {:type :ok}
          (= 409 status) {:type :fail :error [:rejected body]}
          (statuses-of-a-refusal-before-the-node status) {:type :fail :error [:status status body]}
          (problem-types-without-a-proposal (problem-type-of body)) {:type :fail :error [:status status body]}
          :else {:type :info :error [:status status body]})))

(defn outcome-of-an-exception
  [f e]
  (cond (instance? ConnectException e) {:type :fail :error [:connect (.getMessage ^Exception e)]}
        (= :read f) {:type :fail :error [:exception (str e)]}
        (instance? HttpTimeoutException e) {:type :info :error [:timeout]}
        :else {:type :info :error [:exception (str e)]}))

(defprotocol Operation
  (perform [this http node op]))

(defrecord Client [operation http node]
  client/Client
  (open! [this test node]
    (assoc this :http (atom (http-client)) :node node))
  (setup! [this test])
  (invoke! [this test op]
    (let [completion (try (perform operation @http node op)
                          (catch Exception e
                            (outcome-of-an-exception (:f op) e)))]
      (when (= :info (:type completion))
        (.close ^HttpClient @http)
        (reset! http (http-client)))
      (merge op completion)))
  (teardown! [this test])
  (close! [this test]
    (when http (.close ^HttpClient @http))))

(defn client
  [operation]
  (Client. operation nil nil))
