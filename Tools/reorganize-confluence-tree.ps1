<#
    Reorganize the flat per-page PDFs (tools/confluence-pdf-export) into a nested
    folder tree that mirrors the Confluence page hierarchy.

    Rule per page:
      - ancestors path = titles of its parent chain (root -> parent)
      - if the page HAS children: it becomes a folder; its own PDF goes inside as
        "<title>/<title>.pdf"
      - if the page is a LEAF: its PDF goes directly in the parent's folder as "<title>.pdf"

    Source PDFs are matched by the page id embedded in their filename (NNN_<id>_<title>.pdf).
#>

param(
    [string]$Site     = "https://haooding.atlassian.net",
    [string]$SpaceKey = "VTG",
    [string]$FlatDir  = "$PSScriptRoot\confluence-pdf-export",
    [string]$TreeDir  = "$PSScriptRoot\confluence-pdf-tree"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$email = $env:CONFLUENCE_EMAIL; $token = $env:CONFLUENCE_TOKEN
if (-not $email -or -not $token) { throw "Set CONFLUENCE_EMAIL and CONFLUENCE_TOKEN env vars." }
$b64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("$email`:$token"))
$h   = @{ Authorization = "Basic $b64"; Accept = "application/json" }

function Invoke-JsonApi([string]$uri) {
    $resp  = Invoke-WebRequest -Headers $h -Uri $uri -UseBasicParsing -TimeoutSec 60
    [Text.Encoding]::UTF8.GetString($resp.RawContentStream.ToArray()) | ConvertFrom-Json
}
function Get-SafeName([string]$name) {
    $re = "[{0}]" -f [Regex]::Escape(([IO.Path]::GetInvalidFileNameChars() -join ''))
    $s = ($name -replace $re, '_').Trim().TrimEnd('.')
    if ([string]::IsNullOrWhiteSpace($s)) { $s = "untitled" }
    if ($s.Length -gt 100) { $s = $s.Substring(0,100).Trim() }
    $s
}

# 1. gather all pages with id/title/parentId
$sp = Invoke-JsonApi "$Site/wiki/api/v2/spaces?keys=$SpaceKey"
$spaceId = $sp.results[0].id
$pages = @(); $cursor = $null
do {
    $uri = "$Site/wiki/api/v2/spaces/$spaceId/pages?limit=100"
    if ($cursor) { $uri += "&cursor=$cursor" }
    $r = Invoke-JsonApi $uri
    $pages += $r.results
    $cursor = $null
    if ($r._links.next -match "cursor=([^&]+)") { $cursor = [Uri]::UnescapeDataString($Matches[1]) }
} while ($cursor)
Write-Host "Loaded $($pages.Count) pages."

$byId = @{}; foreach ($p in $pages) { $byId[[string]$p.id] = $p }
$hasKids = @{}
foreach ($p in $pages) {
    $parId = [string]$p.parentId
    if ($parId -and $byId.ContainsKey($parId)) { $hasKids[$parId] = $true }
}

# index the flat PDFs by page id (filename pattern NNN_<id>_<title>.pdf)
$flat = @{}
Get-ChildItem $FlatDir -Filter *.pdf | ForEach-Object {
    if ($_.Name -match '^\d+_(\d+)_') { $flat[$Matches[1]] = $_.FullName }
}
Write-Host "Indexed $($flat.Count) flat PDFs."

# ancestor title chain (root -> parent) as a single backslash-joined string, cycle-guarded.
# Returns "" for a top-level page. (Sanitized titles never contain '\'.)
function Get-AncestorPath($page) {
    $dirs = @(); $seen = @{}; $cur = $page
    while ($true) {
        $ppid = [string]$cur.parentId
        if (-not $ppid -or -not $byId.ContainsKey($ppid) -or $seen.ContainsKey($ppid)) { break }
        $seen[$ppid] = $true
        $parent = $byId[$ppid]
        $dirs = ,(Get-SafeName $parent.title) + $dirs
        $cur = $parent
    }
    ($dirs -join '\')
}

if ([IO.Directory]::Exists($TreeDir)) { [IO.Directory]::Delete($TreeDir, $true) }
[IO.Directory]::CreateDirectory($TreeDir) | Out-Null

$placed = 0; $missing = 0
foreach ($p in $pages) {
    $id = [string]$p.id
    $title = Get-SafeName $p.title
    $anc = Get-AncestorPath $p
    $parts = @($TreeDir)
    if ($anc) { $parts += $anc }                          # already backslash-joined
    if ($hasKids.ContainsKey($id)) { $parts += $title }   # page with children -> its own folder
    $dir = $parts -join '\'
    [IO.Directory]::CreateDirectory($dir) | Out-Null

    if ($flat.ContainsKey($id)) {
        $dest = [IO.Path]::Combine($dir, ("{0}.pdf" -f $title))
        if ([IO.File]::Exists($dest)) { $dest = [IO.Path]::Combine($dir, ("{0}_{1}.pdf" -f $title, $id)) }  # collision guard
        [IO.File]::Copy($flat[$id], $dest, $true)
        $placed++
    } else {
        Write-Warning "No flat PDF for page $id ($($p.title))"
        $missing++
    }
}
Write-Host "`nDone. Placed $placed PDFs into tree, $missing missing. Root: $TreeDir"
