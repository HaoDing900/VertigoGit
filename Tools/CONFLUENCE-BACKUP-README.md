# Confluence Wiki → PDF Backup

Exports every page of the Confluence space **VTG** (https://haooding.atlassian.net/wiki)
to PDF — one PDF per page — and sorts them into folders matching the wiki's page tree.

---

## One-time setup (do this once)

### 1. Create an API token
1. Go to https://id.atlassian.com/manage-profile/security/api-tokens
2. Click **Create API token**, give it any name (e.g. "pdf-backup")
3. Click **Copy** (use the button — don't hand-select the text)

### 2. Store the token (and email) as environment variables
Open **PowerShell** and run these two lines. Paste your token inside the quotes:

```powershell
setx CONFLUENCE_EMAIL "haooding@gmail.com"
setx CONFLUENCE_TOKEN "paste-your-token-here"
```

> `setx` saves them permanently for your Windows user, so you never have to type
> the token again. **Close and reopen PowerShell** afterward so the values load.

### 3. Verify it stored correctly
```powershell
(Get-ItemProperty 'HKCU:\Environment' CONFLUENCE_TOKEN).CONFLUENCE_TOKEN.Length
```
A real Atlassian token is ~190+ characters (older ones ~24). If it says **21** or looks
truncated, the copy went wrong — redo step 1–2.

---

## Running a backup (every time)

Open **PowerShell** and run:

```powershell
cd "G:\Epic\Unreal projects\VertigoGit\tools"
./export-confluence-html2pdf.ps1      # renders all pages to PDF (takes ~10-15 min)
./reorganize-confluence-tree.ps1      # sorts them into the folder tree
```

That's it. No token typing — the scripts read it from the environment variables.

### Where the output goes
| Folder | What |
|---|---|
| `confluence-pdf-export\` | Flat list — all pages, numbered in order (`001_...`, `002_...`) |
| `confluence-pdf-tree\`   | Nested folders mirroring the wiki hierarchy |

Both folders are **wiped and rebuilt** each run, so you always get a fresh, complete copy.

Open the tree in Explorer:
```powershell
explorer "G:\Epic\Unreal projects\VertigoGit\tools\confluence-pdf-tree"
```

---

## How the folder tree works
- A page **with sub-pages** becomes a folder containing its own `.pdf` plus its children.
  - e.g. `Production\Production.pdf` + `Production\2026MonthlyMilestone\...`
- A page **with no sub-pages** is just a `.pdf` inside its parent's folder.
- Everything hangs off `VTG Dev` (the space homepage).

---

## Test run (optional)
To try it on just a few pages before doing all ~325:
```powershell
./export-confluence-html2pdf.ps1 -Limit 5
```

---

## Troubleshooting

**403 errors / "no PDF"** — token is wrong, expired, or revoked. Make a new one (setup step 1–2).

**Chinese text looks like `æ å` (garbled)** — encoding bug; the scripts already handle this
by decoding UTF-8 manually. If you see it, make sure you're running the current script
versions in this folder (not an old copy).

**"Edge not found"** — the scripts use Microsoft Edge (headless) to make the PDFs. Edge ships
with Windows; if it's in a custom location, edit the `$edgeExe` paths near the top of
`export-confluence-html2pdf.ps1`.

**Native Confluence "Export to PDF" (403)** — ignore it. Confluence's built-in per-page PDF
endpoint refuses API tokens (needs a browser login), which is why these scripts render the
PDFs themselves instead.

---

## Security note
- The API token = a password to your whole Atlassian account (Confluence **and** Jira: read,
  edit, delete). Treat it like one.
- **Never paste it into chats, emails, screenshots, or the scripts themselves.** Keep it only
  in the environment variable set via `setx`.
- If it ever leaks, revoke it at the token page above and create a new one — it takes seconds
  and breaks nothing.

---

## Files in this folder
| File | Purpose |
|---|---|
| `export-confluence-html2pdf.ps1` | Main exporter (API → HTML → PDF via headless Edge) |
| `reorganize-confluence-tree.ps1` | Sorts the flat PDFs into the wiki folder hierarchy |
| `export-confluence-pdfs.ps1` | Old attempt using Confluence's native PDF endpoint — **doesn't work** (403 with API tokens); kept for reference only |
| `CONFLUENCE-BACKUP-README.md` | This file |
