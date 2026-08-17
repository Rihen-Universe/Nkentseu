// =============================================================================
// Platform/NkoungPlatformApp.cpp
// Implémentation de la plateforme Nkoung.
// =============================================================================
#include "NkoungPlatformApp.h"
#include "../Core/NkoungConfig.h"
#include "../Games/Common/GameFactory.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKTime/NkTime.h"
#include "NKMath/NKMath.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "../UI/NkoungUIColor.h"
#include "../UI/NkoungDraw.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::nkgui;

namespace nkoung {

	namespace {
		// Libellé + couleur d'un statut de jeu (pour la pastille de carte).
		struct StatusStyle {
				const char *label;
				nkentseu::math::NkColor color;
		};

		static StatusStyle StatusOf(GameStatus s) noexcept {
			using C = ui::NkoungUIColor;
			switch (s) {
				case GameStatus::Released:
					return {"Disponible", C::GREEN_SUCCESS()};
				case GameStatus::BetaBuild:
					return {"Beta", C::CYAN_BRIGHT()};
				case GameStatus::AlphaBuild:
					return {"Alpha", C::ORANGE_WARNING()};
				case GameStatus::Prototype:
					return {"Prototype", C::ORANGE_WARNING()};
				case GameStatus::Archived:
					return {"Archive", C::TEXT_TERTIARY()};
				case GameStatus::NotStarted:
				default:
					return {"A venir", C::TEXT_TERTIARY()};
			}
		}
	} // namespace

	NkoungPlatformApp::~NkoungPlatformApp() {
		mCurrentGame.Reset();
		delete mUIBackend;
		if (mTitleFont != mUIFont)
			delete mTitleFont;
		delete mUIFont;
		if (mUIContext)
			mUIContext->Shutdown();
		delete mUIContext;
		delete mRenderTarget;
		mWindow.Close();
	}

	bool NkoungPlatformApp::Initialize(const NkEntryState &state) noexcept {
		// Initialiser la mémoire Nkoung
		memory::InitializeAllocators();

		// Parser les arguments de démarrage
		ParseArguments(state.GetArgs());

		// Créer la fenêtre
		NkWindowConfig cfg;
		cfg.title = "Nkoung - Plateforme de jeux 2D";
		cfg.width = globals::DEFAULT_WINDOW_WIDTH;
		cfg.height = globals::DEFAULT_WINDOW_HEIGHT;
		cfg.centered = true;
		cfg.resizable = globals::WINDOW_RESIZABLE;

		if (!mWindow.Create(cfg)) {
			NKOUNG_LOG_ERROR("Impossible de créer la fenêtre");
			return false;
		}

		// Créer la cible de rendu NKCanvas
		NkContextDesc desc;
		desc.api = mGraphicsApi;

		// Si AUTO, utiliser le backend par défaut de la plateforme
		if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
			desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
			desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
		}
		mRenderTarget = new NkRenderWindow(mWindow, desc);
		if (!mRenderTarget || !mRenderTarget->IsValid()) {
			NKOUNG_LOG_ERROR("Impossible d'initialiser NkRenderWindow");
			delete mRenderTarget;
			mRenderTarget = nullptr;
			return false;
		}

		NKOUNG_LOG_INFOF("Backend graphique: %s", NkGraphicsApiName(desc.api));

