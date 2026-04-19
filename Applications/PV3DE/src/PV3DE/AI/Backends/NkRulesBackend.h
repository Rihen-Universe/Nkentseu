#pragma once
#include "NkConversationEngine.h"
namespace nkentseu { namespace humanoid {
    // Fallback statique — pas de réseau requis
    class NkRulesBackend final : public NkIConvBackend {
    public:
        bool Init(const char*,const char*,const char*) noexcept override {return true;}
        bool Complete(const NkVector<NkConvMessage>& msgs,
                      nk_float32, nk_uint32, NkConvResponse& out) noexcept override;
        bool IsAvailable() const noexcept override { return true; }
        const char* BackendName() const noexcept override { return "RulesBased"; }
    };
}} // namespace
