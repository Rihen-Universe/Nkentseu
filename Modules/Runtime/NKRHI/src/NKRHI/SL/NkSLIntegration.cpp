// =============================================================================
// NkSLIntegration.cpp  — v3.0
// =============================================================================
#include "NkSLIntegration.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

#define NKSL_LOG(...) logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...) logger_src.Errorf("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
namespace nksl {

// =============================================================================
// Singleton
// =============================================================================
static NkSLCompiler* gCompiler = nullptr;

NkSLCompiler& GetCompiler() {
    if (!gCompiler) gCompiler = new NkSLCompiler();
    return *gCompiler;
}

void InitCompiler(const NkString& cacheDir) {
    if (gCompiler) delete gCompiler;
    gCompiler = new NkSLCompiler(cacheDir);
    NKSL_LOG("NkSL compiler initialized (cache: %s)\n",
             cacheDir.Empty() ? "disabled" : cacheDir.CStr());
}

void ShutdownCompiler() {
    if (gCompiler) { gCompiler->GetCache().Flush(); delete gCompiler; gCompiler = nullptr; }
}

// =============================================================================
// CreateShaderFromSource
// =============================================================================
NkShaderHandle CreateShaderFromSource(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    NkSLReflection dummy;
    auto res = CreateShaderWithReflection(device, source, stages, name, opts);
    return res.handle;
}

// =============================================================================
// CreateShaderWithReflection — recommandé
// =============================================================================
ShaderWithReflection CreateShaderWithReflection(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    ShaderWithReflection out;

    if (!device || !device->IsValid()) {
        NKSL_ERR("CreateShaderWithReflection: invalid device\n");
        return out;
    }

    NkSLTarget target = ApiToTarget(device->GetApi());
    NkShaderDesc desc;
    desc.debugName = name.CStr();

    bool ok = BuildShaderDescWithReflection(
        device->GetApi(), source, stages, desc, out.reflection, name, opts);

    if (!ok) {
        NKSL_ERR("CreateShaderWithReflection: compilation failed for '%s'\n", name.CStr());
        return out;
    }

    out.handle = device->CreateShader(desc);
    out.success = out.handle.IsValid();

    if (!out.success)
        NKSL_ERR("CreateShaderWithReflection: NkIDevice::CreateShader failed for '%s'\n", name.CStr());
    else
        NKSL_LOG("Shader '%s' created for API=%s\n", name.CStr(), NkSLTargetName(target));

    return out;
}

// =============================================================================
// CreateShaderFromFile
// =============================================================================
static NkString LoadFile(const NkString& filePath) {
    FILE* f = fopen(filePath.CStr(), "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkVector<char> buf; buf.Resize(sz + 1);
    fread(buf.Data(), 1, sz, f); buf[sz] = '\0'; fclose(f);
    return NkString(buf.Data());
}

static NkString ExtractName(const NkString& filePath) {
    NkString name = filePath;
    int lastSlash = -1;
    for (int i = (int)name.Size()-1; i >= 0; i--)
        if (name[i] == '/' || name[i] == '\\') { lastSlash = i; break; }
    if (lastSlash >= 0) name = name.SubStr(lastSlash + 1);
    int lastDot = -1;
    for (int i = (int)name.Size()-1; i >= 0; i--)
        if (name[i] == '.') { lastDot = i; break; }
    if (lastDot > 0) name = name.SubStr(0, lastDot);
    return name;
}

NkShaderHandle CreateShaderFromFile(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts)
{
    NkString source = LoadFile(filePath);
    if (source.Empty()) {
        NKSL_ERR("CreateShaderFromFile: cannot open '%s'\n", filePath.CStr());
        return {};
    }
    return CreateShaderFromSource(device, source, stages, ExtractName(filePath), opts);
}

ShaderWithReflection CreateShaderFromFileWithReflection(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts)
{
    NkString source = LoadFile(filePath);
    if (source.Empty()) {
        NKSL_ERR("CreateShaderFromFileWithReflection: cannot open '%s'\n", filePath.CStr());
        return {};
    }
    return CreateShaderWithReflection(device, source, stages, ExtractName(filePath), opts);
}

// =============================================================================
// GetReflection
// =============================================================================
NkSLReflection GetReflection(
    const NkString& source,
    NkSLStage       stage,
    const NkString& filename)
{
    return GetCompiler().Reflect(source, stage, filename);
}

// =============================================================================
// BuildShaderDesc
// =============================================================================
bool BuildShaderDesc(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    NkSLReflection dummy;
    return BuildShaderDescWithReflection(api, source, stages, outDesc, dummy, name, opts);
}

bool BuildShaderDescWithReflection(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    NkSLReflection&          outReflection,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    NkSLTarget target = ApiToTarget(api);
    NkSLCompiler& compiler = GetCompiler();
    bool allOk = true;
    bool firstStage = true;

    // Stocker les résultats pour garder les strings vivants
    NkVector<NkSLCompileResultWithReflection> results;
    results.Reserve((uint32)stages.Size());

    for (auto stage : stages) {
        auto compiled = compiler.CompileWithReflection(source, stage, target, opts, name);

        if (!compiled.result.success) {
            NKSL_ERR("BuildShaderDescWithReflection: stage=%s target=%s FAILED for '%s'\n",
                     NkSLStageName(stage), NkSLTargetName(target), name.CStr());
            for (auto& e : compiled.result.errors)
                NKSL_ERR("  line %u: %s\n", e.line, e.message.CStr());
            allOk = false;
            continue;
        }

        // Stocker la reflection du premier stage (vertex)
        if (firstStage && compiled.hasReflection) {
            outReflection = compiled.reflection; firstStage = false;
        }

        results.PushBack(compiled);
        auto& saved = results.Back();

        NkShaderStageDesc sd{};
        sd.entryPoint = opts.entryPoint.CStr();

        switch (stage) {
            case NkSLStage::NK_VERTEX:       sd.stage = NkShaderStage::NK_VERTEX;    break;
            case NkSLStage::NK_FRAGMENT:     sd.stage = NkShaderStage::NK_FRAGMENT;  break;
            case NkSLStage::NK_GEOMETRY:     sd.stage = NkShaderStage::NK_GEOMETRY;  break;
            case NkSLStage::NK_TESS_CONTROL: sd.stage = NkShaderStage::NK_TESS_CTRL; break;
            case NkSLStage::NK_TESS_EVAL:    sd.stage = NkShaderStage::NK_TESS_EVAL; break;
            case NkSLStage::NK_COMPUTE:      sd.stage = NkShaderStage::NK_COMPUTE;   break;
            default: sd.stage = NkShaderStage::NK_VERTEX;
        }

        switch (target) {
            case NkSLTarget::NK_GLSL:
                sd.glslSource = saved.result.source.CStr(); break;
            case NkSLTarget::NK_SPIRV:
                if (saved.result.IsText()) {
                    sd.glslSource = saved.result.source.CStr();
                    NKSL_LOG("No SPIR-V compiler, using GLSL text for Vulkan stage=%s\n", NkSLStageName(stage));
                } else {
                    sd.spirvBinary.Resize((uint64)saved.result.bytecode.Size());
                    memcpy(sd.spirvBinary.Data(), saved.result.bytecode.Data(), (size_t)saved.result.bytecode.Size());
                }
                break;
            case NkSLTarget::NK_HLSL:
                sd.hlslSource = saved.result.source.CStr(); break;
            case NkSLTarget::NK_MSL:
            case NkSLTarget::NK_MSL_SPIRV_CROSS:
                sd.mslSource = saved.result.source.CStr(); break;
            case NkSLTarget::NK_CPLUSPLUS:
                sd.glslSource = saved.result.source.CStr(); break;
            default: break;
        }

        outDesc.AddStage(sd);
    }

    return allOk;
}

// =============================================================================
// Validate
// =============================================================================
NkVector<NkSLCompileError> Validate(const NkString& source, const NkString& filename) {
    return GetCompiler().Validate(source, filename);
}

// =============================================================================
// GetGeneratedSource
// =============================================================================
NkString GetGeneratedSource(
    NkGraphicsApi            api,
    const NkString&          source,
    NkSLStage                stage,
    const NkSLCompileOptions& opts)
{
    NkSLTarget target = ApiToTarget(api);
    NkSLCompileResult res = GetCompiler().Compile(source, stage, target, opts);
    if (!res.success) {
        NkString msg = "// Compilation failed:\n";
        for (auto& e : res.errors)
            msg += "// line " + NkFormat("{0}", e.line) + ": " + e.message + "\n";
        return msg;
    }
    return res.source;
}

// =============================================================================
// GenerateLayoutCPP / GenerateLayoutJSON
// =============================================================================
NkString GenerateLayoutCPP(
    const NkString& source,
    NkSLStage       stage,
    const NkString& varName,
    const NkString& filename)
{
    NkSLReflection reflection = GetCompiler().Reflect(source, stage, filename);
    NkSLReflector reflector;
    return reflector.GenerateLayoutCPP(reflection, varName);
}

NkString GenerateLayoutJSON(
    const NkString& source,
    NkSLStage       stage,
    const NkString& filename)
{
    NkSLReflection reflection = GetCompiler().Reflect(source, stage, filename);
    NkSLReflector reflector;
    return reflector.GenerateLayoutJSON(reflection);
}

} // namespace nksl
} // namespace nkentseu
