// =============================================================================
// NkShaderCompiler.cpp
// =============================================================================
#include "NkShaderCompiler.h"
#include "NKLogger/NkLog.h"
#include "NKRHI/ShaderConvert/NkShaderConvert.h"
#include "NKRHI/SL/NkSLCompiler.h"
#include <cstdio>
#include <cstring>

// Platform-specific D3D compile headers
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   if defined(NK_RHI_DX11_ENABLED) || defined(NK_RHI_DX12_ENABLED)
#       include <d3dcompiler.h>
#       pragma comment(lib, "d3dcompiler.lib")
#   endif
#endif

namespace nkentseu {

    NkShaderCompiler::NkShaderCompiler(NkIDevice* device) : mDevice(device) {
        mBackend = ApiToBackend(device->GetApi());
    }

    NkShaderCompiler::~NkShaderCompiler() { Shutdown(); }

    bool NkShaderCompiler::Initialize(const NkString& cacheDir) {
        mCacheDir = cacheDir;
        NkShaderCache::Global().SetCacheDir(cacheDir);
        logger.Infof("[NkShaderCompiler] Init backend=%s cache=%s\n",
                     NkShaderBackendName(mBackend), cacheDir.CStr());
        return true;
    }

    void NkShaderCompiler::Shutdown() {}

    // =========================================================================
    // Compile (auto-detect backend)
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::Compile(const NkShaderBackendSources& s) {
        return CompileFor(s, mBackend);
    }

    NkShaderCompileResult NkShaderCompiler::CompileFor(const NkShaderBackendSources& s,
                                                        NkShaderBackend backend) {
        switch (backend) {
            case NkShaderBackend::NK_OPENGL:    return CompileOpenGL(s);
            case NkShaderBackend::NK_VULKAN:    return CompileVulkan(s);
            case NkShaderBackend::NK_DIRECTX11: return CompileDX11(s);
            case NkShaderBackend::NK_DIRECTX12: return CompileDX12(s);
            case NkShaderBackend::NK_METAL:     return CompileMetal(s);
            default: {
                NkShaderCompileResult r;
                r.errors = "Unsupported backend";
                return r;
            }
        }
    }

    // =========================================================================
    // Préprocesseur : injection de defines + résolution #include
    // =========================================================================
    NkString NkShaderCompiler::InjectDefines(const NkString& src,
                                              const NkVector<NkString>& defs,
                                              NkShaderBackend backend) const {
        if (defs.Empty()) return src;
        NkString header;
        for (auto& d : defs) {
            header += "#define ";
            header += d;
            header += "\n";
        }
        // Insérer après la première ligne #version
        NkString result = src;
        const char* versionPos = strstr(result.CStr(), "#version");
        if (versionPos) {
            const char* lineEnd = strchr(versionPos, '\n');
            if (lineEnd) {
                uint32 insertOffset = (uint32)(lineEnd - result.CStr()) + 1;
                NkString before = NkString(result.CStr(), insertOffset);
                NkString after  = result.CStr() + insertOffset;
                result = before + header + after;
            }
        } else {
            result = header + result;
        }
        return result;
    }

    NkString NkShaderCompiler::LoadFile(const NkString& path) const {
        FILE* f = fopen(path.CStr(), "rb");
        if (!f) return "";
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        NkVector<char> buf((usize)sz + 1, '\0');
        fread(buf.Data(), 1, (usize)sz, f);
        fclose(f);
        return NkString(buf.Data());
    }

