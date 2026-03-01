#pragma once

// =============================================================================
// NkXLibWindowImpl.h  —  Fenêtre XLib
//
// Pattern identique à Win32 :
//   - NkXLibWindowRegistry : ::Window → NkXLibWindowImpl* (global, mutex).
//   - gCurrentXLibWindow   : thread_local, courant pendant Create().
//   - NkXLibWindowImpl ne stocke PAS de pointeur vers IEventImpl.
//   - Create()  → NkGetEventImpl()->Initialize(this, EncodeWindow(id))
//   - Close()   → NkGetEventImpl()->Shutdown(EncodeWindow(id))
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <unordered_map>
#include <mutex>

namespace nkentseu {

    // Déclaration du display global (défini dans NkXLib.cpp)
    extern Display *nk_xlib_global_display;

    // =========================================================================
    // NkXLibData — données natives de la fenêtre
    // =========================================================================

    struct NkXLibData {
        Display  *display     = nullptr;
        ::Window  window      = 0;
        int       screen      = 0;
        GC        gc          = nullptr;
        Atom      wmDelete    = 0;
        Atom      wmProtocols = 0;
        Cursor    blankCursor = 0;
        bool      isOpen      = false;
        NkU32     width       = 0;
        NkU32     height      = 0;
    };

    // =========================================================================
    // NkXLibWindowImpl
    // =========================================================================

    class NkXLibWindowImpl : public IWindowImpl {
    public:
        NkXLibWindowImpl()  = default;
        ~NkXLibWindowImpl() override;

        bool Create(const NkWindowConfig &config) override;
        void Close()   override;
        bool IsOpen()  const override;

        std::string GetTitle()           const override;
        void        SetTitle(const std::string &t) override;
        NkVec2u     GetSize()            const override;
        NkVec2u     GetPosition()        const override;
        float       GetDpiScale()        const override { return 1.f; }
        NkVec2u     GetDisplaySize()     const override;
        NkVec2u     GetDisplayPosition() const override { return {}; }
        NkError     GetLastError()       const override;

        void SetSize(NkU32 w, NkU32 h)       override;
        void SetPosition(NkI32 x, NkI32 y)   override;
        void SetVisible(bool v)               override;
        void Minimize()                       override;
        void Maximize()                       override;
        void Restore()                        override;
        void SetFullscreen(bool fs)           override;
        void SetMousePosition(NkU32 x, NkU32 y) override;
        void ShowMouse(bool show)             override;
        void CaptureMouse(bool cap)           override;
        void SetProgress(float)               override {}

        void  SetBackgroundColor(NkU32 c);
        NkU32 GetBackgroundColor() const;

        NkSurfaceDesc GetSurfaceDesc() const override;

        // Accesseurs pour NkXLibEventImpl / NkXLibDropImpl
        ::Window GetXlibWindow()      const { return mData.window;      }
        Display *GetDisplay()         const { return mData.display;     }
        Atom     GetWmDeleteAtom()    const { return mData.wmDelete;    }
        Atom     GetWmProtocolsAtom() const { return mData.wmProtocols; }

        /// Encode un ::Window comme clé void* pour IEventImpl (statique, réutilisable)
        static void *EncodeWindow(::Window w) {
            return reinterpret_cast<void *>(static_cast<uintptr_t>(w));
        }

    private:
        NkXLibData mData;
        NkU32      mBgColor = 0x141414FF;
    };

    // =========================================================================
    // NkXLibWindowRegistry — ::Window → NkXLibWindowImpl* (global, thread-safe)
    //
    // Même principe que NkWin32WindowRegistry : les événements XLib portent
    // un ::Window id ; ce registre permet de retrouver l'objet C++ depuis
    // PollEvents() sans stocker de pointeur brut dans l'EventImpl.
    // =========================================================================

    struct NkXLibWindowRegistry {
        std::unordered_map<::Window, NkXLibWindowImpl *> map;
        std::mutex                                        mutex;

        static NkXLibWindowRegistry &Instance() {
            static NkXLibWindowRegistry s;
            return s;
        }

        void Insert(::Window w, NkXLibWindowImpl *impl) {
            std::lock_guard<std::mutex> lk(mutex);
            map[w] = impl;
        }

        void Remove(::Window w) {
            std::lock_guard<std::mutex> lk(mutex);
            map.erase(w);
        }

        NkXLibWindowImpl *Find(::Window w) {
            std::lock_guard<std::mutex> lk(mutex);
            auto it = map.find(w);
            return (it != map.end()) ? it->second : nullptr;
        }
    };

    /// Accès au registre global XLib
    inline NkXLibWindowRegistry &gXLibWindowRegistry() {
        return NkXLibWindowRegistry::Instance();
    }

    /// Fenêtre en cours de création sur ce thread.
    /// Utile pendant Create() avant que le registre soit peuplé — miroir de gCurrentWindow Win32.
    inline thread_local NkXLibWindowImpl *gCurrentXLibWindow = nullptr;

} // namespace nkentseu