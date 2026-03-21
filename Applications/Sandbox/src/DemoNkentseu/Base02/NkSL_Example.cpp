// =============================================================================
// NkSL_Example.cpp
// Démontre l'utilisation du système NkSL dans une application RHI réelle.
//
// Un seul fichier source NkSL remplace tous les shaders par backend :
//   - kPhongShader  → GLSL / SPIR-V / HLSL / MSL / C++ selon l'API
//   - kShadowShader → idem
//
// Plus besoin de NkRHIDemoFullVkSpv.inl, de HLSL séparé, ni de MSL séparé.
// =============================================================================
#include "NkSLIntegration.h"
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Core/NkDeviceFactory.h"

namespace nkentseu {

// =============================================================================
// Shader Phong + shadow mapping en NkSL
// Un seul source couvre tous les backends.
// =============================================================================
static constexpr const char* kPhongShader = R"NKSL(
// ============================================================================
// Phong shader avec shadow mapping — source NkSL unique
// Compilé vers GLSL / SPIR-V / HLSL / MSL / C++ selon la cible
// ============================================================================

// ── Uniform block (binding 0, set 0) ─────────────────────────────────────────
@binding(set=0, binding=0)
uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

// ── Shadow sampler (binding 1, set 0) ────────────────────────────────────────
@binding(set=0, binding=1)
uniform sampler2DShadow uShadowMap;

// ── Vertex shader ─────────────────────────────────────────────────────────────
@location(0) in  vec3 aPos;
@location(1) in  vec3 aNormal;
@location(2) in  vec3 aColor;

@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec3 vColor;
@location(3) out vec4 vShadowCoord;

@stage(vertex)
@entry
void vertMain() {
    vec4 worldPos    = ubo.model * vec4(aPos, 1.0);
    vWorldPos        = worldPos.xyz;
    mat3 normalMat   = mat3(transpose(inverse(ubo.model)));
    vNormal          = normalize(normalMat * aNormal);
    vColor           = aColor;
    vShadowCoord     = ubo.lightVP * worldPos;
    gl_Position      = ubo.proj * ubo.view * worldPos;
}

// ── Fragment shader ───────────────────────────────────────────────────────────
@location(0) in  vec3 vWorldPos;
@location(1) in  vec3 vNormal;
@location(2) in  vec3 vColor;
@location(3) in  vec4 vShadowCoord;

@location(0) out vec4 fragColor;

// PCF 3×3 shadow sampling
float ShadowFactor(vec4 shadowCoord) {
    vec3 proj = shadowCoord.xyz / shadowCoord.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z > 1.0)
        return 1.0;

    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);
    proj.z -= bias;

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            shadow += texture(uShadowMap,
                              vec3(proj.xy + offset, proj.z));
        }
    }
    return shadow / 9.0;
}

@stage(fragment)
@entry
void fragMain() {
    vec3 N     = normalize(vNormal);
    vec3 L     = normalize(-ubo.lightDirW.xyz);
    vec3 V     = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H     = normalize(L + V);

    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 32.0);
    float shadow = ShadowFactor(vShadowCoord);

    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = shadow * diff * vColor;
    vec3 specular = shadow * spec * vec3(0.4);

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)NKSL";

// =============================================================================
// Shader shadow pass (depth-only)
// =============================================================================
static constexpr const char* kShadowShader = R"NKSL(
@binding(set=0, binding=0)
uniform ShadowUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

@location(0) in vec3 aPos;

@stage(vertex)
@entry
void vertMain() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}

@stage(fragment)
@entry
void fragMain() {
    // Depth-only pass : rien à écrire
}
)NKSL";

// =============================================================================
// Shader compute example (tonemapping)
// =============================================================================
static constexpr const char* kTonemapCompute = R"NKSL(
@binding(set=0, binding=0)
uniform TonemapParams {
    float exposure;
    float gamma;
    vec2  resolution;
} params;

@binding(set=0, binding=1)
uniform image2D inputImage;

@binding(set=0, binding=2)
uniform image2D outputImage;

vec3 ACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

@stage(compute)
@entry
void computeMain() {
    // gl_GlobalInvocationID contient les coordonnées du thread
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= int(params.resolution.x) ||
        coord.y >= int(params.resolution.y))
        return;

    vec4 hdr = imageLoad(inputImage, coord);
    vec3 mapped = ACES(hdr.rgb * params.exposure);
    mapped = pow(mapped, vec3(1.0 / params.gamma));
    imageStore(outputImage, coord, vec4(mapped, 1.0));
}
)NKSL";

