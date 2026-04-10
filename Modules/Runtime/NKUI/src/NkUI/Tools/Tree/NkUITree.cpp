/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Generic tree widget implementation.
 * Main data: Recursive draw, virtual scroll, inline rename, DnD reparent.
 * Change this file when: Tree interaction or rendering behavior changes.
 */
#include "NKUI/Tools/Tree/NkUITree.h"
#include "NKUI/NkUIMath.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace nkui {

        // =========================================================================
        //  NkUITreeState helpers
        // =========================================================================

        bool NkUITreeState::IsOpen(NkUITreeNodeID id) const noexcept {
            for (int32 i = 0; i < numOpen; ++i)
                if (openNodes[i] == id) return true;
            return false;
        }

        void NkUITreeState::SetOpen(NkUITreeNodeID id, bool open) noexcept {
            for (int32 i = 0; i < numOpen; ++i) {
                if (openNodes[i] == id) {
                    if (!open) {
                        openNodes[i] = openNodes[--numOpen];
                    }
                    return;
                }
            }
            if (open && numOpen < MAX_OPEN) {
                openNodes[numOpen++] = id;
            }
        }

        bool NkUITreeState::IsSelected(NkUITreeNodeID id) const noexcept {
            for (int32 i = 0; i < numSelected; ++i)
                if (selected[i] == id) return true;
            return false;
        }

        void NkUITreeState::SetSelected(NkUITreeNodeID id, bool sel,
                                        bool clearOthers) noexcept {
            if (clearOthers) {
                numSelected = 0;
            }
            if (sel) {
                if (!IsSelected(id) && numSelected < MAX_SELECTED) {
                    selected[numSelected++] = id;
                }
            } else {
                for (int32 i = 0; i < numSelected; ++i) {
                    if (selected[i] == id) {
                        selected[i] = selected[--numSelected];
                        return;
                    }
                }
            }
        }

        void NkUITreeState::ClearSelection() noexcept {
            numSelected = 0;
        }

        // =========================================================================
        //  Internal constants
        // =========================================================================

        static constexpr float32 kScrollbarW   = 8.f;
        static constexpr float32 kArrowW       = 12.f;
        static constexpr float32 kDndGhostAlpha = 160;
        static constexpr float32 kConnectorOffX = 6.f; // x offset for connector dot

        // =========================================================================
        //  NkUITree::DrawNodeRow
        // =========================================================================

        static void SafeGetLabel(const NkUITreeCallbacks& cb,
                                 NkUITreeNodeID node,
                                 char* buf, int32 len) noexcept {
            if (cb.getLabel) {
                cb.getLabel(node, buf, len, cb.userData);
            } else {
                buf[0] = '\0';
            }
        }

        static void SafeGetIcon(const NkUITreeCallbacks& cb,
                                NkUITreeNodeID node,
                                char* buf, int32 len) noexcept {
            if (cb.getIcon) {
                cb.getIcon(node, buf, len, cb.userData);
            } else {
                buf[0] = '\0';
            }
        }

        void NkUITree::DrawNodeRow(DrawCtx& dc,
                                   NkUITreeNodeID node,
                                   int32 depth,
                                   bool isLastChild,
                                   bool hasChildren) noexcept {
            NkUITreeConfig& cfg   = *const_cast<NkUITreeConfig*>(dc.cfg);
            NkUITreeState&  state = *dc.state;
            NkUIContext&    ctx   = *dc.ctx;
            NkUIDrawList&   dl    = *dc.dl;
            NkUIFont&       font  = *dc.font;

            const float32 rowH   = cfg.rowHeightPx;
            const float32 indX   = dc.clipRect.x + depth * cfg.indentPx;
            const NkRect  rowR   = { dc.clipRect.x, dc.curY,
                                     dc.clipRect.w - kScrollbarW, rowH };

            const bool isSel  = state.IsSelected(node);
            const bool isHov  = (state.hoveredNode == node);
            const bool isDndT = (state.dndActive && state.dndTarget == node);

            // ── Background ───────────────────────────────────────────────────────
            if (isSel)
                dl.AddRectFilled(rowR, cfg.selectedBg);
            else if (isDndT)
                dl.AddRectFilled(rowR, cfg.dndTargetBg);
            else if (isHov)
                dl.AddRectFilled(rowR, cfg.hoveredBg);

            // ── Connector lines ───────────────────────────────────────────────────
            if (cfg.showConnectors && depth > 0) {
                // vertical line from parent row center to this row center
                const float32 lx = dc.clipRect.x + (depth - 1) * cfg.indentPx + kConnectorOffX;
                const float32 ly0= dc.curY - rowH * 0.5f;
                const float32 ly1= dc.curY + rowH * 0.5f;
                dl.AddLine({lx, ly0}, {lx, ly1}, cfg.connectorColor, 1.f);
                // horizontal connector to arrow/label
                const float32 hx1 = indX;
                dl.AddLine({lx, dc.curY + rowH * 0.5f},
                           {hx1, dc.curY + rowH * 0.5f},
                           cfg.connectorColor, 1.f);
            }

            // ── Arrow (expand/collapse) ───────────────────────────────────────────
            const float32 arrowX = indX;
            const float32 arrowCY = dc.curY + rowH * 0.5f;

            if (hasChildren) {
                const bool open = state.IsOpen(node);
                // triangle pointing right (closed) or down (open)
                if (open) {
                    // down arrow: ▼
                    dl.AddTriangleFilled(
                        {arrowX + 2.f, arrowCY - 4.f},
                        {arrowX + kArrowW - 2.f, arrowCY - 4.f},
                        {arrowX + kArrowW * 0.5f, arrowCY + 3.f},
                        {200, 200, 200, 200});
                } else {
                    // right arrow: ▶
                    dl.AddTriangleFilled(
                        {arrowX + 2.f, arrowCY - 5.f},
                        {arrowX + 2.f, arrowCY + 5.f},
                        {arrowX + kArrowW - 2.f, arrowCY},
                        {200, 200, 200, 200});
                }
            }

            // ── Icon ─────────────────────────────────────────────────────────────
            float32 textX = indX + kArrowW;
            char iconBuf[8] = {};
            SafeGetIcon(*dc.cb, node, iconBuf, sizeof(iconBuf));
            if (iconBuf[0]) {
                dc.font->RenderText(dl, {textX, dc.curY + 2.f}, iconBuf, {220, 200, 80, 220});
                textX += cfg.iconColWidthPx;
            }

            // ── Label or rename field ─────────────────────────────────────────────
            if (state.renaming && state.renamingNode == node) {
                // simple rename input: draw rect + text cursor simulation
                const NkRect inputR = { textX, dc.curY + 1.f,
                                        rowR.x + rowR.w - textX - 4.f, rowH - 2.f };
                dl.AddRectFilled(inputR, {30, 30, 40, 240});
                dl.AddRect(inputR, {100, 140, 220, 200}, 1.f);
                dc.font->RenderText(dl, {inputR.x + 2.f, inputR.y + 2.f},
                           state.renameBuffer, {230, 230, 230, 255});

                // keyboard handling
                if (ctx.input.IsKeyPressed(NkKey::NK_ENTER)) {
                    if (dc.cb->onRename)
                        dc.cb->onRename(node, state.renameBuffer, dc.cb->userData);
                    dc.result->event    = NkUITreeEvent::NK_TREE_NODE_RENAMED;
                    dc.result->node     = node;
                    state.renaming      = false;
                    state.renamingNode  = NKUI_TREE_NODE_NONE;
                } else if (ctx.input.IsKeyPressed(NkKey::NK_ESCAPE)) {
                    state.renaming     = false;
                    state.renamingNode = NKUI_TREE_NODE_NONE;
                } else {
                    // append printable chars
                    for (int32 i = 0; i < ctx.input.numInputChars; ++i) {
                        uint32 c = ctx.input.inputChars[i];
                        if (c >= 32 && c < 127) {
                            int32 len = (int32)strlen(state.renameBuffer);
                            if (len < 254) {
                                state.renameBuffer[len]     = (char)c;
                                state.renameBuffer[len + 1] = '\0';
                            }
                        }
                    }
                    // backspace
                    if (ctx.input.IsKeyPressed(NkKey::NK_BACK)) {
                        int32 len = (int32)strlen(state.renameBuffer);
                        if (len > 0) state.renameBuffer[len - 1] = '\0';
                    }
                }
            } else {
                char labelBuf[256] = {};
                SafeGetLabel(*dc.cb, node, labelBuf, sizeof(labelBuf));
                NkColor textCol = isSel ? NkColor{240,240,240,255} : NkColor{200,200,200,220};
                dc.font->RenderText(dl, {textX, dc.curY + 2.f}, labelBuf, textCol);
            }

            // ── Mouse interaction ─────────────────────────────────────────────────
            const NkVec2 mp = ctx.input.mousePos;
            const bool inRow = mp.x >= rowR.x && mp.x < rowR.x + rowR.w &&
                               mp.y >= rowR.y && mp.y < rowR.y + rowR.h;

            if (inRow) {
                state.hoveredNode = node;

                // Arrow click
                const bool inArrow = hasChildren &&
                                     mp.x >= arrowX && mp.x < arrowX + kArrowW;

                if (ctx.input.mouseClicked[0]) {
                    if (inArrow) {
                        bool open = state.IsOpen(node);
                        state.SetOpen(node, !open);
                        dc.result->event   = NkUITreeEvent::NK_TREE_EXPAND_CHANGED;
                        dc.result->node    = node;
                        dc.result->expanded = !open;
                    } else {
                        const bool multi = cfg.allowMultiSelect && ctx.input.shift;
                        state.SetSelected(node, true, !multi);
                        dc.result->event = NkUITreeEvent::NK_TREE_NODE_SELECTED;
                        dc.result->node  = node;
                        if (dc.cb->onSelect)
                            dc.cb->onSelect(state.selected, state.numSelected,
                                            dc.cb->userData);
                    }

                    // DnD start
                    if (cfg.allowDnD && !inArrow &&
                        (!dc.cb->canDrag || dc.cb->canDrag(node, dc.cb->userData))) {
                        state.dndActive = true;
                        state.dndSource = node;
                        state.dndTarget = NKUI_TREE_NODE_NONE;
                    }
                }

                // Double click → activate
                if (ctx.input.mouseDblClick[0] && !inArrow) {
                    dc.result->event = NkUITreeEvent::NK_TREE_NODE_ACTIVATED;
                    dc.result->node  = node;
                }

                // F2 rename
                if (ctx.input.IsKeyPressed(NkKey::NK_F2) && cfg.allowRename && dc.cb->onRename) {
                    char labelBuf[256] = {};
                    SafeGetLabel(*dc.cb, node, labelBuf, sizeof(labelBuf));
                    strncpy(state.renameBuffer, labelBuf, 255);
                    state.renameBuffer[255] = '\0';
                    state.renaming          = true;
                    state.renamingNode      = node;
                }

                // DnD target detection
                if (state.dndActive && state.dndSource != node) {
                    if (!dc.cb->canDrop ||
                        dc.cb->canDrop(state.dndSource, node, dc.cb->userData)) {
                        state.dndTarget = node;
                    }
                }
            }

            // DnD release
            if (state.dndActive && !ctx.input.mouseDown[0]) {
                if (state.dndTarget != NKUI_TREE_NODE_NONE &&
                    state.dndTarget != state.dndSource) {
                    if (dc.cb->onMove)
                        dc.cb->onMove(state.dndSource, state.dndTarget,
                                      dc.cb->userData);
                    dc.result->event     = NkUITreeEvent::NK_TREE_NODE_MOVED;
                    dc.result->node      = state.dndSource;
                    dc.result->newParent = state.dndTarget;
                }
                state.dndActive = false;
                state.dndSource = NKUI_TREE_NODE_NONE;
                state.dndTarget = NKUI_TREE_NODE_NONE;
            }
        }

        // =========================================================================
        //  NkUITree::DrawNodeRecursive
        // =========================================================================

        void NkUITree::DrawNodeRecursive(DrawCtx& dc,
                                         NkUITreeNodeID node,
                                         int32 depth,
                                         bool isLastChild) noexcept {
            const float32 rowH = dc.cfg->rowHeightPx;

            // Virtual scroll: skip rows that are above clip rect
            if (dc.curY + rowH < dc.clipRect.y) {
                dc.curY += rowH;
                // still recurse if open to account for height
                int32 childCount = dc.cb->getChildCount
                    ? dc.cb->getChildCount(node, dc.cb->userData) : 0;
                if (childCount > 0 && dc.state->IsOpen(node)) {
                    for (int32 i = 0; i < childCount; ++i) {
                        NkUITreeNodeID child = dc.cb->getChild(node, i, dc.cb->userData);
                        DrawNodeRecursive(dc, child, depth + 1, i == childCount - 1);
                    }
                }
                return;
            }
            // Skip rows below clip rect
            if (dc.curY > dc.clipRect.y + dc.clipRect.h) {
                dc.curY += rowH;
                return;
            }

            int32 childCount = dc.cb->getChildCount
                ? dc.cb->getChildCount(node, dc.cb->userData) : 0;
            bool hasChildren = (childCount > 0);

            DrawNodeRow(dc, node, depth, isLastChild, hasChildren);
            dc.curY += rowH;

            if (hasChildren && dc.state->IsOpen(node)) {
                for (int32 i = 0; i < childCount; ++i) {
                    NkUITreeNodeID child = dc.cb->getChild(node, i, dc.cb->userData);
                    DrawNodeRecursive(dc, child, depth + 1, i == childCount - 1);
                }
            }
        }

        // =========================================================================
        //  DnD ghost
        // =========================================================================

        static void DrawDnDGhost(NkUIDrawList& dl, NkUIFont& font,
                                 NkVec2 mousePos, const char* label) noexcept {
            NkRect ghost = { mousePos.x + 12.f, mousePos.y - 10.f, 160.f, 20.f };
            dl.AddRectFilled(ghost, {40, 50, 70, (uint8)kDndGhostAlpha});
            dl.AddRect(ghost, {100, 140, 220, 200}, 1.f);
            font.RenderText(dl, {ghost.x + 4.f, ghost.y + 3.f},
                       label, {220, 220, 220, 200});
        }

        // =========================================================================
        //  NkUITree::Draw
        // =========================================================================

        NkUITreeResult NkUITree::Draw(
            NkUIContext&             ctx,
            NkUIDrawList&            dl,
            NkUIFont&                font,
            NkUIID                   id,
            NkRect                   rect,
            NkUITreeNodeID           root,
            const NkUITreeCallbacks& cb,
            const NkUITreeConfig&    cfg,
            NkUITreeState&           state) noexcept
        {
            const NkUITreeNodeID roots[1] = { root };
            return DrawMultiRoot(ctx, dl, font, id, rect,
                                 roots, 1, cb, cfg, state);
        }

        // =========================================================================
        //  NkUITree::DrawMultiRoot
        // =========================================================================

        NkUITreeResult NkUITree::DrawMultiRoot(
            NkUIContext&             ctx,
            NkUIDrawList&            dl,
            NkUIFont&                font,
            NkUIID                   id,
            NkRect                   rect,
            const NkUITreeNodeID*    roots,
            int32                    rootCount,
            const NkUITreeCallbacks& cb,
            const NkUITreeConfig&    cfg,
            NkUITreeState&           state) noexcept
        {
            NkUITreeResult result;

            // Reset hovered each frame
            state.hoveredNode = NKUI_TREE_NODE_NONE;

            // ── Background ───────────────────────────────────────────────────────
            dl.AddRectFilled(rect, {25, 25, 35, 240});

            // ── Clip ─────────────────────────────────────────────────────────────
            dl.PushClipRect(rect);

            // ── Compute total content height (fast pass) ──────────────────────────
            // We reuse the recursive draw which does virtual scrolling, so the
            // content height is just estimated as rows * rowHeight.  For proper
            // scrolling we do a two-pass approach: first pass counts rows, second
            // draws. For simplicity here we use state.contentHeight from prev frame.
            const float32 visH     = rect.h;
            const float32 contH    = state.contentHeight > visH ? state.contentHeight : visH;
            const float32 maxScroll= contH - visH;

            // Mouse wheel scroll
            const NkVec2 mp = ctx.input.mousePos;
            const bool inRect = mp.x >= rect.x && mp.x < rect.x + rect.w &&
                                mp.y >= rect.y && mp.y < rect.y + rect.h;
            if (inRect && ctx.input.mouseWheel != 0.f) {
                state.scrollY -= ctx.input.mouseWheel * cfg.rowHeightPx * 3.f;
            }
            if (state.scrollY < 0.f)       state.scrollY = 0.f;
            if (state.scrollY > maxScroll) state.scrollY = maxScroll > 0.f ? maxScroll : 0.f;

            // ── Scrollbar ────────────────────────────────────────────────────────
            if (contH > visH) {
                const float32 sbX = rect.x + rect.w - kScrollbarW;
                const NkRect  sbBg = { sbX, rect.y, kScrollbarW, rect.h };
                dl.AddRectFilled(sbBg, {40, 40, 50, 180});

                const float32 thumbH  = (visH / contH) * rect.h;
                const float32 thumbY  = rect.y + (state.scrollY / maxScroll) *
                                        (rect.h - thumbH);
                const NkRect  thumb   = { sbX, thumbY, kScrollbarW, thumbH };
                dl.AddRectFilled(thumb, {100, 100, 130, 220});
            }

            // ── Clip rect adjusted for scroll ─────────────────────────────────────
            const NkRect clipR = { rect.x, rect.y,
                                   rect.w - kScrollbarW, rect.h };

            // ── Draw pass ────────────────────────────────────────────────────────
            DrawCtx dc;
            dc.ctx      = &ctx;
            dc.dl       = &dl;
            dc.font     = &font;
            dc.cb       = &cb;
            dc.cfg      = &cfg;
            dc.state    = &state;
            dc.clipRect = clipR;
            dc.curY     = rect.y - state.scrollY;
            dc.result   = &result;

            const float32 startY = dc.curY;

            for (int32 r = 0; r < rootCount; ++r) {
                if (roots[r] != NKUI_TREE_NODE_NONE)
                    DrawNodeRecursive(dc, roots[r], 0, r == rootCount - 1);
            }

            // Update content height for next frame scroll
            state.contentHeight = dc.curY - startY + state.scrollY;

            // ── DnD ghost ────────────────────────────────────────────────────────
            if (state.dndActive && state.dndSource != NKUI_TREE_NODE_NONE) {
                char ghostLabel[256] = {};
                SafeGetLabel(cb, state.dndSource, ghostLabel, sizeof(ghostLabel));
                DrawDnDGhost(dl, font, mp, ghostLabel);
            }

            dl.PopClipRect();

            // ── Border ───────────────────────────────────────────────────────────
            dl.AddRect(rect, {60, 60, 80, 180}, 1.f);

            return result;
        }

        // =========================================================================
        //  NkUITree::ExpandToNode
        // =========================================================================

        bool NkUITree::ExpandToNodeRec(NkUITreeNodeID cur,
                                       NkUITreeNodeID target,
                                       const NkUITreeCallbacks& cb,
                                       NkUITreeState& state) noexcept {
            if (cur == target) return true;

            int32 count = cb.getChildCount
                ? cb.getChildCount(cur, cb.userData) : 0;
            for (int32 i = 0; i < count; ++i) {
                NkUITreeNodeID child = cb.getChild(cur, i, cb.userData);
                if (ExpandToNodeRec(child, target, cb, state)) {
                    state.SetOpen(cur, true);
                    return true;
                }
            }
            return false;
        }

        bool NkUITree::ExpandToNode(NkUITreeNodeID root,
                                    NkUITreeNodeID target,
                                    const NkUITreeCallbacks& cb,
                                    NkUITreeState& state) noexcept {
            return ExpandToNodeRec(root, target, cb, state);
        }

    } // namespace nkui
} // namespace nkentseu
