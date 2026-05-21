// =============================================================================
// NkAudioDemo — main.cpp
// -----------------------------------------------------------------------------
// Console app : valide que NKAudio peut charger un WAV et le jouer via le
// backend natif (WASAPI Windows / CoreAudio mac / ALSA Linux / Oboe Android).
// Si on entend le son, le pipeline complet fonctionne.
// =============================================================================

#include "NKAudio/NkAudio.h"
#include "NKAudio/NkAudioBackends.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::audio;

int main(int argc, char** argv)
{
    // Choix du fichier WAV (defaut : Resources/Audio/powerup.wav).
    const char* defaultPath = "Resources/Audio/powerup.wav";
    const char* path = (argc >= 2) ? argv[1] : defaultPath;

    logger.Info("[NkAudioDemo] Demarrage. Fichier : {0}", path);

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

    // 2. Init AudioEngine avec config par defaut (backend natif AUTO).
    AudioEngineConfig cfg;
    cfg.sampleRate   = sample.sampleRate;   // matche le sample pour eviter le resampling
    cfg.channels     = 2;
    cfg.bufferSize   = 512;
    cfg.backend      = AudioBackendType::WASAPI;  // Force WASAPI sur Windows
    cfg.masterVolume = 0.8f;

    if (!AudioEngine::Instance().Initialize(cfg))
    {
        logger.Error("[NkAudioDemo] AudioEngine::Initialize a echoue");
        AudioLoader::Free(sample);
        return 2;
    }
    logger.Info("[NkAudioDemo] AudioEngine OK : backend={0}, sr={1}, ch={2}, latency={3:.1}ms",
                AudioEngine::Instance().GetBackendName(),
                AudioEngine::Instance().GetSampleRate(),
                AudioEngine::Instance().GetChannels(),
                AudioEngine::Instance().GetLatencyMs());

    // 3. Joue le sample. Handle = identifiant de la voix.
    VoiceParams vp;
    vp.volume = 1.0f;
    vp.pitch  = 1.0f;
    AudioHandle h = AudioEngine::Instance().Play(sample, vp);
    if (!h.IsValid())
    {
        logger.Error("[NkAudioDemo] Play a echoue (pool plein ?)");
        AudioEngine::Instance().Shutdown();
        AudioLoader::Free(sample);
        return 3;
    }
    logger.Info("[NkAudioDemo] Lecture demarree (handle={0})", h.id);

    // 4. Attendre la fin (duree + 0.5s de safety).
    const float waitSec = sample.GetDuration() + 0.5f;
    logger.Info("[NkAudioDemo] Attente {0:.2}s...", waitSec);
    std::this_thread::sleep_for(
        std::chrono::milliseconds((int)(waitSec * 1000.0f)));

    // 5. Cleanup.
    AudioEngine::Instance().Stop(h);
    AudioEngine::Instance().Shutdown();
    AudioLoader::Free(sample);
    logger.Info("[NkAudioDemo] Termine sans crash. Si tu as entendu le son => NKAudio OK !");
    return 0;
}
