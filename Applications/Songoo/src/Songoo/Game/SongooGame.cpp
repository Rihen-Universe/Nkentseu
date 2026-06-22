// =============================================================================
// SongooGame.cpp — Songo'o Game Application
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 ─────────────────────────────────────────────────────────────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define SONGOO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define SONGOO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define SONGOO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define SONGOO_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(SONGOO_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/glx.h>)
#           include <glad/glx.h>
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

#include "SongooGame.h"
#include "Songoo/UI/Scenes/RihenIntroScene.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu { namespace songoo {

    // ── BuildContext ──────────────────────────────────────────────────────────
    AppContext SongooGame::BuildContext() {
        AppContext ctx;
        ctx.window       = &mWindow;
        ctx.renderer     = &mRenderer;
        ctx.font         = &mFont;
        ctx.settings     = &mSettings;
        ctx.scenes       = &mScenes;
        ctx.audio        = &mAudio;
        ctx.viewportW    = static_cast<int>(mViewportW);
        ctx.viewportH    = static_cast<int>(mViewportH);
        ctx.globalTime   = mTime;
        ctx.safe         = SafeArea::From(mWindow, mViewportW, mViewportH);
        ctx.quitRequested = &mQuit;
        return ctx;
    }

    // ── Init ──────────────────────────────────────────────────────────────────
    bool SongooGame::Init() {
        // Sur Android, les fichiers "Resources/Songoo/X" → assets/X
        NkFile::SetAndroidAssetSubFolder("Songoo");

        if (!mGL.Init(mWindow)) {
            logger.Error("[SongooGame] GL init failed");
            return false;
        }
        const auto sz = mWindow.GetSize();
        mViewportW = sz.x;
        mViewportH = sz.y;

        if (!mRenderer.Init()) {
            logger.Error("[SongooGame] Renderer init failed");
            return false;
        }
        if (!mFont.Init()) {
            logger.Error("[SongooGame] Font atlas init failed");
            return false;
        }

        // Audio — tolérant aux échecs
        if (!mAudio.Initialize())
            logger.Warn("[SongooGame] Audio disabled");

        // Scène de départ : intro Rihen (156 frames PNG)
        mScenes.Push(new RihenIntroScene());

        logger.Info("[SongooGame] Init OK — viewport {}x{}", mViewportW, mViewportH);
        return true;
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────
    void SongooGame::Shutdown() {
        AppContext ctx = BuildContext();
        mScenes.Clear(ctx);
        mAudio.Shutdown();
        mFont.Shutdown();
        mRenderer.Shutdown();
        mGL.Shutdown();
    }

    // ── OnResize ──────────────────────────────────────────────────────────────
    void SongooGame::OnResize(uint32 w, uint32 h) {
        mViewportW = w;
        mViewportH = h;
        mGL.OnResize(w, h);
        AppContext ctx = BuildContext();
        mScenes.OnResize(ctx, (int)w, (int)h);
    }

    // ── Update ────────────────────────────────────────────────────────────────
    void SongooGame::Update(float dt) {
        mTime += dt;
        AppContext ctx = BuildContext();
        mScenes.OnUpdate(ctx, dt);
    }

    // ── Render ────────────────────────────────────────────────────────────────
    void SongooGame::Render() {
        if (!mGL.BeginFrame()) return;
        AppContext ctx = BuildContext();
        mScenes.OnRender(ctx);
        mGL.EndFrame();
        mGL.Present();
    }

    // ── OnEvent ───────────────────────────────────────────────────────────────
    void SongooGame::OnEvent(NkEvent& ev) {
        AppContext ctx = BuildContext();
        mScenes.OnEvent(ctx, ev);
    }

    // ── Cycle mobile ──────────────────────────────────────────────────────────
    void SongooGame::OnPause() {
        AppContext ctx = BuildContext();
        mScenes.OnPause(ctx);
        mAudio.PauseBgMusic();
    }

    void SongooGame::OnResume() {
        AppContext ctx = BuildContext();
        mScenes.OnResume(ctx);
        mAudio.ResumeBgMusic();
    }

    bool SongooGame::RecreateSurface() {
        return mGL.RecreateSurface(mWindow);
    }

}} // namespace nkentseu::songoo
