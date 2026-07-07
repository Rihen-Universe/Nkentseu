#pragma once
// =============================================================================
// NkSWFastPath.h  — v3.0
//
// CORRECTIONS v3 :
//   1. Pool vertex sur HEAP (new/delete) au lieu de stack TLS (évite stack overflow)
//   2. Stride lu depuis le pipeline OU déduit depuis le vertex buffer size
//   3. Shadow pass depth-only : colorBuf peut être null → guard correct
//   4. NkVertexSoftware fallback générique : lit stride quelconque
//   5. Texture resolve : chercher dans le descriptor set EN PRIORITÉ
//      la shadow texture (depth) est ignorée pour le color pass
// =============================================================================

#include "NkSoftwareDevice.h"
#include "Raster/NkSWRasterCore.h"   // v4 : cœur barycentrique perspective-correct + clipping
#include "NKThreading/NkThreadPool.h" // v4 Phase 3 : rasterisation tuilée multi-thread
#include "NKMemory/NkAllocator.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

// SIMD
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
#   include <immintrin.h>
#   define NK_SW_SSE2 1
#endif
#if defined(__AVX2__)
#   define NK_SW_AVX2 1
#endif

namespace nkentseu {
    namespace swfast {

        // =============================================================================
        // Pool vertex HEAP
        // =============================================================================
        struct NkSWVertexPool {
            static constexpr uint32 kMaxVerts = 65536;
            NkVertexSoftware* verts    = nullptr;
            uint32            capacity = 0;
        
            ~NkSWVertexPool() { nkentseu::memory::NkGetDefaultAllocator().DeleteArray(verts); }

            NkVertexSoftware* Alloc(uint32 n) {
                if (n > kMaxVerts) n = kMaxVerts;
                if (n > capacity) {
                    nkentseu::memory::NkGetDefaultAllocator().DeleteArray(verts);
                    capacity = kMaxVerts;
                    verts    = nkentseu::memory::NkGetDefaultAllocator().NewArray<NkVertexSoftware>(capacity);
                }
                return verts;
            }
        };
        static thread_local NkSWVertexPool tl_vertPool;
        
        // =============================================================================
        // SIMD
        // =============================================================================
#if 0 // [mort v4] FillSpanFast inutilise — SIMD span gere par swraster / NkSWPixel
        static inline void FillSpanFast(uint8* dst, int32 x0, int32 x1, uint8 r, uint8 g, uint8 b) {
            const uint32 px = sw_detail::PackNative(r, g, b, 255u);
            uint8* p = dst + x0*4; int32 n = x1-x0;
        #ifdef NK_SW_AVX2
            const __m256i v = _mm256_set1_epi32((int)px);
            while (n>=8) { _mm256_storeu_si256((__m256i*)p,v); p+=32; n-=8; }
        #endif
        #ifdef NK_SW_SSE2
            const __m128i v4 = _mm_set1_epi32((int)px);
            while (n>=4) { _mm_storeu_si128((__m128i*)p,v4); p+=16; n-=4; }
        #endif
            uint32* p32=(uint32*)p; while(n-->0) *p32++=px;
        }
#endif // [mort v4] FillSpanFast
        
        static void FastClearColor(uint8* pixels, uint32 w, uint32 h, uint8 r, uint8 g, uint8 b, uint8 a) {
            const uint32 px = sw_detail::PackNative(r,g,b,a);
            usize total = (usize)w*h;
        #ifdef NK_SW_AVX2
            const __m256i v=_mm256_set1_epi32((int)px); uint8* p=pixels;
            while(total>=8){_mm256_storeu_si256((__m256i*)p,v);p+=32;total-=8;}
            uint32* p32=(uint32*)p; while(total-->0) *p32++=px;
        #elif defined(NK_SW_SSE2)
            const __m128i v=_mm_set1_epi32((int)px); uint8* p=pixels;
            while(total>=4){_mm_storeu_si128((__m128i*)p,v);p+=16;total-=4;}
            uint32* p32=(uint32*)p; while(total-->0) *p32++=px;
        #else
            uint32* p32=(uint32*)pixels; for(usize i=0;i<total;++i) p32[i]=px;
        #endif
        }
        
        static void FastClearDepth(float32* buf, uint32 w, uint32 h) {
            const uint32 one=0x3F800000u; uint32* p=(uint32*)buf; usize total=(usize)w*h;
        #ifdef NK_SW_AVX2
            const __m256i v=_mm256_set1_epi32((int)one); usize i=0;
            for(;i+8<=total;i+=8) _mm256_storeu_si256((__m256i*)(p+i),v);
            for(;i<total;++i) p[i]=one;
        #elif defined(NK_SW_SSE2)
            const __m128i v=_mm_set1_epi32((int)one); usize i=0;
            for(;i+4<=total;i+=4) _mm_storeu_si128((__m128i*)(p+i),v);
            for(;i<total;++i) p[i]=one;
        #else
            for(usize i=0;i<total;++i) p[i]=one;
        #endif
        }
        
#if 0 // [mort v4] rasterizer scanline (SWVert/Edge/HStep) — remplace par Raster/NkSWRasterCore.h
        // =============================================================================
        // Varying complet : toutes les varyings d'un vertex projeté en screen space
        // On sépare clairement screen coords (sx,sy,sz) des varyings NkVertexSoftware
        // =============================================================================
        struct SWVert {
            float sx, sy, sz;                // screen x, y, z∈[0,1]
            float r,g,b,a;                   // color
            float u,v;                       // uv
            float nx,ny,nz;                  // normal
            float attrs[16];                 // varyings génériques (worldPos, shadowCoord…)
            uint32 attrCount;                // nb d'attrs réellement utilisés
        };
        