// =============================================================================
// Démonstration de l'utilisation dans NkRHIDemo
// =============================================================================
static NkShaderHandle CreatePhongShader(NkIDevice* device) {
    // Une seule ligne remplace tous les switch(api) { case VULKAN: ... case OPENGL: ... }
    return NkSL::CreateShaderFromSource(
        device,
        kPhongShader,
        { NkSLStage::VERTEX, NkSLStage::FRAGMENT },
        "PhongShadow"
    );
}

static NkShaderHandle CreateShadowShader(NkIDevice* device) {
    return NkSL::CreateShaderFromSource(
        device,
        kShadowShader,
        { NkSLStage::VERTEX, NkSLStage::FRAGMENT },
        "ShadowDepth"
    );
}

static NkShaderHandle CreateTonemapShader(NkIDevice* device) {
    return NkSL::CreateShaderFromSource(
        device,
        kTonemapCompute,
        { NkSLStage::COMPUTE },
        "Tonemap"
    );
}

// =============================================================================
// Démonstration de BuildShaderDesc pour multi-API offline
// =============================================================================
static void DemoOfflineCompile() {
    NkSL::InitCompiler("./shader_cache");

    // Compiler le même shader vers toutes les APIs en une passe
    NkVector<NkSLTarget> targets = {
        NkSLTarget::GLSL,
        NkSLTarget::SPIRV,
        NkSLTarget::HLSL,
        NkSLTarget::MSL,
    };

    NkSLCompiler& compiler = NkSL::GetCompiler();

    for (auto target : targets) {
        for (auto stage : {NkSLStage::VERTEX, NkSLStage::FRAGMENT}) {
            NkSLCompileResult res = compiler.Compile(
                kPhongShader, stage, target, {}, "PhongShadow");

            if (res.success) {
                printf("[NkSL] %s %s: OK (%zu bytes)\n",
                       NkSLTargetName(target),
                       NkSLStageName(stage),
                       (size_t)res.bytecode.Size());
            } else {
                printf("[NkSL] %s %s: FAILED\n",
                       NkSLTargetName(target),
                       NkSLStageName(stage));
                for (auto& e : res.errors)
                    printf("  line %u: %s\n", e.line, e.message.CStr());
            }
        }
    }

    // Persister le cache sur disque
    compiler.GetCache().Flush();
}

// =============================================================================
// Démonstration de la validation
// =============================================================================
static void DemoValidation() {
    static constexpr const char* kBrokenShader = R"NKSL(
        @stage(vertex)
        void main() {
            vec4 pos = vec4(1.0, 2.0, 3.0   // manque la parenthèse fermante
            gl_Position = pos;
        }
    )NKSL";

    auto errors = NkSL::Validate(kBrokenShader, "broken.nksl");
    printf("[NkSL] Validation: %u error(s)\n", (unsigned)errors.Size());
    for (auto& e : errors)
        printf("  line %u: %s\n", e.line, e.message.CStr());
}

// =============================================================================
// Démonstration du hot-reload
// =============================================================================
static void DemoHotReload(NkIDevice* device) {
    NkSL::InitCompiler("./shader_cache");

    NkSLShaderLibrary lib(&NkSL::GetCompiler(), "./shaders");

    // Enregistrer des shaders
    lib.Register("phong",   "phong.nksl",
                 { NkSLStage::VERTEX, NkSLStage::FRAGMENT });
    lib.Register("tonemap", "tonemap.nksl",
                 { NkSLStage::COMPUTE });

    NkSLTarget target = NkSL::ApiToTarget(device->GetApi());
    lib.CompileAll(target);

    // Dans la boucle principale (appeler périodiquement) :
    // uint32 reloaded = lib.HotReload(target);
    // if (reloaded > 0) { /* re-créer les pipelines impactés */ }

    // Obtenir un NkShaderDesc
    NkShaderDesc desc;
    lib.FillShaderDesc("phong", target, desc);
    NkShaderHandle h = device->CreateShader(desc);
    (void)h;
}

// =============================================================================
// Inspection du code généré (debug)
// =============================================================================
static void DemoInspectGenerated(NkGraphicsApi api) {
    printf("\n=== Generated %s (vertex) ===\n",
           NkSLTargetName(NkSL::ApiToTarget(api)));
    NkString generated = NkSL::GetGeneratedSource(
        api, kPhongShader, NkSLStage::VERTEX);
    printf("%s\n", generated.CStr());
}

} // namespace nkentseu
