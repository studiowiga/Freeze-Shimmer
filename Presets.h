// Freeze Shimmer - réglages d'usine
#pragma once
#include <juce_core/juce_core.h>

namespace fsp
{
struct Preset
{
    const char* name;
    int scene;     // 0 intergalactic, 1 off-world, 2 etherdust
    int fmode;     // 0 latch, 1 moment, 2 auto
    //            dry   effect blend  rvbmix time   delay  speed  layer  voice  depth  tone   rate   gliss
    float v[13];
};

static const Preset kPresets[] =
{
    { "CLEAN PLATE",       0, 0, { 0.90f, 0.50f, 0.35f, 1.00f, 0.30f, 0.00f, 0.50f, 0.20f, 0.00f, 0.15f, 0.65f, 0.20f, 0.30f } },
    { "GLACIAL PAD",       0, 0, { 0.75f, 0.60f, 0.65f, 0.90f, 0.85f, 0.00f, 0.40f, 0.30f, 0.00f, 0.50f, 0.50f, 0.20f, 0.35f } },
    { "INFINITE COSMOS",   0, 0, { 0.55f, 0.70f, 0.80f, 1.00f, 0.7214f, 0.00f, 0.60f, 0.50f, 0.20f, 0.55f, 0.45f, 0.30f, 0.50f } },
    { "TAPE ECHO SHIMMER", 0, 0, { 0.85f, 0.50f, 0.55f, 0.50f, 0.50f, 0.805f, 0.50f, 0.20f, 0.20f, 0.30f, 0.55f, 0.35f, 0.30f } },
    { "ETHERDUST GLITCH",  2, 0, { 0.80f, 0.50f, 0.65f, 0.25f, 0.45f, 0.55f, 0.50f, 0.20f, 0.20f, 0.40f, 0.70f, 0.80f, 0.30f } },
    { "FROZEN DRONE",      0, 0, { 0.45f, 1.00f, 0.30f, 0.80f, 0.60f, 0.00f, 0.70f, 1.00f, 0.20f, 0.40f, 0.50f, 0.20f, 0.65f } },
    { "AUTO FREEZE SWELL", 0, 2, { 0.80f, 0.90f, 0.45f, 0.85f, 0.70f, 0.00f, 0.60f, 0.20f, 0.25f, 0.35f, 0.55f, 0.25f, 0.80f } },
    { "DARK CATHEDRAL",    1, 0, { 0.70f, 0.50f, 0.60f, 1.00f, 0.92f, 0.00f, 0.50f, 0.30f, 0.15f, 0.45f, 0.12f, 0.10f, 0.40f } },
    { "TRUE DELAY",        0, 0, { 0.90f, 0.50f, 0.50f, 0.00f, 0.30f, 0.45f, 0.50f, 0.20f, 0.00f, 0.05f, 0.60f, 0.20f, 0.30f } },
    { "FROZEN PLATE",      0, 0, { 0.80f, 0.90f, 0.50f, 1.00f, 0.66f, 0.00f, 0.50f, 0.30f, 0.00f, 0.30f, 0.45f, 0.20f, 0.35f } },
};
static constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}
