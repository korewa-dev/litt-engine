#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"

echo "[build_view] building supported native CLI/view path"
make clean
make all
echo "[build_view] OK: bin/littcli bin/littview"
