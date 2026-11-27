#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    String dbText (float v, int)   { if (v <= -59.9f) return "-inf"; return (v >= 0 ? "+" : "") + String (v, 1); }
    String panText (float v, int)  { const int i = roundToInt (v); return i == 0 ? "0" : (i < 0 ? "L" + String (-i) : "R" + String (i)); }
    String intText (float v, int)  { return String (roundToInt (v)); }
    String sgnText (float v, int)  { const int i = roundToInt (v); return i > 0 ? "+" + String (i) : String (i); }

    std::unique_ptr<AudioParameterFloat> fparam (const String& id, const String& name, float lo, float hi, float step, float def,
                                                 String (*fn) (float, int), const String& label = {})
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (fn).withLabel (label));
    }
}

StringArray MateoHarmonizerAudioProcessor::keyNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

AudioProcessorValueTreeState::ParameterLayout MateoHarmonizerAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // --- sekcja globalna ---
    layout.add (fparam (ids::inputLevel, "Input Level", -24.0f, 24.0f, 0.5f, 0.0f, intText, "dB"));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::link, 1 }, "Link", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::key, 1 }, "Key", keyNames(), 0));
    StringArray ch { "Omni" };
    for (int i = 1; i <= 16; ++i) ch.add (String (i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::midiCh, 1 }, "MIDI Ch.", ch, 1));
    layout.add (fparam (ids::tune, "Tune", 430.0f, 450.0f, 1.0f, 440.0f, intText, "Hz"));

    // --- direct ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::directOn, 1 }, "Direct On/Off", true));
    layout.add (fparam (ids::directLevel, "Direct Level", -60.0f, 6.0f, 0.1f, 0.0f, dbText, "dB"));
    layout.add (fparam (ids::directPan, "Direct Pan", -50.0f, 50.0f, 1.0f, 0.0f, panText));

    StringArray scales;
    for (auto& s : mateo::getScales()) scales.add (s.name);
    const int defaultScale[4] = { 0, 26, 2, 21 }; // MIDI, User 2, Low Octave, High 5th Harmonic Minor

    auto addVoice = [&] (const String& v, const String& name, bool isHarm, int idx)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::on (v), 1 }, name + " On/Off", false));
        if (isHarm)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::scale (v), 1 }, name + " Scale", scales, defaultScale[idx]));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::dual (v), 1 }, name + " Dual On/Off", false));
        }
        layout.add (fparam (ids::random (v), name + " Random", 0.0f, 100.0f, 1.0f, 0.0f, intText));
        layout.add (fparam (ids::formant (v), name + " Formant", -50.0f, 50.0f, 1.0f, 0.0f, sgnText));
        layout.add (fparam (ids::quantise (v), name + " Quantise", 0.0f, 100.0f, 1.0f, 0.0f, intText));
        layout.add (fparam (ids::level (v), name + " Level", -60.0f, 6.0f, 0.1f, 0.0f, dbText, "dB"));
        layout.add (fparam (ids::pan (v), name + " Pan", -50.0f, 50.0f, 1.0f, 0.0f, panText));
    };
    addVoice ("uni", "Unison", false, 0);
    for (int h = 0; h < 4; ++h)
        addVoice ("h" + String (h + 1), "Harmony " + String (h + 1), true, h);

    return layout;
}

MateoHarmonizerAudioProcessor::MateoHarmonizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MateoHarmonizer", createLayout())
{
    pInput = apvts.getRawParameterValue (ids::inputLevel);
    pLink  = apvts.getRawParameterValue (ids::link);
    pKey   = apvts.getRawParameterValue (ids::key);
    pMidiCh = apvts.getRawParameterValue (ids::midiCh);
    pTune  = apvts.getRawParameterValue (ids::tune);
    pDirOn = apvts.getRawParameterValue (ids::directOn);
    pDirLevel = apvts.getRawParameterValue (ids::directLevel);
    pDirPan = apvts.getRawParameterValue (ids::directPan);

    auto fill = [this] (VoicePtrs& vp, const String& v, bool isHarm)
    {
        vp.on = apvts.getRawParameterValue (ids::on (v));
        if (isHarm)
        {
            vp.scale = apvts.getRawParameterValue (ids::scale (v));
            vp.dual  = apvts.getRawParameterValue (ids::dual (v));
        }
        vp.random   = apvts.getRawParameterValue (ids::random (v));
        vp.formant  = apvts.getRawParameterValue (ids::formant (v));
        vp.quantise = apvts.getRawParameterValue (ids::quantise (v));
        vp.level    = apvts.getRawParameterValue (ids::level (v));
        vp.pan      = apvts.getRawParameterValue (ids::pan (v));
    };
    fill (uni, "uni", false);
    for (int h = 0; h < 4; ++h) fill (harm[(size_t) h], "h" + String (h + 1), true);

    for (auto& s : presetStates) s = apvts.copyState();
}

bool MateoHarmonizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void MateoHarmonizerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatency());
}

