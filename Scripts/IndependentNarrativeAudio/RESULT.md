# Independent Narrative audio — 2026-11-02

The existing NE_VTG_Audio_PlayOneShot and NE_VTG_Audio_StopOneShot assets now call VTGNarrativeAudioLibrary. They no longer need BPLM or BP_VTG_AudioManager. Existing NewSound, FadeInTime and VolumeMultiplier values remain on their original Blueprint variables.

Play adds PlaybackID (None), Loop (false), StopWhenDialogueFinished (true), StopFadeOutTime (0 seconds). Stop adds required SoundToStop, PlaybackID (None), FadeOutTime (0 seconds). Stop matches source asset + ID + owning NarrativeComponent (Pawn fallback when there is no component). No wildcard or shared-channel stop is performed. Replaying the exact key replaces only that key; distinct sounds/IDs coexist.

Dialogue cleanup binds to the exact dialogue instance at playback time. End/cancel/replacement stops that dialogue's opted-in entries. An older dialogue ending cannot stop a newer dialogue's entries. Opted-out entries last until natural completion, matching Stop, replacement of that exact key, or world teardown. Quest events outside a dialogue require explicit Stop for loops.

Wave playback uses a transient SoundCue/WavePlayer to override looping without modifying the wave asset. Cue playback uses a private duplicated graph: internal Looping nodes are removed and WavePlayer loops disabled, then the entire graph is optionally repeated. SoundWave looping is seamless; whole-cue repetition is not guaranteed seamless. Other procedural sound types do not support forced loop override. The transient cue duration is assigned explicitly for packaged runtime as well as editor mode.

VolumeMultiplier now directly controls the independent component, without the old AC_Psych 0.4 multiplier. Old BPLM/AudioManager OneShot and Psych functions remain available but do not stop these independent sounds.

Validation:
- VertigoEditor Development build passed.
- -run=VTGLevelAudio -IndependentVerify: 18 checks passed, including actual existing Blueprint event execution in a Game world with no manager, ownership/sound/ID isolation, dialogue instance isolation, loop normalization, source immutability, non-editor duration handling, completion and teardown cleanup.
- Tests use disabled audio output; in-game listening/fade quality and a full packaged build were not performed.
- verify_existing.py checks current dialogue instances after recompilation without saving those dialogue assets.
- Reference audit: 3 Play-referencing dialogue assets; no pre-existing Stop references needing target migration.

Original NE assets are in Backup. Reapply asset wiring with -run=VTGLevelAudio -IndependentUpgrade. Usage: Scripts/LevelAudio/NE_Usage_Notes.md.

Existing-asset validation passed: 5 Play event instances across 3 dialogue assets (pill, discarded pill, apartment clock, and two sobbing events) retain their sound references, volume 1 and fade-in 0, with Loop=false and StopWhenDialogueFinished=true. See existing-events.json and existing-verified.log.
