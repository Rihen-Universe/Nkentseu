/**
 * @File    NkUIWindow.cpp
 * @Brief   FenÃªtres flottantes NkUI â€” production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf SÃ©deris
 * @License Apache-2.0
 *
 * @FonctionnalitÃ©s
 *  - Barre de titre : drag, double-clic collapse, boutons fermeture/min/max
 *  - DÃ©placement : drag de la barre de titre, contrainte dans le viewport
 *  - Redimensionnement : 8 zones (4 bords + 4 coins), taille min/max
 *  - Scroll automatique si contenu dÃ©passe la zone client
 *  - Z-order : focus au clic, remontÃ©e au premier plan
 *  - Collapse : rÃ©duit Ã  la barre de titre avec animation
 *  - Modal : bloque l'interaction avec les autres fenÃªtres
 *  - Flags : NoMove, NoResize, NoTitleBar, NoScrollbar, NoClose,
 *             AlwaysOnTop, NoBringToFrontOnFocus, AutoSize
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Window manager runtime behavior implementation.
 * Main data: Hit-test/front ordering, title bar controls, client clipping.
 * Change this file when: Move/resize/focus/z-order/scroll region issues appear.
 */
#include "NkUI/NkUIWindow.h"
#include "NkUI/NkUIWidgets.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // Statics
        NkUIWindow::Current NkUIWindow::sStack[8]    = {};
        int32               NkUIWindow::sDepth       = 0;
        NkVec2              NkUIWindow::sNextPos     = {};
        NkVec2              NkUIWindow::sNextSize    = {};
        bool                NkUIWindow::sHasNextPos  = false;
        bool                NkUIWindow::sHasNextSize = false;

        static bool WindowCanCapturePointer(const NkUIContext& ctx, NkUIID windowId) noexcept {
            if (!ctx.wm || windowId == NKUI_ID_NONE) {
                return true;
            }
            if (ctx.wm->movingId != NKUI_ID_NONE) {
                return ctx.wm->movingId == windowId;
            }
            if (ctx.wm->resizingId != NKUI_ID_NONE) {
                return ctx.wm->resizingId == windowId;
            }
            if (ctx.wm->hoveredId != NKUI_ID_NONE) {
                return ctx.wm->hoveredId == windowId;
            }
            return false;
        }

        static float32 BaselineY(const NkRect& r, const NkUIFont& font) noexcept {
            const float32 asc = (font.metrics.ascender > 0.f) ? font.metrics.ascender : (font.size * 0.8f);
            const float32 descRaw = (font.metrics.descender >= 0.f) ? font.metrics.descender : -font.metrics.descender;
            const float32 desc = (descRaw > 0.f) ? descRaw : (font.size * 0.2f);
            const float32 lineH = (font.metrics.lineHeight > 0.f) ? font.metrics.lineHeight : (asc + desc);
            const float32 top = r.y + (r.h - lineH) * 0.5f;
            return top + asc;
        }

        static uint8 ComputeResizeEdge(const NkRect& r,
                                       NkVec2 mp,
                                       float32 borderSize,
                                       bool canInteract) noexcept {
            if (!canInteract) {
                return 0;
            }

            const bool inY = (mp.y >= r.y - borderSize) && (mp.y <= r.y + r.h + borderSize);
            const bool inX = (mp.x >= r.x - borderSize) && (mp.x <= r.x + r.w + borderSize);
            uint8 edge = 0;
            if (inY && mp.x >= r.x - borderSize && mp.x <= r.x + borderSize) edge |= 4;       // left
            if (inY && mp.x >= r.x + r.w - borderSize && mp.x <= r.x + r.w + borderSize) edge |= 8; // right
            if (inX && mp.y >= r.y - borderSize && mp.y <= r.y + borderSize) edge |= 1;       // top
            if (inX && mp.y >= r.y + r.h - borderSize && mp.y <= r.y + r.h + borderSize) edge |= 2; // bottom
            return edge;
        }

        static NkUIMouseCursor CursorFromResizeEdge(uint8 edge) noexcept {
            const bool left = (edge & 4u) != 0u;
            const bool right = (edge & 8u) != 0u;
            const bool top = (edge & 1u) != 0u;
            const bool bottom = (edge & 2u) != 0u;

            if ((left && top) || (right && bottom)) {
                return NkUIMouseCursor::NK_RESIZE_NWSE;
            }
            if ((right && top) || (left && bottom)) {
                return NkUIMouseCursor::NK_RESIZE_NESW;
            }
            if (left || right) {
                return NkUIMouseCursor::NK_RESIZE_WE;
            }
            if (top || bottom) {
                return NkUIMouseCursor::NK_RESIZE_NS;
            }
            return NkUIMouseCursor::NK_ARROW;
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  NkUIWindowManager
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindowManager::Init() noexcept {
            numWindows=0; numZOrder=0;
            ::memset(windows,0,sizeof(windows));
            ::memset(zOrder,0,sizeof(zOrder));
            activeId=NKUI_ID_NONE; hoveredId=NKUI_ID_NONE; movingId=NKUI_ID_NONE; resizingId=NKUI_ID_NONE;
            moveOffset={};
            resizeStartMouse={};
            resizeStartPos={};
            resizeStartSize={};
            resizeEdge=0;
        }
        void NkUIWindowManager::Destroy() noexcept { numWindows=0; }

        NkUIWindowState* NkUIWindowManager::FindOrCreate(const char* name,
                                                            NkVec2 defaultPos,
                                                            NkVec2 defaultSize,
                                                            NkUIWindowFlags flags) noexcept
        {
            const NkUIID id=NkHash(name);
            if(NkUIWindowState* ws=Find(id)) return ws;
            if(numWindows>=MAX_WINDOWS) return nullptr;
            NkUIWindowState& ws=windows[numWindows++];
            ::memset(&ws,0,sizeof(ws));
            ::strncpy(ws.name,name,63);
            ws.id          = id;
            ws.pos         = defaultPos;
            ws.size        = defaultSize;
            ws.flags       = flags;
            ws.isOpen      = true;
            ws.isCollapsed = false;
            ws.zOrder      = numWindows;
            ws.scrollX     = 0; ws.scrollY=0;
            ws.contentSize = {};
            // Ajoute au z-order
            if(numZOrder<MAX_WINDOWS) zOrder[numZOrder++]=id;
            return &ws;
        }

        NkUIWindowState* NkUIWindowManager::Find(NkUIID id) noexcept {
            for(int32 i=0;i<numWindows;++i) if(windows[i].id==id) return &windows[i];
            return nullptr;
        }
        NkUIWindowState* NkUIWindowManager::Find(const char* name) noexcept {
            return Find(NkHash(name));
        }

        void NkUIWindowManager::BringToFront(NkUIID id) noexcept {
            // Donne le z-order max Ã  cette fenÃªtre
            NkUIWindowState* ws = Find(id);
            if (!ws || ws->isDocked) {
                return;
            }
            int32 maxZ=0;
            for(int32 i=0;i<numWindows;++i) if(windows[i].zOrder>maxZ) maxZ=windows[i].zOrder;
            ws->zOrder=maxZ+1;
        }

        void NkUIWindowManager::SortZOrder() noexcept {
            // Trie zOrder[] du plus bas au plus haut z
            for(int32 i=0;i<numZOrder-1;++i){
                for(int32 j=0;j<numZOrder-1-i;++j){
                    NkUIWindowState* a=Find(zOrder[j]);
                    NkUIWindowState* b=Find(zOrder[j+1]);
                    if(a&&b&&a->zOrder>b->zOrder){
                        const NkUIID tmp=zOrder[j]; zOrder[j]=zOrder[j+1]; zOrder[j+1]=tmp;
                    }
                }
            }
            // Les fenêtres "always on top" restent en fin de tableau (rendu au-dessus).
            NkUIID ordered[MAX_WINDOWS]={};
            int32 n=0;
            for(int32 i=0;i<numZOrder;++i){
                NkUIWindowState* ws=Find(zOrder[i]);
                if(ws&&!HasFlag(ws->flags,NkUIWindowFlags::NK_ALWAYS_ON_TOP)){
                    ordered[n++]=zOrder[i];
                }
            }
            for(int32 i=0;i<numZOrder;++i){
                NkUIWindowState* ws=Find(zOrder[i]);
                if(ws&&HasFlag(ws->flags,NkUIWindowFlags::NK_ALWAYS_ON_TOP)){
                    ordered[n++]=zOrder[i];
                }
            }
            for(int32 i=0;i<numZOrder;++i){
                zOrder[i]=ordered[i];
            }
        }

        NkUIWindowState* NkUIWindowManager::HitTest(NkVec2 pos, float32 titleBarHeight) noexcept {
            // Teste dans l'ordre inverse du z-order (dernier = au-dessus)
            SortZOrder();
            for(int32 i=numZOrder-1;i>=0;--i){
                NkUIWindowState* ws=Find(zOrder[i]);
                // Les fenÃªtres dockÃ©es ne participent pas au hit-test flottant.
                if(!ws||!ws->isOpen||ws->isDocked) continue;
                const float32 hitH = ws->isCollapsed
                    ? (HasFlag(ws->flags, NkUIWindowFlags::NK_NO_TITLE_BAR) ? ws->size.y : titleBarHeight)
                    : ws->size.y;
                const NkRect r={ws->pos.x,ws->pos.y,ws->size.x,hitH};
                if(NkRectContains(r,pos)) return ws;
            }
            return nullptr;
        }

        void NkUIWindowManager::BeginFrame(NkUIContext& ctx) noexcept {
            SortZOrder();
            // DÃ©placement en cours
            if(movingId!=NKUI_ID_NONE){
                NkUIWindowState* ws=Find(movingId);
                if (ws && ws->isDocked) {
                    movingId = NKUI_ID_NONE;
                }
                if(ws&&!ws->isDocked&&!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_MOVE)){
                    ws->pos.x=ctx.input.mousePos.x-moveOffset.x;
                    ws->pos.y=ctx.input.mousePos.y-moveOffset.y;
                    // Contrainte dans le viewport (garde la barre de titre visible)
                    const float32 minY=0;
                    const float32 titleH=ctx.theme.metrics.titleBarHeight;
                    if(ws->pos.y<minY) ws->pos.y=minY;
                    if(ws->pos.y+titleH>ctx.viewH) ws->pos.y=static_cast<float32>(ctx.viewH)-titleH;
                    if(ws->pos.x+ws->size.x<40) ws->pos.x=40-ws->size.x;
                    if(ws->pos.x>ctx.viewW-40) ws->pos.x=static_cast<float32>(ctx.viewW)-40;
                }
                if(!ctx.input.mouseDown[0]) movingId=NKUI_ID_NONE;
            }
            // Redimensionnement en cours
            if(resizingId!=NKUI_ID_NONE){
                NkUIWindowState* ws=Find(resizingId);
                if (ws && ws->isDocked) {
                    resizingId = NKUI_ID_NONE;
                    resizeEdge = 0;
                }
                if(ws&&!ws->isDocked&&!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_RESIZE)){
                    ctx.SetMouseCursor(CursorFromResizeEdge(resizeEdge));
                    const float32 mx=ctx.input.mousePos.x, my=ctx.input.mousePos.y;
                    const float32 dx = mx - resizeStartMouse.x;
                    const float32 dy = my - resizeStartMouse.y;
                    const float32 minW=ws->minSize.x>0?ws->minSize.x:100.f;
                    const float32 minH=ws->minSize.y>0?ws->minSize.y:60.f;
                    const float32 maxW=ws->maxSize.x>0?ws->maxSize.x:9999.f;
                    const float32 maxH=ws->maxSize.y>0?ws->maxSize.y:9999.f;
                    // resizeEdge : bitmask 1=N 2=S 4=W 8=E
                    float32 newX = resizeStartPos.x;
                    float32 newY = resizeStartPos.y;
                    float32 newW = resizeStartSize.x;
                    float32 newH = resizeStartSize.y;

                    if (resizeEdge & 4u) {
                        newX = resizeStartPos.x + dx;
                        newW = resizeStartSize.x - dx;
                    }
                    if (resizeEdge & 8u) {
                        newW = resizeStartSize.x + dx;
                    }
                    if (resizeEdge & 1u) {
                        newY = resizeStartPos.y + dy;
                        newH = resizeStartSize.y - dy;
                    }
                    if (resizeEdge & 2u) {
                        newH = resizeStartSize.y + dy;
                    }

                    if (newW < minW) {
                        newW = minW;
                        if (resizeEdge & 4u) {
                            newX = resizeStartPos.x + resizeStartSize.x - newW;
                        }
                    }
                    if (newW > maxW) {
                        newW = maxW;
                        if (resizeEdge & 4u) {
                            newX = resizeStartPos.x + resizeStartSize.x - newW;
                        }
                    }
                    if (newH < minH) {
                        newH = minH;
                        if (resizeEdge & 1u) {
                            newY = resizeStartPos.y + resizeStartSize.y - newH;
                        }
                    }
                    if (newH > maxH) {
                        newH = maxH;
                        if (resizeEdge & 1u) {
                            newY = resizeStartPos.y + resizeStartSize.y - newH;
                        }
                    }

                    ws->pos.x = newX;
                    ws->pos.y = newY;
                    ws->size.x = newW;
                    ws->size.y = newH;
                }
                if(!ctx.input.mouseDown[0]) {
                    resizingId=NKUI_ID_NONE;
                    resizeEdge=0;
                }
            }
            // FenÃªtre actuellement survolÃ©e (top-most sous le curseur)
            NkUIWindowState* hovered = nullptr;
            if (movingId != NKUI_ID_NONE) {
                hovered = Find(movingId);
            } else if (resizingId != NKUI_ID_NONE) {
                hovered = Find(resizingId);
            } else {
                hovered = HitTest(ctx.input.mousePos, ctx.theme.metrics.titleBarHeight);
            }
            hoveredId = hovered ? hovered->id : NKUI_ID_NONE;

            // Focus : clic sur une fenÃªtre
            if(ctx.IsMouseClicked(0)&&movingId==NKUI_ID_NONE&&resizingId==NKUI_ID_NONE){
                if(hovered&&!HasFlag(hovered->flags,NkUIWindowFlags::NK_NO_BRING_TO_FRONT_ON_FOCUS)){
                    BringToFront(hovered->id);
                    activeId=hovered->id;
                } else if (hovered) {
                    activeId = hovered->id;
                } else {
                    activeId = NKUI_ID_NONE;
                }
            }
        }

        void NkUIWindowManager::EndFrame(NkUIContext&) noexcept {
            // Normalise les z-orders (Ã©vite overflow)
            SortZOrder();
            for(int32 i=0;i<numZOrder;++i){
                NkUIWindowState* ws=Find(zOrder[i]);
                if(ws) ws->zOrder=i;
            }
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Dessin de l'ombre
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::DrawShadow(NkUIDrawList& dl,
                                    const NkUIWindowState& ws,bool focused) noexcept
        {
            if(!focused) return; // ombre seulement pour la fenÃªtre active
            const float32 sz=3.f;
            const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
            for(int32 i=2;i>=0;--i){
                const float32 t=static_cast<float32>(i)/2.f;
                const NkColor sc={0,0,0,static_cast<uint8>(30*(1-t))};
                const float32 expand=sz*(1-t)*0.45f;
                dl.AddRectFilled({r.x+1.5f-expand,r.y+1.5f-expand,r.w+expand*2,r.h+expand*2},sc,10.f);
            }
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Dessin de la barre de titre
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::DrawTitleBar(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        NkUIFont& font,
                                        NkUIWindowState& ws) noexcept
        {
            const auto& m=ctx.theme.metrics;
            const auto& c=ctx.theme.colors;
            const bool focused=(ws.id==(ctx.wm?ctx.wm->activeId:NKUI_ID_NONE));
            const NkRect titleR={ws.pos.x,ws.pos.y,ws.size.x,m.titleBarHeight};
            const bool canInteract = WindowCanCapturePointer(ctx, ws.id);

            // Fond de la barre de titre
            const NkColor titleBg=focused?c.titleBarActive:c.titleBarBg;
            const float32 rx=ctx.theme.metrics.cornerRadius;
            // Arrondi seulement en haut (ou complÃ¨tement si collapsed)
            if(ws.isCollapsed){
                dl.AddRectFilled(titleR,titleBg,rx);
            } else {
                // Arrondi haut uniquement
                dl.AddRectFilledCorners(titleR,titleBg,rx,0x3); // bits 0+1 = TL+TR
            }

            // Titre
            if(ws.name[0]){
                const float32 btnW=m.titleBarHeight;
                const float32 btnCount=(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_CLOSE)?1.f:0.f)
                                    +(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_COLLAPSE)?1.f:0.f);
                const float32 textMaxW=titleR.w-m.titleBarPadX*2-btnCount*btnW;
                const float32 ty=BaselineY(titleR, font);
                font.RenderText(dl,{titleR.x+m.titleBarPadX,ty},ws.name,c.titleBarText,textMaxW,true);
            }

            // Boutons Ã  droite
            float32 btnX=ws.pos.x+ws.size.x;
            const float32 btnSz=m.titleBarHeight;
            NkRect closeR{};
            NkRect collapseR{};
            bool hasClose = false;
            bool hasCollapse = false;

            // Bouton fermeture
            if(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_CLOSE)){
                btnX-=btnSz;
                closeR = {btnX,titleR.y,btnSz,btnSz};
                hasClose = true;
                const bool hov=canInteract && NkRectContains(closeR,ctx.input.mousePos);
                if(hov) dl.AddRectFilled(closeR,{200,50,50,200},rx);
                // X
                const float32 cx=closeR.x+btnSz*0.5f, cy=closeR.y+btnSz*0.5f, d=btnSz*0.25f;
                dl.AddLine({cx-d,cy-d},{cx+d,cy+d},c.titleBarText,1.5f);
                dl.AddLine({cx+d,cy-d},{cx-d,cy+d},c.titleBarText,1.5f);
                if(hov&&ctx.ConsumeMouseClick(0)){
                    ws.isOpen=false;
                    ctx.ClearActive();
                }
            }
            // Bouton collapse
            if(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_COLLAPSE)){
                btnX-=btnSz;
                collapseR = {btnX,titleR.y,btnSz,btnSz};
                hasCollapse = true;
                const bool hov=canInteract && NkRectContains(collapseR,ctx.input.mousePos);
                if(hov) dl.AddRectFilled(collapseR,c.titleBarBtnHov,0);
                dl.AddArrow({collapseR.x+btnSz*0.5f,collapseR.y+btnSz*0.5f},7.f,ws.isCollapsed?0:1,c.titleBarText);
                if(hov&&ctx.ConsumeMouseClick(0)){
                    ws.isCollapsed=!ws.isCollapsed;
                    ctx.ClearActive();
                }
            }

            NkRect dragR = titleR;
            float32 dragRight = titleR.x + titleR.w;
            if (hasClose) {
                dragRight = closeR.x;
            }
            if (hasCollapse && collapseR.x < dragRight) {
                dragRight = collapseR.x;
            }
            if (dragRight > dragR.x) {
                dragR.w = dragRight - dragR.x;
            } else {
                dragR.w = 0.f;
            }

            // Double-clic pour collapse
            if(canInteract && NkRectContains(dragR,ctx.input.mousePos)&&ctx.input.mouseDblClick[0]
            &&!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_COLLAPSE)){
                ws.isCollapsed=!ws.isCollapsed;
            }

            // Drag pour dÃ©placement
            if(canInteract && NkRectContains(dragR,ctx.input.mousePos)
            &&ctx.ConsumeMouseClick(0)
            &&!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_MOVE)){
                if(ctx.wm){
                    ctx.wm->movingId=ws.id;
                    ctx.wm->moveOffset={ctx.input.mousePos.x-ws.pos.x,ctx.input.mousePos.y-ws.pos.y};
                    ctx.wm->activeId=ws.id;
                    ctx.wm->BringToFront(ws.id);
                }
            }
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  HandleResize
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::HandleResize(NkUIContext& ctx,
                                NkUIDrawList&,
                                NkUIWindowState& ws) noexcept
{
    if(HasFlag(ws.flags,NkUIWindowFlags::NK_NO_RESIZE)||ws.isCollapsed) return;
    const bool canInteract = WindowCanCapturePointer(ctx, ws.id);
    const float32 rb=ctx.theme.metrics.resizeBorder;
    const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
    const NkVec2 mp=ctx.input.mousePos;

    const uint8 edge = ComputeResizeEdge(r, mp, rb, canInteract);
    if (ctx.wm) {
        uint8 cursorEdge = 0;
        if (ctx.wm->resizingId == ws.id) {
            cursorEdge = ctx.wm->resizeEdge;
        } else if (ctx.wm->resizingId == NKUI_ID_NONE) {
            cursorEdge = edge;
        }
        if (cursorEdge != 0u) {
            ctx.SetMouseCursor(CursorFromResizeEdge(cursorEdge));
        }
    }

    if(edge&&ctx.ConsumeMouseClick(0)&&ctx.wm){
        ctx.wm->resizingId = ws.id;
        ctx.wm->resizeEdge = edge;
        ctx.wm->resizeStartMouse = mp;
        ctx.wm->resizeStartPos = ws.pos;
        ctx.wm->resizeStartSize = ws.size;
        ctx.wm->activeId = ws.id;
        ctx.wm->BringToFront(ws.id);
    }
}
void NkUIWindow::DrawResizeBorders(NkUIDrawList& dl,
                                            const NkUIContext& ctx,
                                            const NkUIWindowState& ws) noexcept
        {
            if(HasFlag(ws.flags,NkUIWindowFlags::NK_NO_RESIZE)||ws.isCollapsed) return;
            const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
            const float32 sz=5.f; // taille de la poignÃ©e de coin
            // PoignÃ©e coin bas-droit
            dl.AddResizeGrip({r.x+r.w-sz-2,r.y+r.h-sz-2},sz,
                            ctx.theme.colors.separator);
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Begin â€” ouvre une fenÃªtre
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        bool NkUIWindow::Begin(NkUIContext& ctx,
                                NkUIWindowManager& wm,
                                NkUIDrawList& dl,
                                NkUIFont& font,
                                NkUILayoutStack& ls,
                                const char* name,
                                bool* pOpen,
                                NkUIWindowFlags flags) noexcept
        {
            if(sDepth >= 8) return false;
            ctx.wm = &wm;

            NkVec2 defPos  = {100.f + sDepth*30.f, 100.f + sDepth*20.f};
            NkVec2 defSz   = {400.f, 300.f};
            bool hadNextPos  = sHasNextPos;
            bool hadNextSize = sHasNextSize;
            if(sHasNextPos)  { defPos = sNextPos;  sHasNextPos  = false; }
            if(sHasNextSize) { defSz  = sNextSize; sHasNextSize = false; }

            NkUIWindowState* ws = wm.FindOrCreate(name, defPos, defSz, flags);
            if(!ws) return false;
            ws->flags = flags;

            const NkUIID prevWindowId = ctx.currentWindowId;
            ctx.currentWindowId = ws->id;

            // CORRECTION : appliquer SetNextWindowPos mÃªme sur fenÃªtres existantes
            if(hadNextPos)  ws->pos  = defPos;
            if(hadNextSize) ws->size = defSz;

            // Sync open state
            if(pOpen) ws->isOpen=*pOpen;
            if(!ws->isOpen){ if(pOpen)*pOpen=false; ctx.currentWindowId = prevWindowId; return false; }

            // Met Ã  jour le flag open
            if(pOpen&&!ws->isOpen){ *pOpen=false; ctx.currentWindowId = prevWindowId; return false; }

            if(ws->isDocked){
                ws->isCollapsed=false;
                const NkRect clientR={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
                dl.PushClipRect(clientR,true);

                const float32 padX = ctx.theme.metrics.windowPadX;
                const float32 padY = ctx.theme.metrics.windowPadY;
                const float32 contentW = (clientR.w - padX * 2.f) > 1.f ? (clientR.w - padX * 2.f) : 1.f;
                const NkVec2 contentStart = {clientR.x + padX, clientR.y + padY};

                const int32 savedDepth = ls.depth;
                NkUILayoutNode* node = ls.Push();
                if(node) {
                    *node = NkUILayoutNode{};
                    node->type     = NkUILayoutType::NK_SCROLL;
                    node->bounds   = {contentStart.x, clientR.y, contentW, clientR.h};
                    node->cursor   = contentStart;
                    node->scrollX  = ws->scrollX;
                    node->scrollY  = ws->scrollY;
                    node->scrollXPtr = &ws->scrollX;
                    node->scrollYPtr = &ws->scrollY;
                    node->id       = ws->id;
                    node->contentSize = {};
                    node->itemCount    = 0;
                }

                ctx.SetCursor(contentStart);
                ctx.cursorStart = contentStart;
                ws->contentSize = {};
                sStack[sDepth++] = {ws, &wm, contentStart, {}, ctx.dl, savedDepth, &ls, prevWindowId};
                return true;
            }

            const bool focused=(wm.activeId==ws->id);
            const float32 titleH=HasFlag(ws->flags,NkUIWindowFlags::NK_NO_TITLE_BAR)?0.f:ctx.theme.metrics.titleBarHeight;
            const float32 rx=ctx.theme.metrics.cornerRadius;

            // â”€â”€ Dessin du fond de fenÃªtre â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            DrawShadow(dl,*ws,focused);

            const NkRect winR={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
            const float32 contentH=ws->isCollapsed?0.f:ws->size.y-titleH;

            // Corps de la fenÃªtre
            if(!ws->isCollapsed){
                dl.AddRectFilled({ws->pos.x,ws->pos.y+titleH,ws->size.x,contentH},
                                ctx.theme.colors.bgWindow,rx);
                // Bordure
                dl.AddRect(winR,focused?ctx.theme.colors.border:ctx.theme.colors.separator,1.f,rx);
            }

            // â”€â”€ Barre de titre â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            if(!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_TITLE_BAR))
                DrawTitleBar(ctx,dl,font,*ws);

            // â”€â”€ Handle resize â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            HandleResize(ctx,dl,*ws);
            DrawResizeBorders(dl,ctx,*ws);

            // Si collapsed, pas de contenu
            if(ws->isCollapsed){
                ctx.currentWindowId = prevWindowId;
                return false;
            }

            // â”€â”€ Zone client â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
            const float32 sbW=ctx.theme.metrics.scrollbarWidth;
            float32 clientW = ws->size.x;
            float32 clientH = contentH;
            if (!HasFlag(ws->flags, NkUIWindowFlags::NK_NO_SCROLLBAR)) {
                bool showV = ws->contentSize.y > clientH;
                bool showH = ws->contentSize.x > clientW;
                if (showV) clientW -= sbW;
                if (showH) clientH -= sbW;
                // Recompute once because one scrollbar can force the other.
                showV = ws->contentSize.y > clientH;
                showH = ws->contentSize.x > clientW;
                if (showV && clientW > ws->size.x - sbW) clientW -= sbW;
                if (showH && clientH > contentH - sbW) clientH -= sbW;
            }
            if (clientW < 1.f) clientW = 1.f;
            if (clientH < 1.f) clientH = 1.f;
            const NkRect clientR={ws->pos.x,ws->pos.y+titleH,clientW,clientH};
            // Window content clip must be independent from previous windows.
            dl.PushClipRect(clientR,false);

            // Curseur de dÃ©part
            // const NkVec2 contentStart={ws->pos.x+ctx.theme.metrics.windowPadX,
            //                             ws->pos.y+titleH+ctx.theme.metrics.windowPadY-ws->scrollY};
            // ctx.SetCursor(contentStart);
            // ctx.cursorStart={ws->pos.x+ctx.theme.metrics.windowPadX,contentStart.y};
            // ws->contentSize={};
            const float32 padX = ctx.theme.metrics.windowPadX;
            const float32 padY = ctx.theme.metrics.windowPadY;
            const float32 contentW = (clientR.w - padX * 2.f) > 1.f ? (clientR.w - padX * 2.f) : 1.f;
            const NkVec2 contentStart = {
                clientR.x + padX,
                ws->pos.y + titleH + padY
            };

            const int32 savedDepth = ls.depth;
            NkUILayoutNode* node = ls.Push();
            if(node) {
                *node = NkUILayoutNode{};
                node->type     = NkUILayoutType::NK_SCROLL;
                // Keep symmetric horizontal padding for window content.
                node->bounds   = {contentStart.x, clientR.y, contentW, clientR.h};
                node->cursor   = contentStart;
                node->scrollX  = ws->scrollX;
                node->scrollY  = ws->scrollY;
                node->scrollXPtr = &ws->scrollX;
                node->scrollYPtr = &ws->scrollY;
                node->id       = ws->id;
                node->contentSize = {};
                node->itemCount    = 0;
            }

            ctx.SetCursor(contentStart);
            ctx.cursorStart = contentStart;
            ws->contentSize = {};

            // Empile
            // sStack[sDepth++]={ws,&wm,contentStart,{}};
            // sStack[sDepth++] = {ws, &wm, contentStart, {}, ctx.dl};
            sStack[sDepth++] = {ws, &wm, contentStart, {}, ctx.dl, savedDepth, &ls, prevWindowId};
            return true;
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  End â€” ferme une fenÃªtre
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::End(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIDrawList& dl, NkUILayoutStack& ls) noexcept
        {
            if(sDepth <= 0) return;
            Current& cur = sStack[--sDepth];
            NkUIWindowState* ws = cur.ws;

            if(cur.savedDL) ctx.dl = cur.savedDL; // toujours restaurer ctx.dl

            if(!ws || ws->isCollapsed) {
                ctx.currentWindowId = cur.savedWindowId;
                return;
            }

            // Contenu rÃ©el : curseur actuel - curseur de dÃ©part
            const float32 cursorContentH = ctx.cursor.y - cur.startCursor.y + ctx.theme.metrics.windowPadY + ws->scrollY;
            ws->contentSize.y = cursorContentH;
            ws->contentSize.x = ctx.cursor.x - cur.startCursor.x + ctx.theme.metrics.windowPadX + ws->scrollX;

            while (ls.depth > cur.savedLSDepth + 1) {
                ls.Pop();
            }
            NkUILayoutNode* node = ls.Top();
            if(node && node->type == NkUILayoutType::NK_SCROLL && node->id == ws->id) {
                ws->contentSize.x = node->contentSize.x;
                ws->contentSize.y = node->contentSize.y;
                ws->scrollX = node->scrollX;
                ws->scrollY = node->scrollY;
                ls.Pop();
            } else if (ls.depth > cur.savedLSDepth) {
                ls.depth = cur.savedLSDepth;
            }
            dl.PopClipRect();

            const float32 titleH=ws->isDocked?0.f:(HasFlag(ws->flags,NkUIWindowFlags::NK_NO_TITLE_BAR)?0.f:ctx.theme.metrics.titleBarHeight);
            const float32 fullViewW = ws->size.x;
            const float32 fullViewH = ws->size.y - titleH;
            const float32 sbW = ctx.theme.metrics.scrollbarWidth;

            bool showV = false;
            bool showH = false;
            if (!HasFlag(ws->flags, NkUIWindowFlags::NK_NO_SCROLLBAR)) {
                showV = ws->contentSize.y > fullViewH;
                float32 viewW = fullViewW - (showV ? sbW : 0.f);
                showH = ws->contentSize.x > viewW;
                float32 viewH = fullViewH - (showH ? sbW : 0.f);
                showV = ws->contentSize.y > viewH;
                viewW = fullViewW - (showV ? sbW : 0.f);
                showH = ws->contentSize.x > viewW;
            }

            const float32 viewW = (fullViewW - (showV ? sbW : 0.f)) > 1.f ? (fullViewW - (showV ? sbW : 0.f)) : 1.f;
            const float32 viewH = (fullViewH - (showH ? sbW : 0.f)) > 1.f ? (fullViewH - (showH ? sbW : 0.f)) : 1.f;
            const float32 maxScrollX = (ws->contentSize.x > viewW) ? (ws->contentSize.x - viewW) : 0.f;
            const float32 maxScrollY = (ws->contentSize.y > viewH) ? (ws->contentSize.y - viewH) : 0.f;
            const NkRect wheelRect = {ws->pos.x, ws->pos.y + titleH, viewW, viewH};

            const bool pointerInWheelRect = NkRectContains(wheelRect, ctx.input.mousePos);
            const bool floatingHoverMatch = (wm.hoveredId == ws->id);
            const bool dockedNotBlockedByFloating = ws->isDocked ? (wm.hoveredId == NKUI_ID_NONE) : false;
            const bool allowWheelForWindow = ws->isDocked
                ? (pointerInWheelRect && dockedNotBlockedByFloating)
                : (pointerInWheelRect && floatingHoverMatch);

            if(!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_SCROLL_WITH_MOUSE)
                && allowWheelForWindow)
            {
                if (!ctx.wheelConsumed && ctx.input.mouseWheel != 0.f) {
                    ws->scrollY -= ctx.input.mouseWheel * ctx.theme.metrics.itemHeight * 3.f;
                    ctx.wheelConsumed = true;
                }
                if (!ctx.wheelHConsumed && ctx.input.mouseWheelH != 0.f) {
                    ws->scrollX -= ctx.input.mouseWheelH * ctx.theme.metrics.itemHeight * 3.f;
                    ctx.wheelHConsumed = true;
                }
            }

            if(ws->scrollX < 0.f) ws->scrollX = 0.f;
            if(ws->scrollX > maxScrollX) ws->scrollX = maxScrollX;
            if(ws->scrollY < 0.f) ws->scrollY = 0.f;
            if(ws->scrollY > maxScrollY) ws->scrollY = maxScrollY;

            if(showV){
                const NkRect track={ws->pos.x + fullViewW - sbW,
                                    ws->pos.y+titleH,
                                    sbW,
                                    viewH > 0.f ? viewH : 1.f};
                NkUILayout::DrawScrollbar(ctx,dl,true,track,ws->contentSize.y,viewH,ws->scrollY,
                                        NkHashInt(ws->id,99));
            }
            if(showH){
                const NkRect track={ws->pos.x,
                                    ws->pos.y + titleH + fullViewH - sbW,
                                    viewW > 0.f ? viewW : 1.f,
                                    sbW};
                NkUILayout::DrawScrollbar(ctx,dl,false,track,ws->contentSize.x,viewW,ws->scrollX,
                                        NkHashInt(ws->id,100));
            }
            ctx.currentWindowId = cur.savedWindowId;
            (void)wm;
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Helpers statiques
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::SetNextWindowPos(NkVec2 pos) noexcept {
            sNextPos=pos; sHasNextPos=true;
        }
        void NkUIWindow::SetNextWindowSize(NkVec2 size) noexcept {
            sNextSize=size; sHasNextSize=true;
        }
        NkVec2 NkUIWindow::GetWindowPos(NkUIWindowManager& wm,const char* name) noexcept {
            NkUIWindowState* ws=wm.Find(name); return ws?ws->pos:NkVec2{};
        }
        NkVec2 NkUIWindow::GetWindowSize(NkUIWindowManager& wm,const char* name) noexcept {
            NkUIWindowState* ws=wm.Find(name); return ws?ws->size:NkVec2{};
        }
        bool NkUIWindow::IsWindowFocused(NkUIWindowManager& wm,const char* name) noexcept {
            NkUIWindowState* ws=wm.Find(name); return ws&&ws->id==wm.activeId;
        }
        bool NkUIWindow::IsWindowHovered(NkUIContext& ctx,NkUIWindowManager& wm,const char* name) noexcept {
            NkUIWindowState* ws=wm.Find(name);
            if(!ws) return false;
            return NkRectContains({ws->pos.x,ws->pos.y,ws->size.x,ws->size.y},ctx.input.mousePos);
        }
        void NkUIWindow::SetWindowPos(NkUIWindowManager& wm,const char* name,NkVec2 pos) noexcept {
            NkUIWindowState* ws=wm.Find(name); if(ws) ws->pos=pos;
        }
        void NkUIWindow::SetWindowSize(NkUIWindowManager& wm,const char* name,NkVec2 size) noexcept {
            NkUIWindowState* ws=wm.Find(name); if(ws) ws->size=size;
        }
        void NkUIWindow::SetWindowCollapsed(NkUIWindowManager& wm,const char* name,bool collapsed) noexcept {
            NkUIWindowState* ws=wm.Find(name); if(ws) ws->isCollapsed=collapsed;
        }
        void NkUIWindow::CloseWindow(NkUIWindowManager& wm,const char* name) noexcept {
            NkUIWindowState* ws=wm.Find(name); if(ws) ws->isOpen=false;
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  RenderAll â€” dessine toutes les fenÃªtres dans l'ordre du z-order
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIWindow::RenderAll(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUIDrawList& dl,
                                    NkUIFont& font) noexcept
        {
            // Fond modal si nÃ©cessaire
            for(int32 i=0;i<wm.numZOrder;++i){
                NkUIWindowState* ws=wm.Find(wm.zOrder[i]);
                if(ws&&ws->isOpen&&HasFlag(ws->flags,NkUIWindowFlags::NK_MODAL)){
                    dl.AddRectFilled({0,0,static_cast<float32>(ctx.viewW),
                                    static_cast<float32>(ctx.viewH)},
                                    ctx.theme.colors.overlay);
                    break;
                }
            }
            // Les fenÃªtres sont dessinÃ©es via Begin/End dans le code utilisateur
            // RenderAll fournit juste le traitement des inputs
            wm.BeginFrame(ctx);
            (void)font;
        }
    }
} // namespace nkentseu


