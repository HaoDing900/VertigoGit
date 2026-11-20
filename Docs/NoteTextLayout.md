# Note text fitting

`WB_TextContainer` inherits `UVTGNoteTextWidget`. Its existing Blueprint still
supplies the localized text, original `FontSize`, font style and SizeBox dimensions.
The native widget measures wrapped Slate text within the configured page area,
including the top padding. It checks all sibling text pages (including hidden
WidgetSwitcher pages) and applies the smallest shared reduction from 0 to 4 points.
If text still exceeds the page at the minimum size, that page gets a vertical
ScrollBox, preserving its complete content. Shorter text restores the original
size and removes the overflow ScrollBox. Source text and authored page boundaries
are preserved.

Text, page membership, original size and area changes trigger recalculation;
unchanged pages reuse the previous result. Resolved FText strings are checked so
language changes that update displayed text also invalidate the layout.

Install: `-run=VTGNoteTextLayout` (backs up the previous widget asset to
`Saved/Backups/NoteTextLayout/WB_TextContainer.uasset`).
Verify saved parent: `-run=VTGNoteTextLayout -VerifyOnly`.
Test actual saved widget with Slate: `-run=VTGNoteTextLayout -Test`.
Tests cover short Chinese/English text, shared multi-page sizes, intermediate
reductions, the four-point limit, complete overflow content and size restoration.
Logs: `Saved/Logs/NoteTextLayout-install.log`, `NoteTextLayout-test.log`.

Reopen the editor after rebuilding native code; if the asset was already loaded,
reload it before editing/saving it. In-game visual review is still needed for
the final appearance of individual documents and their backgrounds.
