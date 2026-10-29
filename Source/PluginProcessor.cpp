#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <array>

namespace
{
constexpr int kChunk = 512;
inline float readP (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); }
}

FreezeShimmerProcessor::FreezeShimmerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", fsp::createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pDry = get ("dry");     pEffect = get ("effect"); pBlend = get ("blend");  pRvb = get ("rvbmix");
    pTime = get ("time");   pDelay = get ("delay");   pSpeed = get ("speed");  pLayer = get ("layer");
    pVoice = get ("voice"); pDepth = get ("depth");   pTone = get ("tone");    pRate = get ("rate");
    pGliss = get ("gliss"); pScene = get ("scene");   pFMode = get ("fmode");  pFreeze = get ("freeze");
    pEngage = get ("engage"); pMidiChan = get ("midichan");

    for (auto* v : { &wetL, &wetR, &shL, &shR, &frL, &frR }) v->assign ((size_t) kChunk, 0.0f);
    for (int i = 0; i < kNumQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }

    resetMidiMap();
    applyPreset (0);   // son par défaut : CLEAN PLATE (premier preset) (écrasé par l'état sauvegardé du projet)
    startTimerHz (60);
}

//==============================================================================
// MIDI Learn (v1.2)
FreezeShimmerProcessor::LearnDef FreezeShimmerProcessor::learnDef (int i)
{
    if (i >= 0 && i < fsp::kNumKnobs) return { fsp::kKnobs[i].id, fsp::kKnobs[i].name, fsp::kKnobs[i].cc, learnContinuous };
    switch (i)
    {
        case kLearnScene:   return { "scene",   "SCENE",          fsp::kCcScene,   learnSelector };     // 0-42 / 43-85 / 86-127
        case kLearnFMode:   return { "fmode",   "FREEZE MODE",    fsp::kCcFMode,   learnSelector };
        case kLearnFreeze:  return { "freeze",  "FOOTSWITCH",     fsp::kCcFreeze,  learnFootswitch };   // >= 64 : enfoncé
        case kLearnEngage:  return { "engage",  "ENGAGE",         fsp::kCcEngage,  learnToggle };       // >= 64 : effet actif
        default:            return { "release", "RELEASE FREEZE", fsp::kCcRelease, learnTrigger };      // >= 64 : relâche
    }
}

void FreezeShimmerProcessor::resetMidiMap()
{
    for (int i = 0; i < kNumLearn; ++i) ccMap[i].store (learnDef (i).defaultCC);
}

// Anti-gel v2 (Studio Wiga) : jamais d'appel à l'hôte (setValueNotifyingHost) ni d'allocation depuis le
// thread audio. Le MIDI et les commandes du footswitch mettent la valeur en file ; ce timer (thread
// message) la transmet au DAW et à la fenêtre, et charge les presets demandés par Program Change.
const char* FreezeShimmerProcessor::queuedId (int i)
{
    if (i < fsp::kNumKnobs) return fsp::kKnobs[juce::jmax (0, i)].id;
    if (i == kQScene) return "scene";
    if (i == kQFMode) return "fmode";
    return "engage";
}

void FreezeShimmerProcessor::queueNormalised (int index, float v01)
{
    if (index < 0 || index >= kNumQueued) return;
    pendingValue[index].store (juce::jlimit (0.0f, 1.0f, v01), std::memory_order_relaxed);
    pendingSet[index].store (true, std::memory_order_release);
}

float FreezeShimmerProcessor::engageNow() const
{
    return pendingSet[kQEngage].load (std::memory_order_acquire) ? pendingValue[kQEngage].load (std::memory_order_relaxed)
                                                                 : readP (pEngage);
}

void FreezeShimmerProcessor::timerCallback()
{
    const int pr = pendingPreset.exchange (-1);
    if (pr >= 0) applyPreset (pr);

    for (int i = 0; i < kNumQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            setNormalised (queuedId (i), pendingValue[i].load (std::memory_order_relaxed));
}

bool FreezeShimmerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    const auto in  = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo() || in.isDisabled();
}

void FreezeShimmerProcessor::prepareToPlay (double sr, int block)
{
    sampleRateHz = sr;
    blockSize = block;
    shimmer.prepare (sr);
    freeze.prepare (sr);
    bypassMix = pEngage->load() > 0.5f ? 1.0f : 0.0f;
    lastFreezeParam = false;
    sustainDown = false;
}

void FreezeShimmerProcessor::setNormalised (const char* id, float v01)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
}

void FreezeShimmerProcessor::applyPreset (int index)
{
    index = juce::jlimit (0, fsp::kNumPresets - 1, index);
    const auto& p = fsp::kPresets[index];
    for (int i = 0; i < fsp::kNumKnobs; ++i) setNormalised (fsp::kKnobs[i].id, p.v[i]);
    if (auto* s = apvts.getParameter ("scene")) s->setValueNotifyingHost (s->convertTo0to1 ((float) p.scene));
    if (auto* m = apvts.getParameter ("fmode")) m->setValueNotifyingHost (m->convertTo0to1 ((float) p.fmode));
    setNormalised ("engage", 1.0f);
    releaseRequested.store (true);       // le thread audio coupera le freeze en cours
    presetIndex.store (index);
}