    // =========================================================================
    // OpenGL GLSL
    // Particularités :
    //   - binding simple (pas de set)
    //   - uniform sampler2D uTex; (global, pas de layout set)
    //   - Pour OpenGL 4.1+ : layout(binding=N) sur les samplers/UBOs
    //   - Vertex : gl_Position (convention Y standard)
    //   - Fragment : out vec4 fragColor;
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::CompileOpenGL(const NkShaderBackendSources& s) {
        NkShaderCompileResult result;
        result.backend = NkShaderBackend::NK_OPENGL;

        // Résoudre les sources
        NkString vertSrc = !s.glslVertSrc.Empty() ? s.glslVertSrc
                         : !s.glslVertFile.Empty() ? LoadFile(s.glslVertFile) : NkString();
        NkString fragSrc = !s.glslFragSrc.Empty() ? s.glslFragSrc
                         : !s.glslFragFile.Empty() ? LoadFile(s.glslFragFile) : NkString();

        // Si pas de sources GL, essayer de convertir depuis Vulkan GLSL via SPIRV-Cross
        if (vertSrc.Empty() && (!s.vkVertSrc.Empty() || !s.vkVertFile.Empty())) {
            logger.Infof("[NkShaderCompiler] OpenGL: converting from Vulkan GLSL\n");
            NkString vkVert = !s.vkVertSrc.Empty() ? s.vkVertSrc : LoadFile(s.vkVertFile);
            NkString vkFrag = !s.vkFragSrc.Empty() ? s.vkFragSrc : LoadFile(s.vkFragFile);
            vertSrc = VkGLSLToOpenGLGLSL(vkVert, NkShaderStage::NK_VERTEX);
            fragSrc = VkGLSLToOpenGLGLSL(vkFrag, NkShaderStage::NK_FRAGMENT);
        }

        if (vertSrc.Empty() || fragSrc.Empty()) {
            result.errors = "[OpenGL] No GLSL vertex or fragment source";
            return result;
        }

        // Inject defines
        vertSrc = InjectDefines(vertSrc, s.defines, NkShaderBackend::NK_OPENGL);
        fragSrc = InjectDefines(fragSrc, s.defines, NkShaderBackend::NK_OPENGL);

        // Construire le NkShaderDesc RHI
        NkShaderDesc desc;
        desc.debugName = s.name.CStr();
        desc.AddGLSL(NkShaderStage::NK_VERTEX,   vertSrc);
        desc.AddGLSL(NkShaderStage::NK_FRAGMENT,  fragSrc);
        if (!s.glslCompSrc.Empty())
            desc.AddGLSL(NkShaderStage::NK_COMPUTE, s.glslCompSrc);

        result.handle  = mDevice->CreateShader(desc);
        result.success = result.handle.IsValid();
        result.generatedVert = vertSrc;
        result.generatedFrag = fragSrc;

        if (!result.success)
            result.errors = "[OpenGL] Shader creation failed (check driver logs)";

        return result;
    }

