// SPDX-License-Identifier: GPL-3.0-only
//
// Jangada Studio (web/studio): the firmware's DSP as WebAssembly. Run from the repo root after
//   python3 tools/build_studio.py --native
//   node web/test_studio.mjs
// - the .wasm loads, names its engines and presets (the factory ones, DRONE and the dark ones among them)
// - a script of calls (keys, presets of every engine, a drone held by HOLD and let go, the sequencer with
//   the drum kits, the master DUST / DUCK / FILT, the reverbs, a punch-in effect, MIDI) renders sound:
//   not silent, every sample a 16-bit value, silence again after the drone is let go
// - the same script through engine_native (the same C built for this computer) gives the same samples,
//   bit for bit: the browser plays what the host renders
// - the page (web/studio/index.html) and the worklet compile, every text in pt and en

import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import vm from "node:vm";

const HERE = new URL(".", import.meta.url).pathname, ROOT = join(HERE, "..");
const WASM = join(ROOT, "build/studio/engine.wasm"), NATIVE = join(ROOT, "build/studio/engine_native");
let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
if (!existsSync(WASM)) {
  console.log("no build/studio/engine.wasm (python3 tools/build_studio.py --native): skipped");
  process.exit(0);
}

const { instance } = await WebAssembly.instantiate(readFileSync(WASM));
const x = instance.exports;
const str = (p) => { const b = new Uint8Array(x.memory.buffer); let e = p; while (b[e]) e++; return new TextDecoder().decode(b.subarray(p, e)); };
const ids = {};
for (let k = 0; x.st_id_name(k); k++) ids[str(x.st_id_name(k))] = x.st_id(k);
x.st_init();
const engines = Array.from({ length: x.st_engines() }, (_, e) => ({
  name: str(x.st_engine_name(e)),
  presets: Array.from({ length: x.st_presets(e) }, (_, p) => str(x.st_preset_name(e, p))),
}));
const find = (name) => {
  for (const [e, en] of engines.entries()) { const p = en.presets.indexOf(name); if (p >= 0) return [e, p]; }
  return null;
};
ok(engines.length >= 10 && engines[9].name === "FM6" && engines[0].name === "ANALOG", `engines: ${engines.map((e) => e.name).join(" ")}`);
const drones = engines.flatMap((e) => e.presets.filter((n) => n.startsWith("DRONE")));
ok(drones.length >= 5, `drone presets: ${drones.join(", ")}`);
ok(find("RUST BASS") && find("HURT PAD"), "the dark presets (RUST BASS, HURT PAD)");
ok(x.st_block() === 32 && x.st_rate() === 44100, "blocks of 32 frames at 44.1 kHz");
ok(ids.P_AHOLD > 0 && ids.G_DUST > 0 && ids.G_RTYPE > 0 && ids.P_COUNT > ids.P_E0, "parameter ids by name");
ok(x.st_kits() >= 2 && str(x.st_global_name(ids.G_KIT, 0)) === "GM", `drum kits: ${x.st_kits()}`);

/* ---- the script: [line, what to check of the blocks it renders] */
const KEY = (k) => 1 << k;                      // FM-1 key k: F3 + k on a synth track
const drone = find("DRONE SAW"), fm6 = engines[9].presets.length ? [9, 0] : null, rust = find("RUST BASS");
const script = [];
const seg = (what, blocks, check) => script.push({ line: `render ${blocks}`, what, blocks, check });
const call = (...a) => script.push({ line: a.join(" ") });
call("st_init");
seg("power-on, nothing played: silence", 40, "silent");
call("st_keys", KEY(7) | KEY(11) | KEY(14));
seg("a chord on the keys (track 1, ANALOG ACID)", 300, "sound");
call("st_keys", 0);
seg("keys up: the release", 400, "any");
for (const [e, en] of engines.entries()) {
  if (!en.presets.length) continue;
  const p = en.presets.length - 1;
  call("st_preset", 1, 0); call("st_engine", 1, e); call("st_preset", 1, p); call("st_select", 1);
  call("st_keys", KEY(7) | KEY(14));
  seg(`${en.name} ${en.presets[p]} (track 2)`, 200, "sound");
  call("st_keys", 0);
  seg(`${en.name}: release`, 100, "any");
}
call("st_panic");
seg("panic", 400, "any");
call("st_engine", 0, drone[0]); call("st_preset", 0, drone[1]); call("st_select", 0);
call("st_keys", KEY(7) | KEY(14));
seg("DRONE SAW: keys down", 200, "sound");
call("st_keys", 0);
seg("DRONE SAW: keys up, HOLD keeps it", 600, "sound");
call("st_drone_off");
seg("drone off: it fades", 8000, "any");
seg("drone off: silence", 200, "silent");
if (fm6) { call("st_engine", 2, 9); call("st_preset", 2, 3); call("st_select", 2); call("st_keys", KEY(12)); seg("FM6 F-patch preset (track 3)", 200, "sound"); call("st_keys", 0); }
call("st_engine", 0, rust[0]); call("st_preset", 0, rust[1]);
call("st_global", ids.G_BPM, 132); call("st_global", ids.G_KIT, 1);
call("st_play", 1);
seg("PLAY: factory patterns, synthesised kit", 1500, "sound");
call("st_global", ids.G_DUST, 90); call("st_global", ids.G_DUCK, 80); call("st_global", ids.G_FILT, -40);
call("st_global", ids.G_RTYPE, 2);
seg("master: DUST, DUCK, FILT, PLATE", 1500, "sound");
call("st_global", ids.G_KIT, 0); call("st_global", ids.G_RTYPE, 1); call("st_global", ids.G_FILT, 30);
seg("GM kit (samples), SPRING, FILT high", 1000, "sound");
call("st_select", 3); call("st_keys", KEY(0) | KEY(2));
seg("track 4: kick and snare on the keys", 100, "sound");
call("st_keys", 0); call("st_select", 0);
call("st_fx_layer", 1); call("st_keys", KEY(4));
seg("punch-in effect (FX held + a white key)", 600, "sound");
call("st_keys", 0); call("st_fx_layer", 0);
call("st_midi", 0x9f, 64, 110);
seg("MIDI note on (channel 16: the selected track)", 200, "sound");
call("st_midi", 0x8f, 64, 0);
call("st_play", 0); call("st_panic");
seg("stop", 3000, "any");
call("st_master", 4096); call("st_octave", -1); call("st_select", 1); call("st_keys", KEY(20));
seg("octave down, track 2", 300, "sound");

