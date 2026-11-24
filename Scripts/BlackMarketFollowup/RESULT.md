# Black Market follow-up — 2026-10-24

## Injured palm
Real animation evaluation reproduces palm skin deformation with constant hand/finger bone scale. Selected palm vertices are influenced by hand_l, forearm_twist_l and finger roots. Morph targets are absent; disabling post-process animation does not remove the pulse. Normal palm span variation is about 0.02 cm; injured mesh-space rotation blending produces up to 1.73 cm. Switching only the 70% injury spine layer in ABP_Sa to local-space rotation reduces the largest variation to about 0.26 cm. Mesh orientation, skeleton and weights are unchanged. This is a CPU animation/skinning test, not a rendered full-playthrough observation.

## Alley trigger
The actual BlackMarketStreet trigger instance already has DoOnce? false. The real blocker was the outer HadInteractHoledInAlley branch with an unconnected true output. New persistent conflict/exit-ready guards replace that gate. Combat disables collision and hides the trigger; victory restores it. ReadyToLeave alone destroys both blocker and trigger with separate DestroyActor calls. Completed-checkpoint resume restores interaction unless exit readiness was saved. Combat phase flags are set before the encounter snapshot.

## Underground Story UI
The unfinished AfterAdult branch started at NPC335, bypassing the Story routing event at NPC333. It now starts at NPC333. All three UndergroundLeave dialogue entries set and apply Story immediately before BeginDialogue. The dialogue asset itself is unchanged.

## Validation
Real SmartTrigger overlap dispatch exercised first-time, repeat-before-combat, repeat-after-combat with hole flag true, combat suppression, victory restoration and final destruction. Real NarrativeComponent plus Narrative3Overlay verified Story main box visibility after UndergroundLeave from a previous Action state. Shared combat/checkpoint regression passed. Player save slots were not modified. Source assets are backed up in Backup.

Persisted ABP_Sa was reloaded and animation evaluation repeated: all four standard cases remain below 0.3 cm span variation; normal unchanged. See injured-before.txt and injured-report.txt. Hammer posture checks also retain their original behavior.
