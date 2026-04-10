// =============================================================================
// NkRendererCommand.cpp
// =============================================================================
#include "NkRendererCommand.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
namespace renderer {

    void NkRendererCommand::Reset() {
        mCalls2D.Clear();
        mCalls3D.Clear();
        mSpriteCalls.Clear();
        mPrim2D.Clear();
        mLights.Clear();
        mTextCalls.Clear();
        mDebugLines.Clear();
        mDebugAABBs.Clear();
        mDebugSpheres.Clear();
        mSorted = false;
    }

    // ── 2D ───────────────────────────────────────────────────────────────────

    void NkRendererCommand::Draw2D(const NkDrawCall2D& call) {
        mCalls2D.PushBack(call);
        mSorted = false;
    }

    void NkRendererCommand::DrawSprite(NkTextureHandle tex,
                                       NkVec2f position, NkVec2f size,
                                       NkVec4f color, float32 rotation,
                                       float32 depth, uint32 layer)
    {
        SpriteCall s{};
        s.tex      = tex;
        s.position = position;
        s.size     = size;
        s.color    = color;
        s.rotation = rotation;
        s.depth    = depth;
        s.layer    = layer;
        mSpriteCalls.PushBack(s);
        mSorted = false;
    }

    void NkRendererCommand::DrawRect2D(NkVec2f position, NkVec2f size,
                                       NkVec4f color, float32 depth, uint32 layer)
    {
        Prim2DCall p{};
        p.kind    = Prim2DCall::Kind::RectFill;
        p.p0      = position;
        p.p1      = size;
        p.color   = color;
        p.depth   = depth;
        p.layer   = layer;
        mPrim2D.PushBack(p);
        mSorted = false;
    }

    void NkRendererCommand::DrawRectOutline2D(NkVec2f position, NkVec2f size,
                                              NkVec4f color, float32 thickness,
                                              float32 depth, uint32 layer)
    {
        Prim2DCall p{};
        p.kind    = Prim2DCall::Kind::RectOutline;
        p.p0      = position;
        p.p1      = size;
        p.color   = color;
        p.extra   = thickness;
        p.depth   = depth;
        p.layer   = layer;
        mPrim2D.PushBack(p);
        mSorted = false;
    }

    void NkRendererCommand::DrawCircle2D(NkVec2f center, float32 radius,
                                         NkVec4f color, uint32 segments,
                                         float32 depth, uint32 layer)
    {
        Prim2DCall p{};
        p.kind     = Prim2DCall::Kind::Circle;
        p.p0       = center;
        p.extra    = radius;
        p.color    = color;
        p.segments = segments;
        p.depth    = depth;
        p.layer    = layer;
        mPrim2D.PushBack(p);
        mSorted = false;
    }

    void NkRendererCommand::DrawLine2D(NkVec2f start, NkVec2f end,
                                       NkVec4f color, float32 thickness,
                                       float32 depth, uint32 layer)
    {
        Prim2DCall p{};
        p.kind    = Prim2DCall::Kind::Line;
        p.p0      = start;
        p.p1      = end;
        p.color   = color;
        p.extra   = thickness;
        p.depth   = depth;
        p.layer   = layer;
        mPrim2D.PushBack(p);
        mSorted = false;
    }

    // ── 3D ───────────────────────────────────────────────────────────────────

    void NkRendererCommand::Draw3D(const NkDrawCall3D& call) {
        mCalls3D.PushBack(call);
        mSorted = false;
    }

    void NkRendererCommand::Draw3D(NkMeshHandle mesh, NkMaterialInstHandle mat,
                                   const NkMat4f& transform,
                                   bool castShadow, bool receiveShadow,
                                   uint32 layer)
    {
        NkDrawCall3D c{};
        c.mesh          = mesh;
        c.material      = mat;
        c.transform     = transform;
        c.castShadow    = castShadow ? 1 : 0;
        c.receiveShadow = receiveShadow ? 1 : 0;
        c.layer         = (uint32)layer & 0xFF;
        Draw3D(c);
    }

    // ── Lumières ─────────────────────────────────────────────────────────────

    void NkRendererCommand::AddLight(const NkLightDesc& light) {
        mLights.PushBack(light);
    }

    void NkRendererCommand::AddDirectionalLight(NkVec3f direction, NkVec3f color,
                                                float32 intensity, bool castShadow)
    {
        NkLightDesc l{};
        l.type       = NkLightType::Directional;
        l.direction  = direction;
        l.color      = color;
        l.intensity  = intensity;
        l.castShadow = castShadow;
        mLights.PushBack(l);
    }

    void NkRendererCommand::AddPointLight(NkVec3f position, NkVec3f color,
                                          float32 intensity, float32 radius,
                                          bool castShadow)
    {
        NkLightDesc l{};
        l.type       = NkLightType::Point;
        l.position   = position;
        l.color      = color;
        l.intensity  = intensity;
        l.radius     = radius;
        l.castShadow = castShadow;
        mLights.PushBack(l);
    }

    // ── Texte ─────────────────────────────────────────────────────────────────

    void NkRendererCommand::DrawText(const NkTextDesc& desc) {
        mTextCalls.PushBack(desc);
    }

    void NkRendererCommand::DrawText(NkFontHandle font, const char* text,
                                     NkVec2f position, float32 size,
                                     NkVec4f color, float32 maxWidth, uint32 /*layer*/)
    {
        NkTextDesc d{};
        d.font     = font;
        d.text     = NkString(text ? text : "");
        d.position = position;
        d.size     = size;
        d.color    = color;
        d.maxWidth = maxWidth;
        mTextCalls.PushBack(d);
    }

    // ── Debug / Gizmos ────────────────────────────────────────────────────────

    void NkRendererCommand::DrawDebugLine(const NkDebugLine& line)
    { mDebugLines.PushBack(line); }

    void NkRendererCommand::DrawDebugLine(NkVec3f start, NkVec3f end,
                                          NkVec4f color, float32 thickness, bool depthTest)
    {
        NkDebugLine l{};
        l.start     = start;
        l.end       = end;
        l.color     = color;
        l.thickness = thickness;
        l.depthTest = depthTest;
        mDebugLines.PushBack(l);
    }

    void NkRendererCommand::DrawDebugAABB(const NkDebugAABB& aabb)
    { mDebugAABBs.PushBack(aabb); }

    void NkRendererCommand::DrawDebugAABB(NkVec3f min, NkVec3f max,
                                          NkVec4f color, bool depthTest)
    {
        NkDebugAABB a{};
        a.min       = min;
        a.max       = max;
        a.color     = color;
        a.depthTest = depthTest;
        mDebugAABBs.PushBack(a);
    }

    void NkRendererCommand::DrawDebugSphere(const NkDebugSphere& sphere)
    { mDebugSpheres.PushBack(sphere); }

    void NkRendererCommand::DrawDebugSphere(NkVec3f center, float32 radius,
                                            NkVec4f color, bool depthTest)
    {
        NkDebugSphere s{};
        s.center    = center;
        s.radius    = radius;
        s.color     = color;
        s.depthTest = depthTest;
        mDebugSpheres.PushBack(s);
    }

    void NkRendererCommand::DrawDebugCross(NkVec3f position, float32 size,
                                           NkVec4f color, bool depthTest)
    {
        const float32 h = size * 0.5f;
        DrawDebugLine({position.x-h, position.y, position.z},
                      {position.x+h, position.y, position.z}, color, 1.f, depthTest);
        DrawDebugLine({position.x, position.y-h, position.z},
                      {position.x, position.y+h, position.z}, color, 1.f, depthTest);
        DrawDebugLine({position.x, position.y, position.z-h},
                      {position.x, position.y, position.z+h}, color, 1.f, depthTest);
    }

    void NkRendererCommand::DrawDebugGrid(NkVec3f origin, NkVec3f /*normalAxis*/,
                                          uint32 cells, float32 cellSize, NkVec4f color)
    {
        const float32 half = cells * cellSize * 0.5f;
        for (uint32 i = 0; i <= cells; ++i) {
            const float32 t = -half + i * cellSize;
            // Lignes parallèles à Z
            DrawDebugLine({origin.x + t, origin.y, origin.z - half},
                          {origin.x + t, origin.y, origin.z + half}, color);
            // Lignes parallèles à X
            DrawDebugLine({origin.x - half, origin.y, origin.z + t},
                          {origin.x + half, origin.y, origin.z + t}, color);
        }
    }

    // ── Tri ──────────────────────────────────────────────────────────────────

    uint64 NkRendererCommand::SortKey2D(const NkDrawCall2D& c) {
        // [layer 8b][material.id 24b][depth_quantized 32b]
        const uint64 lay  = (uint64)(c.layer & 0xFF)           << 56;
        const uint64 mat  = (uint64)(c.material.id & 0xFFFFFF) << 32;
        // Convertir depth float en uint32 pour tri stable (0 = devant)
        const uint32 d    = (uint32)((1.f - c.depth) * (float32)0xFFFFFFFFu);
        return lay | mat | (uint64)d;
    }

    uint64 NkRendererCommand::SortKey3D(const NkDrawCall3D& c, bool /*transparent*/) {
        // [layer 8b][castShadow 1b][mat.id 23b][depth 32b]
        const uint64 lay   = (uint64)(c.layer & 0xFF) << 56;
        const uint64 mat   = (uint64)(c.material.id & 0x7FFFFF) << 32;
        return lay | mat;
    }

    void NkRendererCommand::Sort() {
        if (mSorted) return;

        // Tri 2D par layer puis depth
        std::stable_sort(mCalls2D.Begin(), mCalls2D.End(),
            [](const NkDrawCall2D& a, const NkDrawCall2D& b) {
                return SortKey2D(a) < SortKey2D(b);
            });

        // Tri sprites
        std::stable_sort(mSpriteCalls.Begin(), mSpriteCalls.End(),
            [](const SpriteCall& a, const SpriteCall& b) {
                if (a.layer != b.layer) return a.layer < b.layer;
                return a.depth < b.depth;
            });

        // Tri 3D : opaques front-to-back (perf), transparents back-to-front
        std::stable_sort(mCalls3D.Begin(), mCalls3D.End(),
            [](const NkDrawCall3D& a, const NkDrawCall3D& b) {
                return SortKey3D(a, false) < SortKey3D(b, false);
            });

        // Tri primitives 2D
        std::stable_sort(mPrim2D.Begin(), mPrim2D.End(),
            [](const Prim2DCall& a, const Prim2DCall& b) {
                if (a.layer != b.layer) return a.layer < b.layer;
                return a.depth < b.depth;
            });

        mSorted = true;
    }

} // namespace renderer
} // namespace nkentseu