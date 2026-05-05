#pragma once
// =============================================================================
// NkImage.h — CPU-side pixel buffer (load/save via stb_image / stb_image_write)
// Similar to sf::Image — stores raw RGBA pixels, no GPU resource.
// Use NkTexture to upload to the GPU for rendering.
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContext/Renderer/Core/NkRenderer2DTypes.h"

namespace nkentseu {
    namespace renderer {

        class NkImage {
            public:
                NkImage()  = default;
                ~NkImage() = default;

                // ── Create ────────────────────────────────────────────────────────────
                // Create blank image filled with color
                bool Create(uint32 width, uint32 height,
                            const NkColor2D& fillColor = NkColor2D::Transparent());

                // Load from file (PNG, JPG, BMP, TGA, GIF via stb_image)
                bool LoadFromFile(const char* path);

                // Load from memory buffer
                bool LoadFromMemory(const void* data, usize sizeBytes);

                // Load from raw RGBA pixel array
                bool LoadFromPixels(const uint8* pixels, uint32 width, uint32 height);

                // ── Save ──────────────────────────────────────────────────────────────
                // Save to PNG file
                bool SaveToFile(const char* path) const;

                // ── Access ────────────────────────────────────────────────────────────
                uint32 GetWidth()  const { return mWidth; }
                uint32 GetHeight() const { return mHeight; }
                bool   IsValid()   const { return mWidth > 0 && mHeight > 0 && !mPixels.Empty(); }

                // Raw RGBA pixels (width * height * 4 bytes)
                const uint8* GetPixels() const { return mPixels.Data(); }
                uint8*       GetPixels()       { return mPixels.Data(); }

                NkColor2D GetPixel(uint32 x, uint32 y) const;
                void      SetPixel(uint32 x, uint32 y, const NkColor2D& color);

                // Flip vertically (needed for OpenGL which has Y-up)
                void FlipVertically();

                // Basic operations
                void Copy(const NkImage& source,
                        uint32 destX, uint32 destY,
                        const NkRect2i& srcRect = NkRect2i{},
                        bool applyAlpha = false);

                // Mask: replace a specific color with transparent
                void CreateMaskFromColor(const NkColor2D& color, uint8 alpha = 0);

            private:
                uint32           mWidth  = 0;
                uint32           mHeight = 0;
                NkVector<uint8>  mPixels;        // RGBA, row-major
        };

    } // namespace renderer
} // namespace nkentseu