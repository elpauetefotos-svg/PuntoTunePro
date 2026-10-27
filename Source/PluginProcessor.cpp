#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using NR  = juce::NormalisableRange<float>;

    juce::ParameterID pid (const char* id) { return { id, 1 }; }
    juce::ParameterID pid (const juce::String& id) { return { id, 1 }; }

    // duración de cada división (en negras)
    constexpr float kDivBeats[] = { 2.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f };
}

juce::AudioProcessorValueTreeState::ParameterLayout PuntoTuneAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto fl = [&] (const char* id, const char* name, NR range, float def, const char* unit = "")
    {
        p.push_back (std::make_unique<APF> (pid (id), name, range, def,
                                            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    // ---- global ----
    p.push_back (std::make_unique<APB> (pid (ids::bypass), "Bypass", false));
    fl (ids::outGain, "Salida", NR (-24.0f, 12.0f, 0.1f), 0.0f, "dB");

    // ---- autotune ----
    fl (ids::speed,     "Speed",      NR (0.0f, 400.0f, 0.1f, 0.5f), 20.0f, "ms");
    fl (ids::amount,    "Amount",     NR (0.0f, 100.0f, 0.1f),       100.0f, "%");
    fl (ids::humanize,  "Humanize",   NR (0.0f, 100.0f, 0.1f),       0.0f,  "%");
    fl (ids::transpose, "Transpose",  NR (-12.0f, 12.0f, 1.0f),      0.0f,  "st");
    fl (ids::fine,      "Fine",       NR (-100.0f, 100.0f, 0.1f),    0.0f,  "cents");
    fl (ids::refA,      "Ref A",      NR (430.0f, 450.0f, 0.1f),     440.0f, "Hz");
    for (int i = 0; i < 12; ++i)
        p.push_back (std::make_unique<APB> (pid (ids::note (i)), juce::String ("Nota ") + noteNames[i], true));

    // ---- compresor ----
    p.push_back (std::make_unique<APB> (pid (ids::compOn), "Comp On", false));
    fl (ids::compThresh,  "Comp Threshold", NR (-60.0f, 0.0f, 0.1f),        -18.0f, "dB");
    fl (ids::compRatio,   "Comp Ratio",     NR (1.0f, 20.0f, 0.1f, 0.5f),    4.0f,  ":1");
    fl (ids::compAttack,  "Comp Attack",    NR (0.1f, 100.0f, 0.1f, 0.4f),   10.0f, "ms");
    fl (ids::compRelease, "Comp Release",   NR (10.0f, 1000.0f, 1.0f, 0.4f), 120.0f, "ms");
    fl (ids::compMakeup,  "Comp Makeup",    NR (0.0f, 24.0f, 0.1f),          0.0f,  "dB");
    fl (ids::compMix,     "Comp Mix",       NR (0.0f, 100.0f, 0.1f),         100.0f, "%");

    // ---- delay ----
    p.push_back (std::make_unique<APB> (pid (ids::delayOn), "Delay On", false));
    fl (ids::delayTime, "Delay Time", NR (10.0f, 2000.0f, 1.0f, 0.5f), 350.0f, "ms");
    p.push_back (std::make_unique<APB> (pid (ids::delaySync), "Delay Sync", false));
    p.push_back (std::make_unique<APC> (pid (ids::delayDiv), "Delay Division",
                 juce::StringArray { "1/2", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16." }, 1));
    fl (ids::delayFeedback, "Delay Feedback", NR (0.0f, 95.0f, 0.1f),             35.0f, "%");
    fl (ids::delayTone,     "Delay Tone",     NR (500.0f, 16000.0f, 1.0f, 0.4f),  6000.0f, "Hz");
    p.push_back (std::make_unique<APB> (pid (ids::delayPing), "Delay PingPong", true));
    fl (ids::delayMix, "Delay Mix", NR (0.0f, 100.0f, 0.1f), 25.0f, "%");

    // ---- reverb ----
    p.push_back (std::make_unique<APB> (pid (ids::revOn), "Reverb On", false));
    fl (ids::revSize,    "Reverb Size",     NR (0.0f, 100.0f, 0.1f),             50.0f, "%");
    fl (ids::revDamp,    "Reverb Damp",     NR (0.0f, 100.0f, 0.1f),             50.0f, "%");
    fl (ids::revPre,     "Reverb Pre-delay",NR (0.0f, 200.0f, 0.1f, 0.6f),       20.0f, "ms");
    fl (ids::revWidth,   "Reverb Width",    NR (0.0f, 100.0f, 0.1f),             100.0f, "%");
    fl (ids::revLowCut,  "Reverb Low cut",  NR (20.0f, 1000.0f, 1.0f, 0.4f),     120.0f, "Hz");
    fl (ids::revHighCut, "Reverb High cut", NR (1000.0f, 20000.0f, 1.0f, 0.4f),  9000.0f, "Hz");
    fl (ids::revMix,     "Reverb Mix",      NR (0.0f, 100.0f, 0.1f),             20.0f, "%");

    return { p.begin(), p.end() };
}

PuntoTuneAudioProcessor::PuntoTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PUNTO_TUNE_PRO", createLayout())
{
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    pp.outGain = P (ids::outGain);
    pp.speed = P (ids::speed);         pp.amount = P (ids::amount);
    pp.humanize = P (ids::humanize);   pp.transpose = P (ids::transpose);
    pp.fine = P (ids::fine);           pp.refA = P (ids::refA);
    for (int i = 0; i < 12; ++i)       pp.note[i] = apvts.getRawParameterValue (ids::note (i));

    pp.compOn = P (ids::compOn);       pp.compThresh = P (ids::compThresh);
    pp.compRatio = P (ids::compRatio); pp.compAttack = P (ids::compAttack);
    pp.compRelease = P (ids::compRelease); pp.compMakeup = P (ids::compMakeup);
    pp.compMix = P (ids::compMix);

    pp.delayOn = P (ids::delayOn);     pp.delayTime = P (ids::delayTime);
    pp.delaySync = P (ids::delaySync); pp.delayDiv = P (ids::delayDiv);
    pp.delayFb = P (ids::delayFeedback); pp.delayTone = P (ids::delayTone);
    pp.delayPing = P (ids::delayPing); pp.delayMix = P (ids::delayMix);

    pp.revOn = P (ids::revOn);         pp.revSize = P (ids::revSize);
    pp.revDamp = P (ids::revDamp);     pp.revPre = P (ids::revPre);
    pp.revWidth = P (ids::revWidth);   pp.revLowCut = P (ids::revLowCut);
    pp.revHighCut = P (ids::revHighCut); pp.revMix = P (ids::revMix);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ids::bypass));
}

bool PuntoTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PuntoTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    const int nch = juce::jlimit (1, 2, getTotalNumInputChannels());

    engine.prepare (sampleRate, nch);
    comp.prepare (sampleRate);
    delay.prepare (sampleRate);

    reverb.setSampleRate (sampleRate);
    reverb.reset();
    wetBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

    preDelaySize = 1;
    while (preDelaySize < (int) (sampleRate * 0.25)) preDelaySize <<= 1;
    for (auto& b : preDelayBuf) b.assign ((size_t) preDelaySize, 0.0f);
    preDelayPos = 0;
    std::fill (&revLp[0][0], &revLp[0][0] + 4, 0.0f);
    revHp[0] = revHp[1] = 0.0f;
    lastRevLow = lastRevHigh = -1.0f;
    revWasOn = false;

    outGainSmoothed.reset (sampleRate, 0.02);
    outGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pp.outGain->load()));

    setLatencySamples (engine.getLatencySamples());
}

void PuntoTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int nch = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (nch <= 0 || numSamples <= 0) return;

    inPeak.store (buffer.getMagnitude (0, numSamples), std::memory_order_relaxed);

    float* const* data = buffer.getArrayOfWritePointers();

    // ------------------------------------------------------------ AUTOTUNE
    {
        punto::TuneParams tp;
        tp.speedMs        = pp.speed->load();
        tp.amount         = pp.amount->load() * 0.01f;
        tp.humanize       = pp.humanize->load() * 0.01f;
        tp.transposeSemis = pp.transpose->load();
        tp.fineCents      = pp.fine->load();
        tp.referenceA     = pp.refA->load();
        uint16_t mask = 0;
        for (int i = 0; i < 12; ++i)
            if (pp.note[i]->load() > 0.5f) mask |= (uint16_t) (1u << i);
        tp.noteMask = mask;
        engine.process (data, nch, numSamples, tp);
    }

    // ------------------------------------------------------------ COMPRESOR
    {
        punto::CompParams cp;
        cp.on          = pp.compOn->load() > 0.5f;
        cp.thresholdDb = pp.compThresh->load();
        cp.ratio       = pp.compRatio->load();
        cp.attackMs    = pp.compAttack->load();
        cp.releaseMs   = pp.compRelease->load();
        cp.makeupDb    = pp.compMakeup->load();
        cp.mix         = pp.compMix->load() * 0.01f;
        comp.process (data, nch, numSamples, cp);
    }

    // ------------------------------------------------------------ DELAY
    {
        punto::DelayParams dp;
        dp.on       = pp.delayOn->load() > 0.5f;
        dp.timeMs   = pp.delayTime->load();
        if (pp.delaySync->load() > 0.5f)
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm())
                        if (*b > 20.0) bpm = *b;
            const int div = juce::jlimit (0, 8, (int) std::lround (pp.delayDiv->load()));
            dp.timeMs = (float) (60000.0 / bpm) * kDivBeats[div];
        }
        lastDelayMs.store (dp.timeMs, std::memory_order_relaxed);
        dp.feedback = pp.delayFb->load() * 0.01f;
        dp.toneHz   = pp.delayTone->load();
        dp.pingPong = pp.delayPing->load() > 0.5f;
        dp.mix      = pp.delayMix->load() * 0.01f;
        delay.process (data, nch, numSamples, dp);
    }

    // ------------------------------------------------------------ REVERB
    const bool revOn = pp.revOn->load() > 0.5f;
    if (revOn)
    {
        if (! revWasOn) { reverb.reset(); revWasOn = true; }
        processReverb (buffer, nch, numSamples);
    }
    else
        revWasOn = false;

    // ------------------------------------------------------------ SALIDA
    outGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pp.outGain->load()));
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = outGainSmoothed.getNextValue();
        for (int c = 0; c < nch; ++c)
        {
            float v = data[c][n] * g;
            if (! std::isfinite (v)) v = 0.0f;
            data[c][n] = juce::jlimit (-8.0f, 8.0f, v);
        }
    }

    outPeak.store (buffer.getMagnitude (0, numSamples), std::memory_order_relaxed);
}

