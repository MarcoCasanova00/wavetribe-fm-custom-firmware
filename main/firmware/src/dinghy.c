/* SPDX-License-Identifier: GPL-3.0-only */
/* FRANKENTRIBE OS: an FM-1 groovebox firmware, built on Dinghy
 * (zvenson), Felucca (Leo Kuroshita / Hügelton Instruments) and SLOOP (isod89).
 *

 * Everything platform.c does is unchanged: updates, the USB rescue, MIDI (USB + TRS),
 * USB audio, keys, knobs, LEDs, the screen, flash storage. Below is only the app: a
 * 8-part step sequencer (5 synthesized drums + 3 FM voices), a 240x240 groovy
 * screen, and the the source app factory preset catalog (tools/extract/make_catalog.py).
 *
 *   ENC_SELECT  the part (KICK..LEAD)
 *   ENC_PRESETS browse the sound (whole list, per-part cursor) on any page
 *               except SEQ, where it cycles the demo pattern
 *   ENC_ALGORITHM  pattern length page (16/32/48/64 steps)
 *   KNOB1..KNOB4  the selected part's A/B/C/D parameters
 *   GLO+EDIT   opens the SOUNDS browse page (KNOB1 category, KNOB2 sound)
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
#define CV_MAX (240u * 210u)           /* full page canvas (footer hints at y188 + glyph height) */
#ifndef APP_NAME
#define APP_NAME "FRANKENTRIBE OS"
#endif
#include "platform.c"
#if BOARD_SMK37
#include "smk_wheel.h"
#endif

#include "factory_catalog.h"      /* the authentic factory preset names/authors/genres */
#include "factory_drum_kits.h"    /* kit bank: APP + 909; see import_drum_kits.py */
#include "abuse_drum_kit.h"       /* Abuse SFX-derived kit */
#include "factory_wavetables.h"   /* imported single-cycle waves (import_wavetables.py) */
#include "mined_roms.h"           /* EPROM-mined waves from rare synth dumps */
#define TOTAL_WT_COUNT (WT_COUNT + MWT_COUNT)   /* combined wavetable pool */
#define TOTAL_DRUMS N_DRUM_KITS                 /* verified factory kits only */
static const char *drum_kit_name(uint8_t k) {
    return DRUM_KIT_NAMES[k];
}
static drum_smp_t drum_kit_smp(uint8_t k, uint8_t d) {
    return DRUM_BANKS[k][d];
}
/* instrument category tags for UI distinction */
static const char *drum_kit_cat(uint8_t k) {
    (void)k; return "FACTORY";
}
static const char *wt_category(uint16_t w) {
    if (w < WT_COUNT) return "FACTORY";
    if (w < WT_COUNT + 16) return "DE-HOHNER";  /* Hohner Adam */
    if (w < WT_COUNT + 32) return "DE-BOHM";     /* Bohm DD16 */
    if (w < WT_COUNT + 48) return "UK-CHEETAH";  /* Cheetah MD16 */
    return "EPROM-MINED";
}
#include "factory_patchbanks.h"   /* APP, DX7, ROM banks (melting-pot patches) */
#include "fm_engine.h"
#include "fm_patches_tx81z.h"
#include "fm_patches_fb01.h"
#include "abuse_skin.h"           /* ABUSE skin mascot (decoded from Abuse) */
extern uint32_t env_rate_to_inc(uint8_t rate);
             /* 4-operator FM synth engine */

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

/* trim a string until it fits a pixel width (the screen is 240 wide; nothing may overhang) */
static void fit_text(char *s, int32_t maxw)
{
    while (s[0] && text_w(&FONT_S, s) > maxw) {
        char *q = s;
        while (q[1]) q++;
        *q = 0;
    }
}
#define C_BG     RGB(14, 12, 9)     /* near-black chassy LCD */
#define C_PANEL  RGB(26, 22, 16)
#define C_SUB    RGB(70, 56, 40)
#define C_TEXT    RGB(235, 158, 34) /* amber, the app's LCD */
#define C_TEXT2   RGB(255, 210, 120)
#define C_HL     RGB(255, 92, 0)   /* bright orange: the playing step */
#define C_MUTE   RGB(64, 58, 48)
#define C_CELL    RGB(210, 130, 20) /* a step that is set */

/* UI role colours − they follow the active palette (default AMBER): */
#define UI_BG     C_BG                    /* canvas background */
#define UI_PANEL  C_LINE                  /* panels, separators (darkest tint) */
#define UI_DIM    C_DIM                   /* empty/inactive */
#define UI_LABEL  C_GRAY                  /* static labels */
#define UI_AMB    C_AMB                   /* active, secondary text */
#define UI_HI     C_HI                    /* brightest: selection, playhead */
#define UI_MUTE   C_MUTE


/* a clear, scalable Frankenstein-head glyph drawn with primitives */
static void draw_franken(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    int32_t hx = (int32_t)x, hy = (int32_t)y, hw = (int32_t)w, hh = (int32_t)h;
    int32_t bw = hw / 4, bh = hh / 5, ey = hy + hh * 2 / 5;
    /* head body: flat top, an arch of the jaw */
    cv_rect(hx + 1, hy, hw - 2, hh - 1, UI_PANEL);
    /* outline */
    cv_rect(hx + 1, hy, hw - 2, 1, UI_LABEL);
    cv_rect(hx, hy + 1, 1, hh - 2, UI_LABEL);
    cv_rect(hx + hw - 1, hy + 1, 1, hh - 2, UI_LABEL);
    /* the two bolts on the jaw */
    cv_rect(hx - 1, hy + hh * 3 / 5, 3, 3, UI_LABEL);
    cv_rect(hx + hw - 2, hy + hh * 3 / 5, 3, 3, UI_LABEL);
    /* brow stitches above the eyes */
    cv_rect(hx + hw / 4, hy + hh / 5, 1, hh / 6, UI_LABEL);
    cv_rect(hx + hw * 3 / 4 - 1, hy + hh / 5, 1, hh / 6, UI_LABEL);
    /* eyes: two bright blocks */
    cv_rect(hx + hw / 4, ey, bw, bh, UI_HI);
    cv_rect(hx + hw * 3 / 4 - bw, ey, bw, bh, UI_HI);
    /* the scar / mouth */
    cv_rect(hx + hw / 3, hy + hh * 4 / 5, hw / 3, hh / 12, UI_LABEL);
    cv_rect(hx + hw / 2 - 1, hy + hh * 4 / 5, 1, hh / 6, UI_LABEL);
}

/* part names (8 = 5 drums + 3 tonal) */
static const char *const PNAME[8] = {
    "KICK", "SNARE", "CLAP", "CHH", "OHH", "BASS", "CHRD", "LEAD"};

/* ================= synthesis ================= */
#define FS 44100u
#define NDRUM 5u                    /* kick, snare, clap, close hat, open hat */

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
static uint8_t patchbank, patch_ix;
static uint8_t dx7bank_sel;
static uint8_t midi_out;
static uint8_t fx_mode; /* 0 off, 1 chorus, 2 delay, 3 chorus+delay */
static uint8_t view_page;   /* 0 Seq, 1 FX, 2 wave/patch, 3 LFO, 4 Env, 5 Home */
static uint32_t dirty;
static uint8_t part_sel;    /* declared here so the edit helpers can use it */
static uint8_t pal_idx = 0; /* current screen palette (default) */

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
    uint32_t n = drum_kit_smp(drum_kit, d).n, idx;
    const int16_t *smp = drum_kit_smp(drum_kit, d).p;
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

/* --- FM patch loading ---------------------------------------------------- */

static uint32_t midi_clk_acc;  /* frames until next 0xF8 clock pulse */

/* simple mono effect chain (cheap integer DSP, no FPU) */
static int16_t fx_buf[8192];
static uint32_t fx_w;
#define FX_MASK 8191u
static int16_t lfo_phase;