		// Initialiser NKGui (contexte + 2 polices + backend NKCanvas) — même
		// montage que NkPAGui.cpp (pile NKPA/Pong/NKCode).
		mUIContext = new NkGuiContext();
		if (!mUIContext->Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height))) {
			NKOUNG_LOG_ERROR("Impossible d'initialiser NkGuiContext");
			return false;
		}
		SetCurrentContext(mUIContext);

		// Police corps (18) : DroidSans, repli ProggyClean.
		mUIFont = new NkGuiFont();
		if (!mUIFont->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f))
			mUIFont->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
		if (!mUIFont->Valid())
			NKOUNG_LOG_WARN("Aucune police chargée");
		mUIContext->font = mUIFont;

		// Police de titre (30) pour l'en-tête du menu : texId DISTINCT (le défaut
		// 'NKFT' est partagé par toute NkGuiFont -> collision d'atlas sinon).
		mTitleFont = new NkGuiFont();
		mTitleFont->texId = mUIFont->TexId() + 1u;
		if (!mTitleFont->LoadEmbedded(NkEmbeddedFontId::DroidSans, 30.f)) {
			delete mTitleFont;
			mTitleFont = mUIFont;
		}

		mUIBackend = new NkGuiCanvasBackend();
		if (!mUIBackend->Init(mRenderTarget->GetRenderer())) {
			NKOUNG_LOG_ERROR("Impossible d'initialiser NkGuiCanvasBackend");
			return false;
		}
		if (mUIFont->pixels && mUIFont->atlasW > 0 && mUIFont->atlasH > 0)
			mUIBackend->UploadFontGray8(mUIFont->TexId(), mUIFont->pixels, mUIFont->atlasW, mUIFont->atlasH);
		if (mTitleFont != mUIFont && mTitleFont->pixels && mTitleFont->atlasW > 0 && mTitleFont->atlasH > 0)
			mUIBackend->UploadFontGray8(mTitleFont->TexId(), mTitleFont->pixels, mTitleFont->atlasW,
										mTitleFont->atlasH);
		NKOUNG_LOG_INFOF("UI NKGui/NKCanvas prete (atlas corps %dx%d, titre %dx%d)", mUIFont->atlasW,
						 mUIFont->atlasH, mTitleFont->atlasW, mTitleFont->atlasH);

		// Configurer les callbacks d'événements
		auto &events = NkEvents();
		events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent *) { mRunning = false; });
		events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
			mInput.mousePos = {static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
			mUIContext->input.mousePos = mInput.mousePos;
		});
		events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
			if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
				mInput.mouseLPressedThisFrame = true;
				mInput.mouseLPressed = true;
				mUIContext->input.mouseDown[0] = true;
			}
		});
		events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent *e) {
			if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
				mInput.mouseLReleasedThisFrame = true;
				mInput.mouseLPressed = false;
				mUIContext->input.mouseDown[0] = false;
			}
		});

		// Tactile (mobile/web) → mappé sur le même pointeur que la souris (1er contact).
		events.AddEventCallback<NkTouchBeginEvent>([this](NkTouchBeginEvent *e) {
			if (e->GetNumTouches() == 0)
				return;
			const auto &t = e->GetTouch(0);
			const float32 tx = static_cast<float32>(t.clientX), ty = static_cast<float32>(t.clientY);
			mInput.mousePos = {tx, ty};
			mInput.mouseLPressedThisFrame = true;
			mInput.mouseLPressed = true;
			mUIContext->input.mousePos = {tx, ty};
			mUIContext->input.mouseDown[0] = true;
		});
		events.AddEventCallback<NkTouchMoveEvent>([this](NkTouchMoveEvent *e) {
			if (e->GetNumTouches() == 0)
				return;
			const auto &t = e->GetTouch(0);
			const float32 tx = static_cast<float32>(t.clientX), ty = static_cast<float32>(t.clientY);
			mInput.mousePos = {tx, ty};
			mUIContext->input.mousePos = {tx, ty};
		});
		events.AddEventCallback<NkTouchEndEvent>([this](NkTouchEndEvent *) {
			mInput.mouseLReleasedThisFrame = true;
			mInput.mouseLPressed = false;
			mUIContext->input.mouseDown[0] = false;
		});

		InitPlatformMenu();

		NKOUNG_LOG_INFO("Plateforme Nkoung initialisée avec succès");
		return true;
	}

	int NkoungPlatformApp::Run() noexcept {
		while (mRunning && mWindow.IsOpen()) {
			float32 dt = mClock.Tick().delta;
			if (dt > globals::MAX_DELTA_TIME)
				dt = globals::MAX_DELTA_TIME;

			// Input
			mInput.BeginFrame();
			while (NkEvent *ev = NkEvents().PollEvent()) {
				if (mCurrentScene == AppScene::PlatformMenu) {
					HandlePlatformMenuEvent(ev);
				} else if (mCurrentScene == AppScene::GameScene && mCurrentGame) {
					HandleGameEvent(ev);
				}
			}

			if (!mRunning)
				break;

			// Resize robuste
			const math::NkVec2u sz = mRenderTarget->GetSize();
			if (sz.x != mLastWindowWidth || sz.y != mLastWindowHeight) {
				if (mLastWindowWidth != 0 && sz.x > 0 && sz.y > 0) {
					mRenderTarget->OnResize(sz.x, sz.y);
					if (mUIContext) {
						mUIContext->viewW = static_cast<int32>(sz.x);
						mUIContext->viewH = static_cast<int32>(sz.y);
					}
				}
				mLastWindowWidth = sz.x;
				mLastWindowHeight = sz.y;
			}

			// Update
			if (mCurrentScene == AppScene::PlatformMenu) {
				UpdatePlatformMenu(dt);
			} else if (mCurrentScene == AppScene::GameScene && mCurrentGame) {
				UpdateGameScene(dt);
			}

			// Render
			mRenderTarget->Clear(ui::NkoungUIColor::BG_DARK());

			if (mCurrentScene == AppScene::PlatformMenu) {
				RenderPlatformMenu(dt);
			} else if (mCurrentScene == AppScene::GameScene && mCurrentGame) {
				RenderGameScene(dt);
			}

			mRenderTarget->Display();
		}

		NKOUNG_LOG_INFO("Fermeture de la plateforme Nkoung");
		memory::ShutdownAllocators();
		return 0;
	}

	bool NkoungPlatformApp::InitPlatformMenu() noexcept {
		// À implémenter : initialisation simple pour MVP
		return true;
	}

	void NkoungPlatformApp::UpdatePlatformMenu(float32 dt) noexcept {
		(void)dt;
		// À implémenter
	}

	void NkoungPlatformApp::RenderPlatformMenu(float32 dt) noexcept {
		using C = ui::NkoungUIColor;
		if (!mUIContext || !mUIBackend)
			return;

		const math::NkVec2u szu = mRenderTarget->GetSize();
		const float32 W = static_cast<float32>(szu.x);
		const float32 H = static_cast<float32>(szu.y);

		// FPS lissé (affiché dans l'en-tête).
		if (dt > 0.0001f)
			mFpsSmooth = mFpsSmooth * 0.92f + (1.f / dt) * 0.08f;

		// Ouvre la frame UI ; on dessine tout dans la draw-list principale.
		SetCurrentContext(mUIContext);
		mUIContext->viewW = static_cast<int32>(szu.x);
		mUIContext->viewH = static_cast<int32>(szu.y);
		mUIContext->BeginFrame(dt);
		NkGuiDrawList &dl = mUIContext->dl;

		// AddText attend une BASELINE ; on passe un Y de HAUT et on ajoute l'ascent.
		auto drawText = [&](NkGuiFont *f, float32 x, float32 topY, const char *s, const math::NkColor &c,
							float32 maxW = -1.f) {
			if (f && f->Face() && s)
				dl.AddText(f->Face(), f->TexId(), NkVec2{x, topY + f->Ascent()}, s, c, maxW);
		};

		// ── Mise en page ──────────────────────────────────────────────────
		const float32 pad = 24.f;
		const float32 headerH = 64.f;
		const float32 footerH = 38.f;
		const float32 gap = 16.f;
		const int32 cols = 3;
		const int32 rows = 2;

		// Fond global.
		dl.AddRectFilled(NkRect{0.f, 0.f, W, H}, C::BG_DARK());

		// ── En-tête ───────────────────────────────────────────────────────
		dl.AddRectFilled(NkRect{0.f, 0.f, W, headerH}, C::BG_SECONDARY());
		dl.AddRectFilled(NkRect{0.f, headerH - 2.f, W, 2.f}, C::CYAN_BRIGHT());
		drawText(mTitleFont, pad, 14.f, "NKOUNG", C::CYAN_BRIGHT());
		drawText(mUIFont, pad + 138.f, 23.f, "Plateforme de jeux 2D", C::TEXT_SECONDARY());
		if (mUIFont) {
			char fps[32];
			snprintf(fps, sizeof(fps), "FPS %.0f", static_cast<double>(mFpsSmooth));
			const float32 fw = mUIFont->MeasureWidth(fps);
			drawText(mUIFont, W - pad - fw, 22.f, fps, C::TEXT_TERTIARY());
		}

		// ── Grille de cartes ──────────────────────────────────────────────
		const float32 gridTop = headerH + pad;
		const float32 gridBottom = H - footerH - pad;
		const float32 gridLeft = pad;
		const float32 gridRight = W - pad;
		const float32 cardW = (gridRight - gridLeft - gap * (cols - 1)) / cols;
		const float32 cardH = (gridBottom - gridTop - gap * (rows - 1)) / rows;

		const GameInfo *games = GameFactory::GetAllGames();
		const uint32 count = GameFactory::GetGameCount();

		mHoveredGame = -1;
		const float32 mx = mInput.mousePos.x;
		const float32 my = mInput.mousePos.y;

		for (uint32 i = 0; i < count; ++i) {
			const GameInfo &g = games[i];
			const int32 col = static_cast<int32>(i) % cols;
			const int32 row = static_cast<int32>(i) / cols;
			const float32 cx = gridLeft + col * (cardW + gap);
			const float32 cy = gridTop + row * (cardH + gap);
			const NkRect card{cx, cy, cardW, cardH};

			const bool hovered = (mx >= cx && mx < cx + cardW && my >= cy && my < cy + cardH);
			const bool selected = (g.id == mSelectedGameId);
			if (hovered)
				mHoveredGame = static_cast<int32>(i);

			// Clic souris : sélectionne (et lance si jouable).
			if (hovered && mInput.mouseLPressedThisFrame) {
				mSelectedGameId = g.id;
				if (g.playable)
					LaunchGame(g.id);
			}

			// Fond de carte (plus sombre si verrouillée).
			const math::NkColor bg = g.playable ? C::CARD_BG() : math::NkColor{20, 24, 34, 255};
			dl.AddRectFilled(card, bg, 8.f);

			// Bordure selon l'état.
			math::NkColor border = C::BORDER_SUBTLE();
			float32 bth = 1.5f;
			if (selected) {
				border = C::PINK_BRIGHT();
				bth = 3.f;
			} else if (hovered) {
				border = C::CYAN_BRIGHT();
				bth = 2.f;
			} else if (!g.playable) {
				border = C::BORDER_DISABLED();
				bth = 1.5f;
			}
			draw::RectOutline(dl, card, border, bth, 8.f);

			// Titre + sous-titre.
			const float32 tx = cx + 18.f;
			const math::NkColor titleCol = g.playable ? C::TEXT_PRIMARY() : C::TEXT_SECONDARY();
			drawText(mUIFont, tx, cy + 16.f, g.title, titleCol, cardW - 36.f);
			drawText(mUIFont, tx, cy + 42.f, g.subtitle, C::TEXT_SECONDARY(), cardW - 36.f);

			// Pastille de statut (point + libellé) en bas à gauche.
			const StatusStyle st = StatusOf(g.status);
			const float32 by = cy + cardH - 30.f;
			dl.AddCircleFilled(NkVec2{tx + 5.f, by + 7.f}, 5.f, st.color, 0);
			drawText(mUIFont, tx + 16.f, by - 2.f, st.label, st.color);

			// Action en bas à droite.
			if (mUIFont) {
				const char *hint = g.playable ? "Jouer >" : "Verrouille";
				const math::NkColor hc = g.playable ? C::CYAN_BRIGHT() : C::TEXT_TERTIARY();
				const float32 hw = mUIFont->MeasureWidth(hint);
				drawText(mUIFont, cx + cardW - 18.f - hw, by - 2.f, hint, hc);
			}
		}

		// ── Pied de page ──────────────────────────────────────────────────
		dl.AddRectFilled(NkRect{0.f, H - footerH, W, footerH}, C::BG_SECONDARY());
		dl.AddRectFilled(NkRect{0.f, H - footerH, W, 2.f}, C::PINK_BRIGHT());
		drawText(mUIFont, pad, H - footerH + 9.f, "Fleches: naviguer     Entree: jouer     Echap: quitter",
				 C::TEXT_SECONDARY());

		mUIContext->EndFrame();
		mUIBackend->Submit(mUIContext->dl, szu.x, szu.y);
		mUIBackend->Submit(mUIContext->dlOverlay, szu.x, szu.y);
	}

	void NkoungPlatformApp::UpdateGameScene(float32 dt) noexcept {
		if (mCurrentGame) {
			mCurrentGame->Update(dt);

			// Si le jeu souhaite quitter
			if (mCurrentGame->WantExit()) {
				ReturnToPlatformMenu();
			}
		}
	}

	void NkoungPlatformApp::RenderGameScene(float32 dt) noexcept {
		if (!mCurrentGame || !mUIContext || !mUIBackend)
			return;

		const math::NkVec2u szu = mRenderTarget->GetSize();
		const float32 W = static_cast<float32>(szu.x);
		const float32 H = static_cast<float32>(szu.y);

		SetCurrentContext(mUIContext);
		mUIContext->viewW = static_cast<int32>(szu.x);
		mUIContext->viewH = static_cast<int32>(szu.y);
		mUIContext->BeginFrame(dt);

		NkoungFrame frame;
		frame.dl = &mUIContext->dl;
		frame.font = mUIFont;
		frame.titleFont = mTitleFont;
		frame.width = W;
		frame.height = H;
		// Zone sûre = toute la fenêtre pour l'instant (insets mobiles à brancher si exposés par NKWindow).
		frame.safeX = 0.f;
		frame.safeY = 0.f;
		frame.safeW = W;
		frame.safeH = H;
		frame.pointer = mInput.mousePos;
		frame.pointerDown = mInput.mouseLPressed;
		frame.pointerPressed = mInput.mouseLPressedThisFrame;
		frame.pointerReleased = mInput.mouseLReleasedThisFrame;

		mCurrentGame->Render(frame);

		mUIContext->EndFrame();
		mUIBackend->Submit(mUIContext->dl, szu.x, szu.y);
		mUIBackend->Submit(mUIContext->dlOverlay, szu.x, szu.y);
	}

	void NkoungPlatformApp::HandleGameEvent(NkEvent *event) noexcept {
		if (mCurrentGame) {
			mCurrentGame->OnEvent(event);
		}
	}

	void NkoungPlatformApp::HandlePlatformMenuEvent(NkEvent *event) noexcept {
		auto *kp = event->As<NkKeyPressEvent>();
		if (!kp)
			return;

		const int32 count = static_cast<int32>(GameFactory::GetGameCount());
		if (count <= 0)
			return;
		const int32 cols = 3;
		int32 idx = static_cast<int32>(mSelectedGameId);

		switch (kp->GetKey()) {
			case NkKey::NK_RIGHT:
				idx = (idx + 1) % count;
				break;
			case NkKey::NK_LEFT:
				idx = (idx - 1 + count) % count;
				break;
			case NkKey::NK_DOWN:
				idx = (idx + cols) % count;
				break;
			case NkKey::NK_UP:
				idx = (idx - cols + count) % count;
				break;
			case NkKey::NK_ENTER: {
				const GameInfo *info = GameFactory::GetGameInfo(mSelectedGameId);
				if (info && info->playable)
					LaunchGame(mSelectedGameId);
				return;
			}
			case NkKey::NK_ESCAPE:
				mRunning = false;
				return;
			default:
				return;
		}
		mSelectedGameId = static_cast<GameId>(idx);
	}

	void NkoungPlatformApp::LaunchGame(GameId gameId) noexcept {
		NKOUNG_LOG_INFOF("Lancement du jeu: %d", static_cast<int32>(gameId));

		// Créer et initialiser le jeu
		mCurrentGame = GameFactory::CreateGame(gameId, memory::gDefaultAllocator);
		if (!mCurrentGame) {
			NKOUNG_LOG_ERRORF("Impossible de créer le jeu %d", static_cast<int32>(gameId));
			return;
		}

		mCurrentScene = AppScene::GameScene;
	}

	void NkoungPlatformApp::ParseArguments(const NkVector<NkString> &args) noexcept {
		for (usize i = 0; i < args.Size(); ++i) {
			const NkString &arg = args[i];
			if (arg == "--backend=vulkan" || arg == "-bvk") {
				mGraphicsApi = NkGraphicsApi::NK_GFX_API_VULKAN;
			} else if (arg == "--backend=opengl" || arg == "-bgl") {
				mGraphicsApi = NkGraphicsApi::NK_GFX_API_OPENGL;
			} else if (arg == "--backend=dx11" || arg == "-bdx11") {
				mGraphicsApi = NkGraphicsApi::NK_GFX_API_DX11;
			} else if (arg == "--backend=dx12" || arg == "-bdx12") {
				mGraphicsApi = NkGraphicsApi::NK_GFX_API_DX12;
			} else if (arg == "--backend=software" || arg == "-bsw") {
				mGraphicsApi = NkGraphicsApi::NK_GFX_API_SOFTWARE;
			}
		}
	}

	void NkoungPlatformApp::ReturnToPlatformMenu() noexcept {
		NKOUNG_LOG_INFO("Retour au menu de plateforme");

		if (mCurrentGame) {
			mCurrentGame->Unload();
			mCurrentGame.Reset();
		}

		mCurrentScene = AppScene::PlatformMenu;
	}

} // namespace nkoung
