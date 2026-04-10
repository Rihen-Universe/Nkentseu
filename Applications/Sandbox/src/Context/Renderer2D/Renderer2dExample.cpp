// =============================================================================
// Renderer2dExample.cpp — Démo NKRenderer 2D
//
// Sélection du backend :
//   CR2D.exe                    → backend par défaut selon la plateforme
//   CR2D.exe --backend=vulkan   (ou -bvk)
//   CR2D.exe --backend=dx11     (ou -bdx11)
//   CR2D.exe --backend=dx12     (ou -bdx12)
//   CR2D.exe --backend=opengl   (ou -bgl)
//   CR2D.exe --backend=sw       (ou -bsw)
//
// Problèmes corrigés vs la version précédente :
//   • Crash à la fermeture : r2d.reset() AVANT gfx->Shutdown() AVANT window.Close()
//   • Viewport pas plein écran : SetViewport() appelé à chaque frame
//   • Pas de redimensionnement : OnResize + recalcul des vues
//   • NkTexture backend non enregistré : CreateProcedural() sans passer par
//     le dispatch global (le backend gère sa propre texture blanche)
//   • DX12 fond uniquement : Clear() appelé correctement + Begin/End frame
// =============================================================================

#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"

#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkGraphicsApi.h"

#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKContext/Renderer/Core/NkRenderer2DFactory.h"
#include "NKContext/Renderer/Core/NkRenderer2DTypes.h"
#include "NKContext/Renderer/Resources/NkImage.h"
#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Renderer/Resources/NkFont.h"
#include "NKContext/Renderer/Resources/NkSprite.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// =============================================================================
// App data
// =============================================================================
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "CR2D Demo";
    d.appVersion = "1.0.0";
    d.enableEventLogging   = false;
    d.enableRendererDebug  = false;
    d.enableMultiWindow    = false;
    return d;
})());

