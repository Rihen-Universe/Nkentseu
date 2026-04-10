#pragma once
// =============================================================================
// NkCamera.h  —  Caméras 2D et 3D (pures CPU, pas de ressource GPU)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"

namespace nkentseu {
namespace render {

// =============================================================================
// NkCamera3D
// =============================================================================
enum class NkProjType : uint32 { NK_PERSPECTIVE, NK_ORTHOGRAPHIC };

class NkCamera3D {
public:
    // ── Configuration ─────────────────────────────────────────────────────────
    NkProjType type         = NkProjType::NK_PERSPECTIVE;
    NkVec3f    position     = {0,0,5};
    NkVec3f    target       = {0,0,0};
    NkVec3f    up           = {0,1,0};

    // Perspective
    float32    fovDeg       = 60.f;
    float32    aspect       = 16.f/9.f;
    float32    nearZ        = 0.1f;
    float32    farZ         = 1000.f;

    // Orthographic
    float32    orthoLeft    = -5.f;
    float32    orthoRight   =  5.f;
    float32    orthoBottom  = -5.f;
    float32    orthoTop     =  5.f;
    float32    orthoNear    = -100.f;
    float32    orthoFar     =  100.f;

    // NDC convention : false=OpenGL[-1,1], true=Vulkan/DX/Metal[0,1]
    bool       depthZeroToOne = false;

    // ── Matrices ──────────────────────────────────────────────────────────────
    NkMat4f View() const {
        return NkMat4f::LookAt(position, target, up);
    }

    NkMat4f Projection() const {
        if (type == NkProjType::NK_PERSPECTIVE) {
            return NkMat4f::Perspective(NkAngle(fovDeg), aspect, nearZ, farZ);
        }
        return NkMat4f::Orthogonal(
            NkVec2f(orthoLeft,  orthoBottom),
            NkVec2f(orthoRight, orthoTop),
            orthoNear, orthoFar, depthZeroToOne);
    }

    NkMat4f ViewProjection() const { return Projection() * View(); }

    // ── Helpers ───────────────────────────────────────────────────────────────
    NkVec3f Forward() const {
        NkVec3f d = target - position;
        float32 l = NkSqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        return l>1e-6f ? NkVec3f{d.x/l,d.y/l,d.z/l} : NkVec3f{0,0,-1};
    }
    NkVec3f Right() const {
        NkVec3f f=Forward();
        NkVec3f r{f.y*up.z-f.z*up.y, f.z*up.x-f.x*up.z, f.x*up.y-f.y*up.x};
        float32 l=NkSqrt(r.x*r.x+r.y*r.y+r.z*r.z);
        return l>1e-6f?NkVec3f{r.x/l,r.y/l,r.z/l}:NkVec3f{1,0,0};
    }

    void SetAspect(uint32 w, uint32 h) {
        aspect = (h>0)?(float32)w/(float32)h:1.f;
    }

    // Orbite autour d'un point central
    void Orbit(const NkVec3f& center, float32 yawDeg, float32 pitchDeg, float32 dist) {
        pitchDeg = NkClamp(pitchDeg,-89.f,89.f);
        const float32 y=NkToRadians(yawDeg), p=NkToRadians(pitchDeg);
        position.x = center.x + dist*NkCos(p)*NkSin(y);
        position.y = center.y + dist*NkSin(p);
        position.z = center.z + dist*NkCos(p)*NkCos(y);
        target = center;
        up = NkFabs(NkSin(p))>0.98f
             ? NkVec3f{NkCos(y),0,NkSin(y)}
             : NkVec3f{0,1,0};
    }

    // Pan dans le plan de la caméra
    void Pan(float32 dx, float32 dy) {
        NkVec3f r = Right();
        NkVec3f u = up;
        position.x += r.x*dx + u.x*dy;
        position.y += r.y*dx + u.y*dy;
        position.z += r.z*dx + u.z*dy;
        target.x   += r.x*dx + u.x*dy;
        target.y   += r.y*dx + u.y*dy;
        target.z   += r.z*dx + u.z*dy;
    }

    // Zoom (avance vers la cible)
    void Zoom(float32 delta) {
        NkVec3f f=Forward();
        position.x+=f.x*delta; position.y+=f.y*delta; position.z+=f.z*delta;
    }

    // ── Factory ───────────────────────────────────────────────────────────────
    static NkCamera3D Persp(float32 fov,float32 aspect,float32 nr,float32 fr,bool z01=false) {
        NkCamera3D c; c.type=NkProjType::NK_PERSPECTIVE;
        c.fovDeg=fov; c.aspect=aspect; c.nearZ=nr; c.farZ=fr; c.depthZeroToOne=z01;
        return c;
    }
    static NkCamera3D Ortho(float32 l,float32 r,float32 b,float32 t,
                             float32 nr=-100.f,float32 fr=100.f,bool z01=false) {
        NkCamera3D c; c.type=NkProjType::NK_ORTHOGRAPHIC;
        c.orthoLeft=l; c.orthoRight=r; c.orthoBottom=b; c.orthoTop=t;
        c.orthoNear=nr; c.orthoFar=fr; c.depthZeroToOne=z01;
        return c;
    }
    // Caméra lumière pour shadow map orthographique
    static NkCamera3D ShadowLight(const NkVec3f& dir, const NkVec3f& center,
                                   float32 radius=10.f, bool z01=false) {
        NkVec3f nd=dir;
        float32 l=NkSqrt(nd.x*nd.x+nd.y*nd.y+nd.z*nd.z);
        if(l>1e-6f){nd.x/=l;nd.y/=l;nd.z/=l;}
        NkCamera3D c = Ortho(-radius,radius,-radius,radius,1.f,radius*3.f,z01);
        c.position = {center.x-nd.x*radius*2,center.y-nd.y*radius*2,center.z-nd.z*radius*2};
        c.target   = center;
        c.up = NkFabs(nd.y)>.9f ? NkVec3f{1,0,0} : NkVec3f{0,1,0};
        return c;
    }

