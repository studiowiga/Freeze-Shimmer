#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
const char* const kPrimary[7]   = { "dry", "effect", "blend",  "time",  "speed", "voice", "tone" };
const char* const kSecondary[7] = { "dry", "effect", "rvbmix", "delay", "layer", "depth", "rate" };
const Colour kBlue (0xff38bdf8);

Font makeFont (float size, bool bold)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}
}

//==============================================================================
// Potentiomètre vectoriel
void PedalLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                  float startAngle, float endAngle, Slider& s)
{
    const Rectangle<float> b ((float) x, (float) y, (float) w, (float) h);
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const bool sec = (bool) s.getProperties()["secondary"];
    const bool hot = s.isMouseOverOrDragging();
    auto disc = [&] (float k) { return Rectangle<float> (R * 2.0f * k, R * 2.0f * k).withCentre (c); };

    // ombre portée douce
    g.setGradientFill (ColourGradient (Colour (0x99000000), c.x, c.y + R * 0.05f, Colour (0x00000000), c.x, c.y + R * 1.05f, true));
    g.fillEllipse (disc (1.04f));
    // couronne métallique (bronze rosé)
    g.setGradientFill (ColourGradient (Colour (0xffeedacc), c.x - R * 0.55f, c.y - R * 0.85f,
                                       Colour (0xff6a544d), c.x + R * 0.50f, c.y + R * 0.95f, false));
    g.fillEllipse (disc (1.0f));
    g.setColour (Colour (0x66000000));
    g.drawEllipse (disc (1.0f), R * 0.012f);
    // gorge sombre
    g.setColour (Colour (0xff2a2226));
    g.fillEllipse (disc (0.925f));
    // liseré bleu en page secondaire
    if (sec)
    {
        g.setColour (kBlue.withAlpha (hot ? 0.95f : 0.75f));
        g.drawEllipse (disc (0.955f), R * 0.03f);
    }
    // capuchon crème
    {
        g.setGradientFill (ColourGradient (Colour (0xfffffdf7), c.x - R * 0.28f, c.y - R * 0.38f,
                                           Colour (0xffd6cebe), c.x + R * 0.32f, c.y + R * 0.58f, true));
        g.fillEllipse (disc (0.865f));
        // arête du capuchon
        g.setColour (Colour (0x30000000));
        g.drawEllipse (disc (0.865f), R * 0.014f);
        // reflet
        Rectangle<float> hl (R * 0.9f, R * 0.62f);
        hl.setCentre (c.x - R * 0.20f, c.y - R * 0.46f);
        g.setGradientFill (ColourGradient (Colour (0x80ffffff), hl.getCentreX(), hl.getCentreY(),
                                           Colour (0x00ffffff), hl.getRight(), hl.getCentreY(), true));
        g.fillEllipse (hl);
        // ombre intérieure en bas
        g.setGradientFill (ColourGradient (Colour (0x00000000), c.x, c.y + R * 0.15f, Colour (0x2a000000), c.x, c.y + R * 0.85f, false));
        g.fillEllipse (disc (0.84f));
    }
    // repère
    {
        const float a = startAngle + pos * (endAngle - startAngle);
        const float r1 = R * 0.30f, r2 = R * 0.72f;
        Path p;
        p.startNewSubPath (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1);
        p.lineTo (c.x + std::sin (a) * r2, c.y - std::cos (a) * r2);
        g.setColour (sec ? Colour (0xff17566d) : Colour (0xff33262b));
        g.strokePath (p, PathStrokeType (R * 0.075f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

//==============================================================================
// Bouton MODE : LED + gestes
bool ModeButton::hitTest (int x, int y)
{
    const auto c = getLocalBounds().getCentre().toFloat();
    return c.getDistanceFrom (Point<float> ((float) x, (float) y)) <= (float) getWidth() * 0.5f;
}

void ModeButton::paint (Graphics& g)
{
    // LED unique : éteinte en bypass, bleu fixe pédale active, bleu clignotant quand le freeze est actif
    if (! owner.getBool ("engage")) return;
    const bool frozen = owner.proc.frozenState.load();
    if (frozen && ! blinkPhase) return;
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = (float) getWidth() * 0.5f;
    const Colour led = kBlue;
    g.setGradientFill (ColourGradient (led.withAlpha (0.55f), c.x, c.y, led.withAlpha (0.0f), c.x, c.y + R, true));
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    const float cr = R * 0.66f;
    g.setGradientFill (ColourGradient (led.brighter (0.6f).withAlpha (0.80f), c.x - cr * 0.2f, c.y - cr * 0.3f,
                                       led.withAlpha (0.35f), c.x, c.y + cr, true));
    g.fillEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c));
}

void ModeButton::mouseDown (const MouseEvent& e)
{
    longFired = false;
    if (e.mods.isPopupMenu()) { owner.showModeMenu (this); longFired = true; return; }
    startTimer (800);
}

void ModeButton::timerCallback()
{
    stopTimer();
    longFired = true;
    owner.togglePage();   // maintien ~1 s : page secondaire (comme la pédale)
}

void ModeButton::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (w.deltaY > 0.0f)      owner.stepPreset (1);
    else if (w.deltaY < 0.0f) owner.stepPreset (-1);
}

void ModeButton::mouseUp (const MouseEvent& e)
{
    stopTimer();
    if (longFired || e.mods.isPopupMenu()) return;
    if (e.mods.isShiftDown()) owner.cycleMode();
    else                      owner.cycleScene();
}

//==============================================================================
// Footswitch
bool FootSwitch::hitTest (int x, int y)
{
    const auto c = getLocalBounds().getCentre().toFloat();
    return c.getDistanceFrom (Point<float> ((float) x, (float) y)) <= (float) getWidth() * 0.5f;
}

void FootSwitch::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = (float) getWidth() * 0.5f;
    if (owner.proc.frozenState.load())
    {
        const float pulse = 0.55f + 0.25f * std::sin ((float) Time::getMillisecondCounter() * 0.004f);
        g.setGradientFill (ColourGradient (Colour (0x0038d7ff), c.x, c.y, Colour (0xff6fe3ff).withAlpha (pulse), c.x, c.y + R, true));
        g.fillEllipse (Rectangle<float> (R * 2.1f, R * 2.1f).withCentre (c));
    }
    if (pressed)
    {
        g.setColour (Colour (0x30000000));
        g.fillEllipse (Rectangle<float> (R * 1.45f, R * 1.45f).withCentre (c));
    }
}

void FootSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { owner.showModeMenu (this); return; }
    pressed = true;
    ignoreUp = false;
    const auto now = Time::getMillisecondCounter();
    const bool doubleClick = (now - lastDownMs) < 380;
    lastDownMs = now;

    if (doubleClick)
    {
        // double-clic : arrête le freeze
        owner.proc.postCommand (FreezeShimmerProcessor::cmdRelease);
        ignoreUp = true;
        lastDownMs = 0;
    }
    else if (owner.choice ("fmode") != 2)
    {
        // maintenu : fige ce qui est entendu (LATCH : la capture reste après le relâchement ; MOMENT : seulement tant que l'on appuie)
        owner.proc.postCommand (FreezeShimmerProcessor::cmdHoldOn);
    }
    repaint();
}

void FootSwitch::mouseUp (const MouseEvent& e)
{
    if (! pressed) return;
    pressed = false;
    if (! e.mods.isPopupMenu() && ! ignoreUp)
    {
        if (owner.choice ("fmode") == 2) owner.proc.postCommand (FreezeShimmerProcessor::cmdToggleAuto);   // AUTO : arme / désarme
        else                             owner.proc.postCommand (FreezeShimmerProcessor::cmdHoldOff);
    }
    repaint();
}

//==============================================================================
// Ligne d'état
void StatusLine::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float h = b.getHeight();
    g.setColour (Colour (0xcc07121f));
    g.fillRoundedRectangle (b, h * 0.3f);
    g.setColour (Colour (0x9938bdf8));
    g.drawRoundedRectangle (b, h * 0.3f, jmax (1.0f, h * 0.045f));

    // -- gauche : dernier paramètre touché
    const int idx = owner.lastIndex.load();
    String left = "--", right = "MIDI --";
    if (idx >= 0 && idx < fsp::kNumKnobs)
    {
        const float v = owner.lastValue.load();
        left  = String (fsp::kKnobs[idx].name) + "  " + fsp::formatValue (idx, v);
        right = "CC" + String (fsp::kKnobs[idx].cc) + " = " + String (fsp::toMidi (v));
    }
    else if (idx == 100) { const int s = (int) owner.lastValue.load(); left = "SCENE  " + fsp::kSceneNames[jlimit (0, 2, s)]; right = "CC" + String (fsp::kCcScene) + " = " + String (s * 43); }
    else if (idx == 101) { const int s = (int) owner.lastValue.load(); left = "MODE  " + fsp::kModeNames[jlimit (0, 2, s)];  right = "CC" + String (fsp::kCcFMode) + " = " + String (s * 43); }
    else if (idx == 104) { const int pi = jlimit (0, fsp::kNumPresets - 1, (int) owner.lastValue.load()); left = String ("PRESET ") + String (pi + 1) + "  " + String (fsp::kPresets[pi].name); right = "PC " + String (pi); }
    else if (idx == 102) { const bool on = owner.lastValue.load() > 0.5f; left = on ? "ENGAGED" : "BYPASS"; right = "CC" + String (fsp::kCcEngage) + " = " + String (on ? 127 : 0); }

    const bool flash = owner.lastFromMidi.load();
    const Colour txt (0xffd7efff), accent (0xff6fe3ff), dim (0xff86a6bd);
    const float fs1 = jmax (9.0f, h * 0.50f);

    // couleur de la valeur = couleur de l'étiquette sous le potard (blanc : page A, bleu : page B)
    bool blueLabel = false;
    if (idx >= 0 && idx < fsp::kNumKnobs)
    {
        const juce::String kid (fsp::kKnobs[idx].id);
        blueLabel = (kid == "rvbmix" || kid == "delay" || kid == "layer" || kid == "depth" || kid == "rate");
    }
    g.setFont (makeFont (fs1, true));
    g.setColour (blueLabel ? accent : Colour (0xffffffff));
    auto inner = b.reduced (h * 0.45f, 0.0f);
    g.drawText (left, inner.removeFromLeft (inner.getWidth() * (getWidth() > 300 ? 0.36f : 0.5f)), Justification::centredLeft, false);

    g.setFont (makeFont (fs1, true));
    g.setColour (flash ? Colour (0xffa7f3a0) : accent);
    g.drawText (right, inner.removeFromRight (inner.getWidth() * (getWidth() > 300 ? 0.32f : 0.45f)), Justification::centredRight, false);

    // centre : scène · mode · page · état du freeze
    const bool frozen = owner.proc.frozenState.load();
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    String mid = fsp::kSceneNames[owner.choice ("scene")] + dot + fsp::kModeNames[owner.choice ("fmode")]
               + dot + (owner.page() == 0 ? "A" : "B");
    if (frozen) mid += dot + "FROZEN";
    if (getWidth() > 300 && idx != 104)
    {
        g.setFont (makeFont (fs1 * 0.82f, false));
        g.setColour (frozen ? accent : dim);
        g.drawText (mid, inner, Justification::centred, false);
    }
    else if (frozen)
    {
        g.setFont (makeFont (fs1 * 0.82f, true));
        g.setColour (accent);
        g.drawText ("FROZEN", inner, Justification::centred, false);
    }
}

