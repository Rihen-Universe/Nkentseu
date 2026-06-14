// =============================================================================
// NkSLCompiler.cpp  — v4.0
//
// CORRECTIONS v4.0 :
//   1. Cache O(1) — NkUnorderedMap<uint64, NkSLCacheEntry> au lieu de NkVector
//   2. glslang::InitializeProcess() protégé par std::call_once (thread-safe)
//   3. HashSource() inclut glslVulkanVersion + hlslShaderModelDX11/DX12
//   4. Dispatch Compile() vers NK_GLSL_VULKAN, NK_HLSL_DX11, NK_HLSL_DX12
//   5. CompileAllTargets() compatible nouveaux targets
//   6. FillShaderDesc() compatible nouveaux targets
// =============================================================================
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/Frontend/NkSLLexer.h"
#include "NKSL/Frontend/NkSLParser.h"
#include "NKSL/CodeGen/Bytecode/NkSLCodeGenBytecode.h"  // cible NK_BYTECODE
#include "NKSL/VM/NkSLByteCodeIO.h"                      // sérialisation .nkbc
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"   // cache via NKFileSystem (pas de fopen CRT)
#include <cstdio>
#include <cstring>
#include <mutex>      // std::call_once, std::once_flag

// glslang embarqué (standalone, sans Vulkan SDK)
#ifdef NKSL_HAS_GLSLANG
#  include <glslang/Public/ShaderLang.h>
#  include <glslang/Public/ResourceLimits.h>
#  include <glslang/SPIRV/GlslangToSpv.h>
#endif

// shaderc optionnel (Vulkan SDK)
#ifdef NKSL_HAS_SHADERC
#  include <shaderc/shaderc.hpp>
#endif

#define NKSL_LOG(...)  logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...)  logger_src.Errorf("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
using threading::NkScopedLockMutex;

// =============================================================================
// RAII : libère l'AST sur TOUS les chemins de sortie (retours anticipés ET
// exceptions des codegens / allocations). NkSLNode::~NkSLNode() détruit
// récursivement ses enfants, donc supprimer la racine libère tout l'arbre.
// =============================================================================
namespace {
    struct NkSLAstScope {
        NkSLProgramNode* ast = nullptr;
        explicit NkSLAstScope(NkSLProgramNode* a) noexcept : ast(a) {}
        ~NkSLAstScope() { delete ast; }
        NkSLAstScope(const NkSLAstScope&)            = delete;
        NkSLAstScope& operator=(const NkSLAstScope&) = delete;
    };
} // namespace

// =============================================================================
// Hachage FNV-1a 64-bit
// =============================================================================
static uint64 FNV1a64(const void* data, size_t len,
                       uint64 seed = 0xcbf29ce484222325ULL) {
    const uint8* p = static_cast<const uint8*>(data);
    uint64 h = seed;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

// =============================================================================
// NkSLCache — CORRECTION : NkUnorderedMap → recherche O(1)
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
    NkScopedLockMutex lock(mMutex);
    return mEntries.Find(MakeKey(hash, t, s)) != nullptr;
}

bool NkSLCache::Get(uint64 hash, NkSLTarget t, NkSLStage s,
                    NkSLCacheEntry& out) const {
    NkScopedLockMutex lock(mMutex);
    auto* p = mEntries.Find(MakeKey(hash, t, s));
    if (!p) return false;
    out = *p;
    return true;
}

void NkSLCache::Put(uint64 hash, NkSLTarget t, NkSLStage s,
                    const NkSLCompileResult& r) {
    NkScopedLockMutex lock(mMutex);
    uint64 k = MakeKey(hash, t, s);
    NkSLCacheEntry entry;
    entry.sourceHash = hash; entry.target = t; entry.stage = s;
    entry.bytecode   = r.bytecode; entry.source = r.source;
    entry.isSpirv    = (t == NkSLTarget::NK_SPIRV);
    mEntries[k] = entry;
}

void NkSLCache::Clear() {
    NkScopedLockMutex lock(mMutex);
    mEntries.Clear();
}