/* A neutral 4-op patch used by the APP / DX7 / ROM macro banks. */
static void fm_base_patch(fm_patch_t *p, uint8_t alg)
{
    uint8_t o;
    memset(p, 0, sizeof *p);
    p->alg = alg & 7u;
    p->fb = 4;
    p->lfo_speed = 18;
    p->lfo_on = 1;
    p->lfo_wave = 0;
    p->pmd = 6;
    p->amd = 0;
    p->transpose = 24;
    p->pb_range = 2;
    p->sustain = 1;
    p->loop = 0;
    p->gate = 1;
    p->cross = 18;
    p->drive = 20;
    p->flt_cut = 100;
    p->flt_res = 18;
    p->flt_env = 12;
    p->flt_rate = 20;
    p->vca_ar = 31;
    p->vca_dr = 14;
    p->vca_sus = 112;
    p->vca_rr = 20;
    for (o = 0; o < 4; o++) {
        p->op[o].ar = 28;
        p->op[o].d1r = 16;
        p->op[o].d2r = 0;         /* hold while the key is down */
        p->op[o].rr = 11;
        p->op[o].d1l = 13;
        p->op[o].kvs = 3;
        p->op[o].out = o == 0 ? 92 : 70;
        p->op[o].coarse = 4;      /* ratio 1.00 */
        p->op[o].fine = 0;
        p->op[o].det = 3;
    }
}

static void fm_load_template_patch(uint8_t voice)
{
    fm_patch_t p;
    fm_base_patch(&p, 2);
    p.fb = 3;
    p.op[1].coarse = 8;           /* 2:1 modulator */
    p.op[1].out = 82;
    p.op[2].coarse = 12;          /* 3.46:1, metallic edge */
    p.op[2].out = 58;
    p.op[3].coarse = 5;           /* 1.41:1 */
    p.op[3].out = 44;
    p.cross = 24;
    p.drive = 26;
    fm_load_patch(voice, &p);
}

/* Load the selected ROM voice into every tonal part (macros stay per part). */
static void fm_load_tx81z_patch(uint8_t ix)
{
    fm_patch_t p;
    uint8_t v, o;
    int32_t tr;
    if (ix >= FM_PATCH_TX81Z_COUNT) return;
    fm_base_patch(&p, FM_PATCH_TX81Z_ALG[ix]);
    p.fb = FM_PATCH_TX81Z_FB[ix] & 7u;
    p.lfo_speed = FM_PATCH_TX81Z_LFO_SPEED[ix] & 127u;
    p.lfo_delay = FM_PATCH_TX81Z_LFO_DELAY[ix] & 127u;
    p.pmd = FM_PATCH_TX81Z_PMD[ix] & 127u;
    p.amd = FM_PATCH_TX81Z_AMD[ix] & 127u;
    p.lfo_wave = FM_PATCH_TX81Z_LFO_WAVE[ix] & 3u;
    p.ams = FM_PATCH_TX81Z_AMS[ix] & 3u;
    p.pms = FM_PATCH_TX81Z_PMS[ix] & 7u;
    tr = (int32_t)FM_PATCH_TX81Z_TRANSPOSE[ix] - 24;
    if (tr < -12) tr = -12;
    if (tr > 12) tr = 12;
    p.transpose = (uint8_t)(24 + tr);
    p.sustain = 0;                 /* keep the ROM's percussive decay */
    p.cross = (uint8_t)(6 + p.fb * 3);
    p.drive = (uint8_t)(12 + p.fb * 4);
    p.flt_cut = (uint8_t)(88 + (p.op[0].out >> 3));
    p.flt_res = 18;
    p.flt_env = 14;
    p.flt_rate = 20;
    p.vca_ar = 31;
    p.vca_dr = 0;
    p.vca_sus = 110;
    p.vca_rr = 18;
    p.lfo_on = 1;
    for (o = 0; o < 4; o++) {
        p.op[o].ar = FM_PATCH_TX81Z_AR[ix][o] & 31u;
        p.op[o].d1r = FM_PATCH_TX81Z_D1R[ix][o] & 31u;
        p.op[o].d2r = FM_PATCH_TX81Z_D2R[ix][o] & 31u;
        p.op[o].rr = FM_PATCH_TX81Z_RR[ix][o] & 15u;
        p.op[o].d1l = FM_PATCH_TX81Z_D1L[ix][o] & 15u;
        p.op[o].ls = FM_PATCH_TX81Z_LS[ix][o];
        p.op[o].rs = FM_PATCH_TX81Z_RS[ix][o] & 3u;
        p.op[o].ebs = FM_PATCH_TX81Z_EBS[ix][o] & 7u;
        p.op[o].ame = FM_PATCH_TX81Z_AME[ix][o] & 1u;
        p.op[o].kvs = FM_PATCH_TX81Z_KVS[ix][o] & 7u;
        p.op[o].out = FM_PATCH_TX81Z_OUT[ix][o];
        p.op[o].coarse = FM_PATCH_TX81Z_CRS[ix][o] & 63u;
        p.op[o].fine = 0;
        p.op[o].det = FM_PATCH_TX81Z_DET[ix][o] & 7u;
    }
    for (v = 0; v < 3u; v++) fm_load_patch(v, &p);
}

/* Load the selected second ROM voice into every tonal part. */
static void fm_load_fb01_patch(uint8_t ix)
{
    fm_patch_t p;
    uint8_t v, o;
    if (ix >= FM_PATCH_FB01_COUNT) return;
    fm_base_patch(&p, FM_PATCH_FB01_ALG[ix]);
    p.fb = FM_PATCH_FB01_FB[ix] & 7u;
    p.lfo_speed = FM_PATCH_FB01_LFO_SPEED[ix] & 127u;
    p.lfo_delay = FM_PATCH_FB01_LFO_DELAY[ix] & 127u;
    p.pmd = FM_PATCH_FB01_PMD[ix] & 127u;
    p.amd = FM_PATCH_FB01_AMD[ix] & 127u;
    p.lfo_wave = FM_PATCH_FB01_LFO_WAVE[ix] & 3u;
    p.ams = FM_PATCH_FB01_AMS[ix] & 3u;
    p.pms = FM_PATCH_FB01_PMS[ix] & 7u;
    p.transpose = 24;
    p.sustain = 0;
    p.cross = (uint8_t)(6 + p.fb * 3);
    p.drive = (uint8_t)(12 + p.fb * 4);
    p.flt_cut = (uint8_t)(88 + (p.op[0].out >> 3));
    p.flt_res = 18;
    p.flt_env = 14;
    p.flt_rate = 20;
    p.vca_ar = 31;
    p.vca_dr = 0;
    p.vca_sus = 110;
    p.vca_rr = 18;
    p.lfo_on = 1;
    for (o = 0; o < 4; o++) {
        p.op[o].ar = FM_PATCH_FB01_AR[ix][o] & 31u;
        p.op[o].d1r = FM_PATCH_FB01_D1R[ix][o] & 31u;
        p.op[o].d2r = FM_PATCH_FB01_D2R[ix][o] & 31u;
        p.op[o].rr = FM_PATCH_FB01_RR[ix][o] & 15u;
        p.op[o].d1l = FM_PATCH_FB01_D1L[ix][o] & 15u;
        p.op[o].ls = FM_PATCH_FB01_LS[ix][o];
        p.op[o].rs = FM_PATCH_FB01_RS[ix][o] & 3u;
        p.op[o].ebs = FM_PATCH_FB01_EBS[ix][o] & 7u;
        p.op[o].ame = FM_PATCH_FB01_AME[ix][o] & 1u;
        p.op[o].kvs = FM_PATCH_FB01_KVS[ix][o] & 7u;
        p.op[o].out = FM_PATCH_FB01_OUT[ix][o];
        p.op[o].coarse = FM_PATCH_FB01_CRS[ix][o] & 63u;
        p.op[o].fine = 0;
        p.op[o].det = FM_PATCH_FB01_DET[ix][o] & 7u;
    }
    for (v = 0; v < 3u; v++) fm_load_patch(v, &p);
}

