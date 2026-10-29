/* SPDX-License-Identifier: GPL-3.0-only */
/* RIPTIDE browser panel: the same firmware the instrument runs, in an AudioWorklet. */

const NS = "http://www.w3.org/2000/svg";
/* the panel by id, as firmware/src/platform.c numbers them (FM-1 board) */
const B = { "OCT-": 0, "OCT+": 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7,
            HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
/* the encoders, as platform.c numbers them */
const EN = { SELECT: 0, ALGORITHM: 1, "KNOB 1": 2, "KNOB 2": 3, "KNOB 3": 4, "KNOB 4": 5, PRESETS: 6 };
const KEY_LED0 = 14;
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];

const ROW1 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"];
const ROW2 = ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];
const KNOBS = ["MASTER", "SELECT", "PRESETS", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"];
const LAYOUTS = {
  wide: {
    w: 1000, h: 624, screen: [292, 44, 256],
    knobs: [[105, 100, 24], [215, 100, 24], [105, 205, 24], [215, 205, 24],
            [610, 100, 22], [714, 100, 22], [818, 100, 22], [922, 100, 22]],
    trays: [[50, 250, 220, 50], [568, 160, 400, 140]],
    oct: [[60, 258, 90, 34], [170, 258, 90, 34]],
    grid: [582, 174, 54, 48, 63, 62], keybed: [36, 332, 928, 256],
  },
  tall: {
    w: 600, h: 1070, screen: [130, 104, 340],
    knobs: [[66, 64, 24], [186, 64, 24], [414, 64, 24], [534, 64, 24],
            [90, 512, 26], [230, 512, 26], [370, 512, 26], [510, 512, 26]],
    trays: [[14, 572, 572, 146], [150, 728, 300, 62]],
    oct: [[164, 737, 128, 44], [308, 737, 128, 44]],
    grid: [24, 582, 86, 60, 94, 70], keybed: [14, 800, 572, 256],
  },
};

const svg = document.getElementById("panel");
const device = document.getElementById("device");
const canvas = document.getElementById("screen");
const status = document.getElementById("status");
const say = (t) => { status.textContent = t; status.hidden = !t; };

function el(tag, attrs, parent = svg) {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  parent.appendChild(e);
  return e;
}
const SANS = "-apple-system, 'Helvetica Neue', Arial, sans-serif";
const DOT = "'DotGothic16', " + SANS;
function label(parent, x, y, s, size, font = SANS, ink = "var(--label)") {
  const t = el("text", { x, y, "text-anchor": "middle", "font-family": font,
                         "font-size": size, "font-weight": 600, fill: ink, "letter-spacing": ".04em" }, parent);
  t.textContent = s;
  return t;
}
const KEYNAMES = ["F", "F♯", "G", "G♯", "A", "A♯", "B", "C", "C♯", "D", "D♯", "E"];
const keyName = (k) => KEYNAMES[k % 12] + (3 + Math.floor((k + 5) / 12));

let node = null, gain = null, ac = null, layout = null;
let masterValue = 820;
const angle = Object.fromEntries(KNOBS.map((n) => [n, 0]));
angle.MASTER = -135 + 270 * masterValue / 1023;
const knobs = [], buttons = [], keys = [];
const btnBy = new Map(), keyBy = new Map();
const releaseTimers = new Map();
const send = (m) => { if (node) node.port.postMessage(m); };

function build(name) {
  const L = LAYOUTS[name];
  layout = name;
  svg.innerHTML = "";
  svg.setAttribute("viewBox", `0 0 ${L.w} ${L.h}`);
  device.style.aspectRatio = `${L.w} / ${L.h}`;
  for (const d of device.querySelectorAll(".hit")) d.remove();
  knobs.length = buttons.length = keys.length = 0;
  el("rect", { x: 4, y: 4, width: L.w - 8, height: L.h - 8, rx: 40, fill: "var(--body)" });
  for (const [x, y, w, h] of L.trays) el("rect", { x, y, width: w, height: h, rx: 14, fill: "var(--tray)" });
  const [sx, sy, ss] = L.screen;
  el("rect", { x: sx, y: sy, width: ss, height: ss, rx: 18, fill: "#000d00" });
  const inset = ss / 32;
  Object.assign(canvas.style, { left: `${(sx + inset) / L.w * 100}%`, top: `${(sy + inset) / L.h * 100}%`,
                                width: `${(ss - 2 * inset) / L.w * 100}%`, height: `${(ss - 2 * inset) / L.h * 100}%` });
  KNOBS.forEach((n, i) => {
    const [x, y, r] = L.knobs[i], g = el("g", {});
    el("circle", { cx: x, cy: y, r: r + 5, fill: "var(--tray)" }, g);
    el("circle", { cx: x, cy: y, r, fill: "var(--part)", stroke: "var(--edge)", "stroke-width": 2 }, g);
    const ind = el("line", { x1: x, y1: y - r + 5, x2: x, y2: y - r + 14, stroke: "var(--label)",
                             "stroke-width": 3, "stroke-linecap": "round" }, g);
    label(g, x, y - r - 12, n, 16, DOT);
    el("circle", { cx: x, cy: y, r: r + 12, fill: "transparent" }, g);
    knobs.push({ g, ind, x, y, name: n, role: n === "MASTER" ? "master" : EN[n] });
  });
  const button = ([x, y, w, h], n, sub) => {
    const g = el("g", {});
    el("rect", { x, y, width: w, height: h, rx: 8, fill: "var(--part)", stroke: "var(--edge)",
                 "stroke-width": 1.5, class: "face btn" }, g);
    const ink = n === "REC" ? "#ff453a" : undefined;
    if (sub) { label(g, x + w / 2, y + h / 2 - 2, n, 10.8, SANS, ink); label(g, x + w / 2, y + h / 2 + 11, sub, 9.9); }
    else label(g, x + w / 2, y + h / 2 + 4.5, n, 12.6, SANS, ink);
    buttons.push({ g, bit: B[n], name: sub ? `${n} / ${sub}` : n });
  };
  button(L.oct[0], "OCT-");
  button(L.oct[1], "OCT+");
  const [gx, gy, gw, gh, dx, dy] = L.grid;
  for (let i = 0; i < 6; i++) {
    button([gx + i * dx, gy, gw, gh], ROW1[i]);
    button([gx + i * dx, gy + dy, gw, gh], ROW2[i], ROW2[i] === "PLAY" ? "STOP" : "");
  }
  const [x0, y0, w, h] = L.keybed;
  el("rect", { x: x0, y: y0, width: w, height: h, rx: 18, fill: "var(--tray)" });
  const pitch = (w - 24) / 16, kw = pitch * 0.78, kh = h * 0.425, sw = Math.max(3, kw * 0.14);
  const key = (cx, cy, k) => {
    const g = el("g", {}), y = cy - kh / 2;
    el("rect", { x: cx - kw / 2, y, width: kw, height: kh, rx: kw / 2, class: "face key" }, g);
    el("rect", { x: cx - sw / 2, y: y + kw * 0.4, width: sw, height: kh - kw * 0.8, rx: sw / 2, class: "slit" }, g);
    el("rect", { x: cx - pitch / 2, y: cy - h * 0.24, width: pitch, height: h * 0.48, fill: "transparent" }, g);
    keys[k] = { g, name: keyName(k) };
  };
  WHITE.forEach((k, i) => key(x0 + 12 + (i + 0.5) * pitch, y0 + h * 0.75, k));
  BLACK_AFTER.forEach((a, j) => key(x0 + 12 + (a + 1) * pitch, y0 + h * 0.25, BLACK[j]));

  const hit = (c, cls, role) => {
    const b = c.g.getBBox(), d = document.createElement("div");
    d.className = "hit" + (cls ? " " + cls : "");
    Object.assign(d.style, { left: `${b.x / L.w * 100}%`, top: `${b.y / L.h * 100}%`,
                             width: `${b.width / L.w * 100}%`, height: `${b.height / L.h * 100}%` });
    d.setAttribute("role", role);
    d.setAttribute("aria-label", c.name);
    d.title = c.name;
    d.tabIndex = 0;
    d.addEventListener("touchstart", (e) => e.preventDefault(), { passive: false });
    d.addEventListener("contextmenu", (e) => e.preventDefault());
    device.insertBefore(d, document.getElementById("start"));
    c.hit = d;
  };
  knobs.forEach((k) => { hit(k, "knob", "slider"); bindKnob(k); drawKnob(k); });
  buttons.forEach((b) => { hit(b, "", "button"); bindButton(b); });
  keys.forEach((k, i) => { hit(k, "", "button"); bindKey(k, i); });
}

/* ------------------------------------------------------------------ input */
function setButton(b, on) {
  b.g.classList.toggle("held", on);
  b.hit.setAttribute("aria-pressed", on);
  send({ type: "btn", id: b.bit, on });
}
function bindButton(b) {
  let held = false;
  b.hit.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    b.hit.setPointerCapture(e.pointerId);
    btnBy.set("p" + e.pointerId, b);
    held = true;
    setButton(b, true);
  });
  const up = (e) => {
    if (e && e.pointerId !== undefined) btnBy.delete("p" + e.pointerId);
    if (!held) return;
    held = false;
    setButton(b, false);
  };
  b.hit.addEventListener("pointerup", up);
  b.hit.addEventListener("pointercancel", up);
  b.hit.addEventListener("lostpointercapture", up);
  b.hit.addEventListener("keydown", (e) => {
    if ((e.key === "Enter" || e.key === " ") && !e.repeat) { e.preventDefault(); held = true; setButton(b, true); }
  });
  b.hit.addEventListener("keyup", (e) => {
    if (e.key === "Enter" || e.key === " ") { e.preventDefault(); up(); }
  });
}
function setKey(i, on) {
  const k = keys[i];
  if (!k) return;
  k.g.classList.toggle("held", on);
  if (on) {
    clearTimeout(releaseTimers.get(i));
    releaseTimers.delete(i);
    send({ type: "key", k: i, on: true });
  } else if (!releaseTimers.has(i)) {
    releaseTimers.set(i, setTimeout(() => { releaseTimers.delete(i); send({ type: "key", k: i, on: false }); }, 45));
  }
}
function bindKey(k, i) {
  k.hit.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    k.hit.setPointerCapture(e.pointerId);
    keyBy.set("p" + e.pointerId, i);
    setKey(i, true);
  });
  const up = (e) => {
    const id = "p" + e.pointerId;
    if (keyBy.delete(id)) setKey(i, false);
  };
  k.hit.addEventListener("pointerup", up);
  k.hit.addEventListener("pointercancel", up);
  k.hit.addEventListener("lostpointercapture", up);
}
function releaseAll() {
  for (const b of btnBy.values()) setButton(b, false);
  btnBy.clear();
  for (const i of keyBy.values()) setKey(i, false);
  keyBy.clear();
}


