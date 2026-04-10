// -----------------------------------------------------------------------------
// @File    NkUIWidgets.h
// @Brief   Tous les widgets NkUI — API immédiat-mode.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
//
// @Paradigme
//  Chaque widget est une fonction qui :
//    1. Calcule son rect (via NkUILayout)
//    2. Gère les interactions (hover, clic, drag, clavier)
//    3. Délègue le dessin au DrawList (ou au ShapeOverride si défini)
//    4. Retourne un résultat (bool pressé, float modifié, etc.)
//
//  Convention d'ID :
//    Les widgets prennent un "label" : "Mon Bouton" ou "Mon Bouton##id_unique"
//    La partie après ## sert uniquement à l'ID (invisible à l'écran).
//    "##hidden" : pas d'étiquette, ID = "hidden".
//
//  ShapeOverride :
//    Si l'utilisateur a enregistré un override pour ce type de widget,
//    le dessin par défaut est remplacé par son callback. L'interaction
//    (hover/clic) fonctionne toujours normalement.
// -----------------------------------------------------------------------------

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Public widget API declarations.
 * Main data: Buttons, sliders, combo, table, text inputs, helpers.
 * Change this file when: Adding/removing widget APIs or signatures.
 */

#include "NKUI/NkUIContext.h"
#include "NKUI/NkUILayout.h"
#include "NKUI/NkUIFont.h"

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Helpers de label
        // ============================================================

        /// Sépare "Label##ID" → label visuel + ID unique
        struct NkUILabelParts
        {
            char label[256] = {};
            char id[128]    = {};
        };

        NKUI_API NkUILabelParts NkParseLabel(const char* raw) noexcept;
        NKUI_API NkUIID         NkLabelToID(NkUIContext& ctx, const char* raw) noexcept;

        // ============================================================
        // Options pour les widgets
        // ============================================================

        enum class NkUISliderValuePlacement : uint8
        {
            NK_ABOVE_THUMB   = 0,
            NK_BELOW_THUMB   = 1,
            NK_LEFT_OF_SLIDER = 2,
            NK_RIGHT_OF_SLIDER = 3,
            NK_IN_LABEL      = 4,
            NK_HIDDEN        = 5
        };

        struct NkUISliderValueOptions
        {
            NkUISliderValuePlacement placement = NkUISliderValuePlacement::NK_ABOVE_THUMB;
            NkVec2 offset   = {0.f, 0.f};     // Décalage relatif final (pixels)
            float32 gap     = 4.f;            // Espacement de base vis-à-vis du slider
            bool    followThumb = true;       // Above/Below: suit le thumb si true, sinon centré sur slider
        };

        struct NkUITextInputOptions
        {
            bool    allowTextInput      = true;
            bool    repeatBackspace     = true;
            bool    repeatDelete        = true;
            bool    allowEnterNewLine   = true;
            float32 repeatDelay         = 0.35f;
            float32 repeatRate          = 0.05f;
            float32 multilineLineSpacing = 1.2f;
            float32 multilineCharSpacing = 0.35f;
            bool    multilineWrap       = true;
        };

        struct NkUIListBoxOptions
        {
            bool    multiSelect          = false;
            bool    toggleSelection      = true;
            bool    requireCtrlForMulti  = false;
            bool*   selectedMask         = nullptr;
            int32   selectedMaskCount    = 0;
        };

        // ============================================================
        // NkUIWidgets — namespace de tous les widgets
        // ============================================================

        struct NKUI_API NkUI
        {
            // ── Layout ────────────────────────────────────────────────────────────────
            static void BeginRow      (NkUIContext& ctx, NkUILayoutStack& ls, float32 height = 0) noexcept;
            static void EndRow        (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void BeginColumn   (NkUIContext& ctx, NkUILayoutStack& ls, float32 width = 0) noexcept;
            static void EndColumn     (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void BeginGroup    (NkUIContext& ctx, NkUILayoutStack& ls, const char* label = nullptr, bool border = true) noexcept;
            static void EndGroup      (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static bool BeginGrid     (NkUIContext& ctx, NkUILayoutStack& ls, int32 columns, float32 cellH = 0) noexcept;
            static void EndGrid       (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static bool BeginScrollRegion (NkUIContext& ctx, NkUILayoutStack& ls, const char* id, NkRect rect, float32* scrollX = nullptr, float32* scrollY = nullptr) noexcept;
            static void EndScrollRegion   (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static bool BeginSplit    (NkUIContext& ctx, NkUILayoutStack& ls, const char* id, NkRect rect, bool vertical = true, float32* ratio = nullptr) noexcept;
            static bool BeginSplitPane2 (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void EndSplit      (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;

            // Contrainte du prochain item
            static void SetNextWidth   (NkUIContext& ctx, NkUILayoutStack& ls, float32 w) noexcept;
            static void SetNextHeight  (NkUIContext& ctx, NkUILayoutStack& ls, float32 h) noexcept;
            static void SetNextWidthPct(NkUIContext& ctx, NkUILayoutStack& ls, float32 p) noexcept;
            static void SetNextGrow    (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void SameLine       (NkUIContext& ctx, NkUILayoutStack& ls, float32 offset = 0, float32 spacing = -1.f) noexcept;
            static void NewLine        (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void Spacing        (NkUIContext& ctx, NkUILayoutStack& ls, float32 px = -1.f) noexcept;
            static void Separator      (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl) noexcept;

            // ── Texte / Labels ────────────────────────────────────────────────────────
            static void Text          (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* text, NkColor col = {0,0,0,0}) noexcept;
            static void TextSmall     (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept;
            static void TextColored   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, NkColor col, const char* text) noexcept;
            static void TextWrapped   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept;
            static void LabelValue    (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, const char* value) noexcept;

            // ── Boutons ───────────────────────────────────────────────────────────────
            static bool Button        (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, NkVec2 size = {0,0}) noexcept;
            static bool ButtonSmall   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label) noexcept;
            static bool ButtonImage   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, uint32 texId, NkVec2 size, NkColor tint = NkColor::White()) noexcept;
            static bool InvisibleButton (NkUIContext& ctx, NkUILayoutStack& ls, const char* id, NkVec2 size) noexcept;

            // ── Checkbox / Radio / Toggle ─────────────────────────────────────────────
            static bool Checkbox      (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, bool& value) noexcept;
            static bool RadioButton   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& selected, int32 value) noexcept;
            static bool Toggle        (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, bool& value) noexcept;

            // ── Sliders ───────────────────────────────────────────────────────────────
            static bool SliderFloat   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, float32& value, float32 vmin, float32 vmax, const char* fmt = "%.2f", float32 width = 0, const NkUISliderValueOptions* valueOpts = nullptr) noexcept;
            static bool SliderInt     (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& value, int32 vmin, int32 vmax, float32 width = 0, const NkUISliderValueOptions* valueOpts = nullptr) noexcept;
            static bool SliderFloat2  (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, float32* values, float32 vmin, float32 vmax, float32 width = 0, const NkUISliderValueOptions* valueOpts = nullptr) noexcept;
            static bool DragFloat     (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, float32& value, float32 speed = 0.01f, float32 vmin = -1e9f, float32 vmax = 1e9f) noexcept;
            static bool DragInt       (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& value, float32 speed = 1.f, int32 vmin = INT32_MIN, int32 vmax = INT32_MAX) noexcept;

            // ── Input texte ───────────────────────────────────────────────────────────
            static bool InputText     (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, char* buf, int32 bufSize, float32 width = 0, const NkUITextInputOptions* opts = nullptr) noexcept;
            static bool InputInt      (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& value, const NkUITextInputOptions* opts = nullptr) noexcept;
            static bool InputFloat    (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, float32& value, const char* fmt = "%.3f", const NkUITextInputOptions* opts = nullptr) noexcept;
            static bool InputMultiline(NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, char* buf, int32 bufSize, NkVec2 size = {0,0}, const NkUITextInputOptions* opts = nullptr) noexcept;

            // ── Combo / List ──────────────────────────────────────────────────────────
            static bool Combo         (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& selected, const char* const* items, int32 numItems, float32 width = 0) noexcept;
            static bool ListBox       (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, int32& selected, const char* const* items, int32 numItems, int32 visibleCount = 6, const NkUIListBoxOptions* opts = nullptr) noexcept;

            // ── ProgressBar ───────────────────────────────────────────────────────────
            static void ProgressBar   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, float32 fraction, NkVec2 size = {0,0}, const char* overlay = nullptr) noexcept;
            static void ProgressBarVertical (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, float32 fraction, NkVec2 size = {0,0}, const char* overlay = nullptr) noexcept;
            static void ProgressBarCircular (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, float32 fraction, float32 diameter = 0.f, const char* overlay = nullptr) noexcept;

            // ── ColorPicker ───────────────────────────────────────────────────────────
            static bool ColorButton   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, NkColor& col, float32 size = 20.f) noexcept;
            static bool ColorPicker   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, NkColor& col) noexcept;

            // ── Tree ──────────────────────────────────────────────────────────────────
            static bool TreeNode      (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label, bool* open = nullptr) noexcept;
            static void TreePop       (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;

            // ── Table ─────────────────────────────────────────────────────────────────
            static bool BeginTable    (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* id, int32 columns, float32 width = 0) noexcept;
            static void TableHeader   (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, NkUIFont& font, const char* label) noexcept;
            static bool TableNextRow  (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl, bool striped = true) noexcept;
            static void TableNextCell (NkUIContext& ctx, NkUILayoutStack& ls) noexcept;
            static void EndTable      (NkUIContext& ctx, NkUILayoutStack& ls, NkUIDrawList& dl) noexcept;

            // ── Tooltip ───────────────────────────────────────────────────────────────
            static void Tooltip       (NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, const char* text) noexcept;
            static void SetTooltip    (NkUIContext& ctx, const char* text) noexcept;

            // ── Helpers de dessin ─────────────────────────────────────────────────────
            static void DrawWidgetBg  (NkUIDrawList& dl, const NkUITheme& t, NkRect r, NkUIWidgetType type, NkUIWidgetState state) noexcept;
            static void DrawLabel     (NkUIDrawList& dl, NkUIFont& f, const NkUITheme& t, NkRect r, const char* text, bool centered = false) noexcept;
            static NkUIWidgetState CalcState (NkUIContext& ctx, NkUIID id, NkRect r, bool enabled = true) noexcept;

            static void FlushPendingTooltip (NkUIContext& ctx, NkUIFont& font) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu