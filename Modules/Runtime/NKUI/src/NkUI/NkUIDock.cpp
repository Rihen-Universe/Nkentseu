/**
 * @File    NkUIDock.cpp
 * @Brief   Système de docking NkUI — refonte inspirée ImGui.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * PRINCIPES FONDAMENTAUX :
 *
 *  1. FENÊTRES FLOTTANTES TOUJOURS AU-DESSUS
 *     Le dock dessine les zones dockées dans LAYER_WINDOWS (fond).
 *     Les fenêtres flottantes sont dessinées par Begin/End dans le même layer
 *     mais avec un z-order SUPÉRIEUR, donc elles passent par-dessus.
 *     Le WindowManager garantit que les fenêtres flottantes ont un zOrder
 *     toujours plus élevé que les fenêtres dockées (qui ont zOrder=0).
 *
 *  2. TABS STYLE IMGUI
 *     La barre de tabs est en HAUT de la zone dockée.
 *     Chaque tab affiche le nom de la fenêtre + bouton [x] de fermeture.
 *     L'onglet actif est plus clair et légèrement surélevé.
 *     On peut glisser un tab hors de la zone pour undock.
 *
 *  3. ZONES DE DROP
 *     Quand on drag une fenêtre, des zones de drop (L/R/T/B/Center) 
 *     apparaissent sur la zone sous le curseur.
 *     Center = ajouter en tant que nouvel onglet dans ce leaf.
 *
 *  4. SPLITTER
 *     Les nœuds SPLIT_H/SPLIT_V ont un splitter draggable entre leurs enfants.
 */

