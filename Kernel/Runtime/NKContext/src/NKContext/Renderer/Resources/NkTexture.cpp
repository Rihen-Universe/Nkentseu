// =============================================================================
// NkTexture.cpp — GPU texture lifecycle (backend dispatch)
// The renderer backend sets the GPU ID / handle after creation.
// NkTexture only manages the CPU-side metadata; the actual GPU object
// is owned by the backend that created it.
// =============================================================================
#include "NkTexture.h"
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKLogger/NkLog.h"
#include <cstring>

// Forward-declare backend static helpers to avoid circular includes
namespace nkentseu { 
    namespace renderer {

        // Each backend registers static helpers when it initializes:
        // These are resolved at link time — the backend .cpp defines them.
        // We use a simple dispatch table so NkTexture stays backend-agnostic.

        struct NkTextureBackend {
            uint32 (*Create)(uint32 w, uint32 h, const uint8* rgba)       = nullptr;
            void   (*Update)(uint32 id, uint32 x, uint32 y, uint32 w,
                            uint32 h, const uint8* rgba)                  = nullptr;
            void   (*Destroy)(uint32 id)                                   = nullptr;
            void   (*SetFilter)(uint32 id, NkTextureFilter f)              = nullptr;
            void   (*SetWrap)  (uint32 id, NkTextureWrap w)               = nullptr;
        };

        // Active backend dispatch (set by NkIRenderer2D::Initialize via
        // NkTexture::SetBackend()):
        static NkTextureBackend gTextureBackend{};

        void NkTexture_SetBackend(const NkTextureBackend& backend) {
            gTextureBackend = backend;
        }

        // White texture singleton
        NkTexture* NkTexture::sWhiteTexture = nullptr;

        // =============================================================================
        NkTexture::NkTexture(NkTexture&& other) noexcept
            : mWidth(other.mWidth), mHeight(other.mHeight)
            , mHandle(other.mHandle), mGPUId(other.mGPUId)
            , mFilter(other.mFilter), mWrap(other.mWrap)
            , mRenderer(other.mRenderer)
        {
            other.mHandle   = nullptr;
            other.mGPUId    = 0;
            other.mRenderer = nullptr;
        }

        NkTexture& NkTexture::operator=(NkTexture&& other) noexcept {
            if (this != &other) {
                Destroy();
                mWidth    = other.mWidth;
                mHeight   = other.mHeight;
                mHandle   = other.mHandle;
                mGPUId    = other.mGPUId;
                mFilter   = other.mFilter;
                mWrap     = other.mWrap;
                mRenderer = other.mRenderer;
                other.mHandle   = nullptr;
                other.mGPUId    = 0;
                other.mRenderer = nullptr;
            }
            return *this;
        }

        // =============================================================================
        void NkTexture::Destroy() {
            mCPUPixels.Clear();
            if (mGPUId && gTextureBackend.Destroy) {
                gTextureBackend.Destroy(mGPUId);
                mGPUId = 0;
            }
            mHandle   = nullptr;
            mWidth    = 0;
            mHeight   = 0;
            mRenderer = nullptr;
        }

        // =============================================================================
        bool NkTexture::Create(NkIRenderer2D& renderer, uint32 width, uint32 height,
                            const NkColor2D& fillColor) {
            if (width == 0 || height == 0) return false;
            NkImage img;
            if (!img.Create(width, height, fillColor)) return false;
            return LoadFromImage(renderer, img);
        }

        // =============================================================================
        bool NkTexture::LoadFromFile(NkIRenderer2D& renderer, const char* path) {
            NkImage img;
            if (!img.LoadFromFile(path)) return false;
            return LoadFromImage(renderer, img);
        }

