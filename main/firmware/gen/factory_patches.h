/* GENERATED - synth engine patches (note/timbre presets) for BASS/CHRD/LEAD. */
#ifndef FACTORY_PATCHES_H
#define FACTORY_PATCHES_H
#include <stdint.h>
#define N_PATCHES 5u
static const char *const PATCH_NAME[N_PATCHES] = {
    "Init",
    "Bright",
    "Dark",
    "Pluck",
    "Soft",
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
};
#endif /* FACTORY_PATCHES_H */