/* Release every ringing FM voice.  A patch change must not leave the old note
 * sustaining; GR!MM restarts a track's notes when its sound changes (a fade),
 * so we release with the voice envelope rather than cutting abruptly.
 * fm_note_off with note 0 releases the voice regardless of its pitch. */
static void fm_release_all(void)
{
    uint8_t v;
    for (v = 0; v < 3u; v++) fm_note_off(v, 0);
}

/* Load a patch from any bank, applying the macro banks where needed. */
static void load_bank_patch(uint8_t bank, uint8_t ix)
{
    uint8_t i, q;
    if (bank >= N_PATCHBANKS) bank = 0;
    if (ix >= PATCHBANK_COUNT[bank]) ix = 0;
    fm_release_all();              /* stop any sounding notes first */
    if (bank == 3u) {
        fm_load_tx81z_patch(ix);
    } else if (bank == 4u) {
        fm_load_fb01_patch(ix);
    } else {
        for (i = 0; i < 3u; i++)
            for (q = 0; q < 4u; q++)
                part_prm[NDRUM + i][q] = PATCH_PRM3[bank][ix][i][q];
        for (i = 0; i < 3u; i++) fm_load_template_patch(i);
    }
}

/* ---- unified sound browsing (GR!MM-style) ----
 * The FM engine owns one shared sound, but each part keeps its own browse
 * cursor, so turning PRESETS keeps your place when you switch parts (GR!MM's
 * per-track brw_cat / brw_pos).  The whole patch set is one ordered list,
 * grouped by bank (each bank acts as a category). */

static uint32_t snd_total(void)
{
    uint8_t b;
    uint32_t n = 0;
    for (b = 0; b < N_PATCHBANKS; b++) n += PATCHBANK_COUNT[b];
    return n;
}

static void snd_unpack(uint32_t idx, uint8_t *bank, uint8_t *ix)
{
    uint8_t b;
    for (b = 0; b < N_PATCHBANKS; b++) {
        if (idx < PATCHBANK_COUNT[b]) { *bank = b; *ix = (uint8_t)idx; return; }
        idx -= PATCHBANK_COUNT[b];
    }
    *bank = N_PATCHBANKS - 1u;
    *ix = (uint8_t)(PATCHBANK_COUNT[*bank] - 1u);
}

static uint32_t snd_pack(uint8_t bank, uint8_t ix)
{
    uint8_t b;
    uint32_t n = 0;
    if (bank >= N_PATCHBANKS) bank = 0;
    for (b = 0; b < bank; b++) n += PATCHBANK_COUNT[b];
    if (ix >= PATCHBANK_COUNT[bank]) ix = (uint8_t)(PATCHBANK_COUNT[bank] - 1u);
    return n + ix;
}

/* Push the four per-part macros into the live engine. */
static void fm_sync_macros(void)
{
    uint8_t i, q;
    for (i = 0; i < 3u; i++)
        for (q = 0; q < 4u; q++)
            fm_set_macro(i, q, part_prm[NDRUM + i][q]);
}

static const char *patch_display_name(uint8_t bank, uint8_t ix)
{
    if (bank == 3u && ix < FM_PATCH_TX81Z_COUNT) return FM_PATCH_TX81Z_NAME[ix];
    if (bank == 4u && ix < FM_PATCH_FB01_COUNT) return FM_PATCH_FB01_NAME[ix];
    if (bank >= N_PATCHBANKS || ix >= 128u) return "?";
    return PATCH_NAME3[bank][ix];
}

/* -------- live modulation edits (LFO page + Env page) --------
 * The three FM voices share one selected sound, so edits apply to all three
 * (otherwise tuning LFO/env on part 5 wouldn't be heard through part 6/7). */
static void edit_lfo(uint8_t k, int32_t e)
{
    uint8_t base = (uint8_t)(part_sel - NDRUM);
    uint8_t v; int32_t c;
    fm_patch_t pp;
    if (part_sel < NDRUM) return;
    fm_get_patch(base, &pp);
    switch (k) {
    case 0: {                    /* waveform / on-off (0=off, 1..8=wave) */
        v = pp.lfo_on ? (uint8_t)((pp.lfo_wave & 7u) + 1u) : 0u;
        c = (int32_t)v + (e > 0 ? 1 : -1);
        if (c < 0) c = 8;
        if (c > 8) c = 0;
        pp.lfo_on = c > 0;
        pp.lfo_wave = (uint8_t)(c > 0 ? c - 1 : 0);
        break;
    }
    case 1: c = (int32_t)pp.lfo_speed + e; pp.lfo_speed = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); break;
    case 2: c = (int32_t)pp.pmd + e; pp.pmd = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); break;
    default: c = (int32_t)pp.amd + e; pp.amd = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); break;
    }
    for (v = 0; v < 3u; v++) fm_set_lfo(v, pp.lfo_on, pp.lfo_wave, pp.lfo_speed, pp.pmd, pp.amd);
    dirty = 1;
}

static void edit_env(uint8_t k, int32_t e)
{
    uint8_t base = (uint8_t)(part_sel - NDRUM);
    uint8_t v; int32_t c;
    fm_patch_t pp;
    if (part_sel < NDRUM) return;
    fm_get_patch(base, &pp);
    switch (k) {
    case 0: c = (int32_t)pp.vca_ar + e; pp.vca_ar = (uint8_t)(c < 0 ? 0 : c > 31 ? 31 : c); break;
    case 1: c = (int32_t)pp.vca_dr + e; pp.vca_dr = (uint8_t)(c < 0 ? 0 : c > 31 ? 31 : c); break;
    case 2: c = (int32_t)pp.vca_sus + e; pp.vca_sus = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); break;
    default: c = (int32_t)pp.vca_rr + e; pp.vca_rr = (uint8_t)(c < 0 ? 0 : c > 31 ? 31 : c); break;
    }
    for (v = 0; v < 3u; v++) fm_set_vca(v, pp.vca_ar, pp.vca_dr, pp.vca_sus, pp.vca_rr);
    dirty = 1;
}


