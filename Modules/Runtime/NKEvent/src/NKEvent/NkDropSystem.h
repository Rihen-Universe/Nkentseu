#ifndef NKENTSEU_INPUT_NKDROPSYSTEM_H
#define NKENTSEU_INPUT_NKDROPSYSTEM_H

#pragma once

// =============================================================================
// Fichier : NkDropSystem.h
// =============================================================================
// Description :
//   Interface publique du système Drag & Drop cross-platform pour le moteur 
//   NkEntseu. Fournit les points d'entrée pour activer/désactiver le support 
//   de drop sur les fenêtres natives de chaque plateforme.
//
// Architecture :
//   - Abstraction backend via IDropTargetBackend pour isolation plateforme.
//   - Singleton DropSystem pour gestion centralisée des cibles de drop.
//   - Implémentations spécifiques par plateforme (Win32, Cocoa, X11, etc.).
//
// Plateformes supportées :
//   - Windows (Win32)    : OLE IDropTarget + DragAcceptFiles fallback
//   - Windows (UWP)      : Windows.ApplicationModel.DataTransfer
//   - macOS (Cocoa)      : NSView registerForDraggedTypes
//   - iOS (UIKit)        : UIDropInteraction + delegate
//   - Linux (XCB/XLib)   : XDND protocol (Freedesktop DND)
//   - Android            : IntentFilter + ContentResolver
//   - Emscripten (WASM)  : HTML5 Drag & Drop API
//   - Xbox               : Runtime-specific wiring (SDK dépendant)
//   - Fallback Noop      : Pour plateformes sans support ou mode headless
//
// Intégration :
//   - Appel automatique via IEventImpl::Initialize si DropEnabled=true.
//   - Appel manuel possible via NkEnableDropTarget/NkDisableDropTarget.
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkDropEvent.h"

    namespace nkentseu {
        // -------------------------------------------------------------------------
        // Fonction : NkEnableDropTarget
        // -------------------------------------------------------------------------
        // Description :
        //   Active le support drag & drop sur une fenêtre native spécifiée.
        //   Enregistre la fenêtre comme cible valide pour les opérations de drop.
        // Paramètres :
        //   - nativeHandle : Handle natif de la fenêtre (HWND, NSView*, Window, etc.)
        // Comportement :
        //   - Sélectionne le backend approprié selon la plateforme de compilation.
        //   - Initialise le backend singleton si nécessaire.
        //   - Délègue l'enregistrement au backend plateforme-spécifique.
        // Notes :
        //   - Appelée automatiquement si DropEnabled=true dans NkWindowConfig.
        //   - Peut être appelée manuellement pour activation dynamique.
        //   - Thread-safe : protégé par le singleton DropSystem.
        // -------------------------------------------------------------------------
        void NkEnableDropTarget(void* nativeHandle);

        // -------------------------------------------------------------------------
        // Fonction : NkDisableDropTarget
        // -------------------------------------------------------------------------
        // Description :
        //   Désactive le support drag & drop sur une fenêtre native spécifiée.
        //   Retire la fenêtre de la liste des cibles de drop actives.
        // Paramètres :
        //   - nativeHandle : Handle natif de la fenêtre à désactiver.
        // Comportement :
        //   - Délègue la désinscription au backend plateforme-spécifique.
        //   - Ne détruit pas le backend singleton (réutilisable pour d'autres fenêtres).
        // Notes :
        //   - Utile pour désactiver temporairement le drop sur une fenêtre.
        //   - Appeler avant la destruction de la fenêtre pour nettoyage propre.
        // -------------------------------------------------------------------------
        void NkDisableDropTarget(void* nativeHandle);

    } // namespace nkentseu

#endif // NKENTSEU_INPUT_NKDROPSYSTEM_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Activation manuelle du drag & drop sur une fenêtre
// -----------------------------------------------------------------------------
void SetupWindowWithDropSupport(void* nativeWindowHandle) {
    // Création et configuration de la fenêtre
    // auto window = Window::Create(nativeWindowHandle);

    // Activation explicite du support drag & drop
    nkentseu::NkEnableDropTarget(nativeWindowHandle);

    NK_LOG_INFO("Drag & drop activé pour la fenêtre");

    // Optionnel : enregistrement des listeners d'événements de drop
    // auto& eventMgr = Engine::GetEventManager();
    // eventMgr.Subscribe<NkDropFileEvent>(OnDropFileEvent);
    // eventMgr.Subscribe<NkDropTextEvent>(OnDropTextEvent);
    // eventMgr.Subscribe<NkDropImageEvent>(OnDropImageEvent);
}

