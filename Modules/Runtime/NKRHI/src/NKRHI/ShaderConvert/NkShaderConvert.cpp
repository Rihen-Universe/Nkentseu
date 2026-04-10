// =============================================================================
// NkShaderConvert.cpp
// Implémentation de NkShaderFileResolver, NkShaderConverter, NkShaderCache.
// =============================================================================
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// ── glslang (GLSL → SPIR-V) ──────────────────────────────────────────────────
#ifdef NK_RHI_GLSLANG_ENABLED
#   include <glslang/Public/ShaderLang.h>
#   include <glslang/Public/ResourceLimits.h>
#   include <SPIRV/GlslangToSpv.h>
#endif

// ── SPIRV-Cross (SPIR-V → GLSL / HLSL / MSL) ─────────────────────────────────
#ifdef NK_RHI_SPIRVCROSS_ENABLED
#   include <spirv_cross/spirv_glsl.hpp>
#   include <spirv_cross/spirv_hlsl.hpp>
#   include <spirv_cross/spirv_msl.hpp>
#endif

// ── Platform: file removal ────────────────────────────────────────────────────
#ifdef _WIN32
#   include <windows.h>
#   include <direct.h>
#   define NK_MKDIR(p) _mkdir(p)
#else
#   include <sys/stat.h>
#   include <dirent.h>
#   define NK_MKDIR(p) mkdir((p), 0755)
#endif

#include "NKLogger/NkLog.h"

namespace nkentseu {

    // =============================================================================
    // Helpers internes
    // =============================================================================

    static std::string ToStd(const NkString& s) { return std::string(s.CStr()); }
    static NkString   FromStd(const std::string& s) { return NkString(s.c_str()); }

    // Récupère la dernière extension (après le dernier '.')
    static std::string LastExt(const std::string& path) {
        auto dot = path.rfind('.');
        return (dot == std::string::npos) ? "" : path.substr(dot + 1);
    }

    // Retire la dernière extension
    static std::string DropLastExt(const std::string& path) {
        auto dot = path.rfind('.');
        return (dot == std::string::npos) ? path : path.substr(0, dot);
    }

    // =============================================================================
    // NkShaderFileResolver
    // =============================================================================

    NkString NkShaderFileResolver::BasePath(const NkString& path) {
        return FromStd(DropLastExt(ToStd(path)));
    }

    NkString NkShaderFileResolver::FormatExt(const NkString& path) {
        return FromStd(LastExt(ToStd(path)));
    }

    NkString NkShaderFileResolver::StageExt(const NkString& path) {
        // Retire la dernière extension puis extrait la nouvelle dernière extension
        std::string base = DropLastExt(ToStd(path));
        return FromStd(LastExt(base));
    }

    NkSLStage NkShaderFileResolver::StageFrom(const NkString& path) {
        std::string ext = ToStd(StageExt(path));
        if (ext == "vert" || ext == "vs")  return NkSLStage::NK_VERTEX;
        if (ext == "frag" || ext == "fs" || ext == "ps") return NkSLStage::NK_FRAGMENT;
        if (ext == "geom" || ext == "gs")  return NkSLStage::NK_GEOMETRY;
        if (ext == "tesc" || ext == "hs")  return NkSLStage::NK_TESS_CONTROL;
        if (ext == "tese" || ext == "ds")  return NkSLStage::NK_TESS_EVAL;
        if (ext == "comp" || ext == "cs")  return NkSLStage::NK_COMPUTE;
        return NkSLStage::NK_VERTEX; // fallback
    }

    NkString NkShaderFileResolver::ResolveVariant(const NkString& path,
                                                const NkString& targetFmtExt) {
        // "shader.vert.glsl" + "spirv" → "shader.vert.spirv"
        std::string base = DropLastExt(ToStd(path)); // "shader.vert"
        return FromStd(base + "." + ToStd(targetFmtExt));
    }

