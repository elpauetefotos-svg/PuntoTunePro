// Test de humo: procesa audio por toda la cadena y guarda capturas de la interfaz.
// Uso:  smoke <carpeta_salida>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void fillVoice (juce::AudioBuffer<float>& b, double& phase, double f0, double sr)
{
    for (int n = 0; n < b.getNumSamples(); ++n)
    {
        phase += f0 / sr; phase -= std::floor (phase);
        const float v = 0.25f * (float) (2.0 * phase - 1.0);
        for (int c = 0; c < b.getNumChannels(); ++c) b.setSample (c, n, v);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? argv[1] : ".");
    outDir.createDirectory();

    const double sr = 48000.0;
    const int block = 1600;                 // ~33 ms
    PuntoTuneAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    auto setParam = [&] (const char* id, float real)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (real));
    };
    setParam (ids::compOn, 1.0f);
    setParam (ids::delayOn, 1.0f);
    setParam (ids::revOn, 1.0f);
    setParam (ids::speed, 0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (900, 640);

    // --- audio: +30 cents sobre A3 y luego C4 desafinado
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double phase = 0.0;
    bool finite = true;
    float maxOut = 0.0f;

    auto runBlocks = [&] (int count, double f0)
    {
        for (int i = 0; i < count; ++i)
        {
            fillVoice (buf, phase, f0, sr);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < block; ++n)
                {
                    const float v = buf.getSample (c, n);
                    if (! std::isfinite (v)) finite = false;
                    maxOut = juce::jmax (maxOut, std::abs (v));
                }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
    };

    runBlocks (60, 220.0 * std::pow (2.0, 30.0 / 1200.0));
    runBlocks (60, 261.63 * std::pow (2.0, -25.0 / 1200.0));
    runBlocks (60, 293.66 * std::pow (2.0, 20.0 / 1200.0));

    std::printf ("salida finita: %s | pico: %.3f | nota objetivo final: %.2f | detectada: %.2f\n",
                 finite ? "si" : "NO", maxOut, proc.getTargetMidi(), proc.getDetectedMidi());

    // --- capturas de cada pestaña
    juce::PNGImageFormat png;
    const char* names[] = { "Tune", "Comp", "Delay", "Reverb" };
    for (auto* name : names)
    {
        for (auto* child : ed->getChildren())
            if (auto* tb = dynamic_cast<juce::TextButton*> (child))
                if (tb->getButtonText() == name) tb->triggerClick();

        runBlocks (6, 293.66 * std::pow (2.0, 20.0 / 1200.0));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f = outDir.getChildFile (juce::String (name).toLowerCase() + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    }

    ed.reset();
    return finite && maxOut > 0.01f ? 0 : 1;
}
