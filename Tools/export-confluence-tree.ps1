<#
.SYNOPSIS
Export current Confluence Cloud pages and attachments into a local page tree.
.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\Tools\export-confluence-tree.ps1 -OutDir G:\VTG-Docs
#>
[CmdletBinding()]
param(
    [string]$Site = 'https://haooding.atlassian.net',
    [string]$SpaceKey = 'VTG',
    [string]$Email = $env:CONFLUENCE_EMAIL,
    [string]$OutDir = "$PSScriptRoot\confluence-tree-export",
    [switch]$SkipAttachments
)

$ErrorActionPreference = 'Stop'

function Write-CfText([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding $false))
}

function ConvertTo-CfName([string]$Name) {
    $value = ($Name -replace '[<>:"/\\|?*\x00-\x1f]', '_').Trim().TrimEnd('.')
    if (!$value) { $value = 'untitled' }
    if ($value -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') { $value = "_$value" }
    # Keep individual components short; a deep tree can still need Windows long paths.
    if ($value.Length -gt 48) { $value = $value.Substring(0, 48).TrimEnd('.', ' ') }
    return $value
}

function ConvertTo-CfHtml([string]$Text) { [Net.WebUtility]::HtmlEncode($Text) }

function Resolve-CfUrl([string]$Link) {
    if ($Link -match '^https?://') { return [uri]$Link }
    # Confluence sometimes returns /wiki/api/v2/... and sometimes /api/v2/...
    if ($Link.StartsWith('/api/') -or $Link.StartsWith('/download/')) { $Link = "/wiki$Link" }
    return [uri]::new([uri]"$script:CfSite/", $Link)
}

function Invoke-CfRequest([string]$Link, [string]$File) {
    $initial = Resolve-CfUrl $Link
    # Authenticated entry points must stay on the configured Confluence origin.
    if ($initial.GetLeftPart([UriPartial]::Authority) -ne $script:CfSite) {
        throw 'Refused an API/download entry point outside the configured site.'
    }
    for ($attempt = 0; $attempt -lt 6; $attempt++) {
        $url = $initial
        for ($redirect = 0; $redirect -lt 10; $redirect++) {
            if ($url.Scheme -ne 'https') { throw 'Refused a non-HTTPS request.' }
            $request = [Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Get, $url)
            if ($url.GetLeftPart([UriPartial]::Authority) -eq $script:CfSite) {
                $request.Headers.TryAddWithoutValidation('Authorization', $script:CfAuth) | Out-Null
            }
            $request.Headers.TryAddWithoutValidation('Accept', '*/*') | Out-Null
            try {
                $response = $script:CfClient.SendAsync($request, [Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
            } finally { $request.Dispose() }
            try {
                $code = [int]$response.StatusCode
                if ($code -in 301,302,303,307,308) {
                    $url = [uri]::new($url, $response.Headers.Location)
                    continue
                }
                if ($code -eq 429 -or $code -in 500,502,503,504) {
                    $delay = [Math]::Min(60, [Math]::Pow(2, $attempt + 1))
                    if ($response.Headers.RetryAfter) {
                        if ($response.Headers.RetryAfter.Delta) { $delay = $response.Headers.RetryAfter.Delta.TotalSeconds }
                        elseif ($response.Headers.RetryAfter.Date) { $delay = ($response.Headers.RetryAfter.Date - [DateTimeOffset]::UtcNow).TotalSeconds }
                    }
                    # Sleep in short chunks so Ctrl+C remains responsive.
                    Write-Host "HTTP $code; retrying (attempt $($attempt + 1)/6)..."
                    $remaining = [Math]::Max(1, [Math]::Ceiling($delay))
                    while ($remaining -gt 0) { Start-Sleep -Seconds ([Math]::Min(30, $remaining)); $remaining -= 30 }
                    break
                }
                if (!$response.IsSuccessStatusCode) {
                    throw "HTTP $code for $($initial.AbsolutePath). Check token, account permissions and export policies."
                }
                if ($File) {
                    $temp = "$File.part"
                    try {
                        $stream = [IO.File]::Create($temp)
                        try { $response.Content.CopyToAsync($stream).GetAwaiter().GetResult() } finally { $stream.Dispose() }
                        Move-Item -LiteralPath $temp -Destination $File -Force
                    } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force } }
                    return
                }
                return ($response.Content.ReadAsStringAsync().GetAwaiter().GetResult() | ConvertFrom-Json)
            } finally { $response.Dispose() }
        }
        if ($redirect -ge 10) { throw 'Too many download redirects.' }
    }
    throw "Retry limit reached for $($initial.AbsolutePath)."
}

function Get-CfItems([string]$Link) {
    $seen = @{}
    while ($Link) {
        if ($seen.ContainsKey($Link)) { throw 'Repeated API pagination link.' }
        $seen[$Link] = $true
        $data = Invoke-CfRequest $Link
        foreach ($item in $data.results) { $item }
        $Link = $data._links.next
    }
}

function Get-CfRelative([string]$FromDirectory, [string]$ToPath) {
    $from = [uri]::new($FromDirectory.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar)
    return $from.MakeRelativeUri([uri]::new($ToPath)).ToString()
}

function Get-CfNodePath([string]$Id, [hashtable]$Visiting) {
    if ($script:CfPaths.ContainsKey($Id)) { return $script:CfPaths[$Id] }
    if ($Visiting.ContainsKey($Id)) { throw "Cycle in page hierarchy: $Id" }
    $Visiting[$Id] = $true
    $node = $script:CfNodes[$Id]
    $parent = [string]$node.parentId
    $base = $script:CfRoot
    if ($parent -and $script:CfNodes.ContainsKey($parent)) { $base = Get-CfNodePath $parent $Visiting }
    $path = Join-Path $base ("{0} [{1}]" -f (ConvertTo-CfName $node.title), $Id)
    $script:CfPaths[$Id] = $path
    $Visiting.Remove($Id)
    return $path
}

function Start-CfExport {
    Add-Type -AssemblyName System.Net.Http
    $parsed = [uri]$Site
    if ($parsed.Scheme -ne 'https' -or !$parsed.Host -or $parsed.UserInfo) { throw 'Site must be an HTTPS site URL.' }
    $script:CfSite = $parsed.GetLeftPart([UriPartial]::Authority)
    if (!$Email) { $Email = Read-Host 'Atlassian account email' }
    $token = $env:CONFLUENCE_TOKEN
    if (!$token) {
        $secure = Read-Host 'Atlassian API token (hidden)' -AsSecureString
        $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer) }
    }
    if (!$Email -or !$token) { throw 'Email and API token are required.' }
    $script:CfAuth = 'Basic ' + [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes("${Email}:$token"))
    $token = $null
    $handler = [Net.Http.HttpClientHandler]::new()
    $handler.AllowAutoRedirect = $false
    $script:CfClient = [Net.Http.HttpClient]::new($handler)
    $script:CfClient.Timeout = [TimeSpan]::FromMinutes(10)
    try { Export-CfSpace } finally { $script:CfClient.Dispose(); $script:CfAuth = $null }
}

