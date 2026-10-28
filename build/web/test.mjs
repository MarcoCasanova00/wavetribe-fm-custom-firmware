import { createRequire } from "module";
import { fileURLToPath } from "url";
import path from "path";
import fs from "fs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);
const factory = require("./riptide.js");

let instMem = null;
const wasmBinary = fs.readFileSync(path.join(__dirname, "riptide.wasm"));
const mod = await factory({
  instantiateWasm(info, receiveInstance) {
    WebAssembly.instantiate(wasmBinary, info).then((r) => {
      instMem = r.instance.exports.memory;
      receiveInstance(r.instance);
    });
  },
});
const m = typeof mod === "function" ? await mod() : mod;

// init, run a bit
m._web_init();
m._web_run(100);          // 100 ms

const words = m._web_fb_words();
const ptr = m._web_fb_ptr();
const fb = new Uint16Array(instMem.buffer, ptr, words);

let nonBlack = 0, bright = 0, sum = 0;
for (let i = 0; i < fb.length; i++) {
  const v = fb[i];
  if (v) { nonBlack++; sum += v; if (v > 1024) bright++; }
}
console.log("fb words:", words, "nonBlack:", nonBlack, "bright:", bright, "sum:", sum);

// start the sequencer (PLAY = 12), run more, then render audio
m._web_btn(12, 1); m._web_btn(12, 0);
m._web_run(2000);

// audio (allocate wasm memory for the output buffer)
const N = 256;
const bytes = N * 2 * 2;              // N frames * 2 channels * int16
const aptr = m._malloc(bytes);
m._web_audio(aptr, N);
const out = new Int16Array(instMem.buffer, aptr, N * 2);
m._free(aptr);
let peak = 0, energy = 0;
for (let i = 0; i < out.length; i++) { const a = Math.abs(out[i]); if (a > peak) peak = a; energy += a * a; }
console.log("audio peak:", peak, "rms:", Math.round(Math.sqrt(energy / out.length) * 100) / 100);

// a snapshot of the FB centre row (sanity)
let row = "";
for (let x = 0; x < 240; x += 3) { const c = fb[120 * 240 + x]; row += c ? "#" : "."; }
console.log("row120:", row);
process.exit(0);
