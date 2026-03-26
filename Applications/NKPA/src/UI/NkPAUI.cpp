/**
 * @File    NkPAUI.cpp
 * @Brief   Implémentation du pont NKUI ↔ NKPA (pimpl).
 *
 *  Ce fichier est le SEUL à inclure les headers NkUI*.
 *  NkPAUI.h n'inclut aucun header NkUI → pas de conflit NkKey.
 */

#include "UI/NkPAUI.h"

// ── Headers NKUI (isolés ici via pimpl) ──────────────────────────────────────
#include "NkUI/NkUIContext.h"
#include "NkUI/NkUILayout.h"
#include "NkUI/NkUIFont.h"
#include "NkUI/NkUIWidgets.h"
#include "NkUI/NkUIWindow.h"
#include "NkUI/NkUITheme.h"
#include "NkUI/NkUIInput.h"

#include <cstring>

namespace nkentseu {
    namespace nkpa {

        // ─── Impl ─────────────────────────────────────────────────────────────────────

        struct NkPAUI::Impl {
            nkui::NkUIContext        ctx;
            nkui::NkUIFontManager    fonts;
            nkui::NkUIWindowManager  wm;
            nkui::NkUILayoutStack    ls;
            nkui::NkUIInputState     input;
            int32              w = 1280;
            int32              h = 720;
        };

        // ─── Constructeur / Destructeur ───────────────────────────────────────────────

        NkPAUI::NkPAUI()  { mImpl = new Impl(); }
        NkPAUI::~NkPAUI() { delete mImpl; mImpl = nullptr; }

        // ─── Init ─────────────────────────────────────────────────────────────────────

        void NkPAUI::Init(int32 w, int32 h) {
            mImpl->w = w;
            mImpl->h = h;
            mImpl->ctx.Init(w, h);
            mImpl->ctx.SetTheme(nkui::NkUITheme::Dark());
            mImpl->fonts.Init();
            mImpl->wm.Init();
            ::memset(&mImpl->input, 0, sizeof(mImpl->input));
        }

        void NkPAUI::OnResize(int32 w, int32 h) {
            mImpl->w = w;
            mImpl->h = h;
            mImpl->ctx.Init(w, h);
        }

        // ─── Input ────────────────────────────────────────────────────────────────────

        void NkPAUI::BeginInput() {
            mImpl->input.BeginFrame();
        }

        void NkPAUI::SetMousePos(float32 x, float32 y) {
            mImpl->input.SetMousePos(x, y);
        }

        void NkPAUI::SetMouseButton(int32 btn, bool down) {
            mImpl->input.SetMouseButton(btn, down);
        }

        void NkPAUI::AddMouseWheel(float32 dy) {
            mImpl->input.AddMouseWheel(dy);
        }

        // ─── BuildFrame ───────────────────────────────────────────────────────────────

        void NkPAUI::BuildFrame(float32 dt, NkPAUIState& state) {
            auto& im = *mImpl;

            im.ctx.BeginFrame(im.input, dt);
            im.wm.BeginFrame(im.ctx);

            nkui::NkUIFont* font = im.fonts.Default();
            if (!font) {
                im.ctx.EndFrame();
                im.wm.EndFrame(im.ctx);
                return;
            }

            // ── Panneau principal ─────────────────────────────────────────────────────
            bool open = state.showUI;
            nkui::NkUIWindow::SetNextWindowPos({ (float32)im.w - 210.f, 10.f });
            nkui::NkUIWindow::SetNextWindowSize({ 195.f, 230.f });
            if (nkui::NkUIWindow::Begin(im.ctx, im.wm, *im.ctx.dl, *font, ls, "NKPA Controls", &open, nkui::NkUIWindowFlags::NK_NO_RESIZE | nkui::NkUIWindowFlags::NK_NO_COLLAPSE)) {

                // Pause / Resume
                const char* lbl = state.paused ? "Resume" : "Pause";
                if (nkui::NkUI::Button(im.ctx, im.ls, *im.ctx.dl, *font, lbl))
                    state.paused = !state.paused;

                nkui::NkUI::Separator(im.ctx, im.ls, *im.ctx.dl);

                // Vitesse globale
                nkui::NkUI::SliderFloat(im.ctx, im.ls, *im.ctx.dl, *font, "Speed",
                                state.speedScale, 0.1f, 3.0f);

                nkui::NkUI::Separator(im.ctx, im.ls, *im.ctx.dl);

                // Compteurs créatures
                nkui::NkUI::SliderInt(im.ctx, im.ls, *im.ctx.dl, *font, "Fish",
                                state.fishCount, 1, 6);
                nkui::NkUI::SliderInt(im.ctx, im.ls, *im.ctx.dl, *font, "Sharks",
                                state.sharkCount, 0, 3);

                nkui::NkUI::Separator(im.ctx, im.ls, *im.ctx.dl);

                // Afficher / masquer UI
                nkui::NkUI::Checkbox(im.ctx, im.ls, *im.ctx.dl, *font, "Show UI", state.showUI);
            }
            nkui::NkUIWindow::End(im.ctx, im.wm, *im.ctx.dl);

            state.showUI = open;

            im.ctx.EndFrame();
            im.wm.EndFrame(im.ctx);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  ConvertDrawList — NkUIVertex → PaVertex (géométrie seule, pas de textures)
        // ─────────────────────────────────────────────────────────────────────────────

        static void ConvertDrawList(const nkui::NkUIDrawList& dl, MeshBuilder& mb) {
            for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                const nkui::NkUIDrawCmd& cmd = dl.cmds[ci];
                if (cmd.type != nkui::NkUIDrawCmdType::NK_TRIANGLES) continue;
                for (uint32 ii = 0; ii + 2 < cmd.idxCount; ii += 3) {
                    uint32 i0 = dl.idx[cmd.idxOffset + ii + 0];
                    uint32 i1 = dl.idx[cmd.idxOffset + ii + 1];
                    uint32 i2 = dl.idx[cmd.idxOffset + ii + 2];
                    const nkui::NkUIVertex& v0 = dl.vtx[i0];
                    const nkui::NkUIVertex& v1 = dl.vtx[i1];
                    const nkui::NkUIVertex& v2 = dl.vtx[i2];

                    // Unpack couleur RGBA depuis uint32 (format ABGR : r en byte 0)
                    auto unpack = [](uint32 c, float32& r, float32& g, float32& b, float32& a) {
                        r = (float32)( c        & 0xFF) / 255.f;
                        g = (float32)((c >>  8) & 0xFF) / 255.f;
                        b = (float32)((c >> 16) & 0xFF) / 255.f;
                        a = (float32)((c >> 24) & 0xFF) / 255.f;
                    };

                    float32 r0,g0,b0,a0, r1,g1,b1,a1, r2,g2,b2,a2;
                    unpack(v0.col, r0,g0,b0,a0);
                    unpack(v1.col, r1,g1,b1,a1);
                    unpack(v2.col, r2,g2,b2,a2);

                    mb.AddTriangle(
                        { v0.pos.x, v0.pos.y }, r0,g0,b0,a0,
                        { v1.pos.x, v1.pos.y }, r1,g1,b1,a1,
                        { v2.pos.x, v2.pos.y }, r2,g2,b2,a2);
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RenderToMesh
        // ─────────────────────────────────────────────────────────────────────────────

        void NkPAUI::RenderToMesh(MeshBuilder& mb) const {
            for (int32 layer = 0; layer < nkui::NkUIContext::LAYER_COUNT; ++layer) {
                ConvertDrawList(mImpl->ctx.layers[layer], mb);
            }
        }

    } // namespace nkpa
} // namespace nkentseu
