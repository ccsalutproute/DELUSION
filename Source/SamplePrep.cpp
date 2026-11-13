// ======================================================================
//  SamplePrep : outil utilisé UNIQUEMENT pendant la compilation (GitHub).
//  Il lit les enregistrements libres de droits (WAV / FLAC), les nettoie
//  (silence, longueur, boucle sans clic, volume) et écrit des FLAC légers
//  + un fichier manifest.json que le plugin lit au démarrage.
//
//  Usage : SamplePrep job.json dossier_de_sortie
// ======================================================================
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <iostream>

namespace
{
    struct Proc
    {
        juce::AudioBuffer<float> buf;
        double sr = 44100.0;
        int loopStart = -1, loopEnd = -1;
        juce::var def;
    };

    float peakOf (const juce::AudioBuffer<float>& b)
    {
        float p = 0.f;
        for (int c = 0; c < b.getNumChannels(); ++c) p = juce::jmax (p, b.getMagnitude (c, 0, b.getNumSamples()));
        return p;
    }

    void cut (juce::AudioBuffer<float>& b, int start, int len)
    {
        juce::AudioBuffer<float> n (2, juce::jmax (1, len));
        n.clear();
        for (int c = 0; c < 2; ++c) n.copyFrom (c, 0, b, c, start, juce::jmin (len, b.getNumSamples() - start));
        b = std::move (n);
    }
}

int main (int argc, char* argv[])
{
    if (argc < 3) { std::cerr << "usage: SamplePrep job.json outdir\n"; return 1; }

    const juce::File jobFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File outDir  (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    outDir.createDirectory();

    const auto job = juce::JSON::parse (jobFile);
    if (! job.isObject()) { std::cerr << "bad job file\n"; return 1; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::FlacAudioFormat flac;

    juce::Array<juce::var> manifestInstruments;
    int total = 0;
    juce::int64 totalSamples = 0;

    for (auto& inst : *job["instruments"].getArray())
    {
        const juce::String id = inst["id"];
        const bool loop = (bool) inst["loop"];
        const double maxLen = inst.hasProperty ("maxLen") ? (double) inst["maxLen"] : 8.0;
        const float targetPeak = inst.hasProperty ("targetPeak") ? (float) (double) inst["targetPeak"] : 0.9f;

        std::vector<Proc> procs;
        for (auto& r : *inst["regions"].getArray())
        {
            const juce::File src (r["src"].toString());
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (src));
            if (reader == nullptr) { std::cerr << "  ! unreadable: " << src.getFullPathName() << "\n"; continue; }

            Proc p;
            p.def = r;
            p.sr = reader->sampleRate;
            const int n = (int) reader->lengthInSamples;
            p.buf.setSize (2, juce::jmax (1, n));
            reader->read (&p.buf, 0, n, 0, true, reader->numChannels > 1);
            if (reader->numChannels == 1) p.buf.copyFrom (1, 0, p.buf, 0, 0, n);

            const float g = juce::Decibels::decibelsToGain ((float) (double) r.getProperty ("gainDb", 0.0));
            p.buf.applyGain (g);

            // silence au début
            const float pk = juce::jmax (1.0e-6f, peakOf (p.buf));
            int start = 0;
            for (int i = 0; i < n; ++i)
                if (std::abs (p.buf.getSample (0, i)) > pk * 0.01f || std::abs (p.buf.getSample (1, i)) > pk * 0.01f) { start = i; break; }
            start = juce::jmax (0, start - (int) (0.002 * p.sr));
            int len = n - start;

            if (! loop)
            {
                // fin : on coupe quand le son est devenu inaudible
                int last = n - 1;
                for (int i = n - 1; i > start; --i)
                    if (std::abs (p.buf.getSample (0, i)) > pk * 0.0004f || std::abs (p.buf.getSample (1, i)) > pk * 0.0004f) { last = i; break; }
                len = juce::jmin (last - start + 1, (int) (maxLen * p.sr));
                cut (p.buf, start, len);
                const int fade = juce::jmin (len / 4, (int) (0.35 * p.sr));
                p.buf.applyGainRamp (len - fade, fade, 1.f, 0.f);
            }
            else
            {
                len = juce::jmin (len, (int) (maxLen * p.sr));
                cut (p.buf, start, len);
                // boucle : on fond la fin dans le début de la zone de boucle (aucun clic)
                const int lEnd = len - (int) (0.03 * p.sr);
                const int lStart = juce::jmax ((int) (0.35 * p.sr), (int) (lEnd * 0.42));
                const int X = juce::jmin ((int) (0.3 * p.sr), (lEnd - lStart) / 2, lStart);
                if (lEnd > lStart + 64 && X > 32)
                {
                    for (int c = 0; c < 2; ++c)
                    {
                        auto* d = p.buf.getWritePointer (c);
                        for (int i = 0; i < X; ++i)
                        {
                            const float w = (float) i / (float) X * juce::MathConstants<float>::halfPi;
                            const int a = lEnd - X + i, b = lStart - X + i;
                            d[a] = d[a] * std::cos (w) + d[b] * std::sin (w);
                        }
                    }
                    cut (p.buf, 0, lEnd);
                    p.loopStart = lStart;
                    p.loopEnd = lEnd;
                }
            }
            procs.push_back (std::move (p));
        }

        // volume : chaque zone est mise au même niveau, selon sa couche de vélocité
        // (les sources n'ont pas toutes le même volume d'enregistrement)
        juce::ignoreUnused (targetPeak);

        auto dir = outDir.getChildFile (id);
        dir.createDirectory();
        juce::Array<juce::var> regions;
        int idx = 0;
        for (auto& p : procs)
        {
            const float hv = (float) (int) p.def.getProperty ("hivel", 127);
            const float target = 0.9f * (0.4f + 0.6f * juce::jlimit (0.f, 1.f, hv / 127.f));
            p.buf.applyGain (target / juce::jmax (1.0e-6f, peakOf (p.buf)));
            const auto name = juce::String (idx++) + ".flac";
            auto f = dir.getChildFile (name);
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> w (flac.createWriterFor (os.get(), p.sr, 2, 16, {}, 5));
            if (w == nullptr) { std::cerr << "  ! cannot write " << f.getFullPathName() << "\n"; continue; }
            os.release();
            w->writeFromAudioSampleBuffer (p.buf, 0, p.buf.getNumSamples());
            w.reset();

            auto* o = new juce::DynamicObject();
            o->setProperty ("file", id + "/" + name);
            for (auto k : { "lokey", "hikey", "key", "lovel", "hivel" })
                o->setProperty (k, p.def[k]);
            o->setProperty ("tune", p.def.getProperty ("tune", 0.0));
            o->setProperty ("loopStart", p.loopStart);
            o->setProperty ("loopEnd", p.loopEnd);
            o->setProperty ("len", p.buf.getNumSamples());
            regions.add (juce::var (o));
            ++total;
            totalSamples += p.buf.getNumSamples();
        }

        auto* io = new juce::DynamicObject();
        io->setProperty ("id", id);
        io->setProperty ("loop", loop);
        io->setProperty ("regions", regions);
        manifestInstruments.add (juce::var (io));
        std::cout << id << " : " << regions.size() << " regions\n";
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("instruments", manifestInstruments);
    outDir.getChildFile ("manifest.json").replaceWithText (juce::JSON::toString (juce::var (root)));
    std::cout << "TOTAL " << total << " samples, " << (double) totalSamples * 4.0 / 1.0e6 << " MB in RAM (16-bit stereo)\n";
    return 0;
}
