#pragma once
// =============================================================================
// NkRHI_Device_SW.h — Backend Software Rasterizer (CPU)
// Rasterisation CPU complète : triangle setup, interpolation barycentrique,
// z-buffer, pixel shaders émulés via NkFunction, MSAA optionnel.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKMath/NKMath.h"
#include <memory>
#include <cmath>
#include <thread>
#include <algorithm>

namespace nkentseu {

    class NkSoftwareCommandBuffer;

    // =============================================================================
    // Types internes
    // =============================================================================
    using NkSWVec4  = math::NkVec4;
    using NkSWVec3  = math::NkVec3;
    using NkSWVec2  = math::NkVec2;
    using NkSWColor = math::NkVec4;

    struct NkSWVertex {
        NkSWVec4 position  = {0,0,0,1};
        NkSWVec3 normal    = {0,0,1};
        NkSWVec2 uv        = {0,0};
        NkSWVec4 color     = {1,1,1,1};
        // Internal clip-space values used for depth interpolation in raster stage.
        float32  clipZ     = 0.f;
        float32  clipW     = 1.f;
        float32   attrs[16] = {};
        uint32  attrCount = 0;

        void SetAttrVec3(uint32 base, float32 x, float32 y, float32 z) {
            if (base+2 < 16) { attrs[base]=x; attrs[base+1]=y; attrs[base+2]=z;
                                attrCount = math::NkMax(attrCount, base+3); }
        }
        void SetAttrVec4(uint32 base, float32 x, float32 y, float32 z, float32 w) {
            if (base+3 < 16) { attrs[base]=x; attrs[base+1]=y; attrs[base+2]=z; attrs[base+3]=w;
                                attrCount = math::NkMax(attrCount, base+4); }
        }
        NkSWVec3 GetAttrVec3(uint32 base) const {
            return (base+2 < 16) ? NkSWVec3{attrs[base],attrs[base+1],attrs[base+2]} : NkSWVec3{};
        }
        NkSWVec4 GetAttrVec4(uint32 base) const {
            return (base+3 < 16) ? NkSWVec4{attrs[base],attrs[base+1],attrs[base+2],attrs[base+3]} : NkSWVec4{};
        }
    };

    // Signature des shaders logiciels
    using NkSWVertexShader = NkFunction<NkSWVertex(
        const void* vertexData,
        uint32 vertexIndex,
        const void* uniformData)>;

    using NkSWPixelShader = NkFunction<NkSWColor(
        const NkSWVertex& interpolated,
        const void* uniformData,
        const void* texSampler)>;

    using NkSWComputeShader = NkFunction<void(
        uint32 groupCountX,
        uint32 groupCountY,
        uint32 groupCountZ,
        const void* uniformData)>;

    // =============================================================================
    struct NkSWBuffer {
        NkVector<uint8> data;
        NkBufferDesc desc;
        bool mapped = false;
    };

    struct NkSWSampler {
        NkSamplerDesc desc;
        NkSWColor Sample(const NkVector<uint8>& pixels,
                        uint32 width, uint32 height, uint32 bpp,
                        float32 u, float32 v) const;
    };

    struct NkSWTexture {
        NkVector<NkVector<uint8>> mips; // mip[0] = niveau 0
        NkTextureDesc desc;
        bool isRenderTarget = false;
        NkSWSampler defaultSampler;
        uint32 Width (uint32 mip=0) const { return math::NkMax(1u, desc.width  >> mip); }
        uint32 Height(uint32 mip=0) const { return math::NkMax(1u, desc.height >> mip); }
        uint32 Bpp()                const { return NkFormatBytesPerPixel(desc.format); }
        NkSWColor Sample(float32 u, float32 v, uint32 mip=0) const;
        NkSWColor Read(uint32 x, uint32 y, uint32 mip=0) const;
        void Write(uint32 x, uint32 y, const NkSWColor& c, uint32 mip=0);
    };

    struct NkSWShader {
        NkSWVertexShader  vertFn;
        NkSWPixelShader   fragFn;
        bool isCompute = false;
        NkSWComputeShader computeFn;
    };

