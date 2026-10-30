// SPDX-License-Identifier: GPL-3.0-only
// RIPTIDE in an AudioWorklet: riptide.wasm is the whole firmware against the fake FM-1 in
// tests/host.c. Each render quantum asks it for 128 frames, which runs the device's clock
// forward; between quanta the page's input goes in and, about 30 times a second, the screen
// and the panel's LEDs go out. The wasm has only two imports (wasi fd_write, emscripten's
// heap grow), so it can be instantiated here without Emscripten's JavaScript glue.

const N = 240 * 240;
const MAX_FRAMES = 2048;

let ex = null, memory = null, audioPtr = 0, snapPtr = 0, snapLen = 0;

/* printf goes nowhere in the audio thread: report every byte written so libc does not retry */
function fdWrite(fd, iov, iovLen, pwritten)
{
    try {
        const dv = new DataView(memory.buffer);
        let total = 0;
        for (let i = 0; i < iovLen; i++) total += dv.getUint32(iov + i * 8 + 4, true);
        dv.setUint32(pwritten, total, true);
    } catch (e) { /* the heap moved: the message is lost, the device is not */ }
    return 0;
}

function resizeHeap(want)
{
    try {
        const need = want - memory.buffer.byteLength;
        if (need > 0) memory.grow(Math.ceil(need / 65536));
        return 1;
    } catch (e) { return 0; }
}

class Riptide extends AudioWorkletProcessor {
    constructor()
    {
        super();
        this.lastPublish = -1;
        this.period = 1 / 30;
        this.lastLeds = "";
        this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m)
    {
        if (m.type === "load") {
            try {
                const { instance } = await WebAssembly.instantiate(m.wasm, {
                    wasi_snapshot_preview1: { fd_write: fdWrite },
                    env: { emscripten_resize_heap: resizeHeap },
                });
                memory = instance.exports.memory;
                ex = instance.exports;
                ex.__wasm_call_ctors?.();
                ex.web_init();
                audioPtr = ex.malloc(MAX_FRAMES * 4);
                snapLen = ex.web_snap_bytes() * ex.web_snap_count() + 4 * ex.web_snap_count();
                snapPtr = ex.malloc(snapLen);
                this.port.postMessage({ type: "ready" });
            } catch (err) {
                this.port.postMessage({ type: "error", message: String(err && err.message || err) });
            }
            return;
        }
        if (!ex) return;
        if (m.type === "btn") ex.web_btn(m.id | 0, m.on ? 1 : 0);
        else if (m.type === "key") ex.web_key(m.on ? 1 : 0, (1 << (m.k | 0)) >>> 0);
        else if (m.type === "enc") ex.web_enc(m.role | 0, m.n | 0);
        else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
        else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
        else if (m.type === "snapGet") {
            ex.web_snap_export(snapPtr);
            const snap = new Uint8Array(memory.buffer, snapPtr, snapLen).slice();
            this.port.postMessage({ type: "snapOut", data: snap }, [snap.buffer]);
        } else if (m.type === "snapSet") {
            new Uint8Array(memory.buffer, snapPtr, snapLen).set(m.data);
            ex.web_snap_import(snapPtr);
        }
    }

    publish()
    {
        const lo = ex.web_leds_lo() >>> 0, hi = ex.web_leds_hi() >>> 0;
        const glo = ex.web_glow_lo() >>> 0, ghi = ex.web_glow_hi() >>> 0;
        const key = lo + "," + hi + "," + glo + "," + ghi;
        const msg = { type: "frame" };
        if (key !== this.lastLeds) { this.lastLeds = key; msg.leds = [lo, hi, glo, ghi]; }
        const fb = new Uint16Array(memory.buffer, ex.web_fb_ptr(), N).slice();
        msg.fb = fb;
        this.port.postMessage(msg, [fb.buffer]);
    }

    process(inputs, outputs)
    {
        if (!ex) return true;
        const out = outputs[0], n = out[0].length;
        ex.web_audio(audioPtr, n);
        const a = new Int16Array(memory.buffer, audioPtr, n * 2);
        const L = out[0], R = out[1] || out[0];
        for (let i = 0; i < n; i++) {
            L[i] = a[2 * i] / 32768;
            R[i] = a[2 * i + 1] / 32768;
        }
        if (currentTime - this.lastPublish >= this.period) {
            this.lastPublish = currentTime;
            this.publish();
        }
        return true;
    }
}

registerProcessor("riptide", Riptide);
