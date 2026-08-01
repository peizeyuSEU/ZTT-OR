#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo "usage: resume_a3.sh BATCH_DIR"; exit 2; }
BATCH=$(readlink -f "$1")
rm -f "$BATCH/STOP_AFTER_CURRENT" "$BATCH/PAUSED.ok"
exec python3 "$(dirname "$0")/a3_resumable_controller.py" "$BATCH"
