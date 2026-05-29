// =============================================================================
// CameraGLTexture.cpp
// -----------------------------------------------------------------------------
// Implementation : glGenTextures + glTexImage2D(GL_RGBA8) en Create,
// glTexSubImage2D en Update. Linear + clamp-to-edge.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : avant les autres includes pour eviter conflit gl.h ───────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(NKCAMDEM_HAS_GLAD)
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

#include "CameraGLTexture.h"
#include "NKLogger/NkLog.h"

namespace nkentseu
{
    namespace cameradem
    {

        CameraGLTexture::~CameraGLTexture()
        {
            Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        bool CameraGLTexture::Create(uint32 w, uint32 h)
        {
            if (w == 0 || h == 0)
            {
                logger.Error("[CameraGLTexture] Create : dimensions invalides");
                return false;
            }
#if defined(NKCAMDEM_HAS_GLAD)
            // Nettoyage prealable si on recree par-dessus une texture existante.
            if (mTextureId != 0)
            {
                glDeleteTextures(1, &mTextureId);
                mTextureId = 0;
            }

            glGenTextures(1, &mTextureId);
            if (mTextureId == 0)
            {
                logger.Error("[CameraGLTexture] glGenTextures a retourne 0");
                return false;
            }

            glBindTexture(GL_TEXTURE_2D, mTextureId);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            // Allocation immutable, pas de pixels initiaux (live texture).
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                         (int)w, (int)h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            // Pas de mipmaps : la texture change a chaque frame.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

            glBindTexture(GL_TEXTURE_2D, 0);

            mWidth  = w;
            mHeight = h;
            return true;
#else
            (void)w; (void)h;
            logger.Warn("[CameraGLTexture] glad non disponible : texture factice");
            return false;
#endif
        }

        // ─────────────────────────────────────────────────────────────────────
        bool CameraGLTexture::Update(const uint8* rgba)
        {
#if defined(NKCAMDEM_HAS_GLAD)
            if (mTextureId == 0 || rgba == nullptr) return false;
            glBindTexture(GL_TEXTURE_2D, mTextureId);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, 0,
                            (int)mWidth, (int)mHeight,
                            GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            glBindTexture(GL_TEXTURE_2D, 0);
            return true;
#else
            (void)rgba;
            return false;
#endif
        }

        // ─────────────────────────────────────────────────────────────────────
        void CameraGLTexture::Shutdown()
        {
#if defined(NKCAMDEM_HAS_GLAD)
            if (mTextureId != 0)
            {
                glDeleteTextures(1, &mTextureId);
            }
#endif
            mTextureId = 0;
            mWidth     = 0;
            mHeight    = 0;
        }

    } // namespace cameradem
} // namespace nkentseu
