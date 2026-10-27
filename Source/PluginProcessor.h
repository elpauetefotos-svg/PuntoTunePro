#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "dsp/AutotuneEngine.h"
#include "dsp/Compressor.h"
#include "dsp/StereoDelay.h"

class PuntoTuneAudioProcessor : public juce::AudioProcessor
{
public:
    PuntoTuneAudioProcessor();
    ~PuntoTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Punto Tune Pro"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // telemetría para la interfaz
    float getDetectedMidi()  const noexcept { return engine.getDetectedMidi(); }
    float getCorrectedMidi() const noexcept { return engine.getCorrectedMidi(); }
    float getTargetMidi()    const noexcept { return engine.getTargetMidi(); }
    float getCompGainReductionDb() const noexcept { return comp.getGainReductionDb(); }
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> lastDelayMs { 350.0f };   // tiempo real del delay (con sync incluido)

private:
    void processReverb (juce::AudioBuffer<float>& buffer, int nch, int numSamples);

    // punteros a los parámetros (se leen en el hilo de audio sin crear strings)
    struct ParamPtrs
    {
        std::atomic<float> *outGain {}, *speed {}, *amount {}, *humanize {}, *transpose {}, *fine {}, *refA {};
        std::atomic<float> *note[12] {};
        std::atomic<float> *compOn {}, *compThresh {}, *compRatio {}, *compAttack {}, *compRelease {}, *compMakeup {}, *compMix {};
        std::atomic<float> *delayOn {}, *delayTime {}, *delaySync {}, *delayDiv {}, *delayFb {}, *delayTone {}, *delayPing {}, *delayMix {};
        std::atomic<float> *revOn {}, *revSize {}, *revDamp {}, *revPre {}, *revWidth {}, *revLowCut {}, *revHighCut {}, *revMix {};
    } pp;

    punto::AutotuneEngine engine;
    punto::Compressor     comp;
    punto::StereoDelay    delay;

    juce::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    std::vector<float> preDelayBuf[2];
    int preDelayPos = 0, preDelaySize = 1;
    float revLp[2][2] {}, revHp[2] {};
    float lastRevLow = -1.0f, lastRevHigh = -1.0f;
    bool  revWasOn = false;
    float revLpCoeff = 0.5f, revHpCoeff = 0.99f;

    juce::SmoothedValue<float> outGainSmoothed;
    juce::AudioParameterBool* bypassParam = nullptr;
    double sampleRateHz = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuntoTuneAudioProcessor)
};
