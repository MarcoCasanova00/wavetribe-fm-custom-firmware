/* SPDX-License-Identifier: GPL-3.0-only */
/* fm_engine.c - 4-operator FM voice engine.
 *
 * Signal flow per voice:
 *
 *   OP4 -> OP3 -> OP2 -> OP1 -> carrier sum -> VCA -> drive -> SVF -> pan
 *
 * Operators may also cross-modulate and feed back, and each voice carries a
 * dedicated filter envelope (the second, independent envelope) plus a VCA
 * envelope.  Everything is fixed point: Q32 phase, Q15 samples, Q8 macros.
 */
#include "fm_engine.h"
#include "fm_tables.h"
#if defined(DINGHY_HOST) || defined(DINGHY_WEB)
#include <string.h>     /* host/web libc */
#define FM_MEMSET(dst, val, n) memset((dst), (val), (n))
#else
/* bare-metal toolchain: no <string.h>; memset is provided by the compiler */
static void fm_memset(void *s, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
}
#define FM_MEMSET(dst, val, n) fm_memset((dst), (val), (n))
#endif

/* Envelope increments in Q8 level units per sample (32767 << 8 full scale).
 * Rate 0 is a slow 16 s sweep, rate 31 reaches full scale in under a
 * millisecond.  The curve is roughly one octave per 2.2 rate steps, which is
 * close to the Yamaha 4-op EG feel. */
static const uint32_t FM_ENV_INC[32] = {
    12, 16, 23, 31, 42, 58, 79, 109, 149, 204, 280, 384, 526, 721, 988, 1354,
    1856, 2543, 3485, 4775, 6544, 8967, 12288, 16839, 23075, 31621, 43332,
    59380, 81371, 111507, 152804, 209396
};

/* 2 * sin(pi * fc / 44100) * 4096, 40 Hz .. 12 kHz. */
static const uint16_t FM_FLT_COEF[128] = {
    23, 24, 26, 27, 28, 29, 31, 32, 33, 35, 37, 38, 40, 42, 44, 46,
    48, 50, 52, 55, 57, 60, 63, 66, 69, 72, 75, 78, 82, 86, 90, 94,
    98, 103, 107, 112, 118, 123, 129, 135, 141, 147, 154, 161, 168, 176,
    184, 193, 202, 211, 220, 231, 241, 252, 264, 276, 289, 302, 316, 330,
    345, 361, 378, 395, 413, 432, 452, 473, 495, 517, 541, 566, 592, 619,
    647, 677, 708, 740, 774, 810, 847, 886, 926, 968, 1013, 1059, 1107,
    1158, 1211, 1266, 1323, 1384, 1447, 1512, 1581, 1652, 1727, 1805,
    1887, 1972, 2060, 2153, 2249, 2350, 2454, 2563, 2677, 2795, 2918,
    3045, 3178, 3316, 3458, 3606, 3760, 3800, 3800, 3800, 3800, 3800,
    3800, 3800, 3800, 3800, 3800, 3800, 3800, 3800
};

/* Modulator bit masks: bit k set means OPk modulates this operator.  Only
 * higher numbered operators are used as sources, so the fixed computation
 * order OP4 -> OP1 is safe.  OP1 is a carrier in every algorithm. */
static const uint8_t FM_MOD_MASK[FM_ALGORITHMS][FM_OPS_PER_VOICE] = {
    { 0x02, 0x04, 0x08, 0x00 },   /* 0: serial stack 4>3>2>1       */
    { 0x04, 0x08, 0x00, 0x00 },   /* 1: two 2-op stacks           */
    { 0x04, 0x00, 0x08, 0x00 },   /* 2: 4>3>1, OP2 carrier        */
    { 0x06, 0x08, 0x00, 0x00 },   /* 3: 4>2>1 and 3>1             */
    { 0x0A, 0x04, 0x00, 0x00 },   /* 4: 4>1, 3>2>1                */
    { 0x0E, 0x00, 0x00, 0x00 },   /* 5: 4,3,2 all >1              */
    { 0x0C, 0x00, 0x00, 0x00 },   /* 6: 4,3 >1 with OP2 carrier   */
    { 0x00, 0x00, 0x00, 0x00 }    /* 7: four carriers             */
};

