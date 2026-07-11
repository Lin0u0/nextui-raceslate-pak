#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCHIVE="$ROOT/dist/RaceSlate.pakz"
[[ -f "$ARCHIVE" ]] || { echo "missing $ARCHIVE" >&2; exit 1; }
manifest="$(mktemp)"; trap 'rm -f "$manifest"' EXIT
unzip -Z1 "$ARCHIVE" >"$manifest"
for entry in \
  Tools/tg5040/RaceSlate.pak/launch.sh \
  Tools/tg5040/RaceSlate.pak/pak.json \
  Tools/tg5040/RaceSlate.pak/bin/tg5040/raceslate \
  Tools/tg5040/RaceSlate.pak/lib/tg5040/libSDL2-2.0.so.0 \
  Tools/tg5040/RaceSlate.pak/lib/tg5040/libSDL2_ttf-2.0.so.0 \
  Tools/tg5040/RaceSlate.pak/res/cacert.pem \
  Tools/tg5040/RaceSlate.pak/res/fonts/Inter.ttf \
  Tools/tg5040/RaceSlate.pak/res/reference/history.tsv \
  Tools/tg5040/RaceSlate.pak/res/THIRD_PARTY_NOTICES.md \
  Tools/tg5040/.media/RaceSlate.png; do grep -Fx "$entry" "$manifest" >/dev/null || { echo "missing archive entry: $entry" >&2; exit 1; }; done
bytes=$(stat -f %z "$ARCHIVE" 2>/dev/null || stat -c %s "$ARCHIVE")
(( bytes <= 15728640 )) || { echo "package exceeds 15 MiB: $bytes" >&2; exit 1; }
unzip -p "$ARCHIVE" Tools/tg5040/RaceSlate.pak/launch.sh | grep -F 'CURL_CA_BUNDLE' >/dev/null
unzip -p "$ARCHIVE" Tools/tg5040/RaceSlate.pak/pak.json | grep -F '"version": "v' >/dev/null
echo "ok: RaceSlate.pakz ($bytes bytes)"
