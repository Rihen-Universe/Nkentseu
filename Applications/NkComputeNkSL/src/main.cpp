// =============================================================================
// NkComputeNkSL — preuve de bout en bout : un kernel COMPUTE écrit en NkSL,
// compilé vers le backend du device (HLSL/SPIRV/MSL) via nksl::CreateShaderFromSource,
// exécuté sur GPU HEADLESS, résultat relu et vérifié. C'est la fondation du
// backend GPU de NKTensor (on contourne NkML qui est GLSL-only).
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/SL/NkSLIntegration.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"

#include <cstring>
#include <cstdio>
#include <cmath>

using namespace nkentseu;

// Kernel NkSL : addition vectorielle C[i] = A[i] + B[i], borné par un push_constant.
static const char* kVecAdd = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;

@binding(set=0, binding=3) uniform Params { uint count; } pc;

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

static bool RunOn(NkGraphicsApi api) {
    printf("\n--- %s (headless) ---\n", NkGraphicsApiName(api)); fflush(stdout);

    NkDeviceInitInfo di;
    di.api = api;                 // pas de surface -> headless
    di.context.software.threading = true;
    NkIDevice* dev = NkDeviceFactory::Create(di);
    if (!dev || !dev->IsValid()) { printf("  device KO\n"); if (dev) NkDeviceFactory::Destroy(dev); return false; }
    if (!dev->GetCaps().computeShaders) { printf("  pas de compute\n"); NkDeviceFactory::Destroy(dev); return false; }

    // 1) Kernel NkSL -> GLSL (codegen NkSL valide pour le compute), puis on donne
    //    le GLSL au device : il le convertit vers son backend via glslang/SPIRV-Cross
    //    (chemin robuste et prouvé). On contourne ainsi le codegen NkSL->HLSL direct
    //    (bogué pour le compute SSBO : identifiants minusculisés, type d'élément).
    NkSLCompiler slc;
    NkSLCompileResult gl = slc.Compile(NkString(kVecAdd), NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
    if (!gl.success) { printf("  NkSL->GLSL KO\n"); NkDeviceFactory::Destroy(dev); return false; }

    // GLSL Vulkan -> HLSL via glslang -> SPIR-V -> SPIRV-Cross (chemin robuste).
    const uint32 sm = (api == NkGraphicsApi::NK_GFX_API_DX12) ? 60u : 50u;
    NkShaderConvertResult hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, sm, "vecadd");
    if (!hl.success) {
        printf("  GLSL->HLSL KO: %s\n", hl.errors.CStr());
        NkDeviceFactory::Destroy(dev); return false;
    }

    NkShaderDesc sd;
    sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
    sd.debugName = "vecadd";
    NkShaderHandle sh = dev->CreateShader(sd);
    if (!sh.IsValid()) { printf("  CreateShader(HLSL) KO\n"); NkDeviceFactory::Destroy(dev); return false; }

    NkComputePipelineDesc cpd; cpd.shader = sh; cpd.debugName = "vecadd";
    NkPipelineHandle pipe = dev->CreateComputePipeline(cpd);
    if (!pipe.IsValid()) { printf("  pipeline compute KO\n"); NkDeviceFactory::Destroy(dev); return false; }

    // 2) Buffers SSBO  (N=8 : taille arbitraire, bornee par le push_constant)
    const uint32 N = 8;
    float a[N], b[N];
    for (uint32 i = 0; i < N; i++) { a[i] = (float)i; b[i] = (float)(i * 10); }
    NkBufferHandle ba = dev->CreateBuffer(NkBufferDesc::Storage(N * sizeof(float), false));
    NkBufferHandle bb = dev->CreateBuffer(NkBufferDesc::Storage(N * sizeof(float), false));
    // C : buffer storage DEFAULT (UAV) — la lecture CPU passe par une copie
    // staging interne à ReadBuffer (un STAGING+UAV serait invalide en D3D11).
    NkBufferHandle bc = dev->CreateBuffer(NkBufferDesc::Storage(N * sizeof(float), false));
    dev->WriteBuffer(ba, a, N * sizeof(float));
    dev->WriteBuffer(bb, b, N * sizeof(float));

    // 3) Descriptor set (3 storage buffers)
    NkDescriptorSetLayoutDesc ld;
    ld.Add(0, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
    ld.Add(1, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
    ld.Add(2, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
    ld.Add(3, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
    NkDescSetHandle layout = dev->CreateDescriptorSetLayout(ld);

    // UBO params (count) — plus portable que push_constant (pas d'emulation cbuffer b13).
    uint32 count = N;
    NkBufferHandle bparams = dev->CreateBuffer(NkBufferDesc::Uniform(16));
    dev->WriteBuffer(bparams, &count, sizeof(count));
    NkDescSetHandle set = dev->AllocateDescriptorSet(layout);
    // IMPORTANT : binder en STORAGE_BUFFER (UAV), PAS BindUniformBuffer (qui ecrit
    // un descriptor NK_UNIFORM_BUFFER/CBV -> s.uav jamais rempli -> writes perdues).
    auto bindSSBO = [&](uint32 binding, NkBufferHandle buf) {
        NkDescriptorWrite w{};
        w.set = set; w.binding = binding;
        w.type = NkDescriptorType::NK_STORAGE_BUFFER;
        w.buffer = buf;
        dev->UpdateDescriptorSets(&w, 1);
    };
    bindSSBO(0, ba);
    bindSSBO(1, bb);
    bindSSBO(2, bc);
    dev->BindUniformBuffer(set, 3, bparams);   // UBO params -> cbuffer b3

    // 4) Dispatch
    auto* cmd = dev->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set, 0);
    cmd->Dispatch((N + 63) / 64, 1, 1);
    cmd->UAVBarrier(bc);
    cmd->End();
    dev->Submit(&cmd, 1);

    // Diagnostic upload/readback : relire A (uploadé a[i]=i)
    float chkA[N] = { 0 };
    bool rbA = dev->ReadBuffer(ba, chkA, N * sizeof(float));
    printf("  [diag] ReadBuffer(A)=%d  A[0..4]=[%.0f %.0f %.0f %.0f %.0f] (attendu 0 1 2 3 4)\n",
           rbA, chkA[0], chkA[1], chkA[2], chkA[3], chkA[4]);

    // 5) Readback + vérification (on affiche les 8 premiers)
    float c[N] = { 0 };
    dev->ReadBuffer(bc, c, N * sizeof(float));

    bool ok = true;
    printf("  C[0..7] = [");
    for (uint32 i = 0; i < 8; i++) printf("%.0f ", c[i]);
    printf("]  (attendu 0 11 22 33 44 55 66 77)\n");
    for (uint32 i = 0; i < N; i++) if (fabs(c[i] - (a[i] + b[i])) > 0.01f) ok = false;

    dev->FreeDescriptorSet(set);
    dev->DestroyCommandBuffer(cmd);
    dev->DestroyBuffer(ba); dev->DestroyBuffer(bb); dev->DestroyBuffer(bc); dev->DestroyBuffer(bparams);
    dev->DestroyPipeline(pipe);
    dev->DestroyShader(sh);
    dev->DestroyDescriptorSetLayout(layout);
    NkDeviceFactory::Destroy(dev);
    return ok;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "hlsl") == 0) {
        NkSLCompiler c;
        NkSLCompileResult gv = c.Compile(NkString(kVecAdd), NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
        NkShaderConvertResult hl = NkShaderConverter::GlslToHlsl(gv.source, NkSLStage::NK_COMPUTE, 50, "vecadd");
        printf("%s\n", hl.success ? hl.source.CStr() : hl.errors.CStr());
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "dump") == 0) {
        NkSLCompiler c;
        NkSLCompileResult gv = c.Compile(NkString(kVecAdd), NkSLStage::NK_COMPUTE, NkSLTarget::NK_GLSL_VULKAN);
        printf("\n===== NkSL -> GLSL-Vulkan (success=%d) =====\n%s\n", gv.success, gv.source.CStr());
        if (gv.success) {
            NkShaderConvertResult hl = NkShaderConverter::GlslToHlsl(gv.source, NkSLStage::NK_COMPUTE, 50, "vecadd");
            printf("\n===== GLSL-Vulkan -> HLSL (SPIRV-Cross) (success=%d) =====\n%s\nERR: %s\n",
                   hl.success, hl.success ? hl.source.CStr() : "(echec)", hl.errors.CStr());
        }
        return 0;
    }
    printf("=== NkComputeNkSL : kernel NkSL exécuté sur GPU headless ===\n"); fflush(stdout);
    bool ok = false;
    if (RunOn(NkGraphicsApi::NK_GFX_API_DX11)) { printf("\n>>> DX11 : NkSL compute FONCTIONNEL\n"); ok = true; }
    if (!ok) printf("\n>>> Echec NkSL compute headless (DX11).\n");
    return ok ? 0 : 1;
}
