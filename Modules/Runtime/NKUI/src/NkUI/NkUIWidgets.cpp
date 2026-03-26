/**
 * @File    NkUIWidgets.cpp
 * @Brief   Implémentation de tous les widgets NkUI.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NkUI/NkUIWidgets.h"
#include "NkUI/NkUIWindow.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Helpers de label
        // ─────────────────────────────────────────────────────────────────────────────

        NkUILabelParts NkParseLabel(const char* raw) noexcept {
            NkUILabelParts p;
            if(!raw){return p;}
            const char* sep=::strstr(raw,"##");
            if(sep){
                const int32 vlen=static_cast<int32>(sep-raw);
                ::strncpy(p.label,raw,vlen<255?vlen:255);
                ::strncpy(p.id,sep+2,127);
            } else {
                ::strncpy(p.label,raw,255);
                ::strncpy(p.id,raw,127);
            }
            return p;
        }

        NkUIID NkLabelToID(NkUIContext& ctx, const char* raw) noexcept {
            NkUILabelParts p=NkParseLabel(raw);
            return ctx.GetID(p.id[0]?p.id:p.label);
        }

        static int32 NkEncodeUTF8(uint32 cp, char out[4]) noexcept {
            if (cp <= 0x7Fu) {
                out[0] = static_cast<char>(cp);
                return 1;
            }
            if (cp <= 0x7FFu) {
                out[0] = static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu));
                out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 2;
            }
            if (cp <= 0xFFFFu) {
                out[0] = static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu));
                out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 3;
            }
            if (cp <= 0x10FFFFu) {
                out[0] = static_cast<char>(0xF0u | ((cp >> 18) & 0x07u));
                out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
                out[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 4;
            }
            return 0;
        }

        static void NkPopLastUTF8(char* buf) noexcept {
            if (!buf) return;
            int32 len = static_cast<int32>(::strlen(buf));
            if (len <= 0) return;
            int32 i = len - 1;
            while (i > 0 && (static_cast<uint8>(buf[i]) & 0xC0u) == 0x80u) {
                --i;
            }
            buf[i] = 0;
        }

        static bool NkAppendUTF8(char* buf, int32 bufSize, uint32 cp) noexcept {
            if (!buf || bufSize <= 1) return false;
            char encoded[4] = {};
            const int32 bytes = NkEncodeUTF8(cp, encoded);
            if (bytes <= 0) return false;
            const int32 len = static_cast<int32>(::strlen(buf));
            if (len + bytes >= bufSize) return false;
            for (int32 i = 0; i < bytes; ++i) buf[len + i] = encoded[i];
            buf[len + bytes] = 0;
            return true;
        }

        static bool WindowAllowsInputForWidget(const NkUIContext& ctx, NkUIID windowId, NkUIID widgetId) noexcept {
            if (!ctx.wm || windowId == NKUI_ID_NONE) {
                return true;
            }

            if (ctx.wm->movingId != NKUI_ID_NONE) {
                return ctx.wm->movingId == windowId;
            }
            if (ctx.wm->resizingId != NKUI_ID_NONE) {
                return ctx.wm->resizingId == windowId;
            }
            if (ctx.wm->hoveredId != NKUI_ID_NONE && ctx.wm->hoveredId != windowId) {
                return (widgetId != NKUI_ID_NONE) && ctx.IsActive(widgetId);
            }
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  CalcState — état hover/active/focus d'un widget
        // ─────────────────────────────────────────────────────────────────────────────

        NkUIWidgetState NkUI::CalcState(NkUIContext& ctx,NkUIID id,NkRect r,bool enabled) noexcept {
            NkUIWidgetState s=NkUIWidgetState::NK_NORMAL;
            if(!enabled) return NkUIWidgetState::NK_DISABLED;

            const bool windowAllowsInput = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            if(windowAllowsInput && ctx.IsHovered(r)){
                ctx.SetHot(id);
                s=s|NkUIWidgetState::NK_HOVERED;
                if(ctx.input.IsMouseClicked(0) && (ctx.activeId == NKUI_ID_NONE || ctx.activeId == id)) {
                    ctx.SetActive(id);
                }
            }
            if(ctx.IsActive(id)) s=s|NkUIWidgetState::NK_ACTIVE;
            if(ctx.IsFocused(id)) s=s|NkUIWidgetState::NK_FOCUSED;
            return s;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawWidgetBg — fond et bordure standard
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUI::DrawWidgetBg(NkUIDrawList& dl,const NkUITheme& t,
                                NkRect r,NkUIWidgetType type,NkUIWidgetState state) noexcept
        {
            const auto& c=t.colors;
            const auto& m=t.metrics;
            const float32 rx=m.cornerRadius;
            NkColor bg,border;
            const bool hov=HasState(state,NkUIWidgetState::NK_HOVERED);
            const bool act=HasState(state,NkUIWidgetState::NK_ACTIVE);
            const bool dis=HasState(state,NkUIWidgetState::NK_DISABLED);
            const bool foc=HasState(state,NkUIWidgetState::NK_FOCUSED);

            switch(type){
                case NkUIWidgetType::NK_BUTTON:
                    bg=dis?c.accentDisabled:(act?c.buttonActive:(hov?c.buttonHover:c.buttonBg));
                    border=foc?c.borderFocus:(hov?c.borderHover:c.border);
                    break;
                case NkUIWidgetType::NK_INPUT_TEXT:
                case NkUIWidgetType::NK_INPUT_INT:
                case NkUIWidgetType::NK_INPUT_FLOAT:
                    bg=dis?NkColor::Gray(240):c.inputBg;
                    border=foc?c.borderFocus:(hov?c.borderHover:c.border);
                    break;
                case NkUIWidgetType::NK_CHECKBOX:
                case NkUIWidgetType::NK_RADIO:
                    bg=dis?NkColor::Gray(240):c.checkBg;
                    border=foc?c.borderFocus:c.border;
                    break;
                default:
                    bg=c.bgSecondary;
                    border=c.border;
                    break;
            }
            dl.AddRectFilled(r,bg,rx);
            dl.AddRect(r,border,m.borderWidth,rx);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawLabel — texte centré ou aligné à gauche avec padding
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUI::DrawLabel(NkUIDrawList& dl,NkUIFont& f,const NkUITheme& t,
                            NkRect r,const char* text,bool centered) noexcept
        {
            if(!text||!*text) return;
            const float32 tw=f.MeasureWidth(text);
            float32 tx;
            if(centered) tx=r.x+(r.w-tw)*0.5f;
            else tx=r.x+t.metrics.paddingX;
            const float32 ty=r.y+(r.h-f.size)*0.5f+f.metrics.ascender*0.5f;
            f.RenderText(dl,{tx,ty},text,t.colors.textPrimary,r.w-t.metrics.paddingX*2,true);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Layout helpers
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUI::BeginRow(NkUIContext& ctx,NkUILayoutStack& ls,float32 height) noexcept {
            NkUILayoutNode* parent=ls.Top();
            NkUILayoutNode* node=ls.Push(); if(!node) return;
            node->type=NkUILayoutType::NK_ROW;
            node->bounds=parent?NkRect{parent->cursor.x,parent->cursor.y,parent->bounds.w,height>0?height:ctx.GetItemHeight()}
                            :NkRect{ctx.cursor.x,ctx.cursor.y,static_cast<float32>(ctx.viewW),height>0?height:ctx.GetItemHeight()};
            node->cursor={node->bounds.x,node->bounds.y};
            node->lineH=0;
        }

        void NkUI::EndRow(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            NkUILayoutNode* node=ls.Top(); if(!node) return;
            const float32 h=node->lineH>0?node->lineH:node->bounds.h;
            ls.Pop();
            NkUILayoutNode* parent=ls.Top();
            if(parent) NkUILayout::AdvanceItem(ctx,ls,{node->bounds.x,node->bounds.y,node->bounds.w,h+ctx.theme.metrics.itemSpacing});
            else ctx.AdvanceCursor({0,h+ctx.theme.metrics.itemSpacing});
        }

        void NkUI::BeginColumn(NkUIContext& ctx,NkUILayoutStack& ls,float32 width) noexcept {
            NkUILayoutNode* parent=ls.Top();
            NkUILayoutNode* node=ls.Push(); if(!node) return;
            node->type=NkUILayoutType::NK_COLUMN;
            const float32 pw=parent?parent->bounds.w:static_cast<float32>(ctx.viewW);
            const float32 ph=parent?parent->bounds.h:static_cast<float32>(ctx.viewH);
            const float32 px=parent?parent->cursor.x:ctx.cursor.x;
            const float32 py=parent?parent->cursor.y:ctx.cursor.y;
            node->bounds={px,py,width>0?width:pw,ph};
            node->cursor={node->bounds.x,node->bounds.y};
        }

        void NkUI::EndColumn(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            NkUILayoutNode* node=ls.Top(); if(!node) return;
            const NkRect b=node->bounds;
            const float32 h=node->cursor.y-b.y;
            ls.Pop();
            NkUILayoutNode* parent=ls.Top();
            if(parent) NkUILayout::AdvanceItem(ctx,ls,{b.x,b.y,b.w,h});
            else ctx.AdvanceCursor({b.w,h});
        }

        void NkUI::BeginGroup(NkUIContext& ctx,NkUILayoutStack& ls,
                                const char* label,bool border) noexcept {
            NkUILayoutNode* parent=ls.Top();
            NkUILayoutNode* node=ls.Push(); if(!node) return;
            node->type=NkUILayoutType::NK_GROUP;
            const float32 px=parent?parent->cursor.x:ctx.cursor.x;
            const float32 py=parent?parent->cursor.y:ctx.cursor.y;
            const float32 pw=parent?parent->bounds.w:static_cast<float32>(ctx.viewW);
            const float32 ph=parent?parent->bounds.h:static_cast<float32>(ctx.viewH);
            const float32 pad=ctx.theme.metrics.windowPadX;
            node->bounds={px,py,pw,ph};
            node->cursor={px+pad,py+(label&&*label?ctx.theme.metrics.itemHeight:pad)};
            (void)border;
        }

        void NkUI::EndGroup(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            NkUILayoutNode* node=ls.Top(); if(!node) return;
            const NkRect b=node->bounds;
            const float32 h=node->cursor.y-b.y+ctx.theme.metrics.windowPadY;
            ls.Pop();
            if(NkUILayoutNode* parent=ls.Top())
                NkUILayout::AdvanceItem(ctx,ls,{b.x,b.y,b.w,h});
            else ctx.AdvanceCursor({b.w,h});
        }

        bool NkUI::BeginGrid(NkUIContext& ctx,NkUILayoutStack& ls,int32 cols,float32 cellH) noexcept {
            NkUILayoutNode* parent=ls.Top();
            NkUILayoutNode* node=ls.Push(); if(!node) return false;
            node->type=NkUILayoutType::NK_GRID;
            const float32 px=parent?parent->cursor.x:ctx.cursor.x;
            const float32 py=parent?parent->cursor.y:ctx.cursor.y;
            const float32 pw=parent?parent->bounds.w:static_cast<float32>(ctx.viewW);
            node->bounds={px,py,pw,cellH>0?cellH:ctx.GetItemHeight()};
            node->cursor={px,py};
            node->gridCols=cols>0?cols:1;
            node->gridCol=0;
            node->gridCellW=pw/node->gridCols;
            return true;
        }

        void NkUI::EndGrid(NkUIContext& ctx,NkUILayoutStack& ls) noexcept { EndColumn(ctx,ls); }

        bool NkUI::BeginScrollRegion(NkUIContext& ctx,NkUILayoutStack& ls,
                                    const char* id,NkRect rect,
                                    float32* scrollX,float32* scrollY) noexcept {
            NkUILayoutNode* node=ls.Push(); if(!node) return false;
            node->type=NkUILayoutType::NK_SCROLL;
            node->bounds=rect;
            node->id=ctx.GetID(id);
            node->scrollXPtr=scrollX; node->scrollYPtr=scrollY;
            node->scrollX=scrollX?*scrollX:0;
            node->scrollY=scrollY?*scrollY:0;
            node->cursor={rect.x,rect.y};
            node->contentSize={};
            ctx.dl->PushClipRect(rect);
            return true;
        }

        void NkUI::EndScrollRegion(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            NkUILayoutNode* node=ls.Top(); if(!node) return;
            ctx.dl->PopClipRect();
            const auto& m=ctx.theme.metrics;
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, node->id);

            if(canInteract && ctx.IsHovered(node->bounds) && ctx.input.mouseWheel!=0.f){
                node->scrollY -= ctx.input.mouseWheel*m.itemHeight*3.f;
            }
            if(canInteract && ctx.IsHovered(node->bounds) && ctx.input.mouseWheelH!=0.f){
                node->scrollX -= ctx.input.mouseWheelH*m.itemHeight*3.f;
            }

            const float32 maxY = node->contentSize.y>node->bounds.h ? (node->contentSize.y-node->bounds.h) : 0.f;
            const float32 maxX = node->contentSize.x>node->bounds.w ? (node->contentSize.x-node->bounds.w) : 0.f;
            if(node->scrollY<0.f) node->scrollY=0.f;
            if(node->scrollY>maxY) node->scrollY=maxY;
            if(node->scrollX<0.f) node->scrollX=0.f;
            if(node->scrollX>maxX) node->scrollX=maxX;

            // Scrollbar verticale
            if(node->contentSize.y>node->bounds.h){
                NkRect track={node->bounds.x+node->bounds.w-m.scrollbarWidth,node->bounds.y,
                            m.scrollbarWidth,node->bounds.h};
                NkUILayout::DrawScrollbar(ctx,*ctx.dl,true,track,
                                        node->contentSize.y,node->bounds.h,
                                        node->scrollY,NkHashInt(node->id,1));
                if(node->scrollYPtr) *node->scrollYPtr=node->scrollY;
            }
            // Scrollbar horizontale
            if(node->contentSize.x>node->bounds.w){
                NkRect track={node->bounds.x,node->bounds.y+node->bounds.h-m.scrollbarWidth,
                            node->bounds.w,m.scrollbarWidth};
                NkUILayout::DrawScrollbar(ctx,*ctx.dl,false,track,
                                        node->contentSize.x,node->bounds.w,
                                        node->scrollX,NkHashInt(node->id,2));
                if(node->scrollXPtr) *node->scrollXPtr=node->scrollX;
            }
            ls.Pop();
        }

        bool NkUI::BeginSplit(NkUIContext& ctx,NkUILayoutStack& ls,
                            const char* id,NkRect rect,bool vertical,float32* ratio) noexcept {
            NkUILayoutNode* node=ls.Push(); if(!node) return false;
            node->type=NkUILayoutType::NK_SPLIT;
            node->bounds=rect;
            node->id=ctx.GetID(id);
            node->splitVertical=vertical;
            node->splitRatio=ratio?*ratio:0.5f;
            node->splitRatioPtr=ratio;
            node->splitSecondPane=false;
            // Dessin du splitter et mise à jour du ratio
            NkUILayout::DrawSplitter(ctx,*ctx.dl,rect,vertical,node->splitRatio,node->id);
            if(ratio) *ratio=node->splitRatio;
            // Retourne true pour le premier panneau
            // Calcule les bounds du premier panneau
            node->cursor={rect.x,rect.y};
            if(vertical) node->bounds={rect.x,rect.y,rect.w*node->splitRatio-2,rect.h};
            else         node->bounds={rect.x,rect.y,rect.w,rect.h*node->splitRatio-2};
            return true;
        }

        bool NkUI::BeginSplitPane2(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            NkUILayoutNode* node=ls.Top(); if(!node||node->type!=NkUILayoutType::NK_SPLIT) return false;
            node->splitSecondPane=true;
            const NkRect& full=node->bounds;
            const float32 r=node->splitRatio;
            if(node->splitVertical){
                node->bounds={full.x+full.w+4,full.y,full.w*(1-r)-2,full.h};
                // Note: full.h n'est pas correct — on utilise node->bounds initialement stocké
            } else {
                // simplifié
            }
            node->cursor={node->bounds.x,node->bounds.y};
            return true;
        }

        void NkUI::EndSplit(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            if(NkUILayoutNode* node=ls.Top()){
                if(!node->splitSecondPane){ return; } // attend le BeginSplitPane2
            }
            ls.Pop();
        }

        void NkUI::SetNextWidth(NkUIContext&,NkUILayoutStack& ls,float32 w) noexcept {
            if(NkUILayoutNode* n=ls.Top()) n->nextW=NkUIConstraint::Fixed_(w);
        }
        void NkUI::SetNextHeight(NkUIContext&,NkUILayoutStack& ls,float32 h) noexcept {
            if(NkUILayoutNode* n=ls.Top()) n->nextH=NkUIConstraint::Fixed_(h);
        }
        void NkUI::SetNextWidthPct(NkUIContext&,NkUILayoutStack& ls,float32 p) noexcept {
            if(NkUILayoutNode* n=ls.Top()) n->nextW=NkUIConstraint::Percent_(p);
        }
        void NkUI::SetNextGrow(NkUIContext&,NkUILayoutStack& ls) noexcept {
            if(NkUILayoutNode* n=ls.Top()) n->nextW=NkUIConstraint::Grow_();
        }

        void NkUI::SameLine(NkUIContext& ctx,NkUILayoutStack& ls,float32 offset,float32 spacing) noexcept {
            NkUILayoutNode* n=ls.Top();
            if(n){ n->cursor.y-=ctx.GetItemHeight()+ctx.theme.metrics.itemSpacing; n->cursor.x+=offset>0?offset:(spacing>=0?spacing:ctx.theme.metrics.itemSpacing); }
            else ctx.SameLine(offset,spacing);
        }
        void NkUI::NewLine(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            if(NkUILayoutNode* n=ls.Top()){n->cursor.x=n->bounds.x;n->cursor.y+=ctx.GetItemHeight();}
            else ctx.NewLine();
        }
        void NkUI::Spacing(NkUIContext& ctx,NkUILayoutStack& ls,float32 px) noexcept {
            const float32 sp=px>0?px:ctx.theme.metrics.sectionSpacing;
            if(NkUILayoutNode* n=ls.Top()) n->cursor.y+=sp;
            else ctx.Spacing(sp);
        }
        void NkUI::Separator(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl) noexcept {
            NkUILayoutNode* n=ls.Top();
            const float32 x=n?n->cursor.x:ctx.cursor.x;
            const float32 y=n?n->cursor.y:ctx.cursor.y;
            const float32 w=n?n->bounds.w:static_cast<float32>(ctx.viewW);
            dl.AddLine({x,y+3},{x+w,y+3},ctx.theme.colors.separator,1.f);
            if(n) n->cursor.y+=8;
            else ctx.Spacing(8);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Texte
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUI::Text(NkUIContext& ctx,NkUILayoutStack& ls,
                        NkUIDrawList& dl,NkUIFont& font,
                        const char* text,NkColor col) noexcept
        {
            if(!text||!*text) return;
            const float32 tw=font.MeasureWidth(text);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,tw+ctx.GetPaddingX()*2,ctx.GetItemHeight());
            const NkColor c=(col.a==0)?ctx.theme.colors.textPrimary:col;
            font.RenderText(dl,{r.x,r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},text,c);
            NkUILayout::AdvanceItem(ctx,ls,r);
        }

        void NkUI::TextSmall(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* text) noexcept {
            Text(ctx,ls,dl,font,text,ctx.theme.colors.textSecondary);
        }
        void NkUI::TextColored(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,NkColor col,const char* text) noexcept {
            Text(ctx,ls,dl,font,text,col);
        }
        void NkUI::TextWrapped(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* text) noexcept {
            NkUILayoutNode* n=ls.Top();
            const float32 w=n?n->bounds.w:static_cast<float32>(ctx.viewW);
            const float32 x=n?n->cursor.x:ctx.cursor.x;
            const float32 y=n?n->cursor.y:ctx.cursor.y;
            const int32 lines=(int32)(font.MeasureWidth(text)/w)+1;
            const float32 h=font.metrics.lineHeight*lines+ctx.GetPaddingY()*2;
            const NkRect r={x,y,w,h};
            font.RenderTextWrapped(dl,{r.x+ctx.GetPaddingX(),r.y+ctx.GetPaddingY(),r.w-ctx.GetPaddingX()*2,r.h},text,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
        }
        void NkUI::LabelValue(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label,const char* value) noexcept {
            BeginRow(ctx,ls);
            Text(ctx,ls,dl,font,label,ctx.theme.colors.textSecondary);
            Text(ctx,ls,dl,font,value);
            EndRow(ctx,ls);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Button
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::Button(NkUIContext& ctx,NkUILayoutStack& ls,
                        NkUIDrawList& dl,NkUIFont& font,
                        const char* label,NkVec2 size) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 tw=font.MeasureWidth(lp.label);
            const float32 ph=ctx.GetItemHeight();
            const float32 pw=size.x>0?size.x:tw+ctx.GetPaddingX()*2+4;
            const float32 ph2=size.y>0?size.y:ph;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,pw,ph2);
            const NkUIWidgetState state=CalcState(ctx,id,r);

            if(!ctx.CallShapeOverride(NkUIWidgetType::NK_BUTTON,r,state,lp.label)){
                DrawWidgetBg(dl,ctx.theme,r,NkUIWidgetType::NK_BUTTON,state);
                DrawLabel(dl,font,ctx.theme,r,lp.label,true);
            }
            NkUILayout::AdvanceItem(ctx,ls,r);
            ctx.lastId=id;

            // Animation de press
            ctx.Animate(id,HasState(state,NkUIWidgetState::NK_HOVERED)?1.f:0.f);

            // Click : released && was active
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r)){
                ctx.ClearActive();
                return true;
            }
            return false;
        }

        bool NkUI::ButtonSmall(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label) noexcept {
            ctx.PushStyleVar(NkStyleVar::NK_PADDING_X,5.f);
            ctx.PushStyleVar(NkStyleVar::NK_PADDING_Y,3.f);
            const bool r=Button(ctx,ls,dl,font,label);
            ctx.PopStyle(2);
            return r;
        }

        bool NkUI::ButtonImage(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,
                                uint32 texId,NkVec2 size,NkColor tint) noexcept {
            const NkUIID id=ctx.GetID(static_cast<int32>(texId));
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,size.x,size.y);
            const NkUIWidgetState state=CalcState(ctx,id,r);
            dl.AddImage(texId,r,{0,0},{1,1},tint);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r);
        }

        bool NkUI::InvisibleButton(NkUIContext& ctx,NkUILayoutStack& ls,const char* label,NkVec2 size) noexcept {
            const NkUIID id=ctx.GetID(label);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,size.x,size.y);
            const NkUIWidgetState state=CalcState(ctx,id,r);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Checkbox
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::Checkbox(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,bool& value) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 sz=ctx.theme.metrics.checkboxSize;
            const float32 tw=font.MeasureWidth(lp.label);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,sz+ctx.GetPaddingX()+tw,ctx.GetItemHeight());

            // Rect de la case à cocher
            const NkRect box={r.x,r.y+(r.h-sz)*0.5f,sz,sz};
            const NkUIWidgetState state=CalcState(ctx,id,r);
            bool changed=false;

            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r)){
                value=!value; changed=true; ctx.ClearActive();
            }

            if(!ctx.CallShapeOverride(NkUIWidgetType::NK_CHECKBOX,r,
                value?state|NkUIWidgetState::NK_CHECKED:state,lp.label,(float32)value)){
                // Case
                dl.AddRectFilled(box,ctx.theme.colors.checkBg,ctx.theme.metrics.cornerRadiusSm);
                dl.AddRect(box,HasState(state,NkUIWidgetState::NK_FOCUSED)?
                        ctx.theme.colors.borderFocus:ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadiusSm);
                if(value){
                    const float32 anim=ctx.Animate(id,1.f,12.f);
                    if(anim>0.1f){
                        const NkColor mk={ctx.theme.colors.checkMark.r,ctx.theme.colors.checkMark.g,
                                        ctx.theme.colors.checkMark.b,static_cast<uint8>(anim*255)};
                        dl.AddCheckMark({box.x+2,box.y+2},sz-4,mk);
                    }
                } else { ctx.Animate(id,0.f,12.f); }
                // Label
                font.RenderText(dl,{r.x+sz+ctx.GetPaddingX(),r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                lp.label,ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RadioButton
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::RadioButton(NkUIContext& ctx,NkUILayoutStack& ls,
                                NkUIDrawList& dl,NkUIFont& font,
                                const char* label,int32& selected,int32 value) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=NkHashInt(value,ctx.GetID(lp.id[0]?lp.id:lp.label));
            const float32 sz=ctx.theme.metrics.radioSize;
            const float32 tw=font.MeasureWidth(lp.label);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,sz+ctx.GetPaddingX()+tw,ctx.GetItemHeight());
            const NkVec2 center={r.x+sz*0.5f,r.y+r.h*0.5f};
            const NkUIWidgetState state=CalcState(ctx,id,r);
            bool changed=false;

            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r)){
                selected=value; changed=true; ctx.ClearActive();
            }

            dl.AddCircleFilled(center,sz*0.5f,ctx.theme.colors.checkBg);
            dl.AddCircle(center,sz*0.5f,ctx.theme.colors.border,1.f);
            if(selected==value){
                const float32 anim=ctx.Animate(id,1.f,12.f);
                dl.AddCircleFilled(center,sz*0.3f*anim,ctx.theme.colors.checkMark);
            } else ctx.Animate(id,0.f,12.f);
            font.RenderText(dl,{r.x+sz+ctx.GetPaddingX(),r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                            lp.label,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Toggle
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::Toggle(NkUIContext& ctx,NkUILayoutStack& ls,
                        NkUIDrawList& dl,NkUIFont& font,
                        const char* label,bool& value) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 tw2=ctx.GetItemHeight()*1.8f, th=ctx.GetItemHeight()*0.6f;
            const float32 textW=font.MeasureWidth(lp.label);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,tw2+ctx.GetPaddingX()+textW,ctx.GetItemHeight());
            const NkRect track={r.x,r.y+(r.h-th)*0.5f,tw2,th};
            const NkUIWidgetState state=CalcState(ctx,id,r);
            bool changed=false;

            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(r)){
                value=!value; changed=true; ctx.ClearActive();
            }

            const float32 anim=ctx.Animate(id,value?1.f:0.f,10.f);
            const NkColor trackCol=ctx.theme.colors.scrollBg.Lerp(ctx.theme.colors.checkMark,anim);
            const float32 thumbX=track.x+th*0.1f+(track.w-th*1.2f)*anim;
            const NkRect thumb={thumbX,track.y+th*0.1f,th*0.8f,th*0.8f};

            dl.AddRectFilled(track,trackCol,track.h*0.5f);
            dl.AddCircleFilled({thumb.x+thumb.w*0.5f,thumb.y+thumb.h*0.5f},
                                thumb.w*0.5f,NkColor::White());
            font.RenderText(dl,{r.x+tw2+ctx.GetPaddingX(),r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                            lp.label,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  SliderFloat
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::SliderFloat(NkUIContext& ctx,NkUILayoutStack& ls,
                                NkUIDrawList& dl,NkUIFont& font,
                                const char* label,float32& value,
                                float32 vmin,float32 vmax,
                                const char* fmt,float32 width) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 labelW=lp.label[0]?font.MeasureWidth(lp.label)+ctx.GetPaddingX():0;
            const float32 totalW=width>0?width:200.f+labelW;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,totalW,ctx.GetItemHeight());
            const NkRect sliderR={r.x,r.y,r.w-labelW,r.h};

            const NkUIWidgetState state=CalcState(ctx,id,sliderR);
            bool changed=false;

            // Interaction
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)){
                const float32 t=(ctx.input.mousePos.x-sliderR.x)/sliderR.w;
                const float32 newVal=vmin+(vmax-vmin)*::fmaxf(0,::fminf(1,t));
                if(newVal!=value){value=newVal;changed=true;}
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }

            const float32 t=(vmax>vmin)?(value-vmin)/(vmax-vmin):0.f;

            if(!ctx.CallShapeOverride(NkUIWidgetType::NK_SLIDER_FLOAT,r,state,lp.label,t)){
                // Track
                const float32 th=ctx.theme.metrics.sliderTrackH;
                const NkRect track={sliderR.x,sliderR.y+(sliderR.h-th)*0.5f,sliderR.w,th};
                dl.AddRectFilled(track,ctx.theme.colors.sliderTrack,th*0.5f);
                // Fill
                const NkRect fill={track.x,track.y,track.w*t,track.h};
                if(fill.w>0) dl.AddRectFilled(fill,ctx.theme.colors.sliderThumb,th*0.5f);
                // Thumb
                const float32 tw2=ctx.theme.metrics.sliderThumbW;
                const NkVec2 thumbC={sliderR.x+sliderR.w*t,sliderR.y+sliderR.h*0.5f};
                const NkColor thumbC2=(HasState(state,NkUIWidgetState::NK_ACTIVE)||HasState(state,NkUIWidgetState::NK_HOVERED))
                    ?ctx.theme.colors.accentHover:ctx.theme.colors.sliderThumb;
                dl.AddCircleFilled(thumbC,tw2*0.5f,thumbC2);
                dl.AddCircle(thumbC,tw2*0.5f,ctx.theme.colors.border,1.f);
                // Valeur
                char valBuf[32]; ::snprintf(valBuf,sizeof(valBuf),fmt?fmt:"%.2f",value);
                const float32 vw=font.MeasureWidth(valBuf);
                font.RenderText(dl,{thumbC.x-vw*0.5f,sliderR.y-font.size-2},valBuf,ctx.theme.colors.textSecondary);
                // Label
                if(lp.label[0])
                    font.RenderText(dl,{r.x+r.w-labelW+ctx.GetPaddingX()*0.5f,
                                        r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                    lp.label,ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        bool NkUI::SliderInt(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,int32& value,int32 vmin,int32 vmax,float32 width) noexcept
        {
            float32 fv=static_cast<float32>(value);
            const bool r=SliderFloat(ctx,ls,dl,font,label,fv,
                                    static_cast<float32>(vmin),static_cast<float32>(vmax),"%.0f",width);
            value=static_cast<int32>(fv+0.5f);
            return r;
        }

        bool NkUI::SliderFloat2(NkUIContext& ctx,NkUILayoutStack& ls,
                                NkUIDrawList& dl,NkUIFont& font,
                                const char* label,float32* values,float32 vmin,float32 vmax,float32 width) noexcept
        {
            bool changed=false;
            char buf[128]; ::snprintf(buf,sizeof(buf),"%s##0",label);
            SetNextWidth(ctx,ls,width>0?width*0.5f:0);
            changed|=SliderFloat(ctx,ls,dl,font,buf,values[0],vmin,vmax,"%.2f",width>0?width*0.5f:0);
            SameLine(ctx,ls,0,4.f);
            ::snprintf(buf,sizeof(buf),"##%s1",label);
            SetNextWidth(ctx,ls,width>0?width*0.5f:0);
            changed|=SliderFloat(ctx,ls,dl,font,buf,values[1],vmin,vmax,"%.2f",width>0?width*0.5f:0);
            return changed;
        }

        bool NkUI::DragFloat(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,float32& value,
                            float32 speed,float32 vmin,float32 vmax) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 w=100.f+font.MeasureWidth(lp.label)+ctx.GetPaddingX();
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,w,ctx.GetItemHeight());
            const NkUIWidgetState state=CalcState(ctx,id,r);
            bool changed=false;
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)){
                value+=ctx.input.mouseDelta.x*speed;
                if(value<vmin) value=vmin; if(value>vmax) value=vmax;
                changed=(ctx.input.mouseDelta.x!=0);
                if(!ctx.input.mouseDown[0]) ctx.ClearActive();
            }
            DrawWidgetBg(dl,ctx.theme,r,NkUIWidgetType::NK_INPUT_FLOAT,state);
            char buf[32]; ::snprintf(buf,sizeof(buf),"%.3f",value);
            DrawLabel(dl,font,ctx.theme,r,buf,true);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        bool NkUI::DragInt(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,int32& value,float32 speed,int32 vmin,int32 vmax) noexcept
        {
            float32 fv=static_cast<float32>(value);
            const bool r=DragFloat(ctx,ls,dl,font,label,fv,speed,
                                    static_cast<float32>(vmin),static_cast<float32>(vmax));
            value=static_cast<int32>(fv+0.5f);
            if(value<vmin)value=vmin; if(value>vmax)value=vmax;
            return r;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  InputText
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::InputText(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,char* buf,int32 bufSize,float32 width) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 labelW=lp.label[0]?font.MeasureWidth(lp.label)+ctx.GetPaddingX()*2:0;
            const float32 inputW=width>0?width:(200.f);
            const float32 totalW=inputW+labelW;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,totalW,ctx.GetItemHeight());
            const NkRect inputR={r.x,r.y,inputW,r.h};
            const NkUIWidgetState state=CalcState(ctx,id,inputR);
            bool changed=false;

            // Focus au clic
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseClicked(0)){
                ctx.SetFocus(id); ctx.ClearActive();
            }
            // Perte de focus avec Tab ou Escape
            if(ctx.IsFocused(id)){
                if(ctx.input.IsKeyPressed(NkKey::NK_ESCAPE)||ctx.input.IsKeyPressed(NkKey::NK_TAB))
                    ctx.SetFocus(NKUI_ID_NONE);
                // Saisie de caractères
                for(int32 i=0;i<ctx.input.numInputChars;++i){
                    const uint32 c=ctx.input.inputChars[i];
                    if(c=='\b'||c==127){ // backspace/delete
                        const int32 len=static_cast<int32>(::strlen(buf));
                        if(len>0){NkPopLastUTF8(buf); changed=true;}
                    } else if(c>=32){
                        if(NkAppendUTF8(buf, bufSize, c)) changed=true;
                    }
                }
                // Backspace key
                if(ctx.input.IsKeyPressed(NkKey::NK_BACK)){
                    const int32 len=static_cast<int32>(::strlen(buf));
                    if(len>0){NkPopLastUTF8(buf);changed=true;}
                }
            }

            const NkUIWidgetState drawState=ctx.IsFocused(id)?state|NkUIWidgetState::NK_FOCUSED:state;
            DrawWidgetBg(dl,ctx.theme,inputR,NkUIWidgetType::NK_INPUT_TEXT,drawState);
            // Texte ou placeholder
            const char* displayText=buf&&buf[0]?buf:"";
            const NkColor textCol=displayText[0]?ctx.theme.colors.inputText:ctx.theme.colors.inputPlaceholder;
            if(displayText[0]){
                dl.AddRectFilled({inputR.x,inputR.y,inputR.w,inputR.h},{0,0,0,0});
                font.RenderText(dl,{inputR.x+ctx.GetPaddingX(),
                                    inputR.y+(inputR.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                displayText,textCol,inputR.w-ctx.GetPaddingX()*2,true);
            }
            // Curseur clignotant
            if(ctx.IsFocused(id)&&static_cast<int32>(ctx.time*2)%2==0){
                const float32 cx=inputR.x+ctx.GetPaddingX()+font.MeasureWidth(buf);
                const float32 cy=inputR.y+ctx.GetPaddingY();
                dl.AddLine({cx,cy},{cx,cy+inputR.h-ctx.GetPaddingY()*2},
                        ctx.theme.colors.inputCursor,1.5f);
            }
            // Label
            if(lp.label[0])
                font.RenderText(dl,{r.x+inputW+ctx.GetPaddingX()*0.5f,
                                    r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                lp.label,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        bool NkUI::InputInt(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label,int32& value) noexcept {
            char buf[32]; ::snprintf(buf,sizeof(buf),"%d",value);
            if(InputText(ctx,ls,dl,font,label,buf,sizeof(buf))){
                value=static_cast<int32>(::strtol(buf,nullptr,10)); return true;
            }
            return false;
        }

        bool NkUI::InputFloat(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label,float32& value,const char* fmt) noexcept {
            char buf[32]; ::snprintf(buf,sizeof(buf),fmt?fmt:"%.3f",value);
            if(InputText(ctx,ls,dl,font,label,buf,sizeof(buf))){
                value=::strtof(buf,nullptr); return true;
            }
            return false;
        }

        bool NkUI::InputMultiline(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label,char* buf,int32 bufSize,NkVec2 size) noexcept {
            const NkUIID id=ctx.GetID(label);
            const float32 w=size.x>0?size.x:200.f, h=size.y>0?size.y:80.f;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,w,h);
            const NkUIWidgetState state=CalcState(ctx,id,r);
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)) ctx.SetFocus(id);
            bool changed=false;
            if(ctx.IsFocused(id)){
                for(int32 i=0;i<ctx.input.numInputChars;++i){
                    const uint32 c=ctx.input.inputChars[i];
                    const int32 len=static_cast<int32>(::strlen(buf));
                    if(c=='\b'||(int32)c==127){ if(len>0){NkPopLastUTF8(buf);changed=true;} }
                    else if(c=='\n'||c=='\r'){ if(len+1<bufSize){buf[len]='\n';buf[len+1]=0;changed=true;} }
                    else if(c>=32){ if(NkAppendUTF8(buf, bufSize, c)) changed=true; }
                }
            }
            DrawWidgetBg(dl,ctx.theme,r,NkUIWidgetType::NK_INPUT_TEXT,ctx.IsFocused(id)?state|NkUIWidgetState::NK_FOCUSED:state);
            dl.PushClipRect(r);
            font.RenderTextWrapped(dl,{r.x+ctx.GetPaddingX(),r.y+ctx.GetPaddingY(),r.w-ctx.GetPaddingX()*2,r.h-ctx.GetPaddingY()*2},buf,ctx.theme.colors.inputText);
            dl.PopClipRect();
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Combo
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::Combo(NkUIContext& ctx,NkUILayoutStack& ls,
                        NkUIDrawList& dl,NkUIFont& font,
                        const char* label,int32& selected,
                        const char* const* items,int32 numItems,float32 width) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 inputW=width>0?width:160.f;
            const float32 labelW=lp.label[0]?font.MeasureWidth(lp.label)+ctx.GetPaddingX()*2:0;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,inputW+labelW,ctx.GetItemHeight());
            const NkRect comboR={r.x,r.y,inputW,r.h};
            const NkUIWidgetState state=CalcState(ctx,id,comboR);
            bool changed=false;
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseClicked(0)){
                ctx.SetFocus(ctx.IsFocused(id)?NKUI_ID_NONE:id);
                ctx.ClearActive();
            }

            DrawWidgetBg(dl,ctx.theme,comboR,NkUIWidgetType::NK_COMBO,state);
            if(selected>=0&&selected<numItems)
                font.RenderText(dl,{comboR.x+ctx.GetPaddingX(),
                                    comboR.y+(comboR.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                items[selected],ctx.theme.colors.inputText,comboR.w-ctx.theme.metrics.comboArrowW-ctx.GetPaddingX(),true);
            // Flèche
            dl.AddArrow({comboR.x+comboR.w-ctx.theme.metrics.comboArrowW*0.5f,comboR.y+comboR.h*0.5f},
                        8.f,1,ctx.theme.colors.textSecondary);

            // Popup déroulante
            if(ctx.IsFocused(id)){
                const float32 popH=ctx.GetItemHeight()*::fminf(numItems,8.f)+4;
                const NkRect popup={comboR.x,comboR.y+comboR.h,comboR.w,popH};
                // Layer popup
                NkUIDrawList* prevDL=ctx.dl;
                ctx.dl=&ctx.layers[NkUIContext::LAYER_POPUPS];
                ctx.dl->AddRectFilled(popup,ctx.theme.colors.bgPopup,ctx.theme.metrics.cornerRadius);
                ctx.dl->AddRect(popup,ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadius);
                ctx.dl->PushClipRect(popup);
                for(int32 i=0;i<numItems;++i){
                    const NkRect item={popup.x+2,popup.y+2+i*ctx.GetItemHeight(),popup.w-4,ctx.GetItemHeight()};
                    const bool hov=canInteract && ctx.IsHovered(item);
                    if(hov) ctx.dl->AddRectFilled(item,ctx.theme.colors.accent,ctx.theme.metrics.cornerRadiusSm);
                    font.RenderText(*ctx.dl,{item.x+ctx.GetPaddingX(),
                                            item.y+(item.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                    items[i],hov?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary);
                    if(hov&&ctx.input.IsMouseClicked(0)){
                        selected=i; changed=true;
                        ctx.SetFocus(NKUI_ID_NONE);
                    }
                }
                ctx.dl->PopClipRect();
                ctx.dl=prevDL;
                // Ferme si clic ailleurs
                if(canInteract && ctx.input.IsMouseClicked(0)&&!ctx.IsHovered(popup)&&!ctx.IsHovered(comboR))
                    ctx.SetFocus(NKUI_ID_NONE);
            }
            if(lp.label[0])
                font.RenderText(dl,{r.x+inputW+ctx.GetPaddingX()*0.5f,
                                    r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                lp.label,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  ListBox
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::ListBox(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,int32& selected,
                            const char* const* items,int32 numItems,int32 visibleCount) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 h=ctx.GetItemHeight()*visibleCount+4;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,200.f,h);
            const bool canInteract = WindowAllowsInputForWidget(ctx, ctx.currentWindowId, id);

            static float32 sScrollY[256] = {};
            float32& scrollY = sScrollY[id & 255u];
            const float32 itemH = ctx.GetItemHeight();
            const float32 contentH = numItems * itemH + 4.f;
            const float32 maxScroll = contentH > r.h ? (contentH - r.h) : 0.f;

            if (canInteract && ctx.IsHovered(r) && ctx.input.mouseWheel != 0.f) {
                scrollY -= ctx.input.mouseWheel * itemH * 2.f;
            }
            if (scrollY < 0.f) scrollY = 0.f;
            if (scrollY > maxScroll) scrollY = maxScroll;

            const float32 sbW = (contentH > r.h) ? ctx.theme.metrics.scrollbarWidth : 0.f;
            dl.AddRectFilled(r,ctx.theme.colors.inputBg,ctx.theme.metrics.cornerRadius);
            dl.AddRect(r,ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadius);
            dl.PushClipRect(r);
            bool changed=false;
            for(int32 i=0;i<numItems;++i){
                const NkRect item={r.x+2,r.y+2+i*itemH-scrollY,r.w-4-sbW,itemH};
                if(item.y+item.h<r.y||item.y>r.y+r.h) continue;
                const bool sel=(i==selected);
                const bool hov=canInteract && ctx.IsHovered(item);
                if(sel||hov)
                    dl.AddRectFilled(item,sel?ctx.theme.colors.accent:ctx.theme.colors.buttonHover,
                                    ctx.theme.metrics.cornerRadiusSm);
                font.RenderText(dl,{item.x+ctx.GetPaddingX(),
                                    item.y+(item.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                items[i],(sel||hov)?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary);
                if(hov&&ctx.input.IsMouseClicked(0)){selected=i;changed=true;}
            }
            dl.PopClipRect();

            if (contentH > r.h) {
                const NkRect track = {
                    r.x + r.w - ctx.theme.metrics.scrollbarWidth,
                    r.y,
                    ctx.theme.metrics.scrollbarWidth,
                    r.h
                };
                NkUILayout::DrawScrollbar(ctx, dl, true, track, contentH, r.h, scrollY, NkHashInt(id, 101));
            }

            NkUILayout::AdvanceItem(ctx,ls,r);
            return changed;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  ProgressBar
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUI::ProgressBar(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,
                                float32 fraction,NkVec2 size,const char* overlay) noexcept
        {
            const float32 w=size.x>0?size.x:200.f;
            const float32 h=size.y>0?size.y:ctx.GetItemHeight()*0.5f;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,w,h);
            const float32 rx=ctx.theme.metrics.cornerRadius;
            dl.AddRectFilled(r,ctx.theme.colors.sliderTrack,rx);
            const float32 f=fraction<0?0:fraction>1?1:fraction;
            if(f>0){
                NkRect fill=r; fill.w*=f;
                dl.AddRectFilled(fill,ctx.theme.colors.accent,rx);
            }
            dl.AddRect(r,ctx.theme.colors.border,1.f,rx);
            if(overlay&&*overlay){
                NkUIFont fnt; fnt.size=ctx.theme.fonts.small.size;
                fnt.metrics.ascender=fnt.size*0.7f;
                fnt.metrics.spaceWidth=fnt.size*0.4f;
                const float32 ow=fnt.MeasureWidth(overlay);
                fnt.RenderText(dl,{r.x+(r.w-ow)*0.5f,r.y+(r.h-fnt.size)*0.5f+fnt.metrics.ascender*0.5f},
                                overlay,ctx.theme.colors.textPrimary);
            }
            NkUILayout::AdvanceItem(ctx,ls,r);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  ColorButton
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::ColorButton(NkUIContext& ctx,NkUILayoutStack& ls,
                                NkUIDrawList& dl,NkUIFont& font,
                                const char* label,NkColor& col,float32 size) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 tw=font.MeasureWidth(lp.label);
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,size+ctx.GetPaddingX()+tw,ctx.GetItemHeight());
            const NkRect box={r.x,r.y+(r.h-size)*0.5f,size,size};
            const NkUIWidgetState state=CalcState(ctx,id,box);
            // Damier (checkerboard) pour l'alpha
            dl.AddRectFilled(box,NkColor::Gray(200),ctx.theme.metrics.cornerRadiusSm);
            dl.AddRectFilled({box.x,box.y,box.w*0.5f,box.h*0.5f},NkColor::White());
            dl.AddRectFilled({box.x+box.w*0.5f,box.y+box.h*0.5f,box.w*0.5f,box.h*0.5f},NkColor::White());
            // Couleur par-dessus
            dl.AddRectFilled(box,col,ctx.theme.metrics.cornerRadiusSm);
            dl.AddRect(box,HasState(state,NkUIWidgetState::NK_HOVERED)?ctx.theme.colors.borderHover:ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadiusSm);
            if(lp.label[0])
                font.RenderText(dl,{box.x+size+ctx.GetPaddingX(),r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},lp.label,ctx.theme.colors.textPrimary);
            NkUILayout::AdvanceItem(ctx,ls,r);
            (void)id;
            return HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseReleased(0)&&ctx.IsHovered(box);
        }

        bool NkUI::ColorPicker(NkUIContext& ctx,NkUILayoutStack& ls,
                                NkUIDrawList& dl,NkUIFont& font,
                                const char* label,NkColor& col) noexcept
        {
            const float32 sz=ctx.theme.metrics.colorPickerW;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,sz,sz+30);
            dl.AddColorWheel(r,col.r/255.f,col.g/255.f,col.b/255.f);
            (void)font;
            NkUILayout::AdvanceItem(ctx,ls,r);
            return false; // interaction complète en session suivante
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  TreeNode
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUI::TreeNode(NkUIContext& ctx,NkUILayoutStack& ls,
                            NkUIDrawList& dl,NkUIFont& font,
                            const char* label,bool* openPtr) noexcept
        {
            const NkUILabelParts lp=NkParseLabel(label);
            const NkUIID id=ctx.GetID(lp.id[0]?lp.id:lp.label);
            const float32 h=ctx.GetItemHeight();
            NkUILayoutNode* n=ls.Top();
            const float32 indX=n?n->cursor.x:ctx.cursor.x;
            const NkRect r=NkUILayout::NextItemRect(ctx,ls,200.f,h);

            static bool sOpen[512]={};
            const uint32 slot=id%512;
            bool& open=(openPtr?*openPtr:sOpen[slot]);

            const NkUIWidgetState state=CalcState(ctx,id,r);
            if(HasState(state,NkUIWidgetState::NK_ACTIVE)&&ctx.input.IsMouseClicked(0)){
                open=!open; ctx.ClearActive();
            }

            // Fond hover
            if(HasState(state,NkUIWidgetState::NK_HOVERED))
                dl.AddRectFilled(r,ctx.theme.colors.buttonHover,ctx.theme.metrics.cornerRadiusSm);
            // Flèche
            dl.AddArrow({r.x+10,r.y+h*0.5f},7.f,open?1:0,ctx.theme.colors.textSecondary);
            // Label
            font.RenderText(dl,{r.x+22,r.y+(h-font.size)*0.5f+font.metrics.ascender*0.5f},
                            lp.label,ctx.theme.colors.textPrimary);

            NkUILayout::AdvanceItem(ctx,ls,r);

            if(open&&n){
                // Augmente l'indentation
                n->cursor.x+=ctx.theme.metrics.treeIndent;
                n->bounds.x+=ctx.theme.metrics.treeIndent;
                n->bounds.w-=ctx.theme.metrics.treeIndent;
            }
            return open;
        }

        void NkUI::TreePop(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            if(NkUILayoutNode* n=ls.Top()){
                n->cursor.x-=ctx.theme.metrics.treeIndent;
                n->bounds.x-=ctx.theme.metrics.treeIndent;
                n->bounds.w+=ctx.theme.metrics.treeIndent;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Table
        // ─────────────────────────────────────────────────────────────────────────────

        // État de table (stocké dans le contexte via animation slot)
        struct TableState { int32 cols; float32 w; float32 y; int32 row; int32 col; };
        static TableState sTables[64]={};
        static int32 sTableDepth=0;

        bool NkUI::BeginTable(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont&,
                            const char* id,int32 columns,float32 width) noexcept
        {
            if(sTableDepth>=64) return false;
            TableState& ts=sTables[sTableDepth++];
            ts.cols=columns>0?columns:1;
            ts.w=width>0?width:(ls.Top()?ls.Top()->bounds.w:200.f);
            NkUILayoutNode* n=ls.Top();
            ts.y=n?n->cursor.y:ctx.cursor.y;
            ts.row=-1; ts.col=0;
            // Header row background
            const NkRect header={n?n->cursor.x:ctx.cursor.x,ts.y,ts.w,ctx.GetItemHeight()};
            dl.AddRectFilled(header,ctx.theme.colors.bgHeader);
            // Prépare le grid
            NkUI::BeginGrid(ctx,ls,columns,ctx.GetItemHeight());
            (void)id;
            return true;
        }

        void NkUI::TableHeader(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,NkUIFont& font,const char* label) noexcept {
            NkRect r=NkUILayout::NextItemRect(ctx,ls,0,ctx.GetItemHeight());
            font.RenderText(dl,{r.x+ctx.GetPaddingX(),r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                            label,ctx.theme.colors.textOnAccent);
            NkUILayout::AdvanceItem(ctx,ls,r);
        }

        bool NkUI::TableNextRow(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl,bool striped) noexcept {
            if(sTableDepth<=0) return false;
            TableState& ts=sTables[sTableDepth-1];
            ++ts.row; ts.col=0;
            if(NkUILayoutNode* n=ls.Top()){ n->gridCol=0; }
            if(striped&&(ts.row%2==0)){
                NkUILayoutNode* n=ls.Top();
                const NkRect r={n?n->cursor.x:ctx.cursor.x,n?n->cursor.y:ctx.cursor.y,ts.w,ctx.GetItemHeight()};
                dl.AddRectFilled(r,ctx.theme.colors.bgSecondary);
            }
            return true;
        }

        void NkUI::TableNextCell(NkUIContext& ctx,NkUILayoutStack& ls) noexcept {
            if(sTableDepth>0) ++sTables[sTableDepth-1].col;
            (void)ctx;(void)ls;
        }

        void NkUI::EndTable(NkUIContext& ctx,NkUILayoutStack& ls,NkUIDrawList& dl) noexcept {
            EndGrid(ctx,ls);
            if(sTableDepth>0) --sTableDepth;
            (void)dl;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Tooltip
        // ─────────────────────────────────────────────────────────────────────────────

        static char sTooltipText[512]={};
        static float32 sTooltipTimer=0;

        void NkUI::SetTooltip(NkUIContext&,const char* text) noexcept {
            ::strncpy(sTooltipText,text,511);
            sTooltipTimer=0;
        }

        void NkUI::Tooltip(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,const char* text) noexcept {
            if(!text||!*text) return;
            sTooltipTimer+=ctx.dt;
            if(sTooltipTimer<ctx.theme.metrics.tooltipDelay) return;
            const float32 tw=font.MeasureWidth(text);
            const float32 px=ctx.theme.metrics.tooltipPadX;
            const float32 py=ctx.theme.metrics.tooltipPadY;
            float32 tx=ctx.input.mousePos.x+14, ty=ctx.input.mousePos.y+14;
            if(tx+tw+px*2>ctx.viewW) tx=ctx.input.mousePos.x-tw-px*2-14;
            const NkRect r={tx,ty,tw+px*2,font.size+py*2};
            NkUIDrawList& odl=ctx.layers[NkUIContext::LAYER_OVERLAY];
            odl.AddRectFilled(r,ctx.theme.colors.tooltip,ctx.theme.metrics.cornerRadiusSm);
            font.RenderText(odl,{tx+px,ty+py+font.metrics.ascender*0.5f},text,ctx.theme.colors.tooltipText);
        }
    }
} // namespace nkentseu
