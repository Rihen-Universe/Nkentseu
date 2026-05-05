// =============================================================================
// NkRenderer2DDemo.cpp
// Démo NKRenderer v4.0 — rendu 2D fonctionnel
//
// Illustre :
//   • Initialisation de NkRenderer au-dessus de NkIDevice
//   • Rendu 2D : rectangles, cercles, lignes, sprites
//   • Texte via NkTextRenderer
//   • Boucle de frame correcte sans NkISwapchain (device gère tout)
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// ─── Sélection backend ────────────────────────────────────────────────────────
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); ++i) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")
            return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11")
            return NkGraphicsApi::NK_GFX_API_DX11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12")
            return NkGraphicsApi::NK_GFX_API_DX12;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")
            return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")
            return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_NONE;
}

// ─── Point d'entrée ───────────────────────────────────────────────────────────
int nkmain(const NkEntryState& state) {

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NKRenderer 2D Demo";
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;
    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Error("[2DDemo] Echec creation fenetre");
        return 1;
    }

    // ── Device RHI ────────────────────────────────────────────────────────────
    NkGraphicsApi requestedApi = ParseBackend(state.GetArgs());

    NkDeviceInitInfo devInfo;
    devInfo.api     = requestedApi;
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width   = window.GetSize().width;
    devInfo.height  = window.GetSize().height;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    NkIDevice* device = NkDeviceFactory::CreateWithFallback(devInfo, {
        NkGraphicsApi::NK_GFX_API_OPENGL,
        NkGraphicsApi::NK_GFX_API_DX11,
        NkGraphicsApi::NK_GFX_API_DX12,
        NkGraphicsApi::NK_GFX_API_VULKAN,
        NkGraphicsApi::NK_GFX_API_SOFTWARE
    });
#else
    NkIDevice* device = NkDeviceFactory::CreateWithFallback(devInfo, {
        NkGraphicsApi::NK_GFX_API_VULKAN,
        NkGraphicsApi::NK_GFX_API_OPENGL,
        NkGraphicsApi::NK_GFX_API_SOFTWARE
    });
