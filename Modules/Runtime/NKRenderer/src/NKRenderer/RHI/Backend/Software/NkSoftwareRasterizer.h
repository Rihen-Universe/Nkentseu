#pragma once
// =============================================================================
// NkSoftwareRasterizer.h — v4 final
// Corrections :
//   - NkSWGeomContext : callbacks par valeur (NkFunction) plus par pointeur
//   - Suppression des logs debug dans les boucles de rasterisation
// =============================================================================
#include "NkSoftwareTypes.h"

namespace nkentseu {

// =============================================================================
// NkSWGeomContext — contexte passé au geometry shader CPU
// CORRECTION v4 : emitVertex et endPrimitive sont des NkFunction par valeur,
// pas des pointeurs. Évite les dangling pointer sur les lambdas locales.
// =============================================================================
struct NkSWGeomContext {
    NkFunction<void(const NkSWVertex&)> emitVertex;
    NkFunction<void()>                  endPrimitive;
    const void*                          uniformData    = nullptr;
    NkPrimitiveTopology                  outputTopology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
};

// =============================================================================
// Flags de clipping homogène
// =============================================================================
enum NkSWClipFlags : uint32 {
    NK_SW_CLIP_NONE  = 0,
    NK_SW_CLIP_POS_W = 1 << 0,
    NK_SW_CLIP_NEG_X = 1 << 1,
    NK_SW_CLIP_POS_X = 1 << 2,
    NK_SW_CLIP_NEG_Y = 1 << 3,
    NK_SW_CLIP_POS_Y = 1 << 4,
    NK_SW_CLIP_NEG_Z = 1 << 5,
    NK_SW_CLIP_POS_Z = 1 << 6,
};

// =============================================================================
// NkSWRasterizer
// =============================================================================
class NkSWRasterizer {
public:
    void SetState(const NkSWDrawState& state);

    void DrawTriangles   (const NkSWVertex* verts, uint32 count);
    void DrawTrianglesIdx(const NkSWVertex* verts, const uint32* indices, uint32 idxCount);
    void DrawTriangleStrip(const NkSWVertex* verts, uint32 count);
    void DrawTriangleFan (const NkSWVertex* verts, uint32 count);
    void DrawLines       (const NkSWVertex* verts, uint32 count);
    void DrawLineStrip   (const NkSWVertex* verts, uint32 count);
    void DrawLineLoop    (const NkSWVertex* verts, uint32 count);
    void DrawPoints      (const NkSWVertex* verts, uint32 count);

    static NkSWVertex LerpVertex(const NkSWVertex& a, const NkSWVertex& b, float t);

private:
    uint32     ComputeClipFlags(const NkSWVertex& v) const;
    uint32     ClipTriangle(const NkSWVertex v[3], NkSWVertex out[9]) const;
    NkSWVertex PerspectiveDivide(const NkSWVertex& v) const;
    NkSWVertex NDCToScreen(const NkSWVertex& v) const;
    NkSWVertex BaryInterp(const NkSWVertex& s0, const NkSWVertex& s1,
                            const NkSWVertex& s2, float l0, float l1, float l2) const;

    bool    DepthTest   (uint32 x, uint32 y, float z);
    NkVec4f Blend       (const NkVec4f& src, const NkVec4f& dst) const;
    float   ApplyFactor (NkBlendFactor f, float src, float dst, float sA, float dA) const;

    void WriteFragment  (uint32 x, uint32 y, float z, const NkSWVertex& frag);
    void WriteSampleMSAA(uint32 x, uint32 y, uint32 sample, float z, const NkSWVertex& frag);
    void ResolveMSAA    ();

    void RasterTriangle (const NkSWVertex& s0, const NkSWVertex& s1, const NkSWVertex& s2);
    void RasterLine     (const NkSWVertex& s0, const NkSWVertex& s1);
    void RasterPoint    (const NkSWVertex& s);
    void DrawOneTriangle(const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2);

    void RunGeometryShader(const NkSWVertex* verts, uint32 count, NkPrimitiveTopology inputTopo);
    void RunTessellation  (const NkSWVertex* patch, uint32 patchSize);

    NkSWDrawState mState;
};

} // namespace nkentseu