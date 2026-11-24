# Sa outfit footsteps — 2026-10-25

`BP_Player_Sa` → `PlayerAudio` → `Footsteps` now contains:

- `Default Steps`: `/Game/SFX/Footsteps_Volume_02/Cues/Footstep_Boots_01_Cue` through `06_Cue`.
- `Barefoot Steps`: `Footstep_Naked_Feet_Deep_01_Cue`, `05_Cue`, `06_Cue` in the same folder.
- `Barefoot Mesh`: `/Game/Characters/Sa/Naked/Sa_Naked`.

At every foot plant the component compares the character's current body mesh with `Barefoot Mesh`. Naked Sa selects the three barefoot cues; every other body mesh selects the six boots cues. Changing outfit takes effect on the next footstep, including restored appearances. The apartment's `IsSaNaked?` variable is on `BPLM_SaApartment`, so the component reads the actual appearance rather than maintaining another copy of that state.

Configured outfit arrays take priority on all surfaces. The old concrete/metal/water/wood arrays remain as a fallback if `Default Steps` is cleared. Existing foot timing, speed-dependent volume, pitch variation, suppression during combat/falling, attenuation and sewer reverb behavior remain in place. Each footstep randomly chooses one cue and avoids repeating the immediately preceding index.

Runtime: `Source/Vertigo/Private/Audio/VTGPlayerAudioComponent.cpp` and its public header.
Asset setup: `VTGSewerAudioCommandlet.cpp`, `-run=VTGSewerAudio -FootstepsOnly` (only saves the player blueprint).
Validation: `VTGSewerAudioTests.cpp`, `-run=VTGSewerAudio -VerifyOnly`.

The original player asset is backed up in `Backup/BP_Player_Sa.uasset`. Build and commandlet logs are in this directory. Automated validation covers exact requested cue references, clothed → naked → clothed selection, movement restrictions, duplicate notify suppression, and the existing locomotion/reverb checks. Manual in-game listening has not been performed.
