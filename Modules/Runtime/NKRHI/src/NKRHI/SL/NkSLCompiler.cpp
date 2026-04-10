// =============================================================================
// NkSLCompiler.cpp  — v3.0
// =============================================================================
#include "NkSLCompiler.h"
#include "NkSLLexer.h"
#include "NkSLParser.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

// =============================================================================
// glslang embarqué — standalone (SANS dépendance Vulkan SDK)
//
// Pour embarquer glslang :
//   git submodule add https://github.com/KhronosGroup/glslang third_party/glslang
//
// Dans CMakeLists.txt :
//   set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
//   set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
//   set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
//   add_subdirectory(third_party/glslang)
//   target_link_libraries(NkSL PRIVATE glslang SPIRV glslang-default-resource-limits)
//   target_compile_definitions(NkSL PRIVATE NKSL_HAS_GLSLANG)
//
// glslang est MULTIPLATFORM : Windows/Linux/macOS/Android/iOS
// SPIR-V est MULTIPLATFORM : format binaire portable, compilable sur tous les OS
// =============================================================================
#ifdef NKSL_HAS_GLSLANG
#  include <glslang/Public/ShaderLang.h>
#  include <glslang/Public/ResourceLimits.h>
#  include <glslang/SPIRV/GlslangToSpv.h>
#endif

// =============================================================================
// shaderc optionnel (Vulkan SDK) — si disponible, utilisé en priorité
// =============================================================================
#ifdef NKSL_HAS_SHADERC
#  include <shaderc/shaderc.hpp>
#endif

#define NKSL_LOG(...)  logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...)  logger_src.Errorf("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
using threading::NkScopedLock;

// =============================================================================
// Hachage FNV-1a 64-bit
// =============================================================================
static uint64 FNV1a64(const void* data, size_t len, uint64 seed = 0xcbf29ce484222325ULL) {
    const uint8* p = static_cast<const uint8*>(data);
    uint64 h = seed;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
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
    entry.isSpirv    = (t == NkSLTarget::NK_SPIRV);
    mEntries.PushBack(entry);
}

void NkSLCache::Clear() { NkScopedLock lock(mMutex); mEntries.Clear(); }

void NkSLCache::Flush() {
    if (mCacheDir.Empty()) return;
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
            fread(buf.Data(), 1, srcLen, f); buf[srcLen] = '\0';
            e.source = NkString(buf.Data());
        }
        uint32 bcLen=0; fread(&bcLen, sizeof(uint32), 1, f);
        if (bcLen) { e.bytecode.Resize(bcLen); fread(e.bytecode.Data(), 1, bcLen, f); }
        mEntries.PushBack(e);
    }
    fclose(f);
    NKSL_LOG("Cache loaded: %u entries from %s\n", cnt, path.CStr());
}

// =============================================================================
// NkSLCompiler
// =============================================================================
NkSLCompiler::NkSLCompiler(const NkString& cacheDir) : mCache(cacheDir) {}

