# Bello sequence diagnosis

No production assets or source code were edited. Probes loaded the saved PoliceStation map in isolated editor/PIE processes. Runtime changes existed only in those test worlds.

Sequence: /Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_BelloTouchCarBottom

Confirmed cause: a prior dialogue Idle Montage overrides the cutscene downstream in the AnimGraph.

- L3PoliceStation_Still.DialogueNode_NPC_16 starts ANM_Bello_Stand_Idle_still1_Montage for TargetIdentity Bello.
- DialogueNode_NPC_29 starts NLS_L3_BelloTouchCarBottom.
- The Sequence correctly writes Is Cutscene=true on the current ABP_Bello_NPC instance.
- Its body clips use FullBodySlot. In ABP_Bello_NPC, the boolean blend output feeds AnimGraphNode_Slot_3 (DefaultSlot), then NarrativeJaw, then the root. The old idle montage uses DefaultSlot, so it replaces the upstream cutscene pose.

PIE controls:
1. Through NarrativeComponent.PlayDialogueLevelSequence, no prior idle montage: Is Cutscene stayed true; spine_02 component height around 60 at 9s, bent pose.
2. Same playback with the dialogue's prior Idle Montage playing: Is Cutscene stayed true and idle stayed active; spine_02 height around 126 at 7-8s, standing pose.
3. At 8s stopped only that prior idle montage in the isolated test. At 9s Is Cutscene was still true, idle was inactive, the sequence montage remained active, and spine_02 height became 60.76, matching the bent control.

Evidence: pieprobe3.log (no-prior-idle control), pieprobe_idle.log (prior-idle + targeted stop), ABP_Bello_NPC.copy, L3PoliceStation_Still.copy.

The project montage guard was checked but not implicated: the actual 1PlayLevelSequence event default Suspend Montages During Sequence=false and the PIE actor had no montage guard.
