#!/bin/bash
# Remote flashing gate for the Mac that hosts the Cardenza board.
#
# Copy to /Users/mac/cardenza/gate.sh and make it the forced command of the build machine's key
# in ~/.ssh/authorized_keys:
#   restrict,command="/Users/mac/cardenza/gate.sh" ssh-ed25519 AAAA... ubuntu-desktop-to-macbook-20261022
#
# The key then can only run the commands below. Everything stays inside ROOT; the only outside
# paths touched are the board's /dev/cu.usbmodem* serial ports. Every request is appended to
# ROOT/gate.log so the Mac owner can see what was done.
set -eo pipefail

ROOT=/Users/mac/cardenza
FW_DIR="$ROOT/fw"
VENV="$ROOT/.venv"
PORT_GLOB='/dev/cu.usbmodem*'
ESPTOOL_VERSION=4.12.0
FLASH_BYTES=$((16 * 1024 * 1024))
MAX_UPLOAD_BYTES=$((32 * 1024 * 1024))
MAX_GATE_BYTES=$((256 * 1024))
MAX_LOG_SECONDS=600
KEEP_BUILDS=5

# Keep pip caches, Python temp files and any tool dotfiles inside ROOT.
export HOME="$ROOT" TMPDIR="$ROOT/tmp" PIP_CACHE_DIR="$ROOT/tmp/pip-cache"
mkdir -p "$FW_DIR" "$TMPDIR"
cd "$ROOT"

die() { echo "gate: $*" >&2; exit 1; }

usage() {
  cat >&2 <<'EOF'
usage:
  status                       esptool version, serial port, current build
  setup                        create .venv with esptool
  list                         uploaded builds, newest first
  upload <id>                  read a .tar.gz from stdin into fw/<id> and check SHA256SUMS
  flash <id> [seconds]         write fw/<id> with esptool, then print the boot log
  log [seconds]                print the serial log without resetting
  console [seconds]            like log, and also send stdin to the board
  reset [seconds]              hard-reset the board, then print the boot log
  info                         chip type, MAC and flash chip
  erase                        erase the whole flash
  read-flash <offset> <size>   write raw flash contents to stdout (hex values, e.g. 0x0 0x1000000)
  stage-gate                   save a new gate.sh from stdin as gate.sh.next for the owner to review
EOF
  exit 1
}

check_id() { [[ $1 =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$ ]] || die "bad build id '$1'"; }

check_seconds() {
  [[ $1 =~ ^[0-9]{1,3}$ ]] && (( $1 <= MAX_LOG_SECONDS )) || die "seconds must be 0-$MAX_LOG_SECONDS"
}

check_hex() { [[ $1 =~ ^0x[0-9a-fA-F]{1,7}$ ]] || die "bad hex value '$1'"; }

need_esptool() { [[ -x $VENV/bin/esptool.py ]] || die "esptool missing, run setup"; }

esptool() { "$VENV/bin/esptool.py" --chip esp32s3 -p "$PORT" "$@"; }

# Finds the single board port, takes the serial lock and sets PORT.
open_board() {
  local ports=() p
  need_esptool
  for p in $PORT_GLOB; do
    [[ -e $p ]] && ports+=("$p")
  done
  case ${#ports[@]} in
    0) die "no $PORT_GLOB port, is the board plugged in?" ;;
    1) PORT=${ports[0]} ;;
    *) die "more than one board port: ${ports[*]}" ;;
  esac
  mkdir "$ROOT/.lock" 2>/dev/null || die "busy, another command is using the board"
  trap 'rm -rf "$ROOT/.lock" $CLEANUP' EXIT
}

# Reads the port with DTR/RTS released so opening it does not reset the chip. The USB-Serial-JTAG
# port drops out for a moment after a reset, so keep reopening until time runs out. In console
# mode stdin is forwarded to the board as well. (No single quotes in here, it is a bash string.)
SERIAL_PY='
import glob, os, sys, threading, time
import serial

port, seconds, mode, port_glob = sys.argv[1], float(sys.argv[2]), sys.argv[3], sys.argv[4]
end = time.time() + seconds
conn = None
conn_lock = threading.Lock()

def forward_stdin():
    while True:
        data = os.read(0, 4096)
        if not data:
            return
        while time.time() < end:
            with conn_lock:
                target = conn
            try:
                if target is not None:
                    target.write(data)
                    break
            except (serial.SerialException, OSError):
                pass
            time.sleep(0.1)

if mode == "console":
    threading.Thread(target=forward_stdin, daemon=True).start()

while time.time() < end:
    if conn is None:
        candidates = glob.glob(port_glob)
        if port not in candidates and len(candidates) == 1:
            port = candidates[0]
        try:
            fresh = serial.Serial()
            fresh.port, fresh.baudrate, fresh.timeout = port, 115200, 0.2
            fresh.dtr = fresh.rts = False
            fresh.open()
        except (serial.SerialException, OSError):
            time.sleep(0.2)
            continue
        with conn_lock:
            conn = fresh
    try:
        data = conn.read(4096)
    except (serial.SerialException, OSError):
        with conn_lock:
            conn.close()
            conn = None
        continue
    if data:
        sys.stdout.buffer.write(data)
        sys.stdout.flush()
'

serial_io() { "$VENV/bin/python" -c "$SERIAL_PY" "$PORT" "$1" "$2" "$PORT_GLOB"; }

prune() {
  ls -1t "$FW_DIR" | tail -n +$((KEEP_BUILDS + 1)) | while read -r old; do
    rm -rf "${FW_DIR:?}/$old"
  done
}

