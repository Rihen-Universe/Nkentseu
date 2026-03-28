/**
 * @File    NkUIDock.cpp
 * @Brief   SystÃ¨me de docking NkUI â€” production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf SÃ©deris
 * @License Apache-2.0
 *
 * @Architecture
 *  L'arbre de dock est un arbre binaire de NkUIDockNode.
 *  Chaque nÅ“ud est soit :
 *    - Root  : racine de tout l'espace
 *    - Leaf  : contient des fenÃªtres (affichÃ©es en tabs)
 *    - SplitH: deux enfants cÃ´te Ã  cÃ´te (gauche/droite)
 *    - SplitV: deux enfants l'un au-dessus de l'autre (haut/bas)
 *
 *  Quand on ancre une fenÃªtre :
 *    1. On choisit un nÅ“ud cible et une direction (L/R/T/B/Center)
 *    2. Si Center : la fenÃªtre va dans le Leaf (nouveau tab)
 *    3. Si L/R/T/B : le Leaf se transforme en Split, ses fenÃªtres
 *       restent dans un nouveau Leaf enfant, la nouvelle fenÃªtre
 *       va dans l'autre Leaf enfant.
 *
 *  Les dropzones sont calculÃ©es en temps rÃ©el pendant le drag
 *  et dessinÃ©es en overlay.
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Docking layout runtime implementation.
 * Main data: Dock tree updates, splits, tabs and docking operations.
 * Change this file when: Dock attach/detach/split/tab behavior is incorrect.
 */
