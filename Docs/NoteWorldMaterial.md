# Note world material

`PDA_NoteText` now exposes `Document > World Static Material` (Material Interface,
including material instances). `BP_ReadableNote.LoadMesh` applies it to material
slot 0 immediately after setting the world static mesh. None clears the component
override and uses the static mesh default. This field does not change the reading
background mesh or material.

Existing data assets default to None. UnionLeaflet was used as a transient test
fixture; its saved values were not changed. Reopen/reload the data asset Blueprint
and BP_ReadableNote if they were already loaded in the editor when patched. Placed
actors refresh on construction (for example when changing Note Data Asset).

Implementation assets:
- `/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText`
- `/InventorySystemX/Modules/Notes/Actors/BP_ReadableNote`

Original assets: `Saved/Backups/NoteWorldMaterial/`.
The idempotent editor commandlet `-run=VTGNoteWorldMaterial` installs the field and
wiring. `-run=VTGNoteWorldMaterial -VerifyOnly` tests the saved assets without saving.
Tests spawn the actual readable-note Blueprint with a transient copy of
UnionLeaflet, verify construction applies an override, then clear the field and
reload the same mesh to check the previous override is removed.

Logs: `Scripts/NoteWorldMaterial/{build,apply,verify}.log`.

## Background material

`Background > Material > Background Material` is an advanced (collapsed) optional
Material Interface field on PDA_NoteText. It overrides slot 0 of the background
mesh; None restores that mesh's default. Both WB_Note SetMesh call sites pass the
current data asset's material. BP_NoteMeshBackground applies it after the async
mesh load, so it also clears the previous Note's override when switching.

Install: `-run=VTGNoteWorldMaterial -Background`.
Reload-and-test: `-run=VTGNoteWorldMaterial -Background -VerifyOnly`.
Tests exercise the actual background actor's SetMesh event, process its async
continuation, check override application and clearing on the same actor.
Backups: `Saved/Backups/NoteBackgroundMaterial/`.

## Sewer switch investigation

Read-only map inspection confirms BPLM_SewerUnderApt.L3_SwitchColi references
BP_0ISXInteractiveCollision_L3Switch, with a movable root. The saved AfterBoss
event calls EnableSwitchColi, targeting (620.597948, 16201.361817, -692.829100).
An isolated call on the saved map's actual manager moved that exact actor from
Z=-136.747789 to Z=-692.829100. This is an editor-world dispatch test, not a full
PIE lifetime trace; the map was not saved or modified on disk.

The existing live-editor log at `Saved/Logs/Vertigo.log:4015` records a breakpoint
on SetActorLocation in the AfterBoss -> EnableSwitchColi stack. A breakpoint stops
before the node executes. Resume or step over it, and inspect the PIE-world actor
(UEDPIE_0_L_SewerUnderApartment), not the editor-world counterpart. Stopping PIE
restores the editor actor's original location. No Sewer story wiring was changed.

Repeat isolated dispatch: `-run=VTGNoteWorldMaterial -Background -SewerProbe`.
Evidence: `Scripts/NoteWorldMaterial/sewer-actors.json`, `sewer-probe.log`.