#endif

    if (!device || !device->IsValid()) {
        logger.Error("[2DDemo] Echec creation device RHI");
        window.Close();
        return 1;
    }
    logger.Info("[2DDemo] Backend: {0}", NkGraphicsApiName(device->GetApi()));

    // ── NKRenderer ────────────────────────────────────────────────────────────
    NkRendererConfig rendCfg;
    rendCfg.width  = devInfo.width;
    rendCfg.height = devInfo.height;

    NkRenderer* renderer = NkRenderer::Create(device, devInfo.surface, rendCfg);
    if (!renderer) {
        logger.Error("[2DDemo] Echec creation NkRenderer");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    NkRender2D*     r2d  = renderer->GetRender2D();
    NkTextRenderer* text = renderer->GetTextRenderer();

    // ── Etat application ──────────────────────────────────────────────────────
    bool  running = true;
    float time    = 0.f;
    float mouseX  = 0.f, mouseY = 0.f;
    auto& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        mouseX = (float)e->GetX();
        mouseY = (float)e->GetY();
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        rendCfg.width = (uint32)e->GetWidth();
        rendCfg.height = (uint32)e->GetHeight();
        // static uint32 prevW = w, prevH = h;

        // if (w > 0 && h > 0 && (w != prevW || h != prevH)) {
        //     renderer->OnResize(w, h);
        //     rendCfg.width  = w;
        //     rendCfg.height = h;
        //     prevW = w;
        //     prevH = h;
        // }
    });

    NkClock clock;
    logger.Info("[2DDemo] Boucle principale. ESC = quitter.");

    // ── Boucle principale ─────────────────────────────────────────────────────
    while (running) {
        logger.Info();
        events.PollEvents();
        logger.Info();
        if (!running) break;

        float dt = clock.Tick().delta;
        if (dt > 0.1f) dt = 1.f / 60.f;
        time += dt;
        logger.Info();
        if (renderer->GetWidth() == 0 || renderer->GetWidth() == 0) continue;
        logger.Info();

        if (renderer->GetWidth() != rendCfg.width || renderer->GetWidth() != rendCfg.height) {
        logger.Info();
            renderer->OnResize(rendCfg.width, rendCfg.height);
        logger.Info();
        }

        logger.Info();
        uint32 W = renderer->GetWidth();
        logger.Info();
        uint32 H = renderer->GetHeight();
        logger.Info();

        if (!renderer->BeginFrame()) continue;
        logger.Info();

        // ── Rendu 2D ─────────────────────────────────────────────────────────
        if (r2d) {
        logger.Info();
            r2d->Begin(renderer->GetCmd(), W, H);
        logger.Info();

            // Fond dégradé
        logger.Info();
            r2d->FillRectGradV({0, 0, (float)W, (float)H}, {0.05f, 0.05f, 0.12f, 1}, {0.02f, 0.06f, 0.18f, 1});

            // Titre en haut
            // r2d->FillRect({10, 10, 400, 40}, {0,0,0,0.6f});
            // r2d->DrawRect({10, 10, 400, 40}, {0.4f,0.8f,1.f,1}, 2.f);

            // // Animations basées sur le temps
            // float pulse = 0.5f + 0.5f * sinf(time * 2.f);
            // float spin  = time * 90.f; // degrés

            // // Cercle animé
            // NkVec2f cc = {(float)rendCfg.width*0.5f, (float)rendCfg.height*0.4f};
            // float   cr = 80.f + 20.f * pulse;
            // r2d->FillCircle(cc, cr, {pulse, 0.3f, 1.f-pulse, 0.8f});
            // r2d->DrawCircle(cc, cr + 5.f, {1,1,1,0.5f}, 2.f);

            // // Orbite de petits cercles autour du grand
            // for (int i = 0; i < 6; i++) {
            //     float angle = (float)i * (360.f / 6.f) + spin;
            //     float rad   = angle * 3.14159f / 180.f;
            //     NkVec2f pos = {
            //         cc.x + cosf(rad) * 150.f,
            //         cc.y + sinf(rad) * 150.f
            //     };
            //     NkVec4f col = {
            //         0.5f + 0.5f*cosf(rad),
            //         0.5f + 0.5f*sinf(rad),
            //         0.8f, 0.9f
            //     };
            //     r2d->FillCircle(pos, 14.f, col);
            // }

            // // Ligne suivant la souris
            // r2d->DrawLine(cc, {mouseX, mouseY}, {1,1,0,0.7f}, 2.f);

            // // Formes géométriques statiques en bas
            // float by = (float)H - 120.f;
            // r2d->FillRect({30, by, 80, 80}, {0.9f,0.2f,0.2f,0.9f});
            // r2d->FillRoundRect({130, by, 80, 80}, {0.2f,0.9f,0.2f,0.9f}, 12.f);
            // r2d->DrawCircle({275, by+40}, 40.f, {0.2f,0.4f,1.f,0.9f}, 3.f);
            // r2d->DrawRect({320, by, 80, 80}, {1,0.7f,0.1f,1}, 3.f);

            // // Bezier
            // r2d->DrawBezier({500,by+80},{550,by},{650,by},{700,by+80},
            //                  {1,0.5f,0,1}, 2.f);

            // // Indicateur FPS (cadre)
            // NkString fpsStr = NkFormat("FPS: {0:.0f}  |  Backend: {1}", dt > 0 ? 1.f/dt : 0.f, NkGraphicsApiName(device->GetApi()));
            // r2d->FillRect({(float)W-260.f, 10, 250, 32}, {0,0,0,0.6f});
            // r2d->DrawRect({(float)W-260.f, 10, 250, 32}, {0.6f,0.6f,0.6f,1}, 1.f);

            // // Texte (si disponible)
            // if (text) {
            //     NkFontHandle font = text->GetDefaultFont();
            //     if (font.IsValid()) {
            //         text->DrawString({18, 22}, "NKRenderer 2D Demo", font, 18.f, 0xFFFFFFFF);
            //         text->DrawString({(float)W-252.f, 20}, fpsStr.CStr(), font, 14.f, 0xFFFFFFFF);
            //     }
            // }

        logger.Info();
            r2d->End();
        logger.Info();
        }

        logger.Info();
        renderer->EndFrame();
        logger.Info();
        renderer->Present();
        logger.Info();
    }
        logger.Info();

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();
    NkRenderer::Destroy(renderer);
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[2DDemo] Termine proprement.");
    return 0;
}
