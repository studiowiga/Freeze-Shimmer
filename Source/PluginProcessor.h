#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/ShimmerEngine.h"
#include "dsp/FreezeEngine.h"
#include <array>

class FreezeShimmerProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    FreezeShimmerProcessor();
    ~FreezeShimmerProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Freeze Shimmer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return fsp::kNumPresets; }
    int getCurrentProgram() override { return presetIndex.load(); }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override { return i >= 0 && i < fsp::kNumPresets ? juce::String (fsp::kPresets[i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- commandes envoyées par l'interface (thread UI -> thread audio) ----
    // cmdHoldOn / cmdHoldOff : footswitch enfoncé / relâché ; cmdRelease : arrêt du freeze (double-clic)
    enum Command { cmdNone = 0, cmdCapture, cmdRelease, cmdHoldOn, cmdHoldOff, cmdToggleAuto, cmdToggleEngage };
    void postCommand (Command c)
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0) fifoData[(size_t) scope.startIndex1] = (int) c;
        else if (scope.blockSize2 > 0) fifoData[(size_t) scope.startIndex2] = (int) c;
    }

    // ---- états lisibles par l'interface ----
    std::atomic<bool> frozenState { false };
    std::atomic<bool> autoArmedState { true };
    std::atomic<int>  lastMidiCc { -1 }, lastMidiValue { 0 };
    std::atomic<int>  lastMidiStamp { 0 };
    std::atomic<float> outputLevel { 0.0f };

    // page (A = primaire / B = secondaire "bleue") : purement visuel, conservé avec l'état
    std::atomic<int> uiPage { 0 };
    std::atomic<int> uiHeight { 493 };      // hauteur de la fenêtre (mémorisée dans le projet)
    std::atomic<int> presetIndex { 0 };
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

private:
    void handleMidi (juce::MidiBuffer& midi);
    void setNormalised (const char* id, float v01);
    void applyCommand (int cmd);

    // Anti-gel v2 (Studio Wiga) : valeurs demandées par le thread audio (MIDI, commandes du footswitch),
    // transmises à l'hôte par le timer du processeur (60 Hz, thread message)
    void timerCallback() override;
    static constexpr int kQScene = fsp::kNumKnobs, kQFMode = fsp::kNumKnobs + 1, kQEngage = fsp::kNumKnobs + 2;
    static constexpr int kNumQueued = fsp::kNumKnobs + 3;
    static const char* queuedId (int i);
    void queueNormalised (int index, float v01);
    float engageNow() const;           // valeur en file si elle existe, sinon la valeur courante
    std::atomic<float> pendingValue[kNumQueued];
    std::atomic<bool>  pendingSet[kNumQueued];
    std::atomic<int>   pendingPreset { -1 };

    fs::ShimmerEngine shimmer;
    fs::FreezeEngine  freeze;

    std::atomic<bool> releaseRequested { false };
    juce::AbstractFifo fifo { 32 };
    std::array<int, 32> fifoData {};
    std::atomic<float>* pDry, *pEffect, *pBlend, *pRvb, *pTime, *pDelay, *pSpeed, *pLayer, *pVoice, *pDepth, *pTone, *pRate, *pGliss;
    std::atomic<float>* pScene, *pFMode, *pFreeze, *pEngage, *pMidiChan;

    std::vector<float> wetL, wetR, shL, shR, frL, frR;
    float bypassMix = 1.0f;
    bool lastFreezeParam = false;
    bool sustainDown = false;
    double sampleRateHz = 48000.0;
    int blockSize = 512;
};
