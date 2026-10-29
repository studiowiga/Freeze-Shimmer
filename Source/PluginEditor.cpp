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
    const float rot = startAngle + pos * (endAngle - startAngle);   // angle du potentiomètre
    const float twoPi = juce::MathConstants<float>::twoPi;
    auto disc = [] (Point<float> p, float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p); };

    const float skirtR = R * 0.97f, capR = R * 0.80f;
    const Point<float> cs (c.x, c.y + R * 0.07f);     // la jupe est légèrement plus basse (effet cylindre)

    // ombre portée douce
    g.setGradientFill (ColourGradient (Colour (0xb0000000), cs.x, cs.y + R * 0.06f, Colour (0x00000000), cs.x, cs.y + R * 1.12f, true));
    g.fillEllipse (disc ({ cs.x, cs.y + R * 0.05f }, R * 1.10f));

    // jupe cylindrique striée
    g.setGradientFill (ColourGradient (Colour (0xff2a2d33), cs.x, cs.y - skirtR, Colour (0xff050506), cs.x, cs.y + skirtR, false));
    g.fillEllipse (disc (cs, skirtR));
    // stries : elles tournent avec le potentiomètre ; éclairage fixe (haut-gauche)
    {
        const int n = 48;
        for (int i = 0; i < n; ++i)
        {
            const float a = rot + twoPi * (float) i / (float) n;       // position réelle de la strie
            const float lit = 0.5f + 0.5f * std::cos (a + 0.6f);        // 1 = face à la lumière
            const float r1 = skirtR * 0.82f, r2 = skirtR * 0.985f;
            g.setColour (Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, 0.06f + 0.30f * lit * lit));
            g.drawLine (cs.x + std::sin (a) * r1, cs.y - std::cos (a) * r1,
                        cs.x + std::sin (a) * r2, cs.y - std::cos (a) * r2, R * 0.034f);
            const float a2 = a + twoPi / (float) n * 0.5f;
            g.setColour (Colour::fromFloatRGBA (0.0f, 0.0f, 0.0f, 0.55f));
            g.drawLine (cs.x + std::sin (a2) * r1, cs.y - std::cos (a2) * r1,
                        cs.x + std::sin (a2) * r2, cs.y - std::cos (a2) * r2, R * 0.03f);
        }
    }
    // liseré bleu en page secondaire
    if (sec)
    {
        g.setColour (kBlue.withAlpha (hot ? 0.95f : 0.80f));
        g.drawEllipse (disc (cs, skirtR), R * 0.04f);
    }
    // capuchon noir brossé
    g.setGradientFill (ColourGradient (Colour (0xff383b41), c.x - capR * 0.5f, c.y - capR * 0.7f,
                                       Colour (0xff060607), c.x + capR * 0.5f, c.y + capR * 0.8f, false));
    g.fillEllipse (disc (c, capR));
    g.setColour (Colour (0x66ffffff));
    g.drawEllipse (disc (c, capR), R * 0.016f);
    // reflets brossés fixes (deux secteurs opposés)
    {
        Path p1, p2;
        p1.addPieSegment (disc (c, capR * 0.96f), -0.40f, 0.20f, 0.0f);
        p2.addPieSegment (disc (c, capR * 0.96f), juce::MathConstants<float>::pi - 0.40f, juce::MathConstants<float>::pi + 0.20f, 0.0f);
        g.setColour (Colour (0x18ffffff));
        g.fillPath (p1);
        g.fillPath (p2);
    }
    // repère
    {
        const float r1 = capR * 0.18f, r2 = capR * 0.92f;
        Path p;
        p.startNewSubPath (c.x + std::sin (rot) * r1, c.y - std::cos (rot) * r1);
        p.lineTo (c.x + std::sin (rot) * r2, c.y - std::cos (rot) * r2);
        g.setColour (sec ? Colour (0xff8fdcff) : Colour (0xfff4f7fb));
        g.strokePath (p, PathStrokeType (R * 0.085f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}


namespace
{
// halo circulaire : dégradé radial multi-arrêts, alpha → 0 exactement au bord (jamais rogné)
void fillHalo (juce::Graphics& g, juce::Point<float> c, float rCore, float rOuter, juce::Colour col, float peak)
{
    juce::ColourGradient gr (col.withAlpha (peak), c.x, c.y, col.withAlpha (0.0f), c.x + rOuter, c.y, true);
    const double k = (double) (rCore / rOuter);
    gr.addColour (k,              col.withAlpha (peak * 0.85f));
    gr.addColour (k + (1.0 - k) * 0.25, col.withAlpha (peak * 0.50f));
    gr.addColour (k + (1.0 - k) * 0.55, col.withAlpha (peak * 0.20f));
    gr.addColour (k + (1.0 - k) * 0.80, col.withAlpha (peak * 0.05f));
    g.setGradientFill (gr);
    g.fillEllipse (juce::Rectangle<float> (rOuter * 2.0f, rOuter * 2.0f).withCentre (c));
}
}

//==============================================================================
// Bouton MODE : LED + gestes
bool ModeButton::hitTest (int x, int y)
{
    const auto c = getLocalBounds().getCentre().toFloat();
    return c.getDistanceFrom (Point<float> ((float) x, (float) y)) <= coreRadius();
}

void ModeButton::paint (Graphics& g)
{
    // LED complète (recouvre celle de l'image) : éteinte en bypass, bleu fixe pédale active, bleu clignotant en freeze
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = coreRadius();
    const bool on = owner.getBool ("engage") && ! (owner.proc.frozenState.load() && ! blinkPhase);

    // chemise métallique
    g.setGradientFill (ColourGradient (Colour (0xffb9c3cf), c.x - R * 0.4f, c.y - R * 0.5f, Colour (0xff3b4350), c.x + R * 0.5f, c.y + R * 0.6f, false));
    g.fillEllipse (Rectangle<float> (R * 1.70f, R * 1.70f).withCentre (c));
    g.setColour (Colour (0xff0b1018));
    g.fillEllipse (Rectangle<float> (R * 1.38f, R * 1.38f).withCentre (c));

    if (on)
    {
        fillHalo (g, c, R * 0.9f, (float) getWidth() * 0.5f, kBlue, 0.70f);
        g.setGradientFill (ColourGradient (Colour (0xffe6f8ff), c.x - R * 0.2f, c.y - R * 0.25f, kBlue.darker (0.3f), c.x, c.y + R * 0.7f, true));
        g.fillEllipse (Rectangle<float> (R * 1.2f, R * 1.2f).withCentre (c));
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff2b3340), c.x - R * 0.2f, c.y - R * 0.25f, Colour (0xff0d1219), c.x, c.y + R * 0.7f, true));
        g.fillEllipse (Rectangle<float> (R * 1.2f, R * 1.2f).withCentre (c));
    }
}

