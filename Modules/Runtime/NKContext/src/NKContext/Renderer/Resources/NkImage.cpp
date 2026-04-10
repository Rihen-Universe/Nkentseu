// =============================================================================
// NkImage.cpp — stb_image integration
// Define STB macros in ONE translation unit only.
// =============================================================================
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_WRITE_IMPLEMENTATION

// stb headers — expected to be located in a vendor/ or thirdparty/ directory.
// Adjust the include paths to match your project layout.
// E.g. if using CMake FetchContent or a submodule at vendor/stb:
#include "NKContext/STB/stb_image.h"
#include "NKContext/STB/stb_image_write.h"

#include "NkImage.h"
#include "NKLogger/NkLog.h"

#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace renderer {

    // =============================================================================
        bool NkImage::Create(uint32 width, uint32 height, const NkColor2D& fillColor) {
            if (width == 0 || height == 0) return false;
            mWidth  = width;
            mHeight = height;
            mPixels.Resize((NkVector<uint8>::SizeType)(width * height * 4u));
            for (uint32 i = 0; i < width * height; ++i) {
                mPixels[i*4+0] = fillColor.r;
                mPixels[i*4+1] = fillColor.g;
                mPixels[i*4+2] = fillColor.b;
                mPixels[i*4+3] = fillColor.a;
            }
            return true;
        }

        // =============================================================================
        bool NkImage::LoadFromFile(const char* path) {
            if (!path || !*path) { logger.Errorf("[NkImage] Null path"); return false; }
            int w, h, channels;
            uint8* data = stbi_load(path, &w, &h, &channels, 4);
            if (!data) {
                logger.Errorf("[NkImage] stbi_load failed for '%s': %s", path, stbi_failure_reason());
                return false;
            }
            const bool ok = LoadFromPixels(data, (uint32)w, (uint32)h);
            stbi_image_free(data);
            return ok;
        }

        // =============================================================================
        bool NkImage::LoadFromMemory(const void* data, usize sizeBytes) {
            if (!data || sizeBytes == 0) return false;
            int w, h, channels;
            uint8* pixels = stbi_load_from_memory(
                static_cast<const stbi_uc*>(data), (int)sizeBytes,
                &w, &h, &channels, 4);
            if (!pixels) {
                logger.Errorf("[NkImage] stbi_load_from_memory failed: %s", stbi_failure_reason());
                return false;
            }
            const bool ok = LoadFromPixels(pixels, (uint32)w, (uint32)h);
            stbi_image_free(pixels);
            return ok;
        }

        // =============================================================================
        bool NkImage::LoadFromPixels(const uint8* pixels, uint32 width, uint32 height) {
            if (!pixels || width == 0 || height == 0) return false;
            mWidth  = width;
            mHeight = height;
            const usize sz = (usize)width * (usize)height * 4u;
            mPixels.Resize((NkVector<uint8>::SizeType)sz);
            memcpy(mPixels.Data(), pixels, sz);
            return true;
        }

        // =============================================================================
        bool NkImage::SaveToFile(const char* path) const {
            if (!IsValid() || !path || !*path) return false;
            // stbi_write_png expects (path, w, h, channels, data, stride_in_bytes)
            const int ok = stbi_write_png(
                path, (int)mWidth, (int)mHeight, 4,
                mPixels.Data(), (int)(mWidth * 4u));
            if (!ok) {
                logger.Errorf("[NkImage] SaveToFile failed for '%s'", path);
            }
            return ok != 0;
        }

        // =============================================================================
        NkColor2D NkImage::GetPixel(uint32 x, uint32 y) const {
            if (x >= mWidth || y >= mHeight) return {};
            const uint8* p = mPixels.Data() + ((usize)y * mWidth + x) * 4u;
            return {p[0], p[1], p[2], p[3]};
        }

        // =============================================================================
        void NkImage::SetPixel(uint32 x, uint32 y, const NkColor2D& color) {
            if (x >= mWidth || y >= mHeight) return;
            uint8* p = mPixels.Data() + ((usize)y * mWidth + x) * 4u;
            p[0] = color.r;
            p[1] = color.g;
            p[2] = color.b;
            p[3] = color.a;
        }

        // =============================================================================
        void NkImage::FlipVertically() {
            if (!IsValid()) return;
            const uint32 rowSize = mWidth * 4u;
            NkVector<uint8> rowBuf;
            rowBuf.Resize((NkVector<uint8>::SizeType)rowSize);
            for (uint32 y = 0; y < mHeight / 2; ++y) {
                uint8* top = mPixels.Data() + (usize)y * rowSize;
                uint8* bot = mPixels.Data() + (usize)(mHeight - 1 - y) * rowSize;
                memcpy(rowBuf.Data(), top, rowSize);
                memcpy(top, bot, rowSize);
                memcpy(bot, rowBuf.Data(), rowSize);
            }
        }

        // =============================================================================
        void NkImage::Copy(const NkImage& source,
                        uint32 destX, uint32 destY,
                        const NkRect2i& srcRect,
                        bool applyAlpha) {
            if (!IsValid() || !source.IsValid()) return;

            int32 srcX = srcRect.width  > 0 ? srcRect.left  : 0;
            int32 srcY = srcRect.height > 0 ? srcRect.top   : 0;
            int32 srcW = srcRect.width  > 0 ? srcRect.width  : (int32)source.mWidth;
            int32 srcH = srcRect.height > 0 ? srcRect.height : (int32)source.mHeight;

            // Clip to source bounds
            srcW = (srcX + srcW > (int32)source.mWidth)  ? (int32)source.mWidth  - srcX : srcW;
            srcH = (srcY + srcH > (int32)source.mHeight) ? (int32)source.mHeight - srcY : srcH;
            // Clip to dest bounds
            if ((int32)destX + srcW > (int32)mWidth)  srcW = (int32)mWidth  - (int32)destX;
            if ((int32)destY + srcH > (int32)mHeight) srcH = (int32)mHeight - (int32)destY;
            if (srcW <= 0 || srcH <= 0) return;

            for (int32 y = 0; y < srcH; ++y) {
                const uint8* src = source.mPixels.Data() + ((usize)(srcY+y)*source.mWidth + (usize)srcX)*4u;
                uint8*       dst = mPixels.Data()         + ((usize)(destY+y)*mWidth + (usize)destX)*4u;
                if (!applyAlpha) {
                    memcpy(dst, src, (usize)srcW * 4u);
                } else {
                    for (int32 x = 0; x < srcW; ++x) {
                        const uint32 srcA = src[x*4+3];
                        if (srcA == 255u) {
                            memcpy(dst + x*4, src + x*4, 4);
                        } else if (srcA > 0) {
                            const uint32 dstA = dst[x*4+3];
                            const uint32 inv  = 255u - srcA;
                            dst[x*4+0] = (uint8)((src[x*4+0]*srcA + dst[x*4+0]*inv + 127u) / 255u);
                            dst[x*4+1] = (uint8)((src[x*4+1]*srcA + dst[x*4+1]*inv + 127u) / 255u);
                            dst[x*4+2] = (uint8)((src[x*4+2]*srcA + dst[x*4+2]*inv + 127u) / 255u);
                            dst[x*4+3] = (uint8)(srcA + (dstA * inv + 127u) / 255u);
                        }
                    }
                }
            }
        }

        // =============================================================================
        void NkImage::CreateMaskFromColor(const NkColor2D& color, uint8 alpha) {
            if (!IsValid()) return;
            for (uint32 i = 0; i < mWidth * mHeight; ++i) {
                uint8* p = mPixels.Data() + i * 4u;
                if (p[0] == color.r && p[1] == color.g && p[2] == color.b)
                    p[3] = alpha;
            }
        }

    } // namespace renderer
} // namespace nkentseu