// =============================================================================
// NkSLIntegration.cpp  — v4.0
//
// CORRECTIONS :
//   - GetCompiler() : std::call_once → thread-safe, plus de data race
//   - CreateShaderWithReflection : dispatch via ApiToTarget() étendu
//   - BuildShaderDesc : compatible NK_GLSL_VULKAN, NK_HLSL_DX11, NK_HLSL_DX12
// =============================================================================
#include "NkSLIntegration.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <mutex>   // std::call_once, std::once_flag

#define NKSL_LOG(...) logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...) logger_src.Errorf("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
namespace nksl {

// =============================================================================
// Singleton — CORRECTION : std::call_once (thread-safe)
// L'ancien pattern "if (!gCompiler) gCompiler = new NkSLCompiler();" crée une
// data race si deux threads appellent GetCompiler() simultanément avant init.
// std::call_once garantit que l'initialisation est atomique.
// =============================================================================
static NkSLCompiler* gCompiler   = nullptr;
static std::once_flag gInitFlag;

NkSLCompiler& GetCompiler() {
    std::call_once(gInitFlag, []() {
        gCompiler = new NkSLCompiler();
    });
    return *gCompiler;
}

void InitCompiler(const NkString& cacheDir) {
    // Réinitialisation explicite : on remplace le singleton
    // Note : non thread-safe si appelé depuis plusieurs threads simultanément,
    // mais InitCompiler() est censé être appelé une seule fois au démarrage.
    if (gCompiler) {
        gCompiler->GetCache().Flush();
        delete gCompiler;
    }
    gCompiler = new NkSLCompiler(cacheDir);
    // Marquer le flag comme exécuté pour que GetCompiler() ne reconstruise pas
    std::call_once(gInitFlag, []() {}); // no-op si déjà appelé
    NKSL_LOG("NkSL compiler initialized (cache: %s)\n",
             cacheDir.Empty() ? "disabled" : cacheDir.CStr());
}

void ShutdownCompiler() {
    if (gCompiler) {
        gCompiler->GetCache().Flush();
        delete gCompiler;
        gCompiler = nullptr;
    }
}

// =============================================================================
// CreateShaderWithReflection — recommandé
// =============================================================================
ShaderWithReflection CreateShaderWithReflection(
    NkIDevice*                device,
    const NkString&           source,
    const NkVector<NkSLStage>& stages,
    const NkString&           name,
    const NkSLCompileOptions& opts)
{
    ShaderWithReflection out;
    if (!device) {
        NKSL_ERR("CreateShaderWithReflection: device is null\n");
        return out;
    }

    NkSLTarget target = ApiToTarget(device->GetApi());
    NkSLCompiler& compiler = GetCompiler();

    NkShaderDesc desc;
    bool firstStage = true;

    for (auto stage : stages) {
        auto compiled = compiler.CompileWithReflection(source, stage, target, opts, name);
        if (!compiled.result.success) {
            NKSL_ERR("Shader '%s' stage=%s failed:\n", name.CStr(), NkSLStageName(stage));
            for (auto& e : compiled.result.errors)
                NKSL_ERR("  line %u: %s\n", e.line, e.message.CStr());
            return out;
        }

        // Reflection du premier stage
        if (firstStage && compiled.hasReflection) {
            out.reflection = compiled.reflection;
            firstStage = false;
        }

        NkShaderStageDesc sd{};
        sd.entryPoint = opts.entryPoint.CStr();
        switch (stage) {
            case NkSLStage::NK_VERTEX:   sd.stage = NkShaderStage::NK_VERTEX;   break;
            case NkSLStage::NK_FRAGMENT: sd.stage = NkShaderStage::NK_FRAGMENT; break;
            case NkSLStage::NK_COMPUTE:  sd.stage = NkShaderStage::NK_COMPUTE;  break;
            default:                     sd.stage = NkShaderStage::NK_VERTEX;   break;
        }

        if (NkSLTargetIsGLSL(target)) {
            sd.glslSource = compiled.result.source.CStr();
        } else if (NkSLTargetIsHLSL(target)) {
            sd.hlslSource = compiled.result.source.CStr();
        } else if (NkSLTargetIsMSL(target)) {
            sd.mslSource  = compiled.result.source.CStr();
        } else if (target == NkSLTarget::NK_SPIRV) {
            sd.spirvBinary.Resize((uint64)compiled.result.bytecode.Size());
            memcpy(sd.spirvBinary.Data(),
                   compiled.result.bytecode.Data(),
                   (size_t)compiled.result.bytecode.Size());
        } else if (target == NkSLTarget::NK_CPLUSPLUS) {
            sd.glslSource = compiled.result.source.CStr(); // convention : via glslSource
        }

        desc.AddStage(sd);
    }

    out.handle  = device->CreateShader(desc);
    out.success = (out.handle != NkShaderHandle{});
    return out;
}

// =============================================================================
// CreateShaderFromSource
// =============================================================================
NkShaderHandle CreateShaderFromSource(
    NkIDevice*                device,
    const NkString&           source,
    const NkVector<NkSLStage>& stages,
    const NkString&           name,
    const NkSLCompileOptions& opts)
{
    auto res = CreateShaderWithReflection(device, source, stages, name, opts);
    return res.handle;
}

// =============================================================================
// CreateShaderFromFile
// =============================================================================
static NkString ReadFile(const NkString& path) {
    FILE* f = fopen(path.CStr(), "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkVector<char> buf; buf.Resize(sz + 1);
    fread(buf.Data(), 1, sz, f); buf[sz] = '\0'; fclose(f);
    return NkString(buf.Data());
}

NkShaderHandle CreateShaderFromFile(
    NkIDevice*                device,
    const NkString&           filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts)
{
    NkString source = ReadFile(filePath);
    if (source.Empty()) {
        NKSL_ERR("CreateShaderFromFile: cannot read '%s'\n", filePath.CStr());
        return NkShaderHandle{};
    }
    return CreateShaderFromSource(device, source, stages, filePath, opts);
}

ShaderWithReflection CreateShaderFromFileWithReflection(
    NkIDevice*                device,
    const NkString&           filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts)
{
    NkString source = ReadFile(filePath);
    if (source.Empty()) {
        NKSL_ERR("CreateShaderFromFileWithReflection: cannot read '%s'\n", filePath.CStr());
        return {};
    }
    return CreateShaderWithReflection(device, source, stages, filePath, opts);
}

// =============================================================================
// Reflection seule
// =============================================================================
NkSLReflection GetReflection(const NkString& source, NkSLStage stage,
                               const NkString& filename) {
    return GetCompiler().Reflect(source, stage, filename);
}

// =============================================================================
// BuildShaderDesc — compatible tous les targets v4.0
// =============================================================================
bool BuildShaderDesc(
    NkGraphicsApi             api,
    const NkString&           source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&             outDesc,
    const NkString&           name,
    const NkSLCompileOptions& opts)
{
    NkSLReflection dummy;
    return BuildShaderDescWithReflection(api, source, stages, outDesc, dummy, name, opts);
}

bool BuildShaderDescWithReflection(
    NkGraphicsApi             api,
    const NkString&           source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&             outDesc,
    NkSLReflection&           outReflection,
    const NkString&           name,
    const NkSLCompileOptions& opts)
{
    NkSLTarget target = ApiToTarget(api);
    NkSLCompiler& compiler = GetCompiler();
    bool firstStage = true;

    for (auto stage : stages) {
        auto compiled = compiler.CompileWithReflection(source, stage, target, opts, name);
        if (!compiled.result.success) {
            NKSL_ERR("BuildShaderDesc '%s' stage=%s target=%s FAILED\n",
                     name.CStr(), NkSLStageName(stage), NkSLTargetName(target));
            return false;
        }
        if (firstStage && compiled.hasReflection) {
            outReflection = compiled.reflection; firstStage = false;
        }

        NkShaderStageDesc sd{};
        sd.entryPoint = opts.entryPoint.CStr();
        switch (stage) {
            case NkSLStage::NK_VERTEX:   sd.stage = NkShaderStage::NK_VERTEX;   break;
            case NkSLStage::NK_FRAGMENT: sd.stage = NkShaderStage::NK_FRAGMENT; break;
            case NkSLStage::NK_COMPUTE:  sd.stage = NkShaderStage::NK_COMPUTE;  break;
            default:                     sd.stage = NkShaderStage::NK_VERTEX;   break;
        }

        if (NkSLTargetIsGLSL(target)) sd.glslSource = compiled.result.source.CStr();
        if (NkSLTargetIsHLSL(target)) sd.hlslSource = compiled.result.source.CStr();
        if (NkSLTargetIsMSL(target))  sd.mslSource  = compiled.result.source.CStr();
        if (target == NkSLTarget::NK_SPIRV) {
            sd.spirvBinary.Resize((uint64)compiled.result.bytecode.Size());
            memcpy(sd.spirvBinary.Data(), compiled.result.bytecode.Data(),
                   (size_t)compiled.result.bytecode.Size());
        }
        outDesc.AddStage(sd);
    }
    return true;
}

// =============================================================================
// Validate
// =============================================================================
NkVector<NkSLCompileError> Validate(const NkString& source,
                                     const NkString& filename) {
    return GetCompiler().Validate(source, filename);
}

// =============================================================================
// GetGeneratedSource — debug : retourne le code généré pour une cible/stage
// =============================================================================
NkString GetGeneratedSource(
    NkGraphicsApi             api,
    const NkString&           source,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts)
{
    NkSLTarget target = ApiToTarget(api);
    // Pour Vulkan, retourner le GLSL Vulkan (plus lisible que le SPIR-V binaire)
    if (target == NkSLTarget::NK_SPIRV)
        target = NkSLTarget::NK_GLSL_VULKAN;

    auto result = GetCompiler().Compile(source, stage, target, opts);
    return result.success ? result.source : NkString("");
}

// =============================================================================
// GenerateLayoutCPP / GenerateLayoutJSON
// =============================================================================
NkString GenerateLayoutCPP(const NkString& source, NkSLStage stage,
                             const NkString& varName, const NkString& filename) {
    NkSLReflection ref = GetCompiler().Reflect(source, stage, filename);
    NkSLReflector reflector;
    return reflector.GenerateLayoutCPP(ref, varName);
}

NkString GenerateLayoutJSON(const NkString& source, NkSLStage stage,
                              const NkString& filename) {
    NkSLReflection ref = GetCompiler().Reflect(source, stage, filename);
    NkSLReflector reflector;
    return reflector.GenerateLayoutJSON(ref);
}

} // namespace nksl
} // namespace nkentseu
