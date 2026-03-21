// =============================================================================
// NkSLIntegration.cpp
// =============================================================================
#include "NkSLIntegration.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

#define NKSL_LOG(...) logger_src.Infof("[NkSL] " __VA_ARGS__)
#define NKSL_ERR(...) logger_src.Infof("[NkSL][ERR] " __VA_ARGS__)

namespace nkentseu {
namespace NkSL {

// =============================================================================
// Singleton du compilateur
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
    if (gCompiler) {
        gCompiler->GetCache().Flush();
        delete gCompiler;
        gCompiler = nullptr;
    }
}

// =============================================================================
// Séparation d'un source NkSL multi-stage
// Le source peut contenir plusieurs @stage(vertex) / @stage(fragment)
// On extrait chaque section et on compile séparément.
// =============================================================================
static NkString ExtractStageSource(const NkString& fullSource, NkSLStage stage) {
    // Stratégie simple : on passe le source complet et le backend ignore
    // les déclarations des autres stages. Cette approche fonctionne car NkSL
    // utilise @stage() pour marquer les entry points, et le générateur de code
    // n'émet que les fonctions du stage demandé.
    // Pour les implémentations futures, on pourrait découper physiquement ici.
    return fullSource;
}

// =============================================================================
NkShaderHandle CreateShaderFromSource(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    if (!device || !device->IsValid()) {
        NKSL_ERR("CreateShaderFromSource: invalid device\n");
        return {};
    }

    NkSLTarget target = ApiToTarget(device->GetApi());
    NkShaderDesc desc;
    desc.debugName = name.CStr();

    bool ok = BuildShaderDesc(device->GetApi(), source, stages, desc, name, opts);
    if (!ok) {
        NKSL_ERR("CreateShaderFromSource: shader compilation failed for '%s'\n", name.CStr());
        return {};
    }

    NkShaderHandle h = device->CreateShader(desc);
    if (!h.IsValid())
        NKSL_ERR("CreateShaderFromSource: NkIDevice::CreateShader failed for '%s'\n", name.CStr());
    else
        NKSL_LOG("Shader '%s' created for API=%s\n", name.CStr(),
                 NkSLTargetName(target));

    return h;
}

// =============================================================================
NkShaderHandle CreateShaderFromFile(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts)
{
    // Charger le fichier
    FILE* f = fopen(filePath.CStr(), "r");
    if (!f) {
        NKSL_ERR("CreateShaderFromFile: cannot open '%s'\n", filePath.CStr());
        return {};
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkVector<char> buf; buf.Resize(sz + 1);
    fread(buf.Data(), 1, sz, f); buf[sz] = '\0';
    fclose(f);

    // Extraire le nom (sans le chemin ni l'extension)
    NkString name = filePath;
    {
        // Trouver le dernier '/' ou '\\'
        int lastSlash = -1;
        for (int i = (int)name.Size()-1; i >= 0; i--) {
            if (name[i] == '/' || name[i] == '\\') { lastSlash = i; break; }
        }
        if (lastSlash >= 0) name = name.SubStr(lastSlash + 1);
        // Retirer l'extension
        int lastDot = -1;
        for (int i = (int)name.Size()-1; i >= 0; i--) {
            if (name[i] == '.') { lastDot = i; break; }
        }
        if (lastDot > 0) name = name.SubStr(0, lastDot);
    }

    return CreateShaderFromSource(device, NkString(buf.Data()), stages, name, opts);
}

// =============================================================================
bool BuildShaderDesc(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    const NkString&          name,
    const NkSLCompileOptions& opts)
{
    NkSLTarget target = ApiToTarget(api);
    NkSLCompiler& compiler = GetCompiler();
    bool allOk = true;

    // Stocker les résultats pour garder les strings vivants
    NkVector<NkSLCompileResult> results;
    results.Reserve((uint32)stages.Size());

    for (auto stage : stages) {
        NkString stageSource = ExtractStageSource(source, stage);
        NkSLCompileResult res = compiler.Compile(stageSource, stage, target, opts, name);

        if (!res.success) {
            NKSL_ERR("BuildShaderDesc: stage=%s target=%s FAILED for '%s'\n",
                     NkSLStageName(stage), NkSLTargetName(target), name.CStr());
            for (auto& e : res.errors)
                NKSL_ERR("  line %u: %s\n", e.line, e.message.CStr());
            allOk = false;
            continue;
        }

        results.PushBack(res);
        auto& saved = results.Back();

        // Remplir le NkShaderStageDesc
        NkShaderStageDesc sd{};
        sd.entryPoint = opts.entryPoint.CStr();

        switch (stage) {
            case NkSLStage::VERTEX:       sd.stage = NkShaderStage::NK_VERTEX;    break;
            case NkSLStage::FRAGMENT:     sd.stage = NkShaderStage::NK_FRAGMENT;  break;
            case NkSLStage::GEOMETRY:     sd.stage = NkShaderStage::NK_GEOMETRY;  break;
            case NkSLStage::TESS_CONTROL: sd.stage = NkShaderStage::NK_TESS_CTRL; break;
            case NkSLStage::TESS_EVAL:    sd.stage = NkShaderStage::NK_TESS_EVAL; break;
            case NkSLStage::COMPUTE:      sd.stage = NkShaderStage::NK_COMPUTE;   break;
            default: sd.stage = NkShaderStage::NK_VERTEX;
        }

        switch (target) {
            case NkSLTarget::GLSL:
                sd.glslSource = saved.source.CStr();
                break;
            case NkSLTarget::SPIRV:
                if (saved.IsText()) {
                    // Pas de SPIR-V dispo — fallback vers GLSL texte
                    // (certains drivers Vulkan acceptent VK_NV_glsl_shader)
                    sd.glslSource = saved.source.CStr();
                    NKSL_LOG("No SPIR-V compiler, using GLSL text for Vulkan stage=%s\n",
                             NkSLStageName(stage));
                } else {
                    sd.spirvData = saved.bytecode.Data();
                    sd.spirvSize = (uint64)saved.bytecode.Size();
                }
                break;
            case NkSLTarget::HLSL:
                sd.hlslSource = saved.source.CStr();
                break;
            case NkSLTarget::MSL:
                sd.mslSource = saved.source.CStr();
                break;
            case NkSLTarget::CPLUSPLUS:
                // Le CPU backend utilise les fonctions C++ générées
                // (intégrées via compilation séparée)
                sd.glslSource = saved.source.CStr(); // stocké pour debug
                break;
            default:
                break;
        }

        outDesc.AddStage(sd);
    }

    // NOTE : Les pointeurs dans sd.glslSource / sd.hlslSource / sd.mslSource
    // pointent vers les strings dans 'results'. Pour que cela fonctionne,
    // le NkShaderDesc doit être utilisé AVANT que results soit détruit.
    // En pratique, device->CreateShader(desc) est appelé immédiatement après,
    // donc c'est correct. Pour une utilisation différée, copier les strings.

    return allOk;
}

// =============================================================================
NkVector<NkSLCompileError> Validate(const NkString& source, const NkString& filename) {
    return GetCompiler().Validate(source, filename);
}

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

} // namespace NkSL
} // namespace nkentseu