function drawKnob(k) { if (k.ind) k.ind.setAttribute("transform", `rotate(${angle[k.name]} ${k.x} ${k.y})`); }
function turn(k, n) {
  if (!n) return;
  if (k.role === "master") {
    masterValue = Math.max(0, Math.min(1023, masterValue + n * 16));
    angle.MASTER = -135 + 270 * masterValue / 1023;
    if (gain) gain.gain.setTargetAtTime(masterValue / 1023, ac.currentTime, 0.01);
  } else {
    angle[k.name] = (angle[k.name] + n * 18) % 360;
    send({ type: "enc", role: k.role, n });
  }
  drawKnob(k);
}
function bindKnob(k) {
  let drag = null, wheel = 0;
  k.hit.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    k.hit.setPointerCapture(e.pointerId);
    drag = { y: e.clientY, acc: 0 };
  });
  k.hit.addEventListener("pointermove", (e) => {
    if (!drag) return;
    drag.acc += drag.y - e.clientY;
    drag.y = e.clientY;
    const step = k.role === "master" ? 3 : 8, n = Math.trunc(drag.acc / step);
    if (n) { drag.acc -= n * step; turn(k, n); }
  });
  const end = () => { drag = null; };
  k.hit.addEventListener("pointerup", end);
  k.hit.addEventListener("pointercancel", end);
  k.hit.addEventListener("wheel", (e) => {
    e.preventDefault();
    const dy = e.deltaMode === 1 ? e.deltaY * 33 : e.deltaY;
    if (Math.abs(dy) >= 50) { turn(k, dy < 0 ? 1 : -1); wheel = 0; return; }
    wheel -= dy;
    const n = Math.trunc(wheel / 25);
    if (n) { wheel -= n * 25; turn(k, n); }
  }, { passive: false });
  k.hit.addEventListener("keydown", (e) => {
    const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key];
    if (d) { turn(k, d); e.preventDefault(); e.stopPropagation(); }
  });
}
const tall = matchMedia("(orientation: portrait) and (max-width: 700px)");
build(tall.matches ? "tall" : "wide");
tall.addEventListener("change", () => build(tall.matches ? "tall" : "wide"));
document.fonts?.load("16px DotGothic16").then(() => build(layout), () => {});

