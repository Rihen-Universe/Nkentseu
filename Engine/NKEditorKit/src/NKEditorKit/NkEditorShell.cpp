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
            if (!mWindow.Create(wc)) return false;

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
            mBackend.Init(mRenderTarget->GetRenderer());

            mFontOk = mFont.LoadEmbedded(NkEmbeddedFontId::DroidSans, 16.f);
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
                if (e->GetButton() == NkMouseButton::NK_MB_LEFT) mUI.input.mouseDown[0] = true;
            });
            events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent* e) {
                if (e->GetButton() == NkMouseButton::NK_MB_LEFT) mUI.input.mouseDown[0] = false;
            });
            events.AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent* e) {
                const NkKey k = e->GetKey();
                if (k == NkKey::NK_P && e->GetModifiers().ctrl) { mPaletteOpen = !mPaletteOpen; mPaletteSel = 0; return; }
                if (mPaletteOpen) {
                    if      (k == NkKey::NK_ESCAPE) mPaletteOpen = false;
                    else if (k == NkKey::NK_DOWN && mNumCommands > 0) mPaletteSel = (mPaletteSel + 1) % mNumCommands;
                    else if (k == NkKey::NK_UP   && mNumCommands > 0) mPaletteSel = (mPaletteSel - 1 + mNumCommands) % mNumCommands;
                    else if (k == NkKey::NK_ENTER) { ExecuteCommand(mPaletteSel); mPaletteOpen = false; }
                }
            });
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

                BuildMenuBar(ec);

                const float32 menuH = mUI.ItemHeight();
                DockSpace(mUI, "##EditorDock", { 0.f, menuH, W, H - menuH });
                BootstrapDocking();
                DrawPanels(ec);
                DrawCommandPalette(ec);

                mUI.EndFrame();

                mWindow.SetCursor(MapCursor(mUI.wantCursor));

                mRenderTarget->Clear();
                mBackend.Submit(mUI.dl,        sz.x, sz.y);
                mBackend.Submit(mUI.dlOverlay, sz.x, sz.y);
                mRenderTarget->Display();
            }
            return 0;
        }

        // ── Barre de menus ───────────────────────────────────────────────────────
        void NkEditorShell::BuildMenuBar(NkEditorFrameContext& ec) noexcept {
            const float32 W = static_cast<float32>(mUI.viewW);
            if (!BeginMenuBar(mUI, { 0.f, 0.f, W, mUI.ItemHeight() })) return;

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
            for (int32 pass = 0; pass < 2; ++pass) {
                const bool centerPass = (pass == 0);
                for (int32 i = 0; i < mNumPanels; ++i) {
                    NkEditorPanel* p = mPanels[i];
                    if (!p->IsOpen() || !p->Dockable()) continue;
                    const bool isCenter = (p->DefaultSide() == NkEditorDockSide::NK_CENTER);
                    if (isCenter != centerPass) continue;
                    DockBuilderDock(mUI, p->Title(), SideToZone(p->DefaultSide()));
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