static const uint8_t FM_CARRIER[FM_ALGORITHMS] = {
    0x01, 0x03, 0x03, 0x01, 0x01, 0x01, 0x03, 0x0F
};

typedef struct {
    uint32_t phase;
    uint32_t inc;
    int32_t  last;
    int32_t  env;
    uint16_t amp;
    uint16_t kvs_scale;
    uint8_t  state;          /* 0 off 1 attack 2 decay1 3 decay2 4 release 5 hold */
} fm_op_t;

typedef struct {
    fm_patch_t p;
    uint8_t    macro[4];
    fm_op_t    op[FM_OPS_PER_VOICE];
    int32_t    flt_low, flt_band;
    int32_t    flt_e;
    int32_t    vca;
    uint8_t    vca_state;
    uint8_t    note, vel, key, active;
    uint8_t    pan;
    uint8_t    oscmode;    /* 0 normal, 1 sync, 2 ring/AM, 3 PD, 4 VEC */
    uint32_t   lfo_phase;
    uint32_t   vec_phase;  /* slow 2D vector sweep (modes 4) */
} fm_voice_t;

static fm_voice_t fm_voices[FM_MAX_VOICES];

/* ------------------------------------------------------------------ helpers */

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int32_t fm_sine(uint32_t phase)
{
    return FM_SINE[phase >> 22];
}

/* Casio-CZ-style phase distortion: bend the sine read index by an amount.
 * amt 0 = pure sine, higher = sharper (saw/square-like) shapes. */
static uint32_t pd_warp(uint32_t idx, uint8_t amt)
{
    uint32_t a = idx & 1023u;
    uint32_t b;
    if (a >= 512u) a = 1023u - a;
    b = a + ((((uint32_t)amt * (a * (511u - a)))) >> 17);
    if (b > 511u) b = 511u;
    return (idx & 512u) ? (1023u - b) : b;
}

static int32_t fm_wave(uint8_t w, uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t s = FM_SINE[i];
    switch (w & 7u) {
    case 0:                      /* sine */
        return s;
    case 1:                      /* half sine */
        return i < 512u ? s : 0;
    case 2:                      /* rectified sine */
        return s < 0 ? -s : s;
    case 3:                      /* alternating quarter pulses */
        return (i < 256u || (i >= 512u && i < 768u)) ? s : 0;
    case 4: {                    /* triangle */
        int32_t t = (int32_t)(ph >> 16) & 0xFFFF;
        return t < 32768 ? (t * 2 - 32768) : (32767 - (t - 32768) * 2);
    }
    case 5:                      /* saw */
        return (int32_t)(ph >> 16) - 32768;
    case 6:                      /* square */
        return (ph & 0x80000000u) ? -28000 : 28000;
    default: {                   /* sine plus third harmonic */
        int32_t s3 = FM_SINE[(i * 3u) & 1023u];
        return (s * 2 + s3) / 3;
    }
    }
}

static int32_t soft_clip(int32_t x)
{
    const int32_t knee = 22000;
    if (x > knee) x = knee + ((x - knee) >> 2);
    else if (x < -knee) x = -knee + ((x + knee) >> 2);
    if (x > 30000) x = 30000;
    if (x < -30000) x = -30000;
    return x;
}

const char *fm_macro_wave_label(uint8_t macro)
{
    /* Mirrors the rotation used in fm_process: centre (64) = sine. */
    uint8_t w = (uint8_t)((((uint32_t)macro >> 4) + 4u) & 7u);
    static const char *const W[8] = {
        "sine", "half sine", "rect sine", "quarter pulse",
        "triangle", "saw", "square", "sine+3rd"
    };
    return W[w];
}

