#pragma once
#include "Theme.h"
#include "../Params.h"
#include <array>

// ============================================================================
//  Knob con título y valor
// ============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& title,
          const juce::String& suffix = {}, int decimals = 1)
        : titleText (title.toUpperCase())
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 20);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

        // el attachment fija su propio formateo; lo sustituimos por uno con unidades
        slider.textFromValueFunction = [suffix, decimals] (double v) { return juce::String (v, decimals) + suffix; };
        slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        slider.updateText();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (isEnabled() ? theme::dim : theme::faint);
        g.setFont (theme::font (11.5f, true));
        g.drawText (titleText, 0, 0, getWidth(), 16, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop (16);
        slider.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 118), area.getHeight()));
    }

    juce::Slider slider;

private:
    juce::String titleText;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ============================================================================
//  Barra de nivel horizontal (peak con caída)
// ============================================================================
class LevelBar : public juce::Component
{
public:
    explicit LevelBar (const juce::String& label) : name (label) {}

    void setLevel (float linear)
    {
        shown = juce::jmax (linear, shown * 0.86f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::dim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (name, 0, 0, 28, getHeight(), juce::Justification::centredLeft);

        auto bar = r.withTrimmedLeft (30.0f).withSizeKeepingCentre (r.getWidth() - 30.0f, 6.0f);
        g.setColour (theme::line);
        g.fillRoundedRectangle (bar, 3.0f);

        const float db = juce::Decibels::gainToDecibels (shown, -60.0f);
        const float prop = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
        g.setColour (db > -3.0f ? theme::white : juce::Colour (0xffb4b4bc));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (0.0f, bar.getWidth() * prop)), 3.0f);
    }

private:
    juce::String name;
    float shown = 0.0f;
};

// ============================================================================
//  Gráfico de pitch en vivo (estilo piano roll)
// ============================================================================
class PitchView : public juce::Component
{
public:
    explicit PitchView (juce::AudioProcessorValueTreeState& a) : apvts (a)
    {
        for (int i = 0; i < 12; ++i)
            noteRaw[(size_t) i] = apvts.getRawParameterValue (ids::note (i));
        det.fill (-1.0f);
        cor.fill (-1.0f);
    }

