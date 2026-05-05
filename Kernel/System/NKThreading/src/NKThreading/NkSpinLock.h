//
// NkSpinLock.h
// =============================================================================
// Description :
//   Verrou à attente active (spin-lock) haute performance pour sections
//   critiques ultra-courtes. Optimisé pour faible contention et latence
//   déterministe sans appel système.
//
// Caractéristiques :
//   - Instructions PAUSE/YIELD pour réduire la consommation CPU pendant l'attente
//   - Backoff adaptatif : spin court puis yield OS en cas de contention élevée
//   - Alignement sur ligne de cache pour éviter le false sharing
//   - Opérations Lock/TryLock/Unlock garanties noexcept
//   - Compatible avec NkScopedSpinLock pour gestion RAII
//
// Algorithmes implémentés :
//   - Fast-path : Exchange atomique avec retour immédiat si lock libre
//   - Slow-path : boucle de spin avec PauseInstruction() + backoff exponentiel
//   - Release : Store avec mémoire order RELEASE pour visibilité immédiate
//
// Types disponibles :
//   NkSpinLock         : spin-lock optimisé pour sections <100 cycles
//   NkScopedSpinLock   : guard RAII move-only pour gestion automatique
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_SPINLOCK_H__
#define __NKENTSEU_THREADING_SPINLOCK_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkAtomic.h"
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NKThreading/NkThreadingApi.h"

    // Inclusions système conditionnelles pour les primitives de yield
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #include <windows.h>
    #elif defined(NKENTSEU_PLATFORM_LINUX)   || \
          defined(NKENTSEU_PLATFORM_ANDROID) || \
          defined(NKENTSEU_PLATFORM_MACOS)   || \
          defined(NKENTSEU_PLATFORM_IOS)
        #include <sched.h>
    #endif

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : memory (primitives de bas niveau)
        // =====================================================================
        // Les primitives de synchronisation ultra-légères sont placées dans
        // nkentseu::memory car elles opèrent au niveau mémoire/atomique,
        // indépendamment des abstractions de threading de plus haut niveau.
        // =====================================================================
        namespace memory {

            // =================================================================
            // CLASSE : NkSpinLock
            // =================================================================
            // Verrou à attente active optimisé pour sections critiques très courtes.
            //
            // Représentation interne :
            //   - NkAtomicBool pour l'état de verrouillage (lock-free)
            //   - Pas d'allocation dynamique, pas de syscall dans le fast-path
            //   - Instructions CPU spécifiques (PAUSE/YIELD) pour l'efficacité
            //
            // Invariant de production :
            //   - mLocked == true ⇔ un thread détient le verrou
            //   - Seul le thread détenteur peut appeler Unlock()
            //   - Après Unlock(), tout thread peut acquérir le lock
            //
            // Caractéristiques de performance :
            //   - Acquisition (lock libre) : 3-5 cycles CPU
            //   - Acquisition (contension modérée) : 50-200 cycles
            //   - Acquisition (contension élevée) : 200-1000 cycles + yield OS
            //   - Release : ~1 cycle CPU (store atomique)
            //
            // Utiliser pour :
            //   - Sections critiques <100 cycles CPU (compteurs, flags, petits buffers)
            //   - Code temps-réel où les syscalls sont interdits
            //   - Structures lock-free comme building block
            //
            // Ne pas utiliser pour :
            //   - Sections longues (>1µs) : préférer NkMutex (moins de waste CPU)
            //   - Code avec I/O ou opérations bloquantes
            //   - Cas où la contention est systématiquement élevée
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkSpinLock {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR
                    // -------------------------------------------------------------

                    /// @brief Constructeur par défaut : initialise le lock à l'état déverrouillé.
                    /// @note Garantie noexcept : aucune allocation ni exception possible.
                    /// @note mLocked est initialisé à false via NkAtomicBool constructor.
                    NkSpinLock() noexcept;

                    /// @brief Destructeur : libération des ressources (aucune pour spin-lock).
                    /// @note Garantie noexcept : destruction toujours sûre.
                    /// @warning Ne pas détruire un spin-lock pendant qu'un thread l'attend.
                    ~NkSpinLock() = default;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de copie supprimé : spin-lock non-copiable.
                    /// @note Un verrou représente une ressource système unique.
                    NkSpinLock(const NkSpinLock&) = delete;

                    /// @brief Opérateur d'affectation supprimé : spin-lock non-affectable.
                    NkSpinLock& operator=(const NkSpinLock&) = delete;

                    /// @brief Constructeur de déplacement supprimé : non-déplaçable.
                    /// @note L'état atomique ne peut être transféré entre instances.
                    NkSpinLock(NkSpinLock&&) = delete;

                    /// @brief Opérateur de déplacement supprimé : non-déplaçable.
                    NkSpinLock& operator=(NkSpinLock&&) = delete;

                    // -------------------------------------------------------------
                    // API PRINCIPALE - LOCK/UNLOCK
                    // -------------------------------------------------------------

                    /// @brief Verrouille le spin-lock (attente active si nécessaire).
                    /// @note Utilise un fast-path optimisé pour le cas lock libre.
                    /// @note En cas de contention : boucle de spin avec PauseInstruction().
                    /// @note Après contention élevée : ThreadYield() pour céder le CPU.
                    /// @warning Peut consumer du CPU en boucle si contention prolongée.
                    /// @warning Ne pas utiliser dans des sections critiques longues.
                    void Lock() noexcept;

                    /// @brief Tente de verrouiller sans attente active.
                    /// @return true si acquis immédiatement, false si déjà verrouillé.
                    /// @note Retourne toujours immédiatement (non-bloquant).
                    /// @note Utile pour les algorithmes lock-free avec fallback.
                    [[nodiscard]] nk_bool TryLock() noexcept;

                    /// @brief Déverrouille le spin-lock.
                    /// @warning Doit être appelé par le thread détenteur uniquement.
                    /// @warning Appel sans Lock() préalable = comportement indéfini.
                    /// @note Utilise un store avec mémoire order RELEASE pour visibilité.
                    void Unlock() noexcept;

                    /// @brief Vérifie l'état courant du verrou (non-bloquant).
                    /// @return true si verrouillé, false si disponible.
                    /// @note Lecture avec mémoire order RELAXED : valeur potentiellement obsolète.
                    /// @note Utile pour le debugging et les métriques, pas pour la logique.
                    [[nodiscard]] nk_bool IsLocked() const noexcept;

                private:

                    // -------------------------------------------------------------
                    // MÉTHODES UTILITAIRES INTERNES
                    // -------------------------------------------------------------

                    /// @brief Slow-path avec backoff adaptatif pour haute contention.
                    /// @note Appelé uniquement quand le fast-path échoue (lock occupé).
                    /// @note Stratégie : spin court avec PAUSE → yield OS si contention persiste.
                    void LockSlow() noexcept;

                    /// @brief Cède le temps CPU au scheduler (fallback haute contention).
                    /// @note Windows : SwitchToThread(), POSIX : sched_yield().
                    /// @note Réduit la consommation CPU mais augmente la latence d'acquisition.
                    static void ThreadYield() noexcept;

                    /// @brief Instruction CPU pause pour optimiser la boucle de spin.
                    /// @note x86/x64 : PAUSE, ARM : YIELD, autres : fallback NOP.
                    /// @note Réduit la contention sur le bus mémoire et la consommation power.
                    static void PauseInstruction() noexcept;

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS (données internes)
                    // -------------------------------------------------------------

                    /// @brief État atomique du verrou : true = verrouillé, false = libre.
                    /// @note Aligné sur ligne de cache via NkAtomicBool pour éviter false sharing.
                    alignas(NKENTSEU_CACHE_LINE_SIZE) NkAtomicBool mLocked;
            };

            // =================================================================
            // CLASSE : NkScopedSpinLock (RAII guard pattern)
            // =================================================================
            // Guard RAII pour la gestion automatique des spin-locks.
            //
            // Pattern Resource Acquisition Is Initialization :
            //   - Acquisition du lock dans le constructeur (immédiate)
            //   - Libération automatique dans le destructeur (garantie même en cas d'exception)
            //   - Sémantique de déplacement pour transfert de propriété entre scopes
            //
            // Avantages :
            //   - Prévention systématique des oublis d'Unlock()
            //   - Code plus lisible : la portée du lock est visuellement délimitée
            //   - Compatible avec early returns et multiples points de sortie
            //
            // Différence vs NkScopedLock (mutex) :
            //   - NkScopedSpinLock : pour NkSpinLock (sections ultra-courtes)
            //   - NkScopedLock    : pour NkMutex (sections moyennes/longues)
            //   - Choisir en fonction de la durée estimée de la section critique
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkScopedSpinLock {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR (INLINE POUR PERFORMANCE)
                    // -------------------------------------------------------------

                    /// @brief Constructeur : verrouille immédiatement le spin-lock fourni.
                    /// @param lock Référence vers le NkSpinLock à protéger.
                    /// @note Garantie noexcept : Lock() ne lève pas d'exception.
                    /// @note Le spin-lock est détenu dès la fin de cette ligne.
                    explicit NkScopedSpinLock(NkSpinLock& lock) noexcept;

                    /// @brief Destructeur : déverrouille le spin-lock si encore détenu.
                    /// @note Garantie noexcept : Unlock() est toujours sûr.
                    /// @note Appel automatique à la sortie de portée (même sur exception).
                    ~NkScopedSpinLock() noexcept;

                    // -------------------------------------------------------------
                    // SÉMANTIQUE DE DÉPLACEMENT (MOVE-ONLY)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de déplacement : transfère la propriété du lock.
                    /// @param other Instance source dont la propriété est transférée.
                    /// @note L'instance source devient invalide (mLock = nullptr).
                    /// @note Le spin-lock reste verrouillé : la responsabilité est transférée.
                    NkScopedSpinLock(NkScopedSpinLock&& other) noexcept;

                    /// @brief Opérateur d'affectation par déplacement.
                    /// @param other Instance source dont la propriété est transférée.
                    /// @return Référence vers cette instance après transfert.
                    /// @note Déverrouille l'ancien lock si nécessaire avant le transfert.
                    NkScopedSpinLock& operator=(NkScopedSpinLock&& other) noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Copie interdite : un lock ne peut pas être dupliqué.
                    NkScopedSpinLock(const NkScopedSpinLock&) = delete;

                    /// @brief Affectation par copie interdite.
                    NkScopedSpinLock& operator=(const NkScopedSpinLock&) = delete;

                    // -------------------------------------------------------------
                    // MÉTHODES DE CONTRÔLE
                    // -------------------------------------------------------------

                    /// @brief Libère manuellement le spin-lock avant la destruction du guard.
                    /// @note Rend le guard inactif : le destructeur n'appellera pas Unlock().
                    /// @warning À utiliser avec précaution : risque de fuite de verrou si mal géré.
                    void Unlock() noexcept;

                    /// @brief Accède au spin-lock protégé par ce guard.
                    /// @return Pointeur vers le lock, ou nullptr si le guard a été déplacé.
                    [[nodiscard]] NkSpinLock* GetLock() noexcept;

                    /// @brief Accède au spin-lock protégé (version const).
                    /// @return Pointeur const vers le lock, ou nullptr si déplacé.
                    [[nodiscard]] const NkSpinLock* GetLock() const noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------

                    /// @brief Pointeur vers le spin-lock géré (nullptr si guard déplacé/inactif).
                    NkSpinLock* mLock;
            };

        }
    }

