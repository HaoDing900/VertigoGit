# Medicine audio repair

- Added Play Sound notify for SFX_DrunkVomit at 1.74 seconds on CANM_NPC_DrunkByDoor_Vomiting1_Montage; retained Notify_Puke_C.
- DialogueTemplate.DialogueNode_NPC_88 already references SFX_Pill, with inherited VolumeMultiplier=1 and FadeInTime=0.
- SaApartment had no BP_VTG_AudioManager and its BPLM AudioManagerRef was empty. The NE delegates through BPLM, whose validity check skips playback without a manager.
- Placed BP_VTG_AudioManager_SaApartment (an instance of the existing parent blueprint) and explicitly assigned the apartment BPLM reference. All audio components have Auto Activate disabled; AC_Psych uses its existing 0.4 component volume.
- Original montage and map are in Backup. No dialogue or shared audio blueprint changes were needed.
- verify.py reloads the saved map and checks the reference, automatic playback flags, event settings and montage export. A direct editor-world event probe did not route playback; actor Blueprint execution is restricted in editor worlds, so this probe is not a PIE playback test. In-game listening remains unverified.