static uint8_t run, bpm;

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
    uint32_t i;
    /* drain the events (the main loop poked us) */
    while (eqr != eqw) {
        ev_t e = eq[eqr];
        eqr = (eqr + 1u) % EVQN;
        switch (e.kind) {
        case EV_DRUM:
            if (e.p < NDRUM) {
                if (midi_out) {
                    static const uint8_t GM_DRUM[NDRUM] = { 36, 38, 39, 42, 46 };
                    midi_send(0x9A, GM_DRUM[e.p], (uint32_t)(e.vel < 127 ? e.vel : 127));
                }
                drum_trigger(e.p, e.vel);
            }
            break;
        case EV_NOTEON:
            if (e.p >= NDRUM) {
                /* FM note on: use part parameters for patch selection */
                fm_note_on(e.p - NDRUM, e.note, e.vel);
                if (midi_out) midi_send(0x90u + (uint8_t)(e.p - NDRUM), e.note, (uint32_t)(e.vel < 127 ? e.vel : 127));
            }
            break;
        case EV_NOTEOFF:
            if (e.p >= NDRUM) {
                fm_note_off((uint8_t)(e.p - NDRUM), e.note);
                if (midi_out) midi_send(0x80u + (uint8_t)(e.p - NDRUM), e.note, 0);
            }
            break;
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
    
    /* FM voices: the engine renders in small blocks so the stack stays small. */
    {
        uint32_t done = 0;
        int32_t fl[64], fr[64];
        while (done < n) {
            uint32_t len = n - done, j;
            if (len > 64u) len = 64u;
            for (j = 0; j < len; j++) { fl[j] = 0; fr[j] = 0; }
            fm_process(fl, fr, len);
            for (j = 0; j < len; j++) {
                out[2u * (done + j)] += fl[j];
                out[2u * (done + j) + 1u] += fr[j];
            }
            done += len;
        }
    }
    if (midi_out && run) {
        uint32_t bpmx = bpm ? (uint32_t)bpm : 120u;
        uint32_t per_tick = (44100u * 60u) / (bpmx * 24u);
        if (per_tick == 0) per_tick = 1u;
        midi_clk_acc += (uint32_t)n;
        while (midi_clk_acc >= per_tick) {
            midi_send(0xF8, 0, 0);
            midi_clk_acc -= per_tick;
        }
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
static uint16_t snd_mem[NPART];   /* per-part sound browse cursor (GR!MM-style) */
static uint8_t hold_note[NPART];       /* the note each tonal part is sustaining from the sequencer */
static uint16_t step_cur;            /* the step being played (0..63) */
static uint32_t next_step_ms, step_ms;

/* Apply a sound (bank/ix) as the active patch and record it as the focused
 * part's browse position.  Centralises every sound change (PRESETS, CC 93,
 * OCT- category jump, the SOUNDS page, and slot load). */
static void snd_apply(uint8_t bank, uint8_t ix)
{
    if (bank >= N_PATCHBANKS) bank = 0;
    if (ix >= PATCHBANK_COUNT[bank]) ix = 0;
    patchbank = bank;
    patch_ix = ix;
    dx7bank_sel = (bank == 1u);
    load_bank_patch(bank, ix);
    if (part_sel < NPART) snd_mem[part_sel] = (uint16_t)snd_pack(bank, ix);
    dirty = 1;
}

/* Point every part's browse cursor at the current sound (on boot / restore). */
static void snd_mem_reset(void)
{
    uint8_t p;
    uint16_t idx = (uint16_t)snd_pack(patchbank, patch_ix);
    for (p = 0; p < NPART; p++) snd_mem[p] = idx;
}

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
    uint8_t midi_out, dx7_bank, patchbank, patch_ix;
    uint8_t mute[NPART];
    uint8_t prm[8][4];
    struct pat pat;
    /* shared FM sound LFO/VCA tweaks (applied over the bank patch) */
    uint8_t lfo_on, lfo_wave, lfo_speed, pmd, amd;
    uint8_t vca_ar, vca_dr, vca_sus, vca_rr;
};
static struct set S;

static void set_defaults(void)
{
    S.magic[0] = 'R'; S.magic[1] = 'I'; S.magic[2] = 'P'; S.magic[3] = '1';
    S.bpm = 128; S.swing = 0; S.page = 0; S.part_sel = 0; S.pat_sel = 0; S.accent = 40; S.midi_out = 0; S.dx7_bank = 0; S.patchbank = 0; S.patch_ix = 0;
    memset(S.mute, 0, sizeof S.mute);
    memset(S.prm, 64, sizeof S.prm);
    pattern_load(0); memcpy(&S.pat, &pat, sizeof pat);
    S.lfo_on = 1; S.lfo_wave = 0; S.lfo_speed = 18; S.pmd = 6; S.amd = 0;
    S.vca_ar = 31; S.vca_dr = 14; S.vca_sus = 112; S.vca_rr = 20;
}

static uint8_t slot_sel;   /* active user save slot (0..NSLOT-1) */

/* Copy live globals into the S struct (the persistent snapshot). */
static void snapshot_state(void)
{
    fm_patch_t pp;
    S.bpm = bpm; S.swing = swing; S.page = page; S.part_sel = part_sel;
    S.pat_sel = pat_sel; S.accent = accent; S.midi_out = midi_out;
    S.dx7_bank = dx7bank_sel; S.patchbank = patchbank; S.patch_ix = patch_ix;
    memcpy(S.mute, mute, sizeof mute);
    memcpy(S.prm, part_prm, sizeof part_prm);
    memcpy(&S.pat, &pat, sizeof pat);
    /* the three voices share one sound; voice 0 represents it */
    fm_get_patch(0, &pp);
    S.lfo_on = pp.lfo_on; S.lfo_wave = pp.lfo_wave & 7u;
    S.lfo_speed = pp.lfo_speed; S.pmd = pp.pmd; S.amd = pp.amd;
    S.vca_ar = pp.vca_ar; S.vca_dr = pp.vca_dr;
    S.vca_sus = pp.vca_sus; S.vca_rr = pp.vca_rr;
}

/* Apply the saved sound's LFO/VCA tweaks over the (already loaded) bank patch,
 * to all three voices (the sound is shared). */
static void apply_saved_sound(void)
{
    uint8_t v;
    for (v = 0; v < 3u; v++) {
        fm_set_lfo(v, S.lfo_on, S.lfo_wave, S.lfo_speed, S.pmd, S.amd);
        fm_set_vca(v, S.vca_ar, S.vca_dr, S.vca_sus, S.vca_rr);
    }
}

/* Apply S to the live globals (mirrors app_init). */
static void restore_state(void)
{
    bpm = S.bpm; swing = S.swing; page = S.page; part_sel = S.part_sel;
    pat_sel = S.pat_sel; accent = S.accent; midi_out = S.midi_out;
    dx7bank_sel = S.dx7_bank;
    patchbank = S.patchbank; patch_ix = S.patch_ix;
    if (patchbank >= N_PATCHBANKS) patchbank = 0;
    if (patch_ix >= PATCHBANK_COUNT[patchbank]) patch_ix = 0;
    memcpy(mute, S.mute, sizeof mute);
    memcpy(part_prm, S.prm, sizeof part_prm);
    memcpy(&pat, &S.pat, sizeof pat);
    step_ms = 15000u / (bpm ? bpm : 128u);
    load_bank_patch(patchbank, patch_ix);
    apply_saved_sound();
    snd_mem_reset();
    fm_sync_macros();
}

/* User save slots: store/recall a full snapshot into flash slot i. */
static int user_save_slot(uint8_t i)
{
    if (i >= NSLOT) return -1;
    snapshot_state();
    return slot_save(i, &S, sizeof S);
}
static int user_load_slot(uint8_t i)
{
    if (i >= NSLOT) return -1;
    if (slot_load(i, &S, sizeof S)) return -1;
    restore_state();
    return 0;
}
/* True if slot i holds a valid snapshot (CRC-checked read succeeds). */
static int slot_in_use(uint8_t i)
{
    struct set tmp;
    if (i >= NSLOT) return 0;
    return slot_load(i, &tmp, sizeof tmp) == 0;
}
/* Best-effort clear: write a zero-length marker that fails the CRC/length
 * validation, so the slot reads back empty on hardware and host alike.
 */
static void user_clear_slot(uint8_t i)
{
    static const struct set empty;
    if (i >= NSLOT) return;
    slot_save(i, &empty, 0);
}

/* Empty the current pattern (all parts) and rewind. */
static void pattern_clear(void)
{
    memset(pat.gates, 0, sizeof pat.gates);
    memset(pat.notes, 0, sizeof pat.notes);
    step_cur = 0;
}

/* GR!MM-style SAVE page state: action + selected slot. */
static uint8_t action_sel;   /* 0 LOAD, 1 SAVE, 2 CLEAR */

/* ================= the screen: groovy ================= */
static void draw_header(uint32_t playing)
{
    char s[48];
    uint32_t i, k = 0;
    const char *nm, *g;
    int32_t tx;
    cv_begin(240, 34, C_BG);
    /* line 1: face badge + brand (left), tempo (right) */
    draw_franken(3, 3, 15, 18);
    cv_text(23, 1, &FONT_S, "FRANKENTRIBE OS", UI_HI);
    {
        char *p = ap(s, "");
        p = apu(p, bpm);
        p = ap(p, "BPM");
        cv_text(188, 1, &FONT_S, s, playing ? UI_HI : UI_LABEL);
    }
    /* line 2: pattern name + genre, centred and clamped to the width */
    i = pat_sel < N_CATALOG ? pat_sel : N_CATALOG - 1u;
    nm = CATALOG[i].name; g = CATALOG[i].genre;
    while (*nm && k < 12) s[k++] = *nm++;
    s[k++] = ' ';
    if (*g) { while (*g && k < 24) s[k++] = *g++; }
    s[k] = 0;
    fit_text(s, 228);
    tx = ((int32_t)240 - text_w(&FONT_S, s)) >> 1;
    if (tx < 0) tx = 0;
    cv_text((uint32_t)tx, 18, &FONT_S, s, UI_AMB);
    /* separator rule */
    cv_rect(0, 33, 240, 1, UI_PANEL);
    cv_blit(0, 0);
}

static void draw_parts(void)
{
    uint32_t p, s, x, y, w, h, cw = 11, labw = 52, top = 34, rh = 20;
    cv_begin(240, 8u * rh, UI_BG);
    for (p = 0; p < NPART; p++) {
        uint32_t y0 = p * rh, sel = (p == part_sel), mut = mute[p];
        /* label cell: mute mark, accent bar, then the part name */
        if (mut) cv_text(1, y0 + 2, &FONT_S, "M", UI_MUTE);
        if (sel && !mut) cv_rect(2, y0 + 3, 3, rh - 6, UI_HI);
        cv_text(mut ? 12 : 8, y0 + 2, &FONT_S, PNAME[p], sel ? UI_HI : UI_LABEL);
        /* step cells: 16 columns of the visible page */
        for (s = 0; s < 16u; s++) {
            uint32_t st = (step_cur / 16u) * 16u + s;
            uint32_t lit = pat.gates[p][st % NSTEP];
            uint32_t cur = (s == step_cur % 16u);
            x = labw + s * cw; y = y0 + 2;
            w = cw - 2; h = rh - 5;
            if (mut) { cv_rect(x, y, w, h, UI_DIM); continue; }
            if (lit)
                cv_rect(x, y, w, h, cur ? UI_HI : UI_AMB);
            else
                cv_rect(x, y, w, h, cur ? UI_HI : UI_PANEL);
        }
    }
    cv_blit(0, top);
}

static void draw_footer(uint32_t playing)
{
    char s[48];
    char *p = s;
    uint32_t q;
    /* status-bar rule */
    cv_begin(240, 2, C_BG);
    cv_rect(0, 1, 240, 1, UI_PANEL);
    cv_blit(0, 208);
    if (write_arm) {
        p = ap(p, "REC "); p = ap(p, PNAME[part_sel]); p = ap(p, " keys:steps");
    } else if (fm1_in.buttons & (1u << BTN_SEQ)) {
        p = ap(p, "SEQ TOOLS  F3=CLEAR");
    } else if (glomode) {
        if (playing) {
            p = ap(p, "GLO "); p = apu(p, bpm); p = ap(p, "BPM sw");
            p = apu(p, swing); p = ap(p, "% ac"); p = apu(p, accent); p = ap(p, " pg"); p = apu(p, page + 1);
        } else {
            p = ap(p, "GLO tempo/swing/accent/page");
        }
    } else {
        p = ap(p, PNAME[part_sel]);
        for (q = 0; q < 4u; q++) {
            *p++ = ' '; *p++ = (char)('A' + q);
            p = apu(p, part_prm[part_sel][q]);
        }
        p = ap(p, " F"); p = apu(p, fx_mode);
        if (midi_out) p = ap(p, " M");
        *p = 0;
    }
    fit_text(s, 232);
    draw_text_box(2, 210, 236, &FONT_S, s,
                  (write_arm || glomode) ? UI_HI : (playing ? UI_AMB : UI_LABEL), 0);
}

static void draw_fx_page(void)
{
    char s[48]; char *p = s;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "FX", UI_HI);
    cv_rect(0, 18, 240, 1, UI_PANEL);
    p = ap(s, "FX mode: "); p = ap(p, (const char *[]){"OFF","CHORUS","ECHO","COMBO"}[fx_mode]);
    cv_text(4, 24, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "MIDI OUT: "); p = ap(p, midi_out ? "ON" : "OFF");
    cv_text(4, 42, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "Clock: "); p = apu(p, bpm); p = ap(p, " BPM · 1/16 · sw"); p = apu(p, swing); p = ap(p, "%");
    fit_text(s, 232);
    cv_text(4, 60, &FONT_S, s, UI_LABEL);
    p = s; p = ap(p, "FX key (btn 3) cycles modes."); fit_text(s, 232);
    cv_text(4, 188, &FONT_S, s, UI_DIM);
    cv_blit(0, 34);
}
static void draw_wave_page(void)
{
    char s[48]; char *p = s;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "WAVES / PATCH", UI_HI);
    cv_rect(0, 18, 240, 1, UI_PANEL);
    if (part_sel < NDRUM) {
        p = ap(s, "Part "); p = apu(p, part_sel); p = ap(p, "  kit "); p = ap(p, drum_kit_name(drum_kit));
        p = ap(p, " ["); p = ap(p, drum_kit_cat(drum_kit)); p = ap(p, "]");
        cv_text(4, 24, &FONT_S, s, UI_AMB);
    } else {
        p = ap(s, PNAME[part_sel]); p = ap(p, " A: wave ");
        p = ap(p, fm_macro_wave_label(part_prm[part_sel][0]));
        p = ap(p, " ("); p = apu(p, part_prm[part_sel][0]); p = ap(p, ")");
        fit_text(s, 232);
        cv_text(4, 24, &FONT_S, s, UI_AMB);
        p = s; p = ap(p, "B ratio "); p = apu(p, part_prm[part_sel][1]);
        p = ap(p, "  C mod "); p = apu(p, part_prm[part_sel][2]);
        p = ap(p, "  D lvl "); p = apu(p, part_prm[part_sel][3]);
        fit_text(s, 232);
        cv_text(4, 42, &FONT_S, s, UI_AMB);
        p = s;
        if (patchbank < N_PATCHBANKS) {
            p = ap(p, PATCHBANK_NAME[patchbank]);
            p = ap(p, " · "); p = ap(p, patch_display_name(patchbank, patch_ix));
        }
        fit_text(s, 232);
        cv_text(4, 60, &FONT_S, s, UI_HI);
    }
    if (patchbank < N_PATCHBANKS) {
        p = s; p = ap(p, "sound "); p = apu(p, patch_ix + 1); p = ap(p, "/"); p = apu(p, PATCHBANK_COUNT[patchbank]);
        p = ap(p, " · "); p = ap(p, PATCHBANK_NAME[patchbank]);
        fit_text(s, 232);
        cv_text(4, 82, &FONT_S, s, UI_LABEL);
        cv_text(4, 188, &FONT_S, "A: wave · PRESETS browses sound · GLO+EDIT: SOUNDS", UI_DIM);
    }
    cv_blit(0, 34);
}
static void draw_sounds_page(void)
{
    char s[48]; char *p = s;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "SOUNDS", UI_HI);
    cv_rect(0, 18, 240, 1, UI_PANEL);
    /* category (bank) */
    p = s; p = ap(p, "Cat "); p = ap(p, PATCHBANK_NAME[patchbank]);
    p = ap(p, "  "); p = apu(p, patchbank + 1); p = ap(p, "/"); p = apu(p, N_PATCHBANKS);
    fit_text(s, 232);
    cv_text(4, 24, &FONT_S, s, UI_AMB);
    /* sound name */
    p = s; p = ap(p, patch_display_name(patchbank, patch_ix));
    fit_text(s, 232);
    cv_text(4, 44, &FONT_S, s, UI_HI);
    /* position in category */
    p = s; p = ap(p, "sound "); p = apu(p, patch_ix + 1);
    p = ap(p, "/"); p = apu(p, PATCHBANK_COUNT[patchbank]);
    fit_text(s, 232);
    cv_text(4, 64, &FONT_S, s, UI_LABEL);
    cv_text(4, 188, &FONT_S, "KNOB1 cat · KNOB2 sound · PRESETS browses · EDIT exits", UI_DIM);
    cv_blit(0, 34);
}
static const char *wave_label_direct(uint8_t w)
{
    static const char *const W[8] = {
        "sine", "half sine", "rect sine", "quarter pulse",
        "triangle", "saw", "square", "sine+3rd"
    };
    return W[w & 7u];
}

