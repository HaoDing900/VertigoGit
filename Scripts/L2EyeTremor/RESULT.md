# L2 eye tremor curve adjustment

Asset: `/Game/LevelNSequence/In_Level/L2BarStreet/Anim/LSFace__l2BarFightSuprisedAnger`.

## Current revision: 15% fast tremor, freeze, 5% residual tremor

- Baseline remains the original 1.06-second eye pose.
- 1.06–1.466667 seconds: original post-baseline tremor compressed to 3x speed, at 15% of its original amplitude (half of the verified 30% version).
- 1.466667–1.56 seconds: smooth, quick settle to the baseline eye pose.
- 1.56–1.64 seconds: hold that eye pose.
- 1.64–1.68 seconds: smoothly introduce residual tremor; from 1.68 seconds onward it uses 5% of the original amplitude, with the same 3x timing. Source motion is sampled from the recorded 30% reference, scaled by 1/6, with forward/reverse sampling to avoid a loop seam.

Only the eye rotation correction curves are edited. Earlier animation, other bones, eye translation and scale are checked against the current asset before each edit. Backups include the 30% version (`Backup/LSFace__l2BarFightSuprisedAnger.before-fast15.uasset`) and the intermediate fast-15%-then-freeze version (`Backup/LSFace__l2BarFightSuprisedAnger.before-micro5.uasset`). The original unmodified backup remains available.

Build: `build-micro5.log`. Apply and preservation checks: `apply-fast15.log`, `apply-micro5.log`. Fresh-process reload verification: `verify-micro5.log`, using `-run=VTGSaCinePose -L2EyeTremor -Micro5 -VerifyOnly`. Expected final eye samples are in `expected-micro5.txt`. Verification is numerical; no visual editor playback was performed.

## Previous 30% revision

Adjusted only the rotation channels of the existing `c_eye_l` and `c_eye_r` additive transform curves. For times after 1.06 seconds, the final local eye rotation is the shortest-path quaternion interpolation from its original rotation at 1.06 seconds toward its original rotation at the sample time, with weight 0.30. The corresponding additive correction is computed against the unchanged source bone track. Correction keys are linear at 250 Hz, with an explicit key at exactly 1.06 seconds. Earlier curve keys are retained.

Translation, scale, other transform curves, the FK source curves and clip length are unchanged. The original asset is backed up at `Backup/LSFace__l2BarFightSuprisedAnger.uasset`; `before.t3d` records the source data.

Editor build passed. Before/after checks covered 109,210 unchanged bone/time samples and both eye rotations across the clip. Maximum rotation error against the requested adjustment was 0.00000010 radians. A separate process successfully reloaded the saved animation and compared both eyes with `expected.txt` using `-run=VTGSaCinePose -L2EyeTremor -VerifyOnly` (exit 0). Logs: `apply.log`, `verify-final.log`. This is numerical asset verification; no visual animation-editor playback was performed.