//==============================================================================
FreezeShimmerEditor::FreezeShimmerEditor (FreezeShimmerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);

    for (int i = 0; i < 7; ++i)
    {
        knob[i].setLookAndFeel (&look);
        addAndMakeVisible (knob[i]);
    }
    for (int i = 0; i < 6; ++i) knob[i].onRightClick = [this, i] { showSizeMenu (&knob[i]); };
    knob[6].onRightClick = [this] { showKnob7Menu(); };
    addAndMakeVisible (modeButton);
    addAndMakeVisible (footSwitch);
    addAndMakeVisible (status);
    status.setInterceptsMouseClicks (false, false);

    for (int i = 0; i < fsp::kNumKnobs; ++i) proc.apvts.addParameterListener (fsp::kKnobs[i].id, this);
    for (auto id : { "scene", "fmode", "engage" }) proc.apvts.addParameterListener (id, this);

    bindKnobs();

    const double aspect = (double) art::W / (double) art::H;
    setResizable (true, true);
    setResizeLimits (roundToInt ((double) art::heightPct50 * aspect), art::heightPct50,
                     roundToInt ((double) art::heightMax * aspect), art::heightMax);
    getConstrainer()->setFixedAspectRatio (aspect);
    const int h0 = jlimit (art::heightPct50, art::heightMax, proc.uiHeight.load());
    setSize (roundToInt ((double) h0 * aspect), h0);
    lastPresetSeen = proc.presetIndex.load();
    startTimerHz (30);
}

