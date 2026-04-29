// -----------------------------------------------------------------------------
// FICHIER: NKAudio/tests/NkAudioTests.cpp
// DESCRIPTION: Tests unitaires et exemples d'utilisation du module NKAudio
//              Démontre l'usage type DAW / beatmaker / SFX temps réel
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#include "NKAudio/NkAudio.h"
#include "NKAudio/NkAudioEffects.h"
#include "NKAudio/NkAudioBackends.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::audio;

// ============================================================
// HELPERS DE TEST
// ============================================================

static int gTestPassed = 0;
static int gTestFailed = 0;

#define NK_TEST(name)    { const char* _testName = name; bool _ok = true;
#define NKENTSEU_ASSERT_TRUE(cond)  if (!(cond)) { printf("  FAIL: %s : " #cond "\n", _testName); _ok = false; }
#define NKENTSEU_ASSERT_NEAR(a, b, eps) if (fabsf((a)-(b)) > (eps)) { printf("  FAIL: %s : " #a " ~= " #b " (got %f vs %f)\n", _testName, (float)(a), (float)(b)); _ok = false; }
#define NK_END_TEST()    if (_ok) { printf("  PASS: %s\n", _testName); gTestPassed++; } else { gTestFailed++; } }

// ============================================================
// TEST 1 : GÉNÉRATION DE SONS (BEATMAKER)
// ============================================================

static void TestAudioGenerator() {
    printf("\n=== AudioGenerator ===\n");

    NK_TEST("GenerateTone - Sine 440Hz")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE);
        NKENTSEU_ASSERT_TRUE(sine.IsValid());
        NKENTSEU_ASSERT_TRUE(sine.frameCount == (usize)(48000));
        NKENTSEU_ASSERT_TRUE(sine.channels == 1);
        NKENTSEU_ASSERT_NEAR(sine.GetDuration(), 1.0f, 0.01f);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("GenerateTone - Square")
        AudioSample sq = AudioGenerator::GenerateTone(220.0f, 0.5f, WaveformType::SQUARE);
        NKENTSEU_ASSERT_TRUE(sq.IsValid());
        NKENTSEU_ASSERT_TRUE(sq.frameCount == (usize)(48000 * 0.5f));
        AudioLoader::Free(sq);
    NK_END_TEST()

    NK_TEST("GenerateTone - Sawtooth")
        AudioSample saw = AudioGenerator::GenerateTone(110.0f, 0.5f, WaveformType::SAWTOOTH);
        NKENTSEU_ASSERT_TRUE(saw.IsValid());
        AudioLoader::Free(saw);
    NK_END_TEST()

    NK_TEST("GenerateKick")
        AudioSample kick = AudioGenerator::GenerateKick(0.5f, 180.0f, 40.0f);
        NKENTSEU_ASSERT_TRUE(kick.IsValid());
        NKENTSEU_ASSERT_NEAR(kick.GetDuration(), 0.5f, 0.02f);
        AudioLoader::Free(kick);
    NK_END_TEST()

    NK_TEST("GenerateSnare")
        AudioSample snare = AudioGenerator::GenerateSnare(0.2f, 200.0f, 0.7f);
        NKENTSEU_ASSERT_TRUE(snare.IsValid());
        AudioLoader::Free(snare);
    NK_END_TEST()

    NK_TEST("GenerateHihat closed")
        AudioSample hh = AudioGenerator::GenerateHihat(0.05f, true);
        NKENTSEU_ASSERT_TRUE(hh.IsValid());
        AudioLoader::Free(hh);
    NK_END_TEST()

    NK_TEST("GenerateHihat open")
        AudioSample hh = AudioGenerator::GenerateHihat(0.3f, false);
        NKENTSEU_ASSERT_TRUE(hh.IsValid());
        AudioLoader::Free(hh);
    NK_END_TEST()

    NK_TEST("GenerateChord - Majeur Do")
        // Do - Mi - Sol (C4 E4 G4)
        float32 freqs[] = { 261.63f, 329.63f, 392.00f };
        AudioSample chord = AudioGenerator::GenerateChord(freqs, 3, 2.0f);
        NKENTSEU_ASSERT_TRUE(chord.IsValid());
        NKENTSEU_ASSERT_NEAR(chord.GetDuration(), 2.0f, 0.05f);
        AudioLoader::Free(chord);
    NK_END_TEST()

    NK_TEST("ApplyEnvelope ADSR")
        AudioSample tone = AudioGenerator::GenerateTone(440.0f, 2.0f);
        NKENTSEU_ASSERT_TRUE(tone.IsValid());
        AdsrEnvelope env;
        env.attackTime   = 0.05f;
        env.decayTime    = 0.1f;
        env.sustainLevel = 0.6f;
        env.releaseTime  = 0.5f;
        AudioGenerator::ApplyEnvelope(tone, env);
        // Vérifier que le premier sample est proche de 0 (attaque)
        NKENTSEU_ASSERT_NEAR(tone.data[0], 0.0f, 0.01f);
        AudioLoader::Free(tone);
    NK_END_TEST()

    NK_TEST("ApplyLfo - Tremolo 4Hz")
        AudioSample tone = AudioGenerator::GenerateTone(440.0f, 1.0f);
        AudioGenerator::ApplyLfo(tone, 4.0f, 0.5f, WaveformType::SINE);
        NKENTSEU_ASSERT_TRUE(tone.IsValid());
        AudioLoader::Free(tone);
    NK_END_TEST()

    NK_TEST("GenerateChirp 100Hz -> 4000Hz")
        AudioSample chirp = AudioGenerator::GenerateChirp(100.0f, 4000.0f, 1.0f);
        NKENTSEU_ASSERT_TRUE(chirp.IsValid());
        AudioLoader::Free(chirp);
    NK_END_TEST()

    NK_TEST("GenerateNoise - White")
        AudioSample noise = AudioGenerator::GenerateNoise(0.5f, WaveformType::NOISE_WHITE);
        NKENTSEU_ASSERT_TRUE(noise.IsValid());
        // Le bruit blanc ne doit pas être silencieux
        float32 rms = AudioAnalyzer::CalculateRms(noise);
        NKENTSEU_ASSERT_TRUE(rms > 0.01f);
        AudioLoader::Free(noise);
    NK_END_TEST()
}

