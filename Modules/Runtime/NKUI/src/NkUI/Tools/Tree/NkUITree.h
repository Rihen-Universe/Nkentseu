#pragma once
/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Generic tree widget — callback-first provider.
 * Main data: TreeCallbacks, TreeState, TreeConfig, DnD reparent.
 * Change this file when: Tree provider contract or interaction model evolves.
 */
/**
 * @File    NkUITree.h
 * @Brief   Widget arbre générique NkUI — architecture callback-first.
 *          Supporte multi-select, renommage inline, DnD reparent, connecteurs.
 */
#include "NKUI/NkUILayout.h"
#include "NKUI/NkUIFont.h"

namespace nkentseu {
    namespace nkui {

        // =========================================================================
        //  Identifiant de noeud opaque
        // =========================================================================

        using NkUITreeNodeID = uint64;
        static constexpr NkUITreeNodeID NKUI_TREE_NODE_NONE = 0;

        // =========================================================================
        //  Callbacks provider (C function pointers — ABI stable)
        // =========================================================================

        /// Nombre d'enfants directs du noeud. Retourne 0 si feuille.
        using NkUITreeGetChildCountFn = int32(*)(NkUITreeNodeID node, void* userData);

        /// i-eme enfant du noeud.
        using NkUITreeGetChildFn      = NkUITreeNodeID(*)(NkUITreeNodeID node,
                                                           int32 childIndex,
                                                           void* userData);

        /// Libellé à afficher (écrit dans outLabel, max maxLen bytes).
        using NkUITreeGetLabelFn      = void(*)(NkUITreeNodeID node,
                                                char* outLabel, int32 maxLen,
                                                void* userData);

        /// Icône Unicode/ASCII (ex: "D" pour dossier, "F" pour fichier).
        /// Peut écrire "" pour pas d'icône.
        using NkUITreeGetIconFn       = void(*)(NkUITreeNodeID node,
                                                char* outIcon, int32 maxLen,
                                                void* userData);

        /// Retourne true si le noeud peut être draggé.
        using NkUITreeCanDragFn       = bool(*)(NkUITreeNodeID node, void* userData);

        /// Retourne true si 'dragged' peut être déposé comme enfant de 'target'.
        using NkUITreeCanDropFn       = bool(*)(NkUITreeNodeID dragged,
                                                NkUITreeNodeID target,
                                                void* userData);

        /// Appelé quand l'utilisateur valide un renommage.
        using NkUITreeOnRenameFn      = void(*)(NkUITreeNodeID node,
                                                const char* newName,
                                                void* userData);

        /// Appelé quand un noeud est déposé sur un autre (DnD reparent).
        using NkUITreeOnMoveFn        = void(*)(NkUITreeNodeID dragged,
                                                NkUITreeNodeID newParent,
                                                void* userData);

        /// Appelé quand la sélection change. ids = tableau des noeuds sélectionnés.
        using NkUITreeOnSelectFn      = void(*)(const NkUITreeNodeID* ids,
                                                int32 count,
                                                void* userData);

        struct NKUI_API NkUITreeCallbacks {
            NkUITreeGetChildCountFn getChildCount = nullptr;
            NkUITreeGetChildFn      getChild       = nullptr;
            NkUITreeGetLabelFn      getLabel       = nullptr;
            NkUITreeGetIconFn       getIcon        = nullptr;  // optionnel
            NkUITreeCanDragFn       canDrag        = nullptr;  // optionnel
            NkUITreeCanDropFn       canDrop        = nullptr;  // optionnel
            NkUITreeOnRenameFn      onRename       = nullptr;  // optionnel
            NkUITreeOnMoveFn        onMove         = nullptr;  // optionnel
            NkUITreeOnSelectFn      onSelect       = nullptr;  // optionnel
            void*                   userData       = nullptr;
        };

        // =========================================================================
        //  Evénements retournés par Draw()
        // =========================================================================

        enum class NkUITreeEvent : uint8 {
            NK_TREE_NONE = 0,
            NK_TREE_NODE_SELECTED,    // clic simple
            NK_TREE_NODE_ACTIVATED,   // double-clic
            NK_TREE_NODE_RENAMED,     // renommage validé
            NK_TREE_NODE_MOVED,       // DnD reparent terminé
            NK_TREE_EXPAND_CHANGED,   // noeud expand/collapse
        };

        struct NKUI_API NkUITreeResult {
            NkUITreeEvent  event     = NkUITreeEvent::NK_TREE_NONE;
            NkUITreeNodeID node      = NKUI_TREE_NODE_NONE;
            NkUITreeNodeID newParent = NKUI_TREE_NODE_NONE; // DnD
            bool           expanded  = false;               // EXPAND_CHANGED
        };