void NkSLCache::Flush() {
    NkScopedLockMutex lock(mMutex);
    if (mCacheDir.Empty()) return;
    NkString path = mCacheDir + "/nksl.cache";
    // Sérialisation binaire en mémoire puis écriture via NKFileSystem (pas de fopen).
    NkVector<nk_uint8> buf;
    auto wr = [&](const void* p, usize n) {
        const nk_uint8* b = (const nk_uint8*)p;
        for (usize i = 0; i < n; ++i) buf.PushBack(b[i]);
    };
    uint32 count = (uint32)mEntries.Size();
    wr(&count, sizeof(count));
    mEntries.ForEach([&](uint64 /*key*/, NkSLCacheEntry& e) {
        wr(&e.sourceHash, sizeof(e.sourceHash));
        wr(&e.target,     sizeof(e.target));
        wr(&e.stage,      sizeof(e.stage));
        wr(&e.isSpirv,    sizeof(e.isSpirv));
        uint32 bsz = (uint32)e.bytecode.Size();
        wr(&bsz, sizeof(bsz));
        if (bsz) wr(e.bytecode.Data(), bsz);
        uint32 ssz = (uint32)e.source.Size();
        wr(&ssz, sizeof(ssz));
        if (ssz) wr(e.source.CStr(), ssz);
    });
    NkFile::WriteAllBytes(path.CStr(), buf);
}

void NkSLCache::Load() {
    NkScopedLockMutex lock(mMutex);
    NkString path = mCacheDir + "/nksl.cache";
    if (!NkFile::Exists(path.CStr())) return;
    NkVector<nk_uint8> buf = NkFile::ReadAllBytes(path.CStr());
    usize off = 0;
    auto rd = [&](void* p, usize n) -> bool {
        if (off + n > buf.Size()) return false;
        memcpy(p, buf.Data() + off, n); off += n; return true;
    };
    uint32 count = 0;
    if (!rd(&count, sizeof(count))) return;
    for (uint32 i = 0; i < count; i++) {
        NkSLCacheEntry e;
        if (!rd(&e.sourceHash, sizeof(e.sourceHash))) break;
        if (!rd(&e.target,     sizeof(e.target)))     break;
        if (!rd(&e.stage,      sizeof(e.stage)))      break;
        if (!rd(&e.isSpirv,    sizeof(e.isSpirv)))    break;
        uint32 bsz = 0; if (!rd(&bsz, sizeof(bsz))) break;
        if (bsz) { e.bytecode.Resize(bsz); if (!rd(e.bytecode.Data(), bsz)) break; }
        uint32 ssz = 0; if (!rd(&ssz, sizeof(ssz))) break;
        if (ssz) {
            NkVector<char> sbuf; sbuf.Resize(ssz+1);
            if (!rd(sbuf.Data(), ssz)) break;
            sbuf[ssz] = '\0';
            e.source = NkString(sbuf.Data());
        }
        uint64 k = MakeKey(e.sourceHash, e.target, e.stage);
        mEntries[k] = e;
    }
}

// =============================================================================
// NkSLCompiler
// =============================================================================
NkSLCompiler::NkSLCompiler(const NkString& cacheDir) : mCache(cacheDir) {}

void NkSLCompiler::SetCacheDir(const NkString& dir) { mCache.SetDirectory(dir); }

// Hash inclut maintenant tous les paramètres différenciant les targets
uint64 NkSLCompiler::HashSource(const NkString& src, NkSLStage stage,
                                  NkSLTarget target,
                                  const NkSLCompileOptions& opts) const {
    uint64 h = FNV1a64(src.CStr(), src.Size());
    h = FNV1a64(&stage,                     sizeof(stage), h);
    h = FNV1a64(&target,                    sizeof(target), h);
    h = FNV1a64(&opts.glslVersion,          sizeof(uint32), h);
    h = FNV1a64(&opts.glslVulkanVersion,    sizeof(uint32), h);
    h = FNV1a64(&opts.hlslShaderModelDX11,  sizeof(uint32), h);
    h = FNV1a64(&opts.hlslShaderModelDX12,  sizeof(uint32), h);
    h = FNV1a64(&opts.mslVersion,           sizeof(uint32), h);
    h = FNV1a64(&opts.dx12DefaultSpace,     sizeof(uint32), h);
    h = FNV1a64(&opts.dx12BindlessHeap,     sizeof(bool),   h);
    h = FNV1a64(&opts.vkDrawParams,         sizeof(bool),   h);
    return h;
}

