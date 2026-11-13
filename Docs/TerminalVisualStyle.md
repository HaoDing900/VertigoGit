# Terminal visual style

Document's outer canvas rectangle now matches the Quest/Map/Mail reference
(`WB_NoteMainWidget.Overlay` follows `WB_TabWidget_Mail.Overlay_1`).
Save / Load copies the Document tab's icon and background brush, and uses the
Mail content rectangle inside the shared terminal frame. Its solid teal screen
has been replaced by transparency; slot rows, details and buttons use neutral
translucent fills over the Document artwork.

Widgets under `/Game/Inventory/InventoryXBased/UMG`, together with their reachable
game widget dependencies and rich text tables, use the project's Noto Sans SC
font assets. Original sizes, spacing, outlines and weight distinctions are kept.
External rich text and CommonUI style assets are copied/derived into this UI's
`Styles` folder so the terminal does not change plugin defaults. CommonUI fonts
must be changed on the style CDO: changing only a CommonTextBlock's resolved Font
is discarded by its serializer and restored from its Style on reload.

Maintenance commandlet (editor closed):

```
UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGAlleyRepair -TerminalStyle -unattended -NullRHI
UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGAlleyRepair -TerminalStyle -VerifyOnly -unattended -NullRHI
UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGAlleyRepair -TerminalSave -VerifyOnly -unattended -NullRHI
```

Add `-Preview -AllowCommandletRendering` instead of `-NullRHI` for offscreen
1920x1080 screenshots of the four visible terminal pages. These previews use a
transient world without the full player/gameplay initialization and are not PIE
screenshots. Backups and preview outputs are in `Scripts/TerminalStyle`.

Validation (2026-11-07): Development Editor build passed. A fresh commandlet
process verified the reference rectangle, 46 widget blueprints, two rich text
tables and one CommonUI style with zero remaining font changes. Runtime checks
verified equal, loadable Document/Save icons, the Document artwork, transparent
save screen and Noto heading. Five offscreen pages were rendered and inspected.
The existing TerminalSave regression passed slot protection, confirmation,
navigation/page reuse and in-memory state serialization checks. Both test drivers
create a LocalPlayer to exercise terminal focus in their transient worlds.
Full PIE interaction has not been exercised for this visual change.

## Terminal input safety and temporary Document visibility

The authored Document entry, page and resources remain in the assets. Runtime
tab preparation now excludes it from both buttons and Q/E navigation; old index
zero requests are redirected to Quest in both page selection and menu selection.
The Save / Load icon and artwork continue to use Document resources.

Save / Load now copies the shared background's blur settings and canvas rectangle,
placing the blur behind the terminal frame and artwork.

The player inventory action uses Enhanced Input `Started` rather than every-frame
`Triggered`. A per-player gate rejects presses in the first 1.5 game seconds and
shares a 250ms cooldown between the game action and the terminal's Tab close key.
Repeated UI keydown events are consumed. Rejected presses are discarded rather
than scheduled for later. The existing readiness delay also uses 1.5 seconds.

The player's unconditional `Set Input Mode UI Only` after `OpenInventory` is
replaced by `SynchronizeTerminalInput`. The plugin's configurable input-mode
branch alone was insufficient in this project: simply removing the old override
left the visible terminal in Game mode. The replacement checks the resulting
terminal visibility, assigns its real owning player and HUD reference, then sets
UI Only, cursor and terminal focus. It runs after the full HUD open call returns.
Hidden/destructed terminals release their owned input to Game Only and flush held
keys. Tab is handled in the root preview event through the authored Close function
so individual pages cannot swallow it. Existing mail/dialogue locks remain in place.

`WB_TabSwitcher` derives from `UVTGTerminalWidget` to filter repeated/too-fast Tab
events before they reach child widgets. The Blueprint contents remain intact.

Apply/verify these Blueprint hooks with `-run=VTGAlleyRepair -TerminalInteraction`
and `-TerminalInteraction -VerifyOnly`, respectively. Pre-change asset copies are
under `Saved/QA/TerminalInteractionBefore`.

Input change validation: Development Editor compiled; apply and fresh-process
verification completed successfully. The regression exercises the 1.499/1.5s
boundary, shared cooldown over 100 simulated rapid cycles, repeated-key filtering,
slot protections and in-memory serialization. Runtime preview verifies four menu
entries, the retained Save icon, and that a direct index-zero selection goes to
Quest while the authored Document child still exists. The four visible pages were
rendered offscreen. Full startup/input interaction in PIE remains unverified.

Focus regression correction: the previous timer-only regression missed the visible
terminal remaining in Game mode. The regression now attaches a real FSceneViewport
to a transient GameViewportClient and verifies 100 UI Only/Game Only transitions,
viewport input suppression, cursor state, missing owning-player repair and hidden
resynchronization. It also sends Tab through the native preview handler and the
actual authored Blueprint Close function, then checks terminal collapse and Game
input restoration. Build and fresh-process TerminalInteraction verification pass.

### Real PIE ownership correction

The scene-viewport test still missed deferred input changes from the game's real
controller/UI stack. BaseLevel PIE reproduced the report with the visible second
terminal instance: a second after opening, it was in Game Only with no cursor or
focus, and Slate Tab did not close it (`TerminalPIEBaseline.log`, `pie-baseline.json`).

Terminal now acquires priority 50 ownership through NarrativeUIInputComponent,
the same mechanism used by dialogue and Note. AVTGPlayerController can therefore
reject a later Game Only request while it remains open. A one-shot Slate post-tick
handoff finalizes focus after UI attachment/deferred configuration. Close releases
only the terminal's request, preserving any other active modal and balancing the
movement/look lock. The authoritative player HUD property is used to find its
terminal rather than taking the first component with a compatible field.

The isolated regression now spawns the actual PC_Game_VTG class and injects a
trailing gameplay-mode/cursor request while Terminal owns input. Real BaseLevel
PIE verification is driven by `pie_input.py` and the diagnostic console commands
in VTGTabSwitcherProbe.cpp; it uses the actual inventory action, Slate Q/E/Tab and
Slate pointer events, with no manually created terminal or forced focus.

Final verification: BaseLevel normal PIE passed five complete cycles of the real
inventory action opening Terminal, Slate E switching pages, mouse hit testing and
button-down/up selecting Quest, and Slate Tab closing. Open UI retained keyboard
focus, UI Only, cursor and viewport input suppression across frames; closed UI
returned to Game Only with no visible terminal. Evidence: `pie-input.json`
(`passed: true`, five cycles), `Saved/Logs/TerminalPIEFinal.log`. The revised
actual-controller isolated regression also passed (`TerminalOwnershipVerify.log`).

Save background follow-up: Save / Load now instantiates the complete authored
WB_Background instead of copied blur/frame widgets. This preserves Image_40,
BackgroundBlur and Terminal as the exact shared hierarchy. See MapPresentation.md
and its actual PIE Save / Load capture for the current implementation.