// -----------------------------------------------------------------------------
// Exemple 2 : Désactivation conditionnelle du drag & drop
// -----------------------------------------------------------------------------
void ToggleDropSupport(bool enable, void* nativeWindowHandle) {
    if (enable) {
        nkentseu::NkEnableDropTarget(nativeWindowHandle);
        NK_LOG_DEBUG("Support drag & drop activé");
    } else {
        nkentseu::NkDisableDropTarget(nativeWindowHandle);
        NK_LOG_DEBUG("Support drag & drop désactivé");
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Activation via configuration de fenêtre
// -----------------------------------------------------------------------------
void CreateWindowWithConfig(const NkWindowConfig& config) {
    // Création de la fenêtre avec configuration
    // auto window = Window::Create(config);

    // Activation automatique si DropEnabled=true dans la config
    if (config.DropEnabled && window->GetNativeHandle()) {
        nkentseu::NkEnableDropTarget(window->GetNativeHandle());
        NK_LOG_INFO("Drag & drop activé via configuration");
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Nettoyage à la destruction d'une fenêtre
// -----------------------------------------------------------------------------
void OnDestroyWindow(void* nativeWindowHandle) {
    // Désactivation du support drag & drop avant destruction
    if (nativeWindowHandle) {
        nkentseu::NkDisableDropTarget(nativeWindowHandle);
        NK_LOG_DEBUG("Support drag & drop désactivé pour destruction");
    }

    // Destruction de la fenêtre
    // Window::Destroy(nativeWindowHandle);
}

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion multi-fenêtres avec drop support
// -----------------------------------------------------------------------------
class MultiWindowManager {
public:
    void AddWindowWithDrop(void* nativeHandle, const NkString& windowId) {
        if (!nativeHandle) {
            NK_LOG_ERROR("Handle nul pour la fenêtre {}", windowId);
            return;
        }

        // Activation du drop pour cette fenêtre
        nkentseu::NkEnableDropTarget(nativeHandle);

        // Enregistrement dans le gestionnaire
        mWindows[windowId] = nativeHandle;

        NK_LOG_INFO("Fenêtre {} ajoutée avec support drag & drop", windowId);
    }

    void RemoveWindow(const NkString& windowId) {
        auto it = mWindows.find(windowId);
        if (it != mWindows.end()) {
            // Désactivation avant suppression
            nkentseu::NkDisableDropTarget(it->second);
            mWindows.erase(it);
            NK_LOG_DEBUG("Fenêtre {} supprimée", windowId);
        }
    }

    void EnableDropForAll(bool enable) {
        for (const auto& [id, handle] : mWindows) {
            if (enable) {
                nkentseu::NkEnableDropTarget(handle);
            } else {
                nkentseu::NkDisableDropTarget(handle);
            }
        }
        NK_LOG_INFO("Support drag & drop {} pour toutes les fenêtres", 
            enable ? "activé" : "désactivé");
    }

private:
    std::unordered_map<NkString, void*> mWindows;
};

// -----------------------------------------------------------------------------
// Exemple 6 : Implémentation d'un backend personnalisé (extension)
// -----------------------------------------------------------------------------
class NkCustomDropTargetBackend : public nkentseu::IDropTargetBackend {
public:
    void Enable(void* nativeHandle) override {
        if (!nativeHandle) {
            return;
        }

        // Logique spécifique à votre plateforme/backend
        // Exemple : enregistrement dans un système de notification personnalisé
        NK_LOG_DEBUG("Backend custom: activation pour handle 0x{:X}", 
            reinterpret_cast<uintptr_t>(nativeHandle));

        mActiveTargets.insert(nativeHandle);
    }

    void Disable(void* nativeHandle) override {
        if (!nativeHandle) {
            return;
        }

        NK_LOG_DEBUG("Backend custom: désactivation pour handle 0x{:X}",
            reinterpret_cast<uintptr_t>(nativeHandle));

        mActiveTargets.erase(nativeHandle);
    }

    bool IsTargetActive(void* nativeHandle) const {
        return mActiveTargets.count(nativeHandle) > 0;
    }

private:
    std::unordered_set<void*> mActiveTargets;
};

// -----------------------------------------------------------------------------
// Exemple 7 : Test de l'API publique du système de drop
// -----------------------------------------------------------------------------
void TestDropSystemAPI() {
    // Test avec handle nul (doit être sans effet)
    nkentseu::NkEnableDropTarget(nullptr);
    nkentseu::NkDisableDropTarget(nullptr);
    NK_LOG_DEBUG("Tests avec handle nul : OK (no-op attendu)");

    // Test avec handle factice (simulation)
    void* fakeHandle = reinterpret_cast<void*>(0x12345678);
    nkentseu::NkEnableDropTarget(fakeHandle);
    NK_LOG_DEBUG("Activation simulée pour handle factice");

    nkentseu::NkDisableDropTarget(fakeHandle);
    NK_LOG_DEBUG("Désactivation simulée pour handle factice");

    NK_LOG_INFO("Tests de l'API DropSystem passés");
}
*/