#include "UILayer.h"
#include "Noge/Core/NkApplication.h"
#include "NKRenderer/NkRenderer.h" // SetUIOverlayCallback (passe Overlay2D)
#include "NKLogger/NkLog.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkui;

		// ── Trampoline d'upload des atlas de polices (Gray8 → backend RHI) ────
		// Même mécanisme que la démo de référence NkUIDemoNKEngine (Base04) :
		// NkUIFontManager::UploadDirtyAtlases prend un pointeur de fonction C.
		static NkUIRHIBackend *sFontUploadBackend = nullptr;

		static void FontAtlasUploadGray8(uint32 texId, const uint8 *data, int32 w, int32 h) {
			if (sFontUploadBackend)
				sFontUploadBackend->UploadTextureGray8(texId, data, w, h);
		}

		UILayer::UILayer(const NkString &name, NkIDevice *device, NkICommandBuffer *cmd, NkGraphicsApi api) noexcept
			: NkOverlay(name), mDevice(device), mCmd(cmd), mApi(api) {
		}

		UILayer::~UILayer() = default;

		// =====================================================================
		void UILayer::OnAttach() {
			nk_uint32 W = mDevice ? mDevice->GetSwapchainWidth() : 1280;
			nk_uint32 H = mDevice ? mDevice->GetSwapchainHeight() : 720;

			NkUIFontConfig fontCfg;
			fontCfg.yAxisUp = false;
			fontCfg.enableAtlas = true;
			fontCfg.enableBitmapFallback = true;
			fontCfg.defaultFontSize = 14.f;

			if (!mCtx.Init((nk_int32)W, (nk_int32)H, fontCfg)) {
				logger.Errorf("[UILayer] NkUIContext::Init échoué — UI désactivée (pas de police par défaut)\n");
				return; // mUIReady reste false → OnUIRender ne dessine rien
			}
			// Garantie post-Init : fontManager.Default() != nullptr (police
			// bitmap intégrée ajoutée par NkUIFontManager::Init/AddBuiltin).
			mUIReady = true;
			mCtx.SetTheme(NkUITheme::Dark());
			mWM.Init();

			float32 menuH = mCtx.theme.metrics.titleBarHeight;
			mDock.Init({0.f, menuH, (float32)W, (float32)H - menuH});

			// [FIX 2026-07-25] Câblage du RENDU NKUI (l'UI était construite mais
			// jamais soumise au GPU → écran gris). Pattern de la démo Base04 :
			//  1. Backend NKUI→NKRHI initialisé sur la render pass swapchain.
			//  2. Atlas de polices uploadés sur le GPU (Gray8 → RGBA8).
			//  3. Callback enregistré dans la passe Overlay2D du render graph :
			//     Submit(ctx.layers[]) dans une render pass active chaque frame.
			if (mDevice) {
				if (mBackend.Init(mDevice, mDevice->GetSwapchainRenderPass(), mApi)) {
					mBackendReady = true;
					sFontUploadBackend = &mBackend;
					mCtx.fontManager.UploadDirtyAtlases(reinterpret_cast<void *>(&FontAtlasUploadGray8));

					auto *renderer = NkApplication::Get().GetRenderer();
					if (renderer) {
						renderer->SetUIOverlayCallback([this](NkICommandBuffer *cmd) {
							if (!mBackendReady || !mDevice)
								return;
							mBackend.Submit(cmd, mCtx, mDevice->GetSwapchainWidth(), mDevice->GetSwapchainHeight());
						});
					} else {
						logger.Errorf("[UILayer] GetRenderer() nul — UI non soumise au render graph\n");
					}
				} else {
					logger.Errorf("[UILayer] NkUIRHIBackend::Init échoué — UI non rendue\n");
				}
			}

			// Initialiser les panels
			if (mEditorLayer) {
				mAssetBrowser.Init(&mEditorLayer->GetAssetManager(), ".");
			}

			// Log sink : transmet les messages NkLogger → ConsolePanel
			// (Enregistrement du sink NkLogger — Phase 3 quand NkLoggerSink est implémenté)

			logger.Infof("[UILayer] Attaché {}x{} (backend RHI: {})\n", W, H, mBackendReady ? 1 : 0);
		}

		void UILayer::OnDetach() {
			// Désenregistrer le callback AVANT de détruire le backend (le render
			// graph ne doit plus nous appeler pendant/après la destruction).
			if (auto *renderer = NkApplication::Get().GetRenderer())
				renderer->SetUIOverlayCallback(renderer::NkUIOverlayCallback{});
			if (sFontUploadBackend == &mBackend)
				sFontUploadBackend = nullptr;
			if (mDevice)
				mDevice->WaitIdle();
			if (mBackendReady) {
				mBackend.Destroy();
				mBackendReady = false;
			}
			mDL = nullptr;
			mCtx.Destroy();
			mWM.Destroy();
			mDock.Destroy();
			logger.Infof("[UILayer] Détaché\n");
		}

		void UILayer::OnUpdate(float dt) {
			(void)dt;
		}

		void UILayer::OnRender() {
		}

		// =====================================================================
		void UILayer::OnUIRender() {
			if (!mDevice || !mUIReady)
				return;

			nk_uint32 W = mDevice->GetSwapchainWidth();
			nk_uint32 H = mDevice->GetSwapchainHeight();
			if (W == 0 || H == 0)
				return;

			mCtx.viewW = (nk_int32)W;
			mCtx.viewH = (nk_int32)H;

			ComputeLayout();

			mInput.dt = 1.f / 60.f;
			mCtx.BeginFrame(mInput, mInput.dt);
			mWM.BeginFrame(mCtx);
			mInput.BeginFrame();
			mLS.depth = 0;

			// [FIX 2026-07-25] Dessiner dans la liste du CONTEXTE (reset par
			// BeginFrame, rendue par NkUIRHIBackend::Submit) — plus de liste
			// orpheline.
			mDL = mCtx.dl;
			if (!mDL)
				return;

			RenderMenuBar();
			RenderViewport();
			if (mShowSceneTree)
				RenderSceneTree();
			if (mShowInspector)
				RenderInspector();
			if (mShowAssetBrowser)
				RenderAssetBrowser();
			if (mShowConsole)
				RenderConsole();

			mCtx.EndFrame();
			mWM.EndFrame(mCtx);
		}

		// =====================================================================
		bool UILayer::OnEvent(NkEvent *event) {
			if (!event)
				return false;
			UpdateInputState(event);
			// Ne pas consommer les events — EditorLayer les traite aussi
			return false;
		}

		// =====================================================================
		void UILayer::UpdateInputState(const NkEvent *event) noexcept {
			if (!event)
				return;

			// As<T>() est non-const dans NkEvent — cast local contrôlé
			NkEvent *ev = const_cast<NkEvent *>(event);

			if (auto *mm = ev->As<NkMouseMoveEvent>()) {
				float32 x = (float32)mm->GetX();
				float32 y = (float32)mm->GetY();
				mInput.SetMousePos(x, y);

				// Transmettre delta à ViewportLayer si la souris est dans le viewport
				if (mViewportLayer) {
					auto &ms = mViewportLayer->GetMouseState();
					if (ms.isHovered) {
						ms.dx = x - mPrevMouseX;
						ms.dy = y - mPrevMouseY;
						ms.x = x - mLayout.viewport.x;
						ms.y = y - mLayout.viewport.y;
					}
				}
				mPrevMouseX = x;
				mPrevMouseY = y;
				return;
			}

			if (auto *mbp = ev->As<NkMouseButtonPressEvent>()) {
				int btn = (mbp->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
						  : (mbp->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
						  : (mbp->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																			  : -1;
				if (btn >= 0) {
					mInput.SetMouseButton(btn, true);
					if (mViewportLayer) {
						auto &ms = mViewportLayer->GetMouseState();
						if (btn == 0)
							ms.leftDown = true;
						if (btn == 1)
							ms.rightDown = true;
					}
				}
				return;
			}

			if (auto *mbr = ev->As<NkMouseButtonReleaseEvent>()) {
				int btn = (mbr->GetButton() == NkMouseButton::NK_MB_LEFT)	  ? 0
						  : (mbr->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
						  : (mbr->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2
																			  : -1;
				if (btn >= 0) {
					mInput.SetMouseButton(btn, false);
					if (mViewportLayer) {
						auto &ms = mViewportLayer->GetMouseState();
						if (btn == 0)
							ms.leftDown = false;
						if (btn == 1)
							ms.rightDown = false;
					}
				}
				return;
			}

			if (auto *mw = ev->As<NkMouseWheelVerticalEvent>()) {
				float32 delta = (float32)mw->GetDeltaY();
				mInput.AddMouseWheel(delta);
				if (mViewportLayer) {
					auto &ms = mViewportLayer->GetMouseState();
					if (ms.isHovered)
						ms.scroll += delta;
				}
				return;
			}

			if (auto *kp = ev->As<NkKeyPressEvent>()) {
				mInput.SetKey(kp->GetKey(), true);
				// Alt pour l'orbite
				if (mViewportLayer) {
					auto &ms = mViewportLayer->GetMouseState();
					ms.altDown = kp->HasAlt();
				}
				return;
			}

			if (auto *kr = ev->As<NkKeyReleaseEvent>()) {
				mInput.SetKey(kr->GetKey(), false);
				if (mViewportLayer) {
					auto &ms = mViewportLayer->GetMouseState();
					ms.altDown = kr->HasAlt();
				}
				return;
			}

			if (auto *te = ev->As<NkTextInputEvent>()) {
				mInput.AddInputChar(te->GetCodepoint());
				return;
			}
		}

		// =====================================================================
		void UILayer::ComputeLayout() noexcept {
			float32 W = (float32)mCtx.viewW;
			float32 H = (float32)mCtx.viewH;
			float32 menuH = mCtx.theme.metrics.titleBarHeight;

			float32 leftW = W * 0.22f;
			float32 rightW = W * 0.22f;
			float32 vpW = W - leftW - rightW;
			float32 topH = (H - menuH) * 0.65f;
			float32 botH = H - menuH - topH;

			mLayout.menuBar = {0, 0, W, menuH};
			mLayout.sceneTree = {0, menuH, leftW, topH};
			mLayout.viewport = {leftW, menuH, vpW, topH};
			mLayout.inspector = {leftW + vpW, menuH, rightW, topH};
			mLayout.assetBrowser = {0, menuH + topH, W * 0.6f, botH};
			mLayout.console = {W * 0.6f, menuH + topH, W * 0.4f, botH};

			// Mettre à jour le FBO si le viewport a changé
			if (mViewportLayer) {
				nk_uint32 vpWu = (nk_uint32)vpW;
				nk_uint32 vpHu = (nk_uint32)topH;
				if (vpWu != mViewportLayer->GetViewportWidth() || vpHu != mViewportLayer->GetViewportHeight()) {
					mViewportLayer->ResizeFBO(vpWu, vpHu);
				}

				// Mettre à jour isHovered
				float32 mx = mInput.mousePos.x;
				float32 my = mInput.mousePos.y;
				auto &ms = mViewportLayer->GetMouseState();
				ms.isHovered = (mx >= mLayout.viewport.x && mx < mLayout.viewport.x + mLayout.viewport.w &&
								my >= mLayout.viewport.y && my < mLayout.viewport.y + mLayout.viewport.h);
			}
		}

		// =====================================================================
		void UILayer::RenderMenuBar() noexcept {
			NkUIFont &font = *mCtx.fontManager.Default();

			if (!NkUIMenu::BeginMenuBar(mCtx, *mDL, font, mLayout.menuBar))
				return;

			// ── Menu Fichier ──────────────────────────────────────────────────
			if (NkUIMenu::BeginMenu(mCtx, *mDL, font, "Fichier")) {
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Nouveau projet", "Ctrl+N")) {
					// TODO : NewProjectDialog
				}
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Ouvrir projet", "Ctrl+O")) {
					// TODO : FileDialog → mEditorLayer->GetProjectManager().Load(path)
				}
				NkUIMenu::Separator(mCtx, *mDL);
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Sauvegarder", "Ctrl+S")) {
					if (mEditorLayer)
						mEditorLayer->GetProjectManager().Save();
				}
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Sauvegarder sous…", "Ctrl+Shift+S")) {
					// TODO : FileDialog
				}
				NkUIMenu::Separator(mCtx, *mDL);
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Quitter", "Alt+F4")) {
					NkApplication::Get().Quit();
				}
				NkUIMenu::EndMenu(mCtx);
			}

			// ── Menu Édition ──────────────────────────────────────────────────
			if (NkUIMenu::BeginMenu(mCtx, *mDL, font, "Édition")) {
				bool canUndo = mEditorLayer && mEditorLayer->GetHistory().CanUndo();
				bool canRedo = mEditorLayer && mEditorLayer->GetHistory().CanRedo();

				NkString undoLabel = "Annuler";
				NkString redoLabel = "Rétablir";
				if (canUndo) {
					undoLabel += " ";
					undoLabel += mEditorLayer->GetHistory().UndoName();
				}
				if (canRedo) {
					redoLabel += " ";
					redoLabel += mEditorLayer->GetHistory().RedoName();
				}

				if (NkUIMenu::MenuItem(mCtx, *mDL, font, undoLabel.CStr(), "Ctrl+Z", nullptr, canUndo)) {
					mEditorLayer->GetHistory().Undo();
				}
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, redoLabel.CStr(), "Ctrl+Y", nullptr, canRedo)) {
					mEditorLayer->GetHistory().Redo();
				}
				NkUIMenu::Separator(mCtx, *mDL);
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Supprimer", "Suppr")) {
					if (mEditorLayer) {
						// TODO : déclencher la suppression (EditorLayer::DeleteSelectedEntity)
					}
				}
				NkUIMenu::EndMenu(mCtx);
			}

			// ── Menu Affichage ────────────────────────────────────────────────
			if (NkUIMenu::BeginMenu(mCtx, *mDL, font, "Affichage")) {
				NkUIMenu::MenuItem(mCtx, *mDL, font, "Hiérarchie", nullptr, &mShowSceneTree);
				NkUIMenu::MenuItem(mCtx, *mDL, font, "Inspecteur", nullptr, &mShowInspector);
				NkUIMenu::MenuItem(mCtx, *mDL, font, "Assets", nullptr, &mShowAssetBrowser);
				NkUIMenu::MenuItem(mCtx, *mDL, font, "Console", nullptr, &mShowConsole);
				NkUIMenu::Separator(mCtx, *mDL);
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Réinitialiser layout")) {
					mShowSceneTree = mShowInspector = mShowAssetBrowser = mShowConsole = true;
				}
				NkUIMenu::EndMenu(mCtx);
			}

			// ── Menu Projet ───────────────────────────────────────────────────
			if (NkUIMenu::BeginMenu(mCtx, *mDL, font, "Projet")) {
				bool playing = mEditorLayer && mEditorLayer->IsPlaying();
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, playing ? "Arrêter" : "Lancer", "F5")) {
					if (mEditorLayer) {
						if (playing)
							mEditorLayer->Stop();
						else
							mEditorLayer->Play();
					}
				}
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Pause", "F6", nullptr, playing)) {
					if (mEditorLayer)
						mEditorLayer->Pause();
				}
				NkUIMenu::Separator(mCtx, *mDL);
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "Paramètres du projet…")) {
					// TODO : ProjectSettingsDialog
				}
				NkUIMenu::EndMenu(mCtx);
			}

			// ── Menu Aide ─────────────────────────────────────────────────────
			if (NkUIMenu::BeginMenu(mCtx, *mDL, font, "Aide")) {
				if (NkUIMenu::MenuItem(mCtx, *mDL, font, "À propos d'Noge")) {
					// TODO : AboutDialog
				}
				NkUIMenu::EndMenu(mCtx);
			}

			NkUIMenu::EndMenuBar(mCtx);
		}

		// =====================================================================
		void UILayer::RenderViewport() noexcept {
			NkUIFont &font = *mCtx.fontManager.Default();
			auto &r = mLayout.viewport;

			NkUIWindow::SetNextWindowPos({r.x, r.y});
			NkUIWindow::SetNextWindowSize({r.w, r.h});

			if (!NkUIWindow::Begin(mCtx, mWM, *mDL, font, mLS, "Viewport##vp", nullptr,
								   NkUIWindowFlags::NK_NO_TITLE_BAR | NkUIWindowFlags::NK_NO_RESIZE |
									   NkUIWindowFlags::NK_NO_MOVE | NkUIWindowFlags::NK_NO_SCROLLBAR |
									   NkUIWindowFlags::NK_NO_BACKGROUND)) {
				NkUIWindow::End(mCtx, mWM, *mDL, mLS);
				return;
			}

			if (mViewportLayer) {
				NkTextureHandle tex = mViewportLayer->GetOutputTexture();
				if (tex.IsValid()) {
					// Afficher la texture FBO plein panel
					mDL->AddImage((uint32)tex.id, {r.x, r.y, r.w, r.h}, {0.f, 0.f}, {1.f, 1.f}, NkColor::White);
				} else {
					// Placeholder gris
					mDL->AddRectFilled({r.x, r.y, r.w, r.h}, NkColor{40, 40, 40, 255}, 0.f);
					mDL->AddText({r.x + r.w * 0.5f - 80.f, r.y + r.h * 0.5f}, "Viewport — aucune scène",
								NkColor{100, 100, 100, 255});
				}

				// Indicateur mode gizmo (coin supérieur gauche)
				if (mEditorLayer) {
					const char *modeStr = "T";
					switch (mEditorLayer->GetGizmoSystem().mode) {
						case NkGizmoMode::Rotate:
							modeStr = "R";
							break;
						case NkGizmoMode::Scale:
							modeStr = "S";
							break;
						default:
							modeStr = "T";
							break;
					}
					char modeBuf[32];
					snprintf(modeBuf, sizeof(modeBuf), "[%s] %s", modeStr,
							 mEditorLayer->GetGizmoSystem().space == NkGizmoSpace::World ? "World" : "Local");
					mDL->AddText({r.x + 8.f, r.y + 8.f}, modeBuf, NkColor{200, 200, 200, 200});
				}

				// Indicateur play/stop
				if (mEditorLayer && mEditorLayer->IsPlaying()) {
					mDL->AddRectFilled({r.x + r.w - 60.f, r.y + 4.f, 52.f, 18.f}, NkColor{0, 180, 0, 180}, 4.f);
					mDL->AddText({r.x + r.w - 52.f, r.y + 6.f}, "■ PLAY", NkColor{255, 255, 255, 255});
				}
			}

			NkUIWindow::End(mCtx, mWM, *mDL, mLS);
		}

		// =====================================================================
		void UILayer::RenderSceneTree() noexcept {
			if (!mWorld || !mEditorLayer)
				return;
			mSceneTree.Render(mCtx, mWM, *mDL, *mCtx.fontManager.Default(), mLS, *mWorld, mScene,
							  mEditorLayer->GetSelectionManager(), &mEditorLayer->GetHistory(), mLayout.sceneTree);
		}

		// =====================================================================
		void UILayer::RenderInspector() noexcept {
			if (!mWorld || !mEditorLayer)
				return;
			mInspector.Render(mCtx, mWM, *mDL, *mCtx.fontManager.Default(), mLS, *mWorld,
							  mEditorLayer->GetSelectionManager(), &mEditorLayer->GetHistory(), mLayout.inspector);
		}

		// =====================================================================
		void UILayer::RenderAssetBrowser() noexcept {
			mAssetBrowser.Render(mCtx, mWM, *mDL, *mCtx.fontManager.Default(), mLS, mLayout.assetBrowser);
		}

		// =====================================================================
		void UILayer::RenderConsole() noexcept {
			mConsole.Render(mCtx, mWM, *mDL, *mCtx.fontManager.Default(), mLS, mLayout.console);
		}

	} // namespace noge
} // namespace nkentseu