// =============================================================================
// Sélection du backend depuis les args ligne de commande
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); ++i) {
        const NkString& a = args[i];
        if (a == "--backend=vulkan" || a == "-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (a == "--backend=dx11"   || a == "-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (a == "--backend=dx12"   || a == "-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (a == "--backend=metal"  || a == "-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (a == "--backend=sw"     || a == "-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (a == "--backend=opengl" || a == "-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    // ── Backend par défaut selon la plateforme ────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    return NkGraphicsApi::NK_API_DIRECTX11;   // DX11 = meilleur compat Windows
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    return NkGraphicsApi::NK_API_METAL;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    return NkGraphicsApi::NK_API_OPENGLES;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    return NkGraphicsApi::NK_API_WEBGL;
#else
    return NkGraphicsApi::NK_API_OPENGL;
#endif
}

// =============================================================================
// Crée le NkContextDesc pour l'API demandée
// =============================================================================
static NkContextDesc MakeDesc(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_API_VULKAN:     return NkContextDesc::MakeVulkan();
        case NkGraphicsApi::NK_API_DIRECTX11:  return NkContextDesc::MakeDirectX11();
        case NkGraphicsApi::NK_API_DIRECTX12:  return NkContextDesc::MakeDirectX12();
        case NkGraphicsApi::NK_API_METAL:      return NkContextDesc::MakeMetal();
        case NkGraphicsApi::NK_API_SOFTWARE:   return NkContextDesc::MakeSoftware();
        case NkGraphicsApi::NK_API_OPENGLES:   return NkContextDesc::MakeOpenGLES(3, 0);
        case NkGraphicsApi::NK_API_WEBGL:      return NkContextDesc::MakeOpenGLES(3, 0);
        case NkGraphicsApi::NK_API_OPENGL:
        default:                               return NkContextDesc::MakeOpenGL(3, 3);
    }
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const NkEntryState& state) {

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title     = "NKRenderer 2D Demo";
    cfg.width     = 1280;
    cfg.height    = 720;
    cfg.centered  = true;
    cfg.resizable = true;

    NkWindow window;
    if (!window.Create(cfg)) {
        logger.Error("Window creation failed");
        return -1;
    }

    // ── Backend ───────────────────────────────────────────────────────────────
    const NkGraphicsApi requestedApi = ParseBackend(state.args);
    logger.Infof("Requested backend: %s", NkGraphicsApiName(requestedApi));

    NkIGraphicsContext* gfx = nullptr;

    // Essai du backend demandé
    {
        NkContextDesc desc = MakeDesc(requestedApi);
        gfx = NkContextFactory::Create(window, desc);
    }

    // Repli automatique : DX11 → OpenGL → Software
    if (!gfx && requestedApi != NkGraphicsApi::NK_API_DIRECTX11) {
        logger.Warnf("Backend %s failed — trying DirectX 11", NkGraphicsApiName(requestedApi));
        gfx = NkContextFactory::Create(window, NkContextDesc::MakeDirectX11());
    }
    if (!gfx && requestedApi != NkGraphicsApi::NK_API_OPENGL) {
        logger.Warnf("DX11 failed — trying OpenGL");
        gfx = NkContextFactory::Create(window, NkContextDesc::MakeOpenGL(3, 3));
    }
    if (!gfx) {
        logger.Warn("OpenGL failed — trying Software");
        gfx = NkContextFactory::Create(window, NkContextDesc::MakeSoftware());
    }
    if (!gfx) {
        logger.Error("All backends failed");
        window.Close();
        return -2;
    }

    logger.Infof("GFX API: %s | %s", NkGraphicsApiName(gfx->GetApi()), gfx->GetInfo().renderer);

    // ── Renderer 2D ───────────────────────────────────────────────────────────
    NkIRenderer2D* r2dRaw = NkRenderer2DFactory::Create(gfx);
    if (!r2dRaw) {
        logger.Errorf("2D renderer creation failed for %s", NkGraphicsApiName(gfx->GetApi()));
        gfx->Shutdown();
        delete gfx;
        window.Close();
        return -3;
    }
    // Utiliser un scope RAII pour garantir l'ordre de destruction
    // r2d doit être détruit AVANT gfx
    struct R2DGuard {
        NkIRenderer2D* ptr;
        ~R2DGuard() { if (ptr) { ptr->Shutdown(); delete ptr; } }
    } r2dGuard{ r2dRaw };
    NkIRenderer2D& r2d = *r2dRaw;

    // ── Titre de fenêtre avec backend ─────────────────────────────────────────
    {
        NkString title = "NKRenderer 2D — ";
        title += NkGraphicsApiName(gfx->GetApi());
        window.SetTitle(title.CStr());
    }

    // ── Ressources ────────────────────────────────────────────────────────────
    // Texture procédurale (pas de fichier assets nécessaire)
    // On crée directement une NkImage en mémoire et on la charge
    NkTexture playerTex;
    {
        // Damier 64×64 bleu/blanc comme texture de test
        NkImage img;
        img.Create(64, 64);
        for (uint32 y = 0; y < 64; ++y) {
            for (uint32 x = 0; x < 64; ++x) {
                bool checker = ((x/8 + y/8) % 2 == 0);
                NkColor2D c = checker ? NkColor2D{100, 180, 255, 255}
                                      : NkColor2D{255, 255, 255, 200};
                img.SetPixel(x, y, c);
            }
        }
        if (!playerTex.LoadFromImage(r2d, img)) {
            logger.Warn("[Demo] Procedural texture failed — backend texture API not available");
        }
    }

    // Police — optionnelle
    NkFont font;
    const bool hasFont = font.LoadFromFile(r2d, "assets/Roboto-Regular.ttf");
    if (!hasFont) logger.Warn("Font not found — text will be skipped");

    // ── Sprite ────────────────────────────────────────────────────────────────
    NkSprite player;
    if (playerTex.IsValid()) {
        player.SetTexture(playerTex, true);
        player.SetOrigin({32.f, 32.f});
        player.SetScale({2.f, 2.f});
    }

    // ── Texte ─────────────────────────────────────────────────────────────────
    NkText titleText;
    if (hasFont) {
        titleText.SetFont(font);
        titleText.SetString("NkEngine - 2D Renderer Demo");
        titleText.SetCharacterSize(24);
        titleText.SetFillColor(NkColor2D::White());
        titleText.SetPosition({20.f, 16.f});
    }

    NkText fpsText;
    if (hasFont) {
        fpsText.SetFont(font);
        fpsText.SetCharacterSize(16);
        fpsText.SetFillColor({220, 220, 60, 255});
        fpsText.SetPosition({20.f, 52.f});
    }

    // ── Vues ──────────────────────────────────────────────────────────────────
    uint32 winW = cfg.width;
    uint32 winH = cfg.height;

    // gameView : caméra monde (peut être déplacée, zoomée, etc.)
    // Par défaut elle couvre toute la fenêtre (pixel = unité monde).
    NkView2D gameView;
    NkView2D uiView;   // toujours en pixels écran

    auto UpdateViews = [&]() {
        gameView.center = { winW * 0.5f, winH * 0.5f };
        gameView.size   = { (float)winW, (float)winH };
        uiView          = r2d.GetDefaultView();
        // Mise à jour du viewport renderer (plein écran)
        r2d.SetViewport({ 0, 0, (int32)winW, (int32)winH });
    };
    UpdateViews();

    // ── Boucle principale ─────────────────────────────────────────────────────
    auto& events = NkEvents();
    bool  running = true;
    float time    = 0.f;
    float fpsAccum= 0.f;
    int   fpsFrames = 0;
    char  fpsBuf[128] = {};

    NkChrono chrono;

    while (running && window.IsOpen()) {
        // ── Événements ────────────────────────────────────────────────────────
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
                    gfx->OnResize(winW, winH);
                    UpdateViews();
                }
            }
        }
        if (!running) break;

        // ── Delta time ────────────────────────────────────────────────────────
        const float dt = (float)chrono.Reset().seconds;
        time     += dt;
        fpsAccum += dt;
        ++fpsFrames;

        if (fpsAccum >= 0.5f) {
            const float fps = (float)fpsFrames / fpsAccum;
            snprintf(fpsBuf, sizeof(fpsBuf),
                     "%.0f fps | %s | %ux%u",
                     fps, NkGraphicsApiName(gfx->GetApi()), winW, winH);
            if (hasFont) fpsText.SetString(fpsBuf);
            fpsAccum  = 0.f;
            fpsFrames = 0;
        }

        // ── Début de frame ────────────────────────────────────────────────────
        if (!gfx->BeginFrame()) {
            NkChrono::Sleep(16.f);
            continue;
        }

        // ── Fond ──────────────────────────────────────────────────────────────
        r2d.Clear({18, 18, 30, 255});
        r2d.Begin();

        // ── Couche Monde (gameView) ────────────────────────────────────────────
        r2d.SetView(gameView);
        r2d.SetBlendMode(NkBlendMode::NK_ALPHA);

        // — Fond étoilé —
        for (int i = 0; i < 80; ++i) {
            float bx = fmodf((float)(i * 79) + time * 30.f * (1.f + i * 0.012f), (float)winW);
            float by = (float)(i * 37 % (int)winH);
            uint8 br = (uint8)(80 + i * 2 % 175);
            float sz = 1.f + (float)(i % 3);
            r2d.DrawFilledCircle({bx, by}, sz, {br, br, br, 180}, 4);
        }

        // — Primitives de demo —
        const float cx = winW * 0.5f;
        const float cy = winH * 0.5f;

        // Triangle rouge animé
        float ta = time * 0.8f;
        r2d.DrawFilledTriangle(
            {cx - 300.f + cosf(ta)*10.f,     cy + 150.f},
            {cx - 260.f,                      cy + 70.f },
            {cx - 220.f + cosf(ta+1.f)*10.f, cy + 150.f},
            {220, 60, 60, 220}
        );
        r2d.DrawTriangle(
            {cx - 300.f + cosf(ta)*10.f, cy + 150.f},
            {cx - 260.f,                 cy + 70.f },
            {cx - 220.f + cosf(ta+1.f)*10.f, cy + 150.f},
            {255, 120, 120, 255}, 2.f, {255, 120, 120, 255}
        );

        // Rectangle vert
        r2d.DrawFilledRect({cx - 80.f, cy + 70.f, 120.f, 80.f}, {50, 180, 80, 200});
        r2d.DrawRect      ({cx - 80.f, cy + 70.f, 120.f, 80.f}, {120, 255, 140, 255}, 2.f, {120, 255, 140, 255});

        // Cercle bleu animé
        float cr = 40.f + sinf(time * 2.f) * 10.f;
        r2d.DrawFilledCircle({cx + 200.f, cy + 110.f}, cr, {60, 100, 220, 200}, 32);
        r2d.DrawCircle      ({cx + 200.f, cy + 110.f}, cr, {150, 190, 255, 255}, 32, 2.f, {150, 190, 255, 255});

        // Ligne animée
        r2d.DrawLine(
            {cx + 320.f, cy + 70.f},
            {cx + 320.f + 60.f * cosf(time * 3.f),
             cy + 150.f + 50.f * sinf(time * 3.f)},
            {255, 210, 60, 255}, 3.f
        );

        // Sprite au centre (si texture valide)
        if (playerTex.IsValid()) {
            player.SetPosition({cx, cy - 30.f});
            player.SetRotation(time * 60.f);
            player.SetColor({
                (uint8)(128 + 127*sinf(time*1.5f)),
                (uint8)(128 + 127*cosf(time*1.3f)),
                255, 255
            });
            r2d.Draw(player);
        }

        // Orbes additifs autour du centre
        r2d.SetBlendMode(NkBlendMode::NK_ADD);
        for (int i = 0; i < 5; ++i) {
            float ax = cx + cosf(time + i * 1.257f) * 100.f;
            float ay = cy - 30.f + sinf(time*1.2f + i * 1.257f) * 60.f;
            NkColor2D ac{(uint8)(60+i*30),(uint8)(40+i*20),(uint8)(200-i*20), 90};
            r2d.DrawFilledCircle({ax, ay}, 18.f, ac, 16);
        }

        // ── Couche UI (pixels écran fixes) ────────────────────────────────────
        r2d.Flush();
        r2d.SetBlendMode(NkBlendMode::NK_ALPHA);
        r2d.SetView(uiView);

        // Barre du haut semi-transparente
        r2d.DrawFilledRect({0.f, 0.f, (float)winW, 80.f}, {0, 0, 0, 140});

        // Indicateur backend (rectangle coloré)
        NkColor2D apiColor = {100, 100, 100, 255};
        switch (gfx->GetApi()) {
            case NkGraphicsApi::NK_API_VULKAN:    apiColor = {180, 60,  60,  255}; break;
            case NkGraphicsApi::NK_API_DIRECTX11: apiColor = {60,  120, 200, 255}; break;
            case NkGraphicsApi::NK_API_DIRECTX12: apiColor = {40,  80,  240, 255}; break;
            case NkGraphicsApi::NK_API_OPENGL:    apiColor = {60,  180, 100, 255}; break;
            case NkGraphicsApi::NK_API_SOFTWARE:  apiColor = {160, 100, 40,  255}; break;
            default: break;
        }
        r2d.DrawFilledRect({0.f, 0.f, 6.f, 80.f}, apiColor);

        if (hasFont) {
            r2d.Draw(titleText);
            r2d.Draw(fpsText);
        } else {
            // Fallback sans police : barres colorées comme indicateurs
            r2d.DrawFilledRect({20.f, 18.f, 260.f, 22.f}, {200, 200, 200, 200});
            r2d.DrawFilledRect({20.f, 50.f, 180.f, 16.f}, {220, 200, 60,  200});
        }

        // ── Fin de frame ──────────────────────────────────────────────────────
        r2d.End();

        gfx->EndFrame();
        gfx->Present();

        // Cap 60 fps
        const auto elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // ── Nettoyage ordonné ─────────────────────────────────────────────────────
    // ORDRE OBLIGATOIRE :
    //   1. Vider les ressources GPU (textures, sprites, fonts)
    //   2. Détruire le renderer 2D  (r2dGuard détruit r2dRaw)
    //   3. Shutdown + delete du contexte graphique
    //   4. Fermer la fenêtre

    // 1. Libérer les ressources GPU avant de détruire le renderer
    playerTex.Destroy();
    font.Destroy();

    // 2. Renderer 2D (RAII via r2dGuard — destructeur appelé ici)
    r2dGuard.ptr->Shutdown();
    delete r2dGuard.ptr;
    r2dGuard.ptr = nullptr;

    // 3. Contexte graphique
    gfx->Shutdown();
    delete gfx;
    gfx = nullptr;

    // 4. Fenêtre
    window.Close();

    return 0;
}