        static SWVert ToSWVert(const NkVertexSoftware& vtx, float W, float H) {
            SWVert s;
            const float invW = (vtx.position.w > 1e-7f) ? 1.f/vtx.position.w : 0.f;
            s.sx = (vtx.position.x * invW + 1.f) * 0.5f * W;
            s.sy = (1.f - vtx.position.y * invW) * 0.5f * H;
            s.sz = vtx.position.z * invW * 0.5f + 0.5f;
            s.r  = vtx.color.r; s.g = vtx.color.g; s.b = vtx.color.b; s.a = vtx.color.a;
            s.u  = vtx.uv.x;    s.v = vtx.uv.y;
            s.nx = vtx.normal.x; s.ny = vtx.normal.y; s.nz = vtx.normal.z;
            s.attrCount = vtx.attrCount < 16 ? vtx.attrCount : 16;
            for (uint32 i=0; i<s.attrCount; ++i) s.attrs[i] = vtx.attrs[i];
            for (uint32 i=s.attrCount; i<16; ++i) s.attrs[i] = 0.f;
            return s;
        }
        
        // =============================================================================
        // Edge interpolator — travaille UNIQUEMENT en screen space
        // Tous les deltas sont exprimés par ligne (dy en pixels)
        // =============================================================================
        struct Edge {
            float x, z;
            float r,g,b,a, u,v, nx,ny,nz;
            float attrs[16];
            uint32 attrCount;
        
            float dx,dz;
            float dr,dg,db,da, du,dv, dnx,dny,dnz;
            float dattrs[16];
        
            void Init(const SWVert& A, const SWVert& B, float startY) {
                attrCount = A.attrCount < B.attrCount ? B.attrCount : A.attrCount;
                const float dy = B.sy - A.sy;
                if (fabsf(dy) < 1e-6f) {
                    // Edge horizontal : deltas nuls, valeurs de A
                    x=A.sx; z=A.sz;
                    r=A.r; g=A.g; b=A.b; a=A.a;
                    u=A.u; v=A.v;
                    nx=A.nx; ny=A.ny; nz=A.nz;
                    for (uint32 i=0;i<attrCount;++i) attrs[i]=A.attrs[i];
                    dx=dz=dr=dg=db=da=du=dv=dnx=dny=dnz=0;
                    for (uint32 i=0;i<attrCount;++i) dattrs[i]=0;
                    return;
                }
                const float inv = 1.f/dy;
                const float t   = startY - A.sy;
        
                dx  = (B.sx-A.sx)*inv;   dz  = (B.sz-A.sz)*inv;
                dr  = (B.r -A.r )*inv;   dg  = (B.g -A.g )*inv;
                db  = (B.b -A.b )*inv;   da  = (B.a -A.a )*inv;
                du  = (B.u -A.u )*inv;   dv  = (B.v -A.v )*inv;
                dnx = (B.nx-A.nx)*inv;   dny = (B.ny-A.ny)*inv;   dnz = (B.nz-A.nz)*inv;
                for (uint32 i=0;i<attrCount;++i) dattrs[i]=(B.attrs[i]-A.attrs[i])*inv;
        
                x  = A.sx + dx*t;     z  = A.sz + dz*t;
                r  = A.r  + dr*t;     g  = A.g  + dg*t;
                b  = A.b  + db*t;     a  = A.a  + da*t;
                u  = A.u  + du*t;     v  = A.v  + dv*t;
                nx = A.nx + dnx*t;    ny = A.ny + dny*t;    nz = A.nz + dnz*t;
                for (uint32 i=0;i<attrCount;++i) attrs[i]=A.attrs[i]+dattrs[i]*t;
            }
        
            void Step() {
                x+=dx; z+=dz;
                r+=dr; g+=dg; b+=db; a+=da;
                u+=du; v+=dv;
                nx+=dnx; ny+=dny; nz+=dnz;
                for (uint32 i=0;i<attrCount;++i) attrs[i]+=dattrs[i];
            }
        };
        
        // Interpolation horizontale optimisée
        // Au lieu d'une structure HSpan complète, on interpole inline avec SIMD sur attrs
        struct HStep {
            float z,r,g,b,a,u,v,nx,ny,nz;
            float attrs[16];
            uint32 attrCount;
        
            float dz,dr,dg,db,da,du,dv,dnx,dny,dnz;
            float dattrs[16];
        
            void Init(const Edge& L, const Edge& R, float spanW, float offX) {
                attrCount = L.attrCount;
                const float inv = spanW > 1e-6f ? 1.f/spanW : 0.f;
        
        #define HINIT(f) d##f=(R.f-L.f)*inv; f=L.f+d##f*offX
                HINIT(z); HINIT(r); HINIT(g); HINIT(b); HINIT(a);
                HINIT(u); HINIT(v); HINIT(nx); HINIT(ny); HINIT(nz);
        #undef HINIT
        
        #ifdef NK_SW_SSE2
                // SIMD sur 4 attrs à la fois
                const uint32 n4 = (attrCount >> 2) << 2; // arrondi inf à multiple de 4
                for (uint32 i=0; i<n4; i+=4) {
                    __m128 vA  = _mm_loadu_ps(L.attrs+i);
                    __m128 vB  = _mm_loadu_ps(R.attrs+i);
                    __m128 vD  = _mm_mul_ps(_mm_sub_ps(vB,vA), _mm_set1_ps(inv));
                    __m128 vV  = _mm_add_ps(vA, _mm_mul_ps(vD, _mm_set1_ps(offX)));
                    _mm_storeu_ps(dattrs+i, vD);
                    _mm_storeu_ps(attrs+i,  vV);
                }
                for (uint32 i=n4; i<attrCount; ++i) {
                    dattrs[i]=(R.attrs[i]-L.attrs[i])*inv;
                    attrs[i]=L.attrs[i]+dattrs[i]*offX;
                }
        #else
                for (uint32 i=0;i<attrCount;++i) {
                    dattrs[i]=(R.attrs[i]-L.attrs[i])*inv;
                    attrs[i]=L.attrs[i]+dattrs[i]*offX;
                }
        #endif
            }
        
