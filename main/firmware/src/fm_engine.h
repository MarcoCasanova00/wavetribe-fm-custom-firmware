#ifndef FM_ENGINE_H
#define FM_ENGINE_H

#include <stdint.h>

/* 4-Operator FM Engine (TX81Z-style) */
/* 8 voices max, 4 operators per voice, 8 algorithms */

#define FM_MAX_VOICES   8
#define FM_OPS_PER_VOICE 4
#define FM_ALGORITHMS    8

/* Algorithm definitions: which ops modulate which */
/* 0 = carrier, 1-3 = modulators (output to carrier or next modulator) */
static const uint8_t fm_algorithm[FM_ALGORITHMS][FM_OPS_PER_VOICE] = {
    /* Alg 0: 4-op stack: 3->2->1->0 */
    { 0, 0, 1, 2 },
    /* Alg 1: 3->2->1, 3->0 */
    { 0, 1, 2, 2 },
    /* Alg 2: 3->2->1, 3->0, 1->0 */
    { 0, 1, 2, 0 },
    /* Alg 3: 3->2->1->0, 3->0 */
    { 0, 0, 1, 2 },
    /* Alg 4: 3->2, 1->0, 3->0 */
    { 0, 1, 0, 2 },
    /* Alg 5: 3->2->1, 3->0 (parallel) */
    { 0, 1, 2, 0 },
    /* Alg 6: all parallel (3,2,1->0) */
    { 0, 0, 0, 0 },
    /* Alg 7: all carriers (no modulation) */
    { 0, 0, 0, 0 },
};

/* Modulation routing: which ops receive modulation from which */
static const int8_t fm_mod_routing[FM_ALGORITHMS][FM_OPS_PER_VOICE][FM_OPS_PER_VOICE] = {
    /* Alg 0: 3->2->1->0 */
    { {0,0,0,0}, {1,0,0,0}, {0,1,0,0}, {0,0,1,0} },
    /* Alg 1: 3->2->1, 3->0 */
    { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,0} },
    /* Alg 2: 3->2->1, 3->0, 1->0 */
    { {1,1,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,0} },
    /* Alg 3: 3->2->1->0, 3->0 */
    { {1,0,0,0}, {1,0,0,0}, {0,1,0,0}, {0,0,1,0} },
    /* Alg 4: 3->2, 1->0, 3->0 */
    { {1,0,0,0}, {0,0,0,0}, {0,1,0,0}, {0,0,0,0} },
    /* Alg 5: 3->2->1, 3->0 (parallel) */
    { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,0} },
    /* Alg 6: all parallel to carrier 0 */
    { {1,1,1,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} },
    /* Alg 7: all carriers */
    { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} },
};

#define FM_OP_COUNT (FM_MAX_VOICES * FM_OPS_PER_VOICE)

typedef struct {
    /* Phase accumulator */
    uint32_t phase;
    uint32_t phase_inc;
    
    /* Envelope */
    uint8_t env_state;      /* 0=off, 1=attack, 2=decay, 3=sustain, 4=release */
    int32_t env_level;      /* Q15 */
    uint32_t env_counter;
    uint32_t env_inc_attack;
    uint32_t env_inc_decay;
    uint32_t env_inc_release;
    uint16_t sustain_level;
    
    /* Frequency */
    uint32_t freq_base;     /* base frequency (note + detune) */
    int32_t detune;         /* detune in cents */
    
    /* Modulation */
    int32_t mod_index;      /* modulation index (0-127) */
    uint8_t feedback;       /* self-feedback (0-7) */
    
    /* Output */
    int32_t output;         /* current output sample */
    int32_t last_output;    /* for feedback */
    
    /* State */
    uint8_t active;
    uint8_t key;
    uint8_t velocity;
} fm_op_t;

typedef struct {
    fm_op_t ops[FM_OPS_PER_VOICE];
    uint8_t algorithm;
    uint8_t active;
    uint8_t key;
    uint8_t velocity;
    int32_t pitch_bend;
    int32_t mod_wheel;
    int32_t pitch_bend_range;
} fm_voice_t;

typedef struct {
    /* Envelope rates (0-99) per operator */
    uint8_t ar[4];     /* Attack Rate */
    uint8_t dr[4];     /* Decay Rate */
    uint8_t sr[4];     /* Sustain Rate */
    uint8_t rr[4];     /* Release Rate */
    uint8_t sl[4];     /* Sustain Level (0-99) */
    uint8_t tl[4];     /* Total Level (0-99) */
    
    /* Frequency per operator */
    int16_t coarse[4];     /* coarse tune (0-31) */
    int16_t fine[4];       /* fine tune (0-99) */
    int16_t detune[4];     /* detune (-7..+7) */
    
    /* Modulation per operator */
    uint8_t am[4];         /* AM enable */
    uint8_t vib[4];        /* Vibrato enable */
    uint8_t eg_bias[4];    /* EG Bias */
    uint8_t ks[4];         /* Key Scale */
    uint8_t ars[4];        /* Attack Rate Scaling */
    uint8_t ams[4];        /* AM Sensitivity */
    uint8_t fms[4];        /* FM Sensitivity */
    
    /* Feedback */
    uint8_t fb;         /* Feedback (0-7) */
    
    /* Algorithm */
    uint8_t alg;        /* Algorithm (0-7) */
    uint8_t op_mode;    /* 0=FM, 1=AM */
} fm_patch_t;

/* Global FM state */
extern fm_voice_t fm_voices[FM_MAX_VOICES];
extern fm_patch_t fm_patches[32];  /* 32 patches */

/* Initialize FM engine */
void fm_init(void);

/* Note on/off */
void fm_note_on(uint8_t voice, uint8_t note, uint8_t velocity);
void fm_note_off(uint8_t voice, uint8_t note);

/* Patch management */
void fm_load_patch(uint8_t voice, const fm_patch_t *patch);
void fm_set_algorithm(uint8_t voice, uint8_t alg);

/* Parameter changes */
void fm_set_param(uint8_t voice, uint8_t op, uint8_t param, uint8_t value);

/* Audio processing - call per sample frame */
void fm_process(int32_t *out_l, int32_t *out_r, uint32_t frames);

/* Utility */
int32_t fm_sine(uint32_t phase);
uint32_t fm_note_to_inc(uint8_t note, int32_t bend, int32_t detune);

#endif