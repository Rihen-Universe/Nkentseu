/**
 * @File    NkUIMenu.cpp
 * @Brief   MenuBar, Menu, MenuItem, Popup, Modal.
 */

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Menu/popup/modal runtime implementation.
 * Main data: Open/close policy, hover/click handling, popup layering.
 * Change this file when: Menu visibility/focus/open-state regressions appear.
 */
#include "NkUI/NkUIMenu.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Statics
        // ─────────────────────────────────────────────────────────────────────────────
        NkUIMenuState     NkUIMenu::sMenus[MAX_MENUS]  = {};
        NkUIMenuState*    NkUIMenu::sMenuStack[MAX_MENUS] = {};
        int32             NkUIMenu::sMenuDepth          = 0;
        NkUIID            NkUIMenu::sOpenTopLevelMenuId = NKUI_ID_NONE;
        NkRect            NkUIMenu::sMenuBarRect        = {};
        float32           NkUIMenu::sMenuBarCursorX     = 0.f;
        bool              NkUIMenu::sInMenuBar          = false;
        NkUIMenu::PopupEntry NkUIMenu::sPopups[MAX_POPUPS]={};
        int32             NkUIMenu::sPopupDepth         = 0;

        // ─────────────────────────────────────────────────────────────────────────────
        //  Helpers
        // ─────────────────────────────────────────────────────────────────────────────

        static float32 BaselineY(const NkRect& r, const NkUIFont& font) noexcept {
            const float32 asc = (font.metrics.ascender > 0.f) ? font.metrics.ascender : (font.size * 0.8f);
            const float32 descRaw = (font.metrics.descender >= 0.f) ? font.metrics.descender : -font.metrics.descender;
            const float32 desc = (descRaw > 0.f) ? descRaw : (font.size * 0.2f);
            const float32 lineH = (font.metrics.lineHeight > 0.f) ? font.metrics.lineHeight : (asc + desc);
            const float32 top = r.y + (r.h - lineH) * 0.5f;
            return top + asc;
        }

        void NkUIMenu::CloseAllMenus(NkUIContext& ctx) noexcept {
            // Ferme proprement les clips des menus actuellement empilés.
            while (sMenuDepth > 0) {
                ctx.layers[NkUIContext::LAYER_POPUPS].PopClipRect();
                sMenuStack[sMenuDepth - 1] = nullptr;
                --sMenuDepth;
            }
            sOpenTopLevelMenuId = NKUI_ID_NONE;
            for(int32 i=0;i<MAX_MENUS;++i) {
                sMenus[i].open = false;
                sMenus[i].itemCount = 0;
                sMenuStack[i] = nullptr;
            }
        }

        bool NkUIMenu::IsAnyMenuOpen(NkUIContext&) noexcept {
            for(int32 i=0;i<MAX_MENUS;++i) {
                if (sMenus[i].open) return true;
            }
            return false;
        }

        NkRect NkUIMenu::CalcPanelRect(NkRect trigger,int32 depth,int32 itemCount) noexcept {
            const float32 h=ITEM_H*itemCount+8.f;
            if(depth==0){
                // Depuis la menubar : ouvre vers le bas
                return {trigger.x,trigger.y+trigger.h,PANEL_W,h};
            }
            // Sous-menu : ouvre à droite
            return {trigger.x+trigger.w-4,trigger.y,PANEL_W,h};
        }

        bool NkUIMenu::DrawMenuButton(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font, const char* label,NkRect r,bool open,bool enabled) noexcept
        {
            const auto& t=ctx.theme;
            const bool hov=ctx.IsHovered(r);
            const NkColor bg=open?t.colors.accent:(hov&&enabled?t.colors.buttonHover:NkColor::Transparent());
            if(bg.a>0) dl.AddRectFilled(r,bg,t.metrics.cornerRadiusSm);
            const NkColor tc=!enabled?t.colors.textDisabled:(open||hov?t.colors.textOnAccent:t.colors.textPrimary);
            font.RenderText(dl, {r.x + t.metrics.paddingX, BaselineY(r, font)}, label, tc);
            return hov && enabled;
        }

        NkRect NkUIMenu::DrawMenuPanel(NkUIContext& ctx,NkUIDrawList& dl,
                                        const NkUIMenuState& ms) noexcept
        {
            const auto& t=ctx.theme;
            // Animation d'ouverture
            const float32 anim=ctx.Animate(ms.id,ms.open?1.f:0.f,14.f);
            NkRect pr=ms.panelRect;
            pr.h*=anim;
            NkUIDrawList& pdl=ctx.layers[NkUIContext::LAYER_POPUPS];
            pdl.AddShadow(pr,8,NkColor::Gray(0,60),{2,2});
            pdl.AddRectFilled(pr,t.colors.bgPopup,t.metrics.cornerRadius);
            pdl.AddRect(pr,t.colors.border,1.f,t.metrics.cornerRadius);
            pdl.PushClipRect(pr);
            return pr;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  MenuBar
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIMenu::BeginMenuBar(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,NkRect rect) noexcept
        {
            // Runtime stack strictement frame-local pour éviter les états "fantômes".
            sMenuDepth = 0;
            for (int32 i = 0; i < MAX_MENUS; ++i) {
                sMenuStack[i] = nullptr;
            }
            sMenuBarRect=rect; sMenuBarCursorX=rect.x+8.f; sInMenuBar=true;
            const auto& t=ctx.theme;
            dl.AddRectFilled(rect,t.colors.bgHeader);
            dl.AddLine({rect.x,rect.y+rect.h},{rect.x+rect.w,rect.y+rect.h},t.colors.border,1.f);
            (void)font;
            if (ctx.input.IsKeyPressed(NkKey::NK_ESCAPE)) {
                CloseAllMenus(ctx);
            }
            // Clic en dehors de la menubar et des panneaux de menu -> ferme tout.
            if(ctx.IsMouseClicked(0) && !NkRectContains(rect,ctx.input.mousePos)){
                bool inPanel = false;
                for (int32 i = 0; i < MAX_MENUS; ++i) {
                    if (sMenus[i].open && NkRectContains(sMenus[i].panelRect, ctx.input.mousePos)) {
                        inPanel = true;
                        break;
                    }
                }
                if(!inPanel) {
                    CloseAllMenus(ctx);
                }
            }
            return true;
        }

        void NkUIMenu::EndMenuBar(NkUIContext& ctx) noexcept { sInMenuBar=false; (void)ctx; }

        // ─────────────────────────────────────────────────────────────────────────────
        //  BeginMenu
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIMenu::BeginMenu(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, const char* label, bool enabled) noexcept
        {
            NkUIID id = ctx.GetID(label);
            if (sInMenuBar) {
                // Les IDs de menubar doivent être isolés des widgets "normaux".
                id = NkHash(label, 0x7f4a5d3bu);
            }
            const float32 lw = font.MeasureWidth(label)+ctx.theme.metrics.paddingX*2;
            NkRect btnR;
            int32 depth = 0;

            if(sInMenuBar){
                btnR = {sMenuBarCursorX,sMenuBarRect.y,lw,sMenuBarRect.h};
                sMenuBarCursorX += lw+4;
                depth = 0;
            } else {
                // Sous-menu : item dans un panel existant
                if(sMenuDepth <= 0) return false;
                NkUIMenuState* parent = sMenuStack[sMenuDepth - 1];
                if (!parent) return false;
                const NkRect& pr = parent->panelRect;
                const float32 iy = pr.y + 8 + static_cast<float32>(parent->itemCount) * ITEM_H;
                btnR = {pr.x + 4, iy, pr.w - 8, ITEM_H};
                parent->itemCount += 1;
                depth = sMenuDepth;
            }

            // Trouve ou crée l'entrée
            NkUIMenuState* ms = nullptr;
            for(int32 i = 0; i < MAX_MENUS; ++i) {
                if(sMenus[i].id == id) { 
                    ms=&sMenus[i];
                    break;
                }
            }
            if(!ms) {
                for(int32 i=0;i<MAX_MENUS;++i) {
                    if(!sMenus[i].id) {
                        ms = &sMenus[i];
                        ms->id = id;
                        break;
                    }
                }
            }
            if(!ms) return false;

            ms->rect = btnR; 
            ms->depth = depth;

            if (sInMenuBar) {
                ms->open = (sOpenTopLevelMenuId == id);
            }

            NkUIDrawList& menuDL = sInMenuBar ? dl : ctx.layers[NkUIContext::LAYER_POPUPS];
            const bool hov = DrawMenuButton(ctx,menuDL,font,label,btnR,ms->open,enabled);

            // Flèche pour indiquer sous-menu si pas dans menubar
            if(!sInMenuBar){
                menuDL.AddArrow({btnR.x+btnR.w-14,btnR.y+btnR.h*0.5f},6.f,0,ctx.theme.colors.textSecondary);
            }

            // Toggle à l'hover en menubar (ou clic en sous-menu)
            if(sInMenuBar){
                // En menubar:
                // - clic sur menu fermé   -> ouvre ce menu uniquement
                // - clic sur menu ouvert  -> ferme tous les menus
                // - hover pendant qu'un menu est ouvert -> bascule sur ce menu
                const bool anyOpen = (sOpenTopLevelMenuId != NKUI_ID_NONE);
                if (!enabled && sOpenTopLevelMenuId == id) {
                    CloseAllMenus(ctx);
                } else if (enabled && hov && ctx.ConsumeMouseClick(0)) {
                    if (sOpenTopLevelMenuId == id) {
                        CloseAllMenus(ctx);
                    } else {
                        CloseAllMenus(ctx);
                        sOpenTopLevelMenuId = id;
                    }
                } else if (enabled && hov && anyOpen && sOpenTopLevelMenuId != id) {
                    CloseAllMenus(ctx);
                    sOpenTopLevelMenuId = id;
                }
                ms->open = enabled && (sOpenTopLevelMenuId == id);
            } else {
                if(hov && enabled) ms->open=true;
            }

            if(!ms->open) return false;

            // Calcule et dessine le panel
            // Compte les items (approximatif : on le fixe à 10 max pour le calcul initial)
            ms->panelRect = CalcPanelRect(btnR,depth,10);
            DrawMenuPanel(ctx,dl,*ms);

            // Empile
            if(sMenuDepth < MAX_MENUS) {
                sMenuStack[sMenuDepth] = ms;
                ms->itemCount = 0;
                ++sMenuDepth;
            }
            return true;
        }

        void NkUIMenu::EndMenu(NkUIContext& ctx) noexcept {
            if(sMenuDepth>0){
                NkUIMenuState* ms = sMenuStack[sMenuDepth - 1];
                if (ms) ms->itemCount = 0;
                ctx.layers[NkUIContext::LAYER_POPUPS].PopClipRect();
                sMenuStack[sMenuDepth - 1] = nullptr;
                --sMenuDepth;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  MenuItem
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIMenu::MenuItem(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,
                                const char* label,const char* shortcut,
                                bool* selected,bool enabled) noexcept
        {
            if(sMenuDepth<=0) return false;
            NkUIMenuState* msPtr = sMenuStack[sMenuDepth - 1];
            if (!msPtr) return false;
            NkUIMenuState& ms = *msPtr;
            NkUIDrawList& pdl = ctx.layers[NkUIContext::LAYER_POPUPS];

            // Position dans le panel
            const NkRect pr = ms.panelRect;
            const int32 itemIdx = ms.rect.y > 0 ? static_cast<int32>(ms.rect.y) : 0;
            (void)itemIdx;
            // Calcule Y depuis le début du panel
            // On compte les items ajoutés en utilisant un compteur stocké dans rect.y
            const float32 iy = pr.y+8+static_cast<int32>(ms.itemCount)*ITEM_H;
            const NkRect itemR = {pr.x+4,iy,pr.w-8,ITEM_H};

            // Avance le compteur
            ms.itemCount += 1;

            const bool hov = ctx.IsHovered(itemR) && enabled;
            const bool act = hov && ctx.ConsumeMouseClick(0);

            if(hov) pdl.AddRectFilled(itemR,ctx.theme.colors.accent,ctx.theme.metrics.cornerRadiusSm);

            // Checkmark si selected
            if(selected && *selected){
                pdl.AddCheckMark({itemR.x+2,itemR.y+(itemR.h-10)*0.5f},10.f, hov?ctx.theme.colors.textOnAccent:ctx.theme.colors.accent);
            }

            const NkColor tc=!enabled?ctx.theme.colors.textDisabled:
                                    (hov?ctx.theme.colors.textOnAccent:ctx.theme.colors.textPrimary);
            const float32 y = BaselineY(itemR, font);
            font.RenderText(pdl,{itemR.x+18, y}, label,tc);

            // Raccourci à droite
            if(shortcut&&*shortcut){
                const float32 sw=font.MeasureWidth(shortcut);
                font.RenderText(pdl,{itemR.x+itemR.w-sw-8, y},
                                shortcut,!enabled?ctx.theme.colors.textDisabled:ctx.theme.colors.textSecondary);
            }

            if(act){
                if(selected) {
                    *selected = !(*selected);
                }
                CloseAllMenus(ctx);
                return true;
            }
            (void)dl;
            return false;
        }

        void NkUIMenu::Separator(NkUIContext& ctx,NkUIDrawList& dl) noexcept {
            if(sMenuDepth<=0){(void)dl;return;}
            NkUIMenuState* msPtr = sMenuStack[sMenuDepth - 1];
            if (!msPtr) { (void)dl; return; }
            NkUIMenuState& ms=*msPtr;
            NkUIDrawList& pdl=ctx.layers[NkUIContext::LAYER_POPUPS];
            const NkRect pr=ms.panelRect;
            const float32 iy=pr.y+8+static_cast<int32>(ms.itemCount)*ITEM_H+ITEM_H*0.5f;
            pdl.AddLine({pr.x+8,iy},{pr.x+pr.w-8,iy},ctx.theme.colors.separator,1.f);
            // Le séparateur occupe une ligne logique dans la pile des items.
            ms.itemCount += 1;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Context Menu
        // ─────────────────────────────────────────────────────────────────────────────

        static NkUIID sContextMenuId=0;
        static bool   sContextMenuOpen=false;
        static NkVec2 sContextMenuPos={};

        void NkUIMenu::OpenContextMenu(NkUIContext& ctx,const char* id) noexcept {
            sContextMenuId=ctx.GetID(id);
            sContextMenuOpen=true;
            sContextMenuPos=ctx.input.mousePos;
        }

        bool NkUIMenu::BeginContextMenu(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,const char* id) noexcept
        {
            // Ouvre automatiquement au clic droit
            if(ctx.ConsumeMouseClick(1)) OpenContextMenu(ctx,id);
            if(!sContextMenuOpen||ctx.GetID(id)!=sContextMenuId) return false;
            // Ferme si clic gauche hors du menu
            if(ctx.IsMouseClicked(0)){
                const NkRect mr={sContextMenuPos.x,sContextMenuPos.y,PANEL_W,200.f};
                if(!NkRectContains(mr,ctx.input.mousePos)){sContextMenuOpen=false;return false;}
            }
            // Crée un faux menu state pour réutiliser MenuItem
            NkUIMenuState& ms=sMenus[0];
            ms.id=sContextMenuId; ms.depth=0; ms.open=true; ms.itemCount=0;
            ms.panelRect={sContextMenuPos.x,sContextMenuPos.y,PANEL_W,200.f};
            // Dessine le panel
            NkUIDrawList& pdl=ctx.layers[NkUIContext::LAYER_POPUPS];
            pdl.AddRectFilled(ms.panelRect,ctx.theme.colors.bgPopup,ctx.theme.metrics.cornerRadius);
            pdl.AddRect(ms.panelRect,ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadius);
            pdl.PushClipRect(ms.panelRect);
            sMenuDepth=1;
            sMenuStack[0] = &ms;
            (void)dl;(void)font;
            return true;
        }

        void NkUIMenu::EndContextMenu(NkUIContext& ctx) noexcept {
            if(sMenuDepth>0){
                ctx.layers[NkUIContext::LAYER_POPUPS].PopClipRect();
                sMenuStack[0] = nullptr;
                sMenuDepth=0;
            }
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Popup
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIMenu::OpenPopup(NkUIContext& ctx,const char* id) noexcept {
            const NkUIID pid=ctx.GetID(id);
            for(int32 i=0;i<MAX_POPUPS;++i){
                if(sPopups[i].id==pid){ sPopups[i].open=true; return; }
                if(!sPopups[i].id){ sPopups[i].id=pid; sPopups[i].open=true;
                                    sPopups[i].pos=ctx.input.mousePos; return; }
            }
        }

        void NkUIMenu::ClosePopup(NkUIContext& ctx) noexcept {
            if(sPopupDepth>0){ sPopups[sPopupDepth-1].open=false; --sPopupDepth; }
            (void)ctx;
        }

        bool NkUIMenu::BeginPopup(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,
                                    const char* id,NkVec2 size) noexcept
        {
            const NkUIID pid=ctx.GetID(id);
            PopupEntry* pe=nullptr;
            for(int32 i=0;i<MAX_POPUPS;++i) if(sPopups[i].id==pid){pe=&sPopups[i];break;}
            if(!pe||!pe->open) return false;

            const float32 pw=size.x>0?size.x:200.f, ph=size.y>0?size.y:150.f;
            // Centre dans le viewport
            const float32 px=::fminf(pe->pos.x,static_cast<float32>(ctx.viewW)-pw-10);
            const float32 py=::fminf(pe->pos.y,static_cast<float32>(ctx.viewH)-ph-10);
            const NkRect pr={px,py,pw,ph};
            pe->size={pw,ph};

            // Overlay semi-transparent
            NkUIDrawList& pdl=ctx.layers[NkUIContext::LAYER_POPUPS];
            pdl.AddRectFilled({0,0,(float32)ctx.viewW,(float32)ctx.viewH},NkColor::Gray(0,80));
            // Panel popup
            pdl.AddShadow(pr,12,NkColor::Gray(0,80),{3,3});
            pdl.AddRectFilled(pr,ctx.theme.colors.bgPopup,ctx.theme.metrics.cornerRadiusLg);
            pdl.AddRect(pr,ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadiusLg);
            pdl.PushClipRect(pr);

            // Ferme si Escape
            if(ctx.input.IsKeyPressed(NkKey::NK_ESCAPE)){pe->open=false;pdl.PopClipRect();return false;}
            // Ferme si clic hors du popup
            if(ctx.IsMouseClicked(0)&&!NkRectContains(pr,ctx.input.mousePos)){
                pe->open=false;pdl.PopClipRect();return false;}

            if(sPopupDepth<MAX_POPUPS) ++sPopupDepth;
            ctx.dl=&pdl; // redirige les widgets vers le layer popup
            ctx.SetCursor({pr.x+ctx.theme.metrics.windowPadX,pr.y+ctx.theme.metrics.windowPadY});
            (void)dl;(void)font;
            return true;
        }

        void NkUIMenu::EndPopup(NkUIContext& ctx,NkUIDrawList& dl) noexcept {
            ctx.layers[NkUIContext::LAYER_POPUPS].PopClipRect();
            if(sPopupDepth>0) --sPopupDepth;
            ctx.dl=&ctx.layers[NkUIContext::LAYER_WINDOWS]; // restore
            (void)dl;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Modal
        // ─────────────────────────────────────────────────────────────────────────────

        static char  sModalTitle[64]={};
        static bool  sModalOpen=false;
        static NkVec2 sModalSize={400,200};

        void NkUIMenu::OpenModal(NkUIContext&,const char* title) noexcept {
            ::strncpy(sModalTitle,title,63); sModalOpen=true;
        }

        bool NkUIMenu::BeginModal(NkUIContext& ctx,NkUIDrawList& dl,NkUIFont& font,
                                    const char* title,bool* open,NkVec2 size) noexcept
        {
            if(!sModalOpen&&(!open||!*open)) return false;
            if(open) sModalOpen=*open;
            sModalSize=size;

            const float32 pw=size.x, ph=size.y;
            const float32 px=(ctx.viewW-pw)*0.5f, py=(ctx.viewH-ph)*0.5f;
            const NkRect pr={px,py,pw,ph};

            NkUIDrawList& pdl=ctx.layers[NkUIContext::LAYER_OVERLAY];
            // Overlay opaque qui bloque l'UI sous-jacente
            pdl.AddRectFilled({0,0,(float32)ctx.viewW,(float32)ctx.viewH},NkColor::Gray(0,160));

            // Fenêtre modale
            pdl.AddShadow(pr,16,NkColor::Gray(0,100),{4,4});
            pdl.AddRectFilled(pr,ctx.theme.colors.bgWindow,ctx.theme.metrics.cornerRadiusLg);
            pdl.AddRect(pr,ctx.theme.colors.border,1.f,ctx.theme.metrics.cornerRadiusLg);

            // Barre de titre
            const float32 tbH=ctx.theme.metrics.titleBarHeight;
            const NkRect tbR={pr.x,pr.y,pr.w,tbH};
            pdl.AddRectFilledCorners(tbR,ctx.theme.colors.titleBarBg,ctx.theme.metrics.cornerRadiusLg,0x3);
            font.RenderText(pdl,{pr.x+ctx.theme.metrics.titleBarPadX,
                                pr.y+(tbH-font.size)*0.5f+font.metrics.ascender*0.5f},
                            title,ctx.theme.colors.titleBarText);

            // Bouton fermer
            const NkVec2 xc={pr.x+pr.w-tbH*0.5f,pr.y+tbH*0.5f};
            const bool xhov=ctx.IsHoveredCircle(xc,tbH*0.25f);
            pdl.AddCircleFilled(xc,tbH*0.22f,xhov?NkColor{220,60,60,255}:ctx.theme.colors.titleBarBtn);
            if(xhov){
                pdl.AddLine({xc.x-5,xc.y-5},{xc.x+5,xc.y+5},NkColor::White(),1.5f);
                pdl.AddLine({xc.x+5,xc.y-5},{xc.x-5,xc.y+5},NkColor::White(),1.5f);
                if(ctx.ConsumeMouseClick(0)){
                    sModalOpen=false; if(open)*open=false;
                    (void)dl; return false;
                }
            }
            // Escape ferme aussi
            if(ctx.input.IsKeyPressed(NkKey::NK_ESCAPE)){
                sModalOpen=false; if(open)*open=false; (void)dl; return false;
            }

            pdl.PushClipRect({pr.x,pr.y+tbH,pr.w,pr.h-tbH});
            ctx.dl=&pdl;
            ctx.SetCursor({pr.x+ctx.theme.metrics.windowPadX,pr.y+tbH+ctx.theme.metrics.windowPadY});
            (void)dl;(void)font;
            return true;
        }

        void NkUIMenu::EndModal(NkUIContext& ctx,NkUIDrawList& dl) noexcept {
            ctx.layers[NkUIContext::LAYER_OVERLAY].PopClipRect();
            ctx.dl=&ctx.layers[NkUIContext::LAYER_WINDOWS];
            (void)dl;
        }
    }
} // namespace nkentseu
