# AcceptWithResult, stage 2 of the code

Model change: the work carries the entries of the taken log from the decided index of the node up to d (AnswerWork, switch WorkWithoutBase); a node runs each decided entry again and goes down when its result differs (HandleDecide, mutation NoCheckAtDecide). TLC 2.19 BFS, -deadlock. Model commit: the commit of this file.

## MC_aw_stage1_l1.cfg
```
Model checking completed. No error has been found.
11085229 states generated, 2926586 distinct states found, 0 states left on queue.
The depth of the complete state graph search is 38.
Finished in 02min 16s at (2026-10-31 09:57:53)
```
## MC_aw_stage1_c0_r1.cfg
```
Model checking completed. No error has been found.
5369725 states generated, 1345267 distinct states found, 0 states left on queue.
The depth of the complete state graph search is 42.
Finished in 01min 48s at (2026-10-31 09:57:25)
```
## MC_aw_stage1_recovery_l1.cfg
```
Model checking completed. No error has been found.
45493709 states generated, 10531857 distinct states found, 0 states left on queue.
The depth of the complete state graph search is 41.
Finished in 04min 32s at (2026-10-31 10:02:26)
```
