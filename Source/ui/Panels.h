#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"
#include <random>

namespace layout
{
    inline juce::Rectangle<int> knobsCard() { return { 20, 446, 860, 150 }; }
    inline juce::Rectangle<int> sideFull()  { return { 636, 72, 244, 360 }; }
    inline juce::Rectangle<int> visualFull(){ return { 20, 72, 600, 360 }; }

    inline void layoutKnobs (std::initializer_list<juce::Component*> knobs)
    {
        auto area = knobsCard().reduced (14, 6);
        const int w = area.getWidth() / (int) knobs.size();
        int i = 0;
        for (auto* k : knobs)
            k->setBounds (area.getX() + (i++) * w, area.getY(), w, area.getHeight());
    }
}

class PanelBase : public juce::Component
{
public:
    PanelBase() { setInterceptsMouseClicks (false, true); }
};

static void sectionLabel (juce::Graphics& g, const char* utf8, int x, int y, int w)
{
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f, true));
    g.drawText (theme::U (utf8).toUpperCase(), x, y, w, 14, juce::Justification::centredLeft);
}

// ============================================================================
//  TUNE
// ============================================================================
class TunePanel : public PanelBase
{
public:
    explicit TunePanel (juce::AudioProcessorValueTreeState& a)
        : apvts (a), pitch (a), keys (a),
          kSpeed (a, ids::speed, "Speed", " ms", 1),
          kAmount (a, ids::amount, "Amount", " %", 0),
          kHumanize (a, ids::humanize, "Humanize", " %", 0),
          kTranspose (a, ids::transpose, "Transpose", " st", 0),
          kFine (a, ids::fine, "Fine", " cents", 0),
          kRef (a, ids::refA, "Ref A", " Hz", 1)
    {
        addAndMakeVisible (pitch);
        addAndMakeVisible (keys);
        addAndMakeVisible (readout);
        for (auto* k : { &kSpeed, &kAmount, &kHumanize, &kTranspose, &kFine, &kRef })
            addAndMakeVisible (*k);

        for (int i = 0; i < 12; ++i) keyBox.addItem (noteNames[i], i + 1);
        int n = 0;
        const auto* scales = getScales (n);
        for (int i = 0; i < n; ++i) scaleBox.addItem (theme::U (scales[i].name), i + 1);

        keyBox.setSelectedItemIndex (juce::jlimit (0, 11, (int) apvts.state.getProperty ("key", 0)), juce::dontSendNotification);
        scaleBox.setSelectedItemIndex (juce::jlimit (0, n - 1, (int) apvts.state.getProperty ("scale", 0)), juce::dontSendNotification);
        keyBox.onChange   = [this] { applyScale(); };
        scaleBox.onChange = [this] { applyScale(); };
        addAndMakeVisible (keyBox);
        addAndMakeVisible (scaleBox);
    }

    void tick (float detected, float corrected, float target)
    {
        pitch.push (detected, corrected, target);
        readout.update (detected, target);
        keys.setActivePitchClass (detected > 0.0f ? ((int) std::lround (detected) % 12 + 12) % 12 : -1);
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawCard (g, layout::knobsCard().toFloat());
        auto side = juce::Rectangle<int> (636, 336, 244, 96);
        theme::drawCard (g, side.toFloat());
        sectionLabel (g, "TÓNICA", 652, 346, 70);
        sectionLabel (g, "Escala", 736, 346, 100);
    }

