# Clinic / ShoppingCenter_Yard dialogue fixes

## Changes

- Yard: the three authored `Set Idle Anim For ABP` calls now synchronously exit the cinematic branch and evaluate the skeletal pose before the next camera cut. The existing idle assets and actor placement remain authored in the level manager.
- Camera Tick restoration now lives in Narrative itself. Before dialogue-began callbacks or Blueprint camera events run, Narrative records the original view target and its actor Tick state. After dialogue Sequence teardown it restores exactly that state, including an initially disabled Tick. Direct dialogue replacements inherit the original return camera instead of the outgoing cinematic shot. Restoration is synchronous, with no late Tick timer. The earlier Clinic-only event has been removed from node `L2_Street_NPC_DialogueNode_NPC_9`.
- Clinic exam button: `L5Clinic_Still_DialogueNode_NPC_232` now enables the manager's `ISXColi_L5Button` and ensures `Q_L5Clinic` state `QuestState_11`. The previous `QuestState_10` does not exist in the authored quest; state 11 owns `QuestBranch_78`. The existing ensure-state event preserves its no-rollback behavior.
- Player montage cleanup: Start and Interrupt NE calls record actual player montage instance IDs. At dialogue completion, wait 0.75 seconds then blend out for 0.25 seconds. Insert restoration is released before cleanup so an old dialogue idle cannot be resurrected. A newer instance of the same montage is protected; a new dialogue montage takes ownership and cancels the old timer. NPC montages keep their previous behavior.
- Side dialogue: `Line_Side` and `DialogueBox_Side` share `SideDialogueContainer`, an auto-sized Overlay, with text-driven height, 352 wrapping width and 24 horizontal / 16 vertical padding. The image uses the existing brush. Text/background visibility bindings keep their existing widget names.

## Validation commands

Build UE 5.3 `VertigoEditor Win64 Development`, then use `UnrealEditor-Cmd.exe Vertigo.uproject` with:

- `-run=VTGClinicFix -VerifyOnly -unattended -nullrhi -nosound -nop4`: reload and verify authored assets.
- `-run=VTGClinicFix -RuntimeTest -unattended -nullrhi -nosound -nop4`: real player animation and Blueprint NE checks, button/quest/camera integration checks in a transient world.
- `-run=VTGClinicFix -Preview -AllowCommandletRendering -unattended -nosound -nop4`: short Chinese, long Chinese, long English rendering and text/background geometry assertions; outputs `Scripts/ClinicFix/side-preview-*.png`.
- `-run=VTGNarrativeInsert -Test -unattended -nullrhi -nosound -nop4`: existing NPC insert/restore and Instant behavior regression suite.

Migration is explicit and idempotent: `-run=VTGClinicFix` applies the authored changes, and `-run=VTGNarrativeInsert -InstantEvents` reconstructs the montage playback nodes. Original Clinic/Yard/Overlay resources are copied to `Scripts/ClinicFix/Backup` before saving. No migration runs at editor startup.

Full-map PIE playback remains necessary to judge the exact Yard sequence-to-idle visual transition and lobby camera behavior along the complete authored story path. Commandlet checks do not substitute for that visual playthrough.

## Sa_Cine ABP follow-up

`BP_Cine_Sa` and `BP_StillNPC_Sa` both use `/Game/Characters/Sa/ABP_SaCine`.
Its cinematic `FullBodySlot` previously had no source pose. When the Sequence stopped driving the slot while `IsCutscene` was still true, its body pose evaluated to the reference pose, including the same-frame transition back to gameplay.

The slot now uses `Cache_BasePosture` as its continuously updated source. Both assignments to `BaseIdleAnm` (animation initialization and `SetBaseIdle`) reject invalid inputs, preserving the current idle. The class has an explicit valid standing idle default. Additive slots retain their identity/additive inputs.

The before/after skeletal evaluation test reproduced a zero body-rotation difference from the reference pose before the fix, and a valid authored stair idle after the fix, including the exit frame and the following 12 frames. Logs: `Scripts/SaCinePose/before.log`, `after-test.log`. Re-run with `-run=VTGSaCinePose -Test`; inspect the asset with `-VerifyOnly`. Explicit migration uses `-run=VTGSaCinePose`, with the original asset backed up in `Scripts/SaCinePose/Backup`.

Generic camera checks (`Scripts/SaCinePose/camera-test2.log`) pass for originally enabled/disabled Tick, zero/nonzero camera blend, direct replacement, repeated teardown and destroyed cameras. These supersede the original Clinic-specific camera checks below.

## Clinic button consumption and Ulises reminder loop

`L5_Button` used to set `L5_HadPressedButton=true`, play the existing free-move reaction, and call `L5_ShowUlisesInteractionIcon`. That last event reset the flag to false. Consequently the next Ulises interaction requested NPC_232 again, whose event re-enabled the same button.

