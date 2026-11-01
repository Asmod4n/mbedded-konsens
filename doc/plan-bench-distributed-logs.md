# Plan: one benchmark for konsensd and other replicated logs

Branch bench-distributed-logs, from main a6e8b50. Owner 2026-10-29
20:36: "installiere die mal andere distributed logs. und baue für jedes
den selben benchmark zum vergleichen." Owner 19:53: the goal is a write
path "so nah an rohem SQLite speed wie geht".

Marks: "provisional" marks a form or a name that waits on the owner.
Review: doc/review-plan-bench-distributed-logs.md; its findings are
folded in.

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
   the network. Its storage is checked first: a row whose storage does
   not sync each write is marked as not durable.
2. Raw SQLite is the reference, in two rows: one INSERT and one commit
   for each write (the floor of the latency), and 64 INSERTs in one
   commit (the ceiling of the throughput). One process, the table form
   (key, value), WAL, synchronous=FULL, the checkpoint form of
   konsens.db (PASSIVE at 1000 frames). It is a C++ program that links
   the libsqlite3 that konsens links.
3. The cases commit_lmdb, write_lmdb, write_batch_lmdb and
   write_lmdb_threads of konsens_bench are the steps between raw SQLite
   and konsensd over HTTP. They show where the time goes.

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

1. One driver program in Go (Ubuntu golang-go). It stays outside the
   tree, because its client modules come from proxy.golang.org and
   not from the distribution; each result file holds the SHA-256 of
   its source and of go.sum. The driver and the nodes run in one podman
   container built from localhost/cxx-matrix-sqlite with the Ubuntu
   packages of section 1.2, limited to 3 processors (--cpus=3).
2. Each write puts a new key with a value of random bytes, 64 B or
   1 KiB, to the leader. No key is written twice. The leader is found
   before the samples: konsensd from its stderr line, etcd from
   endpoint status, rqlite from /status, dqlite from client.Leader,
   ZooKeeper from srvr, NATS from the stream info. A leader change
   during a sample marks the sample.
3. Two loads, both closed loops: 1 write in flight, and 64 writes in
   flight. Each system takes the client form of its own documentation:
   konsensd, rqlite and dqlite-demo take 64 HTTP connections with
   keep-alive (MaxIdleConnsPerHost and MaxConnsPerHost 64); etcd, NATS
   and ZooKeeper take one client. The row records the number of
   connections. Each connection is warmed up before the first sample,
   so that a session of konsensd is registered once.
4. Each round starts each system from fresh data directories and stops
   it after its samples (konsensd with SIGTERM, a clean stop). The
   systems run alternately, round by round. 31 samples of a fixed count
   of writes each, after a warm-up that covers the JIT of the JVM and
   one checkpoint of konsens. The noise floor is konsensd built twice,
   as two arms. konsensd runs in two builds, g++ 16 and clang 22
   Release.
5. Units: writes per second and the median and p99 latency in µs. Each
   number names the system, the version, the value size, the
   concurrency and the build.
6. The bench leaves one processor free, runs as the user of the bench,
   not as root, not pinned, with the priority recorded. The data
   directories lie on /mnt/zrun (btrfs with zstd on a loop file); the
   result file names the file system, its mount options and the loop
   mode.
7. Each row also records the fsyncs per write (strace -c -f, where the
   container allows it), the CPU time per write of each process, the
   SQLite version that the row runs, and the version and toolchain of
   each binary.
8. The writes are not the same work, and the result says so: konsensd
   appends a log entry and applies it to the state in SQLite; etcd adds
   an MVCC revision; ZooKeeper updates a tree in memory; NATS appends a
   message; dqlite and rqlite run an SQL INSERT.

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
- The driver, the raw-SQLite program, their binaries, the Go and cargo
  caches in the scratchpad: deleted after the run; the source digests
  stand in the result file.
- The data directories of the nodes on /mnt/zrun: deleted after each
  run.
- The result file under bench/results: recorded in the state file.