    void resized() override
    {
        pitch.setBounds (20, 72, 600, 250);
        keys.setBounds (20, 336, 600, 96);
        readout.setBounds (636, 72, 244, 250);
        keyBox.setBounds (652, 368, 76, 32);
        scaleBox.setBounds (736, 368, 128, 32);
        layout::layoutKnobs ({ &kSpeed, &kAmount, &kHumanize, &kTranspose, &kFine, &kRef });
    }

private:
    void applyScale()
    {
        int n = 0;
        const auto* scales = getScales (n);
        const int scaleIdx = juce::jlimit (0, n - 1, scaleBox.getSelectedItemIndex());
        const int root = juce::jlimit (0, 11, keyBox.getSelectedItemIndex());
        bool on[12] = {};
        for (int i = 0; i < scales[scaleIdx].count; ++i)
            on[(root + scales[scaleIdx].notes[i]) % 12] = true;

        for (int i = 0; i < 12; ++i)
            if (auto* p = apvts.getParameter (ids::note (i)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (on[i] ? 1.0f : 0.0f);
                p->endChangeGesture();
            }
        apvts.state.setProperty ("key", root, nullptr);
        apvts.state.setProperty ("scale", scaleIdx, nullptr);
    }

    juce::AudioProcessorValueTreeState& apvts;
    PitchView pitch;
    NoteKeyboard keys;
    NoteReadout readout;
    juce::ComboBox keyBox, scaleBox;
    Knob kSpeed, kAmount, kHumanize, kTranspose, kFine, kRef;
};

// ============================================================================
//  COMPRESOR
// ============================================================================
class CompCurveView : public juce::Component
{
public:
    explicit CompCurveView (juce::AudioProcessorValueTreeState& a) : apvts (a)
    {
        thr = a.getRawParameterValue (ids::compThresh);  ratio = a.getRawParameterValue (ids::compRatio);
        makeup = a.getRawParameterValue (ids::compMakeup); mix = a.getRawParameterValue (ids::compMix);
        on = a.getRawParameterValue (ids::compOn);
    }

    static float staticOut (float inDb, float t, float r)
    {
        const float knee = 6.0f, over = inDb - t, invR = 1.0f / juce::jmax (1.0f, r);
        if (2.0f * over < -knee) return inDb;
        if (2.0f * std::abs (over) <= knee)
        {
            const float x = over + knee * 0.5f;
            return inDb + (invR - 1.0f) * x * x / (2.0f * knee);
        }
        return t + over * invR;
    }

