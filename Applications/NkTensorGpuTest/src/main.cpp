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

    // ---- 5) Ops ÉLÉMENTAIRES dispatchées sur GPU (résidence) : Sub/Mul/Relu/Sig/Tanh
    //         Même API ops:: que le CPU ; on compare au CPU de référence. -----------
    {
        const int64 N = 1024;
        NkTensor a = NkTensor::Arange(-512.0, 512.0);     // [-512 .. 511], Numel=1024
        NkTensor b = NkTensor::Arange(0.0, 1024.0);       // [0 .. 1023]
        // Référence CPU
        NkTensor rSub = ops::Sub(a, b);
        NkTensor rMul = ops::Mul(a, b);
        NkTensor rRel = ops::Relu(a);
        NkTensor rSig = ops::Sigmoid(a);
        NkTensor rTan = ops::Tanh(a);
        // Versions GPU (opérandes résidents GPU -> dispatch automatique)
        NkTensor ag = a.ToGPU(), bg = b.ToGPU();
        auto maxErr = [N](const NkTensor& gpuRes, const NkTensor& cpuRef) -> float {
            NkTensor back = gpuRes.ToCPU();
            if (!back.IsValid() || back.Numel() != N) return 1e9f;
            const float* r = back.DataAs<float>();
            const float* o = cpuRef.DataAs<float>();
            float e = 0.0f;
            for (int64 i = 0; i < N; i++) { float d = fabsf(r[i] - o[i]); if (d > e) e = d; }
            return e;
        };
        NkTensor gSub = ops::Sub(ag, bg);
        NkTensor gMul = ops::Mul(ag, bg);
        NkTensor gRel = ops::Relu(ag);
        NkTensor gSig = ops::Sigmoid(ag);
        NkTensor gTan = ops::Tanh(ag);
        float eSub = maxErr(gSub, rSub), eMul = maxErr(gMul, rMul);
        float eRel = maxErr(gRel, rRel), eSig = maxErr(gSig, rSig), eTan = maxErr(gTan, rTan);
        printf("  err max GPU vs CPU : sub=%.2e mul=%.2e relu=%.2e sigmoid=%.2e tanh=%.2e\n",
               eSub, eMul, eRel, eSig, eTan);
        check(gSub.Device() == NkDevice::NK_GPU && eSub < 1e-3f, "ops::Sub  dispatché GPU == CPU");
        check(gMul.Device() == NkDevice::NK_GPU && eMul < 1e-1f, "ops::Mul  dispatché GPU == CPU");
        check(gRel.Device() == NkDevice::NK_GPU && eRel < 1e-3f, "ops::Relu dispatché GPU == CPU");
        check(gSig.Device() == NkDevice::NK_GPU && eSig < 1e-3f, "ops::Sigmoid dispatché GPU == CPU");
        check(gTan.Device() == NkDevice::NK_GPU && eTan < 1e-3f, "ops::Tanh dispatché GPU == CPU");
    }

    // ---- 6) RÉDUCTIONS dispatchées sur GPU : Sum/Mean/Max global + par axe -------
    {
        float m[12] = { 1,2,3,  4,5,6,  7,8,9,  10,11,12 };   // [4,3] connu
        NkShape s; s.PushBack(4); s.PushBack(3);
        NkTensor cpu = NkTensor::FromData(s, m, NkDType::NK_F32);
        NkTensor g   = cpu.ToGPU();
        auto scal = [](const NkTensor& t) -> float {
            NkTensor c = t.ToCPU(); return c.IsValid() ? c.DataAs<float>()[0] : 1e9f;
        };
        auto errV = [](const NkTensor& gr, const NkTensor& cr) -> float {
            NkTensor b = gr.ToCPU();
            if (!b.IsValid() || b.Numel() != cr.Numel()) return 1e9f;
            const float* x = b.DataAs<float>(); const float* y = cr.DataAs<float>();
            float e = 0.0f; for (int64 i = 0; i < cr.Numel(); i++) { float d = fabsf(x[i]-y[i]); if (d>e) e=d; }
            return e;
        };
        // Global (attendus : somme=78, moyenne=6.5, max=12)
        check(fabsf(scal(ops::Sum(g))  - 78.0f) < 1e-3f, "ops::Sum  global GPU (=78)");
        check(fabsf(scal(ops::Mean(g)) -  6.5f) < 1e-3f, "ops::Mean global GPU (=6.5)");
        check(fabsf(scal(ops::Max(g))  - 12.0f) < 1e-3f, "ops::Max  global GPU (=12)");
        // Par axe : axe0 -> [3]=[22,26,30], axe1 -> [4]=[6,15,24,33]
        float e0s = errV(ops::Sum(g,0),  ops::Sum(cpu,0));
        float e1s = errV(ops::Sum(g,1),  ops::Sum(cpu,1));
        float e0m = errV(ops::Mean(g,0), ops::Mean(cpu,0));
        float e1x = errV(ops::Max(g,1),  ops::Max(cpu,1));
        printf("  err reductions axe : sum0=%.2e sum1=%.2e mean0=%.2e max1=%.2e\n", e0s, e1s, e0m, e1x);
        check(ops::Sum(g,0).Device() == NkDevice::NK_GPU && e0s < 1e-3f, "ops::Sum  axe0 dispatché GPU == CPU");
        check(e1s < 1e-3f, "ops::Sum  axe1 GPU == CPU");
        check(e0m < 1e-3f, "ops::Mean axe0 GPU == CPU");
        check(e1x < 1e-3f, "ops::Max  axe1 GPU == CPU");
    }

    // ---- 7) PERMUTE / TRANSPOSE sur GPU (gather par strides, résidence) ---------
    {
        // Transpose 2D : [3,4] -> [4,3]
        float m[12] = { 1,2,3,4,  5,6,7,8,  9,10,11,12 };
        NkShape s; s.PushBack(3); s.PushBack(4);
        NkTensor cpu  = NkTensor::FromData(s, m, NkDType::NK_F32);
        NkTensor cpuT = cpu.Transpose(0, 1).Contiguous();          // référence CPU
        NkTensor gT   = cpu.ToGPU().Transpose(0, 1).Contiguous();  // gather GPU
        NkTensor back = gT.ToCPU();
        float e = 0.0f; const float* x = back.DataAs<float>(); const float* y = cpuT.DataAs<float>();
        for (int64 i = 0; i < 12; i++) { float d = fabsf(x[i]-y[i]); if (d>e) e=d; }
        printf("  transpose GPU back[0..3]=[%.0f %.0f %.0f %.0f] (attendu 1 5 9 2)\n", x[0], x[1], x[2], x[3]);
        check(gT.Device() == NkDevice::NK_GPU && back.Numel() == 12 && e < 1e-4f,
              "Transpose+Contiguous sur GPU == CPU");

        // Permute 3D : [2,3,4] -> ordre (2,0,1) = [4,2,3]
        float m3[24]; for (int i = 0; i < 24; i++) m3[i] = (float)i;
        NkShape s3; s3.PushBack(2); s3.PushBack(3); s3.PushBack(4);
        NkTensor c3 = NkTensor::FromData(s3, m3, NkDType::NK_F32);
        NkShape ord; ord.PushBack(2); ord.PushBack(0); ord.PushBack(1);
        NkTensor refP = c3.Permute(ord).Contiguous();
        NkTensor gP   = c3.ToGPU().Permute(ord).Contiguous();
        NkTensor bP   = gP.ToCPU();
        float e2 = 0.0f; const float* xp = bP.DataAs<float>(); const float* yp = refP.DataAs<float>();
        for (int64 i = 0; i < 24; i++) { float d = fabsf(xp[i]-yp[i]); if (d>e2) e2=d; }
        check(gP.Device() == NkDevice::NK_GPU && bP.Numel() == 24 && e2 < 1e-4f,
              "Permute 3D (2,0,1)+Contiguous sur GPU == CPU");
    }

    // ---- 8) im2col / col2im sur GPU (conv résidente) == référence CPU -----------
    {
        const int64 B=1, Cin=2, H=3, W=3, kH=2, kW=2, stride=1, pad=0;
        const int64 outH=2, outW=2, K=Cin*kH*kW, M=B*outH*outW;
        float xd[18]; for (int i = 0; i < 18; i++) xd[i] = (float)(i + 1);   // x [1,2,3,3]
        NkShape xs; xs.PushBack(1); xs.PushBack(2); xs.PushBack(3); xs.PushBack(3);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);

        // Référence CPU im2col (même formule que le noyau)
        float ref[32];
        for (int64 b=0;b<B;b++) for (int64 oh=0;oh<outH;oh++) for (int64 ow=0;ow<outW;ow++) {
            int64 row=(b*outH+oh)*outW+ow;
            for (int64 ic=0;ic<Cin;ic++) for (int64 ky=0;ky<kH;ky++) for (int64 kx=0;kx<kW;kx++) {
                int64 iy=oh*stride-pad+ky, ix=ow*stride-pad+kx, kcol=(ic*kH+ky)*kW+kx;
                ref[row*K+kcol] = (iy>=0&&iy<H&&ix>=0&&ix<W) ? xd[((b*Cin+ic)*H+iy)*W+ix] : 0.f;
            }
        }
        NkTensor gcol = NkGpuIm2Col(x.ToGPU(), kH,kW,stride,pad,outH,outW);
        NkTensor bcol = gcol.ToCPU();
        float e=0; const float* c=bcol.DataAs<float>();
        for (int64 i=0;i<M*K;i++){ float d=fabsf(c[i]-ref[i]); if (d>e) e=d; }
        check(gcol.Device()==NkDevice::NK_GPU && bcol.Numel()==M*K && e<1e-4f, "im2col GPU == CPU");

        // Référence CPU col2im (accumulation)
        float refx[18]; for (int i=0;i<18;i++) refx[i]=0.f;
        for (int64 b=0;b<B;b++) for (int64 oh=0;oh<outH;oh++) for (int64 ow=0;ow<outW;ow++) {
            int64 row=(b*outH+oh)*outW+ow;
            for (int64 ic=0;ic<Cin;ic++) for (int64 ky=0;ky<kH;ky++) for (int64 kx=0;kx<kW;kx++) {
                int64 iy=oh*stride-pad+ky, ix=ow*stride-pad+kx;
                if (iy>=0&&iy<H&&ix>=0&&ix<W){ int64 kcol=(ic*kH+ky)*kW+kx; refx[((b*Cin+ic)*H+iy)*W+ix]+=ref[row*K+kcol]; }
            }
        }
        NkTensor gdx = NkGpuCol2Im(gcol, B,Cin,H,W,kH,kW,stride,pad,outH,outW);
        NkTensor bdx = gdx.ToCPU();
        float e2=0; const float* dxp=bdx.DataAs<float>();
        for (int64 i=0;i<B*Cin*H*W;i++){ float d=fabsf(dxp[i]-refx[i]); if (d>e2) e2=d; }
        printf("  im2col err=%.2e  col2im err=%.2e\n", e, e2);
        check(gdx.Device()==NkDevice::NK_GPU && bdx.Numel()==18 && e2<1e-4f, "col2im GPU == CPU");
    }

    // ---- 9) Softmax par ligne sur GPU == référence CPU -------------------------
    {
        float m[8] = { 1,2,3,4,  2,1,0,-1 };   // [2,4]
        NkShape s; s.PushBack(2); s.PushBack(4);
        NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
        NkTensor gsm = NkGpuSoftmaxRows(x.ToGPU());
        NkTensor bsm = gsm.ToCPU();
        float ref[8];
        for (int r = 0; r < 2; r++) {
            const float* row = m + r * 4; float mx = row[0];
            for (int c = 1; c < 4; c++) if (row[c] > mx) mx = row[c];
            double sum = 0; float e[4];
            for (int c = 0; c < 4; c++) { e[c] = (float)std::exp((double)(row[c] - mx)); sum += e[c]; }
            for (int c = 0; c < 4; c++) ref[r * 4 + c] = (float)(e[c] / sum);
        }
        float es = 0; const float* p = bsm.DataAs<float>();
        for (int i = 0; i < 8; i++) { float d = fabsf(p[i] - ref[i]); if (d > es) es = d; }
        check(gsm.Device() == NkDevice::NK_GPU && es < 1e-4f, "softmax par ligne GPU == CPU");
    }

    // ---- 10) MaxPool2D GPU (forward + backward) == référence CPU ---------------
    {
        // x[1,1,4,4], kernel=2, stride=2 -> [1,1,2,2]
        float xd[16] = { 1,2,3,4,  5,6,7,8,  9,10,11,12,  13,14,15,16 };
        NkShape xs; xs.PushBack(1); xs.PushBack(1); xs.PushBack(4); xs.PushBack(4);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
        NkTensor arg;
        NkTensor out = NkGpuMaxPool2D(x.ToGPU(), 2, 2, arg);
        NkTensor oc = out.ToCPU();
        // Réf CPU : max de chaque fenêtre 2x2 -> [6,8,14,16]
        float refOut[4] = { 6, 8, 14, 16 };
        float eo = 0; const float* op = oc.DataAs<float>();
        for (int i = 0; i < 4; i++) { float d = fabsf(op[i] - refOut[i]); if (d > eo) eo = d; }
        check(out.Device() == NkDevice::NK_GPU && eo < 1e-4f, "maxpool2d forward GPU == CPU");
        // Backward : grad=ones[1,1,2,2] -> dX vaut 1 aux argmax (positions 6,8,14,16), 0 sinon.
        NkTensor grad = NkTensor::Ones(NkShape{ (int64)1, (int64)1, (int64)2, (int64)2 });
        NkTensor dX = NkGpuMaxPool2DBackward(grad.ToGPU(), arg, 1, 1, 4, 4, 2, 2, 2, 2);
        NkTensor dc = dX.ToCPU();
        const float* dp = dc.DataAs<float>();
        double sum = 0; for (int i = 0; i < 16; i++) sum += dp[i];
        // 4 fenêtres -> exactement 4 positions à 1 ; les argmax sont les indices 5,7,13,15.
        bool routed = (dp[5] == 1.f && dp[7] == 1.f && dp[13] == 1.f && dp[15] == 1.f);
        check(dX.Device() == NkDevice::NK_GPU && fabs(sum - 4.0) < 1e-4 && routed,
              "maxpool2d backward GPU (grad routé aux argmax) == CPU");
    }

    // ---- 11) Exp GPU == CPU ----------------------------------------------------
    {
        float m[6] = { -1, 0, 1, 2, -0.5f, 0.5f };
        NkShape s; s.PushBack(6);
        NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
        NkTensor g = ops::Exp(x.ToGPU());
        NkTensor b = g.ToCPU();
        float e = 0; const float* p = b.DataAs<float>();
        for (int i = 0; i < 6; i++) { float d = fabsf(p[i] - (float)std::exp((double)m[i])); if (d > e) e = d; }
        check(g.Device() == NkDevice::NK_GPU && e < 1e-4f, "exp GPU == CPU");
    }

    // ---- 12) Upsample2x GPU (forward + backward) == CPU ------------------------
    {
        float xd[4] = { 1, 2, 3, 4 };   // [1,1,2,2]
        NkShape xs; xs.PushBack(1); xs.PushBack(1); xs.PushBack(2); xs.PushBack(2);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
        NkTensor up = NkGpuUpsample2x(x.ToGPU());
        NkTensor uc = up.ToCPU(); const float* u = uc.DataAs<float>();
        // nearest ×2 : out[oy,ox]=in[oy/2,ox/2] -> ligne0 = 1 1 2 2, etc.
        float ref[16]; for (int oy = 0; oy < 4; oy++) for (int ox = 0; ox < 4; ox++) ref[oy*4+ox] = xd[(oy/2)*2 + (ox/2)];
        float eu = 0; for (int i = 0; i < 16; i++) { float d = fabsf(u[i]-ref[i]); if (d>eu) eu=d; }
        check(up.Device() == NkDevice::NK_GPU && eu < 1e-4f, "upsample2x forward GPU == CPU");
        // backward : grad=ones[1,1,4,4] -> chaque entrée reçoit 4.
        NkTensor grad = NkTensor::Ones(NkShape{ (int64)1,(int64)1,(int64)4,(int64)4 });
        NkTensor dIn = NkGpuUpsample2xBackward(grad.ToGPU(), 1, 1, 2, 2);
        NkTensor dc = dIn.ToCPU(); const float* dpp = dc.DataAs<float>();
        bool ok = true; for (int i = 0; i < 4; i++) if (fabsf(dpp[i]-4.f) > 1e-4f) ok = false;
        check(dIn.Device() == NkDevice::NK_GPU && ok, "upsample2x backward GPU == CPU");
    }

    // ---- 13) ConvTranspose2D GPU (forward + dX + dW) == CPU --------------------
    {
        // x[1,1,2,2], w[1,1,2,2], stride1 pad0 -> y[1,1,3,3]
        float xd[4] = { 1, 2, 3, 4 }; float wd[4] = { 1, 1, 1, 1 };
        NkShape xs; xs.PushBack(1); xs.PushBack(1); xs.PushBack(2); xs.PushBack(2);
        NkShape ws; ws.PushBack(1); ws.PushBack(1); ws.PushBack(2); ws.PushBack(2);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
        NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
        NkTensor y = NkGpuConvTranspose2D(x.ToGPU(), w.ToGPU(), 1, 0);
        NkTensor yc = y.ToCPU(); const float* yp = yc.DataAs<float>();
        float ref[9];
        for (int oy = 0; oy < 3; oy++) for (int ox = 0; ox < 3; ox++) {
            float s = 0;
            for (int ky = 0; ky < 2; ky++) for (int kx = 0; kx < 2; kx++) {
                int iy = oy - ky, ix = ox - kx;
                if (iy >= 0 && iy < 2 && ix >= 0 && ix < 2) s += xd[iy*2+ix] * wd[ky*2+kx];
            }
            ref[oy*3+ox] = s;
        }
        float ef = 0; for (int i = 0; i < 9; i++) { float d = fabsf(yp[i]-ref[i]); if (d>ef) ef=d; }
        check(y.Device() == NkDevice::NK_GPU && ef < 1e-4f, "convT2d forward GPU == CPU");
        // dX avec grad=ones[1,1,3,3] -> chaque dX = somme(w) = 4.
        NkTensor grad = NkTensor::Ones(NkShape{ (int64)1,(int64)1,(int64)3,(int64)3 });
        NkTensor dX = NkGpuConvTranspose2DBackwardX(grad.ToGPU(), w.ToGPU(), 1,1,2,2,1,2,2,1,0,3,3);
        NkTensor dxc = dX.ToCPU(); const float* dxp = dxc.DataAs<float>();
        bool okx = true; for (int i = 0; i < 4; i++) if (fabsf(dxp[i]-4.f) > 1e-4f) okx = false;
        check(dX.Device() == NkDevice::NK_GPU && okx, "convT2d dX GPU == CPU");
        // dW avec grad=ones -> chaque dW = somme(x) = 10.
        NkTensor dW = NkGpuConvTranspose2DBackwardW(x.ToGPU(), grad.ToGPU(), 1,1,2,2,1,2,2,1,0,3,3);
        NkTensor dwc = dW.ToCPU(); const float* dwp = dwc.DataAs<float>();
        bool okw = true; for (int i = 0; i < 4; i++) if (fabsf(dwp[i]-10.f) > 1e-4f) okw = false;
        check(dW.Device() == NkDevice::NK_GPU && okw, "convT2d dW GPU == CPU");
    }

    // ---- 14) Conv3D GPU (forward + dX + dW) == CPU ----------------------------
    {
        // x[1,1,3,3,3], w[1,1,2,2,2]=ones, stride1 pad0 -> y[1,1,2,2,2]
        float xd[27]; for (int i = 0; i < 27; i++) xd[i] = (float)(i + 1);
        float wd[8];  for (int i = 0; i < 8; i++)  wd[i]  = 1.f;
        NkShape xs; xs.PushBack(1); xs.PushBack(1); xs.PushBack(3); xs.PushBack(3); xs.PushBack(3);
        NkShape ws; ws.PushBack(1); ws.PushBack(1); ws.PushBack(2); ws.PushBack(2); ws.PushBack(2);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
        NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
        // forward GPU
        NkTensor y = NkGpuConv3D(x.ToGPU(), w.ToGPU(), 1, 0);
        NkTensor yc = y.ToCPU(); const float* yp = yc.DataAs<float>();
        float refY[8];
        for (int od=0;od<2;od++) for (int oy=0;oy<2;oy++) for (int ox=0;ox<2;ox++) {
            float s=0; for (int kz=0;kz<2;kz++) for (int ky=0;ky<2;ky++) for (int kx=0;kx<2;kx++)
                s += xd[(od+kz)*9+(oy+ky)*3+(ox+kx)] * wd[kz*4+ky*2+kx];
            refY[od*4+oy*2+ox]=s;
        }
        float ef=0; for (int i=0;i<8;i++){float d=fabsf(yp[i]-refY[i]); if(d>ef)ef=d;}
        check(y.Device()==NkDevice::NK_GPU && ef<1e-3f, "conv3d forward GPU == CPU");
        // dX avec grad=ones[1,1,2,2,2]
        NkTensor grad = NkTensor::Ones(NkShape{(int64)1,(int64)1,(int64)2,(int64)2,(int64)2});
        NkTensor dX = NkGpuConv3DBackwardX(grad.ToGPU(), w.ToGPU(), x.ToGPU(), 1, 0);
        NkTensor dxc = dX.ToCPU(); const float* dxp = dxc.DataAs<float>();
        float refDX[27];
        for (int iz=0;iz<3;iz++) for (int iy=0;iy<3;iy++) for (int ix=0;ix<3;ix++) {
            float s=0; for (int kz=0;kz<2;kz++) for (int ky=0;ky<2;ky++) for (int kx=0;kx<2;kx++) {
                int od=iz-kz,oy=iy-ky,ox=ix-kx;
                if (od>=0&&od<2&&oy>=0&&oy<2&&ox>=0&&ox<2) s+=wd[kz*4+ky*2+kx];
            }
            refDX[iz*9+iy*3+ix]=s;
        }
        float ex=0; for (int i=0;i<27;i++){float d=fabsf(dxp[i]-refDX[i]); if(d>ex)ex=d;}
        check(dX.Device()==NkDevice::NK_GPU && ex<1e-3f, "conv3d dX GPU == CPU");
        // dW avec grad=ones
        NkTensor dW = NkGpuConv3DBackwardW(grad.ToGPU(), x.ToGPU(), w.ToGPU(), 1, 0);
        NkTensor dwc = dW.ToCPU(); const float* dwp = dwc.DataAs<float>();
        float refDW[8];
        for (int kz=0;kz<2;kz++) for (int ky=0;ky<2;ky++) for (int kx=0;kx<2;kx++) {
            float s=0; for (int od=0;od<2;od++) for (int oy=0;oy<2;oy++) for (int ox=0;ox<2;ox++)
                s += xd[(od+kz)*9+(oy+ky)*3+(ox+kx)];
            refDW[kz*4+ky*2+kx]=s;
        }
        float ew=0; for (int i=0;i<8;i++){float d=fabsf(dwp[i]-refDW[i]); if(d>ew)ew=d;}
        check(dW.Device()==NkDevice::NK_GPU && ew<1e-3f, "conv3d dW GPU == CPU");
    }

    // ---- 15) ConvTranspose3D GPU (forward + dX + dW) == CPU -------------------
    {
        // x[1,1,2,2,2], w[1,1,2,2,2]=ones, stride1 pad0 -> y[1,1,3,3,3]
        float xd[8]; for (int i=0;i<8;i++) xd[i]=(float)(i+1);   // somme = 36
        float wd[8]; for (int i=0;i<8;i++) wd[i]=1.f;            // somme = 8
        NkShape xs; xs.PushBack(1); xs.PushBack(1); xs.PushBack(2); xs.PushBack(2); xs.PushBack(2);
        NkShape ws; ws.PushBack(1); ws.PushBack(1); ws.PushBack(2); ws.PushBack(2); ws.PushBack(2);
        NkTensor x = NkTensor::FromData(xs, xd, NkDType::NK_F32);
        NkTensor w = NkTensor::FromData(ws, wd, NkDType::NK_F32);
        NkTensor y = NkGpuConvTranspose3D(x.ToGPU(), w.ToGPU(), 1, 0);
        NkTensor yc = y.ToCPU(); const float* yp = yc.DataAs<float>();
        float refY[27];
        for (int od=0;od<3;od++) for (int oy=0;oy<3;oy++) for (int ox=0;ox<3;ox++) {
            float s=0; for (int kz=0;kz<2;kz++) for (int ky=0;ky<2;ky++) for (int kx=0;kx<2;kx++) {
                int iz=od-kz,iy=oy-ky,ix=ox-kx;
                if (iz>=0&&iz<2&&iy>=0&&iy<2&&ix>=0&&ix<2) s+=xd[iz*4+iy*2+ix]*wd[kz*4+ky*2+kx];
            }
            refY[od*9+oy*3+ox]=s;
        }
        float ef=0; for (int i=0;i<27;i++){float d=fabsf(yp[i]-refY[i]); if(d>ef)ef=d;}
        check(y.Device()==NkDevice::NK_GPU && ef<1e-3f, "convT3d forward GPU == CPU");
        // dX (grad=ones[1,1,3,3,3]) -> chaque = somme(w) = 8 ; dW -> chaque = somme(x) = 36.
        NkTensor grad = NkTensor::Ones(NkShape{(int64)1,(int64)1,(int64)3,(int64)3,(int64)3});
        NkTensor dX = NkGpuConvTranspose3DBackwardX(grad.ToGPU(), w.ToGPU(), x.ToGPU(), 1, 0);
        NkTensor dxc = dX.ToCPU(); const float* dxp = dxc.DataAs<float>();
        bool okx=true; for (int i=0;i<8;i++) if (fabsf(dxp[i]-8.f)>1e-3f) okx=false;
        check(dX.Device()==NkDevice::NK_GPU && okx, "convT3d dX GPU == CPU");
        NkTensor dW = NkGpuConvTranspose3DBackwardW(grad.ToGPU(), x.ToGPU(), w.ToGPU(), 1, 0);
        NkTensor dwc = dW.ToCPU(); const float* dwp = dwc.DataAs<float>();
        bool okw=true; for (int i=0;i<8;i++) if (fabsf(dwp[i]-36.f)>1e-3f) okw=false;
        check(dW.Device()==NkDevice::NK_GPU && okw, "convT3d dW GPU == CPU");
    }

    // ---- 16) Matmul par LOTS (batched) GPU == CPU -----------------------------
    {
        // [2,2,3] · [2,3,2] -> [2,2,2]
        float ad[12]; for (int i=0;i<12;i++) ad[i]=(float)(i+1);
        float bd[12]; for (int i=0;i<12;i++) bd[i]=(float)(i+1)*0.5f;
        NkShape as; as.PushBack(2); as.PushBack(2); as.PushBack(3);
        NkShape bs; bs.PushBack(2); bs.PushBack(3); bs.PushBack(2);
        NkTensor A = NkTensor::FromData(as, ad, NkDType::NK_F32);
        NkTensor B = NkTensor::FromData(bs, bd, NkDType::NK_F32);
        NkTensor cCpu = ops::Matmul(A, B);                    // CPU batched
        NkTensor cGpu = ops::Matmul(A.ToGPU(), B.ToGPU());    // GPU batched
        NkTensor cg = cGpu.ToCPU();
        float e = 0; const float* x = cg.DataAs<float>(); const float* y = cCpu.DataAs<float>();
        for (int i = 0; i < 8; i++) { float d = fabsf(x[i]-y[i]); if (d>e) e=d; }
        check(cGpu.Device()==NkDevice::NK_GPU && cg.Numel()==8 && e<1e-3f, "batched matmul GPU == CPU");
    }

    // ---- 17) LayerNorm (dernier axe) GPU (forward + backward) == CPU ----------
    {
        float m[8] = { 1,3,2,5,  -1,0,4,2 };   // [2,4]
        NkShape s; s.PushBack(2); s.PushBack(4);
        NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
        // forward
        NkTensor gy = NkGpuLayerNormStd(x.ToGPU());
        NkTensor by = gy.ToCPU(); const float* yp = by.DataAs<float>();
        float refY[8];
        for (int r = 0; r < 2; r++) {
            const float* xr = m + r*4;
            double mean=0; for (int c=0;c<4;c++) mean+=xr[c]; mean/=4.0;
            double var=0;  for (int c=0;c<4;c++){ double t=xr[c]-mean; var+=t*t; } var/=4.0;
            double inv=1.0/std::sqrt(var+1e-5);
            for (int c=0;c<4;c++) refY[r*4+c]=(float)((xr[c]-mean)*inv);
        }
        float ef=0; for (int i=0;i<8;i++){ float d=fabsf(yp[i]-refY[i]); if(d>ef)ef=d; }
        check(gy.Device()==NkDevice::NK_GPU && ef<1e-3f, "layernorm forward GPU == CPU");
        // backward avec grad = petit motif
        float gd[8] = { 0.1f,-0.2f,0.3f,0.05f,  -0.4f,0.2f,0.1f,-0.1f };
        NkShape gs; gs.PushBack(2); gs.PushBack(4);
        NkTensor grad = NkTensor::FromData(gs, gd, NkDType::NK_F32);
        NkTensor gdx = NkGpuLayerNormStdBackward(x.ToGPU(), grad.ToGPU());
        NkTensor bdx = gdx.ToCPU(); const float* dxp = bdx.DataAs<float>();
        float refDX[8];
        for (int r = 0; r < 2; r++) {
            const float* xr = m + r*4; const float* gr = gd + r*4;
            double mean=0; for (int c=0;c<4;c++) mean+=xr[c]; mean/=4.0;
            double var=0;  for (int c=0;c<4;c++){ double t=xr[c]-mean; var+=t*t; } var/=4.0;
            double inv=1.0/std::sqrt(var+1e-5);
            double m1=0,m2=0; for (int c=0;c<4;c++){ double xh=(xr[c]-mean)*inv; m1+=gr[c]; m2+=gr[c]*xh; } m1/=4.0; m2/=4.0;
            for (int c=0;c<4;c++){ double xh=(xr[c]-mean)*inv; refDX[r*4+c]=(float)(inv*(gr[c]-m1-xh*m2)); }
        }
        float ex=0; for (int i=0;i<8;i++){ float d=fabsf(dxp[i]-refDX[i]); if(d>ex)ex=d; }
        check(gdx.Device()==NkDevice::NK_GPU && ex<1e-3f, "layernorm backward GPU == CPU");
    }

    // ---- 18) Softmax backward + Softmax CAUSAL GPU == CPU ---------------------
    {
        // softmax backward : y = softmax(scores) [2,3], grad motif -> dx == formule.
        float sd[6] = { 1, 2, 0.5f,  -1, 0, 2 };
        NkShape s2; s2.PushBack(2); s2.PushBack(3);
        NkTensor sc = NkTensor::FromData(s2, sd, NkDType::NK_F32);
        NkTensor y  = NkGpuSoftmaxRows(sc.ToGPU());
        NkTensor yc = y.ToCPU(); const float* yp = yc.DataAs<float>();
        float gd[6] = { 0.2f,-0.1f,0.3f,  0.4f,-0.2f,0.1f };
        NkTensor grad = NkTensor::FromData(s2, gd, NkDType::NK_F32);
        NkTensor gdx = NkGpuSoftmaxBackward(y, grad.ToGPU());
        NkTensor bdx = gdx.ToCPU(); const float* dxp = bdx.DataAs<float>();
        float refDX[6];
        for (int r = 0; r < 2; r++) {
            const float* yr = yp + r*3; const float* gr = gd + r*3;
            double sdot=0; for (int c=0;c<3;c++) sdot += (double)gr[c]*yr[c];
            for (int c=0;c<3;c++) refDX[r*3+c] = (float)(yr[c]*(gr[c]-sdot));
        }
        float ex=0; for (int i=0;i<6;i++){ float d=fabsf(dxp[i]-refDX[i]); if(d>ex)ex=d; }
        check(gdx.Device()==NkDevice::NK_GPU && ex<1e-3f, "softmax backward GPU == CPU");

        // softmax CAUSAL sur [1,1,3,3] (T=3) : triangle supérieur strict == 0 (futur masqué).
        float scores[9] = { 1,2,3,  4,5,6,  7,8,9 };
        NkShape s3; s3.PushBack(1); s3.PushBack(1); s3.PushBack(3); s3.PushBack(3);
        NkTensor sc3 = NkTensor::FromData(s3, scores, NkDType::NK_F32);
        NkTensor yc3 = NkGpuSoftmaxCausal(sc3.ToGPU()).ToCPU();
        const float* p = yc3.DataAs<float>();
        // ligne i (requête) : colonnes j>i doivent valoir 0 ; ligne somme (j<=i) == 1.
        bool okMask = (p[1]==0.f && p[2]==0.f && p[5]==0.f);          // (0,1)(0,2)(1,2) masqués
        bool okSum  = fabsf((p[0]) - 1.f) < 1e-4f                     // ligne0 : 1 seule clé
                    && fabsf((p[3]+p[4]) - 1.f) < 1e-4f               // ligne1 : 2 clés
                    && fabsf((p[6]+p[7]+p[8]) - 1.f) < 1e-4f;         // ligne2 : 3 clés
        check(okMask && okSum, "softmax causal GPU (futur masqué + lignes normalisées)");
    }

    // ---- 19) GELU GPU (forward + backward) == CPU -----------------------------
    {
        float m[6] = { -2,-0.5f,0,0.5f,1,2 };
        NkShape s; s.PushBack(6);
        NkTensor x = NkTensor::FromData(s, m, NkDType::NK_F32);
        NkTensor gy = NkGpuGelu(x.ToGPU()).ToCPU(); const float* yp = gy.DataAs<float>();
        const double c = 0.7978845608;
        float refY[6]; for (int i=0;i<6;i++){ double v=m[i]; double inner=c*(v+0.044715*v*v*v); refY[i]=(float)(0.5*v*(1.0+std::tanh(inner))); }
        float ef=0; for (int i=0;i<6;i++){ float d=fabsf(yp[i]-refY[i]); if(d>ef)ef=d; }
        check(ef<1e-3f, "gelu forward GPU == CPU");
        float gd[6]={0.1f,-0.2f,0.3f,0.4f,-0.1f,0.2f};
        NkTensor grad = NkTensor::FromData(s, gd, NkDType::NK_F32);
        NkTensor dx = NkGpuGeluBackward(x.ToGPU(), grad.ToGPU()).ToCPU(); const float* dxp = dx.DataAs<float>();
        float refDX[6]; for (int i=0;i<6;i++){ double v=m[i]; double v2=v*v; double inner=c*(v+0.044715*v2*v); double t=std::tanh(inner);
            double dg=0.5*(1.0+t)+0.5*v*(1.0-t*t)*c*(1.0+3.0*0.044715*v2); refDX[i]=(float)(gd[i]*dg); }
        float ex=0; for (int i=0;i<6;i++){ float d=fabsf(dxp[i]-refDX[i]); if(d>ex)ex=d; }
        check(ex<1e-3f, "gelu backward GPU == CPU");
    }

    // ---- 20) Embedding GPU (forward + backward scatter-add) == CPU -------------
    {
        float td[6] = { 1,2, 3,4, 5,6 };   // table [3,2]
        float idd[4] = { 0,1,2,1 };        // indices [4]
        NkShape ts; ts.PushBack(3); ts.PushBack(2);
        NkShape is; is.PushBack(4);
        NkTensor table = NkTensor::FromData(ts, td, NkDType::NK_F32);
        NkTensor idx   = NkTensor::FromData(is, idd, NkDType::NK_F32);
        NkTensor out = NkGpuEmbedding(table.ToGPU(), idx.ToGPU()).ToCPU();   // [4,2]
        const float* op = out.DataAs<float>();
        float refO[8] = { 1,2, 3,4, 5,6, 3,4 };
        float eo=0; for (int i=0;i<8;i++){ float d=fabsf(op[i]-refO[i]); if(d>eo)eo=d; }
        check(out.Numel()==8 && eo<1e-4f, "embedding forward GPU == CPU");
        // backward : grad=ones[4,2] -> dTable[0]=[1,1], [1]=[2,2] (row1 utilisé 2×), [2]=[1,1]
        NkTensor grad = NkTensor::Ones(NkShape{ (int64)4, (int64)2 });
        NkTensor dt = NkGpuEmbeddingBackward(grad.ToGPU(), idx.ToGPU(), 3, 2).ToCPU();  // [3,2]
        const float* dp = dt.DataAs<float>();
        float refDT[6] = { 1,1, 2,2, 1,1 };
        float ed=0; for (int i=0;i<6;i++){ float d=fabsf(dp[i]-refDT[i]); if(d>ed)ed=d; }
        check(dt.Numel()==6 && ed<1e-4f, "embedding backward GPU (scatter-add) == CPU");
    }

    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
    gpu.Shutdown();
    return g_fail;
}
