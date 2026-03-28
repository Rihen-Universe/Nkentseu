#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Menu bar, menu, popup and modal API declarations.
 * Main data: Menu stack state and menu item contracts.
 * Change this file when: Menu APIs or menu state model must evolve.
 */
/**
 * @File    NkUIMenu.h
 * @Brief   MenuBar, Menu, MenuItem, ContextMenu, Popup, Modal.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Trois niveaux de menus :
 *    MenuBar  — barre horizontale en haut de l'application
 *    Menu     — menu déroulant vertical (depuis MenuBar ou clic droit)
 *    MenuItem — entrée de menu (label, raccourci, sous-menu, séparateur)
 *
 *  Popups :
 *    BeginPopup / EndPopup  — popup non-modal (se ferme au clic extérieur)
 *    BeginModal / EndModal  — dialogue modal (overlay, bloque l'UI)
 *    OpenPopup / ClosePopup — contrôle programmatique
 *
 *  Tous les menus sont rendus sur le layer LAYER_POPUPS (au-dessus de tout).
 *
 *  Usage :
 *    if (NkUIMenu::BeginMenuBar(ctx, dl, font, {0,0,1280,25})) {
 *        if (NkUIMenu::BeginMenu(ctx, dl, font, "Fichier")) {
 *            if (NkUIMenu::MenuItem(ctx, dl, font, "Nouveau", "Ctrl+N")) { ... }
 *            if (NkUIMenu::MenuItem(ctx, dl, font, "Ouvrir", "Ctrl+O")) { ... }
 *            NkUIMenu::Separator(ctx, dl);
 *            if (NkUIMenu::MenuItem(ctx, dl, font, "Quitter", "Alt+F4")) { quit=true; }
 *            NkUIMenu::EndMenu(ctx);
 *        }
 *        NkUIMenu::EndMenuBar(ctx);
 *    }
 */
#include "NkUI/NkUIContext.h"
#include "NkUI/NkUIFont.h"
#include "NkUI/NkUILayout.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  État d'un menu déroulant
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUIMenuState {
            NkUIID  id        = 0;
            NkRect  rect      = {};   // rect du bouton qui a ouvert ce menu
            NkRect  panelRect = {};   // rect du panneau déroulant
            bool    open      = false;
            float32 openAnim  = 0.f;
            int32   depth     = 0;    // profondeur (0 = menubar, 1 = sous-menu, etc.)
            int32   itemCount = 0;  // ← AJOUTER
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIMenu — API statique (immédiat-mode)
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIMenu {

            // ── MenuBar ───────────────────────────────────────────────────────────────
            /// Ouvre la barre de menus. rect = zone de la barre (ex: {0,0,W,25}).
            static bool BeginMenuBar(NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    NkRect rect) noexcept;
            static void EndMenuBar  (NkUIContext& ctx) noexcept;

            // ── Menu déroulant ────────────────────────────────────────────────────────
            /// Menu dans la menubar ou sous-menu d'un item.
            static bool BeginMenu   (NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    const char* label,
                                    bool enabled = true) noexcept;
            static void EndMenu     (NkUIContext& ctx) noexcept;

            // ── Items ─────────────────────────────────────────────────────────────────
            /// Item cliquable avec label et raccourci optionnel. Retourne true si activé.
            static bool MenuItem    (NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    const char* label,
                                    const char* shortcut = nullptr,
                                    bool*       selected = nullptr,
                                    bool        enabled  = true) noexcept;

            /// Séparateur horizontal dans un menu.
            static void Separator   (NkUIContext& ctx, NkUIDrawList& dl) noexcept;

            // ── Menu contextuel (clic droit) ──────────────────────────────────────────
            /// Ouvre un menu contextuel à la position de la souris.
            static void OpenContextMenu (NkUIContext& ctx, const char* id) noexcept;
            static bool BeginContextMenu(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        NkUIFont& font,
                                        const char* id = "##ctx") noexcept;
            static void EndContextMenu  (NkUIContext& ctx) noexcept;

            // ── Popup ─────────────────────────────────────────────────────────────────
            /// Ouvre un popup par son ID (appel programmatique).
            static void OpenPopup  (NkUIContext& ctx, const char* id) noexcept;
            static void ClosePopup (NkUIContext& ctx) noexcept;
            /// Begin/End popup — retourne true si le popup est ouvert.
            static bool BeginPopup (NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    const char* id,
                                    NkVec2 size = {200,0}) noexcept;
            static void EndPopup   (NkUIContext& ctx, NkUIDrawList& dl) noexcept;

            // ── Modal ─────────────────────────────────────────────────────────────────
            static void OpenModal  (NkUIContext& ctx, const char* title) noexcept;
            static bool BeginModal (NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    const char* title,
                                    bool* open = nullptr,
                                    NkVec2 size = {400,200}) noexcept;
            static void EndModal   (NkUIContext& ctx, NkUIDrawList& dl) noexcept;

            // ── État interne ──────────────────────────────────────────────────────────
            static void CloseAllMenus(NkUIContext& ctx) noexcept;
            static bool IsAnyMenuOpen(NkUIContext& ctx) noexcept;

        private:
            static constexpr int32 MAX_MENUS   = 16;
            static constexpr int32 MAX_POPUPS  = 8;
            static constexpr float32 ITEM_H    = 26.f;
            static constexpr float32 PANEL_W   = 200.f;

            static NkUIMenuState sMenus[MAX_MENUS];
            // Pile runtime des menus actuellement "BeginMenu ouverts" cette frame.
            // Séparée de sMenus (état persistant par ID) pour éviter la corruption d'état.
            static NkUIMenuState* sMenuStack[MAX_MENUS];
            static int32         sMenuDepth;
            // Un seul menu top-level (menubar) peut être ouvert à la fois.
            static NkUIID        sOpenTopLevelMenuId;
            static NkRect        sMenuBarRect;
            static float32       sMenuBarCursorX;
            static bool          sInMenuBar;

            // Stack des popups ouverts
            struct PopupEntry { NkUIID id; NkVec2 pos; NkVec2 size; bool open; };
            static PopupEntry    sPopups[MAX_POPUPS];
            static int32         sPopupDepth;

            static NkRect DrawMenuPanel(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        const NkUIMenuState& ms) noexcept;
            static bool   DrawMenuButton(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        NkUIFont& font,
                                        const char* label,
                                        NkRect r,
                                        bool open, bool enabled) noexcept;
            static NkRect CalcPanelRect (NkRect trigger, int32 depth,
                                        int32 itemCount) noexcept;
        };
    }
} // namespace nkentseu
