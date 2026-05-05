#pragma once
// =============================================================================
// NkSoftwareContext.h — Rendu logiciel CPU production-ready
// =============================================================================
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Core/NkSurfaceDesc.h"
#include "NKContainers/NkContainers.h"
#include "NKMath/NkFunctions.h"
#include "NKMath/NKMath.h"

#include "NkSWPixel.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    struct NkSoftwareFramebuffer {
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
            // SIMD clear : 4 pixels/cycle (SSE2)
            for (uint32 y = 0; y < height; ++y) {
                nkentseu::sw_detail::ClearRow(RowPtr(y), width, r, g, b, a);
            }
            if (depthEnabled) ClearDepth(1.0f);
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

        math::NkColor GetPixel(uint32 x, uint32 y) {
            if (x >= width || y >= height) return math::NkColor();
            uint8* p = RowPtr(y) + x * 4u;
            uint8 r, g, b, a;
            nkentseu::sw_detail::LoadPixel(p, r, g, b, a);
            return math::NkColor(r, g, b, a);
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
                uint8* row = RowPtr(static_cast<uint32>(py));
                if (!blend || a == 255u) {
                    nkentseu::sw_detail::FillSpanOpaque(row, x0, x1, r, g, b);
                } else {
                    nkentseu::sw_detail::BlendSpanAlpha(row, x0, x1, r, g, b, a);
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
                    uint8* p = RowPtr(uy) + ux * 4u;
                    if (blend && outA < 255u) {
                        nkentseu::sw_detail::BlendPixel(p, outR, outG, outB, outA);
                    } else {
                        nkentseu::sw_detail::StorePixel(p, outR, outG, outB, outA);
                    }
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

        void Swap(NkSoftwareFramebuffer& other) {
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
                nkentseu::sw_detail::StorePixel(p, r, g, b, a);
                return;
            }
            if (a == 0u) return;
            nkentseu::sw_detail::BlendPixel(p, r, g, b, a);
        }
    };

    // -------------------------------------------------------------------------
    struct NkSoftwareContextData {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        void* hwnd      = nullptr;  // HWND
        void* hdc       = nullptr;  // HDC
        void* dibBitmap = nullptr;  // HBITMAP DIBSection
        void* dibDC     = nullptr;  // HDC mémoire
        void* dibBits   = nullptr;  // Pointeur pixels DIB
#elif defined(NKENTSEU_WINDOWING_XLIB)
        void*         display  = nullptr;
        unsigned long window   = 0;
        void*         gc       = nullptr;  // GC
        void*         ximage   = nullptr;  // XImage
        void*         shmInfo  = nullptr;  // XShmSegmentInfo*
        bool          useSHM   = false;
        int           shmid    = -1;
#elif defined(NKENTSEU_WINDOWING_XCB)
        void*    connection = nullptr;
        unsigned long window= 0;
        uint32   gc         = 0;   // xcb_gcontext_t
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
        void*  wlDisplay  = nullptr;
        void*  wlSurface  = nullptr;
        void*  wlBuffer   = nullptr;
        void*  shmPixels  = nullptr;
        bool   waylandConfigured = false;
        uint32 shmStride  = 0;
        uint64 shmSize    = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
        void*  nativeWindow = nullptr;
#elif defined(NKENTSEU_PLATFORM_MACOS)
        void*  nsView    = nullptr;
        void*  cgContext = nullptr;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        const char* canvasId = "#canvas";
#endif
        uint32 width  = 0;
        uint32 height = 0;
    };

    // -------------------------------------------------------------------------
    class NkSoftwareContext final : public NkIGraphicsContext {
        public:
            NkSoftwareContext()  = default;
            ~NkSoftwareContext() override;

            bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
            void          Shutdown()                                             override;
            bool          IsValid()   const                                      override;
            bool          BeginFrame()                                           override;
            void          EndFrame()                                             override;
            void          Present()                                              override;
            bool          OnResize(uint32 w, uint32 h)                           override;
            void          SetVSync(bool enabled)                                 override;
            bool          GetVSync() const                                       override;
            NkGraphicsApi GetApi()   const                                       override;
            NkContextInfo GetInfo()  const                                       override;
            NkContextDesc GetDesc()  const                                       override;
            void*         GetNativeContextData()                                 override;
            bool          SupportsCompute() const                                override;

            NkSoftwareFramebuffer& GetBackBuffer()  { return mBackBuffer; }
            NkSoftwareFramebuffer& GetFrontBuffer() { return mFrontBuffer; }

        private:
            bool InitNativePresenter (const NkSurfaceDesc& surf);
            void ShutdownNativePresenter();
            void PresentNative();

            NkSoftwareFramebuffer  mBackBuffer;
            NkSoftwareFramebuffer  mFrontBuffer;
            NkSoftwareContextData  mData;
            NkContextDesc          mDesc;
            NkSurfaceDesc          mCachedSurface; // PATCH OnResize : stocké pour réinit
            bool                   mIsValid = false;
            bool                   mVSync   = false;
    };

} // namespace nkentseu
