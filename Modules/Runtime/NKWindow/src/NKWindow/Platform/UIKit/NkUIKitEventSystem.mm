// =============================================================================
// NkUIKitEventSystem.mm
// Implémentation UIKit (iOS) des méthodes platform-spécifiques de NkEventSystem.
//
// Sur iOS, les événements tactiles et de cycle de vie sont délivrés via
// UIKit à travers UIViewController / AppDelegate. PumpOS() traite la
// RunLoop CFRunLoop pour pomper les événements en attente.
//
// Les événements touch sont produits par NkUIKitWindow via une UIView dédiée
// déléguées et injectées dans NkEventSystem via Enqueue_Public().
// =============================================================================

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Platform/UIKit/NkUIKitEventSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkTouchEvent.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include <mutex>

namespace nkentseu {

    // =============================================================================
    // NkEventSystem — méthodes platform-spécifiques UIKit
    // =============================================================================

    bool NkEventSystem::Init() {
        if (mReady) return true;
        mTotalEventCount = 0;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;
        mData.mInitialized = true;
        mReady   = true;
        return true;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping) return;
        mPumping = true;

        // Sur iOS, pumper la CFRunLoop en mode non-bloquant pour traiter
        // les callbacks UIKit en attente (timers, sources réseau, etc.)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE);

        // Vérifier les demandes de fermeture venant de l'AppDelegate
        NkU32 count = NkSystem::Instance().GetWindowCount();
        for (NkU32 i = 0; i < count; ++i) {
            NkWindow* w = NkSystem::Instance().GetWindowAt(i);
            if (!w) continue;
            // Pas de wantsClose sur iOS (le système gère le cycle de vie)
        }

        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "UIKit";
    }

    // =============================================================================
    // Enqueue_Public — exposé pour les UIView delegates
    // =============================================================================

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS
