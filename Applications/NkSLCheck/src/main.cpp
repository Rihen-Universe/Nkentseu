// =============================================================================
// NkSLCheck — valide que les shaders instanciés NkSL se convertissent vers chaque
// backend. Compile VS + FS via le VRAI NkSLCompiler vers GL / Vulkan-GLSL /
// HLSL-DX11 / HLSL-DX12 et imprime succès + code généré complet. Pas de GPU requis.
// Sert à diagnostiquer : (A) éclairage FS cassé GL/DX, (B) instancing DX11.
// =============================================================================
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/Core/NkSLTypes.h"
#include <cstdio>

using namespace nkentseu;

// VS instancié — copie EXACTE de instanced.vert.nksl (set=1 binding=2 instances).
static const char* kInstancedVS = R"NKSL(
@binding(set=0, binding=0)
uniform CameraUBO {
    mat4 view; mat4 proj; mat4 viewProj; mat4 invViewProj;
    vec4 camPos; vec4 camDir; vec2 viewport; float time; float deltaTime;
} cam;

@binding(set=1, binding=2)
uniform InstanceUBO {
    mat4 models[128];
    vec4 tints[128];
} inst;

@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;
@location(2) in vec3 aTangent;
@location(3) in vec2 aUV;
@location(4) in vec2 aUV2;
@location(5) in vec4 aColor;

@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec3 vTangent;
@location(3) out vec2 vUV;
@location(4) out vec4 vColor;

@stage(vertex)
@entry
void main() {
    mat4 m        = inst.models[gl_InstanceID];
    vec4 worldPos = m * vec4(aPos, 1.0);
    vWorldPos     = worldPos.xyz;
    mat3 nm       = mat3(transpose(inverse(m)));
    vNormal       = normalize(nm * aNormal);
    vTangent      = normalize(nm * aTangent);
    vUV           = aUV;
    vColor        = inst.tints[gl_InstanceID];
    gl_Position   = cam.viewProj * worldPos;
}
)NKSL";

// FS instancié — copie EXACTE de instanced.frag.nksl (autonome, tint+éclairage).
static const char* kInstancedFS = R"NKSL(
@location(0) in vec3 vWorldPos;
@location(1) in vec3 vNormal;
@location(3) in vec2 vUV;
@location(4) in vec4 vColor;

@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3 albedo = vColor.rgb;
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4, 0.8, 0.5));
    float diff = max(dot(N, L), 0.0);
    float hemi = 0.5 + 0.5 * N.y;
    vec3 lit = albedo * (vec3(0.30) + vec3(0.40) * hemi + vec3(0.85) * diff);
    fragColor = vec4(lit, 1.0);
}
)NKSL";

static void Check(NkSLCompiler& c, NkSLTarget t, NkSLStage stage,
                  const char* src, const char* name) {
    NkSLCompileResult r = c.Compile(NkString(src), stage, t);
    printf("\n========================= %s : success=%d =========================\n",
           name, r.success ? 1 : 0);
    if (!r.success) {
        for (const auto& e : r.errors)
            printf("  ERREUR ligne %u: %s\n", e.line, e.message.CStr());
        return;
    }
    printf("%s\n", r.source.CStr());
}

static NkString ReadAll(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("  [!] introuvable: %s\n", path); return NkString(""); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    NkString s; s.Resize((uint32)sz);
    fread(s.Data(), 1, (size_t)sz, f); fclose(f);
    return s;
}

int main() {
    NkSLCompiler c;

    // Diagnostic ciblé : compare l'émission d'entrée d'un shader QUI MARCHE (PBR,
    // nommé main) vs le shader instancié, en HLSL-DX12. Cherche 'main(' vs 'vertmain'.
    printf("=== DIAGNOSTIC : PBR fragment — lumières GL vs VK ===\n");
    {
        NkString pbrFS = ReadAll("Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl");
        if (!pbrFS.Empty()) {
            NkSLCompileResult gl = c.Compile(pbrFS, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_GLSL);
            NkSLCompileResult vk = c.Compile(pbrFS, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_GLSL_VULKAN);
            printf("\n----- PBR FS GLSL-OpenGL (success=%d, %u octets) -----\n%s\n",
                   gl.success?1:0, (unsigned)gl.source.Size(), gl.source.CStr());
            printf("\n----- PBR FS GLSL-Vulkan (success=%d, %u octets) -----\n%s\n",
                   vk.success?1:0, (unsigned)vk.source.Size(), vk.source.CStr());
        }
    }
    printf("\n\n");
    printf("=== NkSL -> backends : VERTEX instancié ===\n");
    Check(c, NkSLTarget::NK_GLSL,        NkSLStage::NK_VERTEX, kInstancedVS, "VS GLSL-OpenGL");
    Check(c, NkSLTarget::NK_GLSL_VULKAN, NkSLStage::NK_VERTEX, kInstancedVS, "VS GLSL-Vulkan");
    Check(c, NkSLTarget::NK_HLSL_DX11,   NkSLStage::NK_VERTEX, kInstancedVS, "VS HLSL-DX11");
    Check(c, NkSLTarget::NK_HLSL_DX12,   NkSLStage::NK_VERTEX, kInstancedVS, "VS HLSL-DX12");

    printf("\n\n=== NkSL -> backends : FRAGMENT éclairé ===\n");
    Check(c, NkSLTarget::NK_GLSL,        NkSLStage::NK_FRAGMENT, kInstancedFS, "FS GLSL-OpenGL");
    Check(c, NkSLTarget::NK_GLSL_VULKAN, NkSLStage::NK_FRAGMENT, kInstancedFS, "FS GLSL-Vulkan");
    Check(c, NkSLTarget::NK_HLSL_DX11,   NkSLStage::NK_FRAGMENT, kInstancedFS, "FS HLSL-DX11");
    Check(c, NkSLTarget::NK_HLSL_DX12,   NkSLStage::NK_FRAGMENT, kInstancedFS, "FS HLSL-DX12");
    printf("\n=== FIN ===\n");
    return 0;
}
