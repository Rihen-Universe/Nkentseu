// -----------------------------------------------------------------------------
// @File    NkUITheme.h
// @Brief   Thème NkUI — couleurs, mesures, polices, animations.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
//
// @Design
//  NkUITheme contient TOUTES les valeurs visuelles de l'interface.
//  Il peut être :
//    - Construit manuellement (valeurs C++)
//    - Chargé depuis un fichier JSON (via NkUIThemeJSON)
//    - Hot-reloadé en runtime (NkUITheme::LoadJSON retourne un nouveau thème)
//
//  NkUIStyle est le style calculé d'un widget spécifique.
//  Il hérite du thème mais peut être surchargé localement via NkUI::PushStyle().
//
//  ShapeOverride : l'utilisateur peut enregistrer un callback pour dessiner
//  n'importe quel widget d'une façon entièrement personnalisée, sans toucher
//  au reste du système.
// -----------------------------------------------------------------------------

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Theme, palette, metrics and animation tuning structures.
 * Main data: Colors, spacing, radii, typography and style knobs.
 * Change this file when: You add style options or theme presets.
 */

#include "NKUI/NkUIExport.h"

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Palette de couleurs sémantiques
        // ============================================================

        struct NKUI_API NkUIColorPalette
        {
            // Fond
            NkColor bgPrimary       = NkColor::Gray(245);
            NkColor bgSecondary     = NkColor::Gray(235);
            NkColor bgTertiary      = NkColor::Gray(220);
            NkColor bgWindow        = NkColor::Gray(250);
            NkColor bgPopup         = NkColor::Gray(252);
            NkColor bgHeader        = NkColor::Gray(60);

            // Bordures
            NkColor border          = NkColor::Gray(200);
            NkColor borderFocus     = {0,120,255,255};
            NkColor borderHover     = NkColor::Gray(170);

            // Texte
            NkColor textPrimary     = NkColor::Gray(20);
            NkColor textSecondary   = NkColor::Gray(100);
            NkColor textDisabled    = NkColor::Gray(180);
            NkColor textOnAccent    = NkColor::White();

            // Accents
            NkColor accent          = {0,120,255,255};
            NkColor accentHover     = {0,100,230,255};
            NkColor accentActive    = {0,80,200,255};
            NkColor accentDisabled  = NkColor::Gray(180);

            // Widgets
            NkColor buttonBg        = NkColor::Gray(230);
            NkColor buttonHover     = NkColor::Gray(215);
            NkColor buttonActive    = NkColor::Gray(195);
            NkColor buttonText      = NkColor::Gray(20);
            NkColor checkBg         = NkColor::White();
            NkColor checkMark       = {0,120,255,255};
            NkColor sliderTrack     = NkColor::Gray(210);
            NkColor sliderThumb     = {0,120,255,255};
            NkColor inputBg         = NkColor::White();
            NkColor inputText       = NkColor::Gray(20);
            NkColor inputPlaceholder= NkColor::Gray(160);
            NkColor inputCursor     = {0,120,255,255};
            NkColor inputSelection  = {0,120,255,80};
            NkColor scrollBg        = NkColor::Gray(230);
            NkColor scrollThumb     = NkColor::Gray(180);
            NkColor scrollThumbHov  = NkColor::Gray(150);

            // Fenêtres / Dock
            NkColor titleBarBg      = NkColor::Gray(55);
            NkColor titleBarActive  = NkColor::Gray(40);
            NkColor titleBarText    = NkColor::White();
            NkColor titleBarBtn     = NkColor::Gray(120);
            NkColor titleBarBtnHov  = NkColor::Gray(160);
            NkColor separator       = NkColor::Gray(200);
            NkColor dockZone        = {0,120,255,60};
            NkColor dockZoneBorder  = {0,120,255,200};
            NkColor tabBg           = NkColor::Gray(225);
            NkColor tabActive       = NkColor::Gray(250);
            NkColor tabText         = NkColor::Gray(60);
            NkColor tabActiveText   = NkColor::Gray(20);

            // États spéciaux
            NkColor success         = {40,180,40,255};
            NkColor warning         = {220,160,0,255};
            NkColor danger          = {220,50,50,255};
            NkColor info            = {0,120,255,255};
            NkColor tooltip         = NkColor::Gray(50);
            NkColor tooltipText     = NkColor::White();
            NkColor overlay         = NkColor::Gray(0,160);
        };

        // ============================================================
        // Mesures (espacements, tailles, coins)
        // ============================================================

        struct NKUI_API NkUIMetrics
        {
            // Padding intérieur des widgets
            float32 paddingX        = 10.f;
            float32 paddingY        = 6.f;

            // Espacement entre widgets dans un layout
            float32 spacingX        = 8.f;
            float32 spacingY        = 6.f;
            float32 itemSpacing     = 6.f;
            float32 sectionSpacing  = 14.f;

            // Coins arrondis
            float32 cornerRadius    = 5.f;
            float32 cornerRadiusLg  = 9.f;
            float32 cornerRadiusSm  = 3.f;

            // Bordures
            float32 borderWidth     = 1.f;
            float32 borderWidthFocus= 2.f;

            // Hauteurs standard
            float32 itemHeight      = 28.f;
            float32 itemHeightSm    = 22.f;
            float32 itemHeightLg    = 36.f;

            // Fenêtres
            float32 titleBarHeight  = 32.f;
            float32 titleBarPadX    = 12.f;
            float32 scrollbarWidth  = 12.f;
            float32 scrollbarMinLen = 20.f;
            float32 resizeBorder    = 6.f;
            float32 windowPadX      = 12.f;
            float32 windowPadY      = 8.f;
            float32 windowMinW      = 100.f;
            float32 windowMinH      = 60.f;

            // Dock
            float32 dockZoneSize    = 60.f;     // zone de dépôt
            float32 dockTabHeight   = 28.f;
            float32 dockSplitW      = 4.f;

            // Widgets spécifiques
            float32 checkboxSize    = 16.f;
            float32 radioSize       = 16.f;
            float32 sliderThumbW    = 14.f;
            float32 sliderTrackH    = 4.f;
            float32 colorPickerW    = 200.f;
            float32 comboArrowW     = 22.f;
            float32 treeIndent      = 18.f;
            float32 tooltipDelay    = 0.5f;    // secondes
            float32 tooltipPadX     = 8.f;
            float32 tooltipPadY     = 5.f;
        };

        // ============================================================
        // Polices (références — résolues par NkUIFont)
        // ============================================================

        struct NKUI_API NkUIFontDef
        {
            char    family[64]  = "sans-serif";   // nom ou chemin
            float32 size        = 14.f;
            bool    bold        = false;
            bool    italic      = false;
            float32 lineHeight  = 1.3f;
            float32 letterSpacing = 0.f;
        };

        struct NKUI_API NkUIFonts
        {
            NkUIFontDef body;
            NkUIFontDef small    = { .size = 11.f };
            NkUIFontDef large    = { .size = 17.f };
            NkUIFontDef heading  = { .size = 16.f, .bold = true };
            NkUIFontDef mono     = { .family = "monospace", .size = 13.f };
            NkUIFontDef icon     = {};   // police d'icônes (optionnel)
        };

        // ============================================================
        // Animations
        // ============================================================

        struct NKUI_API NkUIAnimDef
        {
            float32 hoverDuration   = 0.12f;
            float32 pressDuration   = 0.08f;
            float32 openDuration    = 0.18f;
            float32 closeDuration   = 0.14f;
            float32 scrollDuration  = 0.25f;

            // Easing : 0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out
            uint8   hoverEasing     = 2;
            uint8   openEasing      = 3;
            bool    enabled         = true;
        };

        // ============================================================
        // NkUITheme — thème complet
        // ============================================================

        struct NKUI_API NkUITheme
        {
            char            name[64]    = "NkDefault";
            bool            darkMode    = false;
            NkUIColorPalette colors;
            NkUIMetrics      metrics;
            NkUIFonts        fonts;
            NkUIAnimDef      anim;

            // Thèmes prédéfinis
            static NkUITheme Default()      noexcept;
            static NkUITheme Dark()         noexcept;
            static NkUITheme Minimal()      noexcept;
            static NkUITheme HighContrast() noexcept;
        };

        // ============================================================
        // NkUIWidgetType — liste de tous les types de widgets
        // ============================================================

        enum class NkUIWidgetType : uint8
        {
            NK_NONE = 0,
            NK_BUTTON,
            NK_BUTTON_SMALL,
            NK_BUTTON_LARGE,
            NK_CHECKBOX,
            NK_RADIO,
            NK_TOGGLE,
            NK_SLIDER_FLOAT,
            NK_SLIDER_INT,
            NK_SLIDER_FLOAT2,
            NK_INPUT_TEXT,
            NK_INPUT_INT,
            NK_INPUT_FLOAT,
            NK_INPUT_MULTILINE,
            NK_COMBO,
            NK_COMBO_MULTI,
            NK_LIST_BOX,
            NK_TREE_NODE,
            NK_TREE_LEAF,
            NK_TABLE,
            NK_TABLE_ROW,
            NK_TABLE_CELL,
            NK_PROGRESS_BAR,
            NK_PROGRESS_CIRCLE,
            NK_SEPARATOR,
            NK_SPACER,
            NK_LABEL,
            NK_LABEL_SMALL,
            NK_LABEL_LARGE,
            NK_IMAGE,
            NK_CANVAS,
            NK_COLOR_PICKER,
            NK_COLOR_BUTTON,
            NK_SCROLLBAR,
            NK_TITLE_BAR,
            NK_CLOSE_BUTTON,
            NK_MIN_BUTTON,
            NK_MAX_BUTTON,
            NK_TAB,
            NK_TAB_BAR,
            NK_MENU_BAR,
            NK_MENU_ITEM,
            NK_MENU,
            NK_TOOLTIP,
            NK_MODAL,
            NK_CUSTOM,
            NK_COUNT
        };

        // ============================================================
        // NkUIWidgetState — état combiné d'un widget
        // ============================================================

        enum class NkUIWidgetState : uint8
        {
            NK_NORMAL   = 0,
            NK_HOVERED  = 1 << 0,
            NK_ACTIVE   = 1 << 1,
            NK_FOCUSED  = 1 << 2,
            NK_DISABLED = 1 << 3,
            NK_SELECTED = 1 << 4,
            NK_CHECKED  = 1 << 5,
        };

        // Opérateur OU binaire pour NkUIWidgetState
        NKUI_INLINE NkUIWidgetState operator|(NkUIWidgetState a, NkUIWidgetState b) noexcept
        {
            return static_cast<NkUIWidgetState>(static_cast<uint8>(a) | static_cast<uint8>(b));
        }

        // Vérifie si un état contient un drapeau donné
        NKUI_INLINE bool HasState(NkUIWidgetState s, NkUIWidgetState flag) noexcept
        {
            return (static_cast<uint8>(s) & static_cast<uint8>(flag)) != 0;
        }

    } // namespace nkui
} // namespace nkentseu