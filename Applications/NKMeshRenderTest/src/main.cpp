// =============================================================================
// NKMeshRenderTest — VOIR le maillage généré : rasteriseur logiciel from-scratch.
//   Régénère la forme (métaballs -> Surface Nets), la rend en 3/4 (caméra
//   perspective + z-buffer + éclairage diffus Lambert) et écrit une image PPM.
//   Preuve visuelle « l'IA génère une forme 3D -> on la voit ». (Rendu GPU dans
//   une scène Nkentseu = étape suivante.)
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const int  W = 480, H = 480;

int main() {
    printf("=== NKMeshRenderTest : rendu logiciel d'un maillage généré ===\n\n");

    // 1) Régénère la forme (métaballs 40^3 -> Surface Nets).
    const uint32 N = 40;
    NkVector<float> field; field.Resize(N * N * N);
    const float centers[3][3] = { { -0.6f, 0.f, 0.f }, { 0.6f, 0.f, 0.f }, { 0.f, 0.7f, 0.2f } };
    for (uint32 z = 0; z < N; ++z) for (uint32 y = 0; y < N; ++y) for (uint32 x = 0; x < N; ++x) {
        float px = -2.f + 4.f*(float)x/(N-1), py = -2.f + 4.f*(float)y/(N-1), pz = -2.f + 4.f*(float)z/(N-1);
        float s = 0.f;
        for (int b = 0; b < 3; ++b) { float dx=px-centers[b][0],dy=py-centers[b][1],dz=pz-centers[b][2]; s += 1.0f/(dx*dx+dy*dy+dz*dz+0.05f); }
        field[x + N*(y + N*z)] = s;
    }
    gen::NkMesh mesh = gen::SurfaceNets(field.Data(), N, N, N, /*iso*/ 3.0f, 1.0f);
    printf("  maillage : %u sommets, %u triangles\n", mesh.VertexCount(), mesh.TriangleCount());
    if (mesh.TriangleCount() == 0) { printf("  maillage vide\n"); return 1; }

    // 2) Centre + échelle, rotation 3/4.
    const uint32 vc = mesh.VertexCount();
    float mn[3]={1e30f,1e30f,1e30f}, mx[3]={-1e30f,-1e30f,-1e30f};
    for (uint32 i=0;i<vc;++i) for (int k=0;k<3;++k){ float p=mesh.positions[i*3+k]; if(p<mn[k])mn[k]=p; if(p>mx[k])mx[k]=p; }
    float ctr[3]={(mn[0]+mx[0])*0.5f,(mn[1]+mx[1])*0.5f,(mn[2]+mx[2])*0.5f};
    float ext=0.f; for(int k=0;k<3;++k){float e=mx[k]-mn[k]; if(e>ext)ext=e;} if(ext<1e-6f)ext=1e-6f;
    float scale = 1.6f/ext;
    const float ay=0.6f, ax=0.5f, ca=cosf(ay), sa=sinf(ay), cb=cosf(ax), sb=sinf(ax);
    auto xform = [&](const float* p, float* o){
        float x=(p[0]-ctr[0])*scale, y=(p[1]-ctr[1])*scale, z=(p[2]-ctr[2])*scale;
        float x1= x*ca + z*sa, z1= -x*sa + z*ca;           // rot Y
        float y2= y*cb - z1*sb, z2= y*sb + z1*cb;           // rot X
        o[0]=x1; o[1]=y2; o[2]=z2;
    };
    NkVector<float> rp; rp.Resize(vc*3);
    for (uint32 i=0;i<vc;++i) xform(&mesh.positions[i*3], &rp[i*3]);

    // 3) Rasterisation (perspective + z-buffer + Lambert).
    NkVector<float> fb; fb.Resize(W*H*3);
    NkVector<float> zb; zb.Resize(W*H);
    for (int i=0;i<W*H;++i){ fb[i*3+0]=0.06f; fb[i*3+1]=0.07f; fb[i*3+2]=0.10f; zb[i]=1e30f; } // fond bleu nuit
    const float d=3.0f, focal=2.2f;
    float lx=0.4f,ly=0.7f,lz=0.6f; { float L=sqrtf(lx*lx+ly*ly+lz*lz); lx/=L;ly/=L;lz/=L; }

    auto project = [&](const float* v, float& sx, float& sy, float& depth){
        float denom = d - v[2]; if (denom<0.1f) denom=0.1f;
        sx = ( v[0]*focal/denom*0.5f+0.5f)*W;
        sy = (1.0f-(v[1]*focal/denom*0.5f+0.5f))*H;
        depth = denom;
    };

    const uint32 tc = mesh.TriangleCount();
    for (uint32 t=0;t<tc;++t){
        const float* a=&rp[mesh.triangles[t*3+0]*3];
        const float* b=&rp[mesh.triangles[t*3+1]*3];
        const float* c=&rp[mesh.triangles[t*3+2]*3];
        // normale (repère caméra) -> éclairage
        float ux=b[0]-a[0],uy=b[1]-a[1],uz=b[2]-a[2], wx=c[0]-a[0],wy=c[1]-a[1],wz=c[2]-a[2];
        float nx=uy*wz-uz*wy, ny=uz*wx-ux*wz, nz=ux*wy-uy*wx;
        float nl=sqrtf(nx*nx+ny*ny+nz*nz); if(nl<1e-8f)continue; nx/=nl;ny/=nl;nz/=nl;
        float diff=fabsf(nx*lx+ny*ly+nz*lz);           // double face
        float I=0.2f+0.8f*diff;
        float cr=I*0.95f, cg=I*0.55f, cbl=I*0.30f;     // teinte orange

        float ax_,ay_,ad,bx_,by_,bd,cx_,cy_,cd;
        project(a,ax_,ay_,ad); project(b,bx_,by_,bd); project(c,cx_,cy_,cd);
        int minx=(int)floorf(fminf(ax_,fminf(bx_,cx_))), maxx=(int)ceilf(fmaxf(ax_,fmaxf(bx_,cx_)));
        int miny=(int)floorf(fminf(ay_,fminf(by_,cy_))), maxy=(int)ceilf(fmaxf(ay_,fmaxf(by_,cy_)));
        if(minx<0)minx=0; if(miny<0)miny=0; if(maxx>=W)maxx=W-1; if(maxy>=H)maxy=H-1;
        float area=(bx_-ax_)*(cy_-ay_)-(by_-ay_)*(cx_-ax_); if(fabsf(area)<1e-6f)continue;
        for(int py=miny;py<=maxy;++py) for(int px=minx;px<=maxx;++px){
            float fx=px+0.5f, fy=py+0.5f;
            float w0=((bx_-fx)*(cy_-fy)-(by_-fy)*(cx_-fx))/area;
            float w1=((cx_-fx)*(ay_-fy)-(cy_-fy)*(ax_-fx))/area;
            float w2=1.0f-w0-w1;
            if(w0<0||w1<0||w2<0) continue;
            float depth=w0*ad+w1*bd+w2*cd;
            int idx=py*W+px;
            if(depth<zb[idx]){ zb[idx]=depth; fb[idx*3+0]=cr; fb[idx*3+1]=cg; fb[idx*3+2]=cbl; }
        }
    }

    // 4) Écrit l'image PPM (P6).
    const char* path="Build/nkgen_render.ppm";
    FILE* f=fopen(path,"wb");
    int shaded=0;
    if(f){
        fprintf(f,"P6\n%d %d\n255\n",W,H);
        for(int i=0;i<W*H;++i){
            unsigned char rgb[3];
            for(int k=0;k<3;++k){ float v=fb[i*3+k]; if(v<0)v=0; if(v>1)v=1; rgb[k]=(unsigned char)(v*255.0f+0.5f); }
            fwrite(rgb,1,3,f);
            if(zb[i]<1e29f) ++shaded;
        }
        fclose(f);
    }
    printf("  rendu -> %s  (%d pixels couverts par la forme)\n", path, shaded);

    int pass=0,fail=0;
    auto check=[&](bool ok,const char* n){ (ok?pass:fail)++; printf("  [ %s ] %s\n", ok?"OK":"KO", n); };
    check(f!=nullptr,        "image PPM écrite");
    check(shaded>2000,       "la forme générée est visible (assez de pixels rendus)");
    printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
    return fail==0?0:1;
}