FreezeShimmerEditor::~FreezeShimmerEditor()
{
    stopTimer();
    for (int i = 0; i < fsp::kNumKnobs; ++i) proc.apvts.removeParameterListener (fsp::kKnobs[i].id, this);
    for (auto id : { "scene", "fmode", "engage" }) proc.apvts.removeParameterListener (id, this);
    for (auto& a : attach) a.reset();
    for (auto& k : knob) k.setLookAndFeel (nullptr);
}

int FreezeShimmerEditor::choice (const char* id) const { return (int) proc.apvts.getRawParameterValue (id)->load(); }
bool FreezeShimmerEditor::getBool (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

void FreezeShimmerEditor::setChoice (const char* id, int index)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void FreezeShimmerEditor::setBool (const char* id, bool v)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (v ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void FreezeShimmerEditor::loadPreset (int index)
{
    proc.applyPreset (index);
    lastPresetSeen = proc.presetIndex.load();
    lastIndex.store (104);
    lastValue.store ((float) lastPresetSeen);
    lastFromMidi.store (false);
}

void FreezeShimmerEditor::stepPreset (int delta)
{
    loadPreset ((proc.presetIndex.load() + delta + fsp::kNumPresets) % fsp::kNumPresets);
}

void FreezeShimmerEditor::setWindowHeight (int h)
{
    h = jlimit (art::heightPct50, art::heightMax, h);
    setSize (roundToInt ((double) h * (double) art::W / (double) art::H), h);
}

void FreezeShimmerEditor::cycleScene() { setChoice ("scene", (choice ("scene") + 1) % 3); }
void FreezeShimmerEditor::cycleMode()  { setChoice ("fmode", (choice ("fmode") + 1) % 3); }

Colour FreezeShimmerEditor::sceneColour() const
{
    switch (choice ("scene"))
    {
        case 0:  return Colour (0xff4ade80);   // intergalactic : vert
        case 1:  return Colour (0xfffb923c);   // off-world : orange
        default: return Colour (0xfff43f5e);   // etherdust : rouge
    }
}

void FreezeShimmerEditor::setPage (int p)
{
    proc.uiPage.store (p);
    bindKnobs();
    repaint();
}

int FreezeShimmerEditor::knobParamIndex (int k) const
{
    const char* id = (k == 6 && knob7Gliss) ? "gliss" : (page() == 0 ? kPrimary[k] : kSecondary[k]);
    return fsp::findKnob (id);
}

void FreezeShimmerEditor::bindKnobs()
{
    for (int i = 0; i < 7; ++i)
    {
        attach[i].reset();
        const int pi = knobParamIndex (i);
        const bool sec = (i >= 2 && page() == 1) || (i == 6 && knob7Gliss);
        knob[i].getProperties().set ("secondary", sec);
        knob[i].setDoubleClickReturnValue (true, (double) fsp::kKnobs[pi].def);
        attach[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, fsp::kKnobs[pi].id, knob[i]);
        knob[i].repaint();
    }
    lastIndex.store (-1);
}

void FreezeShimmerEditor::showKnob7Menu()
{
    PopupMenu m;
    m.addSectionHeader ("Bouton TONE / RATE / GLISS");
    m.addItem (1, "TONE / RATE (suit la page)", true, ! knob7Gliss);
    m.addItem (2, "GLISS (freeze)", true, knob7Gliss);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&knob[6]), [this] (int r)
    {
        if (r == 0) return;
        knob7Gliss = (r == 2);
        bindKnobs();
    });
}

