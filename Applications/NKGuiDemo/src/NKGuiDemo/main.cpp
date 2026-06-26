// =============================================================================
// NKGuiDemo — validation Phase 2 du Cœur NKGui.
// Prouve le pipeline complet : input (NKEvent) -> NkGuiContext -> NkGuiDrawList
// -> backend NKCanvas -> écran, avec interaction (boutons hot/active/click) et
// l'état MoveWindow (boîte déplaçable). Pas encore de texte (Phase 3 = NKFont).
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
#include "NKLogger/NkLog.h"
#include "NKGui/NKGui.h"
#include "NKImage/Core/NkImage.h"   // chargement de vraies images (PNG/JPG/...)
#include "NKReflection/NkRegistry.h"      // reflexion : macros + registre + NkClass
#include "NKReflection/NkInspector.h"     // EnumerateEditableProperties / SetPropertyByName
#include "NKReflection/NkReflectVariant.h"
#include "NKContainers/String/NkString.h"
#include "NkGuiCanvasBackend.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;

// ── DOGFOODING NKEditorKit (tranche 1) : un INSPECTEUR generique pilote par la
//    REFLEXION (NKReflection). On reflechit une struct de demo, puis DrawInspector
//    genere le bon widget NKGui par propriete et reecrit la valeur si editee.
//    1re brique a extraire ensuite dans NKEditorKit-sur-NKGui. ──
struct DemoEntity {
    NKENTSEU_REFLECT_CLASS(DemoEntity)
public:
    float32 posX = 1.5f;
    NKENTSEU_REFLECT_PROPERTY(posX)
public:
    float32 posY = -0.5f;
    NKENTSEU_REFLECT_PROPERTY(posY)
public:
    float32 scale = 1.0f;
    NKENTSEU_REFLECT_PROPERTY(scale)
public:
    bool visible = true;
    NKENTSEU_REFLECT_PROPERTY(visible)
public:
    int32 health = 80;
    NKENTSEU_REFLECT_PROPERTY(health)
public:
    NkString name = NkString("Joueur");
    NKENTSEU_REFLECT_PROPERTY(name)
};
NKENTSEU_REGISTER_CLASS(DemoEntity)

// Inspecteur GENERIQUE : pour chaque propriete editable de `cls`, dessine le widget
// NKGui adapte a sa categorie et reecrit la valeur en live si elle change.
static void DrawInspector(NkGuiContext& ctx, void* obj, const reflection::NkClass* cls) {
    using namespace nkentseu::reflection;
    if (!cls || !obj) return;
    NkVector<NkEditableProperty> props = EnumerateEditableProperties(cls, obj);
    for (usize i = 0; i < props.Size(); ++i) {
        const NkEditableProperty& p = props[i];
        if (p.hidden) continue;
        const char* lbl = p.displayName ? p.displayName : p.name;
        switch (p.category) {
            case NkTypeCategory::NK_BOOL: {
                bool v = p.value.ToBool();
                if (Checkbox(ctx, lbl, v)) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<bool>(v));
                break;
            }
            case NkTypeCategory::NK_FLOAT32:
            case NkTypeCategory::NK_FLOAT64: {
                float32 v = static_cast<float32>(p.value.ToFloat64());
                const bool ch = p.hasRange ? SliderFloat(ctx, lbl, v, p.rangeMin, p.rangeMax)
                                           : DragFloat(ctx, lbl, v, 0.05f);
                if (ch) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<float32>(v));
                break;
            }
            case NkTypeCategory::NK_INT8:  case NkTypeCategory::NK_INT16:
            case NkTypeCategory::NK_INT32: case NkTypeCategory::NK_INT64:
            case NkTypeCategory::NK_UINT8: case NkTypeCategory::NK_UINT16:
            case NkTypeCategory::NK_UINT32: case NkTypeCategory::NK_UINT64: {
                int32 v = static_cast<int32>(p.value.ToInt64());
                const bool ch = p.hasRange ? DragInt(ctx, lbl, v, 0.25f, static_cast<int32>(p.rangeMin), static_cast<int32>(p.rangeMax))
                                           : DragInt(ctx, lbl, v);
                if (ch) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<int32>(v));
                break;
            }
            case NkTypeCategory::NK_STRING: {
                char buf[256] = {};
                NkString s; p.value.Get<NkString>(s);
                const char* cs = s.CStr();
                int32 n = 0; while (cs && cs[n] && n < 255) { buf[n] = cs[n]; ++n; }
                buf[n] = '\0';
                if (InputText(ctx, lbl, buf, 256)) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<NkString>(NkString(buf)));
                break;
            }
            default: {
                char line[160];
                std::snprintf(line, sizeof(line), "%s : (%s)", lbl,
                              p.isContainer ? "liste" : p.isObject ? "objet" : "type non gere");
                Text(ctx, line);
                break;
            }
        }
    }
}

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NKGuiDemo";
    d.appVersion = "0.1.0";
    return d;
})());

