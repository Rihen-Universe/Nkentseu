#pragma once
// =============================================================================
// NkShaderBackend.h  — NKRenderer v4.0  (Shader/)
//
// Abstraction de compilation shader multi-API.
//
// Backends supportés :
//   GL   — GLSL OpenGL 4.60 core
//   VK   — GLSL Vulkan (SPIR-V via glslang)
//   DX11 — HLSL SM 5.0 (D3DCompile)
//   DX12 — HLSL SM 6.x (DXC)
//   MSL  — Metal Shading Language 2.x
//   NKSL — NkSL (NKRenderer Shader Language, compile vers tous les autres)
//
// Différences importantes par backend :
//   GL 4.60 :
//     • layout(location=N)  — attributs vertex
//     • layout(binding=N)   — samplers/UBOs
//     • texture() / textureLod()
//     • Pas de push constants
//     • Flip Y en sortie du vertex shader : gl_Position.y *= -1 (si convention Vulkan)
//
//   Vulkan GLSL :
//     • layout(set=N, binding=M) — descriptor sets
//     • layout(push_constant)    — push constants
//     • Y négatif dans la projection (convention Vulkan NDC)
//     • gl_VertexIndex au lieu de gl_VertexID
//     • #extension GL_ARB_separate_shader_objects : enable obligatoire
//
//   HLSL DX11 (SM 5.0) :
//     • cbuffer / ConstantBuffer<T> register(bN)
//     • Texture2D t : register(tN); SamplerState s : register(sN)
//     • SV_Position, SV_Target, SV_Depth
//     • float4x4 = row-major par défaut (vs column-major OpenGL)
//     • mul(v, M) — ordre inversé vs GLSL (M * v)
//
//   HLSL DX12 (SM 6.x) :
//     • Idem DX11 + bindless : ResourceDescriptorHeap[], SamplerDescriptorHeap[]
//     • RootConstants au lieu de push constants
//     • Wave intrinsics : WaveActiveSum(), WavePrefixSum(), etc.
//     • DXC requis (fxc obsolète)
//
//   MSL 2.x (Metal) :
//     • struct VertexIn / VertexOut avec [[attribute(N)]]
//     • constant T& ubo [[buffer(N)]]
//     • texture2d<float> t [[texture(N)]], sampler s [[sampler(N)]]
//     • Coordonnées UV Y inversé vs OpenGL
//     • Device functions : device, threadgroup, constant address spaces
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRenderer/Core/NkRendererConfig.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Étape de shader
    // =========================================================================
    enum class NkShaderStage : uint8 {
        NK_VERTEX    = 0,
        NK_FRAGMENT  = 1,
        NK_GEOMETRY  = 2,
        NK_COMPUTE   = 3,
        NK_TESS_CTRL = 4,
        NK_TESS_EVAL = 5,
        NK_MESH      = 6,   // DX12/Vulkan mesh shaders
        NK_TASK      = 7,
    };

    // =========================================================================
    // Résultat de compilation
    // =========================================================================
    struct NkShaderCompileResult {
        bool            success  = false;
        NkString        errors;
        NkVector<uint8> bytecode;   // SPIR-V (VK) ou DXBC/DXIL (DX11/12) ou source (GL/MSL)
        NkString        preprocessed; // pour debug
    };

    // =========================================================================
    // Options de compilation
    // =========================================================================
    struct NkShaderCompileOptions {
        NkString         entryPoint  = "main";
        NkString         defines;       // "DEFINE1=1;DEFINE2;..."
        bool             debug        = false;
        bool             optimize     = true;
        bool             generateSPIRV= false;  // pour VK depuis GLSL
        uint32           smMajor      = 5;
        uint32           smMinor      = 0;
    };

    // =========================================================================
    // NkShaderBackend — interface compilateur
    // =========================================================================
    class NkShaderBackend {
    public:
        virtual ~NkShaderBackend() = default;

        virtual NkShaderCompileResult Compile(const NkString&             source,
                                               NkShaderStage               stage,
                                               const NkShaderCompileOptions& opts = {}) = 0;

        virtual bool SupportsHotReload()  const = 0;
        virtual NkString GetBackendName() const = 0;
    };

    // =========================================================================
    // Backends concrets
    // =========================================================================
    class NkShaderBackendGL final : public NkShaderBackend {
    public:
        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;
        bool     SupportsHotReload() const override { return true; }
        NkString GetBackendName()    const override { return "OpenGL GLSL 4.60"; }
    };

    class NkShaderBackendVK final : public NkShaderBackend {
    public:
        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;
        bool     SupportsHotReload() const override { return false; }
        NkString GetBackendName()    const override { return "Vulkan GLSL → SPIR-V"; }
    };

    class NkShaderBackendDX11 final : public NkShaderBackend {
    public:
        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;
        bool     SupportsHotReload() const override { return true; }
        NkString GetBackendName()    const override { return "HLSL DX11 SM5"; }
    };

    class NkShaderBackendDX12 final : public NkShaderBackend {
    public:
        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;
        bool     SupportsHotReload() const override { return false; }
        NkString GetBackendName()    const override { return "HLSL DX12 SM6 (DXC)"; }
    };

    class NkShaderBackendMSL final : public NkShaderBackend {
    public:
        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;
        bool     SupportsHotReload() const override { return false; }
        NkString GetBackendName()    const override { return "Metal MSL 2.x"; }
    };

    // =========================================================================
    // NkShaderBackendNkSL — NkSL → backend cible
    // NkSL est un GLSL-like avec extensions :
    //   @vertex / @fragment / @compute — déclaration d'étapes
    //   @uniform Block { ... }         — uniform buffer
    //   @push { ... }                  — push constants (Vulkan) / cbuffer (DX)
    //   @in / @out                     — in/out avec location automatique
    //   @texture(N)                    — sampler unifié
    //   @target GL | VK | DX11 | DX12 | MSL — bloc conditionnel
    // =========================================================================
    class NkShaderBackendNkSL final : public NkShaderBackend {
    public:
        explicit NkShaderBackendNkSL(NkGraphicsApi targetApi);

        NkShaderCompileResult Compile(const NkString& src, NkShaderStage stage,
                                       const NkShaderCompileOptions& opts = {}) override;

        // Transpile NkSL vers code source du backend cible
        NkString Transpile(const NkString& nkslSource, NkGraphicsApi target) const;

        bool     SupportsHotReload() const override { return true; }
        NkString GetBackendName()    const override { return "NkSL (transpiler)"; }

    private:
        NkGraphicsApi       mTarget;
        NkShaderBackend*    mDelegate = nullptr; // backend cible instancié

        NkString TranspileToGL  (const NkString& src) const;
        NkString TranspileToVK  (const NkString& src) const;
        NkString TranspileToDX11(const NkString& src) const;
        NkString TranspileToDX12(const NkString& src) const;
        NkString TranspileToMSL (const NkString& src) const;
    };

    // =========================================================================
    // Fabrique
    // =========================================================================
    NkShaderBackend* NkCreateShaderBackend(NkGraphicsApi api, bool useNkSL = false);

} // namespace renderer
} // namespace nkentseu
