// =============================================================================
// CameraFormatDemo.cpp -- Verification cross-format de ConvertToRGBA8.
//
// Refonte OpenGL pur (glad) : utilise GLContext + CameraGLTexture +
// FullscreenBlit. Aucune dependance NkTexture / NkSprite / NkIRenderer2D.
//
// Genere quatre NkCameraFrame synthetiques (YUYV / NV12 / YUV420 I420 /
// MJPEG via NkJPEGCodec::Encode) puis :
//   1. Appelle NkCameraSystem::ConvertToRGBA8 sur chacune.
//   2. Sauvegarde le resultat en PNG via NkCameraSystem::SaveFrameToFile.
//   3. Upload les pixels dans une CameraGLTexture et les affiche en grille 2x2.
//
// Tres utile pour valider les decodeurs sans webcam physique.
// =============================================================================

#include "Format/CameraFormatDemo.h"

#include "NKWindow/NKWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"

#include "NKCamera/NkCameraSystem.h"

// NkImage du namespace nkentseu (NKImage) : utilise pour NkJPEGCodec::Encode.
// On NE FAIT PAS `using namespace renderer;` pour eviter toute ambiguite.
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
// NkJPEGCodec::Encode retourne un buffer alloue via NkAlloc (NKMemory custom),
// PAS std::malloc. Doit etre libere avec NkFree (la doc du header NkJPEGCodec.h
// qui dit "alloue avec malloc / liberer avec free" est trompeuse).
#include "NKMemory/NkAllocator.h"

#include "Render/GLContext.h"
#include "Render/CameraGLTexture.h"
#include "Render/FullscreenBlit.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
    namespace cameradem {

        // Resolution commune aux 4 frames synthetiques.
        static constexpr uint32 NK_FORMAT_TEST_W     = 640;
        static constexpr uint32 NK_FORMAT_TEST_H     = 480;
        static constexpr uint32 NK_FORMAT_TEST_COUNT = 4;

        // =====================================================================
        // Generation de frames synthetiques (recopie du fichier d'origine,
        // lignes 80-220 ; ces builders sont corrects et inchanges).
        //
        // Pour les formats YUV-based on choisit U = V = 128 (chrominance neutre)
        // => le resultat sera un gradient grayscale, ce qui permet de verifier
        // visuellement que la luminance est bien decodee. Pour MJPEG on encode
        // un veritable gradient RGB via NkJPEGCodec.
        // =====================================================================

        // YUYV : entrelace [Y0, U, Y1, V] pour chaque paire (x, x+1).
        // 2 bytes par pixel => stride = w * 2.
        static NkCameraFrame BuildYUYVTestFrame(uint32 w, uint32 h)
        {
            NkCameraFrame f;
            f.width  = w;
            f.height = h;
            f.format = NkPixelFormat::NK_PIXEL_YUYV;
            f.stride = w * 2;
            f.data.Resize((usize)f.stride * h);
            uint8* p = f.data.Data();
            for (uint32 y = 0; y < h; ++y) {
                for (uint32 x = 0; x < w; x += 2) {
                    const uint8 y0 = (uint8)((x       * 255u) / (w - 1));
                    const uint8 y1 = (uint8)(((x + 1) * 255u) / (w - 1));
                    const usize row = (usize)y * f.stride;
                    p[row + (usize)x * 2 + 0] = y0;
                    p[row + (usize)x * 2 + 1] = 128;  // U
                    p[row + (usize)x * 2 + 2] = y1;
                    p[row + (usize)x * 2 + 3] = 128;  // V
                }
            }
            return f;
        }

        // NV12 : plan Y (w*h) suivi du plan UV entrelace (w*h/2).
        static NkCameraFrame BuildNV12TestFrame(uint32 w, uint32 h)
        {
            NkCameraFrame f;
            f.width  = w;
            f.height = h;
            f.format = NkPixelFormat::NK_PIXEL_NV12;
            f.stride = w;
            const usize sizeY  = (usize)w * h;
            const usize sizeUV = (usize)w * (h / 2);
            f.data.Resize(sizeY + sizeUV);
            uint8* p = f.data.Data();
            for (uint32 y = 0; y < h; ++y) {
                for (uint32 x = 0; x < w; ++x) {
                    p[(usize)y * w + x] = (uint8)((x * 255u) / (w - 1));
                }
            }
            std::memset(p + sizeY, 128, sizeUV);
            return f;
        }

        // YUV420 I420 : 3 plans separes Y (w*h), U (w/2 * h/2), V (w/2 * h/2).
        static NkCameraFrame BuildI420TestFrame(uint32 w, uint32 h)
        {
            NkCameraFrame f;
            f.width  = w;
            f.height = h;
            f.format = NkPixelFormat::NK_PIXEL_YUV420;
            f.stride = w;
            const usize sizeY = (usize)w * h;
            const usize sizeC = (usize)(w / 2) * (h / 2);
            f.data.Resize(sizeY + 2 * sizeC);
            uint8* p = f.data.Data();
            for (uint32 y = 0; y < h; ++y) {
                for (uint32 x = 0; x < w; ++x) {
                    p[(usize)y * w + x] = (uint8)((x * 255u) / (w - 1));
                }
            }
            std::memset(p + sizeY,         128, sizeC);  // plan U
            std::memset(p + sizeY + sizeC, 128, sizeC);  // plan V
            return f;
        }

        // MJPEG : encode un veritable NkImage RGB24 (3 bandes verticales R/G/B
        // avec gradient horizontal) via NkJPEGCodec::Encode.
        static NkCameraFrame BuildMJPEGTestFrame(uint32 w, uint32 h)
        {
            NkCameraFrame f;
            f.format = NkPixelFormat::NK_PIXEL_MJPEG;

            // Qualification ::nkentseu::NkImage obligatoire pour expliciter
            // qu'on utilise le NkImage de NKImage (pas un eventuel namespace
            // renderer::NkImage).
            ::nkentseu::NkImage* img = ::nkentseu::NkImage::Alloc(
                (int32)w, (int32)h, NkImagePixelFormat::NK_RGB24);
            if (!img) return f;
            const int32 stride = img->Stride();
            uint8* pix = img->Pixels();
            const int32 third = (int32)(w / 3);
            for (uint32 y = 0; y < h; ++y) {
                uint8* row = pix + (usize)y * stride;
                for (uint32 x = 0; x < w; ++x) {
                    const uint8 g = (uint8)((x * 255u) / (w - 1));
                    uint8* px = row + x * 3;
                    if ((int32)x < third) {
                        px[0] = g; px[1] = 0; px[2] = 0;        // bande R
                    } else if ((int32)x < 2 * third) {
                        px[0] = 0; px[1] = g; px[2] = 0;        // bande G
                    } else {
                        px[0] = 0; px[1] = 0; px[2] = g;        // bande B
                    }
                }
            }

            uint8* jpegBuf  = nullptr;
            usize  jpegSize = 0;
            const bool ok = NkJPEGCodec::Encode(*img, jpegBuf, jpegSize, 85);
            // ATTENTION : NkImage::Alloc utilise nkMalloc + placement new
            // pour le wrapper. Free() libere a la fois les pixels ET le
            // wrapper. JAMAIS delete img apres Free() -> double-free,
            // heap corruption (crash c0000374 sur Windows). Cf. commentaire
            // dans Pong/Render/Texture2D.cpp lignes 86-87.
            img->Free();

            if (!ok || !jpegBuf || jpegSize == 0) {
                logger.Warn("[Format] NkJPEGCodec::Encode a echoue pour MJPEG synthetique");
                if (jpegBuf) memory::NkFree(jpegBuf);
                return f;
            }

            f.width  = w;
            f.height = h;
            f.stride = 0;  // non utilise pour MJPEG
            f.data.Resize(jpegSize);
            std::memcpy(f.data.Data(), jpegBuf, jpegSize);
            memory::NkFree(jpegBuf);
            return f;
        }

        // =====================================================================
        // Slot : frame source + conversion RGBA8 + texture GPU.
        // =====================================================================
        struct FormatSlot {
            NkString        label;
            NkCameraFrame   frame;        // Frame ORIGINALE (format natif).
            NkCameraFrame   converted;    // Apres ConvertToRGBA8.
            bool            convertOk = false;
            CameraGLTexture texture;
        };

        // ---------------------------------------------------------------------
        // Quad NDC (helper local).
        // ---------------------------------------------------------------------
        struct NdcRect { float x, y, w, h; };

        // Rect NDC du quadrant `index` dans une grille 2x2.
        // Layout : 0 = haut-gauche, 1 = haut-droite, 2 = bas-gauche, 3 = bas-droite.
        static NdcRect QuadrantNdc(uint32 index)
        {
            const float col = (float)(index % 2);
            const float row = (float)(index / 2);
            const float x = -1.f + col;
            const float y =  0.f - row;
            return { x, y, 1.f, 1.f };
        }

        // Aspect-fit en NDC dans un quadrant.
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

        // ---------------------------------------------------------------------
        // Conversion + sauvegarde PNG.
        // ---------------------------------------------------------------------
        static void RunSlotConversion(FormatSlot& slot, const char* baseName)
        {
            if (!slot.frame.IsValid() && slot.frame.format != NkPixelFormat::NK_PIXEL_MJPEG) {
                logger.Warnf("[Format] %s : frame source invalide", baseName);
                slot.convertOk = false;
                return;
            }
            slot.converted = slot.frame;
            slot.convertOk = NkCameraSystem::ConvertToRGBA8(slot.converted);
            if (!slot.convertOk) {
                logger.Errorf("[Format] %s : ConvertToRGBA8 a echoue", baseName);
                return;
            }
            const NkString path = NkString::Fmt("format_{0}.png", baseName);
            if (NkCameraSystem::SaveFrameToFile(slot.converted, path, 90)) {
                logger.Infof("[Format] %s : converti et sauve dans %s",
                             baseName, path.CStr());
            } else {
                logger.Warnf("[Format] %s : conversion OK mais Save a echoue",
                             baseName);
            }
        }

        // =====================================================================
        // Point d'entree de la demo format.
        // =====================================================================
        int RunCameraFormatDemo(const NkEntryState& /*state*/)
        {
            // -- Fenetre ------------------------------------------------------
            NkWindowConfig cfg;
            cfg.title     = "NkCameraDemos -- Format (ConvertToRGBA8)";
            cfg.width     = 1280;
            cfg.height    = 720;
            cfg.centered  = true;
            cfg.resizable = true;

            NkWindow window(cfg);
            if (!window.IsOpen()) {
                logger.Error("[Format] Echec creation fenetre");
                return -1;
            }

            // -- Contexte OpenGL ---------------------------------------------
            GLContext gl;
            if (!gl.Init(window)) {
                logger.Error("[Format] GLContext::Init a echoue");
                window.Close();
                return -2;
            }

            FullscreenBlit blit;
            if (!blit.Initialize()) {
                logger.Error("[Format] FullscreenBlit::Initialize a echoue");
                gl.Shutdown();
                window.Close();
                return -3;
            }

            // -- Construction des 4 slots ------------------------------------
            FormatSlot slots[NK_FORMAT_TEST_COUNT];
            slots[0].label = "YUYV";
            slots[0].frame = BuildYUYVTestFrame(NK_FORMAT_TEST_W, NK_FORMAT_TEST_H);
            slots[1].label = "NV12";
            slots[1].frame = BuildNV12TestFrame(NK_FORMAT_TEST_W, NK_FORMAT_TEST_H);
            slots[2].label = "I420";
            slots[2].frame = BuildI420TestFrame(NK_FORMAT_TEST_W, NK_FORMAT_TEST_H);
            slots[3].label = "MJPEG";
            slots[3].frame = BuildMJPEGTestFrame(NK_FORMAT_TEST_W, NK_FORMAT_TEST_H);

            // DIAGNOSTIC : sauvegarde du buffer JPEG brut (avant decode).
            // Ouvrir le .jpg dans un viewer externe permet de savoir si le
            // bug "MJPEG grayscale" vient de Encode ou de Decode :
            //   - image externe COULEUR  -> Decode est cassee (renvoie GRAY8)
            //   - image externe GRAYSCALE-> Encode est cassee (ecrit 1 canal)
            if (slots[3].frame.data.Size() > 0) {
                FILE* fjpg = std::fopen("format_MJPEG_raw.jpg", "wb");
                if (fjpg) {
                    std::fwrite(slots[3].frame.data.Data(), 1,
                                slots[3].frame.data.Size(), fjpg);
                    std::fclose(fjpg);
                    logger.Infof("[Format] buffer MJPEG brut sauve : "
                                 "format_MJPEG_raw.jpg (%zu octets)",
                                 slots[3].frame.data.Size());
                }
            }

            // Conversion + sauvegarde PNG + upload GPU.
            for (uint32 i = 0; i < NK_FORMAT_TEST_COUNT; ++i) {
                RunSlotConversion(slots[i], slots[i].label.CStr());
                if (slots[i].convertOk && slots[i].converted.IsValid()) {
                    if (slots[i].texture.Create(slots[i].converted.width,
                                                slots[i].converted.height)) {
                        slots[i].texture.Update(slots[i].converted.data.Data());
                    }
                }
                logger.Infof("[Format] slot[%u] %s : %s", i,
                             slots[i].label.CStr(),
                             slots[i].convertOk ? "OK" : "ECHEC");
            }

            // -- Etat fenetre ------------------------------------------------
            uint32 winW = cfg.width;
            uint32 winH = cfg.height;
            (void)winW; (void)winH;  // pas utilises (grille fixe en NDC)

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
                        if (nw > 0 && nh > 0) {
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

                // -- Rendu : grille 2x2 ---------------------------------------
                if (!gl.BeginFrame()) {
                    NkChrono::Sleep(16.f);
                    continue;
                }

                blit.Clear(0.04f, 0.04f, 0.06f);

                for (uint32 i = 0; i < NK_FORMAT_TEST_COUNT; ++i) {
                    if (!slots[i].texture.IsValid()) continue;
                    const NdcRect q   = QuadrantNdc(i);
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
            for (uint32 i = 0; i < NK_FORMAT_TEST_COUNT; ++i) {
                slots[i].texture.Shutdown();
            }
            blit.Shutdown();
            gl.Shutdown();
            window.Close();
            return 0;
        }

    } // namespace cameradem
} // namespace nkentseu