    void push (float detected, float corrected, float target)
    {
        det[(size_t) head] = detected;
        cor[(size_t) head] = corrected;
        head = (head + 1) % N;
        targetMidi = target;

        const float ref = detected > 0.0f ? detected : -1.0f;
        if (ref > 0.0f) centre += 0.05f * (ref - centre);
        centre = juce::jlimit (30.0f, 100.0f, centre);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto all = getLocalBounds().toFloat();
        theme::drawCard (g, all);

        const float labelW = 44.0f;
        auto plot = all.reduced (1.0f).withTrimmedLeft (labelW);
        plot = plot.withTrimmedRight (10.0f).reduced (0.0f, 8.0f);

        uint16_t mask = 0;
        for (int i = 0; i < 12; ++i)
            if (noteRaw[(size_t) i]->load() > 0.5f) mask |= (uint16_t) (1u << i);

        const float lo = centre - visible * 0.5f;
        auto yOf = [&] (float midi) { return plot.getBottom() - (midi - lo) / visible * plot.getHeight(); };

        // --- filas de notas
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (all.reduced (1.0f).toNearestInt());

            const int m0 = (int) std::floor (lo) - 1, m1 = (int) std::ceil (lo + visible) + 1;
            for (int m = m0; m <= m1; ++m)
            {
                const int pc = ((m % 12) + 12) % 12;
                const bool allowed = (mask >> pc) & 1;
                const float yTop = yOf ((float) m + 0.5f), yBot = yOf ((float) m - 0.5f);

                if (theme::isBlackKey (pc))
                {
                    g.setColour (juce::Colours::white.withAlpha (0.025f));
                    g.fillRect (juce::Rectangle<float> (all.getX() + 1.0f, yTop, all.getWidth() - 2.0f, yBot - yTop));
                }
                if (allowed && (float) m == targetMidi)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.10f));
                    g.fillRect (juce::Rectangle<float> (plot.getX(), yTop, plot.getWidth() + 10.0f, yBot - yTop));
                }

                g.setColour (juce::Colours::white.withAlpha (allowed ? 0.20f : 0.05f));
                g.drawHorizontalLine ((int) yOf ((float) m), plot.getX(), plot.getRight() + 10.0f);

                g.setColour (allowed ? theme::text : theme::faint);
                g.setFont (theme::font (11.0f, allowed));
                g.drawText (theme::noteName (m), (int) all.getX() + 10, (int) (yTop), (int) labelW - 10,
                            (int) juce::jmax (1.0f, yBot - yTop), juce::Justification::centredLeft);
            }

            // --- trazos
            auto makePath = [&] (const std::array<float, N>& v)
            {
                juce::Path p;
                bool pen = false;
                for (int i = 0; i < N; ++i)
                {
                    const float val = v[(size_t) ((head + i) % N)];
                    const float x = plot.getX() + plot.getWidth() * (float) i / (float) (N - 1);
                    if (val < 0.0f) { pen = false; continue; }
                    const float y = yOf (val);
                    if (! pen) { p.startNewSubPath (x, y); pen = true; }
                    else        p.lineTo (x, y);
                }
                return p;
            };

            g.setColour (theme::dim.withAlpha (0.85f));
            g.strokePath (makePath (det), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            auto corrected = makePath (cor);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.strokePath (corrected, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (theme::white);
            g.strokePath (corrected, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // leyenda
        g.setFont (theme::font (10.5f, true));
        const float lx = all.getRight() - 112.0f, ly = all.getY() + 12.0f;
        g.setColour (theme::dim);
        g.fillEllipse (lx, ly + 3.0f, 6.0f, 6.0f);
        g.drawText ("ENTRADA", (int) lx + 10, (int) ly, 60, 12, juce::Justification::centredLeft);
        g.setColour (theme::white);
        g.fillEllipse (lx + 62.0f, ly + 3.0f, 6.0f, 6.0f);
        g.drawText ("SALIDA", (int) lx + 72, (int) ly, 40, 12, juce::Justification::centredLeft);
    }

private:
    static constexpr int N = 210;               // ~7 s a 30 fps
    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::atomic<float>*, 12> noteRaw {};
    std::array<float, N> det {}, cor {};
    int head = 0;
    float centre = 57.0f, visible = 19.0f, targetMidi = -1.0f;
};

// ============================================================================
//  Teclado de notas permitidas
// ============================================================================
class NoteKeyboard : public juce::Component
{
public:
    explicit NoteKeyboard (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < 12; ++i)
        {
            auto* param = apvts.getParameter (ids::note (i));
            attach[(size_t) i] = std::make_unique<juce::ParameterAttachment> (*param,
                [this, i] (float v) { enabled[(size_t) i] = v > 0.5f; repaint(); });
            attach[(size_t) i]->sendInitialUpdate();
        }
    }

    void setActivePitchClass (int pc)
    {
        if (pc != active) { active = pc; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        theme::drawCard (g, getLocalBounds().toFloat());
        auto area = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
        const float wk = area.getWidth() / 7.0f;
        static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };

        for (int k = 0; k < 7; ++k)
        {
            const int pc = whitePc[k];
            auto kr = juce::Rectangle<float> (area.getX() + wk * (float) k + 1.5f, area.getY(), wk - 3.0f, area.getHeight());
            juce::Path p;
            p.addRoundedRectangle (kr.getX(), kr.getY(), kr.getWidth(), kr.getHeight(), 7.0f, 7.0f, false, false, true, true);
            const bool on = enabled[(size_t) pc];
            g.setColour (on ? theme::text : theme::card2);
            g.fillPath (p);
            g.setColour (on ? theme::text : theme::line);
            g.strokePath (p, juce::PathStrokeType (1.0f));

            g.setColour (on ? juce::Colours::black : theme::dim);
            g.setFont (theme::font (12.0f, true));
            g.drawText (noteNames[pc], kr.withTrimmedBottom (7.0f), juce::Justification::centredBottom);

            if (pc == active)
            {
                g.setColour (on ? juce::Colours::black : theme::white);
                g.fillEllipse (kr.getCentreX() - 3.5f, kr.getBottom() - 30.0f, 7.0f, 7.0f);
            }
        }

        for (int pc : { 1, 3, 6, 8, 10 })
        {
            auto kr = blackRect (pc, area);
            const bool on = enabled[(size_t) pc];
            g.setColour (on ? juce::Colour (0xffb9b9c1) : juce::Colour (0xff07070a));
            g.fillRoundedRectangle (kr, 5.0f);
            g.setColour (on ? theme::white : theme::line);
            g.drawRoundedRectangle (kr.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (on ? juce::Colours::black : theme::faint);
            g.setFont (theme::font (10.0f, true));
            g.drawText (noteNames[pc], kr.withTrimmedBottom (5.0f), juce::Justification::centredBottom);

            if (pc == active)
            {
                g.setColour (on ? juce::Colours::black : theme::white);
                g.fillEllipse (kr.getCentreX() - 3.0f, kr.getBottom() - 24.0f, 6.0f, 6.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int pc = pitchClassAt (e.position);
        if (pc < 0) return;
        dragValue = ! enabled[(size_t) pc];
        set (pc, dragValue);
        lastDragged = pc;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int pc = pitchClassAt (e.position);
        if (pc < 0 || pc == lastDragged) return;
        set (pc, dragValue);
        lastDragged = pc;
    }

    void mouseUp (const juce::MouseEvent&) override { lastDragged = -1; }

private:
    juce::Rectangle<float> blackRect (int pc, juce::Rectangle<float> area) const
    {
        const float wk = area.getWidth() / 7.0f;
        const float bw = wk * 0.62f, bh = area.getHeight() * 0.60f;
        float boundary = 0.0f;                       // posición (en teclas blancas) del borde izquierdo
        switch (pc) { case 1: boundary = 1; break; case 3: boundary = 2; break;
                      case 6: boundary = 4; break; case 8: boundary = 5; break; default: boundary = 6; break; }
        return { area.getX() + wk * boundary - bw * 0.5f, area.getY(), bw, bh };
    }

    int pitchClassAt (juce::Point<float> pos) const
    {
        auto area = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
        if (! area.contains (pos)) return -1;
        for (int pc : { 1, 3, 6, 8, 10 })
            if (blackRect (pc, area).contains (pos)) return pc;
        static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const int k = juce::jlimit (0, 6, (int) ((pos.x - area.getX()) / (area.getWidth() / 7.0f)));
        return whitePc[k];
    }

    void set (int pc, bool on) { attach[(size_t) pc]->setValueAsCompleteGesture (on ? 1.0f : 0.0f); }

    std::array<std::unique_ptr<juce::ParameterAttachment>, 12> attach;
    std::array<bool, 12> enabled {};
    int active = -1, lastDragged = -1;
    bool dragValue = false;
};

// ============================================================================
//  Lectura de nota + barra de cents
// ============================================================================
class NoteReadout : public juce::Component
{
public:
    void update (float detected, float target)
    {
        det = detected; tgt = target;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::drawCard (g, r);

        g.setColour (theme::dim);
        g.setFont (theme::font (11.0f, true));
        g.drawText ("NOTA OBJETIVO", 18, 14, getWidth() - 36, 14, juce::Justification::centredLeft);

        const bool has = det > 0.0f;
        const int shownMidi = tgt >= 0.0f ? (int) std::lround (tgt) : (int) std::lround (det);

        g.setColour (has ? theme::white : theme::faint);
        g.setFont (theme::font (62.0f, true));
        g.drawText (has ? theme::noteName (shownMidi) : juce::String::fromUTF8 ("\xE2\x80\x94"),
                    0, 44, getWidth(), 76, juce::Justification::centred);

        // cents
        const float cents = (has && tgt >= 0.0f) ? juce::jlimit (-99.0f, 99.0f, (det - tgt) * 100.0f) : 0.0f;
        g.setColour (theme::dim);
        g.setFont (theme::font (11.0f, true));
        g.drawText ("ENTRADA", 18, 138, 80, 14, juce::Justification::centredLeft);
        g.setColour (has ? theme::text : theme::faint);
        g.setFont (theme::font (15.0f, true));
        g.drawText (has ? ((cents > 0.0f ? "+" : "") + juce::String ((int) std::lround (cents)) + " cents") : juce::String ("sin voz"),
                    getWidth() - 140, 136, 122, 18, juce::Justification::centredRight);

        auto bar = juce::Rectangle<float> (18.0f, 172.0f, (float) getWidth() - 36.0f, 6.0f);
        g.setColour (theme::line);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (theme::faint);
        g.fillRect (bar.getCentreX() - 1.0f, bar.getY() - 5.0f, 2.0f, bar.getHeight() + 10.0f);
        if (has)
        {
            const float px = bar.getCentreX() + (cents / 50.0f) * (bar.getWidth() * 0.5f);
            const float cx = juce::jlimit (bar.getX(), bar.getRight(), px);
            g.setColour (theme::white);
            g.fillEllipse (cx - 6.0f, bar.getCentreY() - 6.0f, 12.0f, 12.0f);
        }
        g.setColour (theme::faint);
        g.setFont (theme::font (10.0f, true));
        g.drawText ("-50", 18, 186, 40, 12, juce::Justification::centredLeft);
        g.drawText ("+50", getWidth() - 58, 186, 40, 12, juce::Justification::centredRight);
    }

private:
    float det = -1.0f, tgt = -1.0f;
};
