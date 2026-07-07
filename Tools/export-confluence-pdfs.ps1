<#
    Export every page in a Confluence Cloud space to its own PDF.

    Auth (set these before running):
        $env:CONFLUENCE_EMAIL = "haooding@gmail.com"
        $env:CONFLUENCE_TOKEN = "<your api token>"

    Usage:
        ./export-confluence-pdfs.ps1                 # defaults: space VTG -> ./confluence-pdf-export
        ./export-confluence-pdfs.ps1 -SpaceKey VTG -OutDir "C:\some\folder"
#>

param(
    [string]$Site     = "https://haooding.atlassian.net",
    [string]$SpaceKey = "VTG",
    [string]$OutDir   = "$PSScriptRoot\confluence-pdf-export"
)

$ErrorActionPreference = "Stop"

$email = $env:CONFLUENCE_EMAIL
$token = $env:CONFLUENCE_TOKEN
if (-not $email -or -not $token) {
    throw "Set `$env:CONFLUENCE_EMAIL and `$env:CONFLUENCE_TOKEN first."
}

# Basic auth header: base64("email:token")
$pair    = "{0}:{1}" -f $email, $token
$b64     = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($pair))
$headers = @{ Authorization = "Basic $b64"; Accept = "application/json" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Get-SafeName([string]$name) {
    $invalid = [IO.Path]::GetInvalidFileNameChars() -join ''
    $re = "[{0}]" -f [Regex]::Escape($invalid)
    ($name -replace $re, '_').Trim()
}

# 1. Resolve the space id from its key (REST API v2)
Write-Host "Resolving space '$SpaceKey'..."
$spaceResp = Invoke-RestMethod -Headers $headers -Method Get `
    -Uri "$Site/wiki/api/v2/spaces?keys=$SpaceKey"
$spaceId = $spaceResp.results[0].id
if (-not $spaceId) { throw "Space '$SpaceKey' not found (check the key and your token)." }
Write-Host "  space id = $spaceId"

# 2. Page through every page in the space
$pages  = @()
$cursor = $null
do {
    $uri = "$Site/wiki/api/v2/spaces/$spaceId/pages?limit=100"
    if ($cursor) { $uri += "&cursor=$cursor" }
    $resp   = Invoke-RestMethod -Headers $headers -Method Get -Uri $uri
    $pages += $resp.results
    $next   = $resp._links.next
    $cursor = $null
    if ($next -and $next -match "cursor=([^&]+)") {
        $cursor = [Uri]::UnescapeDataString($Matches[1])
    }
} while ($cursor)

Write-Host "Found $($pages.Count) pages. Exporting to $OutDir`n"

# 3. Export each page to PDF via the flyingpdf action endpoint
$pdfHeaders = @{ Authorization = "Basic $b64" }
$ok = 0; $fail = 0
foreach ($p in $pages) {
    $safe = Get-SafeName $p.title
    $file = Join-Path $OutDir ("{0}_{1}.pdf" -f $p.id, $safe)
    $url  = "$Site/wiki/spaces/flyingpdf/pdfpageexport.action?pageId=$($p.id)"
    try {
        $r = Invoke-WebRequest -Headers $pdfHeaders -Method Get -Uri $url -OutFile $file -PassThru
        # Sanity check: real PDFs start with %PDF
        $head = [IO.File]::ReadAllBytes($file)[0..3] -join ','
        if ($head -eq "37,80,68,70") {
            Write-Host ("  OK   {0}  ({1:N0} KB)" -f $p.title, ((Get-Item $file).Length/1KB))
            $ok++
        } else {
            Write-Warning ("  Not a PDF for '{0}' - server returned HTML (may need async/large-export handling)." -f $p.title)
            Rename-Item $file ($file -replace '\.pdf$','.html')
            $fail++
        }
    } catch {
        Write-Warning ("  FAIL {0}: {1}" -f $p.title, $_.Exception.Message)
        $fail++
    }
    Start-Sleep -Milliseconds 300   # be polite to the API
}

Write-Host "`nDone. $ok PDFs written, $fail failed. Folder: $OutDir"