/* Sample one of the eight operator/LFO waveforms at a full 32-bit phase.
 * Mirrors fm_wave() in the engine, but only needs the exported fm_sine. */
static int32_t lfo_sample(uint8_t w, uint32_t ph)
{
    int32_t s = fm_sine(ph);
    uint32_t i = ph >> 22;
    switch (w & 7u) {
    case 0: return s;
    case 1: return i < 512u ? s : 0;
    case 2: return s < 0 ? -s : s;
    case 3: return (i < 256u || (i >= 512u && i < 768u)) ? s : 0;
    case 4: {
        int32_t t = (int32_t)(ph >> 16) & 0xFFFF;
        return t < 32768 ? (t * 2 - 32768) : (32767 - (t - 32768) * 2);
    }
    case 5: return (int32_t)(ph >> 16) - 32768;
    case 6: return (ph & 0x80000000u) ? -28000 : 28000;
    default: {
        int32_t s3 = fm_sine(((uint32_t)((i * 3u) & 1023u)) << 22);
        return (s * 2 + s3) / 3;
    }
    }
}

static void draw_wave_plot(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t wave)
{
    int32_t px, lx = -1, ly = 0;
    cv_rect(x - 1, y - 1, w + 2, h + 2, C_LINE);
    cv_rect(x, y + h / 2, w, 1, C_DIM);            /* centre axis */
    for (px = 0; px < w; px++) {
        uint32_t ph = (uint32_t)(((uint64_t)px * 0x100000000ull) / (uint32_t)w);
        int32_t v = lfo_sample(wave, ph);
        int32_t py = y + h / 2 - (int32_t)((int64_t)v * h / 2 / 32767);
        if (lx >= 0) cv_line(lx, ly, x + px, py, C_HI);
        lx = x + px; ly = py;
    }
}

