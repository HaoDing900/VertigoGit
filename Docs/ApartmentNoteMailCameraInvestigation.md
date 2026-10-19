# Apartment Note → Mail camera investigation (2026-10-18, read-only)

## Confirmed saved graph

- L3_SaApartmentD1 NPC_1 bound event → GetActorOfClass(BPLM_SaApartment) → OpenNote_L3UlisesHistory. Bound event remains START-only (default unchanged).
- OpenNote_L3UlisesHistory → OpenStoryNote(PDA_L3_Apt_UlisesHistoryMessage, NoteID=L3UlisesHistory).
- Parent BPLM requests mail-reading hold, exits dialogue, then waits 0.3 s before opening Note.
- OnStoryNoteFinished(NoteID=L3UlisesHistory) → L3_ReadMailIfCloseNote → Delay 0.3 s → BeginDialogue(StartFromID=L3_SaApartmentD1_Mail).
- Mail cleanup in BP_Player_Sa waits 0.2 s plus 0.8 s, enumerates BP_0FixedCamera actors, selects Activated camera, then if CanRestoreMailViewTarget returns true blends to FoundActiveCamera for 1 s. Otherwise restores player or skips the view-target change.
- Existing CanRestoreMailViewTarget only compares Controller.GetViewTarget() == Reader (the player Actor). It does not distinguish two different camera components on that Actor or old and new mail sessions.

## Remaining risks, not runtime-proven causes

- Note logical close can release a deferred old mail cleanup while the Note completion callback starts a new mail dialogue 0.3 s later. Old cleanup may run after the new session starts.
- The Actor-level ownership guard protects a later external lock-camera Actor (Sewer case) but cannot identify a newer terminal camera on the same Sa Actor. Thus a delayed old cleanup can still pass the guard and choose an Activated fixed camera.
- Enumerating Activated fixed cameras is not the same as restoring the camera that actually owned the view before this particular mail session. More than one Activated camera creates ambiguous selection.
- Late camera-component deactivation may affect a newer session even if SetViewTarget itself is guarded. Cancellation must cover all delayed cleanup side effects, not just the final blend.
- Narrative also restores its captured OldViewTarget at dialogue end when DialogueBlendOutTime > 0, potentially overlapping another handoff.

## Recommended design before changing behavior

1. Give each mail-reading session a generation/token. Every delayed cleanup step validates it; starting a newer session invalidates the old cleanup. Keep same-session Note holding intact.
2. Record the actual return view target at mail start, rather than selecting an arbitrary Activated camera at cleanup. Define explicit fallback if the saved camera is destroyed or a cinematic has taken ownership.
3. Use a single camera ownership/handoff path covering view target and active camera components. Actor equality alone is insufficient.
4. Record runtime timestamps before implementing: session token, Note close, mail finish, next BeginDialogue, current/pending view target, active MailCamera/FollowCamera, selected fixed camera, and each blend initiator.

No camera code or story asset was changed in this investigation. These are candidate races derived from saved graphs; a runtime trace is still required to identify the exact winning writer in the reported scene.
