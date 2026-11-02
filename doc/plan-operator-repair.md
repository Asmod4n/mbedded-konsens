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

1. konsensd makes the directory `control` in its data directory with
   the permissions `owner_all` and nothing else, and refuses to start
   when the directory exists with other permissions or another owner.
   The directory exists before the bind, so no other user can open the
   socket between the bind and a change of its permissions.
2. konsensd listens on `unix:<data_directory>/control/control.sock`. No
   configuration key exists for it. A path longer than `sun_path` makes
   the start fail with the message of `listener_on`, as for the other
   two listeners.
3. The control listener takes HTTP/1.1 as the client listener does, with
   its own parse: `take_control_request` in `program/rest.cpp`. It
   knows one target and one method:
   - `POST /repair/allow-one-more` calls
     `node::allow_one_more_repair()` when
     `node.waits_for_the_operator()` is true, and answers 204.
   - When the node does not wait for the operator, the answer is 409
     with the problem type `not-waiting-for-the-operator` in
     doc/problems.md.
   - Another target is 404, another method on the target is 405, as on
     the client listener.
   A control connection takes no client call. A client connection takes
   no control request.
4. The result of `allow_one_more_repair` is a `result`. An error goes to
   the client of the operator as the problem of `problem_of`, and the
   loop goes on, as for a client call. The repair writes its count in
   node.db before it returns; a write that fails throws `sync_error`,
   which ends konsensd as today.
5. The repair requests that `allow_one_more_repair` sends leave in the
   same pass of the loop, through the path that sends the output of a
   tick.
6. When the node starts to wait for the operator, konsensd writes once
   to stderr: `konsensd: node <pid> waits for the operator; POST
   /repair/allow-one-more on <socket path> allows one more repair`.
7. doc/problems.md gets the new type. README.md names the control
   socket in the part on konsensd.

## 4. What does not change

- The library, the wire format and the model. The change calls a member
  that test_node already tests ("the second damage waits for the
  operator, and allow_one_more_repair allows one more").
- The client listener and its routes.

## 5. Tests

1. program/test/test_rest.cpp: `take_control_request` for the one route,
   for another target (404), another method (405), a body on the POST
   (the body is read and dropped), and a request that is not whole yet.
2. program/test/three_nodes.sh, after the repair that it has now:
   - damage one record of node 2 again in the same way, while node 2
     is stopped, and start node 2;
   - check that the log of node 2 says that it waits for the operator;
   - check that a read at node 2 answers 503;
   - check that a POST on the control socket answers 204, and that a
     second POST answers 409;
   - check that node 2 ends its repair and serves a write that node 1
     reads.
3. A test that the start refuses a control directory with the
   permissions 0755.
4. Each change fails its test without the change.
5. Release with g++ 16 in the Ubuntu 26.10 container that the Jepsen
   thread uses for konsensd; the tests of the library are not touched.

## 6. Data

The build directory of the test container. It is made again by a build.
