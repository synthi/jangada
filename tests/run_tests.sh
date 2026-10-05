#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the drum kit, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in build/felucca.dis
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# After an intended change of the sound: GOLDEN_UPDATE=1 sh tests/run_tests.sh, review the diff
# of tests/golden.txt, commit it with the change. After an intended change of the cost (or a new
# compiler): BUDGET_UPDATE=1 (rewrites cpu_baseline.txt and target_budget.txt). VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

[ -f build/felucca.fwsc ] || { echo "run ./build.sh first"; exit 1; }

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions)" "$OUT/upreset_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

$CC -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry" "$OUT/ota_test" build/felucca.fwsc

head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/arp_test" tests/arp_test.c -lm
run "arp: UPDN / UDI / RPT, long divisions (Jangada)" "$OUT/arp_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/step_test" tests/step_test.c -lm
run "steps: RTCH ratchet and CHNC chance (Jangada)" "$OUT/step_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/punch_test" tests/punch_test.c -lm
run "master: punch-in FX, DUST, DUCK, FILT (Jangada, after SLOOP)" "$OUT/punch_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/chord_test" tests/chord_test.c -lm
run "chords: one key, a chord of the scale (Jangada, after SLOOP)" "$OUT/chord_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumkit_test" tests/drumkit_test.c -lm
run "drum kits: synthesised, every GM note (Jangada, after SLOOP)" "$OUT/drumkit_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_test" tests/midi_test.c -lm
run "MIDI: bend, sustain, CCs, clock in / out (Jangada)" "$OUT/midi_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
run "reverbs: ROOM, SPRING, PLATE (Jangada)" "$OUT/reverb_test"
$CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/layers_test" tests/layers_test.c -lm
run "layers: SEQ steps and tools, undo, ENGINE, screens (Jangada)" "$OUT/layers_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/kit_test" tests/kit_test.c -lm
run "kit: GM 42 / 44 / 49 are not toms (Felucca#25)" "$OUT/kit_test"
run "keys: stable parameter keys (Jangada)" python3 tests/keys_test.py
run "editor mock tables == firmware (Jangada)" python3 tools/gen_editor_tables.py --check
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
run "mod: the modulation matrix (Jangada)" "$OUT/mod_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/track4_test" tests/track4_test.c -lm
run "track 4: DRUM / SYNTH (Jangada)" "$OUT/track4_test"
$CC -O2 -o "$OUT/enc_test" tests/enc_test.c
run "encoders: detents learned from evidence (Felucca #23)" "$OUT/enc_test"
run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
mkdir -p build/tracks_demo
run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
$CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
run "project formats (JNG1 keyed; Felucca FUN3 / FUN2 / FUN1 read)" "$OUT/project_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
mkdir -p build/slicer_demo
run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
# host CPU baseline per platform: instruction counts differ between the Mac and Linux x86-64
case "$(uname -s)-$(uname -m)" in
Darwin-*) CPU_BASE=tests/cpu_baseline.txt ;;
*) CPU_BASE="tests/cpu_baseline.$(uname -s | tr A-Z a-z)-$(uname -m).txt" ;;
esac
run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt "$CPU_BASE"
# SLICE (tests/slice_test.c) needs a FELUCCA_SLICE=1 build; the engine is not built by default

run "regression: target cost of the render loops" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py

if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol, samples, packages, update protocol" node web/test_web.mjs
else
    echo "== skip web tests (no node)"
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
