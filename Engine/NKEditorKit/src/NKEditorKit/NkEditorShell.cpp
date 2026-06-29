// =============================================================================
// NkEditorShell.cpp — implementation de la coquille d'editeur (sur NKGui).
//   events -> BeginFrame -> menubar -> DockSpace -> panneaux -> palette -> rendu.
// =============================================================================
#include "NKEditorKit/NkEditorShell.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"   // backend de rendu par defaut (IDE)

#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKMemory/NkAllocator.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;

namespace nkentseu {
    namespace editorkit {

        namespace {
            void CopyStr(char* dst, const char* src, usize cap) noexcept {
                if (!dst || cap == 0) return;
                usize i = 0;
                if (src) { for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i]; }
                dst[i] = '\0';
            }

            bool StartsWith(const char* s, const char* pre) noexcept {
                for (; *pre; ++s, ++pre) if (*s != *pre) return false;
                return true;
            }
            bool StrEqual(const char* a, const char* b) noexcept {
                while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
                return *a == *b;
            }

            constexpr NkColor kPaletteBg     = { 30, 33, 42, 250 };
            constexpr NkColor kPaletteBorder = { 90, 150, 230, 255 };
            constexpr NkColor kPaletteSel    = { 64, 110, 200, 235 };
            constexpr NkColor kTextPrimary   = { 235, 236, 242, 255 };
            constexpr NkColor kTextSecondary = { 180, 185, 196, 255 };
            constexpr NkColor kTextTertiary  = { 130, 135, 148, 255 };
            constexpr NkColor kBackdrop      = { 0, 0, 0, 130 };

            NkWindow::NkCursorType MapCursor(NkGuiCursor c) noexcept {
                switch (c) {
                    case NkGuiCursor::Text:     return NkWindow::NkCursorType::TextInput;
                    case NkGuiCursor::Hand:     return NkWindow::NkCursorType::Hand;
                    case NkGuiCursor::ResizeEW: return NkWindow::NkCursorType::ResizeWE;
                    case NkGuiCursor::ResizeNS: return NkWindow::NkCursorType::ResizeNS;
                    default:                    return NkWindow::NkCursorType::Arrow;
                }
            }

            int32 SideToZone(NkEditorDockSide s) noexcept {
                switch (s) {
                    case NkEditorDockSide::NK_LEFT:   return 1;
                    case NkEditorDockSide::NK_RIGHT:  return 2;
                    case NkEditorDockSide::NK_TOP:    return 3;
                    case NkEditorDockSide::NK_BOTTOM: return 4;
                    case NkEditorDockSide::NK_CENTER:
                    default:                          return 0;
                }
            }
        } // namespace

        // ── NkEditorPanel : constructeur (defini dans la lib) ────────────────────
        NkEditorPanel::NkEditorPanel(const char* title, NkEditorDockSide defaultSide) noexcept {
            CopyStr(mTitle, title ? title : "Panel", sizeof(mTitle));
            mDefaultSide = defaultSide;
        }

        // ── Cycle de vie ─────────────────────────────────────────────────────────
        NkEditorShell::~NkEditorShell() {
            if (mRenderer) {                     // libere le contexte GPU avant la fenetre
                mRenderer->Shutdown();
                if (mOwnsRenderer) memory::NkGetDefaultAllocator().Delete(mRenderer);
                mRenderer = nullptr;
            }
            mWindow.Close();
        }

        bool NkEditorShell::Init(const NkEditorShellConfig& config) noexcept {
            mGraphicsApi = config.graphicsApi;

            NkWindowConfig wc;
            wc.title     = config.title;
            wc.width     = config.width;
            wc.height    = config.height;
            wc.minWidth  = 1024;             // taille MINI de l'IDE (sidebar + panneau lisibles)
            wc.minHeight = 640;
            wc.centered  = true;
            wc.resizable = config.resizable;
            wc.frame     = false;            // SANS bordure OS -> barre de titre custom (VSCode)
            if (!mWindow.Create(wc)) return false;
            CopyStr(mTitle, config.title, sizeof(mTitle));

            // Backend de rendu : injecte (app NKRHI/NKRenderer) ou NKCanvas par
            // defaut (IDE). Resolution AUTO->API faite par l'impl elle-meme.
            if (config.renderer) {
                mRenderer = config.renderer; mOwnsRenderer = false;
            } else {
                mRenderer = memory::NkGetDefaultAllocator().New<NkEditorCanvasRenderer>();
                mOwnsRenderer = true;
            }
            if (!mRenderer || !mRenderer->Init(mWindow, mGraphicsApi)) {
                if (mRenderer && mOwnsRenderer) memory::NkGetDefaultAllocator().Delete(mRenderer);
                mRenderer = nullptr;
                return false;
            }

            mUI.Init(static_cast<int32>(config.width), static_cast<int32>(config.height));

            // Theme GitHub Dark (palette fournie). #0D1117 fond, #191D23 surfaces,
            // #010409 chrome sombre, #1F6FEB accent, #DFDFDF texte, #519ABA secondaire.
            NkGuiTheme& t   = mUI.theme;
            t.bgPrimary     = {  13,  17,  23, 255 };   // editeur #0D1117
            t.panel         = {   1,   4,   9, 255 };   // sidebar #010409 (plus sombre)
            t.header        = {  25,  29,  35, 255 };   // titres/menus #191D23
            t.button        = {  25,  29,  35, 255 };
            t.buttonHover   = {  33,  39,  48, 255 };   // hover liste (#191D23 + un poil)
            t.buttonActive  = {  31, 111, 235, 255 };   // selection active #1F6FEB
            t.border        = {  33,  39,  48, 255 };   // bord subtil
            t.text          = { 223, 223, 223, 255 };   // #DFDFDF
            t.textDisabled  = { 125, 133, 144, 255 };   // muted
            t.selection     = {  31, 111, 235, 200 };   // #1F6FEB (semi)
            t.accent        = {  31, 111, 235, 255 };   // #1F6FEB
            t.track         = {  13,  17,  23, 255 };
            t.tabBar        = {  25,  29,  35, 255 };   // barre d'onglets #191D23
            t.tab           = {  25,  29,  35, 255 };   // onglet inactif #191D23
            t.tabHover      = {  33,  39,  48, 255 };
            t.tabActive     = {  13,  17,  23, 255 };   // onglet actif = fond editeur #0D1117
            t.rounding      = 0.f;                       // coins droits

            mDefaultTheme = mUI.theme;   // theme par defaut (vit dans l'app) -> 'Reinitialiser'
            NkLoadTheme(mUI.theme);      // applique le theme utilisateur sauvegarde s'il existe
            mDefaultSyntax = mUI.syntax; // couleurs langages par defaut
            NkLoadSyntax(mUI.syntax);    // applique les couleurs langages sauvegardees

            // Un nœud de dock à 1 seul panneau n'affiche PAS de barre d'onglets
            // (l'éditeur ne montre que ses onglets de fichiers ; pas de tab "Editeur").
            mUI.dockHideSingleTab = true;

            // Presse-papiers : relie le contexte NKGui a la fenetre OS (NKWindow).
            mUI.clipboardUser  = &mWindow;
            mUI.clipboardGetFn = [](void* u, NkString& out) { out = static_cast<NkWindow*>(u)->GetClipboardText(); };
            mUI.clipboardSetFn = [](void* u, const char* t) { static_cast<NkWindow*>(u)->SetClipboardText(NkString(t)); };

            // Hook barre d'onglets : le panneau ACTIF dessine ses actions a droite.
            mUI.dockHeaderUser = this;
            mUI.dockHeaderFn = [](NkGuiContext& c, const NkRect& bar, NkGuiId win, void* u) {
                auto* self = static_cast<NkEditorShell*>(u);
                for (int32 i = 0; i < self->mNumPanels; ++i)
                    if (c.GetId(self->mPanels[i]->Title()) == win) { self->mPanels[i]->OnTabBarActions(c, bar); break; }
            };


            // ── DEUX polices distinctes (comme VSCode), pilotees par les reglages ──
            // INTERFACE (proportionnelle, defaut Inter) + CODE/TERMINAL (monospace,
            // defaut DejaVu Sans Mono). Atlas SEPARES => texId distincts. Le choix est
            // lu depuis le fichier de config (modifiable par l'utilisateur a chaud).
            mCodeFont.texId = mFont.TexId() + 1u;   // atlas distinct (anti-collision backend)
            NkLoadFontPrefs(mFontPrefs);
            LoadFontsFromPrefs();

            mUI.windowDockingEnabled = true;   // fusion de fenetres flottantes (opt-in)

            mLastWidth  = config.width;
            mLastHeight = config.height;

            // Acces aux Preferences via la palette (Ctrl+P) — fiable quel que soit le menu.
            RegisterCommand("Preferences : Polices",  +[](void* u) { static_cast<NkEditorShell*>(u)->OpenPreferences(0); }, this);
            RegisterCommand("Preferences : Theme",    +[](void* u) { static_cast<NkEditorShell*>(u)->OpenPreferences(1); }, this);
            RegisterCommand("Preferences : Langages", +[](void* u) { static_cast<NkEditorShell*>(u)->OpenPreferences(2); }, this);

            HookEvents();
            return true;
        }

