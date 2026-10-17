// Freeze Shimmer - définition des paramètres, correspondance MIDI CC, formatage d'affichage
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace fsp
{
struct Info
{
    const char* id;        // identifiant APVTS
    const char* name;      // nom affiché (étiquette de la pédale)
    int   cc;              // numéro de CC MIDI (-1 = aucun)
    float def;             // valeur par défaut (0..1)
};

// 13 potentiomètres : l'ordre définit aussi la table MIDI
static const Info kKnobs[] =
{
    { "dry",    "DRY",     20, 0.80f },
    { "effect", "EFFECT",  21, 0.70f },
    { "blend",  "BLEND",   22, 0.50f },
    { "rvbmix", "RVB MIX", 23, 0.70f },
    { "time",   "TIME",    24, 0.55f },
    { "delay",  "DELAY",   25, 0.00f },
    { "speed",  "SPEED",   26, 0.50f },
    { "layer",  "LAYER",   27, 0.30f },
    { "voice",  "VOICE",   28, 0.50f },
    { "depth",  "DEPTH",   29, 0.35f },
    { "tone",   "TONE",    30, 0.55f },
    { "rate",   "RATE",    31, 0.30f },
    { "gliss",  "GLISS",   32, 0.35f },
};
static constexpr int kNumKnobs = (int) (sizeof (kKnobs) / sizeof (kKnobs[0]));

// CC de commande
static constexpr int kCcScene   = 102;  //  0-42 Intergalactic | 43-85 Off-World | 86-127 Etherdust
static constexpr int kCcFMode   = 103;  //  0-42 Latch | 43-85 Moment | 86-127 Auto
static constexpr int kCcFreeze  = 104;  //  >= 64 : footswitch enfoncé
static constexpr int kCcEngage  = 105;  //  >= 64 : effet actif, < 64 : bypass
static constexpr int kCcRelease = 106;  //  >= 64 : relâche le freeze (Latch / Auto)
static constexpr int kCcSustain = 64;   //  pédale de sustain = footswitch

static const juce::StringArray kSceneNames { "INTERGALACTIC", "OFF-WORLD", "ETHERDUST" };
static const juce::StringArray kModeNames  { "LATCH", "MOMENT", "AUTO" };

inline int findKnob (const juce::String& id)
{
    for (int i = 0; i < kNumKnobs; ++i) if (id == kKnobs[i].id) return i;
    return -1;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const auto& k = kKnobs[i];
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { k.id, version }, k.name,
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.0f), k.def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (
                                                               [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; })));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scene", version }, "Scene", kSceneNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fmode", version }, "Freeze Mode", kModeNames, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "freeze", version }, "Freeze (footswitch)", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "engage", version }, "Engage", true));
    StringArray chans { "OMNI" };
    for (int c = 1; c <= 16; ++c) chans.add (String (c));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "midichan", version }, "MIDI Channel", chans, 0));
    return layout;
}

// valeur lisible dans la ligne d'état
inline juce::String formatValue (int knobIndex, float v)
{
    using juce::String;
    const juce::String id (kKnobs[knobIndex].id);
    if (id == "time")
    {
        if (v >= 0.985f) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"));   // ∞
        const float rt = 0.4f * std::pow (45.0f, v / 0.985f);
        return String (rt, 1) + " s";
    }
    if (id == "delay")  return v <= 0.001f ? String ("OFF") : String (juce::roundToInt (950.0f * v)) + " ms";
    if (id == "rate")   return String (0.15f + (5.0f - 0.15f) * v, 2) + " Hz";
    if (id == "speed")  { const float t = 0.02f * std::pow (150.0f, v); return t < 1.0f ? String (juce::roundToInt (t * 1000.0f)) + " ms" : String (t, 1) + " s"; }
    if (id == "gliss")  { const float t = 0.02f + 5.0f * std::pow (v, 2.2f); return t < 1.0f ? String (juce::roundToInt (t * 1000.0f)) + " ms" : String (t, 1) + " s"; }
    if (id == "layer")  { const int n = 1 + (int) std::floor (juce::jlimit (0.0f, 0.999f, v) * 4.0f); return String (n) + (n > 1 ? " layers" : " layer"); }
    if (id == "voice")  return v < 0.25f ? String ("PLATE") : (v < 0.75f ? String ("SHIMMER ") + String (juce::roundToInt (v * 100.0f)) + "%" : String ("STRINGS"));
    return String (juce::roundToInt (v * 100.0f)) + " %";
}

inline int toMidi (float v) { return juce::jlimit (0, 127, juce::roundToInt (v * 127.0f)); }
} // namespace fsp