    // Remplit un NkSceneUBO
    void FillSceneUBO(NkSceneUBO& ubo, const NkTransform3D& modelTr,
                       const NkVec3f& lightDir,
                       const NkMat4f& lightVP={},
                       float32 time=0.f, float32 dt=0.f) const {
        const NkMat4f mm=modelTr.ToMatrix(), vm=View(), pm=Projection();
        memcpy(ubo.model,   mm.data, 64);
        memcpy(ubo.view,    vm.data, 64);
        memcpy(ubo.proj,    pm.data, 64);
        memcpy(ubo.lightVP, lightVP.data, 64);
        ubo.lightDir[0]=lightDir.x; ubo.lightDir[1]=lightDir.y;
        ubo.lightDir[2]=lightDir.z; ubo.lightDir[3]=1.f;
        ubo.eyePos[0]=position.x; ubo.eyePos[1]=position.y;
        ubo.eyePos[2]=position.z; ubo.eyePos[3]=0.f;
        ubo.time=time; ubo.dt=dt;
        ubo.ndcZScale =depthZeroToOne?1.f:0.5f;
        ubo.ndcZOffset=depthZeroToOne?0.f:0.5f;
    }

    // Ray depuis NDC
    struct Ray { NkVec3f origin; NkVec3f direction; };
    Ray ScreenRay(float32 ndcX, float32 ndcY) const {
        // Approximation rapide via matrice inverse
        // ndcX, ndcY in [-1,1]
        Ray r; r.origin = position;
        NkVec3f f=Forward(), ri=Right();
        float32 fovR=NkToRadians(fovDeg)*.5f;
        float32 tanH=NkTan(fovR);
        r.direction.x = f.x + ri.x*ndcX*tanH*aspect + up.x*(-ndcY)*tanH;
        r.direction.y = f.y + ri.y*ndcX*tanH*aspect + up.y*(-ndcY)*tanH;
        r.direction.z = f.z + ri.z*ndcX*tanH*aspect + up.z*(-ndcY)*tanH;
        float32 l=NkSqrt(r.direction.x*r.direction.x+r.direction.y*r.direction.y+r.direction.z*r.direction.z);
        if(l>1e-6f){r.direction.x/=l;r.direction.y/=l;r.direction.z/=l;}
        return r;
    }
};

// =============================================================================
// NkCamera2D
// =============================================================================
class NkCamera2D {
public:
    NkVec2f position = {0,0};
    float32 rotation = 0.f;      // degrees
    float32 zoom     = 1.f;
    float32 width    = 1280.f;
    float32 height   = 720.f;
    bool    yFlip    = true;     // true = Y vers le bas (pixel convention)

    NkMat4f View() const {
        NkMat4f T = NkMat4f::Translation({-position.x,-position.y,0});
        NkMat4f R = NkMat4f::RotationZ(NkAngle(-rotation));
        return R * T;
    }

    NkMat4f Projection() const {
        const float32 hw = (width *.5f)/zoom;
        const float32 hh = (height*.5f)/zoom;
        return NkMat4f::Orthogonal(
            NkVec2f(-hw, yFlip? -hh : hh),
            NkVec2f( hw, yFlip?  hh :-hh),
            -1.f, 1.f, false);
    }

    NkMat4f ViewProjection() const { return Projection() * View(); }

    NkVec2f ScreenToWorld(float32 sx, float32 sy) const {
        const float32 ndcX=(sx/(width *.5f)-1.f)/zoom;
        const float32 ndcY=yFlip?(1.f-sy/(height*.5f))/zoom:(sy/(height*.5f)-1.f)/zoom;
        return {position.x+ndcX*(width*.5f), position.y+ndcY*(height*.5f)};
    }
    NkVec2f WorldToScreen(float32 wx, float32 wy) const {
        const float32 rx=(wx-position.x)*zoom;
        const float32 ry=(wy-position.y)*zoom;
        const float32 sx=(rx/(width*.5f)+1.f)*(width*.5f);
        const float32 sy=yFlip?(1.f-ry/(height*.5f))*(height*.5f):(ry/(height*.5f)+1.f)*(height*.5f);
        return {sx,sy};
    }

    void Pan(float32 dx, float32 dy) { position.x+=dx; position.y+=dy; }
    void SetCenter(float32 x,float32 y){ position={x,y}; }
    void SetSize(uint32 w, uint32 h) { width=(float32)w; height=(float32)h; }
    void SetZoom(float32 z){ zoom=NkMax(0.01f,z); }
    void ZoomToFit(const NkRectF& r) {
        position={r.x+r.w*.5f, r.y+r.h*.5f};
        zoom=NkMin(width/r.w, height/r.h);
    }

    // Pixel-space : 1 world unit = 1 pixel, origine = haut-gauche
    static NkCamera2D PixelSpace(float32 w, float32 h) {
        NkCamera2D c; c.width=w; c.height=h; c.zoom=1.f;
        c.position={w*.5f, h*.5f}; return c;
    }
};

} // namespace render
} // namespace nkentseu