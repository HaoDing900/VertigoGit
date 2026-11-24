# Bar hologram boundary and fist direction

L2_StreetOutsideBar now references BP_FightingAreaCollision_BarHologram, a child of the original BP_FightingAreaCollision. It retains the authored actor transform and all original collision box transforms/extents. Twenty-three non-colliding hologram mesh components line the existing three main boundaries, using the exact SM_HologramLogo mesh and MI_HologramLogo_VTGRestrict material from BPA_StageBlockingVolume_l4BlockAlley. The original parent BP and BlackMarketStreet are unchanged. Activation calls the parent collision logic and shows the new visuals; deactivation hides visuals and calls the parent's existing actor destruction behavior. The original decal actor is suppressed only in the new child. Original L2 map backup is in Backup.

Wind now tracks the actual wrist-to-knuckle direction (hand_l/r -> Sa's c_middle1_l/r bones), with absolute world rotation updated while attacking. The private Niagara asset's random initial rotations and angular rotation are zero, and its velocity axis is local X. Source environment wind remains unchanged.

Validation: Development build passed. Original activation/deactivation behavior checked on a disposable preview actor, including destruction on deactivation. Saved map reloaded and verified reference, actor transform, every original collision shape, exact mesh/material references, 23 style components, and inactive visibility. BlackMarketStreet SHA256 matches the pre-change hash. Fist regression passed including both actual hand-axis checks and prior combat/equipment/cleanup/recovery tests. Tests use NullRHI; in-game visual preview and GPU profiling were not performed.

Final fist checks passed: 25. BlackMarketStreet unchanged: True.
