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
        const uint32* spirvWords, uint32 wordCount, NkSLStage stage)
    {
        NkShaderConvertResult out;

    #ifdef NK_RHI_SPIRVCROSS_ENABLED
        try {
            spirv_cross::CompilerGLSL compiler(spirvWords, wordCount);

            // Inspecter les ressources avant de compiler.
            auto resources = compiler.get_shader_resources();

            spirv_cross::CompilerGLSL::Options opts;
            opts.version = 450;
            opts.es      = false;
            // flip_vert_y : pour les VS géométrie dont les matrices sont en convention
            // VK (NDC Y=-1 au top). Deux exceptions ne doivent PAS être flippées :
            //
            // 1. VS "2D pur" (ex. Render2D) : push_constant sans aucun UBO.
            //    La matrice ortho est en convention GL (NDC Y=+1 au top) ; la flipper
            //    l'inverserait. Distinguant : has_pc && !has_ubo.
            //
            // 2. VS depth-only (ex. shadow pass) : aucun varying en sortie (stage_outputs
            //    vide — seul gl_Position est écrit). La convention Y du shadow map doit
            //    rester cohérente avec le sampling PBR (cascadeMats non-flippées) ;
            //    flipper créerait un décalage UV Y → ombres sur les mauvais pixels.
            if (stage == NkSLStage::NK_VERTEX && !resources.stage_inputs.empty()) {
                bool purePC   = !resources.push_constant_buffers.empty()
                             && resources.uniform_buffers.empty();
                bool depthOnly = resources.stage_outputs.empty();
                opts.vertex.flip_vert_y = !purePC && !depthOnly;
            }
            compiler.set_common_options(opts);

            // Renommer la variable push_constant en _PushConstants pour que
            // NkOpenGLCommandBuffer::PushConstants (emulation GL) puisse la trouver.
            for (auto& pc_res : resources.push_constant_buffers) {
                compiler.set_name(pc_res.id, "_PushConstants");
            }
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
    // NkShaderConverter — conversion GLSL Vulkan-style → cible (memory)
    // Source canonique : GLSL Vulkan (avec layout(set=,binding=), push_constant…).
    // Chaine GlslToSpirv (glslang) → SpirvToHlsl/Msl/Glsl (SPIRV-Cross). En cas
    // d'echec d'une etape, l'erreur est propagee sans appeler la suivante.
    // =============================================================================

    NkShaderConvertResult NkShaderConverter::GlslToHlsl(
        const NkString& glslSource, NkSLStage stage, uint32 hlslShaderModel,
        const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToHlsl(spv, stage, hlslShaderModel);
    }

    NkShaderConvertResult NkShaderConverter::GlslToMsl(
        const NkString& glslSource, NkSLStage stage, const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToMsl(spv, stage);
    }

    NkShaderConvertResult NkShaderConverter::GlslToGlsl(
        const NkString& glslSource, NkSLStage stage, const NkString& debugName)
    {
        NkShaderConvertResult spv = GlslToSpirv(glslSource, stage, debugName);
        if (!spv.success) return spv;
        return SpirvToGlsl(spv, stage);
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
        // Cache hit -> marquer la cle comme touchee pour la GC.
        MarkTouched(key);
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
            MarkTouched(key);
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
        MarkTouched(key);
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

    // ── Tracking session ─────────────────────────────────────────────────────
    void NkShaderCache::MarkTouched(uint64 key) const noexcept {
        // Insertion ordonnee + dedup : on garde mTouchedKeys tri pour lookup
        // O(log n) via binary search. Cout par-Load/Save acceptable (10s
        // d'entrees par session typique).
        if (!mTouchedSorted) {
            mTouchedKeys.PushBack(key);
            mTouchedSorted = false;
            return;
        }
        // binary search insert
        nk_size lo = 0, hi = mTouchedKeys.Size();
        while (lo < hi) {
            nk_size mid = (lo + hi) >> 1u;
            if (mTouchedKeys[mid] < key) lo = mid + 1;
            else                          hi = mid;
        }
        if (lo < mTouchedKeys.Size() && mTouchedKeys[lo] == key) return; // dedup
        mTouchedKeys.PushBack(key);  // append puis on retriera au prochain IsTouched
        mTouchedSorted = false;
    }

    bool NkShaderCache::IsTouched(uint64 key) const noexcept {
        if (!mTouchedSorted) {
            std::sort(mTouchedKeys.begin(), mTouchedKeys.end());
            mTouchedSorted = true;
        }
        nk_size lo = 0, hi = mTouchedKeys.Size();
        while (lo < hi) {
            nk_size mid = (lo + hi) >> 1u;
            if (mTouchedKeys[mid] < key)      lo = mid + 1;
            else if (mTouchedKeys[mid] > key) hi = mid;
            else                              return true;
        }
        return false;
    }

    // ── Purge generique : parcourt le dossier, supprime selon le predicat ────
    uint32 NkShaderCache::PurgeImpl(const NkVector<uint64>& keepKeys,
                                     bool ageCheck, uint64 maxAgeSeconds) noexcept {
        if (mCacheDir.Empty()) return 0;
        // Trier keepKeys pour binary search
        NkVector<uint64> sorted = keepKeys;
        std::sort(sorted.begin(), sorted.end());

        auto isKept = [&](uint64 k) -> bool {
            nk_size lo = 0, hi = sorted.Size();
            while (lo < hi) {
                nk_size mid = (lo + hi) >> 1u;
                if (sorted[mid] < k)      lo = mid + 1;
                else if (sorted[mid] > k) hi = mid;
                else                       return true;
            }
            return false;
        };

        // Now() en secondes Unix epoch (pour ageCheck).
        uint64 nowSec = (uint64)time(nullptr);
        uint32 removed = 0;
        std::string dir = ToStd(mCacheDir);

    #ifdef _WIN32
        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*.nksc";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                const char* name = fd.cFileName;
                // Parse 16-char hex prefix -> key
                if (strlen(name) < 21) continue; // "%016llx.nksc" = 21 chars
                uint64 key = 0;
                if (sscanf(name, "%16llx.nksc", (unsigned long long*)&key) != 1) continue;

                bool shouldRemove = false;
                if (!keepKeys.Empty()) {
                    shouldRemove = !isKept(key);
                }
                if (ageCheck) {
                    ULARGE_INTEGER ull;
                    ull.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
                    ull.HighPart = fd.ftLastWriteTime.dwHighDateTime;
                    // FILETIME = 100ns intervalles depuis 1601. Conversion en sec Unix.
                    uint64 fileSec = (ull.QuadPart / 10000000ULL) - 11644473600ULL;
                    if (nowSec > fileSec && (nowSec - fileSec) > maxAgeSeconds)
                        shouldRemove = true;
                }
                if (shouldRemove) {
                    std::string full = dir + "\\" + name;
                    if (DeleteFileA(full.c_str())) ++removed;
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    #else
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                std::string name = ent->d_name;
                if (name.size() != 21 || name.substr(name.size()-5) != ".nksc") continue;
                uint64 key = 0;
                if (sscanf(name.c_str(), "%16llx.nksc", (unsigned long long*)&key) != 1) continue;

                std::string full = dir + "/" + name;
                bool shouldRemove = false;
                if (!keepKeys.Empty()) {
                    shouldRemove = !isKept(key);
                }
                if (ageCheck) {
                    struct stat st {};
                    if (stat(full.c_str(), &st) == 0) {
                        uint64 fileSec = (uint64)st.st_mtime;
                        if (nowSec > fileSec && (nowSec - fileSec) > maxAgeSeconds)
                            shouldRemove = true;
                    }
                }
                if (shouldRemove && remove(full.c_str()) == 0) ++removed;
            }
            closedir(d);
        }
    #endif
        return removed;
    }

    uint32 NkShaderCache::PurgeUnused(const NkVector<uint64>& livingKeys) noexcept {
        return PurgeImpl(livingKeys, /*ageCheck=*/false, 0);
    }

    uint32 NkShaderCache::PurgeUnusedThisSession() noexcept {
        // Tri local (la signature de PurgeImpl trie de toute façon).
        if (!mTouchedSorted) {
            std::sort(mTouchedKeys.begin(), mTouchedKeys.end());
            mTouchedSorted = true;
        }
        return PurgeImpl(mTouchedKeys, /*ageCheck=*/false, 0);
    }

    uint32 NkShaderCache::PurgeOlderThan(uint64 maxAgeSeconds) noexcept {
        NkVector<uint64> empty;
        return PurgeImpl(empty, /*ageCheck=*/true, maxAgeSeconds);
    }

} // namespace nkentseu
