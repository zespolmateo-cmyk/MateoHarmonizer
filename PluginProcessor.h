#pragma once

#include <JuceHeader.h>
#include "dsp/MateoEngine.h"

namespace ids
{
    inline const juce::String inputLevel { "inputLevel" }, link { "link" }, key { "key" }, midiCh { "midiCh" }, tune { "tune" };
    inline const juce::String directOn { "directOn" }, directLevel { "directLevel" }, directPan { "directPan" };

    // prefiks glosu: "uni" (unison) albo "h1".."h4"
    inline juce::String on (const juce::String& v)       { return v + "On"; }
    inline juce::String scale (const juce::String& v)    { return v + "Scale"; }
    inline juce::String dual (const juce::String& v)     { return v + "Dual"; }
    inline juce::String random (const juce::String& v)   { return v + "Random"; }
    inline juce::String formant (const juce::String& v)  { return v + "Formant"; }
    inline juce::String quantise (const juce::String& v) { return v + "Quantise"; }
    inline juce::String level (const juce::String& v)    { return v + "Level"; }
    inline juce::String pan (const juce::String& v)      { return v + "Pan"; }
}

class MateoHarmonizerAudioProcessor : public juce::AudioProcessor,
                                      private juce::AsyncUpdater
{
public:
    MateoHarmonizerAudioProcessor();
    ~MateoHarmonizerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mateo Harmonizer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    // programy hosta = 4 presety
    int getNumPrograms() override { return 4; }
    int getCurrentProgram() override { return activePreset.load(); }
    void setCurrentProgram (int index) override { recallPreset (index); }
    const juce::String getProgramName (int index) override { return getPresetName (index); }
    void changeProgramName (int index, const juce::String& newName) override { setPresetName (index, newName); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- presety 1-4 ----
    void recallPreset (int index);
    void storePreset (int index);
    juce::String getPresetName (int index) const;
    void setPresetName (int index, const juce::String& name);
    std::atomic<int> activePreset { 0 };
    std::atomic<bool> editMode { false };
    std::atomic<int> hotkeyMode { 0 }; // 0 = klawisze 1-4, 1 = F1-F4, 2 = MIDI Program Change, 3 = wylaczone

    // ---- do wyswietlania ----
    float takeInputPeakDb();
    int getEffectiveKey() const { return effectiveKey.load(); }
    float getDetectedHz() const { return detectedHz.load(); }

    static juce::StringArray keyNames();

private:
    void handleAsyncUpdate() override;
    void readParams (mateo::EngineParams& p) const;

    mateo::HarmonizerEngine engine;

    struct VoicePtrs
    {
        std::atomic<float>* on = nullptr; std::atomic<float>* scale = nullptr; std::atomic<float>* dual = nullptr;
        std::atomic<float>* random = nullptr; std::atomic<float>* formant = nullptr; std::atomic<float>* quantise = nullptr;
        std::atomic<float>* level = nullptr; std::atomic<float>* pan = nullptr;
    };
    std::atomic<float>* pInput = nullptr; std::atomic<float>* pLink = nullptr; std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pMidiCh = nullptr; std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pDirOn = nullptr; std::atomic<float>* pDirLevel = nullptr; std::atomic<float>* pDirPan = nullptr;
    VoicePtrs uni;
    std::array<VoicePtrs, 4> harm;

    std::array<juce::ValueTree, 4> presetStates;
    std::array<juce::String, 4> presetNames { "Init", "Init", "Init", "Init" };
    juce::CriticalSection presetLock;

    std::atomic<int> pendingPreset { -1 };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<int> effectiveKey { 0 };
    std::atomic<float> detectedHz { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MateoHarmonizerAudioProcessor)
};
