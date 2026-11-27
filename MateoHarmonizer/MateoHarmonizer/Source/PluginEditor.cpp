#include "PluginEditor.h"

using namespace juce;

// Polskie znaki: literaly w UTF-8
static String pl (const char* s) { return String::fromUTF8 (s); }

// =============================================================================
//  FacePanel - tlo, panele, napisy
// =============================================================================
void FacePanel::paint (Graphics& g)
{
    // tlo
    ColourGradient bgGrad (Colour (0xff15181d), 0, 0, skin::bg, 0, (float) baseH, false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // panele
    for (auto& b : boxes)
    {
        auto r = b.r.toFloat();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 2), 8.0f);
        ColourGradient pg (skin::panelTop, 0, r.getY(), skin::panel, 0, r.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (skin::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.04f));
        g.drawLine (r.getX() + 8, r.getY() + 1.5f, r.getRight() - 8, r.getY() + 1.5f, 1.0f);
    }

    // logo
    {
        auto r = Rectangle<float> (256, 4, 304, 34);
        g.setFont (skin::font (22.0f));
        g.setColour (Colours::white);
        const String mateo ("Mateo");
        GlyphArrangement ga;
        ga.addLineOfText (skin::font (22.0f), mateo, 0, 0);
        const float mateoW = ga.getBoundingBox (0, -1, true).getWidth();

        const String harm ("HARMONIZER");
        const float x0 = r.getRight() - 12 - mateoW - 8 - 112;
        g.drawText (mateo, Rectangle<float> (x0, r.getY(), mateoW + 4, r.getHeight()), Justification::centredLeft);

        g.setColour (skin::accent);
        g.setFont (skin::font (12.5f));
        float x = x0 + mateoW + 8;
        for (auto ch : harm)
        {
            const String s = String::charToString (ch);
            g.drawText (s, Rectangle<float> (x, r.getY() + 3, 12, r.getHeight() - 3), Justification::centred);
            x += 11.2f;
        }
        // pasek akcentu (kolory sekcji)
        const Colour cols[6] = { skin::direct, skin::unison, skin::harm[0], skin::harm[1], skin::harm[2], skin::harm[3] };
        const float bw = 11.0f;
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (cols[i].withAlpha (0.9f));
            g.fillRoundedRectangle (r.getX() + 12 + i * (bw + 4), r.getCentreY() - 1.5f, bw, 3.0f, 1.5f);
        }
    }

    // napisy
    for (auto& t : texts)
    {
        g.setColour (t.c);
        g.setFont (skin::font (t.size));
        g.drawFittedText (t.text, t.r, t.j, 2, 1.0f);
    }

    // nazwy sekcji z kolorowa kropka
    for (auto& d : dots)
    {
        g.setColour (d.c.withAlpha (0.35f));
        g.fillEllipse ((float) d.p.x - 1, (float) d.p.y + 1, 10, 10);
        g.setColour (d.c);
        g.fillEllipse ((float) d.p.x + 1, (float) d.p.y + 3, 6, 6);
        g.setColour (skin::text);
        g.setFont (skin::font (11.0f));
        g.drawText (d.text, d.p.x + 13, d.p.y, 140, 12, Justification::centredLeft);
    }
}

// =============================================================================
//  Edytor
// =============================================================================
namespace
{
    // Pozycje X pokretel w rzedach UNISON i HARMONY (jak w oryginale)
    constexpr int xRandom = 270, xFormant = 330, xQuantise = 390, xLevel = 462, xPan = 524;
    constexpr int rowDU = 124;                              // rzad DIRECT / UNISON
    inline int rowH (int i) { return 212 + i * 86; }        // rzedy HARMONY 1..4
}

