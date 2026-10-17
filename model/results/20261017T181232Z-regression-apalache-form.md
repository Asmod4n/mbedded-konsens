# Regression of the model in the Apalache form, 20261017T181232Z

The model was changed for Apalache: one record type for all messages,
closed forms in place of RECURSIVE operators, type annotations. This
check runs three complete TLC runs of the changed model and compares
each count with the run of the model before the change.

Tool: TLC 2.19 (tla2tools.jar in the scratchpad), OpenJDK 21.0.12.1,
3 workers, breadth-first search, `-deadlock`. Machine: 4 processors.

| Config | Before (run) | After: generated / distinct / depth | Start, end (UTC) | Same |
|---|---|---|---|---|
| MC_base_monotone.cfg | 13489555 (20261017T121020Z) | 46061842 / 13489555 / 35 | 20261017T164116Z, 20261017T171802Z | yes |
| MC_f.cfg | 5256014 (20261017T104327Z) | 24984559 / 5256014 / 42 | 20261017T171820Z, 20261017T173903Z | yes |
| MC_e.cfg | 12810589 (20261017T114843Z) | 45092947 / 12810589 / 35 | 20261017T173903Z, 20261017T181206Z | yes |

No property was violated. The change keeps the states of these three
configurations.
