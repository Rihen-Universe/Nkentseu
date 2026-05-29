// =============================================================================
// CameraMultiDemo.cpp -- Grille 2x2 de cameras simultanees via NkMultiCamera.
//
// Refonte OpenGL pur (glad) : utilise GLContext + CameraGLTexture +
// FullscreenBlit. Aucune dependance NkTexture / NkSprite / NkIRenderer2D.
//
// Ouvre jusqu'a 4 cameras via NkMultiCamera et les affiche dans une grille
// 2x2 (chaque quadrant = un flux). Les quadrants sans camera laissent le
// fond de clear visible. Echap = quitter.
// =============================================================================

#include "Multi/CameraMultiDemo.h"

#include "NKWindow/NKWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"

#include "NKCamera/NkCameraSystem.h"

#include "Render/GLContext.h"
#include "Render/CameraGLTexture.h"
#include "Render/FullscreenBlit.h"

namespace nkentseu {
    namespace cameradem {

        // Max de quadrants gere par la grille (toujours 2x2).
        static constexpr uint32 NKENTSEU_CAM_MULTI_MAX = 4;

        // ---------------------------------------------------------------------
        // Slot par quadrant : pointe sur un stream NkMultiCamera + cache GPU.
        // ---------------------------------------------------------------------
        struct CameraSlot {
            NkMultiCamera::Stream* stream  = nullptr;
            uint32                 devIdx  = 0;
            NkString               name;
            CameraGLTexture        texture;
            bool                   everValid = false;
        };

        // ---------------------------------------------------------------------
        // Renvoie le rect NDC du quadrant `index` dans une grille 2x2.
        // Layout : 0 = haut-gauche, 1 = haut-droite, 2 = bas-gauche, 3 = bas-droite
        // NDC : (-1, -1) bottom-left, (+1, +1) top-right.
        // ---------------------------------------------------------------------
        struct NdcRect { float x, y, w, h; };

        static NdcRect QuadrantNdc(uint32 index)
        {
            // index modulo 2 = colonne (0 = gauche, 1 = droite)
            // index / 2     = ligne   (0 = haut,   1 = bas)
            // En NDC haut = +1, donc rang 0 (haut) => y = 0..+1, rang 1 (bas) => y = -1..0.
            const float col = (float)(index % 2);
            const float row = (float)(index / 2);
            const float x = -1.f + col;          // -1 ou 0
            const float y =  0.f - row;          //  0 ou -1
            return { x, y, 1.f, 1.f };
        }

        // Aspect-fit a l'interieur d'un quadrant NDC (avec un padding NDC).
        static NdcRect AspectFitInQuadrant(uint32 srcW, uint32 srcH,
                                           const NdcRect& q, float padding)
        {
            const float availW = q.w - 2.f * padding;
            const float availH = q.h - 2.f * padding;
            if (srcW == 0 || srcH == 0 || availW <= 0.f || availH <= 0.f) {
                return q;
            }
            const float srcAR = (float)srcW / (float)srcH;
            const float dstAR = availW / availH;
            float outW = availW;
            float outH = availH;
            if (srcAR > dstAR) outH = availW / srcAR;
            else               outW = availH * srcAR;
            return {
                q.x + padding + (availW - outW) * 0.5f,
                q.y + padding + (availH - outH) * 0.5f,
                outW, outH
            };
        }

