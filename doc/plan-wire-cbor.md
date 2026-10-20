# Plan: the messages between the nodes as CBOR

Source: the owner, 2026-10-20 06:17 to 07:07 (UTC+2). mbedded-cbor is
complete for correctness. For tests, the messages between the nodes are
naive RPC messages: a header with the method and the kind, then the
message. `cbor::schema` (Packed CBOR) needs C++26 reflection, so the
first form uses `cbor::structs`, which runs on C++23 through Boost.PFR.
Packed comes when `cbor::schema` has a form without reflection. This
plan gets one review before the build.

## 1. Facts checked in this session

- `cbor::structs` encodes and decodes `decide`, `accept_decide`,
  `promise` (with and without `log_sync`) and `recovery_response` of
  `include/konsens/message.hpp` as they are, with no change to the
  structs. Each round trip gives the same bytes. Builds: g++ 16.2 with
  `-std=c++23` and with `-std=c++26 -freflection`, clang 22 with
  `-std=c++23`, all with the warnings of this tree and `-Werror`
  (Ubuntu 26.10 in podman, a test file outside the tree).
- mbedded-cbor at 105ab12 does not compile with g++ 13 or g++ 14:
  `std::ranges::starts_with` is missing in their libstdc++
  (`diagnostic_notation.hpp:373`, `:624`, `query.hpp:294`).
- `cbor::schema` has no form without reflection (`schema.hpp:2467`
  uses `std::meta`; `doc/schema.md:19`).

## 2. Form on the wire

One message is a CBOR sequence (RFC 8742) of two items:

1. The header, a map: `method`, `message`, `kind`, `from`, `to`.
2. The body, a map: the members of the struct that `kind` names.

| Field | Type | Meaning |
|---|---|---|
| `method` | `std::uint8_t` | the CoAP method code of RFC 7252 section 12.1.1: 1 GET, 2 POST, 3 PUT, 4 DELETE |
| `message` | `std::uint8_t` | the index in `konsens::message`: 0 Paxos, 1 BLE |
| `kind` | `std::uint8_t` | the index in `paxos_msg` or in `heartbeat_msg` |
| `from`, `to` | `node_id` | as in `paxos_message` and `ble_message` |

Every message between the nodes has method 2 (POST). A header with
another method gives an error. The field stands so that client calls
(GET for a read, POST for a write) can use the same header later.

No length prefix: a CBOR item carries its own length, and
`cbor::sequence` splits the two items. Packed CBOR needs a length
prefix (its root stands at the end); it comes with Packed.

No CBOR tag: the header names the struct. The tags of the provisional
block (owner, 2026-10-20 06:20) come with Packed.

## 3. Code

- `include/konsens/wire.hpp`, namespace `konsens`:
  - `std::expected<std::string, std::error_code> encode(const message& m)`
  - `std::expected<message, std::error_code> decode(std::string_view bytes)`
- `decode` checks, in this order: two items and nothing after them,
  the header, the method, `message` and `kind` inside their variant,
  then the body as the struct that `kind` names. Each error branch is
  `[[unlikely]]`. A `cbor::error` converts to `std::error_code`.
- The struct for `kind` comes from the variant by index
  (`std::variant_alternative_t`), so no second list of the kinds exists.
- `node` does not change. The provisional API is the two free functions
  (the card on the API is not answered yet).
- Build: mbedded-cbor as a git submodule `deps/mbedded-cbor` at
  a4c5624 (owner, 2026-10-20: the state he publishes; from 105ab12 only
  `schema.hpp` and `validity.hpp` changed), through `add_subdirectory` with `CBOR_INSTALL` off. Boost.PFR
  from the distribution package (`libboost-dev`, Tumbleweed
  `boost-devel`).
- The host build with g++ 13 stops. The builds are g++ 16 and clang 22
  in podman, Debug with ASan and UBSan, and Release.

## 4. Tests

- `test/test_wire.cpp`:
  - A round trip of every alternative of `paxos_msg` and of
    `heartbeat_msg`, each with values that are not zero: `decode` of
    `encode` equals the input, and a second `encode` gives the same
    bytes.
  - Errors: an empty input; a cut input at each length shorter than the
    message; a third item after the body; a method other than 2; a
    `message` of 2; a `kind` one past the end of each variant; a body
    of another kind under the header.
  - The expectations come from RFC 8949 and RFC 8742, not from the
    output of `encode`: the test builds the wrong inputs by hand.
- `counter_stress`: a flag `--cbor` (provisional name) sends every
  message through `encode` and `decode`. Seeds 1 to 300 with
  `--loss 50 --duplicate 50 --restart 20` pass with the flag, in Debug
  with sanitizers and in Release.

## 5. Not in this plan

Packed CBOR, CBOR tags, a length prefix, TLS, a socket, client calls
over the wire, CoAP itself. No speed figure: the plan measures nothing.

## 6. Order

1. Submodule, CMake, `wire.hpp`, `test_wire.cpp`; both compilers green.
2. `--cbor` in `counter_stress`; the 300 seeds.
3. README: one section on the wire form.
4. State file in Claude-state.

## 7. Changes after the review

The review `doc/review-plan-wire-cbor.md` (approve with changes) holds
18 findings. Each is applied as its fix says, with these choices:

- F1, F13: `decode(std::string bytes)` takes the bytes by value, moves
  them into one shared owner, splits the two items with
  `cbor::item_size`, and decodes each item with the owner and a view.
- F2: `decode` copies the body once more out of `cbor::ref`. The copy
  is provisional (question to the owner).
- F3: the test of a wrong body uses a body that lacks a member of the
  kind in the header, and expects `key_not_found`. Kinds with the same
  keys (`heartbeat` and `heartbeat_ack`, `prepare_req` and
  `not_accepted`) are named in the test as a known limit of the naive
  form.
- F4: `field_count_of` becomes one public function in `storage.hpp`.
  `persistent_storage.cpp` and `decode` both call it for each entry.
- F5: each struct in `message.hpp` gets a defaulted `operator==`.
- F6, F7, F9: distinct values per member; both values of each `bool`;
  each optional empty and set; all five entry kinds; all 256 values of
  `method` and of `kind`. Three messages are checked against bytes
  written by hand from RFC 8949 and RFC 8746.
- F8: RFC 8949, RFC 8742, RFC 8746 and RFC 7252 go into `refs/` in the
  commit of `wire.cpp`.
- F10: the checks of konsens give values of a new enum `wire_errc` in
  the form of `storage_errc`, one value for each check (provisional
  names; question to the owner). Errors of mbedded-cbor pass through.
  Each error test checks the exact value.
- F11, F12: the header is the struct `header` (RFC 7252 section 3) with
  `method`, `kind`, `from`, `to`. `kind` is one index over the
  alternatives of `paxos_msg` followed by those of `heartbeat_msg`. The
  method POST is a named constant. A `static_assert` holds both
  variants under 256 alternatives together.
- F14: Tumbleweed package `libboost_headers-devel`.
- F15: the code stands in `src/wire.cpp`; mbedded-cbor is linked
  PRIVATE, so its compile definitions do not reach the users of
  konsens.
- F16: the builds are g++ 16 and clang 22 (section 1 shows clang 22
  compiles `cbor::structs`). The host build directories go.
- F17: `--cbor` takes no value and stands in the usage text. With it,
  the network of `counter_stress` holds bytes and counts each kind.
  For each seed of 1 to 300, the output with `--cbor` equals the output
  without it, apart from the counts of kinds. A ctest entry runs it.
- F18: BLE does not check `from` and `to`. This goes on the list of
  open work, not into this plan.
