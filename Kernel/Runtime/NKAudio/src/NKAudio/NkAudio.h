// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudio.h
// DESCRIPTION: Header principal du module audio Nkentseu - STL-free, AAA-grade
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Zero STL. Utilise NkVector, NkFunction, NkAllocator de la fondation.
//        Conçu pour des applications AAA (DAW, moteurs de jeu, SFX temps réel).
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIO_H
#define NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIO_H

// ============================================================
// INCLUDES FONDATION (STL MAISON)
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkMacros.h"
#include "NKCore/NkInline.h"
#include "NkAudioExport.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/NkAtomic.h"

// ============================================================
// FORWARD DECLARATIONS
// ============================================================

namespace nkentseu {
    namespace audio {

        class AudioEngine;
        class IAudioBackend;
        class IAudioEffect;
        struct AudioSample;
        struct AudioHandle;
        struct AudioSource3D;
        struct AudioListener3D;

    } // namespace audio
} // namespace nkentseu

// ============================================================
// CONSTANTES GLOBALES AUDIO
// ============================================================

namespace nkentseu {
    namespace audio {

        /// Fréquence d'échantillonnage par défaut (qualité CD)
        constexpr int32 AUDIO_DEFAULT_SAMPLE_RATE  = 48000;

        /// Nombre de canaux par défaut (stéréo)
        constexpr int32 AUDIO_DEFAULT_CHANNELS      = 2;

        /// Taille du buffer audio (frames) - compromis latence/stabilité AAA
        constexpr int32 AUDIO_DEFAULT_BUFFER_SIZE   = 256;

        /// Nombre maximal de sources simultanées (AAA : 256 voix)
        constexpr int32 AUDIO_MAX_VOICES            = 256;

        /// Nombre maximal d'effets par voix
        constexpr int32 AUDIO_MAX_EFFECTS_PER_VOICE = 8;

        /// Nombre maximal d'effets sur le bus master
        constexpr int32 AUDIO_MAX_MASTER_EFFECTS    = 16;

        /// Taille de la FFT par défaut
        constexpr int32 AUDIO_DEFAULT_FFT_SIZE      = 2048;

        /// Vitesse du son dans l'air (m/s) pour effet Doppler
        constexpr float32 AUDIO_SPEED_OF_SOUND      = 343.0f;

        /// Valeur d'identifiant invalide pour AudioHandle
        constexpr uint32 AUDIO_INVALID_ID           = 0u;

    } // namespace audio
} // namespace nkentseu

