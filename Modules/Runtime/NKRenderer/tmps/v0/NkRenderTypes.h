#pragma once
// =============================================================================
// NkRenderTypes.h
// Types fondamentaux NKRenderer — indépendants de NKRHI.
// L'utilisateur n'inclut JAMAIS les headers NKRHI directement.
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NkMath.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"
#include "NKRenderer/Core/NkRef.h"
#include <cstring>

// Pi en float
#ifndef NK_PI_F
#define NK_PI_F 3.14159265358979323846f
#endif

namespace nkentseu {
namespace render {

using namespace math;

// =============================================================================
// Résultat d'opération
// =============================================================================
enum class NkRenderResult : uint32 {
    NK_OK = 0,
    NK_ERROR_INVALID_DEVICE,
    NK_ERROR_INVALID_HANDLE,
    NK_ERROR_COMPILE_FAILED,
    NK_ERROR_OUT_OF_MEMORY,
    NK_ERROR_NOT_SUPPORTED,
    NK_ERROR_IO,
    NK_ERROR_UNKNOWN,
};
inline bool NkOk(NkRenderResult r) { return r == NkRenderResult::NK_OK; }

// =============================================================================
// Formats pixel
// =============================================================================
enum class NkPixelFormat : uint32 {
    NK_UNDEFINED = 0,
    NK_R8,       NK_RG8,    NK_RGB8,   NK_RGBA8,
    NK_RGBA8_SRGB, NK_BGRA8, NK_BGRA8_SRGB,
    NK_RGBA16F,  NK_RGBA32F,
    NK_R32F,     NK_RG32F,  NK_RGB32F,
    NK_R16F,     NK_RG16F,
    NK_R8_UINT,  NK_R32_UINT,
    NK_D16,      NK_D24S8,  NK_D32F,  NK_D32FS8,
    NK_BC1_SRGB, NK_BC3_SRGB, NK_BC5, NK_BC7_SRGB,
    NK_ETC2_SRGB, NK_ASTC4x4_SRGB,
    NK_COUNT,
};

// =============================================================================
// Topologie
// =============================================================================
enum class NkPrimitive : uint32 {
    NK_TRIANGLES, NK_TRIANGLE_STRIP, NK_LINES, NK_LINE_STRIP, NK_POINTS,
};

// =============================================================================
// Fill / Cull
// =============================================================================
enum class NkFillMode : uint32 { NK_SOLID, NK_WIREFRAME, NK_POINT };
enum class NkCullMode : uint32 { NK_NONE, NK_BACK, NK_FRONT };
enum class NkFrontFace: uint32 { NK_CCW, NK_CW };

// =============================================================================
// Blend mode
// =============================================================================
enum class NkBlendMode : uint32 {
    NK_OPAQUE, NK_ALPHA, NK_ADDITIVE, NK_MULTIPLY, NK_PREMULT_ALPHA,
};

// =============================================================================
// Filter / Wrap
// =============================================================================
enum class NkFilterMode : uint32 { NK_NEAREST, NK_LINEAR, NK_ANISOTROPIC };
enum class NkMipFilter  : uint32 { NK_NONE,    NK_NEAREST, NK_LINEAR     };
enum class NkWrapMode   : uint32 { NK_REPEAT, NK_MIRROR, NK_CLAMP, NK_BORDER };

// =============================================================================
// Texture kind
// =============================================================================
enum class NkTextureKind : uint32 {
    NK_2D, NK_CUBE, NK_2D_ARRAY, NK_3D,
    NK_RENDER_TARGET, NK_DEPTH_STENCIL,
};

// =============================================================================
// MSAA
// =============================================================================
enum class NkMSAA : uint32 { NK_1X=1, NK_2X=2, NK_4X=4, NK_8X=8, NK_16X=16 };

// =============================================================================
// Shader stage flags
// =============================================================================
enum class NkShaderStage : uint32 {
    NK_VERTEX       = 1 << 0,
    NK_FRAGMENT     = 1 << 1,
    NK_COMPUTE      = 1 << 2,
    NK_GEOMETRY     = 1 << 3,
    NK_TESS_CTRL    = 1 << 4,
    NK_TESS_EVAL    = 1 << 5,
    NK_ALL_GRAPHICS = (1<<0)|(1<<1)|(1<<3)|(1<<4)|(1<<5),
    NK_ALL          = 0xFF,
};
inline NkShaderStage operator|(NkShaderStage a, NkShaderStage b) {
    return (NkShaderStage)((uint32)a|(uint32)b);
}
inline bool NkHasStage(NkShaderStage flags, NkShaderStage bit) {
    return ((uint32)flags & (uint32)bit) != 0;
}

// =============================================================================
// Depth compare op
// =============================================================================
enum class NkDepthOp : uint32 {
    NK_NEVER, NK_LESS, NK_EQUAL, NK_LESS_EQUAL,
    NK_GREATER, NK_NOT_EQUAL, NK_GREATER_EQUAL, NK_ALWAYS,
};

// =============================================================================
// Couleur RGBA float
// =============================================================================
struct NkColorF {
    float32 r=0.f, g=0.f, b=0.f, a=1.f;
    NkColorF() = default;
    NkColorF(float32 r, float32 g, float32 b, float32 a=1.f):r(r),g(g),b(b),a(a){}
    explicit NkColorF(const math::NkColor& c)
        :r(c.r/255.f),g(c.g/255.f),b(c.b/255.f),a(c.a/255.f){}

