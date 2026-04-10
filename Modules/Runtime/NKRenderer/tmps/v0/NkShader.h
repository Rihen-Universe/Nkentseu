#pragma once
// =============================================================================
// NkShader.h  —  Shader RefCounted, sources séparées par backend
//
// RÈGLE ABSOLUE : chaque backend a ses propres sources.
//   GL   → GLSL 4.60 core  (SetGL)
//   VK   → GLSL SPIR-V     (SetVK / SetVKSpirv)
//   DX11 → HLSL SM 5.0     (SetDX11)
//   DX12 → HLSL SM 6.x     (SetDX12)
//   MTL  → MSL 2.x         (SetMTL)
//   SW   → nullptr (callbacks CPU)
//
// Pas de source "multi-backend" : un shader DX11 ≠ DX12, un shader GL ≠ VK.
// =============================================================================
#include "NKRenderer/Core/NkRef.h"
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace render {

class NkRenderDevice;

// =============================================================================
// Source d'un stage pour un backend
// =============================================================================
struct NkShaderStageSource {
    NkShaderStage    stage      = NkShaderStage::NK_VERTEX;
    NkString         source;          // GLSL / HLSL / MSL
    NkString         entryPoint = "main";
    NkVector<uint32> spirv;           // SPIR-V précompilé (VK uniquement)

    bool HasGLSL()  const { return !source.Empty(); }
    bool HasSPIRV() const { return !spirv.IsEmpty(); }
};

// =============================================================================
// Descripteur complet — sources séparées par backend
// =============================================================================
struct NkShaderDesc {
    const char* name = nullptr;

    // ── OpenGL 4.6 ──────────────────────────────────────────────────────────
    NkVector<NkShaderStageSource> gl;
    // ── Vulkan GLSL/SPIRV ───────────────────────────────────────────────────
    NkVector<NkShaderStageSource> vk;
    // ── DirectX 11 HLSL SM 5.0 ──────────────────────────────────────────────
    NkVector<NkShaderStageSource> dx11;
    // ── DirectX 12 HLSL SM 6.x ──────────────────────────────────────────────
    NkVector<NkShaderStageSource> dx12;
    // ── Metal MSL 2.x ───────────────────────────────────────────────────────
    NkVector<NkShaderStageSource> mtl;

    // ── API fluide ───────────────────────────────────────────────────────────
    NkShaderDesc& SetGL(NkShaderStage s, const char* src, const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.source=src; ss.entryPoint=entry;
        gl.PushBack(ss); return *this;
    }
    NkShaderDesc& SetVK(NkShaderStage s, const char* src, const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.source=src; ss.entryPoint=entry;
        vk.PushBack(ss); return *this;
    }
    NkShaderDesc& SetVKSpirv(NkShaderStage s, const uint32* words, uint32 count,
                               const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.entryPoint=entry;
        ss.spirv.Resize(count);
        for(uint32 i=0;i<count;++i) ss.spirv[i]=words[i];
        vk.PushBack(ss); return *this;
    }
    NkShaderDesc& SetDX11(NkShaderStage s, const char* src, const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.source=src; ss.entryPoint=entry;
        dx11.PushBack(ss); return *this;
    }
    NkShaderDesc& SetDX12(NkShaderStage s, const char* src, const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.source=src; ss.entryPoint=entry;
        dx12.PushBack(ss); return *this;
    }
    NkShaderDesc& SetMTL(NkShaderStage s, const char* src,
                          const char* entry="main") {
        NkShaderStageSource ss{}; ss.stage=s; ss.source=src; ss.entryPoint=entry;
        mtl.PushBack(ss); return *this;
    }

    bool IsValid() const {
        return !gl.IsEmpty()||!vk.IsEmpty()||!dx11.IsEmpty()||!dx12.IsEmpty()||!mtl.IsEmpty();
    }

    // Retourne les sources actives pour une API donnée
    const NkVector<NkShaderStageSource>* SourcesFor(NkGraphicsApi api) const;
};

// =============================================================================
// NkShader — ressource GPU RefCounted
// =============================================================================
class NkShader : public NkRefCounted {
public:
    const NkShaderDesc& Desc()    const { return mDesc; }
    const char*         Name()    const { return mDesc.name ? mDesc.name : ""; }
    uint64  RHIShaderId()         const { return mRHIShaderId; }
    bool    HasStage(NkShaderStage s) const {
        return (mActiveStages & (uint32)s) != 0;
    }

protected:
    friend class NkRenderDevice;
    explicit NkShader(const NkShaderDesc& d) : mDesc(d), mRHIShaderId(0), mActiveStages(0) {}
    virtual void Destroy() override;

    NkShaderDesc mDesc;
    uint64       mRHIShaderId  = 0;
    uint32       mActiveStages = 0;  // Flags NkShaderStage
    NkRenderDevice* mDevice    = nullptr;
};

using NkShaderPtr = NkRef<NkShader>;

// =============================================================================
// NkShaderLibrary — registre nommé, compilation lazy
// =============================================================================
class NkShaderLibrary {
public:
    void           Register(const char* name, NkShaderDesc desc);
    const NkShaderDesc* Find(const char* name) const;
    void           Remove(const char* name);
    uint32         Count() const;
    void           Clear();

    struct Entry { NkString name; NkShaderDesc desc; };
    const NkVector<Entry>& Entries() const { return mEntries; }

private:
    NkVector<Entry> mEntries;
};

} // namespace render
} // namespace nkentseu