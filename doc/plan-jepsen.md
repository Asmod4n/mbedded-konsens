# Plan: a Jepsen test of konsensd

Source: the owner, 2026-11-01 15:58 (card: "Echtes Jepsen in podman"),
after the comparison with Jepsen ParticleDB 1.1.4. Base: main 63a8ff3.
This plan changes no code of the library and of konsensd.

## 1. Goal

Jepsen runs against five konsensd processes, injects faults, records
the history of every client call and checks it. The checkers are those
that Jepsen uses for a key-value store: Knossos (linearizability of a
register) and Elle (the rw-register model).

## 2. What konsensd offers a test

- REST over HTTP/1.1: `GET`, `PUT` and `DELETE` on `/kv/<key>`
  (`program/rest.hpp:15`, `program/rest.cpp:39-43`). One key per call;
  there is no transaction over several keys.
- One configuration TOML per node (`program/test/three_nodes.sh:24-35`).
- SIGTERM gives a clean stop (`program/serve.cpp:775-781`); SIGKILL is a
  crash.
- No call reconfigures the cluster at run time (searched `program/` for
  "reconfigure": none).

So the workload is a register per key, and Elle reads it as rw-register
with one operation per transaction. Elle list-append needs an append
operation, which konsensd does not have.

## 3. The test

A Clojure project in `jepsen/` of this repository, beside `model/`. It
is not part of the CMake build. Its form follows the tests of
jepsen-io (for example `jepsen-io/etcd`): `db`, `client`, `nemesis`,
`workload` and `checker` namespaces, and one `core.clj` with the CLI
(`--workload`, `--faults`, `--rate`, `--time-limit`, `--concurrency`,
`--nemesis-interval`).

- **db.** Implements `db/DB`, `db/Kill`, `db/Pause` and `db/LogFiles`.
  It copies the konsensd binary into each node, writes the TOML
  (`program/test/three_nodes.sh:29-41`), passes `--first-start` only at
  the setup (`program/main.cpp:47`), starts as a user that is not root,
  stops with SIGTERM, kills with SIGKILL. The teardown wipes the data
  directory and the `.real` directory of LazyFS.
- **client.** One keep-alive connection per Jepsen process, no shared
  pool. Each connection of konsensd is one session, and the next request
  is read only after the current call is answered
  (`program/serve.cpp:414-417`, README.md:405-406). So the client closes
  the connection after every `:info`, and the process opens a new one.
  Clients spread over all five nodes.
- **reply mapping.** konsensd answers with problem details of RFC 9457
  and `"type": "about:blank"` (`program/rest.cpp:209`); only the status
  and the detail differ.
  - A write: 204 is `:ok`. `:fail` only for 409 `rejected` (decided as a
    no-op, `client.hpp:41`, README.md:277), for a refusal of the parser
    before the node (400, 404 of the path, 405, 411, 413, 431, 501, 505)
    and for a refused connect. Everything else is `:info`: 500, 503
    with or without Retry-After (a `retry` resends the same request
    number, and the first proposal can still be decided,
    `program/serve.cpp:461-464`), a timeout, a reset.
  - A read: 200 is the value, 404 "the key is not there" is nil,
    everything else is `:fail`.
- **workloads.** Two, chosen by `--workload`, as jepsen-io/etcd does:
  - `register`: `jepsen.independent` with a limit of writes per key and
    key rotation, Knossos `linearizable` over a register per key.
  - `rw-register`: Elle `rw-register`, `:strict-serializable`,
    `:linearizable-keys? true`, a unique value per write. With one
    operation per transaction it infers little; it stays because it
    reads the whole history at once.
- **checkers.** Beside the workload checker: `stats`, `perf`,
  `timeline`, `unhandled-exceptions`, and `log-file-pattern` for
  "AddressSanitizer", "node_fault" and "konsensd: konsens:".
- **final phase.** Heal every fault, wait a quiet time, and read each
  key on each node that votes.

## 4. Faults

