// =============================================================================
// NkTensorGpuTest — valide le contexte GPU de NKTensor (NkTensorGpu) au runtime :
// buffers GPU, upload/download, kernel élémentaire (add) écrit en NkSL, matmul.
// =============================================================================
#include "NKTensor/NkTensorGpu.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const char* kAddNkSL = R"NKSL(
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

static int g_ok = 0, g_fail = 0;
static void check(bool cond, const char* what) {
    printf("  [%s] %s\n", cond ? " OK " : "FAIL", what);
    if (cond) ++g_ok; else ++g_fail;
}

int main() {
    printf("=== NkTensorGpuTest ===\n");
    NkTensorGpu& gpu = NkTensorGpu::Get();
    printf("GPU disponible : %d  (backend: %s)\n", gpu.IsAvailable(), gpu.BackendName());
    if (!gpu.IsAvailable()) { printf("Pas de GPU compute -> test ignoré.\n"); return 0; }

    // ---- 1) Élémentaire : C = A + B --------------------------------------------
    {
        const uint32 N = 100;
        float a[N], b[N], c[N] = {0};
        for (uint32 i = 0; i < N; i++) { a[i] = (float)i; b[i] = (float)(2 * i); }
        uint64 ba = gpu.CreateBuffer(N * sizeof(float));
        uint64 bb = gpu.CreateBuffer(N * sizeof(float));
        uint64 bc = gpu.CreateBuffer(N * sizeof(float));
        gpu.Upload(ba, a, N * sizeof(float));
        gpu.Upload(bb, b, N * sizeof(float));
        bool ran = gpu.RunBinary("add", NkString(kAddNkSL), ba, bb, bc, N);
        gpu.Download(bc, c, N * sizeof(float));
        bool ok = ran;
        for (uint32 i = 0; i < N; i++) if (fabs(c[i] - (a[i] + b[i])) > 1e-4f) ok = false;
        printf("  add: C[0..4]=[%.0f %.0f %.0f %.0f %.0f] (attendu 0 3 6 9 12)\n",
               c[0], c[1], c[2], c[3], c[4]);
        check(ok, "add elementwise (N=100) sur GPU == CPU");
        gpu.DestroyBuffer(ba); gpu.DestroyBuffer(bb); gpu.DestroyBuffer(bc);
    }

    // ---- 2) MatMul : C[2x2] = A[2x3] · B[3x2] = [[58,64],[139,154]] -------------
    {
        float a[6] = { 1,2,3,4,5,6 };
        float b[6] = { 7,8,9,10,11,12 };
        float c[4] = { 0,0,0,0 };
        uint64 ba = gpu.CreateBuffer(6 * sizeof(float));
        uint64 bb = gpu.CreateBuffer(6 * sizeof(float));
        uint64 bc = gpu.CreateBuffer(4 * sizeof(float));
        gpu.Upload(ba, a, 6 * sizeof(float));
        gpu.Upload(bb, b, 6 * sizeof(float));
        bool ran = gpu.RunMatMul(ba, bb, bc, 2, 2, 3);   // M=2,N=2,K=3
        gpu.Download(bc, c, 4 * sizeof(float));
        printf("  matmul: C=[%.0f %.0f %.0f %.0f] (attendu 58 64 139 154)\n",
               c[0], c[1], c[2], c[3]);
        bool ok = ran && fabs(c[0]-58)<0.5f && fabs(c[1]-64)<0.5f
                      && fabs(c[2]-139)<0.5f && fabs(c[3]-154)<0.5f;
        check(ok, "matmul 2x3 * 3x2 sur GPU");
        gpu.DestroyBuffer(ba); gpu.DestroyBuffer(bb); gpu.DestroyBuffer(bc);
    }

    // ---- 3) Intégration tenseur : roundtrip CPU -> GPU -> CPU ------------------
    {
        NkTensor cpu = NkTensor::Arange(0.0, 12.0);   // [0,1,...,11]
        NkTensor g   = cpu.ToGPU();
        NkTensor back = g.ToCPU();
        bool ok = g.IsValid() && back.IsValid()
               && g.Device() == NkDevice::NK_GPU
               && back.Device() == NkDevice::NK_CPU
               && back.Numel() == 12;
        if (ok) {
            const float* r = back.DataAs<float>();
            const float* o = cpu.DataAs<float>();
            for (int i = 0; i < 12; i++) if (fabs(r[i] - o[i]) > 1e-4f) ok = false;
        }
        printf("  roundtrip: back[0..3]=[%.0f %.0f %.0f %.0f] (attendu 0 1 2 3)\n",
               back.DataAs<float>()[0], back.DataAs<float>()[1],
               back.DataAs<float>()[2], back.DataAs<float>()[3]);
        check(ok, "NkTensor CPU -> ToGPU -> ToCPU preserve les donnees");
    }

    // ---- 4) API UNIFIÉE : ops::Matmul dispatché automatiquement sur GPU --------
    {
        float av[6] = { 1,2,3,4,5,6 };
        float bv[6] = { 7,8,9,10,11,12 };
        NkShape sa; sa.PushBack(2); sa.PushBack(3);
        NkShape sb; sb.PushBack(3); sb.PushBack(2);
        NkTensor a = NkTensor::FromData(sa, av, NkDType::NK_F32).ToGPU();
        NkTensor b = NkTensor::FromData(sb, bv, NkDType::NK_F32).ToGPU();
        NkTensor c = ops::Matmul(a, b);           // <- MÊME API que le CPU, routée GPU
        NkTensor cpu = c.ToCPU();
        const float* r = cpu.DataAs<float>();
        printf("  ops::Matmul(GPU) -> C=[%.0f %.0f %.0f %.0f] (attendu 58 64 139 154)\n",
               r[0], r[1], r[2], r[3]);
        bool ok = c.IsValid() && c.Device() == NkDevice::NK_GPU
               && fabs(r[0]-58)<0.5f && fabs(r[3]-154)<0.5f;
        check(ok, "ops::Matmul dispatché sur GPU (API unifiée)");
    }

    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
    gpu.Shutdown();
    return g_fail;
}
