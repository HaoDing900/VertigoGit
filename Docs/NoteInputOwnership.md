# Note and dialogue input ownership

`UNarrativeUIInputComponent` lives on each local player controller. Non-freemove
Narrative dialogues acquire it at `OnBeginDialogue` and release it at
`OnEndDialogue`. The project Note root, `WB_NoteMainWidget`, derives from
`UVTGNoteInputWidget`. `OnStateChanged_Event` in that project widget calls
`SetNoteInputActive(State != Closed)`. Construct acquires the initial request;
Destruct is only a fallback for forced removal. Logical close must release even
when a closing animation or cached Slate widget keeps the root alive.

An open Note has priority 100; dialogues have priority 0. The latest request wins
within the same priority. Opening the Note before the dialogue ends is safe:
ending the dialogue releases only that dialogue's request. The Note keeps UI-only
input, cursor, keyboard focus, and the movement/look lock. When the last request
ends, gameplay input returns. Independent cinematic ignore-input counts remain
balanced; the component never resets the controller's input-ignore counters.

`PC_Game_VTG` derives from `AVTGPlayerController`. Its input-mode entry point
applies the requested mode, then rejects any mode that re-enables viewport game
input while a UI owner is active. The owning component immediately restores
UI-only input and the winning owner's focus. Genuine UI-only requests, such as
pause/history overlays, retain their own focus. `ShouldShowMouseCursor` respects
UI ownership even if legacy Blueprint code later writes the cursor flag false.
This uses the viewport's actual input behavior, not debug-only mode names, so it
also works in Shipping.

This covers both InventorySystemX's trailing input changes and BP_Player_Sa's
mail-camera cleanup: that flow waits 0.8 seconds before calling Sa_RestoreInput,
which previously overrode the Note long after its Construct/next-tick callback.
There is no fixed-duration delay, per-frame polling, or scheduled focus reset in
the runtime fix. Note logical state and destruction drive ownership.

New story Notes using the existing root need no per-level input-mode nodes.
Other modal UI can use the component's `Acquire`/`Release` API with its own owner
and focus widget. Release on logical close, with destruction as a fallback.
Free-movement dialogues retain their existing input behavior. A follow-up
dialogue can acquire its request before or after Note release; only the final
request releases gameplay input, so no handoff delay is necessary.

The Notes plugin's assets and the Sewer dialogue's story connections are
unchanged. The project widget reparent migration is recorded in
`Scripts/MailDialogueDiagnosis/apply_note_input.py`. Controller migration is in
`apply_controller_input_guard.py`. Original asset backups are under
`Saved/Backups/NoteInputOwnership/`.
`-run=VTGNoteStateInput` installs the project widget's logical-state connection;
`-run=VTGNoteStateInput -VerifyOnly` checks the saved wiring without modifying it.
The Closed enum value is read from the Notes plugin's actual RemoveBaseWidget
route instead of being hardcoded.

## Sewer story continuation callback

The parent BPLM dispatches ` OnStoryNoteFinished(NoteID)` (the existing function
name has a leading space). Sewer must override that exact parent function. A
plain custom event named `CustomEvent` is not an override, even if its output is
wired into the correct NoteID switch and downstream close events.

`-run=VTGNoteCallback` restores the lost Sewer override while preserving its exec
and NoteID connections. `-VerifyOnly` validates the saved asset. The commandlet
tests real generated Blueprint dispatch on a transient copy: Police and Overdue
IDs must reach their respective continuation events; an unknown ID must not.
Only the transient test copy replaces continuation bodies with native markers,
so tests do not launch cinematic/story side effects or change saved event bodies.

Validation: `Vertigo.Dialogue.NoteInputOwnership` checks real dialogue begin/end,
Note-to-dialogue handoff, repeated acquisition/release, overlapping modals,
independent cinematic locks, and the saved Note root's parent class.
`Vertigo.Dialogue.NoteInputLifecycle` runs actual Note viewport construction,
keyboard focus, removal, late GameOnly/GameAndUI overrides after 0.8 seconds, actual
viewport input suppression, logical closure while the root remains in the
viewport, cached reopening without Construct, and dialogue handoff in PIE. Also run
`Vertigo.Dialogue.CinematicRegression` when changing dialogue lifecycle behavior.

## Mail row navigation

`VTGMailRowJump` extends the Blueprint function-node context menu for
`BP_Player_Sa.SetMailData` and `ForceReadMail`. Right-click the call node or its
MailID pin and choose `Jump to Mail DataTable Row`. The action opens
`DT_Mail_ID_FillHere` and selects the literal MailID using Unreal's DataTable row
selection API. Connected, empty, None, or nonexistent IDs cannot navigate.
`Vertigo.Editor.MailRowJump` checks both functions, actual Sewer call sites,
generated context menus, and connected-pin handling.
