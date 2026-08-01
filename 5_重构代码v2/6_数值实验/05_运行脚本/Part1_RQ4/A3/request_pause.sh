#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo "usage: request_pause.sh BATCH_DIR"; exit 2; }
touch "$1/STOP_AFTER_CURRENT"
echo "Pause requested. The current GA-BP run will finish and no new task will be started."
