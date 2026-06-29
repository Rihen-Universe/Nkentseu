// =============================================================================
// main.cpp — NkAnimaEditor : éditeur d'animation (timeline) sur NKEditorKit.
// L'app ne touche QUE l'Editor Kit + AnimBridge (pas NKRenderer directement, pour
// éviter le conflit de types NKRenderer/NKCanvas). L'anim vit dans AnimBridge.cpp.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "AnimBridge.h"
#include "Panels.h"
#include "NkEditorRHIRenderer.h"   // UI sur NKRHI/NKRenderer (pas NKCanvas)

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkAnimaEditor";
    d.appVersion = "0.1.0";
    return d;
})());

static void CmdUndo(void*)   { nkanima::AnimUndo(); }
static void CmdRedo(void*)   { nkanima::AnimRedo(); }
static void CmdInsert(void*) { nkanima::AnimInsertKeyAtCursor(); }
static void CmdQuit(void* u) { if (u) static_cast<NkEditorShell*>(u)->RequestClose(); }

// Hook pré-UI : rend le viewport 3D dans l'offscreen (device partagé) AVANT la passe
// UI, puis publie sa texture au backend NKGui pour AddImage. user = NkEditorRHIRenderer*.
static void PreUI3D(NkICommandBuffer* cmd, void* user) {
    auto* r = static_cast<nkanima::NkEditorRHIRenderer*>(user);
    nkanima::Anim3DRenderOffscreen(cmd);
    nkanima::Anim3DRegisterInto(&r->GetBackend(), nkanima::ANIM_VIEWPORT_TEXID);
}

int nkmain(const NkEntryState& state) {
    // Backend graphique depuis les args (-bgl défaut, -bvk, -bdx11, -bdx12, -bsw).
    NkEditorGfxApi gfx = NkEditorGfxApi::OpenGL;
    for (const auto& a : state.GetArgs()) {
        if      (a == "-bvk"   || a == "--backend=vulkan") gfx = NkEditorGfxApi::Vulkan;
        else if (a == "-bdx11" || a == "--backend=dx11")   gfx = NkEditorGfxApi::DX11;
        else if (a == "-bdx12" || a == "--backend=dx12")   gfx = NkEditorGfxApi::DX12;
        else if (a == "-bsw"   || a == "--backend=sw")     gfx = NkEditorGfxApi::Software;
        else if (a == "-bgl"   || a == "--backend=opengl") gfx = NkEditorGfxApi::OpenGL;
    }

    auto shell = memory::NkMakeUnique<NkEditorShell>();
    // Backend de rendu NKRHI/NKRenderer injecte (PAS NKCanvas) : l'UI NKGui et le
    // viewport 3D partageront ce device. Doit survivre au shell.
    static nkanima::NkEditorRHIRenderer rhi;
    NkEditorShellConfig cfg;
    cfg.title    = "NkAnimaEditor — timeline (NkAnima M1.c)";
    cfg.width    = 1280;
    cfg.height   = 720;
    cfg.graphicsApi = gfx;        // choisi par -b<backend> (OpenGL par défaut)
    cfg.renderer = &rhi;
    if (!shell || !shell->Init(cfg)) return -1;

    // ── Thème « Dark Pro » épuré (cf interface.md §1 : fond #1a1a1a, accent cyan) ──
    {
        nkgui::NkGuiTheme t;
        t.bgPrimary    = {  26,  26,  28, 255 };   // #1a1a1c quasi-noir
        t.panel        = {  30,  31,  34, 255 };
        t.header       = {  34,  35,  39, 255 };
        t.button       = {  40,  42,  47, 255 };
        t.buttonHover  = {  52,  56,  64, 255 };
        t.buttonActive = {   0, 168, 208, 255 };   // cyan profond
        t.border       = {  46,  48,  55, 255 };
        t.text         = { 222, 226, 232, 255 };
        t.textDisabled = { 110, 114, 124, 255 };
        t.selection    = {   0, 150, 190, 200 };
        t.accent       = {   0, 212, 255, 255 };    // #00d4ff cyan
        t.track        = {  24,  25,  28, 255 };
        t.tabBar       = {  20,  20,  22, 255 };
        t.tab          = {  28,  29,  32, 255 };
        t.tabHover     = {  40,  42,  47, 255 };
        t.tabActive    = {  34,  36,  41, 255 };
        t.rounding     = 4.f;
        t.framePadX    = 12.f;
        t.framePadY    = 7.f;
        shell->Ui().theme = t;
    }

    nkanima::AnimInit("Resources/Models/CesiumMan/CesiumMan.glb");

    // Viewport 3D : partage le device NKRHI de l'éditeur + rend en début de frame.
    nkanima::Anim3DSetSharedDevice(rhi.GetDevice());
    rhi.SetPreUI(&PreUI3D, &rhi);

    static nkanima::PreviewPanel  preview;
    static nkanima::TimelinePanel timeline;
    shell->AddPanel(&preview);
    shell->AddPanel(&timeline);

    shell->RegisterCommand("Edition: Inserer cle", &CmdInsert, nullptr, "I");
    shell->RegisterCommand("Edition: Annuler",     &CmdUndo,   nullptr, "Ctrl+Z");
    shell->RegisterCommand("Edition: Refaire",     &CmdRedo,   nullptr, "Ctrl+Y");
    shell->RegisterCommand("Application: Quitter", &CmdQuit,   shell.Get(), "Ctrl+Q");

    return shell->Run();
}