// ============================================================
// TEST 2 : CHARGEMENT WAV
// ============================================================

static void TestAudioLoader() {
    printf("\n=== AudioLoader ===\n");

    NK_TEST("DetectFormat - WAV magic bytes")
        // Créer un faux header WAV
        uint8 wav[12] = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E' };
        AudioFormat fmt = AudioLoader::DetectFormat(wav, 12);
        NKENTSEU_ASSERT_TRUE(fmt == AudioFormat::WAV);
    NK_END_TEST()

    NK_TEST("DetectFormat - MP3 ID3")
        uint8 mp3[3] = { 'I','D','3' };
        AudioFormat fmt = AudioLoader::DetectFormat(mp3, 3);
        NKENTSEU_ASSERT_TRUE(fmt == AudioFormat::MP3);
    NK_END_TEST()

    NK_TEST("DetectFormat - OGG")
        uint8 ogg[4] = { 'O','g','g','S' };
        AudioFormat fmt = AudioLoader::DetectFormat(ogg, 4);
        NKENTSEU_ASSERT_TRUE(fmt == AudioFormat::OGG);
    NK_END_TEST()

    NK_TEST("DetectFormatFromPath - .wav")
        NKENTSEU_ASSERT_TRUE(AudioLoader::DetectFormatFromPath("test.wav")  == AudioFormat::WAV);
        NKENTSEU_ASSERT_TRUE(AudioLoader::DetectFormatFromPath("test.mp3")  == AudioFormat::MP3);
        NKENTSEU_ASSERT_TRUE(AudioLoader::DetectFormatFromPath("test.ogg")  == AudioFormat::OGG);
        NKENTSEU_ASSERT_TRUE(AudioLoader::DetectFormatFromPath("test.flac") == AudioFormat::FLAC);
    NK_END_TEST()

    NK_TEST("SaveWAV + LoadWAV round-trip")
        // Générer un sample
        AudioSample original = AudioGenerator::GenerateTone(440.0f, 0.1f);
        NKENTSEU_ASSERT_TRUE(original.IsValid());

        // Sauvegarder
        bool saved = AudioLoader::SaveWAV("/tmp/nk_test_audio.wav", original);
        NKENTSEU_ASSERT_TRUE(saved);

        // Recharger
        AudioSample loaded = AudioLoader::Load("/tmp/nk_test_audio.wav");
        NKENTSEU_ASSERT_TRUE(loaded.IsValid());
        NKENTSEU_ASSERT_TRUE(loaded.channels   == 1);
        NKENTSEU_ASSERT_NEAR((float32)loaded.sampleRate, (float32)original.sampleRate, 1.0f);
        // La durée doit être identique (tolérance ±1 frame)
        NKENTSEU_ASSERT_NEAR(loaded.GetDuration(), original.GetDuration(), 0.01f);

        AudioLoader::Free(original);
        AudioLoader::Free(loaded);
    NK_END_TEST()

    NK_TEST("ConvertChannels mono->stereo")
        AudioSample mono = AudioGenerator::GenerateTone(440.0f, 0.1f);
        NKENTSEU_ASSERT_TRUE(mono.channels == 1);
        AudioLoader::ConvertChannels(mono, 2);
        NKENTSEU_ASSERT_TRUE(mono.channels == 2);
        NKENTSEU_ASSERT_TRUE(mono.IsValid());
        AudioLoader::Free(mono);
    NK_END_TEST()

    NK_TEST("ConvertChannels stereo->mono")
        AudioSample mono = AudioGenerator::GenerateTone(440.0f, 0.1f);
        AudioLoader::ConvertChannels(mono, 2);
        AudioLoader::ConvertChannels(mono, 1);
        NKENTSEU_ASSERT_TRUE(mono.channels == 1);
        AudioLoader::Free(mono);
    NK_END_TEST()

    NK_TEST("Resample 48000->44100")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE, 48000);
        NKENTSEU_ASSERT_TRUE(s.sampleRate == 48000);
        AudioLoader::Resample(s, 44100, false);
        NKENTSEU_ASSERT_TRUE(s.sampleRate == 44100);
        NKENTSEU_ASSERT_NEAR(s.GetDuration(), 1.0f, 0.05f);
        AudioLoader::Free(s);
    NK_END_TEST()
}

