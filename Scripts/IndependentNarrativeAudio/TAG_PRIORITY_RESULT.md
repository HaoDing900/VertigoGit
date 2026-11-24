# Tag priority and external audio — 2026-11-03

Build: tag-build-final.log succeeded.
Asset upgrade: tag-apply.log succeeded; Play/Stop Blueprints compiled with zero errors and warnings.
Validation: tag-verify.log succeeded. Tag overrides the audio fallback, an unknown tag never falls back, owners stay isolated, same-tag different-source play replaces previous playback, and SoundToStop carries CPF_AdvancedDisplay.

External Audio Components must be explicitly registered using RegisterAudioForNarrativeStop with a nonempty PlaybackID and the same NarrativeComponent used by the Stop NE. Registration does not start or reconfigure audio. Stop removes tracking and stops/fades the registered component without destroying externally owned persistent components. Natural completion/unregister release tracking. A component repurposed for another source is not stopped by its stale registration. Reuse should unregister before replay and register after replay.

Existing loop and dialogue-end cleanup checks also passed. Tests use disabled audio output; no in-game listening was performed. Asset backups: TagPriorityBackup. Usage: Scripts/LevelAudio/NE_Usage_Notes.md.
