// =============================================================================
// main.cpp  — NKRenderer v4.0 — Point d'entrée complet
//
// Intégration NKWindow + NKEvent + NKRenderer :
//   • NkWindow    crée la fenêtre native + context
//   • NkEventSystem gère les événements
//   • NkRenderer  se connecte via NkSurfaceDesc (seul lien)
//   • Resize      déclenche renderer->OnResize() depuis NkGraphicsContextResizeEvent
//
// Usage :
//   ./NKRenderer_Demo [0-9]      (numéro de démo, défaut=0)
// =============================================================================
#include "Demo/Demo.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkGraphicsEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/NkRHI.h"    // NkCreateDevice()
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::renderer::demo;

// =============================================================================
// Boucle d'application principale
// =============================================================================
class NkDemoApp {
public:
    static constexpr uint32 WIDTH  = 1280;
    static constexpr uint32 HEIGHT = 720;

    bool Init(int demoIdx) {
        // ── 1. Initialiser le système de plateforme ─────────────────────────
        if (!NkSystem::Initialize()) {
            fprintf(stderr, "[App] NkSystem::Initialize failed\n");
            return false;
        }

        // ── 2. Créer la fenêtre ─────────────────────────────────────────────
        NkWindowConfig winCfg;
        winCfg.title   = "NKRenderer v4.0 — Demo";
        winCfg.width   = WIDTH;
        winCfg.height  = HEIGHT;
        winCfg.resizable = true;
        winCfg.centered  = true;

        mWindow = NkWindow::Create(winCfg);
        if (!mWindow) {
            fprintf(stderr, "[App] NkWindow::Create failed\n");
            return false;
        }

        // ── 3. Créer le contexte graphique ─────────────────────────────────
        NkContextConfig ctxCfg;
        ctxCfg.api       = NkGraphicsApi::NK_OPENGL;
        ctxCfg.majorVersion = 4;
        ctxCfg.minorVersion = 6;
        ctxCfg.vsync     = true;
        ctxCfg.debug     = true;

        mContext = NkContext::Create(mWindow, ctxCfg);
        if (!mContext) {
            fprintf(stderr, "[App] NkContext::Create failed\n");
            return false;
        }

        // ── 4. Créer le device RHI ──────────────────────────────────────────
        NkDeviceDesc devDesc;
        devDesc.api      = ctxCfg.api;
        devDesc.context  = mContext;
        devDesc.debug    = true;
        mDevice = NkCreateDevice(devDesc);
        if (!mDevice) {
            fprintf(stderr, "[App] NkCreateDevice failed\n");
            return false;
        }

        // ── 5. Obtenir la surface de la fenêtre ─────────────────────────────
        NkSurfaceDesc surface = mWindow->GetSurfaceDesc();

        // ── 6. Créer et initialiser la démo ─────────────────────────────────
        mDemo = CreateDemo(demoIdx);
        if (!mDemo) {
            fprintf(stderr, "[App] CreateDemo(%d) failed\n", demoIdx);
            return false;
        }
        if (!mDemo->Init(mDevice, surface, WIDTH, HEIGHT)) {
            fprintf(stderr, "[App] Demo::Init failed\n");
            return false;
        }

        // ── 7. S'abonner aux événements ──────────────────────────────────────
        auto& evSys = NkEventSystem::GetInstance();

        // Resize
        evSys.Subscribe<NkGraphicsContextResizeEvent>(
            [this](const NkGraphicsContextResizeEvent& e) {
                if (mDemo && mDemo->GetRenderer())
                    mDemo->GetRenderer()->OnResize(e.width, e.height);
                mWidth  = e.width;
                mHeight = e.height;
            });

        // Fermeture fenêtre
        evSys.Subscribe<NkWindowCloseEvent>(
            [this](const NkWindowCloseEvent&) { mRunning = false; });

        // Clavier : Échap pour quitter, Tab pour changer de démo
        evSys.Subscribe<NkKeyDownEvent>(
            [this](const NkKeyDownEvent& e) {
                if (e.keyCode == NkKey::NK_ESCAPE) mRunning = false;
                if (e.keyCode == NkKey::NK_TAB) {
                    mDemoIdx = (mDemoIdx + 1) % 10;
                    SwitchDemo(mDemoIdx);
                }
                if (e.keyCode == NkKey::NK_F1)  SwitchDemo(0);
                if (e.keyCode == NkKey::NK_F2)  SwitchDemo(1);
                if (e.keyCode == NkKey::NK_F3)  SwitchDemo(2);
                if (e.keyCode == NkKey::NK_F4)  SwitchDemo(3);
                if (e.keyCode == NkKey::NK_F5)  SwitchDemo(4);
                if (e.keyCode == NkKey::NK_F6)  SwitchDemo(5);
                if (e.keyCode == NkKey::NK_F7)  SwitchDemo(6);
                if (e.keyCode == NkKey::NK_F8)  SwitchDemo(7);
                if (e.keyCode == NkKey::NK_F9)  SwitchDemo(8);
                if (e.keyCode == NkKey::NK_F10) SwitchDemo(9);
            });

        mDemoIdx = demoIdx;
        mRunning = true;
        mWidth   = WIDTH;
        mHeight  = HEIGHT;

        printf("[App] Démo %d initialisée : %s\n",
               demoIdx, mDemo->GetName());
        printf("[App] Touches : TAB=démo suivante | F1-F10=démo directe | ESC=quitter\n");

        return true;
    }