    void paint (juce::Graphics& g) override
    {
        auto all = getLocalBounds().toFloat();
        theme::drawCard (g, all);
        auto p = all.reduced (46.0f, 34.0f);

        const float lo = -60.0f, hi = 6.0f;
        auto xOf = [&] (float db) { return p.getX() + (db - lo) / (hi - lo) * p.getWidth(); };
        auto yOf = [&] (float db) { return p.getBottom() - (db - lo) / (hi - lo) * p.getHeight(); };

        g.setFont (theme::font (10.5f, true));
        for (float d = -60.0f; d <= 0.0f; d += 12.0f)
        {
            g.setColour (theme::line);
            g.drawVerticalLine ((int) xOf (d), p.getY(), p.getBottom());
            g.drawHorizontalLine ((int) yOf (d), p.getX(), p.getRight());
            g.setColour (theme::dim);
            g.drawText (juce::String ((int) d), (int) xOf (d) - 20, (int) p.getBottom() + 6, 40, 12, juce::Justification::centred);
            g.drawText (juce::String ((int) d), (int) p.getX() - 40, (int) yOf (d) - 6, 32, 12, juce::Justification::centredRight);
        }
        g.setColour (theme::dim);
        g.drawText ("ENTRADA dB", (int) p.getRight() - 90, (int) p.getBottom() + 20, 90, 12, juce::Justification::centredRight);
        g.drawText ("SALIDA dB", (int) all.getX() + 12, (int) all.getY() + 10, 90, 12, juce::Justification::centredLeft);

        // diagonal (sin compresión)
        juce::Path diag;
        diag.startNewSubPath (xOf (lo), yOf (lo));
        diag.lineTo (xOf (0.0f), yOf (0.0f));
        const float dashes[] = { 4.0f, 5.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
        g.setColour (theme::faint);
        g.fillPath (dashed);

        // umbral
        const float t = thr->load();
        g.setColour (theme::dim.withAlpha (0.6f));
        g.drawVerticalLine ((int) xOf (t), p.getY(), p.getBottom());

        // curva
        const bool enabled = on->load() > 0.5f;
        const float mk = juce::Decibels::decibelsToGain (makeup->load());
        const float mx = mix->load() * 0.01f;
        juce::Path curve;
        bool started = false;
        for (float xd = lo; xd <= 0.0f; xd += 0.5f)
        {
            const float outDb = staticOut (xd, t, ratio->load());
            const float gainLin = juce::Decibels::decibelsToGain (outDb - xd) * mk;
            const float yLin = juce::Decibels::decibelsToGain (xd) * ((1.0f - mx) + gainLin * mx);
            const float yd = juce::jlimit (lo, hi, juce::Decibels::gainToDecibels (yLin, -80.0f));
            if (! started) { curve.startNewSubPath (xOf (xd), yOf (yd)); started = true; }
            else           curve.lineTo (xOf (xd), yOf (yd));
        }
        g.setColour (enabled ? theme::white.withAlpha (0.14f) : juce::Colours::transparentBlack);
        g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (enabled ? theme::white : theme::faint);
        g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float> *thr {}, *ratio {}, *makeup {}, *mix {}, *on {};
};

class CompPanel : public PanelBase
{
public:
    CompPanel (juce::AudioProcessorValueTreeState& a)
        : apvts (a), curve (a), onButton ("Activo"),
          kThresh (a, ids::compThresh, "Threshold", " dB", 1),
          kRatio (a, ids::compRatio, "Ratio", " : 1", 1),
          kAttack (a, ids::compAttack, "Attack", " ms", 1),
          kRelease (a, ids::compRelease, "Release", " ms", 0),
          kMakeup (a, ids::compMakeup, "Makeup", " dB", 1),
          kMix (a, ids::compMix, "Mix", " %", 0)
    {
        addAndMakeVisible (curve);
        addAndMakeVisible (onButton);
        onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ids::compOn, onButton);
        for (auto* k : { &kThresh, &kRatio, &kAttack, &kRelease, &kMakeup, &kMix })
            addAndMakeVisible (*k);
        onParam = a.getRawParameterValue (ids::compOn);
    }

    void tick (float grDb)
    {
        // ataque instantáneo, caída suave del medidor
        if (grDb < gr) gr = grDb;
        else           gr = juce::jmin (grDb, gr + 1.2f);
        const float a = onParam->load() > 0.5f ? 1.0f : 0.45f;
        for (auto* k : { &kThresh, &kRatio, &kAttack, &kRelease, &kMakeup, &kMix }) k->setAlpha (a);
        repaint (layout::sideFull());
        curve.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawCard (g, layout::knobsCard().toFloat());
        auto s = layout::sideFull().toFloat();
        theme::drawCard (g, s);

        sectionLabel (g, "REDUCCIÓN DE GANANCIA", 656, 150, 204);
        g.setColour (onParam->load() > 0.5f ? theme::white : theme::faint);
        g.setFont (theme::font (40.0f, true));
        g.drawText (juce::String (gr, 1) + " dB", 646, 170, 224, 52, juce::Justification::centred);

        auto bar = juce::Rectangle<float> (656.0f, 240.0f, 204.0f, 10.0f);
        g.setColour (theme::line);
        g.fillRoundedRectangle (bar, 5.0f);
        const float prop = juce::jlimit (0.0f, 1.0f, -gr / 24.0f);
        g.setColour (theme::white);
        g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * prop), 5.0f);
        g.setColour (theme::faint);
        g.setFont (theme::font (10.0f, true));
        g.drawText ("-24", 656, 256, 40, 12, juce::Justification::centredLeft);
        g.drawText ("0", 820, 256, 40, 12, juce::Justification::centredRight);

        g.setColour (theme::dim);
        g.setFont (theme::font (12.0f));
        g.drawFittedText (theme::U ("Compresor suave (rodilla 6 dB). Mezcla < 100 % = compresión paralela: mantiene los transitorios de la voz."),
                          656, 292, 204, 120, juce::Justification::topLeft, 6);
    }

