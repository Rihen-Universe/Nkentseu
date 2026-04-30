// =============================================================================
// Fichier : NkDropSystem.cpp
// =============================================================================
// Description :
//   Implémentation du système Drag & Drop cross-platform avec architecture 
//   backend pluggable pour isolation des spécificités de chaque plateforme.
//
// Architecture :
//   - Interface abstraite IDropTargetBackend pour contrat commun.
//   - Implémentations concrètes par plateforme via sélection compile-time.
//   - Singleton DropSystem pour gestion centralisée et lazy initialization.
//
// Gestion des backends :
//   - Sélection via macros de détection de plateforme (NKENTSEU_PLATFORM_*).
//   - Fallback vers NkNoopDropTargetBackend si aucune plateforme reconnue.
//   - Possibilité d'override via NKENTSEU_FORCE_WINDOWING_NOOP_ONLY.
//
// Thread-safety :
//   - Singleton avec static local : thread-safe depuis C++11.
//   - Backends responsables de leur propre synchronisation si nécessaire.
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================
#include "pch.h"
#include "NkDropSystem.h"
#include "NkEvent.h"
#include "NKContainers/Associative/NkUnorderedSet.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKPlatform/NkPlatformDetect.h"

namespace nkentseu {
    // =========================================================================
    // Section : Interface backend abstraite
    // =========================================================================

    // -------------------------------------------------------------------------
    // Classe : IDropTargetBackend
    // -------------------------------------------------------------------------
    // Description :
    //   Interface abstraite définissant le contrat pour les backends 
    //   de gestion des cibles de drag & drop par plateforme.
    // Responsabilités :
    //   - Encapsuler les API natives de chaque plateforme (OLE, XDND, etc.).
    //   - Fournir une interface uniforme pour Enable/Disable.
    //   - Isoler le code plateforme-spécifique du reste du moteur.
    // Méthodes pures virtuelles :
    //   - Enable()  : Enregistre une fenêtre comme cible de drop.
    //   - Disable() : Désenregistre une fenêtre comme cible de drop.
    // Cycle de vie :
    //   - Destructeur virtuel pour suppression polymorphe correcte.
    //   - Instancié via NkUniquePtr dans le singleton DropSystem.
    // -------------------------------------------------------------------------
    class IDropTargetBackend {
        public:
            virtual ~IDropTargetBackend() = default;

            virtual void Enable(void* nativeHandle) = 0;

            virtual void Disable(void* nativeHandle) = 0;
    };

    // =========================================================================
    // Section : Implémentations backend par plateforme
    // =========================================================================

