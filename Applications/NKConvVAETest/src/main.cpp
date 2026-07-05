// =============================================================================
// NKConvVAETest — VAE CONVOLUTIONNEL : générer des images depuis du bruit.
//   Encodeur Conv + décodeur ConvTranspose (upsampling appris). Prouve la pile
//   générative convolutionnelle (la vraie architecture pour monter en résolution).
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 WH = 8, D = WH * WH;

static void Prototype(uint32 kind, float* out) {
    for (uint32 i = 0; i < D; ++i) out[i] = 0.f;
    for (uint32 y = 0; y < WH; ++y) for (uint32 x = 0; x < WH; ++x) {
        bool on = false;
        switch (kind) {
            case 0: on = (x == 3 || x == 4); break;
            case 1: on = (y == 3 || y == 4); break;
            case 2: on = (x == y || x + 1 == y || x == y + 1); break;
            default: on = (x == 0 || y == 0 || x == WH-1 || y == WH-1);
        }
        if (on) out[y * WH + x] = 1.f;
    }
}
static void PrintImage(const float* img, const char* title) {
    printf("    %s\n", title);
    const char* ramp = " .:oO#";
    for (uint32 y = 0; y < WH; ++y) {
        printf("      ");
        for (uint32 x = 0; x < WH; ++x) { float v = img[y*WH+x]; if(v<0)v=0; if(v>1)v=1; printf("%c", ramp[(int)(v*5.f+0.5f)]); }
        printf("\n");
    }
}
static void FillRandn(NkTensor& t, uint32& s) {
    float* p = t.DataAs<float>(); int64 n = NkShapeNumel(t.Shape());
    for (int64 i = 0; i < n; ++i) {
        s = s*1664525u+1013904223u; double u1=(double)((s>>8)&0xFFFFu)/65535.0;
        s = s*1664525u+1013904223u; double u2=(double)((s>>8)&0xFFFFu)/65535.0;
        if (u1<1e-7) u1=1e-7;
        p[i] = (float)(std::sqrt(-2.0*std::log(u1))*std::cos(6.2831853*u2));
    }
}

int main() {
    printf("=== NKConvVAETest : VAE convolutionnel, génération depuis bruit ===\n\n");

    const uint32 NCLS = 4, PER = 12, N = NCLS * PER;
    NkTensor X = NkTensor::Zeros(NkShape{ (int64)N, 1, (int64)WH, (int64)WH });  // [N,1,8,8]
    {
        float* xp = X.DataAs<float>(); float proto[D]; uint32 s = 909u;
        for (uint32 c = 0; c < NCLS; ++c) {
            Prototype(c, proto);
            for (uint32 k = 0; k < PER; ++k) {
                uint32 i = c * PER + k;
                for (uint32 j = 0; j < D; ++j) {
                    s = s*1664525u+1013904223u;
                    float noise = ((float)((s>>9)&0x7FFFu)/32767.f-0.5f)*0.25f;
                    float v = proto[j]+noise; xp[i*D+j] = v<0?0:(v>1?1:v);
                }
            }
        }
    }

    const uint32 LAT = 8, CH = 8;
    gen::NkConvVAE vae(WH, CH, LAT, 77u);
    NkVector<NkVar> params; vae.Parameters(params);
    optim::NkAdam adam(params, 0.003f);

    NkVar xin = NkVar::Leaf(X, false);
    NkTensor epsT = NkTensor::Zeros(NkShape{ (int64)N, (int64)LAT });
    uint32 rng = 555u;

    printf("-- Entraînement (conv encodeur + convT décodeur + KL) --\n");
    double reconMse = 0.0;
    for (int e = 0; e <= 500; ++e) {
        NkVar mu, logvar;
        vae.Encode(xin, mu, logvar);
        FillRandn(epsT, rng);
        NkVar z    = vae.Reparam(mu, logvar, NkVar::Leaf(epsT, false));
        NkVar xhat = vae.Decode(z);
        NkVar recon = autograd::MSE(xhat, xin);
        NkVar kl    = gen::KLDivergence(mu, logvar);
        NkVar loss  = autograd::Add(recon, autograd::MulScalar(kl, 0.0008));
        loss.Backward();
        adam.Step();
        reconMse = recon.Value().GetItem(NkShape{ (int64)0 });
        if (e % 100 == 0)
            printf("  époque %3d : recon MSE = %.5f  KL = %.3f\n",
                   e, reconMse, kl.Value().GetItem(NkShape{ (int64)0 }));
    }

    // GÉNÉRATION depuis bruit.
    printf("\n-- GÉNÉRATION depuis z ~ N(0,1) (décodeur convolutionnel) --\n");
    const uint32 G = 6;
    NkTensor zN = NkTensor::Zeros(NkShape{ (int64)G, (int64)LAT });
    FillRandn(zN, rng);
    NkTensor gen = vae.Decode(NkVar::Leaf(zN, false)).Value().Contiguous();  // [G,1,8,8]
    const float* gp = gen.DataAs<float>();
    double totalVar = 0.0;
    for (uint32 s = 0; s < G; ++s) {
        char title[40]; snprintf(title, sizeof(title), "échantillon %u (généré, conv)", s);
        PrintImage(gp + s * D, title);
        double m=0,v=0; for (uint32 j=0;j<D;++j) m+=gp[s*D+j]; m/=D;
        for (uint32 j=0;j<D;++j){ double d=gp[s*D+j]-m; v+=d*d; } totalVar += v/D;
    }
    const double avgVar = totalVar / G;

    int pass=0, fail=0;
    auto check=[&](bool ok,const char* n){ (ok?pass:fail)++; printf("  [ %s ] %s\n", ok?"OK":"KO", n); };
    check(reconMse < 0.06, "VAE conv reconstruit (recon MSE < 0.06)");
    check(avgVar   > 0.01, "images générées structurées (décodeur conv-transposée)");
    printf("\n=== Résultat : %d OK, %d échec(s) (var moy = %.4f) ===\n", pass, fail, avgVar);
    return fail==0?0:1;
}
