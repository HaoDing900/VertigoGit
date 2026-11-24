# Sewer combat feel — 2026-11-21

Tools: `-run=VTGSewerFeelAudit` (read-only timing dump), `-run=VTGSewerCombatFeel [-Audit|-Apply]`
(no flag = verify from disk). Pre-change assets in `Backup/`.

| What | Before | After |
|---|---|---|
| Boss `HitReacts` (SeaMonster_P1) | all None | AM_EggProducer_GotHit x4 |
| `AM_EggProducer_GotHit` | 1.125s @ rate 1.0 | 0.703s @ rate 1.6 |
| Headbutt ability | absent | restored, 420cm, CD 3.0s, x1.5 damage |
| Headbutt readability | none | NS_AcidSplash_Med1 at 0.850s on spine_006, aimed at target |
| Hammer combo 1 damage | 0.428s | 0.295s (rate 1.45) |
| Hammer combo 2 damage | 0.306s | 0.235s (rate 1.30) |
| First-hit hitstop | 0.0 | 0.07 |
| Hitstop scope | player only | attacker + every target hit |
| Camera shake on landing a hit | none | Sudden_Cam_Shake_Slight @ 0.05 |
| Dodge i-frames on Sa | none (notify looked up BP_Player_May) | 0.083-0.520s, the authored window |
| Dodge vs radial damage | impossible | same window as point damage |
| Perfect-dodge slow motion | whole montage, point damage only | the i-frame window, both damage paths |

Logs: `audit-feel.log`, `apply-assets.log`, `apply-montages.log`, `apply-player.log`,
`apply-self.log`, `verify.log`. Full write-up in `Docs/CombatSewerPolish.md`.

Not playtested.