    void resized() override
    {
        curve.setBounds (layout::visualFull());
        onButton.setBounds (656, 92, 180, 28);
        layout::layoutKnobs ({ &kThresh, &kRatio, &kAttack, &kRelease, &kMakeup, &kMix });
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    CompCurveView curve;
    juce::ToggleButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    Knob kThresh, kRatio, kAttack, kRelease, kMakeup, kMix;
    std::atomic<float>* onParam = nullptr;
    float gr = 0.0f;
};

// ============================================================================
//  DELAY
// ============================================================================
class EchoView : public juce::Component
{
public:
    EchoView (juce::AudioProcessorValueTreeState& a, PuntoTuneAudioProcessor& p) : proc (p)
    {
        fb = a.getRawParameterValue (ids::delayFeedback); mix = a.getRawParameterValue (ids::delayMix);
        ping = a.getRawParameterValue (ids::delayPing);   on = a.getRawParameterValue (ids::delayOn);
    }

    void paint (juce::Graphics& g) override
    {
        auto all = getLocalBounds().toFloat();
        theme::drawCard (g, all);
        auto p = all.reduced (36.0f, 40.0f);
        const float mid = p.getCentreY();
        const bool enabled = on->load() > 0.5f;
        const bool pp = ping->load() > 0.5f;
        const float f = fb->load() * 0.01f, m = mix->load() * 0.01f;
        const float tMs = proc.lastDelayMs.load();

        g.setColour (theme::line);
        g.drawHorizontalLine ((int) mid, p.getX(), p.getRight());

        // nº de ecos audibles (amplitud > 2 %) para repartir el ancho
        int taps = 3;
        for (int k = 1; k < 12; ++k)
            if (std::pow (f, (float) (k - 1)) * (0.25f + 0.75f * m) >= 0.02f) taps = k;
        taps = juce::jlimit (3, 11, taps);
        const float stepX = p.getWidth() / ((float) taps + 0.5f);
        auto bar = [&] (float x, float h, bool up, juce::Colour c)
        {
            g.setColour (c);
            const float hh = juce::jmax (3.0f, h);
            const float y = up ? mid - hh : mid;
            g.fillRoundedRectangle (x - 6.0f, y, 12.0f, hh, 5.0f);
        };
        bar (p.getX(), p.getHeight() * 0.5f - 4.0f, true,  enabled ? theme::white : theme::faint);
        bar (p.getX(), p.getHeight() * 0.5f - 4.0f, false, enabled ? theme::white.withAlpha (0.35f) : theme::faint.withAlpha (0.5f));

        for (int k = 1; k <= taps; ++k)
        {
            const float amp = std::pow (f, (float) (k - 1)) * (0.25f + 0.75f * m);
            if (amp < 0.02f) break;
            const float h = amp * (p.getHeight() * 0.5f - 4.0f);
            const float x = p.getX() + stepX * (float) k;
            g.setColour (theme::faint);
            g.setFont (theme::font (10.0f, true));
            g.drawText (juce::String ((int) std::lround (tMs * (float) k)) + " ms", (int) x - 30, (int) p.getBottom() + 6, 60, 12, juce::Justification::centred);
            const auto c = enabled ? theme::white : theme::faint;
            if (pp)
            {
                const bool left = (k % 2) == 1;
                bar (x, h, left, c);
                bar (x, h * 0.3f, ! left, c.withAlpha (0.25f));
            }
            else
            {
                bar (x, h, true, c);
                bar (x, h, false, c.withAlpha (0.45f));
            }
        }

        g.setColour (theme::dim);
        g.setFont (theme::font (10.5f, true));
        g.drawText ("L", (int) all.getX() + 12, (int) (mid - 22.0f), 16, 12, juce::Justification::centred);
        g.drawText ("R", (int) all.getX() + 12, (int) (mid + 10.0f), 16, 12, juce::Justification::centred);
        g.setFont (theme::font (12.0f, true));
        g.drawText (juce::String ((int) std::lround (tMs)) + " ms entre ecos",
                    (int) all.getX() + 20, (int) all.getBottom() - 30, 300, 16, juce::Justification::centredLeft);
    }

private:
    PuntoTuneAudioProcessor& proc;
    std::atomic<float> *fb {}, *mix {}, *ping {}, *on {};
};

class DelayPanel : public PanelBase
{
public:
    DelayPanel (juce::AudioProcessorValueTreeState& a, PuntoTuneAudioProcessor& p)
        : apvts (a), echo (a, p), onButton ("Activo"), syncButton ("Sync al tempo"), pingButton ("Ping-pong"),
          kTime (a, ids::delayTime, "Time", " ms", 0),
          kFeedback (a, ids::delayFeedback, "Feedback", " %", 0),
          kTone (a, ids::delayTone, "Tone", " Hz", 0),
          kMix (a, ids::delayMix, "Mix", " %", 0)
    {
        addAndMakeVisible (echo);
        for (auto* b : { &onButton, &syncButton, &pingButton }) addAndMakeVisible (*b);
        onAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ids::delayOn, onButton);
        syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ids::delaySync, syncButton);
        pingAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ids::delayPing, pingButton);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (a.getParameter (ids::delayDiv)))
            divBox.addItemList (choice->choices, 1);
        addAndMakeVisible (divBox);
        divAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (a, ids::delayDiv, divBox);

        for (auto* k : { &kTime, &kFeedback, &kTone, &kMix }) addAndMakeVisible (*k);
        onParam   = a.getRawParameterValue (ids::delayOn);
        syncParam = a.getRawParameterValue (ids::delaySync);
    }

    void tick()
    {
        const bool on = onParam->load() > 0.5f, sync = syncParam->load() > 0.5f;
        const float al = on ? 1.0f : 0.45f;
        for (auto* k : { &kFeedback, &kTone, &kMix }) k->setAlpha (al);
        kTime.setAlpha (on ? (sync ? 0.4f : 1.0f) : 0.45f);
        kTime.setEnabled (! sync);
        divBox.setEnabled (sync);
        divBox.setAlpha (sync ? 1.0f : 0.45f);
        echo.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawCard (g, layout::knobsCard().toFloat());
        theme::drawCard (g, layout::sideFull().toFloat());
        sectionLabel (g, "DIVISIÓN (CON SYNC)", 656, 238, 204);
        g.setColour (theme::dim);
        g.setFont (theme::font (12.0f));
        g.drawFittedText ("Con sync activado el tiempo sigue el BPM del proyecto. El eco mantiene la voz seca intacta y se suma por encima.",
                          656, 296, 204, 110, juce::Justification::topLeft, 6);
    }

    void resized() override
    {
        echo.setBounds (layout::visualFull());
        onButton.setBounds (656, 92, 180, 28);
        syncButton.setBounds (656, 142, 200, 28);
        pingButton.setBounds (656, 182, 200, 28);
        divBox.setBounds (656, 256, 204, 32);
        layout::layoutKnobs ({ &kTime, &kFeedback, &kTone, &kMix });
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    EchoView echo;
    juce::ToggleButton onButton, syncButton, pingButton;
    juce::ComboBox divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, syncAttach, pingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttach;
    Knob kTime, kFeedback, kTone, kMix;
    std::atomic<float> *onParam {}, *syncParam {};
};

