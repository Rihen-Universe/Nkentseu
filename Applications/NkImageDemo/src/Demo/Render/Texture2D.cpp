// =============================================================================
// Texture2D.cpp
// -----------------------------------------------------------------------------
// Implementation : charge un fichier image via NkImage::Load(), convertit en
// RGBA32 si necessaire, puis upload en texture GL_RGBA8 avec mipmaps.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : avant les autres includes pour eviter conflit gl.h ───────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(PONG_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if defined(__has_include)
#           if __has_include(<glad/glx.h>)
#               include <glad/glx.h>
#           endif
#       endif
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <glad/gles2.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <glad/gles2.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool
#endif

#include "Texture2D.h"
#include "NKImage/Core/NkImage.h"
#include "NKLogger/NkLog.h"

namespace nkentseu
{
    namespace demo
    {

        Texture2D::~Texture2D()
        {
            Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        // LoadFromFile
        // Charge l'image via NkImage::Load(path, 4) — desiredChannels=4 force
        // une conversion en RGBA32 cote NKImage. Pattern identique a
        // NkRHIDemoFullImage : on garantit ainsi que le buffer recu est de
        // taille (W * H * 4) octets, ce qui matche exactement l'upload
        // glTexImage2D(GL_RGBA, GL_UNSIGNED_BYTE). Sans la conversion, un PNG
        // RGB (3 canaux) crash l'upload car GL lit 4 octets/pixel.
        // ─────────────────────────────────────────────────────────────────────
        // Decode UNIQUEMENT (pas d'appel GL). Safe depuis un worker thread.
        NkImage* Texture2D::DecodeFromFile(const char* path)
        {
            // 4 = force RGBA32. NkImage convertit en interne si la source est
            // RGB / GRAY / GRAY+A.
            NkImage* img = NkImage::Load(path, /*desiredChannels=*/4);
            if (img == nullptr || !img->IsValid())
            {
                logger.Error("[Texture2D] DecodeFromFile failed: {0}", path);
                // Free libere les pixels + wrapper (alloue nkMalloc+placement
                // new). JAMAIS `delete img` — double-free.
                if (img != nullptr) img->Free();
                return nullptr;
            }
            return img;
        }

        // Upload sur le thread GL d'une image deja decodee. Libere l'image
        // apres upload.
        bool Texture2D::UploadFromImage(NkImage* img)
        {
            if (img == nullptr || !img->IsValid())
            {
                if (img != nullptr) img->Free();
                return false;
            }
            mWidth  = img->Width();
            mHeight = img->Height();

            // Generation + upload de la texture GL.
            glGenTextures(1, &mTextureId);
            glBindTexture(GL_TEXTURE_2D, mTextureId);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            const int stridePixels = img->Stride() / 4;  // bytes -> pixels (RGBA = 4 bpp)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, stridePixels);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                         mWidth, mHeight, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, img->Pixels());
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            // Mipmaps pour le downscaling propre.
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            img->Free();
            return true;
        }

        bool Texture2D::LoadFromFile(const char* path)
        {
            NkImage* img = DecodeFromFile(path);
            if (img == nullptr) return false;
            const bool ok = UploadFromImage(img);
            if (ok)
            {
                logger.Info("[Texture2D] loaded {0} : {1}x{2}, texId={3}",
                            path, mWidth, mHeight, mTextureId);
            }
            return ok;
        }

        // ─────────────────────────────────────────────────────────────────────
        void Texture2D::Shutdown()
        {
            if (mTextureId != 0)
            {
                glDeleteTextures(1, &mTextureId);
                mTextureId = 0;
            }
            mWidth  = 0;
            mHeight = 0;
        }

    } // namespace demo
} // namespace nkentseu