    struct NkSWPipeline {
        uint64 shaderId   = 0;
        bool isCompute    = false;
        bool depthTest    = true;
        bool depthWrite   = true;
        bool blendEnable  = false;
        NkCompareOp  depthOp   = NkCompareOp::NK_LESS;
        NkCullMode   cullMode  = NkCullMode::NK_BACK;
        NkFrontFace  frontFace = NkFrontFace::NK_CCW;
        NkBlendFactor srcColor = NkBlendFactor::NK_SRC_ALPHA;
        NkBlendFactor dstColor = NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
        NkPrimitiveTopology topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        uint32 vertexStride = 0;
    };

    struct NkSWRenderPass   { NkRenderPassDesc desc; };
    struct NkSWFramebuffer  {
        uint64 colorId   = 0;
        uint64 depthId   = 0;
        uint32 w=0, h=0;
    };
    struct NkSWDescSetLayout { NkDescriptorSetLayoutDesc desc; };
    struct NkSWDescSet {
        struct Binding { uint32 slot=0; NkDescriptorType type{}; uint64 bufId=0; uint64 texId=0; uint64 sampId=0; };
        NkVector<Binding> bindings;
    };
    struct NkSWFence { bool signaled=false; };

    // =============================================================================
    // Rastériseur interne
    // =============================================================================
    struct NkSWRasterState {
        NkSWTexture*   colorTarget = nullptr;
        NkSWTexture*   depthTarget = nullptr;
        NkSWPipeline*  pipeline    = nullptr;
        NkSWShader*    shader      = nullptr;
        const void*    uniformData = nullptr;
        const void*    vertexData  = nullptr;
        uint32         vertexStride= 0;
        bool           wireframe   = false;
    };

    struct NkSWRasterizer {
            void SetState(const NkSWRasterState& state) { mState = state; }

            void DrawTriangles(const NkSWVertex* vertices, uint32 count);
            void DrawTriangle (const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2);
            void DrawLine     (const NkSWVertex& v0, const NkSWVertex& v1);
            void DrawPoint    (const NkSWVertex& v0);

            using Vertex = math::NkVec3;
            using Color  = math::NkColor;

            struct VertexColor {
                Vertex position;
                Color color;

                VertexColor() : position(), color() {}
                VertexColor(const Vertex& inPosition, const Color& inColor)
                    : position(inPosition), color(inColor) {}
                VertexColor(float32 x, float32 y, float32 z,
                            uint8 r, uint8 g, uint8 b, uint8 a = 255u)
                    : position(x, y, z), color(r, g, b, a) {}
            };

            NkVector<uint8> pixels;
            NkVector<float32> depth;
            uint32 width  = 0;
            uint32 height = 0;
            uint32 stride = 0;
            uint32 clipMinX = 0;
            uint32 clipMinY = 0;
            uint32 clipMaxX = 0;
            uint32 clipMaxY = 0;
            bool   depthEnabled = true;

            void Resize(uint32 w, uint32 h) {
                width = w;
                height = h;
                stride = w * 4u;

                const usize pixelCount = static_cast<usize>(stride) * static_cast<usize>(h);
                pixels.Assign(static_cast<uint8>(0), static_cast<NkVector<uint8>::SizeType>(pixelCount));

                ResetClipRect();
                if (depthEnabled) {
                    const usize depthCount = static_cast<usize>(w) * static_cast<usize>(h);
                    depth.Assign(1.0f, static_cast<NkVector<float32>::SizeType>(depthCount));
                } else {
                    depth.Clear();
                }
            }

            void Clear(uint8 r = 0, uint8 g = 0, uint8 b = 0, uint8 a = 255) {
                for (usize i = 0; i < pixels.Size(); i += 4) {
                    pixels[i + 0] = r;
                    pixels[i + 1] = g;
                    pixels[i + 2] = b;
                    pixels[i + 3] = a;
                }

                if (depthEnabled) {
                    ClearDepth(1.0f);
                }
            }

            void ClearDepth(float32 clearValue = 1.0f) {
                if (!depthEnabled) return;

                const usize depthCount = static_cast<usize>(width) * static_cast<usize>(height);
                if (depth.Size() != depthCount) {
                    depth.Assign(clearValue, static_cast<NkVector<float32>::SizeType>(depthCount));
                    return;
                }

                for (usize i = 0; i < depth.Size(); ++i) {
                    depth[i] = clearValue;
                }
            }

            void EnableDepthBuffer(bool enabled = true, float32 clearValue = 1.0f) {
                depthEnabled = enabled;
                if (!enabled) {
                    depth.Clear();
                    return;
                }

                const usize depthCount = static_cast<usize>(width) * static_cast<usize>(height);
                depth.Assign(clearValue, static_cast<NkVector<float32>::SizeType>(depthCount));
            }

