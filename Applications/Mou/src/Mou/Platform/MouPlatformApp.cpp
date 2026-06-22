// =============================================================================
// Platform/MouPlatformApp.cpp
// Implémentation de la plateforme Mú (menu enfant 3 cartes + scènes de jeu).
// =============================================================================
#include "MouPlatformApp.h"
#include "../Core/MouConfig.h"
#include "../Games/Common/GameFactory.h"
#include "../Games/Common/MouFrame.h"
#include "../UI/MouUIColor.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include "../Assets/MouAssets.h"
#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::nkui;

namespace mou {

    using C = ui::MouUIColor;

    namespace {
        // Musique de fond (relatif à assets/) du menu et de chaque jeu.
        // Modifiable librement : il suffit de pointer vers un autre fichier audios/.
        const char* kMenuMusic = "audios/sunny_playground.mp3";
        const char* MusicForGame(GameId id) noexcept {
            switch (id) {
                case GameId::Couleurs: return "audios/sunbeam_hop.mp3";
                case GameId::Compter:  return "audios/sunny_parade.mp3";
                case GameId::Calcul:   return "audios/balafon_market_parade.mp3";
                case GameId::Formes:   return "audios/little_drummers_dance.mp3";
                case GameId::Animaux:  return "audios/safari_lullaby.mp3";
                case GameId::Memoire:  return "audios/moonlit_marimba_parade.mp3";
                default:               return kMenuMusic;
            }
        }

        // Voix d'encouragement (jouées en alternance sur succès / erreur).
        const char* kFelicitations[] = { "felicitation_bravo", "felicitation_super", "felicitation_tu_es_fort" };
        const char* kEncouragements[] = { "encouragement_essaie", "encouragement_presque" };
        constexpr uint32 kFelicitationCount  = 3;
        constexpr uint32 kEncouragementCount = 2;

        // Voix de consigne à l'entrée d'un jeu (Animaux gère la sienne par manche).
        const char* ConsigneVoice(GameId id) noexcept {
            switch (id) {
                case GameId::Couleurs: return "couleurs_consigne";
                case GameId::Compter:  return "compter_consigne";
                case GameId::Calcul:   return "calcul_consigne";
                case GameId::Formes:   return "formes_consigne";
                case GameId::Memoire:  return "memoire_consigne";
                default:               return nullptr;   // Animaux : voix par manche
            }
        }

        // Couleur de carte par jeu (héros + accents).
        math::NkColor CardColor(GameId id) noexcept {
            switch (id) {
                case GameId::Couleurs: return C::CORAL();
                case GameId::Compter:  return C::SKYB();
                case GameId::Calcul:   return C::LEAF();
                case GameId::Formes:   return C::GRAPE();
                case GameId::Animaux:  return C::ORANGE();
                case GameId::Memoire:  return C::SUNNY();
                default:               return C::SUNNY();
            }
        }
    }  // namespace

    MouPlatformApp::~MouPlatformApp() {
        mAudio.Shutdown();
        mCurrentGame.Reset();
        delete mUIBackend;
        delete mUIWindowManager;
        delete mUIContext;
        delete mRenderTarget;
        mWindow.Close();
    }

    bool MouPlatformApp::Initialize(const NkEntryState& state) noexcept {
        memory::InitializeAllocators();
        ParseArguments(state.GetArgs());

        NkWindowConfig cfg;
        cfg.title     = "Mu - jeux educatifs";
        cfg.width     = globals::DEFAULT_WINDOW_WIDTH;
        cfg.height    = globals::DEFAULT_WINDOW_HEIGHT;
        cfg.centered  = true;
        cfg.resizable = globals::WINDOW_RESIZABLE;

        #if defined(NKENTSEU_PLATFORM_ANDROID)
            cfg.fullscreen        = true;
            cfg.hideSystemUI      = true;
            cfg.screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
            cfg.lockOrientation   = true;
        #endif

        if (!mWindow.Create(cfg)) {
            MOU_LOG_ERROR("Impossible de creer la fenetre");
            return false;
        }

        #if defined(NKENTSEU_PLATFORM_ANDROID)
            mWindow.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
            mWindow.SetLockOrientation(true);
            mWindow.SetFullscreen(true);
            mWindow.SetHideSystemUI(true);
        #endif

        NkContextDesc desc;
        desc.api = mGraphicsApi;
        if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
            desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
        }
        mRenderTarget = new NkRenderWindow(mWindow, desc);
        if (!mRenderTarget || !mRenderTarget->IsValid()) {
            MOU_LOG_ERROR("Impossible d'initialiser NkRenderWindow");
            delete mRenderTarget; mRenderTarget = nullptr;
            return false;
        }
        MOU_LOG_INFOF("Backend graphique: %s", NkGraphicsApiName(desc.api));

