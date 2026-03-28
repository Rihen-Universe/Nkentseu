#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Central immediate-mode context state declaration.
 * Main data: IDs, focus/hot/active, layers, style stack, frame state.
 * Change this file when: You modify global UI state lifecycle or context APIs.
 */
/**
 * @File    NkUIContext.h
 * @Brief   Contexte global NkUI — état de l'interface par frame.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkUIContext est l'objet central de NkUI. Contrairement à ImGui
 *  qui utilise un singleton global statique, NkUIContext est explicite
 *  et peut exister en plusieurs instances simultanées (utile pour les
 *  rendus offline en parallèle ou les contextes multi-fenêtres).
 *
 *  Contenu :
 *    - Hot ID  : widget sous le curseur (hover)
 *    - Active ID : widget en interaction (clic maintenu, drag, édition)
 *    - Focus ID : widget actif au clavier (tab order)
 *    - DrawList : la liste de dessin courante (une par fenêtre)
 *    - État des fenêtres et du docking
 *    - Thème courant
 *    - Pile de layouts
 *    - Pile de styles
 *    - Animations en cours
 *
 *  Cycle de vie par frame :
 *    ctx.BeginFrame(input, dt);
 *      // ... appels widgets ...
 *    ctx.EndFrame();
 *    renderer.Submit(ctx);
 */
#include "NkUI/NkUITheme.h"
#include "NkUI/NkUIInput.h"
#include "NkUI/NkUIDrawList.h"

namespace nkentseu {
    namespace nkui {

        // Forward declarations (évite les inclusions circulaires)
        struct NkUIWindowManager;
        struct NkUILayoutStack;
        struct NkUIFont;

