#pragma once
// =============================================================================
// NkShaderCompiler.h
// Compilateur multi-backend : compile les sources vers le bon format selon l'API.
// =============================================================================
#include "NkShaderBackend.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {

    // =========================================================================
    // NkShaderCompiler — compile les sources pour n'importe quel backend
    // =========================================================================
    class NkShaderCompiler {
    public:
        explicit NkShaderCompiler(NkIDevice* device);
        ~NkShaderCompiler();

        bool Initialize(const NkString& cacheDir = "Build/ShaderCache");
        void Shutdown();

        // ── Compilation principale ─────────────────────────────────────────────
        // Détecte le backend depuis l'API du device et compile les sources appropriées.
        NkShaderCompileResult Compile(const NkShaderBackendSources& sources);

        // Compilation forcée sur un backend spécifique
        NkShaderCompileResult CompileFor(const NkShaderBackendSources& sources,
                                          NkShaderBackend backend);

        // ── Compilation par backend ────────────────────────────────────────────
        NkShaderCompileResult CompileOpenGL  (const NkShaderBackendSources& s);
        NkShaderCompileResult CompileVulkan  (const NkShaderBackendSources& s);
        NkShaderCompileResult CompileDX11    (const NkShaderBackendSources& s);
        NkShaderCompileResult CompileDX12    (const NkShaderBackendSources& s);
        NkShaderCompileResult CompileMetal   (const NkShaderBackendSources& s);

        // ── Conversion cross-backend (via NkSL/SPIRV-Cross) ───────────────────
        // Convertit GLSL Vulkan → OpenGL GLSL (retire set=, adapte les bindings)
        NkString VkGLSLToOpenGLGLSL(const NkString& vkSrc, NkShaderStage stage);
        // Convertit GLSL → HLSL (via SPIRV-Cross)
        NkString GLSLToHLSL  (const NkString& glslSrc, NkShaderStage stage, uint32 sm = 50);
        // Convertit GLSL → MSL (via SPIRV-Cross)
        NkString GLSLToMSL   (const NkString& glslSrc, NkShaderStage stage);
        // Convertit HLSL → GLSL (rare, via DXC + SPIRV-Cross)
        NkString HLSLToGLSL  (const NkString& hlslSrc, NkShaderStage stage);

        // ── SPIRV ──────────────────────────────────────────────────────────────
        bool GLSLToSPIRV(const NkString& src, NkShaderStage stage,
                          const NkVector<NkString>& defines,
                          NkVector<uint8>& outSpirv, NkString& outErrors);
        bool SPIRVToGLSL(const NkVector<uint8>& spirv, NkShaderStage stage,
                          bool forOpenGL, NkString& out);

        // ── Cache ──────────────────────────────────────────────────────────────
        bool HasCached(const NkString& key, NkShaderBackend backend) const;
        bool LoadFromCache(const NkString& key, NkShaderBackend backend,
                            NkVector<uint8>& bytecode) const;
        void SaveToCache(const NkString& key, NkShaderBackend backend,
                          const NkVector<uint8>& bytecode);
        void ClearCache();

        // ── Hot-reload ─────────────────────────────────────────────────────────
        void EnableHotReload(bool v) { mHotReload = v; }
        bool CheckAndReload(NkShaderBackendSources& sources);

        NkShaderBackend GetCurrentBackend() const { return mBackend; }
        NkIDevice*      GetDevice()         const { return mDevice; }

    private:
        NkIDevice*      mDevice;
        NkShaderBackend mBackend = NkShaderBackend::NK_OPENGL;
        NkString        mCacheDir;
        bool            mHotReload = false;

        // Builds du shader RHI desc depuis bytecodes
        NkShaderHandle BuildShaderHandle(
            const NkVector<uint8>& vertBC, const NkVector<uint8>& fragBC,
            const NkString& vertSrc, const NkString& fragSrc,
            const NkString& vertEntry, const NkString& fragEntry,
            NkShaderStage vertStage, NkShaderStage fragStage);

        NkString HashSources(const NkShaderBackendSources& s, NkShaderBackend b) const;
        NkString InjectDefines(const NkString& src, const NkVector<NkString>& defs,
                                NkShaderBackend backend) const;
        NkString LoadFile(const NkString& path) const;

        // Préprocesseur #include inline
        NkString Preprocess(const NkString& src, const NkString& baseDir,
                             NkShaderBackend backend) const;
        NkUnorderedMap<NkString, NkString> mVirtualIncludes;
    };

} // namespace nkentseu
