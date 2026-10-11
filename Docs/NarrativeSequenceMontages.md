# Narrative Sequence Montage suspension

`/Game/Narrative/Events/1PlayLevelSequence` now exposes **Suspend Montages During Sequence**, default **false**. Enable it on the Narrative event instance that should take over animation.

When enabled, `PlayDialogueLevelSequence` resolves actors bound by the root Sequence and its compiled subsequences, saves each skeletal mesh's active Montages, and calls `Montage_Stop(0)` before `Play()`. It also stops Montages started on those actors while the Sequence owns them. Unbound actors are unaffected.

After the Sequence finishes, stops, or its player is destroyed, a component running in `TG_PostUpdateWork` resumes the saved Montages after Sequencer has restored animation state. Position, play rate, paused state, and next-section links are restored. Sequence pause keeps the suspension. Multiple enabled sequences sharing a character release it only when all have ended. Destroyed characters and replaced AnimInstances are skipped safely.

This uses Unreal's normal Montage stop/play operations. Existing interruption callbacks will fire, and original per-instance completion delegates and blend weights are not reconstructed. For gameplay Montages whose interruption changes gameplay state, verify the relevant gameplay callbacks in PIE. Spawnables created later have no pre-existing Montage to save; bindings reassigned after playback starts are not captured.

Validation:

- Build: UE 5.3 `VertigoEditor Win64 Development`.
- Blueprint migration/verification: `UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGSequenceMontage -unattended -nullrhi -nosound -nop4`.
- Runtime checks with a real authored Montage and a bound character: same command with `-Test`.

The commandlet migration is opt-in and idempotent; it does not run on editor startup.
