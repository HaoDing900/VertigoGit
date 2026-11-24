# Offline integration tests. Does not contact Atlassian or read credentials.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\export-confluence-tree.ps1"

function Assert-Cf($Condition, [string]$Message) {
    if (!$Condition) { throw "Assertion failed: $Message" }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('cf-tree-test-' + [guid]::NewGuid().ToString('N'))
$OutDir = $testRoot
$script:CfSite = 'https://haooding.atlassian.net'
$script:SimulateFailure = $false
$script:Requests = [Collections.Generic.List[string]]::new()
function Invoke-CfRequest([string]$Link, [string]$File) {
    $script:Requests.Add($Link)
    switch -Regex ($Link) {
        '^/wiki/api/v2/spaces\?keys=' {
            return @{results=@(@{id='99'; key='VTG'; name='VTG Dev'}); _links=@{}}
        }
        '^/wiki/api/v2/spaces/99/pages' {
            return @{results=@(@{id='1'; title='Audio'; parentId=''; position=1}); _links=@{next='/api/v2/pages?cursor=second'}}
        }
        '^/api/v2/pages\?cursor=second$' {
            return @{results=@(
                @{id='2'; title='Blip 中文'; parentId='5'; parentType='folder'; position=2},
                @{id='3'; title='A/B'; parentId='1'; parentType='page'; position=3},
                @{id='4'; title='A:B'; parentId='1'; parentType='page'; position=4}
            ); _links=@{}}
        }
        '^/wiki/api/v2/folders/5$' { return @{id='5'; title='SFX Resources'; parentId='1'; parentType='page'; position=1} }
        '^/wiki/api/v2/pages/(\d+)\?body-format=view$' {
            if ($script:SimulateFailure -and $Matches[1] -eq '3') { throw 'Simulated page failure' }
            return @{body=@{view=@{value='<p>中文</p><a href="/wiki/spaces/VTG/pages/2/Blip#part">child</a><img src="/wiki/download/attachments/2/sound.png?version=1&amp;x=2"><a href="https://example.com/">external</a>'}}}
        }
        '^/wiki/api/v2/pages/2/attachments\?' {
            return @{results=@(@{id='att7'; title='sound.png'; downloadLink='/download/attachments/2/sound.png?version=1'}); _links=@{}}
        }
        '^/wiki/api/v2/pages/\d+/attachments\?' { return @{results=@(); _links=@{}} }
        '^/download/attachments/2/' { Write-CfText $File 'fake attachment'; return }
        default { throw "Unexpected request: $Link" }
    }
}

try {
    Export-CfSpace
    $root = $script:CfRoot
    $manifest = Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Cf ($manifest.Count -eq 4) 'pagination must include all four pages'
    $failureJson = Get-Content -LiteralPath (Join-Path $root 'failures.json') -Raw
    Assert-Cf (($failureJson -replace '\s', '') -eq '[]') "successful export failure list: $failureJson"
    $childPath = $script:CfPaths['2']
    Assert-Cf ($childPath.Contains('Audio [1]')) 'parent hierarchy exists'
    Assert-Cf ($childPath.Contains('SFX Resources [5]')) 'folder ancestor preserved'
    Assert-Cf ($script:CfPaths['3'] -ne $script:CfPaths['4']) 'sanitized same-name paths must not collide'
    $html = Get-Content -LiteralPath (Join-Path $childPath 'index.html') -Raw -Encoding UTF8
    Assert-Cf ($html.Contains('href="index.html#part"')) 'page link rewritten and fragment retained'
    Assert-Cf ($html -match 'src="attachments/sound%20(?:%5B|\[)att7(?:%5D|\]).png"') "attachment image rewritten: $html"
    Assert-Cf ($html.Contains('中文')) 'Unicode preserved'
    Assert-Cf ($html.Contains('href="https://example.com/"')) 'external link preserved'
    Assert-Cf (Test-Path -LiteralPath (Join-Path $childPath 'attachments/sound [att7].png')) 'attachment downloaded'
    Assert-Cf ((ConvertTo-CfName 'CON') -eq '_CON') 'Windows device filename sanitized'
    Assert-Cf ((Resolve-CfUrl '/api/v2/pages').AbsoluteUri -eq 'https://haooding.atlassian.net/wiki/api/v2/pages') 'relative v2 next URL resolved'
    $script:SimulateFailure = $true
    $SkipAttachments = [switch]$true
    $script:Requests.Clear()
    $failed = $false
    try { Export-CfSpace } catch {
        if ($_.Exception.Message -notlike '*partial export*') { throw }
        $failed = $true
    }
    Assert-Cf $failed 'partial export must return failure'
    $errors = Get-Content -LiteralPath (Join-Path $script:CfRoot 'failures.json') -Raw | ConvertFrom-Json
    Assert-Cf (@($errors).Count -eq 1 -and @($errors)[0].id -eq '3') 'failed page recorded'
    Assert-Cf (@($script:Requests | Where-Object { $_ -like '*attachments*' }).Count -eq 0) 'skip attachments avoids attachment requests'
    Assert-Cf (Test-Path -LiteralPath (Join-Path $script:CfRoot 'index.html')) 'partial export retains index'
    Write-Host 'PASS: pagination, tree, folder parents, links, attachments, Unicode, collisions, partial failures and skip mode.'
} finally {
    $resolved = [IO.Path]::GetFullPath($testRoot)
    $tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ($resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf) -like 'cf-tree-test-*') {
        if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    }
}