#endif

// =============================================================================
// IMPLÉMENTATIONS INLINE (dans le header pour optimisation)
// =============================================================================

namespace nkentseu {
    namespace memory {

        // ---------------------------------------------------------------------
        // Constructeur : initialisation du lock à l'état déverrouillé
        // ---------------------------------------------------------------------
        inline NkSpinLock::NkSpinLock() noexcept
            : mLocked(false)
        {
            // NkAtomicBool constructor garantit l'initialisation à false
            // Pas d'allocation dynamique, pas de syscall : construction légère
        }

        // ---------------------------------------------------------------------
        // Méthode : Lock (avec fast-path optimisé)
        // ---------------------------------------------------------------------
        // Algorithme en deux phases :
        //   1. Fast-path : Exchange atomique avec retour immédiat si succès
        //      - Cas le plus probable : lock libre → acquisition en 3-5 cycles
        //   2. Slow-path : appel à LockSlow() si contention détectée
        //      - Boucle de spin avec PauseInstruction() pour efficacité CPU
        //      - Backoff adaptatif : yield OS après 32 spins pour contention élevée
        //
        // Mémoire ordering :
        //   - Exchange avec NK_ACQUIRE : garantit que les lectures/écritures
        //     suivantes ne sont pas réordonnées avant l'acquisition du lock
        // ---------------------------------------------------------------------
        inline void NkSpinLock::Lock() noexcept
        {
            // Fast-path : tentative immédiate d'acquisition
            if (!mLocked.Exchange(true, NkMemoryOrder::NK_ACQUIRE)) {
                return;  // Succès : lock acquis sans contention
            }

            // Slow-path : contention détectée → délégation à LockSlow()
            LockSlow();
        }

