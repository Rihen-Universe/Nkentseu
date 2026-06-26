#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorContext.h
// @Brief   Contexte de frame passe a chaque panneau (NkEditorPanel::OnUI).
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// NkEditorFrameContext = le contexte NKGui courant + le delta time. Sur NKGui,
// tout (fenetres, dock, layout, police, draw lists) vit DANS NkGuiContext : ce
// contexte se reduit donc a un pointeur + dt, avec des helpers minces (l'API
// NKGui etant deja terse : `ec.Text("x")` -> `nkgui::Text(*ui, "x")`).
//
// L'Editor Kit ne REIMPLEMENTE pas l'UI : il ASSEMBLE NKGui (docking, fenetres,
// widgets deja complets) dans un cadre « application d'edition ».
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKGui/NKGui.h"

namespace nkentseu {
    namespace editorkit {

        // Cote de docking par defaut d'un panneau dans le shell.
        enum class NkEditorDockSide : uint8 {
            NK_LEFT = 0,
            NK_RIGHT,
            NK_TOP,
            NK_BOTTOM,
            NK_CENTER
        };

        struct NKEDITORKIT_API NkEditorFrameContext {
            nkgui::NkGuiContext* ui = nullptr;  ///< contexte NKGui courant
            float32              dt = 0.f;       ///< delta time de la frame

            // ── Acces direct (widgets NKGui non wrappes) ────────────────────────
            nkgui::NkGuiContext& Ui() const noexcept { return *ui; }

            // ── Helpers widgets (wrappers minces sur nkgui::) ───────────────────
            void Text(const char* s) const noexcept { nkgui::Text(*ui, s); }
            void Separator()         const noexcept { nkgui::Separator(*ui); }
            bool Button(const char* label) const noexcept { return nkgui::Button(*ui, label); }
            bool Checkbox(const char* label, bool& value) const noexcept {
                return nkgui::Checkbox(*ui, label, value);
            }
            bool SliderFloat(const char* label, float32& value,
                             float32 vmin, float32 vmax) const noexcept {
                return nkgui::SliderFloat(*ui, label, value, vmin, vmax);
            }
        };

    } // namespace editorkit
} // namespace nkentseu