        // =====================================================================
        // Point d'entree de la demo multi.
        // =====================================================================
        int RunCameraMultiDemo(const NkEntryState& /*state*/)
        {
            // -- Fenetre ------------------------------------------------------
            NkWindowConfig cfg;
            cfg.title     = "NkCameraDemos -- Multi (2x2)";
            cfg.width     = 1280;
            cfg.height    = 720;
            cfg.centered  = true;
            cfg.resizable = true;

            NkWindow window(cfg);
            if (!window.IsOpen()) {
                logger.Error("[Multi] Echec creation fenetre");
                return -1;
            }

            // -- Contexte OpenGL ---------------------------------------------
            GLContext gl;
            if (!gl.Init(window)) {
                logger.Error("[Multi] GLContext::Init a echoue");
                window.Close();
                return -2;
            }

            FullscreenBlit blit;
            if (!blit.Initialize()) {
                logger.Error("[Multi] FullscreenBlit::Initialize a echoue");
                gl.Shutdown();
                window.Close();
                return -3;
            }

            // -- Enumeration et ouverture des cameras ------------------------
            auto& cam = NkCamera();
            if (!cam.Init()) {
                logger.Warnf("[Multi] NkCamera::Init() echoue : %s",
                             cam.GetLastError().CStr());
            }

            NkVector<NkCameraDevice> devices = cam.EnumerateDevices();
            logger.Infof("[Multi] %u camera(s) detectee(s)",
                         (uint32)devices.Size());

            NkMultiCamera multi;
            CameraSlot slots[NKENTSEU_CAM_MULTI_MAX];

            const uint32 toOpen = (uint32)devices.Size() < NKENTSEU_CAM_MULTI_MAX
                                    ? (uint32)devices.Size()
                                    : NKENTSEU_CAM_MULTI_MAX;
            for (uint32 i = 0; i < toOpen; ++i) {
                NkCameraConfig camCfg;
                camCfg.deviceIndex  = devices[i].index;
                // VGA pour limiter la bande passante (4 flux HD saturent l'USB).
                camCfg.preset       = NkCameraResolution::NK_CAM_RES_VGA;
                camCfg.outputFormat = NkPixelFormat::NK_PIXEL_RGBA8;

                NkMultiCamera::Stream& s = multi.Open(devices[i].index, camCfg);
                slots[i].stream = &s;
                slots[i].devIdx = devices[i].index;
                slots[i].name   = devices[i].name;
                logger.Infof("[Multi] Ouverture device[%u] : %s",
                             devices[i].index, devices[i].name.CStr());
            }
            for (uint32 i = toOpen; i < NKENTSEU_CAM_MULTI_MAX; ++i) {
                slots[i].name = NkString::Fmt("Camera {0} -- no signal", i);
            }

            // -- Etat fenetre ------------------------------------------------
            uint32 winW = cfg.width;
            uint32 winH = cfg.height;

            // -- Boucle ------------------------------------------------------
            auto& events = NkEvents();
            bool running = true;
            NkChrono chrono;

            while (running && window.IsOpen()) {
                while (NkEvent* ev = events.PollEvent()) {
                    if (ev->Is<NkWindowCloseEvent>()) {
                        running = false;
                        break;
                    }
                    if (auto* re = ev->As<NkWindowResizeEvent>()) {
                        const uint32 nw = re->GetWidth();
                        const uint32 nh = re->GetHeight();
                        if (nw > 0 && nh > 0 && (nw != winW || nh != winH)) {
                            winW = nw; winH = nh;
                            gl.OnResize(winW, winH);
                        }
                        continue;
                    }
                    if (auto* ke = ev->As<NkKeyPressEvent>()) {
                        if (ke->GetKey() == NkKey::NK_ESCAPE) {
                            running = false;
                            break;
                        }
                    }
                }
                if (!running) break;

                // -- Pull des frames + upload GPU pour chaque slot actif -----
                for (uint32 i = 0; i < NKENTSEU_CAM_MULTI_MAX; ++i) {
                    if (!slots[i].stream) continue;
                    NkCameraFrame frame;
                    if (!slots[i].stream->GetLastFrame(frame)) continue;
                    if (frame.format != NkPixelFormat::NK_PIXEL_RGBA8) {
                        if (!NkCameraSystem::ConvertToRGBA8(frame)) continue;
                    }
                    if (!frame.IsValid()) continue;

                    if (!slots[i].texture.IsValid()
                        || slots[i].texture.Width()  != frame.width
                        || slots[i].texture.Height() != frame.height) {
                        slots[i].texture.Shutdown();
                        if (!slots[i].texture.Create(frame.width, frame.height)) {
                            continue;
                        }
                    }
                    slots[i].texture.Update(frame.data.Data());
                    slots[i].everValid = true;
                }

                // -- Rendu ----------------------------------------------------
                if (!gl.BeginFrame()) {
                    NkChrono::Sleep(16.f);
                    continue;
                }

                blit.Clear(0.03f, 0.03f, 0.06f);

                for (uint32 i = 0; i < NKENTSEU_CAM_MULTI_MAX; ++i) {
                    if (!slots[i].stream || !slots[i].everValid) continue;
                    if (!slots[i].texture.IsValid()) continue;

                    const NdcRect q   = QuadrantNdc(i);
                    // Padding NDC ~0.02 = ~1% du quadrant.
                    const NdcRect fit = AspectFitInQuadrant(
                        slots[i].texture.Width(),
                        slots[i].texture.Height(),
                        q, 0.02f);
                    blit.Draw(slots[i].texture.Id(),
                              fit.x, fit.y, fit.w, fit.h);
                }

                gl.EndFrame();
                gl.Present();

                const auto elapsed = chrono.Elapsed();
                if (elapsed.milliseconds < 16) {
                    NkChrono::Sleep(16 - elapsed.milliseconds);
                } else {
                    NkChrono::YieldThread();
                }
                (void)chrono.Reset();
            }

            // -- Cleanup -----------------------------------------------------
            multi.CloseAll();
            cam.Shutdown();

            for (uint32 i = 0; i < NKENTSEU_CAM_MULTI_MAX; ++i) {
                slots[i].texture.Shutdown();
            }
            blit.Shutdown();
            gl.Shutdown();
            window.Close();
            return 0;
        }

    } // namespace cameradem
} // namespace nkentseu
