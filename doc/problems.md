# Problem types of konsensd

konsensd answers an error with problem details of RFC 9457
(`application/problem+json`). This file defines the problem types of
konsensd, as RFC 9457 section 4 asks. An error that has no type here
comes with the type `about:blank`, and its status code says what it
is.

Each type states its type URI, its title, its status code, whether
the answer carries `Retry-After`, and what a client knows and does.

## retry

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#retry`
- Title: The node cannot take the call now
- Status: 503, with `Retry-After`

The node knows no leader, or its promise changed. A write may have
been proposed before the change, so its outcome is unknown. Send the
call again on the same connection: the session gives back the result
of a write that was decided.

## building

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#building`
- Title: The node builds
- Status: 503, with `Retry-After`

The node builds its store from a peer. It took no part in the call.
Send the call to another node, or again after the build.

## reconfiguring

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#reconfiguring`
- Title: The configuration ends
- Status: 503, with `Retry-After`

The configuration of the node ends with a StopSign. A write may have
been proposed before the StopSign, so its outcome is unknown. Send the
call to a node of the next configuration.

## retired

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#retired`
- Title: The node is retired
- Status: 503, with `Retry-After`

The node started without a clean stop and votes never again. It took
no part in the call, so the call changed nothing. Send the call to
another node.

## repairing

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#repairing`
- Title: The node repairs its store
- Status: 503, with `Retry-After`

The node repairs a damage of its log. A write that waited when the
repair started may have been proposed, so its outcome is unknown. Send
the call to another node, or again after the repair.

## rejected

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#rejected`
- Title: The cluster rejected the write
- Status: 409

No result of the write has a majority. The write changed nothing.

## stopped

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#stopped`
- Title: The node stopped
- Status: 503, without `Retry-After`

The node stopped before the write was decided. The write may still be
decided. A node of a cluster that shuts down refuses each new write with
this answer, and such a write changed nothing.

## not-waiting-for-the-operator

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#not-waiting-for-the-operator`
- Title: The node does not wait for the operator
- Status: 409

The answer of `POST /repair/allow-one-more` on the control socket. The
node does not wait for the operator in a repair, so the call changed
nothing. A node that votes, a node that repairs already and a node
without its meta record give this answer.

## repair-does-not-start

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#repair-does-not-start`
- Title: The repair does not start
- Status: 500

The answer of `POST /repair/allow-one-more` on the control socket. The
operator allowed the repair, but the store of the node is not
consistent, and the repair does not start. The node takes no part in
the cluster. A stop ends it without a clean stop, so it is retired at
its next start. Replace the node.

## cluster-did-not-shut-down

- Type: `https://github.com/Asmod4n/mbedded-konsens/blob/main/doc/problems.md#cluster-did-not-shut-down`
- Title: The cluster did not shut down
- Status: 503, with `Retry-After`

The answer of `POST /cluster/shutdown` on the control socket. The leader
changed before it decided the entry of the shutdown, so no node shut
down. Call again.
