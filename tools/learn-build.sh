#!/usr/bin/env bash
# Vercel build for the Learn site and the browser simulator.
set -euo pipefail
cd "$(dirname "$0")/.."

TOOLS="$PWD/build/learn-tools"
python3 -m pip install --disable-pip-version-check --target "$TOOLS" --upgrade \
  cmake==3.31.6 Pillow==11.3.0
export PYTHONPATH="$TOOLS${PYTHONPATH:+:$PYTHONPATH}"
CMAKE="$TOOLS/cmake/data/bin/cmake"
HOST_BUILD=build/learn-host

# Both dependencies are public; retain the exact commits in this repository.
git submodule update --init Library/third_party/amy Devices/Cardenza/CardenzaHAL
"$CMAKE" -S . -B "$HOST_BUILD" -DDEVICE=CardenzaSim -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build "$HOST_BUILD" --target CardenzaSimShot CardenzaLearnDump --parallel 2
python3 tools/wiki/generate.py --host-build "$HOST_BUILD"

export CARDENZA_LEARN_URL=https://cardenza.203.io
export CARDENZA_LEARN_BASE_URL=/
export CARDENZA_SIMULATOR_URL=https://cardenza.203.io/sim/
export CARDENZA_LEARN_COMMIT="${VERCEL_GIT_COMMIT_SHA:-$(git rev-parse HEAD)}"
python3 - <<'PY'
import json, os, pathlib
pathlib.Path('wiki/generated/static/deployment.json').write_text(
    json.dumps({'commit': os.environ['CARDENZA_LEARN_COMMIT']}) + '\n')
PY
npm --prefix wiki run build
npm --prefix wiki run test:search

# Keep the SDK's environment, including its bundled Node.js, local to the simulator build.
(
  WEB_EMSDK="$PWD/build/emsdk"
  if [[ ! -x "$WEB_EMSDK/emsdk" ]]; then
    git clone --depth 1 --branch 4.0.22 https://github.com/emscripten-core/emsdk.git "$WEB_EMSDK"
  fi
  "$WEB_EMSDK/emsdk" install 4.0.22
  "$WEB_EMSDK/emsdk" activate 4.0.22
  source "$WEB_EMSDK/emsdk_env.sh"
  emcmake "$CMAKE" -S . -B build/learn-web -DDEVICE=CardenzaWeb -DCMAKE_BUILD_TYPE=Release \
    -DCARDENZA_WEB_SITE_DIR="$PWD/wiki/build/sim"
  "$CMAKE" --build build/learn-web --target CardenzaWeb --parallel 2
)