        // NKUI.
        NkUIFontConfig fontConfig;
        fontConfig.yAxisUp = false;
        fontConfig.enableAtlas = true;
        fontConfig.enableBitmapFallback = true;
        fontConfig.defaultFontSize = 24.f;

        mUIContext = new NkUIContext();
        if (!mUIContext->Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height), fontConfig)) {
            MOU_LOG_ERROR("Impossible d'initialiser NkUIContext");
            return false;
        }
        mUIContext->SetTheme(NkUITheme::Dark());

        mUIWindowManager = new NkUIWindowManager();
        mUIWindowManager->Init();

        mUIBackend = new NkUICanvasBackend();
        mUIBackend->Init(mRenderTarget->GetRenderer());

        settings::Load();   // volumes utilisateur (best-effort)

        // Audio : moteur + musique du menu (boucle), volume piloté par Réglages.
        mAudio.Init();
        mAudio.PlayMusic(kMenuMusic);

        // Chargeur d'assets (SVG -> texture) partagé avec les jeux.
        mAssets.Init(mUIBackend);
        mMascotTex  = mAssets.LoadSvg("mascot_nana.svg",   256, 256);
        mIconTex[0] = mAssets.LoadSvg("icon_couleurs.svg", 160, 160);
        mIconTex[1] = mAssets.LoadSvg("icon_compter.svg",  160, 160);
        mIconTex[2] = mAssets.LoadSvg("icon_calcul.svg",   160, 160);
        mIconTex[3] = mAssets.LoadSvg("icon_formes.svg",   160, 160);
        mIconTex[4] = mAssets.LoadSvg("icon_animaux.svg",  160, 160);
        mIconTex[5] = mAssets.LoadSvg("icon_memoire.svg",  160, 160);
        // Intro Rihen : logo complet (frame 83 de l'anim = symbole + wordmark + slogan).
        // Noge : texte (logo non défini).
        mRihenTex = mAssets.LoadAsset("brand/rihen-logo.png", 0, 0);
        mRihenAspect = (mAssets.LastH() > 0) ? (float32)mAssets.LastW() / (float32)mAssets.LastH() : 1.f;
        mNogeTex = 0;

        int32 fontId = mUIContext->fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 24.f);
        if (fontId < 0) fontId = mUIContext->fontManager.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 18.f);
        mBodyFontId = static_cast<uint32>(fontId < 0 ? 0 : fontId);
        mUIFont = mUIContext->fontManager.Get(mBodyFontId);

        int32 titleId = mUIContext->fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 48.f);
        mTitleFontId = static_cast<uint32>(titleId < 0 ? (fontId < 0 ? 0 : fontId) : titleId);
        mTitleFont = mUIContext->fontManager.Get(mTitleFontId);
        if (!mTitleFont) mTitleFont = mUIFont;

        // Événements → pointeur unifié.
        auto& events = NkEvents();
        events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent*) { mRunning = false; });
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
        // Arrière-plan : l'ANativeWindow est détruite -> on cesse de rendre.
        events.AddEventCallback<NkWindowHiddenEvent>([this](NkWindowHiddenEvent*) {
            mActive = false;
            mAudio.Pause();           // coupe la musique en arrière-plan
        });
        // Retour d'arrière-plan : l'ANativeWindow a été recréée -> recréer la
        // surface EGL (sinon écran noir au resume) puis reprendre le rendu.
        events.AddEventCallback<NkWindowShownEvent>([this](NkWindowShownEvent*) {
            if (mRenderTarget) mRenderTarget->RecreateSurface();
            mActive = true;
            mAudio.Resume();
        });