void ModeButton::mouseDown (const MouseEvent& e)
{
    longFired = false;
    if (e.mods.isPopupMenu()) { owner.showModeMenu (this, FreezeShimmerProcessor::kLearnScene, FreezeShimmerProcessor::kLearnFMode); longFired = true; return; }
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
    return c.getDistanceFrom (Point<float> ((float) x, (float) y)) <= coreRadius();
}

void FootSwitch::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = coreRadius();
    if (owner.proc.frozenState.load())
    {
        const float pulse = 0.55f + 0.25f * std::sin ((float) Time::getMillisecondCounter() * 0.004f);
        fillHalo (g, c, R * 0.95f, (float) getWidth() * 0.5f, Colour (0xff4fd8ff), pulse);
    }
    if (pressed)
    {
        g.setColour (Colour (0x30000000));
        g.fillEllipse (Rectangle<float> (R * 1.45f, R * 1.45f).withCentre (c));
    }
}

void FootSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { owner.showModeMenu (this, FreezeShimmerProcessor::kLearnFreeze, FreezeShimmerProcessor::kLearnRelease); return; }
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

    // MIDI Learn : « MIDI LEARN: move a control for X », puis « X = CC n » pendant 2 s
    if (owner.learnLine.isNotEmpty())
    {
        g.setFont (makeFont (jmax (9.0f, h * 0.50f), true));
        g.setColour (Colour (0xffa7f3a0));
        g.drawFittedText (owner.learnLine, b.reduced (h * 0.45f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
        return;
    }

    // -- gauche : dernier paramètre touché
    const int idx = owner.lastIndex.load();
    String left = "--", right = "MIDI --";
    if (idx >= 0 && idx < fsp::kNumKnobs)
    {
        const float v = owner.lastValue.load();
        left  = String (fsp::kKnobs[idx].name) + "  " + fsp::formatValue (idx, v);
        right = owner.ccLabel (idx).removeCharacters (" ") + " = " + String (fsp::toMidi (v));
    }
    else if (idx == 100) { const int s = (int) owner.lastValue.load(); left = "SCENE  " + fsp::kSceneNames[jlimit (0, 2, s)]; right = owner.ccLabel (FreezeShimmerProcessor::kLearnScene).removeCharacters (" ") + " = " + String (s * 43); }
    else if (idx == 101) { const int s = (int) owner.lastValue.load(); left = "MODE  " + fsp::kModeNames[jlimit (0, 2, s)];  right = owner.ccLabel (FreezeShimmerProcessor::kLearnFMode).removeCharacters (" ") + " = " + String (s * 43); }
    else if (idx == 104) { const int pi = jlimit (0, fsp::kNumPresets - 1, (int) owner.lastValue.load()); left = String ("PRESET ") + String (pi + 1) + "  " + String (fsp::kPresets[pi].name); right = "PC " + String (pi); }
    else if (idx == 102) { const bool on = owner.lastValue.load() > 0.5f; left = on ? "ENGAGED" : "BYPASS"; right = owner.ccLabel (FreezeShimmerProcessor::kLearnEngage).removeCharacters (" ") + " = " + String (on ? 127 : 0); }

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
    background   = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);       // 780 x 990
    background1x = ImageCache::getFromMemory (BinaryData::background_1x_jpg, BinaryData::background_1x_jpgSize); // 390 x 495 (net à la taille par défaut)

    for (int i = 0; i < 7; ++i)
    {
        knob[i].setLookAndFeel (&look);
        addAndMakeVisible (knob[i]);
    }
    for (int i = 0; i < 6; ++i) knob[i].onRightClick = [this, i] { showSizeMenu (&knob[i], knobParamIndex (i)); };
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
    setOpaque (true);                      // anti-gel : l'hôte ne redessine jamais ce qui est derrière la fenêtre
    const int h0 = jlimit (art::heightPct50, art::heightMax, proc.uiHeight.load());
    setSize (roundToInt ((double) h0 * aspect), h0);
    rebuildCaches();
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
    addLearnSection (m, fsp::kKnobs[knobParamIndex (6)].name, knobParamIndex (6));
    m.addSectionHeader ("Bouton TONE / RATE / GLISS");
    m.addItem (1, "TONE / RATE (suit la page)", true, ! knob7Gliss);
    m.addItem (2, "GLISS (freeze)", true, knob7Gliss);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&knob[6]), [this] (int r)
    {
        if (r == 0 || handleLearnResult (r)) return;
        knob7Gliss = (r == 2);
        bindKnobs();
    });
}

