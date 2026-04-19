#include "UILayer.h"
#include "NKLogger/NkLog.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include <cstdio>

namespace nkentseu {
    namespace Unkeny {

        using namespace nkui;

        UILayer::UILayer(const NkString& name,
                         NkIDevice* device,
                         NkICommandBuffer* cmd,
                         NkGraphicsApi api) noexcept
            : Overlay(name), mDevice(device), mCmd(cmd), mApi(api) {}

        UILayer::~UILayer() = default;

        // =====================================================================
        void UILayer::OnAttach() {
            nk_uint32 W = mDevice ? mDevice->GetSwapchainWidth()  : 1280;
            nk_uint32 H = mDevice ? mDevice->GetSwapchainHeight() : 720;

            NkUIFontConfig fontCfg;
            fontCfg.yAxisUp              = false;
            fontCfg.enableAtlas          = true;
            fontCfg.enableBitmapFallback = true;
            fontCfg.defaultFontSize      = 14.f;

            if (!mCtx.Init((nk_int32)W, (nk_int32)H, fontCfg)) {
                logger.Errorf("[UILayer] NkUIContext::Init échoué\n");
                return;
            }
            mCtx.SetTheme(NkUITheme::Dark());
            mWM.Init();

            float32 menuH = mCtx.theme.metrics.titleBarHeight;
            mDock.Init({0.f, menuH, (float32)W, (float32)H - menuH});

            mDL.Init();

            // Initialiser les panels
            if (mEditorLayer) {
                mAssetBrowser.Init(&mEditorLayer->GetAssetManager(), ".");
            }

            // Log sink : transmet les messages NkLogger → ConsolePanel
            // (Enregistrement du sink NkLogger — Phase 3 quand NkLoggerSink est implémenté)

            logger.Infof("[UILayer] Attaché {}x{}\n", W, H);
        }

        void UILayer::OnDetach() {
            if (mDevice) mDevice->WaitIdle();
            mDL.Destroy();
            mCtx.Destroy();
            mWM.Destroy();
            mDock.Destroy();
            logger.Infof("[UILayer] Détaché\n");
        }

        void UILayer::OnUpdate(float dt) { (void)dt; }
        void UILayer::OnRender()         {}

        // =====================================================================
        void UILayer::OnUIRender() {
            if (!mDevice) return;

            nk_uint32 W = mDevice->GetSwapchainWidth();
            nk_uint32 H = mDevice->GetSwapchainHeight();
            if (W == 0 || H == 0) return;

            mCtx.viewW = (nk_int32)W;
            mCtx.viewH = (nk_int32)H;

            ComputeLayout();

            mInput.dt = 1.f / 60.f;
            mCtx.BeginFrame(mInput, mInput.dt);
            mWM.BeginFrame(mCtx);
            mInput.BeginFrame();
            mLS.depth = 0;
            mDL.Reset();

            RenderMenuBar();
            RenderViewport();
            if (mShowSceneTree)    RenderSceneTree();
            if (mShowInspector)    RenderInspector();
            if (mShowAssetBrowser) RenderAssetBrowser();
            if (mShowConsole)      RenderConsole();

            mCtx.EndFrame();
            mWM.EndFrame(mCtx);
        }

        // =====================================================================
        bool UILayer::OnEvent(NkEvent* event) {
            if (!event) return false;
            UpdateInputState(event);
            // Ne pas consommer les events — EditorLayer les traite aussi
            return false;
        }

