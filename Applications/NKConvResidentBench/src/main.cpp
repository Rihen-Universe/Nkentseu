// =============================================================================
// NKConvResidentBench — Conv2D (FORWARD + BACKWARD via autograd) :
//   (A) feuilles CPU  = statu quo : im2col/col2im/permute sur CPU, matmul
//       auto-déchargé sur GPU (transferts CPU<->GPU à chaque étape).
//   (B) feuilles GPU-RÉSIDENTES : tout reste sur GPU (aucun transfert).
// On mesure le temps d'un pas complet (fwd + Sum + backward) et on vérifie que
// les gradients dX/dW sont IDENTIQUES entre les deux chemins.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h"

#include <cstdio>
#include <chrono>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static double MaxErr(const NkTensor& a, const NkTensor& b) {
    if (!a.IsValid() || !b.IsValid() || a.Numel() != b.Numel()) return 1e30;
    const float* x = a.DataAs<float>(); const float* y = b.DataAs<float>();
    double e = 0.0; const int64 n = a.Numel();
    for (int64 i = 0; i < n; i++) { double d = std::fabs((double)x[i] - (double)y[i]); if (d > e) e = d; }
    return e;
}

int main() {
    printf("=== NKConvResidentBench (Conv2D forward+backward : CPU vs GPU-résident) ===\n");

    // ---- Configuration conv (grosse conv, comme le bench im2col) --------------
    const int64 B = 8, Cin = 64, H = 32, W = 32, Cout = 128, k = 3;
    const int32 stride = 1, pad = 1;
    const int64 outH = (H + 2 * pad - k) / stride + 1, outW = (W + 2 * pad - k) / stride + 1;
    printf("x=[%lld,%lld,%lld,%lld]  w=[%lld,%lld,%lld,%lld]  stride=%d pad=%d  -> out=[%lld,%lld,%lld,%lld]\n",
           (long long)B,(long long)Cin,(long long)H,(long long)W,
           (long long)Cout,(long long)Cin,(long long)k,(long long)k, stride, pad,
           (long long)B,(long long)Cout,(long long)outH,(long long)outW);

    // ---- Données déterministes -----------------------------------------------
    const int64 nX = B * Cin * H * W, nW = Cout * Cin * k * k;
    float* xd = new float[nX]; for (int64 i = 0; i < nX; i++) xd[i] = std::sin(0.01 * (double)i);
    float* wd = new float[nW]; for (int64 i = 0; i < nW; i++) wd[i] = 0.1 * std::cos(0.02 * (double)i);
    NkShape xs; xs.PushBack(B); xs.PushBack(Cin); xs.PushBack(H); xs.PushBack(W);
    NkShape wsh; wsh.PushBack(Cout); wsh.PushBack(Cin); wsh.PushBack(k); wsh.PushBack(k);
    NkTensor xCpu = NkTensor::FromData(xs, xd, NkDType::NK_F32);
    NkTensor wCpu = NkTensor::FromData(wsh, wd, NkDType::NK_F32);
    delete[] xd; delete[] wd;

    // Un pas complet : forward Conv2D -> Sum(loss) -> Backward ; renvoie dX, dW (CPU).
    auto step = [&](bool gpu, NkTensor& dX, NkTensor& dW, double& lossOut) {
        NkVar x = NkVar::Leaf(gpu ? xCpu.ToGPU() : xCpu, true);
        NkVar w = NkVar::Leaf(gpu ? wCpu.ToGPU() : wCpu, true);
        NkVar out  = autograd::Conv2D(x, w, stride, pad);
        NkVar loss = autograd::Sum(out);
        loss.Backward();
        NkTensor lc = loss.Value().ToCPU();
        lossOut = lc.IsValid() ? lc.GetItem(NkShape{ (int64)0 }) : 0.0;
        dX = x.Grad().ToCPU().Contiguous();
        dW = w.Grad().ToCPU().Contiguous();
    };

    NkTensorGpu& gpu = NkTensorGpu::Get();
    printf("GPU compute : %s (backend %s)\n\n", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());

    // ---- (A) Chemin CPU (feuilles CPU) ---------------------------------------
    NkTensor dXc, dWc; double lossC = 0.0;
    const int Rc = 2;
    step(false, dXc, dWc, lossC);   // 1er (chauffe le device pour le matmul déchargé)
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int r = 0; r < Rc; r++) step(false, dXc, dWc, lossC);
    auto t1 = std::chrono::high_resolution_clock::now();
    double cpuMs = std::chrono::duration<double, std::milli>(t1 - t0).count() / Rc;

    // ---- (B) Chemin GPU-résident (feuilles GPU) ------------------------------
    double cpuVsGpu = -1.0, gpuMs = -1.0, errX = -1.0, errW = -1.0;
    if (gpu.IsAvailable()) {
        NkTensor dXg, dWg; double lossG = 0.0;
        step(true, dXg, dWg, lossG);   // warmup 1 (compile les kernels)
        step(true, dXg, dWg, lossG);   // warmup 2
        const int Rg = 8;
        auto t2 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < Rg; r++) step(true, dXg, dWg, lossG);
        auto t3 = std::chrono::high_resolution_clock::now();
        gpuMs = std::chrono::duration<double, std::milli>(t3 - t2).count() / Rg;
        errX = MaxErr(dXc, dXg);
        errW = MaxErr(dWc, dWg);
        cpuVsGpu = (gpuMs > 0.0) ? cpuMs / gpuMs : -1.0;
        printf("loss CPU=%.4f  loss GPU=%.4f\n", lossC, lossG);
    }

    printf("\n--- Temps d'un pas conv (forward + backward) ---\n");
    printf("  (A) feuilles CPU (matmul déchargé + transferts) : %9.2f ms\n", cpuMs);
    if (gpuMs > 0.0) {
        printf("  (B) feuilles GPU-RÉSIDENTES (aucun transfert)   : %9.2f ms   (moyenne, régime établi)\n", gpuMs);
        printf("  Accélération (A/B)                              : %8.1fx\n", cpuVsGpu);
        printf("\n--- Correction (gradients GPU-résident vs CPU) ---\n");
        printf("  err max dX = %.3e   err max dW = %.3e\n", errX, errW);
        bool ok = errX < 1e-2 && errW < 1e-2;   // tolérance : ordre de somme GPU/CPU différent
        printf("  [%s] gradients identiques (dX, dW)\n", ok ? " OK " : "FAIL");
        gpu.Shutdown();
        return ok ? 0 : 1;
    }
    printf("  (GPU indisponible -> pas de mesure résidente)\n");
    return 0;
}