    void Run() {
        auto& evSys = NkEventSystem::GetInstance();
        auto  prevTime = std::chrono::high_resolution_clock::now();
        uint32 frameCount = 0;
        float32 fpsTimer  = 0.f;

        while (mRunning && mWindow->IsOpen()) {
            // ── Calculer dt ─────────────────────────────────────────────────
            auto  now = std::chrono::high_resolution_clock::now();
            float32 dt = std::chrono::duration<float32>(now - prevTime).count();
            prevTime  = now;
            if (dt > 0.1f) dt = 0.1f; // cap 100ms pour le debugger

            // ── Traiter les événements plateforme ────────────────────────────
            NkSystem::PollEvents();
            evSys.ProcessQueue();

            // ── Update + Render ───────────────────────────────────────────────
            if (mDemo) {
                mDemo->Update(dt);
                mDemo->Render();
            }

            // ── Swap (géré par NkContext ou par le renderer) ─────────────────
            mContext->SwapBuffers();

            // ── FPS dans le titre ─────────────────────────────────────────────
            frameCount++;
            fpsTimer += dt;
            if (fpsTimer >= 1.f) {
                char title[128];
                snprintf(title, sizeof(title),
                         "NKRenderer v4.0 — %s  |  %.0f FPS  |  %.2f ms",
                         mDemo ? mDemo->GetName() : "?",
                         (float32)frameCount / fpsTimer,
                         fpsTimer / frameCount * 1000.f);
                mWindow->SetTitle(title);
                frameCount = 0; fpsTimer = 0.f;
            }
        }
    }

    void Shutdown() {
        if (mDemo)    { mDemo->Shutdown(); delete mDemo; mDemo=nullptr; }
        if (mDevice)  { NkDestroyDevice(mDevice); mDevice=nullptr; }
        if (mContext) { NkContext::Destroy(mContext); mContext=nullptr; }
        if (mWindow)  { NkWindow::Destroy(mWindow);  mWindow=nullptr; }
        NkSystem::Shutdown();
        printf("[App] Shutdown complet.\n");
    }

private:
    NkWindow*    mWindow  = nullptr;
    NkContext*   mContext = nullptr;
    NkIDevice*   mDevice  = nullptr;
    DemoBase*    mDemo    = nullptr;
    bool         mRunning = false;
    uint32       mWidth   = WIDTH;
    uint32       mHeight  = HEIGHT;
    int          mDemoIdx = 0;

    // Accès au renderer depuis l'app (pour resize)
    NkRenderer* GetRenderer() const {
        return mDemo ? mDemo->GetRendererPublic() : nullptr;
    }

    DemoBase* CreateDemo(int idx) {
        switch (idx) {
            case 0:  return new Demo01_Basic3D();
            case 1:  return new Demo02_Game2D();
            case 2:  return new Demo03_Film();
            case 3:  return new Demo04_Archviz();
            case 4:  return new Demo05_Animation();
            case 5:  return new Demo06_VFX();
            case 6:  return new Demo07_Offscreen();
            case 7:  return new Demo08_PV3DE();
            case 8:  return new Demo09_MultiVP();
            case 9:  return new Demo10_NkSL();
            default: return new Demo01_Basic3D();
        }
    }

    void SwitchDemo(int newIdx) {
        if (newIdx == mDemoIdx && mDemo) return;
        printf("[App] Passage démo %d → %d\n", mDemoIdx, newIdx);
        if (mDemo) { mDemo->Shutdown(); delete mDemo; mDemo=nullptr; }
        mDemoIdx = newIdx;
        mDemo = CreateDemo(newIdx);
        if (mDemo) {
            NkSurfaceDesc surface = mWindow->GetSurfaceDesc();
            if (!mDemo->Init(mDevice, surface, mWidth, mHeight)) {
                fprintf(stderr,"[App] SwitchDemo Init(%d) failed\n", newIdx);
            } else {
                printf("[App] Demo %d : %s\n", newIdx, mDemo->GetName());
            }
        }
    }
};

// ── Ajout d'un accès public au renderer dans DemoBase ────────────────────────
// (Ajouter dans Demo.h : NkRenderer* GetRendererPublic() const { return mRenderer; })

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    int demoIdx = 0;
    if (argc > 1) demoIdx = atoi(argv[1]);
    if (demoIdx < 0 || demoIdx > 9) demoIdx = 0;

    printf("=============================================================\n");
    printf(" NKRenderer v4.0 — Système de rendu haut niveau\n");
    printf(" Démo : %d / 9\n", demoIdx);
    printf("=============================================================\n");
    printf(" Architecturé au-dessus de NKRHI (NKWindow + NKEvent)\n");
    printf(" Backends : OpenGL · Vulkan · DX11 · DX12 · Metal · NkSL\n");
    printf(" Modules  : PBR · Toon · Skin · Hair · Animation · VFX\n");
    printf("            PostProcess · Shadow · Text · Offscreen · PV3DE\n");
    printf("=============================================================\n\n");

    NkDemoApp app;
    if (!app.Init(demoIdx)) {
        fprintf(stderr, "[main] Initialisation échouée\n");
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