void NkSLCompiler::SetCacheDir(const NkString& dir) { mCache.SetDirectory(dir); }

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
// Compile — point d'entrée principal
// =============================================================================
NkSLCompileResult NkSLCompiler::Compile(
    const NkString&          source,
    NkSLStage                stage,
    NkSLTarget               target,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    // 1. Cache
    if (mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        NkSLCacheEntry cached;
        if (mCache.Get(h, target, stage, cached)) {
            NkSLCompileResult res;
            res.success = true; res.source = cached.source;
            res.bytecode = cached.bytecode; res.target = target; res.stage = stage;
            NKSL_LOG("Cache hit: %s stage=%s target=%s\n",
                     filename.CStr(), NkSLStageName(stage), NkSLTargetName(target));
            return res;
        }
    }

    // 2. Prétraitement
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        NkSLCompileResult res; res.success=false; res.errors=ppErrors;
        res.target=target; res.stage=stage; return res;
    }

    // 3. Lexer → Parser → AST
    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (parser.HasErrors()) {
        NkSLCompileResult res; res.success=false; res.errors=parser.GetErrors();
        res.target=target; res.stage=stage; delete ast;
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u: %s\n", filename.CStr(), e.line, e.message.CStr());
        return res;
    }

    // 4. Génération
    NkSLCompileResult res;
    switch (target) {
        case NkSLTarget::NK_GLSL: {
            NkSLCodeGenGLSL gen; res = gen.Generate(ast, stage, opts); break;
        }
        case NkSLTarget::NK_SPIRV: {
            NkSLCodeGenGLSL glslGen;
            NkSLCompileOptions glslOpts = opts; glslOpts.glslVersion = 450;
            NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
            if (!glslRes.success) { res = glslRes; break; }
            res = CompileToSPIRV(glslRes.source, stage, opts);
            if (!res.success) {
                NKSL_ERR("GLSL→SPIR-V failed, returning GLSL text\n");
                res = glslRes; res.target = NkSLTarget::NK_GLSL;
            }
            break;
        }
        case NkSLTarget::NK_HLSL: {
            NkSLCodeGenHLSL gen; res = gen.Generate(ast, stage, opts); break;
        }
        case NkSLTarget::NK_MSL: {
            // Choisir entre le générateur natif et SPIRV-Cross selon les options
            if (opts.preferSpirvCrossForMSL) {
                NkSLCodeGenMSLSpirvCross gen;
                res = gen.Generate(ast, stage, opts);
                // Si SPIRV-Cross échoue, fallback vers le générateur natif
                if (!res.success) {
                    NKSL_LOG("SPIRV-Cross MSL failed, falling back to native MSL\n");
                    NkSLCodeGen_MSL nativeGen;
                    res = nativeGen.Generate(ast, stage, opts);
                }
            } else {
                NkSLCodeGen_MSL gen; res = gen.Generate(ast, stage, opts);
            }
            break;
        }
        case NkSLTarget::NK_MSL_SPIRV_CROSS: {
            NkSLCodeGenMSLSpirvCross gen; res = gen.Generate(ast, stage, opts); break;
        }
        case NkSLTarget::NK_CPLUSPLUS: {
            NkSLCodeGenCPP gen; res = gen.Generate(ast, stage, opts); break;
        }
        default:
            res.success = false; res.AddError(0, "Unsupported target"); break;
    }

    delete ast;

    // 5. Cache
    if (res.success && mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, res);
    }

    if (!res.success)
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u: %s\n", filename.CStr(), e.line, e.message.CStr());
    else
        NKSL_LOG("Compiled %s stage=%s target=%s (%zu bytes)\n",
                 filename.CStr(), NkSLStageName(stage), NkSLTargetName(target),
                 (size_t)res.bytecode.Size());

    return res;
}

// =============================================================================
// CompileWithReflection — compile + reflection en une seule passe
// =============================================================================
NkSLCompileResultWithReflection NkSLCompiler::CompileWithReflection(
    const NkString&          source,
    NkSLStage                stage,
    NkSLTarget               target,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkSLCompileResultWithReflection out;

    // Prétraitement + parsing pour avoir l'AST
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        out.result.success = false; out.result.errors = ppErrors;
        return out;
    }

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (parser.HasErrors()) {
        out.result.success = false; out.result.errors = parser.GetErrors();
        delete ast; return out;
    }

    // Reflection depuis l'AST (avant la génération de code)
    NkSLReflector reflector;
    out.reflection    = reflector.Reflect(ast, stage);
    out.hasReflection = true;

    // Génération de code (réutilise l'AST déjà parsé)
    switch (target) {
        case NkSLTarget::NK_GLSL: {
            NkSLCodeGenGLSL gen;
            out.result = gen.Generate(ast, stage, opts); break;
        }
        case NkSLTarget::NK_SPIRV: {
            NkSLCodeGenGLSL glslGen;
            NkSLCompileOptions glslOpts = opts; glslOpts.glslVersion = 450;
            NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
            if (!glslRes.success) { out.result = glslRes; break; }
            out.result = CompileToSPIRV(glslRes.source, stage, opts);
            if (!out.result.success) { out.result = glslRes; out.result.target = NkSLTarget::NK_GLSL; }
            break;
        }
        case NkSLTarget::NK_HLSL: {
            NkSLCodeGenHLSL gen;
            out.result = gen.Generate(ast, stage, opts); break;
        }
        case NkSLTarget::NK_MSL:
        case NkSLTarget::NK_MSL_SPIRV_CROSS: {
            if (opts.preferSpirvCrossForMSL || target == NkSLTarget::NK_MSL_SPIRV_CROSS) {
                NkSLCodeGenMSLSpirvCross gen;
                out.result = gen.Generate(ast, stage, opts);
                if (!out.result.success) {
                    NkSLCodeGen_MSL nativeGen;
                    out.result = nativeGen.Generate(ast, stage, opts);
                }
            } else {
                NkSLCodeGen_MSL gen;
                out.result = gen.Generate(ast, stage, opts);
            }
            break;
        }
        case NkSLTarget::NK_CPLUSPLUS: {
            NkSLCodeGenCPP gen;
            out.result = gen.Generate(ast, stage, opts); break;
        }
        default:
            out.result.success = false; out.result.AddError(0, "Unsupported target"); break;
    }

    delete ast;

    if (out.result.success && mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, out.result);
    }

    return out;
}

