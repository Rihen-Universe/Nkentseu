// =============================================================================
// NkGpuProbe — le compute GPU NKRHI tourne-t-il sur cette machine (headless) ?
// Crée un device (DX11 puis DX12), fait une matmul via NkMLContext, et compare
// au résultat attendu. C'est le test qui gouverne le backend GPU de NKTensor.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkML.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;

static bool TryApi(NkGraphicsApi api) {
    printf("\n--- device %s (HEADLESS, sans fenêtre) ---\n", NkGraphicsApiName(api)); fflush(stdout);

    NkDeviceInitInfo di;
    di.api = api;               // pas de di.surface -> headless (compute-only)
    di.width = 0;
    di.height = 0;
    di.context.software.threading = true;

    NkIDevice* dev = NkDeviceFactory::Create(di);
    if (!dev || !dev->IsValid()) {
        printf("  device KO\n");
        if (dev) NkDeviceFactory::Destroy(dev);
        return false;
    }
    const bool cs = dev->GetCaps().computeShaders;
    printf("  device OK — compute shaders : %s\n", cs ? "oui" : "non");
    if (!cs) { NkDeviceFactory::Destroy(dev); return false; }

    NkMLContext ml;
    if (!ml.Init(dev)) { printf("  NkMLContext::Init KO\n"); NkDeviceFactory::Destroy(dev); return false; }

    // C[2x2] = A[2x3] x B[3x2]  = [[58,64],[139,154]]
    NkTensor A = ml.CreateTensor({ 2, 3 });
    NkTensor B = ml.CreateTensor({ 3, 2 });
    NkTensor C = ml.CreateTensor({ 2, 2 });
    float a[6] = { 1,2,3,4,5,6 };
    float b[6] = { 7,8,9,10,11,12 };
    ml.Upload(A, a);
    ml.Upload(B, b);
    ml.MatMul(A, B, C);
    float c[4] = { 0,0,0,0 };
    ml.Download(C, c);

    printf("  C = [%.1f %.1f %.1f %.1f]  (attendu 58 64 139 154)\n",
           c[0], c[1], c[2], c[3]);
    const bool ok = (fabs(c[0] - 58) < 0.5 && fabs(c[1] - 64) < 0.5 &&
                     fabs(c[2] - 139) < 0.5 && fabs(c[3] - 154) < 0.5);

    ml.Shutdown();
    NkDeviceFactory::Destroy(dev);
    return ok;
}

int main() {
    printf("=== NkGpuProbe : compute NKRHI HEADLESS (sans fenêtre) ? ===\n");
    fflush(stdout);

    bool ok = false;
    if      (TryApi(NkGraphicsApi::NK_GFX_API_DX11)) { printf("\n>>> DX11 compute headless : FONCTIONNEL\n"); ok = true; }
    else if (TryApi(NkGraphicsApi::NK_GFX_API_DX12)) { printf("\n>>> DX12 compute headless : FONCTIONNEL\n"); ok = true; }

    if (!ok) printf("\n>>> Aucun device compute headless fonctionnel (DX11/DX12).\n");
    return ok ? 0 : 1;
}