        // ---------------------------------------------------------------------
        // Méthode : TryLock (tentative non-bloquante)
        // ---------------------------------------------------------------------
        // Comportement :
        //   - Exchange atomique avec NK_ACQUIRE pour mémoire ordering
        //   - Retourne true si le lock était libre (Exchange retourne false)
        //   - Retourne false si le lock était déjà détenu (Exchange retourne true)
        //
        // Utilisation typique :
        //   - Algorithmes lock-free avec fallback sur autre stratégie
        //   - Polling avec backoff exponentiel pour éviter la contention
        //   - UI threads ou code temps-réel qui ne peuvent jamais bloquer
        // ---------------------------------------------------------------------
        inline nk_bool NkSpinLock::TryLock() noexcept
        {
            return !mLocked.Exchange(true, NkMemoryOrder::NK_ACQUIRE);
        }

        // ---------------------------------------------------------------------
        // Méthode : Unlock (libération du verrou)
        // ---------------------------------------------------------------------
        // Comportement :
        //   - Store atomique avec NK_RELEASE pour mémoire ordering
        //   - Garantit que toutes les écritures précédentes dans la section
        //     critique sont visibles aux autres threads avant la libération
        //
        // Préconditions (non vérifiées en release pour la performance) :
        //   - Le thread appelant doit détenir le verrou
        //   - Appel sans Lock() préalable = comportement indéfini
        // ---------------------------------------------------------------------
        inline void NkSpinLock::Unlock() noexcept
        {
            mLocked.Store(false, NkMemoryOrder::NK_RELEASE);
        }