            void Step() {
                z+=dz; r+=dr; g+=dg; b+=db; a+=da;
                u+=du; v+=dv; nx+=dnx; ny+=dny; nz+=dnz;
        #ifdef NK_SW_SSE2
                const uint32 n4=(attrCount>>2)<<2;
                for (uint32 i=0;i<n4;i+=4) {
                    __m128 vV=_mm_loadu_ps(attrs+i);
                    __m128 vD=_mm_loadu_ps(dattrs+i);
                    _mm_storeu_ps(attrs+i, _mm_add_ps(vV,vD));
                }
                for (uint32 i=n4;i<attrCount;++i) attrs[i]+=dattrs[i];
        #else
                for (uint32 i=0;i<attrCount;++i) attrs[i]+=dattrs[i];
        #endif
            }
        
            NkVertexSoftware ToFrag(float fx, float fy) const {
                NkVertexSoftware f{};
                f.position = {fx, fy, z, 1.f};
                f.color    = {r,g,b,a};
                f.uv       = {u,v};
                f.normal   = {nx,ny,nz};
                f.attrCount= attrCount;
                for (uint32 i=0;i<attrCount;++i) f.attrs[i]=attrs[i];
                return f;
            }
        };
        
#endif // [mort v4] SWVert/Edge/HStep scanline
        // =============================================================================
        // NkSWTextureBatch
        // =============================================================================
        #ifndef NK_SW_TEXTURE_BATCH_DEFINED
        #define NK_SW_TEXTURE_BATCH_DEFINED
        static constexpr uint32 kMaxTextures = 8;
        struct NkSWTextureBatch {
            const NkSWTexture* tex[kMaxTextures]={};
            uint32 count=0;
        };
        #endif
        
#if 0 // [mort v4] ancien scanline — remplace par swraster::DrawTriangle (perspective-correct + clip)
        // =============================================================================
        // DrawTriangleFast — version corrigée et optimisée
        // =============================================================================
        static void DrawTriangleFast(
            const NkVertexSoftware& v0,
            const NkVertexSoftware& v1,
            const NkVertexSoftware& v2,
            uint8*   colorBuf,
            float32* depthBuf,
            uint32 W, uint32 H,
            int32 clipX0, int32 clipY0, int32 clipX1, int32 clipY1,
            const NkSWShader* shader,
            const void* uniformData,
            const void* texPtr,
            bool depthTest, bool depthWrite)
        {
            if (W==0||H==0) return;
            // Reject derrière near plane
            if (v0.position.w<=0.f && v1.position.w<=0.f && v2.position.w<=0.f) return;
        
            const float fW=(float)W, fH=(float)H;
        
            // Convertir en screen space — coords explicites, pas dans position.x/y
            SWVert s0=ToSWVert(v0,fW,fH);
            SWVert s1=ToSWVert(v1,fW,fH);
            SWVert s2=ToSWVert(v2,fW,fH);
        
            // Backface / dégénéré
            const float area2=(s1.sx-s0.sx)*(s2.sy-s0.sy)-(s2.sx-s0.sx)*(s1.sy-s0.sy);
            if (fabsf(area2)<0.5f) return;
        
            // Tri par Y croissant (screen Y = vers le bas)
            if (s0.sy>s1.sy) { SWVert t=s0; s0=s1; s1=t; }
            if (s1.sy>s2.sy) { SWVert t=s1; s1=s2; s2=t; }
            if (s0.sy>s1.sy) { SWVert t=s0; s0=s1; s1=t; }
        
            const int32 yMin=(int32)ceilf(s0.sy);
            const int32 yMax=(int32)floorf(s2.sy);
            const int32 cy0 = yMin<clipY0?clipY0:yMin;
            const int32 cy1 = yMax>clipY1-1?clipY1-1:yMax;
            if (cy0>cy1) return;
        
            const bool hasShader = shader && shader->fragFn;
            const bool depthOnly = (colorBuf==nullptr);
            const bool hasDepth  = depthBuf && (depthTest||depthWrite);
        
            // Texture
            const NkSWTexture* tex=nullptr; bool hasTex=false;
            if (!hasShader && !depthOnly && texPtr) {
                const auto* batch=static_cast<const NkSWTextureBatch*>(texPtr);
                if (batch&&batch->count>0&&batch->tex[0]&&!batch->tex[0]->mips.Empty())
                    {tex=batch->tex[0];hasTex=true;}
            }
            const int32 texW=hasTex?(int32)tex->Width(0):0;
            const int32 texH=hasTex?(int32)tex->Height(0):0;
            const uint8* texData=hasTex?tex->mips[0].Data():nullptr;
            const uint32 texBpp=hasTex?tex->Bpp():0;
        
            // Initialiser les deux arêtes
            Edge eL, eMid;
            eL.Init(s0, s2, (float)cy0);           // arête longue s0→s2
            bool inBottom = (float)cy0 > s1.sy;
            if (!inBottom) eMid.Init(s0, s1, (float)cy0);
            else           eMid.Init(s1, s2, (float)cy0);
        
            for (int32 y=cy0; y<=cy1; ++y) {
                if (!inBottom && (float)y > s1.sy) {
                    eMid.Init(s1, s2, (float)y);
                    inBottom=true;
                }
        
                // Assurer L.x <= R.x
                Edge* pL=&eL, *pR=&eMid;
                if (pL->x > pR->x) { Edge* t=pL; pL=pR; pR=t; }
        
                const int32 x0=(int32)ceilf(pL->x-0.5f);
                const int32 x1=(int32)floorf(pR->x+0.5f);
                const int32 cx0s=x0<clipX0?clipX0:x0;
                const int32 cx1s=x1>clipX1-1?clipX1-1:x1;
        
                if (cx0s<=cx1s) {
                    const float spanW=pR->x-pL->x;
                    const float offX=(float)cx0s-pL->x+0.5f;
        
                    float32* depthRow=hasDepth?(depthBuf+(uint32)y*W):nullptr;
        
                    // ── FAST PATH : depth-only (shadow pass) ─────────────────────────
                    if (depthOnly) {
                        if (depthRow) {
                            HStep hs; hs.dz=(pR->z-pL->z)*(spanW>1e-6f?1.f/spanW:0.f);
                            hs.z=pL->z+hs.dz*offX;
                            for (int32 x=cx0s;x<=cx1s;++x) {
                                if (!depthTest||hs.z<=depthRow[x])
                                    if (depthWrite) depthRow[x]=hs.z;
                                hs.z+=hs.dz;
                            }
                        }
                        eL.Step(); eMid.Step(); continue;
                    }
        
                    uint8* row=colorBuf+(uint32)y*W*4u;
        
                    // ── FAST PATH : couleur unie, sans profondeur, sans shader ────────
                    const float inv2=spanW>1e-6f?1.f/spanW:0.f;
                    const bool uniformCol=
                        fabsf((pR->r-pL->r)*inv2)<0.5f/255.f &&
                        fabsf((pR->g-pL->g)*inv2)<0.5f/255.f &&
                        fabsf((pR->b-pL->b)*inv2)<0.5f/255.f;
        
                    if (!hasShader && !hasTex && !hasDepth && uniformCol) {
                        FillSpanFast(row, cx0s, cx1s+1,
                            (uint8)(pL->r*255.f+.5f),
                            (uint8)(pL->g*255.f+.5f),
                            (uint8)(pL->b*255.f+.5f));
                        eL.Step(); eMid.Step(); continue;
                    }
        
                    // ── CHEMIN GÉNÉRAL : interpoler toutes les varyings ───────────────
                    HStep hs;
                    hs.Init(*pL, *pR, spanW, offX);
        
                    for (int32 x=cx0s; x<=cx1s; ++x) {
                        if (depthRow) {
                            if (depthTest && hs.z>depthRow[x]) { hs.Step(); continue; }
                            if (depthWrite) depthRow[x]=hs.z;
                        }
        
                        uint8 sr,sg,sb,sa;
                        if (hasShader) {
                            // Fragment shader reçoit TOUTES les varyings correctement interpolées
                            NkVertexSoftware frag=hs.ToFrag((float)x+.5f,(float)y+.5f);
                            auto c=shader->fragFn(frag,uniformData,texPtr);
                            sr=(uint8)(c.r*255.f+.5f); sg=(uint8)(c.g*255.f+.5f);
                            sb=(uint8)(c.b*255.f+.5f); sa=(uint8)(c.a*255.f+.5f);
                        } else if (hasTex) {
                            const int32 tx=(int32)(hs.u*texW), ty_=(int32)(hs.v*texH);
                            const int32 px=tx<0?0:(tx>=texW?texW-1:tx);
                            const int32 py=ty_<0?0:(ty_>=texH?texH-1:ty_);
                            const uint8* tp=texData+((uint32)py*texW+(uint32)px)*texBpp;
                            float tr,tg,tb,ta;
                            if(texBpp>=4){tr=tp[0]/255.f;tg=tp[1]/255.f;tb=tp[2]/255.f;ta=tp[3]/255.f;}
                            else if(texBpp==3){tr=tp[0]/255.f;tg=tp[1]/255.f;tb=tp[2]/255.f;ta=1.f;}
                            else{tr=tg=tb=tp[0]/255.f;ta=1.f;}
                            sr=(uint8)((tr*hs.r)*255.f+.5f); sg=(uint8)((tg*hs.g)*255.f+.5f);
                            sb=(uint8)((tb*hs.b)*255.f+.5f); sa=(uint8)((ta*hs.a)*255.f+.5f);
                        } else {
                            sr=(uint8)(hs.r*255.f+.5f); sg=(uint8)(hs.g*255.f+.5f);
                            sb=(uint8)(hs.b*255.f+.5f); sa=(uint8)(hs.a*255.f+.5f);
                        }
        
                        uint8* p=row+x*4;
                        if (sa==255u) sw_detail::StorePixel(p,sr,sg,sb,sa);
                        else if (sa>0u) sw_detail::BlendPixel(p,sr,sg,sb,sa);
        
                        hs.Step();
                    }
                }
                eL.Step(); eMid.Step();
            }
        }
        
#endif // [mort v4] ancien scanline DrawTriangleFast
        // =============================================================================
        // Ressources résolues
        // =============================================================================
        struct NkSWResolvedResources {
            uint8*   colorBuf=nullptr;
            float32* depthBuf=nullptr;
            uint32   W=0,H=0;
            const NkSWShader* shader=nullptr;
            const void*       uniforms=nullptr;
            NkSWTextureBatch  texBatch;
            bool depthTest=false, depthWrite=false;
            // v4 : état pipeline complet (perspective-correct core)
            NkCompareOp   depthOp   = NkCompareOp::NK_LESS;
            NkCullMode    cullMode  = NkCullMode::NK_NONE;
            NkFrontFace   frontFace = NkFrontFace::NK_CCW;
            bool          blendEnable = false;
            NkBlendFactor srcColor  = NkBlendFactor::NK_SRC_ALPHA;
            NkBlendFactor dstColor  = NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
            NkPrimitiveTopology topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            uint32 stride=0;
            const uint8* vdata=nullptr;
            uint64 vbCap=0;
            bool valid=false;
            bool vertexless=false;   // draw plein-écran gl_VertexID (pas de VBO) : le vertFn synthétise les sommets
            bool colorFloat=false;   // RT couleur HDR RGBA32F (float, sans clamp) — sinon RGBA8/BGRA
            uint32 colorBpp=4u;      // octets/pixel du color buffer (4 ou 16)
            // Viewport SOUS-RECT (slots du shadow atlas). vpW==0 => plein RT (mapping historique).
            float32 vpX=0, vpY=0, vpW=0, vpH=0; bool vpFlipY=true;
        };
        
