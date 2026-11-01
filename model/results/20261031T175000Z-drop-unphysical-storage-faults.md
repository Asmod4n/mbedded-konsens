# SequencePaxos without the storage faults missing_accepted, missing_promise, missing_decided

The meta record is one row with one CRC (persistent_storage.cpp), so one field of it is never lost alone; a damaged record is a CRC error, an earlier record is stale_meta. The three kinds are removed; 26 configs that used only them are deleted, 33 configs keep their other kinds. TLC 2.19, simulation, seed 7, depth 60, one worker each.

| Config | Limit | States | Result |
|---|---|---|---|
| MC_st_start_cover.sim | 5.5 min | - | NoRepairLimitReached violated, as a cover must be |
| MC_st_start_only_cover.sim | 5.5 min | - | NoRepairLimitReached violated, as a cover must be |
| MC_st_start.sim | 10 min | 5975795 states | no violation, stopped by the limit |
| MC_s_rollback.sim | 10 min | 7545821 states | no violation, stopped by the limit |
| MC_r_meta.sim | 10 min | 8389687 states | no violation, stopped by the limit |
