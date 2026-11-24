# -*- coding: utf-8 -*-
"""Turn one of Docs/*.md into the paste-into-Confluence HTML these docs ship as.

    python md_to_confluence.py Docs/HardInitiate.md [out.html]

Output defaults to <name>.zh-CN.confluence.html next to the source, which is the name the
existing exports use. The stylesheet is copied from HardInitiate.zh-CN.confluence.html so a
re-export of any doc keeps looking like the ones already pasted into the wiki.

Deliberately small: headings, tables, fenced code, lists, blockquotes, bold, inline code and
links are everything these docs use. Anything fancier is a reason to look at the Markdown, not
to grow this.
"""
import html
import io
import os
import re
import sys

STYLE = """body{font:15px/1.7 "Segoe UI","Microsoft YaHei",sans-serif;max-width:1000px;margin:24px auto;padding:0 20px;color:#172b4d}
h1{border-bottom:2px solid #dfe1e6;padding-bottom:8px}
h2{margin-top:32px;border-bottom:1px solid #dfe1e6;padding-bottom:6px}
h3{margin-top:24px}
h4{margin-top:18px}
table{border-collapse:collapse;width:100%;margin:12px 0}
th,td{border:1px solid #c1c7d0;padding:7px 10px;text-align:left;vertical-align:top}
th{background:#f4f5f7}
pre{background:#f4f5f7;border:1px solid #c1c7d0;padding:10px;overflow-x:auto}
code{background:#f4f5f7;padding:1px 4px;font-family:Consolas,monospace;font-size:.92em}
pre code{background:none;padding:0}
blockquote{border-left:3px solid #c1c7d0;margin:12px 0;padding:4px 14px;color:#5e6c84}
hr{border:none;border-top:1px solid #dfe1e6;margin:28px 0}
.howto{background:#deebff;border:1px solid #4c9aff;padding:12px 16px;margin-bottom:24px;font-size:14px}
.warn{background:#fffae6;border:1px solid #ffab00;padding:10px 14px;margin:12px 0}"""

HOWTO = ('<div class="howto">\n'
         '<strong>贴进 Confluence：</strong> 在这个页面里 Ctrl+A → Ctrl+C，'
         '然后到 Confluence 页面里 Ctrl+V。\n'
         '表格、标题、代码块都会保留。'
         '<em>（这个蓝框本身也会被一起复制，贴完删掉它。）</em>\n'
         '</div>')

CODE_SPAN = "\x00CODE%d\x00"


def inline(text):
    """Inline markdown -> HTML. Code spans are pulled out first so their contents are never
    treated as markup, and so a '|' inside one cannot split a table cell."""
    spans = []

    def stash(match):
        spans.append(match.group(1))
        return CODE_SPAN % (len(spans) - 1)

    text = re.sub(r"`([^`]+)`", stash, text)
    text = html.escape(text, quote=False)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w*])\*([^*]+)\*(?![\w*])", r"<em>\1</em>", text)
    for i, span in enumerate(spans):
        text = text.replace(CODE_SPAN % i, "<code>%s</code>" % html.escape(span, quote=False))
    return text


def split_row(row):
    return [cell.strip() for cell in row.strip().strip("|").split("|")]


def convert(md):
    lines = md.replace("\r\n", "\n").split("\n")
    out, i = [], 0
    while i < len(lines):
        line = lines[i]

        # lstrip, not startswith: a fence nested under a list item is indented, and treating it as
        # a paragraph dumps the backticks straight onto the page.
        if line.lstrip().startswith("```"):
            indent = len(line) - len(line.lstrip())
            body, i = [], i + 1
            while i < len(lines) and not lines[i].lstrip().startswith("```"):
                body.append(lines[i][indent:] if lines[i][:indent].strip() == "" else lines[i])
                i += 1
            i += 1
            out.append("<pre><code>%s</code></pre>" % html.escape("\n".join(body), quote=False))
            continue

        if re.match(r"^\s*$", line):
            i += 1
            continue

        if re.match(r"^(---+|\*\*\*+)\s*$", line):
            out.append("<hr />")
            i += 1
            continue

        heading = re.match(r"^(#{1,6})\s+(.*)$", line)
        if heading:
            level = len(heading.group(1))
            out.append("<h%d>%s</h%d>" % (level, inline(heading.group(2)), level))
            i += 1
            continue

        # A table: a header row, a |---| separator, then body rows.
        if line.lstrip().startswith("|") and i + 1 < len(lines) and re.match(r"^\s*\|[\s:|-]+\|\s*$", lines[i + 1]):
            header = split_row(line)
            i += 2
            rows = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            out.append("<table>")
            out.append("<tr>%s</tr>" % "".join("<th>%s</th>" % inline(c) for c in header))
            for row in rows:
                out.append("<tr>%s</tr>" % "".join("<td>%s</td>" % inline(c) for c in row))
            out.append("</table>")
            continue

        if line.lstrip().startswith(">"):
            body = []
            while i < len(lines) and lines[i].lstrip().startswith(">"):
                body.append(re.sub(r"^\s*>\s?", "", lines[i]))
                i += 1
            out.append("<blockquote>\n<p>%s</p>\n</blockquote>" % inline("\n".join(body).strip()))
            continue

        ordered = re.match(r"^\s*\d+\.\s+", line)
        bullet = re.match(r"^\s*[-*]\s+", line)
        if ordered or bullet:
            tag = "ol" if ordered else "ul"
            pattern = r"^\s*\d+\.\s+" if ordered else r"^\s*[-*]\s+"
            items = []
            while i < len(lines) and re.match(pattern, lines[i]):
                item = [re.sub(pattern, "", lines[i])]
                i += 1
                # continuation lines of the same bullet
                while i < len(lines) and lines[i].startswith("   ") and not re.match(pattern, lines[i]):
                    item.append(lines[i].strip())
                    i += 1
                items.append(" ".join(item))
            out.append("<%s>" % tag)
            out.extend("<li>%s</li>" % inline(item) for item in items)
            out.append("</%s>" % tag)
            continue

        body = []
        while i < len(lines) and lines[i].strip() and not re.match(
                r"^(#{1,6}\s|\s*```|\s*\||\s*>|\s*[-*]\s|\s*\d+\.\s|---+\s*$)", lines[i]):
            body.append(lines[i])
            i += 1
        if body:
            out.append("<p>%s</p>" % inline("\n".join(body)))
        else:
            i += 1
    return "\n\n".join(out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    source = sys.argv[1]
    title = os.path.splitext(os.path.basename(source))[0]
    target = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(source), "%s.zh-CN.confluence.html" % title)
    with io.open(source, encoding="utf-8") as handle:
        md = handle.read()
    page = ("<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"utf-8\" />\n"
            "<title>%s.zh-CN</title>\n<style>\n%s\n</style>\n</head>\n<body>\n%s\n\n%s\n</body>\n</html>\n"
            % (title, STYLE, HOWTO, convert(md)))
    with io.open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(page)
    print("wrote %s (%d bytes)" % (target, len(page.encode("utf-8"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