/* the computer keyboard */
const KEYMAP = {};
["KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period", "Slash"].forEach((c, i) => KEYMAP[c] = { key: WHITE[i] });
[["KeyS", 1], ["KeyD", 3], ["KeyF", 5], ["KeyH", 8], ["KeyJ", 10], ["KeyL", 13], ["Semicolon", 15], ["Quote", 17]].forEach(([c, k]) => KEYMAP[c] = { key: k });
["KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO"].forEach((c, i) => KEYMAP[c] = { key: 18 + i });
["Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal"]
  .forEach((c, i) => KEYMAP[c] = { btn: B[[...ROW1, ...ROW2][i]] });
Object.assign(KEYMAP, { BracketLeft: { btn: B["OCT-"] }, BracketRight: { btn: B["OCT+"] } });
const ARROWS = { ArrowUp: [EN.SELECT, 1], ArrowDown: [EN.SELECT, -1], ArrowRight: [EN.ALGORITHM, 1], ArrowLeft: [EN.ALGORITHM, -1] };
addEventListener("keydown", (e) => {
  if (!node || e.metaKey || e.ctrlKey || e.altKey || e.target.closest?.(".hit.knob")) return;
  if (ARROWS[e.code]) { const [r, n] = ARROWS[e.code]; turn(knobs.find((k) => k.role === r), n); e.preventDefault(); return; }
  const m = KEYMAP[e.code];
  if (!m) return;
  e.preventDefault();
  if (e.repeat) return;
  if (m.key !== undefined) { keyBy.set("k" + e.code, m.key); setKey(m.key, true); }
  else { btnBy.set("k" + e.code, buttons.find((b) => b.bit === m.btn)); setButton(buttons.find((b) => b.bit === m.btn), true); }
});
addEventListener("keyup", (e) => {
  const id = "k" + e.code;
  if (keyBy.delete(id)) setKey(KEYMAP[e.code].key, false);
  const b = btnBy.get(id);
  if (b) { btnBy.delete(id); setButton(b, false); }
});
addEventListener("blur", releaseAll);