    bool NkShaderFileResolver::FileExists(const NkString& path) {
        FILE* f = fopen(path.CStr(), "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

    NkVector<NkString> NkShaderFileResolver::FindVariants(const NkString& path) {
        NkVector<NkString> result;
        static const char* kFormats[] = { "glsl", "spirv", "spv", "hlsl", "msl", nullptr };
        for (int i = 0; kFormats[i]; ++i) {
            NkString variant = ResolveVariant(path, NkString(kFormats[i]));
            if (FileExists(variant))
                result.PushBack(variant);
        }
        return result;
    }

    // =============================================================================
    // glslang — initialisation one-time
    // =============================================================================

    #ifdef NK_RHI_GLSLANG_ENABLED

    static EShLanguage ToGlslangStage(NkSLStage stage) {
        switch (stage) {
            case NkSLStage::NK_VERTEX:       return EShLangVertex;
            case NkSLStage::NK_FRAGMENT:     return EShLangFragment;
            case NkSLStage::NK_GEOMETRY:     return EShLangGeometry;
            case NkSLStage::NK_TESS_CONTROL: return EShLangTessControl;
            case NkSLStage::NK_TESS_EVAL:    return EShLangTessEvaluation;
            case NkSLStage::NK_COMPUTE:      return EShLangCompute;
            default:                         return EShLangVertex;
        }
    }

    struct GlslangInit {
        GlslangInit()  { glslang::InitializeProcess(); }
        ~GlslangInit() { glslang::FinalizeProcess(); }
    };

    static GlslangInit& GetGlslangInit() {
        static GlslangInit s;
        return s;
    }

    #endif // NK_RHI_GLSLANG_ENABLED

    // =============================================================================
    // NkShaderConverter — capacités
    // =============================================================================

    bool NkShaderConverter::CanGlslToSpirv() {
    #ifdef NK_RHI_GLSLANG_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToGlsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToHlsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    bool NkShaderConverter::CanSpirvToMsl() {
    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        return true;
    #else
        return false;
    #endif
    }

    // =============================================================================
    // NkShaderConverter::GlslToSpirv
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslToSpirv(
        const NkString& glslSource,
        NkSLStage       stage,
        const NkString& debugName)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_GLSLANG_ENABLED
        (void)GetGlslangInit(); // assure l'initialisation

        EShLanguage lang = ToGlslangStage(stage);
        glslang::TShader shader(lang);

        const char* src = glslSource.CStr();
        const char* names[1] = { debugName.CStr() };
        shader.setStringsWithLengthsAndNames(&src, nullptr, names, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv,   glslang::EShTargetSpv_1_0);
        shader.setEntryPoint("main");

        const TBuiltInResource* resources = GetDefaultResources();
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        if (!shader.parse(resources, 450, false, messages)) {
            out.success = false;
            out.errors  = NkString(shader.getInfoLog());
            return out;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages)) {
            out.success = false;
            out.errors  = NkString(program.getInfoLog());
            return out;
        }

        std::vector<unsigned int> spirvWords;
        glslang::SpvOptions spvOptions;
        spvOptions.generateDebugInfo = false;
        spvOptions.disableOptimizer  = true;
        spvOptions.optimizeSize      = false;

        glslang::GlslangToSpv(*program.getIntermediate(lang), spirvWords, &spvOptions);

        if (spirvWords.empty() || spirvWords[0] != 0x07230203) {
            out.success = false;
            out.errors = NkString("SPIR-V generation failed: invalid magic number");
            return out;
        }

        // Packer les uint32 en bytes
        out.binary.Resize((uint32)(spirvWords.size() * sizeof(unsigned int)));
        memcpy(out.binary.Data(), spirvWords.data(), out.binary.Size());
        out.success = true;

        // LOG : vérifier le magic number
        if (out.SpirvWordCount() > 0) {
            logger.Info("[GlslToSpirv] Generated SPIR-V for {0}: {1} words, magic=0x{2:08x}\n",
                        debugName.CStr(),
                        (unsigned long long)out.SpirvWordCount(),
                        (unsigned long long)out.SpirvWords()[0]);
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_GLSLANG_ENABLED non défini — glslang non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToGlsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToGlsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage /*stage*/)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerGLSL compiler(spirvWords, wordCount);
            spirv_cross::CompilerGLSL::Options opts;
            opts.version = 450;
            opts.es      = false;
            compiler.set_common_options(opts);
            out.source  = NkString(compiler.compile().c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToHlsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToHlsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage /*stage*/,
        uint32 hlslShaderModel)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerHLSL compiler(spirvWords, wordCount);
            spirv_cross::CompilerHLSL::Options opts;
            opts.shader_model = hlslShaderModel;
            compiler.set_hlsl_options(opts);
            out.source  = NkString(compiler.compile().c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::SpirvToMsl
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::SpirvToMsl(
        const uint32* spirvWords, uint32 wordCount, NkSLStage /*stage*/)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerMSL compiler(spirvWords, wordCount);
            spirv_cross::CompilerMSL::Options opts;
            opts.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
            compiler.set_msl_options(opts);
            out.source  = NkString(compiler.compile().c_str());
            out.success = true;
        } catch (const std::exception& e) {
            out.success = false;
            out.errors  = NkString(e.what());
        }
    #else
        out.success = false;
        out.errors  = NkString("NK_RHI_SPIRVCROSS_ENABLED non défini — SPIRV-Cross non disponible.");
    #endif

        return out;
    }