void PuntoTuneAudioProcessor::processReverb (juce::AudioBuffer<float>& buffer, int nch, int numSamples)
{
    wetBuffer.setSize (2, numSamples, false, false, true);

    // entrada (mono se duplica)
    for (int c = 0; c < 2; ++c)
        wetBuffer.copyFrom (c, 0, buffer, juce::jmin (c, nch - 1), 0, numSamples);

    // pre-delay
    const int pd = juce::jlimit (0, preDelaySize - 1, (int) (pp.revPre->load() * 0.001f * (float) sampleRateHz));
    const int pmask = preDelaySize - 1;
    for (int c = 0; c < 2; ++c)
    {
        float* w = wetBuffer.getWritePointer (c);
        auto& pb = preDelayBuf[c];
        int pos = preDelayPos;
        for (int n = 0; n < numSamples; ++n)
        {
            pb[(size_t) pos] = w[n];
            w[n] = pb[(size_t) ((pos - pd) & pmask)];
            pos = (pos + 1) & pmask;
        }
    }
    preDelayPos = (preDelayPos + numSamples) & pmask;

    // reverb (solo señal húmeda)
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.2f + 0.78f * pp.revSize->load() * 0.01f;
    rp.damping    = pp.revDamp->load() * 0.01f;
    rp.wetLevel   = 0.33f;
    rp.dryLevel   = 0.0f;
    rp.width      = pp.revWidth->load() * 0.01f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1), numSamples);

    // filtros de la cola
    const float low  = pp.revLowCut->load();
    const float high = pp.revHighCut->load();
    if (low != lastRevLow || high != lastRevHigh)
    {
        revHpCoeff = 1.0f - std::exp (-6.2831853f * low  / (float) sampleRateHz);
        revLpCoeff = 1.0f - std::exp (-6.2831853f * juce::jmin (high, 0.45f * (float) sampleRateHz) / (float) sampleRateHz);
        lastRevLow = low; lastRevHigh = high;
    }
    for (int c = 0; c < 2; ++c)
    {
        float* w = wetBuffer.getWritePointer (c);
        for (int n = 0; n < numSamples; ++n)
        {
            revLp[c][0] += revLpCoeff * (w[n] - revLp[c][0]);
            revLp[c][1] += revLpCoeff * (revLp[c][0] - revLp[c][1]);
            revHp[c]    += revHpCoeff * (revLp[c][1] - revHp[c]);
            w[n] = revLp[c][1] - revHp[c];
        }
    }

    // mezcla
    const float mix = pp.revMix->load() * 0.01f;
    float* const* data = buffer.getArrayOfWritePointers();
    const float* wl = wetBuffer.getReadPointer (0);
    const float* wr = wetBuffer.getReadPointer (1);
    if (nch == 1)
    {
        for (int n = 0; n < numSamples; ++n)
            data[0][n] += 0.5f * (wl[n] + wr[n]) * mix;
    }
    else
    {
        for (int n = 0; n < numSamples; ++n)
        {
            data[0][n] += wl[n] * mix;
            data[1][n] += wr[n] * mix;
        }
    }
}

juce::AudioProcessorEditor* PuntoTuneAudioProcessor::createEditor()
{
    return new PuntoTuneAudioProcessorEditor (*this);
}

void PuntoTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PuntoTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PuntoTuneAudioProcessor();
}
