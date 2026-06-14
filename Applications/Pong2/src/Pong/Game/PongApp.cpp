// =============================================================================
// PongApp.cpp
// -----------------------------------------------------------------------------
// Implementation : push RihenIntroScene au demarrage, le SceneManager se
// charge ensuite des transitions Rihen -> Noge -> Splash -> MainMenu.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 ────────────────────────────────────────────────────────────────────
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

#include "PongApp.h"
#include "Pong/UI/Scenes/RihenIntroScene.h"
#include "Pong/UI/Scenes/MainMenuScene.h"
#include "Pong/Net/AfricaPlaces.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include <cstdio>

namespace nkentseu
{
    namespace pong
    {

        // ─────────────────────────────────────────────────────────────────────
        // BuildContext — assemble un AppContext frais (recalcule SafeArea).
        // ─────────────────────────────────────────────────────────────────────
        AppContext PongApp::BuildContext()
        {
            AppContext ctx;
            ctx.window     = &mWindow;
            ctx.renderer   = &mRenderer;
            ctx.font       = &mFont;
            ctx.settings   = &mSettings;
            ctx.scenes     = &mScenes;
            ctx.network    = &mNetwork;
            ctx.discovery  = &mDiscovery;
            ctx.audio      = &mAudio;
            ctx.viewportW  = static_cast<int>(mViewportW);
            ctx.viewportH  = static_cast<int>(mViewportH);
            ctx.globalTime    = mTime;
            ctx.safe          = SafeArea::From(mWindow, mViewportW, mViewportH);
            ctx.quitRequested = &mQuit;
            // Copie l'identite Pays/Ville-Code pour que les scenes y aient acces.
            std::snprintf(ctx.myCountry, sizeof(ctx.myCountry), "%s", mMyCountry);
            std::snprintf(ctx.myCity,    sizeof(ctx.myCity),    "%s", mMyCity);
            std::snprintf(ctx.myCode,    sizeof(ctx.myCode),    "%s", mMyCode);
            return ctx;
        }

        // ─────────────────────────────────────────────────────────────────────
        bool PongApp::Init()
        {
            // Sur Android, les fichiers C++ "Resources/Pong/X" sont en realite
            // empaquetes dans assets/X par Jenga. On indique a NkFile le
            // sous-dossier app a stripper pour la resolution AAssetManager.
            // No-op hors Android.
            NkFile::SetAndroidAssetSubFolder("Pong");

            if (!mGL.Init(mWindow))
            {
                logger.Error("[PongApp] GL init failed");
                return false;
            }
            const auto sz = mWindow.GetSize();
            mViewportW = sz.x;
            mViewportH = sz.y;

            if (!mRenderer.Init())
            {
                logger.Error("[PongApp] Renderer init failed");
                return false;
            }
            if (!mFont.Init())
            {
                logger.Error("[PongApp] Font atlas init failed");
                return false;
            }

            // Init reseau (sockets plateforme). Idempotent — peut etre rappele.
            NetworkSession::PlatformInit();

            // Init audio (NKAudio + generation procedural des SFX). Si echec,
            // l'app continue mais sans son (mAudio.IsInitialized() == false).
            if (!mAudio.Initialize())
            {
                logger.Warn("[PongApp] Audio init FAILED - le jeu tournera sans son");
            }

            // Genere l'identifiant Pays/Ville-Code aleatoire pour cette
            // session. Affiche dans le NetworkLobbyScene comme carte de
            // visite, envoye via PktHello a la connexion. Code 9 chiffres
            // ajoute pour reduire le risque de collision a quasi-zero.
            africa::PickRandomCountryCityCode(mMyCountry, sizeof(mMyCountry),
                                              mMyCity,    sizeof(mMyCity),
                                              mMyCode,    sizeof(mMyCode));
            logger.Info("[PongApp] Identite reseau : {0}/{1}-{2}",
                        mMyCountry, mMyCity, mMyCode);
            // Injecte cette identite dans la session reseau : sera envoyee
            // automatiquement au pair via PktHello des Connected.
            mNetwork.SetLocalIdentity(mMyCountry, mMyCity, mMyCode);

            // Premiere scene : intro Rihen (qui chaine vers Noge puis Splash).
            mScenes.Push(new RihenIntroScene());

            logger.Info("[PongApp] Init OK");
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::Shutdown()
        {
            // Vider la pile de scenes proprement (OnExit).
            AppContext ctx = BuildContext();
            mScenes.Clear(ctx);

            // Ferme la session reseau et libere les sockets plateforme.
            mNetwork.Shutdown();
            NetworkSession::PlatformShutdown();

            // Stop AudioEngine + free samples.
            mAudio.Shutdown();

            mFont.Shutdown();
            mRenderer.Shutdown();
            mGL.Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::OnResize(uint32 w, uint32 h)
        {
            mViewportW = w;
            mViewportH = h;
            mGL.OnResize(w, h);
            AppContext ctx = BuildContext();
            mScenes.Resize(ctx, (int)w, (int)h);
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::Update(float dt)
        {
            mTime += dt;
            AppContext ctx = BuildContext();
            // Tick reseau (drain interne du thread reseau). Aucun cout si
            // la session est Idle.
            mNetwork.Tick(dt);
            // Tick decouverte LAN : emet beacon (host) + drain scan (client).
            // No-op tant que les sockets internes n'ont pas ete demarres
            // par NetworkLobbyScene.
            mDiscovery.Tick(dt);
            // Applique les transitions push/replace/pop en attente.
            mScenes.ApplyPending(ctx);
            // Update la scene active.
            mScenes.Update(ctx, dt);
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::OnEvent(NkEvent& ev)
        {
            AppContext ctx = BuildContext();
            mScenes.Event(ctx, ev);
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::OnPause()
        {
            AppContext ctx = BuildContext();
            mScenes.Pause(ctx);
        }

        void PongApp::OnResume()
        {
            AppContext ctx = BuildContext();
            mScenes.Resume(ctx);
        }

        bool PongApp::RecreateSurface()
        {
            return mGL.RecreateSurface(mWindow);
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::Render()
        {
            if (!mGL.BeginFrame()) return;
            AppContext ctx = BuildContext();
            mScenes.Render(ctx);
            mGL.EndFrame();
            mGL.Present();
        }

        // ─────────────────────────────────────────────────────────────────────
        // State helpers (utilises par Apps.cpp pour l'auto-pause au focus lost).
        // Pour l'instant on n'a pas encore PlayingScene/PausedScene, on retourne
        // SplashScreen par defaut. Sera affine quand on ajoutera GameplayScene.
        // ─────────────────────────────────────────────────────────────────────
        GameState PongApp::State() const noexcept
        {
            return GameState::SplashScreen;
        }
        void PongApp::SetState(GameState /*s*/) noexcept
        {
            // No-op pour l'instant — sera mappe au SceneManager.
        }

    } // namespace pong
} // namespace nkentseu