    // =============================================================================
    // NkShaderConverter::LoadFile
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::LoadFile(const NkString& path) {
        NkShaderConvertResult out;
        std::string ext = ToStd(NkShaderFileResolver::FormatExt(path));

        FILE* f = fopen(path.CStr(), "rb");
        if (!f) {
            out.errors = NkString("Impossible d'ouvrir : ") + path;
            return out;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        bool isBinary = (ext == "spirv" || ext == "spv");

        if (isBinary) {
            out.binary.Resize((uint32)sz);
            fread(out.binary.Data(), 1, sz, f);
        } else {
            std::string buf(sz, '\0');
            fread(&buf[0], 1, sz, f);
            out.source = NkString(buf.c_str());
        }
        fclose(f);
        out.success = true;
        return out;
    }

    // =============================================================================
    // NkShaderConverter::LoadAsSpirv
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::LoadAsSpirv(const NkString& path) {
        std::string ext = ToStd(NkShaderFileResolver::FormatExt(path));
        if (ext == "spirv" || ext == "spv") {
            return LoadFile(path);
        }
        if (ext == "glsl") {
            NkShaderConvertResult src = LoadFile(path);
            if (!src.success) return src;
            NkSLStage stage = NkShaderFileResolver::StageFrom(path);
            return GlslToSpirv(src.source, stage, path);
        }
        NkShaderConvertResult out;
        out.errors = NkString("Impossible de charger comme SPIR-V : extension non supportée (") +
                    NkString(ext.c_str()) + NkString(")");
        return out;
    }

    // =============================================================================
    // NkShaderConverter — raccourcis fichier GLSL → texte cible
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslFileToHlsl(const NkString& glslPath, uint32 sm) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToHlsl(spv, NkShaderFileResolver::StageFrom(glslPath), sm);
    }