Each unclean end retires the node for good: a kill, a lost write of
LazyFS, a damage that writes a fault proof (`node.hpp:1112-1119`,
README.md:98-103), and a SIGTERM that ends as SIGKILL. konsensd cannot
replace a retired node at run time. So the nemesis of this test counts
the retired nodes, and stops at f = 2 for the whole run. The combined
kill nemesis of Jepsen picks a majority too
(`jepsen/nemesis/combined.clj`), so it is not used as it is. After two
retired nodes, a partition or a pause leaves no majority, and the test
accepts that no call ends then.

| Fault | How | What the design says |
|---|---|---|
| partition | nftables on each node (a custom `jepsen.net`): halves, one node alone, a ring | the minority answers no call; no lost or stale value |
| pause | SIGSTOP and SIGCONT | as a slow node |
| clean restart | SIGTERM, start | the node votes again (`node.hpp:79-81`, three_nodes.sh:199-203) |
| crash | SIGKILL, start | the node retires (`node.hpp:67-90`, three_nodes.sh:264-278) |
| lost unsynced writes after a clean stop | SIGTERM, LazyFS "clear cache", start | the node votes again with no loss: the clean stop synced |
| lost unsynced writes after a crash | SIGKILL, LazyFS "clear cache", start | the node retires |
| file damage | the file-corruption package of Jepsen (bitflip, truncate) on `konsens.db` and its WAL of one stopped node | a damaged record is repaired from the peers, and the node votes again (README.md:91-97); a damage of the WAL is cut by SQLite at the first bad frame, the mark disagrees with the meta, and the node retires |

The clock row of version 1 is gone: `date -s` in a container sets the
clock of the host, and konsensd reads only `steady_clock`
(`program/serve.cpp:46`).

## 5. Environment

- podman, as every container here. One control container and five node
  containers on one podman network.
- Node image: Tumbleweed with sshd as PID 1 (`ssh-keygen -A`, `sshd -D`),
  root login by key, sudo, bash, tar, curl, procps, nftables, fuse3,
  and LazyFS 0.3.0 built into the image (g++, cmake, fuse3-devel).
  `NET_ADMIN` for nftables, `/dev/fuse` with `SYS_ADMIN` for LazyFS.
  The test uses `os/noop`, not `jepsen.os.debian`, and calls only
  `jepsen.lazyfs/mount!`, because its `install!` uses Debian packages.
  LazyFS mounts for the user of konsensd, because konsensd refuses root
  (`src/persistent_storage.cpp:1906`).
- The default seccomp profile of podman has no io_uring. The node
  containers get a profile that allows io_uring, so konsensd runs on the
  kernel ring as in production; the run records which engine ran.
- Control image: OpenJDK 21 from Tumbleweed, gnuplot, graphviz, and
  Leiningen. Leiningen is no package of Tumbleweed; it comes from its
  origin, as Jepsen and LazyFS do. None of them builds the tree.
- Jepsen 0.3.15 from Clojars (reachable from here, checked).
- Checked on this machine, 2026-11-01: nftables rules work in a podman
  container with `NET_ADMIN`; `/dev/fuse` exists (`CONFIG_FUSE_FS=y`);
  netem does not exist (`CONFIG_NET_SCH_NETEM` is not set), so no fault
  slows or reorders packets.

## 6. Steps

1. Hypothesis in one file: the control container reaches five node
   containers over SSH, Jepsen 0.3.15 runs its `noop` test, and SQLite
   in WAL mode with fdatasync works on a LazyFS mount. Nothing else is
   built before that works.
2. db and client with no fault; Knossos and Elle find no anomaly.
3. One fault at a time, in the order of the table. Each fault first
   runs against a konsensd with a mutation that must be found, on a
   branch that never merges. The mutation of a fault removes what
   protects against it, all of it:
   - lost writes: no sync of the WAL (`sync_the_wal` and the fsync
     completion do nothing) and no retirement (`kind_of_the_start`
     gives `clean` after an unclean stop). This is the form of the
     `--unsafe-no-fsync` flag of etcd, which jepsen-io/etcd uses.
   - crash: no retirement.
   - file damage: no CRC check.
   - partition: a read without the read index.
   A fault whose mutation is not found is not trusted.