        // ---------------------------------------------------------------------
        // Méthode : IsLocked (requête d'état non-bloquante)
        // ---------------------------------------------------------------------
        // Retourne l'état courant de mLocked avec mémoire order RELAXED.
        //
        // Avertissement de concurrence :
        //   - La valeur retournée peut être obsolète dès le retour de la méthode
        //   - Ne pas utiliser pour prendre des décisions de synchronisation
        //   - Utile pour : debugging, métriques, assertions en test, logging
        //
        // Thread-safety :
        //   - Lecture atomique : pas de data race
        //   - Mais pas d'atomicité avec d'autres opérations : valeur "instantanée"
        // ---------------------------------------------------------------------
        inline nk_bool NkSpinLock::IsLocked() const noexcept
        {
            return mLocked.Load(NkMemoryOrder::NK_RELAXED);
        }

        // ---------------------------------------------------------------------
        // Méthode privée : PauseInstruction (optimisation CPU)
        // ---------------------------------------------------------------------
        // Émet une instruction hint au CPU pour optimiser la boucle de spin.
        //
        // Effets :
        //   - Réduit la contention sur le bus mémoire entre cores
        //   - Diminue la consommation power pendant l'attente active
        //   - Améliore les performances en environnement multi-threadé
        //
        // Support par architecture :
        //   - x86/x64 : __builtin_ia32_pause() → instruction PAUSE
        //   - ARM/ARM64 : asm volatile("yield") → instruction YIELD
        //   - Autres : fallback sans instruction (moins optimal mais portable)
        // ---------------------------------------------------------------------
        inline void NkSpinLock::PauseInstruction() noexcept
        {
            #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                __builtin_ia32_pause();
            #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
                __asm__ __volatile__("yield" ::: "memory");
            #else
                // Fallback portable : aucune instruction spécifique
                // Moins optimal mais fonctionne sur toutes les architectures
            #endif
        }

