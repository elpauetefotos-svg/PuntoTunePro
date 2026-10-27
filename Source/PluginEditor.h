#pragma once
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Panels.h"

// Botón de pestaña con punto de estado (efecto activo)
class TabButton : public juce::TextButton
{
public:
    std::function<bool()> isOn;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        juce::TextButton::paintButton (g, over, down);
        if (isOn && isOn())
        {
            g.setColour (getToggleState() ? juce::Colours::black : theme::white);
            g.fillEllipse ((float) getWidth() - 17.0f, (float) getHeight() * 0.5f - 3.0f, 6.0f, 6.0f);
        }
    }
};

class PuntoTuneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit PuntoTuneAudioProcessorEditor (PuntoTuneAudioProcessor&);
    ~PuntoTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int index);

    PuntoTuneAudioProcessor& proc;
    PuntoLookAndFeel laf;

    TabButton tabTune, tabComp, tabDelay, tabReverb;
    juce::TextButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

    TunePanel   tunePanel;
    CompPanel   compPanel;
    DelayPanel  delayPanel;
    ReverbPanel reverbPanel;

    LevelBar inBar { "IN" }, outBar { "OUT" };
    juce::Slider outSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAttach;

    std::atomic<float> *compOn {}, *delayOn {}, *revOn {};
    int currentTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuntoTuneAudioProcessorEditor)
};
