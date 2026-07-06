#pragma once
// =============================================================================
// NkSWRayTrace.h — Ray-tracing CPU (Phase 4 BPR, fondation)
//
// Chemin de rendu ALTERNATIF au rasterizer, orienté qualité (« Best Preview
// Render » façon ZBrush) : rayons caméra → intersection Möller-Trumbore →
// shading Lambert + ombres portées (shadow rays). BVH et Monte-Carlo (ombres
// douces / AO) à venir ; ce premier jet est en brute-force + ombre dure.
//
// Références Scratchapixel (archive locale D:\Scratchapixel\markdown) :
//   - ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
//   - ray-tracing-generating-camera-rays/generating-camera-rays
//   - introduction-to-shading/diffuse-lambertian-shading
//   - introduction-to-shading/ligth-and-shadows
//
// Auto-contenu : math vectoriel local (aucune dépendance NKMath) pour éviter
// tout couplage d'API. Périmètre strict : uniquement src/NKRHI/Software/.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKThreading/NkThreadPool.h"   // rendu multi-thread (par lignes)
#include "NKMath/NKMath.h"              // caméra = matrices view/proj (convention NKRHI/Grid3D)
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace nkentseu {
    namespace swtrace {

        // ── Math vectoriel local (indépendant de NKMath) ─────────────────────────
        struct V3 {
            float x=0, y=0, z=0;
            V3() {}
            V3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
        };
        static inline V3 operator+(const V3&a,const V3&b){ return V3(a.x+b.x,a.y+b.y,a.z+b.z); }
        static inline V3 operator-(const V3&a,const V3&b){ return V3(a.x-b.x,a.y-b.y,a.z-b.z); }
        static inline V3 operator*(const V3&a,float s){ return V3(a.x*s,a.y*s,a.z*s); }
        static inline float Dot(const V3&a,const V3&b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
        static inline V3 Cross(const V3&a,const V3&b){
            return V3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
        }
        static inline V3 Normalize(const V3&a){
            float l=sqrtf(Dot(a,a)); return l>1e-8f ? V3(a.x/l,a.y/l,a.z/l) : a;
        }

        struct Ray { V3 o, d; };                 // origine, direction (normalisée)
        struct Tri { V3 v0, v1, v2; V3 color; float reflect = 0.f; }; // + réflectivité [0..1]

        // ── Intersection Möller-Trumbore ─────────────────────────────────────────
        // Retourne true + t (distance) si le rayon touche le triangle (t > tMin).
        static inline bool IntersectMT(const Ray& r, const Tri& tri, float tMin, float& tOut) {
            const V3 e1 = tri.v1 - tri.v0;
            const V3 e2 = tri.v2 - tri.v0;
            const V3 p  = Cross(r.d, e2);
            const float det = Dot(e1, p);
            if (fabsf(det) < 1e-8f) return false;          // rayon parallèle
            const float invDet = 1.f / det;
            const V3 tv = r.o - tri.v0;
            const float u = Dot(tv, p) * invDet;
            if (u < 0.f || u > 1.f) return false;
            const V3 q = Cross(tv, e1);
            const float v = Dot(r.d, q) * invDet;
            if (v < 0.f || u + v > 1.f) return false;
            const float t = Dot(e2, q) * invDet;
            if (t <= tMin) return false;
            tOut = t;
            return true;
        }

        // ── Accélération BVH (median split) ──────────────────────────────────────
        // Réf. Scratchapixel : ray-tracing-polygon-mesh/* (acceleration structures).
        static inline float AxisVal(const V3& v, int a){ return a==0?v.x:(a==1?v.y:v.z); }

        struct AABB {
            V3 lo{ 1e30f, 1e30f, 1e30f};
            V3 hi{-1e30f,-1e30f,-1e30f};
            void Grow(const V3& p){
                lo.x=p.x<lo.x?p.x:lo.x; lo.y=p.y<lo.y?p.y:lo.y; lo.z=p.z<lo.z?p.z:lo.z;
                hi.x=p.x>hi.x?p.x:hi.x; hi.y=p.y>hi.y?p.y:hi.y; hi.z=p.z>hi.z?p.z:hi.z;
            }
        };

        // Slab test : retourne tNear (>=0, clampé) si la boîte est touchée avant tMax, sinon -1.
        static inline float RayAABB(const V3& o, const V3& invD, const V3& lo, const V3& hi, float tMax){
            float t1=(lo.x-o.x)*invD.x, t2=(hi.x-o.x)*invD.x;
            float tmin=fminf(t1,t2), tmax=fmaxf(t1,t2);
            t1=(lo.y-o.y)*invD.y; t2=(hi.y-o.y)*invD.y;
            tmin=fmaxf(tmin,fminf(t1,t2)); tmax=fminf(tmax,fmaxf(t1,t2));
            t1=(lo.z-o.z)*invD.z; t2=(hi.z-o.z)*invD.z;
            tmin=fmaxf(tmin,fminf(t1,t2)); tmax=fminf(tmax,fmaxf(t1,t2));
            if (tmax>=fmaxf(tmin,0.f) && tmin<tMax) return tmin<0.f?0.f:tmin;
            return -1.f;
        }

        struct BVHNode { V3 lo, hi; int start, count, left, right; };

        struct BVH {
            const Tri* tris=nullptr; int n=0;
            NkVector<int>     idx;
            NkVector<BVHNode> nodes;

            V3 Centroid(int t) const {
                const Tri& tr=tris[t];
                return V3((tr.v0.x+tr.v1.x+tr.v2.x)/3.f,
                          (tr.v0.y+tr.v1.y+tr.v2.y)/3.f,
                          (tr.v0.z+tr.v1.z+tr.v2.z)/3.f);
            }

            int Subdivide(int start, int count){
                const int nodeIdx=(int)nodes.Size();
                AABB b, cb;
                for (int i=0;i<count;++i){
                    const Tri& tr=tris[idx[start+i]];
                    b.Grow(tr.v0); b.Grow(tr.v1); b.Grow(tr.v2);
                    cb.Grow(Centroid(idx[start+i]));
                }
                BVHNode node; node.lo=b.lo; node.hi=b.hi;
                node.start=start; node.count=count; node.left=-1; node.right=-1;
                nodes.PushBack(node);
                if (count<=2) return nodeIdx;                       // feuille

                const V3 ext = cb.hi - cb.lo;
                const int axis = (ext.x>ext.y)?((ext.x>ext.z)?0:2):((ext.y>ext.z)?1:2);
                const float split = (AxisVal(cb.lo,axis)+AxisVal(cb.hi,axis))*0.5f;

                int i=start, j=start+count-1;
                while (i<=j){
                    if (AxisVal(Centroid(idx[i]),axis) < split) ++i;
                    else { const int tmp=idx[i]; idx[i]=idx[j]; idx[j]=tmp; --j; }
                }
                int leftCount=i-start;
                if (leftCount==0 || leftCount==count) leftCount=count/2;   // fallback médian
                const int L=Subdivide(start, leftCount);
                const int R=Subdivide(start+leftCount, count-leftCount);
                nodes[nodeIdx].count=0; nodes[nodeIdx].left=L; nodes[nodeIdx].right=R;
                return nodeIdx;
            }

            void Build(const Tri* t, int cnt){
                tris=t; n=cnt; idx.Clear(); nodes.Clear();
                if (cnt<=0) return;
                for (int i=0;i<cnt;++i) idx.PushBack(i);
                Subdivide(0, cnt);
            }

            int Closest(const Ray& r, float tMin, float& tBest) const {
                tBest=1e30f; int hit=-1;
                if (nodes.Empty()) return -1;
                const V3 invD(1.f/r.d.x, 1.f/r.d.y, 1.f/r.d.z);
                int stack[64]; int sp=0; stack[sp++]=0;
                while (sp>0){
                    const BVHNode& nd=nodes[stack[--sp]];
                    if (RayAABB(r.o,invD,nd.lo,nd.hi,tBest)<0.f) continue;
                    if (nd.count>0){
                        for (int i=0;i<nd.count;++i){
                            const int ti=idx[nd.start+i]; float t;
                            if (IntersectMT(r,tris[ti],tMin,t) && t<tBest){ tBest=t; hit=ti; }
                        }
                    } else if (sp<62){ stack[sp++]=nd.left; stack[sp++]=nd.right; }
                }
                return hit;
            }

            bool Any(const Ray& r, float tMin, float maxT) const {
                if (nodes.Empty()) return false;
                const V3 invD(1.f/r.d.x, 1.f/r.d.y, 1.f/r.d.z);
                int stack[64]; int sp=0; stack[sp++]=0;
                while (sp>0){
                    const BVHNode& nd=nodes[stack[--sp]];
                    if (RayAABB(r.o,invD,nd.lo,nd.hi,maxT)<0.f) continue;
                    if (nd.count>0){
                        for (int i=0;i<nd.count;++i){
                            const int ti=idx[nd.start+i]; float t;
                            if (IntersectMT(r,tris[ti],tMin,t) && t<maxT) return true;
                        }
                    } else if (sp<62){ stack[sp++]=nd.left; stack[sp++]=nd.right; }
                }
                return false;
            }
        };

        static inline V3 TriNormal(const Tri& t) {
            return Normalize(Cross(t.v1 - t.v0, t.v2 - t.v0));
        }
        static inline float Clamp01(float v){ return v<0.f?0.f:(v>1.f?1.f:v); }

        // Base orthonormale (t,b) autour de n (Frisvad simplifié)
        static inline void Basis(const V3& n, V3& t, V3& b) {
            if (fabsf(n.x) > fabsf(n.z)) t = Normalize(V3(-n.y, n.x, 0.f));
            else                         t = Normalize(V3(0.f, -n.z, n.y));
            b = Cross(n, t);
        }

        // Softness de l'ombre : rayon angulaire du disque de lumière (tan). 0 = ombre dure.
        // Plus grand ⇒ pénombre plus large. ~0.09 ≈ soleil « doux ».
        // ── Réglages de qualité d'échantillonnage : preset Full (still) ou Live (rapide) ──
        struct Quality {
            int   shadow   = 32;      // rayons d'ombre douce (Monte-Carlo)
            float coneTan  = 0.09f;   // rayon angulaire de la lumière (largeur de pénombre)
            int   ao       = 16;      // rayons d'ambient occlusion
            float aoRadius = 1.6f;    // portée de l'AO
            int   maxDepth = 2;       // rebonds de réflexion
            static Quality Full() { return Quality{}; }
            static Quality Live() { Quality q; q.shadow = 10; q.ao = 8; q.maxDepth = 1; return q; }
        };

        // ── Ambient occlusion Monte-Carlo (hémisphère cosinus, spirale Vogel) ────
        // Assombrit les creux/contacts (rayons bloqués par géométrie proche).
        static inline float AmbientOcclusion(const V3& p, const V3& nrm, const BVH& bvh, const Quality& q) {
            if (q.ao <= 0) return 1.f;
            V3 t, b; Basis(nrm, t, b);
            int occ = 0;
            for (int k = 0; k < q.ao; ++k) {
                const float rr = sqrtf((k + 0.5f) / q.ao);
                const float th = (float)k * 2.3999632f;
                const float dz = sqrtf(1.f - rr * rr);   // pondération cosinus
                const V3 dir = Normalize(t * (rr * cosf(th)) + b * (rr * sinf(th)) + nrm * dz);
                Ray ar; ar.o = p + nrm * 1e-3f; ar.d = dir;
                if (bvh.Any(ar, 1e-3f, q.aoRadius)) ++occ;
            }
            return 1.f - (float)occ / (float)q.ao;
        }

        // ── Shading : Lambert + OMBRES DOUCES (Monte-Carlo, disque de lumière) ────
        // Réf. Scratchapixel : monte-carlo-methods-in-practice/monte-carlo-methods,
        //                      introduction-to-lighting/* (lumières surfaciques).
        static inline V3 Shade(const V3& hitP, const V3& nrm, const V3& albedo,
                               const V3& lightDir, const BVH& bvh, const Quality& q) {
            const V3 toLight = Normalize(lightDir * -1.f);   // direction VERS la lumière
            float ndl = Dot(nrm, toLight);
            if (ndl < 0.f) ndl = 0.f;

            // Visibilité douce : N shadow rays vers un disque angulaire autour de la lumière.
            // Motif fixe en spirale de Vogel (golden angle) → pénombre lisse, sans bruit.
            float shadow = 1.f;
            if (ndl > 0.f && q.shadow > 0) {
                V3 tt, bb; Basis(toLight, tt, bb);
                int occ = 0;
                for (int k = 0; k < q.shadow; ++k) {
                    const float rr = sqrtf((k + 0.5f) / q.shadow);
                    const float th = (float)k * 2.3999632f;     // golden angle
                    const float dx = rr * cosf(th) * q.coneTan;
                    const float dy = rr * sinf(th) * q.coneTan;
                    Ray sr; sr.o = hitP + nrm * 1e-3f;
                    sr.d = Normalize(toLight + tt * dx + bb * dy);
                    if (bvh.Any(sr, 1e-3f, 1e30f)) ++occ;
                }
                shadow = 1.f - (float)occ / (float)q.shadow;
            }
            const float ao = AmbientOcclusion(hitP, nrm, bvh, q);
            const float ambient = 0.28f;
            const float diff = ambient * ao + (1.f - ambient) * ndl * shadow;
            return V3(Clamp01(albedo.x * diff), Clamp01(albedo.y * diff), Clamp01(albedo.z * diff));
        }

        // ── Tracé récursif : diffus + RÉFLEXIONS (Whitted) modulées par Fresnel-Schlick ──
        // Réf. Scratchapixel : introduction-to-ray-tracing/adding-reflection-and-refraction,
        //                      introduction-to-shading/reflection-refraction-fresnel.
        static V3 TraceRay(const Ray& r, const BVH& bvh, const Tri* tris,
                           const V3& lightDir, const V3& sky, int depth, const Quality& q) {
            float t;
            const int hit = bvh.Closest(r, 1e-3f, t);
            if (hit < 0) return sky;

            const V3 hp = r.o + r.d * t;
            V3 nrm = TriNormal(tris[hit]);
            if (Dot(nrm, r.d) > 0.f) nrm = nrm * -1.f;

            V3 col = Shade(hp, nrm, tris[hit].color, lightDir, bvh, q);

            const float refl = tris[hit].reflect;
            if (refl > 0.f && depth < q.maxDepth) {
                float cosI = -Dot(r.d, nrm); if (cosI < 0.f) cosI = 0.f;
                const float fres = refl + (1.f - refl) * powf(1.f - cosI, 5.f);   // Schlick
                Ray rr;
                rr.d = Normalize(r.d - nrm * (2.f * Dot(r.d, nrm)));               // réflexion
                rr.o = hp + nrm * 1e-3f;
                const V3 rc = TraceRay(rr, bvh, tris, lightDir, sky, depth + 1, q);
                col = col * (1.f - fres) + rc * fres;
            }
            return col;
        }

        // ── Rendu plein cadre (pinhole camera) → buffer RGBA8 (row-major, top-down) ──
        // Caméra = matrices view/proj (convention NKRHI). Rayons reconstruits par inverse
        // view-proj (comme Tools/Grid3D). Fonctionne pour perspective ET orthographique.
        static inline void RenderScene(
            uint8* out, int W, int H,
            const math::NkMat4f& view, const math::NkMat4f& proj,
            const Tri* tris, int n, const V3& lightDir, const V3& skyColor,
            const Quality& q = Quality{}, bool bgra = false)   // bgra: backbuffer natif Windows
        {
            const math::NkMat4f invVP = (proj * view).Inverse();  // NDC → monde

            // Construction du BVH (une fois) → toutes les intersections passent par lui.
            BVH bvh; bvh.Build(tris, n);

            // Une ligne d'image = une tâche. Pixels indépendants + BVH read-only → sans verrou.
            auto renderRow = [&](int y) {
                for (int x = 0; x < W; ++x) {
                    const float ndcX = 2.f * ((x + 0.5f) / W) - 1.f;
                    const float ndcY = 1.f - 2.f * ((y + 0.5f) / H);
                    const math::NkVec3f wn = invVP.TransformPoint(math::NkVec3f(ndcX, ndcY, -1.f)); // near
                    const math::NkVec3f wf = invVP.TransformPoint(math::NkVec3f(ndcX, ndcY,  1.f)); // far
                    Ray r;
                    r.o = V3(wn.x, wn.y, wn.z);
                    r.d = Normalize(V3(wf.x - wn.x, wf.y - wn.y, wf.z - wn.z));

                    const V3 c = TraceRay(r, bvh, tris, lightDir, skyColor, 0, q);
                    uint8* o = out + ((usize)y * W + x) * 4u;
                    const uint8 R=(uint8)(Clamp01(c.x)*255.f+.5f), G=(uint8)(Clamp01(c.y)*255.f+.5f), B=(uint8)(Clamp01(c.z)*255.f+.5f);
                    if (bgra) { o[0]=B; o[1]=G; o[2]=R; o[3]=255u; }   // ordre natif GDI
                    else      { o[0]=R; o[1]=G; o[2]=B; o[3]=255u; }
                }
            };

            static const bool s_rtNoMT = [](){ const char* e = std::getenv("NK_SW_RT_NOMT"); return e && e[0]=='1'; }();
            threading::NkThreadPool& pool = threading::NkThreadPool::GetGlobal();
            if (!s_rtNoMT && pool.GetNumWorkers() > 1) {
                const nk_size grain = (nk_size)((uint32)H / (pool.GetNumWorkers() * 4u) + 1u);
                pool.ParallelFor((nk_size)H, [&](nk_size yy){ renderRow((int)yy); }, grain);
                pool.Join();
            } else {
                for (int y = 0; y < H; ++y) renderRow(y);
            }
        }

        // ── Scène de démo BPR (sol réfléchissant + cube + sphère) → nb de triangles ──
        static inline int BuildDemoScene(Tri* tris, int cap) {
            auto pushQuad = [](Tri* t, int& k, int c, V3 a, V3 b, V3 cc, V3 d, V3 col, float refl=0.f){
                if (k+2>c) return; t[k++]={a,b,cc,col,refl}; t[k++]={a,cc,d,col,refl};
            };
            int n = 0;
            const V3 g(0.75f,0.75f,0.78f);
            pushQuad(tris,n,cap, V3(-6,0,-6),V3(6,0,-6),V3(6,0,6),V3(-6,0,6), g, 0.25f);
            const float m=-0.7f,p=0.7f,y0=0.3f,y1=1.7f; const V3 rc(0.85f,0.22f,0.18f);
            pushQuad(tris,n,cap, V3(m,y1,m),V3(p,y1,m),V3(p,y1,p),V3(m,y1,p), rc);
            pushQuad(tris,n,cap, V3(m,y0,p),V3(p,y0,p),V3(p,y0,m),V3(m,y0,m), rc);
            pushQuad(tris,n,cap, V3(m,y0,p),V3(m,y1,p),V3(p,y1,p),V3(p,y0,p), rc);
            pushQuad(tris,n,cap, V3(p,y0,m),V3(p,y1,m),V3(m,y1,m),V3(m,y0,m), rc);
            pushQuad(tris,n,cap, V3(m,y0,m),V3(m,y1,m),V3(m,y1,p),V3(m,y0,p), rc);
            pushQuad(tris,n,cap, V3(p,y0,p),V3(p,y1,p),V3(p,y1,m),V3(p,y0,m), rc);
            const float PI=3.14159265f; const V3 sc(2.3f,1.0f,-0.2f); const float rad=1.0f; const V3 sb(0.20f,0.45f,0.85f);
            const int STK=24,SLI=32;
            auto sp=[&](float th,float ph){ return V3(sc.x+rad*sinf(th)*cosf(ph), sc.y+rad*cosf(th), sc.z+rad*sinf(th)*sinf(ph)); };
            for(int i=0;i<STK;++i){ const float t0=PI*i/STK,t1=PI*(i+1)/STK;
                for(int j=0;j<SLI;++j){ const float p0=2*PI*j/SLI,p1=2*PI*(j+1)/SLI;
                    pushQuad(tris,n,cap, sp(t0,p0),sp(t1,p0),sp(t1,p1),sp(t0,p1), sb, 0.4f); } }
            return n;
        }

        // ── Rendu BPR LIVE dans le backbuffer (ordre natif), caméra orbitale animée ──
        // Utilisé par le device software quand NK_SW_RT=1 : chaque Present ray-trace la scène.
        static inline void RenderLive(uint8* backbuf, int W, int H, uint32 frame, bool bgra) {
            if (!backbuf || W <= 0 || H <= 0) return;
            static Tri tris[4096];
            const int n = BuildDemoScene(tris, 4096);
            const float ang = (float)frame * 0.01f;                 // orbite lente
            const float R = 6.5f;
            const math::NkVec3f eye(sinf(ang)*R + 0.9f, 3.2f, cosf(ang)*R);
            const math::NkMat4f view = math::NkMat4f::LookAt(eye, math::NkVec3f(0.9f,0.8f,-0.1f), math::NkVec3f(0.f,1.f,0.f));
            const math::NkMat4f proj = math::NkMat4f::Perspective(math::NkAngle(45.f), (float)W/(float)H, 0.1f, 100.f);
            const V3 lightDir = Normalize(V3(-0.6f, -1.0f, -0.4f));
            RenderScene(backbuf, W, H, view, proj, tris, n, lightDir, V3(0.55f,0.75f,0.95f), Quality::Live(), bgra);
        }

        // ── Self-test : ray-trace une scène (sol + cube + ombre) → fichier PPM ────
        // Déclenché par NK_SW_RT_TEST=1. Valide la fondation ray-tracing.
        static inline void SelfTest(const char* ppmPath) {
            static Tri tris[4096];
            const int n = BuildDemoScene(tris, 4096);

            const int W = 640, H = 360;
            static uint8 buf[W * H * 4];
            const math::NkVec3f eye(4.2f, 3.2f, 6.2f);
            const math::NkMat4f view = math::NkMat4f::LookAt(eye, math::NkVec3f(0.9f,0.8f,-0.1f), math::NkVec3f(0.f,1.f,0.f));
            const math::NkMat4f proj = math::NkMat4f::Perspective(math::NkAngle(45.f), (float)W/(float)H, 0.1f, 100.f);
            const V3 lightDir = Normalize(V3(-0.6f, -1.0f, -0.4f)); // descendante, vers -x/-z
            RenderScene(buf, W, H, view, proj, tris, n, lightDir, V3(0.55f, 0.75f, 0.95f), Quality::Full(), false);

            // Écriture PPM (P6) — format trivial, test uniquement
            FILE* f = fopen(ppmPath, "wb");
            if (!f) return;
            fprintf(f, "P6\n%d %d\n255\n", W, H);
            for (int i = 0; i < W * H; ++i) fwrite(buf + i*4, 1, 3, f); // RGB
            fclose(f);
        }

    } // namespace swtrace
} // namespace nkentseu
