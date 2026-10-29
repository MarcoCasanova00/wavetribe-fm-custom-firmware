/* SPDX-License-Identifier: GPL-3.0-only */
/* RIPTIDE: an groovy groovebox firmware for the M-VAVE FM-1, built on Dinghy
 * (zvenson), Felucca (Leo Kuroshita / Hügelton Instruments) and SLOOP (isod89).
 *
 * Everything platform.c does is unchanged: updates, the USB rescue, MIDI (USB + TRS),
 * USB audio, keys, knobs, LEDs, the screen, flash storage. Below is only the app: a
 * 8-part step sequencer (5 synthesized drums + 3 FM voices), a 240x240 groovy
 * screen, and the the source app factory preset catalog (tools/extract/make_catalog.py).
 *
 *   ENC_SELECT  the part (KICK..LEAD)
 *   ENC_PRESETS the pattern (the the app catalog; show name/genre)
 *   ENC_ALGORITHM  pattern length page (16/32/48/64 steps)
 *   KNOB1..KNOB4  the selected part's A/B/C/D parameters
 *   GLO          holds a global page: KNOB1 tempo, KNOB2 swing, KNOB3 accent, KNOB4 page
 *   PLAY         start / stop
 *   REC (hold)   write mode: keys 0-15 toggle steps of the selected part
 *   HOME (hold)  mute mode: keys 0-7 toggle mute of a part
 *   keys         play the selected part live (a drum hits; a tonal part plays the note)
 *
 * Rules (the chip has no FPU, no libgcc): integers only, no float, no 64-bit division.
 * tests/host.c runs this file on a computer: the screen as PNG, the sound as WAV. */

static void app_idle(void);
#define LCD_IDLE app_idle
#define CV_MAX (240u * 192u)           /* a taller canvas for the 8-row step grid */
#include "platform.c"
#if BOARD_SMK37
#include "smk_wheel.h"
#endif

#include "factory_catalog.h"      /* the authentic factory preset names/authors/genres */
#include "factory_drum_kits.h"    /* kit bank: APP + 909; see import_drum_kits.py */
#include "factory_wavetables.h"   /* imported single-cycle waves (import_wavetables.py) */
#include "factory_patches.h"      /* synth engine patches (patch helper) */

#ifdef DINGHY_HOST
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...)
#endif

/* a tiny unsigned-decimal formatter (the chip has no libc sprintf) */
static void utoa(uint32_t v, char *s)
{
    char t[8]; uint32_t i = 0, j = 0;
    do { t[i++] = (char)('0' + v % 10u); v /= 10u; } while (v && i < 7u);
    while (i) s[j++] = t[--i];
    s[j] = 0;
}

/* copy a C string to *p, return the NUL, so a line can be built up piece by piece */
static char *ap(char *p, const char *t)
{
    while ((*p++ = *t++)) { }
    return p - 1;
}
static char *apu(char *p, uint32_t v) { char t[8]; utoa(v, t); return ap(p, t); }

/* ================= the screen ================= */
#define C_BG     RGB(14, 12, 9)     /* near-black chassy LCD */
#define C_PANEL  RGB(26, 22, 16)
#define C_SUB    RGB(70, 56, 40)
#define C_TEXT    RGB(235, 158, 34) /* amber, the app's LCD */
#define C_TEXT2   RGB(255, 210, 120)
#define C_HL     RGB(255, 92, 0)   /* bright orange: the playing step */
#define C_MUTE   RGB(64, 58, 48)
#define C_CELL    RGB(210, 130, 20) /* a step that is set */

/* part names (8 = 5 drums + 3 tonal) */
static const char *const PNAME[8] = {
    "KICK", "SNARE", "CLAP", "CHH", "OHH", "BASS", "CHRD", "LEAD"};

/* ================= synthesis ================= */
#define FS 44100u
#define NV 6u                       /* polyphonic FM voices */
#define NDRUM 5u                    /* kick, snare, clap, close hat, open hat */

