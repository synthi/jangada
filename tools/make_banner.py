#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Jangada: the animated README banner, drawn the way the FM-1 draws its screen.

  tools/make_banner.py SOUND.wav docs/jangada.gif

240 x 80 pixels (the width of the FM-1's screen), the CHOQUE palette (firmware/src/gfx.c),
the firmware's own font (Terminus, assets/fonts), scaled 3x with square pixels. The trace is
an oscilloscope of SOUND.wav, a render of the firmware's DSP on the host (tests/hostsim.c)."""
import sys
import wave
import array
from pathlib import Path
from PIL import Image, ImageDraw, BdfFontFile

ROOT = Path(__file__).resolve().parents[1]
W, H, SCALE, FRAMES, MS = 240, 80, 3, 40, 60
# CHOQUE, darkest to brightest (gfx.c), and white for what is touched
PAL = [(0, 0, 0), (72, 0, 44), (140, 0, 88), (215, 0, 135), (255, 20, 170), (255, 80, 215), (255, 255, 255)]


def font():
    with open(ROOT / "assets/fonts/ter-u16n.bdf", "rb") as f:
        bdf = BdfFontFile.BdfFontFile(f)
    out = Path("/tmp") / "jangada-ter16"
    bdf.save(str(out))
    from PIL import ImageFont
    return ImageFont.load(str(out) + ".pil")


def samples(path):
    w = wave.open(str(path))
    a = array.array("h", w.readframes(w.getnframes()))
    if w.getnchannels() == 2:
        a = a[0::2]
    return a, w.getframerate()


def text_big(img, xy, s, f, color, k=2):
    """Terminus at k x: drawn small, then pixel-scaled (square pixels, as on the FM-1)"""
    tw = int(f.getlength(s)) + 1
    mask = Image.new("L", (tw, 16), 0)
    ImageDraw.Draw(mask).text((0, 0), s, font=f, fill=255)
    mask = mask.resize((tw * k, 16 * k), Image.NEAREST)
    img.paste(color, (xy[0], xy[1], xy[0] + tw * k, xy[1] + 16 * k), mask)


def main():
    wav, out = Path(sys.argv[1]), Path(sys.argv[2])
    snd, rate = samples(wav)
    f = font()
    start = int(rate * 6.0)                      # into the drone, after its slow attack
    hop = int(rate * MS / 1000)
    peak = max(1, max(abs(v) for v in snd[start: start + FRAMES * hop + W * 3]))
    frames = []
    trail = []
    for i in range(FRAMES):
        img = Image.new("P", (W, H), 0)
        img.putpalette(sum(PAL, ()))
        d = ImageDraw.Draw(img)
        for x in range(0, W, 6):                 # the scope's dotted centre line, as the FM-1 draws graphs
            d.point((x, 54), fill=1)
        # the trace of this frame, the two before it dimmer (a phosphor)
        seg = snd[start + i * hop: start + i * hop + W * 3]
        pts = [(x, 54 - int(seg[x * 3] / peak * 17)) for x in range(min(W, len(seg) // 3))]
        trail = ([pts] + trail)[:3]
        for age, p in reversed(list(enumerate(trail))):
            d.line(p, fill=(5, 3, 2)[age], width=1)
        text_big(img, (2, 1), "JANGADA", f, 4)
        d.text((124, 1), "FM-1", font=f, fill=3)
        d.text((124, 16), "FIRMWARE", font=f, fill=3)
        # the sequencer's 16 steps, one lit, walking
        for k in range(16):
            x = 1 + k * 15
            on = k == (i * 16 // FRAMES)
            d.rectangle((x, 75, x + 11, 78), fill=6 if on else (3 if k % 4 == 0 else 2))
        frames.append(img.resize((W * SCALE, H * SCALE), Image.NEAREST))
    out.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=MS, loop=0, optimize=True, disposal=1)
    print(f"{out}: {out.stat().st_size // 1024} KiB, {FRAMES} frames")


if __name__ == "__main__":
    main()
