#!/bin/sh
set -eu

PAK_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PLATFORM="${PLATFORM:-tg5040}"
SHARED_USERDATA_PATH="${SHARED_USERDATA_PATH:-/mnt/SDCARD/.userdata/shared}"
LOGS_PATH="${LOGS_PATH:-$SHARED_USERDATA_PATH/logs}"
HOME="$SHARED_USERDATA_PATH/RaceSlate"
BIN_PATH="$PAK_DIR/bin/$PLATFORM/raceslate"

mkdir -p "$HOME" "$LOGS_PATH"
test -x "$BIN_PATH" || { echo "missing RaceSlate binary: $BIN_PATH" >&2; exit 1; }
export HOME PATH="$PAK_DIR/bin/$PLATFORM:$PATH"
export LD_LIBRARY_PATH="$PAK_DIR/lib/$PLATFORM${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CURL_CA_BUNDLE="$PAK_DIR/res/cacert.pem"
exec "$BIN_PATH" --assets "$PAK_DIR/res" --data "$HOME" >>"$LOGS_PATH/RaceSlate.txt" 2>&1
