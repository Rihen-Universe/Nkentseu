#pragma once
// =============================================================================
// NkShaderBackend.h
// Gestion multi-backend des shaders : GL, Vulkan, DX11, DX12, Metal.
//
// DIFFÉRENCES IMPORTANTES PAR API :
//
// OpenGL GLSL (#version 460 core / #version 330 core)
//   - Uniforms via glUniform* ou UBO avec layout(binding=N)
//   - Pas de descriptor sets
//   - gl_Position en vertex, fragColor comme out
//   - Textures : uniform sampler2D tTex;
//   - Push constants : pas supportés → uniforms classiques
//
// Vulkan GLSL (#version 460 / #version 450)
//   - layout(set=N, binding=M) pour TOUS les uniforms
//   - Descriptor sets (set 0..3)
//   - gl_Position identique mais convention Y inversée vs OpenGL
//   - layout(push_constant) uniform PC { ... };
//   - Compile vers SPIR-V via glslang
//   - Coordonnées Z : [0,1] (Vulkan) vs [-1,1] (OpenGL)
//
// HLSL DirectX11 (Shader Model 5.0)
//   - cbuffer Constants : register(b0) { ... }
//   - SV_Position au lieu de gl_Position
//   - Textures : Texture2D tTex : register(t0); SamplerState s : register(s0)
//   - PSMain / VSMain comme entry points
//   - Pas de push constants → cbuffer avec slot dédié
//   - Matrix : row_major par défaut (transposées vs GLSL)
//
// HLSL DirectX12 (Shader Model 6.0+)
//   - Root Signature explicite
//   - Root constants : b0 (push constants analogues)
//   - Descriptor heaps : bindless possible (SM6.6 ResourceDescriptorHeap)
//   - SM6.0 wave intrinsics
//   - Différences cbuffer binding : DX12 gère root parameters
//
// Metal Shading Language (MSL 2.x / 3.x)
//   - vertex/fragment/kernel comme qualificateurs de fonction
//   - [[position]] au lieu de gl_Position
//   - Buffers : constant MyUBO& ubo [[buffer(0)]]
//   - Textures : texture2d<float> tex [[texture(0)]]
//   - Samplers : sampler s [[sampler(0)]]
//   - Pas de push constants → argument buffer ou buffer slot 0
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // =========================================================================
    // Backend shader cible
    // =========================================================================
    enum class NkShaderBackend : uint8 {
        NK_OPENGL,       // GLSL 3.30..4.60 → compilé au runtime via driver GL
        NK_VULKAN,       // GLSL → SPIR-V via glslang (offline ou runtime)
        NK_DIRECTX11,    // HLSL SM5.0 → compilé via D3DCompile / FXC
        NK_DIRECTX12,    // HLSL SM6.0+ → compilé via DXC
        NK_METAL,        // MSL 2.x → compilé via xcrun metallib / runtime MTLLibrary
        NK_SOFTWARE,     // C++ CPU (NkSL → lambda)
        NK_COUNT
    };

    inline const char* NkShaderBackendName(NkShaderBackend b) {
        switch(b){
            case NkShaderBackend::NK_OPENGL:   return "OpenGL GLSL";
            case NkShaderBackend::NK_VULKAN:   return "Vulkan GLSL→SPIRV";
            case NkShaderBackend::NK_DIRECTX11:return "HLSL SM5 (DX11)";
            case NkShaderBackend::NK_DIRECTX12:return "HLSL SM6 (DX12)";
            case NkShaderBackend::NK_METAL:    return "MSL (Metal)";
            case NkShaderBackend::NK_SOFTWARE: return "C++ Software";
            default:                           return "Unknown";
        }
    }

    // Convertit NkGraphicsApi → NkShaderBackend
    inline NkShaderBackend ApiToBackend(NkGraphicsApi api) {
        switch(api) {
            case NkGraphicsApi::NK_GFX_API_OPENGL:
            case NkGraphicsApi::NK_GFX_API_OPENGLES:
            case NkGraphicsApi::NK_GFX_API_WEBGL:    return NkShaderBackend::NK_OPENGL;
            case NkGraphicsApi::NK_GFX_API_VULKAN:   return NkShaderBackend::NK_VULKAN;
            case NkGraphicsApi::NK_GFX_API_D3D11:return NkShaderBackend::NK_DIRECTX11;
            case NkGraphicsApi::NK_GFX_API_D3D12:return NkShaderBackend::NK_DIRECTX12;
            case NkGraphicsApi::NK_GFX_API_METAL:    return NkShaderBackend::NK_METAL;
            case NkGraphicsApi::NK_GFX_API_SOFTWARE: return NkShaderBackend::NK_SOFTWARE;
            default:                             return NkShaderBackend::NK_OPENGL;
        }
    }

    // =========================================================================
    // NkShaderStageSource — source pour un stage (vert/frag/compute) sur UN backend
    // =========================================================================
    struct NkShaderStageSource {
        NkShaderStage stage       = NkShaderStage::NK_VERTEX;
        NkShaderBackend backend   = NkShaderBackend::NK_OPENGL;

        // Source texte
        NkString source;           // code source (GLSL / HLSL / MSL)
        NkString filePath;         // chemin fichier (pour hot-reload)
        NkString entryPoint;       // "main" (GLSL/MSL) | "VSMain","PSMain" (HLSL)

        // Bytecode précompilé
        NkVector<uint8> bytecode;  // SPIR-V (Vulkan) | DXBC/DXIL (DX) | metallib

        bool HasSource()   const { return !source.Empty() || !filePath.Empty(); }
        bool HasBytecode() const { return !bytecode.Empty(); }
    };

    // =========================================================================
    // NkShaderBackendSources — toutes les sources pour TOUS les backends
    // d'un matériau/effet. Structure centrale partagée par NkShaderSystem.
    // =========================================================================
    struct NkShaderBackendSources {
        NkString name;

        // ── OpenGL GLSL ────────────────────────────────────────────────────────
        // Note: OpenGL utilise binding simple, pas de set
        // Différences vs Vulkan GLSL :
        //   - Pas de layout(set=N, ...)
        //   - Samplers : uniform sampler2D uTex; (pas de set)
        //   - Pas de push_constant
        //   - Version : #version 460 core (ou 330 pour compatibilité)
        NkString glslVertSrc;
        NkString glslFragSrc;
        NkString glslCompSrc;
        NkString glslGeomSrc;
        NkString glslTescSrc;
        NkString glslTeseSrc;
        NkString glslVertFile;
        NkString glslFragFile;

        // ── Vulkan GLSL ────────────────────────────────────────────────────────
        // Différences vs OpenGL GLSL :
        //   - layout(set=0, binding=0) obligatoire sur TOUT
        //   - layout(push_constant) uniform PC { mat4 model; ... }
        //   - #version 460 (pas "core")
        //   - Attention au Y flip : Vulkan Y pointe vers le bas en NDC
        //   - gl_Position.y = -gl_Position.y; (ou utiliser viewport flip)
        //   - Profondeur : [0,1] vs [-1,1] OpenGL (VK_EXT_depth_range_unrestricted)
        NkString vkVertSrc;
        NkString vkFragSrc;
        NkString vkCompSrc;
        NkString vkVertFile;
        NkString vkFragFile;
        // SPIR-V précompilé (évite la dépendance glslang au runtime)
        NkVector<uint8> vkVertSpirv;
        NkVector<uint8> vkFragSpirv;
        NkVector<uint8> vkCompSpirv;

        // ── HLSL DirectX11 (SM5.0) ────────────────────────────────────────────
        // Différences clés :
        //   - cbuffer CB : register(b1) { float4x4 viewProj; float4x4 model; }
        //     → le binding est par register (b=cbuffer, t=texture, s=sampler)
        //   - Textures + Samplers séparés obligatoirement :
        //     Texture2D tAlbedo : register(t0);
        //     SamplerState sSampler : register(s0);
        //   - struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; }
        //   - float4x4 est row_major par défaut → transposer vs GLSL/MSL
        //   - Pas de push constants → cbuffer slot b0 réservé aux données par-objet
        //   - SV_Position (pas gl_Position)
        //   - SV_Target0 (pas fragColor)
        NkString dx11VertSrc;     // entry VSMain
        NkString dx11FragSrc;     // entry PSMain
        NkString dx11CompSrc;     // entry CSMain
        NkString dx11File;        // fichier .hlsl commun (vertex+pixel)
        NkString dx11VertEntry  = "VSMain";
        NkString dx11PixelEntry = "PSMain";
        NkVector<uint8> dx11VertBytecode;  // DXBC précompilé (FXC)
        NkVector<uint8> dx11FragBytecode;

        // ── HLSL DirectX12 (SM6.0+) ───────────────────────────────────────────
        // Différences vs DX11 :
        //   - Root Signature (définie dans le shader ou externally)
        //   - Root constants : register(b0, space0) → équivalent push constants
        //     "RootConstants(num32BitConstants=16, b0)"
        //   - Descriptor tables (groupes de SRV/CBV/UAV/Sampler)
        //   - SM6.0 : Wave intrinsics (WaveActiveSum, etc.)
        //   - SM6.6 : ResourceDescriptorHeap[index] → bindless
        //     Texture2D myTex = ResourceDescriptorHeap[texIndex];
        //   - Inline samplers dans root signature
        //   - DXC obligatoire (pas FXC)
        NkString dx12VertSrc;
        NkString dx12FragSrc;
        NkString dx12CompSrc;
        NkString dx12File;
        NkString dx12RootSig;     // root signature HLSL inline ou fichier
        NkString dx12VertEntry  = "VSMain";
        NkString dx12PixelEntry = "PSMain";
        NkVector<uint8> dx12VertBytecode;  // DXIL (DXC output)
        NkVector<uint8> dx12FragBytecode;

        // ── Metal MSL ─────────────────────────────────────────────────────────
        // Différences majeures :
        //   - vertex shader : vertex float4 vmain(uint vid [[vertex_id]],
        //       constant VertexData* verts [[buffer(0)]],
        //       constant FrameUBO& ubo     [[buffer(1)]]) [[position]] → return val
        //   - fragment shader : fragment float4 fmain(V2F in [[stage_in]],
        //       texture2d<float> tex [[texture(0)]],
        //       sampler samp [[sampler(0)]],
        //       constant MatUBO& mat [[buffer(0)]])
        //   - Pas de UBO séparé au sens Vulkan, buffers indexés
        //   - [[buffer(0)]] = vertex buffer OU constant buffer (attention conflit)
        //     Convention : [[buffer(0..7)]] vertex, [[buffer(8..15)]] constants
        //   - Samplers définis dans le code OU passés comme paramètre [[sampler]]
        //   - [[position]] sur le return type du vertex shader
        //   - Metal 3 : mesh shaders [[mesh]] [[object]]
        NkString mslVertSrc;
        NkString mslFragSrc;
        NkString mslCompSrc;     // kernel
        NkString mslFile;        // .metal commun
        NkString mslVertEntry  = "vertMain";
        NkString mslFragEntry  = "fragMain";
        NkString mslCompEntry  = "kernelMain";
        NkVector<uint8> mslLibData;  // metallib précompilé

        // ── Software C++ ──────────────────────────────────────────────────────
        NkString cppSrc;

        // ── Defines communs (injectés dans tous les backends) ──────────────────
        NkVector<NkString> defines;

        // ── Helpers ───────────────────────────────────────────────────────────
        bool HasBackend(NkShaderBackend b) const {
            switch(b) {
                case NkShaderBackend::NK_OPENGL:
                    return !glslVertSrc.Empty() || !glslVertFile.Empty();
                case NkShaderBackend::NK_VULKAN:
                    return !vkVertSrc.Empty() || !vkVertFile.Empty() || !vkVertSpirv.Empty();
                case NkShaderBackend::NK_DIRECTX11:
                    return !dx11VertSrc.Empty() || !dx11File.Empty() || !dx11VertBytecode.Empty();
                case NkShaderBackend::NK_DIRECTX12:
                    return !dx12VertSrc.Empty() || !dx12File.Empty() || !dx12VertBytecode.Empty();
                case NkShaderBackend::NK_METAL:
                    return !mslVertSrc.Empty() || !mslFile.Empty() || !mslLibData.Empty();
                default: return false;
            }
        }
    };

    // =========================================================================
    // Résultat de compilation multi-backend
    // =========================================================================
    struct NkShaderCompileResult {
        bool           success   = false;
        NkShaderHandle handle;
        NkString       errors;
        NkString       warnings;
        NkShaderBackend backend  = NkShaderBackend::NK_OPENGL;

        // Sources générées (pour debug/export)
        NkString generatedVert;
        NkString generatedFrag;
        NkString generatedComp;
        NkVector<uint8> vertBytecode;   // SPIRV / DXBC / DXIL / metallib
        NkVector<uint8> fragBytecode;

        bool IsValid() const { return success && handle.IsValid(); }
    };

} // namespace nkentseu
