// =============================================================================
// PongApp.cpp
// -----------------------------------------------------------------------------
// Implementation : push RihenIntroScene au demarrage, le SceneManager se
// charge ensuite des transitions Rihen -> Noge -> Splash -> MainMenu.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#include "PongApp.h"
#include "Pong/PongConfig.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/Scenes/RihenIntroScene.h"
#include "Pong/UI/Scenes/MainMenuScene.h"
#include "Pong/Net/AfricaPlaces.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"

// Cible de rendu NKCanvas (remplace l'ancien GLContext + GLRenderer2D raw-GL).
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"

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
            ctx.renderer   = (mTarget != nullptr) ? &mTarget->GetRenderer2D() : nullptr;
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

            // Backend graphique choisi dynamiquement via pong.config
            // (opengl | vulkan | dx11 | dx12 | software | auto). Permet de
            // valider chaque backend NKCanvas sans recompiler.
            const PongConfig pcfg = PongConfig::LoadOrCreateDefault();
            NkContextDesc desc;
            desc.api = (pcfg.backend == NkGraphicsApi::NK_GFX_API_AUTO)
                     ? PongConfig::PickBestForPlatform()
                     : pcfg.backend;
            logger.Info("[PongApp] Backend graphique : {0}",
                        PongConfig::BackendName(desc.api));

            // Validation GPU optionnelle (pong.config: debug=1) : couche debug
            // DX11/DX12 + validation layers Vulkan. Les messages sont forwardes
            // vers NkLogger (utile pour diagnostiquer un backend qui n'affiche
            // rien). Off par defaut (overhead). Le check D3D12SDKLayers.dll cote
            // contexte degrade proprement si la feature Graphics Tools manque.
            if (pcfg.debug)
            {
                desc.dx11.debugDevice        = true;
                desc.dx12.debugDevice        = true;
                desc.vulkan.validationLayers = true;
                desc.vulkan.debugMessenger   = true;
                logger.Warn("[PongApp] VALIDATION GPU ACTIVE (pong.config debug=1) - backend {0}",
                            PongConfig::BackendName(desc.api));
            }

            // Cree le contexte GPU + le renderer 2D NKCanvas sur la fenetre.
            // (Remplace l'ancien duo GLContext + GLRenderer2D raw-GL.)
            mTarget = new renderer::NkRenderWindow(mWindow, desc);
            if (mTarget == nullptr || !mTarget->IsValid())
            {
                logger.Error("[PongApp] NkRenderWindow init FAILED (backend {0})",
                             PongConfig::BackendName(desc.api));
                return false;
            }

            const auto sz = mWindow.GetSize();
            mViewportW = sz.x;
            mViewportH = sz.y;

            // Atlas de polices : rasterise via NKFont puis upload en NkTexture
            // (NKCanvas) — a besoin du renderer pour creer la texture GPU.
            if (!mFont.Init(mTarget->GetRenderer2D()))
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
            delete mTarget;
            mTarget = nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::OnResize(uint32 w, uint32 h)
        {
            mViewportW = w;
            mViewportH = h;
            if (mTarget) mTarget->OnResize(w, h);
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
            // Desktop : NO-OP. La swapchain est geree par OnResize (event resize,
            // teste dans la boucle). On NE declenche SURTOUT PAS OnResize ici :
            // l'appeler au demarrage (event Shown) AVANT que la 1ere frame ne soit
            // presentee fait, sur DX12, un reset de cmdList non-consommee -> echec
            // -> contexte invalide -> ecran blanc.
            // Android : la recreation reelle de surface apres APP_CMD_INIT_WINDOW
            // devra etre cablee dans le contexte NKCanvas (TODO hardware).
            return mTarget != nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────
        void PongApp::Render()
        {
            if (mTarget == nullptr || !mTarget->IsValid()) return;
            // Le cycle de frame est possede ICI, pas dans les scenes :
            //   Clear()   -> ouvre la frame + efface l'ecran (fond de la scene)
            //   Render()  -> les scenes ne font QUE dessiner (pas de Begin/End)
            //   Display() -> termine la frame + presente (swap buffers)
            // La couleur de fond vient de la scene active (Scene::BackgroundColor,
            // defaut theme::Dark ; RihenIntro -> blanc).
            Scene* top = mScenes.Top();
            mTarget->Clear(top ? top->BackgroundColor() : theme::Dark());
            AppContext ctx = BuildContext();
            mScenes.Render(ctx);
            mTarget->Display();
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
