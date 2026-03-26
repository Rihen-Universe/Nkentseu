/**
 * @File    NkUILayout.cpp
 * @Brief   Logique de layout — placement, scroll, splitter.
 */
#include "NkUI/NkUILayout.h"
#include "NkUI/NkUIWindow.h"
#include <cmath>

namespace nkentseu {
    namespace nkui {

        static bool LayoutWindowCanInteract(const NkUIContext& ctx, NkUIID widgetId) noexcept {
            if (!ctx.wm || ctx.currentWindowId == NKUI_ID_NONE) {
                return true;
            }
            if (ctx.wm->movingId != NKUI_ID_NONE) {
                return ctx.wm->movingId == ctx.currentWindowId;
            }
            if (ctx.wm->resizingId != NKUI_ID_NONE) {
                return ctx.wm->resizingId == ctx.currentWindowId;
            }
            if (ctx.wm->hoveredId != NKUI_ID_NONE && ctx.wm->hoveredId != ctx.currentWindowId) {
                return widgetId != NKUI_ID_NONE && ctx.IsActive(widgetId);
            }
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  ResolveWidth / ResolveHeight
        // ─────────────────────────────────────────────────────────────────────────────

        float32 NkUILayout::ResolveWidth(const NkUILayoutNode& node,
                                        const NkUIConstraint& c,float32 want) noexcept
        {
            float32 v=want;
            switch(c.type){
                case NkUIConstraint::NK_FIXED:   v=c.value; break;
                case NkUIConstraint::NK_PERCENT: v=node.bounds.w*c.value; break;
                case NkUIConstraint::NK_GROW:    v=node.bounds.w; break;
                default: v=want; break;
            }
            if(v<c.minSize) v=c.minSize;
            if(v>c.maxSize) v=c.maxSize;
            return v;
        }

        float32 NkUILayout::ResolveHeight(const NkUILayoutNode& node,
                                            const NkUIConstraint& c,float32 want) noexcept
        {
            float32 v=want;
            switch(c.type){
                case NkUIConstraint::NK_FIXED:   v=c.value; break;
                case NkUIConstraint::NK_PERCENT: v=node.bounds.h*c.value; break;
                case NkUIConstraint::NK_GROW:    v=node.bounds.h; break;
                default: v=want; break;
            }
            if(v<c.minSize) v=c.minSize;
            if(v>c.maxSize) v=c.maxSize;
            return v;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NextItemRect — calcule où placer le prochain widget
        // ─────────────────────────────────────────────────────────────────────────────

        NkRect NkUILayout::NextItemRect(NkUIContext& ctx,
                                        NkUILayoutStack& stack,
                                        float32 wantW,float32 wantH) noexcept
        {
            NkUILayoutNode* node=stack.Top();
            if(!node){
                // Pas de layout actif — utilise le curseur du contexte
                return {ctx.cursor.x,ctx.cursor.y,wantW,wantH};
            }

            float32 x=node->cursor.x-node->scrollX;
            float32 y=node->cursor.y-node->scrollY;
            float32 w=ResolveWidth(*node,node->nextW,wantW);
            float32 h=ResolveHeight(*node,node->nextH,wantH);

            // Reset les contraintes one-shot
            node->nextW={NkUIConstraint::NK_AUTO};
            node->nextH={NkUIConstraint::NK_AUTO};

            switch(node->type){
                case NkUILayoutType::NK_ROW:
                    // Horizontal : la largeur est résolue, la hauteur = max de la ligne
                    w=ResolveWidth(*node,{NkUIConstraint::NK_AUTO},wantW);
                    h=wantH;
                    // Limite à l'espace restant
                    {const float32 rem=node->bounds.x+node->bounds.w-x-node->scrollX;
                    if(w>rem) w=rem;}
                    break;
                case NkUILayoutType::NK_COLUMN:
                    // Vertical : pleine largeur par défaut
                    w=(node->nextW.type==NkUIConstraint::NK_AUTO)?node->bounds.w:w;
                    h=wantH;
                    break;
                case NkUILayoutType::NK_GRID:
                    w=node->gridCellW;
                    h=wantH;
                    x=node->bounds.x+node->gridCol*node->gridCellW-node->scrollX;
                    y=node->cursor.y-node->scrollY;
                    break;
                case NkUILayoutType::NK_SCROLL:
                    // w=(node->nextW.type==NkUIConstraint::NK_AUTO)?node->bounds.w:w;
                    // h=wantH;
                    // x et y depuis le cursor, mais offset par scrollY
                    x = node->cursor.x;
                    y = node->cursor.y - node->scrollY;  // ← applique le scroll ici
                    w = (node->nextW.type == NkUIConstraint::NK_AUTO) ? node->bounds.w : w;
                    h = wantH;
                    break;
                default:
                    break;
            }

            return {x,y,w>0?w:wantW,h>0?h:wantH};
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  AdvanceItem — avance le curseur après placement d'un item
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUILayout::AdvanceItem(NkUIContext& ctx,
                                    NkUILayoutStack& stack,
                                    NkRect placed) noexcept
        {
            NkUILayoutNode* node=stack.Top();
            const float32 sp=ctx.theme.metrics.itemSpacing;

            if(!node){
                ctx.AdvanceCursor({placed.w,placed.h});
                return;
            }

            ++node->itemCount;

            // Met à jour la taille du contenu pour le scroll
            const float32 bx=placed.x+placed.w+node->scrollX;
            const float32 by=placed.y+placed.h+node->scrollY;
            if(bx-node->bounds.x>node->contentSize.x) node->contentSize.x=bx-node->bounds.x;
            if(by-node->bounds.y>node->contentSize.y) node->contentSize.y=by-node->bounds.y;

            switch(node->type){
                case NkUILayoutType::NK_ROW:
                    node->cursor.x+=placed.w+sp;
                    if(placed.h>node->lineH) node->lineH=placed.h;
                    break;
                case NkUILayoutType::NK_COLUMN:
                case NkUILayoutType::NK_SCROLL:
                    // node->cursor.y+=placed.h+sp;
                    // if(placed.w>node->colW) node->colW=placed.w;
                    node->cursor.y += placed.h + sp;
                    // contentSize = distance parcourue depuis le début, sans le scroll
                    {
                        const float32 absY = node->cursor.y - node->bounds.y;
                        if (absY > node->contentSize.y) node->contentSize.y = absY;
                    }
                    break;
                case NkUILayoutType::NK_GRID:
                    ++node->gridCol;
                    if(node->gridCol>=node->gridCols){
                        node->gridCol=0;
                        node->cursor.y+=placed.h+sp;
                    }
                    break;
                default:
                    // Free : avance verticalement
                    node->cursor.y+=placed.h+sp;
                    break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawScrollbar
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUILayout::DrawScrollbar(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        bool vertical,
                                        NkRect track,
                                        float32 contentSize,
                                        float32 viewSize,
                                        float32& scroll,
                                        NkUIID id) noexcept
        {
            if(contentSize<=viewSize) return false; // pas besoin de scroll

            const auto& c=ctx.theme.colors;
            const auto& m=ctx.theme.metrics;
            const float32 ratio=viewSize/contentSize;
            const float32 thumbLen=(vertical?track.h:track.w)*ratio;
            const float32 thumbMin=m.scrollbarMinLen;
            const float32 tl=thumbLen<thumbMin?thumbMin:thumbLen;
            const float32 maxScroll=contentSize-viewSize;
            const float32 scrollFrac=maxScroll>0?scroll/maxScroll:0.f;
            const float32 trackLen=(vertical?track.h:track.w)-tl;

            // Rect du thumb
            NkRect thumb=track;
            if(vertical){ thumb.y+=trackLen*scrollFrac; thumb.h=tl; thumb.w-=2; thumb.x+=1; }
            else         { thumb.x+=trackLen*scrollFrac; thumb.w=tl; thumb.h-=2; thumb.y+=1; }

            // Dessin
            dl.AddRectFilled(track,c.scrollBg,m.cornerRadiusSm);
            const bool canInteract = LayoutWindowCanInteract(ctx, id);
            const bool hov=canInteract && ctx.IsHovered(thumb);
            const bool act=ctx.IsActive(id);
            dl.AddRectFilled(thumb,act||hov?c.scrollThumbHov:c.scrollThumb,m.cornerRadiusSm);

            // Interaction drag
            if(hov&&ctx.input.IsMouseClicked(0)){
                ctx.SetActive(id);
            }
            if(act){
                const float32 delta=(vertical?ctx.input.mouseDelta.y:ctx.input.mouseDelta.x);
                if(delta!=0){
                    scroll+=delta*(contentSize/viewSize);
                    if(scroll<0) scroll=0;
                    if(scroll>maxScroll) scroll=maxScroll;
                }
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }

            // Molette souris
            const float32 wheelDelta = vertical ? ctx.input.mouseWheel : ctx.input.mouseWheelH;
            if(canInteract&&ctx.IsHovered(track)&&wheelDelta!=0){
                scroll-=wheelDelta*m.itemHeight*3;
                if(scroll<0) scroll=0;
                if(scroll>maxScroll) scroll=maxScroll;
            }

            return act;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawSplitter
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUILayout::DrawSplitter(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        NkRect rect,
                                        bool vertical,
                                        float32& ratio,
                                        NkUIID id) noexcept
        {
            const auto& c=ctx.theme.colors;
            const auto& m=ctx.theme.metrics;
            const float32 hw=m.dockSplitW*0.5f;
            NkRect splitter=rect;
            if(vertical){
                splitter.x=rect.x+rect.w*ratio-hw;
                splitter.w=m.dockSplitW;
            } else {
                splitter.y=rect.y+rect.h*ratio-hw;
                splitter.h=m.dockSplitW;
            }

            const bool canInteract = LayoutWindowCanInteract(ctx, id);
            const bool hov=canInteract && ctx.IsHovered(splitter);
            const bool act=ctx.IsActive(id);

            dl.AddRectFilled(splitter,hov||act?c.accentHover:c.separator);

            if(hov&&ctx.input.IsMouseClicked(0)) ctx.SetActive(id);
            if(act){
                const float32 d=vertical
                    ?(ctx.input.mousePos.x-rect.x)/rect.w
                    :(ctx.input.mousePos.y-rect.y)/rect.h;
                if(d>0.05f&&d<0.95f) ratio=d;
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
                return true;
            }
            return false;
        }
    }
} // namespace nkentseu