4. The results go to `jepsen/results/<time stamp>/`, one directory per
   run, with the history, the checker output and the node logs.

## 6a. Result of step 1 (2026-11-01)

- `jepsen/run.sh` builds both images, starts five node containers on
  the podman network `jepsen`, and runs the control container. The
  control container reaches n1 to n5 over SSH, and the noop test of
  Jepsen 0.3.15 ends with `{:valid? true}`
  (`jepsen/results/konsensd-noop/2026-11-01T18:02:38.763763307Z/`).
- `jepsen/fetch.sh` fetches LazyFS 0.3.0, toml11 3.7.1 and spdlog
  1.10.0 at fixed commits, and Leiningen 2.11.2 with its SHA-256. The
  node image builds LazyFS from these sources, because the build of
  LazyFS fetches toml11 and spdlog itself.
- SQLite in WAL mode on a LazyFS mount, as the user konsens: two rows
  written with `synchronous=FULL` and one with `synchronous=OFF`, then
  `lazyfs::clear-cache`. After the fault the database holds the two
  synced rows, and `integrity_check` gives `ok`. So the fdatasync of
  SQLite reaches LazyFS, and LazyFS drops what was not synced. The
  check ran in one file outside the tree.
- Found on the way: podman gives `/dev/fuse` the mode 0600, so the node
  container makes it 0666 at its start; Jepsen calls `git` on the
  control node; the build of LazyFS needs
  `CMAKE_POLICY_VERSION_MINIMUM=3.5` with CMake 4.
- Here the containers reach no host on the internet. `run.sh` mounts
  `jepsen/build/m2`, so the dependencies that Leiningen fetched on the
  host are there. On a machine with a direct connection, Leiningen in
  the control container fetches them itself.

## 6b. Result of step 2 (2026-11-01)

- The node image is Ubuntu 26.10, as `containers/konsensd` is, and not
  Tumbleweed: Tumbleweed has no package of ada-url and of toml++, so
  konsensd does not build or run there, and Jepsen starts a daemon with
  `start-stop-daemon`, which Tumbleweed does not have. The control image
  stays Tumbleweed.
- `jepsen/src/jepsen/konsensd/db.clj` uploads konsensd, mounts the data
  directory on LazyFS with `jepsen.lazyfs/mount!` as the user konsens,
  writes the TOML with the addresses of the peers, and starts all five
  nodes with `--first-start`. Kill, pause and the log files are there for
  step 3.
- `client.clj` maps the replies as section 3 says. It uses one
  `java.net.http.HttpClient` per process and makes a new one after each
  `:info`.
- Main d89007a, release g++ 16, no fault, 30 s, 20 operations per
  second, ten processes. register: 613 operations, all `:ok`, Knossos
  finds every key linearizable. rw-register: 599 transactions, Elle
  finds no anomaly. No log holds a pattern of a fault. Results in
  `jepsen/results/konsensd-register/` and
  `jepsen/results/konsensd-rw-register/`.
- Found on the way: rhizome needs `java.awt.headless=true`, and the
  generator of Elle must go to the clients only.

## 6c. Result of step 3, first part (2026-11-01)

Main d89007a, release g++ 16, ten processes, a fault every 10 s on
average. Each run is one file `jepsen/results/<test>/<time>.tar.xz`.

| Faults | Workload | Time | Result |
|---|---|---|---|
| partition | register, rw-register | 120 s | valid |
| pause | register, rw-register | 120 s | valid |
| restart, lose-after-stop | register | 120 s | valid; every node votes after each stop |
| restart, crash, lose-after-stop, lose-after-crash | register | 120 s | valid; two nodes retire |
| all six | register, rw-register | 300 s | valid; two nodes retire |

Mutations, each on a branch that never merges:

