"""A Cardenza running this firmware on this computer's USB, from the command line. The generic side
(list, command, console, app) is CardenzaHAL's tools/cardenza_usb.py, imported from the submodule; this
file adds what only this firmware answers.

usage: python3 tools/usb/cardenza_usb.py list | command <identify|reboot|launcher|download>
       | console <seconds> | app <text> [seconds]     (CardenzaHAL's tool)
       | stats | test | guide                          (this firmware)

  stats     what has gone through its USB MIDI and audio (app report "S")
  test      plays and records through its USB audio and checks the counts
  guide     walks the Learn guide link (SysEx F0 23 7F) through a start, a lesson, a key and the end

Needs: pip install hidapi (HID), sounddevice numpy (list audio, test). MIDI goes through macOS CoreMIDI
(no package); elsewhere the MIDI parts report that and the HID parts still work.
"""

import importlib.util
import os
import re
import sys
import threading
import time

HAL_TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Devices", "Cardenza",
                        "CardenzaHAL", "tools", "cardenza_usb.py")


def load_hal():
    if not os.path.exists(HAL_TOOL):
        raise SystemExit("usb: %s is missing (git submodule update --init)" % os.path.normpath(HAL_TOOL))
    spec = importlib.util.spec_from_file_location("cardenza_hal_usb", HAL_TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


hal = load_hal()
GUIDE_TO = bytes([0xF0, 0x23, 0x7F])
GUIDE_FROM = bytes([0xF0, 0x24, 0x7F])

STEP = "starting"


def step(name):
    """Says what runs now, so a watchdog timeout can tell where it stopped."""
    global STEP
    STEP = name
    print("...  " + name, flush=True)


def watchdog(seconds):
    """Ends the process when an action runs too long (e.g. a USB audio stream that never starts),
    wherever it blocks."""
    def expire():
        print("usb: timed out after %d s during: %s" % (seconds, STEP), flush=True)
        os._exit(124)
    timer = threading.Timer(seconds, expire)
    timer.daemon = True
    timer.start()


def stats(device=None):
    own = device is None
    if own:
        device, error = hal.open_hid()
        if device is None:
            raise SystemExit("usb: " + error)
    hal.send(device, hal.REPORT_APP, [ord("S")])
    answer = hal.receive(device, hal.REPORT_APP, 1.5)
    if own:
        device.close()
    return answer.rstrip(b"\0").decode(errors="replace") if answer else None


def parse_stats(text):
    """Keep complete counters when the fixed HID report truncates a trailing field."""
    values = {}
    for item in (text or "").split():
        name, separator, value = item.partition("=")
        if separator:
            try:
                values[name] = int(value, 10)
            except ValueError:
                pass
    return values


def audio_device(direction):
    import sounddevice
    key = "max_input_channels" if direction == "in" else "max_output_channels"
    for index, d in enumerate(sounddevice.query_devices()):
        if "Cardenza" in d["name"] and d[key] >= 2:
            return index
    return None


def guide():
    """The guide link (Applications/Synth/GrooveAppGuide.cpp) as the Learn page uses it, over SysEx
    F0 23 7F <text> F7: start, a lesson start, the state, a key from the page, the end."""
    port, error = hal.midi()
    if port is None:
        raise SystemExit("usb: %s" % error)
    failures = []
    buffered = [b""]

    def check(name, ok, detail=""):
        print("%s %s %s" % ("ok  " if ok else "FAIL", name, detail))
        if not ok:
            failures.append(name)

    def exchange(text, seconds=0.8):
        port.send(GUIDE_TO + text.encode("ascii") + b"\xf7")
        answers = []
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            found, buffered[0] = hal.split_sysex(buffered[0] + port.take(), GUIDE_FROM)
            answers.extend(answer.decode("ascii", "replace") for answer in found)
            time.sleep(0.01)
        return answers

    def field(answers, name):
        states = [a[2:].split("n", 1)[0] for a in answers if a.startswith("GS")]
        for state in reversed(states):
            found = [int(v) for k, v in re.findall(r"([a-z])(\d+)", state) if k == name]
            if found:
                return found[0]
        return None

    try:
        port.take()
        hello = exchange("G?")
        check("guide link", any(a.startswith("G!1") for a in hello), " ".join(hello))
        started = exchange("GE", 3.0)
        check("guide starts", "GE1" in started, " ".join(a for a in started if not a.startswith("GS")))
        lesson = exchange("GLs2p0t1", 1.0)
        check("lesson start: SEQ, T1", field(lesson, "s") == 2 and field(lesson, "t") == 0, (lesson or [""])[-1])
        exchange("GH0d1b", 0.3)
        asked = exchange("GQ")
        check("state on request", field(asked, "s") == 2, (asked or [""])[-1])
        exchange("GK0e1", 0.3)
        moved = exchange("GK0e0", 1.0)
        check("TAB from the page: next track", field(moved, "t") == 1, (moved or [""])[-1])
        ended = exchange("GX", 3.0)
        check("guide ends, the working state is back", "GX" in ended, " ".join(a for a in ended if not a.startswith("GS")))
    finally:
        port.close()
    print("failures: %d" % len(failures))
    return 1 if failures else 0


def test():
    import numpy
    import sounddevice
    failures = []

    def check(name, ok, detail=""):
        print("%s %s %s" % ("ok  " if ok else "FAIL", name, detail))
        if not ok:
            failures.append(name)

    step("identify over HID")
    transport, answer = hal.command("identify")
    check("identify (%s)" % transport, bool(answer) and answer[0] == 0xC4, answer[2:].rstrip(b"\0").decode(errors="replace") if answer else "")
    step("identify over SysEx")
    answer, error = hal.sysex_command(hal.COMMANDS["identify"])
    check("identify (sysex)", answer is not None and answer[0] == (0xC4 & 0x7F), error or "")

    step("HID statistics")
    device, error = hal.open_hid()
    check("hid open", device is not None, error or "")
    before = stats(device) if device else None
    print("     " + str(before))

    # MIDI in: notes on channel 1 reach the firmware (and it plays them on track 1).
    step("MIDI ports")
    port, error = hal.midi()
    check("midi ports", port is not None, error or "")
    # Audio to the computer: record while the notes play.
    recorded = None
    in_device = audio_device("in")
    check("audio input device", in_device is not None)
    recording_started = time.time()
    if in_device is not None:
        step("recording from the device")
        recording = sounddevice.rec(int(2.0 * 44100), samplerate=44100, channels=2, dtype="int16", device=in_device)
        time.sleep(0.3)
    try:
        if port is not None:
            step("notes over MIDI")
            for note in (60, 64, 67):
                port.send([0x90, note, 100])
            time.sleep(1.0)
            for note in (60, 64, 67):
                port.send([0x80, note, 0])
    finally:
        if port:
            port.close()
    if in_device is not None:
        # A fixed time, never sounddevice.wait(): a stream that delivers nothing would wait for ever.
        time.sleep(max(0.0, recording_started + 2.2 - time.time()))
        sounddevice.stop()
        recorded = numpy.asarray(recording, dtype=numpy.float64)
        rms = float(numpy.sqrt(numpy.mean(recorded ** 2))) if recorded.size else 0.0
        check("audio from the device", rms > 50.0, "rms=%.0f peak=%d" % (rms, int(numpy.abs(recorded).max()) if recorded.size else 0))

    # Audio from the computer: a 1 kHz tone at -12 dBFS for 2 s.
    out_device = audio_device("out")
    check("audio output device", out_device is not None)
    if out_device is not None:
        step("playing to the device")
        t = numpy.arange(int(2.0 * 44100)) / 44100.0
        tone = (0.25 * 32767 * numpy.sin(2 * numpy.pi * 1000.0 * t)).astype(numpy.int16)
        sounddevice.play(numpy.stack([tone, tone], axis=1), samplerate=44100, device=out_device)
        time.sleep(2.2)
        sounddevice.stop()

    step("HID statistics again")
    after = stats(device) if device else None
    print("     " + str(after))
    if before and after:
        b, a = parse_stats(before), parse_stats(after)
        required = {"midi_in", "to_pc", "from_pc", "peak"}
        missing = (required - b.keys()) | (required - a.keys())
        check("USB statistics fields", not missing, "missing: " + ", ".join(sorted(missing)) if missing else "")
        if not missing:
            check("midi into the firmware", a["midi_in"] - b["midi_in"] >= 6, "%s -> %s" % (b["midi_in"], a["midi_in"]))
            check("frames to the computer", a["to_pc"] > b["to_pc"], "%s -> %s" % (b["to_pc"], a["to_pc"]))
            check("frames from the computer", a["from_pc"] - b["from_pc"] > 44100, "%s -> %s peak=%s" % (b["from_pc"], a["from_pc"], a["peak"]))
            check("level from the computer", 6000 < a["peak"] < 10000, "peak=%s (expect about 8192)" % a["peak"])
    if device:
        device.close()
    print("failures: %d" % len(failures))
    return 1 if failures else 0


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    action = sys.argv[1]
    if action in ("list", "command", "console", "app"):
        hal.main()
        return
    limits = {"stats": 15, "test": 90, "guide": 30}
    if len(sys.argv) != 2 or action not in limits:
        raise SystemExit(__doc__)
    watchdog(limits[action])
    if action == "stats":
        print(stats())
    elif action == "test":
        raise SystemExit(test())
    else:
        raise SystemExit(guide())


if __name__ == "__main__":
    main()
