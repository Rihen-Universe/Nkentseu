// =============================================================================
// NkEditorShell.cpp — implementation de la coquille d'editeur (sur NKGui).
//   events -> BeginFrame -> menubar -> DockSpace -> panneaux -> palette -> rendu.
// =============================================================================
#include "NKEditorKit/NkEditorShell.h"

#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"

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
            mRenderTarget.Reset();   // libere le contexte GPU avant la fenetre
            mWindow.Close();
        }

        bool NkEditorShell::Init(const NkEditorShellConfig& config) noexcept {
            mGraphicsApi = config.graphicsApi;

            NkWindowConfig wc;
            wc.title     = config.title;
            wc.width     = config.width;
            wc.height    = config.height;
            wc.centered  = true;
            wc.resizable = config.resizable;
            wc.frame     = false;            // SANS bordure OS -> barre de titre custom (VSCode)
            if (!mWindow.Create(wc)) return false;
            CopyStr(mTitle, config.title, sizeof(mTitle));

            NkContextDesc desc;
            desc.api = mGraphicsApi;
            if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
                desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
            }
            mRenderTarget = memory::NkMakeUnique<NkRenderWindow>(mWindow, desc);
            if (!mRenderTarget || !mRenderTarget->IsValid()) {
                mRenderTarget.Reset();
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

            mBackend.Init(mRenderTarget->GetRenderer());

            // DejaVu Sans Mono = MONOSPACE + couverture Unicode LARGE (accents, latin
            // etendu, grec, cyrillique, box-drawing, fleches...) -> ideal editeur/terminal.
            // Repli DroidSans (+ box-drawing dessine en primitives) puis ProggyClean.
            mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::DejaVuSansMono, 15.f);
            if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::DroidSans, 16.f);
            if (!mFontOk) mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 13.f);
            mUI.font = &mFont;
            if (mFontOk) mBackend.UploadFontGray8(mFont.TexId(), mFont.pixels, mFont.atlasW, mFont.atlasH);

            mUI.windowDockingEnabled = true;   // fusion de fenetres flottantes (opt-in)

            mLastWidth  = config.width;
            mLastHeight = config.height;

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
        int NkEditorShell::Run() noexcept {
            while (mRunning && mWindow.IsOpen()) {
                float32 dt = mClock.Tick().delta;
                if (dt <= 0.f) dt = 1.f / 60.f;
                if (dt > 0.1f) dt = 0.1f;

                while (NkEvent* ev = NkEvents().PollEvent()) { (void)ev; }
                if (!mRunning) break;

                // Resize : suit la taille de la fenetre OS.
                const math::NkVec2u wsz = mRenderTarget->GetWindow().GetSize();
                if (wsz.x > 0 && wsz.y > 0 && (wsz.x != mLastWidth || wsz.y != mLastHeight)) {
                    mRenderTarget->OnResize(wsz.x, wsz.y);
                    mLastWidth = wsz.x; mLastHeight = wsz.y;
                }
                const math::NkVec2u sz = mRenderTarget->GetSize();
                if (sz.x > 0 && sz.y > 0) { mUI.viewW = static_cast<int32>(sz.x); mUI.viewH = static_cast<int32>(sz.y); }

                mUI.BeginFrame(dt);

                const float32 W = static_cast<float32>(mUI.viewW);
                const float32 H = static_cast<float32>(mUI.viewH);
                mUI.dl.AddRectFilled({ 0.f, 0.f, W, H }, mUI.theme.bgPrimary);

                NkEditorFrameContext ec; ec.ui = &mUI; ec.dt = dt;

                const float32 titleH    = mUI.ItemHeight();
                const float32 toolbarH  = mToolbarFn ? mUI.ItemHeight() + mUI.S(8.f) : 0.f;
                const float32 footerH   = mUI.S(22.f);
                const float32 activityW = mUI.S(48.f);

                // Barre de titre custom UNE ligne : logo + menus | infos | min/max/close.
                DrawTitleBar(ec, { 0.f, 0.f, W, titleH });
                // Barre d'outils Visual Studio (config/plateforme cible + Build/Run + emulateur).
                if (mToolbarFn) DrawToolbar(ec, { 0.f, titleH, W, toolbarH });

                const float32 bodyTop = titleH + toolbarH;
                const float32 bodyH   = H - bodyTop - footerH;
                DrawActivityBar({ 0.f, bodyTop, activityW, bodyH });
                DockSpace(mUI, "##EditorDock", { activityW, bodyTop, W - activityW, bodyH });
                BootstrapDocking();
                DrawPanels(ec);
                DrawStatusBar(footerH);
                HandleEdgeResize(W, H);                  // bords de redimensionnement (fenetre sans bordure)
                DrawCommandPalette(ec);

                // Bordure de NOTRE fenetre (l'OS n'en dessine plus) — sauf si maximisee.
                if (!mWindow.IsMaximized())
                    mUI.dlOverlay.AddRect({ 0.f, 0.f, W, H }, { 48, 54, 61, 255 }, 1.f);   // bord #30363d

                mUI.EndFrame();

                mWindow.SetCursor(MapCursor(mUI.wantCursor));

                mRenderTarget->Clear();
                mBackend.Submit(mUI.dl,        sz.x, sz.y);
                mBackend.Submit(mUI.dlOverlay, sz.x, sz.y);
                mRenderTarget->Display();
            }
            return 0;
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

            // Logo (carre accent) a l'extreme gauche.
            const float32 lg = mUI.S(12.f);
            dl.AddRectFilled({ bar.x + pad, cy - lg * 0.5f, lg, lg }, accent);
            const float32 menuX = bar.x + pad + lg + mUI.S(8.f);

            auto inR = [&](const NkRect& r){ return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            const NkColor hovBg = { 255, 255, 255, 26 };
            const float32 bw = mUI.S(46.f);
            const NkRect cClose = { bar.x + bar.w - bw,       bar.y, bw, bar.h };
            const NkRect cMax   = { bar.x + bar.w - bw * 2.f, bar.y, bw, bar.h };
            const NkRect cMin   = { bar.x + bar.w - bw * 3.f, bar.y, bw, bar.h };

            // Menus a la suite du logo, DANS la barre de titre.
            BuildMenuBar(ec, { menuX, bar.y, cMin.x - menuX, bar.h });

            // Infos specifiques au centre (ex. fichier actif), si elles tiennent.
            const char* info = mTitleCenter[0] ? mTitleCenter : mTitle;
            if (mUI.font && mUI.font->Face() && info[0]) {
                const float32 iw = mUI.font->MeasureWidth(info);
                const float32 ix = bar.x + (bar.w - iw) * 0.5f;
                const float32 by = bar.y + (bar.h - mUI.font->LineHeight()) * 0.5f + mUI.font->Ascent();
                dl.AddText(mUI.font->Face(), mUI.font->TexId(), { ix, by }, info, { 150, 150, 150, 255 });
            }

            bool consumed = false;
            // Minimiser (trait).
            { const bool h = inR(cMin); if (h) dl.AddRectFilled(cMin, hovBg);
              const float32 gx = cMin.x + cMin.w * 0.5f;
              dl.AddLine({ gx - mUI.S(5.f), cy }, { gx + mUI.S(5.f), cy }, fg, 1.f);
              if (h && mUI.input.mouseClicked[0]) { mWindow.Minimize(); consumed = true; } }
            // Maximiser / restaurer (carre, ou double carre si maximise).
            { const bool h = inR(cMax); if (h) dl.AddRectFilled(cMax, hovBg);
              const float32 gx = cMax.x + cMax.w * 0.5f, s = mUI.S(9.f);
              if (mWindow.IsMaximized()) {
                  dl.AddRect({ gx - s * 0.5f + 2.f, cy - s * 0.5f - 2.f, s - 2.f, s - 2.f }, fg, 1.f);
                  dl.AddRectFilled({ gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f }, bg);
                  dl.AddRect({ gx - s * 0.5f - 2.f, cy - s * 0.5f + 2.f, s - 2.f, s - 2.f }, fg, 1.f);
              } else {
                  dl.AddRect({ gx - s * 0.5f, cy - s * 0.5f, s, s }, fg, 1.f);
              }
              if (h && mUI.input.mouseClicked[0]) { mWindow.Maximize(); consumed = true; } }
            // Fermer (X, survol rouge).
            { const bool h = inR(cClose); if (h) dl.AddRectFilled(cClose, { 232, 17, 35, 255 });
              const NkColor xc = h ? NkColor{ 255, 255, 255, 255 } : fg;
              const float32 gx = cClose.x + cClose.w * 0.5f, s = mUI.S(5.f);
              dl.AddLine({ gx - s, cy - s }, { gx + s, cy + s }, xc, 1.2f);
              dl.AddLine({ gx - s, cy + s }, { gx + s, cy - s }, xc, 1.2f);
              if (h && mUI.input.mouseClicked[0]) { mRunning = false; consumed = true; } }

            // Glissement : barre moins les controles ; EXCLUT les menus (hotId != NONE
            // quand un menu est survole) pour ne pas deplacer la fenetre en cliquant un menu.
            const NkRect drag = { bar.x, bar.y, cMin.x - bar.x, bar.h };
            const bool inDrag    = m.x >= drag.x && m.x < drag.x + drag.w && m.y >= drag.y && m.y < drag.y + drag.h;
            const bool overMenu  = (mUI.hotId != NKGUI_ID_NONE);
            if (!consumed && inDrag && !overMenu) {
                if (mUI.input.mouseDoubleClicked[0]) { mWindow.Maximize(); mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false; }
                else if (mUI.input.mouseClicked[0])  { mWindow.BeginDragMove(); mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false; }
            }
        }

        // ── Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) ─
        void NkEditorShell::DrawToolbar(NkEditorFrameContext& ec, const NkRect& rect) noexcept {
            auto& dl = mUI.dl;
            dl.AddRectFilled(rect, { 45, 45, 45, 255 });                                      // #2d2d2d
            dl.AddRectFilled({ rect.x, rect.y + rect.h - 1.f, rect.w, 1.f }, { 60, 60, 60, 255 });  // separateur
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
        void NkEditorShell::HandleEdgeResize(float32 W, float32 H) noexcept {
            if (mWindow.IsMaximized()) return;
            if (!mUI.input.mouseClicked[0]) return;
            const float32 b = mUI.S(5.f);
            const NkVec2  m = mUI.input.mousePos;
            const bool L = m.x <= b, R = m.x >= W - b, T = m.y <= b, Bm = m.y >= H - b;
            if (!(L || R || T || Bm)) return;
            using E = NkWindow::NkResizeEdge;
            E e;
            if      (T && L) e = E::TopLeft;     else if (T && R) e = E::TopRight;
            else if (Bm && L) e = E::BottomLeft; else if (Bm && R) e = E::BottomRight;
            else if (L) e = E::Left;             else if (R) e = E::Right;
            else if (T) e = E::Top;              else e = E::Bottom;
            mWindow.BeginResize(e);
            mUI.input.mouseDown[0] = mUI.input.mouseClicked[0] = false;
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

            if (BeginMenu(mUI, "Fichier")) {
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
            if (mAppMenuFn) mAppMenuFn(ec, mAppMenuUser);

            EndMenuBar(mUI);
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

    } // namespace editorkit
} // namespace nkentseu