cmd_status() {
  echo "root: $ROOT"
  if [[ -x $VENV/bin/esptool.py ]]; then
    echo "esptool: $("$VENV/bin/python" -c 'import esptool; print(esptool.__version__)')"
  else
    echo "esptool: not installed"
  fi
  ls $PORT_GLOB 2>/dev/null || echo "port: none"
  [[ -f $ROOT/current ]] && echo "current: $(cat "$ROOT/current")"
  [[ -f $ROOT/gate.sh.next ]] && echo "staged gate: $ROOT/gate.sh.next"
  return 0
}

cmd_setup() {
  if [[ ! -x $VENV/bin/python ]]; then
    /opt/homebrew/bin/python3 -m venv "$VENV"
  fi
  "$VENV/bin/python" -m pip install -q --disable-pip-version-check "esptool==$ESPTOOL_VERSION"
  cmd_status
}

cmd_upload() {
  local id=$1 tmp
  check_id "$id"
  [[ -e $FW_DIR/$id ]] && die "build $id already exists"
  tmp=$(mktemp -d "$FW_DIR/.upload.XXXXXX")
  # head caps the stream so a runaway upload cannot fill the disk; bsdtar refuses absolute
  # paths, '..' and writes through symlinks by default.
  if ! head -c "$MAX_UPLOAD_BYTES" | tar -xzf - -C "$tmp"; then
    rm -rf "$tmp"
    die "bad archive"
  fi
  if [[ ! -f $tmp/flash_args || ! -f $tmp/SHA256SUMS ]] \
      || ! (cd "$tmp" && shasum -a 256 -c SHA256SUMS >/dev/null); then
    rm -rf "$tmp"
    die "archive incomplete or checksum mismatch"
  fi
  mv "$tmp" "$FW_DIR/$id"
  prune
  echo "uploaded $id"
}

cmd_flash() {
  local id=$1 seconds=${2:-10}
  check_id "$id"
  check_seconds "$seconds"
  [[ -f $FW_DIR/$id/flash_args ]] || die "unknown build $id"
  open_board
  (cd "$FW_DIR/$id" && esptool -b 460800 --before default_reset --after hard_reset write_flash @flash_args)
  echo "$id" > "$ROOT/current"
  echo "--- boot log ($seconds s) ---"
  serial_io "$seconds" log
}

cmd_log() {
  local mode=$1 seconds=${2:-10}
  check_seconds "$seconds"
  open_board
  serial_io "$seconds" "$mode"
}

cmd_reset() {
  local seconds=${1:-10}
  check_seconds "$seconds"
  open_board
  esptool --before default_reset --after hard_reset chip_id >/dev/null
  echo "--- boot log ($seconds s) ---"
  serial_io "$seconds" log
}

cmd_info() {
  open_board
  esptool --before default_reset --after hard_reset flash_id
}

cmd_erase() {
  open_board
  esptool --before default_reset --after hard_reset erase_flash
  rm -f "$ROOT/current"
}

# esptool progress goes to stderr so stdout carries only the flash bytes.
cmd_read_flash() {
  local offset=$1 size=$2 out
  check_hex "$offset"
  check_hex "$size"
  (( size > 0 && offset + size <= FLASH_BYTES )) || die "range outside the 16 MB flash"
  open_board
  out=$(mktemp "$TMPDIR/read-flash.XXXXXX")
  CLEANUP="'$out'"
  esptool -b 921600 --before default_reset --after hard_reset read_flash "$offset" "$size" "$out" >&2
  cat "$out"
}

# A staged gate does nothing until the Mac owner moves it over gate.sh.
cmd_stage_gate() {
  local next="$ROOT/gate.sh.next"
  head -c "$MAX_GATE_BYTES" > "$next.tmp"
  if ! bash -n "$next.tmp"; then
    rm -f "$next.tmp"
    die "syntax error, nothing staged"
  fi
  mv "$next.tmp" "$next"
  chmod 644 "$next"
  echo "staged $next sha256=$(shasum -a 256 "$next" | cut -d' ' -f1)"
  echo "activate on the Mac: mv $next $ROOT/gate.sh && chmod 755 $ROOT/gate.sh"
}

# Under the forced command the request arrives in SSH_ORIGINAL_COMMAND; read -a splits it on
# whitespace without glob or variable expansion. A leading path to this script is ignored so the
# same call works before and after the key is restricted.
if [[ -n $SSH_ORIGINAL_COMMAND ]]; then
  read -r -a ARGS <<< "$SSH_ORIGINAL_COMMAND"
else
  ARGS=("$@")
fi
[[ ${ARGS[0]} == */gate.sh || ${ARGS[0]} == gate.sh ]] && ARGS=("${ARGS[@]:1}")
printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "${ARGS[*]}" >> "$ROOT/gate.log"

case ${ARGS[0]}:${#ARGS[@]} in
  status:1) cmd_status ;;
  setup:1) cmd_setup ;;
  list:1) ls -1t "$FW_DIR" ;;
  upload:2) cmd_upload "${ARGS[1]}" ;;
  flash:2 | flash:3) cmd_flash "${ARGS[1]}" "${ARGS[2]}" ;;
  log:1 | log:2) cmd_log log "${ARGS[1]}" ;;
  console:1 | console:2) cmd_log console "${ARGS[1]}" ;;
  reset:1 | reset:2) cmd_reset "${ARGS[1]}" ;;
  info:1) cmd_info ;;
  erase:1) cmd_erase ;;
  read-flash:3) cmd_read_flash "${ARGS[1]}" "${ARGS[2]}" ;;
  stage-gate:1) cmd_stage_gate ;;
  *) usage ;;
esac