MateoHarmonizerAudioProcessorEditor::MateoHarmonizerAudioProcessorEditor (MateoHarmonizerAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (face);
    face.setLookAndFeel (&lnf);

    const Colour lbl = skin::dim;

    // ------------------------------------------------------------ panele
    face.addBox ({ 4, 4, 248, 34 });     // hotkeys + edit
    face.addBox ({ 256, 4, 304, 34 });   // logo
    face.addBox ({ 4, 42, 329, 78 });    // presety
    face.addBox ({ 337, 42, 223, 78 });  // link / input / key / midi / tune
    face.addBox ({ 4, rowDU, 182, 84 }); // direct
    face.addBox ({ 190, rowDU, 370, 84 });
    for (int i = 0; i < 4; ++i) face.addBox ({ 4, rowH (i), 556, 82 });

    // ------------------------------------------------------------ PRESET HOTKEYS + EDIT
    face.addText ("PRESET HOTKEYS", { 12, 6, 114, 10 }, lbl, 9.0f);
    hotkeys.addItem ("1 - 2 - 3 - 4", 1);
    hotkeys.addItem ("F1 - F2 - F3 - F4", 2);
    hotkeys.addItem ("MIDI Prog. Change", 3);
    hotkeys.addItem ("Off", 4);
    hotkeys.setSelectedId (proc.hotkeyMode.load() + 1, dontSendNotification);
    hotkeys.onChange = [this] { proc.hotkeyMode = hotkeys.getSelectedId() - 1; };
    hotkeys.setTooltip (pl ("Jak przełączać presety 1–4: klawisze 1–4, F1–F4 albo komunikat MIDI Program Change (1–4)"));
    hotkeys.setBounds (12, 18, 114, 16);
    face.addAndMakeVisible (hotkeys);

    face.addText ("EDIT", { 168, 14, 38, 10 }, lbl, 9.0f, Justification::centredRight);
    editButton.setColour (ToggleButton::tickColourId, skin::ledRed);
    editButton.setBounds (212, 9, 24, 24);
    editButton.setToggleState (proc.editMode.load(), dontSendNotification);
    editButton.onClick = [this] { proc.editMode = editButton.getToggleState(); refreshPresetUI(); };
    editButton.setTooltip (pl ("EDIT: gdy włączony, przycisk presetu ZAPISUJE bieżące ustawienia, a nazwę presetu można zmienić, klikając w nią"));
    face.addAndMakeVisible (editButton);

    // ------------------------------------------------------------ PRESET #1..#4
    for (int i = 0; i < 4; ++i)
    {
        const int col = i % 2, row = i / 2;
        const int nx = col == 0 ? 12 : 176;
        const int ty = row == 0 ? 46 : 82;
        const int bx = col == 0 ? 144 : 308;

        face.addText ("PRESET #" + String (i + 1), { nx, ty, 124, 10 }, lbl, 9.0f);

        auto& nm = presetNames[(size_t) i];
        nm.setText (proc.getPresetName (i), dontSendNotification);
        nm.setFont (skin::font (11.0f));
        nm.setJustificationType (Justification::centred);
        nm.setColour (Label::backgroundColourId, skin::lcd);
        nm.setColour (Label::outlineColourId, skin::border);
        nm.setColour (Label::textColourId, skin::accent);
        nm.onTextChange = [this, i] { proc.setPresetName (i, presetNames[(size_t) i].getText()); };
        nm.setBounds (nx, ty + 12, 124, 18);
        nm.setTooltip (pl ("Nazwa presetu #") + String (i + 1) + pl (" (zmiana nazwy w trybie EDIT)"));
        face.addAndMakeVisible (nm);

        auto& led = presetLeds[(size_t) i];
        led.setColour (skin::accent);
        led.setBounds (bx + 6, ty + 1, 10, 10);
        face.addAndMakeVisible (led);

        auto& b = presetButtons[(size_t) i];
        b.setBounds (bx, ty + 14, 22, 22);
        b.onClick = [this, i] { presetButtonClicked (i); };
        b.setTooltip ("Preset #" + String (i + 1) + pl (": wczytaj (w trybie EDIT: zapisz)"));
        face.addAndMakeVisible (b);
    }

    // ------------------------------------------------------------ LINK / INPUT / KEY / MIDI / TUNE
    linkButton = &addToggle (ids::link, "LINK", skin::accent, 358, 64, 46);
    linkButton->setTooltip (pl ("LINK: tonacja (KEY) automatycznie podąża za ostatnią nutą zagraną z MIDI"));

    auto& in = addKnob (ids::inputLevel, "INPUT LEVEL", "dB", skin::accent, true, 403, 57, 30);
    in.setTooltip (pl ("Poziom sygnału wejściowego (−24…+24 dB). Diody pokazują wysterowanie: −12 / −6 / 0 dB"));

    const int ledY[3] = { 60, 75, 90 };
    const char* ledTxt[3] = { "0", "-6", "-12" };
    for (int i = 0; i < 3; ++i)
    {
        inputLeds[(size_t) i].setBounds (430, ledY[i], 11, 11);
        face.addAndMakeVisible (inputLeds[(size_t) i]);
        face.addText (ledTxt[i], { 443, ledY[i], 20, 11 }, lbl, 9.0f, Justification::centredLeft);
    }

    face.addText ("KEY", { 466, 46, 46, 10 }, lbl, 9.0f);
    keyCombo = &addCombo (ids::key, { 467, 58, 44, 17 }, skin::accent);
    keyCombo->setTooltip (pl ("KEY: tonacja dla skal diatonicznych (High/Low 3rd, 4th, 5th, 6th…)"));
    face.addText ("MIDI CH.", { 466, 80, 46, 10 }, lbl, 9.0f);
    auto& mc = addCombo (ids::midiCh, { 467, 92, 44, 17 }, skin::accent);
    mc.setTooltip (pl ("Kanał MIDI dla nut (skala MIDI) i zmiany presetów; Omni = wszystkie kanały"));

    auto& tn = addKnob (ids::tune, "TUNE", "Hz", skin::accent, true, 536, 57, 30);
    tn.setTooltip (pl ("Strojenie: częstotliwość dźwięku A4 (430–450 Hz)"));

    // ------------------------------------------------------------ DIRECT
    {
        const int y = rowDU;
        auto& lv = addKnob (ids::directLevel, "LEVEL (dB)", {}, skin::direct, false, 40, y + 18, 36);
        lv.setTooltip (pl ("Głośność sygnału oryginalnego (suchego)"));
        auto& pn = addKnob (ids::directPan, "PAN", {}, skin::direct, true, 98, y + 18, 36);
        pn.setTooltip (pl ("Panorama sygnału oryginalnego"));
        auto& on = addToggle (ids::directOn, "DIRECT\nON/OFF", skin::direct, 156, y + 34, y + 6);
        on.setTooltip (pl ("Włącza / wyłącza sygnał oryginalny"));
        face.addSectionName ("DIRECT", { 12, y + 68 }, skin::direct);
    }

    // ------------------------------------------------------------ UNISON
    {
        const int y = rowDU;
        auto& on = addToggle (ids::on ("uni"), "UNISON\nON/OFF", skin::unison, 217, y + 34, y + 6);
        on.setTooltip (pl ("Głos UNISON: zdublowany głos na tej samej wysokości (z korekcją QUANTISE działa jak autotune)"));
        addVoiceKnobs ("uni", skin::unison, y);
        face.addSectionName ("UNISON", { 198, y + 68 }, skin::unison);
    }

    // ------------------------------------------------------------ HARMONY 1..4
    for (int i = 0; i < 4; ++i)
    {
        const int y = rowH (i);
        const String v = "h" + String (i + 1);
        const Colour c = skin::harm[i];

        face.addText ("SCALE", { 12, y + 24, 140, 10 }, lbl, 9.0f, Justification::centredLeft);
        auto& sc = addCombo (ids::scale (v), { 12, y + 36, 140, 18 }, c);
        sc.setTooltip (pl ("SCALE: interwał harmonii. MIDI = nuta z klawiatury MIDI; High/Low = w górę/w dół w tonacji KEY; User = stały interwał"));

        auto& on = addToggle (ids::on (v), "HARMONY\nON/OFF", c, 172, y + 32, y + 6);
        on.setTooltip (pl ("Włącza / wyłącza głos HARMONY ") + String (i + 1));
        auto& du = addToggle (ids::dual (v), "DUAL\nON/OFF", c, 214, y + 32, y + 6);
        du.setTooltip (pl ("DUAL: dodaje drugi, lekko rozstrojony głos tej harmonii (szerokie stereo)"));

        addVoiceKnobs (v, c, y);
        face.addSectionName ("HARMONY " + String (i + 1), { 12, y + 64 }, c);
    }

    // ------------------------------------------------------------ okno
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) FacePanel::baseW / FacePanel::baseH);
    setResizeLimits (FacePanel::baseW * 3 / 4, FacePanel::baseH * 3 / 4, FacePanel::baseW * 2, FacePanel::baseH * 2);
    setSize (roundToInt (FacePanel::baseW * 1.35), roundToInt (FacePanel::baseH * 1.35));

    refreshPresetUI();
    startTimerHz (30);
}

