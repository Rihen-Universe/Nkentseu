/**
 * @File    NkUIDock.cpp
 * @Brief   Système de docking NkUI — production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Architecture
 *  L'arbre de dock est un arbre binaire de NkUIDockNode.
 *  Chaque nœud est soit :
 *    - Root  : racine de tout l'espace
 *    - Leaf  : contient des fenêtres (affichées en tabs)
 *    - SplitH: deux enfants côte à côte (gauche/droite)
 *    - SplitV: deux enfants l'un au-dessus de l'autre (haut/bas)
 *
 *  Quand on ancre une fenêtre :
 *    1. On choisit un nœud cible et une direction (L/R/T/B/Center)
 *    2. Si Center : la fenêtre va dans le Leaf (nouveau tab)
 *    3. Si L/R/T/B : le Leaf se transforme en Split, ses fenêtres
 *       restent dans un nouveau Leaf enfant, la nouvelle fenêtre
 *       va dans l'autre Leaf enfant.
 *
 *  Les dropzones sont calculées en temps réel pendant le drag
 *  et dessinées en overlay.
 */
#include "NkUI/NkUIDock.h"
#include "NkUI/NkUIWidgets.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIDockManager — Init / Destroy
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::Init(NkRect viewport) noexcept {
            numNodes=0; rootIdx=-1;
            isDragging=false; isDraggingTab=false;
            dragWindowId=NKUI_ID_NONE; dragTargetNode=-1;
            ::memset(nodes,0,sizeof(nodes));
            // Crée le nœud racine
            rootIdx=AllocNode();
            if(rootIdx>=0){
                nodes[rootIdx].type=NkUIDockNodeType::NK_ROOT;
                nodes[rootIdx].rect=viewport;
            }
        }

        void NkUIDockManager::Destroy() noexcept { numNodes=0; rootIdx=-1; }

        void NkUIDockManager::SetViewport(NkRect r) noexcept {
            if(rootIdx>=0) nodes[rootIdx].rect=r;
            RecalcRectsAll();
        }

        int32 NkUIDockManager::AllocNode() noexcept {
            if(numNodes>=MAX_NODES) return -1;
            const int32 idx=numNodes++;
            ::memset((void*)&nodes[idx],0,sizeof(NkUIDockNode));
            nodes[idx].splitRatio=0.5f;
            nodes[idx].activeTab =0;
            nodes[idx].parent    =-1;
            nodes[idx].children[0]=nodes[idx].children[1]=-1;
            return idx;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RecalcRects — recalcule les rects enfants depuis le parent
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::RecalcRects(int32 idx) noexcept {
            if(idx<0||idx>=numNodes) return;
            NkUIDockNode& n=nodes[idx];
            if(n.type==NkUIDockNodeType::NK_LEAF||n.type==NkUIDockNodeType::NK_ROOT) return;

            const int32 c0=n.children[0], c1=n.children[1];
            if(c0<0||c1<0) return;

            if(n.type==NkUIDockNodeType::NK_SPLIT_H){
                const float32 w0=n.rect.w*n.splitRatio;
                nodes[c0].rect={n.rect.x,n.rect.y,w0,n.rect.h};
                nodes[c1].rect={n.rect.x+w0,n.rect.y,n.rect.w-w0,n.rect.h};
            } else { // SplitV
                const float32 h0=n.rect.h*n.splitRatio;
                nodes[c0].rect={n.rect.x,n.rect.y,n.rect.w,h0};
                nodes[c1].rect={n.rect.x,n.rect.y+h0,n.rect.w,n.rect.h-h0};
            }
            RecalcRects(c0);
            RecalcRects(c1);
        }

        void NkUIDockManager::RecalcRectsAll() noexcept { RecalcRects(rootIdx); }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DockWindow — ancre une fenêtre dans un nœud
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::DockWindow(NkUIWindowManager& wm,
                                        NkUIID windowId, int32 nodeIdx,
                                        NkUIDockDrop drop) noexcept
        {
            if(nodeIdx<0||nodeIdx>=numNodes) return false;
            NkUIWindowState* ws=wm.Find(windowId);
            if(!ws) return false;

            if(drop==NkUIDockDrop::NK_CENTER||drop==NkUIDockDrop::NK_TAB){
                // Ajoute la fenêtre dans ce Leaf
                NkUIDockNode& node=nodes[nodeIdx];
                if(node.numWindows>=NkUIDockNode::MAX_WINDOWS) return false;
                node.windows[node.numWindows++]=windowId;
                node.activeTab=node.numWindows-1;
                node.type=NkUIDockNodeType::NK_LEAF;
                ws->isDocked=true; ws->dockNodeId=static_cast<NkUIID>(nodeIdx);
                return true;
            }

            // Split : crée deux Leaf enfants
            const int32 c0=AllocNode(); if(c0<0) return false;
            const int32 c1=AllocNode(); if(c1<0) return false;

            NkUIDockNode& target=nodes[nodeIdx];

            // Détermine l'orientation
            const bool horizontal=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_RIGHT);
            target.type=horizontal?NkUIDockNodeType::NK_SPLIT_H:NkUIDockNodeType::NK_SPLIT_V;
            target.children[0]=c0; target.children[1]=c1;
            target.splitRatio=0.5f;

            // Copie les fenêtres existantes dans le premier enfant
            nodes[c0]=NkUIDockNode{};
            nodes[c0].type=NkUIDockNodeType::NK_LEAF;
            nodes[c0].parent=nodeIdx;
            nodes[c0].splitRatio=0.5f;
            for(int32 i=0;i<target.numWindows;++i)
                nodes[c0].windows[nodes[c0].numWindows++]=target.windows[i];
            nodes[c0].activeTab=target.activeTab;
            target.numWindows=0;

            // La nouvelle fenêtre va dans le second enfant
            nodes[c1]=NkUIDockNode{};
            nodes[c1].type=NkUIDockNodeType::NK_LEAF;
            nodes[c1].parent=nodeIdx;
            nodes[c1].splitRatio=0.5f;

            // Direction : Left/Top → nouvelle fenêtre à gauche/haut (c0), sinon c1
            const int32 newWindowChild=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_TOP)?0:1;
            const int32 existingChild=1-newWindowChild;
            if(newWindowChild==0){
                // Échange : la nouvelle fenêtre va dans c0
                nodes[c0].numWindows=0;
                nodes[c0].windows[nodes[c0].numWindows++]=windowId;
                nodes[c0].activeTab=0;
                for(int32 i=0;i<(int32)target.numWindows;++i)
                    nodes[c1].windows[nodes[c1].numWindows++]=target.windows[i];
                nodes[c1].activeTab=target.activeTab;
            } else {
                nodes[c1].windows[nodes[c1].numWindows++]=windowId;
                nodes[c1].activeTab=0;
            }
            (void)existingChild;

            ws->isDocked=true; ws->dockNodeId=static_cast<NkUIID>(newWindowChild==0?c0:c1);

            // Recalcule les rects
            RecalcRects(nodeIdx);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  UndockWindow — détache une fenêtre du dock
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::UndockWindow(NkUIWindowManager& wm, NkUIID windowId) noexcept {
            NkUIWindowState* ws=wm.Find(windowId);
            if(!ws||!ws->isDocked) return;
            const int32 nodeIdx=static_cast<int32>(ws->dockNodeId);
            if(nodeIdx<0||nodeIdx>=numNodes) return;
            NkUIDockNode& node=nodes[nodeIdx];
            // Retire la fenêtre du nœud
            for(int32 i=0;i<node.numWindows;++i){
                if(node.windows[i]==windowId){
                    for(int32 j=i;j<node.numWindows-1;++j) node.windows[j]=node.windows[j+1];
                    --node.numWindows;
                    if(node.activeTab>=node.numWindows&&node.activeTab>0) --node.activeTab;
                    break;
                }
            }
            ws->isDocked=false; ws->dockNodeId=0;
            // Si le nœud est vide et a un parent, on peut le supprimer
            // (simplification : on laisse le nœud vide pour l'instant)
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  FindNodeAt — trouve le Leaf sous un point
        // ─────────────────────────────────────────────────────────────────────────────

        int32 NkUIDockManager::FindNodeAt(NkVec2 pos) const noexcept {
            for(int32 i=0;i<numNodes;++i){
                if(nodes[i].type!=NkUIDockNodeType::NK_LEAF) continue;
                if(NkRectContains(nodes[i].rect,pos)) return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  DrawDropZones — dessine les zones de dépôt pendant le drag
        // ─────────────────────────────────────────────────────────────────────────────

        NkUIDockDrop NkUIDockManager::DrawDropZones(NkUIContext& ctx,
                                                    NkUIDrawList& dl,
                                                    int32 nodeIdx) noexcept
        {
            if(nodeIdx<0||nodeIdx>=numNodes) return NkUIDockDrop::NK_NONE;
            const NkRect& r=nodes[nodeIdx].rect;
            const float32 sz=ctx.theme.metrics.dockZoneSize;
            const NkColor zoneCol=ctx.theme.colors.dockZone;
            const NkColor zoneBorder=ctx.theme.colors.dockZoneBorder;
            const float32 rx=8.f;
            const NkVec2 mp=ctx.input.mousePos;

            struct Zone { NkUIDockDrop drop; NkRect rect; };
            const Zone zones[]={
                {NkUIDockDrop::NK_LEFT,   {r.x+r.w*0.1f,        r.y+r.h*0.5f-sz*0.5f, sz, sz}},
                {NkUIDockDrop::NK_RIGHT,  {r.x+r.w*0.9f-sz,     r.y+r.h*0.5f-sz*0.5f, sz, sz}},
                {NkUIDockDrop::NK_TOP,    {r.x+r.w*0.5f-sz*0.5f,r.y+r.h*0.1f,          sz, sz}},
                {NkUIDockDrop::NK_BOTTOM, {r.x+r.w*0.5f-sz*0.5f,r.y+r.h*0.9f-sz,       sz, sz}},
                {NkUIDockDrop::NK_CENTER, {r.x+r.w*0.5f-sz*0.5f,r.y+r.h*0.5f-sz*0.5f,  sz, sz}},
            };

            NkUIDockDrop hovered=NkUIDockDrop::NK_NONE;
            for(const auto& z:zones){
                const bool hov=NkRectContains(z.rect,mp);
                if(hov) hovered=z.drop;
                dl.AddRectFilled(z.rect,hov?zoneBorder.WithAlpha(180):zoneCol,rx);
                dl.AddRect(z.rect,zoneBorder,2.f,rx);
                // Icône directionnelle
                const NkVec2 c={z.rect.x+z.rect.w*0.5f, z.rect.y+z.rect.h*0.5f};
                switch(z.drop){
                    case NkUIDockDrop::NK_LEFT:   dl.AddArrow(c,sz*0.3f,2,zoneBorder); break;
                    case NkUIDockDrop::NK_RIGHT:  dl.AddArrow(c,sz*0.3f,0,zoneBorder); break;
                    case NkUIDockDrop::NK_TOP:    dl.AddArrow(c,sz*0.3f,3,zoneBorder); break;
                    case NkUIDockDrop::NK_BOTTOM: dl.AddArrow(c,sz*0.3f,1,zoneBorder); break;
                    case NkUIDockDrop::NK_CENTER:
                        dl.AddRectFilled({c.x-sz*0.2f,c.y-sz*0.2f,sz*0.4f,sz*0.4f},zoneBorder,4.f);
                        break;
                    default: break;
                }
            }
            return hovered;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  RenderNode — dessine un nœud et ses enfants
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::RenderNode(NkUIContext& ctx,
                                        NkUIDrawList& dl,
                                        NkUIFont& font,
                                        NkUIWindowManager& wm,
                                        int32 idx,
                                        NkUILayoutStack& ls) noexcept
        {
            if(idx<0||idx>=numNodes) return;
            NkUIDockNode& node=nodes[idx];

            if(node.type==NkUIDockNodeType::NK_SPLIT_H||node.type==NkUIDockNodeType::NK_SPLIT_V){
                // Splitter draggable
                const bool vertical=(node.type==NkUIDockNodeType::NK_SPLIT_H);
                const NkUIID splitId=NkHashInt(idx,0xDC);
                NkUILayout::DrawSplitter(ctx,dl,node.rect,vertical,node.splitRatio,splitId);
                if(node.splitRatio<0.05f)node.splitRatio=0.05f;
                if(node.splitRatio>0.95f)node.splitRatio=0.95f;
                RecalcRects(idx);
                // Render les enfants
                RenderNode(ctx,dl,font,wm,node.children[0],ls);
                RenderNode(ctx,dl,font,wm,node.children[1],ls);
                return;
            }

            if(node.type!=NkUIDockNodeType::NK_LEAF&&node.type!=NkUIDockNodeType::NK_ROOT) return;
            if(node.numWindows==0) return;

            const NkRect& r=node.rect;
            const float32 tabH=ctx.theme.metrics.dockTabHeight;

            // Barre d'onglets
            const float32 tabW=r.w/node.numWindows;
            dl.AddRectFilled({r.x,r.y,r.w,tabH},ctx.theme.colors.bgHeader);

            for(int32 t=0;t<node.numWindows;++t){
                const NkRect tabR={r.x+t*tabW,r.y,tabW,tabH};
                const bool active=(t==node.activeTab);
                const bool hov=NkRectContains(tabR,ctx.input.mousePos);

                dl.AddRectFilled(tabR,active?ctx.theme.colors.tabActive:
                                (hov?ctx.theme.colors.buttonHover:ctx.theme.colors.tabBg));

                // Label de l'onglet
                NkUIWindowState* ws=wm.Find(node.windows[t]);
                if(ws){
                    font.RenderText(dl,{tabR.x+ctx.theme.metrics.paddingX,
                                        tabR.y+(tabH-font.size)*0.5f+font.metrics.ascender*0.5f},
                                    ws->name,active?ctx.theme.colors.tabActiveText:ctx.theme.colors.tabText,
                                    tabW-ctx.theme.metrics.paddingX*2-14,true);
                    // Bouton fermeture de l'onglet
                    const NkRect closeR={tabR.x+tabR.w-14,tabR.y+tabH*0.5f-6,12,12};
                    const bool closHov=NkRectContains(closeR,ctx.input.mousePos);
                    if(active||hov){
                        const NkColor cx=closHov?NkColor{200,50,50,255}:ctx.theme.colors.tabText;
                        const float32 cx2=closeR.x+6,cy=closeR.y+6,d=4;
                        dl.AddLine({cx2-d,cy-d},{cx2+d,cy+d},cx,1.f);
                        dl.AddLine({cx2+d,cy-d},{cx2-d,cy+d},cx,1.f);
                        if(closHov&&ctx.input.IsMouseClicked(0)){
                            UndockWindow(wm,node.windows[t]);
                            if(ws) ws->isOpen=false;
                        }
                    }
                }
                // Sélection d'onglet au clic
                if(hov&&ctx.input.IsMouseClicked(0)&&!NkRectContains({tabR.x+tabR.w-14,tabR.y,14,tabH},ctx.input.mousePos))
                    node.activeTab=t;

                // Détachement par drag
                if(hov&&ctx.input.IsMouseDown(0)&&ctx.input.mouseDelta.y>5.f&&ws){
                    UndockWindow(wm,ws->id);
                    ws->isOpen=true;
                    ws->pos={ctx.input.mousePos.x-ws->size.x*0.5f,ctx.input.mousePos.y-10};
                }
            }

            // Contenu de l'onglet actif
            if(node.activeTab<node.numWindows){
                NkUIWindowState* ws=wm.Find(node.windows[node.activeTab]);
                if(ws&&ws->isOpen){
                    const NkRect clientR={r.x,r.y+tabH,r.w,r.h-tabH};
                    dl.PushClipRect(clientR,true);
                    dl.AddRectFilled(clientR,ctx.theme.colors.bgWindow);
                    // Contenu rendu via Begin/End dans le code utilisateur avec BeginDocked
                    ws->pos={r.x,r.y+tabH};
                    ws->size={r.w,r.h-tabH};
                    dl.PopClipRect();
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginFrame — gère le drag & drop pendant le frame
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::BeginFrame(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUIDrawList& dl,
                                        NkUIFont&) noexcept
        {
            // Détecte si une fenêtre est en train d'être draguée vers le dock
            if(!isDragging){
                // Cherche une fenêtre flottante draguée (movingId actif)
                if(wm.movingId!=NKUI_ID_NONE){
                    isDragging=true;
                    dragWindowId=wm.movingId;
                }
            }

            if(isDragging&&dragWindowId!=NKUI_ID_NONE){
                // Trouve le nœud sous le curseur
                const int32 nodeUnder=FindNodeAt(ctx.input.mousePos);
                if(nodeUnder>=0){
                    dragTargetNode=nodeUnder;
                    // Overlay semi-transparent sur le nœud cible
                    dl.AddRectFilled(nodes[nodeUnder].rect,ctx.theme.colors.dockZone.WithAlpha(40));
                    // Dropzones
                    const NkUIDockDrop hovered=DrawDropZones(ctx,dl,nodeUnder);
                    if(!ctx.input.mouseDown[0]&&hovered!=NkUIDockDrop::NK_NONE){
                        // Relâchement sur une dropzone → ancrage
                        DockWindow(wm,dragWindowId,nodeUnder,hovered);
                        wm.movingId=NKUI_ID_NONE;
                        isDragging=false; dragWindowId=NKUI_ID_NONE; dragTargetNode=-1;
                    }
                }
                if(!ctx.input.mouseDown[0]){
                    isDragging=false; dragWindowId=NKUI_ID_NONE; dragTargetNode=-1;
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Render — dessine tout l'arbre de dock
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::Render(NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    NkUIWindowManager& wm,
                                    NkUILayoutStack& ls) noexcept
        {
            if(rootIdx>=0) RenderNode(ctx,dl,font,wm,rootIdx,ls);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginDocked — rendu du contenu d'une fenêtre ancrée
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIDockManager::BeginDocked(NkUIContext& ctx,
                                            NkUIWindowManager& wm,
                                            NkUIDrawList& dl,
                                            const char* name) noexcept
        {
            NkUIWindowState* ws=wm.Find(name);
            if(!ws||!ws->isDocked||!ws->isOpen) return false;
            const int32 nodeIdx=static_cast<int32>(ws->dockNodeId);
            if(nodeIdx<0||nodeIdx>=numNodes) return false;
            NkUIDockNode& node=nodes[nodeIdx];
            // Vérifie que c'est l'onglet actif
            if(node.activeTab<0||node.activeTab>=node.numWindows) return false;
            if(node.windows[node.activeTab]!=ws->id) return false;
            // La fenêtre est active dans son nœud — on set le curseur
            const float32 tabH=ctx.theme.metrics.dockTabHeight;
            const NkRect clientR={ws->pos.x,ws->pos.y,ws->size.x,ws->size.y};
            const NkVec2 start={clientR.x+ctx.theme.metrics.windowPadX,
                                clientR.y+ctx.theme.metrics.windowPadY};
            ctx.SetCursor(start);
            ctx.cursorStart=start;
            dl.PushClipRect(clientR,true);
            (void)tabH;
            return true;
        }

        void NkUIDockManager::EndDocked(NkUIContext&,NkUIDrawList& dl) noexcept {
            dl.PopClipRect();
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Serialisation JSON de la config dock (layout persistence)
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIDockManager::SaveLayout(const char* path) const noexcept {
            FILE* f=::fopen(path,"wb"); if(!f) return;
            ::fprintf(f,"{\n  \"numNodes\":%d,\n  \"rootIdx\":%d,\n  \"nodes\":[\n",numNodes,rootIdx);
            for(int32 i=0;i<numNodes;++i){
                const NkUIDockNode& n=nodes[i];
                ::fprintf(f,"    {\"type\":%d,\"rect\":[%.1f,%.1f,%.1f,%.1f],"
                            "\"ratio\":%.4f,\"children\":[%d,%d],"
                            "\"numWindows\":%d,\"activeTab\":%d}%s\n",
                        (int32)n.type,n.rect.x,n.rect.y,n.rect.w,n.rect.h,
                        n.splitRatio,n.children[0],n.children[1],
                        n.numWindows,n.activeTab,i<numNodes-1?",":"");
            }
            ::fprintf(f,"  ]\n}\n");
            ::fclose(f);
        }

        bool NkUIDockManager::LoadLayout(const char* path) noexcept {
            FILE* f=::fopen(path,"rb"); if(!f) return false;
            // Parsing JSON minimal (lecture du numNodes et des nœuds)
            char buf[65536]={};
            ::fread(buf,1,sizeof(buf)-1,f);
            ::fclose(f);
            // TODO: parser complet — pour l'instant stub
            (void)buf;
            return false;
        }
    }
} // namespace nkentseu