/* Advance one operator envelope.  Returns nonzero while the operator lives. */
static int op_env_tick(fm_op_t *o, const fm_op_patch_t *p,
                       uint8_t loop, uint8_t gate, uint8_t sustain, uint8_t key)
{
    switch (o->state) {
    case 1: {                                   /* attack */
        o->env += (int32_t)FM_ENV_INC[p->ar & 31u];
        if (o->env >= 32767 << 8) { o->env = 32767 << 8; o->state = 2; }
        break;
    }
    case 2: {                                   /* decay 1 */
        int32_t target = ((int32_t)p->d1l * (32767 << 8)) / 15;
        if (o->env > target) {
            o->env -= (int32_t)FM_ENV_INC[p->d1r & 31u];
            if (o->env < target) o->env = target;
        }
        if (o->env <= target) {
            if (loop && key) o->state = 1;
            else if (sustain && key) o->state = 5;
            else if (gate && !key) o->state = 4;
            else o->state = 3;
        }
        break;
    }
    case 3:                                     /* decay 2 to silence */
        o->env -= (int32_t)FM_ENV_INC[p->d2r & 31u];
        if (o->env <= 0) { o->env = 0; o->state = 0; }
        break;
    case 4:                                     /* release */
        /* rr is 0..15: double it to address the 0..31 rate table */
        o->env -= (int32_t)FM_ENV_INC[(p->rr & 15u) * 2u];
        if (o->env <= 0) { o->env = 0; o->state = 0; }
        break;
    case 5:                                     /* hold decay 1 level */
        if (!key) o->state = gate ? 4 : 3;
        break;
    default:
        return 0;
    }
    return o->state != 0;
}

/* ------------------------------------------------------------------- API */

void fm_init(void)
{
    uint32_t v, o;
    FM_MEMSET(fm_voices, 0, sizeof fm_voices);
    for (v = 0; v < FM_MAX_VOICES; v++) {
        for (o = 0; o < 4; o++) fm_voices[v].macro[o] = 64;
        fm_voices[v].pan = 0;
    }
}

void fm_reset(void)
{
    uint32_t v;
    for (v = 0; v < FM_MAX_VOICES; v++) {
        fm_voices[v].active = 0;
        fm_voices[v].vca_state = 0;
        fm_voices[v].vca = 0;
    }
}

void fm_all_off(void)
{
    fm_reset();
}

void fm_load_patch(uint8_t voice, const fm_patch_t *patch)
{
    fm_voice_t *v;
    uint32_t o;
    if (voice >= FM_MAX_VOICES || !patch) return;
    v = &fm_voices[voice];
    v->p = *patch;
    if (v->p.alg >= FM_ALGORITHMS) v->p.alg &= 7u;
    if (v->p.fb > 7u) v->p.fb &= 7u;
    for (o = 0; o < 4; o++) {
        uint32_t out = v->p.op[o].out;
        if (out > 99u) out = 99u;
        /* Quadratic level curve: 99 -> full scale, 50 -> about -12 dB. */
        v->op[o].amp = (uint16_t)((out * out * 32767u) / (99u * 99u));
        v->op[o].env = 0;
        v->op[o].state = 0;
        v->op[o].last = 0;
    }
}

void fm_set_macro(uint8_t voice, uint8_t index, uint8_t value)
{
    if (voice >= FM_MAX_VOICES || index >= 4u) return;
    fm_voices[voice].macro[index] = value;
}

void fm_set_lfo(uint8_t voice, uint8_t on, uint8_t wave, uint8_t speed,
                uint8_t pmd, uint8_t amd)
{
    if (voice >= FM_MAX_VOICES) return;
    fm_voice_t *v = &fm_voices[voice];
    v->p.lfo_on = !!on;
    v->p.lfo_wave = wave & 7u;
    v->p.lfo_speed = speed & 127u;
    v->p.pmd = pmd & 127u;
    v->p.amd = amd & 127u;
}

void fm_set_vca(uint8_t voice, uint8_t ar, uint8_t dr, uint8_t sus, uint8_t rr)
{
    if (voice >= FM_MAX_VOICES) return;
    fm_voice_t *v = &fm_voices[voice];
    v->p.vca_ar = ar & 31u;
    v->p.vca_dr = dr & 31u;
    v->p.vca_sus = sus & 127u;
    v->p.vca_rr = rr & 31u;
}

void fm_set_oscmode(uint8_t voice, uint8_t mode)
{
    if (voice >= FM_MAX_VOICES) return;
    if (mode > 4u) mode = 4u;
    fm_voices[voice].oscmode = mode;
}

