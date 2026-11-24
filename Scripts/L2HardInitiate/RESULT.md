# BPLM_L2StreetBarFight -> hard initiate — 2026-11-22

`-run=VTGL2HardInitiate [-Apply]` (no flag = verify from disk). Backup in `Backup/`.

Before: `On Stage Begin -> Set Stage -> Run Hard Initiate -> Switch -> Set L2_CurrentStage ->
L2_Activate Level Manager`, and `Hard Initiate -> Switch(Get Stage) -> L2_Activate Level Manager`.
A normal arrival ran Activate three times (manual call, the chain itself, then the base class).

After:

```
On Stage Begin   -> Set Stage
Hard Initiate    -> Switch(Get Active Stage)
                      Stage_BarStreet_L2 -> Set L2_CurrentStage -> L2_Activate Level Manager
                      Default            -> Print String (warning)
```

Removed 3 nodes (Run Hard Initiate, the stage-begin Switch, the duplicate Activate call).
Blueprint compiled 0 errors 0 warnings; verified from disk in a separate process.

Not playtested.