    NkShaderConvertResult NkShaderConverter::GlslFileToMsl(const NkString& glslPath) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToMsl(spv, NkShaderFileResolver::StageFrom(glslPath));
    }

    NkShaderConvertResult NkShaderConverter::GlslFileToGlsl(const NkString& glslPath) {
        NkShaderConvertResult spv = LoadAsSpirv(glslPath);
        if (!spv.success) return spv;
        return SpirvToGlsl(spv, NkShaderFileResolver::StageFrom(glslPath));
    }

    // =============================================================================
    // NkShaderCache — helpers internes
    // =============================================================================

    static void EnsureDirExists(const std::string& dir) {
        if (dir.empty()) return;
    #ifdef _WIN32
        CreateDirectoryA(dir.c_str(), nullptr);
    #else
        NK_MKDIR(dir.c_str());
    #endif
    }

    // FNV-1a 64-bit
    static uint64 Fnv1a64(const void* data, size_t len, uint64 hash = 14695981039346656037ULL) {
        const uint8* p = static_cast<const uint8*>(data);
        for (size_t i = 0; i < len; ++i) {
            hash ^= (uint64)p[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static const uint32 kNkscMagic = 0x4353474E; // 'NKSC' little-endian

    // =============================================================================
    // NkShaderCache
    // =============================================================================

    void NkShaderCache::SetCacheDir(const NkString& dir) noexcept {
        mCacheDir = dir;
        EnsureDirExists(ToStd(dir));
    }

    uint64 NkShaderCache::ComputeKey(const NkString& source,
                                    NkSLStage       stage,
                                    const NkString& targetFormat) noexcept {
        uint64 h = 14695981039346656037ULL;
        h = Fnv1a64(source.CStr(), strlen(source.CStr()), h);
        h = Fnv1a64(&stage, sizeof(stage), h);
        h = Fnv1a64(targetFormat.CStr(), strlen(targetFormat.CStr()), h);
        return h;
    }

    NkString NkShaderCache::KeyToPath(uint64 key) const noexcept {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llx.nksc", (unsigned long long)key);
        std::string dir = ToStd(mCacheDir);
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            dir += '/';
        return FromStd(dir + buf);
    }

    // NkShaderConvertResult NkShaderCache::Load(uint64 key) const noexcept {
    //     NkShaderConvertResult out;
    //     if (mCacheDir.Empty()) return out;

    //     NkString path = KeyToPath(key);
    //     FILE* f = fopen(path.CStr(), "rb");
    //     if (!f) return out;

    //     uint32 magic = 0; uint64 storedKey = 0; uint32 size = 0;
    //     if (fread(&magic,     sizeof(magic),     1, f) != 1 || magic != kNkscMagic ||
    //         fread(&storedKey, sizeof(storedKey), 1, f) != 1 || storedKey != key    ||
    //         fread(&size,      sizeof(size),      1, f) != 1) {
    //         fclose(f);
    //         return out;
    //     }

    //     out.binary.Resize(size);
    //     if (fread(out.binary.Data(), 1, size, f) != size) {
    //         fclose(f);
    //         out.binary.Clear();
    //         return out;
    //     }
    //     fclose(f);
    //     out.success = true;
    //     return out;
    // }

    NkShaderConvertResult NkShaderCache::Load(uint64 key) const noexcept {
        NkShaderConvertResult out;
        if (mCacheDir.Empty()) return out;

        NkString path = KeyToPath(key);
        FILE* f = fopen(path.CStr(), "rb");
        if (!f) return out;

        uint32 magic = 0; 
        uint64 storedKey = 0; 
        uint32 size = 0;
        
        if (fread(&magic,     sizeof(magic),     1, f) != 1 || magic != kNkscMagic ||
            fread(&storedKey, sizeof(storedKey), 1, f) != 1 || storedKey != key    ||
            fread(&size,      sizeof(size),      1, f) != 1) {
            fclose(f);
            return out;
        }

        // Allouer seulement pour les données SPIR-V, pas pour le header
        out.binary.Resize(size);
        if (fread(out.binary.Data(), 1, size, f) != size) {
            fclose(f);
            out.binary.Clear();
            return out;
        }
        fclose(f);
        out.success = true;
        return out;
    }

    bool NkShaderCache::Save(uint64 key, const NkShaderConvertResult& result) noexcept {
        if (mCacheDir.Empty() || !result.success) return false;
        EnsureDirExists(ToStd(mCacheDir));

        NkString path = KeyToPath(key);

        // Choix de la donnée à sauvegarder : binaire (SPIR-V) prioritaire, sinon texte
        const uint8* data = nullptr;
        uint32 size = 0;
        std::string srcBuf;
        // if (!result.binary.IsEmpty()) {
        //     data = result.binary.Data();
        //     size = (uint32)result.binary.Size();
        // } 
        if (!result.binary.IsEmpty()) {
            // Vérifier que c'est bien du SPIR-V valide
            const uint32* words = reinterpret_cast<const uint32*>(result.binary.Data());
            if (result.binary.Size() >= 4 && words[0] != 0x07230203) {
                logger.Info("[ShaderCache] Warning: saving non-SPIRV data (magic=0x{0:08x})\n", words[0]);
            }
            
            FILE* f = fopen(path.CStr(), "wb");
            if (!f) return false;
            
            // Écrire le header
            fwrite(&kNkscMagic, sizeof(kNkscMagic), 1, f);
            fwrite(&key, sizeof(key), 1, f);
            
            uint32 dataSize = (uint32)result.binary.Size();
            fwrite(&dataSize, sizeof(dataSize), 1, f);
            
            // Écrire les données brutes
            fwrite(result.binary.Data(), 1, dataSize, f);
            fclose(f);
            return true;
        }
        
        srcBuf = ToStd(result.source);
        data   = reinterpret_cast<const uint8*>(srcBuf.data());
        size   = (uint32)srcBuf.size();
            

        FILE* f = fopen(path.CStr(), "wb");
        if (!f) return false;

        fwrite(&kNkscMagic, sizeof(kNkscMagic), 1, f);
        fwrite(&key,        sizeof(key),        1, f);
        fwrite(&size,       sizeof(size),       1, f);
        fwrite(data,        1, size,               f);
        fclose(f);
        return true;
    }

    void NkShaderCache::Invalidate(uint64 key) noexcept {
        if (mCacheDir.Empty()) return;
        NkString path = KeyToPath(key);
    #ifdef _WIN32
        DeleteFileA(path.CStr());
    #else
        remove(path.CStr());
    #endif
    }

    void NkShaderCache::Clear() noexcept {
        if (mCacheDir.Empty()) return;
        std::string dir = ToStd(mCacheDir);
    #ifdef _WIN32
        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*.nksc";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                std::string full = dir + "\\" + fd.cFileName;
                DeleteFileA(full.c_str());
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    #else
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                std::string name = ent->d_name;
                if (name.size() > 5 && name.substr(name.size()-5) == ".nksc") {
                    std::string full = dir + "/" + name;
                    remove(full.c_str());
                }
            }
            closedir(d);
        }
    #endif
    }

    NkShaderCache& NkShaderCache::Global() noexcept {
        static NkShaderCache s;
        return s;
    }

} // namespace nkentseu
