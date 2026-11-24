# Cyan order dialogue flow

Entry blueprint already selects Cyan for first entry (setting L4_CyanConvHappened? true before BeginDialogue), and Identity for subsequent entries.

The dialogue asset now connects NPC_426 to NPC_177. NPC_177 has a negated VTGDialogueStartCondition for the Identity entry ID. This uses per-instance launch parameters, so the persistent flag changing before playback cannot suppress the first continuation. Other entry points keep their existing access to NPC_177.

Backup: Backup/L4_BlackMarketInvestigation_Still_MainInvestigation.uasset

Validation: editor build; dialogue compilation (0 errors/warnings); reload and Narrative GetReplyChain checks from NPC_426 (Cyan: 9 nodes, including NPC_177; Identity: only NPC_426). No full in-game playthrough performed.

Apply: UnrealEditor-Cmd.exe Vertigo.uproject -run=VTGBlackMarketFollowup -CyanOrderFlow -unattended -nullrhi -nosound
Verify saved asset: add -VerifyOnly.

Restart an already-open editor before using the changed asset, to load the new native condition class and saved dialogue. Avoid saving an older in-memory copy over the updated dialogue.
