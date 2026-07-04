// =============================================================================
// NkSLComputeCheck — vérifie que le COMPUTE NkSL se convertit vers TOUS les
// backends (GLSL-OpenGL, GLSL-Vulkan, SPIR-V, HLSL-DX11, HLSL-DX12, MSL Metal,
// MSL via SPIRV-Cross). Sans GPU : on compile et on inspecte le code généré.
//
// Répond à : « NkSL fait-il déjà la conversion du compute, y compris vers Metal ? »
// =============================================================================
#include "NKSL/Compiler/NkSLCompiler.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;

// --- Shader compute NkSL : addition vectorielle C[i] = A[i] + B[i] -----------
// Storage buffers (SSBO), push constant (count), workgroup 64, builtin invocation.
static const char* kVecAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@push_constant
uniform Params { uint count; } pc;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) {
        C.data[i] = A.data[i] + B.data[i];
    }
}
)NKSL";

// --- Shader compute NkSL : matmul naïf (le kernel qui servira à NKTensor) -----
static const char* kMatmul = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@push_constant
uniform Params { uint M; uint K; uint N; } pc;

layout(local_size_x = 16, local_size_y = 16) in;

@stage(compute)
@entry
void main() {
    uint row = gl_GlobalInvocationID.y;
    uint col = gl_GlobalInvocationID.x;
    if (row < pc.M && col < pc.N) {
        float acc = 0.0;
        for (uint k = 0u; k < pc.K; k = k + 1u) {
            acc = acc + A.data[row * pc.K + k] * B.data[k * pc.N + col];
        }
        C.data[row * pc.N + col] = acc;
    }
}
)NKSL";

// --- Vertex shader NkSL minimal ---------------------------------------------
static const char* kVS = R"NKSL(
@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;
@location(0) out vec3 vNormal;

@stage(vertex)
@entry
void main() {
    vNormal = aNormal;
    gl_Position = vec4(aPos, 1.0);
}
)NKSL";

// --- Fragment shader NkSL minimal -------------------------------------------
static const char* kFS = R"NKSL(
@location(0) in vec3 vNormal;
@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3 N = normalize(vNormal);
    float d = max(dot(N, vec3(0.4, 0.8, 0.5)), 0.0);
    fragColor = vec4(vec3(0.2) + vec3(0.8) * d, 1.0);
}
)NKSL";

static int g_ok = 0, g_fail = 0;

static void Convert(NkSLCompiler& c, const char* shaderName, const char* src,
                    NkSLStage stage, NkSLTarget t, bool showSnippet) {
    (void)showSnippet;
    const char* tn = NkSLTargetName(t);
    printf("  ... %-14s %-16s : ", shaderName, tn); fflush(stdout);
    NkSLCompileResult r = c.Compile(NkString(src), stage, t);
    if (r.success) {
        ++g_ok;
        if (t == NkSLTarget::NK_SPIRV) {
            const unsigned bin = (unsigned)r.bytecode.Size();
            printf("OK (SPIR-V binaire : %u mots)\n", bin / 4);
        } else {
            printf("OK (%u octets de code généré)\n", (unsigned)r.source.Size());
        }
        fflush(stdout);
    } else {
        ++g_fail;
        printf("FAIL\n");
        for (uint32 i = 0; i < r.errors.Size() && i < 3; i++)
            printf("         ligne %u: %s\n", r.errors[i].line, r.errors[i].message.CStr());
        fflush(stdout);
    }
}

int main(int argc, char** argv) {
    const NkSLTarget targets[] = {
        NkSLTarget::NK_GLSL,           // 0 OpenGL compute
        NkSLTarget::NK_GLSL_VULKAN,    // 1 Vulkan compute (GLSL)
        NkSLTarget::NK_SPIRV,          // 2 Vulkan compute (SPIR-V binaire)
        NkSLTarget::NK_HLSL_DX11,      // 3 DX11 compute (CS 5.0)
        NkSLTarget::NK_HLSL_DX12,      // 4 DX12 compute (SM6+)
        NkSLTarget::NK_MSL,            // 5 Metal compute (kernel, natif)
        NkSLTarget::NK_MSL_SPIRV_CROSS // 6 Metal compute (via SPIRV-Cross)
    };
    const int NT = (int)(sizeof(targets) / sizeof(targets[0]));

    NkSLCompiler c;

    // Mode dump : argv[1]="dump" -> affiche le MSL généré du VS et du FS (preuve).
    if (argc > 1 && NkString(argv[1]) == NkString("dump")) {
        NkSLCompileResult vs = c.Compile(NkString(kVS), NkSLStage::NK_VERTEX,   NkSLTarget::NK_MSL);
        NkSLCompileResult fs = c.Compile(NkString(kFS), NkSLStage::NK_FRAGMENT, NkSLTarget::NK_MSL);
        NkSLCompileResult cs = c.Compile(NkString(kMatmul), NkSLStage::NK_COMPUTE, NkSLTarget::NK_MSL);
        printf("===== VS -> MSL (Metal) =====\n%s\n", vs.success ? vs.source.CStr() : "ECHEC");
        printf("\n===== FS -> MSL (Metal) =====\n%s\n", fs.success ? fs.source.CStr() : "ECHEC");
        printf("\n===== Matmul (compute) -> MSL (Metal) =====\n%s\n", cs.success ? cs.source.CStr() : "ECHEC");
        return (vs.success && fs.success && cs.success) ? 0 : 1;
    }

    // Mode isolé : argv[1] = index de cible (un process par cible).
    if (argc > 1) {
        int idx = atoi(argv[1]);
        if (idx < 0 || idx >= NT) { printf("index invalide\n"); return 2; }
        NkSLTarget t = targets[idx];
        printf("[cible %d = %s]\n", idx, NkSLTargetName(t));
        Convert(c, "VecAdd(cs)", kVecAdd, NkSLStage::NK_COMPUTE,  t, false);
        Convert(c, "Matmul(cs)", kMatmul, NkSLStage::NK_COMPUTE,  t, false);
        Convert(c, "VS(vertex)", kVS,     NkSLStage::NK_VERTEX,   t, false);
        Convert(c, "FS(frag)",   kFS,     NkSLStage::NK_FRAGMENT, t, false);
        return g_fail;
    }

    printf("=== NkSLComputeCheck — conversion NkSL (compute + VS + FS) vers tous backends ===\n\n");
    printf("[COMPUTE] VecAdd\n");
    for (NkSLTarget t : targets) Convert(c, "VecAdd(cs)", kVecAdd, NkSLStage::NK_COMPUTE, t, false);
    printf("\n[COMPUTE] Matmul\n");
    for (NkSLTarget t : targets) Convert(c, "Matmul(cs)", kMatmul, NkSLStage::NK_COMPUTE, t, false);
    printf("\n[VERTEX] VS\n");
    for (NkSLTarget t : targets) Convert(c, "VS(vertex)", kVS, NkSLStage::NK_VERTEX, t, false);
    printf("\n[FRAGMENT] FS\n");
    for (NkSLTarget t : targets) Convert(c, "FS(frag)", kFS, NkSLStage::NK_FRAGMENT, t, false);
    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
    return g_fail;
}
