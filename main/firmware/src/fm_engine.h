/* SPDX-License-Identifier: GPL-3.0-only */
/* fm_engine.h - 4-operator FM voice engine with Polivoks-style extras.
 *
 * The engine is integer only (no FPU, no 64-bit division) and renders into
 * caller supplied left/right accumulators.  It is deliberately independent of
 * the display layer so the host test can drive it without a board.
 *
 * Each voice owns one patch: four operators, one of eight algorithms, a
 * dedicated filter envelope and an independent VCA envelope.  The four macros
 * (A wave, B ratio, C modulation, D level) are applied live by the UI.
 */
#ifndef FM_ENGINE_H
#define FM_ENGINE_H

#include <stdint.h>

#define FM_MAX_VOICES 8
#define FM_OPS_PER_VOICE 4
#define FM_ALGORITHMS 8

/* One operator as decoded from the ROM dump. */
typedef struct {
    uint8_t ar;      /* attack rate            0..31  */
    uint8_t d1r;     /* decay 1 rate           0..31  */
    uint8_t d2r;     /* decay 2 rate           0..31  */
    uint8_t rr;      /* release rate           0..15  */
    uint8_t d1l;     /* decay 1 level          0..15  */
    uint8_t ls;      /* level scaling          0..99  */
    uint8_t rs;      /* rate scaling           0..3   */
    uint8_t ebs;     /* EG bias sensitivity    0..7   */
    uint8_t ame;     /* AM enable              0/1    */
    uint8_t kvs;     /* key velocity sens      0..7   */
    uint8_t out;     /* output level           0..99  */
    uint8_t coarse;  /* coarse frequency       0..63  */
    uint8_t fine;    /* fine frequency         0..63  */
    uint8_t det;     /* detune                 0..6   */
} fm_op_patch_t;

/* A complete voice. */
typedef struct {
    fm_op_patch_t op[FM_OPS_PER_VOICE];
    uint8_t alg;          /* algorithm 0..7                            */
    uint8_t fb;           /* feedback  0..7                            */
    uint8_t lfo_speed;    /* 0..127                                    */
    uint8_t lfo_delay;    /* 0..127                                    */
    uint8_t pmd;          /* pitch modulation depth 0..127             */
    uint8_t amd;          /* amplitude modulation depth 0..127         */
    uint8_t lfo_wave;     /* 0..3                                      */
    uint8_t ams;          /* AM sensitivity 0..3                       */
    uint8_t pms;          /* pitch modulation sensitivity 0..7         */
    uint8_t transpose;    /* 24 = no transposition                     */
    uint8_t pb_range;     /* pitch bend range (stored, not wired)      */
    uint8_t sustain;      /* hold decay 1 level while key is down      */
    uint8_t loop;         /* looping AD envelopes                      */
    uint8_t gate;         /* 1 = gate envelopes, 0 = free running      */
    uint8_t cross;        /* operator cross modulation 0..127          */
    uint8_t drive;        /* VCA / output drive 0..127                 */
    uint8_t flt_cut;      /* filter cutoff 0..127                      */
    uint8_t flt_res;      /* filter resonance 0..127                   */
    uint8_t flt_env;      /* filter envelope amount 0..127             */
    uint8_t flt_rate;     /* filter envelope rate 0..31                */
    uint8_t vca_ar;       /* VCA attack rate 0..31                     */
    uint8_t vca_dr;       /* VCA decay rate 0..31                      */
    uint8_t vca_sus;      /* VCA sustain level 0..127                   */
    uint8_t vca_rr;       /* VCA release rate 0..31                     */
    uint8_t lfo_on;       /* LFO enable 0/1                            */
} fm_patch_t;

void fm_init(void);
void fm_reset(void);

void fm_note_on(uint8_t voice, uint8_t note, uint8_t velocity);
void fm_note_off(uint8_t voice, uint8_t note);
void fm_all_off(void);

void fm_load_patch(uint8_t voice, const fm_patch_t *patch);
void fm_set_macro(uint8_t voice, uint8_t index, uint8_t value);

/* runtime voice mode (not part of a patch):
 * 0 normal, 1 hard sync, 2 ring/AM, 3 phase distortion (amount = macro C). */
void fm_set_oscmode(uint8_t voice, uint8_t mode);

/* Live modulation edits: update the running voice's patch in place. */
void fm_set_lfo(uint8_t voice, uint8_t on, uint8_t wave, uint8_t speed,
                uint8_t pmd, uint8_t amd);
void fm_set_vca(uint8_t voice, uint8_t ar, uint8_t dr, uint8_t sus, uint8_t rr);
void fm_get_patch(uint8_t voice, fm_patch_t *out);

/* Add the engine output to the supplied accumulators (frames, not samples). */
void fm_process(int32_t *out_l, int32_t *out_r, uint32_t frames);

/* Utility used by the arpeggiator / UI preview. */
int32_t fm_sine(uint32_t phase);

/* Name of the operator waveform selected by macro A (mirrors fm_engine). */
const char *fm_macro_wave_label(uint8_t macro);

#endif /* FM_ENGINE_H */
