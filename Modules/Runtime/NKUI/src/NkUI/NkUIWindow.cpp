/**
 * @File    NkUIWindow.cpp
 * @Brief   Fenêtres flottantes NkUI — production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Fonctionnalités
 *  - Barre de titre : drag, double-clic collapse, boutons fermeture/min/max
 *  - Déplacement : drag de la barre de titre, contrainte dans le viewport
 *  - Redimensionnement : 8 zones (4 bords + 4 coins), taille min/max
 *  - Scroll automatique si contenu dépasse la zone client
 *  - Z-order : focus au clic, remontée au premier plan
 *  - Collapse : réduit à la barre de titre avec animation
 *  - Modal : bloque l'interaction avec les autres fenêtres
 *  - Flags : NoMove, NoResize, NoTitleBar, NoScrollbar, NoClose,
 *             AlwaysOnTop, NoBringToFrontOnFocus, AutoSize
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
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIWindowManager
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIWindowManager::Init() noexcept {
            numWindows=0; numZOrder=0;
            ::memset(windows,0,sizeof(windows));
            ::memset(zOrder,0,sizeof(zOrder));
            activeId=NKUI_ID_NONE; hoveredId=NKUI_ID_NONE; movingId=NKUI_ID_NONE; resizingId=NKUI_ID_NONE;
            moveOffset={}; resizeEdge=0;
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
            // Donne le z-order max à cette fenêtre
            int32 maxZ=0;
            for(int32 i=0;i<numWindows;++i) if(windows[i].zOrder>maxZ) maxZ=windows[i].zOrder;
            NkUIWindowState* ws=Find(id);
            if(ws) ws->zOrder=maxZ+1;
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
        }

        NkUIWindowState* NkUIWindowManager::HitTest(NkVec2 pos) noexcept {
            // Teste dans l'ordre inverse du z-order (dernier = au-dessus)
            SortZOrder();
            for(int32 i=numZOrder-1;i>=0;--i){
                NkUIWindowState* ws=Find(zOrder[i]);
                if(!ws||!ws->isOpen) continue;
                const NkRect r={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
                if(NkRectContains(r,pos)) return ws;
            }
            return nullptr;
        }

        void NkUIWindowManager::BeginFrame(NkUIContext& ctx) noexcept {
            SortZOrder();
            // Déplacement en cours
            if(movingId!=NKUI_ID_NONE){
                NkUIWindowState* ws=Find(movingId);
                if(ws&&!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_MOVE)){
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
                if(ws&&!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_RESIZE)){
                    const float32 mx=ctx.input.mousePos.x, my=ctx.input.mousePos.y;
                    const float32 minW=ws->minSize.x>0?ws->minSize.x:100.f;
                    const float32 minH=ws->minSize.y>0?ws->minSize.y:60.f;
                    const float32 maxW=ws->maxSize.x>0?ws->maxSize.x:9999.f;
                    const float32 maxH=ws->maxSize.y>0?ws->maxSize.y:9999.f;
                    // resizeEdge : bitmask 1=N 2=S 4=W 8=E
                    if(resizeEdge&4){ // gauche
                        float32 newX=mx, newW=ws->pos.x+ws->size.x-newX;
                        if(newW<minW){newW=minW;newX=ws->pos.x+ws->size.x-minW;}
                        if(newW>maxW){newW=maxW;newX=ws->pos.x+ws->size.x-maxW;}
                        ws->pos.x=newX; ws->size.x=newW;
                    }
                    if(resizeEdge&8){ // droite
                        float32 newW=mx-ws->pos.x;
                        if(newW<minW)newW=minW; if(newW>maxW)newW=maxW;
                        ws->size.x=newW;
                    }
                    if(resizeEdge&1){ // haut
                        float32 newY=my, newH=ws->pos.y+ws->size.y-newY;
                        if(newH<minH){newH=minH;newY=ws->pos.y+ws->size.y-minH;}
                        if(newH>maxH){newH=maxH;newY=ws->pos.y+ws->size.y-maxH;}
                        ws->pos.y=newY; ws->size.y=newH;
                    }
                    if(resizeEdge&2){ // bas
                        float32 newH=my-ws->pos.y;
                        if(newH<minH)newH=minH; if(newH>maxH)newH=maxH;
                        ws->size.y=newH;
                    }
                }
                if(!ctx.input.mouseDown[0]) resizingId=NKUI_ID_NONE;
            }
            // Fenêtre actuellement survolée (top-most sous le curseur)
            NkUIWindowState* hovered = nullptr;
            if (movingId != NKUI_ID_NONE) {
                hovered = Find(movingId);
            } else if (resizingId != NKUI_ID_NONE) {
                hovered = Find(resizingId);
            } else {
                hovered = HitTest(ctx.input.mousePos);
            }
            hoveredId = hovered ? hovered->id : NKUI_ID_NONE;

            // Focus : clic sur une fenêtre
            if(ctx.input.IsMouseClicked(0)&&movingId==NKUI_ID_NONE&&resizingId==NKUI_ID_NONE){
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
            // Normalise les z-orders (évite overflow)
            SortZOrder();
            for(int32 i=0;i<numZOrder;++i){
                NkUIWindowState* ws=Find(zOrder[i]);
                if(ws) ws->zOrder=i;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Dessin de l'ombre
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIWindow::DrawShadow(NkUIDrawList& dl,
                                    const NkUIWindowState& ws,bool focused) noexcept
        {
            if(!focused) return; // ombre seulement pour la fenêtre active
            const float32 sz=8.f;
            const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
            for(int32 i=3;i>=0;--i){
                const float32 t=static_cast<float32>(i)/3.f;
                const NkColor sc={0,0,0,static_cast<uint8>(40*(1-t))};
                const float32 expand=sz*(1-t)*0.5f;
                dl.AddRectFilled({r.x+3-expand,r.y+3-expand,r.w+expand*2,r.h+expand*2},sc,10.f);
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Dessin de la barre de titre
        // ─────────────────────────────────────────────────────────────────────────────

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
            // Arrondi seulement en haut (ou complètement si collapsed)
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
                const float32 ty=titleR.y+(titleR.h-font.size)*0.5f+font.metrics.ascender*0.5f;
                font.RenderText(dl,{titleR.x+m.titleBarPadX,ty},ws.name,c.titleBarText,textMaxW,true);
            }

            // Boutons à droite
            float32 btnX=ws.pos.x+ws.size.x;
            const float32 btnSz=m.titleBarHeight;

            // Bouton fermeture
            if(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_CLOSE)){
                btnX-=btnSz;
                const NkRect closeR={btnX,titleR.y,btnSz,btnSz};
                const bool hov=canInteract && NkRectContains(closeR,ctx.input.mousePos);
                if(hov) dl.AddRectFilled(closeR,{200,50,50,200},rx);
                // X
                const float32 cx=closeR.x+btnSz*0.5f, cy=closeR.y+btnSz*0.5f, d=btnSz*0.25f;
                dl.AddLine({cx-d,cy-d},{cx+d,cy+d},c.titleBarText,1.5f);
                dl.AddLine({cx+d,cy-d},{cx-d,cy+d},c.titleBarText,1.5f);
                if(hov&&ctx.input.IsMouseClicked(0)){
                    ws.isOpen=false;
                    ctx.ClearActive();
                }
            }
            // Bouton collapse
            if(!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_COLLAPSE)){
                btnX-=btnSz;
                const NkRect colR={btnX,titleR.y,btnSz,btnSz};
                const bool hov=canInteract && NkRectContains(colR,ctx.input.mousePos);
                if(hov) dl.AddRectFilled(colR,c.titleBarBtnHov,0);
                dl.AddArrow({colR.x+btnSz*0.5f,colR.y+btnSz*0.5f},7.f,ws.isCollapsed?0:1,c.titleBarText);
                if(hov&&ctx.input.IsMouseClicked(0)){
                    ws.isCollapsed=!ws.isCollapsed;
                    ctx.ClearActive();
                }
            }

            // Double-clic pour collapse
            if(canInteract && NkRectContains(titleR,ctx.input.mousePos)&&ctx.input.mouseDblClick[0]
            &&!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_COLLAPSE)){
                ws.isCollapsed=!ws.isCollapsed;
            }

            // Drag pour déplacement
            if(canInteract && NkRectContains(titleR,ctx.input.mousePos)
            &&ctx.input.IsMouseClicked(0)
            &&!HasFlag(ws.flags,NkUIWindowFlags::NK_NO_MOVE)){
                if(ctx.wm){
                    ctx.wm->movingId=ws.id;
                    ctx.wm->moveOffset={ctx.input.mousePos.x-ws.pos.x,ctx.input.mousePos.y-ws.pos.y};
                    ctx.wm->BringToFront(ws.id);
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  HandleResize
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIWindow::HandleResize(NkUIContext& ctx,
                                        NkUIDrawList&,
                                        NkUIWindowState& ws) noexcept
        {
            if(HasFlag(ws.flags,NkUIWindowFlags::NK_NO_RESIZE)||ws.isCollapsed) return;
            const bool canInteract = WindowCanCapturePointer(ctx, ws.id);
            const float32 rb=ctx.theme.metrics.resizeBorder;
            const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
            const NkVec2 mp=ctx.input.mousePos;

            // Détermine le bitmask d'arête sous le curseur
            uint8 edge=0;
            if(canInteract && mp.x>=r.x-rb&&mp.x<=r.x+rb){ edge|=4; } // gauche
            if(canInteract && mp.x>=r.x+r.w-rb&&mp.x<=r.x+r.w+rb){ edge|=8; } // droite
            if(canInteract && mp.y>=r.y-rb&&mp.y<=r.y+rb){ edge|=1; } // haut
            if(canInteract && mp.y>=r.y+r.h-rb&&mp.y<=r.y+r.h+rb){ edge|=2; } // bas

            if(edge&&ctx.input.IsMouseClicked(0)&&ctx.wm){
                ctx.wm->resizingId=ws.id;
                ctx.wm->resizeEdge=edge;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawResizeBorders
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIWindow::DrawResizeBorders(NkUIDrawList& dl,
                                            const NkUIContext& ctx,
                                            const NkUIWindowState& ws) noexcept
        {
            if(HasFlag(ws.flags,NkUIWindowFlags::NK_NO_RESIZE)||ws.isCollapsed) return;
            const NkRect r={ws.pos.x,ws.pos.y,ws.size.x,ws.size.y};
            const float32 sz=8.f; // taille de la poignée de coin
            // Poignée coin bas-droit
            dl.AddResizeGrip({r.x+r.w-sz-2,r.y+r.h-sz-2},sz,
                            ctx.theme.colors.separator);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Begin — ouvre une fenêtre
        // ─────────────────────────────────────────────────────────────────────────────

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

            const NkUIID prevWindowId = ctx.currentWindowId;
            ctx.currentWindowId = ws->id;

            // CORRECTION : appliquer SetNextWindowPos même sur fenêtres existantes
            if(hadNextPos)  ws->pos  = defPos;
            if(hadNextSize) ws->size = defSz;

            // Sync open state
            if(pOpen) ws->isOpen=*pOpen;
            if(!ws->isOpen){ if(pOpen)*pOpen=false; ctx.currentWindowId = prevWindowId; return false; }

            // Met à jour le flag open
            if(pOpen&&!ws->isOpen){ *pOpen=false; ctx.currentWindowId = prevWindowId; return false; }

            const bool focused=(wm.activeId==ws->id);
            const float32 titleH=HasFlag(flags,NkUIWindowFlags::NK_NO_TITLE_BAR)?0.f:ctx.theme.metrics.titleBarHeight;
            const float32 rx=ctx.theme.metrics.cornerRadius;

            // ── Dessin du fond de fenêtre ─────────────────────────────────────────────
            DrawShadow(dl,*ws,focused);

            const NkRect winR={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
            const float32 contentH=ws->isCollapsed?0.f:ws->size.y-titleH;

            // Corps de la fenêtre
            if(!ws->isCollapsed){
                dl.AddRectFilled({ws->pos.x,ws->pos.y+titleH,ws->size.x,contentH},
                                ctx.theme.colors.bgWindow,rx);
                // Bordure
                dl.AddRect(winR,focused?ctx.theme.colors.border:ctx.theme.colors.separator,1.f,rx);
            }

            // ── Barre de titre ────────────────────────────────────────────────────────
            if(!HasFlag(flags,NkUIWindowFlags::NK_NO_TITLE_BAR))
                DrawTitleBar(ctx,dl,font,*ws);

            // ── Handle resize ─────────────────────────────────────────────────────────
            HandleResize(ctx,dl,*ws);
            DrawResizeBorders(dl,ctx,*ws);

            // Si collapsed, pas de contenu
            if(ws->isCollapsed){
                ctx.currentWindowId = prevWindowId;
                return false;
            }

            // ── Zone client ───────────────────────────────────────────────────────────
            const float32 sbW=HasFlag(flags,NkUIWindowFlags::NK_NO_SCROLLBAR)?0.f:ctx.theme.metrics.scrollbarWidth;
            const NkRect clientR={ws->pos.x,ws->pos.y+titleH,ws->size.x-sbW,contentH};
            dl.PushClipRect(clientR,true);

            // Curseur de départ
            // const NkVec2 contentStart={ws->pos.x+ctx.theme.metrics.windowPadX,
            //                             ws->pos.y+titleH+ctx.theme.metrics.windowPadY-ws->scrollY};
            // ctx.SetCursor(contentStart);
            // ctx.cursorStart={ws->pos.x+ctx.theme.metrics.windowPadX,contentStart.y};
            // ws->contentSize={};
            const NkVec2 contentStart = {
                ws->pos.x + ctx.theme.metrics.windowPadX,
                ws->pos.y + titleH + ctx.theme.metrics.windowPadY
            };

            const int32 savedDepth = ls.depth;
            NkUILayoutNode* node = ls.Push();
            if(node) {
                node->type     = NkUILayoutType::NK_SCROLL;
                node->bounds   = clientR;  // rect client sans scrollbar
                node->cursor   = contentStart;
                node->scrollY  = ws->scrollY;
                node->scrollYPtr = &ws->scrollY;
                node->id       = ws->id;
                node->contentSize = {};
                node->itemCount    = 0;
            }

            ctx.SetCursor(contentStart);
            ctx.cursorStart = {ws->pos.x + ctx.theme.metrics.windowPadX, contentStart.y};
            ws->contentSize = {};

            // Empile
            // sStack[sDepth++]={ws,&wm,contentStart,{}};
            // sStack[sDepth++] = {ws, &wm, contentStart, {}, ctx.dl};
            sStack[sDepth++] = {ws, &wm, contentStart, {}, ctx.dl, savedDepth, &ls, prevWindowId};
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  End — ferme une fenêtre
        // ─────────────────────────────────────────────────────────────────────────────

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

            // Contenu réel : curseur actuel - curseur de départ
            const float32 contentH=ctx.cursor.y-cur.startCursor.y+ctx.theme.metrics.windowPadY+ws->scrollY;
            ws->contentSize.y=contentH;

            while (ls.depth > cur.savedLSDepth + 1) {
                ls.Pop();
            }
            NkUILayoutNode* node = ls.Top();
            if(node && node->type == NkUILayoutType::NK_SCROLL && node->id == ws->id) {
                ws->contentSize.y = node->contentSize.y;
                ls.Pop();
            } else if (ls.depth > cur.savedLSDepth) {
                ls.depth = cur.savedLSDepth;
            }
            dl.PopClipRect();

            // Scrollbar verticale si nécessaire
            const float32 titleH=HasFlag(ws->flags,NkUIWindowFlags::NK_NO_TITLE_BAR)?0.f:ctx.theme.metrics.titleBarHeight;
            const float32 viewH=ws->size.y-titleH;
            const NkRect clientR = {ws->pos.x, ws->pos.y + titleH, ws->size.x, viewH};
            const float32 maxScroll = (contentH > viewH) ? (contentH - viewH) : 0.f;

            if(!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_SCROLL_WITH_MOUSE)
                && wm.hoveredId == ws->id
                && NkRectContains(clientR, ctx.input.mousePos)
                && ctx.input.mouseWheel != 0.f)
            {
                ws->scrollY -= ctx.input.mouseWheel * ctx.theme.metrics.itemHeight * 3.f;
            }

            if(ws->scrollY < 0.f) ws->scrollY = 0.f;
            if(ws->scrollY > maxScroll) ws->scrollY = maxScroll;

            if(!HasFlag(ws->flags,NkUIWindowFlags::NK_NO_SCROLLBAR)&&contentH>viewH){
                const NkRect track={ws->pos.x+ws->size.x-ctx.theme.metrics.scrollbarWidth,
                                    ws->pos.y+titleH,
                                    ctx.theme.metrics.scrollbarWidth,
                                    viewH};
                NkUILayout::DrawScrollbar(ctx,dl,true,track,contentH,viewH,ws->scrollY,
                                        NkHashInt(ws->id,99));
            }
            ctx.currentWindowId = cur.savedWindowId;
            (void)wm;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Helpers statiques
        // ─────────────────────────────────────────────────────────────────────────────

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

        // ─────────────────────────────────────────────────────────────────────────────
        //  RenderAll — dessine toutes les fenêtres dans l'ordre du z-order
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIWindow::RenderAll(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUIDrawList& dl,
                                    NkUIFont& font) noexcept
        {
            // Fond modal si nécessaire
            for(int32 i=0;i<wm.numZOrder;++i){
                NkUIWindowState* ws=wm.Find(wm.zOrder[i]);
                if(ws&&ws->isOpen&&HasFlag(ws->flags,NkUIWindowFlags::NK_MODAL)){
                    dl.AddRectFilled({0,0,static_cast<float32>(ctx.viewW),
                                    static_cast<float32>(ctx.viewH)},
                                    ctx.theme.colors.overlay);
                    break;
                }
            }
            // Les fenêtres sont dessinées via Begin/End dans le code utilisateur
            // RenderAll fournit juste le traitement des inputs
            wm.BeginFrame(ctx);
            (void)font;
        }
    }
} // namespace nkentseu
