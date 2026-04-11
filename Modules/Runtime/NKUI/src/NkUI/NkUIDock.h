#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Docking data structures and docking API.
 * Main data: Dock nodes, split model, tabs conditionnel, child docking, edge highlight.
 * Change this file when: Docking behavior or node model needs extension.
 */
/**
 * @File    NkUIDock.h
 * @Brief   Systeme de docking NkUI v2 — zones, drag & drop, tabs conditionnels,
 *          split, detach, child docking, surbrillance directionnelle.
 */
#include "NKUI/NkUIWindow.h"

namespace nkentseu {
    namespace nkui {

        enum class NkUIDockNodeType : uint8 {
            NK_ROOT=0, NK_LEAF, NK_SPLIT_H, NK_SPLIT_V
        };

        enum class NkUIDockDrop : uint8 {
            NK_NONE=0, NK_LEFT, NK_RIGHT, NK_TOP, NK_BOTTOM, NK_CENTER, NK_TAB
        };

        struct NKUI_API NkUIDockNode {
            static constexpr int32 MAX_WINDOWS = 16;
            NkUIDockNodeType type       = NkUIDockNodeType::NK_LEAF;
            NkRect           rect       = {};
            float32          splitRatio = 0.5f;
            int32            parent     = -1;
            int32            children[2]= {-1,-1};
            NkUIID           windows[MAX_WINDOWS] = {};
            int32            numWindows = 0;
            int32            activeTab  = 0;
            float32          scrollY    = 0.f;
            // v2
            NkUIID           parentWindowId = NKUI_ID_NONE; // pour child docking
        };

        // ── Décorations de la barre d'onglets du dock (globales) ─────────────────
        // Callback appelé pour chaque décoration, de droite à gauche.
        // Signature : float32 fn(NkUIContext&, NkUIDrawList&, NkUIFont&,
        //                        int32 nodeIdx, NkRect decoRect, void* userData)
        // Retour : largeur effectivement consommée (ignoré pour l'instant,
        //          on utilise la largeur déclarée à l'enregistrement).
        using NkTabDecoFn = float32(*)(NkUIContext&,
                                    NkUIDrawList&,
                                    NkUIFont&,
                                    int32  nodeIdx,
                                    NkRect decoRect,
                                    void*  userData);
    
        struct NkTabDecoration {
            NkTabDecoFn fn        = nullptr;
            void*       userData  = nullptr;
            float32     width     = 24.f;
            bool        enabled   = true;
            NkUIID      id        = 0;
        };

        struct NKUI_API NkUIDockManager {
                static constexpr int32 MAX_NODES = 128;
                NkUIDockNode  nodes[MAX_NODES] = {};
                int32         numNodes   = 0;
                int32         rootIdx    = -1;
                // Drag state
                bool          isDragging     = false;
                bool          isDraggingTab  = false;
                NkUIID        dragWindowId   = NKUI_ID_NONE;
                int32         dragTargetNode = -1;
                // v2 — child docking
                NkUIID        childWindowId  = NKUI_ID_NONE; // fenetre parente courante (si child dock actif)

                void  Init(NkRect viewport) noexcept;
                void  Destroy() noexcept;
                void  SetViewport(NkRect r) noexcept;

                int32 AllocNode() noexcept;
                void  RecalcRects(int32 idx) noexcept;
                void  RecalcRectsAll() noexcept;

                /// Ancre une fenetre dans un noeud avec la direction donnee.
                /// Respecte NK_NO_TABS : si tabs interdits et drop == CENTER/TAB, degrade en NK_BOTTOM.
                /// Respecte NK_NO_DOCK  : refuse le docking si la fenetre a ce flag.
                bool  DockWindow(NkUIWindowManager& wm, NkUIID windowId,
                                int32 nodeIdx, NkUIDockDrop drop) noexcept;
                /// Detache une fenetre du dock
                void  UndockWindow(NkUIWindowManager& wm, NkUIID windowId) noexcept;

                /// Retourne true si le noeud accepte les tabs
                /// (aucune fenetre du node n'a NK_NO_TABS, et la fenetre draggee non plus).
                bool  NodeAllowsTabs(const NkUIWindowManager& wm,
                                    int32 nodeIdx,
                                    NkUIID draggedWindowId = NKUI_ID_NONE) const noexcept;

