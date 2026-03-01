#pragma once

// =============================================================================
// NkXCBWindowImpl.h  —  Fenêtre XCB
//
// Pattern identique à Win32 / XLib :
//   - NkXCBWindowRegistry : map xcb_window_t -> NkXCBWindowImpl* (mutex-protégé)
//   - gCurrentXCBWindow   : thread_local pour la fenêtre en cours de création
//   - EncodeWindow(w)     : encode xcb_window_t en void* (clé universelle)
//   - Create() → NkGetEventImpl()->Initialize(this, EncodeWindow(id))
//   - Close()  → NkGetEventImpl()->Shutdown(EncodeWindow(id))
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include <xcb/xcb.h>
#include <unordered_map>
#include <mutex>

namespace nkentseu {

    // =========================================================================
    // NkXCBWindowRegistry — registre global xcb_window_t -> NkXCBWindowImpl*
    // =========================================================================

    class NkXCBWindowImpl;

    struct NkXCBWindowRegistry {
        std::unordered_map<xcb_window_t, NkXCBWindowImpl *> map;
        std::mutex mutex;

        static NkXCBWindowRegistry &Instance() {
            static NkXCBWindowRegistry s;
            return s;
        }

        void Insert(xcb_window_t w, NkXCBWindowImpl *impl) {
            std::lock_guard<std::mutex> lk(mutex);
            map[w] = impl;
        }

        void Remove(xcb_window_t w) {
            std::lock_guard<std::mutex> lk(mutex);
            map.erase(w);
        }

        NkXCBWindowImpl *Find(xcb_window_t w) {
            std::lock_guard<std::mutex> lk(mutex);
            auto it = map.find(w);
            return (it != map.end()) ? it->second : nullptr;
        }
    };

    inline NkXCBWindowRegistry &gXCBWindowRegistry() {
        return NkXCBWindowRegistry::Instance();
    }

    inline thread_local NkXCBWindowImpl *gCurrentXCBWindow = nullptr;

    // =========================================================================
    // NkXCBData
    // =========================================================================

    struct NkXCBData {
        xcb_connection_t *connection  = nullptr;
        xcb_screen_t     *screen      = nullptr;
        xcb_window_t      window      = XCB_WINDOW_NONE;
        xcb_gcontext_t    gc          = 0;
        xcb_atom_t        wmDelete    = XCB_ATOM_NONE;
        xcb_atom_t        wmProtocols = XCB_ATOM_NONE;
        xcb_cursor_t      blankCursor = 0;
        bool              isOpen      = false;
        NkU32             width       = 0;
        NkU32             height      = 0;
    };

    // =========================================================================
    // NkXCBWindowImpl
    // =========================================================================

    class NkXCBWindowImpl : public IWindowImpl {
    public:
        NkXCBWindowImpl()  = default;
        ~NkXCBWindowImpl() override;

        bool Create(const NkWindowConfig &config) override;
        void Close() override;
        bool IsOpen() const override;

        std::string GetTitle()          const override;
        void        SetTitle(const std::string &t) override;
        NkVec2u     GetSize()           const override;
        NkVec2u     GetPosition()       const override;
        float       GetDpiScale()       const override { return 1.f; }
        NkVec2u     GetDisplaySize()    const override;
        NkVec2u     GetDisplayPosition() const override { return {}; }
        NkError     GetLastError()      const override;
        NkU32       GetBackgroundColor() const override;
        void        SetBackgroundColor(NkU32 c) override;

        void SetSize(NkU32 w, NkU32 h)      override;
        void SetPosition(NkI32 x, NkI32 y)  override;
        void SetVisible(bool v)              override;
        void Minimize()                      override;
        void Maximize()                      override;
        void Restore()                       override;
        void SetFullscreen(bool fs)          override;
        void SetMousePosition(NkU32 x, NkU32 y) override;
        void ShowMouse(bool show)            override;
        void CaptureMouse(bool cap)          override;
        void SetProgress(float)              override {}

        NkSurfaceDesc GetSurfaceDesc() const override;

        // --- Accesseurs XCB bruts (utilisés par NkXCBEventImpl) ---
        xcb_window_t     GetXcbWindow()         const { return mData.window; }
        xcb_connection_t *GetConnection()        const { return mData.connection; }
        xcb_atom_t        GetWmDeleteAtom()      const { return mData.wmDelete; }
        xcb_atom_t        GetWmProtocolsAtom()   const { return mData.wmProtocols; }

        /// @brief Encode un xcb_window_t en void* (clé universelle pour IEventImpl)
        static void *EncodeWindow(xcb_window_t w) noexcept {
            return reinterpret_cast<void *>(static_cast<uintptr_t>(w));
        }

        void BlitSoftwareFramebuffer(const NkU8 *rgba8, NkU32 w, NkU32 h) override;

    private:
        NkXCBData   mData;
        NkWindowConfig mConfig;
        NkU32       mBgColor  = 0xFF000000;
        NkError     mLastError;
    };

} // namespace nkentseu
