// =============================================================================
// NkSLCompiler.cpp
// =============================================================================
#include "NkSLCompiler.h"
#include "NkSLLexer.h"
#include "NkSLParser.h"
#include "NKLogger/NkLog.h"
// #include "NKCore/NkHash.h"
#include <cstdio>
#include <cstring>

// Si shaderc est disponible (Vulkan SDK)
#ifdef NKSL_HAS_SHADERC
#  include <shaderc/shaderc.hpp>
#endif

// Si glslang est disponible directement
#ifdef NKSL_HAS_GLSLANG
#  include <glslang/Public/ShaderLang.h>
#  include <SPIRV/GlslangToSpv.h>
#endif

#define NKSL_LOG(...)  logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...)  logger_src.Infof("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
using threading::NkScopedLock;

// =============================================================================
// Hachage FNV-1a 64-bit
// =============================================================================
static uint64 FNV1a64(const void* data, size_t len, uint64 seed = 0xcbf29ce484222325ULL) {
    const uint8* p = static_cast<const uint8*>(data);
    uint64 h = seed;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// =============================================================================
// NkSLCache
// =============================================================================
NkSLCache::NkSLCache(const NkString& cacheDir) : mCacheDir(cacheDir) {
    if (!cacheDir.Empty()) Load();
}

uint64 NkSLCache::MakeKey(uint64 hash, NkSLTarget t, NkSLStage s) const {
    uint64 k = hash;
    k = FNV1a64(&t, sizeof(t), k);
    k = FNV1a64(&s, sizeof(s), k);
    return k;
}

bool NkSLCache::Has(uint64 hash, NkSLTarget t, NkSLStage s) const {
    NkScopedLock lock(mMutex);
    uint64 k = MakeKey(hash, t, s);
    for (auto& e : mEntries) if (MakeKey(e.sourceHash, e.target, e.stage) == k) return true;
    return false;
}

bool NkSLCache::Get(uint64 hash, NkSLTarget t, NkSLStage s, NkSLCacheEntry& out) const {
    NkScopedLock lock(mMutex);
    uint64 k = MakeKey(hash, t, s);
    for (auto& e : mEntries) {
        if (MakeKey(e.sourceHash, e.target, e.stage) == k) { out = e; return true; }
    }
    return false;
}

void NkSLCache::Put(uint64 hash, NkSLTarget t, NkSLStage s, const NkSLCompileResult& r) {
    NkScopedLock lock(mMutex);
    uint64 k = MakeKey(hash, t, s);
    for (auto& e : mEntries) {
        if (MakeKey(e.sourceHash, e.target, e.stage) == k) {
            e.bytecode = r.bytecode; e.source = r.source; return;
        }
    }
    NkSLCacheEntry entry;
    entry.sourceHash = hash; entry.target = t; entry.stage = s;
    entry.bytecode   = r.bytecode; entry.source = r.source;
    entry.isSpirv    = (t == NkSLTarget::SPIRV);
    mEntries.PushBack(entry);
}

void NkSLCache::Clear() {
    NkScopedLock lock(mMutex);
    mEntries.Clear();
}

void NkSLCache::Flush() {
    if (mCacheDir.Empty()) return;
    // Sérialisation binaire simple :
    // [entry_count: u32] [per entry: hash u64, target u32, stage u32, src_len u32, src..., bc_len u32, bc...]
    NkString path = mCacheDir + "/nksl.cache";
    FILE* f = fopen(path.CStr(), "wb");
    if (!f) return;

    NkScopedLock lock(mMutex);
    uint32 cnt = (uint32)mEntries.Size();
    fwrite(&cnt, sizeof(cnt), 1, f);
    for (auto& e : mEntries) {
        fwrite(&e.sourceHash, sizeof(uint64), 1, f);
        uint32 tgt = (uint32)e.target, stg = (uint32)e.stage;
        fwrite(&tgt, sizeof(uint32), 1, f);
        fwrite(&stg, sizeof(uint32), 1, f);
        uint32 srcLen = (uint32)e.source.Size();
        fwrite(&srcLen, sizeof(uint32), 1, f);
        if (srcLen) fwrite(e.source.CStr(), 1, srcLen, f);
        uint32 bcLen = (uint32)e.bytecode.Size();
        fwrite(&bcLen, sizeof(uint32), 1, f);
        if (bcLen) fwrite(e.bytecode.Data(), 1, bcLen, f);
    }
    fclose(f);
}

void NkSLCache::Load() {
    if (mCacheDir.Empty()) return;
    NkString path = mCacheDir + "/nksl.cache";
    FILE* f = fopen(path.CStr(), "rb");
    if (!f) return;

    uint32 cnt = 0;
    fread(&cnt, sizeof(cnt), 1, f);
    for (uint32 i = 0; i < cnt && !feof(f); i++) {
        NkSLCacheEntry e;
        fread(&e.sourceHash, sizeof(uint64), 1, f);
        uint32 tgt=0, stg=0;
        fread(&tgt, sizeof(uint32), 1, f);
        fread(&stg, sizeof(uint32), 1, f);
        e.target = (NkSLTarget)tgt; e.stage = (NkSLStage)stg;
        uint32 srcLen=0; fread(&srcLen, sizeof(uint32), 1, f);
        if (srcLen) {
            NkVector<char> buf; buf.Resize(srcLen+1);
            fread(buf.Data(), 1, srcLen, f);
            buf[srcLen] = '\0';
            e.source = NkString(buf.Data());
        }
        uint32 bcLen=0; fread(&bcLen, sizeof(uint32), 1, f);
        if (bcLen) {
            e.bytecode.Resize(bcLen);
            fread(e.bytecode.Data(), 1, bcLen, f);
        }
        mEntries.PushBack(e);
    }
    fclose(f);
    NKSL_LOG("Cache chargé : %u entrées depuis %s\n", cnt, path.CStr());
}

// =============================================================================
// NkSLCompiler
// =============================================================================
NkSLCompiler::NkSLCompiler(const NkString& cacheDir)
    : mCache(cacheDir) {}

void NkSLCompiler::SetCacheDir(const NkString& dir) {
    mCache.SetDirectory(dir);  // NkSLCache::SetDirectory() — évite la copie du mutex
}

uint64 NkSLCompiler::HashSource(const NkString& src, NkSLStage stage,
                                 NkSLTarget target,
                                 const NkSLCompileOptions& opts) const {
    uint64 h = FNV1a64(src.CStr(), src.Size());
    h = FNV1a64(&stage,  sizeof(stage),  h);
    h = FNV1a64(&target, sizeof(target), h);
    h = FNV1a64(&opts.glslVersion, sizeof(uint32), h);
    h = FNV1a64(&opts.hlslShaderModel, sizeof(uint32), h);
    h = FNV1a64(&opts.mslVersion, sizeof(uint32), h);
    return h;
}

// =============================================================================
NkSLCompileResult NkSLCompiler::Compile(
    const NkString&          source,
    NkSLStage                stage,
    NkSLTarget               target,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    // 1. Vérifier le cache
    if (mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        NkSLCacheEntry cached;
        if (mCache.Get(h, target, stage, cached)) {
            NkSLCompileResult res;
            res.success  = true;
            res.source   = cached.source;
            res.bytecode = cached.bytecode;
            res.target   = target;
            res.stage    = stage;
            NKSL_LOG("Cache hit: %s stage=%s target=%s\n",
                     filename.CStr(), NkSLStageName(stage), NkSLTargetName(target));
            return res;
        }
    }

    // 2. Prétraitement (#include, #define macros)
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        NkSLCompileResult res;
        res.success = false;
        res.errors  = ppErrors;
        res.target  = target;
        res.stage   = stage;
        return res;
    }

    // 3. Lexer → Parser → AST
    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (parser.HasErrors()) {
        NkSLCompileResult res;
        res.success = false;
        res.errors  = parser.GetErrors();
        res.target  = target;
        res.stage   = stage;
        delete ast;
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u: %s\n", filename.CStr(), e.line, e.message.CStr());
        return res;
    }

    // 4. Génération de code cible
    NkSLCompileResult res;
    switch (target) {
        case NkSLTarget::GLSL: {
            NkSLCodeGen_GLSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        case NkSLTarget::SPIRV: {
            // Étape 1 : générer le GLSL intermédiaire
            NkSLCodeGen_GLSL glslGen;
            NkSLCompileOptions glslOpts = opts;
            glslOpts.glslVersion = 450; // GLSL 4.50 recommandé pour Vulkan
            NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
            if (!glslRes.success) { res = glslRes; break; }
            // Étape 2 : GLSL → SPIR-V
            res = CompileToSPIRV(glslRes.source, stage, opts);
            if (!res.success) {
                // Fallback : retourner le GLSL si pas de compilateur SPIR-V
                NKSL_ERR("GLSL→SPIR-V failed, returning GLSL text for manual compilation\n");
                res = glslRes;
                res.target = NkSLTarget::GLSL; // signal que c'est du texte
            }
            break;
        }
        case NkSLTarget::HLSL: {
            NkSLCodeGen_HLSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        case NkSLTarget::MSL: {
            NkSLCodeGen_MSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        case NkSLTarget::CPLUSPLUS: {
            NkSLCodeGen_CPP gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        default:
            res.success = false;
            res.AddError(0, "Unsupported target");
            break;
    }

    delete ast;

    // 5. Mettre en cache si succès
    if (res.success && mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, res);
    }

    if (!res.success) {
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u: %s\n", filename.CStr(), e.line, e.message.CStr());
    } else {
        NKSL_LOG("Compiled %s stage=%s target=%s (%zu bytes)\n",
                 filename.CStr(), NkSLStageName(stage), NkSLTargetName(target),
                 (size_t)res.bytecode.Size());
    }

    return res;
}

// =============================================================================
NkSLCompiler::MultiTargetResult NkSLCompiler::CompileAllTargets(
    const NkString&           source,
    NkSLStage                 stage,
    const NkVector<NkSLTarget>& targets,
    const NkSLCompileOptions&  opts,
    const NkString&            filename)
{
    MultiTargetResult multi;
    for (auto t : targets) {
        multi.results.PushBack(Compile(source, stage, t, opts, filename));
    }
    return multi;
}

// =============================================================================
NkSLCompileResult NkSLCompiler::CompileToSPIRV(
    const NkString&          glslSource,
    NkSLStage                stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::SPIRV;
    res.stage  = stage;

#ifdef NKSL_HAS_SHADERC
    if (GLSLtoSPIRV_shaderc(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true;
        return res;
    }
#endif

#ifdef NKSL_HAS_GLSLANG
    if (GLSLtoSPIRV_glslang(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true;
        return res;
    }
#endif

    // Aucun compilateur SPIR-V disponible :
    // Retourner le GLSL comme texte (avec un avertissement)
    res.success = false;
    res.AddError(0,
        "No SPIR-V compiler available (install Vulkan SDK with shaderc or glslang). "
        "Build with -DNKSL_HAS_SHADERC or -DNKSL_HAS_GLSLANG.");
    return res;
}

// =============================================================================
#ifdef NKSL_HAS_SHADERC
bool NkSLCompiler::GLSLtoSPIRV_shaderc(
    const NkString&          glsl,
    NkSLStage                stage,
    const NkSLCompileOptions& opts,
    NkVector<uint8>&         spirvOut,
    NkVector<NkSLCompileError>& errors)
{
    shaderc_shader_kind kind;
    switch (stage) {
        case NkSLStage::VERTEX:       kind = shaderc_vertex_shader;   break;
        case NkSLStage::FRAGMENT:     kind = shaderc_fragment_shader; break;
        case NkSLStage::GEOMETRY:     kind = shaderc_geometry_shader; break;
        case NkSLStage::TESS_CONTROL: kind = shaderc_tess_control_shader; break;
        case NkSLStage::TESS_EVAL:    kind = shaderc_tess_evaluation_shader; break;
        case NkSLStage::COMPUTE:      kind = shaderc_compute_shader;  break;
        default: kind = shaderc_vertex_shader;
    }

    shaderc::Compiler       compiler;
    shaderc::CompileOptions shadercOpts;
    shadercOpts.SetTargetEnvironment(shaderc_target_env_vulkan,
        opts.vulkanVersion >= 120 ? shaderc_env_version_vulkan_1_2 :
        opts.vulkanVersion >= 110 ? shaderc_env_version_vulkan_1_1 :
                                    shaderc_env_version_vulkan_1_0);
    if (opts.optimize) shadercOpts.SetOptimizationLevel(shaderc_optimization_level_performance);
    if (opts.debugInfo) shadercOpts.SetGenerateDebugInfo();

    auto result = compiler.CompileGlslToSpv(glsl.CStr(), kind, "nksl_shader", shadercOpts);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        // Parser les messages d'erreur shaderc
        NkString msg(result.GetErrorMessage());
        errors.PushBack({0, 0, "", msg, true});
        NKSL_ERR("shaderc: %s\n", msg.CStr());
        return false;
    }

    // Copier le SPIR-V
    const uint32* words = result.cbegin();
    size_t wordCount = result.cend() - result.cbegin();
    spirvOut.Resize(wordCount * sizeof(uint32));
    memcpy(spirvOut.Data(), words, wordCount * sizeof(uint32));

    NKSL_LOG("shaderc: SPIR-V generated (%zu words)\n", wordCount);
    return true;
}
#else
bool NkSLCompiler::GLSLtoSPIRV_shaderc(const NkString&, NkSLStage,
    const NkSLCompileOptions&, NkVector<uint8>&, NkVector<NkSLCompileError>&)
{ return false; }
#endif

// =============================================================================
#ifdef NKSL_HAS_GLSLANG
static const TBuiltInResource kDefaultGlslangResources = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};

bool NkSLCompiler::GLSLtoSPIRV_glslang(
    const NkString&          glsl,
    NkSLStage                stage,
    const NkSLCompileOptions& opts,
    NkVector<uint8>&         spirvOut,
    NkVector<NkSLCompileError>& errors)
{
    static bool initialized = false;
    if (!initialized) { glslang::InitializeProcess(); initialized = true; }

    EShLanguage lang;
    switch (stage) {
        case NkSLStage::VERTEX:       lang = EShLangVertex;   break;
        case NkSLStage::FRAGMENT:     lang = EShLangFragment; break;
        case NkSLStage::GEOMETRY:     lang = EShLangGeometry; break;
        case NkSLStage::TESS_CONTROL: lang = EShLangTessControl; break;
        case NkSLStage::TESS_EVAL:    lang = EShLangTessEvaluation; break;
        case NkSLStage::COMPUTE:      lang = EShLangCompute;  break;
        default: lang = EShLangVertex;
    }

    glslang::TShader shader(lang);
    const char* src = glsl.CStr();
    shader.setStrings(&src, 1);

    int glslVer = (int)opts.glslVersion;
    if (glslVer < 450) glslVer = 450;

    shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
    uint32 vkVer = opts.vulkanVersion;
    glslang::EshTargetClientVersion clientVer =
        vkVer >= 120 ? glslang::EShTargetVulkan_1_2 :
        vkVer >= 110 ? glslang::EShTargetVulkan_1_1 :
                       glslang::EShTargetVulkan_1_0;
    shader.setEnvClient(glslang::EShClientVulkan, clientVer);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    EShMessages msgs = EShMsgSpvRules;
    if (opts.debugInfo) msgs = (EShMessages)(msgs | EShMsgDebugInfo);

    if (!shader.parse(&kDefaultGlslangResources, glslVer, false, msgs)) {
        errors.PushBack({0, 0, "", NkString(shader.getInfoLog()), true});
        NKSL_ERR("glslang parse: %s\n", shader.getInfoLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(msgs)) {
        errors.PushBack({0, 0, "", NkString(program.getInfoLog()), true});
        NKSL_ERR("glslang link: %s\n", program.getInfoLog());
        return false;
    }

    std::vector<uint32> spv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOpts;
    spvOpts.generateDebugInfo = opts.debugInfo;
    spvOpts.disableOptimizer  = !opts.optimize;
    glslang::GlslangToSpv(*program.getIntermediate(lang), spv, &logger, &spvOpts);

    if (!logger.getAllMessages().empty())
        NKSL_LOG("glslang SPV: %s\n", logger.getAllMessages().c_str());

    spirvOut.Resize(spv.size() * sizeof(uint32));
    memcpy(spirvOut.Data(), spv.data(), spv.size() * sizeof(uint32));

    NKSL_LOG("glslang: SPIR-V generated (%zu words)\n", spv.size());
    return true;
}
#else
bool NkSLCompiler::GLSLtoSPIRV_glslang(const NkString&, NkSLStage,
    const NkSLCompileOptions&, NkVector<uint8>&, NkVector<NkSLCompileError>&)
{ return false; }
#endif

// =============================================================================
NkVector<NkSLCompileError> NkSLCompiler::Validate(const NkString& source,
                                                    const NkString& filename) {
    NkSLLexer  lexer(source, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    delete ast;
    NkVector<NkSLCompileError> errors = parser.GetErrors();
    for (auto& w : parser.GetWarnings()) errors.PushBack(w);
    return errors;
}

// =============================================================================
NkString NkSLCompiler::Preprocess(const NkString& source,
                                    const NkString& baseDir,
                                    NkVector<NkSLCompileError>* errors)
{
    // Préprocesseur simple : gestion des #include "file.nksl"
    // et des #define / #ifdef / #endif
    NkString result;
    const char* p = source.CStr();
    uint32 line = 1;

    while (*p) {
        // Début de ligne
        const char* lineStart = p;

        // Sauter les espaces
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#') {
            p++;
            // Lire le nom de la directive
            while (*p == ' ' || *p == '\t') p++;
            NkString directive;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                directive += *p++;
            }

            if (directive == "include") {
                // Lire le nom du fichier
                while (*p == ' ' || *p == '\t') p++;
                char delim = *p++;
                char closeDelim = (delim == '"') ? '"' : '>';
                NkString fname;
                while (*p && *p != closeDelim && *p != '\n') fname += *p++;
                if (*p == closeDelim) p++;

                // Charger et insérer le contenu du fichier
                NkString fullPath = baseDir.Empty() ? fname : (baseDir + "/" + fname);
                FILE* f = fopen(fullPath.CStr(), "r");
                if (f) {
                    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                    NkVector<char> buf; buf.Resize(sz + 1);
                    fread(buf.Data(), 1, sz, f); buf[sz] = '\0';
                    fclose(f);
                    // Récursion
                    NkString included = Preprocess(NkString(buf.Data()), baseDir, errors);
                    result += "// #include \"" + fname + "\"\n";
                    result += included;
                    result += "\n";
                } else {
                    if (errors)
                        errors->PushBack({line, 0, "", "#include file not found: " + fname, false});
                    // Continuer sans le fichier
                }
            } else if (directive == "pragma" || directive == "version" ||
                       directive == "extension") {
                // Ignorer les directives de version/pragma (gérées par le backend)
                while (*p && *p != '\n') p++;
            } else if (directive == "define" || directive == "ifdef" ||
                       directive == "ifndef" || directive == "endif" ||
                       directive == "if"     || directive == "else"  ||
                       directive == "elif"   || directive == "undef") {
                // Passer telles quelles (le backend GLSL les gère)
                result += '#';
                result += directive;
                while (*p && *p != '\n') result += *p++;
            } else {
                // Directive inconnue : passer telle quelle
                result += '#';
                result += directive;
                while (*p && *p != '\n') result += *p++;
            }
        } else {
            // Ligne normale : copier jusqu'au \n
            while (*p && *p != '\n') result += *p++;
        }

        if (*p == '\n') { result += '\n'; p++; line++; }
        else if (*p == '\r') { p++; if (*p == '\n') { result += '\n'; p++; } line++; }
    }

    return result;
}

// =============================================================================
bool NkSLCompiler::FillShaderDesc(
    const NkString&          nkslSource,
    NkSLStage                stage,
    NkSLTarget               targetApi,
    NkShaderDesc&            outDesc,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkSLStage nkStage = stage;
    NkShaderStage rhiStage;
    switch (nkStage) {
        case NkSLStage::VERTEX:       rhiStage = NkShaderStage::NK_VERTEX;    break;
        case NkSLStage::FRAGMENT:     rhiStage = NkShaderStage::NK_FRAGMENT;  break;
        case NkSLStage::GEOMETRY:     rhiStage = NkShaderStage::NK_GEOMETRY;  break;
        case NkSLStage::TESS_CONTROL: rhiStage = NkShaderStage::NK_TESS_CTRL; break;
        case NkSLStage::TESS_EVAL:    rhiStage = NkShaderStage::NK_TESS_EVAL; break;
        case NkSLStage::COMPUTE:      rhiStage = NkShaderStage::NK_COMPUTE;   break;
        default: rhiStage = NkShaderStage::NK_VERTEX;
    }

    NkSLCompileResult res = Compile(nkslSource, nkStage, targetApi, opts, filename);

    if (!res.success) {
        NKSL_ERR("FillShaderDesc: compilation failed for %s\n", filename.CStr());
        return false;
    }

    NkShaderStageDesc stageDesc{};
    stageDesc.stage      = rhiStage;
    stageDesc.entryPoint = opts.entryPoint.CStr();

    switch (targetApi) {
        case NkSLTarget::SPIRV:
            // bytecode = SPIR-V binaire
            stageDesc.spirvData = res.bytecode.Data();
            stageDesc.spirvSize = (uint64)res.bytecode.Size();
            // Stocker le bytecode dans le desc (le caller doit le garder vivant !)
            // On copie dans un buffer persistant via le cache
            {
                NkSLCacheEntry cached;
                uint64 h = HashSource(nkslSource, nkStage, targetApi, opts);
                mCache.Get(h, targetApi, nkStage, cached);
                stageDesc.spirvData = cached.bytecode.Data();
                stageDesc.spirvSize = (uint64)cached.bytecode.Size();
            }
            break;
        case NkSLTarget::GLSL:
            stageDesc.glslSource = res.source.CStr();
            break;
        case NkSLTarget::HLSL:
            stageDesc.hlslSource = res.source.CStr();
            break;
        case NkSLTarget::MSL:
            stageDesc.mslSource  = res.source.CStr();
            break;
        default:
            break;
    }

    outDesc.AddStage(stageDesc);
    return true;
}

// =============================================================================
// NkSLShaderLibrary
// =============================================================================
NkSLShaderLibrary::NkSLShaderLibrary(NkSLCompiler* compiler, const NkString& baseDir)
    : mCompiler(compiler), mBaseDir(baseDir) {}

bool NkSLShaderLibrary::Register(const NkString& name, const NkString& filePath,
                                   const NkVector<NkSLStage>& stages) {
    // Charger le fichier source
    NkString fullPath = mBaseDir.Empty() ? filePath : (mBaseDir + "/" + filePath);
    FILE* f = fopen(fullPath.CStr(), "r");
    if (!f) {
        NKSL_ERR("ShaderLibrary: cannot open '%s'\n", fullPath.CStr());
        return false;
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkVector<char> buf; buf.Resize(sz + 1);
    fread(buf.Data(), 1, sz, f); buf[sz] = '\0';
    fclose(f);

    ShaderEntry entry;
    entry.name       = name;
    entry.sourcePath = fullPath;
    entry.source     = NkString(buf.Data());
    entry.stages     = stages;
    entry.dirty      = true;
    mEntries.PushBack(entry);
    return true;
}

bool NkSLShaderLibrary::RegisterInline(const NkString& name, const NkString& source,
                                        const NkVector<NkSLStage>& stages) {
    ShaderEntry entry;
    entry.name   = name;
    entry.source = source;
    entry.stages = stages;
    entry.dirty  = true;
    mEntries.PushBack(entry);
    return true;
}

bool NkSLShaderLibrary::CompileAll(NkSLTarget target, const NkSLCompileOptions& opts) {
    bool allOk = true;
    for (auto& e : mEntries) {
        if (!e.dirty) continue;
        e.compiled.Clear();
        for (auto stage : e.stages) {
            auto res = mCompiler->Compile(e.source, stage, target, opts, e.name);
            e.compiled.PushBack(res);
            if (!res.success) {
                allOk = false;
                NKSL_ERR("ShaderLibrary: '%s' stage=%s FAILED\n",
                         e.name.CStr(), NkSLStageName(stage));
            }
        }
        e.dirty = false;
    }
    return allOk;
}

const NkSLCompileResult* NkSLShaderLibrary::Get(const NkString& name,
                                                  NkSLStage stage,
                                                  NkSLTarget target) const {
    for (auto& e : mEntries) {
        if (e.name != name) continue;
        for (uint32 i = 0; i < (uint32)e.stages.Size(); i++) {
            if (e.stages[i] == stage && i < (uint32)e.compiled.Size())
                return &e.compiled[i];
        }
    }
    return nullptr;
}

bool NkSLShaderLibrary::FillShaderDesc(const NkString& name, NkSLTarget target,
                                        NkShaderDesc& outDesc,
                                        const NkSLCompileOptions& opts) const {
    for (auto& e : mEntries) {
        if (e.name != name) continue;
        for (auto stage : e.stages) {
            const NkSLCompileResult* res = Get(name, stage, target);
            if (!res || !res->success) {
                // Essayer de compiler à la demande
                auto compiled = const_cast<NkSLCompiler*>(mCompiler)->Compile(
                    e.source, stage, target, opts, name);
                if (!compiled.success) return false;
                // Ajouter directement
                NkShaderStageDesc sd{};
                NkShaderStage rhiStage;
                switch (stage) {
                    case NkSLStage::VERTEX:   rhiStage = NkShaderStage::NK_VERTEX;   break;
                    case NkSLStage::FRAGMENT: rhiStage = NkShaderStage::NK_FRAGMENT; break;
                    case NkSLStage::COMPUTE:  rhiStage = NkShaderStage::NK_COMPUTE;  break;
                    default: rhiStage = NkShaderStage::NK_VERTEX;
                }
                sd.stage = rhiStage;
                if (target == NkSLTarget::GLSL)   sd.glslSource = compiled.source.CStr();
                if (target == NkSLTarget::HLSL)   sd.hlslSource = compiled.source.CStr();
                if (target == NkSLTarget::MSL)    sd.mslSource  = compiled.source.CStr();
                if (target == NkSLTarget::SPIRV)  {
                    sd.spirvData = compiled.bytecode.Data();
                    sd.spirvSize = compiled.bytecode.Size();
                }
                outDesc.AddStage(sd);
                continue;
            }
            NkShaderStageDesc sd{};
            NkShaderStage rhiStage;
            switch (stage) {
                case NkSLStage::VERTEX:   rhiStage = NkShaderStage::NK_VERTEX;   break;
                case NkSLStage::FRAGMENT: rhiStage = NkShaderStage::NK_FRAGMENT; break;
                case NkSLStage::COMPUTE:  rhiStage = NkShaderStage::NK_COMPUTE;  break;
                default: rhiStage = NkShaderStage::NK_VERTEX;
            }
            sd.stage = rhiStage;
            if (target == NkSLTarget::GLSL)  sd.glslSource = res->source.CStr();
            if (target == NkSLTarget::HLSL)  sd.hlslSource = res->source.CStr();
            if (target == NkSLTarget::MSL)   sd.mslSource  = res->source.CStr();
            if (target == NkSLTarget::SPIRV) {
                sd.spirvData = res->bytecode.Data();
                sd.spirvSize = res->bytecode.Size();
            }
            outDesc.AddStage(sd);
        }
        return true;
    }
    NKSL_ERR("ShaderLibrary: shader '%s' not found\n", name.CStr());
    return false;
}

uint32 NkSLShaderLibrary::HotReload(NkSLTarget target, const NkSLCompileOptions& opts) {
    uint32 reloaded = 0;
    for (auto& e : mEntries) {
        if (e.sourcePath.Empty()) continue;
        // Vérifier si le fichier a changé (mtime)
        FILE* f = fopen(e.sourcePath.CStr(), "r");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        NkVector<char> buf; buf.Resize(sz+1);
        fread(buf.Data(), 1, sz, f); buf[sz]='\0';
        fclose(f);
        NkString newSrc(buf.Data());
        if (newSrc != e.source) {
            e.source = newSrc;
            e.dirty  = true;
            NKSL_LOG("HotReload: '%s' changed, recompiling...\n", e.name.CStr());
        }
        if (e.dirty) {
            CompileAll(target, opts);
            reloaded++;
        }
    }
    return reloaded;
}

} // namespace nkentseu