        // Hash FNV-1a 32 bits (rapide, pas de collision pour les IDs UI)
        NKUI_INLINE NkUIID NkHash(const char* str, NkUIID seed=2166136261u) noexcept {
            NkUIID h=seed;
            while(*str){h^=static_cast<uint8>(*str++);h*=16777619u;}
            return h?h:1u;
        }
        NKUI_INLINE NkUIID NkHashInt(int32 v,NkUIID seed=2166136261u) noexcept {
            NkUIID h=seed;
            for(int32 i=0;i<4;++i){h^=static_cast<uint8>(v>>(i*8));h*=16777619u;}
            return h?h:1u;
        }
        NKUI_INLINE NkUIID NkHashPtr(const void* p,NkUIID seed=2166136261u) noexcept {
            return NkHashInt(static_cast<int32>(reinterpret_cast<usize>(p)&0xFFFFFFFF),seed);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        //  Animation state machine
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUIAnim {
            NkUIID  id      = 0;
            float32 value   = 0.f;   // [0,1]
            float32 target  = 0.f;
            float32 speed   = 8.f;   // unités/seconde
        };

        // Easing functions
        NKUI_INLINE float32 NkEaseOut(float32 t) noexcept {return 1.f-(1.f-t)*(1.f-t);}
        NKUI_INLINE float32 NkEaseInOut(float32 t) noexcept {
            return t<0.5f?2*t*t:1-(-2*t+2)*(-2*t+2)*0.5f;}

        // ─────────────────────────────────────────────────────────────────────────────
        //  ShapeOverride — callback pour dessiner un widget librement
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUIShapeOverrideCtx {
            NkUIDrawList*    dl;
            NkRect           rect;
            NkUIWidgetState  state;
            const NkUITheme* theme;
            const void*      userData;
            const char*      label;   // peut être nullptr
            float32          value;   // valeur normalisée [0,1] pour sliders
        };
        using NkUIShapeOverrideFn = void(*)(NkUIShapeOverrideCtx&);

        // ─────────────────────────────────────────────────────────────────────────────
        //  StyleVar push/pop
        // ─────────────────────────────────────────────────────────────────────────────
        enum class NkStyleVar : uint8 {
            NK_ITEM_SPACING, NK_PADDING_X, NK_PADDING_Y,
            NK_CORNER_RADIUS, NK_BORDER_WIDTH, NK_ALPHA,
            NK_BUTTON_BG, NK_BUTTON_TEXT, NK_TEXT_COLOR,
            NK_COUNT
        };
        struct NkStyleVarEntry {
            NkStyleVar var;
            union { float32 f = 0.f; NkColor col; } prev;
        };

        // Mouse cursor hint exported by NKUI and consumed by platform adapters.
        enum class NkUIMouseCursor : uint8 {
            NK_ARROW = 0,
            NK_TEXT_INPUT,
            NK_HAND,
            NK_RESIZE_NS,
            NK_RESIZE_WE,
            NK_RESIZE_NWSE,
            NK_RESIZE_NESW
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIContext
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIContext {
            // ── IDs ──────────────────────────────────────────────────────────────────
            NkUIID   hotId     = NKUI_ID_NONE;  // widget survolé
            NkUIID   activeId  = NKUI_ID_NONE;  // widget actif (clic/drag)
            NkUIID   focusId   = NKUI_ID_NONE;  // widget focus clavier
            NkUIID   lastId    = NKUI_ID_NONE;  // dernier widget créé
            NkUIID   currentWindowId = NKUI_ID_NONE; // fenêtre en cours de build

            // ── Input ─────────────────────────────────────────────────────────────────
            NkUIInputState  input;
            float32         time     = 0.f;
            float32         dt       = 0.016f;
            uint64          frameNum = 0;
            bool            wheelConsumed  = false;
            bool            wheelHConsumed = false;
            bool            mouseClickConsumed[5] = {};
            bool            mouseReleaseConsumed[5] = {};
            NkUIMouseCursor mouseCursor    = NkUIMouseCursor::NK_ARROW;

            // ── Thème ─────────────────────────────────────────────────────────────────
            NkUITheme  theme;
            float32    globalAlpha = 1.f;

            // ── DrawLists ─────────────────────────────────────────────────────────────
            // Une DrawList par layer : background, windows, popups, overlay
            static constexpr int32 LAYER_BG      = 0;
            static constexpr int32 LAYER_WINDOWS = 1;
            static constexpr int32 LAYER_POPUPS  = 2;
            static constexpr int32 LAYER_OVERLAY = 3;
            static constexpr int32 LAYER_COUNT   = 4;
            NkUIDrawList  layers[LAYER_COUNT];
            NkUIDrawList* dl = nullptr;  // DrawList courante (pointe vers layers[x])

            // ── Viewport ──────────────────────────────────────────────────────────────
            int32  viewW = 1280;
            int32  viewH = 720;

            // ── ID stack (pour les scopes ##label) ───────────────────────────────────
            NkUIID  idStack[64];
            int32   idDepth = 0;

            // ── Style stack ───────────────────────────────────────────────────────────
            NkStyleVarEntry styleStack[128];
            int32           styleDepth = 0;

            // ── Animations ────────────────────────────────────────────────────────────
            NkUIAnim  anims[512];
            int32     numAnims = 0;

            // ── Curseur de layout courant ─────────────────────────────────────────────
            NkVec2  cursor       = {};
            NkVec2  cursorStart  = {};
            float32 lineHeight   = 0.f;
            bool    sameLine     = false;

            // ── Shape overrides ───────────────────────────────────────────────────────
            NkUIShapeOverrideFn overrides[static_cast<int32>(NkUIWidgetType::NK_COUNT)] = {};
            const void*         overrideUD[static_cast<int32>(NkUIWidgetType::NK_COUNT)] = {};
            NkUIFont*           activeFont = nullptr;

            // ─────────────────────────────────────────────────────────────────────────
            //  Cycle de vie
            // ─────────────────────────────────────────────────────────────────────────
            bool Init(int32 viewportW=1280, int32 viewportH=720) noexcept;
            void Destroy() noexcept;

            /// Appeler au début de chaque frame avec l'état input actuel
            void BeginFrame(const NkUIInputState& input, float32 dt_) noexcept;

            /// Appeler à la fin de chaque frame — finalise les DrawLists
            void EndFrame() noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  Gestion des IDs
            // ─────────────────────────────────────────────────────────────────────────
            NkUIID  GetID(const char* str)  const noexcept;
            NkUIID  GetID(int32 n)          const noexcept;
            NkUIID  GetID(const void* ptr)  const noexcept;
            void    PushID(const char* str) noexcept;
            void    PushID(int32 n)         noexcept;
            void    PopID()                 noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  États des widgets
            // ─────────────────────────────────────────────────────────────────────────
            bool IsHot(NkUIID id)    const noexcept { return hotId==id; }
            bool IsActive(NkUIID id) const noexcept { return activeId==id; }
            bool IsFocused(NkUIID id)const noexcept { return focusId==id; }

            void SetHot(NkUIID id)    noexcept { hotId=id; }
            void SetActive(NkUIID id) noexcept { activeId=id; }
            void SetFocus(NkUIID id)  noexcept { focusId=id; }
            void ClearHot()           noexcept { hotId=NKUI_ID_NONE; }
            void ClearActive()        noexcept { activeId=NKUI_ID_NONE; }

            // ─────────────────────────────────────────────────────────────────────────
            //  Hit testing
            // ─────────────────────────────────────────────────────────────────────────
            bool IsHovered(NkRect r) const noexcept {
                return NkRectContains(r,input.mousePos);
            }
            bool IsHoveredCircle(NkVec2 c,float32 r) const noexcept {
                const float32 dx=input.mousePos.x-c.x, dy=input.mousePos.y-c.y;
                return dx*dx+dy*dy<=r*r;
            }

            // ─────────────────────────────────────────────────────────────────────────
            //  Animations
            // ─────────────────────────────────────────────────────────────────────────
            float32 Animate(NkUIID id,float32 target,float32 speed=8.f) noexcept;
            void    AnimateToward(NkUIID id,float32 target) noexcept;
            void    StepAnimations() noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  Style stack
            // ─────────────────────────────────────────────────────────────────────────
            void PushStyleColor(NkStyleVar var,NkColor col) noexcept;
            void PushStyleVar(NkStyleVar var,float32 val)   noexcept;
            void PopStyle(int32 count=1) noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  Shape overrides
            // ─────────────────────────────────────────────────────────────────────────
            void SetShapeOverride(NkUIWidgetType type,
                                NkUIShapeOverrideFn fn,
                                const void* ud=nullptr) noexcept;
            void ClearShapeOverride(NkUIWidgetType type) noexcept;
            bool CallShapeOverride(NkUIWidgetType type,
                                    NkRect rect,NkUIWidgetState state,
                                    const char* label=nullptr,float32 val=0.f) noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  Layout helpers
            // ─────────────────────────────────────────────────────────────────────────
            void  SetCursor(NkVec2 pos) noexcept { cursor=pos; }
            void  AdvanceCursor(NkVec2 size) noexcept;
            void  SameLine(float32 offsetX=0,float32 spacing=-1.f) noexcept;
            void  NewLine() noexcept;
            void  Spacing(float32 pixels=-1.f) noexcept;
            NkVec2 GetCursor() const noexcept { return cursor; }
            NkVec2 GetCursorStart() const noexcept { return cursorStart; }
            bool IsMouseClicked(int32 button = 0) const noexcept {
                return button >= 0 && button < 5
                    && input.IsMouseClicked(button)
                    && !mouseClickConsumed[button];
            }
            bool IsMouseReleased(int32 button = 0) const noexcept {
                return button >= 0 && button < 5
                    && input.IsMouseReleased(button)
                    && !mouseReleaseConsumed[button];
            }
            bool ConsumeMouseClick(int32 button = 0) noexcept {
                if (!IsMouseClicked(button)) {
                    return false;
                }
                mouseClickConsumed[button] = true;
                return true;
            }
            bool ConsumeMouseRelease(int32 button = 0) noexcept {
                if (!IsMouseReleased(button)) {
                    return false;
                }
                mouseReleaseConsumed[button] = true;
                return true;
            }
            void SetMouseCursor(NkUIMouseCursor cursor_) noexcept { mouseCursor = cursor_; }
            NkUIMouseCursor GetMouseCursor() const noexcept { return mouseCursor; }
            void SetActiveFont(NkUIFont* font) noexcept { activeFont = font; }
            NkUIFont* GetActiveFont() const noexcept { return activeFont; }

            // ─────────────────────────────────────────────────────────────────────────
            //  Thème
            // ─────────────────────────────────────────────────────────────────────────
            void SetTheme(const NkUITheme& t) noexcept { theme=t; }
            const NkUITheme& GetTheme() const noexcept { return theme; }
            bool LoadThemeJSON(const char* path) noexcept;
            bool ParseThemeJSON(const char* json, usize len) noexcept;
            void SaveThemeJSON(const char* path) const noexcept;

            // ─────────────────────────────────────────────────────────────────────────
            //  Accès aux couleurs/métriques (avec stack override)
            // ─────────────────────────────────────────────────────────────────────────
            NkColor GetColor(NkUIWidgetType type,NkUIWidgetState state) const noexcept;
            float32 GetCornerRadius() const noexcept {return theme.metrics.cornerRadius;}
            float32 GetItemHeight()   const noexcept {return theme.metrics.itemHeight;}
            float32 GetPaddingX()     const noexcept {return theme.metrics.paddingX;}
            float32 GetPaddingY()     const noexcept {return theme.metrics.paddingY;}

        // Pointeur vers le WindowManager actif (set par NkUIWindow::Begin)
            NkUIWindowManager* wm = nullptr;

        private:
            bool  mInitialized = false;
        };
    }
} // namespace nkentseu