// ============================================================================
//  REVERB
// ============================================================================
class TailView : public juce::Component
{
public:
    explicit TailView (juce::AudioProcessorValueTreeState& a)
    {
        size = a.getRawParameterValue (ids::revSize);   damp = a.getRawParameterValue (ids::revDamp);
        pre = a.getRawParameterValue (ids::revPre);     mix = a.getRawParameterValue (ids::revMix);
        on = a.getRawParameterValue (ids::revOn);
    }

    void paint (juce::Graphics& g) override
    {
        auto all = getLocalBounds().toFloat();
        theme::drawCard (g, all);
        auto p = all.reduced (36.0f, 40.0f);
        const bool enabled = on->load() > 0.5f;

        const float s = size->load() * 0.01f, d = damp->load() * 0.01f;
        const float decaySec = 0.4f + 5.6f * s * s;               // cola aproximada
        const float preSec = pre->load() * 0.001f;
        const float totalSec = juce::jmax (1.0f, preSec + decaySec * 1.5f);
        const float level = 0.25f + 0.75f * mix->load() * 0.01f;

        g.setColour (theme::line);
        g.drawHorizontalLine ((int) p.getBottom(), p.getX(), p.getRight());

        std::mt19937 rng (7);
        std::uniform_real_distribution<float> rnd (0.35f, 1.0f);
        const int lines = 170;
        for (int i = 0; i < lines; ++i)
        {
            const float t = totalSec * (float) i / (float) lines;
            const float x = p.getX() + p.getWidth() * (t / totalSec);
            const float r = rnd (rng);
            if (t < preSec) continue;
            const float env = std::exp (-3.0f * (t - preSec) / decaySec);
            const float h = p.getHeight() * 0.9f * level * env * r;
            if (h < 1.0f) continue;
            const float alpha = juce::jlimit (0.25f, 1.0f, 1.0f - d * 0.55f * (t / totalSec) * 4.0f);
            g.setColour ((enabled ? theme::white : theme::faint).withAlpha (alpha));
            g.fillRoundedRectangle (x - 1.5f, p.getBottom() - h, 3.0f, h, 1.5f);
        }

        // señal directa + pre-delay
        g.setColour (enabled ? theme::white : theme::faint);
        g.fillRoundedRectangle (p.getX() - 4.0f, p.getY() + p.getHeight() * 0.12f, 8.0f, p.getHeight() * 0.88f, 4.0f);

        g.setColour (theme::dim);
        g.setFont (theme::font (12.0f, true));
        g.drawText ("cola ~ " + juce::String (decaySec, 1) + " s   " + theme::U ("\xC2\xB7") + "   pre-delay " + juce::String ((int) std::lround (preSec * 1000.0f)) + " ms",
                    (int) all.getX() + 20, (int) all.getBottom() - 30, 360, 16, juce::Justification::centredLeft);
    }

private:
    std::atomic<float> *size {}, *damp {}, *pre {}, *mix {}, *on {};
};

class ReverbPanel : public PanelBase
{
public:
    explicit ReverbPanel (juce::AudioProcessorValueTreeState& a)
        : apvts (a), tail (a), onButton ("Activo"),
          kSize (a, ids::revSize, "Size", " %", 0),
          kDamp (a, ids::revDamp, "Damp", " %", 0),
          kPre (a, ids::revPre, "Pre-delay", " ms", 0),
          kWidth (a, ids::revWidth, "Width", " %", 0),
          kLow (a, ids::revLowCut, "Low cut", " Hz", 0),
          kHigh (a, ids::revHighCut, "High cut", " Hz", 0),
          kMix (a, ids::revMix, "Mix", " %", 0)
    {
        addAndMakeVisible (tail);
        addAndMakeVisible (onButton);
        onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ids::revOn, onButton);
        for (auto* k : { &kSize, &kDamp, &kPre, &kWidth, &kLow, &kHigh, &kMix }) addAndMakeVisible (*k);
        onParam = a.getRawParameterValue (ids::revOn);
    }

    void tick()
    {
        const float al = onParam->load() > 0.5f ? 1.0f : 0.45f;
        for (auto* k : { &kSize, &kDamp, &kPre, &kWidth, &kLow, &kHigh, &kMix }) k->setAlpha (al);
        tail.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawCard (g, layout::knobsCard().toFloat());
        theme::drawCard (g, layout::sideFull().toFloat());
        g.setColour (theme::dim);
        g.setFont (theme::font (12.0f));
        g.drawFittedText ("Reverb tipo sala con pre-delay y filtros en la cola. Low cut y High cut limpian los graves y los agudos de la reverb sin tocar la voz.\n\nMix = nivel de la reverb por encima de la voz seca.",
                          656, 150, 204, 250, juce::Justification::topLeft, 12);
    }

    void resized() override
    {
        tail.setBounds (layout::visualFull());
        onButton.setBounds (656, 92, 180, 28);
        layout::layoutKnobs ({ &kSize, &kDamp, &kPre, &kWidth, &kLow, &kHigh, &kMix });
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    TailView tail;
    juce::ToggleButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    Knob kSize, kDamp, kPre, kWidth, kLow, kHigh, kMix;
    std::atomic<float>* onParam = nullptr;
};
