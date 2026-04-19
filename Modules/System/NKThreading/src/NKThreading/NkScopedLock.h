#pragma once
// =============================================================================
// NKThreading/NkScopedLock.h
// =============================================================================
// RAII guard pour NkMutex, NkSpinLock et tout mutex NK (duck typing sur
// Lock()/Unlock()). Remplace std::lock_guard dans tous les modules NK.
//
// Noms exposés :
//   NkScopedLock<TMutex>   — guard générique (Lock au ctor, Unlock au dtor)
//   NkLockGuard            — alias court sur NkScopedLock<NkMutex>
//
// Usage :
//   NkMutex m;
//   { NkLockGuard lock(m); /* section critique */ }
//
//   NkSpinLock sl;
//   { NkScopedLock<NkSpinLock> lock(sl); }
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {
    namespace threading {

        // =====================================================================
        // NkScopedLock<TMutex>
        // =====================================================================
        template<typename TMutex>
        class NkScopedLock {
        public:
            explicit NkScopedLock(TMutex& m) noexcept : mMutex(m) {
                mMutex.Lock();
            }

            ~NkScopedLock() noexcept {
                mMutex.Unlock();
            }

            // Non-copiable, non-movable
            NkScopedLock(const NkScopedLock&)            = delete;
            NkScopedLock& operator=(const NkScopedLock&) = delete;

        private:
            TMutex& mMutex;
        };

        // Alias court — le plus utilisé dans le code existant
        using NkLockGuard = NkScopedLock<NkMutex>;

    } // namespace threading

    // Alias dans nkentseu:: directement pour compatibilité avec le code
    // de NkEventBus et d'autres modules qui n'ont pas le sous-namespace
    using NkLockGuard  = threading::NkLockGuard;

    template<typename T>
    using NkScopedLock = threading::NkScopedLock<T>;

} // namespace nkentseu
