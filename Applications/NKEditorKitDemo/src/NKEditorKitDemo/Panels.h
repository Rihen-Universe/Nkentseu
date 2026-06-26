#pragma once
// =============================================================================
// Panels.h — Panneaux de demonstration pour NKEditorKit.
// Montre comment une application derive NkEditorPanel et dessine son contenu
// via les helpers de NkEditorFrameContext (ec.Text, ec.Button, ec.Checkbox...).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"

namespace nkedemo {

    using nkentseu::float32;
    using nkentseu::int32;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;   // NkRect / NkColor / NkVec2 / draw list

    // ── Explorateur : arborescence de fichiers fictive ───────────────────────
    class ExplorerPanel : public NkEditorPanel {
    public:
        ExplorerPanel() : NkEditorPanel("Explorateur", NkEditorDockSide::NK_LEFT) {}
        void OnUI(NkEditorFrameContext& ec) override {
            ec.Text("Projet : MonJeu");
            ec.Separator();
            static const char* files[] = {
                "src/Main.cpp", "src/Player.cpp", "src/Player.h",
                "assets/hero.png", "assets/level1.json", "MonJeu.jenga",
            };
            for (int32 i = 0; i < 6; ++i) {
                if (ec.Button(files[i])) mSelected = i;
            }
            ec.Separator();
            char buf[64] = "Selection : (aucune)";
            if (mSelected >= 0) {
                const char* s = files[mSelected];
                int32 k = 0; const char* pre = "Selection : ";
                for (; pre[k]; ++k) buf[k] = pre[k];
                for (int32 j = 0; s[j] && k + 1 < 64; ++j, ++k) buf[k] = s[j];
                buf[k] = '\0';
            }
            ec.Text(buf);
        }
    private:
        int32 mSelected = -1;
    };

    // ── Inspecteur : reglages d'edition ──────────────────────────────────────
    class InspectorPanel : public NkEditorPanel {
    public:
        // NB : centre (onglet avec le Viewport). Le split droite « propre » du dock
        // manager reste a affiner ; en attendant on regroupe en onglets au centre.
        InspectorPanel() : NkEditorPanel("Inspecteur", NkEditorDockSide::NK_CENTER) {}
        void OnUI(NkEditorFrameContext& ec) override {
            ec.Text("Transform");
            ec.SliderFloat("Position X", mPosX, -100.f, 100.f);
            ec.SliderFloat("Position Y", mPosY, -100.f, 100.f);
            ec.SliderFloat("Echelle",    mScale, 0.1f, 5.f);
            ec.Separator();
            ec.Text("Options");
            ec.Checkbox("Visible", mVisible);
            ec.Checkbox("Verrouille", mLocked);
            ec.Checkbox("Projette une ombre", mShadow);
        }
    private:
        float32 mPosX = 0.f, mPosY = 0.f, mScale = 1.f;
        bool mVisible = true, mLocked = false, mShadow = true;
    };

    // ── Console : journal applicatif ─────────────────────────────────────────
    class ConsolePanel : public NkEditorPanel {
    public:
        ConsolePanel() : NkEditorPanel("Console", NkEditorDockSide::NK_BOTTOM) {}
        void OnUI(NkEditorFrameContext& ec) override {
            ec.Text("[info]  NKEditorKit demarre.");
            ec.Text("[info]  Coquille dockable prete (NKGui docking).");
            ec.Text("[info]  Ctrl+P -> palette de commandes.");
            ec.Text("[ok]    4 panneaux enregistres : Explorateur, Viewport, Inspecteur, Console.");
            ec.Text("[hint]  Glissez un onglet pour fractionner la disposition.");
        }
    };

    // ── Viewport : zone de dessin 2D (placeholder) ───────────────────────────
    class ViewportPanel : public NkEditorPanel {
    public:
        ViewportPanel() : NkEditorPanel("Viewport", NkEditorDockSide::NK_CENTER) {}
        void OnUI(NkEditorFrameContext& ec) override {
            ec.Text("Viewport 2D (NKCanvas) — placeholder");
            auto&        ctx  = ec.Ui();
            const NkRect area = ctx.NextItemRect(520.f, 320.f);   // reserve une zone + avance le layout
            auto&        dl   = ctx.DL();
            dl.AddRectFilled(area, NkColor{18, 21, 28, 255}, 4.f);
            dl.AddRect(area, NkColor{60, 66, 82, 255}, 1.f);
            for (int32 i = 1; i < 12; ++i) {
                const float32 x = area.x + area.w * (float32)i / 12.f;
                const float32 y = area.y + area.h * (float32)i / 12.f;
                dl.AddLine({x, area.y}, {x, area.y + area.h}, NkColor{38, 43, 56, 160}, 1.f);
                dl.AddLine({area.x, y}, {area.x + area.w, y}, NkColor{38, 43, 56, 160}, 1.f);
            }
            const NkVec2 c = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
            dl.AddCircleFilled(c, 36.f, NkColor{90, 150, 230, 255});
        }
    };

} // namespace nkedemo