// Hook de style DEMO : re-skinne UNIQUEMENT les boutons dont le libelle commence
// par '*' (pilule a degrade orange->magenta) ; tout le reste garde le defaut.
// Prouve qu'on peut re-styliser un widget SANS toucher a sa logique (Button() inchange).
static bool DemoStyle(NkGuiContext& ctx, const NkGuiStyleItem& it, void*) {
    using namespace nkentseu::nkgui;
    if (it.kind == NkGuiStyleKind::Button && it.label && it.label[0] == '*') {
        const NkColor a = it.active ? NkColor{ 255, 200, 90, 255 } : it.hovered ? NkColor{ 255, 160, 60, 255 } : NkColor{ 235, 120, 40, 255 };
        const NkColor b = it.active ? NkColor{ 255, 120, 190, 255 } : it.hovered ? NkColor{ 235, 80, 165, 255 } : NkColor{ 205, 55, 140, 255 };
        ctx.DL().AddRectFilledMultiColor(it.rect, a, b, b, a);
        ctx.DL().AddRect(it.rect, NkColor{ 255, 255, 255, 200 }, 1.5f);
        if (ctx.font && ctx.font->Valid()) {
            const char*   lbl = it.label + 1;                       // saute le '*'
            const float32 tw  = ctx.font->MeasureWidth(lbl);
            const float32 tx  = it.rect.x + (it.rect.w - tw) * 0.5f;
            const float32 by  = it.rect.y + (it.rect.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), { tx, by }, lbl, NkColor{ 255, 255, 255, 255 });
        }
        return true;   // dessine par l'app -> NKGui saute le defaut
    }
    // #1 : pastilles de dock 100% CUSTOM (rondes + orange si visée ; bord = ligne orange).
    if (it.kind == NkGuiStyleKind::DockTarget) {
        if (it.value == 5) { ctx.DL().AddRectFilled(it.rect, NkColor{ 255, 130, 50, 255 }, 0.f); return true; }
        const NkVec2  c   = { it.rect.x + it.rect.w * 0.5f, it.rect.y + it.rect.h * 0.5f };
        const float32 rad = it.rect.w * 0.5f;
        ctx.DL().AddCircleFilled(c, rad,        it.active ? NkColor{ 255, 130, 50, 255 } : NkColor{ 64, 70, 86, 240 });
        ctx.DL().AddCircleFilled(c, rad - 2.f,  it.active ? NkColor{ 255, 175, 95, 255 } : NkColor{ 82, 90, 110, 240 });
        const NkColor ic = it.active ? NkColor{ 35, 20, 8, 255 } : NkColor{ 230, 232, 240, 255 };
        const float32 a = 5.f, cx = c.x, cy = c.y;
        if      (it.value == 0) ctx.DL().AddCircleFilled(c, 3.f, ic);                                              // onglet
        else if (it.value == 1) ctx.DL().AddTriangleFilled({ cx - a, cy }, { cx + a*0.5f, cy - a }, { cx + a*0.5f, cy + a }, ic);
        else if (it.value == 2) ctx.DL().AddTriangleFilled({ cx + a, cy }, { cx - a*0.5f, cy - a }, { cx - a*0.5f, cy + a }, ic);
        else if (it.value == 3) ctx.DL().AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a*0.5f }, { cx + a, cy + a*0.5f }, ic);
        else                    ctx.DL().AddTriangleFilled({ cx, cy + a }, { cx - a, cy - a*0.5f }, { cx + a, cy - a*0.5f }, ic);
        return true;
    }
    return false;      // defaut pour tout le reste
}

int nkmain(const NkEntryState& state) {
    (void)state;

    NkWindow window;
    NkWindowConfig cfg;
    cfg.title     = "NKGui - Demo Phase 2 (Coeur : drawlist + interaction)";
    cfg.width     = 1100;
    cfg.height    = 800;
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

    auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
    if (!ctxPtr) return -1;
    NkGuiContext& ctx = *ctxPtr;
    ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height));
    ctx.styleFn = &DemoStyle;   // hook de style (override de dessin par widget)
    SetCurrentContext(&ctx);

    nkgdemo::NkGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer())) return -1;

    // Police par défaut (embarquée) + upload de l'atlas dans le backend.
    // DPI/HiDPI : pour un ecran a 150%, faire SetUiScale(ctx, 1.5f) + charger la police a
    // 18.f * 1.5f (atlas net). Ici echelle 1.0 par defaut.
    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f)) {
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    }
    ctx.font = fontPtr.Get();
    if (fontPtr->Valid()) {
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
    }

    // ── VRAIE image chargee depuis le disque (NKImage) -> texture backend ──────
    uint32 imgTexId = 0u; int32 imgW = 0, imgH = 0;
    {
        static const char* kCandidates[] = {
            "Applications/Mou/assets/brand/rihen-logo.png",
            "Applications/Mou/assets/brand/noge-logo.png",
            "Resources/Icons/ContentBrowser/FileIcon.png",
            "../../../Applications/Mou/assets/brand/rihen-logo.png",
        };
        NkImage img;
        bool ok = false;
        for (const char* p : kCandidates) if (img.Load(p, 4)) { ok = true; break; }
        if (ok && img.Width() > 0 && img.Height() > 0 && img.Pixels()) {
            imgW = img.Width(); imgH = img.Height();
            static NkVector<uint8> rgba;
            rgba.Resize(static_cast<usize>(imgW) * imgH * 4u);
            for (int32 y = 0; y < imgH; ++y) {
                const uint8* src = img.RowPtr(y);
                uint8*       dst = rgba.Data() + static_cast<usize>(y) * imgW * 4u;
                for (int32 x = 0; x < imgW * 4; ++x) dst[x] = src[x];
            }
            imgTexId = 0x494D4731u;   // 'IMG1'
            backend.UploadImageRGBA(imgTexId, rgba.Data(), imgW, imgH);
            logger.Info("Image chargee : {0}x{1}", imgW, imgH);
        } else {
            logger.Info("Image demo introuvable (cwd) -> section image vide");
        }
    }

    // ── Glue d'entrée (souris) ───────────────────────────────────────────────
    auto& events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        ctx.input.mousePos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = true;
        if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = true;
        ctx.input.ctrlDown  = e->GetModifiers().ctrl;
        ctx.input.shiftDown = e->GetModifiers().shift;
        ctx.input.altDown   = e->GetModifiers().alt;
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  ctx.input.mouseDown[0] = false;
        if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) ctx.input.mouseDown[1] = false;
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        ctx.input.wheel += static_cast<float32>(e->GetDeltaY());   // >0 = haut ; consommé en EndFrame
        const auto m = e->GetModifiers();                         // modificateurs au moment de la molette
        ctx.input.ctrlDown = m.ctrl; ctx.input.shiftDown = m.shift; ctx.input.altDown = m.alt;
    });
    events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent* e) {
        ctx.input.wheelH += static_cast<float32>(e->GetDeltaX());  // molette horizontale
    });
    // Double-clic détecté par l'OS : NKEvent l'émet comme événement DÉDIÉ (le 2e
    // clic n'arrive PAS comme un mouseDown normal) → on l'injecte explicitement.
    events.AddEventCallback<NkMouseDoubleClickEvent>([&](NkMouseDoubleClickEvent* e) {
        ctx.input.mousePos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.SetDoubleClick(0);
    });
    // Saisie texte + touches d'édition (pour InputText). On pose l'état ENFONCÉ
    // (press/release) pour permettre la répétition au maintien.
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        ctx.input.PushChar(e->GetCodepoint());
    });
    auto setKey = [&](NkKey k, bool down) {
        switch (k) {
            case NkKey::NK_LEFT:   ctx.input.SetKey(NkGuiKey::Left,      down); break;
            case NkKey::NK_RIGHT:  ctx.input.SetKey(NkGuiKey::Right,     down); break;
            case NkKey::NK_UP:     ctx.input.SetKey(NkGuiKey::Up,        down); break;
            case NkKey::NK_DOWN:   ctx.input.SetKey(NkGuiKey::Down,      down); break;
            case NkKey::NK_HOME:   ctx.input.SetKey(NkGuiKey::Home,      down); break;
            case NkKey::NK_END:    ctx.input.SetKey(NkGuiKey::End,       down); break;
            case NkKey::NK_BACK:   ctx.input.SetKey(NkGuiKey::Backspace, down); break;
            case NkKey::NK_DELETE: ctx.input.SetKey(NkGuiKey::Delete,    down); break;
            case NkKey::NK_ENTER:  ctx.input.SetKey(NkGuiKey::Enter,     down); break;
            case NkKey::NK_ESCAPE: ctx.input.SetKey(NkGuiKey::Escape,    down); break;
            default: break;
        }
    };
    bool captureRequested = false;   // F12 → capture la frame courante (apres Display)
    float32 pendingScale  = 0.f;     // F9 → cycle l'echelle DPI (applique dans la boucle)
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        setKey(e->GetKey(), true);
        ctx.input.ctrlDown  = e->GetModifiers().ctrl;
        ctx.input.shiftDown = e->GetModifiers().shift;
        if (e->GetKey() == NkKey::NK_F12) captureRequested = true;
        if (e->GetKey() == NkKey::NK_F9) {                          // cycle DPI 1.0→1.25→1.5→2.0
            const float32 s = ctx.scale;
            pendingScale = s < 1.1f ? 1.25f : s < 1.4f ? 1.5f : s < 1.9f ? 2.0f : 1.0f;
        }
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        setKey(e->GetKey(), false);
        ctx.input.ctrlDown  = e->GetModifiers().ctrl;
        ctx.input.shiftDown = e->GetModifiers().shift;
    });

    NkClock clock;
    NkVec2  boxPos { 950.f, 322.f };   // sous le panneau Controle
    NkVec2  dragOff{ 0.f, 0.f };
    int32   clicks[2] = { 0, 0 };
    bool    cbVisible = true, cbLock = false, cbShadow = true;
    float32 sPosX = 0.f, sScale = 1.f;
    uint32  lastW = 0, lastH = 0;   // 0 → sync swapchain à la fenêtre dès la 1re frame

    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt <= 0.f) dt = 1.f / 60.f;
        if (dt > 0.1f) dt = 0.1f;

        while (NkEvent* ev = NkEvents().PollEvent()) { (void)ev; }
        if (!running) break;

        // Synchroniser la SWAPCHAIN à la taille de la FENÊTRE. target->GetSize()
        // renvoie la taille de la swapchain (qui ne change QU'À OnResize) — s'en
        // servir pour détecter le resize était faux : la swapchain restait figée à
        // sa taille de création (rendu basse-résolution étiré). On pilote OnResize
        // depuis la taille fenêtre (inclut la 1re frame → sync initiale).
        const math::NkVec2u wsz = target->GetWindow().GetSize();
        if (wsz.x > 0 && wsz.y > 0 && (wsz.x != lastW || wsz.y != lastH)) {
            target->OnResize(wsz.x, wsz.y);
            lastW = wsz.x; lastH = wsz.y;
        }
        const math::NkVec2u sz = target->GetSize();   // = wsz après OnResize
        if (sz.x > 0 && sz.y > 0) {
            ctx.viewW = static_cast<int32>(sz.x);
            ctx.viewH = static_cast<int32>(sz.y);
        }

        if (pendingScale > 0.f) {                       // F9 : appliquer la nouvelle echelle DPI
            SetUiScale(ctx, pendingScale);
            if (fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f * pendingScale))
                backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
            pendingScale = 0.f;
        }

        ctx.BeginFrame(dt);
        NkGuiDrawList& dl = ctx.dl;
        const float32 W = static_cast<float32>(ctx.viewW);
        const float32 H = static_cast<float32>(ctx.viewH);

        // Fond + barre d'en-tête + TITRE (texte NKFont).
        dl.AddRectFilled({ 0.f, 0.f, W, H }, ctx.theme.bgPrimary);
        dl.AddRectFilled({ 0.f, 0.f, W, 40.f }, ctx.theme.header);
        dl.AddRectFilled({ 0.f, 38.f, W, 2.f }, ctx.theme.accent);
        TextAt(ctx, { 16.f, 11.f }, "NKGui - Phase 3 : layout + widgets (NKFont)");

        // Barre de menus + sous-menus IMBRIQUES (clic droit = menu contextuel).
        if (BeginMenuBar(ctx, { 0.f, 40.f, W, 24.f })) {
            if (BeginMenu(ctx, "Fichier")) {
                MenuItem(ctx, "Nouveau", "Ctrl+N");
                MenuItem(ctx, "Ouvrir",  "Ctrl+O");
                if (BeginMenu(ctx, "Recents")) {              // sous-menu
                    MenuItem(ctx, "projet_a.nk");
                    MenuItem(ctx, "projet_b.nk");
                    EndMenu(ctx);
                }
                MenuItem(ctx, "Quitter", "Alt+F4");
                EndMenu(ctx);
            }
            if (BeginMenu(ctx, "Edition")) {
                MenuItem(ctx, "Annuler", "Ctrl+Z");
                MenuItem(ctx, "Refaire", "Ctrl+Y", false);   // desactive
                EndMenu(ctx);
            }
            if (BeginMenu(ctx, "Affichage")) {
                MenuItem(ctx, "Plein ecran", "F11");
                if (BeginMenu(ctx, "Grille")) {              // sous-menu
                    MenuItem(ctx, "Afficher");
                    if (BeginMenu(ctx, "Densite")) {         // SOUS-SOUS-menu
                        MenuItem(ctx, "Fine");
                        MenuItem(ctx, "Normale");
                        MenuItem(ctx, "Large");
                        EndMenu(ctx);
                    }
                    EndMenu(ctx);
                }
                EndMenu(ctx);
            }
            EndMenuBar(ctx);
        }

        // ── Panneau de CONTROLE : affiche/masque les sections (show/hide) ──────
        static bool showWidgets = true, showStructure = true, showDragBox = true,
                    showListBox = true, showMulti = true, showHScroll = true,
                    showTable = true;
        {
            const NkRect ctrlR = { 920.f, 64.f, 175.f, 300.f };
            if (BeginPanel(ctx, "Controle", ctrlR)) {
                Text(ctx, "Afficher :");
                Checkbox(ctx, "Widgets",     showWidgets);
                Checkbox(ctx, "Structure",   showStructure);
                Checkbox(ctx, "Boite drag",  showDragBox);
                Checkbox(ctx, "ListBox",     showListBox);
                Checkbox(ctx, "Multi-ligne", showMulti);
                Checkbox(ctx, "Scroll H",    showHScroll);
                Checkbox(ctx, "Table",       showTable);
                EndPanel(ctx);
            }
        }

        // Panneau de widgets (auto-layout NKGui : Text/Button/Checkbox/Slider).
        const NkRect panel = { 40.f, 64.f, 440.f, H - 168.f };
        if (showWidgets && BeginPanel(ctx, "Widgets", panel)) {
            Text(ctx, "Boutons :");
            if (Button(ctx, "Bouton normal")) ++clicks[0];
            if (ctx.IsItemHovered()) SetTooltip(ctx, "Incremente le compteur de clics (infobulle overlay)");
            ctx.SameLine();
            {
                const NkRect rb = ctx.NextItemRect(180.f, ctx.ItemHeight());
                if (RepeatButton(ctx, "Rafale (maintenir)", rb, 0.20f, 0.03f)) ++clicks[1];
            }
            char buf[64];
            ::snprintf(buf, sizeof(buf), "Clics: %d        Rafale: %d", clicks[0], clicks[1]);
            Text(ctx, buf);
            // Bouton RE-SKINNE via le hook de style (libelle '*…') — logique inchangee.
            if (Button(ctx, "*Bouton stylise (hook)")) ++clicks[0];

            Separator(ctx);
            Text(ctx, "Options :");
            // Case TRI-ÉTAT « tout sélectionner » : Mixed quand 1 ou 2/3 cochées.
            const int32 onCount = (cbVisible ? 1 : 0) + (cbLock ? 1 : 0) + (cbShadow ? 1 : 0);
            NkGuiCheck all = (onCount == 0) ? NkGuiCheck::Off
                           : (onCount == 3) ? NkGuiCheck::On
                                            : NkGuiCheck::Mixed;
            if (CheckboxTristate(ctx, "Tout selectionner", all)) {
                const bool v = (all == NkGuiCheck::On);
                cbVisible = cbLock = cbShadow = v;
            }
            Checkbox(ctx, "Visible", cbVisible);
            Checkbox(ctx, "Verrouille", cbLock);
            Checkbox(ctx, "Projette une ombre", cbShadow);

            Separator(ctx);
            Text(ctx, "Reglages :");
            SliderFloat(ctx, "Position X", sPosX, -100.f, 100.f);
            SliderFloat(ctx, "Echelle", sScale, 0.1f, 5.f);
            static float32 dragF = 1.5f;
            DragFloat(ctx, "Drag H (glisser/2-clic)", dragF, 0.05f, 0.f, 100.f);
            static float32 dragV = 50.f;
            DragFloat(ctx, "Drag V (vertical)", dragV, 0.2f, 0.f, 100.f, NkGuiDragDir::Vertical);
            static int32 stepI = 10;
            InputInt(ctx, "Stepper (-/+)", stepI, 1, 0, 999);

            Separator(ctx);
            Text(ctx, "Saisie :");
            static char nameBuf[64] = "Edite-moi (clic + clavier)";
            InputText(ctx, "Nom", nameBuf, static_cast<int32>(sizeof(nameBuf)));
            static char pathBuf[96] = "";
            InputText(ctx, "Chemin", pathBuf, static_cast<int32>(sizeof(pathBuf)));
            static char pwBuf[32]   = "secret";
            InputTextEx(ctx, "Mot de passe", pwBuf, 32, NkGuiInputFlags::Password);
            static char numBuf[16]  = "";
            InputTextEx(ctx, "Numerique", numBuf, 16, NkGuiInputFlags::CharsDecimal);
            static char codeBuf[32] = "AB12";
            InputTextEx(ctx, "Code MAJ (max 6)", codeBuf, 32,
                        NkGuiInputFlags::Uppercase | NkGuiInputFlags::NoBlank, 6);
            static char roBuf[32]   = "Lecture seule";
            InputTextEx(ctx, "Read-only", roBuf, 32, NkGuiInputFlags::ReadOnly);

            Separator(ctx);
            Text(ctx, "Liste deroulante (combo, popup overlay) :");
            static int32 comboSel = 0;
            static const char* comboOpts[4] = { "Option A", "Option B", "Option C", "Option D" };
            if (BeginCombo(ctx, "Mode", comboOpts[comboSel], 4)) {
                for (int32 i = 0; i < 4; ++i)
                    if (Selectable(ctx, comboOpts[i], i == comboSel)) { comboSel = i; ctx.ClosePopup(); }
                EndCombo(ctx);
            }

            Separator(ctx);
            Text(ctx, "Graphiques (survol = valeur) :");
            static float32 prog = 0.f; prog += dt * 0.2f; if (prog > 1.f) prog -= 1.f;
            ProgressBar(ctx, prog);
            static const float32 wave[16] = { 0.2f, 0.5f, 0.8f, 0.6f, 0.9f, 0.4f, 0.3f, 0.7f,
                                              1.0f, 0.6f, 0.2f, 0.5f, 0.8f, 0.35f, 0.55f, 0.75f };
            PlotLines(ctx, "Courbe", wave, 16);
            static const float32 bars[8] = { 3.f, 7.f, 2.f, 9.f, 5.f, 8.f, 4.f, 6.f };
            PlotHistogram(ctx, "Histo", bars, 8);

            Separator(ctx);
            Text(ctx, "Couleur (clic pastille = picker) :");
            static float32 col1[4] = { 0.40f, 0.70f, 1.00f, 1.00f };
            ColorEdit4(ctx, "Barre", col1);
            static float32 col2[4] = { 1.00f, 0.55f, 0.20f, 1.00f };
            ColorEdit4(ctx, "Roue", col2, NkGuiColorFlags::Wheel);

            Separator(ctx);
            Text(ctx, "Image reelle (chargee via NKImage) :");
            if (imgTexId != 0u && imgW > 0 && imgH > 0) {
                // Affiche l'image a une largeur fixe en preservant le ratio.
                const float32 dispW = 150.f;
                const float32 dispH = dispW * static_cast<float32>(imgH) / static_cast<float32>(imgW);
                Image(ctx, imgTexId, dispW, dispH);
                ctx.SameLine();
                static int32 ibClicks = 0;
                if (ImageButton(ctx, "ib", imgTexId, 44.f, 44.f)) ++ibClicks;
            } else {
                Text(ctx, "(aucune image trouvee)");
            }

            EndPanel(ctx);
        }

        // Second panneau : arbre (feuilles sélectionnables) + onglets (1 désactivé).
        const NkRect panel2 = { 500.f, 64.f, 400.f, 466.f };
        if (showStructure && BeginPanel(ctx, "Structure", panel2)) {
            Text(ctx, "Arbre (double-clic = renommer) :");
            static int32 selLeaf = -1;
            static char rootName[64] = "Racine";
            static char subName[64]  = "Sous-arbre";
            static char leafA[64]  = "Enfant A";
            static char leaf1[64]  = "Feuille 1";
            static char leaf2[64]  = "Feuille 2 (non renommable)";
            if (TreeNodeEditable(ctx, "root", rootName, 64)) {
                if (SelectableEditable(ctx, "lA", leafA, 64, selLeaf == 0)) selLeaf = 0;
                if (TreeNodeEditable(ctx, "sub", subName, 64)) {
                    if (SelectableEditable(ctx, "l1", leaf1, 64, selLeaf == 1)) selLeaf = 1;
                    // allowRename=false → cette feuille refuse le renommage (mais reste sélectionnable).
                    if (SelectableEditable(ctx, "l2", leaf2, 64, selLeaf == 2, /*allowRename=*/false)) selLeaf = 2;
                    TreePop(ctx);
                }
                // Élément NON sélectionnable selon le contexte (désactivé).
                ctx.BeginDisabled(true);
                Selectable(ctx, "Enfant B (desactive)", false);
                ctx.EndDisabled();
                TreePop(ctx);
            }
            Separator(ctx);
            Text(ctx, "Onglets (double-clic = renommer ; 3e desactive) :");
            static char tab0[32] = "Infos";
            static char tab1[32] = "Style";
            static char tab2[32] = "Avance";
            static char* kTabs[3] = { tab0, tab1, tab2 };
            static const bool kTabEn[3] = { true, true, false };   // "Avance" desactive
            static const bool kTabRn[3] = { true, true, false };   // 3e onglet : pas de rename
            const int32 tsel = TabBarEditable(ctx, "tabs", kTabs, 3, 32, kTabEn, kTabRn);
            if      (tsel == 0) Text(ctx, "Contenu : Infos");
            else if (tsel == 1) Text(ctx, "Contenu : Style");
            else                Text(ctx, "Contenu : Avance");

            Separator(ctx);
            Text(ctx, "Liste multi-select (double-clic = renommer) :");
            static bool listSel[6] = {};
            static char listItems[6][64] = { "Pomme", "Banane", "Cerise", "Datte", "Figue", "Goyave" };
            static const char* listIds[6] = { "f0", "f1", "f2", "f3", "f4", "f5" };
            ctx.BeginSelectList("fruits", listSel, 6, NkGuiSelectFlags::MultiSelect);
            for (int32 i = 0; i < 6; ++i) SelectItemEditable(ctx, listIds[i], listItems[i], 64);
            ctx.EndSelectList();

            // Arbre à CASES tri-état + cascade hiérarchique (modèle hybride : NKGui
            // ne stocke rien, on possède states[]+parent[] et on appelle les helpers).
            Separator(ctx);
            Text(ctx, "Arbre a cases (Alt+clic = cascade recursive) :");
            // Etat initial : wood.png coche, rock.png decoche -> Textures et Assets
            // s'affichent en MIXTE (tiret) des le demarrage (preuve tri-etat).
            static NkGuiCheck cs[6] = { NkGuiCheck::Off, NkGuiCheck::Off, NkGuiCheck::On,
                                        NkGuiCheck::Off, NkGuiCheck::Off, NkGuiCheck::Off };
            static const int32 par[6] = { -1, 0, 1, 1, 0, 4 };  // hierarchie
            static char nAssets[32] = "Assets", nTex[32] = "Textures", nWood[32] = "wood.png",
                        nRock[32]   = "rock.png", nSnd[32] = "Sounds",  nBeep[32] = "beep.wav";
            auto crow = [&](const char* ck, int32 idx) {
                if (CheckBox3(ctx, ck, cs[idx]) && ctx.input.altDown)
                    NkGuiTreeCascade(cs, par, 6, idx, cs[idx]);  // cascade SEULEMENT avec Alt
                ctx.SameLine(4.f);
            };
            crow("c0", 0); if (TreeNodeEditable(ctx, "h0", nAssets, 32)) {
                crow("c1", 1); if (TreeNodeEditable(ctx, "h1", nTex, 32)) {
                    crow("c2", 2); SelectableEditable(ctx, "h2", nWood, 32, false);
                    crow("c3", 3); SelectableEditable(ctx, "h3", nRock, 32, false);
                    TreePop(ctx);
                }
                crow("c4", 4); if (TreeNodeEditable(ctx, "h4", nSnd, 32)) {
                    crow("c5", 5); SelectableEditable(ctx, "h5", nBeep, 32, false);
                    TreePop(ctx);
                }
                TreePop(ctx);
            }
            NkGuiTreeRecomputeMixed(cs, par, 6);   // parents reflètent leurs enfants (tri-état)

            EndPanel(ctx);
        }

        // Panneau TABLE : colonnes redimensionnables (glisser les separateurs
        // d'en-tete -> largeurs persistees), zebrures (RowBg), en-tete. La table
        // vit dans le layout du panneau (elle scrolle si elle deborde).
        if (showTable && BeginPanel(ctx, "Table", { 500.f, 540.f, 400.f, 234.f })) {
            Text(ctx, "Clic en-tete = trier ; double-clic Nom = editer :");
            static char tNames[6][32] = { "main.cpp", "NkGuiWidgets.cpp", "NkImage.h",
                                          "icon.png", "data.bin", "README.md" };
            static const char* tTypes[6] = { "C++", "C++", "Header", "Image", "Binaire", "Texte" };
            static const int32 tSizes[6] = { 1240, 18650, 920, 4096, 65536, 312 };
            static const char* nameIds[6] = { "n0", "n1", "n2", "n3", "n4", "n5" };
            if (BeginTable(ctx, "files", 3,
                           NkGuiTableFlags::Borders | NkGuiTableFlags::RowBg | NkGuiTableFlags::Resizable
                           | NkGuiTableFlags::ResizableOuter | NkGuiTableFlags::Sortable)) {
                TableSetupColumn(ctx, "Nom", 0.f);     // 0 = colonne etirable
                TableSetupColumn(ctx, "Type", 80.f);
                TableSetupColumn(ctx, "Taille", 70.f);
                TableHeadersRow(ctx);

                // Tri : l'app reordonne ses donnees (ici un index order[] trie par insertion).
                static int32 order[6] = { 0, 1, 2, 3, 4, 5 };
                int32 scol = -1; bool sasc = true;
                if (TableGetSortColumn(ctx, &scol, &sasc)) {
                    auto cmp = [&](int32 a, int32 b) -> int32 {        // <0 si a avant b
                        if (scol == 2) return (tSizes[a] < tSizes[b]) ? -1 : (tSizes[a] > tSizes[b] ? 1 : 0);
                        const char* sa = (scol == 0) ? tNames[a] : tTypes[a];
                        const char* sb = (scol == 0) ? tNames[b] : tTypes[b];
                        int32 k = 0; while (sa[k] && sa[k] == sb[k]) ++k;
                        return static_cast<int32>(static_cast<uint8>(sa[k])) - static_cast<int32>(static_cast<uint8>(sb[k]));
                    };
                    for (int32 a = 1; a < 6; ++a) {                    // tri par insertion (stable, 0 stdlib)
                        const int32 key = order[a]; int32 j = a - 1;
                        while (j >= 0) {
                            const int32 c = cmp(order[j], key);
                            if (sasc ? c > 0 : c < 0) { order[j + 1] = order[j]; --j; } else break;
                        }
                        order[j + 1] = key;
                    }
                }

                for (int32 r = 0; r < 6; ++r) {
                    const int32 i = order[r];
                    TableNextRow(ctx);
                    // Cellule "Nom" EDITABLE (double-clic) ; les 2 autres en lecture.
                    TableNextColumn(ctx); TableCellText(ctx, nameIds[i], tNames[i], 32, /*editable=*/true);
                    TableNextColumn(ctx); Text(ctx, tTypes[i]);
                    TableNextColumn(ctx);
                    char sz[24]; std::snprintf(sz, sizeof(sz), "%d o", tSizes[i]);
                    Text(ctx, sz);
                }
                EndTable(ctx);
            }
            EndPanel(ctx);
        }

        // Boîte déplaçable : prouve l'état d'interaction MoveWindow (active drag).
        if (showDragBox) {
            const NkGuiId boxId = ctx.GetId("dragbox");
            const NkRect  boxR  = { boxPos.x, boxPos.y, 110.f, 80.f };
            // Soumise APRÈS les boutons → au-dessus. ItemHoverable résout le z-ordre :
            // au-dessus d'un bouton, c'est la boîte qui capture, pas le bouton dessous.
            const bool hovered = ctx.ItemHoverable(boxR, boxId);
            if (hovered && ctx.input.mouseClicked[0]) {
                ctx.activeId = boxId;
                dragOff = { ctx.input.mousePos.x - boxPos.x, ctx.input.mousePos.y - boxPos.y };
            }
            if (ctx.activeId == boxId) {
                ctx.interact = NkGuiInteract::MoveWindow;
                boxPos = { ctx.input.mousePos.x - dragOff.x, ctx.input.mousePos.y - dragOff.y };
                if (ctx.input.mouseReleased[0]) ctx.activeId = NKGUI_ID_NONE;
            }
            const NkColor col = (ctx.activeId == boxId) ? ctx.theme.buttonActive
                              : hovered                 ? ctx.theme.buttonHover
                                                        : ctx.theme.button;
            dl.AddRectFilled(boxR, col, 6.f);
            dl.AddRect(boxR, ctx.theme.accent, 2.f);
            dl.AddCircleFilled({ boxR.x + boxR.w * 0.5f, boxR.y + boxR.h * 0.5f }, 16.f, ctx.theme.accent);
        }

        // ListBox DEFILANTE (scroll molette + scrollbar draggable) — 15 elements.
        if (showListBox) {
            const NkRect lbR = { 920.f, 414.f, 175.f, 150.f };
            TextAt(ctx, { lbR.x, lbR.y - 22.f }, "ListBox (scroll) :");
            static int32 lbSel = -1;
            if (BeginListBox(ctx, "lbDemo", lbR)) {
                for (int32 i = 0; i < 15; ++i) {
                    char s[20]; std::snprintf(s, sizeof(s), "Element %02d", i + 1);
                    if (Selectable(ctx, s, lbSel == i)) lbSel = i;
                }
                EndListBox(ctx);
            }
        }

        // Éditeur de texte MULTI-LIGNE (Entrée = nouvelle ligne, flèches, scroll).
        if (showMulti) {
            const NkRect mlR = { 920.f, 578.f, 175.f, 165.f };
            TextAt(ctx, { mlR.x, mlR.y - 22.f }, "Multi-ligne :");
            static char mlBuf[512] =
                "Editeur multi-ligne.\nEntree = nouvelle ligne.\nFleches haut/bas "
                "pour naviguer.\nMolette / barre pour defiler.\nTapez du texte ici.";
            InputTextMultiline(ctx, "mlEdit", mlBuf, static_cast<int32>(sizeof(mlBuf)), mlR);
        }

        // Zone a SCROLL HORIZONTAL (contenu plus large que la vue) — sous le panneau.
        if (showHScroll) {
            ctx.hscrollMod = NkGuiKeyMod::Ctrl;   // CONFIGURABLE : ici Ctrl+molette (défaut framework = Maj)
            const NkRect hsR = { 40.f, H - 96.f, 440.f, 64.f };
            TextAt(ctx, { hsR.x, hsR.y - 22.f }, "Scroll horizontal (Ctrl+molette / barre basse) :");
            if (BeginChild(ctx, "hsBox", hsR, true, /*horizontal=*/true)) {
                for (int32 i = 0; i < 7; ++i) {
                    char b[18]; std::snprintf(b, sizeof(b), "Colonne %d", i + 1);
                    if (i > 0) ctx.SameLine();
                    Button(ctx, b);
                }
                EndChild(ctx);
            }
        }

        // Menu CONTEXTUEL : clic droit n'importe ou → ouvre a la position souris.
        if (ctx.input.mouseClicked[1])
            ctx.OpenPopupAt(ctx.GetId("ctxmenu"), ctx.input.mousePos);
        if (BeginPopupMenu(ctx, "ctxmenu")) {
            MenuItem(ctx, "Couper",  "Ctrl+X");
            MenuItem(ctx, "Copier",  "Ctrl+C");
            MenuItem(ctx, "Coller",  "Ctrl+V");
            if (BeginMenu(ctx, "Plus")) {                 // sous-menu du contextuel
                MenuItem(ctx, "Dupliquer");
                MenuItem(ctx, "Supprimer");
                EndMenu(ctx);
            }
            EndPopupMenu(ctx);
        }

        // ── DOCKING : un DockSpace + 2 fenetres ancrables (glisser la barre de titre
        //    dessus -> boussole ; glisser un onglet vers le bas -> desancre). ──
        static bool dockInit = false;
        if (!dockInit) {                                   // layout par defaut au boot
            DockBuilderDock(ctx, "Inspecteur", 0);         // onglet
            DockBuilderDock(ctx, "Proprietes", 2);         // split a droite
            dockInit = true;
        }
        // Dock RESPONSIVE : le rect suit la taille de la fenetre (W/H) → il s'agrandit
        // quand la fenetre OS grandit. (Pour remplir TOUT l'ecran : DockSpaceOverViewport.)
        ctx.dockTabAddButton     = true;   // active le bouton « + » sur les barres d'onglets
        ctx.windowDockingEnabled = true;   // #3 (opt-in) : fusionner des fenetres flottantes en hote
        DockSpace(ctx, "mainDock", { 150.f, 470.f, W - 290.f, H - 500.f });

        // Clic « + » → cree un nouvel onglet (l'app decide quoi creer).
        static char  extraStore[16][24];
        static int32 extraCount = 0;
        if (const int32 addNode = DockTabAddRequest(ctx); addNode >= 0 && extraCount < 16) {
            std::snprintf(extraStore[extraCount], 24, "Onglet %d", extraCount + 1);
            DockAddTab(ctx, extraStore[extraCount], addNode);
            ++extraCount;
        }
        for (int32 i = 0; i < extraCount; ++i)
            if (Begin(ctx, extraStore[i])) { Text(ctx, "Onglet cree via +."); EndWindow(ctx); }

        if (Begin(ctx, "Inspecteur")) {
            Text(ctx, "Glisser la barre = deplacer/ancrer.");
            Text(ctx, "Glisser l'onglet bas = desancrer.");
            static float32 wv = 0.5f;
            SliderFloat(ctx, "Valeur", wv, 0.f, 1.f);
            if (Button(ctx, "Action")) {}
            for (int32 i = 0; i < 8; ++i) {                // contenu long -> scroll
                char ln[32]; std::snprintf(ln, sizeof(ln), "Propriete %02d", i + 1);
                Text(ctx, ln);
            }
            EndWindow(ctx);
        }
        if (Begin(ctx, "Proprietes")) {
            Text(ctx, "Fenetre ancree (onglet/split).");
            static bool wc = true; Checkbox(ctx, "Visible", wc);
            static char wn[32] = "objet_01";
            InputText(ctx, "Nom", wn, 32);
            static float32 wcol[4] = { 0.4f, 0.8f, 0.5f, 1.f };
            ColorEdit4(ctx, "Couleur", wcol);
            EndWindow(ctx);
        }

        // #3 : fenetres FLOTTANTES — glisser l'une sur l'autre (boussole : onglet OU
        // split) → fusionnent en un hote de dock flottant ; on peut en ajouter d'autres.
        if (Begin(ctx, "Calque A")) {
            Text(ctx, "Glisse ma barre sur Calque B");
            Text(ctx, "-> fenetre a onglets.");
            EndWindow(ctx);
        }
        if (Begin(ctx, "Calque B")) {
            Text(ctx, "Hote de dock flottant.");
            static bool cb = false; Checkbox(ctx, "Option", cb);
            EndWindow(ctx);
        }
        if (Begin(ctx, "Calque C")) { Text(ctx, "3e fenetre."); EndWindow(ctx); }

        // ── CONTENEURS DE LAYOUT (Vague A) : HBox, Grid, accordeon, Stack ──
        if (Begin(ctx, "Conteneurs")) {
            Text(ctx, "Flex Row [60px | grow | grow x2] :");
            { const float32 sz[] = { 60.f, -1.f, -2.f };
              BeginRow(ctx, 0.f, sz, 3);
                Button(ctx, "60px"); Button(ctx, "grow"); Button(ctx, "grow x2");
              EndRow(ctx); }
            Separator(ctx);
            Text(ctx, "Flow (wrap auto) :");
            BeginFlow(ctx);
            for (int32 i = 0; i < 8; ++i) { char b[10]; std::snprintf(b, 10, "tag%d", i + 1); Button(ctx, b); }
            EndFlow(ctx);
            Separator(ctx);
            Text(ctx, "Stack (superpose + ancrage) :");
            BeginStack(ctx, 150.f, 56.f);
                StackAnchor(ctx, 4); Button(ctx, "Centre");
                StackAnchor(ctx, 8); Button(ctx, "BasDroite");
            EndStack(ctx);
            Separator(ctx);
            if (CollapsingHeader(ctx, "Section repliable")) {
                Text(ctx, "Contenu de la section.");
                BeginHBox(ctx); Button(ctx, "Oui"); Button(ctx, "Non"); EndHBox(ctx);
            }
            Separator(ctx);
            Text(ctx, "HBox (horizontal) :");
            BeginHBox(ctx);
                Button(ctx, "Un"); Button(ctx, "Deux"); Button(ctx, "Trois");
            EndHBox(ctx);
            Separator(ctx);
            Text(ctx, "Grid 3 colonnes :");
            BeginGrid(ctx, 3);
                for (int32 i = 0; i < 6; ++i) { char b[8]; std::snprintf(b, 8, "G%d", i + 1); Button(ctx, b); }
            EndGrid(ctx);
            EndWindow(ctx);
        }

        // ── INSPECTEUR generique pilote par la REFLEXION (dogfood NKEditorKit) ──
        static DemoEntity demoEntity;
        if (Begin(ctx, "Inspecteur (reflexion)")) {
            Text(ctx, "DemoEntity (proprietes auto via NKReflection) :");
            Separator(ctx);
            DrawInspector(ctx, &demoEntity, &DemoEntity::GetStaticClass());
            EndWindow(ctx);
        }

        // Curseur souhaité par NKGui → curseur OS (OPTIONNEL : l'app choisit d'appliquer).
        // Ex. DragFloat pose ↔ / ↕ pendant le survol/glisser. SetCursor est persistant.
        {
            NkWindow::NkCursorType c = NkWindow::NkCursorType::Arrow;
            switch (ctx.wantCursor) {
                case NkGuiCursor::Text:     c = NkWindow::NkCursorType::TextInput; break;
                case NkGuiCursor::Hand:     c = NkWindow::NkCursorType::Hand;      break;
                case NkGuiCursor::ResizeEW: c = NkWindow::NkCursorType::ResizeWE;  break;
                case NkGuiCursor::ResizeNS: c = NkWindow::NkCursorType::ResizeNS;  break;
                default: break;
            }
            window.SetCursor(c);
        }

        ctx.EndFrame();

        target->Clear();
        backend.Submit(dl, sz.x, sz.y);                 // couche principale
        backend.Submit(ctx.dlOverlay, sz.x, sz.y);      // couche popups/overlay (au-dessus)
        target->Display();        // Capture d'ecran (F12) : APRES Display() — la frame presentee. Self-capture
        // NKCanvas pixel-perfect (readback backbuffer -> NkImage::Save). Format selon
        // l'extension. API publique : target->Capture(path) (programmatique aussi).
        if (captureRequested) {
            captureRequested = false;
            static int32 shotN = 0;
            char path[64];
            std::snprintf(path, sizeof(path), "capture_%03d.png", shotN++);
            const bool ok = target->Capture(path);
            if (ok) logger.Info("Capture ecran -> {0}", path);
            else    logger.Error("Capture ecran ECHEC ({0})", path);
        }
    }

    SetCurrentContext(nullptr);
    ctx.Shutdown();
    return 0;
}
