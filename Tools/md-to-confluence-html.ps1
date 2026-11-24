<#
.SYNOPSIS
  Turns one of the Docs/*.md files into an HTML file you open in a browser, select all, copy, and
  paste into Confluence. The paste keeps headings, tables, bold, code blocks and links.

.DESCRIPTION
  Written for Windows PowerShell 5.1, which has no ConvertFrom-Markdown (that is PS 6+), and this
  machine has neither pandoc nor a working python - hence a converter rather than a one-liner.

  It handles the subset the Vertigo docs actually use: ATX headings, pipe tables, fenced code
  blocks, blockquotes, bullet and numbered lists, horizontal rules, and inline **bold**, `code`
  and [links](target). Anything else passes through as a paragraph.

  Relative links between the docs (SaveSystem.md and friends) cannot resolve on a wiki, so they are
  written as plain bold text instead of dead links. Pass -KeepRelativeLinks to keep them as links.

.PARAMETER Path
  The markdown file. Defaults to the save-system maintenance doc.

.PARAMETER OutFile
  Where to write the HTML. Defaults to <Path> with the extension replaced by .confluence.html.

.PARAMETER Open
  Open the result in the default browser when done.

.EXAMPLE
  .\tools\md-to-confluence-html.ps1 -Open

.EXAMPLE
  .\tools\md-to-confluence-html.ps1 -Path Docs\SaveSystem.md -Open
#>
[CmdletBinding()]
param(
	[string] $Path = 'Docs\SaveSystemMaintenance.zh-CN.md',
	[string] $OutFile,
	[switch] $Open,
	[switch] $KeepRelativeLinks
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Path)) { throw "Markdown file not found: $Path" }
$resolved = (Resolve-Path -LiteralPath $Path).Path
if (-not $OutFile) { $OutFile = [IO.Path]::ChangeExtension($resolved, $null) + 'confluence.html' }

# Read as UTF-8 explicitly. Get-Content -Raw would use the ANSI codepage here and mojibake every
# Chinese character - the same class of bug as the export script's Invoke-RestMethod gotcha.
$text = [IO.File]::ReadAllText($resolved, [Text.Encoding]::UTF8)
$lines = $text -split "\r?\n"

function Convert-Inline {
	param([string] $Text)
	if ($null -eq $Text) { return '' }

	# HTML-escape first, so nothing in the prose can inject markup.
	$s = $Text -replace '&', '&amp;' -replace '<', '&lt;' -replace '>', '&gt;'

	# Code spans before bold, so a ** inside `code` is not treated as emphasis.
	$s = [regex]::Replace($s, '`([^`]+)`', { param($m) '<code>' + $m.Groups[1].Value + '</code>' })
	$s = [regex]::Replace($s, '\*\*(.+?)\*\*', '<strong>$1</strong>')

	# Links. A relative *.md target cannot work on a wiki, so by default it becomes plain bold text.
	$s = [regex]::Replace($s, '\[([^\]]+)\]\(([^)]+)\)', {
		param($m)
		$label = $m.Groups[1].Value
		$href = $m.Groups[2].Value
		if (-not $KeepRelativeLinks -and $href -notmatch '^(https?:|mailto:)') { return '<strong>' + $label + '</strong>' }
		return '<a href="' + $href + '">' + $label + '</a>'
	})
	return $s
}

function Split-Row {
	param([string] $Line)
	$trimmed = $Line.Trim()
	$trimmed = $trimmed -replace '^\|', '' -replace '\|$', ''
	# Not a full markdown parser: a literal | inside a cell would need escaping, which these docs
	# do not use. Said plainly so nobody debugs it later.
	return @($trimmed -split '\|' | ForEach-Object { Convert-Inline $_.Trim() })
}

$out = New-Object Text.StringBuilder
$i = 0
$n = $lines.Count

while ($i -lt $n) {
	$line = $lines[$i]

	# ---- fenced code block -------------------------------------------------------------------
	if ($line -match '^\s*```') {
		$lang = ($line -replace '^\s*```', '').Trim()
		$i++
		$body = New-Object Collections.Generic.List[string]
		while ($i -lt $n -and $lines[$i] -notmatch '^\s*```') { $body.Add($lines[$i]); $i++ }
		$i++ # closing fence
		$escaped = ($body -join "`n") -replace '&', '&amp;' -replace '<', '&lt;' -replace '>', '&gt;'
		$class = ''
		if ($lang) { $class = ' class="language-' + $lang + '"' }
		[void]$out.AppendLine('<pre><code' + $class + '>' + $escaped + '</code></pre>')
		continue
	}

	# ---- heading -----------------------------------------------------------------------------
	if ($line -match '^(#{1,6})\s+(.*)$') {
		$level = $Matches[1].Length
		[void]$out.AppendLine("<h$level>" + (Convert-Inline $Matches[2]) + "</h$level>")
		$i++
		continue
	}

	# ---- horizontal rule ---------------------------------------------------------------------
	if ($line -match '^\s*(---+|\*\*\*+)\s*$') {
		[void]$out.AppendLine('<hr />')
		$i++
		continue
	}

	# ---- table (a pipe row followed by a |---|---| separator) --------------------------------
	if ($line -match '^\s*\|' -and ($i + 1) -lt $n -and $lines[$i + 1] -match '^\s*\|[\s:\-|]+\|\s*$') {
		$head = Split-Row $line
		$i += 2
		[void]$out.AppendLine('<table><thead><tr>')
		foreach ($cell in $head) { [void]$out.AppendLine('<th>' + $cell + '</th>') }
		[void]$out.AppendLine('</tr></thead><tbody>')
		while ($i -lt $n -and $lines[$i] -match '^\s*\|') {
			[void]$out.AppendLine('<tr>')
			foreach ($cell in (Split-Row $lines[$i])) { [void]$out.AppendLine('<td>' + $cell + '</td>') }
			[void]$out.AppendLine('</tr>')
			$i++
		}
		[void]$out.AppendLine('</tbody></table>')
		continue
	}

	# ---- blockquote --------------------------------------------------------------------------
	if ($line -match '^\s*>') {
		$body = New-Object Collections.Generic.List[string]
		while ($i -lt $n -and $lines[$i] -match '^\s*>') {
			$body.Add(($lines[$i] -replace '^\s*>\s?', ''))
			$i++
		}
		[void]$out.AppendLine('<blockquote>')
		$para = New-Object Collections.Generic.List[string]
		foreach ($b in $body) {
			if ($b.Trim() -eq '') {
				if ($para.Count) { [void]$out.AppendLine('<p>' + (Convert-Inline ($para -join ' ')) + '</p>'); $para.Clear() }
			}
			else { $para.Add($b) }
		}
		if ($para.Count) { [void]$out.AppendLine('<p>' + (Convert-Inline ($para -join ' ')) + '</p>') }
		[void]$out.AppendLine('</blockquote>')
		continue
	}

	# ---- lists -------------------------------------------------------------------------------
	if ($line -match '^\s*[-*]\s+(.*)$' -or $line -match '^\s*\d+\.\s+(.*)$') {
		$ordered = $line -match '^\s*\d+\.\s'
		$tag = 'ul'
		if ($ordered) { $tag = 'ol' }
		[void]$out.AppendLine("<$tag>")
		while ($i -lt $n) {
			$l = $lines[$i]
			if ($ordered -and $l -match '^\s*\d+\.\s+(.*)$') { [void]$out.AppendLine('<li>' + (Convert-Inline $Matches[1]) + '</li>') }
			elseif (-not $ordered -and $l -match '^\s*[-*]\s+(.*)$') { [void]$out.AppendLine('<li>' + (Convert-Inline $Matches[1]) + '</li>') }
			else { break }
			$i++
		}
		[void]$out.AppendLine("</$tag>")
		continue
	}

	# ---- blank -------------------------------------------------------------------------------
	if ($line.Trim() -eq '') { $i++; continue }

	# ---- paragraph (consecutive plain lines joined) ------------------------------------------
	$para = New-Object Collections.Generic.List[string]
	while ($i -lt $n) {
		$l = $lines[$i]
		if ($l.Trim() -eq '') { break }
		if ($l -match '^(#{1,6})\s' -or $l -match '^\s*\|' -or $l -match '^\s*>' -or $l -match '^\s*```' -or
			$l -match '^\s*[-*]\s+' -or $l -match '^\s*\d+\.\s+' -or $l -match '^\s*(---+|\*\*\*+)\s*$') { break }
		$para.Add($l)
		$i++
	}
	if ($para.Count) { [void]$out.AppendLine('<p>' + (Convert-Inline ($para -join ' ')) + '</p>') }
}

$title = [IO.Path]::GetFileNameWithoutExtension($resolved)
$html = @"
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8" />
<title>$title</title>
<style>
body{font:15px/1.7 "Segoe UI","Microsoft YaHei",sans-serif;max-width:1000px;margin:24px auto;padding:0 20px;color:#172b4d}
h1{border-bottom:2px solid #dfe1e6;padding-bottom:8px}
h2{margin-top:32px;border-bottom:1px solid #dfe1e6;padding-bottom:6px}
table{border-collapse:collapse;width:100%;margin:12px 0}
th,td{border:1px solid #c1c7d0;padding:7px 10px;text-align:left;vertical-align:top}
th{background:#f4f5f7}
pre{background:#f4f5f7;border:1px solid #c1c7d0;padding:10px;overflow-x:auto}
code{background:#f4f5f7;padding:1px 4px;font-family:Consolas,monospace;font-size:.92em}
pre code{background:none;padding:0}
blockquote{border-left:3px solid #c1c7d0;margin:12px 0;padding:4px 14px;color:#5e6c84}
.howto{background:#deebff;border:1px solid #4c9aff;padding:12px 16px;margin-bottom:24px;font-size:14px}
</style>
</head>
<body>
<div class="howto">
<strong>贴进 Confluence：</strong> 在这个页面里 Ctrl+A → Ctrl+C，然后到 Confluence 页面里 Ctrl+V。
表格、标题、代码块都会保留。<em>（这个蓝框本身也会被一起复制，贴完删掉它。）</em>
</div>
$($out.ToString())
</body>
</html>
"@

[IO.File]::WriteAllText($OutFile, $html, (New-Object Text.UTF8Encoding $false))
Write-Output "Wrote $OutFile"

if ($Open) { Start-Process $OutFile }