static const uint32_t INC_C9[12] = {   /* MIDI 120..131; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t note_inc(uint8_t note)   /* 0..127 -> phase step per frame */
{
    return note >= 120u ? INC_C9[note % 12u] : INC_C9[note % 12u] >> (10u - note / 12u);
}

static int32_t sine(uint32_t ph)         /* -32767..32767 over 2^32 */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

/* --- drums: authentic the app one-shots, played from flash (DRUM_BANK).
 * A drum is a playback cursor into a const int16 sample. The part's A knob sets a
 * decay/muffle (a moving average over the tail), B a pitch offset (playback rate),
 * D the level. B runs at native pitch; B>64 speeds up (brighter/shorter), <64 slows. */
enum { DK_KICK, DK_SNARE, DK_CLAP, DK_CHH, DK_OHH };
typedef struct {
    uint32_t pos;                   /* sample cursor (fixed point, 16 fractional bits) */
    uint32_t inc;                   /* playback rate: 1<<16 = native pitch */
    int32_t env;                    /* amplitude envelope 0..~32767 */
    int32_t prev;                   /* low-pass state for the muffle */
    uint32_t muffle;                /* decay knob state */
    uint8_t active;
} drm_t;
/* per-part synthesis parameters A..D (0..127), edited by the knobs */
static uint8_t part_prm[8][4];

static drm_t drum[NDRUM];
static uint8_t drum_kit;
static uint8_t patch_sel;
static uint8_t fx_mode; /* 0 off, 1 chorus, 2 delay, 3 chorus+delay */

static void drum_trigger(uint32_t d, uint32_t vel)
{
    drm_t *k = &drum[d];
    uint32_t b = (uint32_t)part_prm[d][1];            /* B: pitch */
    k->pos = 0;
    /* 1<<16 native; B 0..127 -> ~0.5x..1.5x */
    k->inc = 65536u + ((uint32_t)((int32_t)b - 64) * 512u);
    k->env = (int32_t)(8000 + vel * 190u) * ((int32_t)part_prm[d][3] + 32u) >> 7;
    k->prev = 0;
    k->muffle = (uint32_t)part_prm[d][0];             /* A: decay/muffle */
    k->active = 1;
}

static int32_t drum_tick(uint32_t d)
{
    drm_t *k = &drum[d];
    uint32_t n = DRUM_BANKS[drum_kit][d].n, idx;
    const int16_t *smp = DRUM_BANKS[drum_kit][d].p;
    int32_t s, e;
    uint32_t inc = k->inc, pos = k->pos;
    if (!k->active) return 0;
    idx = pos >> 16;
    if (idx >= n) { k->active = 0; return 0; }
    s = smp[idx];
    e = k->env;
    /* muffle: a one-pole low-pass whose strength follows the A knob */
    {
        int32_t f = (int32_t)k->muffle;               /* 0..127 */
        k->prev += ((s - k->prev) * f) >> 9;          /* f/512 smoothing */
        if (f > 32) s = k->prev;
    }
    /* decay envelope: shortens with the A knob too */
    if (e > 0) {
        int32_t dec = 4080 - (int32_t)k->muffle * 2;  /* lower A = faster decay */
        e = (e * dec) >> 12;
        if (e < 40) e = 0;
        k->env = e;
        s = (s * e) >> 15;
    } else {
        k->active = 0;
        return 0;
    }
    k->pos = pos + inc;
    return s;
}

/* --- FM voices --- */
typedef struct {
    uint32_t cph, mph, cinc, minc;
    int32_t modidx, gate, amp;
    uint8_t note, age, wave;
} sv_t;
static sv_t sv[NV];
static uint32_t sv_ages;

/* simple mono effect chain (cheap integer DSP, no FPU) */
static int16_t fx_buf[8192];
static uint32_t fx_w;
#define FX_MASK 8191u
static int16_t lfo_phase;

static void sv_trigger(uint8_t note, uint8_t vel, uint32_t ratio, uint32_t mod, uint8_t wave)
{
    uint32_t pick = 0, best = 0xFFFFFFFFu, v;
    if (note > 127u || !vel) return;
    for (v = 0; v < NV; v++) {
        uint32_t score = sv[v].note == note && sv[v].gate ? 0u
                       : sv[v].gate ? 0x80000000u + sv[v].age : (uint32_t)sv[v].amp + 1u;
        if (score < best) { best = score; pick = v; }
    }
    sv[pick].note = note;
    sv[pick].age = (uint8_t)++sv_ages;
    sv[pick].cinc = note_inc(note);
    sv[pick].minc = (uint32_t)(((uint64_t)sv[pick].cinc * ratio) >> 16);   /* ratio Q16 */
    sv[pick].modidx = (int32_t)(mod * 64u);       /* 0..127 -> 0..8128 */
    sv[pick].cph = 0; sv[pick].mph = 0;
    sv[pick].gate = (int32_t)(vel * 70u);         /* 127 -> 8890 */
    sv[pick].amp = sv[pick].gate;
    sv[pick].wave = (uint8_t)(wave % WT_COUNT);
}
static void sv_release(uint8_t note)
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (sv[v].note == note) sv[v].gate = 0;
}

