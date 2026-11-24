# Hammer posture correction

Applied a runtime additive bone correction in `/Game/Characters/Sa/ABP_Sa` immediately before the existing camera neck/head controls. The source animations and `/Game/Characters/Sa/BlendSpace_Sa_Hammer` are unchanged.

- Bone: `spine_02_x`.
- Rotation: bone-local X (Unreal Roll), default -15 degrees; translation and scale ignored.
- Active while CurrentMeleeWeapon is SmolHammer, including Hammer attacks and locomotion.
- Disabled for other melee states, active ranged weapons, death, hidden player, and cinematic overrides.
- Unequipping Hammer clears the correction immediately.

Tune in **ABP_Sa > Class Defaults > Hammer Posture** using **Hammer Spine X Degrees** or **Hammer Spine Correction Enabled**.

Validation: VertigoEditor build passed. Apply and fresh-process VerifyOnly commandlets exited 0. Actual evaluated player mesh poses measured a local rotation delta of Pitch 0, Yaw 0, Roll -15 degrees, with unchanged translation and scale. Empty-handed, ranged, death, cinematic and unequip checks passed. This verifies the evaluated pose; the visual result should be assessed in gameplay.

Original asset backup: `Scripts/HammerPosture/ABP_Sa.before.uasset`. Verification details: `Scripts/HammerPosture/verify.log`.

Repair commandlet: `-run=VTGHammerPosture`; read-only verification: `-run=VTGHammerPosture -VerifyOnly`.