void fm_get_patch(uint8_t voice, fm_patch_t *out)
{
    if (voice >= FM_MAX_VOICES || !out) return;
    *out = fm_voices[voice].p;
}

void fm_note_on(uint8_t voice, uint8_t note, uint8_t velocity)
{
    fm_voice_t *v;
    uint32_t o;
    if (voice >= FM_MAX_VOICES) return;
    v = &fm_voices[voice];
    v->note = note;
    v->vel = velocity;
    v->key = 1;
    v->active = 1;
    v->vca = 0;
    v->vca_state = 1;
    v->flt_low = 0;
    v->flt_band = 0;
    v->flt_e = v->p.flt_env ? (32767 << 8) : 0;
    v->lfo_phase = 0;
    for (o = 0; o < 4; o++) {
        fm_op_t *op = &v->op[o];
        int32_t ks = 256 - ((int32_t)v->p.op[o].kvs * (127 - (int32_t)velocity) >> 4);
        op->phase = 0;
        op->last = 0;
        op->env = 0;
        op->state = 1;
        op->kvs_scale = (uint16_t)clamp32(ks, 64, 256);
    }
}

void fm_note_off(uint8_t voice, uint8_t note)
{
    fm_voice_t *v;
    uint32_t o;
    if (voice >= FM_MAX_VOICES) return;
    v = &fm_voices[voice];
    if (!v->active) return;
    if (note && v->note != note) return;
    v->key = 0;
    if (v->p.gate) {
        for (o = 0; o < 4; o++)
            if (v->op[o].state) v->op[o].state = 4;
    }
    /* The VCA always releases so a note can never hang. */
    v->vca_state = 4;
}

