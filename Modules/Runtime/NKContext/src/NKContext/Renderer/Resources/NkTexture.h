#pragma once
// =============================================================================
// NkTexture.h — GPU texture resource (backend-agnostic handle)
// Similar to sf::Texture — wraps a GPU texture object.
// Create from NkImage or from file (loads → NkImage → uploads to GPU).
//
// Usage:
//   NkTexture tex;
//   tex.LoadFromFile(renderer, "assets/tileset.png");
//   tex.SetSmooth(true);
//   tex.SetRepeated(true);
//   NkSprite sprite(tex);
// =============================================================================
#include "NkImage.h"
#include "NKContext/Renderer/Core/NkRenderer2DTypes.h"

namespace nkentseu {
    namespace renderer {

        class NkIRenderer2D;

        // Texture filtering modes
        enum class NkTextureFilter : uint8 {
            NK_NEAREST,   // pixel-art, sharp
            NK_LINEAR,    // smooth interpolation
        };

        // Texture wrapping modes
        enum class NkTextureWrap : uint8 {
            NK_CLAMP,
            NK_REPEAT,
            NK_MIRROR_REPEAT,
        };

        // =========================================================================
        class NkTexture {
            public:
                NkTexture()  = default;
                ~NkTexture() { Destroy(); }

                // Non-copyable (GPU resource ownership)
                NkTexture(const NkTexture&)            = delete;
                NkTexture& operator=(const NkTexture&) = delete;

                // Movable
                NkTexture(NkTexture&& other) noexcept;
                NkTexture& operator=(NkTexture&& other) noexcept;

                // ── Create / load ─────────────────────────────────────────────────────
                bool Create(NkIRenderer2D& renderer, uint32 width, uint32 height,
                            const NkColor2D& fillColor = NkColor2D::Transparent());

                bool LoadFromFile  (NkIRenderer2D& renderer, const char* path);
                bool LoadFromImage (NkIRenderer2D& renderer, const NkImage& image,
                                    const NkRect2i& area = NkRect2i{});
                bool LoadFromMemory(NkIRenderer2D& renderer,
                                    const void* data, usize sizeBytes);

                // Update sub-region (fast path — no full re-upload)
                bool Update(const uint8* pixels, uint32 width, uint32 height,
                            uint32 destX, uint32 destY);
                bool Update(const NkImage& image, uint32 destX = 0, uint32 destY = 0);

                // Download back to CPU (slow)
                NkImage CopyToImage() const;

                // ── Params ────────────────────────────────────────────────────────────
                void SetFilter (NkTextureFilter filter);
                void SetWrap   (NkTextureWrap wrap);
                void GenerateMipmap();

                NkTextureFilter GetFilter() const { return mFilter; }
                NkTextureWrap   GetWrap()   const { return mWrap; }

                // ── Info ──────────────────────────────────────────────────────────────
                uint32 GetWidth()  const { return mWidth; }
                uint32 GetHeight() const { return mHeight; }
                bool   IsValid()   const { return mHandle != nullptr || mGPUId != 0; }
                NkVec2f GetSize()  const { return {(float)mWidth, (float)mHeight}; }

                // Normalized UV rect for sub-texture
                NkRect2f GetTexCoords(const NkRect2i& rect) const;

                // ── Native handle ─────────────────────────────────────────────────────
                // Internal — used by backend renderers to bind the texture.
                // Cast to the appropriate backend type.
                void*    GetHandle() const { return mHandle; }
                uint32   GetGPUId()  const { return mGPUId; }   // OpenGL texture ID
                void     SetHandle(void* h)  { mHandle = h; }
                void     SetGPUId(uint32 id) { mGPUId = id; }

                // ── White 1x1 texture ─────────────────────────────────────────────────
                // Used internally to draw untextured geometry through the same shader path.
                static NkTexture* GetWhiteTexture(NkIRenderer2D& renderer);

                void Destroy();
                const uint8* GetCPUPixels() const { return mCPUPixels.Empty() ? nullptr : mCPUPixels.Data(); }
                bool         HasCPUPixels() const { return !mCPUPixels.Empty(); }
            private:

                uint32          mWidth  = 0;
                uint32          mHeight = 0;
                void*           mHandle = nullptr;  // backend-specific (VkImage, ID3D11Texture2D*, etc.)
                uint32          mGPUId  = 0;        // OpenGL name
                NkTextureFilter mFilter = NkTextureFilter::NK_LINEAR;
                NkTextureWrap   mWrap   = NkTextureWrap::NK_CLAMP;

                // Owning renderer — needed for lazy updates
                NkIRenderer2D*  mRenderer = nullptr;
                NkVector<uint8> mCPUPixels;

                // White texture singleton per renderer context
                static NkTexture* sWhiteTexture;
        };

    } // namespace renderer
} // namespace nkentseu