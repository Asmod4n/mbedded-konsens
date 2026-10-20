# Review: plan for the messages between the nodes as CBOR

Plan: `doc/plan-wire-cbor.md`, commit 617eaa5.
mbedded-cbor: origin/main 105ab12.
The reviewer read the code and the documents. The reviewer did not
build or run anything.

## Verdict

Approve with changes.

The form on the wire and the order of the work are sound. Findings 1,
2, 3, 4 and 5 change the code or the tests before the build can start.
Findings 2, 10 and 12 need a decision of the owner. The other findings
are local changes to the plan.

## Findings

### 1. `decode(std::string_view)` cannot call `cbor::structs` without a copy

Fault: `structs<T>::decode` takes an owner of the bytes. A
`std::string_view` alone does not compile. An empty owner throws
`std::logic_error`. A decode that takes only a view must allocate a new
owner and copy the bytes into it. The rule "A parser copies nothing"
forbids this copy.

Evidence: `mbedded-cbor/doc/advanced/structs.md:39-40` ("A `std::string`
lvalue, a `std::string_view` alone and a `char const *` do not
compile"). `include/cbor/structs.hpp:1349-1391`.
`include/cbor/validity.hpp:178-182` (`use_count() == 0` throws).

Fix: `decode` takes `std::string bytes` by value. It moves the bytes
into one `std::shared_ptr<std::string const>`. It splits the bytes with
`cbor::item_size`. It calls the form
`decode(std::shared_ptr<void const> owner, part)` for the header and
for the body, with that one owner. No byte is copied.

### 2. A `cbor::ref<T>` gives the value only as `T const &`

Fault: `structs<T>::decode` returns `cbor::ref<T>`. The value in it is
private. `*r` gives `T const &`. No call moves the value out. So
`decode` must copy the body (every `std::vector<entry>` and each
payload) to return a `konsens::message`. This conflicts with "A parser
copies nothing". If `decode` returns `cbor::ref<message>`, it shows a
type of its dependency, which conflicts with "A module owns the types
of its dependency and shows them to nobody".

Evidence: `include/cbor/ref.hpp:63-69` (`value` is private),
`ref.hpp:100-114` (`operator*` gives `T const &`).

Fix: a question to the owner. Options:
(a) Copy the body once, mark the copy as provisional, and continue.
(b) Add to mbedded-cbor a decode that returns `T` for a `T` that holds
no view (no `std::string_view`, no `std::span<std::byte const>`).
(c) Return `cbor::ref<message>` from konsens.
Proposal: (a) now, (b) as open work in mbedded-cbor.

### 3. The test "a body of another kind under the header" cannot pass as written

Fault: some kinds have the same members. `heartbeat` and
`heartbeat_ack` both have `n` and `id`. `prepare_req` and
`not_accepted` both have `n`. Also, `structs` skips a key that no
member has. So a `decide` body under the header of `not_accepted`
decodes with no error. Only a body that does not have a member of the
kind of the header gives an error. If the test picks a pair that
fails, the test shows nothing about the general case.

Evidence: `include/konsens/message.hpp:56-58`, `:100-102`, `:125-133`.
`mbedded-cbor/doc/advanced/structs.md`, section 4: "a key that no
member has: the entry is skipped".

Fix: replace the test. (a) A body that does not have a member of the
kind of the header gives `key_not_found`. (b) A body with the same keys
as the kind of the header decodes as the kind of the header. The README
says that the header alone names the kind.

### 4. `entry_kind` takes a value that is not a member of the enum

Fault: `structs` decodes an enum as its underlying integer and does not
check the value against the enumerators. A body with an entry of kind
200 decodes into `entry_kind{200}`. This breaks "A value does not
change its type without a check". The check that a kind is valid
already exists in persistent storage. "A check for safety exists once"
requires that `decode` calls the same function.

Evidence: `include/cbor/structs.hpp:571-574` (`integer_read` of the
underlying type, no range check), `:408-409`.
`include/konsens/storage.hpp:25`. `src/persistent_storage.cpp:163-175`
(`field_count_of` gives `std::nullopt` for a kind that is not valid)
and `:240-243`.

Fix: move the check of a valid `entry_kind` to one public function.
Persistent storage and `decode` call it. A test decodes an entry with
each of the 256 values of the underlying type. Five values are valid.

### 5. The round trip compares values that have no `operator==`

Fault: the test says "`decode` of `encode` equals the input". Only
`entry` has `operator==`. `ballot` and `sequence_number` have `<=>`.
No struct in `message.hpp` has `==`, so `std::variant` and
`paxos_message` cannot be compared. The test does not compile.

Evidence: `include/konsens/message.hpp:51-169` (no `operator==`),
`include/konsens/storage.hpp:35`.

Fix: add `friend bool operator==(const T&, const T&) = default;` to
each struct in `message.hpp`. An aggregate stays an aggregate, so
Boost.PFR and reflection read it as before. The plan lists this change
of `message.hpp`.

### 6. Values that are not zero can hide two exchanged members

Fault: "values that are not zero" lets two members of one type hold the
same value. Examples: `n` and `n_accepted` in `promise` (both
`ballot`), `decided_idx` and `accepted_idx`. A decode that exchanges
two such members passes the test.

Evidence: `include/konsens/message.hpp:60-73`, `:112-119`.

Fix: each scalar member in the test has a value that no other member
has. Each `bool` is tested with both values. Each `std::optional` is
tested empty and with a value. The entries hold each of the five
values of `entry_kind`.

### 7. No test checks the form on the wire against the specification

Fault: a round trip and a second `encode` show only that `encode` and
`decode` agree with each other. The bytes of the wire do not appear in
a test. The rule "The expectation is written from the specification"
is met only for the inputs that are wrong. Also, the plan names RFC
8949 and RFC 8742, but a `std::vector<std::byte>` is an RFC 8746 typed
array with tag 64, and `std::vector<std::uint64_t>` is tag 71. A
change of the order of the alternatives in a variant changes the
indices on the wire, and no test sees it.

Evidence: `mbedded-cbor/doc/advanced/structs.md`, section 3 (typed
arrays, tag 64). Plan, section 4, lines 82-83.
`include/konsens/storage.hpp:29`, `message.hpp:136`.

Fix: the test holds bytes written by hand for one header, for one
`accept_decide` with a payload, and for one `read_index_reply`. The
keys stand in the order of RFC 8949 section 4.2.1. The plan adds RFC
8746 to its list of specifications.

### 8. The specifications are not in the tree

Fault: the rule "A grammar is read, not remembered" requires the
specifications in the tree, in the commit of the first function that
reads them. `refs/` holds only `rfc7143.txt`.

Evidence: `refs/` (`omnipaxos`, `papers.md`, `rfc7143.txt`).

Fix: step 1 of the plan adds RFC 8949, RFC 8742, RFC 8746 and RFC 7252
to `refs/`, verbatim.

### 9. The fields of the header are finite sets, and the tests check one value

Fault: `method`, `message` and `kind` are `std::uint8_t`. The rule
"every member of the set is checked" requires a test of each of the
256 values. The plan tests one wrong method, one wrong `message` and
one wrong `kind` for each variant.

Evidence: plan, section 4, lines 78-81.

Fix: a test decodes a header with each of the 256 values of `method`,
of `message`, and of `kind` under each value of `message`. The test
also gives 256 (`number_out_of_range`) and -1 (`incorrect_type`).

### 10. The plan does not say which error each check gives

Fault: three checks of `decode` have no `cbor::error`: an item after
the body, a method other than POST, and an index out of its variant.
Since 105ab12, `structs` no longer gives `syntax_error` for bytes after
the item. Also, a `std::error_code` with the category of mbedded-cbor
shows a type of the dependency: a caller compares it with
`cbor::error`. A test that checks only `!has_value()` passes when
`decode` fails for a wrong reason.

Evidence: `git show 105ab12` (the check of the bytes after the item is
removed from `structs.hpp:1480`). `include/cbor/error.hpp:133-141`.
`include/konsens/persistent_storage.hpp:22-25` (`storage_errc`, the
form that konsens already has).

Fix: a question to the owner (an error path). Options:
(a) The checks of konsens give `std::errc::bad_message` (POSIX
`EBADMSG`). A `cbor::error` passes as it is.
(b) A new enum of konsens, in the form of `storage_errc`, for every
fault, with the `cbor::error` mapped into it.
Proposal: (a). It makes no name. Each error test checks the exact
value. The empty input and each cut input give `too_little_data`.

### 11. Numbers and conversions have no name and no check

Fault: "method 2" is a number with no name. The index of a variant is
`std::size_t`. The header holds `std::uint8_t`. The plan does not
check the conversion.

Evidence: plan, section 2, line 40. Rules "A number has a name" and "A
value does not change its type without a check".

Fix: a named constant `post` with the value 2. Its source is RFC 7252
section 12.1.1 (code 0.02). A `static_assert` that the size of each
variant is at most 256.

### 12. The header member `message` hides the type `konsens::message`, and the header has no name

Fault: in namespace `konsens`, a member named `message` hides the type
`konsens::message` inside the header struct. The rule "No name hides
another name" forbids it. The owner named "method" and "kind". The
name `message` has no source. The plan does not name the struct of the
header.

Evidence: `include/konsens/message.hpp:169`. Plan, section 2, line 30
and line 36.

Fix: a question to the owner. Options:
(a) One `kind` for both variants: 0 to 14 for `paxos_msg`, 15 and 16
for `heartbeat_msg`. The field `message` is removed.
(b) Another name for the field, from a source.
Proposal: (a). The struct of the header is `header`, the word of RFC
7252 section 3.

### 13. `cbor::sequence` does not fit the split; `cbor::item_size` fits

Fault: `cbor::sequence` also takes an owner. It gives no error when the
second item is missing: it gives one item and ends. `decode` then must
count the items itself.

Evidence: `include/cbor/sequence.hpp:30-41`, `:84-92`.
`include/cbor/item_size.hpp:202-209` (takes a `std::string_view`, no
owner, no allocation).

Fix: section 2 names `cbor::item_size`. `decode` calls it for the
header, then for the rest. An empty rest gives `too_little_data` from
`item_size`. A rest that is not empty after the body gives the error of
finding 10. A test puts a well-formed item after the body, and also
one byte `1c` that is not well-formed.

### 14. The package name for Tumbleweed is not the one that mbedded-cbor uses

Fault: the plan names `boost-devel`. The Containerfile of mbedded-cbor
for Tumbleweed installs `libboost_headers-devel`. The state file of
mbedded-cbor keeps a wrong assumption: a Tumbleweed image had no Boost
headers, and the build of plan r18 stopped.

Evidence: `mbedded-cbor/containers/tumbleweed.Containerfile:3`.
`Claude-state/mbedded-cbor.md:326-328`.

Fix: name `libboost_headers-devel`. Not checked: whether `boost-devel`
exists in Tumbleweed. Not checked again: `libboost-dev` on Ubuntu 26.10
(section 1 used it).

### 15. The plan does not say where the code of the wire stands and how it links

Fault: if `wire.hpp` holds the bodies and `konsens` links
`mbedded-cbor` as `PUBLIC`, every test and example includes the headers
of mbedded-cbor. They also get its `INTERFACE` definitions
`_GLIBCXX_ASSERTIONS` and `_LIBCPP_HARDENING_MODE`. This changes the
Release build of every target, and later measurements of
`counter_stress` with it. It also shows the dependency to every
includer.

Evidence: `mbedded-cbor/CMakeLists.txt:7`, `:12`.
`mbedded-konsens/CMakeLists.txt:14-18`.

Fix: `include/konsens/wire.hpp` declares `encode` and `decode`.
`src/wire.cpp` defines them. `konsens` links `mbedded-cbor` as
`PRIVATE`. `CBOR_INSTALL` is already off under `add_subdirectory`
(`mbedded-cbor/CMakeLists.txt:14`); setting it does no harm.

### 16. The compilers in the plan are below the floor of mbedded-cbor

Fault: the README of mbedded-cbor states g++ 16 or clang 23. The plan
builds with clang 22. Section 1 checked clang 22 with one test file
only. The plan also stops the build on the host (g++ 13.3). The build
directories on the host (`build`, `build-a` to `build-d`,
`build-stress`, `build-stress-asan`) then stop too.

Evidence: `mbedded-cbor/README.md:64`. Plan, section 3, lines 68-69.
`g++ --version` on the host: 13.3.0.

Fix: the plan says that clang 22 is below the stated floor and that the
full build in step 1 decides. The README and the state file of konsens
say that the build needs the container.

### 17. `--cbor` in `counter_stress` needs more to show something

Fault: (a) The parser of the options takes only flags with a value.
(b) The plan does not say where the bytes stand. If `send` encodes and
decodes at once, a duplicate is a copy of the decoded value, and the
channel never holds bytes. (c) Seeds that pass do not show that every
kind went through the wire. (d) A run with `--cbor` and a run without
it are not compared.

Evidence: `examples/counter_stress.cpp:92-100`, `:116-121` (the flags
and the usage text), `:227-243` (`send` and the duplicate),
`:333-336` (the delivery).

Fix: (a) `--cbor` takes the form of `--no-reconnected`; the usage text
names it. (b) The channels hold the bytes. `send` encodes. The delivery
decodes. A duplicate is a copy of the bytes. A failure of `decode`
ends the run through `fail`. (c) The run counts the messages of each
kind and prints the count. (d) The draws of the random generator do
not depend on `--cbor`, so for each seed from 1 to 300 the output with
`--cbor` is the same as the output without it. The plan checks this.
(e) A ctest entry `counter_stress_cbor`.

### 18. Ballot leader election does not check the sender

Fault: when the messages come from bytes, any peer can send a
`heartbeat_reply`. `sequence_paxos::handle` drops a message from a pid
that is not a peer. `ballot_leader_election::handle` checks neither
`from` nor `to`. This is not a fault of memory. It is a gap that the
wire makes reachable.

Evidence: `include/konsens/sequence_paxos.hpp:244`.
`include/konsens/ballot_leader_election.hpp:47-54`.

Fix: section 5 names it as open work. It is not in this plan.

## Questions for the owner

1. Finding 2: copy the body once, as provisional (proposal), or a
   decode without `ref` in mbedded-cbor, or show `cbor::ref`.
2. Finding 10: `std::errc::bad_message` for the checks of konsens
   (proposal), or a new enum of konsens.
3. Finding 12: one `kind` for both variants (proposal), or another
   name for the field `message`.
