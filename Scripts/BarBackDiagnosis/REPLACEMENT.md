# Narrative insert replacement

The ExecuteEvent entry of NE_PlayMontage_InterruptRes now calls UVTGNarrativeInsertLibrary::PlayNarrativeInsert. TargetIdentity and InterruptMontage remain the existing editable fields.

UVTGNarrativeInsertState stores the original underlying montage once per character and retains it across replacements from separate event instances. Replacement guards synchronous interruption callbacks; delayed callbacks cannot clear another active insert. A naturally completed final insert restores the original position, playback rate, pause state, and section links. External interruptions release ownership without resurrecting the replaced insert.

Build: VertigoEditor Win64 Development.
Regression: UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGNarrativeInsert -Test -unattended -nullrhi -nosound.
Asset verification: -run=VTGNarrativeInsert -VerifyOnly.

Regression results: insert-event-tests.log (9 checks passed). Tests invoke separate instances of the actual Narrative Event Blueprint, using the authored Sa montage assets as transient copies. The held first montage explicitly has Auto Blend Out disabled. These are headless animation/event checks, not a full L2Bar PIE playthrough.

Asset backup before migration: NE_PlayMontage_InterruptRes.before-replacement.uasset. No montage settings were changed by this fix.