function Export-CfSpace {
    $spaces = @(Get-CfItems ("/wiki/api/v2/spaces?keys={0}&limit=100" -f [uri]::EscapeDataString($SpaceKey)))
    $space = $spaces | Where-Object { $_.key -eq $SpaceKey } | Select-Object -First 1
    if (!$space) { throw "Space '$SpaceKey' not found or not accessible." }
    Write-Host "Loading published pages in $SpaceKey..."
    $pages = @(Get-CfItems "/wiki/api/v2/spaces/$($space.id)/pages?limit=100&status=current&depth=all")
    if (!$pages.Count) { throw 'No visible published pages found; no export was created.' }
    $script:CfNodes = @{}
    $script:CfPaths = @{}
    $failures = [Collections.Generic.List[object]]::new()
    foreach ($page in $pages) { $script:CfNodes[[string]$page.id] = $page }
    # Resolve folder ancestors too; otherwise pages under folders would be flattened.
    foreach ($page in $pages) {
        $node = $page
        $seen = @{}
        while ($node.parentId -and !$script:CfNodes.ContainsKey([string]$node.parentId)) {
            $id = [string]$node.parentId
            if ($seen.ContainsKey($id)) { throw "Cycle in parent metadata: $id" }
            $seen[$id] = $true
            try {
                if ($node.parentType -ne 'folder') { throw "Unlisted parent $id (type $($node.parentType))" }
                $node = Invoke-CfRequest "/wiki/api/v2/folders/$id"
                $script:CfNodes[$id] = $node
            } catch {
                $failures.Add(@{type='hierarchy'; id=$id; message=$_.Exception.Message})
                break
            }
        }
    }
    # A new dated folder avoids mixing stale exports with a later snapshot.
    $script:CfRoot = Join-Path ([IO.Path]::GetFullPath($OutDir)) ("{0}-{1}-{2}" -f (ConvertTo-CfName $SpaceKey), (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0,6))
    New-Item -ItemType Directory -Path $script:CfRoot -Force | Out-Null
    foreach ($id in @($script:CfNodes.Keys)) {
        $path = Get-CfNodePath $id @{}
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
    $manifest = [Collections.Generic.List[object]]::new()
    $htmlBodies = @{}
    $fileLinks = @{}
    $count = 0
    foreach ($page in $pages) {
        $id = [string]$page.id
        $path = $script:CfPaths[$id]
        $count++
        Write-Host "[$count/$($pages.Count)] $($page.title)"
        $entry = [ordered]@{id=$id; title=$page.title; parentId=$page.parentId; type='page'; position=$page.position; path=(Get-CfRelative $script:CfRoot (Join-Path $path 'index.html')); exported=$false; attachments=@()}
        try {
            $detail = Invoke-CfRequest "/wiki/api/v2/pages/${id}?body-format=view"
            if ($null -eq $detail.body.view.value) { throw 'API did not return a rendered page body.' }
            Write-CfText (Join-Path $path 'page.json') ($detail | ConvertTo-Json -Depth 100)
            $htmlBodies[$id] = [string]$detail.body.view.value
            $entry.exported = $true
        } catch {
            $failures.Add(@{type='page'; id=$id; message=$_.Exception.Message})
            $htmlBodies[$id] = '<p>Page export failed. See failures.json.</p>'
        }
        if (!$SkipAttachments) {
            try {
                $attachments = @(Get-CfItems "/wiki/api/v2/pages/$id/attachments?limit=100")
                foreach ($attachment in $attachments) {
                    try {
                        $folder = Join-Path $path 'attachments'
                        New-Item -ItemType Directory -Path $folder -Force | Out-Null
                        $ext = [IO.Path]::GetExtension([string]$attachment.title)
                        $name = ConvertTo-CfName ([IO.Path]::GetFileNameWithoutExtension([string]$attachment.title))
                        $safeExt = $ext -replace '[^a-zA-Z0-9.]', '_'
                        if ($safeExt.Length -gt 16) { $safeExt = $safeExt.Substring(0,16) }
                        $target = Join-Path $folder ("{0} [{1}]{2}" -f $name, (ConvertTo-CfName ([string]$attachment.id)), $safeExt)
                        $download = $attachment.downloadLink
                        if (!$download) { $download = $attachment._links.download }
                        if (!$download) { throw 'Missing attachment download URL.' }
                        Invoke-CfRequest $download $target
                        $entry.attachments += @{id=$attachment.id; title=$attachment.title; path=(Get-CfRelative $script:CfRoot $target)}
                        $uri = Resolve-CfUrl $download
                        $fileLinks[$uri.AbsolutePath] = $target
                    } catch { $failures.Add(@{type='attachment'; id=$attachment.id; pageId=$id; message=$_.Exception.Message}) }
                }
            } catch { $failures.Add(@{type='attachment-list'; id=$id; message=$_.Exception.Message}) }
        }
        $manifest.Add($entry)
    }
    # Rewrite standard page and attachment links after all local targets are known.
    foreach ($page in $pages) {
        $id = [string]$page.id
        $path = $script:CfPaths[$id]
        $rewrite = [Text.RegularExpressions.MatchEvaluator]{ param($m)
            $raw = [Net.WebUtility]::HtmlDecode($m.Groups[3].Value)
            try { $uri = Resolve-CfUrl $raw } catch { return $m.Value }
            $target = $null
            if ($uri.GetLeftPart([UriPartial]::Authority) -eq $script:CfSite) {
                if ($fileLinks.ContainsKey($uri.AbsolutePath)) { $target = $fileLinks[$uri.AbsolutePath] }
                elseif ($uri.AbsolutePath -match '/pages/(\d+)' -or $uri.Query -match '(?:\?|&)pageId=(\d+)') {
                    $otherId = $Matches[1]
                    if ($script:CfPaths.ContainsKey($otherId)) { $target = Join-Path $script:CfPaths[$otherId] 'index.html' }
                }
            }
            if ($target) { $raw = (Get-CfRelative $path $target) + $uri.Fragment }
            elseif ($raw -notmatch '^(#|data:|mailto:|tel:|javascript:)') { $raw = $uri.AbsoluteUri }
            return $m.Groups[1].Value + '=' + $m.Groups[2].Value + (ConvertTo-CfHtml $raw) + $m.Groups[2].Value
        }
        $body = [regex]::Replace($htmlBodies[$id], '(?i)\b(href|src)\s*=\s*(["''])(.*?)\2', $rewrite)
        $attachmentHtml = foreach ($a in ($manifest | Where-Object { $_.id -eq $id }).attachments) {
            '<li><a href="{0}">{1}</a></li>' -f (ConvertTo-CfHtml (Get-CfRelative $path (Join-Path $script:CfRoot ([uri]::UnescapeDataString($a.path))))), (ConvertTo-CfHtml $a.title)
        }
        $back = Get-CfRelative $path (Join-Path $script:CfRoot 'index.html')
        $source = "$script:CfSite/wiki/spaces/$SpaceKey/pages/$id"
        Write-CfText (Join-Path $path 'index.html') ("<!doctype html><html><head><meta charset='utf-8'><title>{0}</title><style>body{{font:16px/1.6 system-ui;max-width:1100px;margin:32px auto;padding:20px}}img{{max-width:100%;height:auto}}table{{border-collapse:collapse}}td,th{{border:1px solid #bbb;padding:8px}}pre{{white-space:pre-wrap}} </style></head><body><nav><a href='{1}'>Contents</a> | <a href='{2}'>Confluence source</a></nav><h1>{0}</h1>{3}<h2>Attachments</h2><ul>{4}</ul></body></html>" -f (ConvertTo-CfHtml $page.title), (ConvertTo-CfHtml $back), (ConvertTo-CfHtml $source), $body, ($attachmentHtml -join "`n"))
    }
    # Generate nested indexes, including empty parent folder landing pages.
    function Get-CfTreeHtml([string]$ParentId) {
        $children = @($script:CfNodes.Values | Where-Object {
            $p = [string]$_.parentId
            if ($ParentId) { $p -eq $ParentId } else { !$p -or !$script:CfNodes.ContainsKey($p) }
        } | Sort-Object @{Expression={if ($null -eq $_.position) {[long]::MaxValue} else {[long]$_.position}}}, title)
        $items = foreach ($child in $children) {
            $childId = [string]$child.id
            $link = Get-CfRelative $script:CfRoot (Join-Path $script:CfPaths[$childId] 'index.html')
            '<li><a href="{0}">{1}</a>{2}</li>' -f (ConvertTo-CfHtml $link), (ConvertTo-CfHtml $child.title), (Get-CfTreeHtml $childId)
        }
        if ($items) { return '<ul>' + ($items -join "`n") + '</ul>' }
        return ''
    }
    foreach ($id in $script:CfNodes.Keys) {
        if (!$htmlBodies.ContainsKey($id)) {
            $relative = Get-CfRelative $script:CfPaths[$id] (Join-Path $script:CfRoot 'index.html')
            Write-CfText (Join-Path $script:CfPaths[$id] 'index.html') ("<!doctype html><meta charset='utf-8'><h1>{0}</h1><a href='{1}'>Contents</a>" -f (ConvertTo-CfHtml $script:CfNodes[$id].title), (ConvertTo-CfHtml $relative))
        }
    }
    Write-CfText (Join-Path $script:CfRoot 'index.html') ("<!doctype html><meta charset='utf-8'><title>{0}</title><h1>{0}</h1><p>Published pages: {1}. Failures: {2}. Attachments skipped: {3}.</p>{4}" -f (ConvertTo-CfHtml $space.name), $pages.Count, $failures.Count, $SkipAttachments.IsPresent, (Get-CfTreeHtml ''))
    Write-CfText (Join-Path $script:CfRoot 'manifest.json') (ConvertTo-Json -InputObject @($manifest.ToArray()) -Depth 100)
    Write-CfText (Join-Path $script:CfRoot 'failures.json') (ConvertTo-Json -InputObject @($failures.ToArray()) -Depth 20)
    Write-Host "Saved to: $script:CfRoot"
    if ($failures.Count) { throw "Export finished with $($failures.Count) failures. Review failures.json; this is a partial export." }
    Write-Host 'Export complete. Open index.html to browse.'
}

if ($MyInvocation.InvocationName -ne '.') { Start-CfExport }