// =============================================================================
// Compile — point d'entrée principal
// =============================================================================
NkSLCompileResult NkSLCompiler::Compile(
    const NkString&           source,
    NkSLStage                 stage,
    NkSLTarget                target,
    const NkSLCompileOptions& opts,
    const NkString&           filename)
{
    // 1. Cache O(1)
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
        NkSLCompileResult res; res.success = false;
        res.errors = ppErrors; res.target = target; res.stage = stage;
        return res;
    }

    // 3. Lexer → Parser → AST
    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    NkSLAstScope astScope(ast); // libération garantie (retours + exceptions)

    if (parser.HasErrors()) {
        NkSLCompileResult res; res.success = false;
        res.errors = parser.GetErrors(); res.target = target; res.stage = stage;
        for (auto& e : res.errors)
            NKSL_ERR("%s:%u: %s\n", filename.CStr(), e.line, e.message.CStr());
        return res;
    }

    // 3.5. Diagnostics sémantiques NON bloquants.
    // L'analyseur détecte déjà variables non déclarées, mismatch de type/retour,
    // swizzles invalides, etc. (NkSLSemantic). On les remonte en WARNINGS sans
    // jamais faire échouer un shader qui compilait : l'analyse reste permissive
    // par endroits (overloads, conversions implicites) et on évite toute
    // régression sur les shaders existants. Pour une validation BLOQUANTE,
    // utiliser CompileWithSemantic().
    NkSLSemantic semDiag(stage);
    NkSLSemanticResult semRes = semDiag.Analyze(ast);

    // 4. Dispatch par target
    NkSLCompileResult res;
    switch (target) {

        // ── GLSL OpenGL ────────────────────────────────────────────────────────
        case NkSLTarget::NK_GLSL: {
            NkSLCodeGenGLSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }

        // ── GLSL Vulkan (NOUVEAU) ──────────────────────────────────────────────
        case NkSLTarget::NK_GLSL_VULKAN: {
            NkSLCodeGenGLSLVulkan gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }

        // ── SPIR-V via GLSL Vulkan → glslang ──────────────────────────────────
        case NkSLTarget::NK_SPIRV: {
            // Générer d'abord le GLSL Vulkan (set/binding corrects)
            NkSLCodeGenGLSLVulkan vkGen;
            NkSLCompileResult glslRes = vkGen.Generate(ast, stage, opts);
            if (!glslRes.success) { res = glslRes; break; }
            res = CompileToSPIRV(glslRes.source, stage, opts);
            if (!res.success) {
                NKSL_ERR("GLSL-Vulkan→SPIR-V failed, returning GLSL-Vulkan text\n");
                res = glslRes; res.target = NkSLTarget::NK_GLSL_VULKAN;
            }
            break;
        }

        // ── HLSL DirectX 11 / SM5 (renommé, était NK_HLSL) ────────────────────
        case NkSLTarget::NK_HLSL_DX11: {
            NkSLCodeGenHLSL_DX11 gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }

        // ── HLSL DirectX 12 / SM6 (NOUVEAU) ───────────────────────────────────
        case NkSLTarget::NK_HLSL_DX12: {
            NkSLCodeGenHLSL_DX12 gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }

        // ── MSL natif ─────────────────────────────────────────────────────────
        case NkSLTarget::NK_MSL: {
            if (opts.preferSpirvCrossForMSL) {
                NkSLCodeGenMSLSpirvCross scGen;
                res = scGen.Generate(ast, stage, opts);
                if (!res.success) {
                    NKSL_LOG("SPIRV-Cross MSL failed, falling back to native\n");
                    NkSLCodeGen_MSL nativeGen;
                    res = nativeGen.Generate(ast, stage, opts);
                }
            } else {
                NkSLCodeGen_MSL gen;
                res = gen.Generate(ast, stage, opts);
            }
            break;
        }

        // ── MSL via SPIRV-Cross ────────────────────────────────────────────────
        case NkSLTarget::NK_MSL_SPIRV_CROSS: {
            NkSLCodeGenMSLSpirvCross gen;
            res = gen.Generate(ast, stage, opts);
            if (!res.success) {
                NkSLCodeGen_MSL fallback;
                res = fallback.Generate(ast, stage, opts);
                res.target = NkSLTarget::NK_MSL;
            }
            break;
        }

        // ── C++ Software Rasterizer ────────────────────────────────────────────
        case NkSLTarget::NK_CPLUSPLUS: {
            NkSLCodeGenCPP gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }

        // ── Bytecode NkSLVM (Software, sérialisable en .nkbc) ──────────────────
        case NkSLTarget::NK_BYTECODE: {
            NkSLCodeGenBytecode bgen;
            NkSLByteProgram     prog;
            NkVector<NkSLCompileError> bcErrs;
            if (bgen.Generate(ast, stage, prog, bcErrs)) {
                NkSLByteCodeSerialize(prog, res.bytecode);
                res.success = true;
            } else {
                res.success = false;
                for (auto& e : bcErrs) res.errors.PushBack(e);
            }
            res.target = NkSLTarget::NK_BYTECODE;
            res.stage  = stage;
            break;
        }

        default: {
            res.success = false;
            res.AddError(0, "Unknown target: " + NkString(NkSLTargetName(target)));
            break;
        }
    }

    // Remonter les diagnostics sémantiques en warnings (non bloquant).
    for (auto& e : semRes.errors)   res.warnings.PushBack(e);
    for (auto& e : semRes.warnings) res.warnings.PushBack(e);

    // (AST libéré automatiquement par astScope en fin de portée.)

    // 5. Mise en cache
    if (mCacheEnabled && res.success) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, res);
    }

    return res;
}

