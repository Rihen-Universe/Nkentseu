#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Layout stack/node model and layout algorithm API.
 * Main data: Row/column/grid/scroll/split node definitions and helpers.
 * Change this file when: Widget placement rules or scroll/split contracts evolve.
 */
/**
 * @File    NkUILayout.h
 * @Brief   Système de layout NkUI — Row, Column, Tab, Group, Splitter, Scroll.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Le layout NkUI est implémenté en mode immédiat comme ImGui, mais avec
 *  un système de contraintes flex-like qui permet de définir des tailles
 *  relatives (pourcentage) ou absolues, avec min/max.
 *
 *  Hiérarchie de layouts :
 *    NkUILayoutStack  — pile des layouts actifs (max 32 niveaux)
 *    NkUILayoutNode   — un nœud de layout (row, column, group, tab, split)
 *
 *  Fonctionnement :
 *    NkUI::BeginRow(ctx)   — empile un layout horizontal
 *      NkUI::SetNextWidth(ctx, 100)   — taille du prochain widget
 *      NkUI::Button(ctx, "A")          — widget placé à la position calculée
 *      NkUI::Button(ctx, "B")          — widget suivant
 *    NkUI::EndRow(ctx)     — dépile, avance le curseur du parent
 *
 *  Scroll :
 *    NkUI::BeginScrollRegion(ctx, id, rect, &scrollX, &scrollY)
 *    NkUI::EndScrollRegion(ctx)
 *    Affiche une scrollbar si le contenu dépasse le rect.
 */
#include "NkUI/NkUIContext.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Types de layout
        // ─────────────────────────────────────────────────────────────────────────────
        enum class NkUILayoutType : uint8 {
            NK_NONE=0,
            NK_FREE,    // positionnement libre (défaut)
            NK_ROW,     // horizontal gauche-droite
            NK_COLUMN,  // vertical haut-bas
            NK_GRID,    // grille N colonnes
            NK_SPLIT,   // deux panneaux séparés par un splitter draggable
            NK_TAB,     // barre d'onglets
            NK_SCROLL,  // région scrollable
            NK_GROUP,   // groupe avec bordure optionnelle
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  Contrainte de taille d'un item
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUIConstraint {
            enum Type : uint8 { NK_AUTO=0, NK_FIXED, NK_PERCENT, NK_GROW } type = NK_AUTO;
            float32 value   = 0.f;   // pixels ou [0,1] selon type
            float32 minSize = 0.f;
            float32 maxSize = 1e9f;

            static NkUIConstraint Auto_()              noexcept { return {NK_AUTO,0}; }
            static NkUIConstraint Fixed_(float32 px)   noexcept { return {NK_FIXED,px}; }
            static NkUIConstraint Percent_(float32 p)  noexcept { return {NK_PERCENT,p}; }
            static NkUIConstraint Grow_(float32 flex=1)noexcept { return {NK_GROW,flex}; }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  Nœud de layout (une entrée dans la pile)
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUILayoutNode {
            NkUILayoutType type    = NkUILayoutType::NK_FREE;
            NkRect         bounds  = {};    // zone du layout dans le viewport
            NkVec2         cursor  = {};    // curseur courant dans le layout
            NkVec2         contentSize={};  // taille du contenu (pour scroll)
            float32        lineH   = 0.f;   // hauteur de la ligne courante (row)
            float32        colW    = 0.f;   // largeur colonne courante (column)
            NkUIConstraint nextW;           // contrainte du prochain item (largeur)
            NkUIConstraint nextH;           // contrainte du prochain item (hauteur)
            float32        scrollX = 0.f;
            float32        scrollY = 0.f;
            float32*       scrollXPtr = nullptr;  // pointeur de l'appelant
            float32*       scrollYPtr = nullptr;
            NkUIID         id      = 0;
            bool           clipped = false;  // clip rect actif
            int32          itemCount= 0;
            // Grid
            int32          gridCols= 1;
            int32          gridCol = 0;
            float32        gridCellW=0;
            // Split
            float32        splitRatio=0.5f;
            float32*       splitRatioPtr=nullptr;
            bool           splitVertical=true;
            bool           splitSecondPane=false;
            // Tab
            int32*         activeTab=nullptr;
            int32          tabCount=0;
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUILayoutStack — pile des layouts actifs
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUILayoutStack {
            static constexpr int32 MAX_DEPTH = 32;
            NkUILayoutNode nodes[MAX_DEPTH];
            int32          depth = 0;

            NkUILayoutNode* Top()    noexcept { return depth>0?&nodes[depth-1]:nullptr; }
            NkUILayoutNode* Push()   noexcept { return depth<MAX_DEPTH?&nodes[depth++]:nullptr; }
            void            Pop()    noexcept { if(depth>0)--depth; }
            bool            Empty()  const noexcept { return depth==0; }
            int32           Depth()  const noexcept { return depth; }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUILayout — logique de calcul de position et taille
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUILayout {
            /**
             * @Brief Calcule le rect du prochain widget selon la contrainte et le layout.
             * @param ctx    Contexte UI
             * @param stack  Pile de layout active
             * @param wantW  Largeur naturelle du widget (si contrainte = Auto)
             * @param wantH  Hauteur naturelle du widget
             * @return       Rect de placement dans l'espace viewport
             */
            static NkRect NextItemRect(NkUIContext& ctx,
                                        NkUILayoutStack& stack,
                                        float32 wantW, float32 wantH) noexcept;

            /// Signale que le dernier widget a occupé ce rect (avance le curseur)
            static void   AdvanceItem(NkUIContext& ctx,
                                    NkUILayoutStack& stack,
                                    NkRect placed) noexcept;

            /// Résout la largeur d'un item selon sa contrainte et le layout parent
            static float32 ResolveWidth(const NkUILayoutNode& node,
                                        const NkUIConstraint& c,
                                        float32 want) noexcept;
            static float32 ResolveHeight(const NkUILayoutNode& node,
                                        const NkUIConstraint& c,
                                        float32 want) noexcept;

            /// Dessine une scrollbar verticale/horizontale, retourne true si cliquée
            static bool DrawScrollbar(NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    bool vertical,
                                    NkRect track,
                                    float32 contentSize,
                                    float32 viewSize,
                                    float32& scroll,
                                    NkUIID id) noexcept;

            /// Dessine un splitter draggable, met à jour ratio
            static bool DrawSplitter(NkUIContext& ctx,
                                    NkUIDrawList& dl,
                                    NkRect rect,
                                    bool vertical,
                                    float32& ratio,
                                    NkUIID id) noexcept;
        };
    }
} // namespace nkentseu
