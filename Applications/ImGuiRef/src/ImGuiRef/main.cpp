// =============================================================================
// ImGuiRef — Référence vivante Dear ImGui (docking + tous widgets) sur Nkentseu.
// PHASE 0 de la refonte UI (ROADMAP_UI_REWRITE.private.md) : le VRAI Dear ImGui
// tourne via NkImGuiCanvasBackend (ImGui -> NKCanvas) + glue NKEvent -> io.
// C'est l'ÉTALON ; le futur framework NKGui sera réécrit à notre manière (noms
// neufs, immédiat + retenu) en s'inspirant du comportement observé ici.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKTime/NkClock.h"
#include "NKMemory/NkUniquePtr.h"

#include "ImGui/NkImGuiCanvasBackend.h"   // Integrations/ImGui
#include "imgui.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "ImGuiRef";
    d.appVersion = "0.1.0";
    return d;
})());

// --- Mapping NkKey -> ImGuiKey (sous-ensemble suffisant pour la démo) --------
static ImGuiKey MapKey(NkKey k) {
    using K = NkKey;
    const int ki = static_cast<int>(k);
    if (k >= K::NK_A && k <= K::NK_Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + (ki - static_cast<int>(K::NK_A)));
    switch (k) {
        case K::NK_TAB:        return ImGuiKey_Tab;
        case K::NK_LEFT:       return ImGuiKey_LeftArrow;
        case K::NK_RIGHT:      return ImGuiKey_RightArrow;
        case K::NK_UP:         return ImGuiKey_UpArrow;
        case K::NK_DOWN:       return ImGuiKey_DownArrow;
        case K::NK_ENTER:      return ImGuiKey_Enter;
        case K::NK_ESCAPE:     return ImGuiKey_Escape;
        case K::NK_SPACE:      return ImGuiKey_Space;
        case K::NK_BACK:       return ImGuiKey_Backspace;
        case K::NK_DELETE:     return ImGuiKey_Delete;
        case K::NK_INSERT:     return ImGuiKey_Insert;
        case K::NK_HOME:       return ImGuiKey_Home;
        case K::NK_END:        return ImGuiKey_End;
        case K::NK_PAGE_UP:    return ImGuiKey_PageUp;
        case K::NK_PAGE_DOWN:  return ImGuiKey_PageDown;
        default:               return ImGuiKey_None;
    }
}

int nkmain(const NkEntryState& state) {
    (void)state;

    // ── Fenêtre + cible de rendu NKCanvas ────────────────────────────────────
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title     = "ImGui Reference - Dear ImGui (docking) sur Nkentseu";
    cfg.width     = 1280;
    cfg.height    = 720;
    cfg.centered  = true;
    cfg.resizable = true;
    if (!window.Create(cfg)) return -1;

    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
    auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
    if (!target || !target->IsValid()) return -1;

    // ── Contexte Dear ImGui + docking ────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;   // pas de imgui.ini
    ImGui::StyleColorsDark();
    io.DisplaySize = ImVec2(static_cast<float>(cfg.width), static_cast<float>(cfg.height));

    NkImGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer())) return -1;
    backend.RebuildFontAtlas();

    // ── Glue d'entrée NKEvent -> ImGui io ────────────────────────────────────
    auto& events = NkEvents();
    bool running = true;

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });

    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        io.AddMousePosEvent(static_cast<float>(e->GetX()), static_cast<float>(e->GetY()));
    });
    auto mouseBtn = [&](NkMouseButton b, bool down) {
        int idx = (b == NkMouseButton::NK_MB_LEFT)   ? 0
                : (b == NkMouseButton::NK_MB_RIGHT)  ? 1
                : (b == NkMouseButton::NK_MB_MIDDLE) ? 2 : -1;
        if (idx >= 0) io.AddMouseButtonEvent(idx, down);
    };
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) { mouseBtn(e->GetButton(), true); });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) { mouseBtn(e->GetButton(), false); });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        io.AddMouseWheelEvent(0.f, static_cast<float>(e->GetDeltaY()));
    });
    events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent* e) {
        io.AddMouseWheelEvent(static_cast<float>(e->GetDeltaX()), 0.f);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        if (const char* s = e->GetUtf8()) io.AddInputCharactersUTF8(s);
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        io.AddKeyEvent(ImGuiMod_Ctrl,  e->HasCtrl());
        io.AddKeyEvent(ImGuiMod_Shift, e->HasShift());
        io.AddKeyEvent(ImGuiMod_Alt,   e->HasAlt());
        const ImGuiKey k = MapKey(e->GetKey());
        if (k != ImGuiKey_None) io.AddKeyEvent(k, true);
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        io.AddKeyEvent(ImGuiMod_Ctrl,  e->HasCtrl());
        io.AddKeyEvent(ImGuiMod_Shift, e->HasShift());
        io.AddKeyEvent(ImGuiMod_Alt,   e->HasAlt());
        const ImGuiKey k = MapKey(e->GetKey());
        if (k != ImGuiKey_None) io.AddKeyEvent(k, false);
    });

    // ── Boucle principale ────────────────────────────────────────────────────
    NkClock clock;
    uint32  lastW = cfg.width, lastH = cfg.height;
    bool    showDemo = true;

    while (running && window.IsOpen()) {
        float dt = clock.Tick().delta;
        if (dt <= 0.f)  dt = 1.f / 60.f;
        if (dt > 0.1f)  dt = 0.1f;

        while (NkEvent* ev = NkEvents().PollEvent()) { (void)ev; }
        if (!running) break;

        const math::NkVec2u sz = target->GetSize();
        if (sz.x > 0 && sz.y > 0) {
            if (sz.x != lastW || sz.y != lastH) {
                if (lastW != 0) target->OnResize(sz.x, sz.y);
                lastW = sz.x; lastH = sz.y;
            }
            io.DisplaySize = ImVec2(static_cast<float>(sz.x), static_cast<float>(sz.y));
        }
        io.DeltaTime = dt;

        ImGui::NewFrame();

        // Dockspace plein écran (les fenêtres peuvent s'y ancrer).
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        // Étalon : TOUS les widgets / tables / docking d'ImGui.
        ImGui::ShowDemoWindow(&showDemo);

        // Panneau Nkentseu (rappel du rôle de référence).
        if (ImGui::Begin("Nkentseu - Reference")) {
            ImGui::TextUnformatted("Dear ImGui (docking) rendu via NKCanvas (NkImGuiCanvasBackend).");
            ImGui::TextUnformatted("Etalon pour la reecriture du framework NKGui (immediat + retenu).");
            ImGui::Separator();
            ImGui::Text("FPS: %.1f  (dt %.3f ms)", io.Framerate, dt * 1000.0f);
            ImGui::Checkbox("Afficher la demo ImGui", &showDemo);
        }
        ImGui::End();

        ImGui::Render();

        target->Clear();
        backend.RenderDrawData(ImGui::GetDrawData(), sz.x, sz.y);
        target->Display();
    }

    backend.Destroy();
    ImGui::DestroyContext();
    return 0;
}
