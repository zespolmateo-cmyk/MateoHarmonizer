// Nowoczesna skorka Mateo Harmonizer
#pragma once
#include <JuceHeader.h>

namespace skin
{
    const juce::Colour bg        { 0xff0e1014 };
    const juce::Colour panel     { 0xff181b21 };
    const juce::Colour panelTop  { 0xff1e2229 };
    const juce::Colour border    { 0xff2a2f38 };
    const juce::Colour text      { 0xffd5dbe4 };
    const juce::Colour dim       { 0xff7c8695 };
    const juce::Colour lcd       { 0xff0a0c0f };
    const juce::Colour accent    { 0xff38bdf8 }; // globalny (cyjan)
    const juce::Colour direct    { 0xffe5e7eb };
    const juce::Colour unison    { 0xffa78bfa };
    const juce::Colour harm[4]   { juce::Colour (0xff4f8cff), juce::Colour (0xfff5c542), juce::Colour (0xff34d399), juce::Colour (0xfffb923c) };
    const juce::Colour ledRed    { 0xffff4d4d };
    const juce::Colour ledYellow { 0xfffacc15 };
    const juce::Colour ledGreen  { 0xff22c55e };

    inline juce::Font font (float h, bool bold = true) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
}

class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        using namespace juce;
        setColour (Slider::textBoxBackgroundColourId, skin::lcd);
        setColour (Slider::textBoxOutlineColourId, skin::border);
        setColour (Slider::textBoxTextColourId, skin::text);
        setColour (Slider::textBoxHighlightColourId, skin::accent.withAlpha (0.4f));
        setColour (ComboBox::backgroundColourId, skin::lcd);
        setColour (ComboBox::outlineColourId, skin::border);
        setColour (ComboBox::textColourId, skin::accent);
        setColour (ComboBox::arrowColourId, skin::dim);
        setColour (PopupMenu::backgroundColourId, Colour (0xff15181d));
        setColour (PopupMenu::textColourId, skin::text);
        setColour (PopupMenu::highlightedBackgroundColourId, skin::accent.withAlpha (0.25f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (Label::textColourId, skin::text);
        setColour (Label::textWhenEditingColourId, skin::text);
        setColour (Label::backgroundWhenEditingColourId, skin::lcd);
        setColour (Label::outlineWhenEditingColourId, skin::accent);
        setColour (TextEditor::backgroundColourId, skin::lcd);
        setColour (TextEditor::textColourId, skin::text);
        setColour (TextEditor::highlightColourId, skin::accent.withAlpha (0.35f));
        setColour (TextEditor::focusedOutlineColourId, skin::accent);
        setColour (CaretComponent::caretColourId, skin::accent);
        setColour (TextButton::buttonColourId, skin::panelTop);
    }

    // ---------------------------------------------------------- pokretla
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto accent = s.findColour (Slider::rotarySliderFillColourId);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float size = jmin (bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre (size, size);
        const auto c = bounds.getCentre();
        const float r = size * 0.5f;
        const float trackW = jmax (2.5f, r * 0.13f);
        const float arcR = r - trackW * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // tor
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (skin::border.brighter (0.15f));
        g.strokePath (track, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));

        // wartosc
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
            g.setColour (accent.withAlpha (0.25f));
            g.strokePath (val, PathStrokeType (trackW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (val, PathStrokeType (trackW, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // galka
        const float kr = r - trackW * 2.0f;
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - kr, c.y - kr + 1.5f, kr * 2.0f, kr * 2.0f);
        ColourGradient grad (Colour (0xff2f343d), c.x, c.y - kr, Colour (0xff16191e), c.x, c.y + kr, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - kr + 0.5f, c.y - kr + 0.5f, kr * 2.0f - 1.0f, kr * 2.0f - 1.0f, 1.0f);

        // wskaznik
        Path ptr;
        const float pw = jmax (2.0f, kr * 0.14f);
        ptr.addRoundedRectangle (-pw * 0.5f, -kr + 2.5f, pw, kr * 0.45f, pw * 0.5f);
        g.setColour (s.isEnabled() ? Colours::white.withAlpha (0.92f) : skin::dim);
        g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (skin::font (10.5f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        juce::Slider::SliderLayout layout;
        auto b = s.getLocalBounds();
        const int tbH = 14, tbW = juce::jmin (40, b.getWidth());
        layout.textBoxBounds = juce::Rectangle<int> (0, 0, tbW, tbH).withCentre ({ b.getCentreX(), b.getBottom() - tbH / 2 });
        layout.sliderBounds  = b.withTrimmedBottom (tbH + 2);
        return layout;
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited())
        {
            auto r = l.getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (skin::lcd);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (skin::border);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (l.findColour (juce::Label::textColourId));
            g.setFont (skin::font (10.5f));
            g.drawFittedText (l.getText(), l.getLocalBounds(), juce::Justification::centred, 1);
            return;
        }
        LookAndFeel_V4::drawLabel (g, l);
    }

    // ---------------------------------------------------------- przyciski ON/OFF
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        using namespace juce;
        const auto accent = b.findColour (ToggleButton::tickColourId);
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();

        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), 5.0f);

        ColourGradient grad (on ? accent.withAlpha (0.95f) : Colour (0xff2b3038), r.getX(), r.getY(),
                             on ? accent.darker (0.55f) : Colour (0xff1a1d22), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.0f);

        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (1.0f), 6.0f, 2.0f);
        }
        g.setColour (Colours::white.withAlpha (over ? 0.25f : 0.10f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        // dioda w srodku
        auto led = r.withSizeKeepingCentre (r.getWidth() * 0.42f, 3.0f).withY (r.getY() + r.getHeight() * 0.28f);
        g.setColour (on ? Colours::white.withAlpha (0.9f) : Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (led, 1.5f);
        if (down) { g.setColour (Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r, 5.0f); }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), 5.0f);
        ColourGradient grad (down ? Colour (0xff1a1d22) : Colour (0xff30353e), r.getX(), r.getY(),
                             down ? Colour (0xff2b3038) : Colour (0xff1b1e23), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (over ? 0.25f : 0.10f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    // ---------------------------------------------------------- listy (SCALE, KEY ...)
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (skin::lcd);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (false) ? skin::accent : skin::border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        if (box.isEnabled() && w > 60)
        {
            Path arrow;
            const float ax = (float) w - 9.0f, ay = (float) h * 0.5f;
            arrow.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
            g.setColour (skin::dim);
            g.fillPath (arrow);
        }
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return skin::font (11.0f); }
    juce::Font getPopupMenuFont() override { return skin::font (13.0f, false); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        const bool small = box.getWidth() <= 60;
        label.setBounds (small ? 1 : 4, 1, box.getWidth() - (small ? 2 : 18), box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centred);
    }
};

// Dioda LED (presety, miernik wejscia)
class Led : public juce::Component
{
public:
    explicit Led (juce::Colour c = skin::accent) : colour (c) { setInterceptsMouseClicks (false, false); }
    void setOn (float level) { if (std::abs (level - amount) > 0.01f) { amount = level; repaint(); } }
    void setColour (juce::Colour c) { colour = c; repaint(); }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (amount > 0.0f)
        {
            g.setColour (colour.withAlpha (0.35f * amount));
            g.fillEllipse (r.expanded (1.0f));
        }
        g.setColour (amount > 0.0f ? colour.withMultipliedBrightness (0.6f + 0.4f * amount) : Colour (0xff2a2f38));
        g.fillEllipse (r.reduced (1.0f));
        g.setColour (Colours::white.withAlpha (amount > 0 ? 0.5f : 0.08f));
        g.fillEllipse (r.reduced (r.getWidth() * 0.32f).translated (-r.getWidth() * 0.1f, -r.getHeight() * 0.1f));
    }
private:
    juce::Colour colour;
    float amount = 0.0f;
};