        void NkEditorShell::HookEvents() noexcept {
            auto& events = NkEvents();

            events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent*) {
                mRunning = false;
            });
            events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent* e) {
                mUI.input.mousePos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
            });
            events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent* e) {
                if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  mUI.input.mouseDown[0] = true;
                if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) mUI.input.mouseDown[1] = true;
                mUI.input.ctrlDown  = e->GetModifiers().ctrl;
                mUI.input.shiftDown = e->GetModifiers().shift;
                mUI.input.altDown   = e->GetModifiers().alt;
            });
            events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent* e) {
                if (e->GetButton() == NkMouseButton::NK_MB_LEFT)  mUI.input.mouseDown[0] = false;
                if (e->GetButton() == NkMouseButton::NK_MB_RIGHT) mUI.input.mouseDown[1] = false;
            });
            // Molette : scroll vertical + horizontal (consommee en EndFrame par NKGui).
            events.AddEventCallback<NkMouseWheelVerticalEvent>([this](NkMouseWheelVerticalEvent* e) {
                mUI.input.wheel += static_cast<float32>(e->GetDeltaY());
                const auto m = e->GetModifiers();
                mUI.input.ctrlDown = m.ctrl; mUI.input.shiftDown = m.shift; mUI.input.altDown = m.alt;
            });
            events.AddEventCallback<NkMouseWheelHorizontalEvent>([this](NkMouseWheelHorizontalEvent* e) {
                mUI.input.wheelH += static_cast<float32>(e->GetDeltaX());
            });
            // Double-clic OS (evenement dedie) : injecte pour la selection de mot.
            events.AddEventCallback<NkMouseDoubleClickEvent>([this](NkMouseDoubleClickEvent* e) {
                mUI.input.mousePos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
                if (e->GetButton() == NkMouseButton::NK_MB_LEFT) mUI.input.SetDoubleClick(0);
            });
            // Saisie texte (codepoints) -> file de caracteres NKGui.
            events.AddEventCallback<NkTextInputEvent>([this](NkTextInputEvent* e) {
                mUI.input.PushChar(e->GetCodepoint());
            });
            events.AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent* e) {
                const NkKey k = e->GetKey();
                MapEditKey(k, true);
                mUI.input.ctrlDown  = e->GetModifiers().ctrl;
                mUI.input.shiftDown = e->GetModifiers().shift;
                mUI.input.altDown   = e->GetModifiers().alt;
                if (e->GetModifiers().ctrl) {   // raccourcis copier/couper/coller/tout-selectionner
                    if      (k == NkKey::NK_C) mUI.input.wantCopy      = true;
                    else if (k == NkKey::NK_X) mUI.input.wantCut       = true;
                    else if (k == NkKey::NK_V) mUI.input.wantPaste     = true;
                    else if (k == NkKey::NK_A) mUI.input.wantSelectAll = true;
                }
                if (k == NkKey::NK_P && e->GetModifiers().ctrl) { mPaletteOpen = !mPaletteOpen; mPaletteSel = 0; return; }
                if (mPaletteOpen) {
                    if      (k == NkKey::NK_ESCAPE) mPaletteOpen = false;
                    else if (k == NkKey::NK_DOWN && mNumCommands > 0) mPaletteSel = (mPaletteSel + 1) % mNumCommands;
                    else if (k == NkKey::NK_UP   && mNumCommands > 0) mPaletteSel = (mPaletteSel - 1 + mNumCommands) % mNumCommands;
                    else if (k == NkKey::NK_ENTER) { ExecuteCommand(mPaletteSel); mPaletteOpen = false; }
                    return;
                }
                // Raccourcis Ctrl+<lettre> (ex. Ctrl+S, Ctrl+B) meme pendant la frappe.
                if (e->GetModifiers().ctrl) TryRunShortcut(k, e->GetModifiers().shift);
            });
            events.AddEventCallback<NkKeyReleaseEvent>([this](NkKeyReleaseEvent* e) {
                MapEditKey(e->GetKey(), false);
                mUI.input.ctrlDown  = e->GetModifiers().ctrl;
                mUI.input.shiftDown = e->GetModifiers().shift;
                mUI.input.altDown   = e->GetModifiers().alt;
            });
        }

        // Mappe une touche OS d'edition vers l'etat ENFONCE NKGui (press/release ->
        // repetition au maintien geree par NKGui). Sans ce pont, aucune navigation
        // clavier ni edition de texte dans les panneaux.
        void NkEditorShell::MapEditKey(NkKey k, bool down) noexcept {
            switch (k) {
                case NkKey::NK_LEFT:   mUI.input.SetKey(NkGuiKey::Left,      down); break;
                case NkKey::NK_RIGHT:  mUI.input.SetKey(NkGuiKey::Right,     down); break;
                case NkKey::NK_UP:     mUI.input.SetKey(NkGuiKey::Up,        down); break;
                case NkKey::NK_DOWN:   mUI.input.SetKey(NkGuiKey::Down,      down); break;
                case NkKey::NK_HOME:   mUI.input.SetKey(NkGuiKey::Home,      down); break;
                case NkKey::NK_END:    mUI.input.SetKey(NkGuiKey::End,       down); break;
                case NkKey::NK_BACK:   mUI.input.SetKey(NkGuiKey::Backspace, down); break;
                case NkKey::NK_DELETE: mUI.input.SetKey(NkGuiKey::Delete,    down); break;
                case NkKey::NK_ENTER:  mUI.input.SetKey(NkGuiKey::Enter,     down); break;
                case NkKey::NK_ESCAPE: mUI.input.SetKey(NkGuiKey::Escape,    down); break;
                default: break;
            }
        }

        // ── Enregistrement ───────────────────────────────────────────────────────
        bool NkEditorShell::AddPanel(NkEditorPanel* panel) noexcept {
            if (!panel || mNumPanels >= MAX_PANELS) return false;
            mPanels[mNumPanels++] = panel;
            return true;
        }

        bool NkEditorShell::RegisterCommand(const char* name, NkEditorCommandFn fn,
                                            void* user, const char* shortcut) noexcept {
            if (!name || !fn || mNumCommands >= MAX_COMMANDS) return false;
            NkEditorCommand& c = mCommands[mNumCommands++];
            CopyStr(c.name, name, sizeof(c.name));
            CopyStr(c.shortcut, shortcut ? shortcut : "", sizeof(c.shortcut));
            c.fn = fn; c.user = user;
            return true;
        }

        void NkEditorShell::ExecuteCommand(int32 index) noexcept {
            if (index < 0 || index >= mNumCommands) return;
            if (mCommands[index].fn) mCommands[index].fn(mCommands[index].user);
        }

        // ── Boucle principale ────────────────────────────────────────────────────
        // Callback OS appele pendant la boucle modale de resize/move (timer 0xB1A5) :
        // redessine UNE frame a la nouvelle taille -> supprime le "stretch" du framebuffer.
        void NkEditorShell::SizeMoveFrameThunk(void* user) noexcept {
            if (user) static_cast<NkEditorShell*>(user)->RenderFrame();
        }

        int NkEditorShell::Run() noexcept {
            NkEvents().SetSizeMoveFrameCallback(&SizeMoveFrameThunk, this);   // anti-stretch pendant le resize natif
            while (mRunning && mWindow.IsOpen()) {
                while (NkEvent* ev = NkEvents().PollEvent()) { (void)ev; }
                if (!mRunning) break;
                RenderFrame();
                // Hand-off NATIF differe : BeginResize/BeginDragMove BLOQUENT (boucle modale
                // OS) ; on les lance ICI (hors frame) pour eviter la re-entrance de RenderFrame.
                // Pendant la boucle modale, le callback ci-dessus rappelle RenderFrame -> rendu live.
                if (mPendingDragMove)             { mPendingDragMove = false;   mWindow.BeginDragMove(); }
                else if (mPendingResizeEdge >= 0) { const NkWindow::NkResizeEdge e = static_cast<NkWindow::NkResizeEdge>(mPendingResizeEdge); mPendingResizeEdge = -1; mWindow.BeginResize(e); }
            }
            return 0;
        }

        void NkEditorShell::RenderFrame() noexcept {
            float32 dt = mClock.Tick().delta;
            if (dt <= 0.f) dt = 1.f / 60.f;
            if (dt > 0.1f) dt = 0.1f;

            // Resize : suit la taille de la fenetre OS.
            const math::NkVec2u wsz = mWindow.GetSize();
            if (wsz.x > 0 && wsz.y > 0 && (wsz.x != mLastWidth || wsz.y != mLastHeight)) {
                mRenderer->OnResize(wsz.x, wsz.y);
                mLastWidth = wsz.x; mLastHeight = wsz.y;
            }
            const math::NkVec2u sz = mRenderer->Size();
            if (sz.x > 0 && sz.y > 0) { mUI.viewW = static_cast<int32>(sz.x); mUI.viewH = static_cast<int32>(sz.y); }

                mUI.BeginFrame(dt);

                const float32 W = static_cast<float32>(mUI.viewW);
                const float32 H = static_cast<float32>(mUI.viewH);
                mUI.dl.AddRectFilled({ 0.f, 0.f, W, H }, mUI.theme.bgPrimary);

                NkEditorFrameContext ec; ec.ui = &mUI; ec.dt = dt;

                // Ecran de demarrage (launcher) : occupe tout le corps, sans barre
                // d'outils / panneaux / barre d'etat (façon « page de demarrage » VS).
                const bool fullScreen = mUI.appFullScreen && mStartScreenFn;

                const float32 titleH    = mUI.ItemHeight() + mUI.S(10.f);   // barre de titre legerement plus grande
                mUI.titleBarH = titleH;   // l'ecran de demarrage doit commencer en dessous
                const float32 toolbarH  = (mToolbarFn && !fullScreen) ? mUI.ItemHeight() + mUI.S(8.f) : 0.f;
                const float32 footerH   = fullScreen ? 0.f : mUI.S(22.f);
                const float32 activityW = mUI.S(48.f);

                // Barre de titre custom UNE ligne : logo + menus | infos | min/max/close.
                DrawTitleBar(ec, { 0.f, 0.f, W, titleH });
                // Barre d'outils Visual Studio (config/plateforme cible + Build/Run + emulateur).
                if (mToolbarFn && !fullScreen) DrawToolbar(ec, { 0.f, titleH, W, toolbarH });

                const float32 bodyTop = titleH + toolbarH;
                const float32 bodyH   = H - bodyTop - footerH;

                // MODALE : quand Preferences est ouvert, le corps (panneaux/editeur)
                // ne doit pas reagir aux clics/molette/frappes destines au popup. On
                // masque l'input pour le corps puis on le restaure pour DrawPreferences.
                // IDEM quand la souris est au-dessus d'un menu DEROULANT ouvert (deja
                // dessine + gere dans la barre de titre ci-dessus) : sinon l'editeur /
                // les zones a hit-test « brut » derriere le menu recoivent les clics.
                bool overPopup = false;
                for (int32 i = 0; i < mUI.popupDepth; ++i)
                    if (nkgui::NkGuiRectContains(mUI.popupRects[i], mUI.input.mousePos)) { overPopup = true; break; }
                const bool modal = mShowPrefs || mUI.appModal || overPopup;
                nkgui::NkGuiInput savedInput;
                if (modal) {
                    savedInput = mUI.input;
                    mUI.input.mousePos = { -100000.f, -100000.f };
                    for (int32 i = 0; i < 3; ++i) { mUI.input.mouseClicked[i] = false; mUI.input.mouseDown[i] = false; mUI.input.mouseDoubleClicked[i] = false; }
                    mUI.input.wheel = mUI.input.wheelH = 0.f;
                    mUI.input.charCount = 0;
                    mUI.input.wantCopy = mUI.input.wantCut = mUI.input.wantPaste = mUI.input.wantSelectAll = false;
                }

                if (fullScreen) {
                    // Launcher : remplace barre d'activite + dock + panneaux.
                    mStartScreenFn(ec, mStartScreenUser);
                } else {
                    DrawActivityBar({ 0.f, bodyTop, activityW, bodyH });
                    DockSpace(mUI, "##EditorDock", { activityW, bodyTop, W - activityW, bodyH });
                    BootstrapDocking();
                    DrawPanels(ec);
                    DrawStatusBar(footerH);
                }
                HandleEdgeResize(W, H);                  // bords de redimensionnement (fenetre sans bordure)

                if (modal) mUI.input = savedInput;       // restaure pour le popup
                DrawCommandPalette(ec);
                DrawPreferences(ec);                     // fenetre Preferences (menu dedie)
                if (mOverlayFn) mOverlayFn(ec, mOverlayUser);   // dialogues modaux de l'app (creation/proprietes)

                // Bordure de NOTRE fenetre (l'OS n'en dessine plus) — sauf si maximisee.
                if (!mWindow.IsMaximized())
                    mUI.dlOverlay.AddRect({ 0.f, 0.f, W, H }, { 48, 54, 61, 255 }, 1.f);   // bord #30363d

                mUI.EndFrame();

                mWindow.SetCursor(MapCursor(mUI.wantCursor));

                mRenderer->BeginFrame();
                mRenderer->SubmitDrawList(mUI.dl,        sz.x, sz.y);
                mRenderer->SubmitDrawList(mUI.dlOverlay, sz.x, sz.y);
                mRenderer->EndFrame();
        }

        // ── Activity bar (bande verticale d'icones a gauche, facon VSCode) ────────
        void NkEditorShell::DrawActivityBar(const NkRect& bar) noexcept {
            auto& dl = mUI.dl;
            const NkColor barBg  = {   1,   4,   9, 255 };   // activity bar #010409
            const NkColor on     = { 255, 255, 255, 255 };
            const NkColor off    = { 133, 133, 133, 255 };
            const NkColor hov    = { 229, 229, 229, 255 };
            const NkColor accent = {   0, 122, 204, 255 };
            dl.AddRectFilled(bar, barBg);
            if (!mUI.font) {}
            const float32 cell = bar.w;                       // cellule carree = largeur barre
            const NkVec2  m    = mUI.input.mousePos;

            // Icones vectorielles (dessinees relativement au centre (cx,cy)).
            auto drawIcon = [&](int32 idx, float32 cy, bool bottom) {
                const NkRect r = { bar.x, cy - cell * 0.5f, cell, cell };
                const bool hovered = m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
                const bool active  = (mActivityIndex == idx) && !bottom;
                const NkColor c = active ? on : hovered ? hov : off;
                if (active) dl.AddRectFilled({ bar.x, r.y, mUI.S(2.f), cell }, accent);  // barre d'accent
                const float32 cx = bar.x + cell * 0.5f, ic = cy;
                const float32 s  = mUI.S(8.f);
                switch (idx) {
                    case 0: {  // Explorateur : un document + lignes de texte
                        dl.AddRect({ cx - s * 0.8f, ic - s, s * 1.6f, s * 2.f }, c, 1.5f);
                        for (int k = 0; k < 3; ++k) {
                            const float32 ly = ic - s * 0.4f + k * s * 0.6f;
                            dl.AddLine({ cx - s * 0.5f, ly }, { cx + s * 0.45f, ly }, c, 1.f);
                        }
                    } break;
                    case 1: {  // Recherche : anneau + manche
                        dl.AddCircleFilled({ cx - s * 0.25f, ic - s * 0.25f }, s * 0.7f, c);
                        dl.AddCircleFilled({ cx - s * 0.25f, ic - s * 0.25f }, s * 0.45f, barBg);
                        dl.AddLine({ cx + s * 0.3f, ic + s * 0.3f }, { cx + s, ic + s }, c, 2.f);
                    } break;
                    case 2: {  // Controle de source : branche (3 noeuds + liens)
                        const NkVec2 a = { cx - s * 0.6f, ic - s * 0.8f };
                        const NkVec2 b = { cx - s * 0.6f, ic + s * 0.8f };
                        const NkVec2 d = { cx + s * 0.7f, ic };
                        dl.AddLine(a, b, c, 1.5f); dl.AddLine({ cx - s * 0.6f, ic }, d, c, 1.5f);
                        dl.AddCircleFilled(a, s * 0.35f, c); dl.AddCircleFilled(b, s * 0.35f, c);
                        dl.AddCircleFilled(d, s * 0.35f, c);
                    } break;
                    case 3: {  // Executer/Deboguer : triangle play
                        dl.AddTriangleFilled({ cx - s * 0.6f, ic - s }, { cx - s * 0.6f, ic + s }, { cx + s, ic }, c);
                    } break;
                    case 4: {  // Extensions : 4 carres (un detache)
                        const float32 q = s * 0.6f;
                        dl.AddRectFilled({ cx - s, ic - s, q, q }, c);
                        dl.AddRectFilled({ cx - s + q + 2.f, ic - s, q, q }, c);
                        dl.AddRectFilled({ cx - s, ic - s + q + 2.f, q, q }, c);
                        dl.AddRect({ cx + 2.f, ic + 2.f, q, q }, c, 1.5f);
                    } break;
                    case 5: {  // Reglages : roue dentee (anneau + centre)
                        dl.AddCircleFilled({ cx, ic }, s * 0.9f, c);
                        dl.AddCircleFilled({ cx, ic }, s * 0.5f, barBg);
                        dl.AddCircleFilled({ cx, ic }, s * 0.2f, c);
                    } break;
                    default: break;
                }
                // Clic : icone 0 = bascule la sidebar (1er panneau cote LEFT).
                if (hovered && mUI.input.mouseClicked[0]) {
                    if (!bottom) mActivityIndex = idx;
                    if (idx == 0) {
                        for (int32 p = 0; p < mNumPanels; ++p)
                            if (mPanels[p]->DefaultSide() == NkEditorDockSide::NK_LEFT) { mPanels[p]->SetOpen(!mPanels[p]->IsOpen()); break; }
                    }
                }
            };

            const float32 top = bar.y + cell * 0.5f;
            for (int32 i = 0; i < 5; ++i) drawIcon(i, top + i * cell, false);
            drawIcon(5, bar.y + bar.h - cell * 0.5f, true);   // Reglages en bas
        }

        // ── Barre de titre custom (UNE ligne : logo + menus | infos | controles) ──
        // Layout facon VSCode : [logo][Fichier Affichage ...]   <infos centre>   [─ ☐ ✕]
        void NkEditorShell::DrawTitleBar(NkEditorFrameContext& ec, const NkRect& bar) noexcept {
            auto& dl = mUI.dl;
            const NkColor bg     = {   1,   4,   9, 255 };   // barre de titre #010409
            const NkColor fg     = { 204, 204, 204, 255 };
            const NkColor accent = {   0, 122, 204, 255 };
            dl.AddRectFilled(bar, bg);
            const NkVec2  m   = mUI.input.mousePos;
            const float32 pad = mUI.S(8.f);
            const float32 cy  = bar.y + bar.h * 0.5f;

            // Logo NKCode a l'extreme gauche. Wordmark complet (aspect>0) => icone+nom
            // deja dans l'image, on NE re-ecrit PAS "nkcode". Sinon icone carree (+ texte).
            float32 cursorX  = bar.x + pad;
            bool    wordmark = false;
            if (mTitleLogoTex) {
                const float32 lg = bar.h * 0.62f;
                if (mTitleLogoAspect > 0.f) {                       // logo complet (ratio preserve)
                    const float32 lw = lg * mTitleLogoAspect;
                    dl.AddImage(mTitleLogoTex, { cursorX, cy - lg * 0.5f, lw, lg }, { 0.f, 0.f }, { 1.f, 1.f }, { 255, 255, 255, 255 });
                    cursorX += lw + mUI.S(10.f);
                    wordmark = true;
                } else {                                            // icone carree
                    dl.AddImage(mTitleLogoTex, { cursorX, cy - lg * 0.5f, lg, lg }, { 0.f, 0.f }, { 1.f, 1.f }, { 255, 255, 255, 255 });
                    cursorX += lg + mUI.S(8.f);
                }
            } else {
                const float32 lg = mUI.S(12.f);
                dl.AddRectFilled({ cursorX, cy - lg * 0.5f, lg, lg }, accent);
                cursorX += lg + mUI.S(8.f);
            }
            // Sur le launcher : nom "nkcode" (seulement si pas deja dans le wordmark) + AUCUN menu.
            float32 menuX = cursorX;
            if (mUI.appFullScreen && !wordmark && mUI.font && mUI.font->Face()) {
                const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
                dl.AddText(mUI.font->Face(), mUI.font->TexId(), { cursorX, by }, "nkcode", fg);
                menuX = cursorX + mUI.font->MeasureWidth("nkcode") + mUI.S(12.f);
            }

            auto inR = [&](const NkRect& r){ return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            const NkColor hovBg = { 255, 255, 255, 26 };
            const float32 bw = mUI.S(42.f);
            const NkRect cClose = { bar.x + bar.w - bw,       bar.y, bw, bar.h };
            const NkRect cMax   = { bar.x + bar.w - bw * 2.f, bar.y, bw, bar.h };
            const NkRect cMin   = { bar.x + bar.w - bw * 3.f, bar.y, bw, bar.h };

            // Menus DANS la barre de titre (uniquement dans l'editeur, pas le launcher).
            if (!mUI.appFullScreen) BuildMenuBar(ec, { menuX, bar.y, cMin.x - menuX, bar.h });
            else mUI.menuBarX = menuX;   // pour les zones de drag

            // Infos specifiques au centre (ex. fichier actif) = "panneau du milieu".
            // On retient son rect [titleLx, titleRx] pour delimiter les zones de drag.
            float32 titleLx = bar.x + bar.w, titleRx = bar.x + bar.w;   // vide par defaut
            const char* info = mTitleCenter[0] ? mTitleCenter : mTitle;
            if (mUI.font && mUI.font->Face() && info[0] && !mUI.appFullScreen) {
                const float32 iw = mUI.font->MeasureWidth(info);
                const float32 ix = bar.x + (bar.w - iw) * 0.5f;
                titleLx = ix - mUI.S(10.f); titleRx = ix + iw + mUI.S(10.f);   // + petite marge
                const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
                dl.AddText(mUI.font->Face(), mUI.font->TexId(), { ix, by }, info, { 150, 150, 150, 255 });
            }

            bool consumed = false;
            // Chip arrondi inset (style design) pour le fond de survol des controles.
            auto chip = [&](const NkRect& r) -> NkRect { const float32 vy = mUI.S(5.f), hx = mUI.S(3.f); return { r.x + hx, r.y + vy, r.w - 2.f * hx, r.h - 2.f * vy }; };
            const float32 cround = mUI.S(4.f);
            // Minimiser (trait).
            { const bool h = inR(cMin); if (h) dl.AddRectFilled(chip(cMin), hovBg, cround);
              const float32 gx = cMin.x + cMin.w * 0.5f;
              dl.AddLine({ gx - mUI.S(5.f), cy }, { gx + mUI.S(5.f), cy }, fg, 1.f);
              if (h && mUI.input.mouseClicked[0]) { mWindow.Minimize(); consumed = true; } }
            // Maximiser / restaurer (carre, ou double carre si maximise).
            { const bool h = inR(cMax); if (h) dl.AddRectFilled(chip(cMax), hovBg, cround);
              const float32 gx = cMax.x + cMax.w * 0.5f, s = mUI.S(9.f);
              if (mWindow.IsMaximized()) {
                  dl.AddRect({ gx - s * 0.5f + 2.f, cy - s * 0.5f - 2.f, s - 2.f, s - 2.f }, fg, 1.f);
                  dl.AddRectFilled({ gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f }, bg);
                  dl.AddRect({ gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f }, fg, 1.f);
              } else {
                  dl.AddRect({ gx - s * 0.5f, cy - s * 0.5f, s, s }, fg, 1.f);
              }
              if (h && mUI.input.mouseClicked[0]) { mWindow.Maximize(); consumed = true; } }
            // Fermer (X, survol rouge #f85149 arrondi).
            { const bool h = inR(cClose); if (h) dl.AddRectFilled(chip(cClose), { 248, 81, 73, 255 }, cround);
              const NkColor xc = h ? NkColor{ 255, 255, 255, 255 } : fg;
              const float32 gx = cClose.x + cClose.w * 0.5f, s = mUI.S(5.f);
              dl.AddLine({ gx - s, cy - s }, { gx + s, cy + s }, xc, 1.2f);
              dl.AddLine({ gx - s, cy + s }, { gx + s, cy - s }, xc, 1.2f);
              if (h && mUI.input.mouseClicked[0]) { mRunning = false; consumed = true; } }

            // Glissement de fenetre facon VSCode : DEUX zones de deplacement, dans les
            // GAPS uniquement -> (1) apres le dernier menu et avant le panneau central,
            // (2) apres le panneau central et avant les controles de fenetre. Les menus
            // et le panneau central ne sont JAMAIS des zones de drag. En plus, le
            // deplacement ne demarre qu'apres un vrai GLISSEMENT (seuil) : un simple clic
            // ne deplace jamais la fenetre.
            const bool inBarY = (m.y >= bar.y && m.y < bar.y + bar.h);
            const float32 gap1L = mUI.menuBarX + 2.f, gap1R = titleLx;       // menus -> centre
            const float32 gap2L = titleRx,            gap2R = cMin.x;        // centre -> controles
            const bool inGap1 = inBarY && m.x >= gap1L && m.x < gap1R;
            const bool inGap2 = inBarY && m.x >= gap2L && m.x < gap2R;
            const bool dragArea = inGap1 || inGap2;
            if (!consumed && dragArea && mUI.input.mouseDoubleClicked[0]) {
                mWindow.Maximize(); mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false; mTitleDragArmed = false;
            } else if (!consumed && dragArea && mUI.input.mouseClicked[0]) {
                mTitleDragArmed = true; mDragStartX = m.x; mDragStartY = m.y;   // arme, sans deplacer encore
            }
            if (mTitleDragArmed) {
                if (!mUI.input.mouseDown[0]) {
                    mTitleDragArmed = false;                                    // relache sans bouger = simple clic
                } else {
                    const float32 dx = m.x - mDragStartX, dy = m.y - mDragStartY;
                    if (dx * dx + dy * dy > 25.f) {                            // > ~5 px = vrai glissement
                        mTitleDragArmed = false;
                        mPendingDragMove = true;   // hand-off natif en fin de boucle Run() (anti re-entrance)
                        mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false;
                    }
                }
            }
        }

        // ── Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) ─
        void NkEditorShell::DrawToolbar(NkEditorFrameContext& ec, const NkRect& rect) noexcept {
            auto& dl = mUI.dl;
            dl.AddRectFilled(rect, { 22, 26, 32, 255 });                                       // gris sombre #161A20
            dl.AddRectFilled({ rect.x, rect.y + rect.h - 1.f, rect.w, 1.f }, { 40, 45, 53, 255 });  // separateur
            if (!mToolbarFn) return;
            // Region de layout horizontale : l'app pose Button/Combo + SameLine().
            const float32 itemH = mUI.ItemHeight();
            mUI.layout.region     = rect;
            mUI.layout.cursor     = { rect.x + mUI.S(8.f), rect.y + (rect.h - itemH) * 0.5f };
            mUI.layout.lineStartX = mUI.layout.cursor.x;
            mUI.layout.curLineH   = 0.f;
            mUI.layout.maxX       = mUI.layout.cursor.x;
            mToolbarFn(ec, mToolbarUser);
        }

        // ── Bords de redimensionnement (fenetre sans bordure) ─────────────────────
        // Redimensionnement MANUEL par les bords. Le resize natif (WM_NCLBUTTONDOWN/
        // HTLEFT) ne fonctionne pas dans ce setup borderless ; on gere donc tout a la
        // main via GetPosition/GetSize + SetPosition/SetSize, en suivant la souris ECRAN.
        void NkEditorShell::HandleEdgeResize(float32 W, float32 H) noexcept {
            if (mWindow.IsMaximized()) return;
            const float32 b = mUI.S(7.f);
            const NkVec2  m = mUI.input.mousePos;                 // coords CLIENT (NCHITTEST=HTCLIENT)
            const bool L = m.x <= b, R = m.x >= W - b, T = m.y <= b, Bm = m.y >= H - b;
            const int32 edge = (L ? 1 : 0) | (R ? 2 : 0) | (T ? 4 : 0) | (Bm ? 8 : 0);
            if (!edge) return;

            // Curseur de redimensionnement au survol d'un bord (pas de diagonale dispo).
            if ((edge & 3) && !(edge & 12))      mUI.wantCursor = NkGuiCursor::ResizeEW;   // gauche/droite
            else if ((edge & 12) && !(edge & 3)) mUI.wantCursor = NkGuiCursor::ResizeNS;   // haut/bas
            else                                  mUI.wantCursor = NkGuiCursor::ResizeEW;   // coin

            // Clic sur un bord -> HAND-OFF NATIF a l'OS (resize fluide + aero-snap), sans
            // contournement : chaque backend implemente NkWindow::BeginResize nativement.
            if (mUI.input.mouseClicked[0]) {
                NkWindow::NkResizeEdge e = NkWindow::NkResizeEdge::Left;
                switch (edge) {
                    case 1:  e = NkWindow::NkResizeEdge::Left;        break;
                    case 2:  e = NkWindow::NkResizeEdge::Right;       break;
                    case 4:  e = NkWindow::NkResizeEdge::Top;         break;
                    case 8:  e = NkWindow::NkResizeEdge::Bottom;      break;
                    case 1 | 4:  e = NkWindow::NkResizeEdge::TopLeft;     break;
                    case 2 | 4:  e = NkWindow::NkResizeEdge::TopRight;    break;
                    case 1 | 8:  e = NkWindow::NkResizeEdge::BottomLeft; break;
                    case 2 | 8:  e = NkWindow::NkResizeEdge::BottomRight;break;
                    default: return;
                }
                mUI.input.mouseClicked[0] = false;            // consomme (pas de drag de barre de titre)
                mTitleDragArmed = false;                      // annule un eventuel arme de drag titre
                mPendingResizeEdge = static_cast<int32>(e);   // declenche le hand-off natif en fin de boucle Run()
            }
        }

        // ── Barre d'etat (footer facon VSCode : bande bleue en bas) ───────────────
        void NkEditorShell::DrawStatusBar(float32 footerH) noexcept {
            const float32 W = static_cast<float32>(mUI.viewW);
            const float32 H = static_cast<float32>(mUI.viewH);
            const NkRect  bar = { 0.f, H - footerH, W, footerH };
            mUI.dl.AddRectFilled(bar, NkColor{ 1, 4, 9, 255 });                       // status bar #010409
            mUI.dl.AddRectFilled({ 0.f, bar.y, W, 1.f }, NkColor{ 33, 39, 48, 255 }); // liseret haut
            if (!mUI.font || !mUI.font->Face()) return;
            const NkColor fg = { 223, 223, 223, 255 };                               // texte #DFDFDF
            const float32 pad = mUI.S(10.f);
            const float32 by  = bar.y + (footerH - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
            if (mFooterLeft[0])
                mUI.dl.AddText(mUI.font->Face(), mUI.font->TexId(), { bar.x + pad, by }, mFooterLeft, fg);
            if (mFooterRight[0]) {
                const float32 rw = mUI.font->MeasureWidth(mFooterRight);
                mUI.dl.AddText(mUI.font->Face(), mUI.font->TexId(), { bar.x + W - pad - rw, by }, mFooterRight, fg);
            }
        }

        // Construit la combinaison "Ctrl+[Shift+]X" depuis la touche et l'execute
        // si un raccourci enregistre correspond. NkKeyToString -> "NK_X".
        void NkEditorShell::TryRunShortcut(NkKey k, bool shift) noexcept {
            const char* kn = NkKeyToString(k);
            if (!kn || kn[0] != 'N' || kn[1] != 'K' || kn[2] != '_' || !kn[3] || kn[4]) return;  // exige "NK_X"
            char combo[24]; usize n = 0;
            for (const char* p = "Ctrl+"; *p; ++p) combo[n++] = *p;
            if (shift) for (const char* p = "Shift+"; *p; ++p) combo[n++] = *p;
            combo[n++] = kn[3]; combo[n] = '\0';
            for (int32 i = 0; i < mNumCommands; ++i) {
                const char* s = mCommands[i].shortcut;
                usize j = 0; bool eq = true;
                for (; s[j] && combo[j]; ++j) if (s[j] != combo[j]) { eq = false; break; }
                if (eq && !s[j] && !combo[j]) { ExecuteCommand(i); return; }
            }
        }

        void NkEditorShell::SetFooter(const char* left, const char* right) noexcept {
            CopyStr(mFooterLeft,  left  ? left  : "", sizeof(mFooterLeft));
            CopyStr(mFooterRight, right ? right : "", sizeof(mFooterRight));
        }

        void NkEditorShell::SetTitleInfo(const char* center) noexcept {
            CopyStr(mTitleCenter, center ? center : "", sizeof(mTitleCenter));
        }

        // ── Barre de menus ───────────────────────────────────────────────────────
        void NkEditorShell::BuildMenuBar(NkEditorFrameContext& ec, const NkRect& rect) noexcept {
            if (!BeginMenuBar(mUI, rect)) return;

            // Sur l'ecran de demarrage (launcher), PAS de menus (Fichier, etc.) : on
            // appelle quand meme mAppMenuFn (il pose les flags appFullScreen/appModal
            // chaque frame -> indispensable au maintien du launcher).
            if (mUI.appFullScreen) { if (mAppMenuFn) mAppMenuFn(ec, mAppMenuUser); EndMenuBar(mUI); return; }

            if (BeginMenu(mUI, "Fichier")) {
                if (mFileMenuFn) mFileMenuFn(ec, mFileMenuUser);   // Nouveau/Enregistrer/Deploiement (app)
                if (MenuItem(mUI, "Palette de commandes", "Ctrl+P")) { mPaletteOpen = !mPaletteOpen; mPaletteSel = 0; }
                if (MenuItem(mUI, "Quitter", "Alt+F4")) mRunning = false;
                EndMenu(mUI);
            }
            if (BeginMenu(mUI, "Affichage")) {
                for (int32 i = 0; i < mNumPanels; ++i)
                    if (MenuItem(mUI, mPanels[i]->Title())) mPanels[i]->SetOpen(!mPanels[i]->IsOpen());
                EndMenu(mUI);
            }
            if (BeginMenu(mUI, "Fenetre")) {
                if (MenuItem(mUI, "Reinitialiser la disposition")) ResetLayout();
                EndMenu(mUI);
            }
            // Menu dedie aux reglages (extensible : polices, theme, et plus a venir).
            if (BeginMenu(mUI, "Preferences")) {
                if (MenuItem(mUI, "Polices..."))  OpenPreferences(0);
                if (MenuItem(mUI, "Theme..."))    OpenPreferences(1);
                if (MenuItem(mUI, "Langages...")) OpenPreferences(2);
                EndMenu(mUI);
            }
            if (mAppMenuFn) mAppMenuFn(ec, mAppMenuUser);

            EndMenuBar(mUI);
        }

        // ── Etat d'interface par projet (maximise + panneaux ouverts) ────────────
        void NkEditorShell::LoadUiState(const char* path) noexcept {
            if (!path || !*path) return;
            const NkString txt = NkFile::ReadAllText(NkPath(path));
            if (txt.Empty()) return;                 // pas de config -> on garde l'etat courant
            // Parse ligne par ligne : "maximized=0/1" et "panel=<titre>".
            bool sawPanel = false;
            // 1re passe : si des lignes panel= existent, on ferme tout avant d'ouvrir.
            const char* p = txt.CStr();
            for (const char* q = p; *q; ++q) if (q[0]=='p'&&q[1]=='a'&&q[2]=='n'&&q[3]=='e'&&q[4]=='l') { sawPanel = true; break; }
            if (sawPanel) for (int32 i = 0; i < mNumPanels; ++i) mPanels[i]->SetOpen(false);

            NkString line;
            auto apply = [&](const NkString& ln) {
                const char* s = ln.CStr();
                if (StartsWith(s, "maximized=")) { if (s[10] == '1') mWindow.Maximize(); else mWindow.Restore(); }
                else if (StartsWith(s, "panel=")) {
                    const char* name = s + 6;
                    for (int32 i = 0; i < mNumPanels; ++i)
                        if (StrEqual(mPanels[i]->Title(), name)) { mPanels[i]->SetOpen(true); break; }
                }
            };
            for (const char* c = p; ; ++c) {
                if (*c == '\n' || *c == '\r' || *c == '\0') { if (!line.Empty()) { apply(line); line.Clear(); } if (*c == '\0') break; }
                else line += *c;
            }
        }

        void NkEditorShell::SaveUiState(const char* path) noexcept {
            if (!path || !*path) return;
            NkPath p(path);
            NkDirectory::CreateRecursive(p.GetParent());   // cree <ws>/.nkcode/ si besoin
            NkString out;
            out += "maximized="; out += mWindow.IsMaximized() ? "1" : "0"; out += "\n";
            for (int32 i = 0; i < mNumPanels; ++i)
                if (mPanels[i]->IsOpen()) { out += "panel="; out += mPanels[i]->Title(); out += "\n"; }
            NkFile::WriteAllText(p, out);
        }

        // ── Panneaux (le docking est gere DANS Begin) ────────────────────────────
        void NkEditorShell::DrawPanels(NkEditorFrameContext& ec) noexcept {
            const float32 menuH = mUI.ItemHeight();
            for (int32 i = 0; i < mNumPanels; ++i) {
                NkEditorPanel* p = mPanels[i];
                if (!p->IsOpen()) continue;
                if (mDockBootstrap && !p->Dockable()) {        // 1er placement des flottants
                    SetNextWindowPos(mUI, 60.f + i * 28.f, menuH + 40.f + i * 28.f);
                    SetNextWindowSize(mUI, 360.f, 280.f);
                }
                if (Begin(mUI, p->Title(), p->OpenPtr())) {
                    p->OnUI(ec);
                    EndWindow(mUI);
                }
            }
        }

        // ── Bootstrap docking : disposition par defaut (centre puis cotes) ───────
        void NkEditorShell::BootstrapDocking() noexcept {
            if (!mDockBootstrap) return;
            // Centre d'abord, puis les côtés. Les panneaux d'un MÊME côté sont
            // regroupés en ONGLETS (ex. Terminal + Sortie partagent la barre du bas).
            for (int32 pass = 0; pass < 2; ++pass) {
                const bool  centerPass = (pass == 0);
                const char* sideFirst[8] = {};   // 1er panneau ancré par zone (les suivants = onglets)
                for (int32 i = 0; i < mNumPanels; ++i) {
                    NkEditorPanel* p = mPanels[i];
                    if (!p->IsOpen() || !p->Dockable()) continue;
                    const bool isCenter = (p->DefaultSide() == NkEditorDockSide::NK_CENTER);
                    if (isCenter != centerPass) continue;
                    const int32 zone = SideToZone(p->DefaultSide());
                    if (zone >= 0 && zone < 8 && sideFirst[zone])
                        DockBuilderDockTab(mUI, p->Title(), sideFirst[zone]);
                    else {
                        DockBuilderDock(mUI, p->Title(), zone);
                        if (zone >= 0 && zone < 8) sideFirst[zone] = p->Title();
                    }
                }
            }
            mDockBootstrap = false;
        }

        // ── Palette de commandes (overlay, Ctrl+P) ───────────────────────────────
        void NkEditorShell::DrawCommandPalette(NkEditorFrameContext&) noexcept {
            if (!mPaletteOpen || !mFontOk) return;
            const float32 W = static_cast<float32>(mUI.viewW);
            const float32 H = static_cast<float32>(mUI.viewH);

            const float32 pw = 480.f, rowH = mUI.ItemHeight() + 4.f, headH = mUI.ItemHeight() + 12.f;
            const int32   count = mNumCommands;
            const float32 ph = headH + 6.f + count * rowH + 8.f;
            const float32 px = (W - pw) * 0.5f, py = H * 0.16f;
            const float32 asc = mFont.Ascent(), lh = mFont.LineHeight();
            auto& dl = mUI.dlOverlay;

            dl.AddRectFilled({ 0.f, 0.f, W, H }, kBackdrop);
            dl.AddRectFilled({ px, py, pw, ph }, kPaletteBg, 8.f);
            dl.AddRect({ px, py, pw, ph }, kPaletteBorder, 1.5f);
            dl.AddText(mFont.Face(), mFont.TexId(), { px + 14.f, py + 9.f + asc }, "Palette de commandes", kTextPrimary);

            const float32 listTop = py + headH + 2.f;
            for (int32 i = 0; i < count; ++i) {
                const NkRect r = { px + 6.f, listTop + i * rowH, pw - 12.f, rowH - 2.f };
                const bool hov = NkGuiRectContains(r, mUI.input.mousePos);
                if (hov) mPaletteSel = i;
                const bool sel = (i == mPaletteSel);
                if (sel) dl.AddRectFilled(r, kPaletteSel, 5.f);

                const float32 ty = r.y + (r.h - lh) * 0.5f + asc;
                dl.AddText(mFont.Face(), mFont.TexId(), { r.x + 12.f, ty }, mCommands[i].name, sel ? kTextPrimary : kTextSecondary);
                if (mCommands[i].shortcut[0]) {
                    const float32 sw = mFont.MeasureWidth(mCommands[i].shortcut);
                    dl.AddText(mFont.Face(), mFont.TexId(), { r.x + r.w - 12.f - sw, ty }, mCommands[i].shortcut, kTextTertiary);
                }
                if (hov && mUI.input.mouseClicked[0]) { ExecuteCommand(i); mPaletteOpen = false; }
            }
        }

        // ── Polices : (re)charge mFont (interface) + mCodeFont (code) depuis les
        //    reglages, avec replis surs, puis re-upload des atlas au backend. ──
        void NkEditorShell::LoadFontsFromPrefs() noexcept {
            // Police chargee a uiSize x DPI : le LAYOUT est mis a l'echelle (ctx.S) mais
            // la police etait a une taille ABSOLUE -> texte trop petit sur ecran scale.
            // On la met a la meme echelle pour des proportions correctes (lisible).
            const float32 dpi = mUI.S(1.f) > 0.5f ? mUI.S(1.f) : 1.f;
            const float32 uiPx = mFontPrefs.uiSize * dpi, codePx = mFontPrefs.codeSize * dpi;
            mFontOk = NkResolveFont(mFont, mFontPrefs.uiFont, uiPx);
            if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Inter, uiPx);
            if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::Karla, 16.f * dpi);
            if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 13.f * dpi);
            mUI.font = &mFont;
            if (mFontOk && mRenderer) mRenderer->UploadFontGray8(mFont.TexId(), mFont.pixels, mFont.atlasW, mFont.atlasH);

            mCodeFont.texId = mFont.TexId() + 1u;   // atlas distinct (anti-collision backend)
            bool codeOk = NkResolveFont(mCodeFont, mFontPrefs.codeFont, codePx);
            if (!codeOk) codeOk = mCodeFont.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, codePx);
            if (!codeOk) codeOk = mCodeFont.LoadEmbedded(NkEmbeddedFontId::Cousine, 15.f * dpi);
            if (codeOk) { if (mRenderer) mRenderer->UploadFontGray8(mCodeFont.TexId(), mCodeFont.pixels, mCodeFont.atlasW, mCodeFont.atlasH); mUI.codeFont = &mCodeFont; }
            else mUI.codeFont = &mFont;
        }

        // ── Fenetre Preferences (overlay, menu dedie) : categories a gauche
        //    (Polices, Theme, ... extensible), contenu a droite. ──
        void NkEditorShell::DrawPreferences(NkEditorFrameContext&) noexcept {
            if (!mShowPrefs || !mFontOk) return;
            const float32 W = static_cast<float32>(mUI.viewW), H = static_cast<float32>(mUI.viewH);
            const float32 asc = mFont.Ascent(), lh = mFont.LineHeight();
            auto& dl = mUI.dlOverlay;
            const NkVec2 mp = mUI.input.mousePos;
            const bool   click = mUI.input.mouseClicked[0];
            auto hit = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };

            const float32 pw = 620.f, ph = 505.f, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
            dl.AddRectFilled({ 0.f, 0.f, W, H }, kBackdrop);
            // Clic hors fenetre -> ferme (sauf la frame d'ouverture : le clic du menu
            // est lui-meme hors du popup centre, il fermerait aussitot).
            if (mPrefsJustOpened) mPrefsJustOpened = false;
            else if (click && !NkGuiRectContains({ px, py, pw, ph }, mp)) { mShowPrefs = false; }
            dl.AddRectFilled({ px, py, pw, ph }, kPaletteBg, 8.f);
            dl.AddRect({ px, py, pw, ph }, kPaletteBorder, 1.5f);
            dl.AddText(mFont.Face(), mFont.TexId(), { px + 18.f, py + 14.f + asc }, "Preferences", kTextPrimary);

            auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(mFont.Face(), mFont.TexId(), { x, y + asc }, s, c); };
            auto btn = [&](const NkRect& r, const char* s, bool enabled) -> bool {
                const bool hov = enabled && hit(r);
                dl.AddRectFilled(r, hov ? kPaletteSel : NkColor{ 40, 46, 54, 255 }, 5.f);
                dl.AddRect(r, NkColor{ 60, 66, 74, 255 }, 1.f);
                const float32 tw = mFont.MeasureWidth(s);
                dl.AddText(mFont.Face(), mFont.TexId(), { r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc }, s, enabled ? kTextPrimary : kTextTertiary);
                return hov && click;
            };
            auto cycle = [&](NkString& name, const char* const* list, int32 n, int32 dir) {
                int32 idx = 0; for (int32 i = 0; i < n; ++i) if (name == list[i]) { idx = i; break; }
                idx = (idx + dir + n) % n; name = list[idx];
            };

            // ── Sidebar des categories ──
            const float32 sideW = 150.f, cy0 = py + 54.f, catH = 34.f;
            dl.AddRectFilled({ px + 1.f, py + 44.f, sideW, ph - 45.f }, NkColor{ 1, 4, 9, 255 });
            const char* cats[] = { "Polices", "Theme", "Langages" };
            for (int32 i = 0; i < 3; ++i) {
                const NkRect r = { px + 6.f, cy0 + i * catH, sideW - 12.f, catH - 4.f };
                const bool active = (mPrefsTab == i);
                if (active) dl.AddRectFilled(r, kPaletteSel, 5.f);
                else if (hit(r)) dl.AddRectFilled(r, NkColor{ 33, 39, 48, 255 }, 5.f);
                text(r.x + 12.f, r.y + (r.h - lh) * 0.5f, cats[i], active ? kTextPrimary : kTextSecondary);
                if (hit(r) && click) mPrefsTab = i;
            }

            const float32 cx = px + sideW + 24.f;   // colonne contenu
            float32 y = py + 60.f;

            if (mPrefsTab == 0) {
                // ── Categorie Polices ──
                int32 nUi = 0, nCode = 0;
                const char* const* uiList   = NkUiFontNames(&nUi);
                const char* const* codeList = NkCodeFontNames(&nCode);
                auto fontRow = [&](const char* lab, NkString& name, const char* const* list, int32 n, float32& sz) {
                    text(cx, y, lab, kTextSecondary); y += 24.f;
                    if (btn({ cx, y, 28.f, 26.f }, "<", true)) cycle(name, list, n, -1);
                    const NkRect nameBox = { cx + 32.f, y, 160.f, 26.f };
                    dl.AddRectFilled(nameBox, NkColor{ 22, 27, 34, 255 }, 4.f); dl.AddRect(nameBox, NkColor{ 48, 54, 61, 255 }, 1.f);
                    text(nameBox.x + 10.f, nameBox.y + (26.f - lh) * 0.5f, name.CStr(), kTextPrimary);
                    if (btn({ cx + 196.f, y, 28.f, 26.f }, ">", true)) cycle(name, list, n, 1);
                    // Taille
                    text(cx + 246.f, y + (26.f - lh) * 0.5f, "Taille", kTextSecondary);
                    if (btn({ cx + 310.f, y, 26.f, 26.f }, "-", true)) { sz -= 1.f; if (sz < 8.f) sz = 8.f; }
                    char sb[8]; std::snprintf(sb, sizeof(sb), "%d", static_cast<int>(sz + 0.5f));
                    const NkRect szBox = { cx + 338.f, y, 36.f, 26.f };
                    dl.AddRectFilled(szBox, NkColor{ 22, 27, 34, 255 }, 4.f);
                    const float32 sw = mFont.MeasureWidth(sb);
                    text(szBox.x + (szBox.w - sw) * 0.5f, szBox.y + (26.f - lh) * 0.5f, sb, kTextPrimary);
                    if (btn({ cx + 376.f, y, 26.f, 26.f }, "+", true)) { sz += 1.f; if (sz > 40.f) sz = 40.f; }
                    y += 44.f;
                };
                fontRow("Police de l'interface", mFontPrefs.uiFont, uiList, nUi, mFontPrefs.uiSize);
                fontRow("Police du code et du terminal", mFontPrefs.codeFont, codeList, nCode, mFontPrefs.codeSize);
                y += 6.f;
                text(cx, y, "Astuce : Consolas / Segoe UI / Courier New = polices systeme", kTextTertiary); y += 18.f;
                text(cx, y, "(chargees depuis Windows si presentes).", kTextTertiary); y += 28.f;
                if (btn({ cx, y, 120.f, 30.f }, "Appliquer", true)) { NkSaveFontPrefs(mFontPrefs); LoadFontsFromPrefs(); }
                if (btn({ cx + 132.f, y, 150.f, 30.f }, "Reinitialiser", true)) {
                    mFontPrefs = NkFontPrefs{};                 // defaut Inter + DejaVu
                    NkSaveFontPrefs(mFontPrefs); LoadFontsFromPrefs();
                }
            } else if (mPrefsTab == 1) {
                // ── Categorie Theme : couleur de l'interface, NOMMEE par element ──
                struct TE { const char* n; NkColor* c; };
                TE ents[] = {
                    { "Fond principal (editeur)",   &mUI.theme.bgPrimary },
                    { "Panneaux",                   &mUI.theme.panel },
                    { "En-tete / barre de menus",   &mUI.theme.header },
                    { "Bordures",                   &mUI.theme.border },
                    { "Texte",                      &mUI.theme.text },
                    { "Texte desactive",            &mUI.theme.textDisabled },
                    { "Accent (focus, barres)",     &mUI.theme.accent },
                    { "Element selectionne",        &mUI.theme.selection },
                    { "Bouton",                     &mUI.theme.button },
                    { "Bouton (survol)",            &mUI.theme.buttonHover },
                    { "Barre d'onglets",            &mUI.theme.tabBar },
                    { "Onglet actif",               &mUI.theme.tabActive },
                    { "Onglet inactif",             &mUI.theme.tab },
                    { "Onglet (survol)",            &mUI.theme.tabHover },
                    { "Pistes de defilement",       &mUI.theme.track },
                    { "Bouton (actif)",             &mUI.theme.buttonActive },
                };
                const int32 ne = 16;
                if (mThemeSel < 0 || mThemeSel >= ne) mThemeSel = 6;
                text(cx, y, "Theme de l'interface - couleur par element :", kTextSecondary); y += 24.f;

                // Liste 2 colonnes : pastille + nom, cliquable (selection).
                const float32 colW = 196.f, rowH = 22.f; const float32 gridY = y;
                for (int32 i = 0; i < ne; ++i) {
                    const int32 col = i % 2, row = i / 2;
                    const NkRect rr = { cx + col * colW, gridY + row * rowH, colW - 8.f, rowH - 2.f };
                    const bool sel = (mThemeSel == i);
                    if (sel) dl.AddRectFilled(rr, kPaletteSel, 4.f);
                    else if (hit(rr)) dl.AddRectFilled(rr, NkColor{ 33, 39, 48, 255 }, 4.f);
                    const NkRect sw = { rr.x + 4.f, rr.y + 3.f, 15.f, 15.f };
                    dl.AddRectFilled(sw, *ents[i].c, 3.f); dl.AddRect(sw, NkColor{ 60, 66, 74, 255 }, 1.f);
                    text(sw.x + 22.f, rr.y + (rowH - 2.f - lh) * 0.5f, ents[i].n, sel ? kTextPrimary : kTextSecondary);
                    if (hit(rr) && click) mThemeSel = i;
                }
                y = gridY + ((ne + 1) / 2) * rowH + 14.f;

                // Palette : applique une couleur a l'element selectionne.
                text(cx, y, ents[mThemeSel].n, kTextPrimary); y += 22.f;
                static const NkColor pal[] = {
                    { 13, 17, 23, 255 }, { 22, 27, 34, 255 }, { 33, 38, 45, 255 }, { 48, 54, 61, 255 },
                    { 110, 118, 129, 255 }, { 201, 209, 217, 255 }, { 240, 246, 252, 255 }, { 255, 255, 255, 255 },
                    { 31, 111, 235, 255 }, { 56, 139, 253, 255 }, { 163, 113, 247, 255 }, { 188, 140, 255, 255 },
                    { 63, 185, 80, 255 }, { 86, 211, 100, 255 }, { 219, 109, 40, 255 }, { 248, 81, 73, 255 },
                };
                for (int32 j = 0; j < 16; ++j) {
                    const NkRect r = { cx + (j % 8) * 30.f, y + (j / 8) * 30.f, 26.f, 26.f };
                    dl.AddRectFilled(r, pal[j], 5.f); dl.AddRect(r, NkColor{ 60, 66, 74, 255 }, 1.f);
                    if (hit(r) && click) { const uint8 a = ents[mThemeSel].c->a; *ents[mThemeSel].c = pal[j]; ents[mThemeSel].c->a = a; }
                }
                y += 70.f;
                // Enregistrer / Charger (fichier) + Reinitialiser (theme par defaut de l'app).
                if (btn({ cx, y, 130.f, 30.f }, "Enregistrer", true)) NkSaveTheme(mUI.theme);
                if (btn({ cx + 142.f, y, 110.f, 30.f }, "Charger", true)) NkLoadTheme(mUI.theme);
                if (btn({ cx + 264.f, y, 150.f, 30.f }, "Reinitialiser", true)) mUI.theme = mDefaultTheme;
                y += 38.f;
                text(cx, y, "Enregistre/charge le theme dans ~/.nkcode_theme.cfg.", kTextTertiary);
            } else {
                // ── Categorie Langages : couleurs de coloration syntaxique ──
                struct SE { const char* n; NkColor* c; };
                SE ents[] = {
                    { "Texte normal",        &mUI.syntax.text },
                    { "Mot-cle",             &mUI.syntax.keyword },
                    { "Type",                &mUI.syntax.type },
                    { "Chaine de caracteres",&mUI.syntax.string },
                    { "Commentaire",         &mUI.syntax.comment },
                    { "Nombre",              &mUI.syntax.number },
                    { "Preprocesseur / macro",&mUI.syntax.preproc },
                    { "Titre (Markdown)",    &mUI.syntax.heading },
                    { "Code (Markdown)",     &mUI.syntax.mdcode },
                };
                const int32 ne = 9;
                if (mSynSel < 0 || mSynSel >= ne) mSynSel = 1;
                text(cx, y, "Coloration syntaxique (C/C++, Python, NKSL, Markdown) :", kTextSecondary); y += 24.f;
                const float32 rowH = 25.f, gridY = y;
                // Liste : chaque token affiche DANS sa propre couleur (apercu direct).
                for (int32 i = 0; i < ne; ++i) {
                    const NkRect rr = { cx, gridY + i * rowH, 264.f, rowH - 3.f };
                    const bool sel = (mSynSel == i);
                    if (sel) dl.AddRectFilled(rr, kPaletteSel, 4.f);
                    else if (hit(rr)) dl.AddRectFilled(rr, NkColor{ 33, 39, 48, 255 }, 4.f);
                    const NkRect sw = { rr.x + 4.f, rr.y + 3.f, 15.f, 15.f };
                    dl.AddRectFilled(sw, *ents[i].c, 3.f); dl.AddRect(sw, NkColor{ 60, 66, 74, 255 }, 1.f);
                    text(sw.x + 24.f, rr.y + (rowH - 3.f - lh) * 0.5f, ents[i].n, *ents[i].c);
                    if (hit(rr) && click) mSynSel = i;
                }
                // Palette a droite : applique au token selectionne.
                const float32 palX = cx + 286.f;
                text(palX, gridY - 24.f + 4.f, ents[mSynSel].n, kTextPrimary);
                static const NkColor sp[] = {
                    { 212, 212, 212, 255 }, { 86, 156, 214, 255 }, { 78, 201, 176, 255 }, { 206, 145, 120, 255 },
                    { 106, 153, 85, 255 },  { 181, 206, 168, 255 }, { 197, 134, 192, 255 }, { 220, 220, 170, 255 },
                    { 156, 220, 254, 255 }, { 244, 71, 71, 255 },  { 215, 186, 125, 255 }, { 96, 139, 78, 255 },
                    { 255, 255, 255, 255 }, { 110, 118, 129, 255 }, { 86, 211, 100, 255 }, { 255, 123, 114, 255 },
                };
                for (int32 j = 0; j < 16; ++j) {
                    const NkRect r = { palX + (j % 4) * 30.f, gridY + (j / 4) * 30.f, 26.f, 26.f };
                    dl.AddRectFilled(r, sp[j], 5.f); dl.AddRect(r, NkColor{ 60, 66, 74, 255 }, 1.f);
                    if (hit(r) && click) { const uint8 a = ents[mSynSel].c->a; *ents[mSynSel].c = sp[j]; ents[mSynSel].c->a = a; }
                }
                float32 by = gridY + ne * rowH + 12.f;
                if (btn({ cx, by, 130.f, 30.f }, "Enregistrer", true)) NkSaveSyntax(mUI.syntax);
                if (btn({ cx + 142.f, by, 110.f, 30.f }, "Charger", true)) NkLoadSyntax(mUI.syntax);
                if (btn({ cx + 264.f, by, 150.f, 30.f }, "Reinitialiser", true)) mUI.syntax = mDefaultSyntax;
            }

            // ── Bouton Fermer (bas-droite) ──
            if (btn({ px + pw - 110.f, py + ph - 40.f, 96.f, 30.f }, "Fermer", true)) mShowPrefs = false;
        }

    } // namespace editorkit
} // namespace nkentseu
