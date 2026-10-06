#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  index.html                  the home page: installer, editor, Studio
  firmware/jangada-VER.fwsc   the package
  webapp/installer/index.html index_pkg.html with fm1pkg.js, fm1ota.js, fm1backup.js and the metadata inlined
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt, fm1backup.js: the editor imports it)
  webapp/studio/              studio/index.html, worklet.js and build/studio/engine.wasm (the firmware's DSP as
                              WebAssembly; built here by tools/build_studio.py when missing or older than its sources)
  src/                        not touched

  web/make_site.py build/jangada-X.Y.fwsc X.Y OUT_DIR

The package identity (FM-1_9xx) is read from the package; the device reports it
after the install.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
WASM = ROOT / "build" / "studio" / "engine.wasm"
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def studio_wasm():
    """build/studio/engine.wasm, (re)built when missing or older than the C it is made of; None: no Zig"""
    srcs = [HERE / "studio" / "engine.c", *(ROOT / "firmware" / "src").glob("*.[ch]"), ROOT / "tools" / "build_studio.py"]
    if WASM.exists() and WASM.stat().st_mtime >= max(f.stat().st_mtime for f in srcs):
        return WASM
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "build_studio.py")])
    if r.returncode == 2:
        print("site: WARNING: no zig, the Studio has no engine (python3 tools/build_studio.py --get-zig)", file=sys.stderr)
        return None
    if r.returncode:
        raise SystemExit("site: tools/build_studio.py failed")
    return WASM


HOME = """<!doctype html>
<html lang="pt">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Jangada</title>
<style>
  :root { --bg: #fff; --fg: #000; --dim: #777; --hot: #e0068f; }
  @media (prefers-color-scheme: dark) { :root { --bg: #0a0207; --fg: #fff; --dim: #8a7a85; --hot: #ff14aa; } }
  body { margin: 0; background: var(--bg); color: var(--fg); font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }
  main { max-width: 520px; margin: 0 auto; padding: 48px 16px; }
  h1 { font-size: 24px; font-weight: 600; margin: 0 0 8px; } h1 b { color: var(--hot); font-weight: 600; }
  a { color: inherit; } li { margin: 0 0 10px; } .small { color: var(--dim); font-size: 13px; }
  a.hot { color: var(--hot); font-weight: 600; }
</style>
</head>
<body>
<main>
  <h1>Jangada</h1>
  <p class="small" id="tag"></p>
  <ul id="links"></ul>
  <p class="small"><a href="https://github.com/zednaked/jangada">github.com/zednaked/jangada</a> · GPL-3.0</p>
</main>
<script>
  const pt = (navigator.language || "en").toLowerCase().startsWith("pt");
  document.documentElement.lang = pt ? "pt" : "en";
  document.getElementById("tag").textContent = pt
    ? "Firmware alternativo para o M-VAVE FM-1: escuro, industrial e brasileiro."
    : "Alternative firmware for the M-VAVE FM-1: dark, industrial and Brazilian.";
  const L = pt ? [["webapp/installer/", "Instalar no FM-1", ""], ["webapp/editor/", "Editor", ""], ["webapp/studio/", "Studio", "hot"]]
               : [["webapp/installer/", "Install on the FM-1", ""], ["webapp/editor/", "Editor", ""], ["webapp/studio/", "Studio", "hot"]];
  const D = pt ? ["grave a Jangada pelo navegador (Chrome / Edge)", "sons e sequências pelo computador", "toque a Jangada no navegador, sem o FM-1"]
               : ["write Jangada from the browser (Chrome / Edge)", "sounds and sequences from the computer", "play Jangada in the browser, without the FM-1"];
  L.forEach(([h, n, c], i) => { const li = document.createElement("li"); li.innerHTML = `<a class="${c}" href="${h}"></a> <span class="small"></span>`;
    li.firstChild.textContent = n; li.lastChild.textContent = "— " + D[i]; document.getElementById("links").append(li); });
</script>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # marker of firmware/loader
        raise SystemExit(f"{pkg}: no Felucca update loader in it")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"jangada-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw, st = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware", out / "webapp" / "studio"
    for d in (inst, ed, fw, st):
        d.mkdir(parents=True, exist_ok=True)
    for old in [*fw.glob("felucca-*.fwsc"), *fw.glob("jangada-*.fwsc")]:          # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    shutil.copy(HERE / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    for f in ("index.html", "worklet.js"):
        shutil.copy(HERE / "studio" / f, st / f)
    wasm = studio_wasm()
    if wasm:
        shutil.copy(wasm, st / "engine.wasm")
    (out / "index.html").write_text(HOME, encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, webapp/studio "
          f"({wasm.stat().st_size if wasm else 'NO'} B engine), firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
