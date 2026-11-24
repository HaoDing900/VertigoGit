# Camera ratio and boundary polish

Requested ratio is UHD 3840x2160 = 16:9. Only camera aspect constraint, filmback height (width preserved) and optional crop ratio are normalized. Cine camera focal length, complete focus settings and relative transform are asserted unchanged for every camera operation. Aspect-ratio tracks are handled without changing lens or transform tracks.

The editor module enforces the same normalization before saving project Level Sequences and maps, and initializes new camera defaults to 16:9. No render output resolution is changed.

Boundary: two sides forming the circled corner move outward by 120 local cm, extending the meeting ends. Existing mesh/shader references are retained. The 23 signs are arranged evenly with 115 cm corner clearance; their actual mesh pivot offset is compensated, and width is reduced to 75 percent without changing height.

Screenshot 3: /Game/Widget/Tutorials/WBP_Tutorial_Battle (combat instructions and continue button).
Screenshot 4: /Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu (GAME PAUSED, Continue, EXIT).

Backups: Backup (boundary map/BP), BackupRatio (camera packages). Final application counts and policy test results are recorded separately in logs.


Final results: 28 maps and 73 referenced sequences checked; 146 placed camera components considered; 25 packages updated. All camera operations asserted unchanged focal length, full focus settings and relative transform. Reload sampling passed for both modified sequence packages plus 12 cameras in L2Bar/L2_StreetOutsideBar. New sequence save policy passed with focal length 53 and manual focus distance 987 preserved. Boundary reload passed: 23 signs, uniform spacing on each side, pivot compensation, unchanged mesh/material references, and the two requested collision offsets. No in-game visual preview was performed.
