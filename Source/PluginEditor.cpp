#include "PluginEditor.h"

PuntoTuneAudioProcessorEditor::PuntoTuneAudioProcessorEditor (PuntoTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      tunePanel (p.apvts), compPanel (p.apvts), delayPanel (p.apvts, p), reverbPanel (p.apvts)
{
    setLookAndFeel (&laf);
    setSize (900, 640);

    compOn  = p.apvts.getRawParameterValue (ids::compOn);
    delayOn = p.apvts.getRawParameterValue (ids::delayOn);
    revOn   = p.apvts.getRawParameterValue (ids::revOn);

    // --- pestañas
    const char* names[] = { "Tune", "Comp", "Delay", "Reverb" };
    TabButton* tabs[] = { &tabTune, &tabComp, &tabDelay, &tabReverb };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i]->setButtonText (names[i]);
        tabs[i]->setClickingTogglesState (true);
        tabs[i]->setRadioGroupId (1001);
        tabs[i]->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (*tabs[i]);
    }
    tabComp.isOn   = [this] { return compOn->load()  > 0.5f; };
    tabDelay.isOn  = [this] { return delayOn->load() > 0.5f; };
    tabReverb.isOn = [this] { return revOn->load()   > 0.5f; };

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::bypass, bypassButton);

    // --- paneles
    addAndMakeVisible (tunePanel);
    addChildComponent (compPanel);
    addChildComponent (delayPanel);
    addChildComponent (reverbPanel);

    // --- pie
    addAndMakeVisible (inBar);
    addAndMakeVisible (outBar);
    outSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    addAndMakeVisible (outSlider);
    outAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::outGain, outSlider);
    outSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    outSlider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    outSlider.updateText();

    tabTune.setToggleState (true, juce::dontSendNotification);
    showTab (0);
    startTimerHz (30);
}

PuntoTuneAudioProcessorEditor::~PuntoTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PuntoTuneAudioProcessorEditor::showTab (int index)
{
    currentTab = index;
    tunePanel.setVisible (index == 0);
    compPanel.setVisible (index == 1);
    delayPanel.setVisible (index == 2);
    reverbPanel.setVisible (index == 3);
    repaint();
}

void PuntoTuneAudioProcessorEditor::timerCallback()
{
    // el gráfico de pitch sigue corriendo aunque estés en otra pestaña
    tunePanel.tick (proc.getDetectedMidi(), proc.getCorrectedMidi(), proc.getTargetMidi());
    if (currentTab == 1) compPanel.tick (proc.getCompGainReductionDb());
    if (currentTab == 2) delayPanel.tick();
    if (currentTab == 3) reverbPanel.tick();

    inBar.setLevel (proc.inPeak.load());
    outBar.setLevel (proc.outPeak.load());

    tabComp.repaint();
    tabDelay.repaint();
    tabReverb.repaint();
}

void PuntoTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // brillo muy sutil arriba
    juce::ColourGradient glow (juce::Colours::white.withAlpha (0.045f), 450.0f, 0.0f,
                               juce::Colours::transparentBlack, 450.0f, 220.0f, false);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds().removeFromTop (220));

    // logo
    const auto f1 = theme::font (23.0f, true);
    g.setColour (theme::white);
    g.setFont (f1);
    g.drawText ("PUNTO TUNE", 26, 14, 170, 32, juce::Justification::centredLeft);

    const float w = theme::textWidth (f1, "PUNTO TUNE");
    auto pill = juce::Rectangle<float> (26.0f + w + 10.0f, 21.0f, 38.0f, 20.0f);
    g.setColour (theme::white);
    g.drawRoundedRectangle (pill, 10.0f, 1.2f);
    g.setFont (theme::font (11.5f, true));
    g.drawText ("PRO", pill, juce::Justification::centred);

    // línea bajo la cabecera
    g.setColour (theme::line);
    g.drawHorizontalLine (60, 20.0f, (float) getWidth() - 20.0f);

    // pie
    g.setColour (theme::faint);
    g.setFont (theme::font (11.0f, true));
    g.drawText (theme::U ("PUNTO TUNE PRO  \xC2\xB7  v1.0.0  \xC2\xB7  latencia ~10 ms"),
                26, 606, 420, 26, juce::Justification::centredLeft);
}

void PuntoTuneAudioProcessorEditor::resized()
{
    const int tabW = 88, gap = 6, tabH = 32;
    const int total = 4 * tabW + 3 * gap;
    int x = (getWidth() - total) / 2 + 20;
    for (auto* t : { &tabTune, &tabComp, &tabDelay, &tabReverb })
    {
        t->setBounds (x, 14, tabW, tabH);
        x += tabW + gap;
    }
    bypassButton.setBounds (getWidth() - 24 - 92, 14, 92, tabH);

    for (juce::Component* c : { (juce::Component*) &tunePanel, (juce::Component*) &compPanel,
                                (juce::Component*) &delayPanel, (juce::Component*) &reverbPanel })
        c->setBounds (getLocalBounds());

    // pie
    inBar.setBounds (470, 612, 120, 16);
    outBar.setBounds (600, 612, 120, 16);
    outSlider.setBounds (736, 608, 144, 24);
}