            bool HasDepthBuffer() const {
                return depthEnabled && !depth.Empty() &&
                    depth.Size() == static_cast<usize>(width) * static_cast<usize>(height);
            }

            uint8*       RowPtr(uint32 y)       { return pixels.Data() + y * stride; }
            const uint8* RowPtr(uint32 y) const { return pixels.Data() + y * stride; }

            void SetPixel(uint32 x, uint32 y, uint8 r, uint8 g, uint8 b, uint8 a = 255) {
                if (x >= width || y >= height) return;
                PutPixelUnchecked(x, y, r, g, b, a, false);
            }

            void DrawPoint(int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint8 a = 255,
                        float32 z = 0.0f, bool blend = true) {
                if (!IsInsideRasterBounds(x, y) || !IsInsideClip(x, y)) return;

                const uint32 px = static_cast<uint32>(x);
                const uint32 py = static_cast<uint32>(y);
                if (!DepthPass(px, py, z)) return;
                PutPixelUnchecked(px, py, r, g, b, a, blend);
            }

            void DrawLine(int32 x0, int32 y0, int32 x1, int32 y1,
                        uint8 r, uint8 g, uint8 b, uint8 a = 255,
                        float32 z0 = 0.0f, float32 z1 = 0.0f, bool blend = true) {
                int32 dx = AbsInt(x1 - x0);
                int32 sx = (x0 < x1) ? 1 : -1;
                int32 dy = -AbsInt(y1 - y0);
                int32 sy = (y0 < y1) ? 1 : -1;
                int32 err = dx + dy;
                int32 step = 0;
                const int32 steps = MaxInt(dx, -dy);

                for (;;) {
                    const float32 t = (steps > 0) ? static_cast<float32>(step) / static_cast<float32>(steps) : 0.0f;
                    DrawPoint(x0, y0, r, g, b, a, z0 + (z1 - z0) * t, blend);
                    if (x0 == x1 && y0 == y1) break;
                    const int32 e2 = err * 2;
                    if (e2 >= dy) { err += dy; x0 += sx; }
                    if (e2 <= dx) { err += dx; y0 += sy; }
                    ++step;
                }
            }

            void DrawRect(int32 x, int32 y, int32 w, int32 h,
                        uint8 r, uint8 g, uint8 b, uint8 a = 255,
                        float32 z = 0.0f, bool blend = true) {
                if (w <= 0 || h <= 0) return;

                const int32 x1 = x + w - 1;
                const int32 y1 = y + h - 1;
                DrawLine(x,  y,  x1, y,  r, g, b, a, z, z, blend);
                DrawLine(x1, y,  x1, y1, r, g, b, a, z, z, blend);
                DrawLine(x1, y1, x,  y1, r, g, b, a, z, z, blend);
                DrawLine(x,  y1, x,  y,  r, g, b, a, z, z, blend);
            }

            void FillRect(int32 x, int32 y, int32 w, int32 h,
                        uint8 r, uint8 g, uint8 b, uint8 a = 255,
                        float32 z = 0.0f, bool blend = true) {
                if (w <= 0 || h <= 0 || width == 0 || height == 0) return;

                int32 x0 = MaxInt(x, static_cast<int32>(clipMinX));
                int32 y0 = MaxInt(y, static_cast<int32>(clipMinY));
                int32 x1 = MinInt(x + w, static_cast<int32>(clipMaxX));
                int32 y1 = MinInt(y + h, static_cast<int32>(clipMaxY));
                if (x0 >= x1 || y0 >= y1) return;

                for (int32 py = y0; py < y1; ++py) {
                    for (int32 px = x0; px < x1; ++px) {
                        const uint32 ux = static_cast<uint32>(px);
                        const uint32 uy = static_cast<uint32>(py);
                        if (!DepthPass(ux, uy, z)) continue;
                        PutPixelUnchecked(ux, uy, r, g, b, a, blend);
                    }
                }
            }