void FreezeShimmerEditor::showModeMenu (Component* target)
{
    PopupMenu m, scenes, modes, chans;
    for (int i = 0; i < 3; ++i) scenes.addItem (1 + i, fsp::kSceneNames[i], true, choice ("scene") == i);
    for (int i = 0; i < 3; ++i) modes.addItem (11 + i, fsp::kModeNames[i], true, choice ("fmode") == i);
    for (int i = 0; i < 17; ++i) chans.addItem (100 + i, i == 0 ? String ("OMNI") : String (i), true, choice ("midichan") == i);
    PopupMenu presets, sizes;
    for (int i = 0; i < fsp::kNumPresets; ++i)
        presets.addItem (200 + i, String (i + 1) + "  " + fsp::kPresets[i].name, true, proc.presetIndex.load() == i);
    const int curH = getHeight();
    sizes.addItem (301, "Default", true, std::abs (curH - art::heightPct50)  < 8);
    sizes.addItem (304, "60 %",   true, std::abs (curH - art::heightPct60)  < 8);
    sizes.addItem (302, "75 %",   true, std::abs (curH - art::heightPct75)  < 8);
    sizes.addItem (303, "100 %",  true, std::abs (curH - art::heightPct100) < 8);
    m.addSubMenu ("Presets d'usine", presets);
    m.addSubMenu ("Window size", sizes);
    m.addSeparator();
    m.addSubMenu ("Scene (shimmer)", scenes);
    m.addSubMenu ("Mode freeze", modes);
    m.addSeparator();
    m.addItem (21, "Boutons : page A (primaire)", true, page() == 0);
    m.addItem (22, "Boutons : page B (secondaire, bleu)", true, page() == 1);
    m.addSeparator();
    m.addItem (30, "Effet actif (bypass si decoche)", true, getBool ("engage"));
    m.addItem (31, "Arreter le freeze (ou double-clic footswitch)");
    m.addSeparator();
    m.addSubMenu ("Canal MIDI", chans);
    m.addItem (40, "Table MIDI...");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (r == 0) return;
        if (r >= 1 && r <= 3)        setChoice ("scene", r - 1);
        else if (r >= 11 && r <= 13) setChoice ("fmode", r - 11);
        else if (r == 21)            setPage (0);
        else if (r == 22)            setPage (1);
        else if (r == 30)            setBool ("engage", ! getBool ("engage"));
        else if (r == 31)            proc.postCommand (FreezeShimmerProcessor::cmdRelease);
        else if (r == 40)            showMidiMap();
        else if (r >= 100 && r <= 116) setChoice ("midichan", r - 100);
        else if (r >= 200 && r < 200 + fsp::kNumPresets) loadPreset (r - 200);
        else if (r == 301) setWindowHeight (art::heightPct50);
        else if (r == 304) setWindowHeight (art::heightPct60);
        else if (r == 302) setWindowHeight (art::heightPct75);
        else if (r == 303) setWindowHeight (art::heightPct100);
    });
}