void MateoHarmonizerAudioProcessor::readParams (mateo::EngineParams& p) const
{
    p.inputDb   = pInput->load();
    p.link      = pLink->load() > 0.5f;
    p.key       = roundToInt (pKey->load());
    p.midiCh    = roundToInt (pMidiCh->load());
    p.tune      = pTune->load();
    p.directOn  = pDirOn->load() > 0.5f;
    p.directDb  = pDirLevel->load();
    p.directPan = pDirPan->load();

    auto get = [] (const VoicePtrs& s, mateo::VoiceParams& d)
    {
        d.on       = s.on->load() > 0.5f;
        d.scale    = s.scale != nullptr ? roundToInt (s.scale->load()) : 0;
        d.dual     = s.dual != nullptr && s.dual->load() > 0.5f;
        d.random   = s.random->load();
        d.formant  = s.formant->load();
        d.quantise = s.quantise->load();
        d.levelDb  = s.level->load();
        d.pan      = s.pan->load();
    };
    get (uni, p.unison);
    for (size_t h = 0; h < 4; ++h) get (harm[h], p.harm[h]);
}

void MateoHarmonizerAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    mateo::EngineParams p;
    readParams (p);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())                  engine.noteOn (m.getNoteNumber(), m.getChannel(), p);
        else if (m.isNoteOff())            engine.noteOff (m.getNoteNumber(), m.getChannel(), p);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isProgramChange() && hotkeyMode.load() == 2 && m.getProgramChangeNumber() < 4
                 && (p.midiCh == 0 || m.getChannel() == p.midiCh))
        {
            pendingPreset = m.getProgramChangeNumber();
            triggerAsyncUpdate();
        }
    }

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = jmax (numIn, 2); c < numOut; ++c) buffer.clear (c, 0, n);

    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (L, R, n, p);

    inputPeak = jmax (inputPeak.load(), engine.takeInputPeak());
    effectiveKey = engine.getEffectiveKey();
    detectedHz = engine.getDetectedHz();
}

float MateoHarmonizerAudioProcessor::takeInputPeakDb()
{
    const float v = inputPeak.exchange (0.0f);
    return Decibels::gainToDecibels (v, -100.0f);
}

// ---------------------------------------------------------------- presety
void MateoHarmonizerAudioProcessor::recallPreset (int index)
{
    if (! isPositiveAndBelow (index, 4)) return;
    ValueTree s;
    {
        const ScopedLock sl (presetLock);
        s = presetStates[(size_t) index].createCopy();
    }
    if (s.isValid())
    {
        for (auto* param : getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (param))
            {
                auto child = s.getChildWithProperty ("id", rp->getParameterID());
                if (child.isValid() && child.hasProperty ("value"))
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
            }
    }
    activePreset = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void MateoHarmonizerAudioProcessor::storePreset (int index)
{
    if (! isPositiveAndBelow (index, 4)) return;
    const ScopedLock sl (presetLock);
    presetStates[(size_t) index] = apvts.copyState();
    activePreset = index;
}

String MateoHarmonizerAudioProcessor::getPresetName (int index) const
{
    if (! isPositiveAndBelow (index, 4)) return {};
    const ScopedLock sl (presetLock);
    return presetNames[(size_t) index];
}

void MateoHarmonizerAudioProcessor::setPresetName (int index, const String& name)
{
    if (! isPositiveAndBelow (index, 4)) return;
    {
        const ScopedLock sl (presetLock);
        presetNames[(size_t) index] = name.isEmpty() ? String ("Init") : name.substring (0, 24);
    }
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void MateoHarmonizerAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) recallPreset (p);
}

// ---------------------------------------------------------------- stan
void MateoHarmonizerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    ValueTree presets ("MateoPresets");
    presets.setProperty ("active", activePreset.load(), nullptr);
    presets.setProperty ("hotkeys", hotkeyMode.load(), nullptr);
    {
        const ScopedLock sl (presetLock);
        for (int i = 0; i < 4; ++i)
        {
            ValueTree slot ("Slot");
            slot.setProperty ("name", presetNames[(size_t) i], nullptr);
            slot.appendChild (presetStates[(size_t) i].createCopy(), nullptr);
            presets.appendChild (slot, nullptr);
        }
    }
    state.appendChild (presets, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MateoHarmonizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    auto presets = state.getChildWithName ("MateoPresets");
    if (presets.isValid())
    {
        state.removeChild (presets, nullptr);
        const ScopedLock sl (presetLock);
        for (int i = 0; i < jmin (4, presets.getNumChildren()); ++i)
        {
            auto slot = presets.getChild (i);
            presetNames[(size_t) i] = slot.getProperty ("name", "Init").toString();
            if (slot.getNumChildren() > 0) presetStates[(size_t) i] = slot.getChild (0).createCopy();
        }
        activePreset = (int) presets.getProperty ("active", 0);
        hotkeyMode   = (int) presets.getProperty ("hotkeys", 0);
    }
    apvts.replaceState (state);
}

AudioProcessorEditor* MateoHarmonizerAudioProcessor::createEditor()
{
    return new MateoHarmonizerAudioProcessorEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MateoHarmonizerAudioProcessor();
}
