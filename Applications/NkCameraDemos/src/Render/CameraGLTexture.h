#pragma once
// =============================================================================
// CameraGLTexture.h
// -----------------------------------------------------------------------------
// Texture GPU dynamique pour flux camera. Allocation immutable en GL_RGBA8,
// upload par frame via glTexSubImage2D. Pas de mipmaps (live texture).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace cameradem
    {

        // ─────────────────────────────────────────────────────────────────────
        // Texture 2D RGBA8 destinee a etre mise a jour chaque frame avec les
        // pixels d'une NkCameraFrame deja convertie en RGBA8.
        // ─────────────────────────────────────────────────────────────────────
        class CameraGLTexture
        {
        public:
            CameraGLTexture()  = default;
            ~CameraGLTexture();

            // Alloue la texture GL_RGBA8 sans uploader de pixels.
            // Retourne false si glGenTextures echoue ou dimensions invalides.
            bool Create(uint32 w, uint32 h);

            // Upload la totalite de la texture avec un buffer RGBA8 contigu
            // (w * h * 4 octets). Retourne false si rgba == nullptr ou
            // texture non initialisee.
            bool Update(const uint8* rgba);

            // Libere la texture GL (glDeleteTextures).
            void Shutdown();

            // Accesseurs
            uint32 Id()      const noexcept { return mTextureId; }
            uint32 Width()   const noexcept { return mWidth; }
            uint32 Height()  const noexcept { return mHeight; }
            bool   IsValid() const noexcept { return mTextureId != 0; }

        private:
            uint32 mTextureId = 0;
            uint32 mWidth     = 0;
            uint32 mHeight    = 0;
        };

    } // namespace cameradem
} // namespace nkentseu