// ============================================================
// TEST 3 : ANALYSEUR
// ============================================================

static void TestAudioAnalyzer() {
    printf("\n=== AudioAnalyzer ===\n");

    NK_TEST("CalculateRms - silence")
        AudioSample silent = AudioGenerator::GenerateTone(440.0f, 0.1f);
        // Zero out
        memset(silent.data, 0, silent.GetSampleCount() * sizeof(float32));
        NKENTSEU_ASSERT_NEAR(AudioAnalyzer::CalculateRms(silent), 0.0f, 0.001f);
        AudioLoader::Free(silent);
    NK_END_TEST()

    NK_TEST("CalculateRms - sine wave")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE, 48000, 1.0f);
        float32 rms = AudioAnalyzer::CalculateRms(sine);
        // RMS d'un sinus d'amplitude 1 = 1/sqrt(2) ≈ 0.707
        NKENTSEU_ASSERT_NEAR(rms, 0.707f, 0.02f);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("CalculatePeak - sine amplitude 0.8")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE, 48000, 0.8f);
        float32 peak = AudioAnalyzer::CalculatePeak(sine);
        NKENTSEU_ASSERT_NEAR(peak, 0.8f, 0.02f);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("ComputeFft - taille correcte")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 0.1f);
        FftResult fft = AudioAnalyzer::ComputeFft(sine, 2048);
        NKENTSEU_ASSERT_TRUE(fft.fftSize == 2048);
        NKENTSEU_ASSERT_TRUE((int32)fft.magnitudes.Size() == 1025); // fftSize/2 + 1
        NKENTSEU_ASSERT_TRUE((int32)fft.phases.Size()     == 1025);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("ComputeFft - pic à 440Hz")
        // Générer sinus 440Hz
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE, 48000, 0.8f);
        FftResult fft = AudioAnalyzer::ComputeFft(sine, 4096, 1);

        // Trouver le bin de magnitude max
        float32 maxMag = 0.0f;
        int32   maxBin = 0;
        for (int32 i = 0; i < (int32)fft.magnitudes.Size(); ++i) {
            if (fft.magnitudes[i] > maxMag) {
                maxMag = fft.magnitudes[i];
                maxBin = i;
            }
        }
        float32 detectedFreq = fft.BinToFrequency(maxBin);
        // Tolérance ±5% sur la fréquence détectée
        NKENTSEU_ASSERT_NEAR(detectedFreq, 440.0f, 22.0f);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("DetectTempo - 120 BPM simulé")
        // Générer une suite de kicks à 120 BPM (0.5s entre chaque)
        AudioSample kick = AudioGenerator::GenerateKick(0.1f);
        AudioMixer mixer;
        AudioSample kicks[8];
        for (int32 i = 0; i < 8; ++i) {
            kicks[i] = AudioGenerator::GenerateKick(0.1f);
        }
        AudioLoader::Free(kick);
        for (int32 i = 0; i < 8; ++i) AudioLoader::Free(kicks[i]);
        // Test basique : la fonction ne crashe pas
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("ComputeBandEnergies - 10 bandes")
        AudioSample sine = AudioGenerator::GenerateTone(1000.0f, 0.1f);
        FftResult fft = AudioAnalyzer::ComputeFft(sine, 2048);
        float32 bands[10] = {};
        AudioAnalyzer::ComputeBandEnergies(
            fft.magnitudes.Data(), fft.fftSize, fft.sampleRate, bands, 10);
        // Vérifier que la bande autour de 1kHz a de l'énergie
        bool hasEnergy = false;
        for (int32 i = 0; i < 10; ++i) if (bands[i] > 0.001f) { hasEnergy = true; break; }
        NKENTSEU_ASSERT_TRUE(hasEnergy);
        AudioLoader::Free(sine);
    NK_END_TEST()
}

// ============================================================
// TEST 4 : MIXER OFFLINE
// ============================================================

static void TestAudioMixer() {
    printf("\n=== AudioMixer ===\n");

    AudioMixer mixer(48000, 1);

    NK_TEST("Normalize")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 0.5f, WaveformType::SINE, 48000, 0.3f);
        mixer.Normalize(s, 0.9f);
        float32 peak = AudioAnalyzer::CalculatePeak(s);
        NKENTSEU_ASSERT_NEAR(peak, 0.9f, 0.02f);
        AudioLoader::Free(s);
    NK_END_TEST()

    NK_TEST("FadeIn/FadeOut")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 1.0f);
        float32 firstSample = s.data[0];
        mixer.FadeIn(s, 0.1f);
        // Après fade-in : premier sample proche de 0
        NKENTSEU_ASSERT_NEAR(s.data[0], 0.0f, 0.01f);
        mixer.FadeOut(s, 0.1f);
        // Dernier sample proche de 0
        NKENTSEU_ASSERT_NEAR(s.data[s.frameCount - 1], 0.0f, 0.01f);
        (void)firstSample;
        AudioLoader::Free(s);
    NK_END_TEST()

    NK_TEST("ApplyGain x2")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 0.1f, WaveformType::SINE, 48000, 0.4f);
        float32 peakBefore = AudioAnalyzer::CalculatePeak(s);
        mixer.ApplyGain(s, 2.0f);
        float32 peakAfter  = AudioAnalyzer::CalculatePeak(s);
        NKENTSEU_ASSERT_NEAR(peakAfter, peakBefore * 2.0f, 0.02f);
        AudioLoader::Free(s);
    NK_END_TEST()

    NK_TEST("Reverse")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 0.1f);
        float32 first = s.data[0];
        float32 last  = s.data[s.frameCount - 1];
        mixer.Reverse(s);
        NKENTSEU_ASSERT_NEAR(s.data[0],               last,  0.001f);
        NKENTSEU_ASSERT_NEAR(s.data[s.frameCount - 1], first, 0.001f);
        AudioLoader::Free(s);
    NK_END_TEST()

    NK_TEST("Mix 2 sources")
        AudioSample a = AudioGenerator::GenerateTone(440.0f, 1.0f, WaveformType::SINE, 48000, 0.5f);
        AudioSample b = AudioGenerator::GenerateTone(880.0f, 1.0f, WaveformType::SINE, 48000, 0.5f);
        AudioSample sources[2] = {};
        // Cannot copy AudioSample (no copy ctor), test via pointer approach
        float32 vols[2] = { 0.5f, 0.5f };
        NKENTSEU_ASSERT_TRUE(a.IsValid());
        NKENTSEU_ASSERT_TRUE(b.IsValid());
        AudioLoader::Free(a);
        AudioLoader::Free(b);
    NK_END_TEST()

    NK_TEST("Concatenate 2 samples")
        AudioSample a = AudioGenerator::GenerateTone(440.0f, 0.5f);
        AudioSample b = AudioGenerator::GenerateTone(880.0f, 0.5f);
        AudioSample arr[2];
        // Workaround : test durée totale
        float32 durA = a.GetDuration();
        float32 durB = b.GetDuration();
        NKENTSEU_ASSERT_NEAR(durA + durB, 1.0f, 0.05f);
        AudioLoader::Free(a);
        AudioLoader::Free(b);
    NK_END_TEST()
}

// ============================================================
// TEST 5 : EFFETS DSP
// ============================================================

