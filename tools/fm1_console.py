#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Talk to the Jangada / Felucca USB serial console on an FM-1 (Linux, standard library only).

  fm1_console.py status               any console command: status, dbg, params, color CHOQUE, ...
  fm1_console.py check [--secs N]     hardware check: identity, CPU over N s, audio deadline, boots
  fm1_console.py --port /dev/ttyACM1 ...

The console is the CDC-ACM port of USB 1209:0001. Without access to /dev/ttyACM*, install
tools/70-jangada.rules (see that file). 'uboot' is refused here: it leaves the app.
"""
import argparse
import glob
import os
import re
import select
import sys
import termios
import time

VID_PID = ("1209", "0001")


def find_port():
    for tty in sorted(glob.glob("/sys/class/tty/ttyACM*")):
        dev = os.path.realpath(os.path.join(tty, "device"))
        for up in (dev, os.path.dirname(dev)):
            try:
                vid = open(os.path.join(up, "idVendor")).read().strip()
                pid = open(os.path.join(up, "idProduct")).read().strip()
            except OSError:
                continue
            if (vid, pid) == VID_PID:
                return "/dev/" + os.path.basename(tty)
    return None


class Console:
    def __init__(self, port):
        try:
            self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
        except PermissionError:
            raise SystemExit(f"no access to {port}: install tools/70-jangada.rules (see the file)")
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0                       # raw: no echo, no line editing, no CR/LF mapping
        a[2] |= termios.CREAD | termios.CLOCAL
        a[6][termios.VMIN], a[6][termios.VTIME] = 0, 0
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.read_until(b"> ", 1.5)                   # greeting on DTR (or an earlier prompt)

    def read_until(self, mark, timeout):
        buf, end = b"", time.monotonic() + timeout
        while time.monotonic() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                buf += os.read(self.fd, 4096)
                if buf.endswith(mark):
                    break
        return buf

    def cmd(self, line, timeout=2.0):
        if re.match(r"\s*uboot\b", line):
            raise SystemExit("refusing 'uboot': it leaves the app for the mask ROM (recovery only)")
        os.write(self.fd, line.encode() + b"\r")
        out = self.read_until(b"\r\n> ", timeout).decode("latin-1").replace("\r", "")
        lines = out.split("\n")
        if lines and lines[0].strip() == line.strip():
            lines = lines[1:]                        # the echoed command
        if lines and lines[-1].startswith(">"):
            lines = lines[:-1]
        return "\n".join(lines).strip("\n")


def kv(text):
    d = {}
    for ln in text.splitlines():
        m = re.match(r"(\w+)[ =](-?\d+)$", ln.strip())
        if m:
            d[m[1]] = int(m[2])
    return d


def check(con, secs):
    """a smoke test of the running firmware: answers, does not drop audio, does not reboot"""
    st0 = con.cmd("status")
    first = st0.splitlines()[0] if st0 else "?"
    a = kv(st0)
    d0 = kv(con.cmd("dbg"))
    peak = 0
    t_end = time.monotonic() + secs
    while time.monotonic() < t_end:
        time.sleep(0.5)
        peak = max(peak, kv(con.cmd("status")).get("cpu_pct", 0))
    b = kv(con.cmd("status"))
    d1 = kv(con.cmd("dbg"))
    late = d1.get("late", 0) - d0.get("late", 0)
    rows = [
        ("firmware", first, True),
        ("cpu_pct (peak over %g s)" % secs, peak, peak < 85),
        ("audio_max_us", b.get("audio_max_us"), b.get("audio_max_us") is not None),
        ("late audio halves", late, late == 0),
        ("boots (no reset during the check)", b.get("boots"), b.get("boots") == a.get("boots")),
        ("uptime grows", b.get("uptime_ms"), b.get("uptime_ms", 0) > a.get("uptime_ms", 0)),
    ]
    bad = 0
    for name, val, ok in rows:
        print(f"{'ok  ' if ok else 'FAIL'} {name:36s} {val}")
        bad += not ok
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="the console tty (default: found by USB id 1209:0001)")
    ap.add_argument("--secs", type=float, default=5, help="check: seconds to watch the CPU")
    ap.add_argument("command", nargs="+", help="a console command, or 'check'")
    a = ap.parse_args()
    port = a.port or find_port()
    if not port:
        raise SystemExit("no FM-1 console found (USB 1209:0001): is Jangada / Felucca running?")
    con = Console(port)
    if a.command == ["check"]:
        sys.exit(check(con, a.secs))
    print(con.cmd(" ".join(a.command)))


if __name__ == "__main__":
    main()
