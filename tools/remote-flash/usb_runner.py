"""Bound one USB worker independently of its Python threads and native libraries."""
import contextlib
import os
import signal
import subprocess
import sys

LIMITS = {"list": 30, "command": 20, "stats": 15, "test": 90}
GRACE_SECONDS = 1


def run():
    arguments = sys.argv[1:]
    action = arguments[0] if arguments else ""
    seconds = LIMITS.get(action, 30)
    if action == "console" and len(arguments) == 2:
        seconds = min(135, max(1, float(arguments[1]) + 15))
    worker = None

    def kill_group(kind):
        if worker is not None:
            with contextlib.suppress(ProcessLookupError):
                os.killpg(worker.pid, kind)

    def stop():
        if worker is None:
            return
        kill_group(signal.SIGTERM)
        try:
            worker.wait(timeout=GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            kill_group(signal.SIGKILL)
            worker.wait(timeout=GRACE_SECONDS + 1)

    def interrupted(number, _frame):
        stop()
        raise SystemExit(128 + number)

    for kind in (signal.SIGHUP, signal.SIGTERM, signal.SIGINT):
        signal.signal(kind, interrupted)
    try:
        worker = subprocess.Popen(
            [sys.executable, "-", *arguments],
            stdin=sys.stdin,
            start_new_session=True,
        )
        try:
            code = worker.wait(timeout=seconds)
            return code if code >= 0 else 128 - code
        except subprocess.TimeoutExpired:
            stop()
            # A blocked terminal must not hold up releasing the Bash-owned device lock.
            with contextlib.suppress(OSError):
                os.set_blocking(2, False)
                os.write(2, ("usb: hard timeout after %g s; last progress identifies the blocked step\n" % seconds).encode())
            return 124
    finally:
        if worker is not None and worker.poll() is None:
            stop()
        # Also remove any descendants left by a worker that has already exited.
        kill_group(signal.SIGKILL)


if __name__ == "__main__":
    raise SystemExit(run())
