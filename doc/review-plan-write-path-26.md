# Review of section 26 of the plan for the write path

Reviewer: a second agent, 2026-10-30, at b6bd3b8. Verdict: approve
after fixes 1-6.

1. Part A: the references stay valid, but the plan must name the two
   windows where they are read (node.hpp:1439-1445 and :1930-1931).
2. Part A changes a public header; test/test_execute_verify.cpp builds
   leader_log from std::vector<entry> at :139-286 and must change.
3. There is no test_selection. A test that compares the old form with
   the new one needs the old form in the tree. Use the adapted tests
   and add one with a gap in the indexes of the map.
4. src/selection.cpp:247 deduces std::reference_wrapper, and
   `e.rejected = true` does not compile; :255 and :277 need get().
5. The count of 140,161 copies is not measured: callgrind gives a call
   to the caller of an inlined function. Give the rows as read.
6. Part B: check_the_done_record decodes a record after the done
   records; apply_the_done_records takes no done_log; the done records
   are read twice per step and begin_the_base decodes again at each
   call. Decode over the whole range read; keep apply out of part B.
7. Part C: the lost-write decision was cited wrongly; the chain CRC is
   modelled, so the TLA+ claim does not cover part C.

The text of section 26 carries fixes 1-7.