#include "NKUI/NkUIDock.h"
#include "NKUI/NkUIWidgets.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Helpers
        // ─────────────────────────────────────────────────────────────────────────────

        static float32 DockBaselineY(const NkRect& r, const NkUIFont& f) noexcept {
            const float32 asc  = f.metrics.ascender  > 0.f ? f.metrics.ascender  : f.size * 0.8f;
            const float32 desc = f.metrics.descender >= 0.f ? f.metrics.descender
                                                             : -f.metrics.descender;
            const float32 lh   = f.metrics.lineHeight > 0.f ? f.metrics.lineHeight
                                                             : asc + (desc > 0.f ? desc : f.size * 0.2f);
            return r.y + (r.h - lh) * 0.5f + asc;
        }

        // Rebind récursif des fenêtres dockées dans un sous-arbre
        static void RebindWindowsRecursive(NkUIDockManager& dm,
                                            NkUIWindowManager& wm,
                                            int32 idx) noexcept
        {
            if (idx < 0 || idx >= dm.numNodes) return;
            NkUIDockNode& n = dm.nodes[idx];
            if (n.type == NkUIDockNodeType::NK_LEAF ||
                n.type == NkUIDockNodeType::NK_ROOT) {
                for (int32 i = 0; i < n.numWindows; ++i) {
                    NkUIWindowState* ws = wm.Find(n.windows[i]);
                    if (ws) {
                        ws->isDocked   = true;
                        ws->dockNodeId = static_cast<NkUIID>(idx);
                        // Les fenêtres dockées ont un z-order de base faible
                        ws->zOrder     = 0;
                    }
                }
                return;
            }
            RebindWindowsRecursive(dm, wm, n.children[0]);
            RebindWindowsRecursive(dm, wm, n.children[1]);
        }

        // Collapse un leaf vide : remonte le frère à la place du parent
        static void CollapseEmptyLeaf(NkUIDockManager& dm,
                                       NkUIWindowManager& wm,
                                       int32 leafIdx) noexcept
        {
            int32 current = leafIdx;
            while (current >= 0 && current < dm.numNodes) {
                NkUIDockNode& cur = dm.nodes[current];
                if (cur.type != NkUIDockNodeType::NK_LEAF || cur.numWindows > 0)
                    break;

                const int32 parentIdx = cur.parent;
                if (parentIdx < 0 || parentIdx >= dm.numNodes) {
                    // Racine : redevient ROOT vide
                    cur.type       = NkUIDockNodeType::NK_ROOT;
                    cur.children[0] = cur.children[1] = -1;
                    break;
                }

                NkUIDockNode& parent    = dm.nodes[parentIdx];
                const int32   sibIdx    = (parent.children[0] == current)
                                          ? parent.children[1] : parent.children[0];
                if (sibIdx < 0 || sibIdx >= dm.numNodes) {
                    parent.type       = NkUIDockNodeType::NK_LEAF;
                    parent.numWindows = 0;
                    parent.activeTab  = 0;
                    parent.children[0] = parent.children[1] = -1;
                    current = parentIdx;
                    continue;
                }

                // Le frère prend la place du parent
                NkUIDockNode sibCopy = dm.nodes[sibIdx];
                sibCopy.parent       = parent.parent;
                sibCopy.rect         = parent.rect;
                dm.nodes[parentIdx]  = sibCopy;

                // Corrige les liens enfants
                if (dm.nodes[parentIdx].children[0] >= 0)
                    dm.nodes[dm.nodes[parentIdx].children[0]].parent = parentIdx;
                if (dm.nodes[parentIdx].children[1] >= 0)
                    dm.nodes[dm.nodes[parentIdx].children[1]].parent = parentIdx;

                // Libère les anciens slots
                dm.nodes[current]  = NkUIDockNode{};
                dm.nodes[sibIdx]   = NkUIDockNode{};
                if (current == dm.rootIdx || sibIdx == dm.rootIdx)
                    dm.rootIdx = parentIdx;

                RebindWindowsRecursive(dm, wm, parentIdx);
                current = parentIdx;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Init / Destroy
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::Init(NkRect viewport) noexcept {
            numNodes       = 0;
            rootIdx        = -1;
            isDragging     = false;
            isDraggingTab  = false;
            dragWindowId   = NKUI_ID_NONE;
            dragTargetNode = -1;
            childWindowId  = NKUI_ID_NONE;
            ::memset(nodes, 0, sizeof(nodes));

            rootIdx = AllocNode();
            if (rootIdx >= 0) {
                nodes[rootIdx].type = NkUIDockNodeType::NK_ROOT;
                nodes[rootIdx].rect = viewport;
            }
        }

        void NkUIDockManager::Destroy() noexcept { numNodes = 0; rootIdx = -1; }

        void NkUIDockManager::SetViewport(NkRect r) noexcept {
            if (rootIdx >= 0) nodes[rootIdx].rect = r;
            RecalcRectsAll();
        }

        int32 NkUIDockManager::AllocNode() noexcept {
            if (numNodes >= MAX_NODES) return -1;
            const int32 idx = numNodes++;
            // ::memset(&nodes[idx], 0, sizeof(NkUIDockNode));
            ::memset((void*)&nodes[idx], 0, sizeof(NkUIDockNode));
            nodes[idx].splitRatio   = 0.5f;
            nodes[idx].parent       = -1;
            nodes[idx].children[0]  = nodes[idx].children[1] = -1;
            nodes[idx].parentWindowId = NKUI_ID_NONE;
            return idx;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RecalcRects
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::RecalcRects(int32 idx) noexcept {
            if (idx < 0 || idx >= numNodes) return;
            NkUIDockNode& n = nodes[idx];
            if (n.type == NkUIDockNodeType::NK_LEAF ||
                n.type == NkUIDockNodeType::NK_ROOT) return;

            const int32 c0 = n.children[0], c1 = n.children[1];
            if (c0 < 0 || c1 < 0) return;

            if (n.type == NkUIDockNodeType::NK_SPLIT_H) {
                const float32 w0 = n.rect.w * n.splitRatio;
                nodes[c0].rect = {n.rect.x,        n.rect.y, w0,             n.rect.h};
                nodes[c1].rect = {n.rect.x + w0,   n.rect.y, n.rect.w - w0, n.rect.h};
            } else {
                const float32 h0 = n.rect.h * n.splitRatio;
                nodes[c0].rect = {n.rect.x, n.rect.y,        n.rect.w, h0};
                nodes[c1].rect = {n.rect.x, n.rect.y + h0,  n.rect.w, n.rect.h - h0};
            }
            RecalcRects(c0);
            RecalcRects(c1);
        }

        void NkUIDockManager::RecalcRectsAll() noexcept { RecalcRects(rootIdx); }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NodeAllowsTabs
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::NodeAllowsTabs(const NkUIWindowManager& wm,
                                              int32 nodeIdx,
                                              NkUIID draggedWinId) const noexcept
        {
            if (nodeIdx < 0 || nodeIdx >= numNodes) return false;
            const NkUIDockNode& node = nodes[nodeIdx];
            for (int32 i = 0; i < node.numWindows; ++i) {
                const NkUIWindowState* ws =
                    const_cast<NkUIWindowManager&>(wm).Find(node.windows[i]);
                if (ws && HasFlag(ws->flags, NkUIWindowFlags::NK_NO_TABS))
                    return false;
            }
            if (draggedWinId != NKUI_ID_NONE) {
                const NkUIWindowState* dws =
                    const_cast<NkUIWindowManager&>(wm).Find(draggedWinId);
                if (dws && HasFlag(dws->flags, NkUIWindowFlags::NK_NO_TABS))
                    return false;
            }
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DockWindow
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::DockWindow(NkUIWindowManager& wm,
                                          NkUIID             winId,
                                          int32              nodeIdx,
                                          NkUIDockDrop       drop) noexcept
        {
            if (nodeIdx < 0 || nodeIdx >= numNodes) return false;
            NkUIWindowState* ws = wm.Find(winId);
            if (!ws) return false;
            if (HasFlag(ws->flags, NkUIWindowFlags::NK_NO_DOCK)) return false;

            // Center/Tab → Bottom si tabs interdits
            if ((drop == NkUIDockDrop::NK_CENTER || drop == NkUIDockDrop::NK_TAB) &&
                !NodeAllowsTabs(wm, nodeIdx, winId))
            {
                drop = NkUIDockDrop::NK_BOTTOM;
            }

            if (drop == NkUIDockDrop::NK_CENTER || drop == NkUIDockDrop::NK_TAB) {
                if (ws->isDocked) UndockWindow(wm, winId);
                NkUIDockNode& node = nodes[nodeIdx];
                // Évite les doublons
                for (int32 i = 0; i < node.numWindows; ++i)
                    if (node.windows[i] == winId) { node.activeTab = i; return true; }
                if (node.numWindows >= NkUIDockNode::MAX_WINDOWS) return false;
                node.windows[node.numWindows++] = winId;
                node.activeTab = node.numWindows - 1;
                node.type      = NkUIDockNodeType::NK_LEAF;
                ws->isDocked   = true;
                ws->dockNodeId = static_cast<NkUIID>(nodeIdx);
                ws->zOrder     = 0; // dockée = z bas
                return true;
            }

            // Directional : split
            if (ws->isDocked) UndockWindow(wm, winId);

            const int32 c0 = AllocNode(); if (c0 < 0) return false;
            const int32 c1 = AllocNode(); if (c1 < 0) return false;

            NkUIDockNode& target = nodes[nodeIdx];

            // Sauvegarde contenu actuel
            NkUIID oldWins[NkUIDockNode::MAX_WINDOWS] = {};
            const int32 oldN = target.numWindows, oldA = target.activeTab;
            for (int32 i = 0; i < oldN; ++i) oldWins[i] = target.windows[i];

            const bool horiz = (drop == NkUIDockDrop::NK_LEFT ||
                                 drop == NkUIDockDrop::NK_RIGHT);
            target.type        = horiz ? NkUIDockNodeType::NK_SPLIT_H
                                       : NkUIDockNodeType::NK_SPLIT_V;
            target.children[0] = c0;
            target.children[1] = c1;
            target.splitRatio  = 0.5f;
            target.numWindows  = 0;
            target.activeTab   = 0;
            ::memset(target.windows, 0, sizeof(target.windows));

            nodes[c0] = NkUIDockNode{};
            nodes[c0].type     = NkUIDockNodeType::NK_LEAF;
            nodes[c0].parent   = nodeIdx;
            nodes[c0].splitRatio = 0.5f;

            nodes[c1] = NkUIDockNode{};
            nodes[c1].type     = NkUIDockNodeType::NK_LEAF;
            nodes[c1].parent   = nodeIdx;
            nodes[c1].splitRatio = 0.5f;

            // La nouvelle fenêtre va à gauche/haut si L/T, droite/bas sinon
            const int32 newChildIdx  = (drop == NkUIDockDrop::NK_LEFT ||
                                         drop == NkUIDockDrop::NK_TOP) ? 0 : 1;
            const int32 existChildIdx = 1 - newChildIdx;

            NkUIDockNode& existNode = nodes[existChildIdx == 0 ? c0 : c1];
            NkUIDockNode& newNode   = nodes[newChildIdx   == 0 ? c0 : c1];

            // Transfère les anciennes fenêtres dans existNode
            for (int32 i = 0; i < oldN && existNode.numWindows < NkUIDockNode::MAX_WINDOWS; ++i) {
                existNode.windows[existNode.numWindows++] = oldWins[i];
                NkUIWindowState* moved = wm.Find(oldWins[i]);
                if (moved) {
                    moved->isDocked   = true;
                    moved->dockNodeId = static_cast<NkUIID>(existChildIdx == 0 ? c0 : c1);
                    moved->zOrder     = 0;
                }
            }
            existNode.activeTab = (oldN > 0) ?
                std::clamp(oldA, 0, existNode.numWindows - 1) : 0;

            newNode.windows[0] = winId;
            newNode.numWindows = 1;
            newNode.activeTab  = 0;
            ws->isDocked       = true;
            ws->dockNodeId     = static_cast<NkUIID>(newChildIdx == 0 ? c0 : c1);
            ws->zOrder         = 0;

            RecalcRects(nodeIdx);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  UndockWindow
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::UndockWindow(NkUIWindowManager& wm, NkUIID winId) noexcept {
            NkUIWindowState* ws = wm.Find(winId);
            if (!ws || !ws->isDocked) return;

            const int32 nodeIdx = static_cast<int32>(ws->dockNodeId);
            if (nodeIdx < 0 || nodeIdx >= numNodes) return;

            NkUIDockNode& node = nodes[nodeIdx];
            for (int32 i = 0; i < node.numWindows; ++i) {
                if (node.windows[i] == winId) {
                    for (int32 j = i; j < node.numWindows - 1; ++j)
                        node.windows[j] = node.windows[j + 1];
                    node.windows[--node.numWindows] = 0;
                    if (node.activeTab >= node.numWindows && node.activeTab > 0)
                        --node.activeTab;
                    break;
                }
            }

            ws->isDocked   = false;
            ws->dockNodeId = 0;

            // Remet dans le z-order flottant avec un z élevé
            bool inZOrder = false;
            for (int32 i = 0; i < wm.numZOrder; ++i)
                if (wm.zOrder[i] == winId) { inZOrder = true; break; }
            if (!inZOrder && wm.numZOrder < NkUIWindowManager::MAX_WINDOWS)
                wm.zOrder[wm.numZOrder++] = winId;

            wm.BringToFront(winId);
            if (wm.movingId   == winId) wm.movingId   = NKUI_ID_NONE;
            if (wm.resizingId == winId) wm.resizingId = NKUI_ID_NONE;

            if (node.numWindows == 0)
                CollapseEmptyLeaf(*this, wm, nodeIdx);
            RecalcRectsAll();
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  FindNodeAt
        // ─────────────────────────────────────────────────────────────────────────────

        int32 NkUIDockManager::FindNodeAt(NkVec2 pos) const noexcept {
            // Cherche d'abord un LEAF
            for (int32 i = 0; i < numNodes; ++i) {
                if (nodes[i].type == NkUIDockNodeType::NK_LEAF &&
                    NkRectContains(nodes[i].rect, pos))
                    return i;
            }
            if (rootIdx >= 0 && NkRectContains(nodes[rootIdx].rect, pos))
                return rootIdx;
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawDirectionalHighlight
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::DrawDirectionalHighlight(NkUIDrawList& dl,
                                                         int32         nodeIdx,
                                                         NkUIDockDrop  drop,
                                                         NkColor       color) const noexcept
        {
            if (nodeIdx < 0 || nodeIdx >= numNodes) return;
            const NkRect& r = nodes[nodeIdx].rect;
            const float32 th = 3.f;
            switch (drop) {
                case NkUIDockDrop::NK_LEFT:
                    dl.AddLine({r.x, r.y}, {r.x, r.y + r.h}, color, th); break;
                case NkUIDockDrop::NK_RIGHT:
                    dl.AddLine({r.x+r.w, r.y}, {r.x+r.w, r.y+r.h}, color, th); break;
                case NkUIDockDrop::NK_TOP:
                    dl.AddLine({r.x, r.y}, {r.x+r.w, r.y}, color, th); break;
                case NkUIDockDrop::NK_BOTTOM:
                    dl.AddLine({r.x, r.y+r.h}, {r.x+r.w, r.y+r.h}, color, th); break;
                default: break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawDropZones
        // ─────────────────────────────────────────────────────────────────────────────

        NkUIDockDrop NkUIDockManager::DrawDropZones(NkUIContext& ctx,
                                                      NkUIDrawList& dl,
                                                      int32        nodeIdx) noexcept
        {
            if (nodeIdx < 0 || nodeIdx >= numNodes) return NkUIDockDrop::NK_NONE;
            const NkRect& r   = nodes[nodeIdx].rect;
            const float32 sz  = ctx.theme.metrics.dockZoneSize;
            const NkColor zBg = ctx.theme.colors.dockZone;
            const NkColor zBd = ctx.theme.colors.dockZoneBorder;
            const NkColor hl  = {100, 180, 255, 220};
            const float32 rx  = 6.f;
            const NkVec2  mp  = ctx.input.mousePos;
            const bool centerOk = ctx.wm ? NodeAllowsTabs(*ctx.wm, nodeIdx, dragWindowId) : true;

            struct Zone { NkUIDockDrop drop; NkRect rect; bool enabled; };
            const Zone zones[] = {
                {NkUIDockDrop::NK_LEFT,   {r.x+r.w*.1f,         r.y+r.h*.5f-sz*.5f, sz, sz}, true},
                {NkUIDockDrop::NK_RIGHT,  {r.x+r.w*.9f-sz,      r.y+r.h*.5f-sz*.5f, sz, sz}, true},
                {NkUIDockDrop::NK_TOP,    {r.x+r.w*.5f-sz*.5f,  r.y+r.h*.1f,         sz, sz}, true},
                {NkUIDockDrop::NK_BOTTOM, {r.x+r.w*.5f-sz*.5f,  r.y+r.h*.9f-sz,      sz, sz}, true},
                {NkUIDockDrop::NK_CENTER, {r.x+r.w*.5f-sz*.5f,  r.y+r.h*.5f-sz*.5f,  sz, sz}, centerOk},
            };

            // Hovered drop
            NkUIDockDrop hovered = NkUIDockDrop::NK_NONE;
            for (const auto& z : zones)
                if (z.enabled && NkRectContains(z.rect, mp)) { hovered = z.drop; break; }

            // Edge detection si aucune icône n'est survolée
            if (hovered == NkUIDockDrop::NK_NONE && NkRectContains(r, mp)) {
                const float32 band = std::clamp((r.w < r.h ? r.w : r.h) * 0.1f,
                                                 sz * 0.4f, sz * 0.9f);
                const float32 dL = mp.x - r.x, dR = r.x+r.w - mp.x;
                const float32 dT = mp.y - r.y, dB = r.y+r.h - mp.y;
                float32 dMin = dL; hovered = NkUIDockDrop::NK_LEFT;
                if (dR < dMin) { dMin = dR; hovered = NkUIDockDrop::NK_RIGHT; }
                if (dT < dMin) { dMin = dT; hovered = NkUIDockDrop::NK_TOP;   }
                if (dB < dMin) { dMin = dB; hovered = NkUIDockDrop::NK_BOTTOM;}
                if (dMin > band) hovered = NkUIDockDrop::NK_NONE;
            }

            // Preview de la zone de drop
            if (hovered != NkUIDockDrop::NK_NONE) {
                NkRect prev = {};
                switch (hovered) {
                    case NkUIDockDrop::NK_LEFT:   prev={r.x,r.y,r.w*.4f,r.h}; break;
                    case NkUIDockDrop::NK_RIGHT:  prev={r.x+r.w*.6f,r.y,r.w*.4f,r.h}; break;
                    case NkUIDockDrop::NK_TOP:    prev={r.x,r.y,r.w,r.h*.4f}; break;
                    case NkUIDockDrop::NK_BOTTOM: prev={r.x,r.y+r.h*.6f,r.w,r.h*.4f}; break;
                    case NkUIDockDrop::NK_CENTER: prev={r.x+r.w*.2f,r.y+r.h*.2f,r.w*.6f,r.h*.6f}; break;
                    default: break;
                }
                dl.AddRectFilled(prev, zBd.WithAlpha(60), rx);
                dl.AddRect(prev, zBd.WithAlpha(200), 2.f, rx);
                if (hovered != NkUIDockDrop::NK_CENTER)
                    DrawDirectionalHighlight(dl, nodeIdx, hovered, hl);
            }

            // Icônes des zones
            for (const auto& z : zones) {
                if (!z.enabled) continue;
                const bool hov = (hovered == z.drop);
                dl.AddRectFilled(z.rect, hov ? zBd.WithAlpha(180) : zBg, rx);
                dl.AddRect(z.rect, zBd, 2.f, rx);
                const NkVec2 c = {z.rect.x + z.rect.w*.5f, z.rect.y + z.rect.h*.5f};
                switch (z.drop) {
                    case NkUIDockDrop::NK_LEFT:   dl.AddArrow(c, sz*.3f, 2, zBd); break;
                    case NkUIDockDrop::NK_RIGHT:  dl.AddArrow(c, sz*.3f, 0, zBd); break;
                    case NkUIDockDrop::NK_TOP:    dl.AddArrow(c, sz*.3f, 3, zBd); break;
                    case NkUIDockDrop::NK_BOTTOM: dl.AddArrow(c, sz*.3f, 1, zBd); break;
                    case NkUIDockDrop::NK_CENTER:
                        dl.AddRectFilled({c.x-sz*.2f,c.y-sz*.2f,sz*.4f,sz*.4f},zBd,3.f); break;
                    default: break;
                }
            }
            return hovered;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RenderNode — cœur du rendu dock
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::RenderNode(NkUIContext&    ctx,
                                          NkUIDrawList&   dl,
                                          NkUIFont&       font,
                                          NkUIWindowManager& wm,
                                          int32           idx,
                                          NkUILayoutStack& ls) noexcept
        {
            if (idx < 0 || idx >= numNodes) return;
            NkUIDockNode& node = nodes[idx];

            // ── SPLIT : dessine splitter et récurse ────────────────────────────────
            if (node.type == NkUIDockNodeType::NK_SPLIT_H ||
                node.type == NkUIDockNodeType::NK_SPLIT_V)
            {
                const bool vertical = (node.type == NkUIDockNodeType::NK_SPLIT_H);
                const NkUIID splitId = NkHashInt(idx, 0xD0CCC);

                NkUILayout::DrawSplitter(ctx, dl, node.rect, vertical,
                                          node.splitRatio, splitId);
                node.splitRatio = std::clamp(node.splitRatio, 0.05f, 0.95f);
                RecalcRects(idx);

                // Ligne de séparation visuelle
                const NkColor sepCol = ctx.theme.colors.separator.WithAlpha(180);
                if (node.children[0] >= 0) {
                    const NkRect& a = nodes[node.children[0]].rect;
                    if (vertical)
                        dl.AddLine({a.x+a.w, node.rect.y}, {a.x+a.w, node.rect.y+node.rect.h},
                                    sepCol, 1.f);
                    else
                        dl.AddLine({node.rect.x, a.y+a.h}, {node.rect.x+node.rect.w, a.y+a.h},
                                    sepCol, 1.f);
                }
                RenderNode(ctx, dl, font, wm, node.children[0], ls);
                RenderNode(ctx, dl, font, wm, node.children[1], ls);
                return;
            }

            // ── LEAF / ROOT avec fenêtres ──────────────────────────────────────────
            if (node.numWindows <= 0) return;
            const NkRect& r    = node.rect;
            const float32 tabH = ctx.theme.metrics.dockTabHeight;
            const NkColor sep  = ctx.theme.colors.separator.WithAlpha(180);

            // Fond de la zone dockée
            dl.AddRectFilled(r, ctx.theme.colors.bgWindow);

            // ── BARRE DE TABS ──────────────────────────────────────────────────────
            const float32 tabW  = r.w / static_cast<float32>(node.numWindows);
            const NkRect  tabBarR = {r.x, r.y, r.w, tabH};
            dl.AddRectFilled(tabBarR, ctx.theme.colors.bgHeader);
            dl.AddLine({r.x, r.y + tabH}, {r.x + r.w, r.y + tabH}, sep, 1.f);

            for (int32 t = 0; t < node.numWindows; ++t) {
                NkUIWindowState* ws = wm.Find(node.windows[t]);
                if (!ws) continue;

                const NkRect tabR = {r.x + t * tabW, r.y, tabW, tabH};
                const bool   active = (t == node.activeTab);
                const bool   hov    = NkRectContains(tabR, ctx.input.mousePos);

                // Fond du tab
                NkColor tabBg = active ? ctx.theme.colors.tabActive
                                       : (hov ? ctx.theme.colors.buttonHover
                                              : ctx.theme.colors.tabBg);
                dl.AddRectFilled(tabR, tabBg);

                // Indicateur "tab actif" : petite ligne en bas du tab
                if (active)
                    dl.AddLine({tabR.x+2, tabR.y+tabR.h-1.f},
                                {tabR.x+tabR.w-2, tabR.y+tabR.h-1.f},
                                ctx.theme.colors.accent, 2.f);

                // Séparateur vertical entre tabs
                if (t > 0)
                    dl.AddLine({tabR.x, tabR.y+3}, {tabR.x, tabR.y+tabH-3}, sep, 1.f);

                // Texte du tab
                const float32 closeW = 14.f;
                const float32 padX   = ctx.theme.metrics.paddingX;
                const NkColor txtCol = active ? ctx.theme.colors.tabActiveText
                                              : ctx.theme.colors.tabText;
                font.RenderText(dl, {tabR.x + padX, DockBaselineY(tabR, font)},
                                ws->name, txtCol,
                                tabW - padX - closeW - 4.f, true);

                // Bouton [x] de fermeture du tab
                const NkRect closeR = {tabR.x + tabW - closeW, tabR.y + (tabH - 12.f) * .5f, 12.f, 12.f};
                const bool   clHov  = NkRectContains(closeR, ctx.input.mousePos);
                if (active || hov) {
                    const NkColor cx = clHov ? NkColor{220, 60, 60, 255}
                                              : ctx.theme.colors.tabText.WithAlpha(180);
                    const float32 cx2 = closeR.x+6, cy2 = closeR.y+6, d = 3.5f;
                    dl.AddLine({cx2-d,cy2-d},{cx2+d,cy2+d}, cx, 1.5f);
                    dl.AddLine({cx2+d,cy2-d},{cx2-d,cy2+d}, cx, 1.5f);
                    if (clHov && ctx.ConsumeMouseClick(0)) {
                        UndockWindow(wm, node.windows[t]);
                        ws->isOpen = false;
                        return; // node modifié, on stop
                    }
                }

                // Clic sur le tab (hors bouton close) → sélection
                if (hov && !NkRectContains(closeR, ctx.input.mousePos) &&
                    ctx.ConsumeMouseClick(0))
                {
                    node.activeTab = t;
                    wm.activeId    = ws->id;
                    // Démarre un éventuel drag de tab
                    isDraggingTab = true;
                    dragWindowId  = ws->id;
                }

                // Drag de tab → undock si déplacement suffisant
                if (isDraggingTab && dragWindowId == ws->id &&
                    ctx.input.IsMouseDown(0) &&
                    wm.movingId == NKUI_ID_NONE)
                {
                    const float32 dx = ctx.input.mouseDelta.x;
                    const float32 dy = ctx.input.mouseDelta.y;
                    if (dx*dx + dy*dy > 36.f) { // 6px threshold
                        // Position au centre de la fenêtre undockée
                        const float32 winW = ws->size.x > 10.f ? ws->size.x : 300.f;
                        const float32 winH = ws->size.y > 10.f ? ws->size.y : 200.f;
                        UndockWindow(wm, ws->id);
                        ws->isOpen  = true;
                        ws->pos     = {ctx.input.mousePos.x - winW * .5f,
                                        ctx.input.mousePos.y - 10.f};
                        ws->size    = {winW, winH};
                        wm.activeId = ws->id;
                        wm.movingId = ws->id;
                        wm.moveOffset = {winW * .5f, 10.f};
                        wm.BringToFront(ws->id);
                        isDraggingTab = false;
                        dragWindowId  = NKUI_ID_NONE;
                        return;
                    }
                }
            }

            if (!ctx.input.mouseDown[0]) {
                isDraggingTab = false;
                dragWindowId  = NKUI_ID_NONE;
            }

            // ── CONTENU DE L'ONGLET ACTIF ──────────────────────────────────────────
            if (node.activeTab >= 0 && node.activeTab < node.numWindows) {
                NkUIWindowState* ws = wm.Find(node.windows[node.activeTab]);
                if (ws && ws->isOpen) {
                    // La zone client est tout ce qui est en dessous des tabs
                    const NkRect clientR = {r.x, r.y + tabH, r.w, r.h - tabH};

                    // Clip et fond du client
                    dl.PushClipRect(clientR, true);
                    dl.AddRectFilled(clientR, ctx.theme.colors.bgWindow);
                    dl.PopClipRect();

                    // Bordure autour de la zone client
                    dl.AddRect(clientR, ctx.theme.colors.separator.WithAlpha(100), 1.f);

                    // Met à jour pos/size de la fenêtre pour que Begin() place les widgets
                    ws->pos  = {r.x, r.y + tabH};
                    ws->size = {r.w, r.h - tabH};

                    // Focus au clic dans la zone client
                    if (NkRectContains(clientR, ctx.input.mousePos) && ctx.IsMouseClicked(0))
                        wm.activeId = ws->id;
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginFrame
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::BeginFrame(NkUIContext&     ctx,
                                          NkUIWindowManager& wm,
                                          NkUIDrawList&    dl,
                                          NkUIFont&) noexcept
        {
            // Démarre un drag depuis le WindowManager (fenêtre dockable en mouvement)
            if (!isDragging && wm.movingId != NKUI_ID_NONE) {
                NkUIWindowState* dws = wm.Find(wm.movingId);
                if (dws && !HasFlag(dws->flags, NkUIWindowFlags::NK_NO_DOCK)) {
                    isDragging   = true;
                    dragWindowId = wm.movingId;
                }
            }

            // Drop zones sur la zone survolée
            if (isDragging && dragWindowId != NKUI_ID_NONE) {
                const int32 nodeUnder = FindNodeAt(ctx.input.mousePos);
                if (nodeUnder >= 0) {
                    dragTargetNode = nodeUnder;
                    NkUIDrawList& overlayDl = ctx.layers[NkUIContext::LAYER_OVERLAY];
                    overlayDl.AddRectFilled(nodes[nodeUnder].rect,
                                             ctx.theme.colors.dockZone.WithAlpha(35));
                    const NkUIDockDrop drop = DrawDropZones(ctx, overlayDl, nodeUnder);
                    if (!ctx.input.mouseDown[0] && drop != NkUIDockDrop::NK_NONE) {
                        DockWindow(wm, dragWindowId, nodeUnder, drop);
                        wm.movingId    = NKUI_ID_NONE;
                        isDragging     = false;
                        dragWindowId   = NKUI_ID_NONE;
                        dragTargetNode = -1;
                    }
                }
                if (!ctx.input.mouseDown[0]) {
                    isDragging     = false;
                    dragWindowId   = NKUI_ID_NONE;
                    dragTargetNode = -1;
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Render
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::Render(NkUIContext&     ctx,
                                      NkUIDrawList&    dl,
                                      NkUIFont&        font,
                                      NkUIWindowManager& wm,
                                      NkUILayoutStack& ls,
                                      int32            rootOverride) noexcept
        {
            const int32 root = (rootOverride >= 0 && rootOverride < numNodes)
                               ? rootOverride : rootIdx;
            if (root >= 0)
                RenderNode(ctx, dl, font, wm, root, ls);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginDocked / EndDocked
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::BeginDocked(NkUIContext&    ctx,
                                           NkUIWindowManager& wm,
                                           NkUIDrawList&  dl,
                                           const char*    name) noexcept
        {
            NkUIWindowState* ws = wm.Find(name);
            if (!ws || !ws->isDocked || !ws->isOpen) return false;
            const int32 nodeIdx = static_cast<int32>(ws->dockNodeId);
            if (nodeIdx < 0 || nodeIdx >= numNodes) return false;
            const NkUIDockNode& node = nodes[nodeIdx];
            if (node.activeTab < 0 || node.activeTab >= node.numWindows) return false;
            if (node.windows[node.activeTab] != ws->id) return false;

            const NkRect clientR = {ws->pos.x, ws->pos.y, ws->size.x, ws->size.y};
            const NkVec2 start   = {clientR.x + ctx.theme.metrics.windowPadX,
                                     clientR.y + ctx.theme.metrics.windowPadY};
            ctx.SetCursor(start);
            ctx.cursorStart = start;
            dl.PushClipRect(clientR, true);
            return true;
        }

        void NkUIDockManager::EndDocked(NkUIContext&, NkUIDrawList& dl) noexcept {
            dl.PopClipRect();
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Child dock
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::BeginChildDock(NkUIContext&     ctx,
                                              NkUIWindowManager& wm,
                                              NkUIDrawList&    dl,
                                              NkUIFont&        font,
                                              NkUILayoutStack& ls,
                                              NkUIID           parentWinId,
                                              NkRect           childViewport) noexcept
        {
            NkUIWindowState* parentWs = wm.Find(parentWinId);
            if (!parentWs) return false;
            if (!HasFlag(parentWs->flags, NkUIWindowFlags::NK_ALLOW_DOCK_CHILD))
                return false;

            if (parentWs->childDockRoot < 0 || parentWs->childDockRoot >= numNodes) {
                parentWs->childDockRoot = AllocNode();
                if (parentWs->childDockRoot < 0) return false;
                nodes[parentWs->childDockRoot].type   = NkUIDockNodeType::NK_ROOT;
                nodes[parentWs->childDockRoot].rect   = childViewport;
                nodes[parentWs->childDockRoot].parent = -1;
                nodes[parentWs->childDockRoot].parentWindowId = parentWinId;
            } else {
                nodes[parentWs->childDockRoot].rect = childViewport;
                RecalcRects(parentWs->childDockRoot);
            }

            childWindowId = parentWinId;
            dl.PushClipRect(childViewport, true);
            Render(ctx, dl, font, wm, ls, parentWs->childDockRoot);
            return true;
        }

        void NkUIDockManager::EndChildDock(NkUIContext&, NkUIDrawList& dl) noexcept {
            childWindowId = NKUI_ID_NONE;
            dl.PopClipRect();
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Sérialisation JSON
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::SaveLayout(const char* path) const noexcept {
            FILE* f = ::fopen(path, "wb");
            if (!f) return;
            ::fprintf(f, "{\"numNodes\":%d,\"rootIdx\":%d,\"nodes\":[\n",
                      numNodes, rootIdx);
            for (int32 i = 0; i < numNodes; ++i) {
                const NkUIDockNode& n = nodes[i];
                ::fprintf(f,
                    "{\"type\":%d,\"rect\":[%.1f,%.1f,%.1f,%.1f],"
                    "\"ratio\":%.4f,\"parent\":%d,\"c0\":%d,\"c1\":%d,"
                    "\"nw\":%d,\"at\":%d}%s\n",
                    (int32)n.type, n.rect.x, n.rect.y, n.rect.w, n.rect.h,
                    n.splitRatio, n.parent, n.children[0], n.children[1],
                    n.numWindows, n.activeTab, i < numNodes-1 ? "," : "");
            }
            ::fprintf(f, "]}\n");
            ::fclose(f);
        }

        bool NkUIDockManager::LoadLayout(const char* path) noexcept {
            FILE* f = ::fopen(path, "rb");
            if (!f) return false;
            char buf[65536] = {};
            ::fread(buf, 1, sizeof(buf)-1, f);
            ::fclose(f);
            int32 rN = 0, rI = -1;
            if (::sscanf(buf, "{\"numNodes\":%d,\"rootIdx\":%d", &rN, &rI) != 2)
                return false;
            if (rN <= 0 || rN > MAX_NODES) return false;
            ::memset(nodes, 0, sizeof(nodes));
            numNodes = rN; rootIdx = rI;
            const char* p = ::strstr(buf, "\"nodes\":[");
            if (!p) return false; p += 9;
            for (int32 i = 0; i < numNodes; ++i) {
                p = ::strstr(p, "{\"type\":");
                if (!p) break;
                int32 tp=0, par=-1, c0=-1, c1=-1, nw=0, at=0;
                float32 rx=0,ry=0,rw=0,rh=0,ratio=0.5f;
                ::sscanf(p,
                    "{\"type\":%d,\"rect\":[%f,%f,%f,%f],"
                    "\"ratio\":%f,\"parent\":%d,\"c0\":%d,\"c1\":%d,"
                    "\"nw\":%d,\"at\":%d}",
                    &tp,&rx,&ry,&rw,&rh,&ratio,&par,&c0,&c1,&nw,&at);
                nodes[i].type        = static_cast<NkUIDockNodeType>(tp);
                nodes[i].rect        = {rx,ry,rw,rh};
                nodes[i].splitRatio  = ratio;
                nodes[i].parent      = par;
                nodes[i].children[0] = c0;
                nodes[i].children[1] = c1;
                nodes[i].numWindows  = 0; // windows rebindées par l'app
                nodes[i].activeTab   = at;
                ++p;
            }
            return true;
        }

    } // namespace nkui
} // namespace nkentseu