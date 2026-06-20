#!/usr/bin/env bash
# Convert the TunnelMovie PNG image-sequences to EXR so ImgMedia decodes them
# multithreaded (fixes the Level Sequence freeze). Keeps identical frame names,
# so the IMS_/MediaPlayer/MediaTexture/Material/Level Sequence chain is untouched.
#
# Originals are moved to a backup folder OUTSIDE Content before deletion.
# Usage:  bash Tools/convert_tunnelmovie_to_exr.sh           (convert all)
#         DRYRUN=1 bash Tools/convert_tunnelmovie_to_exr.sh  (show plan only)
set -euo pipefail

ROOT="G:/Epic/Unreal projects/VertigoGit/Content/Movie/TunnelMovie"
BACKUP="G:/Epic/Unreal projects/VertigoGit/_TunnelMovie_PNG_Backup"
DRYRUN="${DRYRUN:-0}"
FILTER="${1:-}"   # optional: only process folders whose path contains this substring

# ffmpeg may not be on PATH in this shell yet (winget set it but shell not restarted).
# Allow an explicit override:  FFMPEG="/c/Users/.../ffmpeg.exe" bash convert...sh
FFMPEG="${FFMPEG:-ffmpeg}"
command -v "$FFMPEG" >/dev/null 2>&1 || { echo "ERROR: ffmpeg not found. Set FFMPEG=/full/path/ffmpeg.exe"; exit 1; }

# --- pick best EXR compression this ffmpeg build can actually write ---
ENC="$("$FFMPEG" -hide_banner -h encoder=exr 2>/dev/null || true)"
if echo "$ENC" | grep -qiE '\bdwaa\b'; then COMP=dwaa
elif echo "$ENC" | grep -qiE '\bzip16\b'; then COMP=zip16
elif echo "$ENC" | grep -qiE '\bzip1?\b'; then COMP=zip1
else COMP=none; fi

# --- RGBA float pixel format (preserve alpha); stored as half via -format half ---
if echo "$ENC" | grep -qiw gbrapf32le; then PIXFMT=gbrapf32le
else PIXFMT=""; fi   # fall back to ffmpeg default if RGBA-float unavailable

echo "ffmpeg: $("$FFMPEG" -hide_banner -version | head -1)"
echo "EXR compression : $COMP"
echo "EXR pixel format: ${PIXFMT:-<default>}  (-format half)"
echo "Backup dir      : $BACKUP"
echo "Dry run         : $DRYRUN"
echo

shopt -s nullglob
total_in=0 total_out=0
while IFS= read -r d; do
  pngs=("$d"/*.png)
  [ ${#pngs[@]} -eq 0 ] && continue
  if [ -n "$FILTER" ] && [[ "$d" != *"$FILTER"* ]]; then continue; fi
  rel="${d#"$ROOT"/}"
  echo "== $rel  (${#pngs[@]} frames) =="
  if [ "$DRYRUN" = "1" ]; then continue; fi

  mkdir -p "$BACKUP/$rel"
  for p in "${pngs[@]}"; do
    base="$(basename "${p%.png}")"
    exr="$d/$base.exr"
    if [ -n "$PIXFMT" ]; then
      "$FFMPEG" -y -loglevel error -i "$p" -pix_fmt "$PIXFMT" -format half -compression "$COMP" "$exr"
    else
      "$FFMPEG" -y -loglevel error -i "$p" -format half -compression "$COMP" "$exr"
    fi
    [ -s "$exr" ] || { echo "  FAILED: $base"; exit 1; }
    total_in=$((total_in + $(stat -c%s "$p")))
    total_out=$((total_out + $(stat -c%s "$exr")))
    mv "$p" "$BACKUP/$rel/"
  done
  echo "  done -> EXR"
done < <(find "$ROOT" -type d)

echo
printf "PNG in : %d MB\nEXR out: %d MB\n" $((total_in/1048576)) $((total_out/1048576))
echo "Originals backed up under: $BACKUP"
echo "Next: reopen the project (or the IMS_* assets) so ImgMedia rescans the folders as EXR."
