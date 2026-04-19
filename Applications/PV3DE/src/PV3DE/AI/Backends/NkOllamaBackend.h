#pragma once
// =============================================================================
// NkOllamaBackend.h — Backend LLM local via Ollama
// =============================================================================
// Ollama expose une API REST locale sur http://localhost:11434
// Supporte : Llama3, Mistral, Phi-3, Qwen, Gemma, etc.
//
// Installation Ollama :
//   Windows/macOS : télécharger depuis ollama.com
//   Linux         : curl -fsSL https://ollama.com/install.sh | sh
//
// Démarrer un modèle :
//   ollama pull mistral:7b-instruct
//   ollama pull llama3.2:3b
//   ollama run mistral:7b-instruct
//
// API utilisée : POST /api/chat
// =============================================================================

#include "NkConversationEngine.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace humanoid {

        class NkOllamaBackend final : public NkIConvBackend {
        public:
            NkOllamaBackend()  = default;
            ~NkOllamaBackend() noexcept override = default;

            bool Init(const char* endpoint,
                      const char* modelName,
                      const char* apiKey) noexcept override;

            bool Complete(const NkVector<NkConvMessage>& messages,
                          nk_float32 temperature,
                          nk_uint32  maxTokens,
                          NkConvResponse& out) noexcept override;

            bool IsAvailable() const noexcept override { return mAvailable; }
            const char* BackendName() const noexcept override { return "Ollama-Local"; }

        private:
            NkString BuildRequestJSON(const NkVector<NkConvMessage>& msgs,
                                       nk_float32 temp,
                                       nk_uint32  maxTok) const noexcept;

            bool ParseResponse(const NkString& json, NkConvResponse& out) const noexcept;

            // HTTP POST minimal (utilise NKStream ou socket POSIX)
            bool HttpPost(const char* url,
                          const NkString& body,
                          NkString& responseOut) const noexcept;

            NkString mEndpoint  = "http://localhost:11434";
            NkString mModelName = "mistral:7b-instruct";
            bool     mAvailable = false;
        };

    } // namespace humanoid
} // namespace nkentseu