// =============================================================================
// CompileWithReflection
// =============================================================================
    NkSLCompileResultWithReflection NkSLCompiler::CompileWithReflection(
    const NkString&           source,
    NkSLStage                 stage,
    NkSLTarget                target,
    const NkSLCompileOptions& opts,
    const NkString&           filename)
{
    NkSLCompileResultWithReflection out;
    out.result = Compile(source, stage, target, opts, filename);

    if (out.result.success) {
        out.reflection    = Reflect(source, stage, filename);
        out.hasReflection = true;
    }
    return out;
}

// =============================================================================
// Reflect
// =============================================================================
NkSLReflection NkSLCompiler::Reflect(const NkString& source,
                                       NkSLStage stage,
                                       const NkString& filename) {
    NkSLReflection empty;
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) return empty;

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    NkSLAstScope astScope(ast);
    if (parser.HasErrors()) return empty;

    NkSLReflector reflector;
    NkSLReflection result = reflector.Reflect(ast, stage);
    return result;
}

// =============================================================================
// CompileAllTargets
// =============================================================================
NkSLCompiler::MultiTargetResult NkSLCompiler::CompileAllTargets(
    const NkString&            source,
    NkSLStage                  stage,
    const NkVector<NkSLTarget>& targets,
    const NkSLCompileOptions&   opts,
    const NkString&             filename)
{
    MultiTargetResult multi;
    // Parse une seule fois pour la reflection
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (ppErrors.Empty()) {
        NkSLLexer  lexer(processed, filename);
        NkSLParser parser(lexer);
        NkSLProgramNode* ast = parser.Parse();
        NkSLAstScope astScope(ast);
        if (!parser.HasErrors()) {
            NkSLReflector reflector;
            multi.reflection = reflector.Reflect(ast, stage);
        }
    }
    // Compiler chaque target
    for (auto t : targets) {
        multi.results.PushBack(Compile(source, stage, t, opts, filename));
    }
    return multi;
}

