// =============================================================================
// NKConvBenchTest — gain de la conv im2col+GPU vs conv naïve CPU.
//   Grosse conv (beaucoup de canaux, comme un vrai modèle image) : on compare une
//   conv NAÏVE CPU (référence) à notre autograd::Conv2D (im2col -> matmul GPU).
//   Vérifie aussi que les résultats coïncident.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <chrono>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static NkTensor RandT(const NkShape& s, uint32& seed) {
    NkTensor t = NkTensor::Zeros(s); float* p=t.DataAs<float>(); int64 n=NkShapeNumel(s);
    for (int64 i=0;i<n;++i){ seed=seed*1664525u+1013904223u; p[i]=(float)((seed>>9)&0x3FF)/1024.f-0.5f; }
    return t;
}
// Conv2D naïve (référence CPU) : input [B,Cin,H,W] ⊛ weight [Cout,Cin,kH,kW], pad p.
static NkTensor NaiveConv(const NkTensor& x, const NkTensor& w, int pad) {
    const NkShape& xs=x.Shape(); const NkShape& ws=w.Shape();
    int64 B=xs[0],Cin=xs[1],H=xs[2],W=xs[3], Cout=ws[0],kH=ws[2],kW=ws[3];
    int64 oH=H+2*pad-kH+1, oW=W+2*pad-kW+1;
    NkTensor out=NkTensor::Zeros(NkShape{B,Cout,oH,oW});
    const float* xp=x.DataAs<float>(); const float* wp=w.DataAs<float>(); float* op=out.DataAs<float>();
    for (int64 b=0;b<B;++b) for (int64 oc=0;oc<Cout;++oc)
    for (int64 oy=0;oy<oH;++oy) for (int64 ox=0;ox<oW;++ox){
        double s=0;
        for (int64 ic=0;ic<Cin;++ic) for (int64 ky=0;ky<kH;++ky) for (int64 kx=0;kx<kW;++kx){
            int64 iy=oy-pad+ky, ix=ox-pad+kx; if(iy<0||iy>=H||ix<0||ix>=W) continue;
            s += (double)xp[((b*Cin+ic)*H+iy)*W+ix]*(double)wp[((oc*Cin+ic)*kH+ky)*kW+kx];
        }
        op[((b*Cout+oc)*oH+oy)*oW+ox]=(float)s;
    }
    return out;
}
static double MaxDiff(const NkTensor& a, const NkTensor& b){
    NkTensor ac=a.Contiguous(),bc=b.Contiguous(); const float* ap=ac.DataAs<float>(); const float* bp=bc.DataAs<float>();
    int64 n=NkShapeNumel(ac.Shape()); double m=0; for (int64 i=0;i<n;++i){ double d=std::fabs((double)ap[i]-(double)bp[i]); if(d>m)m=d;} return m;
}

int main() {
    printf("=== NKConvBenchTest : conv naïve CPU vs im2col+GPU ===\n\n");
    uint32 s=123u;
    // Grosse conv : B=8, Cin=64, 32x32, Cout=128, k3, pad1 (matmul ~603 MFLOP -> GPU).
    NkTensor x = RandT(NkShape{ 8, 64, 32, 32 }, s);
    NkTensor w = RandT(NkShape{ 128, 64, 3, 3 }, s);
    printf("  conv : [8,64,32,32] * [128,64,3,3] pad1  (im2col matmul M=8192,K=576,N=128 = 603 MFLOP)\n\n");

    // Warm-up : 1er appel = init device GPU + compil kernel (payé une fois en vrai).
    NkTensor warm = autograd::Conv2D(NkVar::Leaf(x,false), NkVar::Leaf(w,false), 1, 1).Value();

    auto t0=std::chrono::high_resolution_clock::now();
    NkTensor cpu = NaiveConv(x, w, 1);
    auto t1=std::chrono::high_resolution_clock::now();
    double cpuMs=std::chrono::duration<double,std::milli>(t1-t0).count();

    // Moyenne sur 5 convs (régime établi, hors init).
    const int R=5; NkTensor gpu;
    auto t2=std::chrono::high_resolution_clock::now();
    for (int r=0;r<R;++r) gpu = autograd::Conv2D(NkVar::Leaf(x,false), NkVar::Leaf(w,false), 1, 1).Value();
    auto t3=std::chrono::high_resolution_clock::now();
    double gpuMs=std::chrono::duration<double,std::milli>(t3-t2).count()/R;

    double err=MaxDiff(cpu,gpu);
    printf("  conv NAÏVE (CPU)      : %8.1f ms\n", cpuMs);
    printf("  conv im2col+GPU       : %8.1f ms   (régime, hors init ; moyenne sur %d)\n", gpuMs, R);
    printf("  speedup               : %8.1fx\n", (gpuMs>0)?cpuMs/gpuMs:0);
    printf("  erreur max            : %.2e   (doit être ~0)\n", err);

    bool ok = (err < 1e-2) && (gpuMs < cpuMs);
    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", ok?1:0, ok?0:1);
    return ok?0:1;
}
