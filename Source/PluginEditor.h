#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Géométrie de l'illustration (pixels de l'image de fond 1250 x 2000)
namespace art
{
constexpr float W = 780.0f, H = 990.0f;   // image 2x (fenêtre par défaut 390 x 495 px)
struct Circle { float x, y, r; };
constexpr Circle knobs[7] = { { 233, 125, 64 }, { 546, 130, 64 }, { 208, 290, 64 }, { 387, 286, 64 },
                              { 578, 292, 64 }, { 212, 462, 64 }, { 568, 464, 64 } };   // centre du capuchon ; r = demi-taille du composant
constexpr Circle modeBtn    = { 388, 84, 18 };
constexpr Circle footswitch = { 388, 866, 56 };
constexpr float statusX = 155, statusY = 735, statusW = 470, statusH = 27;

// tailles de fenêtre : 100 % = 990 px de haut ; ouverture par défaut = 50 % (390 x 495 px)
constexpr int heightPct50 = 495, heightPct60 = 594, heightPct75 = 743, heightPct100 = 990, heightMax = 1485;
}

//==============================================================================
// Potentiomètre vectoriel (dessiné par code, aucun bitmap)
class PedalLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class PedalKnob : public juce::Slider
{
public:
    PedalKnob()
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        setMouseDragSensitivity (240);
        setScrollWheelEnabled (true);
        setPopupMenuEnabled (false);
        setWantsKeyboardFocus (false);
    }
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

//==============================================================================
class FreezeShimmerEditor;

class ModeButton : public juce::Component, private juce::Timer
{
public:
    explicit ModeButton (FreezeShimmerEditor& e) : owner (e) {}
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setBlink (bool on) { blinkPhase = on; repaint(); }
    static constexpr float kPad = 3.0f;                       // le composant est kPad fois plus grand que la LED (halo)
    float coreRadius() const { return (float) getWidth() / (2.0f * kPad); }
private:
    void timerCallback() override;
    FreezeShimmerEditor& owner;
    bool longFired = false, blinkPhase = false;
};

class FootSwitch : public juce::Component
{
public:
    explicit FootSwitch (FreezeShimmerEditor& e) : owner (e) {}
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    static constexpr float kPad = 2.4f;                       // zone agrandie pour le halo
    float coreRadius() const { return (float) getWidth() / (2.0f * kPad); }
private:
    FreezeShimmerEditor& owner;
    bool pressed = false, ignoreUp = false;
    juce::uint32 lastDownMs = 0;
};

class StatusLine : public juce::Component
{
public:
    explicit StatusLine (FreezeShimmerEditor& e) : owner (e) {}
    void paint (juce::Graphics&) override;
private:
    FreezeShimmerEditor& owner;
};

//==============================================================================
class FreezeShimmerEditor : public juce::AudioProcessorEditor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::Timer
{
public:
    explicit FreezeShimmerEditor (FreezeShimmerProcessor&);
    ~FreezeShimmerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    FreezeShimmerProcessor& proc;

    // utilisé par les sous-composants
    int  page() const { return proc.uiPage.load(); }
    void setPage (int p);
    void togglePage() { setPage (1 - page()); }
    int  choice (const char* id) const;
    void setChoice (const char* id, int index);
    void setBool (const char* id, bool v);
    bool getBool (const char* id) const;
    void cycleScene();
    void cycleMode();
    void stepPreset (int delta);
    void loadPreset (int index);
    void setWindowHeight (int h);
    void showModeMenu (juce::Component* target);
    void showSizeMenu (juce::Component* target);
    void showMidiMap();
    int  knobParamIndex (int knob) const;          // indice dans fsp::kKnobs du paramètre actuellement lié
    juce::Colour sceneColour() const;

    // dernier paramètre touché (pour la ligne d'état)
    std::atomic<int>   lastIndex { -1 };
    std::atomic<float> lastValue { 0.0f };
    std::atomic<bool>  lastFromMidi { false };

private:
    void parameterChanged (const juce::String&, float) override;
    void timerCallback() override;
    void bindKnobs();
    void showKnob7Menu();

    PedalLook look;
    juce::Image background, background1x;
    PedalKnob knob[7];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach[7];
    ModeButton modeButton { *this };
    FootSwitch footSwitch { *this };
    StatusLine status { *this };
    bool knob7Gliss = false;
    int lastMidiStampSeen = 0, lastPresetSeen = -1;
    int midiFlashTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeShimmerEditor)
};