//==============================================================================
// MIDI Learn
String FreezeShimmerEditor::ccLabel (int i) const
{
    const int cc = proc.ccMap[jlimit (0, FreezeShimmerProcessor::kNumLearn - 1, i)].load();
    return cc >= 0 ? "CC " + String (cc) : String ("no CC");
}

void FreezeShimmerEditor::addLearnSection (PopupMenu& m, const String& header, int a, int b)
{
    const int lt = proc.learnTarget.load();
    m.addSectionHeader (header);
    for (int i : { a, b })
    {
        if (i < 0) continue;
        const String name = FreezeShimmerProcessor::learnDef (i).name;
        m.addItem (1000 + i, "MIDI Learn: " + name + " (" + ccLabel (i) + ")", true, lt == i);
        m.addItem (1100 + i, "Clear MIDI: " + name, proc.ccMap[i].load() >= 0);
    }
    if (lt >= 0) m.addItem (1200, "Cancel MIDI learn");
    m.addSeparator();
}

void FreezeShimmerEditor::addMidiLearnSubMenu (PopupMenu& m)
{
    PopupMenu midi;
    const int lt = proc.learnTarget.load();
    for (int i = 0; i < FreezeShimmerProcessor::kNumLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (FreezeShimmerProcessor::learnDef (i).name) + " (" + ccLabel (i) + ")", true, lt == i);
    midi.addSeparator();
    if (lt >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Reset MIDI to defaults (CC 20-32, 102-106)");
    m.addSubMenu ("MIDI Learn", midi);
}

bool FreezeShimmerEditor::handleLearnResult (int r)
{
    const int n = FreezeShimmerProcessor::kNumLearn;
    if (r >= 1000 && r < 1000 + n)      proc.learnTarget.store (proc.learnTarget.load() == r - 1000 ? -1 : r - 1000);   // re-cliquer annule
    else if (r >= 1100 && r < 1100 + n) proc.ccMap[r - 1100].store (-1);
    else if (r == 1200)                 proc.learnTarget.store (-1);
    else if (r == 1201)                 { proc.learnTarget.store (-1); proc.resetMidiMap(); }
    else return false;
    return true;
}

void FreezeShimmerEditor::showModeMenu (Component* target, int learnA, int learnB)
{
    PopupMenu m, scenes, modes, chans;
    if (learnA >= 0)
        addLearnSection (m, learnA == FreezeShimmerProcessor::kLearnScene ? "MODE button" : "Footswitch", learnA, learnB);
    for (int i = 0; i < 3; ++i) scenes.addItem (1 + i, fsp::kSceneNames[i], true, choice ("scene") == i);
    for (int i = 0; i < 3; ++i) modes.addItem (11 + i, fsp::kModeNames[i], true, choice ("fmode") == i);
    for (int i = 0; i < 17; ++i) chans.addItem (100 + i, i == 0 ? String ("OMNI") : String (i), true, choice ("midichan") == i);
    PopupMenu presets, sizes;
    for (int i = 0; i < fsp::kNumPresets; ++i)
        presets.addItem (200 + i, String (i + 1) + "  " + fsp::kPresets[i].name, true, proc.presetIndex.load() == i);
    const int curH = getHeight();
    sizes.addItem (301, "60 % (default)", true, std::abs (curH - art::heightPct50)  < 8);
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
    addMidiLearnSubMenu (m);
    m.addItem (40, "Table MIDI...");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (r == 0 || handleLearnResult (r)) return;
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

void FreezeShimmerEditor::showSizeMenu (Component* target, int learnIdx)
{
    const int curH = getHeight();
    PopupMenu m;
    if (learnIdx >= 0) addLearnSection (m, FreezeShimmerProcessor::learnDef (learnIdx).name, learnIdx);
    m.addSectionHeader ("Window size");
    m.addItem (301, "60 % (default)", true, std::abs (curH - art::heightPct50)  < 8);
    m.addItem (302, "75 %",   true, std::abs (curH - art::heightPct75)  < 8);
    m.addItem (303, "100 %",  true, std::abs (curH - art::heightPct100) < 8);
    m.addSeparator();
    addMidiLearnSubMenu (m);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (handleLearnResult (r)) return;
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
    // assignations actuelles (MIDI Learn : clic droit sur une commande)
    String t;
    for (int i = 0; i < fsp::kNumKnobs; ++i)
        t << ccLabel (i) << "  =  " << fsp::kKnobs[i].name << "\n";
    t << "\n" << ccLabel (FreezeShimmerProcessor::kLearnScene)   << "  =  Scene (0-42 / 43-85 / 86-127)"
      << "\n" << ccLabel (FreezeShimmerProcessor::kLearnFMode)   << "  =  Mode freeze LATCH / MOMENT / AUTO"
      << "\n" << ccLabel (FreezeShimmerProcessor::kLearnFreeze)  << "  =  Footswitch freeze (>= 64)"
      << "\nCC " << fsp::kCcSustain << "  =  Pedale de sustain = footswitch (si CC 64 non assigne)"
      << "\n" << ccLabel (FreezeShimmerProcessor::kLearnEngage)  << "  =  Effet actif (>= 64) / bypass"
      << "\n" << ccLabel (FreezeShimmerProcessor::kLearnRelease) << "  =  Relache le freeze"
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

    // moteur logiciel : seulement si la fenêtre hôte a changé, ou au plus une fois par seconde
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    // caches : reconstruits ici seulement (jamais dans paint)
    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    // chien de garde : un redessin demandé n'arrive pas -> on relance le moteur de rendu et on redessine tout
    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    // on ne redessine un composant que si ce qu'il affiche a vraiment changé (zéro travail au repos)
    const bool frozen = proc.frozenState.load();

    // LED MODE : éteinte / bleu fixe / bleu clignotant (en freeze)
    const bool blink = (Time::getMillisecondCounter() / 350) % 2 == 0;
    modeButton.setBlinkPhase (blink);
    const bool modeOn = getBool ("engage") && ! (frozen && ! blink);
    if (modeOn != drawnModeOn) { drawnModeOn = modeOn; askRepaint (modeButton); }

    // footswitch : halo pulsé tant que le freeze est actif (animation continue), un dernier redessin à l'arrêt
    const float pulse = frozen ? 0.55f + 0.25f * std::sin ((float) Time::getMillisecondCounter() * 0.004f) : 0.0f;
    if (frozen != drawnFrozen || std::abs (pulse - drawnPulse) >= 0.002f)
    {
        drawnFrozen = frozen; drawnPulse = pulse;
        askRepaint (footSwitch);
    }

    // MIDI Learn : attente puis confirmation 2 s dans la ligne d'état
    {
        const int learned = proc.lastLearned.exchange (-1);
        const int lt = proc.learnTarget.load();
        if (learned >= 0)
        {
            learnLine = String (FreezeShimmerProcessor::learnDef (learned).name) + " = " + ccLabel (learned);
            learnLineUntil = now + 2000.0;
        }
        else if (lt >= 0)
        {
            learnLine = "MIDI LEARN: move a control for " + String (FreezeShimmerProcessor::learnDef (lt).name);
            learnLineUntil = now + 100.0;
        }
        else if (now >= learnLineUntil)
            learnLine.clear();
    }

    // ligne d'état
    StatusState st;
    st.learn = learnLine;
    st.idx = lastIndex.load(); st.value = lastValue.load(); st.flash = lastFromMidi.load(); st.frozen = frozen;
    st.scene = choice ("scene"); st.fmode = choice ("fmode"); st.page = page();
    if (! drawnStatusValid || st != drawnStatus)
    {
        drawnStatus = st; drawnStatusValid = true;
        askRepaint (status);
    }
}

void FreezeShimmerEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::white);
    // anti-gel : aucun rescale ici, on dessine le cache existant (ou l'original en attendant le timer)
    if (bgScaled.isValid())
    {
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (bgScaled, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
        return;
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    const bool native = std::abs (getHeight() - art::heightPct50) <= 2 && background1x.isValid();
    g.drawImage (native ? background1x : background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

// fond déjà à la taille de la fenêtre en pixels physiques : constructeur et timer, jamais paint()
void FreezeShimmerEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;

    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    // taille par défaut en 100 % : l'image 1x est déjà nette, on la garde telle quelle
    if (background1x.isValid() && std::abs (ph - background1x.getHeight()) <= 2) bgScaled = background1x;
    else if (pw >= background.getWidth())                                         bgScaled = background;
    else bgScaled = background.rescaled (pw, ph, Graphics::highResamplingQuality);
    repaint();
}

float FreezeShimmerEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void FreezeShimmerEditor::askRepaint (Component& c)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (c.getBounds());       // la zone de l'éditeur couvre aussi le composant (redessiné avec)
}

//==============================================================================
// Anti-gel de la fenêtre (Windows) : JUCE 8 dessine par défaut avec Direct2D (carte graphique).
// Avec certains pilotes (Intel notamment) la fenêtre du plugin cesse de se rafraîchir alors que
// le DAW continue de tourner. Le moteur logiciel est stable partout et largement assez rapide ici.
//
// Anti-gel v2 (Studio Wiga) :
//  1. moteur de rendu logiciel forcé sur Windows, appliqué à chaque nouvelle fenêtre hôte (peer)
//     et revérifié toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : le fond mis à l'échelle est refait par le timer seulement,
//     après stabilisation de la taille ou quand l'échelle d'affichage change (au plus toutes les 250 ms) ;
//  3. chien de garde : si un redessin demandé n'est pas arrivé au bout de 1,5 s alors que la fenêtre
//     est visible, le moteur de rendu est réappliqué et toute la fenêtre est redessinée ;
//  4. côté audio : aucun appel à l'hôte depuis le thread audio (voir PluginProcessor).
void FreezeShimmerEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void FreezeShimmerEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void FreezeShimmerEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void FreezeShimmerEditor::resized()
{
    // anti-gel : jamais de setSize ici (mise à l'échelle Windows 125 / 150 % : l'hôte arrondit les tailles) ;
    // les limites du constrainer (jamais plus petit que la taille par défaut) suffisent
    proc.uiHeight.store (jlimit (art::heightPct50, art::heightMax, getHeight()));
    resizeSettle = 4;              // le fond sera refait par le timer quand la taille ne bouge plus
    const float s = (float) getWidth() / art::W;
    auto place = [s] (Component& c, const art::Circle& k, float scale = 1.0f)
    {
        const float d = k.r * 2.0f * s * scale;
        c.setBounds (Rectangle<float> (d, d).withCentre ({ k.x * s, k.y * s }).toNearestInt());
    };
    for (int i = 0; i < 7; ++i) place (knob[i], art::knobs[i]);
    place (modeButton, art::modeBtn, ModeButton::kPad);
    place (footSwitch, art::footswitch, FootSwitch::kPad);
    status.setBounds (Rectangle<float> (art::statusX * s, art::statusY * s, art::statusW * s, art::statusH * s).toNearestInt());
}