/* ---- run it on the .wasm */
const blocks = [];
const t0 = performance.now();
let rendered = 0;
for (const s of script) {
  const [fn, ...a] = s.line.split(" ");
  if (fn !== "render") { x[fn](...a.map(Number)); continue; }
  const out = new Int32Array(s.blocks * 64);
  for (let b = 0; b < s.blocks; b++) out.set(new Int32Array(x.memory.buffer, x.st_render(), 64), b * 64);
  rendered += s.blocks * 32;
  blocks.push(out);
  let peak = 0, bad = 0, sq = 0;
  for (const v of out) { if (!Number.isInteger(v) || v > 32767 || v < -32768) bad++; peak = Math.max(peak, Math.abs(v)); sq += v * v; }
  const rms = Math.sqrt(sq / out.length);
  const good = bad === 0 && (s.check === "sound" ? peak > 300 && rms > 20 : s.check === "silent" ? peak < 64 : true);
  ok(good, `${s.what} (peak ${peak}, rms ${rms.toFixed(0)})`);
}
const secs = rendered / 44100, ms = performance.now() - t0;
console.log(`rendered ${secs.toFixed(1)} s of audio in ${ms.toFixed(0)} ms (${(100 * ms / 1000 / secs).toFixed(1)} % of one core)`);

/* ---- the same on the native build */
if (existsSync(NATIVE)) {
  const raw = execFileSync(NATIVE, { input: script.map((s) => s.line).join("\n") + "\n", maxBuffer: 1 << 28 });
  const nat = new Int32Array(raw.buffer, raw.byteOffset, raw.length / 4);
  const all = new Int32Array(blocks.reduce((n, b) => n + b.length, 0));
  let o = 0;
  for (const b of blocks) { all.set(b, o); o += b.length; }
  let first = -1;
  for (let i = 0; i < Math.max(all.length, nat.length) && first < 0; i++) if (all[i] !== nat[i]) first = i;
  ok(nat.length === all.length && first < 0, `wasm == native, bit for bit (${all.length / 2} frames${first >= 0 ? `, first difference at frame ${first >> 1}` : ""})`);
} else console.log("no build/studio/engine_native: the comparison with the host build is skipped");

/* ---- the page and the worklet compile, texts in pt and en */
const page = readFileSync(join(HERE, "studio/index.html"), "utf8");
const js = page.slice(page.indexOf('<script type="module">') + 22, page.lastIndexOf("</script>"));
const check = (src, what) => {                  // node --check: the syntax of a module
  const f = join(mkdtempSync(join(tmpdir(), "studio-")), "m.mjs");
  writeFileSync(f, src);
  try { execFileSync(process.execPath, ["--check", f], { stdio: "pipe" }); ok(true, what); }
  catch (e) { ok(false, `${what}: ${e.stderr}`); }
};
check(js, "studio page script compiles");
check(readFileSync(join(HERE, "studio/worklet.js"), "utf8"), "worklet compiles");
const text = vm.runInNewContext(js.slice(js.indexOf("/*TEXT-BEGIN*/"), js.indexOf("/*TEXT-END*/")) + "; TEXT");
const keys = (o) => Object.keys(o).sort().join();
ok(text.pt && text.en && keys(text.pt) === keys(text.en), `texts: the same ${Object.keys(text.pt).length} keys in pt and en`);
const used = [...page.matchAll(/data-t="([a-zA-Z0-9]+)"/g)].map((m) => m[1]);
ok(used.every((k) => k in text.pt), "every data-t of the page has a text");
ok(!/Salt/.test(page.slice(0, page.indexOf("<footer"))), "no Salt in the interface (the credit is in the footer)");
ok(/Chance Roth/.test(page), "the credit to Felucca [Salt] (Chance Roth) in the footer");

console.log(failed ? `STUDIO: ${failed} FAILED` : "STUDIO: all ok");
process.exit(failed ? 1 : 0);