void FreezeShimmerEditor::showSizeMenu (Component* target)
{
    const int curH = getHeight();
    PopupMenu m;
    m.addSectionHeader ("Window size");
    m.addItem (301, "Default", true, std::abs (curH - art::heightPct50)  < 8);
    m.addItem (304, "60 %",   true, std::abs (curH - art::heightPct60)  < 8);
    m.addItem (302, "75 %",   true, std::abs (curH - art::heightPct75)  < 8);
    m.addItem (303, "100 %",  true, std::abs (curH - art::heightPct100) < 8);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (r == 301) setWindowHeight (art::heightPct50);
        else if (r == 304) setWindowHeight (art::heightPct60);
        else if (r == 302) setWindowHeight (art::heightPct75);
        else if (r == 303) setWindowHeight (art::heightPct100);
    });
}

void FreezeShimmerEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showSizeMenu (this);
}

void FreezeShimmerEditor::showMidiMap()
{
    String t;
    for (int i = 0; i < fsp::kNumKnobs; ++i)
        t << "CC " << fsp::kKnobs[i].cc << "  =  " << fsp::kKnobs[i].name << "\n";
    t << "\nCC " << fsp::kCcScene   << "  =  Scene (0-42 / 43-85 / 86-127)"
      << "\nCC " << fsp::kCcFMode   << "  =  Mode freeze LATCH / MOMENT / AUTO"
      << "\nCC " << fsp::kCcFreeze  << "  =  Footswitch freeze (>= 64)"
      << "\nCC " << fsp::kCcSustain << "  =  Pedale de sustain = footswitch"
      << "\nCC " << fsp::kCcEngage  << "  =  Effet actif (>= 64) / bypass"
      << "\nCC " << fsp::kCcRelease << "  =  Relache le freeze"
      << "\nProgram Change 0-9  =  Preset d'usine";
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Freeze Shimmer - table MIDI", t);
}

void FreezeShimmerEditor::parameterChanged (const String& id, float v)
{
    const int k = fsp::findKnob (id);
    int idx = k;
    float val = v;
    if (k < 0)
    {
        if (id == "scene") idx = 100;
        else if (id == "fmode") idx = 101;
        else if (id == "engage") idx = 102;
        else return;
    }
    // un bouton affiché ne doit pas écraser l'info d'un paramètre non visible
    lastIndex.store (idx);
    lastValue.store (val);
    lastFromMidi.store (proc.lastMidiStamp.load() != lastMidiStampSeen);
}

void FreezeShimmerEditor::timerCallback()
{
    const int stamp = proc.lastMidiStamp.load();
    if (stamp != lastMidiStampSeen) { lastMidiStampSeen = stamp; midiFlashTicks = 24; }
    if (midiFlashTicks > 0) --midiFlashTicks; else lastFromMidi.store (false);
    const int pr = proc.presetIndex.load();
    if (pr != lastPresetSeen) { lastPresetSeen = pr; lastIndex.store (104); lastValue.store ((float) pr); lastFromMidi.store (false); }
    modeButton.setBlink ((Time::getMillisecondCounter() / 350) % 2 == 0);
    modeButton.repaint();
    status.repaint();
    footSwitch.repaint();
}

void FreezeShimmerEditor::paint (Graphics& g)
{
    g.fillAll (Colours::white);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

void FreezeShimmerEditor::resized()
{
    // jamais plus petit que la taille par défaut (50 %), quelle que soit la demande de l'hôte
    const int minW = roundToInt ((double) art::heightPct50 * (double) art::W / (double) art::H);
    if (getHeight() < art::heightPct50 || getWidth() < minW)
    {
        setSize (minW, art::heightPct50);
        return;
    }
    proc.uiHeight.store (getHeight());
    const float s = (float) getWidth() / art::W;
    auto place = [s] (Component& c, const art::Circle& k, float scale = 1.0f)
    {
        const float d = k.r * 2.0f * s * scale;
        c.setBounds (Rectangle<float> (d, d).withCentre ({ k.x * s, k.y * s }).toNearestInt());
    };
    for (int i = 0; i < 7; ++i) place (knob[i], art::knobs[i]);
    place (modeButton, art::modeBtn, 1.25f);
    place (footSwitch, art::footswitch, 1.1f);
    status.setBounds (Rectangle<float> (art::statusX * s, art::statusY * s, art::statusW * s, art::statusH * s).toNearestInt());
}
