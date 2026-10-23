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

## 2026-10-18 scoped camera trial (after checkpoint c27797f95)

Implemented only for mail cleanup that was actually deferred by an open story
Note (`DeferMailReadingFinish` with live holds). Creating/opening an unrelated Note
alone does not enable the guard. Ordinary mail keeps the original Blueprint
Delay nodes and Activated fixed-camera return rule.

- `ReadMailSafe` notifies the reading generation. This does not move cameras.
- The three delayed terminal-cleanup continuations (0.2 s, 0.8 s, 0.8 s) use
  generation-capturing timers only in a protected handoff. Each timer captures
  its own generation, so a newer invocation cannot overwrite an old BP token.
- A newer reading invalidates old pending continuations, including late camera
  component deactivation and final input restore, not only SetViewTarget.
- Apartment's 0.3 s Note-close continuation notifies only after BeginDialogue
  returns true. Failed dialogue start leaves the old cleanup intact.
- That specific successful Note-to-mail dialogue also restores `IsReadingMail`
  so its own On Finish Reading Mail will run; it bypasses ReadMailSafe, and the
  previous cleanup had already cleared that flag. This compensation is guarded
  by the protected-flow flag and does not affect ordinary dialogue.
- The final valid continuation leaves protected mode; later ordinary mail uses
  the original delays again. Component EndPlay clears scheduled timers.

Changed story assets: BP_Player_Sa and BPLM_SaApartment only. No change to fixed
camera activation, return-camera selection, Narrative event start/end timing,
Sequence camera rules, or global camera arbitration. Backups are in
`Saved/Backups/NoteMailCameraSession/`. `VTGMailFlow -FixNoteMailCamera` applies
and `-VerifyNoteMailCamera` checks saved wiring. The pre-trial Git checkpoint is
`c27797f95`; this trial is not automatically pushed.

Manual acceptance: replay NPC_1 -> Ulises Note -> close -> mail dialogue. Watch
whether the terminal camera remains stable through the new dialogue and returns
normally at its end. Also test ordinary mail close and Sewer Police Note ->
stairs. The trial does not change pre-delay close animation/state work already
executed before a newer reading begins; full visual timing remains a playtest.

## 2026-10-19 original-camera return for Note-linked mail

- Capture the controller's actual view target and Sa's active camera components at `BlendToMailCam`, before the terminal blend. This distinguishes an external Fixed/Lock camera from third-person camera components on Sa without guessing from stale `Activated` flags.
- Preserve that snapshot across the protected mail -> Note -> mail lifetime. Repeated terminal entry cannot overwrite it while `MailCamAlreadyIn?` is true.
- Only a mail finish previously deferred by a story Note uses this return. Ordinary mail retains its existing return rules. The player BP bypasses the Activated-camera result for a protected return, then runs normal terminal cleanup.
- An external current OR pending view target is a later camera takeover and is not replaced. Controller replacement is also rejected. A destroyed original target falls back to Sa.
- Camera return does not call Lock/Unlock, Enable/Disable, or modify FixedCameraLevel / direction flags / input lock counters. Existing mode state remains in charge. Sa camera component selection is restored for a same-actor return and at the protected final continuation when still owned.
- Tests cover original external targets and third-person component selection, repeated Note entry, ordinary-mail isolation, independent input locks, and current/pending external takeovers. These checks do not replace visual PIE validation of the authored Apartment sequence.
- Backup immediately before this asset patch: `Saved/Backups/NoteMailOriginalCamera/BP_Player_Sa.uasset`. No automatic commit or push.
