#pragma once
// =============================================================================
// NkClaudeBackend.h — Backend Claude API (Anthropic)
// =============================================================================
// Utilise l'API Messages d'Anthropic : POST /v1/messages
// Documentation : https://docs.anthropic.com/en/api/messages
//
// Avantages pour PV3DE :
//   - Meilleure compréhension des nuances médicales
//   - Excellente cohérence de personnalité sur plusieurs tours
//   - Supporte les system prompts complexes
//
// Configuration :
//   Définir ANTHROPIC_API_KEY dans les variables d'environnement,
//   ou passer directement via Init(endpoint, model, apiKey).
//
// Modèles recommandés :
//   claude-3-5-haiku-20241022  : rapide, peu coûteux, suffisant pour PV3DE
//   claude-3-5-sonnet-20241022 : meilleure qualité, plus lent
// =============================================================================

#include "NkConversationEngine.h"

namespace nkentseu {
    namespace humanoid {

        class NkClaudeBackend final : public NkIConvBackend {
        public:
            bool Init(const char* endpoint,
                      const char* modelName,
                      const char* apiKey) noexcept override;

            bool Complete(const NkVector<NkConvMessage>& messages,
                          nk_float32 temperature,
                          nk_uint32  maxTokens,
                          NkConvResponse& out) noexcept override;

            bool IsAvailable() const noexcept override { return mApiKeySet; }
            const char* BackendName() const noexcept override { return "Claude-API"; }

        private:
            NkString BuildRequestJSON(const NkVector<NkConvMessage>& msgs,
                                       nk_float32 temp, nk_uint32 maxTok) const noexcept;
            bool ParseResponse(const NkString& json, NkConvResponse& out) const noexcept;
            bool HttpsPost(const NkString& body, NkString& out) const noexcept;

            NkString mApiKey;
            NkString mModel    = "claude-3-5-haiku-20241022";
            NkString mEndpoint = "https://api.anthropic.com";
            bool     mApiKeySet= false;
        };

    } // namespace humanoid
} // namespace nkentseu