/* --- the event queue between the main loop and the audio ISR --- */
#define EVQN 48u
enum { EV_NONE, EV_DRUM, EV_NOTEON, EV_NOTEOFF };
typedef struct { uint8_t kind, p, note, vel; } ev_t;
static volatile uint32_t eqw, eqr;
static ev_t eq[EVQN];
static void eq_put(uint8_t kind, uint8_t p, uint8_t note, uint8_t vel)
{
    uint32_t n = (eqw + 1u) % EVQN;
    if (n == eqr) return;
    eq[eqw].kind = kind; eq[eqw].p = p; eq[eqw].note = note; eq[eqw].vel = vel;
    eqw = n;
}

/* per-part synthesis parameters A..D (0..127), edited by the knobs */
static uint32_t idle_ok;                /* LCD_IDLE: set only around our own drawing */

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t i, v;
    /* drain the events (the main loop poked us) */
    while (eqr != eqw) {
        ev_t e = eq[eqr];
        eqr = (eqr + 1u) % EVQN;
        switch (e.kind) {
        case EV_DRUM: if (e.p < NDRUM) drum_trigger(e.p, e.vel); break;
        case EV_NOTEON:
            if (e.p >= NDRUM) {
                uint32_t pr = e.p - NDRUM, ratio = 65536 + (uint32_t)part_prm[pr][1] * 512u;
                uint8_t wv = (uint8_t)(((part_prm[pr][0] >> 3)) % WT_COUNT);
                sv_trigger(e.note, e.vel, ratio, part_prm[pr][2] + 1u, wv);
            }
            break;
        case EV_NOTEOFF: if (e.p >= NDRUM) sv_release(e.note); break;
        default: break;
        }
    }
    /* drums — sample playback, advanced per frame */
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        uint32_t dd;
        for (dd = 0; dd < NDRUM; dd++)
            if (drum[dd].active) s += drum_tick(dd);
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
    /* voices */
    for (v = 0; v < NV; v++) {
        if (!sv[v].gate && !sv[v].amp) continue;
        uint32_t cph = sv[v].cph, mph = sv[v].mph, minc = sv[v].minc, cinc = sv[v].cinc;
        int32_t amp = sv[v].amp, g = sv[v].gate, mod = sv[v].modidx;
        for (i = 0; i < n; i++) {
            int32_t s, m;
            if (amp < g) { amp += 40; if (amp > g) amp = g; }
            else if (amp > g) { amp -= 8; if (amp < g) amp = g; }
            mph += minc;
            m = sine(mph);
            {
                /* FM-style phase modulation into the wavetable carrier */
                uint32_t ci = cinc + (uint32_t)(((int64_t)cinc * (m * (mod >> 1)) >> 12) >> 16);
                cph += ci;
            }
            s = ((int32_t)WT_DATA[sv[v].wave][cph >> WT_SHIFT] * amp) >> 15;
            out[2u * i] += s; out[2u * i + 1u] += s;
        }
        sv[v].cph = cph; sv[v].mph = mph; sv[v].amp = amp; sv[v].age++;
    }
    /* --- simple global effect chain (no FPU) --- */
    if (fx_mode) {
        uint32_t i;
        for (i = 0; i < n; i++) {
            int32_t x = (int32_t)out[2u * i];
            int32_t lfo = (sine((uint32_t)lfo_phase << 16) + 32767) >> 14; /* 0..31 */
            uint32_t tap1 = (fx_w + FX_MASK + 1u - (200u + (uint32_t)lfo)) & FX_MASK;
            uint32_t tap2 = (fx_w + FX_MASK + 1u - 600u) & FX_MASK;
            int32_t wet = 0;
            if (fx_mode >= 2) wet += (int32_t)fx_buf[tap2];
            if (fx_mode == 1 || fx_mode == 3) wet += (int32_t)fx_buf[tap1];
            fx_buf[fx_w] = (int16_t)x;
            fx_w = (fx_w + 1u) & FX_MASK;
            lfo_phase = (uint16_t)(lfo_phase + 1u);
            x += wet >> 1;
            if (x > 32767) x = 32767; else if (x < -32768) x = -32768;
            out[2u * i] = (int16_t)x;
            out[2u * i + 1u] = (int16_t)x;
        }
    }
}

