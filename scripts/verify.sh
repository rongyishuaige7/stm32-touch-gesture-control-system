#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "$ROOT/scripts/secret_scan.py" --root "$ROOT"
python3 "$ROOT/scripts/check_repo.py" --root "$ROOT"
PYTHONPYCACHEPREFIX="$(mktemp -d)"; export PYTHONPYCACHEPREFIX
PIO_WORK_DIR="$(mktemp -d)"
cleanup(){ rm -rf -- "$PYTHONPYCACHEPREFIX" "$PIO_WORK_DIR"; }
trap cleanup EXIT
python3 -m unittest discover -s "$ROOT/tests" -v
cp -a -- "$ROOT/." "$PIO_WORK_DIR/"
rm -rf -- "$PIO_WORK_DIR/.git" "$PIO_WORK_DIR/.pio"
pio run -d "$PIO_WORK_DIR"
echo "Verification: PASS"
