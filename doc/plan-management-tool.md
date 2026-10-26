# Plan: the management tool in three levels

Status: draft. Nothing is built. The owner answers the open questions
in part 6 first. A second agent reviews the plan after the answers and
before the build.

## 1. Facts checked in this session

Decisions of the owner that hold:

- The management tool is one binary of its own. It only manages. The
  node binary has no user interface (owner, 2026-10-24 12:13, 12:14).
- The user interface is in a webview where a webview can be used. A
  TUI and a CLI are the two other levels (owner, 2026-10-26 14:14).
- The tool runs on a phone with no installation (owner, 2026-10-24
  20:59). Push goes through Web Push with VAPID, with no paid account
  (owner, 20:57 to 21:00). Access goes through the VPS by VPN (owner,
  21:04).
- The tool writes the configs of all nodes, with one seed list and
  distinct node IDs, when it gets no cluster config (owner, 2026-10-24
  12:11).
- The tool finds a node in this order: the node that the operator set
  in the stored config of the tool; a node seen by the discovery
  beacon (7150/udp); the nearest node by subnet and hops; the seed
  node. A hello to all nodes, where the first answer wins, is the
  alternative (owner, 2026-10-25 18:56 to 19:05; task 61be2c4f in
  Claude-state).
- The tool shows a full disk ("no storage"), the single-node stages
  with decided_idx, accepted_idx and n_accepted, and each hardware
  finding with its evidence (owner, 2026-10-17).
- After an automatic repair, the tool shows a message. The operator
  allows the next automatic repair, or no repair happens (owner,
  2026-10-24 20:57).
- The single-node stages start only by a flag at the start of the node
  binary (owner, 2026-10-17 21:09). The tool shows them. The tool does
  not start them.
- The smallest server is 1 vCPU, 1 GB RAM and 40 GB SSD (owner,
  2026-10-25 19:08).
- The first tests use plain TCP. TLS comes later (owner, 2026-10-17
  20:52).

Packages, read in this session:

| part | library | Ubuntu 26.04 | Debian 13 |
|---|---|---|---|
| CLI options | CLI11 | yes (Launchpad) | not checked |
| TUI | FTXUI | yes (Launchpad) | yes, libftxui-dev |
| TUI | notcurses | 24.04 has 3.0.7 | not checked |
| webview, Linux | WebKitGTK 6.0 | 24.04 has 2.52.6 | not checked |
| webview, all OS | webview/webview | no source package | no package |

webview/webview has no package. It is not a candidate unless the
owner makes it an exception, as for CRC32C, libhydrogen and LMDB.

## 2. Form

One core and three front ends. The core is a C++ library inside the
tool. It does all work. Each front end only shows and asks.

```
          +-- cli   (one command, one answer, then exit)
core -----+-- tui   (one terminal, live view)
          +-- web   (an HTTP server; a page for the phone and
                     for the webview)
```

The core:

1. Finds a node, in the order of part 1.
2. Speaks the management protocol to that node (question 6.3).
3. Reads the state of the cluster: nodes, leader, ballot, decided_idx,
   accepted_idx, n_accepted, disk use, findings, repairs.
4. Sends the commands of part 3.
5. Writes the configs of a new cluster.

The web level is the phone level. The page is the same on the phone
and in the webview. The webview is a window that shows the page of a
local web level. So the page is written once.

The tool runs where the operator is, or on the VPS for the phone. On
the VPS it runs as a service with the web level on. The phone opens
the page through the VPN.

## 3. What the first version does

Read only:

- `status`: the nodes, the leader, the indexes and the stage of each
  node.
- `disk`: the use of the disk on each node and "no storage".
- `findings`: the hardware findings with their evidence.
- `watch`: the same as `status`, as a stream (TUI and web only, or
  CLI with one line per change).

Changes:

- `allow-repair`: the operator allows the next automatic repair on
  one node.
- `init`: writes the configs of a new cluster (seed list, node IDs,
  leader priority).
- `connect`: sets the target node in the stored config of the tool.

Not in the first version: a reconfiguration, a stage change, an
action on application data, Web Push. Each comes as a task of its own.

The names of the commands are provisional (question 6.6).

## 4. Order of the build

1. Core and CLI, with tests against the three-node example. The CLI is
   the level that a test can call.
2. Web level and page, on the same core.
3. Webview on the web level.
4. TUI on the same core.
5. Web Push.

Each step is its own branch and is green before the next starts.

## 5. Not in this plan

- The HTTPS part for users of an application. It is open.
- TLS between the tool and a node.
- The VPN setup on the VPS.

## 6. Open questions

6.1 Where the page for the phone comes from.
  a) The tool runs as a service on the VPS and serves it. Proposal.
  b) Each node serves it. This breaks "the node binary has no user
     interface".

6.2 The HTTP server of the web level.
  a) mruby-webmachine. Proposal: it is the HTTP server of the owner.
  b) A small C++ HTTP library from a package.

6.3 The protocol between the tool and a node.
  a) The node-to-node wire: a fixed head and packed CBOR. Proposal:
     the node then needs no HTTP.
  b) HTTP on the node.

6.4 The webview.
  a) Linux only, with WebKitGTK from the package. Other systems open
     the default browser on the local page. Proposal.
  b) webview/webview as a submodule for all systems. This needs an
     exception from the package rule.
  c) No webview. Every system opens the default browser.

6.5 The TUI library.
  a) FTXUI. Proposal: C++, a package in Ubuntu and Debian.
  b) notcurses.
  c) ncurses.

6.6 The name of the binary and of the commands. No specification gives
  them. Proposal: `konsensctl`, after the `ctl` form of `systemctl`,
  and the commands of part 3.

6.7 Who may manage, before TLS exists.
  a) Everyone who reaches the port. The VPN is the only guard.
     Proposal for the first tests only.
  b) A shared key from the start (libhydrogen).
