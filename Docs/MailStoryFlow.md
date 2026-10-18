# Mail, dialogue and story Note lifecycle

`BP_Player_Sa.On Finish Reading Mail` remains the single dialogue-finish entry point.
Its original body is now `FinishMailReadingCleanup`: it clears mail state, closes
the terminal animation, restores the camera and performs the existing input cleanup.

`BPLM.OpenStoryNote` calls `BeginStoryNoteMailReading` **before** attempting to end
the dialogue or waiting for the Note widget. `Keep Mail Reading During Story Note`
(`Mail | Story Note` in BPLM defaults/instances) controls this behavior and defaults
to true. A false setting or invalid Note asset does not acquire a hold.

`UVTGMailReadingComponent` on the reading pawn queues one cleanup continuation while
that hold exists. The Note root binds itself when logically opened and releases on
logical Closed, including cached roots. Destruction provides a forced-removal fallback.
The common BPLM story completion path also releases its hold before dispatching
` OnStoryNoteFinished`. Repeated close signals cannot run the queued continuation
twice. World teardown discards pending work. This component does not set input modes:
dialogue and Note input ownership remains in `UNarrativeUIInputComponent`.

Without a story Note hold, the original finish entry calls the cleanup immediately.
Keep future terminal/camera cleanup in that continuation rather than adding another
independent dialogue-ended listener that bypasses the hold.

## Player idle changes

`BP_Player_Sa` and `ABP_Sa` now implement `BPI_CineAnmInterface.SetBaseIdle`.
The player forwards to its mesh's animation instance. `ABP_Sa` writes `VTGBaseIdle`,
which feeds the sequence pin of the existing standing Idle state. Its default is
the previous `Sa_am_Stand_Idle_02` asset. Locomotion, other states and montage slots
continue to use the existing animation graph. Sewer's sitting and reset messages
can therefore reach the visible player without replacing its animation class.

## Mail IDs and editor navigation

Mail IDs are rows in `/Game/Narrative/Mail/DT_Mail_ID_FillHere`; story Note IDs are
separate identifiers. `L3_HintMailOverdue` uses `L3_Sewer_Overdue` (mail row), whose
dialogue opens Note `L3Overdue`. A nonexistent mail row previously selected
`MailDefault` through SetMailData's fallback.

`VTGMailRowJump` provides row navigation on Set Mail Data / Force Read Mail node
menus and directly in their Mail ID text fields. The custom pin preserves normal
name editing and adds a text-context-menu action; graph menus alone do not receive
right-clicks consumed by the editable text box. Connected values and missing rows
are disabled because their targets cannot be resolved statically.

## Validation and migration

- `-run=VTGMailFlow -Apply` installs the project Blueprint changes and backs up the
  four affected assets in `Saved/Backups/MailStoryFlow` before modifying them.
- `-run=VTGMailFlow -VerifyOnly` reloads and checks saved wiring, executes the actual
  animation interface, and tests the actual player finish entry in a transient
  Blueprint copy whose terminal/camera continuation is replaced by a marker.
- `Vertigo.Dialogue.MailReadingHandoff` covers ordinary mail, the widget-opening
  gap, repeated dialogue finish, logical closure, an unrelated cached Note,
  duplicate closure, completion without a widget, and Note closure before dialogue end.
- `Vertigo.Editor.MailRowJump` verifies actual Sewer call nodes create the custom
  Mail ID pin widget and retain graph context actions.

These headless checks do not assess the final camera framing or sitting pose's
visual alignment in the Sewer map.

## Delayed terminal cleanup and camera ownership

The original terminal cleanup waits 0.2 + 0.8 seconds before restoring its view
target with a one-second blend. After a story Note closes, a stair camera or another
dialogue may already own the view by then. Restoring the old player/fixed camera
at that point makes the view drift toward Sa while the Lock Point actor stays fixed.

`BP_Player_Sa.VTG_MailCameraStillOwned` now checks
`UVTGMailReadingComponent::CanRestoreMailViewTarget`: the possessed reader must
still be the controller's current view target. Otherwise skip only the old view
target change and continue the mail-camera/component cleanup. This also respects
pending camera transitions, since GetViewTarget returns the incoming target.

`-run=VTGMailFlow -FixMailCamera` installs the guard with a backup in
`Saved/Backups/MailCameraOwnership`; `-VerifyMailCamera` reloads and checks it.
The runtime probe executes the actual Blueprint return branch in a transient copy:
it reproduces the old camera takeover without moving the Lock Point, then checks
the guarded path, cleanup completion, an older Activated fixed camera, and ordinary
mail restoration. No story or map asset is changed by the verification.

## Sewer final-line exit

`L3_CanLeave` now calls BPLM's `ReleaseCameraLock` before `Sa_RestoreInput`.
The stair camera acquired `LockPlayerInput`, which increments controller move/look
ignore counters, sets the player's cinematic input flag and disables collision.
`Sa_RestoreInput` only enables pawn/controller input and requests GameOnly; it cannot
balance the camera lock or clear its cinematic flag. Use the camera's paired release
when finishing a camera-owned story scene.

NPC_70's shared node-start/node-finish callback now checks `bStarted` and invokes
Can Leave only on the finish branch, after the final line has played.
The project's Narrative dispatcher historically invokes bound Blueprint callbacks
only at line start despite their start/finish name. NPC_70 therefore explicitly
enables `Run Bound Event On Finish`; this boolean moves that one bound event to
FINISH only. OFF is the default and means START only. Other nodes keep their existing
start-only behavior. A finish-only Blueprint branch without this flag is unreachable.
Finish-only nodes show a pink note below the Narrative node. The separate Events
array keeps its existing timing settings and is unaffected by this boolean.

`-run=VTGMailFlow -FixCanLeave` installs these two Blueprint changes with backups in
`Saved/Backups/CanLeaveFinishCallback` (the initial camera release fix is also
backed up in `Saved/Backups/CanLeave`). `-VerifyCanLeave` checks the saved graph and executes the
actual BPLM lock/leave flow in an isolated world, covering input, collision,
cinematic flags, repeat calls and preservation of independent Note/dialogue locks.
The check now dispatches the saved NPC_70 event through `ProcessNodeEvents` for
both phases in a transient dialogue subclass, rather than calling Can Leave
directly. This covers the caller as well as its cleanup implementation.
It also disables the finish flag temporarily to reproduce the stalled callback,
then restores the saved flag and confirms the actual `PC_Game_VTG` view target
returns from the stair camera to the player. `Vertigo.Dialogue.BoundNodeFinishOptIn`
checks that existing nodes remain start-only and that opted-in nodes run only at finish.
`-AuditBoundEventTiming` scans every project Dialogue and writes an explicit exception
list to `Scripts/MailDialogueDiagnosis/bound-event-timing-audit.txt` without modifying assets.
