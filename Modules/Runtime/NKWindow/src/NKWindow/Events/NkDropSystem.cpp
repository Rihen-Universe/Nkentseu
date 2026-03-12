// =============================================================================
// NkDropSystem.cpp
// SystÃ¨me Drag & Drop cross-platform.
// =============================================================================

#include "NkDropSystem.h"
#include "NkEvent.h"
#include "NKContainers/Associative/NkUnorderedSet.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKPlatform/NkPlatformDetect.h"

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // Interface backend de base
    // ---------------------------------------------------------------------------

    class IDropTargetBackend {
        public:
            virtual ~IDropTargetBackend() = default;
            virtual void Enable(void* nativeHandle) = 0;
            virtual void Disable(void* nativeHandle) = 0;
    };

    // ---------------------------------------------------------------------------
    // Backends par plateforme
    // ---------------------------------------------------------------------------

    #if defined(NKENTSEU_PLATFORM_UWP)
    // UWP backend Ã¢â‚¬â€ Windows.ApplicationModel.DataTransfer
    class NkUWPDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) return;
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) return;
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkUWPDropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_XBOX)
    // Xbox backend Ã¢â‚¬â€ runtime-specific wiring ÃƒÂ  ajouter selon SDK cible.
    class NkXboxDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) return;
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) return;
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkXboxDropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
    // Win32 backend â€” OLE Drag & Drop avec fallback DragAcceptFiles
    class NkWin32DropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) return;
                // HWND is native handle
                // Implementation: Register IDropTarget via OLE or DragAcceptFiles()
                // For now: stub
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) return;
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkWin32DropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_MACOS)
    // Cocoa backend â€” NSView registerForDraggedTypes
    class NkCocoaDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override {
            if (!nativeHandle) return;
            // NSView* is native handle
            // Implementation: [view registerForDraggedTypes:@[NSFilenamesPboardType, ...]];
            mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
        }

        void Disable(void* nativeHandle) override {
            if (!nativeHandle) return;
            mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
        }

    private:
        NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkCocoaDropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_IOS)
    // UIKit backend â€” UIDropInteraction
    class NkUIKitDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override {
            if (!nativeHandle) return;
            // UIView* is native handle
            // Implementation: UIDropInteraction + delegate
            mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
        }

        void Disable(void* nativeHandle) override {
            if (!nativeHandle) return;
            mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
        }

    private:
        NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkUIKitDropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
    // Android backend â€” IntentFilter + ContentResolver
    class NkAndroidDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override {
            if (!nativeHandle) return;
            // JNI handle to Activity
            // Implementation: setOnDragListener() for content URIs
            mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
        }

        void Disable(void* nativeHandle) override {
            if (!nativeHandle) return;
            mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
        }

    private:
        NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkAndroidDropTargetBackend;

    #elif !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY) && \
          (defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_XLIB))
    // XCB/XLib backend â€” XDND protocol (Freedesktop DND)
    class NkLinuxDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override {
            if (!nativeHandle) return;
            // Window (xcb_window_t or X11 Window)
            // Implementation: Set XdndAware atoms + SelectInput for ClientMessage
            mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
        }

        void Disable(void* nativeHandle) override {
            if (!nativeHandle) return;
            mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
        }

    private:
        NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkLinuxDropTargetBackend;

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    // WASM backend â€” HTML5 Drag & Drop API (ondrop, ondragover events)
    class NkEmscriptenDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override {
            if (!nativeHandle) return;
            // HTMLElement* is native handle
            // Implementation: addEventListener("drop"), addEventListener("dragover")
            // Use emscripten_run_script to attach DOM listeners
            mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
        }

        void Disable(void* nativeHandle) override {
            if (!nativeHandle) return;
            mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
        }

    private:
        NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkEmscriptenDropTargetBackend;

    #else
    // Fallback Noop backend
    class NkNoopDropTargetBackend : public IDropTargetBackend {
    public:
        void Enable(void* nativeHandle) override { (void)nativeHandle; }
        void Disable(void* nativeHandle) override { (void)nativeHandle; }
    };
    using PlatformDropBackend = NkNoopDropTargetBackend;
    #endif

    // ---------------------------------------------------------------------------
    // Drop system singleton
    // ---------------------------------------------------------------------------

    class DropSystem {
    public:
        static DropSystem& Instance() {
            static DropSystem sInstance;
            return sInstance;
        }

        void EnableDropTarget(void* nativeHandle) {
            if (!mBackend) {
                memory::NkAllocator& allocator = memory::NkGetDefaultAllocator();
                mBackend = memory::NkUniquePtr<IDropTargetBackend>(
                    allocator.New<PlatformDropBackend>(),
                    memory::NkDefaultDelete<IDropTargetBackend>(&allocator));
            }
            mBackend->Enable(nativeHandle);
        }

        void DisableDropTarget(void* nativeHandle) {
            if (mBackend) mBackend->Disable(nativeHandle);
        }

    private:
        DropSystem() = default;
        memory::NkUniquePtr<IDropTargetBackend> mBackend;
    };

    // ---------------------------------------------------------------------------
    // Public API
    // ---------------------------------------------------------------------------

    void NkEnableDropTarget(void* nativeHandle)
    {
        DropSystem::Instance().EnableDropTarget(nativeHandle);
    }

    void NkDisableDropTarget(void* nativeHandle)
    {
        DropSystem::Instance().DisableDropTarget(nativeHandle);
    }

} // namespace nkentseu