            void DrawTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                            uint8 r, uint8 g, uint8 b, uint8 a = 255, bool blend = true) {
                DrawLine(RoundToInt(v0.x), RoundToInt(v0.y), RoundToInt(v1.x), RoundToInt(v1.y),
                        r, g, b, a, v0.z, v1.z, blend);
                DrawLine(RoundToInt(v1.x), RoundToInt(v1.y), RoundToInt(v2.x), RoundToInt(v2.y),
                        r, g, b, a, v1.z, v2.z, blend);
                DrawLine(RoundToInt(v2.x), RoundToInt(v2.y), RoundToInt(v0.x), RoundToInt(v0.y),
                        r, g, b, a, v2.z, v0.z, blend);
            }

            void DrawTriangle(int32 x0, int32 y0, int32 x1, int32 y1, int32 x2, int32 y2,
                            uint8 r, uint8 g, uint8 b, uint8 a = 255,
                            float32 z0 = 0.0f, float32 z1 = 0.0f, float32 z2 = 0.0f, bool blend = true) {
                DrawTriangle(Vertex{static_cast<float32>(x0), static_cast<float32>(y0), z0},
                            Vertex{static_cast<float32>(x1), static_cast<float32>(y1), z1},
                            Vertex{static_cast<float32>(x2), static_cast<float32>(y2), z2},
                            r, g, b, a, blend);
            }