// =============================================================================
// CompileToSPIRV
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileToSPIRV(
    const NkString&           glslSource,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::NK_SPIRV; res.stage = stage;

#ifdef NKSL_HAS_SHADERC
    if (GLSLtoSPIRV_shaderc(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true; return res;
    }
    res.errors.Clear();
#endif

#ifdef NKSL_HAS_GLSLANG
    if (GLSLtoSPIRV_glslang(glslSource, stage, opts, res.bytecode, res.errors)) {
        res.success = true; return res;
    }
#endif

    if (res.errors.Empty())
        res.AddError(0, "No SPIR-V compiler available (install Vulkan SDK or add glslang submodule)");
    return res;
}

// =============================================================================
// CompileToMSL_SpirvCross
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileToMSL_SpirvCross(
    const NkString&           source,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts,
    const NkString&           filename)
{
    return Compile(source, stage, NkSLTarget::NK_MSL_SPIRV_CROSS, opts, filename);
}

// =============================================================================
// Validate
// =============================================================================
NkVector<NkSLCompileError> NkSLCompiler::Validate(const NkString& source,
                                                    const NkString& filename) {
    NkVector<NkSLCompileError> errors;
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    for (auto& e : ppErrors) errors.PushBack(e);

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    NkSLAstScope astScope(ast);
    for (auto& e : parser.GetErrors())   errors.PushBack(e);
    for (auto& e : parser.GetWarnings()) errors.PushBack(e);

    if (!parser.HasErrors()) {
        NkSLSemantic sem(NkSLStage::NK_VERTEX);
        NkSLSemanticResult semRes = sem.Analyze(ast);
        for (auto& e : semRes.errors)   errors.PushBack(e);
        for (auto& e : semRes.warnings) errors.PushBack(e);
    }

    return errors;
}

// =============================================================================
// Preprocess (#include simple + #pragma once)
// =============================================================================
NkString NkSLCompiler::Preprocess(const NkString& source,
                                    const NkString& baseDir,
                                    NkVector<NkSLCompileError>* errors,
                                    NkVector<NkString>* includedFiles)
{
    // Guard set propre si appelé en récursion depuis l'extérieur
    NkVector<NkString> localGuards;
    if (!includedFiles) includedFiles = &localGuards;

    // Préprocesseur minimal : macros objet (#define NAME val) + compilation
    // conditionnelle (#ifndef/#ifdef/#if/#else/#elif/#endif/#undef). Suffit pour les
    // .glsli partagés (gardes d'inclusion + #define de constantes ex. NK_VOXEL_*).
    struct PPMacro { NkString name; NkString value; };
    NkVector<PPMacro> macros;
    NkVector<bool>    condStack;   // chaque entrée : ce bloc est-il actif ?
    auto isActive = [&]() -> bool {
        for (uint32 i = 0; i < condStack.Size(); i++) if (!condStack[i]) return false;
        return true;
    };
    auto isDefined = [&](const NkString& n) -> bool {
        for (uint32 i = 0; i < macros.Size(); i++) if (macros[i].name == n) return true;
        return false;
    };
    auto isIdent = [](char c) -> bool {
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
    };
    auto firstToken = [&](const NkString& s) -> NkString {
        uint32 i = 0; while (i < s.Size() && (s[i]==' '||s[i]=='\t')) i++;
        NkString t; while (i < s.Size() && isIdent(s[i])) t += s[i++];
        return t;
    };
    // Expansion mot-entier des macros objet dans une ligne de code.
    auto expand = [&](const NkString& lineStr) -> NkString {
        if (macros.Empty()) return lineStr;
        NkString out = lineStr;
        for (uint32 mi = 0; mi < macros.Size(); mi++) {
            const NkString& nm = macros[mi].name;
            const NkString& vl = macros[mi].value;
            if (nm.Empty() || vl.Empty()) continue;
            NkString cur = out; out = "";
            const char* s = cur.CStr(); uint32 slen = cur.Size(), nlen = nm.Size();
            uint32 i = 0;
            while (i < slen) {
                bool match = (i + nlen <= slen) && (memcmp(s+i, nm.CStr(), nlen) == 0);
                bool lb = (i == 0) || !isIdent(s[i-1]);
                bool rb = (i + nlen >= slen) || !isIdent(s[i+nlen]);
                if (match && lb && rb) { out += vl; i += nlen; }
                else { out += s[i]; i++; }
            }
        }
        return out;
    };

    NkString result;
    const char* p = source.CStr();
    uint32 line = 1;

    while (*p) {
        // Skipper les espaces de début de ligne
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            NkString directive;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                directive += *p++;

            // ── Compilation conditionnelle (toujours évaluée, même bloc inactif) ──
            if (directive == "ifndef" || directive == "ifdef" || directive == "if") {
                NkString arg; while (*p && *p != '\n' && *p != '\r') arg += *p++;
                bool val;
                if      (directive == "ifndef") val = !isDefined(firstToken(arg));
                else if (directive == "ifdef")  val =  isDefined(firstToken(arg));
                else { NkString a = firstToken(arg); val = !(a == "0"); } // #if minimal
                condStack.PushBack(isActive() && val);
            } else if (directive == "elif") {
                while (*p && *p != '\n' && *p != '\r') p++;
                if (!condStack.Empty()) condStack[condStack.Size()-1] = false; // déjà pris
            } else if (directive == "else") {
                while (*p && *p != '\n' && *p != '\r') p++;
                if (!condStack.Empty()) {
                    bool parent = true;
                    for (uint32 i = 0; i + 1 < condStack.Size(); i++) if (!condStack[i]) parent = false;
                    condStack[condStack.Size()-1] = parent && !condStack[condStack.Size()-1];
                }
            } else if (directive == "endif") {
                while (*p && *p != '\n' && *p != '\r') p++;
                if (!condStack.Empty()) condStack.PopBack();
            } else if (directive == "define") {
                NkString arg; while (*p && *p != '\n' && *p != '\r') arg += *p++;
                if (isActive()) {
                    NkString name = firstToken(arg);
                    // valeur = reste après le nom (trim gauche).
                    uint32 i = 0; while (i < arg.Size() && (arg[i]==' '||arg[i]=='\t')) i++;
                    i += name.Size();
                    while (i < arg.Size() && (arg[i]==' '||arg[i]=='\t')) i++;
                    NkString val; while (i < arg.Size()) val += arg[i++];
                    macros.PushBack({ name, val });
                }
            } else if (directive == "undef") {
                NkString arg; while (*p && *p != '\n' && *p != '\r') arg += *p++;
                if (isActive()) {
                    NkString name = firstToken(arg);
                    NkVector<PPMacro> keep;
                    for (uint32 i = 0; i < macros.Size(); i++)
                        if (macros[i].name != name) keep.PushBack(macros[i]);
                    macros = keep;
                }
            } else if (!isActive()) {
                // Bloc inactif : on consomme la ligne (y compris #include) sans rien émettre.
                while (*p && *p != '\n' && *p != '\r') p++;
            } else if (directive == "include") {
                while (*p == ' ' || *p == '\t') p++;
                char delim = *p++;
                char closeDelim = (delim == '"') ? '"' : '>';
                NkString fname;
                while (*p && *p != closeDelim && *p != '\n') fname += *p++;
                if (*p == closeDelim) p++;

                NkString fullPath = baseDir.Empty() ? fname : (baseDir + "/" + fname);

                // Garde d'inclusion (#pragma once émulé)
                bool alreadyIncluded = false;
                for (auto& g : *includedFiles) {
                    if (g == fullPath) { alreadyIncluded = true; break; }
                }

                if (!alreadyIncluded) {
                    includedFiles->PushBack(fullPath);
                    if (NkFile::Exists(fullPath.CStr())) {
                        NkString incSrc = NkFile::ReadAllText(fullPath.CStr());

                        // Inclure avec traçage de numéros de lignes (#line)
                        char lineBuf[64];
                        snprintf(lineBuf, sizeof(lineBuf),
                                 "#line 1 \"%s\"\n", fullPath.CStr());
                        result += NkString(lineBuf);
                        result += Preprocess(incSrc, baseDir, errors, includedFiles);
                        snprintf(lineBuf, sizeof(lineBuf),
                                 "#line %u \"%s\"\n", line + 1, filename.Empty() ? "shader" : filename.CStr());
                        result += NkString(lineBuf);
                    } else {
                        if (errors)
                            errors->PushBack({line, 0, "", "#include not found: " + fname, false});
                        // On continue — erreur non fatale
                    }
                } else {
                    // Inclusion déjà faite — on ignore silencieusement
                    result += "// (already included: " + fname + ")\n";
                }
            } else if (directive == "pragma") {
                // #pragma once → on a déjà le mécanisme inclus
                // Les autres #pragma passent tels quels
                NkString rest;
                while (*p && *p != '\n') rest += *p++;
                NkString trimmed = rest;
                // Trim
                while (!trimmed.Empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
                    trimmed = trimmed.SubStr(1);
                if (trimmed.StartsWith("once")) {
                    // Signaler au caller qu'on a trouvé #pragma once
                    // (géré via includedFiles — on ne fait rien de plus)
                } else {
                    result += "#pragma" + rest;
                }
            } else if (directive == "version" || directive == "extension") {
                // Ignoré — on génère nous-même l'en-tête
                while (*p && *p != '\n') p++;
            } else {
                // Directive inconnue (#error, #line, #warning…) en bloc actif : on la
                // CONSOMME sans l'émettre (le lexer/parser NkSL ne gère pas les '#').
                while (*p && *p != '\n') p++;
            }
        } else {
            // Ligne de code : on lit, on expand les macros objet, on émet si actif.
            NkString lineStr;
            while (*p && *p != '\n' && *p != '\r') lineStr += *p++;
            if (isActive()) result += expand(lineStr);
        }

        if (*p == '\n') { result += '\n'; p++; line++; }
        else if (*p == '\r') {
            p++;
            if (*p == '\n') { result += '\n'; p++; }
            line++;
        }
    }
    return result;
}

// =============================================================================
// CompileWithSemantic
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileWithSemantic(
    const NkString&           source,
    NkSLStage                 stage,
    NkSLTarget                target,
    const NkSLCompileOptions& opts,
    const NkString&           filename)
{
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        NkSLCompileResult res; res.success = false;
        res.errors = ppErrors; res.target = target; res.stage = stage;
        return res;
    }

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();
    NkSLAstScope astScope(ast);

    if (parser.HasErrors()) {
        NkSLCompileResult res; res.success = false;
        res.errors = parser.GetErrors(); res.target = target; res.stage = stage;
        return res;
    }

    // Analyse sémantique
    NkSLSemantic sem(stage);
    NkSLSemanticResult semResult = sem.Analyze(ast);
    if (!semResult.success) {
        NkSLCompileResult res; res.success = false;
        res.errors   = semResult.errors;
        res.warnings = semResult.warnings;
        res.target = target; res.stage = stage;
        return res;
    }

    // Génération
    NkSLCompileResult res;
    switch (target) {
        case NkSLTarget::NK_GLSL:          { NkSLCodeGenGLSL         g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_GLSL_VULKAN:   { NkSLCodeGenGLSLVulkan   g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_HLSL_DX11:     { NkSLCodeGenHLSL_DX11    g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_HLSL_DX12:     { NkSLCodeGenHLSL_DX12    g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_MSL:           { NkSLCodeGen_MSL          g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_CPLUSPLUS:     { NkSLCodeGenCPP           g; res = g.Generate(ast, stage, opts); break; }
        case NkSLTarget::NK_BYTECODE: {
            NkSLCodeGenBytecode bgen; NkSLByteProgram prog; NkVector<NkSLCompileError> bcErrs;
            if (bgen.Generate(ast, stage, prog, bcErrs)) { NkSLByteCodeSerialize(prog, res.bytecode); res.success = true; }
            else { res.success = false; for (auto& e : bcErrs) res.errors.PushBack(e); }
            res.target = NkSLTarget::NK_BYTECODE; res.stage = stage; break;
        }
        default:                           { NkSLCodeGenGLSL          g; res = g.Generate(ast, stage, opts); break; }
    }

    for (auto& w : semResult.warnings) res.warnings.PushBack(w);
    return res;
}

// NkSLCompiler::FillShaderDesc* (produisant un NkShaderDesc, type NKRHI) ont été
// déplacés dans NkSLIntegration (NKRHI). NKSL ne dépend pas de NKRHI : le
// compilateur n'expose que Compile/CompileWithReflection (sorties NkSL natives).

// =============================================================================
// GLSLtoSPIRV_glslang
// CORRECTION : protégé par std::call_once (thread-safe)
// =============================================================================
bool NkSLCompiler::GLSLtoSPIRV_glslang(
    const NkString&           glsl,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts,
    NkVector<uint8>&          spirvOut,
    NkVector<NkSLCompileError>& errors)
{
#ifdef NKSL_HAS_GLSLANG
    // InitializeProcess() est appelé une seule fois, même depuis plusieurs threads
    static std::once_flag sInitFlag;
    std::call_once(sInitFlag, []() { glslang::InitializeProcess(); });

    EShLanguage lang;
    switch (stage) {
        case NkSLStage::NK_VERTEX:       lang = EShLangVertex;         break;
        case NkSLStage::NK_FRAGMENT:     lang = EShLangFragment;       break;
        case NkSLStage::NK_GEOMETRY:     lang = EShLangGeometry;       break;
        case NkSLStage::NK_TESS_CONTROL: lang = EShLangTessControl;    break;
        case NkSLStage::NK_TESS_EVAL:    lang = EShLangTessEvaluation; break;
        case NkSLStage::NK_COMPUTE:      lang = EShLangCompute;        break;
        default:                         lang = EShLangVertex;         break;
    }

    glslang::TShader shader(lang);
    const char* src = glsl.CStr();
    shader.setStrings(&src, 1);

    uint32 glslVer = opts.glslVulkanVersion;
    if (glslVer < 450) glslVer = 450;
    shader.setEnvInput(glslang::EShSourceGlsl, lang,
                       glslang::EShClientVulkan, (int)glslVer);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    EShMessages msgs = static_cast<EShMessages>(
        EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(GetDefaultResources(), (int)glslVer, false, msgs)) {
        NkString log(shader.getInfoLog());
        errors.PushBack({0, 0, "", "glslang: " + log, true});
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(msgs)) {
        NkString log(program.getInfoLog());
        errors.PushBack({0, 0, "", "glslang link: " + log, true});
        return false;
    }

    std::vector<uint32_t> spv;
    spv::SpvBuildLogger spvLogger;
    glslang::SpvOptions spvOpts;
    spvOpts.generateDebugInfo = opts.debugInfo;
    spvOpts.optimizeSize      = opts.optimize;
    glslang::GlslangToSpv(*program.getIntermediate(lang), spv, &spvLogger, &spvOpts);

    if (!spvLogger.getAllMessages().empty())
        NKSL_LOG("SPIR-V log: %s\n", spvLogger.getAllMessages().c_str());

    spirvOut.Resize(spv.size() * sizeof(uint32_t));
    memcpy(spirvOut.Data(), spv.data(), spirvOut.Size());
    return true;

#else
    (void)glsl; (void)stage; (void)opts; (void)spirvOut;
    errors.PushBack({0, 0, "", "glslang not available (compile with NKSL_HAS_GLSLANG)", true});
    return false;
#endif
}

// =============================================================================
// GLSLtoSPIRV_shaderc
// =============================================================================
bool NkSLCompiler::GLSLtoSPIRV_shaderc(
    const NkString&           glsl,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts,
    NkVector<uint8>&          spirvOut,
    NkVector<NkSLCompileError>& errors)
{
#ifdef NKSL_HAS_SHADERC
    shaderc::Compiler compiler;
    shaderc::CompileOptions shadercOpts;
    shadercOpts.SetSourceLanguage(shaderc_source_language_glsl);
    if (opts.optimize) shadercOpts.SetOptimizationLevel(shaderc_optimization_level_performance);
    if (opts.debugInfo) shadercOpts.SetGenerateDebugInfo();
    shadercOpts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);

    shaderc_shader_kind kind;
    switch (stage) {
        case NkSLStage::NK_VERTEX:   kind = shaderc_vertex_shader;   break;
        case NkSLStage::NK_FRAGMENT: kind = shaderc_fragment_shader; break;
        case NkSLStage::NK_COMPUTE:  kind = shaderc_compute_shader;  break;
        default:                     kind = shaderc_vertex_shader;   break;
    }

    auto result = compiler.CompileGlslToSpv(glsl.CStr(), kind, "shader.glsl", shadercOpts);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        errors.PushBack({0, 0, "", NkString("shaderc: ") + NkString(result.GetErrorMessage().c_str()), true});
        return false;
    }

    const uint8* data = reinterpret_cast<const uint8*>(result.begin());
    size_t sz = (result.end() - result.begin()) * sizeof(uint32_t);
    spirvOut.Resize(sz);
    memcpy(spirvOut.Data(), data, sz);
    return true;

#else
    (void)glsl; (void)stage; (void)opts; (void)spirvOut;
    errors.PushBack({0, 0, "", "shaderc not available", true});
    return false;
#endif
}

// =============================================================================
// NkSLShaderLibrary — CORRECTION : mutex sur les accès lecture
// =============================================================================
NkSLShaderLibrary::NkSLShaderLibrary(NkSLCompiler* compiler,
                                       const NkString& baseDir)
    : mCompiler(compiler), mBaseDir(baseDir) {}

bool NkSLShaderLibrary::Register(const NkString& name,
                                   const NkString& filePath,
                                   const NkVector<NkSLStage>& stages) {
    NkSLMutexLock lock(mMutex);
    NkString fullPath = mBaseDir.Empty() ? filePath : (mBaseDir + "/" + filePath);
    if (!NkFile::Exists(fullPath.CStr())) { NKSL_ERR("ShaderLibrary: cannot open '%s'\n", fullPath.CStr()); return false; }

    ShaderEntry e;
    e.name       = name;
    e.sourcePath = fullPath;
    e.source     = NkFile::ReadAllText(fullPath.CStr());
    e.stages     = stages;
    e.dirty      = true;
    mEntries.PushBack(e);
    return true;
}

bool NkSLShaderLibrary::RegisterInline(const NkString& name,
                                        const NkString& source,
                                        const NkVector<NkSLStage>& stages) {
    NkSLMutexLock lock(mMutex);
    ShaderEntry e;
    e.name   = name; e.source = source; e.stages = stages; e.dirty = true;
    mEntries.PushBack(e);
    return true;
}

bool NkSLShaderLibrary::CompileAll(NkSLTarget target,
                                    const NkSLCompileOptions& opts) {
    NkSLMutexLock lock(mMutex);
    bool allOk = true;
    for (auto& e : mEntries) {
        if (!e.dirty) continue;
        e.compiled.Clear();
        bool firstStage = true;
        for (auto stage : e.stages) {
            auto compiled = mCompiler->CompileWithReflection(e.source, stage, target,
                                                              opts, e.name);
            e.compiled.PushBack(compiled.result);
            if (firstStage && compiled.hasReflection) {
                e.reflection = compiled.reflection; firstStage = false;
            }
            if (!compiled.result.success) {
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
                                                  NkSLTarget /*target*/) const {
    NkSLMutexLock lock(mMutex);  // CORRECTION : mutex sur la lecture
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
    NkSLMutexLock lock(mMutex);  // CORRECTION : mutex sur la lecture
    for (auto& e : mEntries) {
        if (e.name == name) return &e.reflection;
    }
    return nullptr;
}

// NkSLShaderLibrary::FillShaderDesc* (NkShaderDesc = type NKRHI) retirés :
// l'équivalent vit dans NkSLIntegration (NKRHI). NKSL ne dépend pas de NKRHI.

uint32 NkSLShaderLibrary::HotReload(NkSLTarget target,
                                     const NkSLCompileOptions& opts) {
    uint32 reloaded = 0;
    // Vérification des fichiers sans tenir le mutex (I/O)
    NkVector<NkString> changed;
    {
        NkSLMutexLock lock(mMutex);
        for (auto& e : mEntries) {
            if (e.sourcePath.Empty()) continue;
            if (!NkFile::Exists(e.sourcePath.CStr())) continue;
            NkString newSrc = NkFile::ReadAllText(e.sourcePath.CStr());
            if (newSrc != e.source) {
                e.source = newSrc; e.dirty = true;
                NKSL_LOG("HotReload: '%s' changed\n", e.name.CStr());
            }
        }
    }
    // Compilation en dehors du mutex principal (évite deadlock si Compile réentre)
    CompileAll(target, opts);
    {
        NkSLMutexLock lock(mMutex);
        for (auto& e : mEntries) if (!e.dirty) reloaded++;
    }
    return reloaded;
}

} // namespace nkentseu
