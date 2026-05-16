// =============================================================================
// PongApp.cpp — implementation PoC (etape 1)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// GLAD2 (mêmes inclusions que GLContext.cpp pour avoir glClear/glViewport ici)
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
#else
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <GL/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <GLES3/gl3.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool
#endif

#include "PongApp.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu { 
    namespace pong {

        bool PongApp::Init() {
            if (!mGL.Init(mWindow)) {
                logger.Error("[PongApp] GL init failed");
                return false;
            }
            auto sz = mWindow.GetSize();
            mViewportW = sz.x;
            mViewportH = sz.y;

            mState = GameState::SplashScreen;
            mSplashTimer = 2.4f;
            logger.Info("[PongApp] PoC ready - state=Splash");
            return true;
        }

        void PongApp::Shutdown() {
            mGL.Shutdown();
        }

        void PongApp::OnResize(uint32 w, uint32 h) {
            mViewportW = w;
            mViewportH = h;
            mGL.OnResize(w, h);
        }

        void PongApp::Update(float dt) {
            mTime += dt;
            switch (mState) {
                case GameState::SplashScreen:
                    mSplashTimer -= dt;
                    if (mSplashTimer <= 0.0f) {
                        mState = GameState::MainMenu;
                        logger.Info("[PongApp] → MainMenu");
                    }
                    break;
                default:
                    break;
            }
        }

        void PongApp::RenderClear(float r, float g, float b) {
            glViewport(0, 0, static_cast<int>(mViewportW), static_cast<int>(mViewportH));
            glClearColor(r, g, b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        void PongApp::Render() {
            if (!mGL.BeginFrame()) return;

            // PoC : couleur de fond change subtilement selon l'etat pour valider que tout
            // s'affiche bien sur Windows et Android.
            // Palette neon GDD : dark = #050A14 → (0.020, 0.039, 0.078)
            float r = 0.020f, g = 0.039f, b = 0.078f;
            switch (mState) {
                case GameState::SplashScreen: {
                    // Pulse cyan subtil
                    float pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.4f);
                    g += pulse * 0.04f;
                    b += pulse * 0.06f;
                    break;
                }
                case GameState::MainMenu:
                    // Fond bleu nuit
                    break;
                default:
                    break;
            }
            RenderClear(r, g, b);

            mGL.EndFrame();
            mGL.Present();
        }

    }
} // namespace nkentseu::pong