void fm_process(int32_t *out_l, int32_t *out_r, uint32_t frames)
{
    uint32_t v, i, o, f;

    for (v = 0; v < FM_MAX_VOICES; v++) {
        fm_voice_t *vo = &fm_voices[v];
        fm_patch_t *p = &vo->p;
        uint8_t wave, alg, gate;
        int32_t level_q8, mod_q8, ratio_ofs;
        int32_t pitch_q8, am_q15, lfo;
        int32_t base_note;
        uint32_t lfo_inc;

        if (!vo->active) continue;

        alg = p->alg & 7u;
        gate = p->gate;
        /* Macro A selects the operator waveform. Rotate so the centre value
         * (64) picks sine (case 0), matching the knob/UI centre. */
        wave = (uint8_t)((((uint32_t)vo->macro[0] >> 4) + 4u) & 7u);
        level_q8 = 256 + ((int32_t)vo->macro[3] - 64) * 4;
        level_q8 = clamp32(level_q8, 0, 512);
        mod_q8 = 256 + ((int32_t)vo->macro[2] - 64) * 4;
        mod_q8 = clamp32(mod_q8, 16, 1024);
        ratio_ofs = ((int32_t)vo->macro[1] - 64) >> 4;   /* -4 .. +3 */

        /* Whole-voice transposition (24 = centre), clamped to +/- one octave. */
        base_note = (int32_t)vo->note + (int32_t)p->transpose - 24;
        base_note = clamp32(base_note, 0, 127);

        /* LFO: a block-rate oscillator drives vibrato (pmd) and tremolo
         * (amd).  The waveform is selectable; when disabled the LFO is flat. */
        lfo_inc = 4096u + (uint32_t)p->lfo_speed * p->lfo_speed * 120u;
        if (p->lfo_on) {
            vo->lfo_phase += lfo_inc;
            lfo = fm_wave(p->lfo_wave & 7u, vo->lfo_phase);
        } else {
            lfo = 0;
        }
        pitch_q8 = 256 + ((lfo * (int32_t)p->pmd) >> 17);
        pitch_q8 = clamp32(pitch_q8, 128, 512);
        am_q15 = 32767 - (((lfo + 32767) >> 1) * (int32_t)p->amd >> 9);
        am_q15 = clamp32(am_q15, 16384, 32767);

        /* Refresh operator increments so the ratio macro is live. */
        for (o = 0; o < 4; o++) {
            int32_t coarse = (int32_t)p->op[o].coarse + ratio_ofs;
            uint32_t ratio, inc;
            int32_t det;
            coarse = clamp32(coarse, 0, 63);
            ratio = FM_RATIO_Q8[((uint32_t)coarse << 4) | (p->op[o].fine & 15u)];
            inc = (uint32_t)(((uint64_t)FM_NOTE_INC[base_note] * ratio) >> 8);
            det = (int32_t)p->op[o].det - 3;
            if (det) inc = (inc * (uint32_t)(256 + det * 3)) >> 8;
            vo->op[o].inc = inc;
        }

        for (f = 0; f < frames; f++) {
            int32_t op_out[4];
            int32_t sum = 0;
            int32_t cut, fc, damp;
            uint8_t sync_latch = 0;

            /* vector sweep advances once per sample */
            if (vo->oscmode == 4u) vo->vec_phase += 2u + (p->lfo_speed >> 3);

            for (o = 4; o-- > 0; ) {
                fm_op_t *op = &vo->op[o];
                fm_op_patch_t *pp = &p->op[o];
                int32_t mod = 0;
                uint32_t mask = FM_MOD_MASK[alg][o];

                if (!op->state) { op_out[o] = 0; op->last = 0; continue; }

                op_env_tick(op, pp, p->loop, gate, p->sustain, vo->key);

                if (mask) {
                    if (mask & 1u) mod += op_out[0];
                    if (mask & 2u) mod += op_out[1];
                    if (mask & 4u) mod += op_out[2];
                    if (mask & 8u) mod += op_out[3];
                }
                /* Feedback lives on OP4, the usual 4-op entry point. */
                if (o == 3u && p->fb)
                    mod += (op->last * (int32_t)p->fb) >> 3;
                /* Polivoks-ish cross modulation: the next operator bleeds in. */
                if (p->cross)
                    mod += (vo->op[(o + 1u) & 3u].last * (int32_t)p->cross) >> 8;
                mod = (mod * mod_q8) >> 8;
                mod = clamp32(mod, -32767, 32767);

                {
                    uint32_t p = op->phase + ((uint32_t)mod << 16);
                    /* hard sync: OP1 restarts whenever OP4 wraps a cycle */
                    if (o == 0u && vo->oscmode == 1u && sync_latch) op->phase = 0;
                    /* phase distortion (CZ-style): warp the read index by macro C */
                    if (vo->oscmode == 3u)
                        p = (pd_warp(p >> 22, vo->macro[2]) << 22) | (p & 0x3FFFFFu);
                    {
                        int32_t s;
                        /* vector synthesis: bilinear morph across four adjacent waves */
                        if (vo->oscmode == 4u && o == 0u) {
                            uint32_t vp = vo->vec_phase;
                            uint32_t fx = (vp >> 16) & 255u, fy = ((vp >> 16) + 128u) & 255u;
                            int32_t s0 = fm_wave((wave + 0u) & 7u, p);
                            int32_t s1 = fm_wave((wave + 1u) & 7u, p);
                            int32_t s2 = fm_wave((wave + 2u) & 7u, p);
                            int32_t s3 = fm_wave((wave + 3u) & 7u, p);
                            int32_t w0 = (int32_t)((255u - fx) * (255u - fy));
                            int32_t w1 = (int32_t)(fx * (255u - fy));
                            int32_t w2 = (int32_t)((255u - fx) * fy);
                            int32_t w3 = (int32_t)(fx * fy);
                            s = (s0 * w0 + s1 * w1 + s2 * w2 + s3 * w3) / 65025;
                        } else {
                            s = fm_wave(wave, p);
                        }
                        s = (s * (op->env >> 8)) >> 15;
                        s = (s * (int32_t)op->amp) >> 15;
                        s = (s * (int32_t)op->kvs_scale) >> 8;
                        /* ring/AM: scale the carrier by OP4's output */
                        if (o == 0u && vo->oscmode == 2u) s = (s * op_out[3]) >> 15;
                        op_out[o] = s;
                        op->last = s;
                    }
                }
                {
                    uint32_t ph = op->phase;
                    op->phase += (op->inc * (uint32_t)pitch_q8) >> 8;
                    if (o == 3u && op->phase < ph) sync_latch = 1;
                }
            }

            {
                uint32_t cm = FM_CARRIER[alg];
                if (cm & 1u) sum += op_out[0];
                if (cm & 2u) sum += op_out[1];
                if (cm & 4u) sum += op_out[2];
                if (cm & 8u) sum += op_out[3];
            }

            /* independent VCA envelope: ADSR shaped by the four knobs on the
             * Envelope page.  Hold (sustain=1) keeps the note at the sustain
             * level; percussive patches (sustain=0) decay to silence. */
            switch (vo->vca_state) {
            case 1:                            /* attack */
                vo->vca += (int32_t)FM_ENV_INC[p->vca_ar & 31u];
                if (vo->vca >= (32767 << 8)) { vo->vca = 32767 << 8; vo->vca_state = 2; }
                break;
            case 2: {                          /* decay */
                if (p->sustain) {
                    int32_t target = ((int32_t)p->vca_sus * (32767 << 8)) / 127;
                    if (vo->vca > target) {
                        vo->vca -= (int32_t)FM_ENV_INC[p->vca_dr & 31u];
                        if (vo->vca < target) vo->vca = target;
                    }
                    if (vo->vca <= target) vo->vca_state = 3;
                } else {
                    vo->vca -= (int32_t)FM_ENV_INC[p->vca_dr & 31u];
                    if (vo->vca <= 0) {
                        vo->vca = 0;
                        if (p->loop && vo->key) vo->vca_state = 1;
                        else vo->vca_state = 0;
                    }
                }
                break;
            }
            case 3:                            /* sustain (key-held) */
                if (!vo->key) vo->vca_state = 4;
                break;
            case 4:                            /* release */
                vo->vca -= (int32_t)FM_ENV_INC[p->vca_rr & 31u];
                if (vo->vca <= 0) { vo->vca = 0; vo->vca_state = 0; }
                break;
            default:
                break;
            }

            sum = (sum * (vo->vca >> 8)) >> 15;
            sum = (sum * am_q15) >> 15;
            sum = (sum * level_q8) >> 10;

            /* VCA drive / grit */
            sum = (sum * (64 + (int32_t)p->drive)) >> 7;

            /* resonant 12 dB state variable filter */
            cut = (int32_t)p->flt_cut + (((vo->flt_e >> 8) * (int32_t)p->flt_env) >> 15);
            cut = clamp32(cut, 0, 127);
            fc = FM_FLT_COEF[cut];
            damp = 256 - (int32_t)p->flt_res * 2;
            if (damp < 2) damp = 2;
            vo->flt_low += (fc * vo->flt_band) >> 12;
            {
                int32_t high = sum - vo->flt_low - ((vo->flt_band * damp) >> 8);
                vo->flt_band += (fc * high) >> 12;
            }
            vo->flt_low = clamp32(vo->flt_low, -60000, 60000);
            vo->flt_band = clamp32(vo->flt_band, -60000, 60000);
            sum = vo->flt_low;

            if (p->flt_env) {
                vo->flt_e -= (int32_t)FM_ENV_INC[p->flt_rate & 31u];
                if (vo->flt_e < 0) vo->flt_e = 0;
            }

            sum = soft_clip(sum);

            /* stereo spread: voice 0 left, voice 1 centre, voice 2 right */
            {
                int32_t lq = 256 + vo->pan;
                int32_t rq = 256 - vo->pan;
                out_l[f] += (sum * lq) >> 8;
                out_r[f] += (sum * rq) >> 8;
            }
        }

        if (vo->vca_state == 0 && !p->loop) {
            uint32_t any = 0;
            for (o = 0; o < 4; o++) any |= vo->op[o].state;
            if (!any) vo->active = 0;
        }
    }

    /* Keep the engine from ever sending the mixer into hard overflow. */
    for (i = 0; i < frames; i++) {
        out_l[i] = clamp32(out_l[i], -30000, 30000);
        out_r[i] = clamp32(out_r[i], -30000, 30000);
    }
}