        // =========================================================================
        //  Configuration visuelle
        // =========================================================================

        struct NKUI_API NkUITreeConfig {
            float32 indentPx        = 16.f;   // retrait par niveau
            float32 rowHeightPx     = 20.f;
            float32 iconColWidthPx  = 18.f;
            bool    showConnectors  = true;   // lignes verticales de connexion
            bool    allowMultiSelect= false;
            bool    allowRename     = true;
            bool    allowDnD        = true;
            NkColor connectorColor  = {100, 100, 120, 160};
            NkColor selectedBg      = {60, 110, 200, 180};
            NkColor hoveredBg       = {70, 70, 90, 100};
            NkColor dndTargetBg     = {60, 180, 80, 120};
        };

        // =========================================================================
        //  Etat runtime (tout sur la stack — pas de heap)
        // =========================================================================

        struct NKUI_API NkUITreeState {
            static constexpr int32 MAX_OPEN     = 256;
            static constexpr int32 MAX_SELECTED = 64;

            // Noeuds ouverts (expand)
            NkUITreeNodeID openNodes[MAX_OPEN]     = {};
            int32          numOpen                 = 0;

            // Sélection courante
            NkUITreeNodeID selected[MAX_SELECTED]  = {};
            int32          numSelected             = 0;

            // Renommage inline
            bool           renaming                = false;
            NkUITreeNodeID renamingNode            = NKUI_TREE_NODE_NONE;
            char           renameBuffer[256]       = {};

            // DnD
            bool           dndActive               = false;
            NkUITreeNodeID dndSource               = NKUI_TREE_NODE_NONE;
            NkUITreeNodeID dndTarget               = NKUI_TREE_NODE_NONE;

            // Scroll
            float32        scrollY                 = 0.f;
            float32        contentHeight           = 0.f;

            // Hovered
            NkUITreeNodeID hoveredNode             = NKUI_TREE_NODE_NONE;

            // ── Helpers ──────────────────────────────────────────────────────────
            bool           IsOpen(NkUITreeNodeID id) const noexcept;
            void           SetOpen(NkUITreeNodeID id, bool open) noexcept;
            bool           IsSelected(NkUITreeNodeID id) const noexcept;
            void           SetSelected(NkUITreeNodeID id, bool sel,
                                       bool clearOthers = true) noexcept;
            void           ClearSelection() noexcept;
        };

        // =========================================================================
        //  Widget principal
        // =========================================================================

        struct NKUI_API NkUITree {
            /// Dessine l'arbre depuis une seule racine.
            /// @return événement de cette frame
            static NkUITreeResult Draw(
                NkUIContext&             ctx,
                NkUIDrawList&            dl,
                NkUIFont&                font,
                NkUIID                   id,
                NkRect                   rect,
                NkUITreeNodeID           root,
                const NkUITreeCallbacks& cb,
                const NkUITreeConfig&    cfg,
                NkUITreeState&           state) noexcept;

            /// Dessine un arbre avec plusieurs racines (roots[count]).
            static NkUITreeResult DrawMultiRoot(
                NkUIContext&             ctx,
                NkUIDrawList&            dl,
                NkUIFont&                font,
                NkUIID                   id,
                NkRect                   rect,
                const NkUITreeNodeID*    roots,
                int32                    rootCount,
                const NkUITreeCallbacks& cb,
                const NkUITreeConfig&    cfg,
                NkUITreeState&           state) noexcept;

            /// Expand tous les noeuds du chemin jusqu'à 'target'.
            static bool ExpandToNode(
                NkUITreeNodeID           root,
                NkUITreeNodeID           target,
                const NkUITreeCallbacks& cb,
                NkUITreeState&           state) noexcept;

        private:
            /// Contexte de rendu passé récursivement.
            struct DrawCtx {
                NkUIContext*             ctx;
                NkUIDrawList*            dl;
                NkUIFont*                font;
                const NkUITreeCallbacks* cb;
                const NkUITreeConfig*    cfg;
                NkUITreeState*           state;
                NkRect                   clipRect;
                float32                  curY;       // curseur Y courant (screen)
                NkUITreeResult*          result;
            };

            static void DrawNodeRecursive(DrawCtx& dc,
                                          NkUITreeNodeID node,
                                          int32 depth,
                                          bool  isLastChild) noexcept;

            static void DrawNodeRow(DrawCtx& dc,
                                    NkUITreeNodeID node,
                                    int32 depth,
                                    bool  isLastChild,
                                    bool  hasChildren) noexcept;

            static bool ExpandToNodeRec(NkUITreeNodeID cur,
                                        NkUITreeNodeID target,
                                        const NkUITreeCallbacks& cb,
                                        NkUITreeState& state) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu
