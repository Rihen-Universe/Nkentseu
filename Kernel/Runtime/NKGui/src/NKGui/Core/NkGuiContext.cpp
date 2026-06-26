// =============================================================================
// NkGuiContext.cpp — contexte NKGui (Phase 2).
// =============================================================================
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
    namespace nkgui {

        const char* NkGuiVersion::String() noexcept { return "0.1.0"; }

        namespace {
            // Contexte courant per-thread (non-singleton, comme NkUIContext).
            thread_local NkGuiContext* gCurrentContext = nullptr;
        }
        void SetCurrentContext(NkGuiContext* ctx) noexcept { gCurrentContext = ctx; }
        NkGuiContext* GetCurrentContext() noexcept { return gCurrentContext; }

        bool NkGuiContext::Init(int32 width, int32 height) noexcept {
            viewW = width;
            viewH = height;
            return true;
        }

        void NkGuiContext::Shutdown() noexcept {
            dl.Reset();
            if (GetCurrentContext() == this) SetCurrentContext(nullptr);
        }

        void NkGuiContext::BeginFrame(float32 dt) noexcept {
            input.dt = dt;
            input.NewFrame();              // transitions clic/relâche
            time     += dt;                // blink du caret
            hotIdPrev = hotId;             // le survol résolu de la frame précédente
            hotId     = NKGUI_ID_NONE;     // re-calculé par les widgets (greedy)
            interact  = NkGuiInteract::None;
            lastItemHovered = false;
            wantCursor = NkGuiCursor::Arrow;
            idDepth   = 0;
            disabledDepth = 0;
            inputClickConsumed = false;
            curPopupLevel = -1;            // le dessin reprend sur la couche principale
            winCount    = 0;              // pool de fenêtres ré-attribué cette frame
            curWindow   = -1;
            curWindowId = NKGUI_ID_NONE;
            curWindowDocked = false;
            containerDepth = 0;          // pile de conteneurs ré-attribuée
            overlayDepth   = 0;
            for (uint32 i = 0; i < windowMeta.Size(); ++i) { windowMeta[i].hostRendered = false; windowMeta[i].frameDL = -1; windowMeta[i].dockDL = -2; }  // #3 (dockDL=-2 : sentinelle « feuille non rendue »)
            dl.Reset();
            dlOverlay.Reset();
        }

        void NkGuiContext::EndFrame() noexcept {
            // ── Fenêtres : fusionner leurs draw-lists dans `dl` TRIÉES par z-order
            //    (recouvrement correct) + déterminer la fenêtre survolée (frame suivante).
            if (winCount > 0) {
                int32 order[WinMax];
                for (int32 i = 0; i < winCount; ++i) order[i] = i;
                for (int32 i = 1; i < winCount; ++i) {        // tri insertion par z croissant
                    const int32 k = order[i]; int32 j = i - 1;
                    while (j >= 0 && winZ[order[j]] > winZ[k]) { order[j + 1] = order[j]; --j; }
                    order[j + 1] = k;
                }
                for (int32 i = 0; i < winCount; ++i) dl.Append(winDL[order[i]]);

                const float32 th = ItemHeight();
                hoveredWindowId = NKGUI_ID_NONE; int32 bestZ = -2147483647;
                for (int32 i = 0; i < winCount; ++i) {
                    const NkGuiWindowMeta& m = windowMeta[winMeta[i]];
                    const NkRect hit = m.collapsed ? NkRect{ m.rect.x, m.rect.y, m.rect.w, th } : m.rect;
                    if (NkGuiRectContains(hit, input.mousePos) && m.zOrder > bestZ) {
                        bestZ = m.zOrder; hoveredWindowId = m.id;
                    }
                }
            } else {
                hoveredWindowId = NKGUI_ID_NONE;
            }

            // Fermeture de la chaîne de popups : Échap ferme le niveau le plus
            // profond ; un clic hors de TOUS les popups (et hors de l'ancre) ferme tout.
            if (popupDepth > 0) {
                if (input.KeyPressed(NkGuiKey::Escape)) {
                    --popupDepth;
                } else if (input.mouseClicked[0]) {
                    bool inside = NkGuiRectContains(popupAnchor, input.mousePos);
                    for (int32 i = 0; i < popupDepth && !inside; ++i)
                        if (NkGuiRectContains(popupRects[i], input.mousePos)) inside = true;
                    if (!inside) popupDepth = 0;
                }
            }
            // ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
            // Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
            // caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
            // TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
            if (!input.mouseDown[0] && activeId != NKGUI_ID_NONE) { activeId = NKGUI_ID_NONE; movingWindowId = NKGUI_ID_NONE; }
            // Fin du drag d'ancrage non abouti (relâché hors DockSpace) : on libère.
            if (input.mouseReleased[0]) movingWindowId = NKGUI_ID_NONE;
            // Défocus si un clic a eu lieu hors de tout champ texte.
            if (input.mouseClicked[0] && !inputClickConsumed) inputId = NKGUI_ID_NONE;
            input.wheel  = 0.f;            // molette consommée
            input.wheelH = 0.f;
            input.ClearPerFrameText();     // texte/touches consommés
        }

        void NkGuiContext::PushId(const char* s) noexcept {
            const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
            if (idDepth < 32) idStack[idDepth++] = NkGuiHashStr(s, seed);
        }
        void NkGuiContext::PushId(const void* p) noexcept {
            const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
            if (idDepth < 32) idStack[idDepth++] = NkGuiHashPtr(p, seed);
        }
        void NkGuiContext::PopId() noexcept {
            if (idDepth > 0) --idDepth;
        }
        NkGuiId NkGuiContext::GetId(const char* s) const noexcept {
            const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
            return NkGuiHashStr(s, seed);
        }

        // ── Layout ────────────────────────────────────────────────────────────
        void NkGuiContext::BeginLayout(const NkRect& region) noexcept {
            layout.region     = region;
            layout.cursor     = { region.x + layout.padding, region.y + layout.padding };
            layout.lineStartX = layout.cursor.x;
            layout.curLineH   = 0.f;
            layout.prevItem   = { layout.cursor.x, layout.cursor.y, 0.f, 0.f };
            layout.maxX       = layout.cursor.x;
            layout.maxY       = layout.cursor.y;
            layout.flow       = 0;      // vertical par défaut
            layout.gridIdx    = 0;
        }

        float32 NkGuiContext::ContentWidth() const noexcept {
            const float32 w = (layout.region.x + layout.region.w - layout.padding) - layout.cursor.x;
            return w > 1.f ? w : 1.f;
        }

        float32 NkGuiContext::AvailHeight() const noexcept {
            const float32 h = (layout.region.y + layout.region.h - layout.padding) - layout.cursor.y;
            return h > 1.f ? h : 1.f;
        }

        float32 NkGuiContext::ItemHeight() const noexcept {
            const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
            return lh + 2.f * theme.framePadY;
        }

        NkRect NkGuiContext::NextItemRect(float32 w, float32 h) noexcept {
            // ── HBox : flux horizontal (le curseur avance en X, pas de retour ligne) ──
            if (layout.flow == 1) {
                if (w <= 0.f) w = 120.f;                       // pas de « remplir » en HBox
                const NkRect rect = { layout.cursor.x, layout.cursor.y, w, h };
                layout.prevItem = rect;
                layout.cursor.x += w + layout.itemSpacingX;
                if (h > layout.curLineH) layout.curLineH = h;
                if (layout.cursor.x - layout.itemSpacingX > layout.maxX) layout.maxX = layout.cursor.x - layout.itemSpacingX;
                if (rect.y + h > layout.maxY) layout.maxY = rect.y + h;
                return rect;
            }
            // ── Grid : colonnes régulières, retour ligne automatique tous les gridCols ──
            if (layout.flow == 2) {
                const float32 cw = layout.gridColW > 0.f ? layout.gridColW : ContentWidth();
                if (w <= 0.f || w > cw) w = cw;
                const NkRect rect = { layout.cursor.x, layout.cursor.y, w, h };
                layout.prevItem = rect;
                if (h > layout.curLineH) layout.curLineH = h;
                if (rect.x + w > layout.maxX) layout.maxX = rect.x + w;
                ++layout.gridIdx;
                if (layout.gridCols > 0 && layout.gridIdx >= layout.gridCols) {  // fin de ligne
                    layout.gridIdx = 0;
                    layout.cursor.x = layout.lineStartX;
                    layout.cursor.y += layout.curLineH + layout.itemSpacingY;
                    layout.curLineH = 0.f;
                } else {
                    layout.cursor.x += layout.gridColW + layout.itemSpacingX;
                }
                if (rect.y + h > layout.maxY) layout.maxY = rect.y + h;
                return rect;
            }
            // ── Row : rangée flex horizontale (largeur = cellule pré-calculée, hauteur étirée) ──
            if (layout.flow == 5) {
                const float32 cw = (layout.flexIdx < layout.flexCount) ? layout.flexSlots[layout.flexIdx]
                                                                       : (w > 0.f ? w : 60.f);
                const float32 ch = layout.flexCross > 0.f ? layout.flexCross : h;
                const NkRect rect = { layout.cursor.x, layout.cursor.y, cw, ch };
                layout.prevItem = rect;
                layout.cursor.x += cw + layout.itemSpacingX;
                if (layout.cursor.x - layout.itemSpacingX > layout.maxX) layout.maxX = layout.cursor.x - layout.itemSpacingX;
                if (rect.y + ch > layout.maxY) layout.maxY = rect.y + ch;
                ++layout.flexIdx;
                return rect;
            }
            // ── Column : colonne flex verticale (hauteur = cellule pré-calculée, largeur étirée) ──
            if (layout.flow == 6) {
                const float32 ch = (layout.flexIdx < layout.flexCount) ? layout.flexSlots[layout.flexIdx]
                                                                       : (h > 0.f ? h : ItemHeight());
                const float32 cw = layout.flexCross > 0.f ? layout.flexCross : (w > 0.f ? w : ContentWidth());
                const NkRect rect = { layout.cursor.x, layout.cursor.y, cw, ch };
                layout.prevItem = rect;
                layout.cursor.y += ch + layout.itemSpacingY;
                if (rect.x + cw > layout.maxX) layout.maxX = rect.x + cw;
                if (layout.cursor.y > layout.maxY) layout.maxY = layout.cursor.y;
                ++layout.flexIdx;
                return rect;
            }
            // ── Flow : comme HBox mais passe à la ligne quand ça déborde la région ──
            if (layout.flow == 4) {
                if (w <= 0.f) w = 120.f;
                const float32 rightEdge = layout.region.x + layout.region.w - layout.padding;
                if (layout.cursor.x > layout.lineStartX && layout.cursor.x + w > rightEdge) {
                    layout.cursor.x = layout.lineStartX;                 // retour ligne
                    layout.cursor.y += layout.curLineH + layout.itemSpacingY;
                    layout.curLineH = 0.f;
                }
                const NkRect rect = { layout.cursor.x, layout.cursor.y, w, h };
                layout.prevItem = rect;
                layout.cursor.x += w + layout.itemSpacingX;
                if (h > layout.curLineH) layout.curLineH = h;
                if (rect.x + w > layout.maxX) layout.maxX = rect.x + w;
                if (rect.y + h > layout.maxY) layout.maxY = rect.y + h;
                return rect;
            }
            // ── Stack : tous les enfants superposés, ancrés dans la boîte stackW×stackH ──
            if (layout.flow == 3) {
                if (w <= 0.f) w = layout.stackW > 0.f ? layout.stackW : ContentWidth();
                if (h <= 0.f) h = layout.stackH;
                const float32 ax = static_cast<float32>(layout.stackAnchor % 3) * 0.5f;   // 0, .5, 1
                const float32 ay = static_cast<float32>(layout.stackAnchor / 3) * 0.5f;
                const NkRect rect = { layout.lineStartX + ax * (layout.stackW - w),
                                      layout.cursor.y   + ay * (layout.stackH - h), w, h };
                layout.prevItem = rect;
                const float32 r = layout.lineStartX + layout.stackW, b = layout.cursor.y + layout.stackH;
                if (r > layout.maxX) layout.maxX = r;
                if (b > layout.maxY) layout.maxY = b;
                return rect;                                   // curseur inchangé → superposition
            }
            // ── Vertical (défaut) : un item par ligne, le curseur descend ──
            if (w <= 0.f) w = ContentWidth();
            const NkRect rect = { layout.cursor.x, layout.cursor.y, w, h };
            layout.prevItem = rect;
            if (rect.x + rect.w > layout.maxX) layout.maxX = rect.x + rect.w;  // largeur contenu
            if (h > layout.curLineH) layout.curLineH = h;
            layout.cursor.x = layout.lineStartX;
            layout.cursor.y += layout.curLineH + layout.itemSpacingY;
            if (layout.cursor.y > layout.maxY) layout.maxY = layout.cursor.y;
            layout.curLineH = 0.f;
            return rect;
        }

        void NkGuiContext::SameLine(float32 spacingX) noexcept {
            const float32 sx = spacingX >= 0.f ? spacingX : layout.itemSpacingX;
            layout.cursor.x = layout.prevItem.x + layout.prevItem.w + sx;
            layout.cursor.y = layout.prevItem.y;
            layout.curLineH = layout.prevItem.h;
        }

        void NkGuiContext::Spacing(float32 px) noexcept {
            layout.cursor.y += (px >= 0.f ? px : layout.itemSpacingY);
        }

        void NkGuiContext::Indent(float32 w) noexcept {
            layout.lineStartX += w;
            layout.cursor.x = layout.lineStartX;
        }

        bool NkGuiContext::IsNodeOpen(NkGuiId id) const noexcept {
            for (uint32 i = 0; i < openNodes.Size(); ++i) if (openNodes[i] == id) return true;
            return false;
        }
        void NkGuiContext::SetNodeOpen(NkGuiId id, bool open) noexcept {
            for (uint32 i = 0; i < openNodes.Size(); ++i) {
                if (openNodes[i] == id) {
                    if (!open) { openNodes[i] = openNodes[openNodes.Size() - 1]; openNodes.PopBack(); }
                    return;
                }
            }
            if (open) openNodes.PushBack(id);
        }
        int32 NkGuiContext::GetTabIndex(NkGuiId bar) const noexcept {
            for (uint32 i = 0; i < tabBarKeys.Size(); ++i) if (tabBarKeys[i] == bar) return tabBarSel[i];
            return -1;
        }
        void NkGuiContext::SetTabIndex(NkGuiId bar, int32 idx) noexcept {
            for (uint32 i = 0; i < tabBarKeys.Size(); ++i) if (tabBarKeys[i] == bar) { tabBarSel[i] = idx; return; }
            tabBarKeys.PushBack(bar); tabBarSel.PushBack(idx);
        }

        bool NkGuiContext::IsHovered(const NkRect& r) const noexcept {
            // Un widget actif ailleurs capture le pointeur : pas de survol.
            if (activeId != NKGUI_ID_NONE) return false;
            return NkGuiRectContains(r, input.mousePos);
        }

        namespace {
            int32 FindKey(const NkVector<NkGuiId>& keys, NkGuiId id) noexcept {
                for (uint32 i = 0; i < keys.Size(); ++i) if (keys[i] == id) return static_cast<int32>(i);
                return -1;
            }
        } // namespace

        void NkGuiContext::BeginSelectList(const char* id, bool* mask, int32 count, NkGuiSelectFlags flags) noexcept {
            curSelList = GetId(id);
            selMask    = mask;
            selCount   = count;
            selMulti   = NkGuiHasSelectFlag(flags, NkGuiSelectFlags::MultiSelect);
            selIdx     = 0;
            const int32 si = FindKey(selKeys, curSelList);
            selFocus  = (si >= 0) ? selFocusStore[si]  : -1;
            selAnchor = (si >= 0) ? selAnchorStore[si] : -1;

            // Navigation clavier (seulement si cette liste a le focus).
            if (activeSelList == curSelList && count > 0 && mask) {
                int32 nf = selFocus;
                if (input.KeyPressedRepeat(NkGuiKey::Down)) nf = (nf < 0) ? 0 : (nf + 1 < count ? nf + 1 : count - 1);
                if (input.KeyPressedRepeat(NkGuiKey::Up))   nf = (nf < 0) ? 0 : (nf - 1 >= 0 ? nf - 1 : 0);
                if (nf != selFocus) {
                    selFocus = nf;
                    if (selMulti && input.shiftDown) {
                        const int32 a = selAnchor < 0 ? selFocus : selAnchor;
                        for (int32 i = 0; i < count; ++i) mask[i] = false;
                        const int32 lo = a < selFocus ? a : selFocus, hi = a < selFocus ? selFocus : a;
                        for (int32 i = lo; i <= hi; ++i) mask[i] = true;
                    } else if (!selMulti) {
                        for (int32 i = 0; i < count; ++i) mask[i] = false;
                        if (selFocus >= 0) mask[selFocus] = true;
                        selAnchor = selFocus;
                    }
                }
                if (input.KeyPressed(NkGuiKey::Enter) && selFocus >= 0) {
                    if (selMulti) mask[selFocus] = !mask[selFocus];
                    else { for (int32 i = 0; i < count; ++i) mask[i] = false; mask[selFocus] = true; }
                    selAnchor = selFocus;
                }
            }
        }

        void NkGuiContext::ApplySelectClick(int32 idx) noexcept {
            if (!selMask || idx < 0 || idx >= selCount) return;
            activeSelList = curSelList;
            selFocus = idx;
            if (!selMulti) {
                for (int32 i = 0; i < selCount; ++i) selMask[i] = false;
                selMask[idx] = true; selAnchor = idx;
            } else if (input.shiftDown) {
                const int32 a = selAnchor < 0 ? idx : selAnchor;
                for (int32 i = 0; i < selCount; ++i) selMask[i] = false;
                const int32 lo = a < idx ? a : idx, hi = a < idx ? idx : a;
                for (int32 i = lo; i <= hi; ++i) selMask[i] = true;
            } else if (input.ctrlDown) {
                selMask[idx] = !selMask[idx]; selAnchor = idx;
            } else {
                for (int32 i = 0; i < selCount; ++i) selMask[i] = false;
                selMask[idx] = true; selAnchor = idx;
            }
        }

        void NkGuiContext::EndSelectList() noexcept {
            const int32 si = FindKey(selKeys, curSelList);
            if (si >= 0) { selFocusStore[si] = selFocus; selAnchorStore[si] = selAnchor; }
            else { selKeys.PushBack(curSelList); selFocusStore.PushBack(selFocus); selAnchorStore.PushBack(selAnchor); }
            curSelList = NKGUI_ID_NONE; selMask = nullptr; selCount = 0;
        }

        void NkGuiContext::BeginDisabled(bool disabled) noexcept {
            const bool eff = disabled || IsDisabled();   // une fois désactivé, le reste l'est
            if (disabledDepth < 16) disabledStack[disabledDepth++] = eff;
        }
        void NkGuiContext::EndDisabled() noexcept {
            if (disabledDepth > 0) --disabledDepth;
        }

        bool NkGuiContext::ItemHoverable(const NkRect& r, NkGuiId id) noexcept {
            // Désactivé : aucune interaction.
            if (IsDisabled()) return false;
            // Un popup ouvert capture le pointeur : un widget ne réagit pas si le
            // pointeur est au-dessus d'un popup PLUS PROFOND que le niveau courant
            // (couche principale = -1 ; un item de menu ne capture pas sous son
            // sous-menu déployé).
            for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
                if (NkGuiRectContains(popupRects[i], input.mousePos)) return false;
            // Occlusion par fenêtre : hors popup, un widget ne réagit que si SA fenêtre
            // est celle survolée au-dessus (curWindowId). Bloque le fond ET les fenêtres
            // recouvertes. (hoveredWindowId/curWindowId = NONE pour le fond hors fenêtre.)
            if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
                return false;
            // Hors du CLIP courant (zone défilable, panneau) : pas d'interaction —
            // un item scrollé hors-vue ne doit pas capturer le pointeur.
            if (!NkGuiRectContains(DL().CurrentClip(), input.mousePos)) return false;
            // Bloqué si un AUTRE widget capture le pointeur.
            if (activeId != NKGUI_ID_NONE && activeId != id) return false;
            if (!NkGuiRectContains(r, input.mousePos)) return false;
            // Greedy : le DERNIER widget soumis sous le pointeur écrase hotId →
            // celui dessiné par-dessus gagne. On ne déclare « survolé » QUE le
            // front-most de la frame précédente (hotIdPrev) ; le widget masqué
            // dessous, lui, met à jour hotId mais retourne false → ne capture pas.
            hotId = id;
            return hotIdPrev == id;
        }

        bool NkGuiContext::ButtonBehavior(NkGuiId id, const NkRect& r, NkGuiButtonFlags flags,
                                          float32 repeatDelayOverride, float32 repeatRateOverride,
                                          bool* outHovered, bool* outHeld) noexcept {
            const bool hovered = ItemHoverable(r, id);   // respecte le z-ordre
            const bool repeat  = NkGuiHasFlag(flags, NkGuiButtonFlags::Repeat);

            bool pressed = false;
            if (hovered && input.mouseClicked[0]) {
                activeId = id;
                if (repeat) pressed = true;              // rafale : déclenche dès l'appui
            }

            const bool held = (activeId == id);
            if (held) {
                interact = NkGuiInteract::EditWidget;
                // Rafale : déclenchements typematic tant que maintenu DANS le rect.
                if (repeat && input.mouseDown[0] && NkGuiRectContains(r, input.mousePos)) {
                    const float32 rd = repeatDelayOverride >= 0.f ? repeatDelayOverride : repeatDelay;
                    const float32 rr = repeatRateOverride  >= 0.f ? repeatRateOverride  : repeatRate;
                    const float32 t1 = input.mouseDownDur[0];
                    if (NkGuiCalcTypematic(t1 - input.dt, t1, rd, rr) > 0) pressed = true;
                }
                if (input.mouseReleased[0]) {
                    // Sans Repeat : clic validé au relâchement DANS le rect.
                    if (!repeat && NkGuiRectContains(r, input.mousePos)) pressed = true;
                    activeId = NKGUI_ID_NONE;
                }
            } else if (hovered) {
                interact = NkGuiInteract::HoverWidget;
            }

            if (outHovered) *outHovered = hovered;
            if (outHeld)    *outHeld    = held;
            lastItemHovered = hovered;     // pour IsItemHovered() / SetTooltip
            return pressed;
        }

    } // namespace nkgui
} // namespace nkentseu
