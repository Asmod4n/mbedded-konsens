# Plan: the selection of results walks each open entry once

Source: the owner, 2026-10-31 04:59 ("du suchst die Ursachen").
Measurement: bench/results/2026-10-31-051500Z-konsensd-callgrind-execute-verify.txt.
Base: one-transaction-per-round 86ea5da. Branch: linear-selection.

## 1. What is measured

On the leader at 64 in flight, the selection of results is 26.8 % of
the instructions (select_results 15.3 %, count_answer 4.7 %,
find_the_done_records 3.9 %, move_pointer 2.9 %). compute_result_crc
has 95 calls per write. `holds` and `counts` (src/selection.cpp) each
walk from `pointer_ + 1` to the entry, and `select_results` and
`find_the_done_records` call them for each open entry. So one call of
`select_results` costs the square of the open entries. Each
`selected_result` holds a map with one CRC for each request below it,
so its memory also grows with that square.

## 2. Part A: the CRC of a counted result is computed once

`count_answer` computes `compute_result_crc(index, result)` of each
result once, when it stores the result. The stored value sits beside
the result in `counted_answer`. `counts`, `holds`, `count_answer` and
`move_pointer` read that value and do not compute it again. The CRC
that the sender wrote into `indexed_result.crc` stays as it is: it
goes into the done record as `result_crc`, and each follower checks it
there (node.hpp check_the_done_record, crc_mismatch). This part
changes no verdict.

## 3. Part B: the base of a selected result is one chain value

`selected_result.base` (a map from index to CRC) becomes
`std::uint32_t base_crc`: the chain of `extend_base_crc` over each
request k with pointer_ < k < q, in ascending order, with the CRC of
the result selected for k, from 0. This is the chain that an answer
carries in `answer.base_crc`, and that `counts` already builds for
the indexes below `from_idx` (src/selection.cpp counts,
node.hpp answer_from). The chain names each index, so two bases with
different sets of requests give different chains, except by a
collision of CRC-32C. The map compared one CRC-32C for each index, so
the strength of the comparison is that of the CRC that the answers
already use.

`holds(q)` becomes: q is selected, and its `base_crc` is equal to the
chain of the current selections below q.

`move_pointer(p, chosen)` walks `selected_` once in ascending order.
It builds the chain of the chosen results up to p, and then extends it
with the current selections above p. A selection at q > p stays when
its `base_crc` is equal to that chain at q; its new `base_crc` is the
chain of the selections from p + 1 to q - 1, from 0. Every other
selection above p is dropped. The old form dropped a selection only
when one of its base CRCs at or below p differed from the chosen one;
a difference above p came out later in `holds`, and the selection was
then made again. In the new form such a selection is dropped at once
and is made again in the next `select_results`. The result of a
selection is the same.

## 4. Part C: one walk for each call

`select_results` and `find_the_done_records` walk q from
`pointer_ + 1` upwards and stop at the first request that is not held.
Each keeps, during its walk, the chain of the selections below q, and
for each node the state of `counts`: the chain of the selected
results below `from_idx` of its answer, and whether each of its
results from `from_idx` to q - 1 is equal to the selected one. One
type holds that state, `count_walk`, and it is the one form of the
rule of `counts`. `find_the_counted_nodes` for one index
(find_the_nodes_out, find_the_rejected_record) runs the same walk from
`pointer_ + 1`, as `counts` does now. So the rule exists once, and
the walks of `select_results` and `find_the_done_records` cost one
step for each open entry and node.

## 5. What does not change

- The messages and their encoding on the wire.
- `indexed_result`, `answer`, `work`, `entry`.
- The verdicts of the selection for every input, except where two
  different bases give the same CRC-32C chain.
- The public interface of `selection`, except the type of
  `selected_result::base`. `get_selected()` stays.

## 6. Tests

- test_execute_verify: every test of the selection runs unchanged.
- New test: after `move_pointer`, a selection above the pointer stays
  when the chosen results below it are equal to its base, and is
  dropped when one of them differs.
- New test: a selection whose base has one request less than the
  current selections is not held.
- test_node, test_simulation (the selection runs in every write).

## 7. Measurement

- callgrind of the three nodes, bench-logs/vg13.sh, 4096 writes at 64
  in flight: instructions per write on the leader, and the share of
  the selection, before (86ea5da) and after.
- The bed run of arms3.sh: the base, its copy for A/A, and the arm,
  4 rounds, 1 and 64 in flight, g++ 16. Medians compared.
- One result file for each run, with its context.

## Data that the plan leaves on the disk

The worktree of the branch (made again by git worktree), the build
directory (made again by a build command), the binaries of the arms in
bench-logs/bin (deleted after the result files), the callgrind files
under /mnt/zrun/b/vg13 and vg14 (deleted after the result files).
