/* GENERATED - synth engine patches (note/timbre presets) for BASS/CHRD/LEAD. */
#ifndef FACTORY_PATCHES_H
#define FACTORY_PATCHES_H
#include <stdint.h>
#define N_PATCHES 13u
static const char *const PATCH_NAME[N_PATCHES] = {
    "Init",
    "Bright",
    "Dark",
    "Pluck",
    "Soft",
    "Bell",
    "SawLead",
    "Organ",
    "Strings",
    "NoiseBass",
    "Chimes",
    "PWM",
    "Acid",
};
static const uint8_t PATCH_PRM[N_PATCHES][3][4] = {
  /* Init */
  {
    { 0, 64, 20, 90 },
    { 0, 55, 25, 85 },
    { 0, 70, 30, 80 },
  },
  /* Bright */
  {
    { 2, 30, 40, 95 },
    { 8, 45, 50, 90 },
    { 20, 60, 70, 95 },
  },
  /* Dark */
  {
    { 6, 80, 15, 80 },
    { 2, 70, 20, 80 },
    { 8, 65, 25, 75 },
  },
  /* Pluck */
  {
    { 0, 10, 90, 100 },
    { 1, 35, 60, 90 },
    { 1, 45, 60, 90 },
  },
  /* Soft */
  {
    { 1, 50, 8, 70 },
    { 3, 40, 12, 70 },
    { 4, 50, 15, 65 },
  },
  /* Bell */
  {
    { 12, 90, 40, 85 },
    { 16, 60, 35, 80 },
    { 24, 80, 45, 75 },
  },
  /* SawLead */
  {
    { 0, 45, 30, 95 },
    { 0, 40, 25, 90 },
    { 0, 50, 35, 95 },
  },
  /* Organ */
  {
    { 15, 20, 10, 85 },
    { 15, 20, 10, 85 },
    { 15, 20, 10, 85 },
  },
  /* Strings */
  {
    { 60, 50, 20, 80 },
    { 60, 55, 25, 80 },
    { 60, 60, 30, 80 },
  },
  /* NoiseBass */
  {
    { 100, 120, 90, 90 },
    { 100, 120, 90, 85 },
    { 100, 120, 90, 80 },
  },
  /* Chimes */
  {
    { 30, 90, 70, 80 },
    { 30, 90, 70, 75 },
    { 30, 90, 70, 80 },
  },
  /* PWM */
  {
    { 80, 35, 15, 95 },
    { 80, 35, 15, 90 },
    { 80, 35, 15, 95 },
  },
  /* Acid */
  {
    { 120, 100, 100, 95 },
    { 120, 95, 95, 90 },
    { 120, 100, 100, 95 },
  },
};
#endif /* FACTORY_PATCHES_H */
