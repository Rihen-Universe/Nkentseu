#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Window manager data model and window API declarations.
 * Main data: Window state, z-order, move/resize flags and contracts.
 * Change this file when: Window interaction model or API shape changes.
 */
/**
 * @File    NkUIWindow.h
 * @Brief   Fenêtres flottantes NkUI — titre, resize, move, scroll, z-order.
 */
#include "NkUI/NkUILayout.h"
#include "NkUI/NkUIFont.h"

namespace nkentseu {
    namespace nkui {

        enum class NkUIWindowFlags : uint32 {
            NK_NONE                   = 0,
            NK_NO_TITLE_BAR           = 1<<0,
            NK_NO_RESIZE              = 1<<1,
            NK_NO_MOVE                = 1<<2,
            NK_NO_SCROLLBAR           = 1<<3,
            NK_NO_SCROLL_WITH_MOUSE   = 1<<4,
            NK_NO_COLLAPSE            = 1<<5,
            NK_NO_CLOSE               = 1<<6,
            NK_NO_BACKGROUND          = 1<<7,
            NK_NO_BRING_TO_FRONT_ON_FOCUS = 1<<8,
            NK_ALWAYS_ON_TOP          = 1<<9,
            NK_MODAL                  = 1<<10,
            NK_AUTO_SIZE              = 1<<11,
            NK_CHILD_WINDOW           = 1<<12,
        };
        NKUI_INLINE NkUIWindowFlags operator|(NkUIWindowFlags a,NkUIWindowFlags b)noexcept{
            return static_cast<NkUIWindowFlags>(static_cast<uint32>(a)|static_cast<uint32>(b));}
        NKUI_INLINE bool HasFlag(NkUIWindowFlags flags,NkUIWindowFlags f)noexcept{
            return (static_cast<uint32>(flags)&static_cast<uint32>(f))!=0;}

        struct NKUI_API NkUIWindowState {
            char             name[64]      = {};
            NkUIID           id            = 0;
            NkVec2           pos           = {100,100};
            NkVec2           size          = {400,300};
            NkVec2           minSize       = {100,60};
            NkVec2           maxSize       = {9999,9999};
            NkVec2           contentSize   = {};
            float32          scrollX       = 0;
            float32          scrollY       = 0;
            int32            zOrder        = 0;
            NkUIWindowFlags  flags         = NkUIWindowFlags::NK_NONE;
            bool             isOpen        = true;
            bool             isCollapsed   = false;
            bool             isDocked      = false;
            NkUIID           dockNodeId    = 0;
        };

        struct NKUI_API NkUIWindowManager {
            static constexpr int32 MAX_WINDOWS = 64;
            NkUIWindowState  windows[MAX_WINDOWS] = {};
            int32            numWindows  = 0;
            NkUIID           zOrder[MAX_WINDOWS]  = {};
            int32            numZOrder   = 0;
            NkUIID           activeId    = NKUI_ID_NONE;
            NkUIID           hoveredId   = NKUI_ID_NONE;
            NkUIID           movingId    = NKUI_ID_NONE;
            NkUIID           resizingId  = NKUI_ID_NONE;
            NkVec2           moveOffset  = {};
            NkVec2           resizeStartMouse = {};
            NkVec2           resizeStartPos   = {};
            NkVec2           resizeStartSize  = {};
            uint8            resizeEdge  = 0;

            void Init() noexcept;
            void Destroy() noexcept;
            NkUIWindowState* FindOrCreate(const char* name, NkVec2 pos, NkVec2 size,
                                            NkUIWindowFlags flags=NkUIWindowFlags::NK_NONE) noexcept;
            NkUIWindowState* Find(NkUIID id) noexcept;
            NkUIWindowState* Find(const char* name) noexcept;
            void BringToFront(NkUIID id) noexcept;
            void SortZOrder() noexcept;
            NkUIWindowState* HitTest(NkVec2 pos, float32 titleBarHeight) noexcept;
            void BeginFrame(NkUIContext& ctx) noexcept;
            void EndFrame(NkUIContext& ctx) noexcept;
        };

        // Patch NkUIContext pour stocker le WindowManager
        // (ajout non-intrusif via pointeur)
        // On déclare wm dans NkUIContext via une extension
        // Pour l'instant on utilise un champ global

        struct NKUI_API NkUIWindow {
            // ── API principale ────────────────────────────────────────────────────────
            static bool Begin(NkUIContext& ctx, NkUIWindowManager& wm,
                            NkUIDrawList& dl, NkUIFont& font, NkUILayoutStack& ls,
                            const char* name,
                            bool* pOpen = nullptr,
                            NkUIWindowFlags flags = NkUIWindowFlags::NK_NONE) noexcept;
            static void End(NkUIContext& ctx, NkUIWindowManager& wm,
                            NkUIDrawList& dl, NkUILayoutStack& ls) noexcept;

            // ── Helpers ───────────────────────────────────────────────────────────────
            static void SetNextWindowPos(NkVec2 pos) noexcept;
            static void SetNextWindowSize(NkVec2 size) noexcept;
            static NkVec2 GetWindowPos(NkUIWindowManager& wm, const char* name) noexcept;
            static NkVec2 GetWindowSize(NkUIWindowManager& wm, const char* name) noexcept;
            static bool IsWindowFocused(NkUIWindowManager& wm, const char* name) noexcept;
            static bool IsWindowHovered(NkUIContext& ctx, NkUIWindowManager& wm,
                                        const char* name) noexcept;
            static void SetWindowPos(NkUIWindowManager& wm, const char* name,
                                    NkVec2 pos) noexcept;
            static void SetWindowSize(NkUIWindowManager& wm, const char* name,
                                        NkVec2 size) noexcept;
            static void SetWindowCollapsed(NkUIWindowManager& wm, const char* name,
                                            bool collapsed) noexcept;
            static void CloseWindow(NkUIWindowManager& wm, const char* name) noexcept;

            // ── Rendu ─────────────────────────────────────────────────────────────────
            static void RenderAll(NkUIContext& ctx, NkUIWindowManager& wm,
                                NkUIDrawList& dl, NkUIFont& font) noexcept;

        private:
            struct Current {
                NkUIWindowState*  ws;
                NkUIWindowManager* wm;
                NkVec2             startCursor;
                NkVec2             reserved;
                NkUIDrawList*      savedDL;
                int32              savedLSDepth;   // ← AJOUTER
                NkUILayoutStack*   ls;             // ← AJOUTER
                NkUIID             savedWindowId;
            };

            static Current sStack[8];
            static int32   sDepth;
            static NkVec2  sNextPos;
            static NkVec2  sNextSize;
            static bool    sHasNextPos;
            static bool    sHasNextSize;

            static void DrawShadow(NkUIDrawList& dl, const NkUIWindowState& ws,
                                    bool focused) noexcept;
            static void DrawTitleBar(NkUIContext& ctx, NkUIDrawList& dl,
                                    NkUIFont& font, NkUIWindowState& ws) noexcept;
            static void HandleResize(NkUIContext& ctx, NkUIDrawList& dl,
                                    NkUIWindowState& ws) noexcept;
            static void DrawResizeBorders(NkUIDrawList& dl, const NkUIContext& ctx,
                                        const NkUIWindowState& ws) noexcept;
        };

        // Extension de NkUIContext pour le wm pointer
        // Ajouté ici comme champ extern inline (C++17)
        // En pratique l'utilisateur passe wm explicitement à Begin/End
    }
} // namespace nkentseu
