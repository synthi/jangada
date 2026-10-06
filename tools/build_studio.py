#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the Jangada Studio's DSP (web/studio/engine.c: the firmware's engines, voices, kits,
sequencer and master FX) for WebAssembly, after Felucca [Salt]'s tools/build_browser_audio.py.

  python3 tools/build_studio.py [--zig PATH] [--out build/studio/engine.wasm] [--native]

The compiler is Zig's clang (zig cc, wasm32-freestanding): --zig, $ZIG, `zig` on the PATH or
~/.local/bin/zig (tools/build_studio.py --get-zig puts Zig ZIG_VERSION there, no root needed).
The generated headers (tables, samples, drum kits, FM6 patches) are made by the firmware's own
generators into build/studio/gen. --native also builds build/studio/engine_native (the same C for
this computer: web/test_studio.mjs compares its samples with the WebAssembly's).
Exit 2: no Zig (the caller may skip).
"""
import argparse
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "firmware" / "src"
ENGINE = ROOT / "web" / "studio" / "engine.c"
NATIVE_MAIN = ROOT / "web" / "studio" / "native_main.c"
GEN = ROOT / "build" / "studio" / "gen"
ZIG_VERSION = "0.14.1"
HEADERS = [("gen_tables.py", "felucca_tables.h"), ("gen_samples.py", "felucca_samples.h"),
           ("gen_drumkits.py", "felucca_drumkits.h"), ("gen_fm6_patches.py", "felucca_fm6.h")]
# the C flags of both builds: the firmware's fixed point wraps (-fwrapv), nothing from a libc
CFLAGS = ["-O2", "-fwrapv", "-fno-builtin", "-w", "-I" + str(GEN), "-I" + str(SRC)]


def find_zig(arg):
    for z in (arg, os.environ.get("ZIG"), "zig", str(Path.home() / ".local/bin/zig")):
        p = z and (shutil.which(z) or (z if Path(z).is_file() else None))
        if p:
            return p
    return None


def get_zig():
    """Zig into ~/.local/opt, linked from ~/.local/bin/zig (Linux / macOS, x86-64 / arm64)"""
    arch = {"x86_64": "x86_64", "amd64": "x86_64", "arm64": "aarch64", "aarch64": "aarch64"}[platform.machine().lower()]
    osn = {"Linux": "linux", "Darwin": "macos"}[platform.system()]
    name = f"zig-{arch}-{osn}-{ZIG_VERSION}"
    opt, bin_ = Path.home() / ".local/opt", Path.home() / ".local/bin"
    if not (opt / name / "zig").exists():
        opt.mkdir(parents=True, exist_ok=True)
        url = f"https://ziglang.org/download/{ZIG_VERSION}/{name}.tar.xz"
        print(f"build_studio: downloading {url}")
        with tempfile.TemporaryDirectory() as tmp:
            tar = Path(tmp) / "zig.tar.xz"
            urllib.request.urlretrieve(url, tar)
            with tarfile.open(tar) as t:
                t.extractall(opt)
    bin_.mkdir(parents=True, exist_ok=True)
    link = bin_ / "zig"
    if link.is_symlink() or link.exists():
        link.unlink()
    link.symlink_to(opt / name / "zig")
    print(f"build_studio: {link} -> {opt / name / 'zig'}")
    return str(link)


def generate():
    GEN.mkdir(parents=True, exist_ok=True)
    for script, header in HEADERS:
        subprocess.run([sys.executable, str(ROOT / "tools" / script), str(GEN / header)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)


def build_wasm(zig, out):
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [zig, "cc", "-target", "wasm32-freestanding", *CFLAGS, "-nostdlib", "-fvisibility=hidden",
           "-Wl,--no-entry", "-Wl,--export-dynamic", "-Wl,--strip-all",
           "-Wl,-z,stack-size=65536", "-Wl,--initial-memory=2097152", "-Wl,--max-memory=2097152",
           str(ENGINE), "-o", str(out)]
    subprocess.run(cmd, cwd=ROOT, check=True)
    print(f"build_studio: {out.relative_to(ROOT)} {out.stat().st_size} B")


def build_native(zig):
    """the same engine.c for this computer (cc, else zig cc), with a tiny driver (native_main.c)"""
    out = ROOT / "build" / "studio" / "engine_native"
    cc = [os.environ.get("CC", "cc")] if shutil.which(os.environ.get("CC", "cc")) else [zig, "cc"]
    subprocess.run([*cc, *CFLAGS, "-DSTUDIO_NATIVE", str(NATIVE_MAIN), "-o", str(out), "-lm"], cwd=ROOT, check=True)
    print(f"build_studio: {out.relative_to(ROOT)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--zig")
    ap.add_argument("--out", default=str(ROOT / "build" / "studio" / "engine.wasm"))
    ap.add_argument("--native", action="store_true")
    ap.add_argument("--get-zig", action="store_true", help=f"install Zig {ZIG_VERSION} into ~/.local first")
    a = ap.parse_args()
    zig = get_zig() if a.get_zig else find_zig(a.zig)
    if not zig:
        print("build_studio: no zig (install it: python3 tools/build_studio.py --get-zig)", file=sys.stderr)
        sys.exit(2)
    generate()
    build_wasm(zig, Path(a.out).resolve())
    if a.native:
        build_native(zig)


if __name__ == "__main__":
    main()
