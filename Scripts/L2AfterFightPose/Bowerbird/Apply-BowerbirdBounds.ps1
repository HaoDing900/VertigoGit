$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$target = Join-Path $projectRoot 'Content/Characters/BowerBird/Bowerbird_RIG_ue_renamed_.uasset'
$backup = Join-Path $PSScriptRoot 'Backup/Bowerbird_RIG_ue_renamed_.uasset'
$staged = Join-Path $PSScriptRoot 'Staged/Bowerbird_RIG_ue_renamed_.uasset'
if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
    throw 'Save your work and close Unreal Editor before applying the mesh repair.'
}
if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $backup).Hash) {
    throw 'The source mesh changed after the repair was prepared. Review before replacing it.'
}
Copy-Item -LiteralPath $staged -Destination $target -Force
if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $staged).Hash) {
    throw 'The copied mesh does not match the verified repair.'
}
Write-Output "Applied verified Bowerbird rendering bounds: $target"
