# Review of plan-bench-distributed-logs.md (commit 109fa95)

Reviewer: a second agent of the session, read only, on 2026-10-29.
Folded into the plan at the next commit.

## Facts about konsensd (read at 109fa95)

- A PUT is answered after the durable commit of a majority
  (program/serve.cpp:437, :405). SQLite runs WAL, synchronous=FULL,
  PASSIVE checkpoint at 1000 frames (src/persistent_storage.cpp:39,
  :1026-1051).
- Group commit works across connections only. One connection carries
  one call at a time (serve.cpp:302).
- The first PUT of a connection registers a session; a close expires it
  (serve.cpp:277).
- A PUT to a follower is forwarded. No endpoint names the leader; the
  only sign is a stderr line (serve.cpp:432).
- Three nodes run on one host with their own TOML. A start without a
  clean-stop mark retires the node.

## Blocking

- B1. The Go HTTP client keeps 2 idle connections per host by default.
- B2. "Concurrency 64" differs per system: 64 connections for
  konsensd, multiplexed connections for etcd, NATS and ZooKeeper.
- B3. Raw SQLite needs two rows: one commit per write and N writes per
  commit, with the checkpoint scheme of konsens.
- B4. Raw SQLite must link the libsqlite3 that konsens links. Each row
  records its SQLite version.
- B5. The way to find the leader is not named per system. Leader
  changes during a sample must be recorded.
- B6. A driver in the tree built from proxy.golang.org breaks "What
  builds the tree is the distribution's package".
- B7. The Omni-Paxos storage may not sync each write (from memory).
- B8. Fresh data directories per round are not specified; konsensd
  needs a clean stop to start again.

## Answers (other systems from memory)

- konsens_bench cases are the steps between raw SQLite and konsensd:
  commit_lmdb, write_lmdb, write_batch_lmdb, write_lmdb_threads. No
  raw-SQLite case is in the tree.
- etcd fsyncs its WAL; rqlite fsyncs its BoltDB log and runs SQLite
  with synchronous=OFF; dqlite fsyncs its Raft log; go-dqlite has
  cmd/dqlite-demo with HTTP GET and PUT; ZooKeeper forceSync=yes;
  NATS needs sync_interval always.
- Values are random bytes, because /mnt/zrun compresses.
- Record the fsyncs per write and the CPU time per write of each
  process.
- A/A: konsensd built twice. Every build: g++ 16 and clang 22.
- One processor free through podman --cpus=3, not pinning.

## Not checked by the reviewer

Every fact about the other systems; the Ubuntu golang-*-dev packages;
the sync of omnipaxos_storage; strace in the container; the file
system behind earlier results.
