# Plan: one benchmark for konsensd and other replicated logs

Branch bench-distributed-logs, from main a6e8b50. Owner 2026-10-29
20:36: "installiere die mal andere distributed logs. und baue für jedes
den selben benchmark zum vergleichen." Owner 19:53: the goal is a write
path "so nah an rohem SQLite speed wie geht".

Marks: "provisional" marks a form or a name that waits on the owner.

## 1. Facts checked in this session

1. konsensd serves a key-value store over HTTP/1.1: GET, PUT and DELETE
   on a key (program/rest.cpp:35-114). Its peers talk over TCP
   (README.md:266-322).
2. The image localhost/cxx-matrix-sqlite is Ubuntu "stonking". Its
   archive offers etcd-server 3.5.30, libdqlite-dev 1.18.7,
   libraft-dev 0.22.1, nats-server 2.14.3, zookeeper 3.9.5 and
   golang-go 1.27 (apt-cache policy in the image, 2026-10-29).
3. The proxy of this session passes archive.ubuntu.com, crates.io,
   proxy.golang.org and pypi.org, and refuses github.com downloads of
   release files and most other hosts.
4. etcd and MongoDB are no model for this project (owner 2026-10-16).
   A benchmark against etcd takes no form from it; it gives a number.

## 2. The systems

Each system runs as three nodes on this machine, each node in its own
process with its own data directory on the same file system.

| System | Protocol | Source | Client of the driver |
|---|---|---|---|
| konsensd (this tree) | Omni-Paxos form | build of main | HTTP PUT |
| etcd | Raft | Ubuntu etcd-server | gRPC KV Put (go.etcd.io/etcd/client/v3) |
| dqlite | Raft over SQLite | Ubuntu libdqlite-dev, go-dqlite app | SQL INSERT through go-dqlite |
| rqlite | Raft over SQLite | proxy.golang.org, `go install` | HTTP /db/execute |
| ZooKeeper | Zab | Ubuntu zookeeper | create of a znode (go-zookeeper) |
| NATS JetStream | Raft | Ubuntu nats-server | publish to a stream with 3 replicas |
| Omni-Paxos | Omni-Paxos | crates.io omnipaxos, omnipaxos_storage | in one process, see 2.1 |

1. Omni-Paxos has no server program. Its row runs three nodes in one
   process with its persistent storage and passes messages through
   channels, as konsens_bench does for konsens. That row is compared
   with the in-process case of konsens_bench, not with the rows over
   the network.
2. Raw SQLite is the reference row: one process, the same table form
   (key, value), one INSERT and one commit for each write, WAL and
   synchronous=FULL, as konsens.db uses.

## 3. Durability of each system

A write counts only when the system says that it is on the disk of a
majority. Each system gets the setting for one fsync per commit before
it answers:

- etcd: the default (fsync of the WAL before the answer).
- dqlite and rqlite: the default of their Raft log.
- ZooKeeper: `forceSync=yes` (the default).
- NATS JetStream: `sync_interval: always`; its default syncs every two
  minutes.
- konsensd: the default (SQLite synchronous=FULL).

The result file records the setting of each system and where it was
read. A system that cannot be set to sync each commit gets a row with
that note.

## 4. The workload

1. One driver program in Go (Ubuntu golang-go), in bench/compare/ of
   this tree, with one adapter for each client of section 2. The
   driver and the nodes run in one podman container built from
   localhost/cxx-matrix-sqlite with the Ubuntu packages of section 1.2.
2. Each write puts a new key with a value of 64 B or 1 KiB to the
   leader (the driver finds the leader first, so that no forward
   counts).
3. Two loads: concurrency 1 (latency of one write), and concurrency 64
   (throughput with group commit).
4. 31 samples of a fixed count of writes each, after a warm-up. The
   noise floor is one system run twice. The systems run alternately,
   in rounds.
5. Units: writes per second and the median and p99 latency in µs. Each
   number names the system, the version, the value size, the
   concurrency and the build.
6. The bench leaves one processor free, runs as the user of the bench,
   not as root, not pinned, with the priority recorded. The data
   directories lie on the same file system; the result file names it.

## 5. What this plan does not do

- No change of konsens or konsensd.
- No form taken from any of the systems.
- No run across machines; all nodes share one disk, so the numbers show
  the cost of each design on one disk, not over a network with delay.

## 6. Order

1. This plan is reviewed.
2. The container image is built and each system is started once by hand
   as three nodes.
3. The driver is written, committed, and run once per system with a
   short count, to check each adapter.
4. The full run. The result file goes to bench/results/.

## 7. Data of this plan

- The container image localhost/bench-logs (provisional name): made
  again from its Containerfile.
- The data directories of the nodes on /mnt/zrun: deleted after each
  run.
- The result file under bench/results: recorded in the state file.
