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
    namespace pong
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
        bool Texture2D::LoadFromFile(const char* path)
        {
            // 4 = force RGBA32. NkImage convertit en interne si la source est
            // RGB / GRAY / GRAY+A. C'est la meme convention que celle
            // utilisee par NkRHIDemoFullImage.cpp (Sandbox/Base03).
            NkImage* img = NkImage::Load(path, /*desiredChannels=*/4);
            if (img == nullptr || !img->IsValid())
            {
                logger.Error("[Texture2D] LoadFromFile failed: {0}", path);
                // ATTENTION : NkImage est alloue via nkMalloc + placement new.
                // Free() libere les pixels ET le wrapper (nkFree(this)). On
                // ne doit JAMAIS faire `delete img` — provoque un double-free
                // (heap corruption c0000374) car operator delete tente de
                // re-liberer la memoire deja nkFree'd.
                if (img != nullptr) img->Free();
                return false;
            }

            mWidth  = img->Width();
            mHeight = img->Height();

            // Generation + upload de la texture GL.
            glGenTextures(1, &mTextureId);
            glBindTexture(GL_TEXTURE_2D, mTextureId);
            // ROW_LENGTH explicite : NkImage peut avoir un stride > width*4
            // (alignement interne). On informe GL via UNPACK_ROW_LENGTH pour
            // qu'il sache combien d'octets sauter par ligne. UNPACK_ALIGNMENT=1
            // evite tout padding implicite.
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

            // NkImage : Free() libere a la fois les pixels et le wrapper
            // (alloue via nkMalloc + placement new par Load/Alloc/Wrap).
            // Ne JAMAIS faire `delete img` apres — double-free.
            img->Free();

            logger.Info("[Texture2D] loaded {0} : {1}x{2}, texId={3}",
                        path, mWidth, mHeight, mTextureId);
            return true;
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

    } // namespace pong
} // namespace nkentseu