    // =========================================================================
    // Vulkan GLSL → SPIRV
    // Particularités :
    //   - layout(set=N, binding=M) sur TOUT
    //   - layout(push_constant) uniform PC { ... }
    //   - Y clip : gl_Position.y = -gl_Position.y (si viewport flip désactivé)
    //   - Profondeur [0,1] (utiliser VK_EXT_depth_range_unrestricted sinon)
    //   - #version 460 (sans "core")
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::CompileVulkan(const NkShaderBackendSources& s) {
        NkShaderCompileResult result;
        result.backend = NkShaderBackend::NK_VULKAN;

        // 1. Préférer SPIRV précompilé
        if (!s.vkVertSpirv.Empty() && !s.vkFragSpirv.Empty()) {
            NkShaderDesc desc; desc.debugName = s.name.CStr();
            desc.AddSPIRV(NkShaderStage::NK_VERTEX,   s.vkVertSpirv.Data(), s.vkVertSpirv.Size());
            desc.AddSPIRV(NkShaderStage::NK_FRAGMENT,  s.vkFragSpirv.Data(), s.vkFragSpirv.Size());
            result.handle = mDevice->CreateShader(desc);
            result.success= result.handle.IsValid();
            result.vertBytecode = s.vkVertSpirv;
            result.fragBytecode = s.vkFragSpirv;
            if (!result.success) result.errors = "[Vulkan] SPIRV shader creation failed";
            return result;
        }

        // 2. Compiler GLSL → SPIRV
        NkString vertSrc = !s.vkVertSrc.Empty() ? s.vkVertSrc
                         : !s.vkVertFile.Empty() ? LoadFile(s.vkVertFile) : NkString();
        NkString fragSrc = !s.vkFragSrc.Empty() ? s.vkFragSrc
                         : !s.vkFragFile.Empty() ? LoadFile(s.vkFragFile) : NkString();

        if (vertSrc.Empty() || fragSrc.Empty()) {
            result.errors = "[Vulkan] No Vulkan GLSL source";
            return result;
        }

        vertSrc = InjectDefines(vertSrc, s.defines, NkShaderBackend::NK_VULKAN);
        fragSrc = InjectDefines(fragSrc, s.defines, NkShaderBackend::NK_VULKAN);

        // Compiler vers SPIRV
        NkString errV, errF;
        bool okV = GLSLToSPIRV(vertSrc, NkShaderStage::NK_VERTEX,   s.defines, result.vertBytecode, errV);
        bool okF = GLSLToSPIRV(fragSrc, NkShaderStage::NK_FRAGMENT, s.defines, result.fragBytecode, errF);

        if (!okV || !okF) {
            result.errors = errV + "\n" + errF;
            return result;
        }

        // Cacher
        NkString key = HashSources(s, NkShaderBackend::NK_VULKAN);
        SaveToCache(key + "_vert", NkShaderBackend::NK_VULKAN, result.vertBytecode);
        SaveToCache(key + "_frag", NkShaderBackend::NK_VULKAN, result.fragBytecode);

        NkShaderDesc desc; desc.debugName = s.name.CStr();
        desc.AddSPIRV(NkShaderStage::NK_VERTEX,  result.vertBytecode.Data(), result.vertBytecode.Size());
        desc.AddSPIRV(NkShaderStage::NK_FRAGMENT,result.fragBytecode.Data(), result.fragBytecode.Size());
        if (!s.vkCompSpirv.Empty())
            desc.AddSPIRV(NkShaderStage::NK_COMPUTE, s.vkCompSpirv.Data(), s.vkCompSpirv.Size());

        result.handle  = mDevice->CreateShader(desc);
        result.success = result.handle.IsValid();
        result.generatedVert = vertSrc;
        result.generatedFrag = fragSrc;
        if (!result.success) result.errors = "[Vulkan] Shader creation from SPIRV failed";
        return result;
    }

