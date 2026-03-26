/**
 * @File    NkPAMesh.cpp
 * @Brief   Implémentation de MeshBuilder — triangle fans, quads, nageoires.
 */

#include "Renderer/NkPAMesh.h"
#include <cmath>

namespace nkentseu {
namespace nkpa {

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::Clear() {
    mPixels.Clear();
}

bool  MeshBuilder::IsEmpty()      const { return mPixels.IsEmpty(); }
int32 MeshBuilder::VertexCount()  const { return (int32)mPixels.Size(); }

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::AddTriangle(NkVector2f a, float32 ra, float32 ga, float32 ba, float32 aa,
                               NkVector2f b, float32 rb, float32 gb, float32 bb, float32 ab,
                               NkVector2f c, float32 rc, float32 gc, float32 bc, float32 ac) {
    mPixels.PushBack({a.x, a.y, 0.5f, ra, ga, ba, aa});
    mPixels.PushBack({b.x, b.y, 0.5f, rb, gb, bb, ab});
    mPixels.PushBack({c.x, c.y, 0.5f, rc, gc, bc, ac});
}

void MeshBuilder::AddTriFlat(NkVector2f a, NkVector2f b, NkVector2f c,
                              float32 r, float32 g, float32 bl, float32 al) {
    AddTriangle(a, r,g,bl,al,  b, r,g,bl,al,  c, r,g,bl,al);
}

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::AddCircle(NkVector2f center, float32 radius,
                             float32 r, float32 g, float32 b, float32 a,
                             int32 segs) {
    for (int32 i = 0; i < segs; ++i) {
        float32 t0 = PA_TAU * (float32)i       / (float32)segs;
        float32 t1 = PA_TAU * (float32)(i + 1) / (float32)segs;
        NkVector2f p0 = { center.x + ::cosf(t0)*radius, center.y + ::sinf(t0)*radius };
        NkVector2f p1 = { center.x + ::cosf(t1)*radius, center.y + ::sinf(t1)*radius };
        AddTriFlat(center, p0, p1, r, g, b, a);
    }
}

void MeshBuilder::AddEllipse(NkVector2f center, float32 rx, float32 ry, float32 angle,
                              float32 r, float32 g, float32 b, float32 a, int32 segs) {
    float32 ca = ::cosf(angle), sa = ::sinf(angle);
    for (int32 i = 0; i < segs; ++i) {
        float32 t0 = PA_TAU * (float32)i       / (float32)segs;
        float32 t1 = PA_TAU * (float32)(i + 1) / (float32)segs;
        float32 lx0 = ::cosf(t0)*rx, ly0 = ::sinf(t0)*ry;
        float32 lx1 = ::cosf(t1)*rx, ly1 = ::sinf(t1)*ry;
        NkVector2f p0 = { center.x + lx0*ca - ly0*sa, center.y + lx0*sa + ly0*ca };
        NkVector2f p1 = { center.x + lx1*ca - ly1*sa, center.y + lx1*sa + ly1*ca };
        AddTriFlat(center, p0, p1, r, g, b, a);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::AddQuad(NkVector2f a, float32 ra_,
                           NkVector2f b, float32 rb_,
                           float32 r, float32 g, float32 bl, float32 al) {
    NkVector2f d = (b - a).Normalized().Normal();
    NkVector2f a0 = a + d * ra_, a1 = a - d * ra_;
    NkVector2f b0 = b + d * rb_, b1 = b - d * rb_;
    AddTriFlat(a0, a1, b0, r, g, bl, al);
    AddTriFlat(a1, b1, b0, r, g, bl, al);
}

void MeshBuilder::AddQuadGrad(NkVector2f a, float32 ra_,
                               float32 r0, float32 g0, float32 b0_, float32 a0_,
                               NkVector2f b, float32 rb_,
                               float32 r1, float32 g1, float32 b1_, float32 a1_) {
    NkVector2f d  = (b - a).Normalized().Normal();
    NkVector2f al0 = a + d * ra_, al1 = a - d * ra_;
    NkVector2f bl0 = b + d * rb_, bl1 = b - d * rb_;
    AddTriangle(al0, r0,g0,b0_,a0_,  al1, r0,g0,b0_,a0_,  bl0, r1,g1,b1_,a1_);
    AddTriangle(al1, r0,g0,b0_,a0_,  bl1, r1,g1,b1_,a1_,  bl0, r1,g1,b1_,a1_);
}

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::AddFin(NkVector2f base0, NkVector2f base1, NkVector2f tip,
                          float32 r0, float32 g0, float32 b0, float32 a0,
                          float32 rt, float32 gt, float32 bt, float32 at) {
    AddTriangle(base0, r0,g0,b0,a0,  base1, r0,g0,b0,a0,  tip, rt,gt,bt,at);
}

// ─────────────────────────────────────────────────────────────────────────────

void MeshBuilder::AddBackground(float32 w, float32 h,
                                 float32 tlR, float32 tlG, float32 tlB,
                                 float32 brR, float32 brG, float32 brB) {
    // Deux triangles couvrant tout l'écran avec dégradé coin TL→BR
    AddTriangle({0,0},   tlR,tlG,tlB,1.f,
                {w,0},   (tlR+brR)*0.5f,(tlG+brG)*0.5f,(tlB+brB)*0.5f,1.f,
                {0,h},   (tlR+brR)*0.5f,(tlG+brG)*0.5f,(tlB+brB)*0.5f,1.f);
    AddTriangle({w,0},   (tlR+brR)*0.5f,(tlG+brG)*0.5f,(tlB+brB)*0.5f,1.f,
                {w,h},   brR,brG,brB,1.f,
                {0,h},   (tlR+brR)*0.5f,(tlG+brG)*0.5f,(tlB+brB)*0.5f,1.f);
}

// ─────────────────────────────────────────────────────────────────────────────

NkVector<PaVertex> MeshBuilder::BuildNDC(float32 screenW, float32 screenH) const {
    NkVector<PaVertex> out;
    out.Reserve(mPixels.Size());
    for (int32 i = 0; i < (int32)mPixels.Size(); ++i) {
        const PaVertex& v = mPixels[i];
        // Conversion pixels → NDC : x ∈ [-1,1], y inversé (pixels y-down)
        float32 nx = (v.x / screenW) * 2.0f - 1.0f;
        float32 ny = 1.0f - (v.y / screenH) * 2.0f;
        out.PushBack({ nx, ny, v.z, v.r, v.g, v.b, v.a });
    }
    return out;
}

} // namespace nkpa
} // namespace nkentseu