static void draw_env_plot(int32_t x, int32_t y, int32_t w, int32_t h, const fm_patch_t *pp)
{
    int32_t base = y + h - 2, top = y + 2;
    int32_t susY = base - (h - 4) * (int32_t)pp->vca_sus / 127;
    int32_t aw = 4 + w * (int32_t)(31 - pp->vca_ar) / 96;
    int32_t dw = 6 + w * (int32_t)(31 - pp->vca_dr) / 80;
    int32_t sw = w / 4;
    int32_t rw = w - aw - dw - sw;
    int32_t x1, x2, x3;
    if (rw < 0) rw = 0;
    x1 = x + aw; x2 = x1 + dw; x3 = x2 + sw;
    cv_rect(x - 1, y - 1, w + 2, h + 2, C_LINE);
    cv_rect(x, base, w, 1, C_DIM);                 /* baseline */
    cv_line(x, base, x1, top, C_HI);               /* attack */
    cv_line(x1, top, x2, susY, C_HI);              /* decay */
    cv_line(x2, susY, x3, susY, C_HI);             /* sustain */
    cv_line(x3, susY, x3 + rw, base, C_HI);        /* release */
}

/* ABUSE skin mascot: a 24x26 silhouette tinted in the active highlight colour. */
static void abuse_mascot_draw(int32_t x, int32_t y, uint16_t c)
{
    int32_t r, col;
    for (r = 0; r < ABUSE_MASCOT_H; r++) {
        const uint8_t *row = abuse_mascot[r];
        for (col = 0; col < ABUSE_MASCOT_W; col++) {
            if (row[col >> 3] & (0x80u >> (col & 7)))
                cv_rect(x + col, y + r, 1, 1, c);
        }
    }
}

/* a decorative double rule under a page title, ABUSE-style corner ticks */
static void page_rule(int32_t y)
{
    cv_rect(4, y, 232, 1, C_GRAY);
    cv_rect(4, y + 1, 232, 1, C_LINE);
    cv_rect(3, y - 1, 1, 3, C_HI);
    cv_rect(236, y - 1, 1, 3, C_HI);
}
static void draw_lfo_page(void)
{
    char s[48]; char *p = s;
    fm_patch_t pp;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "LFO / MODULATION", UI_HI);
    page_rule(18);
    abuse_mascot_draw(214, 20, C_HI);
    if (part_sel < NDRUM) {
        cv_text(4, 24, &FONT_S, "drum part - no LFO", UI_AMB);
        cv_text(4, 188, &FONT_S, "select a synth part (5-7)", UI_DIM);
    } else {
        uint8_t vz = (uint8_t)(part_sel - NDRUM);
        fm_get_patch(vz, &pp);
        p = s; p = ap(p, PNAME[part_sel]); p = ap(p, "  LFO ");
        if (pp.lfo_on) { p = ap(p, "on  "); p = ap(p, wave_label_direct(pp.lfo_wave)); }
        else p = ap(p, "off");
        fit_text(s, 232);
        cv_text(4, 24, &FONT_S, s, UI_AMB);
        p = s; p = ap(p, "speed "); p = apu(p, pp.lfo_speed);
        p = ap(p, "  vibrato "); p = apu(p, pp.pmd);
        p = ap(p, "  tremolo "); p = apu(p, pp.amd);
        fit_text(s, 232);
        cv_text(4, 42, &FONT_S, s, UI_AMB);
        draw_wave_plot(8, 64, 224, 72, pp.lfo_on ? pp.lfo_wave : 0u);
        cv_text(4, 188, &FONT_S, "A: wave/off  B: speed  C: vibrato  D: tremolo", UI_DIM);
    }
    cv_blit(0, 34);
}
static void draw_env_page(void)
{
    char s[48]; char *p = s;
    fm_patch_t pp;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "ENVELOPE / VCA", UI_HI);
    page_rule(18);
    abuse_mascot_draw(214, 20, C_HI);
    if (part_sel < NDRUM) {
        cv_text(4, 24, &FONT_S, "drum part - no envelope", UI_AMB);
        cv_text(4, 188, &FONT_S, "select a synth part (5-7)", UI_DIM);
    } else {
        uint8_t vz = (uint8_t)(part_sel - NDRUM);
        fm_get_patch(vz, &pp);
        p = s; p = ap(p, PNAME[part_sel]);
        p = ap(p, "  A "); p = apu(p, pp.vca_ar);
        p = ap(p, " D "); p = apu(p, pp.vca_dr);
        p = ap(p, " S "); p = apu(p, pp.vca_sus);
        p = ap(p, " R "); p = apu(p, pp.vca_rr);
        fit_text(s, 232);
        cv_text(4, 24, &FONT_S, s, UI_AMB);
        draw_env_plot(8, 54, 224, 80, &pp);
        cv_text(4, 188, &FONT_S, "A: attack  B: decay  C: sustain  D: release", UI_DIM);
    }
    cv_blit(0, 34);
}
static void draw_sys_page(void)
{
    char s[48]; char *p = s;
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "HOME", UI_HI);
    page_rule(18);
    p = s; p = ap(p, "Colour: "); p = ap(p, fm_palette_name(pal_idx));
    fit_text(s, 232);
    cv_text(4, 24, &FONT_S, s, UI_HI);
    p = s; p = ap(p, "Drum kit: "); p = ap(p, drum_kit_name(drum_kit)); p = ap(p, " (OCT- / OCT+)"); fit_text(s, 232);
    cv_text(4, 42, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "Patch banks: "); p = apu(p, N_PATCHBANKS);
    p = ap(p, "  Waves: "); p = apu(p, TOTAL_WT_COUNT); fit_text(s, 232);
    cv_text(4, 60, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "MIDI I/O: "); p = ap(p, (midi_out ? "ON" : "OFF"));
    p = ap(p, "  FX: "); p = ap(p, (const char *[]){"OFF","CHORUS","ECHO","COMBO"}[fx_mode]); fit_text(s, 232);
    cv_text(4, 78, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "Slot: "); p = apu(p, slot_sel + 1); p = ap(p, "/"); p = apu(p, NSLOT);
    p = ap(p, "  SAVE=page  GLO+REC=load"); fit_text(s, 232);
    cv_text(4, 96, &FONT_S, s, UI_HI);
    cv_text(4, 188, &FONT_S, "A: colour  pages: SEQ FX EDIT LFO ENV  OCT+ MIDI", UI_DIM);
    cv_blit(0, 34);
}
static void draw_save_page(void)
{
    char s[48]; char *p = s;
    static const char *const ACT[3] = { "LOAD", "SAVE", "CLEAR" };
    cv_begin(240, 206, UI_BG);
    cv_text(4, 2, &FONT_S, "SAVE / SOUNDS", UI_HI);
    page_rule(18);
    p = s; p = ap(p, "Slot: "); p = apu(p, slot_sel + 1); p = ap(p, "/"); p = apu(p, NSLOT);
    p = ap(p, slot_in_use(slot_sel) ? "   [USED]" : "   [empty]");
    fit_text(s, 232);
    cv_text(4, 24, &FONT_S, s, UI_HI);
    p = s; p = ap(p, "Source: "); p = ap(p, PATCHBANK_NAME[patchbank]);
    p = ap(p, " · "); p = ap(p, patch_display_name(patchbank, patch_ix)); fit_text(s, 232);
    cv_text(4, 42, &FONT_S, s, UI_AMB);
    p = s; p = ap(p, "Action: "); p = ap(p, ACT[action_sel]);
    fit_text(s, 232);
    cv_text(4, 60, &FONT_S, s, UI_HI);
    cv_text(4, 188, &FONT_S, "K1 slot · K4 action · OCT+ go · OCT- back", UI_DIM);
    cv_blit(0, 34);
}
static void draw_screen(uint32_t playing)
{
    draw_header(playing);
    if (view_page == 1) {
        draw_fx_page();
    } else if (view_page == 2) {
        draw_wave_page();
    } else if (view_page == 3) {
        draw_lfo_page();
    } else if (view_page == 4) {
        draw_env_page();
    } else if (view_page == 5) {
        draw_sys_page();
    } else if (view_page == 6) {
        draw_save_page();
    } else if (view_page == 7) {
        draw_sounds_page();
    } else {
        draw_parts();
        draw_footer(playing);
    }
}