                /// Dessine une ligne de surbrillance sur le bord cible pendant le drag.
                void  DrawDirectionalHighlight(NkUIDrawList& dl,
                                            int32 nodeIdx,
                                            NkUIDockDrop drop,
                                            NkColor color = {100,180,255,220}) const noexcept;

                /// Trouve le Leaf sous un point
                int32 FindNodeAt(NkVec2 pos) const noexcept;
                /// Noeud racine
                NkUIDockNode* Root() noexcept { return rootIdx>=0?&nodes[rootIdx]:nullptr; }

                /// Dessine les dropzones et retourne la zone survolee.
                /// Filtre NK_CENTER si les tabs sont interdits pour ce node/fenetre.
                NkUIDockDrop DrawDropZones(NkUIContext& ctx, NkUIDrawList& dl,
                                        int32 nodeIdx) noexcept;

                /// Traitement des inputs dock (appele chaque frame avant Begin)
                void BeginFrame(NkUIContext& ctx, NkUIWindowManager& wm,
                                NkUIDrawList& dl, NkUIFont& font) noexcept;

                /// Dessine tout l'arbre de dock (tabs + contenu)
                void Render(NkUIContext& ctx, NkUIDrawList& dl,
                            NkUIFont& font, NkUIWindowManager& wm,
                            NkUILayoutStack& ls,
                            int32 rootOverride = -1) noexcept;

                /// Ouvre un noeud ancre pour dessiner son contenu
                bool BeginDocked(NkUIContext& ctx, NkUIWindowManager& wm,
                                NkUIDrawList& dl, const char* name) noexcept;
                void EndDocked(NkUIContext& ctx, NkUIDrawList& dl) noexcept;

                /// Child docking : cree un sous-arbre de dock dans la zone cliente d'une fenetre.
                /// La fenetre parente doit avoir le flag NK_ALLOW_DOCK_CHILD.
                bool BeginChildDock(NkUIContext& ctx, NkUIWindowManager& wm,
                                    NkUIDrawList& dl, NkUIFont& font,
                                    NkUILayoutStack& ls,
                                    NkUIID parentWindowId, NkRect childViewport) noexcept;
                void EndChildDock(NkUIContext& ctx, NkUIDrawList& dl) noexcept;

                /// Persistance du layout
                void SaveLayout(const char* path) const noexcept;
                bool LoadLayout(const char* path) noexcept;
                
                // ── Décorations de la barre d'onglets ────────────────────────────────────
                // Callback appelé pour chaque décoration, de droite à gauche.
                // Le callback dessine dans dl et retourne la largeur consommée.
                // Si le callback retourne < 0, la décoration est ignorée (désactivée).
                using TabDecoFn = float32(*)(NkUIContext&, NkUIDrawList&, NkUIFont&,
                                            int32 nodeIdx,   // index du nœud dock
                                            NkRect decoRect, // rect alloué pour cette déco
                                            void* userData);
            
                struct TabDecoration {
                    TabDecoFn fn        = nullptr;
                    void*     userData  = nullptr;
                    float32   width     = 24.f;   // largeur réservée (peut être dynamique)
                    bool      enabled   = true;
                    NkUIID    id        = 0;
                };

                // ── Docking ─────────────────────────────────────────────────────────────
                static constexpr int32 MAX_TAB_DECOS = 8;
                NkTabDecoration tabDecos[MAX_TAB_DECOS] = {};
                int32           numTabDecos             = 0;
                bool            dockOverflowButton      = true; // bouton ▶ par défaut
        
                // ── API Docking ─────────────────────────────────────────────────────────────
                bool AddTabDecoration(NkTabDecoFn fn, void* userData, float32 width, NkUIID id = 0) noexcept;
                void RemoveTabDecoration(NkUIID id) noexcept;
                void EnableTabDecoration(NkUIID id, bool enabled) noexcept;
                void SetDockOverflowButton(bool show) noexcept;

            private:
                void RenderNode(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, NkUIWindowManager& wm, int32 idx, NkUILayoutStack& ls) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu
