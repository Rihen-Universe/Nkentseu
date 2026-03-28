#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Docking data structures and docking API.
 * Main data: Dock nodes, split model, docking operations contract.
 * Change this file when: Docking behavior or node model needs extension.
 */
/**
 * @File    NkUIDock.h
 * @Brief   Système de docking NkUI — zones, drag & drop, tabs, split, detach.
 */
#include "NkUI/NkUIWindow.h"

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

            void  Init(NkRect viewport) noexcept;
            void  Destroy() noexcept;
            void  SetViewport(NkRect r) noexcept;

            int32 AllocNode() noexcept;
            void  RecalcRects(int32 idx) noexcept;
            void  RecalcRectsAll() noexcept;

            /// Ancre une fenêtre dans un nœud avec la direction donnée
            bool  DockWindow(NkUIWindowManager& wm, NkUIID windowId,
                            int32 nodeIdx, NkUIDockDrop drop) noexcept;
            /// Détache une fenêtre du dock
            void  UndockWindow(NkUIWindowManager& wm, NkUIID windowId) noexcept;

            /// Trouve le Leaf sous un point
            int32 FindNodeAt(NkVec2 pos) const noexcept;
            /// Nœud racine
            NkUIDockNode* Root() noexcept { return rootIdx>=0?&nodes[rootIdx]:nullptr; }

            /// Dessine les dropzones et retourne la zone survolée
            NkUIDockDrop DrawDropZones(NkUIContext& ctx, NkUIDrawList& dl,
                                        int32 nodeIdx) noexcept;

            /// Traitement des inputs dock (appelé chaque frame avant Begin)
            void BeginFrame(NkUIContext& ctx, NkUIWindowManager& wm,
                            NkUIDrawList& dl, NkUIFont& font) noexcept;

            /// Dessine tout l'arbre de dock (tabs + contenu)
            void Render(NkUIContext& ctx, NkUIDrawList& dl,
                        NkUIFont& font, NkUIWindowManager& wm,
                        NkUILayoutStack& ls) noexcept;

            /// Ouvre un nœud ancré pour dessiner son contenu
            bool BeginDocked(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, const char* name) noexcept;
            void EndDocked(NkUIContext& ctx, NkUIDrawList& dl) noexcept;

            /// Persistance du layout
            void SaveLayout(const char* path) const noexcept;
            bool LoadLayout(const char* path) noexcept;

        private:
            void RenderNode(NkUIContext& ctx, NkUIDrawList& dl,
                            NkUIFont& font, NkUIWindowManager& wm,
                            int32 idx, NkUILayoutStack& ls) noexcept;
        };
    }
} // namespace nkentseu