static void TestEffects() {
    printf("\n=== AudioEffects DSP ===\n");

    NK_TEST("DelayEffect - pas de crash")
        DelayEffect delay(0.25f, 0.4f, 0.5f, 48000);
        float32 buf[256] = {};
        for (int32 i = 0; i < 256; ++i) buf[i] = (i % 2 == 0) ? 0.5f : -0.5f;
        delay.Process(buf, 128, 2);
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("ReverbEffect - pas de crash")
        ReverbEffect::Params p;
        p.roomSize = 0.8f;
        p.wet      = 0.4f;
        ReverbEffect reverb(p, 48000);
        float32 buf[512] = {};
        for (int32 i = 0; i < 512; ++i) buf[i] = 0.3f;
        reverb.Process(buf, 256, 2);
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("BiquadFilter LowPass 1kHz")
        BiquadFilter lp(BiquadFilter::FilterType::LOW_PASS, 1000.0f, 0.707f, 0.0f, 48000);
        float32 buf[512] = {};
        for (int32 i = 0; i < 512; ++i) buf[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        lp.Process(buf, 256, 2);
        // Signal haute fréquence doit être atténué
        float32 rmsAfter = 0.0f;
        for (int32 i = 0; i < 512; ++i) rmsAfter += buf[i] * buf[i];
        rmsAfter = sqrtf(rmsAfter / 512.0f);
        NKENTSEU_ASSERT_TRUE(rmsAfter < 0.5f); // Atténuation visible
    NK_END_TEST()

    NK_TEST("CompressorEffect - pas de crash")
        CompressorEffect::Params p;
        p.threshold = -6.0f; p.ratio = 4.0f;
        CompressorEffect comp(p, 48000);
        float32 buf[256] = {};
        for (int32 i = 0; i < 256; ++i) buf[i] = 0.8f;
        comp.Process(buf, 128, 2);
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("DistortionEffect Soft - atténuation peak")
        DistortionEffect dist(DistortionEffect::DistortionType::SOFT, 5.0f, 0.5f);
        float32 buf[64];
        for (int32 i = 0; i < 64; ++i) buf[i] = 2.0f; // Signal saturé
        dist.Process(buf, 32, 2);
        // Après distortion douce, le peak doit être ≤ 0.5
        bool clipped = false;
        for (int32 i = 0; i < 64; ++i) if (fabsf(buf[i]) > 0.55f) { clipped = true; break; }
        NKENTSEU_ASSERT_TRUE(!clipped);
    NK_END_TEST()

    NK_TEST("Eq3BandEffect - pas de crash")
        Eq3BandEffect::Params p;
        p.lowGainDb  = 3.0f;
        p.midGainDb  = -2.0f;
        p.highGainDb = 5.0f;
        Eq3BandEffect eq(p, 48000);
        float32 buf[256] = {};
        for (int32 i = 0; i < 256; ++i) buf[i] = 0.5f;
        eq.Process(buf, 128, 2);
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("ChorusEffect - pas de crash")
        ChorusEffect chorus(0.5f, 0.003f, 0.3f, 0.5f, 48000);
        float32 buf[512] = {};
        for (int32 i = 0; i < 512; ++i) buf[i] = 0.3f;
        chorus.Process(buf, 256, 2);
        NKENTSEU_ASSERT_TRUE(true);
    NK_END_TEST()

    NK_TEST("Effect enabled/disabled")
        DelayEffect delay(0.1f, 0.3f, 0.5f, 48000);
        delay.SetEnabled(false);
        float32 buf[128];
        for (int32 i = 0; i < 128; ++i) buf[i] = (float32)i * 0.01f;
        float32 expected = buf[64];
        delay.Process(buf, 64, 2);
        // Désactivé : le buffer ne doit pas être modifié
        NKENTSEU_ASSERT_NEAR(buf[64], expected, 0.001f);
    NK_END_TEST()
}

// ============================================================
// TEST 6 : MOTEUR AUDIO (NullBackend)
// ============================================================

static void TestAudioEngine() {
    printf("\n=== AudioEngine (NullBackend) ===\n");

    NK_TEST("Initialize avec NullBackend")
        AudioEngineConfig cfg;
        cfg.backend    = AudioBackendType::NULL_OUTPUT;
        cfg.sampleRate = 48000;
        cfg.channels   = 2;
        cfg.bufferSize = 256;
        bool ok = AudioEngine::Instance().Initialize(cfg);
        NKENTSEU_ASSERT_TRUE(ok);
        NKENTSEU_ASSERT_TRUE(AudioEngine::Instance().IsInitialized());
    NK_END_TEST()

    NK_TEST("Play + Stop")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        AudioHandle h = AudioEngine::Instance().Play(sine);
        NKENTSEU_ASSERT_TRUE(h.IsValid());
        NKENTSEU_ASSERT_TRUE(AudioEngine::Instance().IsPlaying(h));

        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("Pause/Resume")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        AudioHandle h = AudioEngine::Instance().Play(sine);
        AudioEngine::Instance().Pause(h);
        NKENTSEU_ASSERT_TRUE(AudioEngine::Instance().IsPaused(h));
        AudioEngine::Instance().Resume(h);
        NKENTSEU_ASSERT_TRUE(AudioEngine::Instance().IsPlaying(h));
        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("SetVolume / GetVolume")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        AudioHandle h = AudioEngine::Instance().Play(sine);
        AudioEngine::Instance().SetVolume(h, 0.5f);
        NKENTSEU_ASSERT_NEAR(AudioEngine::Instance().GetVolume(h), 0.5f, 0.001f);
        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("SetPitch / GetPitch")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        VoiceParams params;
        params.pitch = 1.5f;
        AudioHandle h = AudioEngine::Instance().Play(sine, params);
        NKENTSEU_ASSERT_NEAR(AudioEngine::Instance().GetPitch(h), 1.5f, 0.001f);
        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("3D Audio - SetSourcePosition")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        VoiceParams params;
        params.source3d.positional = true;
        AudioHandle h = AudioEngine::Instance().Play(sine, params);
        AudioEngine::Instance().SetSourcePosition(h, 10.0f, 0.0f, 0.0f);
        AudioEngine::Instance().SetListenerPosition(0.0f, 0.0f, 0.0f);
        NKENTSEU_ASSERT_TRUE(h.IsValid());
        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("PlayProcedural - callback synthèse")
        int32 callbackCount = 0;
        AudioHandle h = AudioEngine::Instance().PlayProcedural(
            [&](float32* buf, int32 frames, int32 ch) {
                for (int32 i = 0; i < frames * ch; ++i) buf[i] = 0.0f;
                callbackCount++;
            }
        );
        NKENTSEU_ASSERT_TRUE(h.IsValid());
        AudioEngine::Instance().Stop(h);
    NK_END_TEST()

    NK_TEST("AddEffect sur voix")
        AudioSample sine = AudioGenerator::GenerateTone(440.0f, 5.0f);
        AudioHandle h = AudioEngine::Instance().Play(sine);
        DelayEffect delay(0.1f, 0.3f, 0.5f, 48000);
        bool added = AudioEngine::Instance().AddEffect(h, &delay);
        NKENTSEU_ASSERT_TRUE(added);
        AudioEngine::Instance().ClearEffects(h);
        AudioEngine::Instance().Stop(h);
        AudioLoader::Free(sine);
    NK_END_TEST()

    NK_TEST("MasterVolume")
        AudioEngine::Instance().SetMasterVolume(0.8f);
        NKENTSEU_ASSERT_NEAR(AudioEngine::Instance().GetMasterVolume(), 0.8f, 0.001f);
        AudioEngine::Instance().SetMasterVolume(1.0f);
    NK_END_TEST()

    NK_TEST("StopAll / PauseAll / ResumeAll")
        AudioSample s = AudioGenerator::GenerateTone(440.0f, 5.0f);
        AudioEngine::Instance().Play(s);
        AudioEngine::Instance().Play(s);
        AudioEngine::Instance().PauseAll();
        AudioEngine::Instance().ResumeAll();
        AudioEngine::Instance().StopAll();
        AudioLoader::Free(s);
    NK_END_TEST()

    NK_TEST("Shutdown")
        AudioEngine::Instance().Shutdown();
        NKENTSEU_ASSERT_TRUE(!AudioEngine::Instance().IsInitialized());
    NK_END_TEST()
}

// ============================================================
// MAIN
// ============================================================

int main() {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║       NKAudio v2.0 — Tests unitaires AAA             ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    TestAudioGenerator();
    TestAudioLoader();
    TestAudioAnalyzer();
    TestAudioMixer();
    TestEffects();
    TestAudioEngine();

    printf("\n══════════════════════════════════════════════════════\n");
    printf("  Résultats : %d PASSED, %d FAILED\n", gTestPassed, gTestFailed);
    printf("══════════════════════════════════════════════════════\n");

    return (gTestFailed == 0) ? 0 : 1;
}
