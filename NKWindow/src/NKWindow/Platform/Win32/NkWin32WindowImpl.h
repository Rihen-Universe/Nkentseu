#pragma once

// =============================================================================
// NkWin32WindowImpl.h
// Implémentation Win32 de IWindowImpl.
//
// V3 — Corrections :
//   - gWindowMap n'est plus thread_local : les messages Win32 sont toujours
//     dispatché sur le thread créateur, donc une map globale ordinaire suffit
//     et évite le piège où une autre unité de compilation ou un thread auxiliaire
//     verrait une map vide.
//   - gCurrentWindow reste thread_local (utile pendant WM_NCCREATE uniquement).
//   - Create() passe `this` comme lpParam de CreateWindowExW.
//   - Close() retire l'entrée de la map.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <unordered_map>
#include <mutex>

#include "NKWindow/Core/IWindowImpl.h"

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // Données internes Win32
    // ---------------------------------------------------------------------------

    struct NkWin32Data {
        HWND          hwnd        = nullptr;
        HINSTANCE     hinstance   = nullptr;
        DWORD         dwStyle     = 0;
        DWORD         dwExStyle   = 0;
        DEVMODE       dmScreen    = {};
        ITaskbarList3* taskbarList = nullptr;
        bool          isOpen      = false;
    };

    // ---------------------------------------------------------------------------
    // NkWin32WindowImpl
    // ---------------------------------------------------------------------------

    class NkWin32WindowImpl : public IWindowImpl {
    public:
        NkWin32WindowImpl()  = default;
        ~NkWin32WindowImpl() override;

        bool    Create(const NkWindowConfig& config) override;
        void    Close()   override;
        bool    IsOpen()  const override;

        std::string GetTitle()          const override;
        NkVec2u     GetSize()           const override;
        NkVec2u     GetPosition()       const override;
        float       GetDpiScale()       const override;
        NkVec2u     GetDisplaySize()    const override;
        NkVec2u     GetDisplayPosition()const override;
        NkError     GetLastError()      const override;

        void SetTitle(const std::string& title) override;
        void SetSize(NkU32 w, NkU32 h)          override;
        void SetPosition(NkI32 x, NkI32 y)      override;
        void SetVisible(bool v)                 override;
        void Minimize()                         override;
        void Maximize()                         override;
        void Restore()                          override;
        void SetFullscreen(bool fs)             override;

        void SetMousePosition(NkU32 x, NkU32 y) override;
        void ShowMouse(bool show)                override;
        void CaptureMouse(bool cap)              override;

        void SetProgress(float progress) override;

        NkSurfaceDesc GetSurfaceDesc() const override;

        // Accès internes pour NkWin32EventImpl
        HWND                    GetHwnd()      const { return mData.hwnd; }
        HINSTANCE               GetHInstance() const { return mData.hinstance; }
        const NkWindowConfig&   GetConfig()    const { return mConfig; }
        DWORD                   GetStyle()     const { return mData.dwStyle; }

    private:
        NkWin32Data    mData;
        NkWindowConfig mConfig;
        NkError        mLastError;
    };

    // ---------------------------------------------------------------------------
    // Registre global des fenêtres HWND → NkWin32WindowImpl*
    //
    // NON thread_local : Win32 dispatche toujours les messages sur le thread
    // créateur de la fenêtre, donc une map globale protégée par mutex est
    // correcte et évite qu'une autre TU ou un thread auxiliaire voie une map vide.
    // ---------------------------------------------------------------------------

    struct NkWin32WindowRegistry {
        std::unordered_map<HWND, NkWin32WindowImpl*> map;
        std::mutex                                   mutex;

        static NkWin32WindowRegistry& Instance() {
            static NkWin32WindowRegistry s;
            return s;
        }

        void Insert(HWND hwnd, NkWin32WindowImpl* w) {
            std::lock_guard<std::mutex> lk(mutex);
            map[hwnd] = w;
        }

        void Remove(HWND hwnd) {
            std::lock_guard<std::mutex> lk(mutex);
            map.erase(hwnd);
        }

        NkWin32WindowImpl* Find(HWND hwnd) {
            std::lock_guard<std::mutex> lk(mutex);
            auto it = map.find(hwnd);
            return (it != map.end()) ? it->second : nullptr;
        }
    };

    // Raccourci
    inline NkWin32WindowRegistry& gWindowRegistry() {
        return NkWin32WindowRegistry::Instance();
    }

    // Pointeur vers la fenêtre en cours de création sur ce thread
    // (utile uniquement pendant WM_NCCREATE, avant que gWindowRegistry soit peuplé)
    inline thread_local NkWin32WindowImpl* gCurrentWindow = nullptr;

} // namespace nkentseu