// ============================================================
// ENUMERATIONS
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Format de conteneur audio supporté
         */
        enum class AudioFormat {
            UNKNOWN = 0,
            WAV,        ///< PCM non compressé, chargement natif complet
            MP3,        ///< MPEG Layer-3 (décodeur interne minimp3)
            OGG,        ///< Vorbis (décodeur interne stb_vorbis)
            FLAC,       ///< Free Lossless Audio Codec
            OPUS,       ///< Codec moderne (streaming, VoIP)
            AIFF,       ///< Audio Interchange File Format (Apple)
            RAW         ///< PCM brut, format spécifié manuellement
        };

        /**
         * @brief Format des échantillons (représentation binaire)
         */
        enum class SampleFormat {
            UNKNOWN = 0,
            INT_8,      ///< Entier signé 8 bits
            INT_16,     ///< Entier signé 16 bits (standard CD)
            INT_24,     ///< Entier signé 24 bits (studio)
            INT_32,     ///< Entier signé 32 bits
            FLOAT_32,   ///< Virgule flottante 32 bits (traitement DSP)
            FLOAT_64    ///< Virgule flottante 64 bits (précision maximale)
        };

        /**
         * @brief Backend audio bas niveau
         */
        enum class AudioBackendType {
            AUTO = 0,       ///< Sélection automatique selon la plateforme
            DIRECTSOUND,    ///< Windows XP/7 (legacy)
            WASAPI,         ///< Windows Vista+ (recommandé Windows)
            CORE_AUDIO,     ///< macOS / iOS
            ALSA,           ///< Linux (bas niveau)
            PULSE_AUDIO,    ///< Linux (haut niveau)
            OPEN_SL_ES,     ///< Android legacy
            AAUDIO,         ///< Android 8.0+ (recommandé Android)
            WEB_AUDIO,      ///< Emscripten / WebAssembly
            CUSTOM,         ///< Backend utilisateur (plugin)
            NULL_OUTPUT     ///< Aucune sortie (tests, serveurs)
        };

        /**
         * @brief Type de forme d'onde pour synthèse
         */
        enum class WaveformType {
            SINE = 0,   ///< Sinus - son pur
            SQUARE,     ///< Carré - riche en harmoniques impaires
            TRIANGLE,   ///< Triangle - harmoniques impaires plus douces
            SAWTOOTH,   ///< Dent de scie - toutes harmoniques (synthés leads)
            NOISE_WHITE,///< Bruit blanc (toutes fréquences égales)
            NOISE_PINK, ///< Bruit rose (1/f, plus naturel)
            PULSE       ///< Impulsion avec largeur variable (pulse-width)
        };

        /**
         * @brief Type d'effet audio
         */
        enum class AudioEffectType {
            NONE = 0,
            REVERB,         ///< Réverbération (Schroeder/FDN)
            ECHO,           ///< Écho simple
            DELAY,          ///< Délai multi-tap
            CHORUS,         ///< Chorus (modulation de délai)
            FLANGER,        ///< Flanger (chorus court avec feedback)
            PHASER,         ///< Phaser (all-pass filters en cascade)
            DISTORTION,     ///< Distortion / Overdrive / Fuzz
            COMPRESSOR,     ///< Compression dynamique
            LIMITER,        ///< Limitation hard/soft
            GATE,           ///< Noise gate
            EQ_3BAND,       ///< Égaliseur 3 bandes (grave/médium/aigu)
            EQ_PARAMETRIC,  ///< Égaliseur paramétrique N bandes
            LOW_PASS,       ///< Filtre passe-bas (Butterworth / Biquad)
            HIGH_PASS,      ///< Filtre passe-haut
            BAND_PASS,      ///< Filtre passe-bande
            NOTCH,          ///< Filtre coupe-bande
            PITCH_SHIFT,    ///< Décalage de pitch (algorithme overlap-add)
            TIME_STRETCH,   ///< Étirement temporel sans altération pitch
            AUTOPAN,        ///< Panoramique automatique
            TREMOLO,        ///< Trémolo (modulation d'amplitude)
            VIBRATO         ///< Vibrato (modulation de pitch)
        };

        /**
         * @brief État d'une voix audio
         */
        enum class VoiceState {
            FREE = 0,   ///< Voix disponible (pool)
            PLAYING,    ///< Lecture en cours
            PAUSED,     ///< En pause
            STOPPING,   ///< En cours d'arrêt (fade-out)
            FINISHED    ///< Terminée, sera recyclée
        };

        /**
         * @brief Algorithme de distance 3D
         */
        enum class AttenuationModel {
            NONE = 0,       ///< Pas d'atténuation
            INVERSE,        ///< Loi inverse (1/distance)
            LINEAR,         ///< Linéaire
            EXPONENTIAL     ///< Exponentielle
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// STRUCTURES DE DONNÉES
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Handle opaque vers une voix audio active
         *
         * Identifiant léger (4 bytes) permettant de contrôler
         * une source audio en cours de lecture. Inspiré du
         * pattern Handle/Id des moteurs AAA (Frostbite, UE5).
         *
         * @note Invalide si id == AUDIO_INVALID_ID
         */
        struct NKENTSEU_AUDIO_API AudioHandle {
            uint32 id = AUDIO_INVALID_ID;

            NKENTSEU_INLINE bool IsValid()    const { return id != AUDIO_INVALID_ID; }
            NKENTSEU_INLINE operator bool()   const { return IsValid(); }
            NKENTSEU_INLINE bool operator==(const AudioHandle& other) const { return id == other.id; }
            NKENTSEU_INLINE bool operator!=(const AudioHandle& other) const { return id != other.id; }
        };

        /// Handle invalide pré-construit
        constexpr AudioHandle AUDIO_HANDLE_INVALID = { AUDIO_INVALID_ID };

        // ----------------------------------------------------------------

        /**
         * @brief Données PCM d'un échantillon audio chargé
         *
         * Stocke les données brutes en mémoire avec les métadonnées
         * nécessaires au mixeur. Ownership explicite : les données
         * sont gérées par l'allocateur du framework.
         *
         * @note Format interne normalisé : Float32 interleaved stéréo.
         *       La conversion est faite au chargement.
         */
        struct NKENTSEU_AUDIO_API AudioSample {
            // ── Données PCM ──────────────────────────────────────────────
            float32*    data        = nullptr;  ///< Samples Float32 (interleaved)
            usize       frameCount  = 0;        ///< Nombre de frames (samples/channels)

            // ── Métadonnées ──────────────────────────────────────────────
            int32       sampleRate  = 48000;    ///< Fréquence d'échantillonnage (Hz)
            int32       channels    = 2;        ///< Nombre de canaux
            AudioFormat format      = AudioFormat::UNKNOWN;

            // ── Durée calculée ───────────────────────────────────────────
            NKENTSEU_INLINE float32 GetDuration()    const { return (sampleRate > 0) ? (float32)frameCount / (float32)sampleRate : 0.0f; }
            NKENTSEU_INLINE usize   GetSampleCount() const { return frameCount * (usize)channels; }
            NKENTSEU_INLINE bool    IsValid()        const { return data != nullptr && frameCount > 0 && sampleRate > 0 && channels > 0; }

            // ── Allocateur ───────────────────────────────────────────────
            memory::NkAllocator* mAllocator = nullptr; ///< Allocateur responsable (jamais nullptr si IsValid())
        };

        // ----------------------------------------------------------------

        /**
         * @brief Source audio 3D (position, mouvement, cône)
         *
         * Paramètres spatiaux attachés à une voix.
         * Compatible HRTF (Head-Related Transfer Function).
         */
        struct NKENTSEU_AUDIO_API AudioSource3D {
            float32 position[3]     = { 0.0f, 0.0f, 0.0f };   ///< Position monde
            float32 velocity[3]     = { 0.0f, 0.0f, 0.0f };   ///< Vitesse (Doppler)
            float32 direction[3]    = { 0.0f, 0.0f, -1.0f };  ///< Direction de visée

            float32 minDistance     = 1.0f;     ///< Distance sans atténuation
            float32 maxDistance     = 100.0f;   ///< Distance silence total
            float32 rolloffFactor   = 1.0f;     ///< Facteur de chute

            float32 coneInnerAngle  = 360.0f;   ///< Cône intérieur (volume plein) en degrés
            float32 coneOuterAngle  = 360.0f;   ///< Cône extérieur en degrés
            float32 coneOuterGain   = 0.0f;     ///< Gain hors du cône

            AttenuationModel attenuationModel = AttenuationModel::INVERSE;
            bool             positional       = false; ///< Active le mode 3D
            bool             useDoppler       = true;  ///< Active l'effet Doppler
        };

        // ----------------------------------------------------------------

        /**
         * @brief Listener 3D (oreilles du joueur / microphone)
         */
        struct NKENTSEU_AUDIO_API AudioListener3D {
            float32 position[3] = { 0.0f, 0.0f,  0.0f };
            float32 velocity[3] = { 0.0f, 0.0f,  0.0f };
            float32 forward[3]  = { 0.0f, 0.0f, -1.0f };
            float32 up[3]       = { 0.0f, 1.0f,  0.0f };
        };

        // ----------------------------------------------------------------

        /**
         * @brief Paramètres de lecture d'une voix
         */
        struct NKENTSEU_AUDIO_API VoiceParams {
            float32         volume          = 1.0f;     ///< Volume [0, 1+]
            float32         pitch           = 1.0f;     ///< Pitch multiplicateur
            float32         pan             = 0.0f;     ///< Panoramique [-1, 1]
            bool            looping         = false;    ///< Lecture en boucle
            float32         loopStart       = 0.0f;     ///< Début de la boucle (s)
            float32         loopEnd         = -1.0f;    ///< Fin de boucle (-1 = fin)
            float32         fadeInTime      = 0.0f;     ///< Fondu entrant (s)
            float32         startOffset     = 0.0f;     ///< Offset de départ (s)
            int32           priority        = 128;      ///< Priorité de voix [0-255]
            AudioSource3D   source3d;                   ///< Paramètres spatiaux
        };

        // ----------------------------------------------------------------

        /**
         * @brief Envelope ADSR pour synthèse et modulation
         */
        struct NKENTSEU_AUDIO_API AdsrEnvelope {
            float32 attackTime   = 0.01f;   ///< Attaque (s)
            float32 decayTime    = 0.1f;    ///< Déclin (s)
            float32 sustainLevel = 0.7f;    ///< Niveau soutien [0,1]
            float32 releaseTime  = 0.3f;    ///< Relâche (s)
        };

        // ----------------------------------------------------------------

        /**
         * @brief Résultat retourné par AudioAnalyzer::ComputeFFT
         */
        struct NKENTSEU_AUDIO_API FftResult {
            NkVector<float32>   magnitudes;     ///< Magnitudes par bin
            NkVector<float32>   phases;         ///< Phases par bin (radians)
            int32               fftSize  = 0;
            int32               sampleRate = 0;

            /// Fréquence d'un bin donné
            NKENTSEU_INLINE float32 BinToFrequency(int32 bin) const {
                if (fftSize <= 0 || sampleRate <= 0) return 0.0f;
                return (float32)bin * (float32)sampleRate / (float32)fftSize;
            }
        };

        // ----------------------------------------------------------------

        /**
         * @brief Paramètres d'initialisation du moteur audio
         */
        struct NKENTSEU_AUDIO_API AudioEngineConfig {
            AudioBackendType    backend         = AudioBackendType::AUTO;
            int32               sampleRate      = AUDIO_DEFAULT_SAMPLE_RATE;
            int32               channels        = AUDIO_DEFAULT_CHANNELS;
            int32               bufferSize      = AUDIO_DEFAULT_BUFFER_SIZE;
            int32               maxVoices       = AUDIO_MAX_VOICES;
            bool                enableHrtf      = false;    ///< Active le rendu binaurall
            bool                enableDoppler   = true;
            float32             masterVolume    = 1.0f;
            memory::NkAllocator* allocator      = nullptr;  ///< nullptr = allocateur global
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// INTERFACE EFFET AUDIO
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Interface de base pour tous les effets audio DSP
         *
         * Implémente le pattern Strategy pour le traitement DSP.
         * Les effets sont appliqués en chaîne (FX chain) sur chaque voix
         * et/ou sur le bus master. Traitement en Float32 interleaved.
         *
         * @note Process() est appelé depuis le thread audio (temps réel).
         *       NE PAS allouer de mémoire dans Process() !
         *
         * @see AudioEffectType
         */
        class NKENTSEU_AUDIO_API IAudioEffect {
        public:
            virtual ~IAudioEffect() = default;

            /**
             * @brief Traite un bloc d'audio en place (Float32 interleaved)
             *
             * @param buffer Buffer d'entrée/sortie (interleaved, Float32)
             * @param frameCount Nombre de frames dans le buffer
             * @param channels Nombre de canaux
             *
             * @note TEMPS RÉEL : zéro allocation, zéro lock
             * @note buffer != nullptr, frameCount > 0, channels > 0
             */
            virtual void Process(float32* buffer, int32 frameCount, int32 channels) = 0;

            /**
             * @brief Remet l'état interne à zéro (buffers de délai, etc.)
             */
            virtual void Reset() = 0;

            /**
             * @brief Informe l'effet d'un changement de sample rate
             *
             * @param sampleRate Nouveau taux d'échantillonnage
             */
            virtual void OnSampleRateChanged(int32 sampleRate) = 0;

            /// Type de cet effet
            virtual AudioEffectType GetType() const = 0;

            /// Permet de désactiver temporairement sans retirer de la chaîne
            virtual bool IsEnabled() const { return mEnabled; }
            virtual void SetEnabled(bool enabled) { mEnabled = enabled; }

            /// Gain de sortie de l'effet [0, 1+]
            virtual float32 GetWetMix() const  { return mWetMix; }
            virtual void    SetWetMix(float32 mix) { mWetMix = mix; }

        protected:
            bool    mEnabled = true;
            float32 mWetMix  = 1.0f;
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// INTERFACE BACKEND AUDIO
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Interface bas niveau vers le pilote audio de la plateforme
         *
         * Chaque plateforme (Windows/WASAPI, macOS/CoreAudio, Linux/ALSA, etc.)
         * fournit une implémentation concrète. Le callback audio est appelé
         * par le thread audio natif à fréquence fixe.
         *
         * @note Toutes les méthodes sauf SetCallback() sont thread-safe.
         */
        class NKENTSEU_AUDIO_API IAudioBackend {
        public:
            virtual ~IAudioBackend() = default;

            /**
             * @brief Type du callback audio (appelé depuis le thread RT)
             *
             * @param buffer   Buffer de sortie Float32 interleaved
             * @param frames   Nombre de frames à remplir
             * @param channels Nombre de canaux
             */
            using AudioCallback = NkFunction<void(float32* buffer, int32 frames, int32 channels)>;

            /// Initialise le backend avec la configuration donnée
            virtual bool Initialize(int32 sampleRate, int32 channels, int32 bufferSize) = 0;

            /// Libère toutes les ressources du backend
            virtual void Shutdown() = 0;

            /// Enregistre le callback de remplissage du buffer
            virtual void SetCallback(AudioCallback callback) = 0;

            /// Démarre le thread audio / streaming
            virtual void Start() = 0;

            /// Arrête le thread audio
            virtual void Stop() = 0;

            /// Suspend le flux audio (garde les ressources)
            virtual void Pause() = 0;

            /// Reprend après une pause
            virtual void Resume() = 0;

            virtual int32   GetSampleRate()  const = 0;
            virtual int32   GetChannels()    const = 0;
            virtual int32   GetBufferSize()  const = 0;

            /**
             * @brief Latence totale estimée en millisecondes
             * @note Prend en compte buffer hw + overhead plateforme
             */
            virtual float32 GetLatencyMs()   const = 0;

            virtual bool    IsRunning()      const = 0;

            /// Nom lisible du backend (ex: "WASAPI", "CoreAudio")
            virtual const char* GetName()    const = 0;
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUDIO LOADER
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Chargeur / décodeur de fichiers audio
         *
         * Supporte WAV (natif), et via décodeurs légers intégrés :
         * MP3 (minimp3), OGG (stb_vorbis), FLAC.
         * Toutes les sorties sont normalisées en Float32 interleaved.
         *
         * @note Zéro dépendance STL — utilise NkAllocator pour les buffers.
         */
        class NKENTSEU_AUDIO_API AudioLoader {
        public:
            /// Détecte le format depuis les magic bytes
            static AudioFormat DetectFormat(const uint8* data, usize size);

            /// Détecte depuis l'extension du chemin (ASCII uniquement)
            static AudioFormat DetectFormatFromPath(const char* path);

            /**
             * @brief Charge un fichier audio depuis le disque
             *
             * @param path      Chemin vers le fichier (ASCII/UTF-8)
             * @param allocator Allocateur pour les données PCM (nullptr = global)
             * @return          Sample valide ou IsValid() == false si échec
             */
            static AudioSample Load(const char* path,
                                    memory::NkAllocator* allocator = nullptr);

            /**
             * @brief Décode depuis un buffer mémoire déjà chargé
             *
             * @param data      Données brutes du fichier (non-PCM)
             * @param size      Taille des données
             * @param format    Format (UNKNOWN = auto-détection)
             * @param allocator Allocateur pour les données PCM
             */
            static AudioSample LoadFromMemory(const uint8*        data,
                                              usize               size,
                                              AudioFormat         format    = AudioFormat::UNKNOWN,
                                              memory::NkAllocator* allocator = nullptr);

            /**
             * @brief Exporte un sample en fichier WAV sur le disque
             *
             * @param path   Chemin de sortie
             * @param sample Sample à exporter (doit être IsValid())
             * @return       true si succès
             */
            static bool SaveWAV(const char* path, const AudioSample& sample);

            /**
             * @brief Libère les données PCM d'un sample
             *
             * @note Doit être appelé avec le même allocateur utilisé pour Load()
             */
            static void Free(AudioSample& sample);

            // ── Conversions DSP ──────────────────────────────────────────

            /**
             * @brief Convertit vers un autre SampleFormat (in-place)
             *
             * @note Effectue uniquement une réinterprétation + scaling.
             *       La sortie interne reste Float32.
             */
            static void ConvertSampleFormat(AudioSample& sample, SampleFormat target);

            /**
             * @brief Resample (interpolation linéaire ou Sinc)
             *
             * @param sample           Sample à resampler
             * @param targetSampleRate Fréquence cible
             * @param highQuality      true = Sinc (qualité studio), false = linéaire
             */
            static void Resample(AudioSample& sample, int32 targetSampleRate, bool highQuality = true);

            /**
             * @brief Conversion mono <-> stéréo / downmix / upmix
             *
             * @param sample         Sample source
             * @param targetChannels Nombre de canaux cibles (1, 2, 4, 6, 8)
             */
            static void ConvertChannels(AudioSample& sample, int32 targetChannels);

        private:
            // ── Loaders internes ─────────────────────────────────────────
            static AudioSample LoadWAV (const uint8* data, usize size, memory::NkAllocator* alloc);
            static AudioSample LoadMP3 (const uint8* data, usize size, memory::NkAllocator* alloc);
            static AudioSample LoadOGG (const uint8* data, usize size, memory::NkAllocator* alloc);
            static AudioSample LoadFLAC(const uint8* data, usize size, memory::NkAllocator* alloc);

            /// Resampler Sinc (Kaiser-windowed)
            static void ResampleSinc(AudioSample& sample, int32 targetSampleRate);

            /// Resampler linéaire (rapide, acceptable pour jeux)
            static void ResampleLinear(AudioSample& sample, int32 targetSampleRate);
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUDIO GENERATOR (SYNTHÈSE PROCÉDURALE)
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Générateur de signaux audio et synthèse procédurale
         *
         * Fournit les briques de base pour créer des sons sans fichier :
         * oscillateurs, enveloppes, LFO, synthèse additive.
         * Indispensable pour DAW/beatmakers AAA.
         *
         * @example Créer une note de piano synthétique
         * @code
         * AdsrEnvelope env;
         * env.attackTime = 0.005f; env.decayTime = 0.1f;
         * env.sustainLevel = 0.6f; env.releaseTime = 0.8f;
         *
         * AudioSample note = AudioGenerator::GenerateTone(
         *     440.0f, 2.0f, WaveformType::SINE);
         * AudioGenerator::ApplyEnvelope(note, env);
         * @endcode
         */
        class NKENTSEU_AUDIO_API AudioGenerator {
        public:
            /**
             * @brief Génère un ton périodique (oscillateur)
             *
             * @param frequency  Fréquence fondamentale en Hz
             * @param duration   Durée en secondes
             * @param waveform   Forme d'onde
             * @param sampleRate Fréquence d'échantillonnage
             * @param amplitude  Amplitude normalisée [0, 1]
             * @param allocator  Allocateur pour le buffer de sortie
             */
            static AudioSample GenerateTone(float32              frequency,
                                            float32              duration,
                                            WaveformType         waveform    = WaveformType::SINE,
                                            int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                            float32              amplitude   = 0.8f,
                                            memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Génère un bruit (blanc ou rose)
             *
             * @param duration   Durée en secondes
             * @param type       NOISE_WHITE ou NOISE_PINK
             * @param sampleRate Fréquence d'échantillonnage
             * @param amplitude  Amplitude normalisée [0, 1]
             */
            static AudioSample GenerateNoise(float32              duration,
                                             WaveformType         type        = WaveformType::NOISE_WHITE,
                                             int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                             float32              amplitude   = 0.8f,
                                             memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Génère un chirp (sweep de fréquence logarithmique)
             *
             * Utile pour tests de réponse en fréquence et effets SFX.
             *
             * @param startFreq Fréquence de départ (Hz)
             * @param endFreq   Fréquence de fin (Hz)
             * @param duration  Durée (s)
             */
            static AudioSample GenerateChirp(float32              startFreq,
                                             float32              endFreq,
                                             float32              duration,
                                             int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                             memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Génère un accord (synthèse additive de plusieurs fréquences)
             *
             * @param frequencies Tableau de fréquences (Hz)
             * @param count       Nombre de fréquences
             * @param duration    Durée (s)
             * @param amplitudes  Amplitudes individuelles (nullptr = égales)
             */
            static AudioSample GenerateChord(const float32*       frequencies,
                                             int32                count,
                                             float32              duration,
                                             const float32*       amplitudes  = nullptr,
                                             int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                             memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Applique une enveloppe ADSR à un sample
             *
             * @param sample   Sample à modifier (in-place)
             * @param envelope Paramètres ADSR
             */
            static void ApplyEnvelope(AudioSample& sample, const AdsrEnvelope& envelope);

            /**
             * @brief Applique un LFO (oscillateur basse fréquence) sur le volume
             *
             * @param sample    Sample à moduler (in-place)
             * @param rate      Fréquence du LFO (Hz, typiquement 0.1 - 10)
             * @param depth     Profondeur de modulation [0, 1]
             * @param waveform  Forme d'onde du LFO
             */
            static void ApplyLfo(AudioSample& sample,
                                 float32      rate,
                                 float32      depth    = 0.5f,
                                 WaveformType waveform = WaveformType::SINE);

            /**
             * @brief Génère un kick drum (synthèse FM basique)
             *
             * Kick synthétique de type beatmaker/DAW.
             * Descente de fréquence exponentielle + click transient.
             *
             * @param duration    Durée totale (s)
             * @param startFreq   Fréquence de départ (Hz, typique: 150-200)
             * @param endFreq     Fréquence de fin (Hz, typique: 40-60)
             * @param clickAttack Durée du transient de clic (s)
             */
            static AudioSample GenerateKick(float32              duration    = 0.5f,
                                            float32              startFreq   = 180.0f,
                                            float32              endFreq     = 40.0f,
                                            float32              clickAttack = 0.002f,
                                            int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                            memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Génère un snare drum (bruit + ton)
             *
             * @param duration   Durée (s)
             * @param toneFreq   Fréquence du corps (Hz)
             * @param noiseMix   Proportion de bruit [0,1]
             */
            static AudioSample GenerateSnare(float32              duration    = 0.2f,
                                             float32              toneFreq    = 200.0f,
                                             float32              noiseMix    = 0.7f,
                                             int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                             memory::NkAllocator* allocator   = nullptr);

            /**
             * @brief Génère une hihat (charleston)
             *
             * @param duration   Durée (s)
             * @param closed     true = fermée, false = ouverte
             */
            static AudioSample GenerateHihat(float32              duration    = 0.05f,
                                             bool                 closed      = true,
                                             int32                sampleRate  = AUDIO_DEFAULT_SAMPLE_RATE,
                                             memory::NkAllocator* allocator   = nullptr);

        private:
            /// Évalue un oscillateur de type @p waveform à la phase @p phase
            static float32 EvaluateWaveform(WaveformType waveform, float32 phase, float32 pulseWidth = 0.5f);

            /// Générateur de bruit rose (filtre de Voss-McCartney)
            struct PinkNoiseState { float32 b[7] = {}; };
            static float32 GeneratePinkNoiseSample(PinkNoiseState& state);
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUDIO MIXER
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Mixeur audio offline et temps réel
         *
         * Opérations de composition : mix, crossfade, normalisation,
         * fondu, concaténation. Toutes les opérations restituent un
         * nouveau AudioSample alloué via l'allocateur fourni.
         *
         * @note Pour le mixage temps réel, utiliser AudioEngine.
         *       Ce mixeur est destiné aux opérations offline (pré-mixage,
         *       export, édition de pistes type DAW).
         */
        class NKENTSEU_AUDIO_API AudioMixer {
        public:
            /**
             * @brief Construit un mixeur offline
             *
             * @param sampleRate Taux d'échantillonnage de travail
             * @param channels   Nombre de canaux de sortie
             * @param allocator  Allocateur pour les buffers intermédiaires
             */
            explicit AudioMixer(int32                sampleRate = AUDIO_DEFAULT_SAMPLE_RATE,
                                int32                channels   = AUDIO_DEFAULT_CHANNELS,
                                memory::NkAllocator* allocator  = nullptr);

            /**
             * @brief Mixe plusieurs samples ensemble (sommation)
             *
             * @param samples   Tableau de samples sources
             * @param count     Nombre de samples
             * @param volumes   Volumes individuels (nullptr = 1.0 pour tous)
             * @return          Sample résultant
             */
            AudioSample Mix(const AudioSample* samples,
                            int32              count,
                            const float32*     volumes = nullptr);

            /**
             * @brief Fondu enchaîné entre deux samples
             *
             * @param a            Piste de départ
             * @param b            Piste d'arrivée
             * @param crossfadeLen Durée du crossfade (s)
             */
            AudioSample Crossfade(const AudioSample& a,
                                  const AudioSample& b,
                                  float32            crossfadeLen);

            /**
             * @brief Concatène des samples bout à bout
             *
             * @param samples Tableau de samples
             * @param count   Nombre de samples
             */
            AudioSample Concatenate(const AudioSample* samples, int32 count);

            /**
             * @brief Normalise le peak du signal à @p targetPeak
             *
             * @param sample     Sample à normaliser (in-place)
             * @param targetPeak Peak cible [0, 1]
             */
            void Normalize(AudioSample& sample, float32 targetPeak = 0.95f);

            /// Fondu entrant (in-place)
            void FadeIn(AudioSample& sample, float32 duration);

            /// Fondu sortant (in-place)
            void FadeOut(AudioSample& sample, float32 duration);

            /**
             * @brief Ajuste le gain d'un sample (in-place)
             *
             * @param sample Sample à modifier
             * @param gain   Gain à appliquer (1.0 = inchangé)
             */
            void ApplyGain(AudioSample& sample, float32 gain);

            /**
             * @brief Reverse le sample (in-place)
             */
            void Reverse(AudioSample& sample);

            /**
             * @brief Insère un sample dans un autre à une position temporelle
             *
             * @param target   Sample de destination (modifié)
             * @param insert   Sample à insérer
             * @param positionSec Position en secondes
             * @param mix      true = mix, false = remplacement
             */
            void Insert(AudioSample&       target,
                        const AudioSample& insert,
                        float32            positionSec,
                        bool               mix = true);

        private:
            int32                mSampleRate;
            int32                mChannels;
            memory::NkAllocator* mAllocator;

            /// Alloue et initialise un sample vide
            AudioSample AllocateSample(usize frameCount) const;

            /// Garantit que sample est en Float32 / canaux cibles
            void NormalizeSample(AudioSample& sample) const;
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUDIO ANALYZER
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Analyse audio : FFT, spectre, tempo, pitch
         *
         * Outils d'analyse DSP pour visualisation en temps réel
         * (analyseur de spectre type FL Studio), détection de tempo/BPM,
         * détection de note (pitch tracking), et métrologie (RMS, peak).
         *
         * @note ComputeFFT(), ComputeSpectrogram() → allocations offline
         *       Pour temps réel, utiliser les variantes avec buffer pré-alloué.
         */
        class NKENTSEU_AUDIO_API AudioAnalyzer {
        public:
            /// Volume RMS (Root Mean Square) d'un sample
            static float32 CalculateRms(const AudioSample& sample);

            /// Peak absolu (valeur maximale de |sample|)
            static float32 CalculatePeak(const AudioSample& sample);

            /**
             * @brief Détection du tempo (BPM) par autocorrélation
             *
             * @param sample      Sample analysé (≥ 2 secondes recommandé)
             * @param minBpm      BPM minimum (défaut: 60)
             * @param maxBpm      BPM maximum (défaut: 240)
             * @return            BPM détecté, 0 si indétectable
             */
            static float32 DetectTempo(const AudioSample& sample,
                                       float32            minBpm = 60.0f,
                                       float32            maxBpm = 240.0f);

            /**
             * @brief Détection de pitch (fréquence dominante) via autocorrélation YIN
             *
             * @param sample   Sample analysé (mono, canal 0 si stéréo)
             * @param minFreq  Fréquence minimale (Hz)
             * @param maxFreq  Fréquence maximale (Hz)
             * @return         Fréquence fondamentale estimée en Hz, 0 si indétectable
             */
            static float32 DetectPitch(const AudioSample& sample,
                                       float32            minFreq = 50.0f,
                                       float32            maxFreq = 5000.0f);

            /**
             * @brief Transformée de Fourier Rapide (FFT) via Cooley-Tukey
             *
             * @param sample      Sample analysé (premier canal utilisé)
             * @param fftSize     Taille FFT (doit être puissance de 2)
             * @param windowType  0=Rectangle, 1=Hann, 2=Hamming, 3=Blackman
             * @param allocator   Allocateur pour les résultats
             * @return            Résultat avec magnitudes et phases
             */
            static FftResult ComputeFft(const AudioSample&   sample,
                                        int32                fftSize    = AUDIO_DEFAULT_FFT_SIZE,
                                        int32                windowType = 1,
                                        memory::NkAllocator* allocator  = nullptr);

            /**
             * @brief Calcule la FFT en place sur un buffer temps-réel (non alloué)
             *
             * Version temps réel : aucune allocation. Le buffer doit être
             * pré-alloué de taille @p fftSize en Float32.
             *
             * @param inputReal   Buffer d'entrée (real) — modifié en sortie
             * @param outputMag   Magnitudes de sortie (fftSize/2+1 floats)
             * @param fftSize     Taille (puissance de 2)
             */
            static void ComputeFftRealtime(const float32* inputReal,
                                           float32*       outputMag,
                                           int32          fftSize);

            /**
             * @brief Spectrogramme (STFT - Short-Time Fourier Transform)
             *
             * Retourne une matrice [temps][fréquence] de magnitudes.
             * Utile pour visualisation type FL Studio.
             *
             * @param sample     Sample source
             * @param windowSize Taille de la fenêtre (frames)
             * @param hopSize    Pas entre fenêtres (frames)
             * @param allocator  Allocateur
             */
            static NkVector<FftResult> ComputeSpectrogram(const AudioSample&   sample,
                                                           int32                windowSize = 2048,
                                                           int32                hopSize    = 512,
                                                           memory::NkAllocator* allocator  = nullptr);

            /**
             * @brief Calcule les énergies par bande de fréquence (EQ bands)
             *
             * Retourne l'énergie normalisée [0,1] dans N bandes fréquentielles
             * (grave, bas-médium, médium, aigu, très aigu).
             * Idéal pour visualisation analyseur à barres en temps réel.
             *
             * @param magnitudes  Magnitudes FFT (sortie de ComputeFftRealtime)
             * @param fftSize     Taille FFT
             * @param sampleRate  Sample rate
             * @param bands       Tableau de sortie (N bandes)
             * @param bandCount   Nombre de bandes
             */
            static void ComputeBandEnergies(const float32* magnitudes,
                                            int32          fftSize,
                                            int32          sampleRate,
                                            float32*       bands,
                                            int32          bandCount);

        private:
            /// FFT Cooley-Tukey itérative (in-place, complex)
            static void Fft(float32* real, float32* imag, int32 size);

            /// Applique une fenêtre de pondération
            static void ApplyWindow(float32* buffer, int32 size, int32 windowType);

            /// Autocorrélation normalisée (YIN)
            static float32 YinPitchDetect(const float32* data, int32 count, int32 sampleRate,
                                          float32 minFreq, float32 maxFreq);
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// AUDIO ENGINE
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Moteur audio AAA — Singleton thread-safe
         *
         * Cœur du module NKAudio. Gère le pool de voix, le mixage temps
         * réel, les effets, l'audio 3D (HRTF), et le streaming.
         *
         * Architecture :
         *  ┌───────────────────────────────────────────────────┐
         *  │  AudioEngine (API principale, thread-safe)        │
         *  │  ├── VoicePool (AUDIO_MAX_VOICES voix, lock-free) │
         *  │  ├── MasterBus (effets globaux, EQ master)        │
         *  │  ├── AudioMixer (mixage Float32, SIMD)            │
         *  │  ├── SpatialProcessor (3D/HRTF)                   │
         *  │  └── IAudioBackend (WASAPI/CoreAudio/ALSA/…)      │
         *  └───────────────────────────────────────────────────┘
         *
         * @note Initialize() et Shutdown() sont appelés une seule fois
         *       depuis le thread principal. Toutes les autres méthodes
         *       sont thread-safe via des atomics et queues lock-free.
         *
         * @example Usage minimal
         * @code
         * AudioEngineConfig cfg;
         * cfg.sampleRate = 48000;
         * AudioEngine::Instance().Initialize(cfg);
         *
         * AudioSample sample = AudioLoader::Load("kick.wav");
         * AudioHandle h = AudioEngine::Instance().Play(sample);
         *
         * // Plus tard...
         * AudioEngine::Instance().Stop(h);
         * AudioLoader::Free(sample);
         * AudioEngine::Instance().Shutdown();
         * @endcode
         */
        class NKENTSEU_AUDIO_API AudioEngine {
        public:
            /// Accès au singleton
            static AudioEngine& Instance();

            // ── Cycle de vie ─────────────────────────────────────────────

            /**
             * @brief Initialise le moteur audio
             *
             * @param config Configuration complète
             * @return true si succès
             *
             * @pre !IsInitialized()
             * @post IsInitialized() == true si succès
             */
            bool Initialize(const AudioEngineConfig& config = AudioEngineConfig{});

            /**
             * @brief Arrête proprement le moteur
             *
             * Stoppe toutes les voix avec fade-out, libère le backend.
             *
             * @pre IsInitialized()
             * @post IsInitialized() == false
             */
            void Shutdown();

            bool IsInitialized() const;

            // ── Lecture ──────────────────────────────────────────────────

            /**
             * @brief Démarre la lecture d'un sample
             *
             * @param sample  Sample source (doit rester valide pendant la lecture)
             * @param params  Paramètres de lecture
             * @return        Handle de contrôle, invalide si pool plein
             */
            AudioHandle Play(const AudioSample& sample,
                             const VoiceParams& params = VoiceParams{});

            /**
             * @brief Lecture avec callback procédural (synthèse temps réel)
             *
             * Le callback est appelé depuis le thread audio pour remplir
             * les buffers à la demande. Idéal pour synthétiseur logiciel.
             *
             * @param callback Fonction de génération audio
             * @param params   Paramètres de la voix
             *
             * @note TEMPS RÉEL : callback ne doit pas allouer/locker
             */
            using ProceduralCallback = NkFunction<void(float32* buffer, int32 frames, int32 channels)>;
            AudioHandle PlayProcedural(ProceduralCallback callback,
                                       const VoiceParams& params = VoiceParams{});

            // ── Contrôle voix ────────────────────────────────────────────

            void    Stop      (AudioHandle handle, float32 fadeOutTime = 0.0f);
            void    Pause     (AudioHandle handle);
            void    Resume    (AudioHandle handle);

            bool    IsPlaying (AudioHandle handle) const;
            bool    IsPaused  (AudioHandle handle) const;
            bool    IsLooping (AudioHandle handle) const;

            void    SetVolume (AudioHandle handle, float32 volume);
            void    SetPitch  (AudioHandle handle, float32 pitch);
            void    SetPan    (AudioHandle handle, float32 pan);
            void    SetLooping(AudioHandle handle, bool looping);

            float32 GetVolume (AudioHandle handle) const;
            float32 GetPitch  (AudioHandle handle) const;
            float32 GetPan    (AudioHandle handle) const;

            /**
             * @brief Obtient / définit la position de lecture en secondes
             */
            float32 GetPlaybackPosition(AudioHandle handle) const;
            void    SetPlaybackPosition(AudioHandle handle, float32 seconds);

            // ── Audio 3D ─────────────────────────────────────────────────

            void SetSourcePosition   (AudioHandle handle, float32 x, float32 y, float32 z);
            void SetSourceVelocity   (AudioHandle handle, float32 x, float32 y, float32 z);
            void SetSourceDirection  (AudioHandle handle, float32 x, float32 y, float32 z);
            void SetSourcePositional (AudioHandle handle, bool positional);
            void SetSourceMinDistance(AudioHandle handle, float32 dist);
            void SetSourceMaxDistance(AudioHandle handle, float32 dist);

            void SetListenerPosition   (float32 x, float32 y, float32 z);
            void SetListenerVelocity   (float32 x, float32 y, float32 z);
            void SetListenerOrientation(float32 fwdX, float32 fwdY, float32 fwdZ,
                                        float32 upX,  float32 upY,  float32 upZ);

            // ── Effets sur voix ──────────────────────────────────────────

            /**
             * @brief Ajoute un effet à la chaîne d'une voix
             *
             * @param handle Handle de la voix
             * @param effect Pointeur vers l'effet (ownership = appelant)
             * @return true si ajout réussi (chaîne non pleine)
             */
            bool AddEffect   (AudioHandle handle, IAudioEffect* effect);
            void RemoveEffect(AudioHandle handle, IAudioEffect* effect);
            void ClearEffects(AudioHandle handle);

            // ── Bus Master ───────────────────────────────────────────────

            bool AddMasterEffect   (IAudioEffect* effect);
            void RemoveMasterEffect(IAudioEffect* effect);
            void ClearMasterEffects();

            // ── Contrôles globaux ────────────────────────────────────────

            void    SetMasterVolume(float32 volume);
            float32 GetMasterVolume() const;

            void StopAll();
            void PauseAll();
            void ResumeAll();

            // ── Informations ─────────────────────────────────────────────

            AudioBackendType GetBackendType()  const;
            const char*      GetBackendName()  const;
            int32            GetSampleRate()   const;
            int32            GetChannels()     const;
            int32            GetBufferSize()   const;
            float32          GetLatencyMs()    const;
            int32            GetActiveVoices() const;
            int32            GetMaxVoices()    const;

        private:
            AudioEngine();
            ~AudioEngine();

            AudioEngine(const AudioEngine&)            = delete;
            AudioEngine& operator=(const AudioEngine&) = delete;

            // Implémentation cachée (PIMPL — compilation rapide)
            struct Impl;
            Impl* mImpl = nullptr;
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// BACKEND FACTORY
// ============================================================

namespace nkentseu {
    namespace audio {

        /**
         * @brief Factory pour créer et enregistrer des backends audio
         *
         * Permet d'ajouter des backends custom (plugin audio, hardware
         * propriétaire) sans modifier le moteur principal.
         *
         * @example Enregistrer un backend custom
         * @code
         * AudioBackendFactory::Register("MyHardware", []() -> IAudioBackend* {
         *     return new MyHardwareBackend();
         * });
         * @endcode
         */
        class NKENTSEU_AUDIO_API AudioBackendFactory {
        public:
            using CreatorFunc = NkFunction<IAudioBackend*()>;

            /// Enregistre un backend sous un nom
            static void Register(const char* name, CreatorFunc creator);

            /// Crée un backend par nom (nullptr si non trouvé)
            static IAudioBackend* Create(const char* name);

            /// Crée le backend par défaut selon la plateforme
            static IAudioBackend* CreateDefault();

            /// Crée un backend par type enum
            static IAudioBackend* CreateByType(AudioBackendType type);
        };

    } // namespace audio
} // namespace nkentseu

// ============================================================
// MACROS UTILITAIRES
// ============================================================

/**
 * @brief Enregistre un backend audio custom au démarrage
 *
 * @param BackendClass Classe du backend
 * @param Name         Nom d'enregistrement (const char*)
 */
#define NK_REGISTER_AUDIO_BACKEND(BackendClass, Name)                     \
    namespace {                                                            \
        struct BackendClass##_AutoRegister {                               \
            BackendClass##_AutoRegister() {                                \
                nkentseu::audio::AudioBackendFactory::Register(           \
                    Name,                                                  \
                    []() -> nkentseu::audio::IAudioBackend* {             \
                        return new BackendClass();                         \
                    }                                                      \
                );                                                         \
            }                                                              \
        };                                                                 \
        static BackendClass##_AutoRegister g_##BackendClass##_Registrar;  \
    }

#endif // NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIO_H

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-04-18
// Creation Date: 2026-04-18
// ============================================================