// =============================================================================
// Reflect — reflection seule sans génération de code
// =============================================================================
NkSLReflection NkSLCompiler::Reflect(
    const NkString& source,
    NkSLStage       stage,
    const NkString& filename)
{
    NkSLReflection empty;

    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) return empty;

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    if (parser.HasErrors()) { delete ast; return empty; }

    NkSLReflector reflector;
    NkSLReflection result = reflector.Reflect(ast, stage);
    delete ast;
    return result;
}

// =============================================================================
// CompileAllTargets
// =============================================================================
NkSLCompiler::MultiTargetResult NkSLCompiler::CompileAllTargets(
    const NkString&           source,
    NkSLStage                 stage,
    const NkVector<NkSLTarget>& targets,
    const NkSLCompileOptions&  opts,
    const NkString&            filename)
{
    MultiTargetResult multi;
    // Reflection partagée — parsé une seule fois
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        for (auto& t : targets) {
            NkSLCompileResult r; r.success=false; r.errors=ppErrors; r.target=t; r.stage=stage;
            multi.results.PushBack(r);
        }
        return multi;
    }

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (!parser.HasErrors()) {
        NkSLReflector reflector;
        multi.reflection = reflector.Reflect(ast, stage);
    }

    delete ast;

    // Compiler chaque target indépendamment
    for (auto t : targets) {
        multi.results.PushBack(Compile(source, stage, t, opts, filename));
    }
    return multi;
}