/* ================= main ================= */
static uint32_t last_repaint;
static uint32_t saved_ms;

static void seq_stop(void)
{
    uint32_t p;
    if (midi_out) midi_send(0xFC, 0, 0);
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
        /* step length, with a swing on the offbeats
         * (MPC-style: 1/160 of the step per swing unit => up to ~62% at max). */
        {
            uint32_t base = step_ms, extra = (step_cur & 1u) ? (step_ms * swing / 160u) : 0u;
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
    pat_sel = S.pat_sel; accent = S.accent; midi_out = S.midi_out;
    dx7bank_sel = S.dx7_bank;
    patchbank = S.patchbank; patch_ix = S.patch_ix;
    if (patchbank >= N_PATCHBANKS) patchbank = 0;
    if (patch_ix >= PATCHBANK_COUNT[patchbank]) patch_ix = 0;
    memcpy(mute, S.mute, sizeof mute);
    memcpy(part_prm, S.prm, sizeof part_prm);
    memcpy(&pat, &S.pat, sizeof pat);
    fm_init();
    load_bank_patch(patchbank, patch_ix);
    apply_saved_sound();
    snd_mem_reset();
    fm_sync_macros();
    if (pat.gates[0][0] == 0 && pat.gates[0][1] == 0) pattern_load(pat_sel % N_DEMO_PAT);
    step_ms = 15000u / (bpm ? bpm : 128u);
    drum_kit = 1;   /* default to the TR-808 kit (full-bodied); APP is thinner */
    view_page = 0;
    slot_sel = 0;
    action_sel = 1;   /* SAVE default action */
    prev_keys = 0;
    for (k = 0; k < 27u; k++) held[k] = 0;

    lcd_fill(0, 0, 240, 240, C_BG);
    palette_set(0);                  /* default palette: "default" (Abuse look) */
    draw_screen(0);
    last_repaint = 0; saved_ms = 0; dirty = 0;
}

static void app_tick(void)
{
    uint32_t keys, down, up, b, rel, k;
    uint8_t midi_st, midi_d1, midi_d2; int src;
    while ((src = midi_get(&midi_st,&midi_d1,&midi_d2)) != 0) {
        if (midi_st >= 0x90u && midi_st <= 0x9Fu) {
            if (midi_d1 >= 36u && midi_d1 <= 62u) {
                uint8_t kk = (uint8_t)(midi_d1 - 36u);
                if (midi_d2 > 0) {
                    if (part_sel < NDRUM) eq_put(EV_DRUM, part_sel, 0, (uint8_t)(midi_d2 > 127u ? 127u : midi_d2));
                    else { held[kk] = midi_d1; eq_put(EV_NOTEON, part_sel, midi_d1, (uint8_t)(midi_d2 > 127u ? 127u : midi_d2)); }
                } else {
                    if (part_sel >= NDRUM && held[kk]) { eq_put(EV_NOTEOFF, part_sel, held[kk], 0); held[kk]=0; }
                }
            }
        } else if (midi_st >= 0xB0u && midi_st <= 0xBFu) {
            uint8_t cc = midi_d1, v = midi_d2;
            switch (cc) {
                case 1: if (part_sel >= NDRUM) { part_prm[part_sel][0] = (uint8_t)(v >> 2); dirty = 1; } break;
                case 2: if (part_sel >= NDRUM) { part_prm[part_sel][1] = v; dirty = 1; } break;
                case 5: if (part_sel >= NDRUM) { part_prm[part_sel][2] = (uint8_t)(v >> 1); dirty = 1; } break;
                case 7: if (part_sel >= NDRUM) { part_prm[part_sel][3] = v; dirty = 1; } break;
                case 91: fx_mode = (uint8_t)((v * 4) / 128); if (fx_mode > 3) fx_mode = 3; dirty = 1; break;
                case 92: drum_kit = (uint8_t)((v * TOTAL_DRUMS) / 128); if (drum_kit >= TOTAL_DRUMS) drum_kit = TOTAL_DRUMS-1; dirty = 1; break;
                case 93: {
                    uint8_t ps = (uint8_t)((v * PATCHBANK_COUNT[patchbank]) / 128); if (ps >= PATCHBANK_COUNT[patchbank]) ps = PATCHBANK_COUNT[patchbank] - 1;
                    snd_apply(patchbank, ps);
                    dirty = 1; break;
                }
                default: break;
            }
        } else if (midi_st == 0xFAu) { run = 1; step_cur = 0; next_step_ms = fm1_ms + step_ms; dirty = 1; }
        else if (midi_st == 0xFB || midi_st == 0xFC) { run = 0; step_cur = 0; dirty = 1; }
    }

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
            else { run = 1; step_cur = 0; next_step_ms = fm1_ms + step_ms; if (midi_out) midi_send(0xFA, 0, 0); }
            dirty = 1;
        }
        if (b & (1u << BTN_FX)) {
            fx_mode = (uint8_t)((fx_mode + 1u) % 4u);
            view_page = 1;
            dirty = 1;
        }
        if (b & (1u << BTN_SEQ)) {
            view_page = 0;
            dirty = 1;
        }
        if (b & (1u << BTN_EDIT)) {
            view_page = glomode ? 7 : 2;   /* GLO+EDIT = SOUNDS browse page */
            dirty = 1;
        }
        if (b & (1u << BTN_LFO)) {
            view_page = 3;
            dirty = 1;
        }
        if (b & (1u << BTN_ENV)) {
            view_page = 4;
            dirty = 1;
        }
        if (b & (1u << BTN_OCT_UP)) {
            if (view_page == 6) {
                /* execute the chosen action (grimm-style confirm) */
                if (action_sel == 0) user_load_slot(slot_sel);
                else if (action_sel == 1) user_save_slot(slot_sel);
                else user_clear_slot(slot_sel);
            } else {
                midi_out = !midi_out;
            }
            dirty = 1;
        }
        if (b & (1u << BTN_OCT_DOWN) && glomode) {
            snd_apply((uint8_t)((patchbank + 1u) % N_PATCHBANKS), 0);  /* next category, first sound */
        }
        if (b & (1u << BTN_OCT_DOWN) && !glomode && view_page == 6) {
            view_page = 5;          /* back out to HOME */
            dirty = 1;
        }
        if (b & (1u << BTN_GLO)) { glomode = !glomode; dirty = 1; }
        if (b & (1u << BTN_REC) && glomode) {
            user_load_slot(slot_sel);
            dirty = 1;
        }
        if (b & (1u << BTN_REC) && !glomode) { write_arm = !write_arm; dirty = 1; }
        if (b & (1u << BTN_SAVE)) {
            view_page = 6;          /* SAVE page (grimm-style slots) */
            dirty = 1;
        }
        if (b & (1u << BTN_HOME)) {
            view_page = 5;          /* HOME: system + colour selector */
            dirty = 1;
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
                if (view_page == 0) {
                    /* SEQ page: PRESETS cycles the demo pattern (existing behaviour) */
                    int32_t c = (int32_t)pat_sel + e;
                    if (c < 0) c = N_CATALOG - 1;
                    if (c >= (int32_t)N_CATALOG) c = 0;
                    pat_sel = (uint8_t)c;
                    if (pat_sel < (int32_t)N_DEMO_PAT) { pattern_load(pat_sel); memcpy(&S.pat, &pat, sizeof pat); }
                } else {
                    /* Sound browsing across the whole list, from this part's cursor */
                    int32_t cur = (int32_t)snd_mem[part_sel] + (int32_t)e;
                    int32_t tot = (int32_t)snd_total();
                    while (cur < 0) cur += tot;
                    while (cur >= tot) cur -= tot;
                    uint8_t b, x;
                    snd_unpack((uint32_t)cur, &b, &x);
                    snd_apply(b, x);
                }
                dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
                int32_t c = (int32_t)page + e;
                page = (uint8_t)(c < 0 ? 0 : c > 3 ? 3 : c);
                dirty = 1;
            }
            e = fm1_enc_take(ENC_KNOB1);
            if (e && view_page == 7) {   /* SOUNDS: KNOB1 picks the bank/category */
                int32_t b = (int32_t)patchbank + (e > 0 ? 1 : -1);
                uint8_t nb = (uint8_t)((b % N_PATCHBANKS + N_PATCHBANKS) % N_PATCHBANKS);
                snd_apply(nb, patch_ix);
            }
            else if (glomode && e) { int32_t c = bpm + e; bpm = (uint8_t)(c < 60 ? 60 : c > 250 ? 250 : c); step_ms = 15000u / bpm; dirty = 1; }
            else if (e && view_page == 5) {
                int32_t c = (int32_t)pal_idx + (e > 0 ? 1 : -1);
                int32_t n = (int32_t)fm_palette_count();
                pal_idx = (uint8_t)((c % n + n) % n);
                fm_palette_set(pal_idx);
                dirty = 1;
            }
            else if (e && view_page == 6) {
                slot_sel = (uint8_t)((slot_sel + e + NSLOT) % NSLOT);
                dirty = 1;
            }
            else if (e && view_page == 3 && part_sel >= NDRUM) edit_lfo(0, e);
            else if (e && view_page == 4 && part_sel >= NDRUM) edit_env(0, e);
            else if (e) { int32_t c = part_prm[part_sel][0] + e; part_prm[part_sel][0] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB2);
            if (e && view_page == 7) {   /* SOUNDS: KNOB2 picks the sound in the category */
                int32_t c = (int32_t)patch_ix + (e > 0 ? 1 : -1);
                int32_t n = (int32_t)PATCHBANK_COUNT[patchbank];
                if (c < 0) c = n - 1;
                if (c >= n) c = 0;
                snd_apply(patchbank, (uint8_t)c);
            }
            else if (glomode && e) { int32_t c = swing + e; swing = (uint8_t)(c < 0 ? 0 : c > 100 ? 100 : c); dirty = 1; }
            else if (e && part_sel < NDRUM) { int32_t c = drum_kit + (e > 0 ? 1 : -1);
                drum_kit = (uint8_t)(c < 0 ? 0 : c >= (int32_t)TOTAL_DRUMS ? TOTAL_DRUMS - 1 : c); dirty = 1; }
            else if (e && view_page == 3 && part_sel >= NDRUM) edit_lfo(1, e);
            else if (e && view_page == 4 && part_sel >= NDRUM) edit_env(1, e);
            else if (e) { int32_t c = part_prm[part_sel][1] + e; part_prm[part_sel][1] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB3);
            if (glomode && e) { int32_t c = accent + e; accent = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            else if (e && view_page == 3 && part_sel >= NDRUM) edit_lfo(2, e);
            else if (e && view_page == 4 && part_sel >= NDRUM) edit_env(2, e);
            else if (e) { int32_t c = part_prm[part_sel][2] + e; part_prm[part_sel][2] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
            e = fm1_enc_take(ENC_KNOB4);
            if (glomode && e) { int32_t c = (int32_t)page + (e > 0 ? 1 : -1); page = (uint8_t)(c < 0 ? 0 : c > 3 ? 3 : c); dirty = 1; }
            else if (e && view_page == 6) {
                action_sel = (uint8_t)((action_sel + (e > 0 ? 1u : 2u)) % 3u);
                dirty = 1;
            }
            else if (e && view_page == 3 && part_sel >= NDRUM) edit_lfo(3, e);
            else if (e && view_page == 4 && part_sel >= NDRUM) edit_env(3, e);
            else if (e) { int32_t c = part_prm[part_sel][3] + e; part_prm[part_sel][3] = (uint8_t)(c < 0 ? 0 : c > 127 ? 127 : c); dirty = 1; }
        }
        fm_sync_macros();

        /* --- keys: write steps, or play live --- */
        for (k = 0; k < 27u; k++) {
            if (write_arm) {
                if (down >> k & 1u) {
                    uint32_t st = (step_cur / 16u) * 16u + k;
                    if (st < 64u) pat.gates[part_sel][st] ^= 1u;
                    dirty = 1;
                }
            } else if (fm1_in.buttons & (1u << BTN_SEQ)) {
                /* SEQ tools: hold SEQ + a white key (grimm-style).
                 * F3 = CLEAR the whole pattern. */
                if ((down >> k & 1u) && k == 0) { pattern_clear(); dirty = 1; }
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
            S.pat_sel = pat_sel; S.accent = accent; S.midi_out = midi_out; S.dx7_bank = dx7bank_sel; S.patchbank = patchbank; S.patch_ix = patch_ix;
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

#ifdef DINGHY_WEB
/* Persist the 8 user slots to the page across reloads. We expose the slot
 * payloads (valid flag + struct-set bytes each) to the worklet, which hands
 * them to main.js for localStorage.
 */
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE
uint32_t web_snap_bytes(void) { return (uint32_t)sizeof(struct set); }
EMSCRIPTEN_KEEPALIVE
uint32_t web_snap_count(void) { return NSLOT; }
EMSCRIPTEN_KEEPALIVE
void web_snap_export(uint8_t *dst)
{
    uint32_t i, n = (uint32_t)sizeof(struct set);
    for (i = 0; i < NSLOT; i++) {
        if (host_slot[i][0]) {
            dst[0] = 1;
            memcpy(dst + 4, host_slot[i] + 4, n);
        } else {
            dst[0] = 0;
        }
        dst += n + 4;
    }
}
EMSCRIPTEN_KEEPALIVE
void web_snap_import(const uint8_t *src)
{
    uint32_t i, n = (uint32_t)sizeof(struct set);
    for (i = 0; i < NSLOT; i++) {
        if (src[0]) {
            host_slot[i][0] = 1;
            host_slot[i][1] = (uint8_t)n;
            host_slot[i][2] = (uint8_t)(n >> 8);
            host_slot[i][3] = (uint8_t)(n >> 16);
            memcpy(host_slot[i] + 4, src + 4, n);
        } else {
            host_slot[i][0] = 0;
        }
        src += n + 4;
    }
}
#endif /* DINGHY_WEB */
