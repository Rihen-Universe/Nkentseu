// =============================================================================
// NkAudioDemo — main.cpp
// -----------------------------------------------------------------------------
// Console app : valide que NKAudio peut charger un WAV et le jouer via le
// backend natif (WASAPI Windows / CoreAudio mac / ALSA Linux / Oboe Android).
// Si on entend le son, le pipeline complet fonctionne.
// =============================================================================

#include "NKAudio/NKAudio.h"
#include "NKAudio/NkAudioBackends.h"
#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::audio;

// Mode streaming : OpenAudioStream + pull jusqu'a EOF, sauve en WAV.
// Active par "--stream" dans argv. Valide IAudioStream + AudioStreamPlayer.
static int RunStreamingTest(const char* path)
{
    logger.Info("[NkAudioDemo] Mode streaming : {0}", path);
    IAudioStream* stream = OpenAudioStream(path);
    if (!stream) {
        logger.Error("[NkAudioDemo] OpenAudioStream echec");
        return 1;
    }
    int32 sampleRate = stream->GetSampleRate();
    int32 channels   = stream->GetChannels();
    nk_int64 totalFrames = stream->GetFrameCount();
    logger.Info("[NkAudioDemo] Stream ouvert : {0} frames, {1} ch, {2} Hz",
                (long long)totalFrames, channels, sampleRate);

    AudioStreamPlayer player;
    if (!player.Init(sampleRate, channels, 88200)) {
        logger.Error("[NkAudioDemo] player.Init echec");
        delete stream;
        return 2;
    }
    if (!player.Play(stream, /*loop=*/false)) {
        logger.Error("[NkAudioDemo] player.Play echec");
        player.Shutdown();
        return 3;
    }

    // Pull jusqu'a EOF (max 30 sec audio)
    int32 maxFrames = sampleRate * 30;
    if (maxFrames > int32(totalFrames)) maxFrames = int32(totalFrames);
    const int32 chunkFrames = 1024;
    usize bufSize = usize(maxFrames) * usize(channels);
    float32* outBuf = static_cast<float32*>(
        memory::NkAlloc(bufSize * sizeof(float32), nullptr, sizeof(float32)));
    if (!outBuf) {
        player.Shutdown();
        return 4;
    }
    int32 readSoFar = 0;
    int32 nZeroFrames = 0;
    while (readSoFar < maxFrames) {
        int32 want = maxFrames - readSoFar;
        if (want > chunkFrames) want = chunkFrames;
        int32 got = player.ReadFrames(outBuf + usize(readSoFar) * usize(channels), want);
        if (got == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (++nZeroFrames > 1000) {
                logger.Warn("[NkAudioDemo] Timeout streaming (buffer vide trop longtemps)");
                break;
            }
        } else {
            readSoFar += got;
            std::this_thread::sleep_for(std::chrono::microseconds(int(got * 1000000LL / sampleRate)));
        }
    }
    logger.Info("[NkAudioDemo] Streaming OK : {0} / {1} frames lus ({2:.2}s).",
                readSoFar, maxFrames, double(readSoFar) / double(sampleRate));

    // Stats
    float mn = 1e30f, mx = -1e30f;
    int nNonZero = 0;
    usize nStat = usize(readSoFar) * usize(channels);
    if (nStat > 200000) nStat = 200000;
    for (usize i = 0; i < nStat; ++i) {
        float v = outBuf[i];
        if (v != 0.0f) ++nNonZero;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    logger.Info("[NkAudioDemo] Stats : min={0:.4} max={1:.4} nonZero={2}/{3}",
                mn, mx, nNonZero, (int)nStat);

    // Sauve en WAV
    AudioSample s{};
    s.data       = outBuf;
    s.frameCount = usize(readSoFar);
    s.sampleRate = sampleRate;
    s.channels   = channels;
    s.format     = AudioFormat::WAV;
    s.mAllocator = nullptr;
    if (AudioLoader::SaveWAV("Build/test_streaming.wav", s)) {
        logger.Info("[NkAudioDemo] Sauve Build/test_streaming.wav");
    }
    memory::NkFree(outBuf, nullptr);
    player.Shutdown();
    return 0;
}

// Mode HRTF test : orbite une source autour du listener (360° en 6 sec)
// avec HRTF synthetique active. Sauve le resultat en stereo pour ecoute casque.
static int RunHrtfTest(const char* path)
{
    logger.Info("[NkAudioDemo] Mode HRTF test : {0}", path);
    AudioSample srcSample = AudioLoader::Load(path);
    if (!srcSample.IsValid()) {
        logger.Error("[NkAudioDemo] HRTF test : load echec : {0}", path);
        return 1;
    }
    logger.Info("[NkAudioDemo] Source chargee : {0} frames, {1} Hz, {2} ch",
                (int)srcSample.frameCount, srcSample.sampleRate, srcSample.channels);

    // Init engine (sample rate matche la source pour eviter resampling)
    AudioEngineConfig cfg;
    cfg.sampleRate           = srcSample.sampleRate;
    cfg.channels             = 2;
    cfg.bufferSize           = 1024;
    cfg.backend              = AudioBackendType::NULL_OUTPUT;  // pas de lecture, juste capture
    cfg.enableMasterLimiter  = true;
    if (!AudioEngine::Instance().Initialize(cfg)) {
        logger.Error("[NkAudioDemo] HRTF test : Initialize echec");
        AudioLoader::Free(srcSample);
        return 2;
    }
    auto& engine = AudioEngine::Instance();

    // Generer dataset HRTF synthetique (modele spherique)
    if (!engine.GenerateSyntheticHrtf(/*irLength=*/128,
                                       /*nAzimuths=*/36,
                                       /*nElevations=*/9)) {
        logger.Error("[NkAudioDemo] HRTF test : generation dataset echec");
        engine.Shutdown();
        AudioLoader::Free(srcSample);
        return 3;
    }
    logger.Info("[NkAudioDemo] HRTF synthetique genere (36 az x 9 el x 128 samples).");

    // Joue la source en boucle, positionnelle, HRTF active
    VoiceParams vp;
    vp.volume                  = 0.8f;
    vp.looping                 = true;
    vp.source3d.positional     = true;
    vp.source3d.useHrtf        = true;
    vp.source3d.minDistance    = 1.0f;
    vp.source3d.maxDistance    = 100.0f;
    vp.source3d.rolloffFactor  = 1.0f;
    vp.source3d.position[0] = 5.0f; vp.source3d.position[1] = 0.0f; vp.source3d.position[2] = 0.0f;
    AudioHandle h = engine.Play(srcSample, vp);
    if (!h.IsValid()) {
        logger.Error("[NkAudioDemo] HRTF test : Play echec");
        engine.Shutdown();
        AudioLoader::Free(srcSample);
        return 4;
    }

    // Listener au centre, regardant -Z (avant)
    engine.SetListenerPosition(0.0f, 0.0f, 0.0f);
    engine.SetListenerOrientation(0.0f, 0.0f, -1.0f,    /* forward */
                                   0.0f, 1.0f, 0.0f);   /* up */

    // Capture 6 secondes : la source orbite a 5m autour du listener (360°)
    const int32 sampleRate    = srcSample.sampleRate;
    const int32 totalSeconds  = 6;
    const int32 totalFrames   = sampleRate * totalSeconds;
    const int32 chunkFrames   = 256;
    usize bufSize = usize(totalFrames) * 2;  // stereo
    float32* outBuf = static_cast<float32*>(
        memory::NkAlloc(bufSize * sizeof(float32), nullptr, sizeof(float32)));
    if (!outBuf) {
        engine.Shutdown();
        AudioLoader::Free(srcSample);
        return 5;
    }
    ::memset(outBuf, 0, bufSize * sizeof(float32));

    // Boucle de capture : on demande au backend NULL d'avancer le mix
    // (interne via AudioEngine::Tick() ou similaire). Pour simplicite, on appelle
    // directement le mix interne via Play + le backend NULL qui appelle Render.
    // Comme NULL_OUTPUT n'a pas de pull naturel, on simule en demandant au backend
    // de produire des frames.
    int32 written = 0;
    int32 stepIdx = 0;
    int32 nSteps  = totalFrames / chunkFrames;
    for (stepIdx = 0; stepIdx < nSteps; ++stepIdx) {
        // Update position : orbite circulaire dans le plan XZ, rayon 5m
        float32 t   = float32(stepIdx) / float32(nSteps);
        float32 ang = t * 2.0f * 3.14159265f;
        float32 sx  = 5.0f * ::sinf(ang);
        float32 sz  = -5.0f * ::cosf(ang);
        engine.SetSourcePosition(h, sx, 0.0f, sz);
        // Demander au backend NULL de produire chunkFrames
        // (RenderAudio dans NkAudioBackends - on appelle via une API publique)
        engine.RenderToBuffer(outBuf + usize(written) * 2, chunkFrames);
        written += chunkFrames;
    }
    logger.Info("[NkAudioDemo] Capture HRTF terminee : {0} frames stereo ({1}s).",
                written, written / sampleRate);

    // Stats
    float mn = 1e30f, mx = -1e30f;
    int nNonZero = 0;
    usize nStat = usize(written) * 2;
    if (nStat > 200000) nStat = 200000;
    for (usize i = 0; i < nStat; ++i) {
        float v = outBuf[i];
        if (v != 0.0f) ++nNonZero;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    logger.Info("[NkAudioDemo] Stats HRTF : min={0:.4} max={1:.4} nonZero={2}/{3}",
                mn, mx, nNonZero, (int)nStat);

    // Sauve en WAV stereo
    AudioSample out{};
    out.data       = outBuf;
    out.frameCount = usize(written);
    out.sampleRate = sampleRate;
    out.channels   = 2;
    out.format     = AudioFormat::WAV;
    out.mAllocator = nullptr;
    if (AudioLoader::SaveWAV("Build/test_hrtf_orbit.wav", out)) {
        logger.Info("[NkAudioDemo] WAV HRTF sauve : Build/test_hrtf_orbit.wav (ecoute au casque !)");
    }

    memory::NkFree(outBuf, nullptr);
    engine.Stop(h);
    engine.Shutdown();
    AudioLoader::Free(srcSample);
    return 0;
}

int main(int argc, char** argv)
{
    // Choix du fichier WAV (defaut : Resources/Audio/powerup.wav).
    const char* defaultPath = "Resources/Audio/powerup.wav";
    const char* path = defaultPath;
    bool streamMode = false;
    bool hrtfMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--stream") == 0) streamMode = true;
        else if (std::strcmp(argv[i], "--hrtf-test") == 0) hrtfMode = true;
        else if (argv[i][0] != '-') path = argv[i];
    }

    logger.Info("[NkAudioDemo] Demarrage. Fichier : {0} (stream={1} hrtf={2})",
                path, streamMode ? 1 : 0, hrtfMode ? 1 : 0);

    if (hrtfMode)   return RunHrtfTest(path);
    if (streamMode) return RunStreamingTest(path);

    // 1. Charge le WAV en AudioSample (Float32 interleaved).
    AudioSample sample = AudioLoader::Load(path);
    if (!sample.IsValid())
    {
        logger.Error("[NkAudioDemo] Echec chargement WAV : {0}", path);
        logger.Error("[NkAudioDemo] Astuce : lance depuis la racine du projet Nkentseu");
        return 1;
    }
    logger.Info("[NkAudioDemo] WAV charge : {0} frames, {1} Hz, {2} canaux, duree {3:.2}s",
                (int)sample.frameCount, sample.sampleRate, sample.channels,
                sample.GetDuration());

    // 1a. Stats audio rapides (debug : detecter silence / bruit pur).
    {
        usize maxStat = sample.frameCount * usize(sample.channels);
        if (maxStat > 200000) maxStat = 200000;
        float mn = 1e30f, mx = -1e30f;
        double sumAbs = 0.0;
        int nNonZero = 0;
        for (usize i = 0; i < maxStat; ++i) {
            float v = sample.data[i];
            if (v != 0.0f) ++nNonZero;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
            sumAbs += v < 0.0f ? -double(v) : double(v);
        }
        logger.Info("[NkAudioDemo] Stats : min={0:.4} max={1:.4} avgAbs={2:.4} nonZero={3}/{4}",
                    mn, mx, float(sumAbs / double(maxStat)), nNonZero, (int)maxStat);
    }

    // 1b. Si le fichier source n'est PAS un WAV, sauve en WAV pour reecoute.
    const char* dot = nullptr;
    for (const char* p = path; *p; ++p) if (*p == '.') dot = p;
    if (dot && (std::strcmp(dot, ".ogg") == 0 || std::strcmp(dot, ".mp3") == 0
                || std::strcmp(dot, ".mpga") == 0 || std::strcmp(dot, ".flac") == 0))
    {
        // Extraire basename sans extension
        const char* slash = path;
        for (const char* p = path; *p; ++p) if (*p == '/' || *p == '\\') slash = p + 1;
        char outWav[512];
        usize baseLen = usize(dot - slash);
        if (baseLen > 200) baseLen = 200;
        std::snprintf(outWav, sizeof(outWav), "Build/decoded_%.*s.wav",
                      (int)baseLen, slash);
        if (AudioLoader::SaveWAV(outWav, sample)) {
            logger.Info("[NkAudioDemo] Audio decode sauve dans : {0}", outWav);
        }
    }

    // 2. Lecture AUDIO desactivee : pour eviter les processus en parallele
    //    qui se mixent et generent du bruit, on ne joue PAS automatiquement.
    //    L'utilisateur peut ecouter le WAV sauve a son rythme.
    //    Pour reactiver la lecture, definir NKAUDIODEMO_AUTOPLAY a 1.
#if defined(NKAUDIODEMO_AUTOPLAY) && NKAUDIODEMO_AUTOPLAY
    AudioEngineConfig cfg;
    cfg.sampleRate   = sample.sampleRate;
    cfg.channels     = 2;
    cfg.bufferSize   = 512;
    cfg.backend      = AudioBackendType::WASAPI;
    cfg.masterVolume = 0.8f;
    if (!AudioEngine::Instance().Initialize(cfg)) {
        logger.Error("[NkAudioDemo] AudioEngine::Initialize a echoue");
        AudioLoader::Free(sample);
        return 2;
    }
    VoiceParams vp;
    vp.volume = 1.0f; vp.pitch = 1.0f;
    AudioHandle h = AudioEngine::Instance().Play(sample, vp);
    if (!h.IsValid()) {
        AudioEngine::Instance().Shutdown();
        AudioLoader::Free(sample);
        return 3;
    }
    const float waitSec = sample.GetDuration() + 0.5f;
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(waitSec * 1000.0f)));
    AudioEngine::Instance().Stop(h);
    AudioEngine::Instance().Shutdown();
#else
    logger.Info("[NkAudioDemo] Lecture auto desactivee. Ecoute le WAV pour valider.");
#endif

    AudioLoader::Free(sample);
    logger.Info("[NkAudioDemo] Termine sans crash.");
    return 0;
}