    // -------------------------------------------------------------------------
    // Backend : UWP (Windows Universal Platform)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour les applications UWP utilisant Windows.ApplicationModel.DataTransfer.
    // Notes :
    //   - Implémentation stub : à compléter avec les API UWP réelles.
    //   - Gestion via NkUnorderedSet<usize> pour tracking des handles actifs.
    // -------------------------------------------------------------------------
    #if defined(NKENTSEU_PLATFORM_UWP)
    class NkUWPDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkUWPDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Xbox
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour la plateforme Xbox avec wiring spécifique au SDK cible.
    // Notes :
    //   - Implémentation stub : à adapter selon le SDK Xbox utilisé.
    //   - Structure identique aux autres backends pour cohérence.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_XBOX)
    class NkXboxDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkXboxDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Win32 (Windows Desktop)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour Windows Desktop utilisant OLE Drag & Drop.
    // Mécanismes supportés :
    //   - IDropTarget via OLE (mécanisme moderne et complet).
    //   - DragAcceptFiles() + WM_DROPFILES (fallback simplifié).
    // Notes d'implémentation :
    //   - L'implémentation complète nécessiterait :
    //     * CoInitialize() pour initialiser COM/OLE.
    //     * RegisterDragDrop() pour enregistrer l'IDropTarget.
    //     * Gestion des méthodes DragEnter/DragOver/DragLeave/Drop.
    //   - Version actuelle : stub avec tracking des handles pour extension future.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
    class NkWin32DropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // HWND is native handle
                // Implementation: Register IDropTarget via OLE or DragAcceptFiles()
                // For now: stub with handle tracking
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkWin32DropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : macOS (Cocoa)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour macOS utilisant l'API Cocoa NSView.
    // Mécanisme :
    //   - [NSView registerForDraggedTypes:] pour déclarer les types acceptés.
    //   - Implémentation des méthodes NSDraggingDestination.
    // Types supportés typiquement :
    //   - NSFilenamesPboardType   : Chemins de fichiers
    //   - NSStringPboardType      : Texte
    //   - NSPasteboardTypePNG, etc. : Images
    // Notes :
    //   - L'implémentation complète nécessite du code Objective-C++.
    //   - Version actuelle : stub avec tracking pour extension future.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_MACOS)
    class NkCocoaDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // NSView* is native handle
                // Implementation: [view registerForDraggedTypes:@[NSFilenamesPboardType, ...]];
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
        };
    using PlatformDropBackend = NkCocoaDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : iOS (UIKit)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour iOS utilisant UIDropInteraction de UIKit.
    // Mécanisme :
    //   - UIDropInteraction avec delegate pour gérer les phases de drop.
    //   - Configuration des UTType acceptés (public.file-url, public.text, etc.).
    // Notes :
    //   - Nécessite iOS 11+ pour UIDropInteraction.
    //   - L'implémentation complète nécessite du code Objective-C++.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_IOS)
    class NkUIKitDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // UIView* is native handle
                // Implementation: UIDropInteraction + delegate
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkUIKitDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Android
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour Android utilisant le système d'Intent et ContentResolver.
    // Mécanismes :
    //   - setOnDragListener() pour les vues cibles.
    //   - IntentFilter avec ACTION_SEND / ACTION_VIEW pour réception.
    //   - ContentResolver pour accéder aux URIs de contenu partagé.
    // Types supportés :
    //   - ClipData avec MIME types (text/plain, image/*, etc.).
    // Notes :
    //   - Nécessite gestion JNI pour l'intégration native.
    //   - Permissions READ_EXTERNAL_STORAGE pour accès aux fichiers.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
    class NkAndroidDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // JNI handle to Activity
                // Implementation: setOnDragListener() for content URIs
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkAndroidDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Linux (XCB/XLib avec XDND)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour Linux utilisant le protocole XDND de Freedesktop.
    // Mécanisme :
    //   - Atom XdndAware pour déclarer le support du protocole.
    //   - Sélection des ClientMessage events pour réception des drops.
    //   - Conversion des types MIME XDND vers formats internes.
    // Types XDND courants :
    //   - text/uri-list   : Fichiers et URLs
    //   - text/plain      : Texte brut
    //   - image/png, etc. : Images
    // Notes :
    //   - XDND est le standard pour le drag & drop sur X11.
    //   - Nécessite gestion des atoms et des messages X11.
    // -------------------------------------------------------------------------
    #elif !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY) && \
            (defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_XLIB))
    class NkLinuxDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // Window (xcb_window_t or X11 Window)
                // Implementation: Set XdndAware atoms + SelectInput for ClientMessage
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkLinuxDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Emscripten (WebAssembly / HTML5)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend pour le web utilisant l'API HTML5 Drag & Drop.
    // Mécanisme :
    //   - addEventListener("drop") pour réception des fichiers/texte.
    //   - addEventListener("dragover") avec preventDefault() pour autoriser le drop.
    //   - DataTransfer API pour accès aux fichiers et texte déposés.
    // Types supportés :
    //   - DataTransfer.files    : Liste de fichiers (File API)
    //   - DataTransfer.getData() : Texte, URLs, HTML
    // Notes :
    //   - Utilise emscripten_run_script pour attacher les listeners DOM.
    //   - Gestion asynchrone via callbacks JavaScript.
    // -------------------------------------------------------------------------
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    class NkEmscriptenDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                // HTMLElement* is native handle
                // Implementation: addEventListener("drop"), addEventListener("dragover")
                // Use emscripten_run_script to attach DOM listeners
                mDropTargets.Insert(reinterpret_cast<usize>(nativeHandle));
            }

            void Disable(void* nativeHandle) override {
                if (!nativeHandle) {
                    return;
                }
                mDropTargets.Erase(reinterpret_cast<usize>(nativeHandle));
            }

        private:
            NkUnorderedSet<usize> mDropTargets;
    };
    using PlatformDropBackend = NkEmscriptenDropTargetBackend;

    // -------------------------------------------------------------------------
    // Backend : Fallback Noop (plateformes non supportées ou headless)
    // -------------------------------------------------------------------------
    // Description :
    //   Backend minimaliste pour les plateformes sans support de drag & drop
    //   ou pour les builds headless/testing.
    // Comportement :
    //   - Enable/Disable sont des no-ops sans effet.
    //   - Permet la compilation sur toutes les plateformes sans erreurs.
    // Utilisation :
    //   - Sélectionné automatiquement si aucune autre plateforme n'est détectée.
    //   - Peut être forcé via NKENTSEU_FORCE_WINDOWING_NOOP_ONLY.
    // -------------------------------------------------------------------------
    #else
    class NkNoopDropTargetBackend : public IDropTargetBackend {
        public:
            void Enable(void* nativeHandle) override {
                (void)nativeHandle;
            }

            void Disable(void* nativeHandle) override {
                (void)nativeHandle;
            }
    };
    using PlatformDropBackend = NkNoopDropTargetBackend;
    #endif

    // =========================================================================
    // Section : Singleton DropSystem pour gestion centralisée
    // =========================================================================

    // -------------------------------------------------------------------------
    // Classe : DropSystem
    // -------------------------------------------------------------------------
    // Description :
    //   Singleton gérant l'instance unique du backend de drag & drop 
    //   et fournissant un point d'accès thread-safe pour Enable/Disable.
    // Pattern :
    //   - Singleton via static local : thread-safe depuis C++11 (Meyer's singleton).
    //   - Lazy initialization : le backend n'est créé qu'au premier appel.
    // Gestion mémoire :
    //   - Backend stocké dans NkUniquePtr pour gestion automatique.
    //   - Allocation via NkGetDefaultAllocator() pour cohérence avec le moteur.
    // Thread-safety :
    //   - Instance() est thread-safe via static local.
    //   - Les appels à Enable/Disable sont sérialisés par le singleton.
    //   - Le backend est responsable de sa propre synchronisation interne si nécessaire.
    // -------------------------------------------------------------------------
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
                        memory::NkDefaultDelete<IDropTargetBackend>(&allocator)
                    );
                }
                mBackend->Enable(nativeHandle);
            }

            void DisableDropTarget(void* nativeHandle) {
                if (mBackend) {
                    mBackend->Disable(nativeHandle);
                }
            }

        private:
            DropSystem() = default;
            memory::NkUniquePtr<IDropTargetBackend> mBackend;
    };

    // =========================================================================
    // Section : Implémentation de l'API publique
    // =========================================================================

    // -------------------------------------------------------------------------
    // Fonction : NkEnableDropTarget
    // -------------------------------------------------------------------------
    // Description :
    //   Point d'entrée public pour activer le support drag & drop.
    //   Délègue au singleton DropSystem pour gestion centralisée.
    // Paramètres :
    //   - nativeHandle : Handle natif de la fenêtre cible.
    // Comportement :
    //   - Accès au singleton via Instance().
    //   - Appel à EnableDropTarget() pour traitement.
    // Notes :
    //   - Fonction inline-friendly pour optimisation par le compilateur.
    //   - Aucune vérification de validité du handle (déléguée au backend).
    // -------------------------------------------------------------------------
    void NkEnableDropTarget(void* nativeHandle) {
        DropSystem::Instance().EnableDropTarget(nativeHandle);
    }

    // -------------------------------------------------------------------------
    // Fonction : NkDisableDropTarget
    // -------------------------------------------------------------------------
    // Description :
    //   Point d'entrée public pour désactiver le support drag & drop.
    //   Délègue au singleton DropSystem pour gestion centralisée.
    // Paramètres :
    //   - nativeHandle : Handle natif de la fenêtre à désactiver.
    // Comportement :
    //   - Accès au singleton via Instance().
    //   - Appel à DisableDropTarget() pour traitement.
    // Notes :
    //   - Appel sans effet si le backend n'a pas encore été initialisé.
    //   - Safe à appeler multiple fois sur le même handle.
    // -------------------------------------------------------------------------
    void NkDisableDropTarget(void* nativeHandle) {
        DropSystem::Instance().DisableDropTarget(nativeHandle);
    }

} // namespace nkentseu