#pragma once
// =============================================================================
// NkRendererUtils.h
// Utilitaires de rendu : debug draw, gizmos 3D, profiling GPU.
//
// NkDebugDraw    — lignes, sphères, AABB, frustum, grilles en wireframe
// NkGizmo3D      — gizmos d'édition (translation, rotation, scale)
// NkRendererProfiler — mesures GPU per-pass
// NkScreenCapture — capture du framebuffer vers NkImage
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/Command/NkRendererCommand.h"
#include "NKRenderer/3D/NkMeshTypes.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkDebugDraw — rendu debug immédiat (accumulé et vidé chaque frame)
    // =========================================================================
    class NkDebugDraw {
    public:
        explicit NkDebugDraw(NkIRenderer* renderer) : mRenderer(renderer) {}

        // ── Lifecycle ────────────────────────────────────────────────────────
        bool Init();
        void Destroy();

        // ── Accumulateurs (appeler avant Flush) ───────────────────────────────
        void Line       (NkVec3f a, NkVec3f b,       NkVec4f color={1,0,0,1}, bool depthTest=true);
        void Sphere     (NkVec3f c, float32 r,         NkVec4f color={0,1,0,1}, bool depthTest=true);
        void AABB       (NkVec3f mn, NkVec3f mx,       NkVec4f color={0,1,0,1}, bool depthTest=true);
        void OBB        (NkVec3f c, NkVec3f extents, NkMat4f rot, NkVec4f color={0.8f,0.8f,0,1});
        void Cross      (NkVec3f c, float32 size=0.1f, NkVec4f color={1,1,0,1});
        void Axes       (NkMat4f transform, float32 size=1.f);
        void Ray        (NkVec3f origin, NkVec3f dir, float32 length=10.f, NkVec4f c={1,0.5f,0,1});
        void Frustum    (const NkMat4f& invVP, NkVec4f color={0.8f,0.2f,0.8f,1});
        void Grid       (NkVec3f origin, uint32 cells=20, float32 cellSize=1.f, NkVec4f c={0.35f,0.35f,0.35f,0.8f});
        void Circle     (NkVec3f center, NkVec3f normal, float32 radius, NkVec4f c={0,0.8f,1,1}, uint32 segs=32);
        void Arrow      (NkVec3f from, NkVec3f to, NkVec4f c={1,0.8f,0,1}, float32 headSize=0.1f);
        void Text3D     (NkVec3f pos, const char* text, NkVec4f c={1,1,1,1});

        // ── Flush vers NkRendererCommand ─────────────────────────────────────
        void Flush(NkRendererCommand& cmd);

        // ── Reset (appelé en début de frame) ─────────────────────────────────
        void BeginFrame() {
            mLines.Clear();
            mText3D.Clear();
        }

        bool IsValid() const { return mValid; }

    private:
        struct DebugLine  { NkVec3f a, b; NkVec4f color; bool depthTest; };
        struct DebugText3D{ NkVec3f pos; char text[128]; NkVec4f color; };

        NkIRenderer*           mRenderer = nullptr;
        NkVector<DebugLine>    mLines;
        NkVector<DebugText3D>  mText3D;
        bool                   mValid = false;
    };

    inline bool NkDebugDraw::Init()    { mValid=true; return true; }
    inline void NkDebugDraw::Destroy() { mLines.Clear(); mText3D.Clear(); mValid=false; }

    inline void NkDebugDraw::Line(NkVec3f a, NkVec3f b, NkVec4f c, bool dt) {
        mLines.PushBack({a,b,c,dt});
    }
    inline void NkDebugDraw::Sphere(NkVec3f center, float32 r, NkVec4f c, bool dt) {
        const int seg=24; const float32 pi=3.14159f;
        for (int p=0;p<3;++p) {
            NkVec3f prev={};
            for (int i=0;i<=seg;++i) {
                const float32 a=2.f*pi*(float32)i/seg;
                NkVec3f pt={};
                if(p==0) pt={center.x+cosf(a)*r,center.y+sinf(a)*r,center.z};
                else if(p==1) pt={center.x+cosf(a)*r,center.y,center.z+sinf(a)*r};
                else pt={center.x,center.y+cosf(a)*r,center.z+sinf(a)*r};
                if(i>0) mLines.PushBack({prev,pt,c,dt});
                prev=pt;
            }
        }
    }
    inline void NkDebugDraw::AABB(NkVec3f mn, NkVec3f mx, NkVec4f c, bool dt) {
        NkVec3f v[8]={
            {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},
            {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
        };
        int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for(auto& e:edges) mLines.PushBack({v[e[0]],v[e[1]],c,dt});
    }
    inline void NkDebugDraw::Cross(NkVec3f c, float32 size, NkVec4f col) {
        const float32 h=size*0.5f;
        Line({c.x-h,c.y,c.z},{c.x+h,c.y,c.z},col);
        Line({c.x,c.y-h,c.z},{c.x,c.y+h,c.z},col);
        Line({c.x,c.y,c.z-h},{c.x,c.y,c.z+h},col);
    }
    inline void NkDebugDraw::Axes(NkMat4f t, float32 size) {
        auto pt=[&](float32 x,float32 y,float32 z)->NkVec3f{
            NkVec4f p=t*NkVec4f{x,y,z,1};return{p.x,p.y,p.z};};
        NkVec3f o=pt(0,0,0);
        mLines.PushBack({o,pt(size,0,0),{1,0,0,1},true});
        mLines.PushBack({o,pt(0,size,0),{0,1,0,1},true});
        mLines.PushBack({o,pt(0,0,size),{0,0,1,1},true});
    }
    inline void NkDebugDraw::Ray(NkVec3f o, NkVec3f d, float32 l, NkVec4f c) {
        Line(o,{o.x+d.x*l,o.y+d.y*l,o.z+d.z*l},c);
    }
    inline void NkDebugDraw::Grid(NkVec3f origin, uint32 cells, float32 cs, NkVec4f c) {
        const float32 half=(float32)cells*cs*0.5f;
        for(uint32 i=0;i<=cells;++i){
            const float32 t=-half+(float32)i*cs;
            Line({origin.x+t,origin.y,origin.z-half},{origin.x+t,origin.y,origin.z+half},c);
            Line({origin.x-half,origin.y,origin.z+t},{origin.x+half,origin.y,origin.z+t},c);
        }
    }
    inline void NkDebugDraw::Circle(NkVec3f center, NkVec3f /*normal*/, float32 r, NkVec4f c, uint32 segs) {
        const float32 pi=3.14159f;
        NkVec3f prev={center.x+r,center.y,center.z};
        for(uint32 i=1;i<=segs;++i){
            const float32 a=2.f*pi*(float32)i/segs;
            NkVec3f pt={center.x+cosf(a)*r,center.y,center.z+sinf(a)*r};
            mLines.PushBack({prev,pt,c,true}); prev=pt;
        }
    }
    inline void NkDebugDraw::Arrow(NkVec3f from, NkVec3f to, NkVec4f c, float32 hs) {
        Line(from,to,c);
        NkVec3f dir={to.x-from.x,to.y-from.y,to.z-from.z};
        const float32 l=sqrtf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
        if(l<1e-6f) return;
        dir.x/=l; dir.y/=l; dir.z/=l;
        NkVec3f base={to.x-dir.x*hs,to.y-dir.y*hs,to.z-dir.z*hs};
        // Perpendiculaire approx
        NkVec3f perp={-dir.z,0,dir.x};
        const float32 pl=sqrtf(perp.x*perp.x+perp.z*perp.z);
        if(pl>1e-6f){perp.x/=pl;perp.z/=pl;}
        perp.x*=hs*0.4f; perp.z*=hs*0.4f;
        Line({base.x+perp.x,base.y+perp.y,base.z+perp.z},to,c);
        Line({base.x-perp.x,base.y-perp.y,base.z-perp.z},to,c);
    }
    inline void NkDebugDraw::Frustum(const NkMat4f& invVP, NkVec4f c) {
        const float32 zn=-1.f, zf=1.f;
        NkVec3f pts[8];
        int idx=0;
        for(int x=-1;x<=1;x+=2) for(int y=-1;y<=1;y+=2) for(float32 z:{zn,zf}){
            NkVec4f p=invVP*NkVec4f{(float32)x,(float32)y,z,1}; pts[idx++]={p.x/p.w,p.y/p.w,p.z/p.w};
        }
        int edges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
        for(auto& e:edges) mLines.PushBack({pts[e[0]],pts[e[1]],c,false});
    }
    inline void NkDebugDraw::Text3D(NkVec3f pos, const char* text, NkVec4f c) {
        DebugText3D t{}; t.pos=pos; t.color=c;
        strncpy(t.text,text,sizeof(t.text)-1);
        mText3D.PushBack(t);
    }
    inline void NkDebugDraw::Flush(NkRendererCommand& cmd) {
        for(usize i=0;i<mLines.Size();++i) {
            const DebugLine& l=mLines[i];
            cmd.DrawDebugLine(l.a,l.b,l.color,1.f,l.depthTest);
        }
    }

    // =========================================================================
    // NkRendererProfiler — mesures de temps GPU par pass
    // =========================================================================
    class NkRendererProfiler {
    public:
        struct PassStats {
            char    name[64] = {};
            float32 gpuMs    = 0.f;
            float32 cpuMs    = 0.f;
            uint32  drawCalls= 0;
            uint32  triangles= 0;
        };

        void BeginPass(const char* name) {
            PassStats s{}; strncpy(s.name, name, sizeof(s.name)-1);
            mCurrentPass = (uint32)mPasses.Size();
            mPasses.PushBack(s);
        }

        void EndPass(uint32 drawCalls=0, uint32 triangles=0) {
            if (mCurrentPass < mPasses.Size()) {
                mPasses[mCurrentPass].drawCalls = drawCalls;
                mPasses[mCurrentPass].triangles = triangles;
            }
        }

        void BeginFrame() { mPasses.Clear(); }
        void EndFrame()   {}  // en vrai : récupérer les queries GPU ici

        const NkVector<PassStats>& GetPasses() const { return mPasses; }

        float32 GetTotalGPUMs() const {
            float32 total=0.f;
            for(usize i=0;i<mPasses.Size();++i) total+=mPasses[i].gpuMs;
            return total;
        }

    private:
        NkVector<PassStats> mPasses;
        uint32              mCurrentPass = 0;
    };

    // =========================================================================
    // NkScreenCapture — capture du framebuffer vers NkImage
    // =========================================================================
    class NkScreenCapture {
    public:
        explicit NkScreenCapture(NkIRenderer* renderer) : mRenderer(renderer) {}

        // Capture asynchrone (résultat disponible au frame suivant)
        bool RequestCapture(NkRenderTargetHandle source = NkRenderTargetHandle::Null()) {
            mPending = true;
            mSource  = source;
            return true;
        }

        // Tenter de récupérer la capture (retourne nullptr si pas encore prête)
        NkImage* GetResult() {
            if (!mPending || !mResultReady) return nullptr;
            mPending = mResultReady = false;
            return mResult;  // l'appelant prend possession
        }

        // Appeler en fin de frame
        void Update() {
            if (!mPending) return;
            // Dans une vraie impl : initier le readback GPU et attendre
            mResultReady = false;
        }

        bool HasPendingCapture() const { return mPending; }

    private:
        NkIRenderer*          mRenderer   = nullptr;
        NkRenderTargetHandle  mSource;
        NkImage*              mResult     = nullptr;
        bool                  mPending    = false;
        bool                  mResultReady= false;
    };

} // namespace renderer
} // namespace nkentseu