#pragma once
// =============================================================================
// Humanoid/AI/Conversation/NkConversationEngine.h
// =============================================================================
// Interface unifiée pour les LLMs locaux et distants.
//
// PRINCIPE :
//   Le moteur de conversation reçoit un contexte (personnalité + état clinique
//   + historique de la conversation) et produit une réponse textuelle.
//   La réponse est ensuite transmise à NkSpeechEngine pour la synthèse vocale
//   et à NkHumanoidBehavior pour les réactions non-verbales.
//
// BACKENDS DISPONIBLES :
//   NkOllamaBackend   — LLM local via Ollama (Llama3, Mistral, Phi-3, Qwen...)
//   NkClaudeBackend   — Claude API d'Anthropic (qualité maximale)
//   NkOpenAIBackend   — OpenAI GPT-4o (alternatif)
//   NkRuleBackend     — Règles statiques (fallback sans réseau)
//
// USAGE :
//   NkConversationEngine conv;
//   conv.Init(NkConvBackendType::OllamaLocal, "mistral:7b-instruct");
//
//   NkConvRequest req;
//   req.userMessage = "Où avez-vous mal exactement?";
//   req.personality = &myPersonality;
//   req.clinicalState = &myState;
//   conv.Ask(req, [](const NkConvResponse& r) {
//       speechEngine.Speak(r.text.CStr());
//       behaviorEngine.AddStimulus(...);
//   });
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "PV3DE/Core/NkPersonality.h"

namespace nkentseu {
    namespace humanoid {

        // Forward
        class NkClinicalStateRef;

        // =====================================================================
        // Backend disponibles
        // =====================================================================
        enum class NkConvBackendType : nk_uint8 {
            OllamaLocal = 0,  // LLM local via Ollama (recommandé production)
            ClaudeAPI,        // Anthropic Claude (qualité max, nécessite clé API)
            OpenAIAPI,        // OpenAI GPT-4o (alternatif)
            MistralAPI,       // Mistral AI API (open source, RGPD EU)
            RulesBased,       // Fallback statique (pas de réseau requis)
        };

        // =====================================================================
        // Message dans l'historique de conversation
        // =====================================================================
        enum class NkConvRole : nk_uint8 { System = 0, User, Assistant };

        struct NkConvMessage {
            NkConvRole role;
            NkString   content;
            nk_float32 timestamp = 0.f;
        };

        // =====================================================================
        // Requête de conversation
        // =====================================================================
        struct NkConvRequest {
            NkString          userMessage;       // ce que dit le médecin
            const NkPersonality*  personality = nullptr;

            // État clinique simplifié (pas le header complet pour éviter dépendance)
            nk_float32  painLevel           = 0.f;
            nk_float32  anxietyLevel        = 0.f;
            nk_float32  fatigueLevel        = 0.f;
            nk_float32  breathingDifficulty = 0.f;
            nk_float32  heartRate           = 72.f;
            nk_float32  temperature         = 37.f;
            nk_float32  spo2                = 98.f;

            // Contexte de la scène
            NkString    currentCase;   // nom du cas clinique actif
            NkString    topDiagnosis;  // hypothèse principale courante

            // Température LLM (0=déterministe, 1=créatif)
            nk_float32  temperature_llm = 0.7f;
            nk_uint32   maxTokens = 150; // longueur max de la réponse
        };

        // =====================================================================
        // Réponse du LLM
        // =====================================================================
        struct NkConvResponse {
            NkString   text;           // texte de la réponse du patient
            bool       success = false;
            NkString   error;

            // Annotations extraites (si le LLM les produit)
            nk_float32 emotionIntensity = 0.f; // [0,1] intensité émotionnelle perçue
            NkString   dominantEmotion;         // "douleur", "anxiété", etc.
            bool       wantsToStop    = false;  // patient veut arrêter la consultation
        };

        // =====================================================================
        // Interface backend abstraite
        // =====================================================================
        class NkIConvBackend {
        public:
            virtual ~NkIConvBackend() noexcept = default;

            virtual bool Init(const char* endpoint,
                              const char* modelName,
                              const char* apiKey = nullptr) noexcept = 0;

            // Appel synchrone (bloque jusqu'à la réponse)
            virtual bool Complete(const NkVector<NkConvMessage>& messages,
                                  nk_float32 temperature,
                                  nk_uint32  maxTokens,
                                  NkConvResponse& out) noexcept = 0;

            virtual bool IsAvailable() const noexcept = 0;
            virtual const char* BackendName() const noexcept = 0;
        };

        // =====================================================================
        // NkConversationEngine — orchestrateur principal
        // =====================================================================
        class NkConversationEngine {
        public:
            NkConversationEngine()  = default;
            ~NkConversationEngine() noexcept;

            // ── Init ──────────────────────────────────────────────────────────
            bool Init(NkConvBackendType type,
                      const char* modelName  = "mistral:7b-instruct",
                      const char* endpoint   = "http://localhost:11434",
                      const char* apiKey     = nullptr) noexcept;

            void Shutdown() noexcept;

            // ── Conversation ──────────────────────────────────────────────────

            // Appel synchrone — bloque jusqu'à la réponse
            // (appeler depuis un thread séparé si temps réel)
            NkConvResponse Ask(const NkConvRequest& req) noexcept;

            // Réinitialiser l'historique (nouvelle consultation)
            void ResetHistory() noexcept;

            // Accès à l'historique
            const NkVector<NkConvMessage>& GetHistory() const noexcept {
                return mHistory;
            }

            bool IsReady() const noexcept { return mBackend != nullptr; }

        private:
            // Construction du prompt système
            NkString BuildSystemPrompt(const NkConvRequest& req) const noexcept;

            // Ajout d'un message à l'historique
            void PushMessage(NkConvRole role, const NkString& content) noexcept;

            // Extraction des annotations émotionnelles de la réponse
            static void ParseAnnotations(NkConvResponse& resp) noexcept;

            NkIConvBackend*         mBackend = nullptr;
            NkVector<NkConvMessage> mHistory;

            static constexpr nk_uint32 kMaxHistory = 20; // messages gardés
        };

    } // namespace humanoid
} // namespace nkentseu
