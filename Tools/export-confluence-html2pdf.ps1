<#
    Export every page in a Confluence Cloud space to its own PDF, by:
      1. fetching each page's rendered (export_view) HTML via REST API,
      2. inlining images as base64 (fetched with auth so they render),
      3. printing to PDF with headless Microsoft Edge.

    Auth via env vars:
        $env:CONFLUENCE_EMAIL, $env:CONFLUENCE_TOKEN

    Params:
        -Limit N   : only process first N pages (0 = all). Use for testing.
#>

param(
    [string]$Site     = "https://haooding.atlassian.net",
    [string]$SpaceKey = "VTG",
    [string]$OutDir   = "$PSScriptRoot\confluence-pdf-export",
    [int]   $Limit    = 0
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$email = $env:CONFLUENCE_EMAIL; $token = $env:CONFLUENCE_TOKEN
if (-not $email -or -not $token) { throw "Set CONFLUENCE_EMAIL and CONFLUENCE_TOKEN env vars." }
$b64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("$email`:$token"))
$h   = @{ Authorization = "Basic $b64"; Accept = "application/json" }

# Windows PowerShell 5.1's Invoke-RestMethod mis-decodes UTF-8 JSON as Latin-1 when the
# response omits charset=utf-8 (Atlassian does). Read raw bytes and decode UTF-8 ourselves.
function Invoke-JsonApi([string]$uri) {
    $resp  = Invoke-WebRequest -Headers $h -Uri $uri -UseBasicParsing -TimeoutSec 60
    $bytes = $resp.RawContentStream.ToArray()
    [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
}

# locate Edge
$edgeExe = @(
    "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    "C:\Program Files\Microsoft\Edge\Application\msedge.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edgeExe) { throw "Edge not found." }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$tmp = Join-Path $env:TEMP "conf-html2pdf"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

function Get-SafeName([string]$name) {
    $re = "[{0}]" -f [Regex]::Escape(([IO.Path]::GetInvalidFileNameChars() -join ''))
    $s = ($name -replace $re, '_').Trim()
    if ($s.Length -gt 120) { $s = $s.Substring(0,120) }
    $s
}

# Download an image and return a data: URI, or $null on failure
function Get-DataUri([string]$url) {
    try {
        $r  = Invoke-WebRequest -Headers @{ Authorization = "Basic $b64" } -Uri $url -UseBasicParsing -TimeoutSec 30
        $ct = $r.Headers['Content-Type']; if ($ct -is [array]) { $ct = $ct[0] }
        if (-not $ct) { $ct = 'image/png' }
        $bytes = $r.Content
        if ($bytes -isnot [byte[]]) { $bytes = [Text.Encoding]::UTF8.GetBytes([string]$bytes) }
        "data:$ct;base64," + [Convert]::ToBase64String($bytes)
    } catch { $null }
}

# Inline <img src> references that live on the Confluence host
function Inline-Images([string]$html) {
    $rx = [regex]'(?i)<img\b[^>]*?\ssrc\s*=\s*"([^"]+)"'
    $rx.Replace($html, {
        param($m)
        $src = $m.Groups[1].Value
        if ($src -match '^data:') { return $m.Value }
        $abs = $src
        if ($src -match '^/')        { $abs = "$Site$src" }
        elseif ($src -notmatch '^https?://') { return $m.Value }
        # only fetch-with-auth for our own host; leave true externals for Edge to load
        if ($abs -notlike "$Site*") { return $m.Value }
        $data = Get-DataUri $abs
        if ($data) { $m.Value -replace [regex]::Escape($src), $data } else { $m.Value }
    })
}

# 1. resolve space + list pages
$sp = Invoke-JsonApi "$Site/wiki/api/v2/spaces?keys=$SpaceKey"
$spaceId = $sp.results[0].id
if (-not $spaceId) { throw "Space $SpaceKey not found." }

$pages = @(); $cursor = $null
do {
    $uri = "$Site/wiki/api/v2/spaces/$spaceId/pages?limit=100"
    if ($cursor) { $uri += "&cursor=$cursor" }
    $resp = Invoke-JsonApi $uri
    $pages += $resp.results
    $cursor = $null
    if ($resp._links.next -match "cursor=([^&]+)") { $cursor = [Uri]::UnescapeDataString($Matches[1]) }
} while ($cursor)

if ($Limit -gt 0) { $pages = $pages | Select-Object -First $Limit }
Write-Host "Processing $($pages.Count) pages -> $OutDir`n"

$css = @"
<style>
  body{font-family:'Segoe UI',Arial,sans-serif;font-size:12pt;line-height:1.5;color:#172b4d;margin:24px;}
  h1,h2,h3{color:#172b4d;} table{border-collapse:collapse;} th,td{border:1px solid #ccc;padding:6px;}
  img{max-width:100%;height:auto;} pre,code{background:#f4f5f7;border-radius:3px;}
  pre{padding:8px;white-space:pre-wrap;} .panel{border:1px solid #ccc;border-radius:3px;padding:8px;margin:8px 0;}
</style>
"@

$ok = 0; $fail = 0; $i = 0
foreach ($p in $pages) {
    $i++
    $safe = Get-SafeName $p.title
    $pdf  = Join-Path $OutDir ("{0:D3}_{1}_{2}.pdf" -f $i, $p.id, $safe)
    try {
        $r = Invoke-JsonApi "$Site/wiki/api/v2/pages/$($p.id)?body-format=export_view"
        $body = $r.body.export_view.value
        $body = Inline-Images $body
        $doc  = "<!DOCTYPE html><html><head><meta charset='utf-8'>$css</head><body><h1>$($p.title)</h1>$body</body></html>"
        $htmlFile = Join-Path $tmp ("p$($p.id).html")
        [IO.File]::WriteAllText($htmlFile, $doc, [Text.UTF8Encoding]::new($false))
        $uri = ([Uri]$htmlFile).AbsoluteUri
        $args = @(
            "--headless=new","--disable-gpu","--no-first-run","--disable-extensions",
            "--run-all-compositor-stages-before-draw","--virtual-time-budget=8000",
            "--print-to-pdf-no-header","--print-to-pdf=`"$pdf`"","`"$uri`""
        )
        $proc = Start-Process -FilePath $edgeExe -ArgumentList $args -Wait -PassThru -WindowStyle Hidden
        Start-Sleep -Milliseconds 200
        if ((Test-Path $pdf) -and ((Get-Item $pdf).Length -gt 0)) {
            Write-Host ("  [{0}/{1}] OK   {2}" -f $i,$pages.Count,$p.title)
            $ok++
        } else {
            Write-Warning ("  [{0}/{1}] no PDF for {2}" -f $i,$pages.Count,$p.title); $fail++
        }
        Remove-Item $htmlFile -ErrorAction SilentlyContinue
    } catch {
        Write-Warning ("  [{0}/{1}] FAIL {2}: {3}" -f $i,$pages.Count,$p.title,$_.Exception.Message); $fail++
    }
}
Write-Host "`nDone. $ok PDFs, $fail failed. Folder: $OutDir"