- Removed the flag reset from `L5_ShowUlisesInteractionIcon`.
- Added an explicit already-pressed guard to `L5_Button`. The first call sets the existing progress flag, destroys `ISXColi_L5Button`, clears the manager reference, and continues the existing reaction/icon flow. Duplicate calls have no effect, including when the reaction dialogue is rejected.
- Removed the post-button conversation's obsolete `DisableInteract` call on the consumed button. Its placement, delay, NPC_233 dialogue and animation setup remain authored as before.
- Reaching NPC_232 now records that Ulises has requested the button. Its enable event skips already-pressed or invalid button actors. Before pressing, repeated Ulises interactions keep requesting NPC_232; after pressing they request NPC_233.

Build, resource reload verification and actual BPLM branch tests passed. Tests cover three reminder requests, consumption/destruction, duplicate callbacks, icon refresh, optional reaction rejection and progress serialization. The test Narrative component records `BeginDialogue` requests instead of playing the downstream cinematics; complete Clinic PIE playback is still a separate visual check.

Commands: `-run=VTGClinicButtonFlow` applies the idempotent migration; `-VerifyOnly` reloads/checks it; `-Test` executes the runtime routing cases. Logs and the original manager backup are under `Scripts/ClinicButtonFlow/`.

Final routing run: `Scripts/ClinicButtonFlow/test-final.log`, commandlet result 0 and process exit 0. Resource reload check: `Scripts/ClinicButtonFlow/verify.log`, exit 0.

## Clinic room player placement

`L5_ClinicRoom_Loc_SaUlises` now places Ulises first, then calls `PlaceClinicPlayer` at the existing authored player location. The player faces Ulises on the horizontal plane (about 218.91 degrees for the current coordinates), instead of retaining a fixed angle. The latest authored 198-degree angle is retained as the fallback when the target is invalid.

Placement stops movement and consumes pending movement input. A transient component snapshots automatic/controller rotation modes and temporarily disables them. It acquires its own request on Narrative's shared UI input component, then releases that request on the tick after a blocking dialogue begins, allowing Narrative to acquire its own input request first. The original rotation modes are restored; controller-facing modes are synchronized to the final scene yaw. A three-second deadline releases the request if no blocking dialogue starts. Repeated placement does not stack input locks or replace the original snapshot.

`-run=VTGClinicPlacement` applies the explicit, idempotent asset migration; `-VerifyOnly` checks the saved graph; `-Test` exercises the actual BPLM placement with a player/controller and simulated Narrative input handoff. Checks cover four starting directions/mode configurations, residual velocity/input, 65 held-input frames per case, overlapping/manual input locks, failed dialogue handoff, and repeated placement. This is not a full-map cinematic playthrough.

Logs and the immediate pre-migration manager backup are in `Scripts/ClinicFacingDiagnosis/`. Because Unreal Editor was open and holding the normal project DLLs, validation used separately named module DLLs. Restart the editor and load the saved manager from disk before testing the complete story path.

Validation passed: `build-final.log`, `test.log` (four placement configurations and repeated-placement fail-safe, exit 0), `verify.log` (fresh package reload, exit 0), and `button-regression.log` (NPC_232/button/NPC_233 routing preserved, exit 0).

After the editor was closed, a normal unsuffixed DLL build completed (`build-standard.log`). The editor's final save had restored its older placement graph, so that exact saved package was backed up as `Backup/BPLM_Clinic.after-editor-close.uasset` and the scoped placement migration was reapplied. The regular-DLL runtime placement suite passed again (`test-standard.log`, exit 0). The module manifest now points to `UnrealEditor-Vertigo.dll` and `UnrealEditor-VertigoEditor.dll`.

The button-flow verifier now checks the function identity before treating a reused node name as the old `DisableInteract` call. Final normal build and button resource verification passed (`build-standard-final.log`, `button-verify-standard-final.log`, exit 0).

## Results (2026-10-25)

- UE 5.3 Editor build passed.
- Fresh-load resource verification passed (`Scripts/ClinicFix/verify-final.log`).
- All nine runtime cases passed: Start grace/blend timing, same-resource gameplay instance protection, insert cleanup, consecutive dialogue ownership, actual Start NE, actual Interrupt NE, actual button plus quest state/branch activation, actual fixed camera Tick recovery, and replacement-dialogue camera protection (`Scripts/ClinicFix/integration-complete.log`).
- Existing insert/restore regression checks passed (`Scripts/ClinicFix/insert-regression.log`).
- Three rendered side dialogue cases passed geometry assertions and visual inspection (`Scripts/ClinicFix/preview2.log`, `side-preview-0.png` through `side-preview-2.png`).
