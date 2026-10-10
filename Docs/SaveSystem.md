# Vertigo Save / Load — Implementation Reference

Everything routes through one C++ subsystem: **VTGSave Coordinator**.

---

## 0. Get the Coordinator (do this in every graph that saves/loads)

Node: **Get Game Instance Subsystem**
- In the node's **class dropdown**, pick **VTG Save Coordinator**.
- Its output pin (blue object) is the Coordinator. Plug it into the **Target** pin of every save node below.

> Get it fresh in each graph. Do NOT promote it to a variable that lives across level loads.

---

## 1. Node reference (exact names, pins, return types)

All of these have a **Target** pin = the Coordinator from step 0.

| Node | Input pins | Returns |
|------|-----------|---------|
| **Save To Slot** | `Slot` (int), `User Label` (string) | `bool` (success) |
| **Load From Slot** | `Slot` (int) | `bool` |
| **Auto Save** | — | `bool` |
| **Load Auto Save** | — | `bool` |
| **Has Auto Save** | — | `bool` |
| **Delete Slot** | `Slot` (int) | `bool` |
| **Does Slot Exist** | `Slot` (int) | `bool` |
| **Get All Slot Metas** | — | `Out Metas` (array of **VTG Slot Meta**) |
| **Get Auto Save Slot** | — | `int` (it's 0) |
| **Set Persistent Int** | `Key` (Name), `Value` (int) | — |
| **Get Persistent Int** | `Key` (Name), `Default Value` (int) | `int` |
| **Set Persistent Name** | `Key` (Name), `Value` (Name) | — |
| **Get Persistent Name** | `Key` (Name) | `Name` |
| **Clear Persistent Flags** | — | — |
| **Save Checkpoint** | `Checkpoint Id` (Name), `Respawn Transform`, `Order` (int) | `bool` (false = older than the current one) |
| **Retry From Checkpoint** | — | `bool` |
| **Get Resume Checkpoint** | — | `Name` (None = normal level start) |
| **Get Resume Checkpoint Order** | — | `int` |
| **Clear Checkpoint** | — | — |

**Bindable events** (red, for Blueprint-only systems — see §6):
`On Save Subsystems (Slot)`, `On Load Subsystems (Slot)`, `On Slot Saved (Slot)`, `On Slot Loaded (Slot)`.

**VTG Slot Meta** struct fields (from `Get All Slot Metas`, for a load menu):
`Slot Index` (int) · `Display Label` (string) · `Stage` (Name) · `Level Name` (string) ·
`Save Time Utc` (DateTime) · `Save Version` (int) · `Is Valid` (bool — false = empty slot).

---

## 2. Variables / components you must create

| Where | What to add | Type | SaveGame? | Notes |
|-------|-------------|------|-----------|-------|
| Player **Pawn** | Add Component → **VTG Player Progress Component** | (component) | — | Holds the saved player stats below. |
| └ inside that component | `Health` | float | ✅ (already) | Built in. |
| └ | `Max Health` | float | ✅ (already) | Built in. |
| └ | `Counters` | Map\<Name,int\> | ✅ (already) | Free-form ints/currencies. |
| └ | `Flags` | Map\<Name,bool\> | ✅ (already) | Free-form one-off bools. |
| Player BP | `Is Dead?` | bool | no | Gates the restart key. You already have this. |
| Any actor that must persist | the **VTG Saveable** interface | (interface) | — | Class Settings → Implemented Interfaces. |
| └ that actor's vars | each var to persist | any | ✅ **you must tick it** | Tick **SaveGame** in the var's Details panel. |
| └ optional | **Event On Save Restored** | (interface event) | — | Fires right after a load put the vars back. Restoring a var doesn't re-run what normally reacts to it — re-apply the visible result here (show the weapon, open the door). |
| `BP_Player_Sa` | VTG Saveable + `CurrentMeleeWeapon` | E_MeleeType | ✅ (done) | **On Save Restored** → `EquipMeleeWeapon(CurrentMeleeWeapon)`, so the hammer comes back after Retry / Load. The player is saved under the fixed id `VTG.Player`. |
| Pickups (`BP_Hammer`) | the **VTG Saveable** interface | (interface) | — | Nothing else needed: a level-placed saveable actor that was **destroyed** before the save (picked up) is destroyed again on load, so it doesn't reappear next to the restored item. |

> **The one variable rule:** a value is only saved if it lives on a saved object
> (the Progress Component, or a **VTG Saveable** actor) **and its `SaveGame` box is ticked.**
> Untick = not saved. A var on a random actor with no interface = not saved.

---

## 3. Manual save / load

**Save:**
`Get Game Instance Subsystem (VTG Save Coordinator)` → **Save To Slot** (`Slot` = 1, `User Label` = "My Save").

**Load:**
`Get Game Instance Subsystem (VTG Save Coordinator)` → **Load From Slot** (`Slot` = 1).

---

## 4. Checkpoints + death "Retry"

A checkpoint is the autosave slot (0) plus three persistent flags (checkpoint id, level, order),
so it rides on everything in this doc. Two ways to set one:

**A. Place a checkpoint (usual way).** Drag **VTG Checkpoint** (`AVTGCheckpoint`) into the level.
- The green **box** is the trigger: when the player walks in, the game autosaves.
- The green **arrow** (`RespawnPoint`) is where - and facing which way - the player respawns.
  Move/rotate it in the viewport. Retry puts the player on the arrow, NOT where they were standing
  when they crossed the box (they could have been mid-fall).
- **Checkpoint Id**: unique in the level (empty = actor name). BPLM reads it back (section 5).
- **Order**: higher = further along. Walking back into an earlier checkpoint does nothing.
- **Required Stage**: empty = every stage.
- A checkpoint placed on the PlayerStart saves as the level begins.
- Override **Can Activate** to block saving at bad moments; **On Checkpoint Reached** for a toast/sound.

**B. From a BPLM at a story beat** (end of a cutscene, start of a fight):
`...Coordinator` → **Save Checkpoint** (`Checkpoint Id`, `Respawn Transform`, `Order`).
(Plain **Auto Save** still works too - Retry treats any autosave of the current level as its checkpoint.)

**Death screen:** `WBP_Death` derives from C++ `UVTGDeathScreen`. Buttons are bound by name:
`RetryButton` → **Retry From Checkpoint**, `MainMenuButton` → opens `Main Menu Level`
(Class Defaults, set to `M_MainMenu`). The **R** key also retries. Restyle freely, keep the names.

**Retry From Checkpoint** (callable from anywhere):
fades to black → reloads this level's checkpoint (no tunnel loading video) → fades back in after
player/actors/quests/inventory are restored. If this level has no checkpoint yet, it restarts the
level from the top in the same stage.

What a retry gives back: player at the respawn arrow, health full (BP `Health` resets with the
level), inventory and quests as they were at the checkpoint, every enemy alive again (the fight is
retried as a whole).

**New game:** call **Clear Checkpoint** (deletes slot 0 and the checkpoint flags), otherwise
"Continue" / Retry would find the previous run's checkpoint.

---

## 5. Resume mid-scene on reload WITHOUT changing Stage

**Checkpoint levels: branch on the checkpoint id.** A retry reopens the whole map, so the BPLM's
intro sequences / dialogues would play again. In BPLM's **On Stage Begin**, call
**Get Resume Checkpoint** (on the BPLM itself - it's on `VTG Level Manager Base` - or on the
Coordinator) and **Switch on Name**:
- `None` → normal level start, play the intro.
- a checkpoint id → skip what the player has already seen and set the level up for that point
  (e.g. `SewerLairDrop` → skip straight to the lair). It's valid from BeginPlay on, for the whole
  stay in the level.

The older per-flag pattern below still works (BPLM_L2StreetBarFight uses it); use it when you need
state that isn't tied to one checkpoint.

Use a Persistent flag, because changing Stage destroys stage-gated actors, and because a flag is
restored **before** the map opens (so BeginPlay can read it). Actor/Progress/Narrative/ISX state is
only restored **one tick after** load — too late for a BeginPlay decision.

**A. When the phase begins** (e.g. `OnFinished_CombatIntro`), BEFORE **Auto Save**:
`...Coordinator` → **Set Persistent Int** (`Key` = `L2StreetFightPhase`, `Value` = `1`)
→ then **Auto Save**.

**B. On level start** (in BPLM, before your existing intro Branch):
`...Coordinator` → **Get Persistent Int** (`Key` = `L2StreetFightPhase`, `Default Value` = `0`)
→ **Branch** (Condition: the result `== 1`)
- **True** → call **EnterCombat** (your custom event that does the combat setup, skips the sequences).
- **False** → play the intro as normal.

> Make **EnterCombat** a Custom Event holding your combat-setup nodes (set phase, teleport enemy to
> front, equip, set view target). Call it from BOTH `OnFinished_CombatIntro` and the Branch above.

`Key` strings are arbitrary but must match exactly between Set and Get. Use one per phase
(`L2StreetFightPhase`, `L2BarPhase`, ...). `1` here just means "combat reached"; use 0/1 like a bool
or an enum index for more phases.

---

## 6. Inventory (ISX)

The player's ISX inventory (items, ammo, shortcuts, equipped item) is saved into the slot by the
Coordinator itself (save version 2) - no Blueprint hookup needed. The melee weapon (hammer) is not
an ISX item; it is saved on `BP_Player_Sa` (section 2).

**On Save Subsystems / On Load Subsystems** stay available for any other Blueprint-only system that
needs to write its own file next to the slot.

---

## 7. Timing & gotchas (the rules that bite)

- **Restored BEFORE map opens** (readable in BeginPlay): the **Stage**, and **Persistent Int/Name** flags.
- **Restored ONE TICK AFTER map loads:** Player Progress Component, **VTG Saveable** actor vars,
  Narrative quests, ISX inventory. Do not read these in BeginPlay.
- **Stage vs flag:** Stage = "where in the level / what spawns & self-destroys."
  Persistent flag = "sub-progress within a stage." Use a flag for skip-the-intro.
- **One slot = several files** (`VTG_Slot_N`, `VTG_Manifest_N`, `VTG_Narrative_N`) written
  together. Remove with **Delete Slot**, never by hand.
- **Auto Save = slot 0.** Keep manual saves on slot 1+.

## Terminal SAVE / LOAD

The existing terminal menu filters ITEM from its runtime tab list and adds one SAVE / LOAD
entry (reserved index 254). Original assets and existing content page indices are preserved.
Opening the tab never saves. It opens `UVTGTerminalSavePage` inside the same widget switcher.

The page copies the authored mail screen rectangle (`Overlay_1` / `OverlayForMail`) and clips
all content inside it. The original terminal frame image is copied from the mail page background and placed behind this rectangle. Left: scrollable slots;
right: location, timestamp and save description; bottom: Save Game, Load Game, Cancel and Close.
The selected slot is highlighted. The latest existing save is initially selected, otherwise slot 1.

Slot 0 is a read-only automatic checkpoint. Slots 1..9 support manual saves. Empty slots cannot
be loaded. Saving over an existing slot and loading a slot require a second explicit confirmation;
selecting another slot, cancelling or closing the terminal cancels the pending action.
Results and write/load failures are shown inline. Loading uses `LoadFromSlot`, closes the terminal
UI and releases pause/UI-only input before map travel completes. No additional main-menu UI is added.

`SaveTerminalSlot(Slot, Result)` validates manual slots and transition state before calling the
existing save coordinator. Manual saves also capture the player's actual Blueprint Health, while
checkpoint retry keeps its existing health reset behavior. Map/stage, inventory, quests, persistent
progress flags and opted-in VTGSaveable actors remain handled by the existing coordinator.
Unregistered enemy or arbitrary sequence state is not an automatic world snapshot.

The page is never added directly to the viewport. Owner visibility changes and the explicit terminal Close action hide it; reopening refreshes slots and resets confirmation state.

Verification: `-run=VTGAlleyRepair -TerminalSave -VerifyOnly` checks the real terminal blueprint,
repeated construction, screen geometry/clipping, slot protection, empty-load behavior, overwrite/load
confirmation, close/reopen behavior, and player position/health serialization. No user save slots
are written by these tests. Add `-AllowCommandletRendering -PreviewFile=<absolute PNG path>`
(without `-nullrhi`) to render the actual UMG page. This is not a full in-game load/playthrough test.

The 3D terminal prop visibility must not control the 2D terminal page: inventory can open while the prop remains hidden. The frame regression verifies the original terminal texture and UI visibility with the prop hidden.



## BPLM progress snapshots (2026-10-09)

New saves capture level-manager Blueprint scalar progress (bool, number, enum, name, string),
plus the common BPLM GeneralLevelPhase. Runtime references and common camera/input/sequence locks
are deliberately rebuilt, not deserialized. Actor identity and class must match in the saved map.
The coordinator restores progress before world BeginPlay; the native manager also restores before
Blueprint ReceiveBeginPlay/OnStageBegin for actors initialized later. Restoration is idempotent.

BlackMarketEntrance now records the intro, mail notice, distant argument, and meeting sequence
entries using EnterSavedStoryEvent. Entries already present in a loaded snapshot are suppressed;
unplayed entries and new-game starts retain the original flow. These are entered-event flags,
not a serialization of an in-flight dialogue/latent sequence. Save at a settled terminal interaction.
Player mail context and BP_0ISXInteractiveCollision CanInteract? are also saved; interaction component
state is refreshed after world initialization, preserving the story's exit lock/unlock.

Old slots still load, but contain no BPLM snapshot or entered-event ledger. They cannot reconstruct
past event completion. Create a new save after reaching the desired progress with this build.
Other maps receive scalar BPLM persistence; their unconditional story entry graphs need explicit
EnterSavedStoryEvent gates where replay suppression is required. UObject references, latent actions,
and arbitrary Blueprint DoOnce internals are not a general-purpose world snapshot.

Regression: VTGAlleyRepair -LevelProgress -VerifyOnly (in-memory save roundtrip, real entrance BP,
restored vs future events, fresh new game, coordinator early lookup, duplicate restore guard).
Blueprint backup: Saved/QA/LevelProgressBefore/BPLM_BlackMarketEntrance.uasset.

## Main menu Continue (2026-10-09)

WBP_MainMenu now inherits VTGMainMenu and retains the original button tree, art, and new-game graph.
The existing Button is Continue; it is disabled without a readable save whose map is present in the
asset registry. GetLatestValidSlot compares actual payload timestamps across autosave and manual
slots; ContinueLatestSave uses the existing LoadFromSlot pipeline, including BPLM restore.
Map lookup uses AssetRegistry rather than loose-file scanning so cooked IoStore builds work.
The button's visible label is editable in the Widget Blueprint as before.

Windows build: Development; startup M_MainMenu; 14 mainline maps explicitly cooked; sewer, bunker, test, film-booth and old maps are NeverCook.
Final output requested on D:/VertigoBuilds/2026-10-09. Saved user slots are not included in the build.

Exact build map manifest: D:/VertigoBuilds/2026-10-09/IncludedMaps.txt.
