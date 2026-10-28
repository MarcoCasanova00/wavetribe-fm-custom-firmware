/* RIPTIDE web emulator: drives the compiled firmware (riptide.js / .wasm).
 * The LCD is redrawn ~30-60 fps from the firmware's framebuffer; audio is generated
 * by a ScriptProcessorNode pulling from `web_audio`, which advances the device clock. */

const createRiptide = globalThis.createRiptide;   // from the classic riptide.js script

const canvas = document.getElementById("lcd");
const ctx = canvas.getContext("2d");
const img = ctx.createImageData(240, 240);

const NOTES = ["F3","F#3","G3","G#3","A3","A#3","B3","C4","C#4","D4","D#4","E4","F4","F#4",
  "G4","G#4","A4","A#4","B4","C5","C#5","D5","D#5","E5","F5","F#5","G5"];
const BLACK = new Set([1,3,6,8,10,13,15,18,20,22,25]);

let m = null, mem = null, audioPtr = 0, audioFrames = 2048;
let running = false, lastT = 0;

/* --- build the keyboard --- */
const kbd = document.getElementById("kbd");
const keyBits = [];
NOTES.forEach((nm, i) => {
  const el = document.createElement("div");
  el.className = "key" + (BLACK.has(i) ? " black" : "");
  el.textContent = nm;
  kbd.appendChild(el);
  keyBits.push(el);
  el.addEventListener("pointerdown", (e) => { e.preventDefault(); setKey(i, true); });
  el.addEventListener("pointerup", () => setKey(i, false));
  el.addEventListener("pointerleave", () => setKey(i, false));
});
function setKey(i, down) { if (!m) return; m._web_key(down ? 1 : 0, 1 << i); keyBits[i].classList.toggle("down", down); }

/* --- framebuffer -> canvas (RGB565 -> RGBA) --- */
function drawLcd() {
  if (!m) return;
  const _fbcount = m._web_fb_words();
  const fb = new Uint16Array(mem.buffer, m._web_fb_ptr(), m._web_fb_words());
  const fb = new Uint16Array(mem.buffer, m._web_fb_ptr(), fbSz || 240*240);
  if (!fb || !fb.length) { console.error("fb empty", fb); return; }
  const d = img.data;
  for (let i = 0, p = 0; i < 240 * 240; i++) {
    const v = fb[i];
    d[p++] = ((v >> 11) & 31) * 255 / 31;   // r
    d[p++] = ((v >> 5) & 63) * 255 / 63;    // g
    d[p++] = (v & 31) * 255 / 31;           // b
    d[p++] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

/* --- the run loop --- */
function frame(t) {
  if (running) {
    let dt = t - lastT;
    lastT = t;
    if (dt > 100) dt = 100;
    if (dt > 0) m._web_run(Math.round(dt));
    drawLcd();
  }
  requestAnimationFrame(frame);
}

/* --- audio --- */
let actx = null;
function startAudio() {
  actx = new (window.AudioContext || window.webkitAudioContext)();
  audioFrames = 2048;
  audioPtr = m._malloc(audioFrames * 2 * 2);
  const node = actx.createScriptProcessor(audioFrames, 0, 2);
  node.onaudioprocess = (e) => {
    const n = e.outputBuffer.length;
    const out = new Int16Array(mem.buffer, audioPtr, n * 2);
    m._web_audio(audioPtr, n);
    const L = e.outputBuffer.getChannelData(0), R = e.outputBuffer.getChannelData(1);
    for (let i = 0; i < n; i++) { L[i] = out[2 * i] / 32768; R[i] = out[2 * i + 1] / 32768; }
  };
  node.connect(actx.destination);
}

/* --- start overlay --- */
globalThis._wasmInstMem = globalThis._wasmInstMem || null;
document.getElementById("overlay").addEventListener("click", async () => {
  const svgOpts = {
    instantiateWasm: (info, cb) => {
      fetch("riptide.wasm").then(async r => {
        const wasmBin = await r.arrayBuffer();
        WebAssembly.instantiate(wasmBin, info).then(inst => {
          globalThis._wasmInstMem = inst.instance.exports.memory;
          cb(inst.instance);
        });
      });
      return {};
    }
  };
  document.getElementById("overlay").style.display = "none";
  try {
    m = await createRiptide(svgOpts);
    mem = globalThis._wasmInstMem && globalThis._wasmInstMem.buffer
       || (m.HEAPU16 && m.HEAPU16.buffer)
       || (m.memory && m.memory.buffer)
       || (m.wasmMemory ? m.wasmMemory.buffer : null)
       || (m.asm && m.asm.memory && m.asm.memory.buffer);
    if (!mem) throw new Error("could not locate wasm memory");
    m._web_init();
    console.log("init OK. ptr=", m._web_fb_ptr(), "words=", m._web_fb_words(), "mem=", mem && mem.byteLength);
    startAudio();
    running = true;
    lastT = performance.now();
    requestAnimationFrame(frame);
  } catch (err) {
    console.error(err);
    document.getElementById("overlay").innerHTML =
      `<div class="card"><div class="big">LOAD FAILED</div><p>${err.message || err}</p></div>`;
  }
});

/* --- controls --- */
function bind(id, fn) { document.getElementById(id).addEventListener("click", fn); }
function tapBtn(id, on) { if (m) m._web_btn(id, on ? 1 : 0); }
// buttons: momentary (press+release)
bind("b-play", () => { tapBtn(12, 1); setTimeout(() => tapBtn(12, 0), 60); document.getElementById("b-play").classList.toggle("on"); });
bind("b-rec", () => { tapBtn(13, 1); setTimeout(() => tapBtn(13, 0), 60); document.getElementById("b-rec").classList.toggle("on"); });
bind("b-glo", () => { tapBtn(7, 1); setTimeout(() => tapBtn(7, 0), 60); document.getElementById("b-glo").classList.toggle("on"); });
bind("b-home", () => { tapBtn(8, 1); setTimeout(() => tapBtn(8, 0), 60); document.getElementById("b-home").classList.toggle("on"); });
// encoders: role 0=part, 1=page, 6=presets, 2..5=knobs A-D
function enc(role, n) { if (m) m._web_enc(role, n); }
bind("b-selm", () => enc(0, -1));
bind("b-selp", () => enc(0, 1));
bind("b-prsm", () => enc(6, -1));
bind("b-prsp", () => enc(6, 1));
bind("b-pgm", () => enc(1, 1));
bind("b-k1m", () => enc(2, -1)); bind("b-k1p", () => enc(2, 1));
bind("b-k2m", () => enc(3, -1)); bind("b-k2p", () => enc(3, 1));
bind("b-k3m", () => enc(4, -1)); bind("b-k3p", () => enc(4, 1));
bind("b-k4m", () => enc(5, -1)); bind("b-k4p", () => enc(5, 1));

// keyboard: ignore drags off-panel
window.addEventListener("pointerup", () => { for (let i = 0; i < 27; i++) setKey(i, false); });