/* ================= patterns & sequencer ================= */
#define NPART 8u
#define NSTEP 64u

#include "factory_patterns.h"     /* authentic the source app gates (make_patterns.py) */

/* The working pattern: gates come from the factory grids; the synth parts get note
 * lines supplied here (the IPA's grids encode gates, not our synth notes). */
struct pat {
    uint8_t gates[NPART][NSTEP];   /* byte per step, 0/1 */
    uint8_t notes[NPART][NSTEP];   /* tonal parts: note 0 = rest */
};

/* hand-written synth note lines, one per demo slot (bass / chord / lead rows 5-7) */
static const uint8_t DEMO_NOTES[N_DEMO_PAT][3][16] = {
    /* TECHNO */
    { {36,0,0,0, 0,0,48,0, 0,0,48,0, 0,0,0,0}, {0}, {0} },
    /* HOUSE */
    { {36,0,0,0, 0,0,0,0, 36,0,0,0, 0,0,0,0}, {0}, {0} },
    /* DUBSTEP */
    { {36,0,0,0, 0,0,48,0, 0,0,0,0, 0,0,43,0}, {0}, {0} },
    /* D&B */
    { {36,0,0,0, 0,0,0,0, 36,0,0,0, 0,0,0,0}, {0}, {0} },
    /* ELECTRO */
    { {36,0,0,0, 0,0,0,0, 36,0,0,0, 0,0,0,0}, {0}, {0} },
    /* TECHNO2 */
    { {36,0,0,0, 0,0,48,0, 0,0,48,0, 0,0,0,0}, {0}, {0} },
    /* HOUSE2 */
    { {36,0,0,0, 0,0,0,0, 36,0,0,0, 0,0,0,0}, {0}, {0} },
    /* DUB */
    { {36,0,0,0, 0,0,0,0, 48,0,0,0, 0,0,0,0}, {0}, {0} },
};

/* the working pattern, edited on screen and saved to flash */
static struct pat pat;
static uint8_t run, write_arm, glomode;
static uint8_t part_sel, pat_sel, page, swing, accent, bpm;
static uint8_t mute[NPART];
static uint8_t hold_note[NPART];       /* the note each tonal part is sustaining from the sequencer */
static uint16_t step_cur;            /* the step being played (0..63) */
static uint32_t next_step_ms, step_ms;

static void pattern_load(uint8_t i)
{
    uint32_t p, s;
    if (i >= N_DEMO_PAT) i = 0;
    for (p = 0; p < NPART; p++)
        for (s = 0; s < NSTEP; s++) {
            pat.gates[p][s] = DEMO_PAT[i].gates[p][s];
            if (p >= NDRUM)
                pat.notes[p][s] = DEMO_NOTES[i][p - NDRUM][s % 16u];
            else
                pat.notes[p][s] = 0;
        }
}

/* ================= settings (flash) ================= */
struct set {
    uint8_t magic[4];
    uint8_t bpm, swing, page, part_sel, pat_sel, accent;
    uint8_t mute[NPART];
    uint8_t prm[8][4];
    struct pat pat;
};
static struct set S;

static void set_defaults(void)
{
    S.magic[0] = 'R'; S.magic[1] = 'I'; S.magic[2] = 'P'; S.magic[3] = '1';
    S.bpm = 128; S.swing = 0; S.page = 0; S.part_sel = 0; S.pat_sel = 0; S.accent = 40;
    memset(S.mute, 0, sizeof S.mute);
    memset(S.prm, 64, sizeof S.prm);
    pattern_load(0); memcpy(&S.pat, &pat, sizeof pat);
}