#endif
        // Perte/gain de focus (toutes plateformes) : la fenêtre reste visible mais
        // inactive -> on met le JEU et l'AUDIO en pause, et on les reprend au retour.
        events.AddEventCallback<NkWindowFocusLostEvent>([this](NkWindowFocusLostEvent*) {
            mPaused = true;
            mAudio.Pause();
        });
        events.AddEventCallback<NkWindowFocusGainedEvent>([this](NkWindowFocusGainedEvent*) {
            mPaused = false;
            mAudio.Resume();
        });
        events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent* e) {
            mInput.pointerPos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
            mUIInput.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        });
        events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent* e) {
            if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
                mInput.pressedThisFrame = true; mInput.pressed = true;
                mUIInput.SetMouseButton(0, true);
            }
        });
        events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent* e) {
            if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
                mInput.releasedThisFrame = true; mInput.pressed = false;
                mUIInput.SetMouseButton(0, false);
            }
        });
        events.AddEventCallback<NkTouchBeginEvent>([this](NkTouchBeginEvent* e) {
            if (e->GetNumTouches() == 0) return;
            const auto& t = e->GetTouch(0);
            const float32 tx = static_cast<float32>(t.clientX), ty = static_cast<float32>(t.clientY);
            mInput.pointerPos = { tx, ty };
            mInput.pressedThisFrame = true; mInput.pressed = true;
            mUIInput.SetMousePos(tx, ty); mUIInput.SetMouseButton(0, true);
        });
        events.AddEventCallback<NkTouchMoveEvent>([this](NkTouchMoveEvent* e) {
            if (e->GetNumTouches() == 0) return;
            const auto& t = e->GetTouch(0);
            const float32 tx = static_cast<float32>(t.clientX), ty = static_cast<float32>(t.clientY);
            mInput.pointerPos = { tx, ty };
            mUIInput.SetMousePos(tx, ty);
        });
        events.AddEventCallback<NkTouchEndEvent>([this](NkTouchEndEvent*) {
            mInput.releasedThisFrame = true; mInput.pressed = false;
            mUIInput.SetMouseButton(0, false);
        });

        MOU_LOG_INFO("Plateforme Mu initialisee");
        return true;
    }

    int MouPlatformApp::Run() noexcept {
        while (mRunning && mWindow.IsOpen()) {
            float32 dt = mClock.Tick().delta;
            if (dt > globals::MAX_DELTA_TIME) dt = globals::MAX_DELTA_TIME;

            mInput.BeginFrame();
            mUIInput.BeginFrame();
            while (NkEvent* ev = NkEvents().PollEvent()) {
                if (mCurrentScene == AppScene::GameScene && mCurrentGame) HandleGameEvent(ev);
                else HandleMainMenuEvent(ev);  // Splash + MainMenu : ESC quitte
            }
            if (!mRunning) break;

            mAudio.RefreshVolume();   // applique le volume Réglages (musique) en continu

            // Resize robuste.
            const math::NkVec2u sz = mRenderTarget->GetSize();
            if (sz.x != mLastWindowWidth || sz.y != mLastWindowHeight) {
                if (mLastWindowWidth != 0 && sz.x > 0 && sz.y > 0) {
                    mRenderTarget->OnResize(sz.x, sz.y);
                    if (mUIContext) { mUIContext->viewW = (int32)sz.x; mUIContext->viewH = (int32)sz.y; }
                }
                mLastWindowWidth = sz.x; mLastWindowHeight = sz.y;
            }

            // En arrière-plan (Android) : pas de surface valide -> on ne rend pas.
            if (!mActive) { nkentseu::NkChrono::Sleep((nkentseu::int64)16); continue; }

            // Fenêtre sans focus : jeu + audio en pause. On fige tout (pas d'Update, pas
            // de rendu) et on relâche le CPU ; la dernière frame reste affichée.
            if (mPaused) { nkentseu::NkChrono::Sleep((nkentseu::int64)16); continue; }

            if (mCurrentScene == AppScene::GameScene && mCurrentGame) UpdateGameScene(dt);

            mRenderTarget->Clear(C::BG_CREAM());
            if (mCurrentScene == AppScene::IntroRihen) {
                // Le PNG contient déjà symbole + wordmark + slogan -> pas de texte ajouté.
                if (RenderBrandIntro(mRihenTex, mRihenAspect, nullptr, nullptr,
                                     math::NkColor{ 10, 85, 95, 255 }, dt)) {
                    mCurrentScene = AppScene::IntroNoge; mIntroTime = 0.f;
                }
            } else if (mCurrentScene == AppScene::IntroNoge) {
                if (RenderNogeIntro(dt)) {
                    mCurrentScene = AppScene::Splash; mIntroTime = 0.f; mSplashTime = 0.f;
                }
            } else if (mCurrentScene == AppScene::Splash) {
                RenderSplash(dt);
            } else if (mCurrentScene == AppScene::MainMenu) {
                RenderMainMenu(dt);
            } else if (mCurrentScene == AppScene::Settings) {
                RenderSettings(dt);
            } else if (mCurrentScene == AppScene::GameScene && mCurrentGame) {
                RenderGameScene(dt);
            }
            mRenderTarget->Display();
        }

        MOU_LOG_INFO("Fermeture de la plateforme Mu");
        memory::ShutdownAllocators();
        return 0;
    }

    bool MouPlatformApp::RenderBrandIntro(uint32 tex, float32 aspect,
                                          const char* title, const char* subtitle,
                                          const math::NkColor& textCol, float32 dt) noexcept {
        if (!mUIContext || !mUIBackend) return true;
        mIntroTime += dt;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);
        NkUIDrawList& dl = *mUIContext->dl;

        // Fond blanc (marque).
        dl.AddRectFilled(NkRect{ 0.f, 0.f, W, H }, math::NkColor{ 255, 255, 255, 255 });

        // Courbe d'animation : fondu+zoom in, maintien, fondu out.
        const float32 tin = 0.45f, hold = 1.6f, tout = 0.45f, total = tin + hold + tout;
        float32 a = 1.f, sc = 1.f;
        if (mIntroTime < tin) {
            const float32 u = mIntroTime / tin; a = u; sc = 0.85f + 0.15f * u;
        } else if (mIntroTime > tin + hold) {
            float32 u = (mIntroTime - tin - hold) / tout; if (u > 1.f) u = 1.f;
            a = 1.f - u; sc = 1.f + 0.05f * u;
        }
        if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
        const uint8 alpha8 = (uint8)(255.f * a);

        const bool hasTitle = (title && title[0]);
        // Logo (si fourni), ratio préservé. Boîte large car les PNG logo ont du padding.
        float32 titleY = H * 0.46f;
        if (tex) {
            float32 lw = W * 0.72f;
            float32 lh = (aspect > 0.0001f) ? lw / aspect : H * 0.55f;
            const float32 maxH = H * 0.62f;
            if (lh > maxH) { lh = maxH; lw = lh * aspect; }
            lw *= sc; lh *= sc;
            // Centré verticalement si pas de texte sous le logo, sinon plus haut.
            const float32 logoCY = hasTitle ? H * 0.40f : H * 0.50f;
            const math::NkColor tint{ 255, 255, 255, alpha8 };
            dl.AddImage(tex, NkRect{ W * 0.5f - lw * 0.5f, logoCY - lh * 0.5f, lw, lh },
                        NkVec2{ 0.f, 0.f }, NkVec2{ 1.f, 1.f }, tint);
            titleY = logoCY + lh * 0.5f + 24.f;
        }

        // Titre + sous-titre (couleur de marque, avec le même fondu).
        math::NkColor tc = textCol; tc.a = alpha8;
        math::NkColor sc2 = textCol; sc2.a = (uint8)(alpha8 * 0.75f);
        if (mTitleFont && hasTitle)
            mTitleFont->RenderText(dl, NkVec2{ (W - mTitleFont->MeasureWidth(title)) * 0.5f,
                                   titleY + mTitleFont->metrics.ascender }, title, tc);
        if (mUIFont && subtitle && subtitle[0])
            mUIFont->RenderText(dl, NkVec2{ (W - mUIFont->MeasureWidth(subtitle)) * 0.5f,
                                titleY + 64.f + mUIFont->metrics.ascender }, subtitle, sc2);

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);

        return (mIntroTime > total) || mInput.pressedThisFrame;
    }

    bool MouPlatformApp::RenderNogeIntro(float32 dt) noexcept {
        if (!mUIContext || !mUIBackend) return true;
        mIntroTime += dt;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);
        NkUIDrawList& dl = *mUIContext->dl;

        dl.AddRectFilled(NkRect{ 0.f, 0.f, W, H }, math::NkColor{ 255, 255, 255, 255 });

        // Fondu global (in / hold / out).
        const float32 tin = 0.5f, hold = 1.8f, tout = 0.5f, total = tin + hold + tout;
        float32 a = 1.f;
        if (mIntroTime < tin) a = mIntroTime / tin;
        else if (mIntroTime > tin + hold) { float32 u = (mIntroTime - tin - hold) / tout; if (u > 1.f) u = 1.f; a = 1.f - u; }
        if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;

        const uint8 av = (uint8)(255.f * a);
        const math::NkColor navy{ 38, 30, 90, av };
        const math::NkColor navyGlow{ 38, 30, 90, (uint8)(70.f * a) };
        const math::NkColor orange{ 255, 146, 43, av };

        const float32 cx = W * 0.5f;
        const float32 hexCY = H * 0.40f;
        const float32 R = (W < H ? W : H) * 0.14f;

        // Hexagone qui se trace (6 segments) selon le temps.
        float32 prog = mIntroTime / 0.8f; if (prog > 1.f) prog = 1.f;
        const float32 totalSeg = 6.f * prog;
        for (int32 i = 0; i < 6; ++i) {
            const float32 seg = totalSeg - (float32)i;
            if (seg <= 0.f) break;
            const float32 tt = seg > 1.f ? 1.f : seg;
            const float32 a0 = -1.5707963f + 1.0471976f * (float32)i;
            const float32 a1 = -1.5707963f + 1.0471976f * (float32)(i + 1);
            const float32 x0 = cx + std::cos((double)a0) * R, y0 = hexCY + std::sin((double)a0) * R;
            const float32 fx = cx + std::cos((double)a1) * R, fy = hexCY + std::sin((double)a1) * R;
            const float32 x1 = x0 + (fx - x0) * tt, y1 = y0 + (fy - y0) * tt;
            dl.AddLine(NkVec2{ x0, y0 }, NkVec2{ x1, y1 }, navyGlow, 9.f);
            dl.AddLine(NkVec2{ x0, y0 }, NkVec2{ x1, y1 }, navy, 4.f);
        }
        // Cœur orange qui apparaît une fois l'hexagone tracé.
        if (mIntroTime > 0.7f) {
            float32 cp = (mIntroTime - 0.7f) / 0.3f; if (cp > 1.f) cp = 1.f;
            dl.AddCircleFilled(NkVec2{ cx, hexCY }, R * 0.34f * cp, orange, 0);
        }

        // Textes (fondu après l'hexagone).
        float32 ta = (mIntroTime < 0.8f) ? 0.f : (mIntroTime - 0.8f) / 0.6f;
        if (ta > 1.f) ta = 1.f; ta *= a;
        auto drawC = [&](NkUIFont* fnt, float32 y, const char* s, math::NkColor base, float32 mul) {
            if (!fnt || !s) return;
            base.a = (uint8)(255.f * ta * mul);
            fnt->RenderText(dl, NkVec2{ (W - fnt->MeasureWidth(s)) * 0.5f, y + fnt->metrics.ascender }, s, base);
        };
        const float32 belowY = hexCY + R;
        drawC(mUIFont,    belowY + 26.f,  "PROPULSE PAR",   math::NkColor{ 120, 120, 135, 255 }, 0.65f);
        drawC(mTitleFont, belowY + 52.f,  "Noge",           navy,                                1.0f);
        drawC(mUIFont,    belowY + 122.f, "Moteur de jeu",  orange,                              0.9f);

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);

        return (mIntroTime > total) || mInput.pressedThisFrame;
    }

    void MouPlatformApp::RenderSplash(float32 dt) noexcept {
        if (!mUIContext || !mUIBackend) return;
        mSplashTime += dt;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);
        NkUIDrawList& dl = *mUIContext->dl;

        auto drawC = [&](NkUIFont* f, float32 y, const char* s, const math::NkColor& c) {
            if (f && s) f->RenderText(dl, NkVec2{ (W - f->MeasureWidth(s)) * 0.5f, y + f->metrics.ascender }, s, c);
        };

        dl.AddRectFilled(NkRect{ 0.f, 0.f, W, H }, C::BG_CREAM());

        // Mascotte qui rebondit doucement.
        const float32 bob = (float32)std::sin((double)mSplashTime * 3.0) * 14.f;
        const float32 ms = 224.f;
        if (mMascotTex)
            dl.AddImage(mMascotTex, NkRect{ W * 0.5f - ms * 0.5f, H * 0.32f - ms * 0.5f + bob, ms, ms });

        drawC(mTitleFont, H * 0.55f, "Mu", C::SUNNY());
        drawC(mUIFont, H * 0.55f + 64.f, "Apprends en jouant !", C::TEXT_DARK());
        // Clignote doucement.
        if (((int)(mSplashTime * 1.5f)) % 2 == 0)
            drawC(mUIFont, H - 90.f, "touche pour commencer", C::ORANGE());

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);

        if (mSplashTime > 2.6f || mInput.pressedThisFrame) {
            mCurrentScene = AppScene::MainMenu;
        }
    }

    void MouPlatformApp::RenderMainMenu(float32 dt) noexcept {
        if (!mUIContext || !mUIBackend) return;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);
        NkUIDrawList& dl = *mUIContext->dl;

        auto drawText = [&](NkUIFont* f, float32 x, float32 topY, const char* s,
                            const math::NkColor& c, float32 maxW = -1.f) {
            if (f && s) f->RenderText(dl, NkVec2{ x, topY + f->metrics.ascender }, s, c, maxW);
        };
        auto drawTextCentered = [&](NkUIFont* f, float32 x, float32 w, float32 topY,
                                    const char* s, const math::NkColor& c) {
            if (f && s) drawText(f, x + (w - f->MeasureWidth(s)) * 0.5f, topY, s, c);
        };

        // Fond crème.
        dl.AddRectFilled(NkRect{ 0.f, 0.f, W, H }, C::BG_CREAM());

        // En-tête (titre + sous-titre).
        drawTextCentered(mTitleFont, 0.f, W, 40.f, "Mu", C::SUNNY());
        drawTextCentered(mUIFont, 0.f, W, 110.f, "Apprends en jouant !", C::TEXT_DARK());

        // Grille de cartes (3 colonnes, autant de rangées que nécessaire).
        const GameInfo* games = GameFactory::GetAllGames();
        const uint32 count = GameFactory::GetGameCount();

        const uint32 cols = (count <= 3) ? (count > 0 ? count : 1) : 3u;
        const uint32 rows = (count + cols - 1) / cols;
        const float32 gap   = 30.f;
        const float32 topY  = 168.f;
        const float32 botY  = H - 40.f;
        const float32 cardW = (W - 2.f * gap - gap * (cols - 1)) / (float32)cols;
        const float32 cardH = (botY - topY - gap * (rows - 1)) / (float32)rows;

        const float32 px = mInput.pointerPos.x;
        const float32 py = mInput.pointerPos.y;

        for (uint32 i = 0; i < count; ++i) {
            const GameInfo& g = games[i];
            const uint32 gcol = i % cols, grow = i / cols;
            const float32 cx = gap + gcol * (cardW + gap);
            const float32 cy = topY + grow * (cardH + gap);
            const NkRect card{ cx, cy, cardW, cardH };
            const bool hovered = (px >= cx && px < cx + cardW && py >= cy && py < cy + cardH);

            // Ombre basse + carte (couleur du jeu, atténuée si verrouillée).
            math::NkColor col = CardColor(g.id);
            if (!g.playable) col = ui::WithAlpha(col, 110);
            dl.AddRectFilled(NkRect{ cx, cy + 10.f, cardW, cardH }, ui::WithAlpha(C::INK(), 60), 28.f, 28.f);
            dl.AddRectFilled(card, col, 28.f, 28.f);
            dl.AddRect(card, C::INK(), hovered && g.playable ? 8.f : 5.f, 28.f, 28.f);

            // Badge blanc + icône du jeu (en haut de la carte).
            const float32 badgeR = cardH * 0.20f < 66.f ? cardH * 0.20f : 66.f;
            const float32 bcx = cx + cardW * 0.5f;
            const float32 bcy = cy + badgeR + 22.f;
            dl.AddCircleFilled(NkVec2{ bcx, bcy }, badgeR, C::SURFACE(), 0);
            dl.AddCircle(NkVec2{ bcx, bcy }, badgeR, C::INK(), 4.f, 0);
            const uint32 icon = (i < 6) ? mIconTex[i] : 0;
            if (icon) {
                const float32 is = badgeR * 1.5f;
                dl.AddImage(icon, NkRect{ bcx - is * 0.5f, bcy - is * 0.5f, is, is });
            }

            // Titre + sous-titre (texte blanc sur aplat).
            const float32 txtY = bcy + badgeR + 16.f;
            drawTextCentered(mTitleFont, cx, cardW, txtY, g.title, C::TEXT_ON_COLOR());
            drawTextCentered(mUIFont, cx, cardW, txtY + 50.f, g.subtitle, C::TEXT_ON_COLOR());

            // Pastille d'action en bas.
            const char* hint = g.playable ? "Jouer !" : "Bientot";
            drawTextCentered(mUIFont, cx, cardW, cy + cardH - 50.f, hint, C::TEXT_ON_COLOR());

            // Lancement au toucher (jeu jouable uniquement).
            if (hovered && g.playable && mInput.pressedThisFrame) {
                mSelectedGameId = g.id;
                LaunchGame(g.id);
            }
        }

        // Bouton "Reglages" (coin haut-gauche).
        {
            const float32 bw = 230.f, bh = 78.f, bx = 24.f, by = 20.f;
            const bool over = (px >= bx && px < bx + bw && py >= by && py < by + bh);
            NkUIFont* bf = mTitleFont ? mTitleFont : mUIFont;
            dl.AddRectFilled(NkRect{ bx, by, bw, bh }, over ? C::SUNNY() : C::SURFACE(), 20.f, 20.f);
            dl.AddRect(NkRect{ bx, by, bw, bh }, C::INK(), 5.f, 20.f, 20.f);
            drawTextCentered(bf, bx, bw, by + (bh - (bf ? bf->metrics.lineHeight : 18.f)) * 0.5f,
                             "Reglages", C::TEXT_DARK());
            if (over && mInput.pressedThisFrame) mCurrentScene = AppScene::Settings;
        }

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);
    }

    void MouPlatformApp::RenderSettings(float32 dt) noexcept {
        if (!mUIContext || !mUIBackend) return;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);
        NkUIDrawList& dl = *mUIContext->dl;

        auto drawText = [&](NkUIFont* f, float32 x, float32 topY, const char* s, const math::NkColor& c) {
            if (f && s) f->RenderText(dl, NkVec2{ x, topY + f->metrics.ascender }, s, c);
        };
        auto drawC = [&](NkUIFont* f, float32 x, float32 w, float32 topY, const char* s, const math::NkColor& c) {
            if (f && s) drawText(f, x + (w - f->MeasureWidth(s)) * 0.5f, topY, s, c);
        };

        dl.AddRectFilled(NkRect{ 0.f, 0.f, W, H }, C::BG_CREAM());
        drawC(mTitleFont, 0.f, W, 40.f, "Reglages", C::TEXT_DARK());

        const float32 px = mInput.pointerPos.x, py = mInput.pointerPos.y;

        // Curseur (slider) : libellé + barre + pastille. Glisse pour régler 0..1.
        auto slider = [&](float32 y, const char* label, float32& val) {
            const float32 tx = W * 0.30f, tw = W * 0.45f, th = 18.f;
            const float32 ty = y + 14.f;
            drawText(mTitleFont, W * 0.12f, y, label, C::TEXT_DARK());
            dl.AddRectFilled(NkRect{ tx, ty, tw, th }, ui::WithAlpha(C::INK(), 50), th * 0.5f, th * 0.5f);
            dl.AddRectFilled(NkRect{ tx, ty, tw * val, th }, C::LEAF(), th * 0.5f, th * 0.5f);
            const float32 kx = tx + tw * val, ky = ty + th * 0.5f;
            dl.AddCircleFilled(NkVec2{ kx, ky }, 22.f, C::SUNNY(), 0);
            dl.AddCircle(NkVec2{ kx, ky }, 22.f, C::INK(), 4.f, 0);
            // pourcentage
            char pc[8]; std::snprintf(pc, sizeof(pc), "%d%%", (int)(val * 100.f + 0.5f));
            drawText(mUIFont, tx + tw + 28.f, y, pc, C::TEXT_DARK());
            // interaction : appui sur la zone de la barre -> règle la valeur
            if (mInput.pressed && py >= ty - 26.f && py <= ty + th + 26.f && px >= tx - 26.f && px <= tx + tw + 26.f) {
                float32 v = (px - tx) / tw; if (v < 0.f) v = 0.f; if (v > 1.f) v = 1.f;
                val = v;
            }
        };

        slider(H * 0.30f, "Musique", settings::musicVolume);
        slider(H * 0.48f, "Effets",  settings::sfxVolume);

        // Bouton Muet (toggle).
        {
            const float32 bw = 320.f, bh = 92.f, bx = (W - bw) * 0.5f, by = H * 0.64f;
            const bool over = (px >= bx && px < bx + bw && py >= by && py < by + bh);
            NkUIFont* bf = mTitleFont ? mTitleFont : mUIFont;
            const math::NkColor bg = settings::muted ? C::CORAL() : C::SURFACE();
            dl.AddRectFilled(NkRect{ bx, by, bw, bh }, over ? C::ORANGE() : bg, 24.f, 24.f);
            dl.AddRect(NkRect{ bx, by, bw, bh }, C::INK(), 5.f, 24.f, 24.f);
            drawC(bf, bx, bw, by + (bh - (bf ? bf->metrics.lineHeight : 18.f)) * 0.5f,
                  settings::muted ? "Son coupe : OUI" : "Son coupe : non",
                  settings::muted ? C::TEXT_ON_COLOR() : C::TEXT_DARK());
            if (over && mInput.pressedThisFrame) settings::muted = !settings::muted;
        }

        // Bouton Retour (sauvegarde + menu).
        {
            const float32 bw = 280.f, bh = 96.f, bx = (W - bw) * 0.5f, by = H - bh - 30.f;
            const bool over = (px >= bx && px < bx + bw && py >= by && py < by + bh);
            NkUIFont* bf = mTitleFont ? mTitleFont : mUIFont;
            dl.AddRectFilled(NkRect{ bx, by, bw, bh }, over ? C::ORANGE() : C::LEAF(), 26.f, 26.f);
            dl.AddRect(NkRect{ bx, by, bw, bh }, C::INK(), 5.f, 26.f, 26.f);
            drawC(bf, bx, bw, by + (bh - (bf ? bf->metrics.lineHeight : 18.f)) * 0.5f,
                  "Retour", C::TEXT_ON_COLOR());
            if (over && mInput.pressedThisFrame) { settings::Save(); mCurrentScene = AppScene::MainMenu; }
        }

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);
    }

    void MouPlatformApp::UpdateGameScene(float32 dt) noexcept {
        if (mCurrentGame) {
            mCurrentGame->Update(dt);

            // Route le signal audio du jeu -> effet sonore (+ voix d'encouragement).
            switch (mCurrentGame->ConsumeAudioCue()) {
                case MouFeedback::Cue::Good:
                    mAudio.PlaySfx(MouAudio::Sfx::Good);
                    mAudio.PlayVoice(kFelicitations[mVoiceRot++ % kFelicitationCount]);
                    break;
                case MouFeedback::Cue::Win:
                    mAudio.PlaySfx(MouAudio::Sfx::Win);
                    mAudio.PlayVoice("recompense_etoiles");
                    break;
                case MouFeedback::Cue::Bad:
                    mAudio.PlaySfx(MouAudio::Sfx::Bad);
                    mAudio.PlayVoice(kEncouragements[mVoiceRot++ % kEncouragementCount]);
                    break;
                case MouFeedback::Cue::Fail:
                    mAudio.PlaySfx(MouAudio::Sfx::Fail);
                    break;
                case MouFeedback::Cue::Star:
                    mAudio.PlaySfx(MouAudio::Sfx::Tap);
                    break;
                case MouFeedback::Cue::None:
                default: break;
            }
            // Voix de consigne demandée par le jeu (ex. Animaux : "Touche le lion").
            if (const char* v = mCurrentGame->ConsumeVoiceCue()) mAudio.PlayVoice(v);

            if (mCurrentGame->WantExit()) ReturnToMainMenu();
        }
    }

    void MouPlatformApp::RenderGameScene(float32 dt) noexcept {
        if (!mCurrentGame || !mUIContext || !mUIBackend) return;
        const math::NkVec2u szu = mRenderTarget->GetSize();
        const float32 W = static_cast<float32>(szu.x);
        const float32 H = static_cast<float32>(szu.y);

        mUIInput.dt = dt;
        mUIContext->BeginFrame(mUIInput, dt);

        MouFrame frame;
        frame.dl = mUIContext->dl;
        frame.font = mUIFont;
        frame.titleFont = mTitleFont;
        frame.width = W; frame.height = H;
        frame.safeX = 0.f; frame.safeY = 0.f; frame.safeW = W; frame.safeH = H;
        frame.pointer = mInput.pointerPos;
        frame.pointerDown = mInput.pressed;
        frame.pointerPressed = mInput.pressedThisFrame;
        frame.pointerReleased = mInput.releasedThisFrame;

        mCurrentGame->Render(frame);

        mUIContext->EndFrame();
        mUIBackend->Submit(*mUIContext, szu.x, szu.y);
    }

    void MouPlatformApp::HandleGameEvent(NkEvent* event) noexcept {
        if (mCurrentGame) mCurrentGame->OnEvent(event);
    }

    void MouPlatformApp::HandleMainMenuEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mRunning = false;
        }
    }

    void MouPlatformApp::LaunchGame(GameId gameId) noexcept {
        MOU_LOG_INFOF("Lancement du jeu: %d", static_cast<int32>(gameId));
        mCurrentGame = GameFactory::CreateGame(gameId, memory::gDefaultAllocator, &mAssets);
        if (!mCurrentGame) {
            MOU_LOG_ERRORF("Impossible de creer le jeu %d", static_cast<int32>(gameId));
            return;
        }
        mAudio.PlayMusic(MusicForGame(gameId));   // ambiance propre au jeu
        if (const char* v = ConsigneVoice(gameId)) mAudio.PlayVoice(v);
        mCurrentScene = AppScene::GameScene;
    }

    void MouPlatformApp::ReturnToMainMenu() noexcept {
        MOU_LOG_INFO("Retour au menu principal");
        if (mCurrentGame) { mCurrentGame->Unload(); mCurrentGame.Reset(); }
        mAudio.PlayMusic(kMenuMusic);
        mCurrentScene = AppScene::MainMenu;
    }

    void MouPlatformApp::ParseArguments(const NkVector<NkString>& args) noexcept {
        for (usize i = 0; i < args.Size(); ++i) {
            const NkString& arg = args[i];
            if      (arg == "--backend=vulkan"   || arg == "-bvk")   mGraphicsApi = NkGraphicsApi::NK_GFX_API_VULKAN;
            else if (arg == "--backend=opengl"   || arg == "-bgl")   mGraphicsApi = NkGraphicsApi::NK_GFX_API_OPENGL;
            else if (arg == "--backend=dx11"     || arg == "-bdx11") mGraphicsApi = NkGraphicsApi::NK_GFX_API_DX11;
            else if (arg == "--backend=dx12"     || arg == "-bdx12") mGraphicsApi = NkGraphicsApi::NK_GFX_API_DX12;
            else if (arg == "--backend=software" || arg == "-bsw")   mGraphicsApi = NkGraphicsApi::NK_GFX_API_SOFTWARE;
        }
    }

}  // namespace mou
