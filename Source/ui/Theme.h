#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Punto Tune Pro - tema visual: negro / gris / blanco, bordes redondeados.
namespace theme
{
    inline const juce::Colour bg    { 0xff09090b };
    inline const juce::Colour card  { 0xff121215 };
    inline const juce::Colour card2 { 0xff1b1b20 };
    inline const juce::Colour line  { 0xff2a2a31 };
    inline const juce::Colour text  { 0xfff0f0f2 };
    inline const juce::Colour dim   { 0xff8a8a93 };
    inline const juce::Colour faint { 0xff4a4a52 };
    inline const juce::Colour white { 0xffffffff };

    // Texto con acentos: siempre UTF-8 explícito
    inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    inline void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f)
    {
        g.setColour (card);
        g.fillRoundedRectangle (r, radius);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    inline juce::String noteName (int midi)
    {
        static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int pc = ((midi % 12) + 12) % 12;
        const int oct = (int) std::floor ((double) midi / 12.0) - 1;
        return juce::String (n[pc]) + juce::String (oct);
    }

    inline bool isBlackKey (int pitchClass)
    {
        pitchClass = ((pitchClass % 12) + 12) % 12;
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
    }
}

class PuntoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PuntoLookAndFeel()
    {
        using namespace theme;
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, faint);
        setColour (juce::TextEditor::backgroundColourId, card2);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, faint);
        setColour (juce::TextEditor::highlightedTextColourId, text);
        setColour (juce::TextEditor::outlineColourId, line);
        setColour (juce::TextEditor::focusedOutlineColourId, dim);
        setColour (juce::CaretComponent::caretColourId, white);
        setColour (juce::ComboBox::backgroundColourId, card2);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141418));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, white);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::Label::textColourId, text);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return theme::font (13.5f, true); }
    juce::Font getPopupMenuFont() override                       { return theme::font (13.5f, true); }
    juce::Font getLabelFont (juce::Label&) override              { return theme::font (13.0f, true); }

    // Etiquetas sin recuadro (los valores bajo los knobs)
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));
        if (! label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            const auto font = getLabelFont (label);
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);
            auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), area, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textColourId, theme::text);
        l->setColour (juce::TextEditor::backgroundColourId, theme::card2);
        l->setColour (juce::TextEditor::textColourId, theme::text);
        l->setColour (juce::TextEditor::outlineColourId, theme::line);
        l->setColour (juce::TextEditor::focusedOutlineColourId, theme::dim);
        return l;
    }

    // ------------------------------------------------------------------ rotary
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto r = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
        const float cx = r.getCentreX(), cy = r.getCentreY(), rad = d * 0.5f;
        const float angle = startA + pos * (endA - startA);
        const bool hot = s.isMouseOverOrDragging();
        const bool enabled = s.isEnabled();

        const float trackR = rad - 3.0f;
        juce::Path track;
        track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startA, endA, true);
        g.setColour (theme::line);
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // knobs bipolares: el arco parte del centro
        float from = startA;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            from = startA + (float) s.valueToProportionOfLength (0.0) * (endA - startA);

        juce::Path val;
        val.addCentredArc (cx, cy, trackR, trackR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (enabled ? (hot ? theme::white : theme::text) : theme::faint);
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // cuerpo
        const float capR = rad * 0.70f;
        juce::ColourGradient grad (theme::card2.brighter (0.12f), cx, cy - capR, theme::card, cx, cy + capR, false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (hot ? theme::faint : theme::line);
        g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.2f);

        // aguja
        juce::Path p;
        p.addRoundedRectangle (-1.8f, -capR + 4.0f, 3.6f, capR * 0.42f, 1.8f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (enabled ? theme::white : theme::faint);
        g.fillPath (p);
    }

    // ------------------------------------------------------------------ slider horizontal (salida)
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float cy = (float) y + (float) height * 0.5f;
        auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) width, 4.0f);
        g.setColour (theme::line);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (theme::text);
        g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);
        g.setColour (s.isMouseOverOrDragging() ? theme::white : theme::text);
        g.fillEllipse (sliderPos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    }

    // ------------------------------------------------------------------ botones de texto (pestañas, bypass)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hot, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = r.getHeight() * 0.5f;
        if (b.getToggleState())
        {
            g.setColour (down ? theme::text : theme::white);
            g.fillRoundedRectangle (r, rad);
        }
        else
        {
            if (hot || down)
            {
                g.setColour (theme::card2);
                g.fillRoundedRectangle (r, rad);
            }
            g.setColour (theme::line);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (theme::font (12.5f, true));
        g.setColour (b.getToggleState() ? juce::Colours::black : theme::text);
        g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
    }

    // ------------------------------------------------------------------ interruptor
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float sw = 38.0f, sh = 20.0f;
        auto track = juce::Rectangle<float> (r.getX(), r.getCentreY() - sh * 0.5f, sw, sh);
        const bool on = b.getToggleState();
        g.setColour (on ? theme::white : theme::card2);
        g.fillRoundedRectangle (track, sh * 0.5f);
        g.setColour (on ? theme::white : theme::line);
        g.drawRoundedRectangle (track.reduced (0.5f), sh * 0.5f, 1.0f);
        const float tr = sh - 8.0f;
        const float tx = on ? track.getRight() - 4.0f - tr : track.getX() + 4.0f;
        g.setColour (on ? juce::Colours::black : theme::dim);
        g.fillEllipse (tx, track.getCentreY() - tr * 0.5f, tr, tr);

        g.setColour (b.isEnabled() ? (on ? theme::text : theme::dim) : theme::faint);
        g.setFont (theme::font (12.5f, true));
        g.drawText (b.getButtonText().toUpperCase(), (int) (track.getRight() + 10.0f), 0,
                    b.getWidth() - (int) (sw + 10.0f), b.getHeight(), juce::Justification::centredLeft);
    }

    // ------------------------------------------------------------------ combo
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (theme::card2);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (box.hasKeyboardFocus (true) ? theme::dim : theme::line);
        g.drawRoundedRectangle (r, 9.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) width - 18.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        g.setColour (box.isEnabled() ? theme::dim : theme::faint);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