/* ================= the screen: groovy ================= */
static void draw_header(uint32_t playing)
{
    char s[40];
    draw_text_box(2, 2, 60, &FONT_S, APP_NAME, C_TEXT2, 0);
    /* pattern name + genre, from the catalog */
    {
        uint32_t i = pat_sel < N_CATALOG ? pat_sel : N_CATALOG - 1u;
        const char *nm = CATALOG[i].name, *g = CATALOG[i].genre;
        uint32_t k = 0;
        while (*nm && k < 13) s[k++] = *nm++;
        s[k++] = ' ';
        if (*g) { while (*g && k < 22) s[k++] = *g++; }
        s[k] = 0;
        draw_text_box(62, 2, 120, &FONT_S, s, C_TEXT, 0);
    }
    /* tempo */
    {
        char *p = ap(s, "");
        p = apu(p, bpm);
        p = ap(p, "BPM");
        (void)p;
        draw_text_box(170, 2, 68, &FONT_S, s, playing ? C_TEXT2 : C_SUB, 2);
    }
}

static void draw_parts(void)
{
    uint32_t p, s, x, y, w, h, cw = 12, labw = 38, top = 34, rh = 20;
    cv_begin(240, 8u * rh, C_PANEL);
    /* labels */
    for (p = 0; p < NPART; p++) {
        uint32_t y0 = p * rh;
        cv_rect(0, y0, labw, rh, p == part_sel ? C_PANEL : C_BG);
        cv_text(2, y0 + 3, &FONT_S, PNAME[p], p == part_sel ? C_TEXT2 : C_SUB);
        if (mute[p]) cv_text(labw - 12, y0 + 3, &FONT_S, "M", C_MUTE);
    }
    /* cells: 16 columns per page view (position = step_cur) */
    {
        uint32_t base = (step_cur / 16u) * 16u;
        for (p = 0; p < NPART; p++) {
            for (s = 0; s < 16u; s++) {
                uint32_t st = base + s, lit = pat.gates[p][st % NSTEP];
                x = labw + s * cw; y = p * rh;
                w = cw - 2; h = rh - 3;
                if (mute[p]) { cv_rect(x, y, w, h, C_MUTE); continue; }
                if (lit)
                    cv_rect(x, y, w, h, s == step_cur % 16u ? C_HL : C_CELL);
                else
                    cv_rect(x, y, w, h, s == step_cur % 16u ? C_HL : C_PANEL);
                cv_rect(x, y, w, 1, C_SUB);
            }
        }
    }
    cv_blit(0, top);
}

static void draw_footer(uint32_t playing)
{
    char s[48];
    char *p = s;
    uint32_t q;
    draw_text_box(2, 210, 236, &FONT_S, "", C_BG, 0);
    if (write_arm) {
        p = ap(p, "REC  "); p = ap(p, PNAME[part_sel]); p = ap(p, "  keys: steps");
    } else if (glomode) {
        if (playing) {
            p = ap(p, "GLO  "); p = apu(p, bpm); p = ap(p, "BPM sw");
            p = apu(p, swing); p = ap(p, " acc"); p = apu(p, accent);
        } else {
            p = ap(p, "GLO  tempo/swing/accent/page (knobs)");
        }
    } else {
        p = ap(p, PNAME[part_sel]);
        if (part_sel < NDRUM) {
            p = ap(p, "  ["); p = ap(p, DRUM_KIT_NAMES[drum_kit]); p = ap(p, "]");
        }
        for (q = 0; q < 4u; q++) {
            *p++ = ' '; *p++ = (char)('A' + q);
            p = apu(p, part_prm[part_sel][q]);
        }
        *p = 0;
    }
    draw_text_box(2, 212, 236, &FONT_S, s, (write_arm || glomode) ? C_TEXT2 : (playing ? C_TEXT : C_SUB), 0);
}

static void draw_screen(uint32_t playing)
{
    draw_header(playing);
    draw_parts();
    draw_footer(playing);
}

/* ================= main ================= */
static uint32_t last_repaint;
static uint32_t dirty, saved_ms;

static void seq_stop(void)
{
    uint32_t p;
    run = 0;
    for (p = NDRUM; p < NPART; p++)
        if (hold_note[p]) {
            eq_put(EV_NOTEOFF, p, hold_note[p], 0);
            hold_note[p] = 0;
        }
    step_cur = 0;
}

