# NPC Sequence to idle pose recovery

Applied on 2026-10-25 to these six animation blueprints:

| Blueprint | Default idle when no valid idle has been supplied |
| --- | --- |
| `/Game/Characters/ABP_NPCBasic` | `MM_Idle` (compatible UE5 enemy mannequin skeleton) |
| `/Game/Characters/Ulises/ABP_Ulises` | `ANM_Ulises_Sit_Default` |
| `/Game/Characters/BowerBird/ABP_Bowerbird_StillNPC` | `ANM_Bowerbird_Stand_LookAt_02` |
| `/Game/Characters/IPHA-P/ABP_NPC_IPHAP` | Existing `IPHA-Philip_DefaultStand` |
| `/Game/Characters/NPC/Bello/ABP_Bello_NPC` | `ANM_Bello_Stand_Idle_still1` |
| `/Game/Characters/NPC/Fell/ABP_Fell_BasicEnmSkeleton` | `MM_Idle` (compatible UE5 enemy mannequin skeleton) |

Each cinematic body slot chain now receives `Cache_BasePosture` at its previously empty source. Slots with connected sources keep those sources updating while a Sequence/montage overrides them. This lets the underlying pose remain available when the override releases, including before the cutscene flag changes. Face/additive branches and authored slot names are preserved.

Both initialization and `SetBaseIdle` reject null animation inputs. An invalid input preserves the previous valid animation and continues any later initialization steps. A valid runtime idle still replaces the default. Authored looping settings and initial cutscene flags are preserved.

The editor migration is `-run=VTGNPCSequenceIdle`; `-VerifyOnly` checks the saved packages without modifying them. Original assets were backed up before modification under `Scripts/NPCSequenceIdle/Backup/`.

## Validation

- Editor module build passed (`Scripts/NPCSequenceIdle/build.log`).
- Before-change probe reproduced reference poses in all six blueprints (`probe.log`).
- All six saved packages compiled and passed fresh-process reload verification (`apply.log`, `verify.log`).
- Runtime regression passed with zero failed checks (`test.log`, process exit 0). It instantiates actual character blueprints with the target ABP and evaluates real Level Sequence skeletal animation tracks, testing manual stop and natural completion. Both exit paths retain a non-reference body pose on the exit frame, on the cutscene flag change, and over the following 12 frames. Checks also cover default initialization, empty cinematic slots, null idle requests and valid runtime idle replacement.

These are commandlet tests in a transient world, not a full-map PIE visual playthrough. Project loading still reports existing unrelated asset warnings.

## May yard follow-up

`BP_Cine_May` uses `/Game/Characters/May/ABP_NPC_May`, a separate ABP from Sa and the original six targets. It still had an empty source below the cinematic `FullBodySlot`/`DefaultSlot` chain, an empty default `Base Idle Anm`, and two unguarded idle writes. The yard already requested `ANM_LS_L5_MaySitLookingUp` and refreshed May's pose, but that did not fix the missing underlying pose while the cinematic branch still contributed to the output.

The same fallback, source-update and null-write guards were applied to May. The default fallback uses the yard's compatible `ANM_LS_L5_MaySitLookingUp`; valid authored idle changes continue to override it. Facial/jaw processing remains connected. The original May asset is backed up in `Scripts/NPCSequenceIdle/Backup/ABP_NPC_May.uasset`.

May is now included in `VTGNPCSequenceIdle`; pass `-MayOnly` to restrict migration, verification or tests to May. Logs are in `Scripts/MayYardPose/`. The before-change probe reproduced the reference pose. Runtime tests passed both Sequence exit paths and additionally spawned the actual yard BPLM, assigned May/Sa/drone references, and called `L5_Stairs_PositionNIdle`: May received the authored stair idle and retained a non-reference body pose at the synchronous refresh and over the following 13 frames. This remains a transient-world test, not full-map PIE playback.