| Mutation | Faults | Result |
|---|---|---|
| `jepsen-mutation-read-index`: a read is answered without its read index | partition | found: Knossos finds three keys not linearizable |
| `jepsen-mutation-no-sync`: no sync of the WAL, and no retirement | lose-after-stop, lose-after-crash on one, a majority or all nodes | found, but not by Knossos: konsensd finds the damage itself ("a step found a damage of the own log"), stops voting, and the checker of the log fails the run |
| `jepsen-mutation-no-retire`: no retirement | crash, lose-after-crash | not found in 19 crashes |

The third row is expected. A crash with a working sync loses nothing,
so a node that votes again after a crash holds all it promised. The
retirement guards against a loss that a working sync does not cause. No
fault of this test causes such a loss alone; the second row shows the
loss together with a missing sync.

The nemesis of the process:

- `restart`: SIGCONT, SIGTERM, then a start. A node that has not ended
  after 60 s gets SIGKILL.
- `crash`: SIGKILL, then a start.
- `lose-after-stop` and `lose-after-crash`: the same, and LazyFS drops
  what was not synced before the start.
- It acts on one node, a majority or all nodes at once. After each start
  it reads the log of konsensd, and a node that started retired counts.
  A fault that retires never takes the count above two.

Found in the test on the way: sshd as PID 1 reaps no child, so an ended
konsensd stayed a zombie and counted as running; the node containers now
run with `--init`. A stop of a paused node waited for nothing; the stop
now sends SIGCONT first. Knossos ran out of memory with 100 writes per
key and many `:info` writes; the runs with crashes use 20.

Open:

- In the runs with all faults, a clean stop took longer than 60 s three
  times, and the node got SIGKILL after it had written "stopped cleanly".
  So the node voted again. Not checked: whether the exit of the process
  waits on sends to a peer behind a partition.
- A retired node answers every write with 503, and the client must read
  that as `:info`, because konsensd answers a `retry` with the same 503.
  So two retired nodes give many `:info` writes, which cost Knossos
  memory.
- File damage: built in section 6d.

## 6d. Result of step 3, second part (2026-11-01)

Two faults damage the store of one stopped node, and then start it:

- `damage-log` changes the first byte of one random record of the
  table `log`. The node must repair its log from a peer. A node is
  damaged once, because konsensd allows one repair without the
  operator.
- `damage-data` changes the last byte of one random value of the table
  `data`. After the start, the nemesis reads that key at the node
  itself.

A checker of its own fails the run when a damaged log gives no repair,
or when the local read gives the damaged value back.

| Faults | konsensd | Result |
|---|---|---|
| damage-log | main d89007a | valid; five damages, five repairs |
| damage-log | `jepsen-mutation-no-log-check`: no CRC check of a record, no scan of the log at start | found: ten damages, no repair |
| damage-data | main d89007a | found: the node serves the damaged value in 8 of 9 damages |
| crash, damage-log | branch problem-types 5efff24 | valid; two nodes retire, three repairs |

The third row shows the open item of the state file: a row of the
table `data` carries no checksum, so a local read gives a damaged value
back with no error. Knossos did not see it, because the damaged value
was written over before a client read it at that node. `damage-data`
stays out of the nightly run until the rows carry a check.

The fourth row reads the problem type of a 503. A retired or a building
node took no part in the call, so the client records `:fail`: 237 such
writes, and 9 writes stayed `:info`.

Each run keeps `results.edn.xz` and the log of konsensd of each node in
`jepsen/results/<test>/<time>/`, owner 2026-11-01. The whole store of a
run stays outside the tree, in `jepsen/store/`. A run with a binary
`konsensd-<name>` carries `<name>` at the end of its test name.

## 6e. The engine under konsensd (2026-11-01)