static void fire_step(void)
{
    uint32_t p;
    for (p = 0; p < NPART; p++) {
        if (mute[p]) continue;
        if (p < NDRUM) {
            if (pat.gates[p][step_cur])
                eq_put(EV_DRUM, p, 0, 100 + (accent ? (step_cur % 4u == 0u ? accent : 0) : 0));
        } else {
            uint8_t old = hold_note[p], note = pat.notes[p][step_cur];
            if (old) eq_put(EV_NOTEOFF, p, old, 0);
            hold_note[p] = note;
            if (note) eq_put(EV_NOTEON, p, note, 110);
        }
    }
}

static void seq_tick(void)
{
    if (!run) { next_step_ms = fm1_ms + step_ms; return; }
    while (next_step_ms <= fm1_ms) {
        fire_step();
        step_cur++;
        if (step_cur >= 16u * (page + 1u)) step_cur = 0;
        /* step length, with a little swing on the offbeats */
        {
            uint32_t base = step_ms, extra = (step_cur & 1u) ? (step_ms * swing / 200u) : 0u;
            next_step_ms += base + extra;
        }
    }
}

static void app_idle(void) { if (idle_ok) { /* nothing to do here: sequencer runs on the main loop */ } }

static uint32_t prev_keys, last_step = 0xFFFFu, last_pat = 0xFFFFu;
static uint8_t held[27];

static void app_init(void)
{
    uint32_t k;

    /* settings */
    if (settings_load(&S, sizeof S) || memcmp(S.magic, "RIP", 4)) {
        set_defaults();
        if (!settings_save(&S, sizeof S)) { /* keep defaults */ }
    }
    bpm = S.bpm; swing = S.swing; page = S.page; part_sel = S.part_sel;
    pat_sel = S.pat_sel; accent = S.accent; memcpy(mute, S.mute, sizeof mute);
    memcpy(part_prm, S.prm, sizeof part_prm);
    memcpy(&pat, &S.pat, sizeof pat);
    if (pat.gates[0][0] == 0 && pat.gates[0][1] == 0) pattern_load(pat_sel % N_DEMO_PAT);
    step_ms = 15000u / (bpm ? bpm : 128u);
    drum_kit = 0;
    prev_keys = 0;
    for (k = 0; k < 27u; k++) held[k] = 0;

    lcd_fill(0, 0, 240, 240, C_BG);
    draw_screen(0);
    last_repaint = 0; saved_ms = 0; dirty = 0;
}

