#pragma once
// =============================================================================
// Texture2D.h
// -----------------------------------------------------------------------------
// Texture GPU 2D chargee depuis un fichier image via NKImage.
// Stockage interne : GL_RGBA8, mipmaps generes automatiquement.
// Usage : LoadFromFile("Resources/Pong/Textures/logo.png") puis Bind().
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace pong
    {

        class Texture2D
        {
        public:
            Texture2D()  = default;
            ~Texture2D();

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Charge un fichier image (PNG/JPG/etc.) via NkImage puis upload
            /// en texture GL_RGBA8. Genere les mipmaps.
            bool LoadFromFile(const char* path);

            /// Libere la texture GL et l'image source.
            void Shutdown();

            // ── Accesseurs ───────────────────────────────────────────────────
            uint32 Id()       const noexcept { return mTextureId; }
            int    Width()    const noexcept { return mWidth; }
            int    Height()   const noexcept { return mHeight; }
            bool   IsValid()  const noexcept { return mTextureId != 0; }
            float  AspectRatio() const noexcept
            {
                return (mHeight > 0) ? (float)mWidth / (float)mHeight : 1.0f;
            }

        private:
            uint32 mTextureId = 0;
            int    mWidth     = 0;
            int    mHeight    = 0;
        };

    } // namespace pong
} // namespace nkentseu
