/**
 * @File    NkUIMultiViewport.cpp
 * @Brief   Implémentation du système multi-viewport NkUI + NkWindow.
 */

#include "NkUIMultiViewport.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

// Événements dont on a besoin pour les fenêtres secondaires
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────
        //  Helpers locaux
        // ─────────────────────────────────────────────────────────────────────

        // Marge en pixels : une fenêtre est considérée "hors viewport" si son
        // centre ou plus de la moitié de sa surface sort de ces bounds.
        static constexpr float32 kOutsideMargin = 32.f; // px de tolérance

        static int32 ToUiBtn(NkMouseButton b) {
            switch (b) {
                case NkMouseButton::NK_MB_LEFT:    return 0;
                case NkMouseButton::NK_MB_RIGHT:   return 1;
                case NkMouseButton::NK_MB_MIDDLE:  return 2;
                default: return static_cast<int32>(b);
            }
        }

        static void AddUtf8ToInput(NkUIInputState& inp, const char* utf8) {
            if (!utf8) return;
            for (int32 i = 0; utf8[i]; ) {
                const uint8 c0 = static_cast<uint8>(utf8[i]);
                uint32 cp = '?'; int32 n = 1;
                if      (c0 < 0x80u)                         { cp = c0; n = 1; }
                else if ((c0 & 0xE0u) == 0xC0u && utf8[i+1]) { cp = ((c0&0x1F)<<6)|(uint8(utf8[i+1])&0x3F); n=2; }
                else if ((c0 & 0xF0u) == 0xE0u && utf8[i+1] && utf8[i+2]) { cp=((c0&0xF)<<12)|((uint8(utf8[i+1])&0x3F)<<6)|(uint8(utf8[i+2])&0x3F); n=3; }
                inp.AddInputChar(cp);
                i += n;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        //  Init / Destroy
        // ─────────────────────────────────────────────────────────────────────

        bool NkUIMultiViewportManager::Init(NkIDevice*         device,
                                             NkRenderPassHandle mainRenderPass,
                                             NkGraphicsApi      api,
                                             NkWindow*          mainWindow,
                                             int32              mainViewW,
                                             int32              mainViewH) noexcept
        {
            mDevice     = device;
            mApi        = api;
            mMainWindow = mainWindow;
            mMainViewW  = mainViewW;
            mMainViewH  = mainViewH;
            mEnabled    = false;
            mCount      = 0;
            ::memset(mViewports, 0, sizeof(mViewports));
            (void)mainRenderPass; // chaque viewport crée son propre renderpass via device
            return device != nullptr && mainWindow != nullptr;
        }

        void NkUIMultiViewportManager::Destroy() noexcept {
            for (int32 i = 0; i < mCount; ++i) {
                if (mViewports[i].alive) {
                    DestroyViewport(i);
                }
            }
            mCount  = 0;
            mDevice = nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  BeginFrame
        // ─────────────────────────────────────────────────────────────────────

        void NkUIMultiViewportManager::BeginFrame(NkUIContext&       ctx,
                                                   NkUIWindowManager& wm,
                                                   float32            dt,
                                                   int32              mainViewW,
                                                   int32              mainViewH) noexcept
        {
            mMainViewW = mainViewW;
            mMainViewH = mainViewH;

            if (!mEnabled) {
                // Mode désactivé : détruire tous les viewports secondaires existants
                for (int32 i = 0; i < mCount; ++i) {
                    if (mViewports[i].alive) {
                        // Rapatrier la fenêtre dans le viewport principal
                        NkUIWindowState* ws = wm.Find(mViewports[i].windowId);
                        if (ws) {
                            // Replace dans les bounds du viewport principal
                            if (ws->pos.x < 0.f)          ws->pos.x = 20.f;
                            if (ws->pos.y < 0.f)          ws->pos.y = 20.f;
                            if (ws->pos.x > mainViewW)    ws->pos.x = static_cast<float32>(mainViewW - 100);
                            if (ws->pos.y > mainViewH)    ws->pos.y = static_cast<float32>(mainViewH - 100);
                        }
                        DestroyViewport(i);
                    }
                }
                mCount = 0;
                return;
            }

            // ── 1. Détruire les viewports dont la fenêtre NkUI est fermée ────
            for (int32 i = 0; i < mCount; ++i) {
                if (!mViewports[i].alive) continue;
                NkUIWindowState* ws = wm.Find(mViewports[i].windowId);
                bool shouldDestroy = (!ws || !ws->isOpen || ws->isDocked);

                // Si la fenêtre OS est fermée par l'utilisateur
                if (mViewports[i].alive && !mViewports[i].osWindow.IsOpen()) {
                    shouldDestroy = true;
                    if (ws) ws->isOpen = false; // propage la fermeture
                }

                if (shouldDestroy) {
                    DestroyViewport(i);
                }
            }

            // ── 2. Détecter les fenêtres flottantes qui sortent du viewport ──
            for (int32 i = 0; i < wm.numWindows; ++i) {
                NkUIWindowState& ws = wm.windows[i];
                if (!ws.isOpen || ws.isDocked) continue;
                if (FindViewport(ws.id) >= 0) continue; // déjà géré

                if (IsOutsideMainViewport(ws)) {
                    if (mCount < MAX_VIEWPORTS) {
                        CreateViewport(ws.id, wm);
                    } else {
                        logger.Warn("[MultiViewport] Max viewports atteint (%d)\n", MAX_VIEWPORTS);
                    }
                }
            }

            // ── 3. Rapatrier les fenêtres OS qui reviennent dans le viewport ─
            for (int32 i = 0; i < mCount; ++i) {
                if (!mViewports[i].alive) continue;
                NkUIWindowState* ws = wm.Find(mViewports[i].windowId);
                if (!ws) continue;

                // Sync position OS → NkUIWindowState
                SyncOSToWindow(mViewports[i], *ws);

                // Si la fenêtre est revenue dans le viewport principal → rapatrier
                if (!IsOutsideMainViewport(*ws)) {
                    // Redevient un overlay flottant standard
                    DestroyViewport(i);
                    continue;
                }
            }

            // ── 4. Pump events des fenêtres OS secondaires ────────────────────
            NkEventSystem& events = NkEvents();

            for (int32 i = 0; i < mCount; ++i) {
                NkUIViewportState& vp = mViewports[i];
                if (!vp.alive) continue;

                NkUIWindowState* ws = wm.Find(vp.windowId);
                if (!ws) continue;

                // Reset de l'input local
                vp.localInput.BeginFrame();

                // On enregistre des callbacks ciblés sur cette fenêtre OS
                // pour isoler les events. On utilise le windowId de la fenêtre OS.
                const NkWindowId osWinId = vp.osWindow.GetId();

                // Resize
                events.AddEventCallback<NkWindowResizeEvent>([&vp](NkWindowResizeEvent* e) {
                    vp.width  = static_cast<uint32>(e->GetWidth());
                    vp.height = static_cast<uint32>(e->GetHeight());
                }, osWinId);

                // Fermeture
                events.AddEventCallback<NkWindowCloseEvent>([&vp, ws](NkWindowCloseEvent*) {
                    vp.alive     = false;
                    if (ws) ws->isOpen = false;
                }, osWinId);

                // Souris move (position relative à la fenêtre OS)
                events.AddEventCallback<NkMouseMoveEvent>([&vp](NkMouseMoveEvent* e) {
                    vp.localInput.SetMousePos(
                        static_cast<float32>(e->GetX()),
                        static_cast<float32>(e->GetY()));
                }, osWinId);

                // Boutons souris
                events.AddEventCallback<NkMouseButtonPressEvent>([&vp](NkMouseButtonPressEvent* e) {
                    vp.localInput.SetMousePos(
                        static_cast<float32>(e->GetX()),
                        static_cast<float32>(e->GetY()));
                    const int32 btn = ToUiBtn(e->GetButton());
                    if (btn >= 0 && btn < 5) vp.localInput.SetMouseButton(btn, true);
                }, osWinId);

                events.AddEventCallback<NkMouseButtonReleaseEvent>([&vp](NkMouseButtonReleaseEvent* e) {
                    vp.localInput.SetMousePos(
                        static_cast<float32>(e->GetX()),
                        static_cast<float32>(e->GetY()));
                    const int32 btn = ToUiBtn(e->GetButton());
                    if (btn >= 0 && btn < 5) vp.localInput.SetMouseButton(btn, false);
                }, osWinId);

                // Molette
                events.AddEventCallback<NkMouseWheelVerticalEvent>([&vp](NkMouseWheelVerticalEvent* e) {
                    vp.localInput.AddMouseWheel(
                        static_cast<float32>(e->GetOffsetY()),
                        static_cast<float32>(e->GetOffsetX()));
                }, osWinId);

                // Clavier (partagé)
                events.AddEventCallback<NkKeyPressEvent>([&vp](NkKeyPressEvent* e) {
                    if (e->GetKey() != NkKey::NK_UNKNOWN)
                        vp.localInput.SetKey(e->GetKey(), true);
                }, osWinId);

                events.AddEventCallback<NkKeyReleaseEvent>([&vp](NkKeyReleaseEvent* e) {
                    if (e->GetKey() != NkKey::NK_UNKNOWN)
                        vp.localInput.SetKey(e->GetKey(), false);
                }, osWinId);

                events.AddEventCallback<NkTextInputEvent>([&vp](NkTextInputEvent* e) {
                    AddUtf8ToInput(vp.localInput, e->GetUtf8());
                }, osWinId);

                // Sync NkUIWindowState pos/size → fenêtre OS (si déplacée par NkUI)
                SyncWindowToOS(vp, *ws);
            }

            (void)ctx;
            (void)dt;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  Submit — rendu dans chaque viewport secondaire
        // ─────────────────────────────────────────────────────────────────────

        void NkUIMultiViewportManager::Submit(NkUIContext& ctx, NkUIWindowManager& wm) noexcept {
            if (!mEnabled) return;

            for (int32 i = 0; i < mCount; ++i) {
                NkUIViewportState& vp = mViewports[i];
                if (!vp.alive || !vp.osWindow.IsOpen()) continue;
                if (!vp.cmd) continue;

                NkUIWindowState* ws = wm.Find(vp.windowId);
                if (!ws || !ws->isOpen) continue;

                // ── Construire une NkUIContext minimale pour ce viewport ───────
                // On réutilise le même contexte mais on remplace le viewport
                // et on réinitialise les layers pour ne rendre QUE cette fenêtre.
                // Stratégie : créer un DrawList temporaire avec juste les commandes
                // de cette fenêtre.

                // Swapchain resize si nécessaire
                const uint32 fbW = vp.osWindow.GetSize().width;
                const uint32 fbH = vp.osWindow.GetSize().height;
                if (fbW == 0 || fbH == 0) continue;

                if (fbW != vp.width || fbH != vp.height) {
                    mDevice->OnResize(fbW, fbH);
                    vp.width  = fbW;
                    vp.height = fbH;
                }

                // ── Frame GPU ─────────────────────────────────────────────────
                NkFrameContext frame;
                if (!mDevice->BeginFrame(frame)) continue;

                const NkRenderPassHandle  rp  = mDevice->GetSwapchainRenderPass();
                const NkFramebufferHandle fbo = mDevice->GetSwapchainFramebuffer();
                const NkRect2D            area{0, 0, (int32)fbW, (int32)fbH};

                vp.cmd->Reset();
                vp.cmd->Begin();

                if (vp.cmd->BeginRenderPass(rp, fbo, area)) {
                    NkViewport viewport{0.f, 0.f, (float32)fbW, (float32)fbH, 0.f, 1.f};
                    vp.cmd->SetViewport(viewport);
                    vp.cmd->SetScissor(area);

                    // NOTE : Pour le rendu dans le viewport secondaire, on
                    // soumet le contexte principal. Les layers contiennent
                    // toutes les fenêtres. Dans la vraie implémentation,
                    // il faudrait isoler les draw calls de cette fenêtre.
                    // Pour l'instant on soumet tout (simple et fonctionnel
                    // pour les cas où on a 1 fenêtre par OS window).
                    vp.backend.Submit(vp.cmd, ctx, fbW, fbH);

                    vp.cmd->EndRenderPass();
                }

                vp.cmd->End();
                mDevice->SubmitAndPresent(vp.cmd);
                mDevice->EndFrame(frame);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        //  IsSecondaryViewport / GetLocalInput
        // ─────────────────────────────────────────────────────────────────────

        bool NkUIMultiViewportManager::IsSecondaryViewport(NkUIID windowId) const noexcept {
            return FindViewport(windowId) >= 0;
        }

        const NkUIInputState* NkUIMultiViewportManager::GetLocalInput(NkUIID windowId) const noexcept {
            const int32 idx = FindViewport(windowId);
            if (idx < 0) return nullptr;
            return &mViewports[idx].localInput;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  CreateViewport
        // ─────────────────────────────────────────────────────────────────────

        bool NkUIMultiViewportManager::CreateViewport(NkUIID windowId, NkUIWindowManager& wm) noexcept {
            NkUIWindowState* ws = wm.Find(windowId);
            if (!ws) return false;

            // Trouve un slot libre
            int32 slot = -1;
            for (int32 i = 0; i < MAX_VIEWPORTS; ++i) {
                if (!mViewports[i].alive) { slot = i; break; }
            }
            if (slot < 0) return false;

            NkUIViewportState& vp = mViewports[slot];
            vp = NkUIViewportState{};
            vp.windowId = windowId;

            // ── Créer la fenêtre OS ────────────────────────────────────────
            NkWindowConfig cfg;
            cfg.title    = ws->name;
            cfg.x        = static_cast<int32>(ws->pos.x);
            cfg.y        = static_cast<int32>(ws->pos.y);
            cfg.width    = static_cast<uint32>(ws->size.x > 10.f ? ws->size.x : 400.f);
            cfg.height   = static_cast<uint32>(ws->size.y > 10.f ? ws->size.y : 300.f);
            cfg.resizable= true;
            cfg.movable  = true;
            cfg.closable = true;
            cfg.frame    = !HasFlag(ws->flags, NkUIWindowFlags::NK_NO_TITLE_BAR);
            cfg.visible  = true;
            cfg.centered = false;

            // Partage le pixel format avec la fenêtre principale si OpenGL/WGL
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mApi == NkGraphicsApi::NK_API_OPENGL && mMainWindow) {
                cfg.native.win32PixelFormatShareWindowHandle =
                    reinterpret_cast<uintptr>(mMainWindow->mData.mHwnd);
            }
#endif

            if (!vp.osWindow.Create(cfg)) {
                logger.Errorf("[MultiViewport] Impossible de créer la fenêtre OS pour '%s'\n", ws->name);
                return false;
            }

            vp.width  = vp.osWindow.GetSize().width;
            vp.height = vp.osWindow.GetSize().height;

            // ── Créer le backend GPU ───────────────────────────────────────
            const NkRenderPassHandle rp = mDevice->GetSwapchainRenderPass();
            if (!vp.backend.Init(mDevice, rp, mApi)) {
                logger.Errorf("[MultiViewport] Backend GPU échec pour '%s'\n", ws->name);
                vp.osWindow.Close();
                return false;
            }

            // ── Créer le command buffer ────────────────────────────────────
            vp.cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
            if (!vp.cmd || !vp.cmd->IsValid()) {
                logger.Errorf("[MultiViewport] CommandBuffer échec pour '%s'\n", ws->name);
                vp.backend.Destroy();
                vp.osWindow.Close();
                return false;
            }

            vp.alive = true;
            if (slot >= mCount) mCount = slot + 1;

            logger.Infof("[MultiViewport] Viewport secondaire créé pour '%s' (%dx%d @ %d,%d)\n",
                         ws->name, vp.width, vp.height, cfg.x, cfg.y);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  DestroyViewport
        // ─────────────────────────────────────────────────────────────────────

        void NkUIMultiViewportManager::DestroyViewport(int32 idx) noexcept {
            if (idx < 0 || idx >= MAX_VIEWPORTS) return;
            NkUIViewportState& vp = mViewports[idx];
            if (!vp.alive) return;

            if (mDevice && vp.cmd) {
                mDevice->WaitIdle();
                mDevice->DestroyCommandBuffer(vp.cmd);
                vp.cmd = nullptr;
            }
            vp.backend.Destroy();
            if (vp.osWindow.IsOpen()) vp.osWindow.Close();

            vp.alive    = false;
            vp.windowId = NKUI_ID_NONE;

            // Recalcule mCount
            mCount = 0;
            for (int32 i = MAX_VIEWPORTS - 1; i >= 0; --i) {
                if (mViewports[i].alive) { mCount = i + 1; break; }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        //  FindViewport
        // ─────────────────────────────────────────────────────────────────────

        int32 NkUIMultiViewportManager::FindViewport(NkUIID windowId) const noexcept {
            for (int32 i = 0; i < mCount; ++i) {
                if (mViewports[i].alive && mViewports[i].windowId == windowId)
                    return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  IsOutsideMainViewport
        // ─────────────────────────────────────────────────────────────────────

        bool NkUIMultiViewportManager::IsOutsideMainViewport(const NkUIWindowState& ws) const noexcept {
            // La fenêtre est "hors" si plus de la moitié de sa surface est hors des bounds
            const float32 cx = ws.pos.x + ws.size.x * 0.5f;
            const float32 cy = ws.pos.y + ws.size.y * 0.5f;

            const bool outsideX = cx < -kOutsideMargin
                                || cx > static_cast<float32>(mMainViewW) + kOutsideMargin;
            const bool outsideY = cy < -kOutsideMargin
                                || cy > static_cast<float32>(mMainViewH) + kOutsideMargin;

            return outsideX || outsideY;
        }

        // ─────────────────────────────────────────────────────────────────────
        //  SyncWindowToOS — NkUIWindowState → fenêtre OS
        // ─────────────────────────────────────────────────────────────────────

        void NkUIMultiViewportManager::SyncWindowToOS(NkUIViewportState& vp, NkUIWindowState& ws) noexcept {
            if (!vp.alive || !vp.osWindow.IsOpen()) return;

            const auto osPos  = vp.osWindow.GetPosition();
            const auto osSize = vp.osWindow.GetSize();

            // NkUI a bougé la fenêtre → met à jour l'OS
            const float32 epsilon = 2.f;
            const bool posChanged  = ::fabsf(ws.pos.x  - static_cast<float32>(osPos.x))  > epsilon
                                  || ::fabsf(ws.pos.y  - static_cast<float32>(osPos.y))  > epsilon;
            const bool sizeChanged = ::fabsf(ws.size.x - static_cast<float32>(osSize.x)) > epsilon
                                  || ::fabsf(ws.size.y - static_cast<float32>(osSize.y)) > epsilon;

            if (posChanged)
                vp.osWindow.SetPosition(static_cast<int32>(ws.pos.x), static_cast<int32>(ws.pos.y));
            if (sizeChanged)
                vp.osWindow.SetSize(static_cast<uint32>(ws.size.x), static_cast<uint32>(ws.size.y));
        }

        // ─────────────────────────────────────────────────────────────────────
        //  SyncOSToWindow — fenêtre OS → NkUIWindowState
        // ─────────────────────────────────────────────────────────────────────

        void NkUIMultiViewportManager::SyncOSToWindow(NkUIViewportState& vp, NkUIWindowState& ws) noexcept {
            if (!vp.alive || !vp.osWindow.IsOpen()) return;

            // L'OS a bougé la fenêtre (drag natif) → met à jour NkUI
            const auto osPos  = vp.osWindow.GetPosition();
            const auto osSize = vp.osWindow.GetSize();
            ws.pos.x  = static_cast<float32>(osPos.x);
            ws.pos.y  = static_cast<float32>(osPos.y);
            ws.size.x = static_cast<float32>(osSize.x);
            ws.size.y = static_cast<float32>(osSize.y);
        }

    } // namespace nkui
} // namespace nkentseu