// =============================================================================
// CompileToSPIRV
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileToSPIRV(
    const NkString&          glslSource,
    NkSLStage                stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::NK_SPIRV; res.stage = stage;

#ifdef NKSL_HAS_SHADERC
    if (GLSLtoSPIRV_shaderc(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true; return res;
    }
    res.errors.Clear(); // reset errors pour essayer glslang
#endif

#ifdef NKSL_HAS_GLSLANG
    if (GLSLtoSPIRV_glslang(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true; return res;
    }
#endif

    res.success = false;
    res.AddError(0,
        "No SPIR-V compiler available.\n"
        "To embed glslang (recommended, no Vulkan SDK needed):\n"
        "  git submodule add https://github.com/KhronosGroup/glslang third_party/glslang\n"
        "  Then build with -DNKSL_HAS_GLSLANG=1\n"
        "Alternatively install Vulkan SDK and build with -DNKSL_HAS_SHADERC=1");
    return res;
}

// =============================================================================
// CompileToMSL_SpirvCross
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileToMSL_SpirvCross(
    const NkString&          source,
    NkSLStage                stage,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkSLCompileOptions mslOpts = opts;
    mslOpts.preferSpirvCrossForMSL = true;
    return Compile(source, stage, NkSLTarget::NK_MSL_SPIRV_CROSS, mslOpts, filename);
}

// =============================================================================
// FillShaderDesc
// =============================================================================
bool NkSLCompiler::FillShaderDesc(
    const NkString&          nkslSource,
    NkSLStage                stage,
    NkSLTarget               targetApi,
    NkShaderDesc&            outDesc,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkSLReflection dummy;
    return FillShaderDescWithReflection(nkslSource, stage, targetApi, outDesc, dummy, opts, filename);
}

// =============================================================================
// FillShaderDescWithReflection — remplit NkShaderDesc + reflection
// =============================================================================
bool NkSLCompiler::FillShaderDescWithReflection(
    const NkString&          nkslSource,
    NkSLStage                stage,
    NkSLTarget               targetApi,
    NkShaderDesc&            outDesc,
    NkSLReflection&          outReflection,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    auto compiled = CompileWithReflection(nkslSource, stage, targetApi, opts, filename);

    if (!compiled.result.success) {
        NKSL_ERR("FillShaderDescWithReflection: compilation failed for %s\n", filename.CStr());
        return false;
    }

    outReflection = compiled.reflection;

    NkShaderStage rhiStage;
    switch (stage) {
        case NkSLStage::NK_VERTEX:       rhiStage = NkShaderStage::NK_VERTEX;    break;
        case NkSLStage::NK_FRAGMENT:     rhiStage = NkShaderStage::NK_FRAGMENT;  break;
        case NkSLStage::NK_GEOMETRY:     rhiStage = NkShaderStage::NK_GEOMETRY;  break;
        case NkSLStage::NK_TESS_CONTROL: rhiStage = NkShaderStage::NK_TESS_CTRL; break;
        case NkSLStage::NK_TESS_EVAL:    rhiStage = NkShaderStage::NK_TESS_EVAL; break;
        case NkSLStage::NK_COMPUTE:      rhiStage = NkShaderStage::NK_COMPUTE;   break;
        default: rhiStage = NkShaderStage::NK_VERTEX;
    }

    NkShaderStageDesc sd{};
    sd.stage      = rhiStage;
    sd.entryPoint = opts.entryPoint.CStr();

    switch (targetApi) {
        case NkSLTarget::NK_SPIRV:
            sd.spirvBinary.Resize((uint64)compiled.result.bytecode.Size());
            memcpy(sd.spirvBinary.Data(), compiled.result.bytecode.Data(), (size_t)compiled.result.bytecode.Size());
            break;
        case NkSLTarget::NK_GLSL:
            sd.glslSource = compiled.result.source.CStr();
            break;
        case NkSLTarget::NK_HLSL:
            sd.hlslSource = compiled.result.source.CStr();
            break;
        case NkSLTarget::NK_MSL:
        case NkSLTarget::NK_MSL_SPIRV_CROSS:
            sd.mslSource = compiled.result.source.CStr();
            break;
        default: break;
    }

    outDesc.AddStage(sd);
    return true;
}

// =============================================================================
// Validate
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
// Preprocess
// =============================================================================
NkString NkSLCompiler::Preprocess(const NkString& source,
                                    const NkString& baseDir,
                                    NkVector<NkSLCompileError>* errors)
{
    NkString result;
    const char* p = source.CStr();
    uint32 line = 1;

    while (*p) {
        const char* lineStart = p;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            NkString directive;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                directive += *p++;

            if (directive == "include") {
                while (*p == ' ' || *p == '\t') p++;
                char delim = *p++; char closeDelim = (delim == '"') ? '"' : '>';
                NkString fname;
                while (*p && *p != closeDelim && *p != '\n') fname += *p++;
                if (*p == closeDelim) p++;
                NkString fullPath = baseDir.Empty() ? fname : (baseDir + "/" + fname);
                FILE* f = fopen(fullPath.CStr(), "r");
                if (f) {
                    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                    NkVector<char> buf; buf.Resize(sz + 1);
                    fread(buf.Data(), 1, sz, f); buf[sz] = '\0'; fclose(f);
                    NkString included = Preprocess(NkString(buf.Data()), baseDir, errors);
                    result += "// #include \"" + fname + "\"\n";
                    result += included + "\n";
                } else {
                    if (errors)
                        errors->PushBack({line, 0, "", "#include file not found: " + fname, false});
                }
            } else if (directive == "pragma" || directive == "version" || directive == "extension") {
                while (*p && *p != '\n') p++;
            } else {
                result += '#'; result += directive;
                while (*p && *p != '\n') result += *p++;
            }
        } else {
            while (*p && *p != '\n') result += *p++;
        }

        if (*p == '\n') { result += '\n'; p++; line++; }
        else if (*p == '\r') { p++; if (*p == '\n') { result += '\n'; p++; } line++; }
    }
    return result;
}

// =============================================================================
// CompileWithSemantic
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileWithSemantic(
    const NkString&          source,
    NkSLStage                stage,
    NkSLTarget               target,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        NkSLCompileResult res; res.success=false; res.errors=ppErrors; return res;
    }

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (parser.HasErrors()) {
        NkSLCompileResult res; res.success=false; res.errors=parser.GetErrors();
        delete ast; return res;
    }

    NkSLSemantic semantic(stage);
    NkSLSemanticResult semResult = semantic.Analyze(ast);

    if (!semResult.success) {
        NkSLCompileResult res; res.success=false;
        res.errors=semResult.errors; res.warnings=semResult.warnings;
        delete ast;
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u [semantic]: %s\n", filename.CStr(), e.line, e.message.CStr());
        return res;
    }

    NkSLCompileResult res;
    switch (target) {
        case NkSLTarget::NK_GLSL: { NkSLCodeGenGLSL gen; res = gen.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_SPIRV: {
            NkSLCodeGenGLSL glslGen;
            NkSLCompileOptions glslOpts = opts; glslOpts.glslVersion = 450;
            NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
            if (!glslRes.success) { res = glslRes; break; }
            res = CompileToSPIRV(glslRes.source, stage, opts);
            if (!res.success) { res = glslRes; res.target = NkSLTarget::NK_GLSL; }
            break;
        }
        case NkSLTarget::NK_HLSL: { NkSLCodeGenHLSL gen; res = gen.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_MSL:
        case NkSLTarget::NK_MSL_SPIRV_CROSS: {
            if (opts.preferSpirvCrossForMSL) {
                NkSLCodeGenMSLSpirvCross gen; res = gen.Generate(ast, stage, opts);
                if (!res.success) { NkSLCodeGen_MSL ng; res = ng.Generate(ast, stage, opts); }
            } else { NkSLCodeGen_MSL gen; res = gen.Generate(ast, stage, opts); }
            break;
        }
        case NkSLTarget::NK_CPLUSPLUS: { NkSLCodeGenCPP gen; res = gen.Generate(ast, stage, opts); break; }
        default: res.success=false; res.AddError(0, "Unsupported target"); break;
    }

    delete ast;
    for (auto& w : semResult.warnings) res.warnings.PushBack(w);

    if (res.success && mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, res);
    }
    return res;
}

// =============================================================================
// GLSLtoSPIRV_shaderc (Vulkan SDK optionnel)
// =============================================================================
#ifdef NKSL_HAS_SHADERC
bool NkSLCompiler::GLSLtoSPIRV_shaderc(
    const NkString& glsl, NkSLStage stage, const NkSLCompileOptions& opts,
    NkVector<uint8>& spirvOut, NkVector<NkSLCompileError>& errors)
{
    shaderc_shader_kind kind;
    switch (stage) {
        case NkSLStage::NK_VERTEX:       kind = shaderc_vertex_shader;           break;
        case NkSLStage::NK_FRAGMENT:     kind = shaderc_fragment_shader;         break;
        case NkSLStage::NK_GEOMETRY:     kind = shaderc_geometry_shader;         break;
        case NkSLStage::NK_TESS_CONTROL: kind = shaderc_tess_control_shader;     break;
        case NkSLStage::NK_TESS_EVAL:    kind = shaderc_tess_evaluation_shader;  break;
        case NkSLStage::NK_COMPUTE:      kind = shaderc_compute_shader;          break;
        default: kind = shaderc_vertex_shader;
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions shadercOpts;
    shadercOpts.SetTargetEnvironment(shaderc_target_env_vulkan,
        opts.vulkanVersion >= 120 ? shaderc_env_version_vulkan_1_2 :
        opts.vulkanVersion >= 110 ? shaderc_env_version_vulkan_1_1 :
                                    shaderc_env_version_vulkan_1_0);
    if (opts.optimize)  shadercOpts.SetOptimizationLevel(shaderc_optimization_level_performance);
    if (opts.debugInfo) shadercOpts.SetGenerateDebugInfo();
    auto result = compiler.CompileGlslToSpv(glsl.CStr(), kind, "nksl_shader", shadercOpts);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        errors.PushBack({0, 0, "", NkString(result.GetErrorMessage()), true}); return false;
    }
    size_t wordCount = result.cend() - result.cbegin();
    spirvOut.Resize(wordCount * sizeof(uint32));
    memcpy(spirvOut.Data(), result.cbegin(), wordCount * sizeof(uint32));
    NKSL_LOG("shaderc: SPIR-V generated (%zu words)\n", wordCount); return true;
}
#else
bool NkSLCompiler::GLSLtoSPIRV_shaderc(const NkString&, NkSLStage,
    const NkSLCompileOptions&, NkVector<uint8>&, NkVector<NkSLCompileError>&)
{ return false; }
#endif

// =============================================================================
// GLSLtoSPIRV_glslang (embarqué standalone — RECOMMANDÉ)
// =============================================================================
#ifdef NKSL_HAS_GLSLANG
bool NkSLCompiler::GLSLtoSPIRV_glslang(
    const NkString& glsl, NkSLStage stage, const NkSLCompileOptions& opts,
    NkVector<uint8>& spirvOut, NkVector<NkSLCompileError>& errors)
{
    static bool initialized = false;
    if (!initialized) { glslang::InitializeProcess(); initialized = true; }

    EShLanguage lang;
    switch (stage) {
        case NkSLStage::NK_VERTEX:       lang = EShLangVertex;          break;
        case NkSLStage::NK_FRAGMENT:     lang = EShLangFragment;        break;
        case NkSLStage::NK_GEOMETRY:     lang = EShLangGeometry;        break;
        case NkSLStage::NK_TESS_CONTROL: lang = EShLangTessControl;     break;
        case NkSLStage::NK_TESS_EVAL:    lang = EShLangTessEvaluation;  break;
        case NkSLStage::NK_COMPUTE:      lang = EShLangCompute;         break;
        default: lang = EShLangVertex;
    }

    glslang::TShader shader(lang);
    const char* src = glsl.CStr();
    shader.setStrings(&src, 1);

    int glslVer = (int)opts.glslVersion;
    if (glslVer < 450) glslVer = 450;

    shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
    glslang::EshTargetClientVersion clientVer =
        opts.vulkanVersion >= 120 ? glslang::EShTargetVulkan_1_2 :
        opts.vulkanVersion >= 110 ? glslang::EShTargetVulkan_1_1 :
                                    glslang::EShTargetVulkan_1_0;
    shader.setEnvClient(glslang::EShClientVulkan, clientVer);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    EShMessages msgs = EShMsgSpvRules;
    if (opts.debugInfo) msgs = (EShMessages)(msgs | EShMsgDebugInfo);

    // GetDefaultResources() fourni par glslang-default-resource-limits
    if (!shader.parse(GetDefaultResources(), glslVer, false, msgs)) {
        errors.PushBack({0, 0, "", NkString(shader.getInfoLog()), true});
        NKSL_ERR("glslang parse: %s\n", shader.getInfoLog()); return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(msgs)) {
        errors.PushBack({0, 0, "", NkString(program.getInfoLog()), true});
        NKSL_ERR("glslang link: %s\n", program.getInfoLog()); return false;
    }

    std::vector<uint32> spv;
    spv::SpvBuildLogger spvLogger;
    glslang::SpvOptions spvOpts;
    spvOpts.generateDebugInfo = opts.debugInfo;
    spvOpts.disableOptimizer  = !opts.optimize;
    glslang::GlslangToSpv(*program.getIntermediate(lang), spv, &spvLogger, &spvOpts);

    if (!spvLogger.getAllMessages().empty())
        NKSL_LOG("glslang SPV: %s\n", spvLogger.getAllMessages().c_str());

    spirvOut.Resize(spv.size() * sizeof(uint32));
    memcpy(spirvOut.Data(), spv.data(), spv.size() * sizeof(uint32));
    NKSL_LOG("glslang: SPIR-V generated (%zu words)\n", spv.size()); return true;
}
#else
bool NkSLCompiler::GLSLtoSPIRV_glslang(const NkString&, NkSLStage,
    const NkSLCompileOptions&, NkVector<uint8>&, NkVector<NkSLCompileError>&)
{ return false; }
#endif

// =============================================================================
// NkSLShaderLibrary
// =============================================================================
NkSLShaderLibrary::NkSLShaderLibrary(NkSLCompiler* compiler, const NkString& baseDir)
    : mCompiler(compiler), mBaseDir(baseDir) {}

bool NkSLShaderLibrary::Register(const NkString& name, const NkString& filePath,
                                   const NkVector<NkSLStage>& stages) {
    NkString fullPath = mBaseDir.Empty() ? filePath : (mBaseDir + "/" + filePath);
    FILE* f = fopen(fullPath.CStr(), "r");
    if (!f) { NKSL_ERR("ShaderLibrary: cannot open '%s'\n", fullPath.CStr()); return false; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkVector<char> buf; buf.Resize(sz + 1);
    fread(buf.Data(), 1, sz, f); buf[sz] = '\0'; fclose(f);
    ShaderEntry entry;
    entry.name=name; entry.sourcePath=fullPath; entry.source=NkString(buf.Data());
    entry.stages=stages; entry.dirty=true;
    mEntries.PushBack(entry); return true;
}

bool NkSLShaderLibrary::RegisterInline(const NkString& name, const NkString& source,
                                        const NkVector<NkSLStage>& stages) {
    ShaderEntry entry;
    entry.name=name; entry.source=source; entry.stages=stages; entry.dirty=true;
    mEntries.PushBack(entry); return true;
}

bool NkSLShaderLibrary::CompileAll(NkSLTarget target, const NkSLCompileOptions& opts) {
    bool allOk = true;
    for (auto& e : mEntries) {
        if (!e.dirty) continue;
        e.compiled.Clear();
        bool firstStage = true;
        for (auto stage : e.stages) {
            auto compiled = mCompiler->CompileWithReflection(e.source, stage, target, opts, e.name);
            e.compiled.PushBack(compiled.result);
            // Stocker la reflection du premier stage (généralement vertex)
            if (firstStage && compiled.hasReflection) {
                e.reflection = compiled.reflection; firstStage = false;
            }
            if (!compiled.result.success) {
                allOk = false;
                NKSL_ERR("ShaderLibrary: '%s' stage=%s FAILED\n", e.name.CStr(), NkSLStageName(stage));
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

const NkSLReflection* NkSLShaderLibrary::GetReflection(const NkString& name) const {
    for (auto& e : mEntries) {
        if (e.name == name) return &e.reflection;
    }
    return nullptr;
}

bool NkSLShaderLibrary::FillShaderDesc(const NkString& name, NkSLTarget target,
                                        NkShaderDesc& outDesc,
                                        const NkSLCompileOptions& opts) const {
    NkSLReflection dummy;
    return FillShaderDescWithReflection(name, target, outDesc, dummy, opts);
}

bool NkSLShaderLibrary::FillShaderDescWithReflection(const NkString& name,
                                                       NkSLTarget target,
                                                       NkShaderDesc& outDesc,
                                                       NkSLReflection& outReflection,
                                                       const NkSLCompileOptions& opts) const {
    for (auto& e : mEntries) {
        if (e.name != name) continue;
        outReflection = e.reflection;
        for (uint32 i = 0; i < (uint32)e.stages.Size(); i++) {
            auto stage = e.stages[i];
            const NkSLCompileResult* res = (i < (uint32)e.compiled.Size()) ? &e.compiled[i] : nullptr;
            NkSLCompileResult compiled;
            if (!res || !res->success) {
                compiled = const_cast<NkSLCompiler*>(mCompiler)->Compile(e.source, stage, target, opts, name);
                if (!compiled.success) return false;
                res = &compiled;
            }
            NkShaderStageDesc sd{};
            sd.entryPoint = opts.entryPoint.CStr();
            switch (stage) {
                case NkSLStage::NK_VERTEX:   sd.stage = NkShaderStage::NK_VERTEX;   break;
                case NkSLStage::NK_FRAGMENT: sd.stage = NkShaderStage::NK_FRAGMENT; break;
                case NkSLStage::NK_COMPUTE:  sd.stage = NkShaderStage::NK_COMPUTE;  break;
                default: sd.stage = NkShaderStage::NK_VERTEX;
            }
            if (target == NkSLTarget::NK_GLSL)  sd.glslSource = res->source.CStr();
            if (target == NkSLTarget::NK_HLSL)  sd.hlslSource = res->source.CStr();
            if (target == NkSLTarget::NK_MSL || target == NkSLTarget::NK_MSL_SPIRV_CROSS)
                sd.mslSource = res->source.CStr();
            if (target == NkSLTarget::NK_SPIRV) {
                sd.spirvBinary.Resize((uint64)res->bytecode.Size());
                memcpy(sd.spirvBinary.Data(), res->bytecode.Data(), (size_t)res->bytecode.Size());
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
        FILE* f = fopen(e.sourcePath.CStr(), "r");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        NkVector<char> buf; buf.Resize(sz+1);
        fread(buf.Data(), 1, sz, f); buf[sz]='\0'; fclose(f);
        NkString newSrc(buf.Data());
        if (newSrc != e.source) { e.source=newSrc; e.dirty=true; NKSL_LOG("HotReload: '%s' changed\n", e.name.CStr()); }
        if (e.dirty) { CompileAll(target, opts); reloaded++; }
    }
    return reloaded;
}

} // namespace nkentseu