        // =====================================================================
        void UILayer::UpdateInputState(const NkEvent* event) noexcept {
            if (!event) return;

            if (auto* mm = event->As<NkMouseMoveEvent>()) {
                float32 x = (float32)mm->GetX();
                float32 y = (float32)mm->GetY();
                mInput.SetMousePos(x, y);

                // Transmettre delta à ViewportLayer si la souris est dans le viewport
                if (mViewportLayer) {
                    auto& ms = mViewportLayer->GetMouseState();
                    if (ms.isHovered) {
                        ms.dx = x - mPrevMouseX;
                        ms.dy = y - mPrevMouseY;
                        ms.x  = x - mLayout.viewport.x;
                        ms.y  = y - mLayout.viewport.y;
                    }
                }
                mPrevMouseX = x;
                mPrevMouseY = y;
                return;
            }

            if (auto* mb = event->As<NkMouseButtonEvent>()) {
                int btn = (mb->GetButton() == NkMouseButton::NK_LEFT)   ? 0 :
                          (mb->GetButton() == NkMouseButton::NK_RIGHT)  ? 1 :
                          (mb->GetButton() == NkMouseButton::NK_MIDDLE) ? 2 : -1;
                if (btn >= 0) {
                    bool down = mb->IsPressed();
                    mInput.SetMouseButton(btn, down);

                    if (mViewportLayer) {
                        auto& ms = mViewportLayer->GetMouseState();
                        if (btn == 0) ms.leftDown  = down;
                        if (btn == 1) ms.rightDown = down;
                    }
                }
                return;
            }

            if (auto* mw = event->As<NkMouseScrollEvent>()) {
                float32 delta = (float32)mw->GetDeltaY();
                // TODO : mInput.scroll += delta quand NkUIInputState l'expose
                if (mViewportLayer) {
                    auto& ms = mViewportLayer->GetMouseState();
                    if (ms.isHovered) ms.scroll += delta;
                }
                return;
            }

            if (auto* kp = event->As<NkKeyPressEvent>()) {
                mInput.SetKey(static_cast<NkKey>(kp->GetKey()), true);
                // Alt pour l'orbite
                if (mViewportLayer) {
                    auto& ms = mViewportLayer->GetMouseState();
                    ms.altDown = kp->IsAlt();
                }
                return;
            }

            if (auto* kr = event->As<NkKeyReleaseEvent>()) {
                mInput.SetKey(static_cast<NkKey>(kr->GetKey()), false);
                return;
            }

            if (auto* te = event->As<NkTextEvent>()) {
                mInput.AddInputChar(te->GetCodepoint());
                return;
            }
        }

        // =====================================================================
        void UILayer::ComputeLayout() noexcept {
            float32 W     = (float32)mCtx.viewW;
            float32 H     = (float32)mCtx.viewH;
            float32 menuH = mCtx.theme.metrics.titleBarHeight;

            float32 leftW   = W * 0.22f;
            float32 rightW  = W * 0.22f;
            float32 vpW     = W - leftW - rightW;
            float32 topH    = (H - menuH) * 0.65f;
            float32 botH    = H - menuH - topH;

            mLayout.menuBar     = {0, 0, W, menuH};
            mLayout.sceneTree   = {0,         menuH, leftW, topH};
            mLayout.viewport    = {leftW,      menuH, vpW,   topH};
            mLayout.inspector   = {leftW+vpW,  menuH, rightW,topH};
            mLayout.assetBrowser= {0,          menuH+topH, W*0.6f, botH};
            mLayout.console     = {W*0.6f,     menuH+topH, W*0.4f, botH};

            // Mettre à jour le FBO si le viewport a changé
            if (mViewportLayer) {
                nk_uint32 vpWu = (nk_uint32)vpW;
                nk_uint32 vpHu = (nk_uint32)topH;
                if (vpWu != mViewportLayer->GetViewportWidth() ||
                    vpHu != mViewportLayer->GetViewportHeight()) {
                    mViewportLayer->ResizeFBO(vpWu, vpHu);
                }

                // Mettre à jour isHovered
                float32 mx = mInput.mousePos.x;
                float32 my = mInput.mousePos.y;
                auto& ms = mViewportLayer->GetMouseState();
                ms.isHovered = (mx >= mLayout.viewport.x &&
                                mx <  mLayout.viewport.x + mLayout.viewport.w &&
                                my >= mLayout.viewport.y &&
                                my <  mLayout.viewport.y + mLayout.viewport.h);
            }
        }

