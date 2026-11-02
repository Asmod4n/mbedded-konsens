# Plan: the operator allows one more repair in konsensd

Source: the owner, 2026-11-02 07:47, card "Ja, bauen".
Base: main 6318611.

## 1. The fault

The library counts the repairs in node.db (doc/plan-repair.md, section
4a). The first repair runs by itself. At each further damage the node
stops voting, sends no repair request and waits until the caller calls
`node::allow_one_more_repair()` (`include/konsens/node.hpp:726`).

konsensd never calls it. After the first repair, a node that finds a
second damage waits for ever. A stop does not help: `stopped` ends such
a node without a clean-stop mark (`program/serve.cpp:880`), and the next
start retires it. The only way back is a new node.

## 2. The known form

Daemons give the operator their runtime commands through a control
socket: a Unix domain socket that only the owner of the process can
open, and a small client.

- Knot DNS: `knotc` talks to the control socket of `knotd`.
- PowerDNS: `pdns_control` talks to the control socket in `socket-dir`.
- Unbound: `unbound-control` talks to the control interface.
- HAProxy: the runtime API on a stats socket with `level admin`.
- BIRD: `birdc` talks to the control socket of `bird`.

The client port of these daemons does not take the commands of the
operator. The file permissions of the socket are the access control.

konsensd takes this form. The client of the operator is curl with
`--unix-socket` for now, the same tool that three_nodes.sh uses. The
management tool uses the same socket later.

## 3. The change

1. konsensd makes the directory `control` in its data directory and
   sets the permissions `owner_all`. One function,
   `make_a_private_directory`, then reads the directory with `lstat`
   (C++ gives no owner) and refuses the start when the path is a
   symbolic link, no directory, has another owner than the effective
   user, or has other permissions than 0700. The directory exists
   before the bind, so no other user can open the socket.
2. konsensd listens on `unix:<data_directory>/control/control.sock`. No
   configuration key exists for it. The socket file is removed at the
   end, as the other socket files are.
3. The control listener takes HTTP/1.1 as the client listener does.
   `take_control_request` in `program/rest.cpp` knows one target and
   one method: `POST /repair/allow-one-more`. Another target is 404.
   Another method is 405 with `Allow: POST`. A body is refused with
   413: `max_control_body_bytes` is 0.
   The head checks (parse, version, Transfer-Encoding, Content-Length,
   the body limit, a request that is not whole) are one function that
   `take_request` and `take_control_request` both call. Both check the
   target before the method, because RFC 9110 15.5.6 defines 405 for a
   method that the target resource does not support. `problem` carries
   its `Allow` value; the client 405 carries `GET, PUT, DELETE`.
4. The loop gets a third link kind `control`, the operations
   `accept_control` and `receive_control`, and a third multishot
   accept. A control receive takes its buffers from the ring of the
   clients. A control connection takes no client call, and a client
   connection takes no control request.
5. The receive only takes the request and marks the connection. After
   `commit_the_step`, when no step is open, the loop answers each
   marked connection:
   - The node waits in a repair when `repairs()` and
     `waits_for_the_operator()` both hold. A store without its meta
     record sets `waits_for_the_operator()` with no repair, and
     `allow_one_more_repair` throws `std::logic_error` there.
   - It waits in a repair: konsensd calls `allow_one_more_repair()`,
     sends the output of the node with `release_the_output`, and
     answers 204. The repair requests leave in the same pass through
     `arm_the_sends`.
   - It does not wait in a repair: 409 with the problem type
     `not-waiting-for-the-operator`.
   - `allow_one_more_repair` gives an error: 500 with the problem type
     `repair-does-not-start` and the message of the error, and one
     line on stderr. The node then repairs without a repair that
     runs, so the stop does not wait for the repair any more, and a
     stop ends the node without a clean stop.
   - A write of the count that fails throws `sync_error`, which ends
     konsensd as today.
6. At each change from "does not wait in a repair" to "waits in a
   repair", konsensd writes one line to stderr: `konsensd: node <pid>
   waits for the operator; POST /repair/allow-one-more on <socket path>
   allows one more repair`. The field that remembers the last value
   starts at false, so a node that waits at its start writes the line.
7. konsensd resolves the peers, makes the three listeners and the
   control directory before it opens the store. A start that the
   control directory refuses thus ends before the store opens, and
   does not retire the node. The first test run showed this fault:
   the refused start opened the store, ended without a clean stop, and
   the next start was retired.
8. doc/problems.md gets the two types. README.md names the control
   socket, says that the data directory belongs to the user of
   konsensd, and that the path of the socket has at most 107 bytes.

## 4. What does not change

- The library, the wire format and the model. The change calls a member
  that test_node already tests ("the second damage waits for the
  operator, and allow_one_more_repair allows one more").
- The routes of the client listener.

## 5. Tests

1. program/test/test_rest.cpp: `take_control_request` for the one
   route, another target (404), another method (405 with `Allow:
   POST`), a body (413), a request that is not whole yet, and
   `Connection: close`. The 405 of the client listener carries its own
   `Allow`.
2. program/test/three_nodes.sh, after the repair that it has now:
   - a POST at node 1, which votes, answers 409;
   - stop node 2, damage one record again in the same way, start it;
   - the log of node 2 says once that it waits for the operator;
   - a read at node 2 answers 503;
   - a POST on the control socket answers 204, a second POST 409;
   - node 2 ends its repair and serves a write that node 1 reads.
3. three_nodes.sh: the start refuses a control directory with 0755,
   and a symbolic link named `control`. An owner check needs a
   directory of another user and is not tested.
4. three_nodes.sh: a store without its meta record at node 3: the POST
   answers 409, and konsensd lives.
5. three_nodes.sh takes a fourth argument `sync` that sets
   `sync_by_the_caller`. Both forms run.
6. Each check is run once against the code without its change, and
   the failing check is named in the report.
7. Release with g++ 16 in the Ubuntu 26.10 container that the Jepsen
   thread uses for konsensd.

## 6. Data

The build directory of the test container. It is made again by a build.
