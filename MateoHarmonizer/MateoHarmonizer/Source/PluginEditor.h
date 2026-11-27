#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModernLookAndFeel.h"

// Panel o wymiarach bazowych 564 x 556 (jak oryginal), skalowany w edytorze.
class FacePanel : public juce::Component
{
public:
    static constexpr int baseW = 564, baseH = 556;

    struct Text { juce::String text; juce::Rectangle<int> r; juce::Justification j; juce::Colour c; float size; };
    struct Box  { juce::Rectangle<int> r; };
    struct Dot  { juce::String text; juce::Point<int> p; juce::Colour c; };

    void addText (const juce::String& t, juce::Rectangle<int> r, juce::Colour c = skin::dim, float size = 9.0f,
                  juce::Justification j = juce::Justification::centred)
    { texts.push_back ({ t, r, j, c, size }); }
    void addBox (juce::Rectangle<int> r) { boxes.push_back ({ r }); }
    void addSectionName (const juce::String& t, juce::Point<int> p, juce::Colour c) { dots.push_back ({ t, p, c }); }

    void paint (juce::Graphics& g) override;

private:
    std::vector<Text> texts;
    std::vector<Box> boxes;
    std::vector<Dot> dots;
};

class MateoHarmonizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit MateoHarmonizerAudioProcessorEditor (MateoHarmonizerAudioProcessor&);
    ~MateoHarmonizerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Pokretlo: srodek (cx), gora (top), srednica galki (d)
    juce::Slider& addKnob (const juce::String& paramID, const juce::String& title, const juce::String& unit,
                           juce::Colour accent, bool bipolar, int cx, int top, int d = 38);
    juce::ToggleButton& addToggle (const juce::String& paramID, const juce::String& title, juce::Colour accent,
                                   int cx, int top, int titleTop);
    juce::ComboBox& addCombo (const juce::String& paramID, juce::Rectangle<int> r, juce::Colour textColour);

    void addVoiceKnobs (const juce::String& v, juce::Colour accent, int rowY);
    void refreshPresetUI();
    void presetButtonClicked (int index);

    MateoHarmonizerAudioProcessor& proc;
    ModernLookAndFeel lnf;
    FacePanel face;
    juce::TooltipWindow tooltipWindow { this, 700 };

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<SA> sliderAtts;
    juce::OwnedArray<BA> buttonAtts;
    juce::OwnedArray<CA> comboAtts;

    // presety
    juce::ComboBox hotkeys;
    juce::ToggleButton editButton;
    std::array<juce::Label, 4> presetNames;
    std::array<juce::TextButton, 4> presetButtons;
    std::array<Led, 4> presetLeds;

    // miernik wejscia: 0, -6, -12 dB
    std::array<Led, 3> inputLeds { Led (skin::ledRed), Led (skin::ledYellow), Led (skin::ledGreen) };
    std::array<float, 3> ledHold { 0, 0, 0 };

    juce::ComboBox* keyCombo = nullptr;
    juce::ToggleButton* linkButton = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MateoHarmonizerAudioProcessorEditor)
};