        static NkSWResolvedResources ResolveResources(
            NkSoftwareDevice* dev,
            uint64 pipelineId, uint64 descSetId,
            uint64 vbId, uint64 vbOffset, uint64 fbId)
        {
            NkSWResolvedResources r;
            auto* pipe=dev->GetPipe(pipelineId);
            auto* sh  =pipe?dev->GetShader(pipe->shaderId):nullptr;
            auto* vb  =dev->GetBuf(vbId);
            const bool noVB = (!vb||vb->data.Empty()||vbOffset>=vb->data.Size());
            // Draw plein-écran généré (gl_VertexID) : pipeline sans attributs (stride==0) ET shader
            // qui opte pour la synthèse depuis l'index (genVertsFromID). On exige l'opt-in : un
            // vertFn 3D lirait vdata+idx*stride et crasherait sur vdata==nullptr.
            const bool genVerts = pipe && pipe->vertexStride==0 && sh && sh->genVertsFromID && sh->vertFn;
            if (!genVerts && noVB) return r;   // attributs requis mais pas de VB

            auto* fbo=fbId?dev->GetFBO(fbId):nullptr;
            if (!fbo) fbo=dev->GetFBO(dev->GetSwapchainFramebuffer().id);
            if (!fbo) return r;
        
            auto* colorTex=fbo->colorId?dev->GetTex(fbo->colorId):nullptr;
            auto* depthTex=fbo->depthId?dev->GetTex(fbo->depthId):nullptr;
            r.colorBuf=(colorTex&&!colorTex->mips.Empty())?colorTex->mips[0].Data():nullptr;
            r.depthBuf=(depthTex&&!depthTex->mips.Empty())?(float32*)depthTex->mips[0].Data():nullptr;
        
            if (colorTex)      {r.W=colorTex->Width(0);r.H=colorTex->Height(0);
                                r.colorBpp=colorTex->Bpp();
                                r.colorFloat=(colorTex->desc.format==NkGPUFormat::NK_RGBA32_FLOAT);}
            else if (depthTex) {r.W=depthTex->Width(0);r.H=depthTex->Height(0);}
            else return r;
            if (r.W==0||r.H==0) return r;
        
            r.shader    =sh;
            r.depthTest =pipe?pipe->depthTest :true;
            r.depthWrite=pipe?pipe->depthWrite:true;
            if (pipe) {
                r.depthOp    =pipe->depthOp;
                r.cullMode   =pipe->cullMode;
                r.frontFace  =pipe->frontFace;
                r.blendEnable=pipe->blendEnable;
                r.srcColor   =pipe->srcColor;
                r.dstColor   =pipe->dstColor;
                r.topology   =pipe->topology;
            }
            r.stride    =pipe?pipe->vertexStride:0;
            if (genVerts) {
                // Sommets générés : le vertFn ignore vdata et synthétise la position depuis l'index.
                r.vertexless=true;
                r.vdata=nullptr;
                r.vbCap=0;
            } else {
                if (r.stride==0) return r;   // pipeline stride-0 sans opt-in : rien à indexer
                r.vdata=vb->data.Data()+vbOffset;
                r.vbCap=(vb->data.Size()-vbOffset)/r.stride;
            }

            if (descSetId) {
                if (auto* ds=dev->GetDescSet(descSetId)) {
                    uint32 texIdx=0;
                    for (uint32 i=0;i<(uint32)ds->bindings.Size();++i) {
                        const auto& b=ds->bindings[i];
                        if (!r.uniforms &&
                            (b.type==NkDescriptorType::NK_UNIFORM_BUFFER||
                            b.type==NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC)&&b.bufId) {
                            if (auto* ub=dev->GetBuf(b.bufId)) r.uniforms=ub->data.Data();
                        }
                        if ((b.type==NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER||
                            b.type==NkDescriptorType::NK_SAMPLED_TEXTURE)&&b.texId&&texIdx<kMaxTextures) {
                            auto* t=dev->GetTex(b.texId);
                            // Ignorer les depth textures (shadow map) comme color sampler
                            if (t&&t->desc.format!=NkGPUFormat::NK_D32_FLOAT&&
                                t->desc.format!=NkGPUFormat::NK_D24_UNORM_S8_UINT)
                                r.texBatch.tex[texIdx++]=t;
                        }
                    }
                    r.texBatch.count=texIdx;
                }
            }
            // Viewport : n'appliquer un mapping sous-rect QUE si un viewport partiel est actif
            // (largeur>0 ET pas plein RT). Le rendu principal ne set pas de viewport partiel, donc
            // aucune régression. Les slots VSM (SetViewport par slot) tombent ici.
            {
                const NkViewport& vp = dev->SwCurViewport();
                const bool fullRT = (vp.x==0.f && vp.y==0.f &&
                                     (uint32)(vp.width+0.5f)>=r.W && (uint32)(vp.height+0.5f)>=r.H);
                if (vp.width>0.f && vp.height>0.f && !fullRT) {
                    r.vpX=vp.x; r.vpY=vp.y; r.vpW=vp.width; r.vpH=vp.height; r.vpFlipY=vp.flipY;
                }
            }
            r.valid=true;
            return r;
        }
        
