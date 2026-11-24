# Fist wind and recovery

Created /Game/VFX/Wind_VFX/VFX/Particles/NS_fist_Wind from NS_Wind_Flows_02_Fast. Uses .10–.18 second lifetime, constant 30/second spawn curve, initial 6-particle burst, .6-second loop, smaller fist-sized mesh scales, local space, fixed bounds, and zero warmup. Source asset unchanged.

BP_Player_Sa now contains FistWindAndRecovery. It recognizes the 3 actual unarmed WeaponDataMap combo montages, InCombat, living state, equipment and cinematic visibility; attaches up to two Niagara components to the actual BrassKnuckle components. Attack end/interruption/combat exit immediately deactivates and destroys those components. No Niagara components are kept dormant out of combat.

On combat exit, starts a 30-second full-heal timer. Re-entering combat cancels it; a later exit starts a fresh timer. Dead players are not revived. Healing uses UpdateHealthByItem and MaxHealth_Normal and clears ShowLowHealth?. Wounded non-combat save/load starts recovery too.

Development build succeeded. -run=VTGFistWind -Verify completed with 23 passing checks, including real combo montage recognition, both knuckle attachments, component cleanup, timer boundaries/re-entry/death, authored maximum health and low-health flag. Tests use NullRHI, so visual appearance and GPU cost have not been measured in-game. apply2.log records successful Niagara compilation/asset save. Original player asset backup: Scripts/FistWind/Backup.