    // =========================================================================
    // HLSL DirectX 11 (SM5.0)
    // Particularités :
    //   - D3DCompile / FXC
    //   - cbuffer MyBlock : register(bN) { float4x4 myMatrix; }
    //   - Textures/Samplers séparés : Texture2D t : register(tN)
    //   - SamplerState s : register(sN)
    //   - struct VSIn { float3 pos:POSITION; float2 uv:TEXCOORD0; }
    //   - float4 VSMain(VSIn i) : SV_Position
    //   - float4 PSMain(V2F i) : SV_Target
    //   - Matrices column_major (même convention que GLSL)
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::CompileDX11(const NkShaderBackendSources& s) {
        NkShaderCompileResult result;
        result.backend = NkShaderBackend::NK_DIRECTX11;

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(NK_RHI_DX11_ENABLED)
        // Préférer bytecode précompilé
        if (!s.dx11VertBytecode.Empty() && !s.dx11FragBytecode.Empty()) {
            NkShaderDesc desc; desc.debugName = s.name.CStr();
            // DX11 accepte le bytecode DXBC directement
            NkShaderStageDesc vsd, psd;
            vsd.stage     = NkShaderStage::NK_VERTEX;
            vsd.dxilData  = s.dx11VertBytecode.Data();
            vsd.dxilSize  = s.dx11VertBytecode.Size();
            psd.stage     = NkShaderStage::NK_FRAGMENT;
            psd.dxilData  = s.dx11FragBytecode.Data();
            psd.dxilSize  = s.dx11FragBytecode.Size();
            desc.stages.PushBack(vsd);
            desc.stages.PushBack(psd);
            result.handle = mDevice->CreateShader(desc);
            result.success= result.handle.IsValid();
            return result;
        }

        NkString vertSrc = !s.dx11VertSrc.Empty() ? s.dx11VertSrc
                         : !s.dx11File.Empty()    ? LoadFile(s.dx11File) : NkString();
        NkString fragSrc = !s.dx11FragSrc.Empty() ? s.dx11FragSrc
                         : !s.dx11File.Empty()    ? LoadFile(s.dx11File) : vertSrc;

        if (vertSrc.Empty()) {
            result.errors = "[DX11] No HLSL source";
            return result;
        }

        vertSrc = InjectDefines(vertSrc, s.defines, NkShaderBackend::NK_DIRECTX11);
        fragSrc = InjectDefines(fragSrc, s.defines, NkShaderBackend::NK_DIRECTX11);

        auto CompileHLSL = [&](const NkString& src, const NkString& entry,
                                const char* target, NkVector<uint8>& out) -> bool {
            ID3DBlob* blob = nullptr;
            ID3DBlob* err  = nullptr;
            HRESULT hr = D3DCompile(src.CStr(), src.Length(), s.name.CStr(),
                                    nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entry.CStr(), target,
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
            if (FAILED(hr)) {
                if (err) {
                    result.errors += (const char*)err->GetBufferPointer();
                    err->Release();
                }
                if (blob) blob->Release();
                return false;
            }
            out.Resize((uint32)blob->GetBufferSize());
            memcpy(out.Data(), blob->GetBufferPointer(), blob->GetBufferSize());
            blob->Release();
            return true;
        };

        bool okV = CompileHLSL(vertSrc, s.dx11VertEntry,  "vs_5_0", result.vertBytecode);
        bool okF = CompileHLSL(fragSrc, s.dx11PixelEntry, "ps_5_0", result.fragBytecode);

        if (!okV || !okF) return result;

        NkShaderDesc desc; desc.debugName = s.name.CStr();
        desc.AddHLSL(NkShaderStage::NK_VERTEX,  vertSrc, s.dx11VertEntry.CStr());
        desc.AddHLSL(NkShaderStage::NK_FRAGMENT, fragSrc, s.dx11PixelEntry.CStr());
        // Store bytecodes
        {
            NkShaderStageDesc& vs = desc.stages[0];
            vs.dxilData = result.vertBytecode.Data();
            vs.dxilSize = result.vertBytecode.Size();
            NkShaderStageDesc& ps = desc.stages[1];
            ps.dxilData = result.fragBytecode.Data();
            ps.dxilSize = result.fragBytecode.Size();
        }
        result.handle  = mDevice->CreateShader(desc);
        result.success = result.handle.IsValid();
        result.generatedVert = vertSrc;
        result.generatedFrag = fragSrc;
        if (!result.success) result.errors += "\n[DX11] Shader creation failed";
#else
        result.errors = "[DX11] Not available on this platform (Windows + NK_RHI_DX11_ENABLED required)";
#endif
        return result;
    }

    // =========================================================================
    // HLSL DirectX 12 (SM6.0+ via DXC)
    // Particularités vs DX11 :
    //   - DXC (DirectX Shader Compiler) obligatoire — génère DXIL (pas DXBC)
    //   - Root Signature dans le shader :
    //     #define RS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), \
    //       RootConstants(num32BitConstants=16, b0), \
    //       DescriptorTable(CBV(b1), SRV(t0,numDescriptors=8), Sampler(s0))"
    //     [RootSignature(RS)] VSOut VSMain(...)
    //   - Root constants : float4x4 model : register(b0) → push constants
    //   - Descriptor heap indexing (SM6.6) :
    //     Texture2D myTex = ResourceDescriptorHeap[texIndex];
    //   - Wave operations : WaveActiveSum, WaveActiveMin, etc.
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::CompileDX12(const NkShaderBackendSources& s) {
        NkShaderCompileResult result;
        result.backend = NkShaderBackend::NK_DIRECTX12;

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(NK_RHI_DX12_ENABLED)
        if (!s.dx12VertBytecode.Empty() && !s.dx12FragBytecode.Empty()) {
            NkShaderDesc desc; desc.debugName = s.name.CStr();
            NkShaderStageDesc vsd, psd;
            vsd.stage    = NkShaderStage::NK_VERTEX;
            vsd.dxilData = s.dx12VertBytecode.Data();
            vsd.dxilSize = s.dx12VertBytecode.Size();
            psd.stage    = NkShaderStage::NK_FRAGMENT;
            psd.dxilData = s.dx12FragBytecode.Data();
            psd.dxilSize = s.dx12FragBytecode.Size();
            desc.stages.PushBack(vsd); desc.stages.PushBack(psd);
            result.handle  = mDevice->CreateShader(desc);
            result.success = result.handle.IsValid();
            return result;
        }

        NkString vertSrc = !s.dx12VertSrc.Empty() ? s.dx12VertSrc
                         : !s.dx12File.Empty()    ? LoadFile(s.dx12File) : NkString();
        NkString fragSrc = !s.dx12FragSrc.Empty() ? s.dx12FragSrc
                         : !s.dx12File.Empty()    ? LoadFile(s.dx12File) : vertSrc;

        if (vertSrc.Empty()) {
            result.errors = "[DX12] No HLSL SM6 source";
            return result;
        }

        vertSrc = InjectDefines(vertSrc, s.defines, NkShaderBackend::NK_DIRECTX12);
        fragSrc = InjectDefines(fragSrc, s.defines, NkShaderBackend::NK_DIRECTX12);

        // DXC compilation (via IDxcCompiler3)
        // Note: implémentation simplifiée — en production, utiliser DXC SDK
        // L'API NkDirectX12Device expose un helper CompileHLSLWithDXC()
        NkShaderDesc desc; desc.debugName = s.name.CStr();
        desc.AddHLSL(NkShaderStage::NK_VERTEX,  vertSrc, s.dx12VertEntry.CStr());
        desc.AddHLSL(NkShaderStage::NK_FRAGMENT, fragSrc, s.dx12PixelEntry.CStr());
        result.handle  = mDevice->CreateShader(desc);
        result.success = result.handle.IsValid();
        result.generatedVert = vertSrc;
        result.generatedFrag = fragSrc;
        if (!result.success) result.errors = "[DX12] Shader creation failed";
#else
        result.errors = "[DX12] Not available (Windows + NK_RHI_DX12_ENABLED required)";
#endif
        return result;
    }

    // =========================================================================
    // Metal MSL
    // Particularités :
    //   - Fonction vertex : vertex VertOut vmain(uint vid [[vertex_id]],
    //       const device VertexData* verts [[buffer(0)]],
    //       constant FrameUBO& frame [[buffer(1)]])
    //   - Fonction fragment : fragment float4 fmain(VertOut in [[stage_in]],
    //       texture2d<float> tex [[texture(0)]],
    //       sampler s [[sampler(0)]],
    //       constant MatUBO& mat [[buffer(0)]])
    //   - [[position]] est le return value tag (pas un attribut de sortie)
    //   - Les samplers peuvent être créés inline :
    //     constexpr sampler s(filter::linear, address::repeat);
    //   - Matrices : float4x4 (column-major comme GLSL/HLSL)
    //   - Pas de push constants — utiliser [[buffer(0)]] avec un petit struct
    //   - Metal 3 : mesh shaders, argument buffers Tier 2 (bindless)
    // =========================================================================
    NkShaderCompileResult NkShaderCompiler::CompileMetal(const NkShaderBackendSources& s) {
        NkShaderCompileResult result;
        result.backend = NkShaderBackend::NK_METAL;

#if defined(NK_RHI_METAL_ENABLED)
        // Préférer .metallib précompilé
        if (!s.mslLibData.Empty()) {
            NkShaderDesc desc; desc.debugName = s.name.CStr();
            NkShaderStageDesc vsd, psd;
            vsd.stage       = NkShaderStage::NK_VERTEX;
            vsd.metalIRData = s.mslLibData.Data();
            vsd.metalIRSize = s.mslLibData.Size();
            vsd.entryPoint  = s.mslVertEntry.CStr();
            psd.stage       = NkShaderStage::NK_FRAGMENT;
            psd.metalIRData = s.mslLibData.Data();
            psd.metalIRSize = s.mslLibData.Size();
            psd.entryPoint  = s.mslFragEntry.CStr();
            desc.stages.PushBack(vsd); desc.stages.PushBack(psd);
            result.handle  = mDevice->CreateShader(desc);
            result.success = result.handle.IsValid();
            return result;
        }

        NkString vertSrc = !s.mslVertSrc.Empty() ? s.mslVertSrc
                         : !s.mslFile.Empty()    ? LoadFile(s.mslFile) : NkString();
        NkString fragSrc = !s.mslFragSrc.Empty() ? s.mslFragSrc
                         : !s.mslFile.Empty()    ? LoadFile(s.mslFile) : vertSrc;

        if (vertSrc.Empty() && !s.glslVertSrc.Empty()) {
            // Convertir GLSL → MSL via SPIRV-Cross
            logger.Infof("[NkShaderCompiler] Metal: converting from GLSL via SPIRV-Cross\n");
            vertSrc = GLSLToMSL(s.glslVertSrc, NkShaderStage::NK_VERTEX);
            fragSrc = GLSLToMSL(s.glslFragSrc, NkShaderStage::NK_FRAGMENT);
        }

        if (vertSrc.Empty()) {
            result.errors = "[Metal] No MSL source";
            return result;
        }

        vertSrc = InjectDefines(vertSrc, s.defines, NkShaderBackend::NK_METAL);
        fragSrc = InjectDefines(fragSrc, s.defines, NkShaderBackend::NK_METAL);

        NkShaderDesc desc; desc.debugName = s.name.CStr();
        desc.AddMSL(NkShaderStage::NK_VERTEX,   vertSrc, s.mslVertEntry.CStr());
        desc.AddMSL(NkShaderStage::NK_FRAGMENT,  fragSrc, s.mslFragEntry.CStr());
        result.handle  = mDevice->CreateShader(desc);
        result.success = result.handle.IsValid();
        result.generatedVert = vertSrc;
        result.generatedFrag = fragSrc;
        if (!result.success) result.errors = "[Metal] Shader creation failed";
#else
        result.errors = "[Metal] Not available (NK_RHI_METAL_ENABLED required)";
#endif
        return result;
    }

    // =========================================================================
    // Conversion cross-backend
    // =========================================================================
    NkString NkShaderCompiler::VkGLSLToOpenGLGLSL(const NkString& vkSrc,
                                                    NkShaderStage stage) {
        // 1. Compiler Vulkan GLSL → SPIRV
        NkVector<uint8> spirv; NkString err;
        NkVector<NkString> defs;
        if (!GLSLToSPIRV(vkSrc, stage, defs, spirv, err)) {
            logger.Warnf("[NkShaderCompiler] VkToGL: SPIRV compile failed: %s\n", err.CStr());
            return vkSrc; // fallback
        }
        // 2. SPIRV → OpenGL GLSL via SPIRV-Cross
        NkString glsl;
        if (!SPIRVToGLSL(spirv, stage, true, glsl)) {
            logger.Warnf("[NkShaderCompiler] VkToGL: SPIRV-Cross failed\n");
            return vkSrc;
        }
        return glsl;
    }

    NkString NkShaderCompiler::GLSLToHLSL(const NkString& glslSrc, NkShaderStage stage,
                                            uint32 sm) {
        NkVector<uint8> spirv; NkString err; NkVector<NkString> defs;
        if (!GLSLToSPIRV(glslSrc, stage, defs, spirv, err)) return "";
        auto res = NkShaderConverter::SpirvToHlsl(
            reinterpret_cast<const uint32*>(spirv.Data()),
            (uint32)(spirv.Size()/4), (NkSLStage)stage, sm);
        return res.success ? res.source : "";
    }

    NkString NkShaderCompiler::GLSLToMSL(const NkString& glslSrc, NkShaderStage stage) {
        NkVector<uint8> spirv; NkString err; NkVector<NkString> defs;
        if (!GLSLToSPIRV(glslSrc, stage, defs, spirv, err)) return "";
        auto res = NkShaderConverter::SpirvToMsl(
            reinterpret_cast<const uint32*>(spirv.Data()),
            (uint32)(spirv.Size()/4), (NkSLStage)stage);
        return res.success ? res.source : "";
    }

    bool NkShaderCompiler::GLSLToSPIRV(const NkString& src, NkShaderStage stage,
                                         const NkVector<NkString>& defines,
                                         NkVector<uint8>& outSpirv,
                                         NkString& outErrors) {
        NkString fullSrc = InjectDefines(src, defines, NkShaderBackend::NK_VULKAN);
        NkSLStage slStage = (NkSLStage)stage;
        auto res = NkShaderConverter::GlslToSpirv(fullSrc, slStage, "shader");
        if (!res.success) { outErrors = res.errors; return false; }
        outSpirv = res.binary;
        return true;
    }

    bool NkShaderCompiler::SPIRVToGLSL(const NkVector<uint8>& spirv,
                                         NkShaderStage stage, bool forOpenGL,
                                         NkString& out) {
        auto res = NkShaderConverter::SpirvToGlsl(
            reinterpret_cast<const uint32*>(spirv.Data()),
            (uint32)(spirv.Size()/4), (NkSLStage)stage);
        if (!res.success) return false;
        out = res.source;
        return true;
    }

    // =========================================================================
    // Cache
    // =========================================================================
    NkString NkShaderCompiler::HashSources(const NkShaderBackendSources& s,
                                            NkShaderBackend b) const {
        NkString combined = s.name;
        combined += NkShaderBackendName(b);
        combined += s.glslVertSrc + s.glslFragSrc;
        combined += s.vkVertSrc   + s.vkFragSrc;
        combined += s.dx11VertSrc + s.dx11FragSrc;
        combined += s.dx12VertSrc + s.dx12FragSrc;
        combined += s.mslVertSrc  + s.mslFragSrc;
        uint64 key = NkShaderCache::ComputeKey(combined, NkSLStage::NK_VERTEX, "spirv");
        char buf[32]; snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)key);
        return NkString(buf);
    }

    bool NkShaderCompiler::HasCached(const NkString& key, NkShaderBackend b) const {
        (void)key; (void)b; return false;
    }

    bool NkShaderCompiler::LoadFromCache(const NkString& key, NkShaderBackend b,
                                          NkVector<uint8>& bytecode) const {
        (void)key; (void)b; (void)bytecode; return false;
    }

    void NkShaderCompiler::SaveToCache(const NkString& key, NkShaderBackend b,
                                        const NkVector<uint8>& bytecode) {
        (void)key; (void)b; (void)bytecode;
    }

    void NkShaderCompiler::ClearCache() {}

    bool NkShaderCompiler::CheckAndReload(NkShaderBackendSources& sources) {
        (void)sources; return false;
    }

} // namespace nkentseu