        // =============================================================================
        // Rasteriser une liste de triangles (batch)
        // =============================================================================
        static void RasterizeList(
            NkVertexSoftware* verts, uint32 count,
            const NkSWResolvedResources& r,
            int32 cx0, int32 cy0, int32 cx1, int32 cy1)
        {
            // État de rendu (perspective-correct core v4)
            swraster::NkSWRenderState st;
            st.depthTest = r.depthTest; st.depthWrite = r.depthWrite; st.depthOp = r.depthOp;
            st.cullMode = r.cullMode;   st.frontFace = r.frontFace;
            st.blendEnable = r.blendEnable; st.srcColor = r.srcColor; st.dstColor = r.dstColor;
            st.colorFloat = r.colorFloat; st.colorBpp = r.colorBpp;   // RT HDR float (sans clamp) si RGBA32F

            // Miroir du batch de textures pour le sampling non-shader
            swraster::NkSWTexBatch tb;
            tb.count = r.texBatch.count < 8 ? r.texBatch.count : 8;
            for (uint32 i = 0; i < tb.count; ++i) tb.tex[i] = r.texBatch.tex[i];

            // texPtr transmis TEL QUEL au fragment shader (le shader de la démo en dépend)
            const void* texPtr = r.texBatch.count > 0 ? (const void*)&r.texBatch : nullptr;

            const int32 rectW = cx1 - cx0, rectH = cy1 - cy0;
            if (rectW <= 0 || rectH <= 0) return;

            // ── Étape 1 : clip near + projection de TOUS les triangles, UNE seule fois.
            // Setup fait une fois par draw puis réutilisé par toutes les tuiles → plus de
            // re-projection redondante par tuile (buffer thread-local réutilisé, sans alloc/frame).
            // Viewport sous-rect (slots VSM) : nullptr => plein RT (mapping historique inchangé).
            swraster::NkSWViewport vpLocal{ r.vpX, r.vpY, r.vpW, r.vpH, r.vpFlipY };
            const swraster::NkSWViewport* vpPtr = (r.vpW>0.f) ? &vpLocal : nullptr;

            static thread_local NkVector<swraster::ReadyTri> tl_ready;
            tl_ready.Clear();
            {
                swraster::ReadyTri tmp[2];
                #define NK_SW_PUSH(a,b,c) do { \
                        uint32 _k = swraster::ClipProject((a),(b),(c),(float32)r.W,(float32)r.H,tmp,vpPtr); \
                        for (uint32 _i=0;_i<_k;++_i) tl_ready.PushBack(tmp[_i]); } while(0)
                switch (r.topology) {
                    case NkPrimitiveTopology::NK_TRIANGLE_STRIP:
                        for (uint32 i = 2; i < count; ++i) {
                            if (i & 1u) NK_SW_PUSH(verts[i-1], verts[i-2], verts[i]);
                            else        NK_SW_PUSH(verts[i-2], verts[i-1], verts[i]);
                        }
                        break;
                    case NkPrimitiveTopology::NK_TRIANGLE_FAN:
                        for (uint32 i = 2; i < count; ++i) NK_SW_PUSH(verts[0], verts[i-1], verts[i]);
                        break;
                    case NkPrimitiveTopology::NK_TRIANGLE_LIST:
                    default:
                        for (uint32 i = 0; i + 2 < count; i += 3) NK_SW_PUSH(verts[i], verts[i+1], verts[i+2]);
                        break;
                }
                #undef NK_SW_PUSH
            }
            const uint32 triCount = (uint32)tl_ready.Size();
            if (triCount == 0) return;
            const swraster::ReadyTri* tris = tl_ready.Data();

