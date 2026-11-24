# Bar animation audio

Saved 14 requested PlaySound notifies: kick .312 whoosh and 1.32 landing; push-door .31/.74/1.24/1.60 default Sa boots; drunk walk .22/.77/1.37/1.90/2.36/3.04/3.52/3.92 default boots. Footstep references use current /Game/SFX/Character/SaFootstep/Cues paths, volume .6. Existing notifies retained, source animation backups in Backup. verify.log reloads assets, checks the added track counts, non-null sound references and exact authored times in exported text.

Fence BP: /Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision. BPLM_L2StreetBarFight references FightAreaCollision and invokes ActivateCollision; the authored setup activates an existing reference rather than spawning this BP in that call.
