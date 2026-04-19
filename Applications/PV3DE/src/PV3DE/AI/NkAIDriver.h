#pragma once
// =============================================================================
// Humanoid/AI/NkAIDriver.h
// =============================================================================
// Orchestrateur final : connecte NkConversationEngine + NkHumanoidBehavior
// dans une boucle cohérente.
//
// REMPLACE dans PatientLayer :
//   - Le switch manuel EmotionState → ApplyPreset
//   - Les appels directs à NkSpeechEngine::GetResponse()
//
// USAGE dans PatientLayer::OnUpdate() :
//   mAIDriver.Update(dt, mClinicalState);
//
// USAGE pour répondre à une question :
//   mAIDriver.AskQuestion("Où avez-vous mal?", mClinicalState);
//   // → Lance la réponse LLM dans un thread séparé
//   // → Quand disponible : appelle mSpeechEngine.Speak() + mBehavior.AddStimulus()
// =============================================================================

#include "Conversation/NkConversationEngine.h"
#include "PV3DE/Behavior/NkHumanoidBehavior.h"
#include "NKThreading/NkThread.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {
    namespace humanoid {

        // Callback appelé depuis le thread de réponse (thread-safe via EventBus)
        using NkAIResponseCallback =
            NkFunction<void(const NkString& text, nk_float32 emotionIntensity)>;

        struct NkAIDriverConfig {
            NkConvBackendType backendType = NkConvBackendType::OllamaLocal;
            NkString modelName    = "mistral:7b-instruct";
            NkString endpoint     = "http://localhost:11434";
            NkString apiKey;              // vide = lire depuis env

            bool asyncResponse    = true;  // réponse dans un thread séparé
            bool enableBehavior   = true;  // utiliser NkHumanoidBehavior
            bool enableConversation = true; // utiliser LLM
        };

        class NkAIDriver {
        public:
            NkAIDriver() = default;
            ~NkAIDriver() noexcept { Shutdown(); }

            // ── Init ──────────────────────────────────────────────────────────
            bool Init(const NkAIDriverConfig& config,
                      const NkPersonality& personality) noexcept;
            void Shutdown() noexcept;

            // ── Update (appelé chaque frame) ──────────────────────────────────
            // Remplace le switch EmotionState → ApplyPreset dans PatientLayer
            void Update(nk_float32 dt,
                        nk_float32 painLevel,
                        nk_float32 anxietyLevel,
                        nk_float32 fatigueLevel) noexcept;

            // ── Conversation ──────────────────────────────────────────────────
            // Lance une question asynchrone vers le LLM.
            // La réponse arrive via le callback (depuis le thread principal).
            void AskQuestion(const char* question,
                             nk_float32 painLevel,
                             nk_float32 anxietyLevel,
                             nk_float32 fatigueLevel,
                             nk_float32 breathDifficulty,
                             nk_float32 hr, nk_float32 temp, nk_float32 spo2,
                             NkAIResponseCallback callback) noexcept;

            // Vérifier si une réponse est en cours de génération
            bool IsThinking() const noexcept { return mThinking; }

            // Poll : doit être appelé chaque frame pour récupérer la réponse
            // Retourne true si une réponse est disponible ce tick
            bool PollResponse() noexcept;

            // Accès aux sous-systèmes
            NkHumanoidBehavior&     GetBehavior()     noexcept { return mBehavior;     }
            NkConversationEngine&   GetConversation()  noexcept { return mConversation; }

            // Réinitialiser la mémoire de conversation (nouvelle consultation)
            void ResetConversation() noexcept;

            // Configurer le callback de réponse par défaut
            void SetResponseCallback(NkAIResponseCallback cb) noexcept {
                mDefaultCallback = cb;
            }

        private:
            void ProcessPendingResponse() noexcept;

            NkHumanoidBehavior   mBehavior;
            NkConversationEngine mConversation;
            NkAIDriverConfig     mConfig;

            // Thread de réponse LLM
            NkFunction<void()>   mWorkerTask;
            bool                 mThinking = false;

            // Réponse en attente (produite dans le thread LLM)
            struct PendingResponse {
                NkConvResponse response;
                NkAIResponseCallback callback;
                bool ready = false;
            } mPending;

            NkAIResponseCallback mDefaultCallback;
        };

    } // namespace humanoid
} // namespace nkentseu