            // ── Étape 2 : rasterise les triangles pré-projetés dans un clip rect (= une tuile).
            // Chaque tuile possède ses pixels de façon exclusive → aucune écriture concurrente,
            // donc AUCUN verrou. NB : les fragment shaders doivent être read-only (appelés en //).
            auto rasterRect = [&](int32 tX0, int32 tY0, int32 tX1, int32 tY1) {
                for (uint32 t = 0; t < triCount; ++t)
                    swraster::RasterScreenTriangle(tris[t].s[0], tris[t].s[1], tris[t].s[2],
                        r.colorBuf, r.depthBuf, r.W, r.H, tX0, tY0, tX1, tY1,
                        r.shader, r.uniforms, texPtr, &tb, st);
            };

            // Interrupteur de benchmark : NK_SW_NOMT=1 force le rendu mono-thread (A/B perf).
            static const bool s_noMT = [](){ const char* e = std::getenv("NK_SW_NOMT"); return e && e[0]=='1'; }();

            // PERF : ne tuiler/paralléliser que la BOUNDING BOX écran des triangles de CE draw, pas
            // tout le scissor (souvent plein écran). Sinon un petit draw (sphère, cube) dispatchait
            // 240 tuiles au thread pool dont ~238 vides -> l'overhead de dispatch rendait le
            // multi-thread PLUS LENT que le mono-thread (1500+ petits draws par frame).
            float bbx0=1e30f, bby0=1e30f, bbx1=-1e30f, bby1=-1e30f;
            for (uint32 t=0;t<triCount;++t) for (int k=0;k<3;++k){
                const float x=tris[t].s[k].sx, y=tris[t].s[k].sy;
                if(x<bbx0)bbx0=x; if(x>bbx1)bbx1=x; if(y<bby0)bby0=y; if(y>bby1)bby1=y; }
            int32 bx0 = cx0, by0 = cy0, bx1 = cx1, by1 = cy1;
            if (bbx1>=bbx0) { // bbox valide -> restreindre au recouvrement bbox ∩ scissor
                int32 fx0=(int32)floorf(bbx0), fy0=(int32)floorf(bby0);
                int32 fx1=(int32)ceilf(bbx1)+1, fy1=(int32)ceilf(bby1)+1;
                if (fx0>bx0)bx0=fx0; if (fy0>by0)by0=fy0; if (fx1<bx1)bx1=fx1; if (fy1<by1)by1=fy1;
            }
            if (bx1<=bx0 || by1<=by0) return;                 // draw hors écran
            const int32 bW = bx1-bx0, bH = by1-by0;

            threading::NkThreadPool& pool = threading::NkThreadPool::GetGlobal();
            const uint32 workers = (uint32)pool.GetNumWorkers();
            const bool doParallel = !s_noMT && workers > 1 &&
                                    ((int64)bW * (int64)bH) >= (int64)(160 * 160);
            if (!doParallel) { rasterRect(bx0, by0, bx1, by1); return; }

