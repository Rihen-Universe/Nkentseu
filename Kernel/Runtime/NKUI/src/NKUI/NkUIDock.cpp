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
            dragSourceNode = -1;
            childWindowId  = NKUI_ID_NONE;
            ::memset(nodes, 0, sizeof(nodes));

            rootIdx = AllocNode();
            if (rootIdx >= 0) {
                nodes[rootIdx].type = NkUIDockNodeType::NK_ROOT;
                nodes[rootIdx].rect = viewport;
            }
        }

        void NkUIDockManager::Destroy() noexcept { numNodes = 0; rootIdx = -1; }

        void NkUIDockManager::SetViewport(NkRect r, NkUIWindowManager* wm) noexcept {
            if (rootIdx >= 0) nodes[rootIdx].rect = r;
            RecalcRectsAll();
            if (wm) SyncDockedWindowRects(*wm);  // sync immédiat
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

        // =============================================================================
        // SECTION B : Implémentation des méthodes — ajouter dans NkUIContext.cpp
        // =============================================================================
        bool NkUIDockManager::AddTabDecoration(NkTabDecoFn fn, void* userData,
                                        float32 width, NkUIID id) noexcept {
            if (!fn || numTabDecos >= MAX_TAB_DECOS) return false;
            NkTabDecoration& d = tabDecos[numTabDecos++];
            d.fn       = fn;
            d.userData = userData;
            d.width    = width;
            d.enabled  = true;
            // d.id       = id ? id : NkHashPtr(fn, static_cast<NkUIID>(numTabDecos));
            d.id = id ? id : NkHashPtr(reinterpret_cast<const void*>(fn), static_cast<NkUIID>(numTabDecos));
            return true;
        }
        
        void NkUIDockManager::RemoveTabDecoration(NkUIID id) noexcept {
            for (int32 i = 0; i < numTabDecos; ++i) {
                if (tabDecos[i].id == id) {
                    for (int32 j = i; j < numTabDecos - 1; ++j)
                        tabDecos[j] = tabDecos[j + 1];
                    --numTabDecos;
                    return;
                }
            }
        }
        
        void NkUIDockManager::EnableTabDecoration(NkUIID id, bool enabled) noexcept {
            for (int32 i = 0; i < numTabDecos; ++i)
                if (tabDecos[i].id == id) { tabDecos[i].enabled = enabled; return; }
        }
        
        void NkUIDockManager::SetDockOverflowButton(bool show) noexcept {
            dockOverflowButton = show;
        }

        void NkUIDockManager::SyncDockedWindowRects(NkUIWindowManager& wm) noexcept {
            for (int32 i = 0; i < numNodes; ++i) {
                NkUIDockNode& node = nodes[i];
                if (node.type != NkUIDockNodeType::NK_LEAF && node.type != NkUIDockNodeType::NK_ROOT) continue;
                if (node.numWindows <= 0) continue;

                // Calcule le clientRect (sous la tabBar) 
                // identique à ce que fait RenderNode
                const float32 tabH = 0.f; // sera lu depuis ctx.theme dans RenderNode,
                                        // ici on utilise la valeur stockée ou une constante
                // Pour éviter de dépendre de ctx ici, stocker tabH dans NkUIDockNode
                // ou passer la valeur. Solution simple : mettre à jour pos/size
                // pour TOUTES les fenêtres du node avec le rect du node.
                // RenderNode raffinera ensuite avec le vrai tabH.
                for (int32 j = 0; j < node.numWindows; ++j) {
                    NkUIWindowState* ws = wm.Find(node.windows[j]);
                    if (ws && ws->isDocked) {
                        // Approximation sans tabH — sera corrigée au rendu
                        ws->pos  = { node.rect.x, node.rect.y };
                        ws->size = { node.rect.w, node.rect.h };
                    }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RenderNode — cœur du rendu dock
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::RenderNode(NkUIContext&       ctx,
                                        NkUIDrawList&      /*dl_unused*/,
                                        NkUIFont&          font,
                                        NkUIWindowManager& wm,
                                        int32              idx,
                                        NkUILayoutStack&   ls) noexcept
        {
            if (idx < 0 || idx >= numNodes) return;
            NkUIDockNode& node = nodes[idx];
        
            // ── Nœuds split ───────────────────────────────────────────────────────────
            if (node.type == NkUIDockNodeType::NK_SPLIT_H || node.type == NkUIDockNodeType::NK_SPLIT_V) {
                NkUIDrawList& bgDl  = ctx.layers[NkUIContext::LAYER_BG];
                const bool    vert  = (node.type == NkUIDockNodeType::NK_SPLIT_H);
                const NkUIID  spId  = NkHashInt(idx, 0xDC);
        
                NkUILayout::DrawSplitter(ctx, bgDl, node.rect, vert, node.splitRatio, spId);
                node.splitRatio = (node.splitRatio < 0.05f) ? 0.05f
                                : (node.splitRatio > 0.95f) ? 0.95f
                                : node.splitRatio;
                RecalcRects(idx);
        
                const NkColor lc = ctx.theme.colors.separator.WithAlpha(200);
                if (node.children[0] >= 0) {
                    const NkRect& a = nodes[node.children[0]].rect;
                    if (vert)
                        bgDl.AddLine({a.x+a.w, node.rect.y},
                                    {a.x+a.w, node.rect.y+node.rect.h}, lc, 1.f);
                    else
                        bgDl.AddLine({node.rect.x, a.y+a.h},
                                    {node.rect.x+node.rect.w, a.y+a.h}, lc, 1.f);
                }
                RenderNode(ctx, bgDl, font, wm, node.children[0], ls);
                RenderNode(ctx, bgDl, font, wm, node.children[1], ls);
                return;
            }
        
            if ((node.type != NkUIDockNodeType::NK_LEAF && node.type != NkUIDockNodeType::NK_ROOT) || node.numWindows <= 0) return;
        
            // Dock dans LAYER_BG — flottantes (LAYER_WINDOWS) toujours au-dessus
            NkUIDrawList& dl   = ctx.layers[NkUIContext::LAYER_BG];
            const NkRect& r    = node.rect;
            const float32 tabH = ctx.theme.metrics.dockTabHeight;
            const auto&   col  = ctx.theme.colors;
            const auto&   met  = ctx.theme.metrics;
            const NkColor sep  = col.separator.WithAlpha(200);
            const NkVec2  mp   = ctx.input.mousePos;
        
            dl.AddRectFilled({r.x, r.y + tabH, r.w, r.h - tabH}, col.bgWindow);
        
            // =========================================================================
            // CALCUL DE L'ESPACE DÉCORATIONS (lu depuis ctx)
            // =========================================================================
        
            const float32 kOverflowW = dockOverflowButton ? 22.f : 0.f;
        
            float32 decoTotalW = kOverflowW;
            for (int32 i = 0; i < numTabDecos; ++i)
                if (tabDecos[i].enabled && tabDecos[i].fn)
                    decoTotalW += tabDecos[i].width;
        
            const float32 tabsAvailW = r.w - decoTotalW;
        
            // =========================================================================
            // CALCUL DES LARGEURS D'ONGLETS (adaptatif + troncature proportionnelle)
            // =========================================================================
        
            constexpr float32 kPadX    = 10.f;
            constexpr float32 kCloseW  = 18.f;
            constexpr float32 kMinTabW = 40.f;
        
            float32 natW[NkUIDockNode::MAX_WINDOWS] = {};
            float32 finW[NkUIDockNode::MAX_WINDOWS] = {};
            float32 totalNat = 0.f;
        
            for (int32 t = 0; t < node.numWindows; ++t) {
                const NkUIWindowState* ws = wm.Find(node.windows[t]);
                const float32 tw = (ws ? font.MeasureWidth(ws->name) : 40.f)
                                + kPadX * 2.f + kCloseW;
                natW[t]   = tw;
                totalNat += tw;
            }
        
            const float32 avail = (tabsAvailW > 1.f) ? tabsAvailW : 1.f;
            if (totalNat <= avail) {
                for (int32 t = 0; t < node.numWindows; ++t) finW[t] = natW[t];
            } else {
                const float32 ratio = avail / totalNat;
                for (int32 t = 0; t < node.numWindows; ++t)
                    finW[t] = (natW[t] * ratio < kMinTabW) ? kMinTabW : natW[t] * ratio;
            }
        
            float32 totalFin = 0.f;
            for (int32 t = 0; t < node.numWindows; ++t) totalFin += finW[t];
        
            // =========================================================================
            // DÉTECTION DROP SUR BARRE D'ONGLETS
            // (quand isDragging et curseur sur la tabBar de ce nœud)
            // Avant/après selon la moitié de l'onglet survolé.
            // =========================================================================
        
            const NkRect tabBarR = {r.x, r.y, avail, tabH};
            bool  tabDropActive  = false;
            int32 insertBefore   = node.numWindows; // défaut : à la fin

            // APRÈS — ne pas activer tabDrop si on est sur notre propre node source
            // et que la fenêtre y est déjà (évite l'auto-dock)
            const bool dragIsFromThisNode = (dragSourceNode == idx);
            const bool dragWinAlreadyHere = [&]() -> bool {
                for (int32 t = 0; t < node.numWindows; ++t)
                    if (node.windows[t] == dragWindowId) return true;
                return false;
            }();
        
            // if (isDragging && dragWindowId != NKUI_ID_NONE && NkRectContains(tabBarR, mp) && !(dragIsFromThisNode && dragWinAlreadyHere)) {
            if (isDragging && dragWindowId != NKUI_ID_NONE && NkRectContains(tabBarR, mp)) {
                tabDropActive = true;
                float32 tx = r.x;
                for (int32 t = 0; t < node.numWindows; ++t) {
                    const float32 mid = tx + finW[t] * 0.5f;
                    if (mp.x < mid) { insertBefore = t; break; }
                    // Si on dépasse la moitié → insertion après cet onglet
                    if (mp.x >= mid && mp.x < tx + finW[t]) {
                        insertBefore = t + 1;
                        break;
                    }
                    tx += finW[t];
                }
            }
        
            // =========================================================================
            // TRAITEMENT DES CLICS (avant rendu du contenu)
            // =========================================================================
        
            if (!tabDropActive) {
                float32 tx = r.x;
                for (int32 t = 0; t < node.numWindows; ++t) {
                    const float32 tw    = finW[t];
                    const NkRect  tabR  = {tx, r.y, tw, tabH};
                    const NkRect  clR   = {tx + tw - kCloseW,
                                        r.y + (tabH - 14.f) * 0.5f, 14.f, 14.f};

                    const bool dockCanReceiveInput = (ctx.wm == nullptr || ctx.wm->hoveredId == NKUI_ID_NONE);
        
                    // Bouton X
                    // if (NkRectContains(clR, mp) && ctx.ConsumeMouseClick(0)) {
                    if (dockCanReceiveInput && NkRectContains(clR, mp) && ctx.ConsumeMouseClick(0)) {
                        NkUIWindowState* ws = wm.Find(node.windows[t]);
                        UndockWindow(wm, node.windows[t]);
                        if (ws) ws->isOpen = false;
                        break;
                    }
                    // Sélection d'onglet
                    // if (NkRectContains(tabR, mp) && !NkRectContains(clR, mp) &&
                    //     ctx.ConsumeMouseClick(0))
                    // {
                    //     node.activeTab = t;
                    //     wm.activeId    = node.windows[t];
                    //     isDraggingTab  = true;
                    //     dragWindowId   = node.windows[t];
                    // }

                    if (dockCanReceiveInput && NkRectContains(tabR, mp) && !NkRectContains(clR, mp) && ctx.ConsumeMouseClick(0)) {
                        node.activeTab = t;
                        wm.activeId    = node.windows[t];
                        isDraggingTab  = true;
                        dragWindowId   = node.windows[t];
                        dragSourceNode = idx;   // ← stocker le nœud source
                    }
                    tx += tw;
                }
            }
        
            if (node.numWindows > 0 && node.activeTab >= node.numWindows)
                node.activeTab = node.numWindows - 1;
        
            // =========================================================================
            // RENDU DES ONGLETS
            // =========================================================================
        
            dl.AddRectFilled(tabBarR, col.bgHeader);
        
            {
                float32 tx = r.x;
                for (int32 t = 0; t < node.numWindows; ++t) {
                    const float32 tw    = finW[t];
                    const NkRect  tabR  = {tx, r.y, tw, tabH};
                    const bool    act   = (t == node.activeTab);
                    const bool    hov   = !tabDropActive && NkRectContains(tabR, mp);
                    const NkRect  clR   = {tx + tw - kCloseW,
                                        r.y + (tabH - 14.f) * 0.5f, 14.f, 14.f};
                    const bool    clHov = hov && NkRectContains(clR, mp);
                    const NkUIWindowState* ws = wm.Find(node.windows[t]);
        
                    // Fond
                    const NkColor bg = act  ? col.tabActive
                                    : hov  ? col.buttonHover
                                            : col.tabBg;
                    if (act) {
                        dl.AddRectFilledCorners(tabR, bg, met.cornerRadiusSm, 0x3);
                    } else {
                        dl.AddRectFilled(tabR, bg);
                        dl.AddLine({tx, r.y + tabH - 1.f},
                                    {tx + tw, r.y + tabH - 1.f}, sep, 1.f);
                    }
        
                    // Séparateur vertical
                    if (t > 0)
                        dl.AddLine({tx, r.y + 4.f}, {tx, r.y + tabH - 4.f}, sep, 1.f);
        
                    // Texte
                    if (ws) {
                        const float32 maxTW = tw - kPadX * 2.f - kCloseW;
                        const float32 asc   = font.metrics.ascender > 0.f
                                            ? font.metrics.ascender : font.size * 0.8f;
                        const float32 dsc   = font.metrics.descender >= 0.f
                                            ? font.metrics.descender
                                            : -font.metrics.descender;
                        const float32 lh    = font.metrics.lineHeight > 0.f
                                            ? font.metrics.lineHeight
                                            : asc + (dsc > 0.f ? dsc : font.size * 0.2f);
                        font.RenderText(dl, {tx + kPadX, r.y + (tabH - lh) * 0.5f + asc},
                                        ws->name,
                                        act ? col.tabActiveText : col.tabText,
                                        maxTW, /*ellipsis=*/true);
                    }
        
                    // Bouton X
                    if (hov || act) {
                        const NkVec2  cx  = {clR.x + clR.w * 0.5f, clR.y + clR.h * 0.5f};
                        const float32 d   = 3.5f;
                        const NkColor xc  = clHov ? NkColor{220,60,60,255} : col.tabText;
                        if (clHov) dl.AddCircleFilled(cx, 7.f, {200,50,50,180});
                        dl.AddLine({cx.x-d,cx.y-d},{cx.x+d,cx.y+d}, xc, 1.5f);
                        dl.AddLine({cx.x+d,cx.y-d},{cx.x-d,cx.y+d}, xc, 1.5f);
                    }
        
                    // ── Indicateur drop (trait bleu avant/après) ──────────────────────
                    if (tabDropActive) {
                        // Indicateur avant l'onglet t
                        if (insertBefore == t) {
                            const float32 ix = tx - 1.5f;
                            dl.AddLine({ix, r.y + 2.f}, {ix, r.y + tabH - 2.f},
                                    {80,180,255,255}, 3.f);
                            // Petite flèche vers le bas
                            dl.AddTriangleFilled({ix - 5.f, r.y + 2.f},
                                                {ix + 5.f, r.y + 2.f},
                                                {ix, r.y + 9.f},
                                                {80,180,255,220});
                        }
                        // Indicateur après le dernier onglet (insérer en fin)
                        if (t == node.numWindows - 1 && insertBefore == node.numWindows) {
                            const float32 ix = tx + tw - 1.5f;
                            dl.AddLine({ix, r.y + 2.f}, {ix, r.y + tabH - 2.f},
                                    {80,180,255,255}, 3.f);
                            dl.AddTriangleFilled({ix - 5.f, r.y + 2.f},
                                                {ix + 5.f, r.y + 2.f},
                                                {ix, r.y + 9.f},
                                                {80,180,255,220});
                        }
                    }
        
                    tx += tw;
                }
        
                // Ligne basse sur l'espace vide
                if (totalFin < avail)
                    dl.AddLine({r.x + totalFin, r.y + tabH - 1.f},
                                {r.x + avail,   r.y + tabH - 1.f}, sep, 1.f);
            }

            // ── DROP EFFECTIF SUR BARRE D'ONGLETS ─────────────────────────────────────
            if (tabDropActive && !ctx.input.mouseDown[0]) {

                // Cherche si dragWindowId est déjà dans ce nœud
                int32 srcTabIdx = -1;
                for (int32 t = 0; t < node.numWindows; ++t) {
                    if (node.windows[t] == dragWindowId) {
                        srcTabIdx = t;
                        break;
                    }
                }

                if (srcTabIdx >= 0) {
                    // CAS 1 : réordre interne — la fenêtre est déjà dans ce nœud
                    // Ne réordonner que si la position change vraiment
                    if (insertBefore != srcTabIdx && insertBefore != srcTabIdx + 1) {
                        const NkUIID moved = node.windows[srcTabIdx];
                        for (int32 i = srcTabIdx; i < node.numWindows - 1; ++i)
                            node.windows[i] = node.windows[i + 1];
                        node.numWindows--;
                        int32 dest = insertBefore;
                        if (srcTabIdx < insertBefore) dest--;
                        dest = (dest < 0) ? 0 : (dest > node.numWindows ? node.numWindows : dest);
                        for (int32 i = node.numWindows; i > dest; --i)
                            node.windows[i] = node.windows[i - 1];
                        node.windows[dest] = moved;
                        node.numWindows++;
                        node.activeTab = dest;
                    }
                } else {
                    // CAS 2 : ajout depuis un autre nœud ou depuis une fenêtre flottante
                    const int32 prevN = node.numWindows;
                    if (DockWindow(wm, dragWindowId, idx, NkUIDockDrop::NK_CENTER)) {
                        // Réordre si insertBefore n'est pas en fin
                        if (node.numWindows > prevN && insertBefore < prevN) {
                            const NkUIID nw = node.windows[node.numWindows - 1];
                            for (int32 i = node.numWindows - 1; i > insertBefore; --i)
                                node.windows[i] = node.windows[i - 1];
                            node.windows[insertBefore] = nw;
                            node.activeTab = insertBefore;
                        }
                    }
                }

                // Reset complet dans tous les cas
                wm.movingId    = NKUI_ID_NONE;
                isDragging     = false;
                isDraggingTab  = false;
                dragWindowId   = NKUI_ID_NONE;
                dragSourceNode = -1;
                dragTargetNode = -1;
            }
        
            // =========================================================================
            // DÉCORATIONS À DROITE (lues depuis ctx, rendues de droite à gauche)
            // Ordre : [decos user N-1..0] [overflow ▶]
            // =========================================================================
            {
                float32 decoX = r.x + r.w;
        
                // ── Décorations utilisateur (de droite à gauche) ──────────────────────
                for (int32 i = numTabDecos - 1; i >= 0; --i) {
                    NkTabDecoration& deco = tabDecos[i];
                    if (!deco.enabled || !deco.fn) continue;
                    decoX -= deco.width;
                    const NkRect decoR = {decoX, r.y, deco.width, tabH};
                    dl.AddLine({decoX, r.y + 2.f}, {decoX, r.y + tabH - 2.f}, sep, 1.f);
                    deco.fn(ctx, dl, font, idx, decoR, deco.userData);
                }
        
                // ── Bouton overflow ▶ ─────────────────────────────────────────────────
                if (dockOverflowButton) {
                    decoX -= kOverflowW;
                    const NkRect ovR  = {decoX, r.y, kOverflowW, tabH};
                    const bool   hov  = NkRectContains(ovR, mp);
        
                    // ID unique par nœud pour l'état ouvert/fermé
                    static NkUIID sOpenNode = NKUI_ID_NONE;
                    const NkUIID  nodeUid   = NkHashInt(idx, 0x9E3779B9u);
                    const bool    open      = (sOpenNode == nodeUid);
        
                    dl.AddLine({decoX, r.y + 2.f}, {decoX, r.y + tabH - 2.f}, sep, 1.f);
                    dl.AddRectFilled(ovR, (hov || open) ? col.buttonHover : col.bgHeader);
                    // Flèche bas (indique "voir plus d'onglets")
                    dl.AddArrow({ovR.x + ovR.w * 0.5f, ovR.y + ovR.h * 0.5f},
                                6.f, 1, col.textSecondary);
        
                    if (hov && ctx.ConsumeMouseClick(0))
                        sOpenNode = open ? NKUI_ID_NONE : nodeUid;
        
                    // Menu déroulant
                    if (open) {
                        NkUIDrawList& popDl = ctx.layers[NkUIContext::LAYER_POPUPS];
                        const float32 iH   = ctx.theme.metrics.itemHeight;
                        const float32 mH   = iH * node.numWindows + 8.f;
                        const float32 mW   = 200.f;
                        float32 mX = decoX + kOverflowW - mW;
                        if (mX < r.x) mX = r.x;
                        const NkRect menuR = {mX, r.y + tabH, mW, mH};
        
                        popDl.AddRectFilled(menuR, col.bgPopup, met.cornerRadius);
                        popDl.AddRect(menuR, col.border, 1.f, met.cornerRadius);
                        popDl.PushClipRect(menuR);
        
                        for (int32 t = 0; t < node.numWindows; ++t) {
                            const NkUIWindowState* ws = wm.Find(node.windows[t]);
                            if (!ws) continue;
                            const NkRect iR = {menuR.x + 4.f,
                                            menuR.y + 4.f + t * iH,
                                            menuR.w - 8.f, iH};
                            const bool iHov = NkRectContains(iR, mp);
                            const bool iSel = (t == node.activeTab);
        
                            if (iSel || iHov)
                                popDl.AddRectFilled(iR,
                                    iSel ? col.accent : col.buttonHover,
                                    met.cornerRadiusSm);
        
                            const float32 asc  = font.metrics.ascender > 0.f
                                                ? font.metrics.ascender : font.size*0.8f;
                            const float32 dsc  = font.metrics.descender >= 0.f
                                                ? font.metrics.descender
                                                : -font.metrics.descender;
                            const float32 lh   = font.metrics.lineHeight > 0.f
                                                ? font.metrics.lineHeight
                                                : asc + (dsc > 0.f ? dsc : font.size*0.2f);
                            font.RenderText(popDl,
                                {iR.x + 8.f, iR.y + (iH - lh) * 0.5f + asc},
                                ws->name,
                                (iSel || iHov) ? col.textOnAccent : col.textPrimary,
                                iR.w - 12.f, /*ellipsis=*/true);
        
                            if (iHov && ctx.ConsumeMouseClick(0)) {
                                node.activeTab = t;
                                wm.activeId    = node.windows[t];
                                sOpenNode      = NKUI_ID_NONE;
                            }
                        }
                        popDl.PopClipRect();
        
                        if (ctx.IsMouseClicked(0) &&
                            !NkRectContains(menuR, mp) &&
                            !NkRectContains(ovR, mp))
                        {
                            sOpenNode = NKUI_ID_NONE;
                        }
                    }
                }
            }
        
            // =========================================================================
            // ZONE VIDE = DRAG DE L'ONGLET ACTIF (undock)
            // =========================================================================
            {
                const NkRect emptyR = {r.x + totalFin, r.y,
                                    avail - totalFin, tabH};
                if (emptyR.w > 8.f &&
                    NkRectContains(emptyR, mp) &&
                    ctx.input.IsMouseDown(0) &&
                    wm.movingId == NKUI_ID_NONE &&
                    node.activeTab < node.numWindows)
                {
                    const float32 dx = ctx.input.mouseDelta.x;
                    const float32 dy = ctx.input.mouseDelta.y;
                    if (dx*dx + dy*dy > 25.f) {
                        NkUIWindowState* ws = wm.Find(node.windows[node.activeTab]);
                        if (ws) {
                            const float32 ww = ws->size.x > 10.f ? ws->size.x : 300.f;
                            UndockWindow(wm, ws->id);
                            ws->isOpen = true;
                            ws->pos    = {mp.x - ww * 0.5f, mp.y - 10.f};
                            wm.activeId   = ws->id;
                            wm.movingId   = ws->id;
                            wm.moveOffset = {ww * 0.5f, 10.f};
                            wm.BringToFront(ws->id);
                        }
                    }
                }
            }
        
            // =========================================================================
            // DRAG D'ONGLET → UNDOCK
            // =========================================================================
            if (isDraggingTab && dragWindowId != NKUI_ID_NONE) {
                if (ctx.input.IsMouseDown(0) && wm.movingId == NKUI_ID_NONE) {
                    const float32 dx = ctx.input.mouseDelta.x;
                    const float32 dy = ctx.input.mouseDelta.y;
                    if (dx*dx + dy*dy > 25.f) {
                        NkUIWindowState* ws = wm.Find(dragWindowId);
                        if (ws) {
                            const float32 ww = ws->size.x > 10.f ? ws->size.x : 300.f;
                            UndockWindow(wm, ws->id);
                            ws->isOpen = true;
                            ws->pos    = {mp.x - ww * 0.5f, mp.y - 10.f};
                            wm.activeId   = ws->id;
                            wm.movingId   = ws->id;
                            wm.moveOffset = {ww * 0.5f, 10.f};
                            wm.BringToFront(ws->id);
                            isDraggingTab  = false;
                            isDragging     = false;   // ← ajout
                            dragWindowId   = NKUI_ID_NONE;
                            dragSourceNode = -1;      // ← ajout
                        }
                    }
                }
                if (!ctx.input.mouseDown[0]) {
                    isDraggingTab  = false;
                    isDragging     = false;   // ← ajout
                    dragWindowId   = NKUI_ID_NONE;
                    dragSourceNode = -1;      // ← ajout
                }
            }
        
            // =========================================================================
            // CONTENU DE L'ONGLET ACTIF
            // =========================================================================

            for (int32 t = 0; t < node.numWindows; ++t) {
                NkUIWindowState* ws = wm.Find(node.windows[t]);
                if (!ws) continue;
                if (t == node.activeTab && ws->isOpen) {
                    const NkRect clientR = {r.x, r.y + tabH, r.w, r.h - tabH};
                    if (NkRectContains(clientR, mp) && ctx.IsMouseClicked(0))
                        wm.activeId = ws->id;
                    dl.PushClipRect(clientR, true);
                    dl.AddRectFilled(clientR, col.bgWindow);
                    dl.PopClipRect();
                    dl.AddRect({r.x, r.y, r.w, r.h}, col.separator.WithAlpha(120), 1.f);
                    ws->pos  = {r.x, r.y + tabH};
                    ws->size = {r.w, r.h - tabH};
                    ws->isActiveTab = true;   // ← marqué actif
                } else {
                    ws->isActiveTab = false;  // ← marqué inactif
                    // On garde pos/size intacts pour ne pas avoir {0,0}
                    // mais isActiveTab = false suffit à bloquer Begin
                }
            }
        }
        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginFrame
        // ─────────────────────────────────────────────────────────────────────────────
        void NkUIDockManager::BeginFrame(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, NkUIFont&) noexcept {
            if (!isDragging && wm.movingId != NKUI_ID_NONE) {
                NkUIWindowState* dws = wm.Find(wm.movingId);
                if (dws && !HasFlag(dws->flags, NkUIWindowFlags::NK_NO_DOCK)) {
                    isDragging     = true;
                    dragWindowId   = wm.movingId;
                    dragSourceNode = -1;
                }
            }

            if (!isDragging && isDraggingTab && dragWindowId != NKUI_ID_NONE) {
                isDragging = true;
            }

            if (isDragging && dragWindowId != NKUI_ID_NONE) {
                const int32 nodeUnder = FindNodeAt(ctx.input.mousePos);
                if (nodeUnder >= 0) {
                    dragTargetNode = nodeUnder;

                    const float32 tabH = ctx.theme.metrics.dockTabHeight;
                    const NkRect tabBarR = {
                        nodes[nodeUnder].rect.x,
                        nodes[nodeUnder].rect.y,
                        nodes[nodeUnder].rect.w,
                        tabH
                    };
                    const bool mouseOnTabBar = NkRectContains(tabBarR, ctx.input.mousePos);

                    // RenderNode gère TOUS les drops sur tabBar (réordre ET ajout d'onglet)
                    // BeginFrame ne gère que les drops hors tabBar
                    if (!mouseOnTabBar) {
                        NkUIDrawList& overlay = ctx.layers[NkUIContext::LAYER_OVERLAY];
                        overlay.AddRectFilled(nodes[nodeUnder].rect,
                                            ctx.theme.colors.dockZone.WithAlpha(35));
                        const NkUIDockDrop drop = DrawDropZones(ctx, overlay, nodeUnder);
                        if (!ctx.input.mouseDown[0] && drop != NkUIDockDrop::NK_NONE) {
                            DockWindow(wm, dragWindowId, nodeUnder, drop);
                            wm.movingId    = NKUI_ID_NONE;
                            isDragging     = false;
                            isDraggingTab  = false;
                            dragWindowId   = NKUI_ID_NONE;
                            dragSourceNode = -1;
                            dragTargetNode = -1;
                        }
                        // Si bouton relâché HORS tabBar et SANS drop valide → reset
                        if (!ctx.input.mouseDown[0] && drop == NkUIDockDrop::NK_NONE) {
                            isDragging     = false;
                            isDraggingTab  = false;
                            dragWindowId   = NKUI_ID_NONE;
                            dragSourceNode = -1;
                            dragTargetNode = -1;
                        }
                    }
                    // Si mouseOnTabBar : RenderNode gère tout (réordre + ajout onglet externe)
                    // Ne PAS resetter ici — laisser RenderNode consommer l'événement
                } else {
                    // Souris hors de tout nœud
                    if (!ctx.input.mouseDown[0]) {
                        isDragging     = false;
                        isDraggingTab  = false;
                        dragWindowId   = NKUI_ID_NONE;
                        dragTargetNode = -1;
                        dragSourceNode = -1;
                    }
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

/**
 * @File    NkUIDock.cpp — version corrigée
 *
 * FIX 1 — CONTENU ONGLET ACTIF UNIQUEMENT
 *   BeginDocked() retourne false si ws n'est pas l'onglet actif du node.
 *   RenderNode() met à jour pos/size de TOUTES les fenêtres du node (pour Begin()),
 *   mais dessine le fond client UNIQUEMENT pour l'onglet actif.
 *
 * FIX 2 — TABS STYLE NAVIGATEUR
 *   Largeur naturelle (texte + padding + boutonX), tabs collés à gauche,
 *   espace libre visible à droite → style Chrome/Firefox/ImGui.
 *   Troncature proportionnelle si débordement.
 *
 * FIX 3 — OVERFLOW ▾ CLIQUABLE
 *   Toggle open/close. Quand fermé → liste non rendue.
 *   Flèche bas si fermé, flèche haut si ouvert.
 *
 * FIX 4 — DOCK DANS LAYER_BG
 *   Flottantes (LAYER_WINDOWS) toujours visuellement au-dessus.
 */

// #include "NKUI/NkUIDock.h"
// #include "NKUI/NkUIWidgets.h"
// #include <cstring>
// #include <cmath>
// #include <algorithm>

// namespace nkentseu {
//     namespace nkui {

//         static float32 DockBaseY(const NkRect& r, const NkUIFont& f) noexcept {
//             const float32 asc  = f.metrics.ascender  > 0.f ? f.metrics.ascender  : f.size * 0.8f;
//             const float32 desc = f.metrics.descender >= 0.f ? f.metrics.descender : -f.metrics.descender;
//             const float32 lh   = f.metrics.lineHeight > 0.f ? f.metrics.lineHeight
//                                                              : asc + (desc > 0.f ? desc : f.size * 0.2f);
//             return r.y + (r.h - lh) * 0.5f + asc;
//         }

//         static void RebindWindowsRecursive(NkUIDockManager& dm, NkUIWindowManager& wm, int32 idx) noexcept {
//             if (idx < 0 || idx >= dm.numNodes) return;
//             NkUIDockNode& n = dm.nodes[idx];
//             if (n.type == NkUIDockNodeType::NK_LEAF || n.type == NkUIDockNodeType::NK_ROOT) {
//                 for (int32 i = 0; i < n.numWindows; ++i) {
//                     NkUIWindowState* ws = wm.Find(n.windows[i]);
//                     if (ws) { ws->isDocked = true; ws->dockNodeId = (NkUIID)idx; ws->zOrder = 0; }
//                 }
//                 return;
//             }
//             RebindWindowsRecursive(dm, wm, n.children[0]);
//             RebindWindowsRecursive(dm, wm, n.children[1]);
//         }

//         static void CollapseEmptyLeaf(NkUIDockManager& dm, NkUIWindowManager& wm, int32 leafIdx) noexcept {
//             int32 cur = leafIdx;
//             while (cur >= 0 && cur < dm.numNodes) {
//                 NkUIDockNode& c = dm.nodes[cur];
//                 if (c.type != NkUIDockNodeType::NK_LEAF || c.numWindows > 0) break;
//                 const int32 pi = c.parent;
//                 if (pi < 0 || pi >= dm.numNodes) { c.type = NkUIDockNodeType::NK_ROOT; c.children[0]=c.children[1]=-1; break; }
//                 NkUIDockNode& parent = dm.nodes[pi];
//                 const int32 sib = (parent.children[0]==cur) ? parent.children[1] : parent.children[0];
//                 if (sib < 0 || sib >= dm.numNodes) { parent.type=NkUIDockNodeType::NK_LEAF; parent.numWindows=0; parent.activeTab=0; parent.children[0]=parent.children[1]=-1; cur=pi; continue; }
//                 NkUIDockNode sc = dm.nodes[sib]; sc.parent=parent.parent; sc.rect=parent.rect;
//                 dm.nodes[pi] = sc;
//                 if (dm.nodes[pi].children[0]>=0) dm.nodes[dm.nodes[pi].children[0]].parent=pi;
//                 if (dm.nodes[pi].children[1]>=0) dm.nodes[dm.nodes[pi].children[1]].parent=pi;
//                 dm.nodes[cur]=NkUIDockNode{}; dm.nodes[sib]=NkUIDockNode{};
//                 if (cur==dm.rootIdx||sib==dm.rootIdx) dm.rootIdx=pi;
//                 RebindWindowsRecursive(dm,wm,pi); cur=pi;
//             }
//         }

//         void NkUIDockManager::Init(NkRect viewport) noexcept {
//             numNodes=0; rootIdx=-1; isDragging=false; isDraggingTab=false;
//             dragWindowId=NKUI_ID_NONE; dragTargetNode=-1; childWindowId=NKUI_ID_NONE;
//             numTabDecos=0; dockOverflowButton=true;
//             ::memset(nodes,0,sizeof(nodes));
//             rootIdx=AllocNode();
//             if(rootIdx>=0){nodes[rootIdx].type=NkUIDockNodeType::NK_ROOT; nodes[rootIdx].rect=viewport;}
//         }
//         void NkUIDockManager::Destroy() noexcept { numNodes=0; rootIdx=-1; }
//         void NkUIDockManager::SetViewport(NkRect r) noexcept { if(rootIdx>=0) nodes[rootIdx].rect=r; RecalcRectsAll(); }

//         int32 NkUIDockManager::AllocNode() noexcept {
//             if(numNodes>=MAX_NODES) return -1;
//             int32 idx=numNodes++;
//             ::memset((void*)&nodes[idx],0,sizeof(NkUIDockNode));
//             nodes[idx].splitRatio=0.5f; nodes[idx].parent=-1;
//             nodes[idx].children[0]=nodes[idx].children[1]=-1;
//             nodes[idx].parentWindowId=NKUI_ID_NONE; return idx;
//         }

//         void NkUIDockManager::RecalcRects(int32 idx) noexcept {
//             if(idx<0||idx>=numNodes) return;
//             NkUIDockNode& n=nodes[idx];
//             if(n.type==NkUIDockNodeType::NK_LEAF||n.type==NkUIDockNodeType::NK_ROOT) return;
//             int32 c0=n.children[0],c1=n.children[1]; if(c0<0||c1<0) return;
//             if(n.type==NkUIDockNodeType::NK_SPLIT_H){float32 w0=n.rect.w*n.splitRatio; nodes[c0].rect={n.rect.x,n.rect.y,w0,n.rect.h}; nodes[c1].rect={n.rect.x+w0,n.rect.y,n.rect.w-w0,n.rect.h};}
//             else{float32 h0=n.rect.h*n.splitRatio; nodes[c0].rect={n.rect.x,n.rect.y,n.rect.w,h0}; nodes[c1].rect={n.rect.x,n.rect.y+h0,n.rect.w,n.rect.h-h0};}
//             RecalcRects(c0); RecalcRects(c1);
//         }
//         void NkUIDockManager::RecalcRectsAll() noexcept { RecalcRects(rootIdx); }

//         bool NkUIDockManager::NodeAllowsTabs(const NkUIWindowManager& wm, int32 ni, NkUIID di) const noexcept {
//             if(ni<0||ni>=numNodes) return false;
//             const NkUIDockNode& node=nodes[ni];
//             for(int32 i=0;i<node.numWindows;++i){const NkUIWindowState* ws=const_cast<NkUIWindowManager&>(wm).Find(node.windows[i]); if(ws&&HasFlag(ws->flags,NkUIWindowFlags::NK_NO_TABS)) return false;}
//             if(di!=NKUI_ID_NONE){const NkUIWindowState* dws=const_cast<NkUIWindowManager&>(wm).Find(di); if(dws&&HasFlag(dws->flags,NkUIWindowFlags::NK_NO_TABS)) return false;}
//             return true;
//         }

//         bool NkUIDockManager::DockWindow(NkUIWindowManager& wm, NkUIID winId, int32 ni, NkUIDockDrop drop) noexcept {
//             if(ni<0||ni>=numNodes) return false;
//             NkUIWindowState* ws=wm.Find(winId); if(!ws) return false;
//             if(HasFlag(ws->flags,NkUIWindowFlags::NK_NO_DOCK)) return false;
//             if((drop==NkUIDockDrop::NK_CENTER||drop==NkUIDockDrop::NK_TAB)&&!NodeAllowsTabs(wm,ni,winId)) drop=NkUIDockDrop::NK_BOTTOM;
//             if(drop==NkUIDockDrop::NK_CENTER||drop==NkUIDockDrop::NK_TAB){
//                 if(ws->isDocked) UndockWindow(wm,winId);
//                 NkUIDockNode& node=nodes[ni];
//                 for(int32 i=0;i<node.numWindows;++i) if(node.windows[i]==winId){node.activeTab=i;return true;}
//                 if(node.numWindows>=NkUIDockNode::MAX_WINDOWS) return false;
//                 node.windows[node.numWindows++]=winId; node.activeTab=node.numWindows-1;
//                 node.type=NkUIDockNodeType::NK_LEAF; ws->isDocked=true; ws->dockNodeId=(NkUIID)ni; ws->zOrder=0;
//                 return true;
//             }
//             if(ws->isDocked) UndockWindow(wm,winId);
//             int32 c0=AllocNode(); if(c0<0) return false;
//             int32 c1=AllocNode(); if(c1<0) return false;
//             NkUIDockNode& target=nodes[ni];
//             NkUIID ow[NkUIDockNode::MAX_WINDOWS]={}; int32 oldN=target.numWindows,oldA=target.activeTab;
//             for(int32 i=0;i<oldN;++i) ow[i]=target.windows[i];
//             bool horiz=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_RIGHT);
//             target.type=horiz?NkUIDockNodeType::NK_SPLIT_H:NkUIDockNodeType::NK_SPLIT_V;
//             target.children[0]=c0; target.children[1]=c1; target.splitRatio=0.5f; target.numWindows=0; target.activeTab=0;
//             ::memset(target.windows,0,sizeof(target.windows));
//             nodes[c0]=NkUIDockNode{}; nodes[c0].type=NkUIDockNodeType::NK_LEAF; nodes[c0].parent=ni; nodes[c0].splitRatio=0.5f;
//             nodes[c1]=NkUIDockNode{}; nodes[c1].type=NkUIDockNodeType::NK_LEAF; nodes[c1].parent=ni; nodes[c1].splitRatio=0.5f;
//             int32 nc=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_TOP)?0:1, ec=1-nc;
//             NkUIDockNode& exN=nodes[ec==0?c0:c1]; NkUIDockNode& newN=nodes[nc==0?c0:c1];
//             for(int32 i=0;i<oldN&&exN.numWindows<NkUIDockNode::MAX_WINDOWS;++i){exN.windows[exN.numWindows++]=ow[i]; NkUIWindowState* m=wm.Find(ow[i]); if(m){m->isDocked=true;m->dockNodeId=(NkUIID)(ec==0?c0:c1);m->zOrder=0;}}
//             exN.activeTab=(oldN>0)?std::clamp(oldA,0,exN.numWindows-1):0;
//             newN.windows[0]=winId; newN.numWindows=1; newN.activeTab=0;
//             ws->isDocked=true; ws->dockNodeId=(NkUIID)(nc==0?c0:c1); ws->zOrder=0;
//             RecalcRects(ni); return true;
//         }

//         void NkUIDockManager::UndockWindow(NkUIWindowManager& wm, NkUIID winId) noexcept {
//             NkUIWindowState* ws=wm.Find(winId); if(!ws||!ws->isDocked) return;
//             int32 ni=(int32)ws->dockNodeId; if(ni<0||ni>=numNodes) return;
//             NkUIDockNode& node=nodes[ni];
//             for(int32 i=0;i<node.numWindows;++i) if(node.windows[i]==winId){
//                 for(int32 j=i;j<node.numWindows-1;++j) node.windows[j]=node.windows[j+1];
//                 node.windows[--node.numWindows]=0;
//                 if(node.activeTab>=node.numWindows&&node.activeTab>0) --node.activeTab;
//                 break;
//             }
//             ws->isDocked=false; ws->dockNodeId=0;
//             bool inZ=false; for(int32 i=0;i<wm.numZOrder;++i) if(wm.zOrder[i]==winId){inZ=true;break;}
//             if(!inZ&&wm.numZOrder<NkUIWindowManager::MAX_WINDOWS) wm.zOrder[wm.numZOrder++]=winId;
//             wm.BringToFront(winId);
//             if(wm.movingId==winId) wm.movingId=NKUI_ID_NONE;
//             if(wm.resizingId==winId) wm.resizingId=NKUI_ID_NONE;
//             if(node.numWindows==0) CollapseEmptyLeaf(*this,wm,ni);
//             RecalcRectsAll();
//         }

//         int32 NkUIDockManager::FindNodeAt(NkVec2 pos) const noexcept {
//             for(int32 i=0;i<numNodes;++i) if(nodes[i].type==NkUIDockNodeType::NK_LEAF&&NkRectContains(nodes[i].rect,pos)) return i;
//             if(rootIdx>=0&&NkRectContains(nodes[rootIdx].rect,pos)) return rootIdx;
//             return -1;
//         }

//         void NkUIDockManager::DrawDirectionalHighlight(NkUIDrawList& dl, int32 ni, NkUIDockDrop drop, NkColor col) const noexcept {
//             if(ni<0||ni>=numNodes) return;
//             const NkRect& r=nodes[ni].rect; const float32 th=3.f;
//             switch(drop){
//                 case NkUIDockDrop::NK_LEFT:   dl.AddLine({r.x,r.y},{r.x,r.y+r.h},col,th); break;
//                 case NkUIDockDrop::NK_RIGHT:  dl.AddLine({r.x+r.w,r.y},{r.x+r.w,r.y+r.h},col,th); break;
//                 case NkUIDockDrop::NK_TOP:    dl.AddLine({r.x,r.y},{r.x+r.w,r.y},col,th); break;
//                 case NkUIDockDrop::NK_BOTTOM: dl.AddLine({r.x,r.y+r.h},{r.x+r.w,r.y+r.h},col,th); break;
//                 default: break;
//             }
//         }

//         NkUIDockDrop NkUIDockManager::DrawDropZones(NkUIContext& ctx, NkUIDrawList& dl, int32 ni) noexcept {
//             if(ni<0||ni>=numNodes) return NkUIDockDrop::NK_NONE;
//             const NkRect& r=nodes[ni].rect;
//             const float32 sz=ctx.theme.metrics.dockZoneSize;
//             const NkColor zBg=ctx.theme.colors.dockZone, zBd=ctx.theme.colors.dockZoneBorder;
//             const NkColor hl={100,180,255,220}; const float32 rx=6.f; const NkVec2 mp=ctx.input.mousePos;
//             const bool centerOk=ctx.wm?NodeAllowsTabs(*ctx.wm,ni,dragWindowId):true;
//             struct Z{NkUIDockDrop drop;NkRect rect;bool en;};
//             const Z zones[]={
//                 {NkUIDockDrop::NK_LEFT,  {r.x+r.w*.1f,r.y+r.h*.5f-sz*.5f,sz,sz},true},
//                 {NkUIDockDrop::NK_RIGHT, {r.x+r.w*.9f-sz,r.y+r.h*.5f-sz*.5f,sz,sz},true},
//                 {NkUIDockDrop::NK_TOP,   {r.x+r.w*.5f-sz*.5f,r.y+r.h*.1f,sz,sz},true},
//                 {NkUIDockDrop::NK_BOTTOM,{r.x+r.w*.5f-sz*.5f,r.y+r.h*.9f-sz,sz,sz},true},
//                 {NkUIDockDrop::NK_CENTER,{r.x+r.w*.5f-sz*.5f,r.y+r.h*.5f-sz*.5f,sz,sz},centerOk},
//             };
//             NkUIDockDrop hov=NkUIDockDrop::NK_NONE;
//             for(const auto& z:zones) if(z.en&&NkRectContains(z.rect,mp)){hov=z.drop;break;}
//             if(hov==NkUIDockDrop::NK_NONE&&NkRectContains(r,mp)){
//                 const float32 band=std::clamp((r.w<r.h?r.w:r.h)*.1f,sz*.4f,sz*.9f);
//                 float32 dL=mp.x-r.x,dR=r.x+r.w-mp.x,dT=mp.y-r.y,dB=r.y+r.h-mp.y,dMin=dL; hov=NkUIDockDrop::NK_LEFT;
//                 if(dR<dMin){dMin=dR;hov=NkUIDockDrop::NK_RIGHT;} if(dT<dMin){dMin=dT;hov=NkUIDockDrop::NK_TOP;} if(dB<dMin){dMin=dB;hov=NkUIDockDrop::NK_BOTTOM;}
//                 if(dMin>band) hov=NkUIDockDrop::NK_NONE;
//             }
//             if(hov!=NkUIDockDrop::NK_NONE){
//                 NkRect prev={};
//                 switch(hov){case NkUIDockDrop::NK_LEFT:prev={r.x,r.y,r.w*.4f,r.h};break;case NkUIDockDrop::NK_RIGHT:prev={r.x+r.w*.6f,r.y,r.w*.4f,r.h};break;case NkUIDockDrop::NK_TOP:prev={r.x,r.y,r.w,r.h*.4f};break;case NkUIDockDrop::NK_BOTTOM:prev={r.x,r.y+r.h*.6f,r.w,r.h*.4f};break;case NkUIDockDrop::NK_CENTER:prev={r.x+r.w*.2f,r.y+r.h*.2f,r.w*.6f,r.h*.6f};break;default:break;}
//                 dl.AddRectFilled(prev,zBd.WithAlpha(60),rx); dl.AddRect(prev,zBd.WithAlpha(200),2.f,rx);
//                 if(hov!=NkUIDockDrop::NK_CENTER) DrawDirectionalHighlight(dl,ni,hov,hl);
//             }
//             for(const auto& z:zones){
//                 if(!z.en) continue; const bool hh=(hov==z.drop);
//                 dl.AddRectFilled(z.rect,hh?zBd.WithAlpha(180):zBg,rx); dl.AddRect(z.rect,zBd,2.f,rx);
//                 const NkVec2 c={z.rect.x+z.rect.w*.5f,z.rect.y+z.rect.h*.5f};
//                 switch(z.drop){case NkUIDockDrop::NK_LEFT:dl.AddArrow(c,sz*.3f,2,zBd);break;case NkUIDockDrop::NK_RIGHT:dl.AddArrow(c,sz*.3f,0,zBd);break;case NkUIDockDrop::NK_TOP:dl.AddArrow(c,sz*.3f,3,zBd);break;case NkUIDockDrop::NK_BOTTOM:dl.AddArrow(c,sz*.3f,1,zBd);break;case NkUIDockDrop::NK_CENTER:dl.AddRectFilled({c.x-sz*.2f,c.y-sz*.2f,sz*.4f,sz*.4f},zBd,3.f);break;default:break;}
//             }
//             return hov;
//         }

//         bool NkUIDockManager::AddTabDecoration(NkTabDecoFn fn, void* ud, float32 w, NkUIID id) noexcept {
//             if(!fn||numTabDecos>=MAX_TAB_DECOS) return false;
//             NkTabDecoration& d=tabDecos[numTabDecos++]; d.fn=fn; d.userData=ud; d.width=w; d.enabled=true;
//             d.id=id?id:NkHashPtr(fn,(NkUIID)numTabDecos); return true;
//         }
//         void NkUIDockManager::RemoveTabDecoration(NkUIID id) noexcept {
//             for(int32 i=0;i<numTabDecos;++i) if(tabDecos[i].id==id){for(int32 j=i;j<numTabDecos-1;++j) tabDecos[j]=tabDecos[j+1];--numTabDecos;return;}
//         }
//         void NkUIDockManager::EnableTabDecoration(NkUIID id, bool en) noexcept {
//             for(int32 i=0;i<numTabDecos;++i) if(tabDecos[i].id==id){tabDecos[i].enabled=en;return;}
//         }
//         void NkUIDockManager::SetDockOverflowButton(bool show) noexcept { dockOverflowButton=show; }

//         // ─────────────────────────────────────────────────────────────────────────────
//         //  RenderNode — 4 corrections intégrées
//         // ─────────────────────────────────────────────────────────────────────────────

//         void NkUIDockManager::RenderNode(NkUIContext& ctx, NkUIDrawList&, NkUIFont& font,
//                                           NkUIWindowManager& wm, int32 idx, NkUILayoutStack& ls) noexcept
//         {
//             if(idx<0||idx>=numNodes) return;
//             NkUIDockNode& node=nodes[idx];

//             if(node.type==NkUIDockNodeType::NK_SPLIT_H||node.type==NkUIDockNodeType::NK_SPLIT_V){
//                 NkUIDrawList& bgDl=ctx.layers[NkUIContext::LAYER_BG];
//                 bool vert=(node.type==NkUIDockNodeType::NK_SPLIT_H);
//                 NkUILayout::DrawSplitter(ctx,bgDl,node.rect,vert,node.splitRatio,NkHashInt(idx,0xDC));
//                 node.splitRatio=std::clamp(node.splitRatio,0.05f,0.95f); RecalcRects(idx);
//                 const NkColor lc=ctx.theme.colors.separator.WithAlpha(200);
//                 if(node.children[0]>=0){const NkRect& a=nodes[node.children[0]].rect; if(vert) bgDl.AddLine({a.x+a.w,node.rect.y},{a.x+a.w,node.rect.y+node.rect.h},lc,1.f); else bgDl.AddLine({node.rect.x,a.y+a.h},{node.rect.x+node.rect.w,a.y+a.h},lc,1.f);}
//                 RenderNode(ctx,bgDl,font,wm,node.children[0],ls); RenderNode(ctx,bgDl,font,wm,node.children[1],ls); return;
//             }
//             if((node.type!=NkUIDockNodeType::NK_LEAF&&node.type!=NkUIDockNodeType::NK_ROOT)||node.numWindows<=0) return;

//             // FIX 4 : LAYER_BG — flottantes toujours au-dessus
//             NkUIDrawList& dl=ctx.layers[NkUIContext::LAYER_BG];
//             const NkRect& r=node.rect;
//             const float32 tabH=ctx.theme.metrics.dockTabHeight;
//             const auto& col=ctx.theme.colors; const auto& met=ctx.theme.metrics;
//             const NkColor sep=col.separator.WithAlpha(200);
//             const NkVec2 mp=ctx.input.mousePos;

//             dl.AddRectFilled({r.x,r.y+tabH,r.w,r.h-tabH},col.bgWindow);

//             // Espace décorations
//             const float32 kOverflowW=dockOverflowButton?22.f:0.f;
//             float32 decoTW=kOverflowW;
//             for(int32 i=0;i<numTabDecos;++i) if(tabDecos[i].enabled&&tabDecos[i].fn) decoTW+=tabDecos[i].width;
//             const float32 avail=(r.w-decoTW)>1.f?(r.w-decoTW):1.f;

//             // FIX 2 : largeurs naturelles
//             constexpr float32 kPX=10.f,kCW=18.f,kMin=40.f;
//             float32 natW[NkUIDockNode::MAX_WINDOWS]={},finW[NkUIDockNode::MAX_WINDOWS]={},totNat=0.f;
//             for(int32 t=0;t<node.numWindows;++t){const NkUIWindowState* ws=wm.Find(node.windows[t]); natW[t]=(ws?font.MeasureWidth(ws->name):40.f)+kPX*2.f+kCW; totNat+=natW[t];}
//             if(totNat<=avail){for(int32 t=0;t<node.numWindows;++t) finW[t]=natW[t];}
//             else{float32 ratio=avail/totNat; for(int32 t=0;t<node.numWindows;++t) finW[t]=(natW[t]*ratio<kMin)?kMin:natW[t]*ratio;}
//             float32 totFin=0.f; for(int32 t=0;t<node.numWindows;++t) totFin+=finW[t];

//             // Drop sur barre d'onglets
//             const NkRect tbR={r.x,r.y,avail,tabH};
//             bool tabDrop=false; int32 ins=node.numWindows;
//             if(isDragging&&dragWindowId!=NKUI_ID_NONE&&NkRectContains(tbR,mp)){
//                 tabDrop=true; float32 tx=r.x;
//                 for(int32 t=0;t<node.numWindows;++t){if(mp.x<tx+finW[t]*.5f){ins=t;break;} if(mp.x<tx+finW[t]){ins=t+1;break;} tx+=finW[t];}
//             }

//             // Clics avant rendu
//             if(!tabDrop){
//                 float32 tx=r.x;
//                 for(int32 t=0;t<node.numWindows;++t){
//                     const NkRect tabR={tx,r.y,finW[t],tabH};
//                     const NkRect clR={tx+finW[t]-kCW,r.y+(tabH-14.f)*.5f,14.f,14.f};
//                     if(NkRectContains(clR,mp)&&ctx.ConsumeMouseClick(0)){NkUIWindowState* ws=wm.Find(node.windows[t]); UndockWindow(wm,node.windows[t]); if(ws)ws->isOpen=false; break;}
//                     if(NkRectContains(tabR,mp)&&!NkRectContains(clR,mp)&&ctx.ConsumeMouseClick(0)){node.activeTab=t; wm.activeId=node.windows[t]; isDraggingTab=true; dragWindowId=node.windows[t];}
//                     tx+=finW[t];
//                 }
//             }
//             if(node.numWindows>0&&node.activeTab>=node.numWindows) node.activeTab=node.numWindows-1;

//             // Rendu tabs
//             dl.AddRectFilled({r.x,r.y,r.w,tabH},col.bgHeader);
//             {
//                 float32 tx=r.x;
//                 for(int32 t=0;t<node.numWindows;++t){
//                     const float32 tw=finW[t]; const NkRect tabR={tx,r.y,tw,tabH};
//                     const bool act=(t==node.activeTab), hov=!tabDrop&&NkRectContains(tabR,mp);
//                     const NkRect clR={tx+tw-kCW,r.y+(tabH-14.f)*.5f,14.f,14.f}; const bool clH=hov&&NkRectContains(clR,mp);
//                     const NkUIWindowState* ws=wm.Find(node.windows[t]);
//                     // Fond tab
//                     if(act) dl.AddRectFilledCorners(tabR,col.tabActive,met.cornerRadiusSm,0x3);
//                     else{dl.AddRectFilled(tabR,hov?col.buttonHover:col.tabBg); dl.AddLine({tx,r.y+tabH-1.f},{tx+tw,r.y+tabH-1.f},sep,1.f);}
//                     if(t>0) dl.AddLine({tx,r.y+4.f},{tx,r.y+tabH-4.f},sep,1.f);
//                     // Texte
//                     if(ws) font.RenderText(dl,{tx+kPX,DockBaseY(tabR,font)},ws->name,act?col.tabActiveText:col.tabText,tw-kPX*2.f-kCW,true);
//                     // X
//                     if(hov||act){const NkVec2 cx={clR.x+clR.w*.5f,clR.y+clR.h*.5f}; const float32 d=3.5f; const NkColor xc=clH?NkColor{220,60,60,255}:col.tabText; if(clH)dl.AddCircleFilled(cx,7.f,{200,50,50,180}); dl.AddLine({cx.x-d,cx.y-d},{cx.x+d,cx.y+d},xc,1.5f); dl.AddLine({cx.x+d,cx.y-d},{cx.x-d,cx.y+d},xc,1.5f);}
//                     // Indicateur drop
//                     if(tabDrop&&ins==t){dl.AddLine({tx-1.5f,r.y+2.f},{tx-1.5f,r.y+tabH-2.f},{80,180,255,255},3.f); dl.AddTriangleFilled({tx-5.f,r.y+2.f},{tx+5.f,r.y+2.f},{tx,r.y+9.f},{80,180,255,220});}
//                     if(tabDrop&&t==node.numWindows-1&&ins==node.numWindows){float32 ix=tx+tw-1.5f; dl.AddLine({ix,r.y+2.f},{ix,r.y+tabH-2.f},{80,180,255,255},3.f); dl.AddTriangleFilled({ix-5.f,r.y+2.f},{ix+5.f,r.y+2.f},{ix,r.y+9.f},{80,180,255,220});}
//                     tx+=finW[t];
//                 }
//                 // FIX 2 : espace libre visible à droite des tabs
//                 if(totFin<avail) dl.AddLine({r.x+totFin,r.y+tabH-1.f},{r.x+avail,r.y+tabH-1.f},sep,1.f);
//             }

//             // Drop effectif sur barre d'onglets
//             if(tabDrop&&!ctx.input.mouseDown[0]){
//                 int32 prevN=node.numWindows;
//                 if(DockWindow(wm,dragWindowId,idx,NkUIDockDrop::NK_CENTER)){
//                     if(node.numWindows>prevN&&ins<prevN){NkUIID nw=node.windows[node.numWindows-1]; for(int32 i=node.numWindows-1;i>ins;--i)node.windows[i]=node.windows[i-1]; node.windows[ins]=nw; node.activeTab=ins;}
//                 }
//                 wm.movingId=NKUI_ID_NONE; isDragging=false; dragWindowId=NKUI_ID_NONE; dragTargetNode=-1;
//             }

//             // FIX 3 : DÉCORATIONS — overflow cliquable (toggle)
//             {
//                 float32 decoX=r.x+r.w;
//                 if(dockOverflowButton){
//                     decoX-=kOverflowW;
//                     const NkRect ovR={decoX,r.y,kOverflowW,tabH};
//                     const bool hov=NkRectContains(ovR,mp);
//                     static NkUIID sOpenNode=NKUI_ID_NONE;
//                     const NkUIID nodeUid=NkHashInt(idx,0x9E3779B9u);
//                     const bool open=(sOpenNode==nodeUid);
//                     dl.AddLine({decoX,r.y+2.f},{decoX,r.y+tabH-2.f},sep,1.f);
//                     dl.AddRectFilled(ovR,(hov||open)?col.buttonHover:col.bgHeader);
//                     // Flèche : bas=fermé, haut=ouvert
//                     dl.AddArrow({ovR.x+ovR.w*.5f,ovR.y+ovR.h*.5f},6.f,open?3:1,col.textSecondary);
//                     if(hov&&ctx.ConsumeMouseClick(0)) sOpenNode=open?NKUI_ID_NONE:nodeUid;
//                     // FIX 3 : liste rendue SEULEMENT si open
//                     if(open){
//                         NkUIDrawList& popDl=ctx.layers[NkUIContext::LAYER_POPUPS];
//                         const float32 iH=ctx.theme.metrics.itemHeight,mH=iH*node.numWindows+8.f,mW=200.f;
//                         float32 mX=decoX+kOverflowW-mW; if(mX<r.x)mX=r.x;
//                         const NkRect menuR={mX,r.y+tabH,mW,mH};
//                         popDl.AddRectFilled(menuR,col.bgPopup,met.cornerRadius); popDl.AddRect(menuR,col.border,1.f,met.cornerRadius); popDl.PushClipRect(menuR);
//                         for(int32 t=0;t<node.numWindows;++t){
//                             const NkUIWindowState* ws=wm.Find(node.windows[t]); if(!ws)continue;
//                             const NkRect iR={menuR.x+4.f,menuR.y+4.f+t*iH,menuR.w-8.f,iH};
//                             const bool iH2=NkRectContains(iR,mp),iSel=(t==node.activeTab);
//                             if(iSel||iH2)popDl.AddRectFilled(iR,iSel?col.accent:col.buttonHover,met.cornerRadiusSm);
//                             const float32 asc=font.metrics.ascender>0.f?font.metrics.ascender:font.size*.8f;
//                             const float32 dsc=font.metrics.descender>=0.f?font.metrics.descender:-font.metrics.descender;
//                             const float32 lh=font.metrics.lineHeight>0.f?font.metrics.lineHeight:asc+(dsc>0.f?dsc:font.size*.2f);
//                             font.RenderText(popDl,{iR.x+8.f,iR.y+(iH-lh)*.5f+asc},ws->name,(iSel||iH2)?col.textOnAccent:col.textPrimary,iR.w-12.f,true);
//                             if(iH2&&ctx.ConsumeMouseClick(0)){node.activeTab=t;wm.activeId=node.windows[t];sOpenNode=NKUI_ID_NONE;}
//                         }
//                         popDl.PopClipRect();
//                         if(ctx.IsMouseClicked(0)&&!NkRectContains(menuR,mp)&&!NkRectContains(ovR,mp)) sOpenNode=NKUI_ID_NONE;
//                     }
//                 }
//                 for(int32 i=numTabDecos-1;i>=0;--i){NkTabDecoration& d=tabDecos[i]; if(!d.enabled||!d.fn)continue; decoX-=d.width; const NkRect dR={decoX,r.y,d.width,tabH}; dl.AddLine({decoX,r.y+2.f},{decoX,r.y+tabH-2.f},sep,1.f); d.fn(ctx,dl,font,idx,dR,d.userData);}
//             }

//             // Zone vide = drag onglet actif
//             {
//                 const NkRect emR={r.x+totFin,r.y,avail-totFin,tabH};
//                 if(emR.w>8.f&&NkRectContains(emR,mp)&&ctx.input.IsMouseDown(0)&&wm.movingId==NKUI_ID_NONE&&node.activeTab<node.numWindows){
//                     float32 dx=ctx.input.mouseDelta.x,dy=ctx.input.mouseDelta.y;
//                     if(dx*dx+dy*dy>25.f){NkUIWindowState* ws=wm.Find(node.windows[node.activeTab]); if(ws){float32 ww=ws->size.x>10.f?ws->size.x:300.f; UndockWindow(wm,ws->id); ws->isOpen=true; ws->pos={mp.x-ww*.5f,mp.y-10.f}; wm.activeId=ws->id;wm.movingId=ws->id;wm.moveOffset={ww*.5f,10.f};wm.BringToFront(ws->id);}}
//                 }
//             }

//             // Drag tab → undock
//             if(isDraggingTab&&dragWindowId!=NKUI_ID_NONE){
//                 if(ctx.input.IsMouseDown(0)&&wm.movingId==NKUI_ID_NONE){float32 dx=ctx.input.mouseDelta.x,dy=ctx.input.mouseDelta.y; if(dx*dx+dy*dy>25.f){NkUIWindowState* ws=wm.Find(dragWindowId); if(ws){float32 ww=ws->size.x>10.f?ws->size.x:300.f; UndockWindow(wm,ws->id); ws->isOpen=true; ws->pos={mp.x-ww*.5f,mp.y-10.f}; wm.activeId=ws->id;wm.movingId=ws->id;wm.moveOffset={ww*.5f,10.f};wm.BringToFront(ws->id);isDraggingTab=false;dragWindowId=NKUI_ID_NONE;}}}
//                 if(!ctx.input.mouseDown[0]){isDraggingTab=false;dragWindowId=NKUI_ID_NONE;}
//             }

//             // FIX 1 : CONTENU ONGLET ACTIF UNIQUEMENT
//             // Met à jour pos/size de toutes les fenêtres du node (pour Begin()),
//             // mais clip+fond uniquement pour l'onglet actif.
//             if(node.activeTab>=0&&node.activeTab<node.numWindows){
//                 const NkRect clientR={r.x,r.y+tabH,r.w,r.h-tabH};
//                 // Met à jour pos/size de TOUTES les fenêtres (Begin() en a besoin)
//                 for(int32 t=0;t<node.numWindows;++t){NkUIWindowState* ws=wm.Find(node.windows[t]); if(ws){ws->pos={r.x,r.y+tabH};ws->size={r.w,r.h-tabH};}}
//                 // Dessine le fond UNIQUEMENT pour la fenêtre active
//                 NkUIWindowState* aws=wm.Find(node.windows[node.activeTab]);
//                 if(aws&&aws->isOpen){
//                     if(NkRectContains(clientR,mp)&&ctx.IsMouseClicked(0)) wm.activeId=aws->id;
//                     dl.PushClipRect(clientR,true); dl.AddRectFilled(clientR,col.bgWindow); dl.PopClipRect();
//                     dl.AddRect({r.x,r.y,r.w,r.h},col.separator.WithAlpha(120),1.f);
//                 }
//             }
//         }

//         void NkUIDockManager::BeginFrame(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, NkUIFont&) noexcept {
//             if(!isDragging&&wm.movingId!=NKUI_ID_NONE){NkUIWindowState* dws=wm.Find(wm.movingId); if(dws&&!HasFlag(dws->flags,NkUIWindowFlags::NK_NO_DOCK)){isDragging=true;dragWindowId=wm.movingId;}}
//             if(isDragging&&dragWindowId!=NKUI_ID_NONE){
//                 int32 nu=FindNodeAt(ctx.input.mousePos);
//                 if(nu>=0){
//                     dragTargetNode=nu;
//                     const NkRect tbR={nodes[nu].rect.x,nodes[nu].rect.y,nodes[nu].rect.w,ctx.theme.metrics.dockTabHeight};
//                     if(!NkRectContains(tbR,ctx.input.mousePos)){
//                         NkUIDrawList& ov=ctx.layers[NkUIContext::LAYER_OVERLAY];
//                         ov.AddRectFilled(nodes[nu].rect,ctx.theme.colors.dockZone.WithAlpha(35));
//                         NkUIDockDrop drop=DrawDropZones(ctx,ov,nu);
//                         if(!ctx.input.mouseDown[0]&&drop!=NkUIDockDrop::NK_NONE){DockWindow(wm,dragWindowId,nu,drop);wm.movingId=NKUI_ID_NONE;isDragging=false;dragWindowId=NKUI_ID_NONE;dragTargetNode=-1;}
//                     }
//                 }
//                 if(!ctx.input.mouseDown[0]){isDragging=false;dragWindowId=NKUI_ID_NONE;dragTargetNode=-1;}
//             }
//         }

//         void NkUIDockManager::Render(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, NkUIWindowManager& wm, NkUILayoutStack& ls, int32 rootOverride) noexcept {
//             int32 root=(rootOverride>=0&&rootOverride<numNodes)?rootOverride:rootIdx;
//             if(root>=0) RenderNode(ctx,dl,font,wm,root,ls);
//         }

//         // FIX 1 : BeginDocked retourne false si ws n'est pas l'onglet actif
//         bool NkUIDockManager::BeginDocked(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, const char* name) noexcept {
//             NkUIWindowState* ws=wm.Find(name); if(!ws||!ws->isDocked||!ws->isOpen) return false;
//             int32 ni=(int32)ws->dockNodeId; if(ni<0||ni>=numNodes) return false;
//             const NkUIDockNode& node=nodes[ni];
//             if(node.activeTab<0||node.activeTab>=node.numWindows) return false;
//             // GARDE : seule la fenêtre de l'onglet actif retourne true
//             if(node.windows[node.activeTab]!=ws->id) return false;
//             const NkRect cr={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
//             const NkVec2 start={cr.x+ctx.theme.metrics.windowPadX,cr.y+ctx.theme.metrics.windowPadY};
//             ctx.SetCursor(start); ctx.cursorStart=start; dl.PushClipRect(cr,true); return true;
//         }
//         void NkUIDockManager::EndDocked(NkUIContext&, NkUIDrawList& dl) noexcept { dl.PopClipRect(); }

//         bool NkUIDockManager::BeginChildDock(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, NkUIFont& font, NkUILayoutStack& ls, NkUIID pwId, NkRect cv) noexcept {
//             NkUIWindowState* pw=wm.Find(pwId); if(!pw) return false;
//             if(!HasFlag(pw->flags,NkUIWindowFlags::NK_ALLOW_DOCK_CHILD)) return false;
//             if(pw->childDockRoot<0||pw->childDockRoot>=numNodes){pw->childDockRoot=AllocNode(); if(pw->childDockRoot<0)return false; nodes[pw->childDockRoot].type=NkUIDockNodeType::NK_ROOT;nodes[pw->childDockRoot].rect=cv;nodes[pw->childDockRoot].parent=-1;nodes[pw->childDockRoot].parentWindowId=pwId;}
//             else{nodes[pw->childDockRoot].rect=cv;RecalcRects(pw->childDockRoot);}
//             childWindowId=pwId; dl.PushClipRect(cv,true); Render(ctx,dl,font,wm,ls,pw->childDockRoot); return true;
//         }
//         void NkUIDockManager::EndChildDock(NkUIContext&, NkUIDrawList& dl) noexcept { childWindowId=NKUI_ID_NONE; dl.PopClipRect(); }

//         void NkUIDockManager::SaveLayout(const char* path) const noexcept {
//             FILE* f=::fopen(path,"wb"); if(!f)return;
//             ::fprintf(f,"{\"numNodes\":%d,\"rootIdx\":%d,\"nodes\":[\n",numNodes,rootIdx);
//             for(int32 i=0;i<numNodes;++i){const NkUIDockNode& n=nodes[i]; ::fprintf(f,"{\"type\":%d,\"rect\":[%.1f,%.1f,%.1f,%.1f],\"ratio\":%.4f,\"parent\":%d,\"c0\":%d,\"c1\":%d,\"nw\":%d,\"at\":%d}%s\n",(int32)n.type,n.rect.x,n.rect.y,n.rect.w,n.rect.h,n.splitRatio,n.parent,n.children[0],n.children[1],n.numWindows,n.activeTab,i<numNodes-1?",":"");}
//             ::fprintf(f,"]\n}\n"); ::fclose(f);
//         }
//         bool NkUIDockManager::LoadLayout(const char* path) noexcept {
//             FILE* f=::fopen(path,"rb"); if(!f)return false;
//             char buf[65536]={}; ::fread(buf,1,sizeof(buf)-1,f); ::fclose(f);
//             int32 rN=0,rI=-1; if(::sscanf(buf,"{\"numNodes\":%d,\"rootIdx\":%d",&rN,&rI)!=2)return false;
//             if(rN<=0||rN>MAX_NODES)return false;
//             ::memset(nodes,0,sizeof(nodes)); numNodes=rN;rootIdx=rI;
//             const char* p=::strstr(buf,"\"nodes\":["); if(!p)return false; p+=9;
//             for(int32 i=0;i<numNodes;++i){
//                 p=::strstr(p,"{\"type\":"); if(!p)break;
//                 int32 tp=0,par=-1,c0=-1,c1=-1,nw=0,at=0; float32 rx=0,ry=0,rw=0,rh=0,ratio=0.5f;
//                 ::sscanf(p,"{\"type\":%d,\"rect\":[%f,%f,%f,%f],\"ratio\":%f,\"parent\":%d,\"c0\":%d,\"c1\":%d,\"nw\":%d,\"at\":%d}",&tp,&rx,&ry,&rw,&rh,&ratio,&par,&c0,&c1,&nw,&at);
//                 nodes[i].type=(NkUIDockNodeType)tp;nodes[i].rect={rx,ry,rw,rh};nodes[i].splitRatio=ratio;nodes[i].parent=par;nodes[i].children[0]=c0;nodes[i].children[1]=c1;nodes[i].numWindows=0;nodes[i].activeTab=at; ++p;
//             }
//             return true;
//         }

//     } // namespace nkui
// } // namespace nkentseu


// Exemple d'utilisation (côté app) :
//
//   // Ajouter un bouton "+" pour créer un nouvel onglet
//   dock.AddTabDecoration([](NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font,
//                            int32 nodeIdx, NkRect r, void* ud) -> float32 {
//       bool hov = NkRectContains(r, ctx.input.mousePos);
//       dl.AddRectFilled(r, hov ? ctx.theme.colors.buttonHover
//                               : ctx.theme.colors.bgHeader);
//       font.RenderText(dl, {r.x + 6.f, r.y + r.h * 0.5f}, "+",
//                       ctx.theme.colors.textPrimary);
//       if (hov && ctx.ConsumeMouseClick(0)) {
//           // ... créer une nouvelle fenêtre et la docker
//       }
//       return r.w;
//   }, nullptr, 24.f, NkHash("btn_new_tab"));
//
//   // Désactiver le bouton overflow
//   dock.SetOverflowButton(false);