    static NkColorF White()       { return {1,1,1,1}; }
    static NkColorF Black()       { return {0,0,0,1}; }
    static NkColorF Transparent() { return {0,0,0,0}; }
    static NkColorF Red()         { return {1,0,0,1}; }
    static NkColorF Green()       { return {0,1,0,1}; }
    static NkColorF Blue()        { return {0,0,1,1}; }
    static NkColorF Yellow()      { return {1,1,0,1}; }
    static NkColorF Cyan()        { return {0,1,1,1}; }
    static NkColorF Magenta()     { return {1,0,1,1}; }
    static NkColorF Gray(float32 v=0.5f) { return {v,v,v,1.f}; }

    NkColorF WithAlpha(float32 a_) const { return {r,g,b,a_}; }
    NkColorF Lerp(const NkColorF& to, float32 t) const {
        return {r+(to.r-r)*t, g+(to.g-g)*t, b+(to.b-b)*t, a+(to.a-a)*t};
    }
    bool operator==(const NkColorF& o) const {
        return r==o.r && g==o.g && b==o.b && a==o.a;
    }
};

// =============================================================================
// Rect float
// =============================================================================
struct NkRectF {
    float32 x=0,y=0,w=0,h=0;
    NkRectF()=default;
    NkRectF(float32 x,float32 y,float32 w,float32 h):x(x),y(y),w(w),h(h){}
    float32 Right()  const { return x+w; }
    float32 Bottom() const { return y+h; }
    NkVec2f Center() const { return {x+w*.5f,y+h*.5f}; }
    NkVec2f TopLeft()const { return {x,y}; }
    bool IsValid()   const { return w>0&&h>0; }
    bool Contains(float32 px,float32 py) const {
        return px>=x && px<x+w && py>=y && py<y+h;
    }
    bool Contains(NkVec2f p) const { return Contains(p.x,p.y); }
};

// =============================================================================
// Viewport + Scissor
// =============================================================================
struct NkViewportF {
    float32 x=0,y=0,width=0,height=0;
    float32 minDepth=0.f, maxDepth=1.f;
    NkRectF ToRect() const { return {x,y,width,height}; }
};

struct NkScissorI {
    int32 x=0,y=0,width=0,height=0;
};

// =============================================================================
// Transform 3D
// =============================================================================
struct NkTransform3D {
    NkVec3f position = {0,0,0};
    NkVec3f rotation = {0,0,0};   // Euler degrees XYZ
    NkVec3f scale    = {1,1,1};

    NkMat4f ToMatrix() const {
        NkMat4f T  = NkMat4f::Translation(position);
        NkMat4f Rx = NkMat4f::RotationX(NkAngle(rotation.x));
        NkMat4f Ry = NkMat4f::RotationY(NkAngle(rotation.y));
        NkMat4f Rz = NkMat4f::RotationZ(NkAngle(rotation.z));
        NkMat4f S  = NkMat4f::Scale(scale);
        return T * Ry * Rx * Rz * S;
    }
    static NkTransform3D Identity() { return {}; }
};

// =============================================================================
// Transform 2D
// =============================================================================
struct NkTransform2D {
    NkVec2f position = {0,0};
    float32 rotation = 0.f;        // degrees
    NkVec2f scale    = {1,1};
    NkVec2f pivot    = {0.5f,0.5f};// 0=top-left, 0.5=center, 1=bottom-right

    NkMat4f ToMatrix() const {
        NkMat4f To = NkMat4f::Translation({-pivot.x*scale.x,-pivot.y*scale.y,0});
        NkMat4f S  = NkMat4f::Scale({scale.x,scale.y,1});
        NkMat4f R  = NkMat4f::RotationZ(NkAngle(rotation));
        NkMat4f T  = NkMat4f::Translation({position.x,position.y,0});
        return T * R * S * To;
    }
    static NkTransform2D Identity() { return {}; }
    static NkTransform2D FromRect(const NkRectF& r) {
        NkTransform2D t;
        t.position = {r.x + r.w*.5f, r.y + r.h*.5f};
        t.scale    = {r.w, r.h};
        t.pivot    = {0.5f,0.5f};
        return t;
    }
};

// =============================================================================
// AABB 3D
// =============================================================================
struct NkAABB {
    NkVec3f min = {-0.5f,-0.5f,-0.5f};
    NkVec3f max = { 0.5f, 0.5f, 0.5f};
    NkVec3f Center()  const { return (min+max)*0.5f; }
    NkVec3f Extents() const { return (max-min)*0.5f; }
    NkVec3f Size()    const { return max-min; }
    float32 BoundingRadius() const {
        NkVec3f e = Extents();
        return NkSqrt(e.x*e.x + e.y*e.y + e.z*e.z);
    }
    bool IsValid() const {
        return min.x<=max.x && min.y<=max.y && min.z<=max.z;
    }
    static NkAABB FromCenterExtents(NkVec3f c, NkVec3f e) {
        return {c-e, c+e};
    }
};

// =============================================================================
// Index format
// =============================================================================
enum class NkIndexFormat : uint32 { NK_UINT16, NK_UINT32 };

// =============================================================================
// Push constant limit
// =============================================================================
static constexpr uint32 kNkMaxPushConstantBytes = 128u;

} // namespace render
} // namespace nkentseu