# Plan: a reply carries its request number

Source: the owner, 2026-11-01 15:50, the findings of the comparison with
Jepsen ParticleDB 1.1.4 (jepsen-fehlerarten.md 2.12, "Antwort an den
falschen Client"). Base: main 63a8ff3.

## 1. The fault

`client_reply` holds `client_id`, `session` and `response`
(`client.hpp:66-70`). A session may have several requests in work:
`requests_` is keyed by session and request number
(`node.hpp:634`, `:2594`). The reply of a request carries the session
and no request number (`node.hpp:2124`, `:2538`, `:877`; `:877` runs from `stop` and from the start of a repair). The node reads nothing from `client_id`; it keeps it and gives it back.
So a caller that gives each call its own `client_id` can tell the
replies apart today (examples/three_nodes.cpp:194, examples/counter.cpp:172).
A caller with two requests of one session in work under one
`client_id` cannot.

konsensd does not have the fault: it passes the id of the connection as
`client_id`, and a connection has one call in work
(`program/serve.cpp:377`, `:390`). The README example uses one
`client_id` for the registration, the request and the query
(`README.md:246-260`).

VR Revisited (the client table) and the Raft dissertation 6.3 return the
request number with the reply, so the client matches a reply to its
request. That is the form here.

## 2. The change

1. `client_reply` gets a last member `std::uint64_t request_number{}`.
   It stays an aggregate. The existing initializers keep their order.
2. Each reply to a client request carries its request number: the
   reply after the apply (`node.hpp:2124`), the `retry` of an old
   promise (`:2538`) and the error to the waiting calls (`:878`).
3. A reply to a registration, an expiry or a query has request number 0.
   A request number starts at 1 (`node.hpp:623-625`), so 0 names no
   request.
4. The README says it: a reply of a request carries the session and the
   request number; a reply of a query carries only the `client_id`, so
   a caller with several queries in work uses one `client_id` for each.

The name `request_number` is the name of the parameter of
`client_request` and of VR Revisited.

## 3. What this does not cover

- A query has no number of the caller. Two queries with one `client_id`
  stay apart only by order. A number for a query would be a new
  parameter of `client_query` and a new name; that is a question for the
  owner, not part of this plan.
- `requests_.insert_or_assign` replaces the waiting call when a second
  call with the same session and request number comes, and the first
  caller gets no reply (`node.hpp:634`). Not changed here.

## 4. Tests

test_client: "two requests of one session in work get replies with
their request numbers" - register, send requests 1 and 2 of the session
before the decide, run, and find one reply for each number with the
session. And the `retry` after a change of leader carries the number.

Also: the reply to a registration, a query and an expiry carries 0; a
request in work at `stop()` gets `stopped` with its number; the
simulation checks the request number of each request reply.

## 5. Checks

Release g++ 16: test_client and every test that builds a
`client_reply`. The CI runs every build.

## 6. Review 1

No finding blocked. Taken: the narrower fault and the README sentence
that `client_id` comes back unchanged (items 1, 7); the callers of
`:877` (item 3); the tests of item 6; the order of the two requests at
the leader (item 8); the line numbers (item 9); the check in the
simulation (item 10). The member has a default, because without it g++
warns `missing-field-initializers` (item 4).
