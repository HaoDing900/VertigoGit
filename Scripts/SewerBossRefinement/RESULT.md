# L3 Sewer boss and egg refinement

Changes saved locally. No push requested or performed.

## Egg death and physics

`BP_SeaMonster_Egg > Boss Behavior > Use Death Anim / Death Anim` now uses `Montages/AM_SeaMonsterEgg_Death`, containing `/Game/Characters/Monster/MonsterEgg/ANM/Monster_Egg1_Death`.

The montage does not blend back to idle. `VTG_DeathPoseToRagdoll` near its end hands the evaluated pose to the existing Toolkit `OnRagdoll` event. Native handoff synchronizes each body's current bone world transform and scale first. The explosion feedback remains.

The egg uses `PA_SeaMonsterEgg_ScaledBones`, a copy of its existing physics asset with Skip Scale From Animation disabled. Original physics asset unchanged. Chaos still constrains primitive scaling: capsule X/Y scales must match, spheres use uniform scale. Arbitrary nonuniform bone edits therefore do not imply an exact nonuniform capsule/sphere fit; those shapes may need manual adjustment or convex replacements. This change does not regenerate collision shapes or constraint authoring automatically.

## L3 boss health overlay

`BP_Boss_SeaMonster_L3Sewer > BossHealthBar > Boss Health Bar / Caustics`:

- Caustic Overlay Material: requested `MI_Inventory_FogMadeIntoCaustic` from InventorySystemX.
- Caustic Overlay Opacity: 0.4, editable per boss.

Runtime widget creates a dynamic material instance (Opacity parameter 0.65) and paints it over the remaining health only, preserving the existing health, damage trail, name and damage number. Brush opacity multiplies the inherited widget opacity. Original plugin material unchanged.

The real UMG widget rendered offscreen with the material; logged region is 1098x12 pixels at 68% health. `hud-preview.png` is a diagnostic capture, not a gameplay screenshot. The thin material detail is not clear enough in that capture for visual approval; confirm intensity in L3 gameplay.

## Acid projectile

`BP_SeaMonster_AcidProjectile` uses `NS_AcidSpit_Projectile`, copied from the requested `NS_AcidSpit_Burst`. All emitters use local space; independent gravity, noise and fountain travel speed are disabled. Original Niagara system unchanged.

AcidVisual rotates -90 degrees in Pitch, mapping the authored spray +Z to the projectile's +X flight axis. ProjectileMovement faces velocity, uses 850 cm/s and no gravity, matching the Toolkit's full 3D aim. The separate world-space montage burst was removed; its audio and damage notifications remain.

`AcidFlight > Max Travel Distance` is 1800 cm. Visual and collision follow the same actor and end together on hit or range expiry. The existing lifespan is an additional bound. Changing the ability's targeting distance later should also update this maximum range.

## Hanging boss

Only L3's boss uses `ABP_EggProducer_L3Anchored`, copied from `ABP_EggProducer`. The original shared animation blueprint is unchanged. The old BP_Boss_SeaMonster path is already a redirector to BP_Boss_SeaMonster_L3Sewer.

The final graph retains the animation slot, fixes root and the lower `spine` translation/rotation, then adds world-space aim on `spine_001`. Its descendants retain the authored animation. Actor placement is held and CharacterMovement/controller rotation disabled. Mesh tick follows the anchor component, which follows actor/behavior ticks.

`HangingBoss` exposes Anchor Enabled, Max Yaw (80), Max Pitch (35), and Aim Speed (5). Attack targeting and damage remain in BossAI Toolkit.

## Validation

VertigoEditor build passed. `apply-final.log` has all pose/physics/projectile checks passing, including the original Niagara spray axis alignment. `verify.log` is fresh-process readback. Actual evaluated root and lower spine stay unchanged during aim; spine_001 turns; attack montage retains upper-body animation. Actual death montage enters Toolkit ragdoll, with scale verified against Chaos-supported primitive scaling. Flight moves the collision and Niagara by identical distance, with shared range expiry.

`damage.log`: real BossAI projectile ability spawns one projectile; collision damages actual BP_Player_Sa; hit destroys projectile; repeat overlap does not damage twice. Existing attack-buffer checks also passed.

No full L3 gameplay playthrough was performed. Backups are in `Backup/`.

## Questions 5–7: explanation only, no implementation

- Existing `Boss Behavior` already exposes `OnBossDeath`. BPLM can bind to it on the placed boss reference, and query SaveCoordinator's `L3SewerBossDefeated` after loading. Current ResolveCombatState forces noncombat throughout the sewer after that persistent flag is set. A future temporary postboss combat lock should be separate from permanent boss defeat; clear the temporary lock at a directional room-exit trigger, then resume enemy-proximity combat. A wall alone does not exclude targets from enemy queries. L3 boss's existing death graph also kills all Sea Monster Eggs with point damage; remove/limit that behavior if eggs should remain on return.
- Player source values: BP_Player_Sa `Health` and `MaxHealth_Normal`, both currently 100. WBP_Action reads these continuously. Set both to the new maximum for a full-health start; keep current Health in [0, MaxHealth_Normal]. Old save data can restore an older current health. Five heart icons represent fractions of maximum; they are not one icon per HP. Use the existing damage/death flow for damage or kills.
- Attack list: `/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases`, row `SeaMonster_P1`, Abilities. Current entries: Headbutt, Toxic Gas, Spawn Eggs, AcidSpit. Boss Behavior's Phases Table and Phases select this row. Damage, Targeting, Cooldown, Duration and Visuals are there. Egg row is `SeaMonsterEgg_P1`. Snapshot exported to `phases.json`.
