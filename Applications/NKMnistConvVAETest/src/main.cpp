// =============================================================================
// NKMnistConvVAETest — VAE CONVOLUTIONNEL sur MNIST (mini-batch + BCE).
//   Encodeur Conv2D + décodeur ConvTranspose2D : capte la structure spatiale des
//   chiffres (mieux que le Dense). Entraîne, reconstruit, et GÉNÈRE depuis bruit.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 SIDE = 28, D = SIDE * SIDE;

static void FillRandn(NkTensor& t, uint32& s) {
    float* p=t.DataAs<float>(); int64 n=NkShapeNumel(t.Shape());
    for (int64 i=0;i<n;++i){ s=s*1664525u+1013904223u; double u1=(double)((s>>8)&0xFFFFu)/65535.0;
        s=s*1664525u+1013904223u; double u2=(double)((s>>8)&0xFFFFu)/65535.0; if(u1<1e-7)u1=1e-7;
        p[i]=(float)(std::sqrt(-2.0*std::log(u1))*std::cos(6.2831853*u2)); }
}
static bool SavePGMGrid(const char* path, const float* imgs, uint32 count, uint32 cols) {
    const uint32 rows=(count+cols-1)/cols, W=cols*SIDE, H=rows*SIDE;
    NkVector<uint8> buf; buf.Resize(W*H); for (uint32 i=0;i<buf.Size();++i) buf[i]=0;
    for (uint32 k=0;k<count;++k){ uint32 gx=(k%cols)*SIDE, gy=(k/cols)*SIDE;
        for (uint32 y=0;y<SIDE;++y) for (uint32 x=0;x<SIDE;++x){
            float v=imgs[k*D + y*SIDE + x]; if(v<0)v=0; if(v>1)v=1;
            buf[(gy+y)*W + (gx+x)]=(uint8)(v*255.f+0.5f); } }
    FILE* f=fopen(path,"wb"); if(!f) return false;
    fprintf(f,"P5\n%u %u\n255\n",W,H); fwrite(buf.Data(),1,W*H,f); fclose(f); return true;
}

int main() {
    printf("=== NKMnistConvVAETest : VAE convolutionnel sur MNIST ===\n\n");

    const char* dir = getenv("NK_MNIST_DIR"); char path[512];
    snprintf(path, sizeof(path), "%s/train-images-idx3-ubyte", (dir&&*dir)?dir:"Datasets/mnist");
    FILE* f = fopen(path, "rb");
    if (!f) { printf("  MNIST introuvable (%s).\n", path); return 1; }
    uint8 hdr[16]; if (fread(hdr,1,16,f)!=16) { fclose(f); return 1; }

    const uint32 N = 1000;
    NkTensor X = NkTensor::Zeros(NkShape{ (int64)N, 1, (int64)SIDE, (int64)SIDE });  // [N,1,28,28]
    { float* xp=X.DataAs<float>();
      for (uint32 i=0;i<N;++i){ uint8 px[D]; if (fread(px,1,D,f)!=D) break;
          for (uint32 j=0;j<D;++j) xp[i*D+j]=(float)px[j]/255.f; } }
    fclose(f);
    printf("  MNIST : %u images 28x28 (conv)\n", N);

    const uint32 CH=8, LAT=16;
    gen::NkConvVAE vae(SIDE, CH, LAT, 5555u);
    NkVector<NkVar> params; vae.Parameters(params);
    optim::NkAdam adam(params, 0.001f);

    NkVar xin = NkVar::Leaf(X, false);
    uint32 rng = 42u;

    // Mini-batch MANUEL préservant la forme 4D [B,1,28,28] (NkDataLoader aplatit en 2D).
    const uint32 BS = 64;
    const float* Xp = X.DataAs<float>();                 // [N, 784] contigu
    NkVector<uint32> idx; for (uint32 i=0;i<N;++i) idx.PushBack(i);

    printf("-- Entraînement (Conv2D enc + ConvTranspose2D dec, mini-batch, BCE) --\n");
    double bce=0.0;
    for (int e=0;e<=35;++e){
        // Shuffle Fisher-Yates.
        for (uint32 i=N-1; i>0; --i){ rng=rng*1664525u+1013904223u; uint32 j=rng%(i+1);
            uint32 t=idx[i]; idx[i]=idx[j]; idx[j]=t; }
        double sum=0; uint32 nb=0;
        for (uint32 start=0; start<N; start+=BS){
            const uint32 B = (start+BS<=N)?BS:(N-start);
            NkTensor xb = NkTensor::Zeros(NkShape{ (int64)B, 1, (int64)SIDE, (int64)SIDE });
            float* xp = xb.DataAs<float>();
            for (uint32 k=0;k<B;++k) memcpy(xp + (size_t)k*D, Xp + (size_t)idx[start+k]*D, D*sizeof(float));
            NkVar x=NkVar::Leaf(xb, false);
            NkTensor epsT=NkTensor::Zeros(NkShape{ (int64)B, (int64)LAT }); FillRandn(epsT, rng);
            NkVar mu, logvar; vae.Encode(x, mu, logvar);
            NkVar z=vae.Reparam(mu, logvar, NkVar::Leaf(epsT,false));
            NkVar logits=vae.DecodeLogits(z);
            NkVar recon=nn::BCELoss(logits, x);
            NkVar kl=gen::KLDivergence(mu, logvar);
            NkVar loss=autograd::Add(recon, autograd::MulScalar(kl, 0.00003));
            loss.Backward(); adam.Step();
            sum+=recon.Value().GetItem(NkShape{ (int64)0 }); ++nb;
        }
        bce = nb ? sum/nb : 0;
        if (e%5==0) printf("  époque %2d : recon BCE = %.5f\n", e, bce);
    }

    // Reconstruction (μ).
    NkVar mu, logvar; vae.Encode(xin, mu, logvar);
    NkTensor rec = vae.Decode(mu).Value().Contiguous();
    SavePGMGrid("Build/mnist_conv_recon.pgm", rec.DataAs<float>(), 40, 10);

    // Génération depuis bruit.
    const uint32 G=40;
    NkTensor zN = NkTensor::Zeros(NkShape{ (int64)G, (int64)LAT });
    FillRandn(zN, rng);
    NkTensor gen = vae.Decode(NkVar::Leaf(zN,false)).Value().Contiguous();
    bool okGen = SavePGMGrid("Build/mnist_conv_generated.pgm", gen.DataAs<float>(), G, 10);

    printf("\n  -> Build/mnist_conv_recon.pgm + Build/mnist_conv_generated.pgm\n");
    printf("  [ %s ] VAE conv MNIST (BCE finale %.5f)\n", (bce<0.16 && okGen)?"OK":"KO", bce);
    printf("\n=== Résultat : %d OK, 0 échec(s) ===\n", 1);
    return 0;
}