#include "NkUI/NkUIDock.h"
#include "NkUI/NkUIWidgets.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  NkUIDockManager â€” Init / Destroy
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIDockManager::Init(NkRect viewport) noexcept {
            numNodes=0; rootIdx=-1;
            isDragging=false; isDraggingTab=false;
            dragWindowId=NKUI_ID_NONE; dragTargetNode=-1;
            ::memset(nodes,0,sizeof(nodes));
            // CrÃ©e le nÅ“ud racine
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

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  RecalcRects â€” recalcule les rects enfants depuis le parent
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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

        static void RebindDockedWindowsRecursive(NkUIDockManager& dock,
                                                NkUIWindowManager& wm,
                                                int32 idx) noexcept
        {
            if(idx<0||idx>=dock.numNodes) return;
            NkUIDockNode& n=dock.nodes[idx];
            if(n.type==NkUIDockNodeType::NK_LEAF||n.type==NkUIDockNodeType::NK_ROOT){
                for(int32 i=0;i<n.numWindows;++i){
                    NkUIWindowState* ws=wm.Find(n.windows[i]);
                    if(ws){
                        ws->isDocked=true;
                        ws->dockNodeId=static_cast<NkUIID>(idx);
                    }
                }
                return;
            }
            RebindDockedWindowsRecursive(dock,wm,n.children[0]);
            RebindDockedWindowsRecursive(dock,wm,n.children[1]);
        }

        static void CollapseEmptyLeaf(NkUIDockManager& dock,
                                    NkUIWindowManager& wm,
                                    int32 leafIdx) noexcept
        {
            int32 current=leafIdx;
            while(current>=0&&current<dock.numNodes){
                NkUIDockNode& cur=dock.nodes[current];
                if(cur.type!=NkUIDockNodeType::NK_LEAF||cur.numWindows>0){
                    break;
                }
                const int32 parentIdx=cur.parent;
                if(parentIdx<0||parentIdx>=dock.numNodes){
                    cur.type=NkUIDockNodeType::NK_ROOT;
                    cur.children[0]=cur.children[1]=-1;
                    cur.splitRatio=0.5f;
                    cur.activeTab=0;
                    break;
                }

                NkUIDockNode& parent=dock.nodes[parentIdx];
                const int32 siblingIdx=(parent.children[0]==current)?parent.children[1]:parent.children[0];
                if(siblingIdx<0||siblingIdx>=dock.numNodes){
                    parent.type=NkUIDockNodeType::NK_LEAF;
                    parent.numWindows=0;
                    parent.activeTab=0;
                    parent.children[0]=parent.children[1]=-1;
                    current=parentIdx;
                    continue;
                }

                NkUIDockNode siblingCopy=dock.nodes[siblingIdx];
                const int32 grandParent=parent.parent;
                siblingCopy.parent=grandParent;
                siblingCopy.rect=parent.rect;
                dock.nodes[parentIdx]=siblingCopy;

                if(dock.nodes[parentIdx].children[0]>=0&&dock.nodes[parentIdx].children[0]<dock.numNodes){
                    dock.nodes[dock.nodes[parentIdx].children[0]].parent=parentIdx;
                }
                if(dock.nodes[parentIdx].children[1]>=0&&dock.nodes[parentIdx].children[1]<dock.numNodes){
                    dock.nodes[dock.nodes[parentIdx].children[1]].parent=parentIdx;
                }

                dock.nodes[current]=NkUIDockNode{};
                dock.nodes[siblingIdx]=NkUIDockNode{};
                if(current==dock.rootIdx||siblingIdx==dock.rootIdx){
                    dock.rootIdx=parentIdx;
                }

                RebindDockedWindowsRecursive(dock,wm,parentIdx);
                current=parentIdx;
            }
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  DockWindow â€” ancre une fenÃªtre dans un nÅ“ud
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        bool NkUIDockManager::DockWindow(NkUIWindowManager& wm,
                                        NkUIID windowId, int32 nodeIdx,
                                        NkUIDockDrop drop) noexcept
        {
            if(nodeIdx<0||nodeIdx>=numNodes) return false;
            NkUIWindowState* ws=wm.Find(windowId);
            if(!ws) return false;

            if(drop==NkUIDockDrop::NK_CENTER||drop==NkUIDockDrop::NK_TAB){
                NkUIDockNode& node=nodes[nodeIdx];

                if(ws->isDocked&&static_cast<int32>(ws->dockNodeId)==nodeIdx){
                    for(int32 i=0;i<node.numWindows;++i){
                        if(node.windows[i]==windowId){
                            node.activeTab=i;
                            return true;
                        }
                    }
                }

                if(ws->isDocked){
                    UndockWindow(wm,windowId);
                }

                for(int32 i=0;i<node.numWindows;++i){
                    if(node.windows[i]==windowId){
                        node.activeTab=i;
                        node.type=NkUIDockNodeType::NK_LEAF;
                        ws->isDocked=true;
                        ws->dockNodeId=static_cast<NkUIID>(nodeIdx);
                        return true;
                    }
                }

                if(node.numWindows>=NkUIDockNode::MAX_WINDOWS) return false;
                node.windows[node.numWindows++]=windowId;
                node.activeTab=node.numWindows-1;
                node.type=NkUIDockNodeType::NK_LEAF;
                ws->isDocked=true;
                ws->dockNodeId=static_cast<NkUIID>(nodeIdx);
                return true;
            }

            if(ws->isDocked){
                UndockWindow(wm,windowId);
            }

            // Split: create two leaf children.
            const int32 c0=AllocNode(); if(c0<0) return false;
            const int32 c1=AllocNode(); if(c1<0) return false;

            NkUIDockNode& target=nodes[nodeIdx];
            NkUIID oldWindows[NkUIDockNode::MAX_WINDOWS]={};
            const int32 oldCount=target.numWindows;
            const int32 oldActive=target.activeTab;
            for(int32 i=0;i<oldCount&&i<NkUIDockNode::MAX_WINDOWS;++i){
                oldWindows[i]=target.windows[i];
            }

            const bool horizontal=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_RIGHT);
            target.type=horizontal?NkUIDockNodeType::NK_SPLIT_H:NkUIDockNodeType::NK_SPLIT_V;
            target.children[0]=c0; target.children[1]=c1;
            target.splitRatio=0.5f;
            target.numWindows=0;
            target.activeTab=0;
            ::memset(target.windows,0,sizeof(target.windows));

            nodes[c0]=NkUIDockNode{};
            nodes[c0].type=NkUIDockNodeType::NK_LEAF;
            nodes[c0].parent=nodeIdx;
            nodes[c0].splitRatio=0.5f;

            nodes[c1]=NkUIDockNode{};
            nodes[c1].type=NkUIDockNodeType::NK_LEAF;
            nodes[c1].parent=nodeIdx;
            nodes[c1].splitRatio=0.5f;

            // Left/Top => new window in child[0], existing windows in child[1].
            const int32 newWindowChild=(drop==NkUIDockDrop::NK_LEFT||drop==NkUIDockDrop::NK_TOP)?0:1;
            const int32 existingChild=1-newWindowChild;
            NkUIDockNode& existingNode=nodes[existingChild==0?c0:c1];
            NkUIDockNode& newNode=nodes[newWindowChild==0?c0:c1];

            for(int32 i=0;i<oldCount&&existingNode.numWindows<NkUIDockNode::MAX_WINDOWS;++i){
                existingNode.windows[existingNode.numWindows++]=oldWindows[i];
            }
            existingNode.activeTab=existingNode.numWindows>0
                ?std::clamp(oldActive,0,existingNode.numWindows-1)
                :0;
            for(int32 i=0;i<existingNode.numWindows;++i){
                NkUIWindowState* moved=wm.Find(existingNode.windows[i]);
                if(moved){
                    moved->isDocked=true;
                    moved->dockNodeId=static_cast<NkUIID>(existingChild==0?c0:c1);
                }
            }

            newNode.windows[newNode.numWindows++]=windowId;
            newNode.activeTab=0;

            ws->isDocked=true;
            ws->dockNodeId=static_cast<NkUIID>(newWindowChild==0?c0:c1);

            // Recompute child rectangles.
            RecalcRects(nodeIdx);
            return true;
        }

        void NkUIDockManager::UndockWindow(NkUIWindowManager& wm, NkUIID windowId) noexcept {
            NkUIWindowState* ws=wm.Find(windowId);
            if(!ws||!ws->isDocked) return;
            const int32 nodeIdx=static_cast<int32>(ws->dockNodeId);
            if(nodeIdx<0||nodeIdx>=numNodes) return;
            NkUIDockNode& node=nodes[nodeIdx];
            // Retire la fenÃªtre du nÅ“ud
            for(int32 i=0;i<node.numWindows;++i){
                if(node.windows[i]==windowId){
                    for(int32 j=i;j<node.numWindows-1;++j) node.windows[j]=node.windows[j+1];
                    --node.numWindows;
                    if(node.activeTab>=node.numWindows&&node.activeTab>0) --node.activeTab;
                    break;
                }
            }
            ws->isDocked=false; ws->dockNodeId=0;
            if(node.numWindows<0) node.numWindows=0;
            if(node.activeTab<0) node.activeTab=0;
            if(node.activeTab>=node.numWindows&&node.numWindows>0) node.activeTab=node.numWindows-1;
            if(node.numWindows==0){
                CollapseEmptyLeaf(*this,wm,nodeIdx);
            }
            RecalcRectsAll();
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  FindNodeAt â€” trouve le Leaf sous un point
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        int32 NkUIDockManager::FindNodeAt(NkVec2 pos) const noexcept {
            for(int32 i=0;i<numNodes;++i){
                if(nodes[i].type!=NkUIDockNodeType::NK_LEAF) continue;
                if(NkRectContains(nodes[i].rect,pos)) return i;
            }
            if(rootIdx>=0&&rootIdx<numNodes&&NkRectContains(nodes[rootIdx].rect,pos)){
                return rootIdx;
            }
            return -1;
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  DrawDropZones â€” dessine les zones de dÃ©pÃ´t pendant le drag
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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

            NkUIDockDrop hovered = NkUIDockDrop::NK_NONE;
            for(const auto& z:zones){
                if(NkRectContains(z.rect, mp)){
                    hovered = z.drop;
                    break;
                }
            }

            if(hovered == NkUIDockDrop::NK_NONE && NkRectContains(r, mp)){
                // Edge docking only: center remains free-move unless center icon is hovered.
                const float32 edgeBand = std::clamp((r.w < r.h ? r.w : r.h) * 0.10f, sz * 0.45f, sz * 0.95f);
                const float32 dLeft   = mp.x - r.x;
                const float32 dRight  = (r.x + r.w) - mp.x;
                const float32 dTop    = mp.y - r.y;
                const float32 dBottom = (r.y + r.h) - mp.y;
                float32 dMin = dLeft;
                hovered = NkUIDockDrop::NK_LEFT;
                if(dRight < dMin){ dMin = dRight; hovered = NkUIDockDrop::NK_RIGHT; }
                if(dTop < dMin){ dMin = dTop; hovered = NkUIDockDrop::NK_TOP; }
                if(dBottom < dMin){ dMin = dBottom; hovered = NkUIDockDrop::NK_BOTTOM; }
                if(dMin > edgeBand){
                    hovered = NkUIDockDrop::NK_NONE;
                }
            }

            NkRect hoverPreview = {};
            switch(hovered){
                case NkUIDockDrop::NK_LEFT:   hoverPreview = {r.x, r.y, r.w * 0.38f, r.h}; break;
                case NkUIDockDrop::NK_RIGHT:  hoverPreview = {r.x + r.w * 0.62f, r.y, r.w * 0.38f, r.h}; break;
                case NkUIDockDrop::NK_TOP:    hoverPreview = {r.x, r.y, r.w, r.h * 0.38f}; break;
                case NkUIDockDrop::NK_BOTTOM: hoverPreview = {r.x, r.y + r.h * 0.62f, r.w, r.h * 0.38f}; break;
                case NkUIDockDrop::NK_CENTER: hoverPreview = {r.x + r.w * 0.2f, r.y + r.h * 0.2f, r.w * 0.6f, r.h * 0.6f}; break;
                default: break;
            }
            if(hovered != NkUIDockDrop::NK_NONE){
                dl.AddRectFilled(hoverPreview, zoneBorder.WithAlpha(72), rx);
                dl.AddRect(hoverPreview, zoneBorder.WithAlpha(220), 1.5f, rx);
            }

            for(const auto& z:zones){
                const bool hov=(hovered==z.drop);
                dl.AddRectFilled(z.rect,hov?zoneBorder.WithAlpha(180):zoneCol,rx);
                dl.AddRect(z.rect,zoneBorder,2.f,rx);
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

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  RenderNode â€” dessine un nÅ“ud et ses enfants
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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
                const NkColor splitLineCol=ctx.theme.colors.separator.WithAlpha(200);
                if(node.children[0]>=0&&node.children[0]<numNodes){
                    const NkRect& a=nodes[node.children[0]].rect;
                    if(vertical){
                        const float32 x=a.x+a.w;
                        dl.AddLine({x,node.rect.y},{x,node.rect.y+node.rect.h},splitLineCol,1.f);
                    } else {
                        const float32 y=a.y+a.h;
                        dl.AddLine({node.rect.x,y},{node.rect.x+node.rect.w,y},splitLineCol,1.f);
                    }
                }
                // Render les enfants
                RenderNode(ctx,dl,font,wm,node.children[0],ls);
                RenderNode(ctx,dl,font,wm,node.children[1],ls);
                return;
            }

            if(node.type!=NkUIDockNodeType::NK_LEAF&&node.type!=NkUIDockNodeType::NK_ROOT) return;
            if(node.numWindows==0) return;

            const NkRect& r=node.rect;
            const float32 tabH=ctx.theme.metrics.dockTabHeight;
            const NkColor sepCol=ctx.theme.colors.separator.WithAlpha(200);

            // Barre d'onglets
            const float32 tabW=r.w/node.numWindows;
            dl.AddRectFilled({r.x,r.y,r.w,tabH},ctx.theme.colors.bgHeader);
            dl.AddLine({r.x,r.y+tabH-0.5f},{r.x+r.w,r.y+tabH-0.5f},sepCol,1.f);

            for(int32 t=0;t<node.numWindows;++t){
                const NkRect tabR={r.x+t*tabW,r.y,tabW,tabH};
                const bool active=(t==node.activeTab);
                const bool hov=NkRectContains(tabR,ctx.input.mousePos);
                if(t>0){
                    dl.AddLine({tabR.x,r.y+3.f},{tabR.x,r.y+tabH-3.f},sepCol,1.f);
                }

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
                        if(closHov&&ctx.ConsumeMouseClick(0)){
                            UndockWindow(wm,node.windows[t]);
                            if(ws) ws->isOpen=false;
                        }
                    }
                }
                // SÃ©lection d'onglet au clic
                if(hov&&ctx.ConsumeMouseClick(0)&&!NkRectContains({tabR.x+tabR.w-14,tabR.y,14,tabH},ctx.input.mousePos)){
                    node.activeTab=t;
                    wm.activeId=node.windows[t];
                }

                // DÃ©tachement par drag
                if(hov
                    && ws
                    && ctx.input.IsMouseDown(0)
                    && ctx.input.mouseDelta.y>5.f
                    && wm.movingId==NKUI_ID_NONE
                    && wm.resizingId==NKUI_ID_NONE
                    && wm.activeId==ws->id){
                    UndockWindow(wm,ws->id);
                    ws->isOpen=true;
                    ws->pos={ctx.input.mousePos.x-ws->size.x*0.5f,ctx.input.mousePos.y-10};
                    wm.activeId=ws->id;
                    wm.movingId=ws->id;
                    wm.moveOffset={ws->size.x*0.5f,10.f};
                    wm.BringToFront(ws->id);
                }
            }

            // Contenu de l'onglet actif
            if(node.activeTab<node.numWindows){
                NkUIWindowState* ws=wm.Find(node.windows[node.activeTab]);
                if(ws&&ws->isOpen){
                    const NkRect clientR={r.x,r.y+tabH,r.w,r.h-tabH};
                    if(NkRectContains(clientR,ctx.input.mousePos)&&ctx.IsMouseClicked(0)){
                        wm.activeId=ws->id;
                    }
                    dl.PushClipRect(clientR,true);
                    dl.AddRectFilled(clientR,ctx.theme.colors.bgWindow);
                    dl.AddRect(clientR,ctx.theme.colors.separator.WithAlpha(120),1.f);
                    // Contenu rendu via Begin/End dans le code utilisateur avec BeginDocked
                    ws->pos={r.x,r.y+tabH};
                    ws->size={r.w,r.h-tabH};
                    dl.PopClipRect();
                }
            }
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  BeginFrame â€” gÃ¨re le drag & drop pendant le frame
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIDockManager::BeginFrame(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUIDrawList& dl,
                                        NkUIFont&) noexcept
        {
            // DÃ©tecte si une fenÃªtre est en train d'Ãªtre draguÃ©e vers le dock
            if(!isDragging){
                // Cherche une fenÃªtre flottante draguÃ©e (movingId actif)
                if(wm.movingId!=NKUI_ID_NONE){
                    isDragging=true;
                    dragWindowId=wm.movingId;
                }
            }

            if(isDragging&&dragWindowId!=NKUI_ID_NONE){
                // Trouve le nÅ“ud sous le curseur
                const int32 nodeUnder=FindNodeAt(ctx.input.mousePos);
                if(nodeUnder>=0){
                    dragTargetNode=nodeUnder;
                    // Overlay semi-transparent sur le nÅ“ud cible
                    dl.AddRectFilled(nodes[nodeUnder].rect,ctx.theme.colors.dockZone.WithAlpha(40));
                    // Dropzones
                    const NkUIDockDrop hovered=DrawDropZones(ctx,dl,nodeUnder);
                    if(!ctx.input.mouseDown[0]&&hovered!=NkUIDockDrop::NK_NONE){
                        // RelÃ¢chement sur une dropzone â†’ ancrage
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

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Render â€” dessine tout l'arbre de dock
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

        void NkUIDockManager::Render(NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    NkUIWindowManager& wm,
                                    NkUILayoutStack& ls) noexcept
        {
            if(rootIdx>=0) RenderNode(ctx,dl,font,wm,rootIdx,ls);
        }

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  BeginDocked â€” rendu du contenu d'une fenÃªtre ancrÃ©e
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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
            // VÃ©rifie que c'est l'onglet actif
            if(node.activeTab<0||node.activeTab>=node.numWindows) return false;
            if(node.windows[node.activeTab]!=ws->id) return false;
            // La fenÃªtre est active dans son nÅ“ud â€” on set le curseur
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

        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        //  Serialisation JSON de la config dock (layout persistence)
        // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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
            // Parsing JSON minimal (lecture du numNodes et des nÅ“uds)
            char buf[65536]={};
            ::fread(buf,1,sizeof(buf)-1,f);
            ::fclose(f);
            // TODO: parser complet â€” pour l'instant stub
            (void)buf;
            return false;
        }
    }
} // namespace nkentseu
