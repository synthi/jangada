#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Jangada: the stable parameter keys (firmware/src/keys.h) never change.

tests/keys.txt is the list as released, "key P_SYMBOL" per line. A symbol whose key changed or
that is gone fails (saved projects and user presets would read the wrong values); every P_*
parameter of core.h must have a unique key; a new parameter must be appended to keys.txt."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
core = (ROOT / "firmware/src/core.h").read_text()
keys_h = (ROOT / "firmware/src/keys.h").read_text()

enum = re.search(r"enum \{\s*/\* per-track parameters \*/(.*?)P_COUNT", core, re.S).group(1)
enum = re.sub(r"/\*.*?\*/", "", enum, flags=re.S)
params = re.findall(r"\b(P_[A-Z0-9_]+)\b", enum)
table = re.search(r"P_KEY\[P_COUNT\] = \{(.*?)\};", keys_h, re.S).group(1)
key = {s: int(k) for s, k in re.findall(r"\[(P_[A-Z0-9_]+)\]\s*=\s*(\d+)", table)}

bad = []
missing = [p for p in params if p not in key]
if missing:
    bad.append(f"no key for {', '.join(missing)}")
dup = {k for k in key.values() if list(key.values()).count(k) > 1}
if dup:
    bad.append(f"keys used twice: {sorted(dup)}")
if any(k >= 128 for k in key.values()):
    bad.append("keys must be < KEY_MAX (128)")

released = {}
for line in (ROOT / "tests/keys.txt").read_text().splitlines():
    if line.strip() and not line.startswith("#"):
        k, s = line.split()
        released[s] = int(k)
for s, k in released.items():
    if s not in key:
        bad.append(f"{s} (key {k}) is gone: keys are never reused; keep it or retire it in keys.txt")
    elif key[s] != k:
        bad.append(f"{s} was key {k}, is {key[s]} now: saved data would read the wrong values")
new = [s for s in params if s not in released]
if new:
    bad.append(f"not in tests/keys.txt yet (append them): {', '.join(new)}")

for b in bad:
    print("keys:", b)
print(f"keys: {len(params)} parameters, stable keys, unique, as released  {'FAIL' if bad else 'ok'}")
sys.exit(1 if bad else 0)