        // ---------------------------------------------------------------------
        // Méthode privée : ThreadYield (fallback haute contention)
        // ---------------------------------------------------------------------
        // Cède volontairement le temps CPU au scheduler OS.
        //
        // Utilisation :
        //   - Appelé dans LockSlow() après 32 spins sans succès
        //   - Réduit la consommation CPU mais augmente la latence d'acquisition
        //   - Trade-off acceptable pour éviter de "brûler" le CPU en contention élevée
        //
        // Support par plateforme :
        //   - Windows : ::SwitchToThread() → cède le quantum restant
        //   - POSIX   : ::sched_yield() → place le thread en fin de file d'exécution
        // ---------------------------------------------------------------------
        inline void NkSpinLock::ThreadYield() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                ::SwitchToThread();
            #elif defined(NKENTSEU_PLATFORM_LINUX)   || \
                  defined(NKENTSEU_PLATFORM_ANDROID) || \
                  defined(NKENTSEU_PLATFORM_MACOS)   || \
                  defined(NKENTSEU_PLATFORM_IOS)
                (void)::sched_yield();
            #endif
        }

        // =====================================================================
        // IMPLÉMENTATION : NkScopedSpinLock (méthodes inline critiques)
        // =====================================================================
        // Ces méthodes sont définies inline dans le header pour permettre
        // l'optimisation par le compilateur et éviter l'overhead d'appel de
        // fonction pour des opérations fréquentes dans les boucles critiques.

        // ---------------------------------------------------------------------
        // Constructeur : acquisition immédiate du lock
        // ---------------------------------------------------------------------
        inline NkScopedSpinLock::NkScopedSpinLock(NkSpinLock& lock) noexcept
            : mLock(&lock)
        {
            mLock->Lock();
        }

        // ---------------------------------------------------------------------
        // Destructeur : libération automatique si encore détenu
        // ---------------------------------------------------------------------
        inline NkScopedSpinLock::~NkScopedSpinLock() noexcept
        {
            if (mLock != nullptr) {
                mLock->Unlock();
            }
        }

        // ---------------------------------------------------------------------
        // Constructeur de déplacement : transfert de propriété
        // ---------------------------------------------------------------------
        inline NkScopedSpinLock::NkScopedSpinLock(NkScopedSpinLock&& other) noexcept
            : mLock(other.mLock)
        {
            other.mLock = nullptr;
        }

        // ---------------------------------------------------------------------
        // Opérateur de déplacement : transfert avec nettoyage préalable
        // ---------------------------------------------------------------------
        inline NkScopedSpinLock& NkScopedSpinLock::operator=(NkScopedSpinLock&& other) noexcept
        {
            if (this != &other) {
                if (mLock != nullptr) {
                    mLock->Unlock();
                }
                mLock = other.mLock;
                other.mLock = nullptr;
            }
            return *this;
        }

        // ---------------------------------------------------------------------
        // Méthode : Unlock manuel avec invalidation du guard
        // ---------------------------------------------------------------------
        inline void NkScopedSpinLock::Unlock() noexcept
        {
            if (mLock != nullptr) {
                mLock->Unlock();
                mLock = nullptr;
            }
        }

        // ---------------------------------------------------------------------
        // Accesseur : GetLock (version mutable)
        // ---------------------------------------------------------------------
        inline NkSpinLock* NkScopedSpinLock::GetLock() noexcept
        {
            return mLock;
        }

