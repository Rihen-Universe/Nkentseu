// =============================================================================
// ex05_multi_window.cpp
// Multi-fenêtres — deux fenêtres indépendantes, events distingués par windowId
//
// Couvre : NkWindow x2, NkEventSystem multi-fenêtre, NkEvent::GetWindowId(),
//          WM_DESTROY conditionnel (PostQuitMessage seulement si 0 fenêtres),
//          rendu plasma avec couleur de teinte par fenêtre,
//          fermeture individuelle sans tuer l'autre.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKRenderer/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Ajout pour NkChrono et NkElapsedTime
#include "NKMath/NKMath.h"
#include "NKContainers/CacheFriendly/NkArray.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKStream/NkFileStream.h"

#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstdlib>
#include <string>

static bool ParseAppDataBool(const nkentseu::NkString& value, bool& outValue) {
    nkentseu::NkString v = value;
    v.Trim().ToLower();

    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        outValue = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        outValue = false;
        return true;
    }
    return false;
}

static void ApplyAppDataConfigPair(
    nkentseu::NkAppData& d,
    nkentseu::NkString key,
    nkentseu::NkString value)
{
    key.Trim().ToLower();
    value.Trim();

    bool b = false;
    if (key == "appname") {
        d.appName = value;
    } else if (key == "appversion") {
        d.appVersion = value;
    } else if (key == "enableeventlogging" && ParseAppDataBool(value, b)) {
        d.enableEventLogging = b;
    } else if (key == "enablerendererdebug" && ParseAppDataBool(value, b)) {
        d.enableRendererDebug = b;
    } else if (key == "enablemultiwindow" && ParseAppDataBool(value, b)) {
        d.enableMultiWindow = b;
    }
}

static bool LoadAppDataFromFile(const char* path, nkentseu::NkAppData& d) {
    if (!path || path[0] == '\0') {
        return false;
    }

    nkentseu::NkFileStream fs;
    if (!fs.Open(path, nkentseu::NkStream::ReadMode | nkentseu::NkStream::TextMode)) {
        return false;
    }

    const nkentseu::usize size = fs.Size();
    nkentseu::NkVector<char> bytes;
    bytes.Resize(size + 1);

    const nkentseu::usize read = fs.ReadRaw(bytes.Data(), size);
    fs.Close();
    bytes[read] = '\0';

    nkentseu::NkString text(bytes.Data(), read);
    nkentseu::usize start = 0;

    while (start < text.Size()) {
        nkentseu::usize end = text.Find('\n', start);
        if (end == nkentseu::NkString::npos) {
            end = text.Size();
        }

        nkentseu::NkString line = text.SubStr(start, end - start);
        line.Trim();

        if (!line.Empty()) {
            const bool isComment =
                line[0] == '#' ||
                line[0] == ';' ||
                (line.Size() >= 2 && line[0] == '/' && line[1] == '/');

            if (!isComment) {
                const nkentseu::usize eq = line.Find('=');
                if (eq != nkentseu::NkString::npos) {
                    nkentseu::NkString key = line.SubStr(0, eq);
                    nkentseu::NkString value = line.SubStr(eq + 1);
                    ApplyAppDataConfigPair(d, key, value);
                }
            }
        }

        if (end >= text.Size()) {
            break;
        }
        start = end + 1;
    }

    return true;
}

// AppData pattern #7: updater callback with optional env/file override.
static void ConfigureMultiWindowAppData(nkentseu::NkAppData& d) {
    d.appName = "SandboxMultiWindow";
    d.appVersion = "1.0.0";
    d.enableEventLogging = false;
    d.enableRendererDebug = false;
    d.enableMultiWindow = true;

    // 1) Optional file config
    // Env: NK_APPDATA_FILE=/path/to/file.appdata
    // Fallback relative path: Applications/Sandbox/Config/SandboxMultiWindow.appdata
    const char* filePath = std::getenv("NK_APPDATA_FILE");
    if (!filePath || filePath[0] == '\0') {
        filePath = "Applications/Sandbox/Config/SandboxMultiWindow.appdata";
    }
    (void)LoadAppDataFromFile(filePath, d);

    // 2) Optional env override has priority over file.
    const char* forceSingle = std::getenv("NK_SANDBOX_SINGLE_WINDOW");
    if (forceSingle && forceSingle[0] == '1') {
        d.enableMultiWindow = false;
    }
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureMultiWindowAppData);

#ifdef NkMin
#undef NkMin
#endif
#ifdef NkMax
#undef NkMax
#endif
#ifdef NkClamp
#undef NkClamp
#endif

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

// ---------------------------------------------------------------------------
// État par fenêtre
// ---------------------------------------------------------------------------
struct WinState {
    NkWindow*  window    = nullptr;
    NkRenderer renderer;
    NkWindowId id        = 0;
    bool       open      = false;
    bool       neon      = false;
    float      saturation = 1.15f;
    float      time       = 0.f;
    float      timeScale  = 1.f;
    float      phaseX     = 0.f;
    float      phaseY     = 0.f;
    float      hueShift   = 0.f;  // offset de teinte (0.0 / 0.33 / 0.66)
    NkString title;
};

// ---------------------------------------------------------------------------
// Plasma avec décalage de teinte par fenêtre
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, uint32 w, uint32 h,
                        float t, float px, float py,
                        float sat, float hue)
{
    if (!w || !h) return;
    const uint32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (uint32 y = 0; y < h; y += blk) {
        float fy = y/(float)h - 0.5f;
        for (uint32 x = 0; x < w; x += blk) {
            float fx  = x/(float)w - 0.5f;
            float rd  = math::NkSqrt(fx*fx + fy*fy);
            float mix = (math::NkSin((fx+px)*13.5f + t*1.7f)
                        +math::NkSin((fy+py)*11.f  - t*1.3f)
                        +math::NkSin(rd*24.f        - t*2.1f)) * 0.333f;
            uint8 ri = (uint8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+hue+0.f  ))-0.5f)*sat+0.5f)*255);
            uint8 gi = (uint8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+hue+0.33f))-0.5f)*sat+0.5f)*255);
            uint8 bi = (uint8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+hue+0.66f))-0.5f)*sat+0.5f)*255);
            uint32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (uint32 by=0;by<blk&&(y+by)<h;++by)
                for (uint32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((int32)(x+bx),(int32)(y+by),col);
        }
    }
}

// ---------------------------------------------------------------------------
// Traiter les événements d'une fenêtre donnée
// ---------------------------------------------------------------------------
static void ProcessWindowEvent(NkEvent* ev, WinState& ws)
{
    if (!ev) return;

    NkEventDispatcher d(ev);

    // Fermeture individuelle
    if (auto* e = ev->As<NkWindowCloseEvent>()) {
        (void)e;
        logger.Info("[Win:{0}] Close", ws.title.CStr());
        ws.open = false;
        ws.window->Close();
        return;
    }

    // Resize
    if (auto* e = ev->As<NkWindowResizeEvent>()) {
        ws.renderer.Resize(e->GetWidth(), e->GetHeight());
        logger.Info("[Win:{0}] Resize → {1}x{2}", ws.title.CStr(), e->GetWidth(), e->GetHeight());
        return;
    }

    // Clavier
    if (auto* e = ev->As<NkKeyPressEvent>()) {
        switch (e->GetKey()) {
            case NkKey::NK_SPACE:
                ws.neon = !ws.neon;
                logger.Info("[Win:{0}] Neon {1}", ws.title.CStr(), ws.neon ? "ON" : "OFF");
                break;
            case NkKey::NK_UP:
                ws.saturation = math::NkClamp(ws.saturation + 0.1f, 0.1f, 3.0f);
                break;
            case NkKey::NK_DOWN:
                ws.saturation = math::NkClamp(ws.saturation - 0.1f, 0.1f, 3.0f);
                break;
            case NkKey::NK_LEFT:
                ws.timeScale  = math::NkClamp(ws.timeScale - 0.1f, 0.1f, 5.0f);
                break;
            case NkKey::NK_RIGHT:
                ws.timeScale  = math::NkClamp(ws.timeScale + 0.1f, 0.1f, 5.0f);
                break;
            default: break;
        }
        return;
    }

    // Souris — déplacer la phase
    if (auto* e = ev->As<NkMouseMoveEvent>()) {
        if (e->GetButtons().IsDown(NkMouseButton::NK_MB_LEFT)) {
            ws.phaseX += e->GetDeltaX() * 0.003f;
            ws.phaseY += e->GetDeltaY() * 0.003f;
        }
        return;
    }

    if (auto* e = ev->As<NkMouseWheelVerticalEvent>()) {
        const float wheelDelta = static_cast<float>(e->GetDeltaY());
        ws.saturation = math::NkClamp(ws.saturation + wheelDelta * 0.1f, 0.1f, 3.0f);
        return;
    }

    // Focus
    if (auto* e = ev->As<NkWindowFocusGainedEvent>()) {
        logger.Info("[Win:{0}] Focus gained", ws.title.CStr());
        return;
    }
    if (auto* e = ev->As<NkWindowFocusLostEvent>()) {
        logger.Info("[Win:{0}] Focus lost", ws.title.CStr());
        return;
    }

    // Drag & Drop — fenêtre A seulement
    if (auto* e = ev->As<NkDropFileEvent>()) {
        logger.Info("[Win:{0}] Drop {1} file(s):", ws.title.CStr(), e->data.paths.Size());
        for (const auto& f : e->data.paths)
            logger.Info("  {0}", f.CStr());
        return;
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // 1. Runtime déjà initialisé par l'entrypoint NkMain

    // 2. Deux fenêtres
    NkArray<WinState, 2> wins;

    // Fenêtre A — rouge/plasma chaud, en haut à gauche
    NkWindowConfig cfgA;
    cfgA.title       = "ex05 — Fenêtre A (rouge)";
    cfgA.width       = 640;
    cfgA.height      = 480;
    cfgA.x           = 80;
    cfgA.y           = 100;
    cfgA.resizable   = true;
    cfgA.dropEnabled = true;

    // Fenêtre B — cyan/plasma froid, en bas à droite
    NkWindowConfig cfgB;
    cfgB.title       = "ex05 — Fenêtre B (cyan)";
    cfgB.width       = 640;
    cfgB.height      = 480;
    cfgB.x           = 820;
    cfgB.y           = 300;
    cfgB.resizable   = true;
    cfgB.dropEnabled = false;

    NkWindow windowA(cfgA);
    NkWindow windowB(cfgB);

    if (!windowA.IsOpen() || !windowB.IsOpen()) {
        logger.Error("[ex05] Window creation failed");
        return -2;
    }

    // 3. Renderer pour chaque fenêtre
    NkRendererConfig rcfg;
    rcfg.api                   = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;

    // Init wins[0]
    wins[0].window    = &windowA;
    wins[0].id        = windowA.GetId();
    wins[0].open      = true;
    wins[0].hueShift  = 0.0f;   // rouge dominant
    wins[0].timeScale = 1.0f;
    wins[0].title     = "A";
    if (!wins[0].renderer.Create(windowA, rcfg)) { return -3; }

    // Init wins[1]
    wins[1].window    = &windowB;
    wins[1].id        = windowB.GetId();
    wins[1].open      = true;
    wins[1].hueShift  = 0.33f;  // cyan dominant
    wins[1].timeScale = 0.7f;
    wins[1].title     = "B";
    if (!wins[1].renderer.Create(windowB, rcfg)) { return -4; }

    logger.Info("[ex05] Window A id={0}, Window B id={1}",
        static_cast<unsigned long long>(wins[0].id),
        static_cast<unsigned long long>(wins[1].id));
    logger.Info("[ex05] Controls: SPACE=neon, UP/DOWN=saturation, LEFT/RIGHT=speed, LMB drag=phase, scroll=sat");
    logger.Info("[ex05] Close one window independently — the other keeps running");

    auto& es = NkEvents();

    NkChrono chrono;
    NkElapsedTime elapsed;

    // 4. Boucle principale
    while (wins[0].open || wins[1].open)
    {
        NkElapsedTime e = chrono.Reset();

        // -----------------------------------------------------------------------
        // Pomper TOUS les events et les router par windowId
        // -----------------------------------------------------------------------
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            NkWindowId wid = ev->GetWindowId();

            // Touche ESC globale — ferme tout
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (k->GetKey() == NkKey::NK_ESCAPE) {
                    logger.Info("[ex05] ESC — closing all windows");
                    for (auto& w : wins) {
                        w.open = false;
                        if (w.window) w.window->Close();
                    }
                    break;
                }
            }

            // Router vers la bonne fenêtre
            bool dispatched = false;
            for (auto& ws : wins) {
                if (ws.open && ws.id == wid) {
                    ProcessWindowEvent(ev, ws);
                    dispatched = true;
                    break;
                }
            }

            // Event sans fenêtre associée (ex: gamepad connect)
            if (!dispatched) {
                if (auto* e = ev->As<NkGamepadConnectEvent>()) {
                    logger.Info("[ex05] Gamepad connected #{0} (global)", e->GetGamepadIndex());
                }
                if (auto* e = ev->As<NkGamepadDisconnectEvent>()) {
                    logger.Info("[ex05] Gamepad disconnected #{0} (global)", e->GetGamepadIndex());
                }
            }
        }

        // Vérifier si une fenêtre a été fermée par le bouton X
        for (auto& ws : wins) {
            if (ws.open && ws.window && !ws.window->IsOpen()) {
                ws.open = false;
                logger.Info("[ex05] Window {0} closed externally", ws.title.CStr());
            }
        }

        if (!wins[0].open && !wins[1].open) break;

        // -----------------------------------------------------------------------
        // Delta-time
        // -----------------------------------------------------------------------
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;

        // -----------------------------------------------------------------------
        // Rendu de chaque fenêtre ouverte
        // -----------------------------------------------------------------------
        for (auto& ws : wins)
        {
            if (!ws.open || !ws.window || !ws.window->IsOpen()) continue;

            ws.time += dt * ws.timeScale * (ws.neon ? 1.8f : 1.0f);

            ws.renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));

            const auto& fb = ws.renderer.GetFramebufferInfo();
            uint32 w = fb.width  ? fb.width  : 640u;
            uint32 h = fb.height ? fb.height : 480u;

            DrawPlasma(ws.renderer, w, h,
                       ws.time, ws.phaseX, ws.phaseY,
                       ws.saturation, ws.hueShift);

            ws.renderer.EndFrame();
            ws.renderer.Present();
        }

        // -----------------------------------------------------------------------
        // Cap 60fps
        // -----------------------------------------------------------------------
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // 5. Cleanup
    for (auto& ws : wins)
        ws.renderer.Shutdown();

    windowA.Close();
    windowB.Close();
    return 0;
}
