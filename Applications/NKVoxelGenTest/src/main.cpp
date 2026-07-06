// =============================================================================
// NKVoxelGenTest — chaîne complète : BRUIT -> forme 3D -> MAILLAGE OBJ.
//   VAE 3D convolutionnel (Conv3D enc + ConvTranspose3D dec) entraîné sur des
//   volumes de voxels, puis on tire z ~ N(0,1) -> décode -> voxels -> NkMesh -> OBJ.
//   C'est « l'IA imagine une forme 3D et la sort en asset » de bout en bout.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 G = 8, VOX = G * G * G;
static inline uint32 Idx(uint32 x, uint32 y, uint32 z) { return z*G*G + y*G + x; }

static void Shape(uint32 kind, float* out) {
    const float c = (float)(G - 1) * 0.5f;
    for (uint32 z=0; z<G; ++z) for (uint32 y=0; y<G; ++y) for (uint32 x=0; x<G; ++x) {
        float fx=(float)x-c, fy=(float)y-c, fz=(float)z-c; bool on=false;
        switch (kind) {
            case 0: on = (fx*fx+fy*fy+fz*fz) <= 9.0f; break;                 // sphère
            case 1: on = (x>=1&&x<=6&&y>=1&&y<=6&&z>=1&&z<=6); break;         // cube
            case 2: on = (fx*fx+fy*fy) <= 6.0f; break;                       // cylindre
            default: { float r=3.2f-(float)z*0.4f; on=(std::fabs(fx)<=r&&std::fabs(fy)<=r); } // pyramide
        }
        out[Idx(x,y,z)] = on ? 1.f : 0.f;
    }
}
static void PrintSlices(const float* v, const char* title) {
    printf("    %s\n", title);
    const char* ramp = " .:oO#";
    for (uint32 y=0; y<G; ++y) {
        printf("      ");
        for (uint32 z=0; z<G; ++z) {
            for (uint32 x=0; x<G; ++x) { float val=v[Idx(x,y,z)]; if(val<0)val=0; if(val>1)val=1; printf("%c", ramp[(int)(val*5.f+0.5f)]); }
            printf(" ");
        }
        printf("\n");
    }
}
static void FillRandn(NkTensor& t, uint32& s) {
    float* p=t.DataAs<float>(); int64 n=NkShapeNumel(t.Shape());
    for (int64 i=0;i<n;++i){ s=s*1664525u+1013904223u; double u1=(double)((s>>8)&0xFFFFu)/65535.0;
        s=s*1664525u+1013904223u; double u2=(double)((s>>8)&0xFFFFu)/65535.0; if(u1<1e-7)u1=1e-7;
        p[i]=(float)(std::sqrt(-2.0*std::log(u1))*std::cos(6.2831853*u2)); }
}

int main() {
    printf("=== NKVoxelGenTest : bruit -> forme 3D -> maillage OBJ (VAE 3D conv) ===\n\n");

    const uint32 NCLS=4, PER=8, N=NCLS*PER;
    NkTensor X = NkTensor::Zeros(NkShape{ (int64)N, 1, (int64)G, (int64)G, (int64)G });
    {
        float* xp=X.DataAs<float>(); float proto[VOX]; uint32 s=333u;
        for (uint32 c=0;c<NCLS;++c){ Shape(c, proto);
            for (uint32 k=0;k<PER;++k){ uint32 i=c*PER+k;
                for (uint32 j=0;j<VOX;++j){ s=s*1664525u+1013904223u;
                    float noise=((float)((s>>9)&0x7FFFu)/32767.f-0.5f)*0.2f;
                    float v=proto[j]+noise; xp[i*VOX+j]=v<0?0:(v>1?1:v); } } }
    }

    const uint32 LAT=16, CH=8;
    gen::NkVoxelVAE vae(G, CH, LAT, 2024u);
    NkVector<NkVar> params; vae.Parameters(params);
    optim::NkAdam adam(params, 0.004f);

    NkVar xin = NkVar::Leaf(X, false);
    NkTensor epsT = NkTensor::Zeros(NkShape{ (int64)N, (int64)LAT });
    uint32 rng = 909u;

    printf("-- Entraînement du VAE 3D (Conv3D + ConvTranspose3D + KL) --\n");
    double reconMse=0.0;
    for (int e=0;e<=700;++e){
        NkVar mu, logvar; vae.Encode(xin, mu, logvar);
        FillRandn(epsT, rng);
        NkVar z=vae.Reparam(mu, logvar, NkVar::Leaf(epsT,false));
        NkVar xhat=vae.Decode(z);
        NkVar recon=autograd::MSE(xhat, xin);
        NkVar kl=gen::KLDivergence(mu, logvar);
        NkVar loss=autograd::Add(recon, autograd::MulScalar(kl, 0.0002));
        loss.Backward(); adam.Step();
        reconMse=recon.Value().GetItem(NkShape{ (int64)0 });
        if (e%100==0) printf("  époque %3d : recon MSE = %.5f  KL = %.3f\n", e, reconMse, kl.Value().GetItem(NkShape{ (int64)0 }));
    }

    // GÉNÉRATION : z ~ N(0,1) -> forme 3D inédite.
    printf("\n-- GÉNÉRATION 3D depuis z ~ N(0,1) --\n");
    NkTensor zN = NkTensor::Zeros(NkShape{ 1, (int64)LAT });
    FillRandn(zN, rng);
    NkTensor gen = vae.Decode(NkVar::Leaf(zN,false)).Value().Contiguous(); // [1,1,8,8,8]
    const float* gp = gen.DataAs<float>();
    PrintSlices(gp, "forme générée (voxels, tranches z=0..7 ->)");

    uint32 occupied=0; for (uint32 j=0;j<VOX;++j) if (gp[j]>0.5f) ++occupied;

    // Voxels -> maillage -> OBJ.
    gen::NkMesh mesh = gen::VoxelsToMesh(gp, G, 0.5f, 1.0f);
    bool saved = gen::SaveMeshObj("Build/nkgen_voxel_ai.obj", mesh);
    printf("\n  voxels occupés : %u/%u   maillage : %u sommets, %u triangles   OBJ : %s\n",
           occupied, VOX, mesh.VertexCount(), mesh.TriangleCount(), saved ? "Build/nkgen_voxel_ai.obj" : "KO");

    int pass=0, fail=0;
    auto check=[&](bool ok,const char* n){ (ok?pass:fail)++; printf("  [ %s ] %s\n", ok?"OK":"KO", n); };
    check(reconMse < 0.09,        "VAE 3D reconstruit (recon MSE < 0.09)");
    check(occupied > 8,           "forme générée non vide (voxels occupés)");
    check(mesh.TriangleCount()>0 && saved, "maillage 3D généré + OBJ écrit (asset moteur)");

    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
    return fail==0?0:1;
}