MateoHarmonizerAudioProcessorEditor::~MateoHarmonizerAudioProcessorEditor()
{
    stopTimer();
    sliderAtts.clear();
    buttonAtts.clear();
    comboAtts.clear();
    face.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MateoHarmonizerAudioProcessorEditor::addVoiceKnobs (const String& v, Colour c, int y)
{
    auto& r = addKnob (ids::random (v), "RANDOM", {}, c, false, xRandom, y + 18);
    r.setTooltip (pl ("RANDOM: losowe, naturalne wahania wysokości (efekt „żywego” chórku)"));
    auto& f = addKnob (ids::formant (v), "FORMANT", {}, c, true, xFormant, y + 18);
    f.setTooltip (pl ("FORMANT: barwa głosu. M = ciemniejsza (męska), F = jaśniejsza (żeńska)"));
    face.addText ("M", { xFormant - 30, y + 55, 10, 12 }, skin::dim, 9.5f);
    face.addText ("F", { xFormant + 21, y + 55, 10, 12 }, skin::dim, 9.5f);
    auto& q = addKnob (ids::quantise (v), "QUANTISE", {}, c, false, xQuantise, y + 18);
    q.setTooltip (pl ("QUANTISE: siła korekcji do dokładnej nuty (0 = naturalnie, 100 = idealnie w nutę)"));
    auto& l = addKnob (ids::level (v), "LEVEL", "dB", c, false, xLevel, y + 18);
    l.setTooltip (pl ("Głośność tego głosu (dB)"));
    auto& p = addKnob (ids::pan (v), "PAN", {}, c, true, xPan, y + 18);
    p.setTooltip (pl ("Panorama tego głosu (L…R)"));
}

Slider& MateoHarmonizerAudioProcessorEditor::addKnob (const String& paramID, const String& title, const String& unit,
                                                      Colour accent, bool bipolar, int cx, int top, int d)
{
    auto* s = sliders.add (new Slider (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow));
    s->setColour (Slider::rotarySliderFillColourId, accent);
    s->getProperties().set ("bipolar", bipolar);
    s->setTextBoxStyle (Slider::TextBoxBelow, false, 40, 14);
    s->setMouseDragSensitivity (200);
    s->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s->setBounds (cx - 24, top, 48, d + 16);
    face.addAndMakeVisible (s);
    sliderAtts.add (new SA (proc.apvts, paramID, *s));

    if (auto* prm = proc.apvts.getParameter (paramID))
        s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));

    face.addText (title, { cx - 36, top - 12, 72, 10 }, skin::dim, 9.0f);
    if (unit.isNotEmpty())
        face.addText (unit, { cx - 20, top + d + 16, 40, 10 }, skin::dim, 9.0f);
    return *s;
}