            const int32 TILE = 64;
            const int32 tilesX = (bW + TILE - 1) / TILE;
            const int32 tilesY = (bH + TILE - 1) / TILE;
            const uint32 numTiles = (uint32)tilesX * (uint32)tilesY;
            nk_size grain = (nk_size)(numTiles / (workers * 4u));
            if (grain == 0) grain = 1;

            pool.ParallelFor((nk_size)numTiles, [&](nk_size t) {
                const int32 ix = (int32)((uint32)t % (uint32)tilesX);
                const int32 iy = (int32)((uint32)t / (uint32)tilesX);
                int32 tX0 = bx0 + ix * TILE, tY0 = by0 + iy * TILE;
                int32 tX1 = tX0 + TILE; if (tX1 > bx1) tX1 = bx1;
                int32 tY1 = tY0 + TILE; if (tY1 > by1) tY1 = by1;
                rasterRect(tX0, tY0, tX1, tY1);
            }, grain);
            pool.Join();
        }
        
        // =============================================================================
        // ExecuteDrawFast
        // =============================================================================
        static void ExecuteDrawFast(
            NkSoftwareDevice* dev,
            uint64 pipelineId, uint64 descSetId,
            uint64 vbId, uint64 vbOffset, uint64 fbId,
            uint32 firstVertex, uint32 vertexCount, uint32 instanceCount,
            int32 cx0, int32 cy0, int32 cx1, int32 cy1)
        {
            auto r=ResolveResources(dev,pipelineId,descSetId,vbId,vbOffset,fbId);
            if (!r.valid) return;
            if (!r.vertexless && r.vbCap==0) return;

            uint32 safe;
            if (r.vertexless) {
                // Sommets synthétisés par le vertFn depuis l'index (pas de VB à borner).
                safe=vertexCount;
            } else {
                const uint64 avail=r.vbCap>(uint64)firstVertex?r.vbCap-(uint64)firstVertex:0;
                safe=(uint32)((uint64)vertexCount<avail?vertexCount:avail);
            }
            if (safe<3) return;

            dev->SwSetCurStride(r.stride);   // pour le shader taillé NKRenderer (lit vdata+idx*stride)
            NkVertexSoftware* verts=tl_vertPool.Alloc(safe);
            if (!verts) return;

            // Génère les sommets (le vertFn peut dépendre de SwCurInstance pour l'instancing).
            auto genVerts=[&](){
                if (r.vertexless) {
                    // vdata=nullptr -> le vertFn génère le triangle plein-écran depuis l'index.
                    for (uint32 j=0;j<safe;++j)
                        verts[j]=r.shader->vertFn(nullptr,firstVertex+j,r.uniforms);
                } else if (r.shader&&r.shader->vertFn) {
                    for (uint32 j=0;j<safe;++j)
                        verts[j]=r.shader->vertFn(r.vdata,firstVertex+j,r.uniforms);
                } else {
                    for (uint32 j=0;j<safe;++j) {
                        const uint8* src=r.vdata+(uint64)(firstVertex+j)*r.stride;
                        verts[j]={}; verts[j].color={1,1,1,1};
                        if (r.stride>=12){float px,py,pz;memcpy(&px,src,4);memcpy(&py,src+4,4);memcpy(&pz,src+8,4);verts[j].position={px,py,pz,1.f};}
                    }
                }
            };

            // Chemin INSTANCIÉ : re-génère les sommets par instance (vertFn indexe l'InstanceUBO).
            if (r.shader && r.shader->usesInstancing && instanceCount>1) {
                for (uint32 inst=0;inst<instanceCount;++inst) {
                    dev->SwSetCurInstance(inst);
                    genVerts();
                    RasterizeList(verts,safe,r,cx0,cy0,cx1,cy1);
                }
                dev->SwSetCurInstance(0);
                return;
            }

            genVerts();

            // v4 Phase 4 : collecte de la géométrie pour le BPR (passe caméra = swapchain uniquement).
            if (dev->RtCollecting() && (fbId==0 || fbId==dev->GetSwapchainFramebuffer().id))
                for (uint32 i=0;i+2<safe;i+=3)
                    dev->RtAddTriangle(verts[i],verts[i+1],verts[i+2]);

            for (uint32 inst=0;inst<instanceCount;++inst)
                RasterizeList(verts,safe,r,cx0,cy0,cx1,cy1);
        }
        
