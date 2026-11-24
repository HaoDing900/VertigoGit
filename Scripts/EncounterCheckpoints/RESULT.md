# L2 street and sewer boss encounter checkpoints — 2026-10-24

Both BPLMs already inherited VTGLevelManagerBase and therefore participated in value-only snapshots. Their old restart routing relied on a persistent L2 phase or earlier sewer overlap checkpoints rather than an explicit, post-restore encounter rebuild.

## New behavior

- L2: replace the existing post-intro AutoSave with SaveEncounterCheckpoint(L2StreetFightStart, VTG_ResumeL2StreetFight, player transform, order 100), after persistent phase 1 is set and before combat activation. Mark the intro finished before capturing BPLM values. Retry restores lighting, NPC scene, arena collision and the existing combat setup, skipping the intro and opening bark. The existing L2FightState completion component selects VTG_ResumeAfterL2StreetFight for subsequent saves. Its pending post-fight dialogue flag remains authoritative; completed dialogue does not replay. A shared resume is already inside OnSlotLoaded, so the component must finish immediately rather than wait for a second broadcast.
- Sewer: StartBossEncounter creates SewerBossFightStart at order 100 before damage/AI activation. Repeated trigger requests do not resave. AI waits while world restoration is pending. Retry restores the arena fence and interaction state, then starts the fresh boss after player/inventory/Narrative restoration. Egg count and counted IDs are retained from the snapshot. Boss death selects VTG_ResumeAfterSewerBossFight for later saves; this entry hides/disables the fresh boss and reconstructs switch and round-door state without replaying the victory dialogue. Two BPLM flags record door opened/passed state.
- Existing SewerStart / SewerLairDrop checkpoints and legacy L2 phase routing remain available for older saves. The order-100 fight checkpoint supersedes the earlier sewer checkpoints. Old saves cannot recover variables that were never saved.
- Victory changes the in-memory rebuild entry for future saves; it does not immediately overwrite the pre-fight autosave. Death retry restarts the encounter from its boundary, rather than preserving partial enemy damage.

## Saved data

The existing coordinator saves safe BPLM bool/int/float/name/string/enum, reflected value structs, and supported value arrays/sets/maps. Object/actor/component references, delegates, latent actions and DoOnce internals are deliberately rebuilt. Common parent camera/input caches remain excluded. Player/inventory/quest restoration continues through the existing coordinator.

## Verification

Development Editor build passed. Apply and saved-asset verification compile both BPLMs with zero errors/warnings. New tests use SaveGameToMemory / LoadGameFromMemory, compare every captured BPLM value/type, restore before BeginPlay, dispatch actual OnSlotLoaded and call the real authored rebuild paths. They cover stage-intro suppression, L2 enemy placement/combat flag/arena collision, one-shot resume, victory/client state/saved player transform, sewer AI deferral and activation, saved egg progress, death routing, boss suppression and open/closed door state. No user save slots are written.

The isolated L2 fixture bypasses inventory ToggleCombatMode and tutorial widget creation; it does not replace a rendered full PIE playthrough. NPC animation initialization can log missing visual Outline cache warnings in that fixture. New checkpoint tests do not alter player controls or gameplay graph nodes in the saved assets for those fixture accommodations.

Backups: Scripts/EncounterCheckpoints/Backup/BPLM_L2StreetBarFight.uasset and BPLM_SewerUnderApt.uasset. Map files are unchanged.

Commands:

```
UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBlackMarketFollowup -EncounterVerify -unattended -nullrhi -nosound
UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGSewerEncounter -unattended -nullrhi -nosound
```

Final results: apply and saved-asset verification exited 0; original VTGSewerEncounter regression also exited 0, retaining boss start/death gating and egg kill attribution/deduplication checks. Both saved BPLMs compile with zero errors and zero warnings.
