// SPDX-License-Identifier: GPL-3.0-only
// Jangada Studio: the firmware's DSP (engine.wasm) in the audio thread. After Felucca [Salt]'s
// web/audio/worklet.js (Chance Roth, GPL-3.0). The page sends calls ([name, ...args]); the engine renders
// blocks of 32 frames at 44.1 kHz, resampled (linear) to the context's rate when it is another one.
// Every ~50 ms the page gets the state it shows (playing, steps, drones, voices, the punch-in effect).
class JangadaProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.x = new WebAssembly.Instance(options.processorOptions.module, {}).exports;
    this.x.st_init();
    this.block = this.x.st_block();
    this.ratio = this.x.st_rate() / sampleRate;
    this.index = this.block;
    this.position = 1;
    this.a = [0, 0]; this.b = [0, 0];
    this.frames = 0;
    this.port.onmessage = ({ data }) => {
      for (const [fn, ...args] of data) if (typeof this.x[fn] === "function" && fn.startsWith("st_")) this.x[fn](...args);
    };
  }
  next() {
    if (this.index === this.block) {
      this.samples = new Int32Array(this.x.memory.buffer, this.x.st_render(), this.block * 2);
      this.index = 0;
    }
    const i = this.index++ * 2;
    this.b[0] = Math.max(-1, Math.min(1, this.samples[i] / 32768));
    this.b[1] = Math.max(-1, Math.min(1, this.samples[i + 1] / 32768));
  }
  process(inputs, outputs) {
    const out = outputs[0];
    if (!out.length) return true;
    const n = out[0].length;
    for (let i = 0; i < n; i++) {
      while (this.position >= 1) {
        this.a[0] = this.b[0]; this.a[1] = this.b[1];
        this.next(); this.position--;
      }
      for (let c = 0; c < out.length; c++) out[c][i] = this.a[c & 1] + (this.b[c & 1] - this.a[c & 1]) * this.position;
      this.position += this.ratio;
    }
    this.frames += n;
    if (this.frames >= sampleRate / 20) {
      this.frames = 0;
      const x = this.x;
      this.port.postMessage({
        playing: x.st_playing(), steps: [0, 1, 2, 3].map((t) => x.st_step(t)), droning: x.st_droning(),
        voices: x.st_voices(), punch: x.st_punch_now(),
      });
    }
    return true;
  }
}
registerProcessor("jangada-dsp", JangadaProcessor);
