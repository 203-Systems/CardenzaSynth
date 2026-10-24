#!/usr/bin/env bash
# Packs the Cardenza firmware build and flashes it on the remote Mac through gate.sh.
#   tools/remote-flash/push.sh            upload + flash + 10 s boot log
#   LOG_SECONDS=30 REMOTE=macbook tools/remote-flash/push.sh
set -euo pipefail

REMOTE=${REMOTE:-macbook}
GATE=/Users/mac/cardenza/gate.sh
LOG_SECONDS=${LOG_SECONDS:-10}

cd "$(dirname "$0")/../.."
BUILD=${BUILD:-build/Cardenza}
[[ -f $BUILD/flash_args ]] || { echo "push: $BUILD/flash_args missing, run make DEVICE=Cardenza build" >&2; exit 1; }

# flash_args lists "<offset> <file>" after the first line of esptool options.
mapfile -t images < <(awk 'NR > 1 && NF == 2 { print $2 }' "$BUILD/flash_args")
files=(flash_args "${images[@]}")

id="$(date +%Y%m%d-%H%M%S)-$(git rev-parse --short HEAD)"
git diff --quiet HEAD || id+="-dirty"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for f in "${files[@]}"; do
  mkdir -p "$stage/$(dirname "$f")"
  cp "$BUILD/$f" "$stage/$f"
done
(cd "$stage" && sha256sum "${files[@]}" > SHA256SUMS)

echo "push: uploading $id to $REMOTE"
tar -C "$stage" -czf - . | ssh "$REMOTE" "$GATE" upload "$id"
ssh "$REMOTE" "$GATE" flash "$id" "$LOG_SECONDS"