/* ------------------------------------------------------------------ screen and lights */
const ctx = canvas.getContext("2d");
const img = ctx.createImageData(240, 240);
function drawScreen(fb) {
  const d = img.data;
  for (let i = 0; i < 240 * 240; i++) {
    const v = fb[i];
    d[i * 4] = ((v >> 11) & 31) * 255 / 31;
    d[i * 4 + 1] = ((v >> 5) & 63) * 255 / 63;
    d[i * 4 + 2] = (v & 31) * 255 / 31;
    d[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}
function bit(lo, hi, id) { return id < 32 ? (lo >>> id) & 1 : (hi >>> (id - 32)) & 1; }
function drawLights([lo, hi, glo, ghi]) {
  for (const b of buttons) {
    const on = bit(lo, hi, b.bit), dim = bit(glo, ghi, b.bit);
    b.g.classList.toggle("lit", !!on);
    b.g.classList.toggle("dim", !!dim && !on);
  }
  keys.forEach((k, i) => {
    const id = KEY_LED0 + i;
    const on = bit(lo, hi, id), dim = bit(glo, ghi, id);
    k.g.classList.toggle("lit", !!on);
    k.g.classList.toggle("dim", !!dim && !on);
  });
}

/* ------------------------------------------------------------------ switching on */
async function switchOn() {
  const go = document.getElementById("go");
  go.disabled = true;
  go.textContent = "Starting…";
  try {
    ac = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" });
    const [wasm] = await Promise.all([
      fetch("riptide.wasm", { cache: "no-cache" }).then((r) => {
        if (!r.ok) throw new Error(`riptide.wasm: ${r.status}`);
        return r.arrayBuffer();
      }),
      ac.audioWorklet.addModule("worklet.js?v=" + Date.now()),
    ]);
    node = new AudioWorkletNode(ac, "riptide", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
    gain = ac.createGain();
    gain.gain.value = masterValue / 1023;
    node.connect(gain).connect(ac.destination);
    await new Promise((res, rej) => {
      node.port.onmessage = (e) => {
        const m = e.data;
        if (m.type === "ready") res();
        else if (m.type === "error") rej(new Error(m.message));
        else if (m.type === "frame") {
          if (m.fb) drawScreen(m.fb);
          if (m.leds) drawLights(m.leds);
        }
      };
      node.port.postMessage({ type: "load", wasm }, [wasm]);
    });
    await ac.resume();
    document.getElementById("start").hidden = true;
    send({ type: "visible", on: !document.hidden });
    document.addEventListener("visibilitychange", () => send({ type: "visible", on: !document.hidden }));
  } catch (err) {
    node = null;
    go.disabled = false;
    go.textContent = "Switch on";
    say("Could not start: " + (err && err.message || err) + ". A current Chrome, Edge, Firefox or Safari should work.");
  }
}
document.getElementById("go").addEventListener("click", switchOn);