        // =============================================================================
        // ExecuteDrawIndexedFast
        // =============================================================================
        static void ExecuteDrawIndexedFast(
            NkSoftwareDevice* dev,
            uint64 pipelineId, uint64 descSetId,
            uint64 vbId, uint64 vbOffset,
            uint64 ibId, uint64 ibOffset, bool indexUint32,
            uint64 fbId,
            uint32 firstIndex, uint32 indexCount, int32 vertexOffset,
            uint32 instanceCount,
            int32 cx0, int32 cy0, int32 cx1, int32 cy1)
        {
            auto r=ResolveResources(dev,pipelineId,descSetId,vbId,vbOffset,fbId);
            if (!r.valid) return;
            auto* ib=dev->GetBuf(ibId);
            if (!ib||ib->data.Empty()||ibOffset>=ib->data.Size()) return;
        
            const uint64 idxStep=indexUint32?4ull:2ull;
            const uint64 ibCap=(ib->data.Size()-ibOffset)/idxStep;
            if ((uint64)firstIndex>=ibCap) return;
            const uint32 safe=(uint32)((uint64)indexCount<ibCap-(uint64)firstIndex?indexCount:ibCap-(uint64)firstIndex);
            if (safe<3) return;
        
            const uint8* idata=ib->data.Data()+ibOffset;
            dev->SwSetCurStride(r.stride);   // pour le shader taillé NKRenderer (lit vdata+idx*stride)
            NkVertexSoftware* verts=tl_vertPool.Alloc(safe);
            if (!verts) return;

            // Génère les sommets (le vertFn peut dépendre de SwCurInstance pour l'instancing).
            auto genVerts=[&](){
                for (uint32 j=0;j<safe;++j) {
                    const uint32 rawIdx=indexUint32?((const uint32*)idata)[firstIndex+j]:(uint32)((const uint16*)idata)[firstIndex+j];
                    const int64 vi64=(int64)rawIdx+(int64)vertexOffset;
                    if (vi64<0||(uint64)vi64>=r.vbCap){verts[j]={};verts[j].color={1,1,1,1};continue;}
                    const uint32 vi=(uint32)vi64;
                    if (r.shader&&r.shader->vertFn)
                        verts[j]=r.shader->vertFn(r.vdata,vi,r.uniforms);
                    else {
                        const uint8* src=r.vdata+(uint64)vi*r.stride;
                        verts[j]={}; verts[j].color={1,1,1,1};
                        if (r.stride>=12){float px,py,pz;memcpy(&px,src,4);memcpy(&py,src+4,4);memcpy(&pz,src+8,4);verts[j].position={px,py,pz,1.f};}
                    }
                }
            };

            // Chemin INSTANCIÉ (ex. ombres des cubes) : le vertFn indexe l'InstanceUBO via
            // SwCurInstance → on doit RE-GÉNÉRER les sommets pour chaque instance. Sinon (défaut),
            // on génère une fois et on réutilise pour toutes les instances (comportement historique).
            if (r.shader && r.shader->usesInstancing && instanceCount>1) {
                for (uint32 inst=0;inst<instanceCount;++inst) {
                    dev->SwSetCurInstance(inst);
                    genVerts();
                    RasterizeList(verts,safe,r,cx0,cy0,cx1,cy1);
                }
                dev->SwSetCurInstance(0);
                return;
            }

            genVerts();

            // v4 Phase 4 : collecte de la géométrie pour le BPR (passe caméra = swapchain uniquement).
            if (dev->RtCollecting() && (fbId==0 || fbId==dev->GetSwapchainFramebuffer().id))
                for (uint32 i=0;i+2<safe;i+=3)
                    dev->RtAddTriangle(verts[i],verts[i+1],verts[i+2]);

            for (uint32 inst=0;inst<instanceCount;++inst)
                RasterizeList(verts,safe,r,cx0,cy0,cx1,cy1);
        }

        // =============================================================================
        // ExecuteDrawIndirectFast — version indirecte de Draw
        // Lit les arguments depuis un buffer et appelle ExecuteDrawFast
        // =============================================================================
        static void ExecuteDrawIndirectFast(
            NkSoftwareDevice* dev,
            uint64 pipelineId, uint64 descSetId,
            uint64 vbId, uint64 vbOffset,
            uint64 indirectBufId, uint64 indirectOffset,
            uint64 fbId,
            uint32 drawStride,  // généralement 16 (4 * uint32)
            const NkRect2D& scissorRect, bool scissorEnabled)
        {
            auto* indirectBuf = dev->GetBuf(indirectBufId);
            if (!indirectBuf) return;
            if (indirectOffset + drawStride > indirectBuf->data.Size()) return;

            // Lire les arguments depuis le buffer
            const uint8* args = indirectBuf->data.Data() + indirectOffset;
            uint32 vertexCount   = 0;
            uint32 instanceCount = 0;
            uint32 firstVertex   = 0;
            uint32 firstInstance = 0;

            memcpy(&vertexCount,   args + 0, 4);
            memcpy(&instanceCount, args + 4, 4);
            memcpy(&firstVertex,   args + 8, 4);
            memcpy(&firstInstance, args + 12, 4);

            if (vertexCount == 0) return;
            if (instanceCount == 0) instanceCount = 1;

            // Réutiliser le fast-path non indexé
            ExecuteDrawFast(dev, pipelineId, descSetId, vbId, vbOffset, fbId, firstVertex, vertexCount, instanceCount, scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);
        }

        // =============================================================================
        // ExecuteDrawIndexedIndirectFast — version indirecte de DrawIndexed
        // Lit les arguments depuis un buffer et appelle ExecuteDrawIndexedFast
        // =============================================================================
        static void ExecuteDrawIndexedIndirectFast(
            NkSoftwareDevice* dev,
            uint64 pipelineId, uint64 descSetId,
            uint64 vbId, uint64 vbOffset,
            uint64 ibId, uint64 ibOffset, bool indexUint32,
            uint64 indirectBufId, uint64 indirectOffset,
            uint64 fbId,
            uint32 drawStride,  // généralement 20 (5 * uint32)
            const NkRect2D& scissorRect, bool scissorEnabled)
        {
            auto* indirectBuf = dev->GetBuf(indirectBufId);
            if (!indirectBuf) return;
            if (indirectOffset + drawStride > indirectBuf->data.Size()) return;

            const uint8* args = indirectBuf->data.Data() + indirectOffset;
            uint32 indexCount    = 0;
            uint32 instanceCount = 0;
            uint32 firstIndex    = 0;
            int32  vertexOffset  = 0;
            uint32 firstInstance = 0;

            memcpy(&indexCount,    args + 0, 4);
            memcpy(&instanceCount, args + 4, 4);
            memcpy(&firstIndex,    args + 8, 4);
            memcpy(&vertexOffset,  args + 12, 4);
            memcpy(&firstInstance, args + 16, 4);

            if (indexCount == 0) return;
            if (instanceCount == 0) instanceCount = 1;
            
            swfast::ExecuteDrawIndexedFast(dev, pipelineId, descSetId, vbId, vbOffset, ibId, ibOffset, indexUint32, fbId, firstIndex, indexCount, vertexOffset, instanceCount, scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);
        }

    } // namespace swfast
} // namespace nkentseu