void FreezeShimmerProcessor::setCurrentProgram (int i) { applyPreset (i); }

void FreezeShimmerProcessor::applyCommand (int cmd)
{
    // appelé par le thread audio : l'engage passe par la file (anti-gel v2)
    auto engage = [this] (bool on) { queueNormalised (kQEngage, on ? 1.0f : 0.0f); };
    const int mode = (int) readP (pFMode);
    switch ((Command) cmd)
    {
        case cmdCapture:
            if (engageNow() < 0.5f) engage (true);
            freeze.capture();
            break;
        case cmdRelease:                       // double-clic footswitch / CC106 : arrête le freeze
            freeze.releaseAll();
            break;
        case cmdHoldOn:                        // footswitch maintenu : fige ce qui est entendu
            if (engageNow() < 0.5f) engage (true);
            if (mode == 1)      freeze.momentPress();
            else if (mode == 0) freeze.holdPress();
            else                freeze.capture();
            break;
        case cmdHoldOff:                       // relâché : LATCH garde le freeze, MOMENT le coupe
            if (mode == 1)      freeze.momentRelease();
            else                freeze.holdRelease();
            break;
        case cmdToggleAuto:
            autoArmedState.store (! autoArmedState.load());
            freeze.setAutoArmed (autoArmedState.load());
            if (engageNow() < 0.5f) engage (true);
            break;
        case cmdToggleEngage:
            engage (engageNow() < 0.5f);
            break;
        default: break;
    }
}

void FreezeShimmerProcessor::handleMidi (juce::MidiBuffer& midi)
{
    const int chanSel = (int) readP (pMidiChan);   // 0 = omni
    auto footswitch = [this] (bool down) { applyCommand (down ? cmdHoldOn : cmdHoldOff); };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (chanSel != 0 && m.getChannel() != chanSel) continue;

        if (m.isProgramChange())
        {
            pendingPreset.store (m.getProgramChangeNumber() % fsp::kNumPresets);   // chargé par le timer
            continue;
        }
        if (! m.isController()) continue;

        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        const float v01 = (float) val / 127.0f;
        bool handled = true;

        // MIDI Learn : le CC reçu est affecté à la commande visée (il n'agit pas sur le son)
        const int lt = learnTarget.load();
        if (lt >= 0 && lt < kNumLearn && cc < 120)
        {
            for (int j = 0; j < kNumLearn; ++j)
                if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);   // numéro déjà pris : l'autre commande est désassignée
            ccMap[lt].store (cc);
            lastLearned.store (lt);
            learnTarget.store (-1);
            lastMidiCc.store (cc);
            lastMidiValue.store (val);
            lastMidiStamp.fetch_add (1);
            continue;
        }

        int ctl = -1;
        for (int i = 0; i < kNumLearn; ++i)
            if (ccMap[i].load (std::memory_order_relaxed) == cc) { ctl = i; break; }

        if (ctl >= 0 && ctl < fsp::kNumKnobs) queueNormalised (ctl, v01);
        else if (ctl == kLearnScene)          queueNormalised (kQScene, (float) juce::jmin (2, val / 43) / 2.0f);
        else if (ctl == kLearnFMode)          queueNormalised (kQFMode, (float) juce::jmin (2, val / 43) / 2.0f);
        else if (ctl == kLearnEngage)         queueNormalised (kQEngage, val >= 64 ? 1.0f : 0.0f);
        else if (ctl == kLearnRelease)        { if (val >= 64) applyCommand (cmdRelease); }
        else if (ctl == kLearnFreeze || (ctl < 0 && cc == fsp::kCcSustain))   // pédale de sustain = footswitch (si CC64 libre)
        {
            const bool down = val >= 64;
            if (down != sustainDown) { sustainDown = down; footswitch (down); }
        }
        else handled = false;

        if (handled)
        {
            lastMidiCc.store (cc);
            lastMidiValue.store (val);
            lastMidiStamp.fetch_add (1);
        }
    }
}

void FreezeShimmerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, numSamples);

    handleMidi (midi);
    if (releaseRequested.exchange (false)) freeze.releaseAll();
    {
        const int n = fifo.getNumReady();
        if (n > 0)
        {
            const auto scope = fifo.read (n);
            for (int i = 0; i < scope.blockSize1; ++i) applyCommand (fifoData[(size_t) (scope.startIndex1 + i)]);
            for (int i = 0; i < scope.blockSize2; ++i) applyCommand (fifoData[(size_t) (scope.startIndex2 + i)]);
        }
    }

    // bool "freeze" (automation hôte / footswitch GUI en mode MOMENT)
    const bool fz = readP (pFreeze) > 0.5f;
    if (fz != lastFreezeParam)
    {
        lastFreezeParam = fz;
        applyCommand (fz ? cmdHoldOn : cmdHoldOff);
    }

    // ---- paramètres ----
    fs::ShimmerParams sp;
    sp.blend = readP (pBlend);  sp.rvbMix = readP (pRvb);  sp.time = readP (pTime);   sp.delay = readP (pDelay);
    sp.voice = readP (pVoice);  sp.depth = readP (pDepth); sp.tone = readP (pTone);   sp.rate = readP (pRate);
    sp.scene = juce::jlimit (0, 2, (int) readP (pScene));
    shimmer.setParams (sp);

    fs::FreezeParams fp;
    fp.speed = readP (pSpeed); fp.layer = readP (pLayer); fp.gliss = readP (pGliss);
    fp.mode = (fs::FreezeMode) juce::jlimit (0, 2, (int) readP (pFMode));
    freeze.setParams (fp);
    freeze.setAutoArmed (autoArmedState.load());

    const bool engaged = engageNow() > 0.5f;   // tient compte d'un engage encore en file (même bloc)
    if (! engaged && bypassMix > 0.5f) freeze.releaseAll();   // le freeze s'arrête au bypass (comme la pédale)

    const float dryLvl = readP (pDry), fxLvl = readP (pEffect);
    const float b = juce::jlimit (0.0f, 1.0f, sp.blend);
    const float dB = std::cos (b * juce::MathConstants<float>::halfPi);
    const float wB = std::sin (b * juce::MathConstants<float>::halfPi) * 1.1f;
    const float bpCoef = 1.0f - std::exp (-1.0f / (0.012f * (float) sampleRateHz));
    const float target = engaged ? 1.0f : 0.0f;

    auto* chL = buffer.getWritePointer (0);
    auto* chR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inRp = (numIn > 1) ? buffer.getReadPointer (1) : nullptr;

    float peakOut = 0.0f;
    std::array<float, (size_t) kChunk> xl, xr, sL, sR, envG;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int n = juce::jmin (kChunk, numSamples - start);
        float* l = chL + start;
        const float* r = inRp ? inRp + start : l;
        for (int i = 0; i < n; ++i) { xl[(size_t) i] = l[i]; xr[(size_t) i] = r[i]; }

        // entrée des moteurs : coupée en bypass (les queues continuent de sonner)
        for (int i = 0; i < n; ++i)
        {
            bypassMix += bpCoef * (target - bypassMix);
            const float g = bypassMix;
            shL[(size_t) i] = xl[(size_t) i] * g;
            shR[(size_t) i] = xr[(size_t) i] * g;
            frL[(size_t) i] = g;   // enveloppe temporaire (réutilise le tampon)
        }
        shimmer.process (shL.data(), shR.data(), wetL.data(), wetR.data(), n);

        // étage shimmer : BLEND dry/wet
        for (int i = 0; i < n; ++i)
        {
            envG[(size_t) i] = frL[(size_t) i];
            sL[(size_t) i] = shL[(size_t) i] * dB + wetL[(size_t) i] * wB;
            sR[(size_t) i] = shR[(size_t) i] * dB + wetR[(size_t) i] * wB;
        }
        // étage freeze : capture le signal post-shimmer
        freeze.process (sL.data(), sR.data(), frL.data(), frR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float g = envG[(size_t) i];
            float oL = g * (dryLvl * sL[(size_t) i]) + (1.0f - g) * (xl[(size_t) i] + wetL[(size_t) i] * wB) + fxLvl * frL[(size_t) i];
            float oR = g * (dryLvl * sR[(size_t) i]) + (1.0f - g) * (xr[(size_t) i] + wetR[(size_t) i] * wB) + fxLvl * frR[(size_t) i];
            oL = fs::softClip (oL);
            oR = fs::softClip (oR);
            if (chR) { l[i] = oL; chR[start + i] = oR; }
            else     { l[i] = 0.5f * (oL + oR); }
            peakOut = juce::jmax (peakOut, std::abs (oL), std::abs (oR));
        }
    }

    frozenState.store (freeze.isFrozen());
    outputLevel.store (peakOut);
}

juce::AudioProcessorEditor* FreezeShimmerProcessor::createEditor() { return new FreezeShimmerEditor (*this); }

void FreezeShimmerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiPage", uiPage.load(), nullptr);
    state.setProperty ("uiHeight", uiHeight.load(), nullptr);
    state.setProperty ("preset", presetIndex.load(), nullptr);
    for (int i = 0; i < kNumLearn; ++i)
        state.setProperty (juce::String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void FreezeShimmerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiPage.store ((int) vt.getProperty ("uiPage", 0));
            uiHeight.store (juce::jlimit (493, 1479, (int) vt.getProperty ("uiHeight", 493)));
            presetIndex.store (juce::jlimit (0, fsp::kNumPresets - 1, (int) vt.getProperty ("preset", 0)));
            // assignations MIDI Learn (ancien projet sans ces propriétés : CC fixes d'avant)
            for (int i = 0; i < kNumLearn; ++i)
                ccMap[i].store (juce::jlimit (-1, 127, (int) vt.getProperty (juce::String ("cc_") + learnDef (i).id, learnDef (i).defaultCC)));
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FreezeShimmerProcessor(); }