The default seccomp profile of podman 4.9.3 allows no `io_uring_setup`.
So konsensd in every run of 6a to 6d ran on the engine of slipstreamIO,
not on io_uring of the kernel. `JEPSEN_ENGINE` now picks the engine:
`kernel` (the default) gives the nodes the default profile of podman
plus `io_uring_setup`, `io_uring_enter` and `io_uring_register`;
`emulation` keeps the default profile. The db checks the engine after
the setup through the io_uring descriptors of konsensd, and the test
name ends with the engine. The runs of 6a to 6d carry `emulation`.

The slow clean stop of 6c, read with gdb in the node when the stop
took longer than 60 s, in all five cases:

- the main thread waits in `thrd_join` in `slipstream_engine_close`;
- the worker of the engine waits in a blocking `connect` to a peer
  behind the partition, in `slip_posix_run_blocking`.

So the end of konsensd waits for the timeout of the SYN. The kernel does
not: measured in a node container with a DROP rule, a pending io_uring
connect gives `io_uring_queue_exit` back in under 1 ms. The engine of
slipstreamIO hands a connect to its worker, where the kernel issues it
non-blocking and waits on the poll of the socket. That is a fault of
slipstreamIO, and its fix is a change there.

| Engine | Faults | Workload | Time | Result |
|---|---|---|---|---|
| emulation | partition, pause, restart | register | 300 s | valid; 5 of the restarts took more than 60 s |
| kernel | partition, pause, restart | register | 300 s | valid; 32 restarts, no SIGKILL |

When a stop takes more than 60 s, the db writes the threads of konsensd
with gdb and the sockets with `ss` to `konsensd-threads.log`; the node
containers carry gdb and `SYS_PTRACE` for it.

## 7. What this does not cover

- Transactions over several keys: konsensd has none.
- Membership changes: konsensd cannot reconfigure.
- Slow and reordered packets: no netem in this kernel.

## 8. Faults of konsensd that the review found

These are not part of this plan. Each needs its own change.

1. `repairing` has no entry in `problem_of`, so it answers 500
   (`program/rest.cpp:174-188`).
2. SIGTERM during a repair or after a fault: `node::stop()` throws
   `std::logic_error` for `faulted_` or `repairs()` (`node.hpp:267-270`),
   `main` ends with a failure (`program/main.cpp:80-85`), and the next
   start retires the node. A stop that the operator asked for ends as a
   crash.
3. `session_expired`, `stale_request` and a failed commit answer 500,
   where a client cannot tell them from a fault of the server
   (`program/serve.cpp:479-504`).

## 9. Open questions for the owner

1. The place: `jepsen/` in mbedded-konsens, or a repository of its own
   as jepsen-io keeps them. Proposal: `jepsen/` here, so a change of the
   REST form and its test move in one commit.
2. Leiningen from its origin, for the tester only. Proposal: yes.
3. A Jepsen run lasts longer than five minutes, and the rules name only
   fuzzing as the exception. Proposal: a Jepsen run is a second
   exception and runs before a release, as fuzzing does.

Answers of the owner, 2026-11-01 16:17 ("passt"): yes to the three
proposals. Added: Jepsen runs each night in a session of the owner's
account when code changed that day; a change of documents only does not
start it.

## 10. Data this plan leaves on the disk

- `jepsen/results/<test>/<time stamp>/`: the summary of each run, kept
  in the tree.
- `jepsen/store/`: the whole store of each run, one tar.xz each, outside
  the tree; not needed by open work.
- The podman images of the control and the nodes: made again by their
  Containerfiles in `jepsen/`.
- The konsensd builds with mutations: made again from their branches,
  deleted after the run.

## 11. Review 1

A second agent reviewed version 1. Blocking and taken: the mutation of
step 3 (item 1), the reply mapping and RFC 9457 (item 2), one
connection per process (item 3), the count of retired nodes (item 4),
the clock row (item 5), the file damage row (item 6), the environment on
Tumbleweed (item 7). Not blocking and taken: two workloads (item 8),
LazyFS after a clean stop (item 10), io_uring in seccomp (item 11),
nftables through a custom `jepsen.net` (item 12), the parts of
jepsen-io tests (item 13), the citations, the list of data and the
question of the run time (item 14).