static void app_tick(void)
{
    uint32_t keys, down, up, b, rel, k;

    seq_tick();

    keys = fm1_in.notes;
    down = keys & ~prev_keys;
    up = prev_keys & ~keys;
    prev_keys = keys;

        b = fm1_input_edges(&rel);

        /* --- buttons --- */
        if (b & (1u << BTN_PLAY)) {
            if (glomode) { }
            else if (run) seq_stop();
            else { run = 1; step_cur = 0; next_step_ms = fm1_ms + step_ms; }
            dirty = 1;
        }
        if (b & (1u << BTN_FX)) {
            fx_mode = (uint8_t)((fx_mode + 1u) % 4u);
            dirty = 1;
        }
        if (b & (1u << BTN_GLO)) { glomode = !glomode; dirty = 1; }
        if (b & (1u << BTN_REC)) { write_arm = !write_arm; dirty = 1; }
        if (b & (1u << BTN_HOME)) {
            for (k = 0; k < 8u; k++) if (down >> k & 1u) mute[k] = !mute[k];
            seq_stop(); write_arm = 0; dirty = 1;
        }

        /* --- encoders --- */
        {
            int32_t e;
            if ((e = fm1_enc_take(ENC_SELECT)) != 0) {
                int32_t c = (int32_t)part_sel + e;
                part_sel = (uint8_t)(c < 0 ? 0 : c > 7 ? 7 : c);
                dirty = 1;
            }
            e = fm1_enc_take(ENC_PRESETS);
            if (e) {
                if (glomode) {
                    uint32_t i, q;
                    int32_t c = (int32_t)patch_sel + (e > 0 ? 1 : -1);
                    if (c < 0) c = N_PATCHES - 1;
                    if (c >= (int32_t)N_PATCHES) c = 0;
                    patch_sel = (uint8_t)c;
                    for (i = 0u; i < 3u; i++)
                        for (q = 0u; q < 4u; q++)
                            part_prm[NDRUM + (uint8_t)i][q] = PATCH_PRM[patch_sel][i][q];
                } else {
                    int32_t c = (int32_t)pat_sel + e;
                    if (c < 0) c = N_CATALOG - 1;
                    if (c >= (int32_t)N_CATALOG) c = 0;
                    pat_sel = (uint8_t)c;
                    if (pat_sel < (int32_t)N_DEMO_PAT) { pattern_load(pat_sel); memcpy(&S.pat, &pat, sizeof pat); }
                }
                dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
                int32_t c = (int32_t)page + e;
                page = (uint8_t)(c < 0 ? 0 : c > 3 ? 3 : c);
                dirty = 1;
            }
            e = fm1_enc_take(ENC_KNOB1);
            if (glomode && e) { int32_t c = bpm + e; bpm = (uint8_t)(c < 60 ? 60 : c > 250 ? 250 : c); step_ms = 15000u / bpm; dirty = 1; }
            else if (e) { int32_t c = part_prm[part_sel][0] + e; part_prm[part_sel][0] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB2);
            if (glomode && e) { int32_t c = swing + e; swing = (uint8_t)(c < 0 ? 0 : c > 100 ? 100 : c); dirty = 1; }
            else if (e && part_sel < NDRUM) { int32_t c = drum_kit + (e > 0 ? 1 : -1);
                drum_kit = (uint8_t)(c < 0 ? 0 : c >= (int32_t)N_DRUM_KITS ? N_DRUM_KITS - 1 : c); dirty = 1; }
            else if (e) { int32_t c = part_prm[part_sel][1] + e; part_prm[part_sel][1] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB3);
            if (glomode && e) { int32_t c = accent + e; accent = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            else if (e) { int32_t c = part_prm[part_sel][2] + e; part_prm[part_sel][2] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB4);
            if (glomode && e) { int32_t c = (int32_t)page + (e > 0 ? 1 : -1); page = (uint8_t)(c < 0 ? 0 : c > 3 ? 3 : c); dirty = 1; }
            else if (e) { int32_t c = part_prm[part_sel][3] + e; part_prm[part_sel][3] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
        }

        /* --- keys: write steps, or play live --- */
        for (k = 0; k < 27u; k++) {
            if (write_arm) {
                if (down >> k & 1u) {
                    uint32_t st = (step_cur / 16u) * 16u + k;
                    if (st < 64u) pat.gates[part_sel][st] ^= 1u;
                    dirty = 1;
                }
            } else {
                if (down >> k & 1u) {
                    if (part_sel < NDRUM) eq_put(EV_DRUM, part_sel, 0, 110);
                    else {
                        held[k] = (uint8_t)(36 + k);
                        eq_put(EV_NOTEON, part_sel, held[k], 110);
                    }
                }
                if (up >> k & 1u) {
                    if (part_sel >= NDRUM && held[k]) eq_put(EV_NOTEOFF, part_sel, held[k], 0), held[k] = 0;
                }
            }
        }

        /* --- LEDs --- */
        for (k = 0; k < 16u; k++)
            fm1_led_key_glow(KEY_LED(k), pat.gates[part_sel][step_cur / 16u * 16u + k]);
        fm1_led_key(BTN_PLAY, run);
        fm1_led_key(BTN_REC, write_arm);
        fm1_led_key(BTN_GLO, glomode);

        /* --- draw what changed --- */
        idle_ok = 1;
        if (step_cur != last_step || pat_sel != last_pat || dirty || fm1_ms - last_repaint > 250u) {
            draw_screen(run);
            last_step = step_cur; last_pat = pat_sel; last_repaint = fm1_ms; dirty = 0;
        }
        idle_ok = 0;

        /* --- save when quiet & 2 s after a change --- */
        if (dirty && fm1_ms - saved_ms > 2000u && !run) {
            S.bpm = bpm; S.swing = swing; S.page = page; S.part_sel = part_sel;
            S.pat_sel = pat_sel; S.accent = accent;
            memcpy(S.mute, mute, sizeof mute);
            memcpy(S.prm, part_prm, sizeof part_prm);
            memcpy(&S.pat, &pat, sizeof pat);
            saved_ms = fm1_ms;
            if (!settings_save(&S, sizeof S)) dirty = 0;
        }
}

static void app_main(void)
{
    app_init();
    for (;;) {
        platform_poll();
        app_tick();
    }
}
