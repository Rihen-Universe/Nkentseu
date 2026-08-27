// main_00_projet_vierge.cpp
//
// Squelette vierge : NKWindow (fenêtre) + NKEvent (entrées) +
// NKRHI (device GPU) + NKRenderer (renderer haut niveau), sans dessin.
//
// Compilation : lier NKWindow, NKEvent, NKRHI, NKRenderer (chapitre 01).

#include <NKWindow/Core/NkMain.h>
#include <NKWindow/Core/NkWindow.h>
#include <NKEvent/NkEventDispatcher.h>   // NkEvents(), NkInput, NkKeyPressEvent...
#include <NKRHI/Core/NkDeviceFactory.h>
#include <NKRenderer/NkRenderer.h>

#include <cstdlib> // getenv (diag opt-in NK_VK_VALIDATION)

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

// Suppress Win32 GDI macros that collide with renderer method names
#ifdef DrawText
#undef DrawText
#endif
#ifdef DrawTextCentered
#undef DrawTextCentered
#endif

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h" // NK_CAPTURE (validation headless)
#include "NKRenderer/Tools/Offscreen/NkFrameCapture.h"      // NK_RECORD (capture async -> video)
#include "NKMedia/Video/NkVideoRecorder.h"
#include "NKThreading/NkThread.h" // NK_RECORD : finalisation MP4 asynchrone (fix freeze F9)
#include "NKRenderer/Streaming/NkStreamingSystem.h" // NK_STREAM_TEST : self-test streaming reel
#include "NKMemory/NkAllocator.h" // NK_RECORD : recorder alloue (possede par le thread de finalisation)                  // NK_RECORD (encodage MP4/H.264 threade)

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Simulation/NkSimulationRenderer.h"
#include "NKAnimation/NkAnimation.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// ── Métadonnées de l'application (nom affiché dans les logs/l'OS) ──────────
static void ConfigureAppData(NkAppData& d) {
    d.appName = "ProjetVierge";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// ── Point d'entrée cross-plateforme unique (chapitre 01) ────────────────────
int nkmain(const NkEntryState& state) {

    // 1) Fenêtre (chapitre 02) ------------------------------------------------
    NkWindowConfig winCfg;
    winCfg.title     = "Projet vierge — Nkentseu";
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;
    winCfg.vsync     = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        return -1; // échec de création (voir les logs NKLogger)
    }

    // 2) Device GPU (chapitre 03) ---------------------------------------------
    // CreateAutoDetect teste les APIs graphiques dans l'ordre optimal pour la
    // plateforme (Vulkan > DX12 > DX11 > OpenGL > Software sous Windows, etc.)
    // et remplit devInit.api avec celle qui a été choisie.
    NkDeviceInitInfo devInit{};
    devInit.surface = window.GetSurfaceDesc();
    devInit.width   = window.GetSize().x;
    devInit.height  = window.GetSize().y;

    NkIDevice* device = NkDeviceFactory::CreateAutoDetect(devInit);
    if (!device) {
        return -1; // aucune API graphique disponible (cas rarissime)
    }

    // 3) Renderer haut niveau (chapitre 04) -----------------------------------
    // ForGame() est un bon point de départ générique ; d'autres presets
    // existent (For2D, ForMobile, ForEditor, ForMinimal...).
    NkRendererConfig rendCfg = NkRendererConfig::ForGame(devInit.api,
                                                          devInit.width, devInit.height);
    NkRenderer* renderer = NkRenderer::Create(device, rendCfg);
    if (!renderer || !renderer->Initialize()) {
        NkDeviceFactory::Destroy(device);
        return -1;
    }

    // 4) Ressources 2D (chapitre 05) -------------------------------------------
    NkTextureLibrary* textures = renderer->GetTextures();
    NkTextRenderer*   text     = renderer->GetTextRenderer();
    NkRender2D*        r2d     = renderer->GetRender2D();

    // Remplacez ce chemin par une vraie image de votre projet.
    NkTexHandle ballTex = textures->Load("assets/ball.png");
    NkFontHandle uiFont = text->LoadFont("assets/font.ttf", 18.f);

    // État applicatif minimal : une balle qui rebondit dans la fenêtre.
    float ballX = 100.f, ballY = 100.f;
    float velX = 220.f,  velY = 160.f; // pixels / seconde
    const float ballSize = 48.f;

    float dt = 1.f / 60.f; // dt fixe simplifié pour cet exemple — un vrai
                            // projet mesure le delta-temps réel entre deux
                            // frames (module NKTime, hors périmètre de ce
                            // cours), NkEntryState n'expose pas d'horloge.
    
    math::NkVec2u currentSize(renderer->GetWidth(), renderer->GetHeight());

    // 4) Boucle principale -----------------------------------------------------
    bool running = true;
    while (running && window.IsOpen()) {

        // -- Événements (chapitre 02) --
        NkEvent* ev;
        while (NkEvents().PollEvent(ev)) {
            if (auto* resize = ev->As<NkWindowResizeEvent>()) {
                // Toujours prévenir le renderer d'un changement de taille,
                // même dans un projet vierge : sinon le swapchain reste à
                // l'ancienne résolution et l'image sera étirée/déformée.
                currentSize = math::NkVec2u(resize->GetWidth(), resize->GetHeight());

            } else if (auto* closeEv = ev->As<NkWindowCloseEvent>()) {
                (void)closeEv;
                running = false;

            } else if (auto* key = ev->As<NkKeyPressEvent>()) {
                if (key->GetKey() == NkKey::NK_ESCAPE) {
                    running = false;
                }
            }
        }
        if (!running) break;
        
        if (currentSize.width > 0 && currentSize.height && (renderer->GetWidth() != currentSize.width || renderer->GetHeight() != currentSize.height)) {
            renderer->OnResize(currentSize.width, currentSize.height);
        }

        // -- Logique : rebond dans les bords de la fenêtre --
        uint32 screenW = window.GetSize().x, screenH = window.GetSize().y;
        ballX += velX * dt;
        ballY += velY * dt;
        if (ballX < 0.f || ballX + ballSize > (float)screenW) velX = -velX;
        if (ballY < 0.f || ballY + ballSize > (float)screenH) velY = -velY;


        // -- Frame (chapitre 04) --
        // BeginFrame() peut renvoyer false (ex. fenêtre minimisée) : dans ce
        // cas on saute la frame plutôt que de forcer un rendu inutile.
        if (!renderer->BeginFrame()) {
            continue;
        }

        // (rien à dessiner ici pour l'instant — voir les 3 autres fichiers
        NkICommandBuffer* cmd = renderer->GetCmd();

        r2d->Begin(cmd, screenW, screenH); // pas de caméra 2D : centre par défaut

        // Fond plein
        r2d->FillRect({0.f, 0.f, (float)screenW, (float)screenH}, {0.08f, 0.09f, 0.12f, 1.f});

        // Un rectangle et un cercle de debug, juste pour montrer les formes
        r2d->FillRect({40.f, 40.f, 160.f, 60.f}, {0.2f, 0.6f, 0.9f, 1.f});
        r2d->DrawRect({40.f, 40.f, 160.f, 60.f}, {1.f, 1.f, 1.f, 1.f}, 2.f);
        r2d->FillCircle({screenW - 100.f, 100.f}, 40.f, {0.9f, 0.3f, 0.3f, 1.f});

        // Le sprite qui rebondit
        r2d->DrawSprite({ballX, ballY, ballSize, ballSize}, ballTex);

        r2d->End();

        // Le texte se dessine hors Begin/End de NkRender2D (chapitre 05 §4)
        text->DrawText({10.f, 10.f}, "Dessin 2D — Echap pour quitter", uiFont, 18.f, NkColor(1,1,1,1).ToUint32A()); // couleur blanche (RGBA -> ABGR pour le shader 2D)

        //  main_2D.cpp / main_3D.cpp / main_2D_3D_mixte.cpp)

        // Present() AVANT EndFrame() : Present exécute le render graph, ferme le
        // command buffer et soumet ; EndFrame ne fait que clore la frame device.
        renderer->Present();
        renderer->EndFrame();
    }

    // 5) Fermeture propre, dans l'ordre inverse de la création -----------------
    NkRenderer::Destroy(renderer); // met renderer à nullptr
    NkDeviceFactory::Destroy(device); // met device à nullptr
    window.Close();
    return 0;
}