        // =====================================================================
        void UILayer::RenderMenuBar() noexcept {
            NkUIFont& font = *mCtx.fontManager.GetDefault();

            if (!NkUIMenu::BeginMenuBar(mCtx, mDL, font, mLayout.menuBar)) return;

            // ── Menu Fichier ──────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Fichier")) {
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Nouveau projet",  "Ctrl+N")) {
                    // TODO : NewProjectDialog
                }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Ouvrir projet",   "Ctrl+O")) {
                    // TODO : FileDialog → mEditorLayer->GetProjectManager().Load(path)
                }
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Sauvegarder",     "Ctrl+S")) {
                    if (mEditorLayer) mEditorLayer->GetProjectManager().Save();
                }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Sauvegarder sous…","Ctrl+Shift+S")) {
                    // TODO : FileDialog
                }
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Quitter",          "Alt+F4")) {
                    EventBus::Dispatch(NkWindowCloseEvent{});
                }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Édition ──────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Édition")) {
                bool canUndo = mEditorLayer && mEditorLayer->GetHistory().CanUndo();
                bool canRedo = mEditorLayer && mEditorLayer->GetHistory().CanRedo();

                NkString undoLabel = "Annuler";
                NkString redoLabel = "Rétablir";
                if (canUndo) undoLabel += " " + mEditorLayer->GetHistory().UndoName();
                if (canRedo) redoLabel += " " + mEditorLayer->GetHistory().RedoName();

                if (NkUIMenu::MenuItem(mCtx, mDL, font,
                                       undoLabel.CStr(), "Ctrl+Z",
                                       nullptr, canUndo)) {
                    mEditorLayer->GetHistory().Undo();
                }
                if (NkUIMenu::MenuItem(mCtx, mDL, font,
                                       redoLabel.CStr(), "Ctrl+Y",
                                       nullptr, canRedo)) {
                    mEditorLayer->GetHistory().Redo();
                }
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Supprimer", "Suppr")) {
                    if (mEditorLayer) {
                        // Déclencher la suppression via EventBus
                    }
                }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Affichage ────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Affichage")) {
                NkUIMenu::MenuItem(mCtx, mDL, font, "Hiérarchie",    nullptr, &mShowSceneTree);
                NkUIMenu::MenuItem(mCtx, mDL, font, "Inspecteur",    nullptr, &mShowInspector);
                NkUIMenu::MenuItem(mCtx, mDL, font, "Assets",        nullptr, &mShowAssetBrowser);
                NkUIMenu::MenuItem(mCtx, mDL, font, "Console",       nullptr, &mShowConsole);
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Réinitialiser layout")) {
                    mShowSceneTree = mShowInspector = mShowAssetBrowser = mShowConsole = true;
                }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Projet ───────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Projet")) {
                bool playing = mEditorLayer && mEditorLayer->IsPlaying();
                if (NkUIMenu::MenuItem(mCtx, mDL, font,
                                       playing ? "Arrêter" : "Lancer", "F5")) {
                    if (mEditorLayer) {
                        if (playing) mEditorLayer->Stop();
                        else         mEditorLayer->Play();
                    }
                }
                if (NkUIMenu::MenuItem(mCtx, mDL, font,
                                       "Pause", "F6",
                                       nullptr, playing)) {
                    if (mEditorLayer) mEditorLayer->Pause();
                }
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Paramètres du projet…")) {
                    // TODO : ProjectSettingsDialog
                }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Aide ─────────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Aide")) {
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "À propos d'Unkeny")) {
                    // TODO : AboutDialog
                }
                NkUIMenu::EndMenu(mCtx);
            }

            NkUIMenu::EndMenuBar(mCtx);
        }

        // =====================================================================
        void UILayer::RenderViewport() noexcept {
            NkUIFont& font = *mCtx.fontManager.GetDefault();
            auto& r = mLayout.viewport;

            NkUIWindow::SetNextWindowPos ({r.x, r.y});
            NkUIWindow::SetNextWindowSize({r.w, r.h});

            if (!NkUIWindow::Begin(mCtx, mWM, mDL, font, mLS, "Viewport##vp",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_TITLE_BAR |
                                   NkUIWindowFlags::NK_NO_RESIZE    |
                                   NkUIWindowFlags::NK_NO_MOVE      |
                                   NkUIWindowFlags::NK_NO_SCROLLBAR |
                                   NkUIWindowFlags::NK_NO_BACKGROUND)) {
                NkUIWindow::End(mCtx, mWM, mDL, mLS);
                return;
            }

            if (mViewportLayer) {
                NkTextureHandle tex = mViewportLayer->GetOutputTexture();
                if (tex.IsValid()) {
                    // Afficher la texture FBO plein panel
                    mDL.AddImage(
                        (nk_uint32)tex.id,
                        {r.x, r.y, r.w, r.h},
                        {0.f, 0.f}, {1.f, 1.f},
                        NkColor::White
                    );
                } else {
                    // Placeholder gris
                    mDL.AddRectFilled({r.x, r.y, r.w, r.h},
                                      {40, 40, 40, 255}, 0.f);
                    mDL.AddText({r.x + r.w*0.5f - 80.f, r.y + r.h*0.5f},
                                "Viewport — aucune scène",
                                {100, 100, 100, 255});
                }

                // Indicateur mode gizmo (coin supérieur gauche)
                if (mEditorLayer) {
                    const char* modeStr = "T";
                    switch (mEditorLayer->GetGizmoSystem().mode) {
                        case NkGizmoMode::Rotate:    modeStr = "R"; break;
                        case NkGizmoMode::Scale:     modeStr = "S"; break;
                        default:                     modeStr = "T"; break;
                    }
                    char modeBuf[32];
                    snprintf(modeBuf, sizeof(modeBuf), "[%s] %s",
                             modeStr,
                             mEditorLayer->GetGizmoSystem().space ==
                                 NkGizmoSpace::World ? "World" : "Local");
                    mDL.AddText({r.x + 8.f, r.y + 8.f},
                                modeBuf, {200, 200, 200, 200});
                }

                // Indicateur play/stop
                if (mEditorLayer && mEditorLayer->IsPlaying()) {
                    mDL.AddRectFilled({r.x + r.w - 60.f, r.y + 4.f, 52.f, 18.f},
                                      {0, 180, 0, 180}, 4.f);
                    mDL.AddText({r.x + r.w - 52.f, r.y + 6.f},
                                "■ PLAY", {255, 255, 255, 255});
                }
            }

            NkUIWindow::End(mCtx, mWM, mDL, mLS);
        }

        // =====================================================================
        void UILayer::RenderSceneTree() noexcept {
            if (!mWorld || !mEditorLayer) return;
            mSceneTree.Render(mCtx, mWM, mDL,
                              *mCtx.fontManager.GetDefault(), mLS,
                              *mWorld, mScene,
                              mEditorLayer->GetSelectionManager(),
                              &mEditorLayer->GetHistory(),
                              mLayout.sceneTree);
        }

        // =====================================================================
        void UILayer::RenderInspector() noexcept {
            if (!mWorld || !mEditorLayer) return;
            mInspector.Render(mCtx, mWM, mDL,
                              *mCtx.fontManager.GetDefault(), mLS,
                              *mWorld,
                              mEditorLayer->GetSelectionManager(),
                              &mEditorLayer->GetHistory(),
                              mLayout.inspector);
        }

        // =====================================================================
        void UILayer::RenderAssetBrowser() noexcept {
            mAssetBrowser.Render(mCtx, mWM, mDL,
                                 *mCtx.fontManager.GetDefault(), mLS,
                                 mLayout.assetBrowser);
        }

        // =====================================================================
        void UILayer::RenderConsole() noexcept {
            mConsole.Render(mCtx, mWM, mDL, *mCtx.fontManager.GetDefault(), mLS, mLayout.console);
        }

    } // namespace Unkeny
} // namespace nkentseu
