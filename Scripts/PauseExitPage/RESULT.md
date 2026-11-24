# Pause menu exit page — 2026-11-21

`-run=VTGPauseExitPage [-Apply]` (no flag = verify from disk). Backup in `Backup/`.

- Removed `On Clicked (B_Exit) -> Quit Game` from WBP_PauseMenu so the C++ handler owns the button.
- Re-keyed the EXIT GAME label from `"" / 0C8F908E49A77F8406E906A6C53553C1` (auto GUID, empty
  namespace) to `VTG.WidgetLabels / WBP_PauseMenu.ExitEntryText`, which is what the conflict lock in
  `Localization/ManualTranslations.json` was reacting to.
- That JSON entry: namespace + key updated, `approvedDisplayText` true, `lockReason` cleared,
  zh-Hans changed from 退出到主菜单 to 退出游戏.

New page: `UVTGExitConfirmPage`, opened by `UVTGPauseMenu::OpenExitPage`. Not playtested.