ToggleButton& MateoHarmonizerAudioProcessorEditor::addToggle (const String& paramID, const String& title, Colour accent,
                                                              int cx, int top, int titleTop)
{
    auto* b = toggles.add (new ToggleButton());
    b->setColour (ToggleButton::tickColourId, accent);
    b->setBounds (cx - 11, top, 22, 24);
    face.addAndMakeVisible (b);
    buttonAtts.add (new BA (proc.apvts, paramID, *b));
    face.addText (title, { cx - 30, titleTop, 60, title.containsChar ('\n') ? 22 : 10 }, skin::dim, 9.0f);
    return *b;
}

ComboBox& MateoHarmonizerAudioProcessorEditor::addCombo (const String& paramID, Rectangle<int> r, Colour textColour)
{
    auto* c = combos.add (new ComboBox());
    if (auto* prm = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramID)))
        c->addItemList (prm->choices, 1);
    c->setColour (ComboBox::textColourId, textColour);
    c->setBounds (r);
    face.addAndMakeVisible (c);
    comboAtts.add (new CA (proc.apvts, paramID, *c));
    return *c;
}

void MateoHarmonizerAudioProcessorEditor::presetButtonClicked (int index)
{
    if (proc.editMode.load())
        proc.storePreset (index);
    else
        proc.recallPreset (index);
    refreshPresetUI();
}

