#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    // global
    inline constexpr const char* bypass   = "bypass";
    inline constexpr const char* outGain  = "out_gain";

    // autotune
    inline constexpr const char* speed    = "tune_speed";
    inline constexpr const char* amount   = "tune_amount";
    inline constexpr const char* humanize = "tune_humanize";
    inline constexpr const char* transpose= "tune_transpose";
    inline constexpr const char* fine     = "tune_fine";
    inline constexpr const char* refA     = "tune_refa";
    inline const juce::String note (int i) { return "note_" + juce::String (i); }

    // compresor
    inline constexpr const char* compOn      = "comp_on";
    inline constexpr const char* compThresh  = "comp_thresh";
    inline constexpr const char* compRatio   = "comp_ratio";
    inline constexpr const char* compAttack  = "comp_attack";
    inline constexpr const char* compRelease = "comp_release";
    inline constexpr const char* compMakeup  = "comp_makeup";
    inline constexpr const char* compMix     = "comp_mix";

    // delay
    inline constexpr const char* delayOn       = "delay_on";
    inline constexpr const char* delayTime     = "delay_time";
    inline constexpr const char* delaySync     = "delay_sync";
    inline constexpr const char* delayDiv      = "delay_div";
    inline constexpr const char* delayFeedback = "delay_fb";
    inline constexpr const char* delayTone     = "delay_tone";
    inline constexpr const char* delayPing     = "delay_ping";
    inline constexpr const char* delayMix      = "delay_mix";

    // reverb
    inline constexpr const char* revOn      = "rev_on";
    inline constexpr const char* revSize    = "rev_size";
    inline constexpr const char* revDamp    = "rev_damp";
    inline constexpr const char* revPre     = "rev_pre";
    inline constexpr const char* revWidth   = "rev_width";
    inline constexpr const char* revLowCut  = "rev_lowcut";
    inline constexpr const char* revHighCut = "rev_highcut";
    inline constexpr const char* revMix     = "rev_mix";
}

// Escalas (intervalos desde la tónica). Índice 0 = cromática.
struct ScaleDef { const char* name; int count; int notes[12]; };

inline const ScaleDef* getScales (int& n)
{
    static const ScaleDef scales[] = {
        { "Cromática",          12, { 0,1,2,3,4,5,6,7,8,9,10,11 } },
        { "Mayor",               7, { 0,2,4,5,7,9,11 } },
        { "Menor natural",       7, { 0,2,3,5,7,8,10 } },
        { "Menor armónica",      7, { 0,2,3,5,7,8,11 } },
        { "Pentatónica mayor",   5, { 0,2,4,7,9 } },
        { "Pentatónica menor",   5, { 0,3,5,7,10 } },
        { "Blues",               6, { 0,3,5,6,7,10 } },
        { "Dórica",              7, { 0,2,3,5,7,9,10 } },
        { "Frigia dominante",    7, { 0,1,4,5,7,8,10 } },
    };
    n = (int) (sizeof (scales) / sizeof (scales[0]));
    return scales;
}

inline const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