            void FillTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                            uint8 r, uint8 g, uint8 b, uint8 a = 255, bool blend = true) {
                const Color c(r, g, b, a);
                FillTriangleInterpolated(v0, c, v1, c, v2, c, blend);
            }

            void FillTriangle(int32 x0, int32 y0, int32 x1, int32 y1, int32 x2, int32 y2,
                            uint8 r, uint8 g, uint8 b, uint8 a = 255,
                            float32 z0 = 0.0f, float32 z1 = 0.0f, float32 z2 = 0.0f, bool blend = true) {
                FillTriangle(Vertex{static_cast<float32>(x0), static_cast<float32>(y0), z0},
                            Vertex{static_cast<float32>(x1), static_cast<float32>(y1), z1},
                            Vertex{static_cast<float32>(x2), static_cast<float32>(y2), z2},
                            r, g, b, a, blend);
            }

            void FillTriangleInterpolated(const VertexColor& v0,
                                        const VertexColor& v1,
                                        const VertexColor& v2,
                                        bool blend = true) {
                FillTriangleInterpolated(v0.position, v0.color,
                                        v1.position, v1.color,
                                        v2.position, v2.color,
                                        blend);
            }

            void FillTriangleInterpolated(const Vertex& v0, const Color& c0,
                                        const Vertex& v1, const Color& c1,
                                        const Vertex& v2, const Color& c2,
                                        bool blend = true) {
                if (width == 0 || height == 0) return;

                const float32 area = EdgeFunction(v0, v1, v2.x, v2.y);
                if (math::NkIsNearlyZero(area)) {
                    DrawTriangle(v0, v1, v2,
                                ColorToByte(c0.r), ColorToByte(c0.g),
                                ColorToByte(c0.b), ColorToByte(c0.a), blend);
                    return;
                }

                int32 minX = FloorToInt(MinFloat(v0.x, MinFloat(v1.x, v2.x)));
                int32 minY = FloorToInt(MinFloat(v0.y, MinFloat(v1.y, v2.y)));
                int32 maxX = CeilToInt(MaxFloat(v0.x, MaxFloat(v1.x, v2.x)));
                int32 maxY = CeilToInt(MaxFloat(v0.y, MaxFloat(v1.y, v2.y)));

                minX = MaxInt(minX, static_cast<int32>(clipMinX));
                minY = MaxInt(minY, static_cast<int32>(clipMinY));
                maxX = MinInt(maxX, static_cast<int32>(clipMaxX) - 1);
                maxY = MinInt(maxY, static_cast<int32>(clipMaxY) - 1);
                if (minX > maxX || minY > maxY) return;

                const bool ccw = (area > 0.0f);
                const float32 invArea = 1.0f / area;

                for (int32 y = minY; y <= maxY; ++y) {
                    const float32 py = static_cast<float32>(y) + 0.5f;
                    for (int32 x = minX; x <= maxX; ++x) {
                        const float32 px = static_cast<float32>(x) + 0.5f;
                        const float32 w0 = EdgeFunction(v1, v2, px, py);
                        const float32 w1 = EdgeFunction(v2, v0, px, py);
                        const float32 w2 = EdgeFunction(v0, v1, px, py);

                        if (ccw) {
                            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                        } else {
                            if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                        }

                        const float32 b0 = w0 * invArea;
                        const float32 b1 = w1 * invArea;
                        const float32 b2 = w2 * invArea;
                        const float32 z = b0 * v0.z + b1 * v1.z + b2 * v2.z;

                        const uint32 ux = static_cast<uint32>(x);
                        const uint32 uy = static_cast<uint32>(y);
                        if (!DepthPass(ux, uy, z)) continue;

                        const uint8 outR = ToByte(b0 * ColorTo255(c0.r) +
                                                b1 * ColorTo255(c1.r) +
                                                b2 * ColorTo255(c2.r));
                        const uint8 outG = ToByte(b0 * ColorTo255(c0.g) +
                                                b1 * ColorTo255(c1.g) +
                                                b2 * ColorTo255(c2.g));
                        const uint8 outB = ToByte(b0 * ColorTo255(c0.b) +
                                                b1 * ColorTo255(c1.b) +
                                                b2 * ColorTo255(c2.b));
                        const uint8 outA = ToByte(b0 * ColorTo255(c0.a) +
                                                b1 * ColorTo255(c1.a) +
                                                b2 * ColorTo255(c2.a));
                        PutPixelUnchecked(ux, uy, outR, outG, outB, outA, blend);
                    }
                }
            }

            void SetClipRect(int32 x, int32 y, int32 w, int32 h) {
                if (w <= 0 || h <= 0 || width == 0 || height == 0) {
                    clipMinX = clipMinY = clipMaxX = clipMaxY = 0;
                    return;
                }

                const int32 x0 = MaxInt(0, x);
                const int32 y0 = MaxInt(0, y);
                const int32 x1 = MinInt(static_cast<int32>(width), x + w);
                const int32 y1 = MinInt(static_cast<int32>(height), y + h);
                if (x0 >= x1 || y0 >= y1) {
                    clipMinX = clipMinY = clipMaxX = clipMaxY = 0;
                    return;
                }

                clipMinX = static_cast<uint32>(x0);
                clipMinY = static_cast<uint32>(y0);
                clipMaxX = static_cast<uint32>(x1);
                clipMaxY = static_cast<uint32>(y1);
            }

            void ResetClipRect() {
                clipMinX = 0;
                clipMinY = 0;
                clipMaxX = width;
                clipMaxY = height;
            }

            void Swap(NkSWRasterizer& other) {
                pixels.Swap(other.pixels);
                depth.Swap(other.depth);
                SwapValue(width, other.width);
                SwapValue(height, other.height);
                SwapValue(stride, other.stride);
                SwapValue(clipMinX, other.clipMinX);
                SwapValue(clipMinY, other.clipMinY);
                SwapValue(clipMaxX, other.clipMaxX);
                SwapValue(clipMaxY, other.clipMaxY);
                SwapValue(depthEnabled, other.depthEnabled);
            }

            bool IsValid() const { return width > 0 && height > 0 && !pixels.Empty(); }

        private:
            NkSWVertex ClipToNDC(const NkSWVertex& vertex) const;
            NkSWVertex NDCToScreen(const NkSWVertex& vertex, float32 width, float32 height) const;
            NkSWVertex Interpolate(const NkSWVertex& a, const NkSWVertex& b, float32 t) const;
            NkSWVertex BaryInterp(const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2,
                                float32 l0, float32 l1, float32 l2) const;
            bool DepthTest(uint32 x, uint32 y, float32 z);
            NkSWColor BlendColor(const NkSWColor& src, const NkSWColor& dst) const;
            float32 ApplyBlendFactor(NkBlendFactor factor, float32 src, float32 dst, float32 srcAlpha, float32 dstAlpha) const;

            NkSWRasterState mState;

            static int32 AbsInt(int32 v) { return math::NkAbs(v); }
            static int32 MinInt(int32 a, int32 b) { return math::NkMin<int32>(a, b); }
            static int32 MaxInt(int32 a, int32 b) { return math::NkMax<int32>(a, b); }
            static float32 MinFloat(float32 a, float32 b) { return math::NkMin<float32>(a, b); }
            static float32 MaxFloat(float32 a, float32 b) { return math::NkMax<float32>(a, b); }
            static int32 FloorToInt(float32 v) { return static_cast<int32>(math::NkFloor(v)); }
            static int32 CeilToInt(float32 v)  { return static_cast<int32>(math::NkCeil(v)); }
            static int32 RoundToInt(float32 v) { return static_cast<int32>(math::NkRound(v)); }
            static uint8 ToByte(float32 v) {
                const float32 clamped = math::NkClamp<float32>(v, 0.0f, 255.0f);
                return static_cast<uint8>(RoundToInt(clamped));
            }
            static float32 ColorTo255(float32 c) {
                // Legacy Color stores normalized channels [0..1]. Keep compatibility if [0..255] is passed.
                if (c >= 0.0f && c <= 1.0f) {
                    return c * 255.0f;
                }
                return c;
            }
            static uint8 ColorToByte(float32 c) { return ToByte(ColorTo255(c)); }

            template<typename T>
            static void SwapValue(T& a, T& b) {
                T tmp = a;
                a = b;
                b = tmp;
            }

            static float32 EdgeFunction(const Vertex& a, const Vertex& b, float32 px, float32 py) {
                return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
            }

            bool IsInsideRasterBounds(int32 x, int32 y) const {
                return x >= 0 && y >= 0 &&
                    x < static_cast<int32>(width) &&
                    y < static_cast<int32>(height);
            }

            bool IsInsideClip(int32 x, int32 y) const {
                return x >= static_cast<int32>(clipMinX) &&
                    y >= static_cast<int32>(clipMinY) &&
                    x < static_cast<int32>(clipMaxX) &&
                    y < static_cast<int32>(clipMaxY);
            }

            bool DepthPass(uint32 x, uint32 y, float32 z) {
                if (!depthEnabled) return true;
                if (depth.Empty()) return true;

                const usize index = static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x);
                if (index >= depth.Size()) return true;
                if (z > depth[index]) return false;
                depth[index] = z;
                return true;
            }

            void PutPixelUnchecked(uint32 x, uint32 y, uint8 r, uint8 g, uint8 b, uint8 a, bool blend) {
                uint8* p = RowPtr(y) + x * 4u;
                if (!blend || a == 255u) {
                    p[0] = r;
                    p[1] = g;
                    p[2] = b;
                    p[3] = a;
                    return;
                }

                if (a == 0u) return;
                const uint32 srcA = static_cast<uint32>(a);
                const uint32 invA = 255u - srcA;
                p[0] = static_cast<uint8>((static_cast<uint32>(r) * srcA + static_cast<uint32>(p[0]) * invA + 127u) / 255u);
                p[1] = static_cast<uint8>((static_cast<uint32>(g) * srcA + static_cast<uint32>(p[1]) * invA + 127u) / 255u);
                p[2] = static_cast<uint8>((static_cast<uint32>(b) * srcA + static_cast<uint32>(p[2]) * invA + 127u) / 255u);
                p[3] = 255u;
            }
    };

    // =============================================================================
    // NkSoftwareDevice
    // =============================================================================
    class NkSoftwareDevice final : public NkIDevice {
        public:
            NkSoftwareDevice()  = default;
            ~NkSoftwareDevice() override;

            bool          Initialize(const NkDeviceInitInfo& init) override;
            void          Shutdown()                          override;
            bool          IsValid()                     const override { return mIsValid; }
            NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_SOFTWARE; }
            const NkDeviceCaps& GetCaps()               const override { return mCaps; }

            NkBufferHandle  CreateBuffer (const NkBufferDesc& d)                      override;
            void            DestroyBuffer(NkBufferHandle& h)                          override;
            bool WriteBuffer(NkBufferHandle,const void*,uint64,uint64)                override;
            bool WriteBufferAsync(NkBufferHandle,const void*,uint64,uint64)           override;
            bool ReadBuffer(NkBufferHandle,void*,uint64,uint64)                       override;
            NkMappedMemory MapBuffer(NkBufferHandle,uint64,uint64)                    override;
            void           UnmapBuffer(NkBufferHandle)                                override;

            NkTextureHandle  CreateTexture (const NkTextureDesc& d)                   override;
            void             DestroyTexture(NkTextureHandle& h)                        override;
            bool WriteTexture(NkTextureHandle,const void*,uint32)                     override;
            bool WriteTextureRegion(NkTextureHandle,const void*,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32) override;
            bool GenerateMipmaps(NkTextureHandle, NkFilter)                           override;

            NkSamplerHandle  CreateSampler (const NkSamplerDesc& d)                   override;
            void             DestroySampler(NkSamplerHandle& h)                        override;

            NkShaderHandle   CreateShader (const NkShaderDesc& d)                     override;
            void             DestroyShader(NkShaderHandle& h)                          override;

            NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d)  override;
            NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc& d)   override;
            void             DestroyPipeline(NkPipelineHandle& h)                     override;

            NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& d)         override;
            void                DestroyRenderPass (NkRenderPassHandle& h)              override;
            NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& d)        override;
            void                DestroyFramebuffer(NkFramebufferHandle& h)             override;
            NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFB; }
            NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
            NkGPUFormat GetSwapchainFormat()      const override { return NkGPUFormat::NK_RGBA8_UNORM; }
            NkGPUFormat GetSwapchainDepthFormat() const override { return NkGPUFormat::NK_D32_FLOAT;   }
            uint32   GetSwapchainWidth()       const override { return mWidth; }
            uint32   GetSwapchainHeight()      const override { return mHeight; }

            NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) override;
            void            DestroyDescriptorSetLayout(NkDescSetHandle& h)                override;
            NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)           override;
            void            FreeDescriptorSet    (NkDescSetHandle& h)                     override;
            void            UpdateDescriptorSets(const NkDescriptorWrite* w, uint32 n)   override;

            NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType t)                  override;
            void              DestroyCommandBuffer(NkICommandBuffer*& cb)                 override;

            void Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence)     override;
            void SubmitAndPresent(NkICommandBuffer* cb)                                   override;
            NkFenceHandle CreateFence(bool signaled)  override;
            void DestroyFence(NkFenceHandle& h)       override;
            bool WaitFence(NkFenceHandle f,uint64 to) override;
            bool IsFenceSignaled(NkFenceHandle f)     override;
            void ResetFence(NkFenceHandle f)          override;
            void WaitIdle()                           override {}

            bool   BeginFrame(NkFrameContext& frame) override;
            void   EndFrame  (NkFrameContext& frame) override;
            uint32 GetFrameIndex()        const override { return mFrameIndex; }
            uint32 GetMaxFramesInFlight() const override { return 1; }
            uint64 GetFrameNumber()       const override { return mFrameNumber; }
            void   OnResize(uint32 w, uint32 h) override;

            void* GetNativeDevice()       const override { return nullptr; }
            void* GetNativeCommandQueue() const override { return nullptr; }

            // Accès interne
            NkSWBuffer*      GetBuf  (uint64 id);
            NkSWTexture*     GetTex  (uint64 id);
            NkSWSampler*     GetSamp (uint64 id);
            NkSWShader*      GetShader(uint64 id);
            NkSWPipeline*    GetPipe (uint64 id);
            NkSWDescSet*     GetDescSet(uint64 id);
            NkSWFramebuffer* GetFBO  (uint64 id);

            // Présentation — copie le color buffer vers la surface native
            void Present();
            // Accès direct au backbuffer pour présentation (lecture seule)
            const uint8* BackbufferPixels() const;
            uint32 BackbufferWidth()  const { return mWidth; }
            uint32 BackbufferHeight() const { return mHeight; }

        private:
            void CreateSwapchainObjects();
            uint64 NextId() { return ++mNextId; }
            NkAtomic<uint64> mNextId{0};

            NkUnorderedMap<uint64, NkSWBuffer>       mBuffers;
            NkUnorderedMap<uint64, NkSWTexture>       mTextures;
            NkUnorderedMap<uint64, NkSWSampler>       mSamplers;
            NkUnorderedMap<uint64, NkSWShader>        mShaders;
            NkUnorderedMap<uint64, NkSWPipeline>      mPipelines;
            NkUnorderedMap<uint64, NkSWRenderPass>    mRenderPasses;
            NkUnorderedMap<uint64, NkSWFramebuffer>   mFramebuffers;
            NkUnorderedMap<uint64, NkSWDescSetLayout> mDescLayouts;
            NkUnorderedMap<uint64, NkSWDescSet>       mDescSets;
            NkUnorderedMap<uint64, NkSWFence>         mFences;

            NkFramebufferHandle mSwapchainFB;
            NkRenderPassHandle  mSwapchainRP;

            mutable threading::NkMutex mMutex;
            NkDeviceInitInfo    mInit   {};
            NkDeviceCaps        mCaps   {};
            bool                mIsValid= false;
            uint32              mWidth=0, mHeight=0;
            uint32              mFrameIndex  = 0;
            uint64              mFrameNumber = 0;
            uint32              mThreadCount = 0;
            bool                mUseSse = true;
    };

} // namespace nkentseu
