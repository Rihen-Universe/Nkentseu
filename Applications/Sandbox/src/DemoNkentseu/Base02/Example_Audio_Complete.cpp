// example_audio_complete.cpp
#include "nk.hpp"
#include "Audio.hpp"
#include "GraphicsContext.hpp"
#include "Renderer.hpp"
#include <iostream>
#include <cmath>

int nk::nk_main(int argc, char** argv) {
    std::cout << "=== Audio Engine Complete Example ===" << std::endl;
    
    // ====================================================================
    // INITIALISATION
    // ====================================================================
    
    // Liste des backends disponibles
    auto backends = nk::AudioBackendFactory::getAvailableBackends();
    std::cout << "Available audio backends:" << std::endl;
    for (const auto& backend : backends) {
        std::cout << "  - " << backend << std::endl;
    }
    
    // Initialiser le moteur audio
    auto& audioEngine = nk::AudioEngine::instance();
    
    if (!audioEngine.initialize(nk::AudioBackend::Auto, 44100, 2, 512)) {
        std::cout << "Failed to initialize audio engine" << std::endl;
        return 1;
    }
    
    std::cout << "Audio engine initialized:" << std::endl;
    std::cout << "  Sample rate: " << audioEngine.getSampleRate() << " Hz" << std::endl;
    std::cout << "  Channels: " << audioEngine.getChannels() << std::endl;
    std::cout << "  Buffer size: " << audioEngine.getBufferSize() << " frames" << std::endl;
    
    // ====================================================================
    // GÉNÉRATION DE SONS
    // ====================================================================
    
    std::cout << "\n=== Generating sounds ===" << std::endl;
    
    // Générer différentes formes d'onde
    auto sine440 = nk::AudioGenerator::generateTone(440.0f, 2.0f, nk::WaveformType::Sine);
    std::cout << "Generated sine wave at 440 Hz" << std::endl;
    
    auto square220 = nk::AudioGenerator::generateTone(220.0f, 1.5f, nk::WaveformType::Square);
    std::cout << "Generated square wave at 220 Hz" << std::endl;
    
    // Générer un accord (Do majeur: C-E-G)
    std::vector<float> chordFreqs = {261.63f, 329.63f, 392.00f}; // C4, E4, G4
    auto chord = nk::AudioGenerator::generateChord(chordFreqs, 3.0f);
    std::cout << "Generated C major chord" << std::endl;
    
    // Appliquer une enveloppe ADSR
    nk::AudioGenerator::ADSREnvelope envelope;
    envelope.attackTime = 0.1f;
    envelope.decayTime = 0.2f;
    envelope.sustainLevel = 0.6f;
    envelope.releaseTime = 0.5f;
    
    nk::AudioGenerator::applyEnvelope(sine440, envelope);
    std::cout << "Applied ADSR envelope" << std::endl;
    
    // ====================================================================
    // ANALYSE AUDIO
    // ====================================================================
    
    std::cout << "\n=== Audio Analysis ===" << std::endl;
    
    float rms = nk::AudioAnalyzer::calculateRMS(sine440);
    float peak = nk::AudioAnalyzer::calculatePeak(sine440);
    
    std::cout << "Sine 440 Hz:" << std::endl;
    std::cout << "  RMS: " << rms << std::endl;
    std::cout << "  Peak: " << peak << std::endl;
    
    // FFT
    auto fft = nk::AudioAnalyzer::computeFFT(sine440, 2048);
    std::cout << "  FFT computed (" << fft.size() << " bins)" << std::endl;
    
    // Trouver le bin avec la magnitude maximale
    auto maxIt = std::max_element(fft.begin(), fft.end());
    int maxBin = std::distance(fft.begin(), maxIt);
    float detectedFreq = (maxBin * audioEngine.getSampleRate()) / 2048.0f;
    std::cout << "  Detected frequency: " << detectedFreq << " Hz" << std::endl;
    
    // Pitch detection
    float pitch = nk::AudioAnalyzer::detectPitch(sine440);
    std::cout << "  Detected pitch: " << pitch << " Hz" << std::endl;
    
    // ====================================================================
    // MIXAGE
    // ====================================================================
    
    std::cout << "\n=== Audio Mixing ===" << std::endl;
    
    nk::AudioMixer mixer(44100, 2);
    
    // Mixer plusieurs sons
    auto mixed = mixer.mix({sine440, square220, chord}, {0.3f, 0.3f, 0.4f});
    std::cout << "Mixed 3 sounds together" << std::endl;
    
    // Normaliser
    mixer.normalize(mixed, 0.8f);
    std::cout << "Normalized to 0.8 peak" << std::endl;
    
    // Fade in/out
    nk::AudioSample fadeTest = sine440;
    mixer.fadeIn(fadeTest, 0.5f);
    mixer.fadeOut(fadeTest, 0.5f);
    std::cout << "Applied fade in/out" << std::endl;
    
    // ====================================================================
    // PLAYBACK 2D
    // ====================================================================
    
    std::cout << "\n=== 2D Audio Playback ===" << std::endl;
    
    // Créer une source audio 2D
    nk::AudioSource source2D;
    source2D.volume = 0.7f;
    source2D.pan = -0.5f; // Pencher à gauche
    source2D.looping = false;
    source2D.positional = false; // 2D audio
    
    auto handle2D = audioEngine.play(sine440, source2D);
    std::cout << "Playing 2D audio (panned left)" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Changer le pan pendant la lecture
    audioEngine.setPan(handle2D, 0.5f); // Pencher à droite
    std::cout << "Changed pan to right" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    audioEngine.stop(handle2D);
    
    // ====================================================================
    // PLAYBACK 3D SPATIAL
    // ====================================================================
    
    std::cout << "\n=== 3D Spatial Audio ===" << std::endl;
    
    // Configurer le listener (caméra audio)
    audioEngine.setListenerPosition(0.0f, 0.0f, 0.0f);
    audioEngine.setListenerOrientation(
        0.0f, 0.0f, -1.0f,  // Forward (regardant vers -Z)
        0.0f, 1.0f, 0.0f    // Up
    );
    
    // Créer une source 3D
    nk::AudioSource source3D;
    source3D.volume = 1.0f;
    source3D.positional = true;
    source3D.position[0] = 5.0f;  // 5 mètres à droite
    source3D.position[1] = 0.0f;
    source3D.position[2] = -2.0f; // 2 mètres devant
    source3D.velocity[0] = -1.0f; // Se déplace vers la gauche
    source3D.minDistance = 1.0f;
    source3D.maxDistance = 50.0f;
    source3D.rolloffFactor = 1.0f;
    source3D.looping = true;
    
    auto handle3D = audioEngine.play(square220, source3D);
    std::cout << "Playing 3D audio at (5, 0, -2)" << std::endl;
    
    // Animer la source (la déplacer)
    for (int i = 0; i < 50; i++) {
        float t = i / 50.0f;
        float x = 5.0f * std::cos(t * 2.0f * M_PI);
        float z = -2.0f + 5.0f * std::sin(t * 2.0f * M_PI);
        
        audioEngine.setSourcePosition(handle3D, x, 0.0f, z);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    audioEngine.stop(handle3D);
    std::cout << "Stopped 3D audio" << std::endl;
    
    // ====================================================================
    // EFFETS AUDIO
    // ====================================================================
    
    std::cout << "\n=== Audio Effects ===" << std::endl;
    
    // Créer des effets
    nk::DelayEffect delay(0.3f, 0.5f, 0.4f, 44100);
    nk::ReverbEffect reverb(0.7f, 0.5f, 0.3f, 44100);
    nk::LowPassFilter lowpass(2000.0f, 0.7f, 44100);
    nk::DistortionEffect distortion(3.0f, 0.5f);
    
    auto handleFX = audioEngine.play(chord, nk::AudioSource());
    
    // Ajouter des effets
    audioEngine.addEffect(handleFX, &delay);
    std::cout << "Added delay effect" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    audioEngine.addEffect(handleFX, &reverb);
    std::cout << "Added reverb effect" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    audioEngine.clearEffects(handleFX);
    audioEngine.addEffect(handleFX, &lowpass);
    std::cout << "Replaced with lowpass filter" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    audioEngine.stop(handleFX);
    
    // ====================================================================
    // AUDIO PROCEDURAL (Callback)
    // ====================================================================
    
    std::cout << "\n=== Procedural Audio ===" << std::endl;
    
    float phase = 0.0f;
    auto synthCallback = [&](float* buffer, int frameCount, int channels) {
        for (int i = 0; i < frameCount; i++) {
            // Synthèse FM simple
            float modulator = std::sin(phase * 5.0f) * 0.5f;
            float carrier = std::sin(phase + modulator);
            
            for (int ch = 0; ch < channels; ch++) {
                buffer[i * channels + ch] = carrier * 0.3f;
            }
            
            phase += 2.0f * M_PI * 220.0f / 44100.0f;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
    };
    
    auto handleSynth = audioEngine.playCallback(synthCallback);
    std::cout << "Playing procedural FM synthesis" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    audioEngine.stop(handleSynth);
    
    // ====================================================================
    // VISUALISATION AUDIO (avec renderer)
    // ====================================================================
    
    std::cout << "\n=== Audio Visualization ===" << std::endl;
    
    auto& gfxContext = nk::GraphicsContext::instance();
    gfxContext.initialize(nk::RendererAPI::Auto);
    
    nk::WindowConfig windowConfig;
    windowConfig.title = "Audio Visualization";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    
    nk::Window window(windowConfig);
    
    nk::RendererConfig rendererConfig;
    rendererConfig.api = nk::RendererAPI::SoftwareCPU;
    
    nk::Renderer renderer(window, rendererConfig);
    
    // Jouer un son en boucle pour visualiser
    nk::AudioSource vizSource;
    vizSource.looping = true;
    auto vizHandle = audioEngine.play(mixed, vizSource);
    
    auto& eventSystem = nk::EventSystem::instance();
    bool running = true;
    
    eventSystem.setEventCallback<nk::WindowCloseEvent>([&](auto* e) {
        running = false;
    });
    
    eventSystem.setEventCallback<nk::KeyPressedEvent>([&](auto* e) {
        if (e->getKey() == nk::Key::Escape) running = false;
    });
    
    // Calculer le spectrogram une fois
    auto spectrogram = nk::AudioAnalyzer::computeSpectrogram(mixed, 2048, 512);
    
    std::cout << "Spectrogram: " << spectrogram.size() << " frames x " 
              << (spectrogram.empty() ? 0 : spectrogram[0].size()) << " bins" << std::endl;
    
    while (running) {
        while (auto event = eventSystem.pollEvent()) {}
        
        renderer.beginFrame(renderer.packColor(20, 20, 30, 255));
        
        // Dessiner le spectrogram
        if (!spectrogram.empty()) {
            int width = 1280;
            int height = 720;
            
            float xStep = static_cast<float>(width) / spectrogram.size();
            float yStep = static_cast<float>(height) / spectrogram[0].size();
            
            for (size_t t = 0; t < spectrogram.size(); t++) {
                for (size_t f = 0; f < spectrogram[t].size(); f++) {
                    // Convertir dB en couleur
                    float db = spectrogram[t][f];
                    float normalized = (db + 80.0f) / 80.0f; // -80 à 0 dB
                    normalized = std::clamp(normalized, 0.0f, 1.0f);
                    
                    uint8_t intensity = static_cast<uint8_t>(normalized * 255);
                    uint32_t color = renderer.packColor(intensity, intensity / 2, 255 - intensity, 255);
                    
                    int x = static_cast<int>(t * xStep);
                    int y = height - static_cast<int>((f + 1) * yStep);
                    
                    renderer.fillRect(x, y, std::max(1, static_cast<int>(xStep)), 
                                    std::max(1, static_cast<int>(yStep)), color);
                }
            }
        }
        
        renderer.endFrame();
        renderer.present();
    }
    
    audioEngine.stop(vizHandle);
    
    // ====================================================================
    // CLEANUP
    // ====================================================================
    
    std::cout << "\n=== Cleanup ===" << std::endl;
    
    audioEngine.shutdown();
    std::cout << "Audio engine shutdown complete" << std::endl;
    
    return 0;
}