        // =============================================================================
        bool NkTexture::LoadFromImage(NkIRenderer2D& renderer, const NkImage& image,
                                    const NkRect2i& area) {
            if (!image.IsValid()) return false;
            mRenderer = &renderer;

            {
                const uint32 w = image.GetWidth();
                const uint32 h = image.GetHeight();
                const uint8* src = image.GetPixels();
                if (src && w > 0 && h > 0) {
                    const usize byteCount = (usize)w * h * 4;
                    // mCPUPixels.Assign(0, (NkVector<uint8>::SizeType)byteCount);
                    mCPUPixels.Resize(byteCount, 0);
                    memcpy(mCPUPixels.Data(), src, byteCount);
                }
            }

            // If area is given, extract sub-region
            if (area.width > 0 && area.height > 0) {
                NkImage sub;
                sub.Create((uint32)area.width, (uint32)area.height);
                sub.Copy(image, 0, 0, area, false);
                return LoadFromImage(renderer, sub);
            }

            Destroy();
            mWidth  = image.GetWidth();
            mHeight = image.GetHeight();

            if (!gTextureBackend.Create) {
                // No backend registered yet — store as placeholder
                logger.Warnf("[NkTexture] No backend registered, texture will be invalid for rendering");
                return false;
            }
            mGPUId = gTextureBackend.Create(mWidth, mHeight, image.GetPixels());
            if (!mGPUId) {
                logger.Errorf("[NkTexture] Backend Create returned 0");
                return false;
            }
            return true;
        }

        // =============================================================================
        bool NkTexture::LoadFromMemory(NkIRenderer2D& renderer, const void* data, usize sizeBytes) {
            NkImage img;
            if (!img.LoadFromMemory(data, sizeBytes)) return false;
            return LoadFromImage(renderer, img);
        }

        // =============================================================================
        bool NkTexture::Update(const uint8* pixels, uint32 width, uint32 height,
                            uint32 destX, uint32 destY) {
            if (!IsValid() || !pixels || !gTextureBackend.Update) return false;

            if (!mCPUPixels.Empty() && destX == 0 && destY == 0
                && width == mWidth && height == mHeight) {
                memcpy(mCPUPixels.Data(), pixels, (usize)width * height * 4);
            }

            gTextureBackend.Update(mGPUId, destX, destY, width, height, pixels);
            return true;
        }

        bool NkTexture::Update(const NkImage& image, uint32 destX, uint32 destY) {
            if (!image.IsValid()) return false;
            return Update(image.GetPixels(), image.GetWidth(), image.GetHeight(), destX, destY);
        }

        // =============================================================================
        NkImage NkTexture::CopyToImage() const {
            // GPU readback — not implemented in the base layer.
            // Backends can override this if they need it.
            logger.Warnf("[NkTexture] CopyToImage not implemented for this backend");
            return {};
        }

        // =============================================================================
        void NkTexture::SetFilter(NkTextureFilter filter) {
            mFilter = filter;
            if (mGPUId && gTextureBackend.SetFilter)
                gTextureBackend.SetFilter(mGPUId, filter);
        }

        void NkTexture::SetWrap(NkTextureWrap wrap) {
            mWrap = wrap;
            if (mGPUId && gTextureBackend.SetWrap)
                gTextureBackend.SetWrap(mGPUId, wrap);
        }

        void NkTexture::GenerateMipmap() {
            // Backend-specific — no-op in the base layer
        }

        // =============================================================================
        NkRect2f NkTexture::GetTexCoords(const NkRect2i& rect) const {
            if (mWidth == 0 || mHeight == 0) return {0,0,1,1};

            if (rect.width == 0 && rect.height == 0) return {0,0,1,1};

            return {
                (float32)rect.left   / (float32)mWidth,
                (float32)rect.top    / (float32)mHeight,
                (float32)rect.width  / (float32)mWidth,
                (float32)rect.height / (float32)mHeight,
            };
        }

        // =============================================================================
        NkTexture* NkTexture::GetWhiteTexture(NkIRenderer2D& renderer) {
            // The white texture is owned by the backend (NkOpenGLRenderer2D etc.)
            // and accessed via a static singleton per renderer.
            // For now return nullptr — the batch renderer substitutes mWhiteTexId directly.
            (void)renderer;
            return nullptr;
        }

        // =============================================================================
        // Backend registration helper (called from NkOpenGLRenderer2D::Initialize etc.)
        void NkTexture_RegisterOpenGLBackend();   // defined in NkOpenGLRenderer2D.cpp
        void NkTexture_RegisterDX11Backend();     // defined in NkDX11Renderer2D.cpp
        void NkTexture_RegisterDX12Backend();     // defined in NkDX12Renderer2D.cpp
        void NkTexture_RegisterVulkanBackend();   // defined in NkVulkanRenderer2D.cpp

    }
} // namespace nkentseu::renderer