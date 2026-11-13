# Map selection and location introductions

In `WBP_Map_1`, select each `WBP_MapNode` instance in the Designer. Its **Map
Location** category contains **Location Name** and the multiline **Location
Description** field. Both retain their FText localization identities and the
existing manual translation workflow. Empty descriptions display “No information
available.”; no fictional location lore was added.

Unselected nodes display only their existing icons. Selecting one retains its
authored selected texture and shows a black name button at its center. Clicking
the name opens the introduction panel, normally to the left. Once an introduction is open,
selecting another node replaces it with that node's introduction. Deselecting the
node removes both caption and introduction. The old bottom captions, the subarea
`1111` placeholder, and hover tooltip are suppressed. Names use Noto Sans Medium,
introductions use Noto Sans Regular.

The introduction panel is positioned in the map canvas using the map background
image's drawn rectangle. The canvas is authored at nearly zero size around the
terminal center, so using canvas dimensions for clamping incorrectly places the
panel at the center/right. If the proposed introduction's left edge crosses the
30% line of the terminal (center minus 20% of its full width), the panel appears
on the node's right instead. Otherwise it stays on the left. Right-side panels
wrap/scroll within the remaining screen space when necessary.

Save / Load now creates the actual shared `WB_Background`, preserving its entire
Image_40 / BackgroundBlur / Terminal hierarchy, rather than copying selected
properties of the blur and frame. Document artwork remains above that backdrop.

Maintenance: `-run=VTGAlleyRepair -MapPresentation`, with `-VerifyOnly` for a fresh
read-only regression. The older `-MapLabels` entry point aliases the same current
presentation. The changed node Blueprint is backed up in
`Saved/QA/MapPresentationBefore`. Runtime layout code is in VTGMapLocationNode.cpp.
`Scripts/MapPresentation/pie_map.py` runs the actual BaseLevel PIE pointer/keyboard
checks and captures the initial, selected, introduction, switched and Save / Load
states. No save/load action is clicked by that test.

Validation: Development Editor build passed. A fresh commandlet verified four
distinct editable introductions, caption visibility, single-panel replacement and
terminal/save input regressions. BaseLevel real PIE passed initial hidden names,
selecting Bureau, clicking its name, then switching to Commerce, Slickwater and
North Tunnel while maintaining exactly one introduction. Pointer tests assert the
panel is on the expected side of its node. Save / Load
and Tab close were exercised afterwards. `pie-result.json` reports `passed: true`;
the six PNGs in that folder are actual PIE captures. No save/load button was used.

Side-switch follow-up: `MapPanelSidePIE.log` verifies Bureau/North on the right and
Commerce/Slickwater on the left under the 30%-line rule. All four real pointer
selection/description transitions and terminal close passed; the original
left-edge forced narrowing has been removed.