void MateoHarmonizerAudioProcessorEditor::refreshPresetUI()
{
    const bool edit = proc.editMode.load();
    for (int i = 0; i < 4; ++i)
    {
        auto& nm = presetNames[(size_t) i];
        nm.setEditable (edit, edit, false);
        nm.setColour (Label::outlineColourId, edit ? skin::ledRed.withAlpha (0.6f) : skin::border);
        if (! nm.isBeingEdited())
            nm.setText (proc.getPresetName (i), dontSendNotification);
        presetLeds[(size_t) i].setColour (edit ? skin::ledRed : skin::accent);
    }
}

bool MateoHarmonizerAudioProcessorEditor::keyPressed (const KeyPress& k)
{
    const int mode = proc.hotkeyMode.load();
    for (int i = 0; i < 4; ++i)
    {
        const bool hit = (mode == 0 && k.getKeyCode() == '1' + i && ! k.getModifiers().isAnyModifierKeyDown())
                      || (mode == 1 && k.getKeyCode() == KeyPress::F1Key + i);
        if (hit) { proc.recallPreset (i); refreshPresetUI(); return true; }
    }
    return false;
}

void MateoHarmonizerAudioProcessorEditor::timerCallback()
{
    // miernik wejscia
    const float db = proc.takeInputPeakDb();
    const float thr[3] = { 0.0f, -6.0f, -12.0f };
    for (size_t i = 0; i < 3; ++i)
    {
        ledHold[i] = db >= thr[i] ? 1.0f : ledHold[i] * 0.85f;
        if (ledHold[i] < 0.05f) ledHold[i] = 0.0f;
        inputLeds[i].setOn (ledHold[i]);
    }

    // presety
    const int active = proc.activePreset.load();
    for (int i = 0; i < 4; ++i)
        presetLeds[(size_t) i].setOn (i == active ? 1.0f : 0.0f);

    // LINK: pokaz tonacje wynikajaca z MIDI
    if (keyCombo != nullptr && linkButton != nullptr)
    {
        const bool link = linkButton->getToggleState();
        keyCombo->setEnabled (! link);
        if (link)
            keyCombo->setText (MateoHarmonizerAudioProcessor::keyNames()[proc.getEffectiveKey()], dontSendNotification);
        else if (keyCombo->getSelectedItemIndex() < 0 || keyCombo->getText() != keyCombo->getItemText (keyCombo->getSelectedItemIndex()))
            keyCombo->setSelectedItemIndex (roundToInt (proc.apvts.getRawParameterValue (ids::key)->load()), dontSendNotification);
    }
}

void MateoHarmonizerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (skin::bg);
}

void MateoHarmonizerAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) FacePanel::baseW;
    face.setBounds (0, 0, FacePanel::baseW, FacePanel::baseH);
    face.setTransform (AffineTransform::scale (scale));
}