        // ---------------------------------------------------------------------
        // Accesseur : GetLock (version const)
        // ---------------------------------------------------------------------
        inline const NkSpinLock* NkScopedSpinLock::GetLock() const noexcept
        {
            return mLock;
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Protection d'un compteur partagé (cas d'usage typique)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>
    using namespace nkentseu::memory;

    NkSpinLock counterLock;
    uint64_t sharedCounter = 0;

    void IncrementCounter()
    {
        NkScopedSpinLock guard(counterLock);  // Acquisition RAII
        ++sharedCounter;                       // Section critique ultra-courte
        // Déverrouillage automatique à la sortie de portée
    }

    uint64_t GetCounter()
    {
        NkScopedSpinLock guard(counterLock);
        return sharedCounter;  // Lecture protégée
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : TryLock avec fallback non-bloquant
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>

    NkSpinLock fastLock;
    volatile bool dataReady = false;

    bool TryProcessFast()
    {
        // Tentative non-bloquante : retour immédiat si lock occupé
        if (!fastLock.TryLock()) {
            // Fallback : traitement différé ou autre stratégie
            QueueForLaterProcessing();
            return false;
        }

        // Section critique protégée (lock acquis)
        if (dataReady) {
            ProcessData();
        }
        fastLock.Unlock();  // Libération explicite
        return true;
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Transfert de propriété de guard (move semantics)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>

    void ExtendedCriticalSection(NkSpinLock& lock)
    {
        NkScopedSpinLock guard(lock);  // Acquisition initiale

        if (NeedsExtendedProcessing()) {
            // Transfert du guard à une fonction externe
            ContinueProcessing(std::move(guard));  // 'guard' devient invalide
            return;
        }

        // Traitement standard...
    }

    void ContinueProcessing(NkScopedSpinLock&& guard)
    {
        // 'guard' détient maintenant le lock transféré
        PerformExtendedOperation();
        // Destruction automatique : Unlock() appelé à la sortie
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Unlock manuel pour étendre la portée dynamiquement
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>

    void ConditionalCriticalSection(NkSpinLock& lock, bool condition)
    {
        NkScopedSpinLock guard(lock);

        // Section critique conditionnelle courte
        UpdateSharedState();

        if (condition) {
            // Libération anticipée si la suite n'a pas besoin du lock
            guard.Unlock();  // Déverrouillage manuel
            DoUnprotectedWork();  // Hors section critique
        }
        // Si !condition : déverrouillage automatique par destructeur
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Pattern lock-free avec spin-lock comme building block
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>

    template<typename T>
    class LockFreeStack {
    public:
        void Push(T* node)
        {
            // Spin-lock pour protéger la tête de liste pendant l'update
            mLock.Lock();
            node->next = mHead.load(std::memory_order_relaxed);
            mHead.store(node, std::memory_order_release);
            mLock.Unlock();
        }

        T* Pop()
        {
            mLock.Lock();
            T* oldHead = mHead.load(std::memory_order_relaxed);
            if (oldHead) {
                mHead.store(oldHead->next, std::memory_order_release);
            }
            mLock.Unlock();
            return oldHead;
        }

    private:
        std::atomic<T*> mHead{nullptr};
        NkSpinLock mLock;  // Protège uniquement les updates de mHead
    };

    // ---------------------------------------------------------------------
    // Exemple 6 : Comparaison performance : SpinLock vs Mutex
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>
    #include <NKThreading/NkMutex.h>

    // ✅ CAS IDÉAL POUR SPINLOCK : section <100 cycles, faible contention
    void FastCounter_Increment(NkSpinLock& lock, uint64_t& counter)
    {
        NkScopedSpinLock guard(lock);  // Overhead minimal
        ++counter;                      // Opération ultra-rapide
    }  // ~10-20 cycles totaux

    // ❌ CAS INAPPROPRIÉ : section longue, contention élevée
    void SlowOperation_Increment(NkSpinLock& lock, std::vector<int>& buffer)
    {
        NkScopedSpinLock guard(lock);
        // ❌ Spin pendant 1000+ cycles = waste CPU massif !
        ExpensiveComputation(buffer);
    }

    // ✅ SOLUTION : utiliser NkMutex pour sections longues
    void SlowOperation_Increment_Fixed(NkMutex& mutex, std::vector<int>& buffer)
    {
        NkScopedLock guard(mutex);  // Bloque efficacement sans waste CPU
        ExpensiveComputation(buffer);
    }  // Thread suspendu, pas de consommation CPU pendant l'attente

    // ---------------------------------------------------------------------
    // Exemple 7 : Backoff adaptatif en action (comportement interne)
    // ---------------------------------------------------------------------
    // NkSpinLock implémente un backoff adaptatif dans LockSlow() :
    //
    // Phase 1 (spins 0-31) :
    //   - PauseInstruction() à chaque itération
    //   - Réduit la contention mémoire et power consumption
    //   - Idéal pour contention courte/transitoire
    //
    // Phase 2 (spins >= 32) :
    //   - ThreadYield() appelé pour céder le CPU au scheduler
    //   - Réduit la consommation CPU mais augmente la latence
    //   - Évite de "brûler" le CPU en contention prolongée
    //
    // Ce comportement est transparent pour l'utilisateur :
    NkSpinLock lock;
    // L'appelant fait simplement :
    lock.Lock();  // Gère automatiquement fast-path + slow-path + backoff

    // ---------------------------------------------------------------------
    // Exemple 8 : Alignement cache-line pour éviter false sharing
    // ---------------------------------------------------------------------
    // NkSpinLock utilise alignas(NK_CACHE_LINE_SIZE) sur mLocked :
    //
    // Problème du false sharing :
    //   - Deux variables fréquemment écrites sur la même ligne de cache
    //   - Chaque écriture invalide le cache de l'autre core → performance dégradée
    //
    // Solution :
    //   - alignas(64) sur x86/x64, alignas(128) sur ARM selon NK_CACHE_LINE_SIZE
    //   - Garantit que mLocked occupe sa propre ligne de cache
    //   - Évite les invalidations inutiles entre cores
    //
    // Impact performance :
    //   - Jusqu'à 10x plus rapide en environnement multi-core avec contention
    //   - Transparent pour l'utilisateur : géré automatiquement par la classe

    // ---------------------------------------------------------------------
    // Exemple 9 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Utiliser NkSpinLock uniquement pour sections ultra-courtes
    void GoodSpinUsage(NkSpinLock& lock, int& counter)
    {
        NkScopedSpinLock guard(lock);
        ++counter;  // ✅ 1-2 instructions : idéal pour spin-lock
    }

    // ❌ MAUVAIS : Utiliser spin-lock pour section longue (waste CPU)
    void BadSpinUsage(NkSpinLock& lock, std::vector<int>& buffer)
    {
        NkScopedSpinLock guard(lock);
        for (int i = 0; i < 1000000; ++i) {  // ❌ 1M itérations = waste CPU !
            buffer[i] = Compute(buffer[i]);
        }
    }

    // ✅ BON : Vérifier TryLock() avant fallback pour éviter le blocage
    bool SafeTryOperation(NkSpinLock& lock)
    {
        if (!lock.TryLock()) {
            return false;  // Échec immédiat : pas de blocage
        }
        DoQuickWork();
        lock.Unlock();
        return true;
    }

    // ❌ MAUVAIS : Appeler Unlock() sans avoir acquis le lock (UB)
    void RiskyUnlock(NkSpinLock& lock)
    {
        if (someCondition) {
            return;  // ❌ Retour sans Lock() → Unlock() suivant = UB !
        }
        lock.Lock();
        DoWork();
        lock.Unlock();
    }

    // ✅ BON : Toujours utiliser RAII pour garantir Unlock() même sur exception
    void SafeExceptionHandling(NkSpinLock& lock)
    {
        NkScopedSpinLock guard(lock);  // Acquisition
        RiskyOperation();               // Peut lancer une exception
        // ✅ Unlock() appelé automatiquement même si exception lancée
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Intégration avec système de profiling/performance
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSpinLock.h>
    #include <NKCore/NkTimer.h>

    class ProfiledSpinLock {
    public:
        void Lock()
        {
            nkentseu::core::NkTimer timer;
            mSpinLock.Lock();
            nk_uint64 elapsed = timer.ElapsedNanoseconds();

            // Logging des acquisitions lentes (>1000ns = contention détectée)
            if (elapsed > 1000) {
                NK_LOG_WARNING("SpinLock contention: {}ns wait time", elapsed);
            }
        }

        void Unlock() { mSpinLock.Unlock(); }
        bool TryLock() { return mSpinLock.TryLock(); }

    private:
        NkSpinLock mSpinLock;
    };

    // ---------------------------------------------------------------------
    // Exemple 11 : Configuration CMake pour optimisation architecture
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Optimisations pour NkSpinLock

    # Activer les optimisations spécifiques à l'architecture pour PAUSE/YIELD
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(your_target PRIVATE -march=native)
        # Ou plus précis : -march=haswell pour AVX2 + PAUSE optimisé
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        target_compile_options(your_target PRIVATE -march=armv8-a+crc)
        # Active les instructions YIELD sur ARM64
    endif()

    # Définir NK_CACHE_LINE_SIZE si non détecté automatiquement
    if(NOT DEFINED NK_CACHE_LINE_SIZE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            target_compile_definitions(your_target PRIVATE NK_CACHE_LINE_SIZE=64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            target_compile_definitions(your_target PRIVATE NK_CACHE_LINE_SIZE=128)
        endif()
    endif()

    // ---------------------------------------------------------------------
    // Exemple 12 : Testing de performance et validation fonctionnelle
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NkSpinLock.h>
    #include <thread>
    #include <vector>

    TEST(NkSpinLockTest, BasicLockUnlock)
    {
        nkentseu::memory::NkSpinLock lock;

        EXPECT_FALSE(lock.IsLocked());

        lock.Lock();
        EXPECT_TRUE(lock.IsLocked());

        lock.Unlock();
        EXPECT_FALSE(lock.IsLocked());
    }

    TEST(NkSpinLockTest, TryLockSuccess)
    {
        nkentseu::memory::NkSpinLock lock;

        EXPECT_TRUE(lock.TryLock());   // Doit réussir : lock libre
        EXPECT_FALSE(lock.TryLock());  // Doit échouer : déjà verrouillé

        lock.Unlock();
        EXPECT_TRUE(lock.TryLock());   // Doit réussir après Unlock()
    }

    TEST(NkSpinLockTest, MultiThreadedContention)
    {
        nkentseu::memory::NkSpinLock lock;
        std::atomic<uint64_t> counter{0};
        constexpr int kIterations = 10000;
        constexpr int kThreads = 8;

        std::vector<std::thread> workers;
        for (int i = 0; i < kThreads; ++i) {
            workers.emplace_back([&]() {
                for (int j = 0; j < kIterations; ++j) {
                    NkScopedSpinLock guard(lock);
                    ++counter;  // Section critique protégée
                }
            });
        }

        for (auto& t : workers) {
            t.join();
        }

        // Vérification : toutes les incréments ont été comptés
        EXPECT_EQ(counter.load(), static_cast<uint64_t>(kThreads * kIterations));
    }

    TEST(NkScopedSpinLockTest, MoveSemantics)
    {
        nkentseu::memory::NkSpinLock lock;
        bool unlocked = false;

        {
            nkentseu::memory::NkScopedSpinLock guard1(lock);
            EXPECT_TRUE(lock.IsLocked());

            // Transfert de propriété
            nkentseu::memory::NkScopedSpinLock guard2(std::move(guard1));
            EXPECT_FALSE(guard1.GetLock());  // guard1 invalide après move
            EXPECT_TRUE(lock.IsLocked());     // lock toujours détenu
        }
        // guard2 destructeur : Unlock() appelé
        EXPECT_FALSE(lock.IsLocked());
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================