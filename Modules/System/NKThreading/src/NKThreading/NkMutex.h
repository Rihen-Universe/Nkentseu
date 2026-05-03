//
// NkMutex.h
// =============================================================================
// Description :
//   Définition des primitives de synchronisation multiplateforme pour la
//   gestion des sections critiques entre threads. Inclut NkMutex (non-réentrant),
//   NkRecursiveMutex (réentrant) et NkScopedLock (RAII guard pattern).
//
// Caractéristiques :
//   - Abstraction des primitives natives : SRWLock (Windows), pthread_mutex (POSIX)
//   - Garantie noexcept sur toutes les opérations de verrouillage
//   - Support timeout pour TryLockFor() avec boucle active optimisée
//   - Pattern RAII via NkScopedLock pour prévention des deadlocks
//   - Zéro allocation dynamique après construction
//
// Algorithmes implémentés :
//   - TryLockFor : boucle active avec YieldThread() + timestamp monotone
//   - RecursiveMutex : attributs pthread PTHREAD_MUTEX_RECURSIVE ou CRITICAL_SECTION
//   - ScopedLock : sémantique de déplacement pour transfert de propriété de lock
//
// Types disponibles :
//   NkMutex          : mutex standard non-réentrant (performance optimale)
//   NkRecursiveMutex : mutex réentrant (cas d'usage spécifiques uniquement)
//   NkScopedLock     : guard RAII move-only pour gestion automatique des locks
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_MUTEX_H__
#define __NKENTSEU_THREADING_MUTEX_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types de base et des macros d'export du module Threading.
    // Les en-têtes système sont inclus conditionnellement dans le fichier .cpp.

    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKThreading/NkThreadingApi.h"

    #ifdef NKENTSEU_PLATFORM_WINDOWS
        #include <synchapi.h>
    #else
        #include <pthread.h>
    #endif

    // Déclaration anticipée pour éviter l'inclusion circulaire.
    // NkConditionVariable nécessite un accès ami au handle natif du mutex.
    namespace nkentseu {
        namespace threading {
            class NkConditionVariable;
        }
    }

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE : NkMutex
            // =================================================================
            // Mutex standard non-réentrant pour synchronisation entre threads.
            //
            // Représentation interne :
            //   - Windows : SRWLOCK (Slim Reader/Writer Lock) - léger et performant
            //   - POSIX   : pthread_mutex_t avec attributs par défaut
            //
            // Invariant de production :
            //   Un mutex ne peut être déverrouillé que par le thread qui l'a verrouillé.
            //   Violation = comportement indéfini (UB) selon les standards OS.
            //
            // Avantages vs autres primitives :
            //   - Plus léger qu'un sémaphore pour l'exclusion mutuelle simple
            //   - Plus prévisible qu'un spinlock pour les sections longues (>1µs)
            //   - Support natif du priority inheritance sur certaines plateformes POSIX
            //
            // Ne pas utiliser pour :
            //   - Sections critiques très courtes (<100ns) : préférer NkSpinLock
            //   - Cas nécessitant un verrouillage récursif : préférer NkRecursiveMutex
            //   - Synchronisation producteur-consommateur : préférer NkConditionVariable
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkMutex {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR
                    // -------------------------------------------------------------

                    /// @brief Constructeur par défaut : initialise le mutex natif.
                    /// @note Garantie noexcept : aucune exception ne peut être levée.
                    /// @note Windows : InitializeSRWLock() ne peut pas échouer.
                    /// @note POSIX  : pthread_mutex_init() échec marqué via mInitialized.
                    NkMutex() noexcept;

                    /// @brief Destructeur : libère les ressources système associées.
                    /// @note Garantie noexcept : le nettoyage est toujours sûr.
                    /// @note Windows : SRWLock ne nécessite pas de destruction explicite.
                    /// @note POSIX  : pthread_mutex_destroy() appelé seulement si initialisé.
                    ~NkMutex() noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de copie supprimé : mutex non-copiable.
                    /// @note Un mutex représente une ressource système unique.
                    NkMutex(const NkMutex&) = delete;

                    /// @brief Opérateur d'affectation supprimé : mutex non-affectable.
                    NkMutex& operator=(const NkMutex&) = delete;

                    /// @brief Constructeur de déplacement supprimé : mutex non-déplaçable.
                    /// @note Le handle natif ne peut être transféré entre instances.
                    NkMutex(NkMutex&&) = delete;

                    /// @brief Opérateur de déplacement supprimé : mutex non-déplaçable.
                    NkMutex& operator=(NkMutex&&) = delete;

                    // -------------------------------------------------------------
                    // API PRINCIPALE - LOCK/UNLOCK
                    // -------------------------------------------------------------

                    /// @brief Verrouille le mutex de manière bloquante.
                    /// @note Si le mutex est déjà détenu, le thread appelant est suspendu.
                    /// @warning Appel récursif depuis le même thread = deadlock immédiat.
                    /// @warning Ne jamais appeler depuis un signal handler (UB POSIX).
                    void Lock() noexcept;

                    /// @brief Tente de verrouiller le mutex sans blocage.
                    /// @return true si le verrou a été acquis, false sinon.
                    /// @note Retourne immédiatement dans tous les cas (non-bloquant).
                    /// @note Utile pour éviter les deadlocks dans les algorithmes complexes.
                    [[nodiscard]] nk_bool TryLock() noexcept;

                    /// @brief Déverrouille le mutex précédemment acquis.
                    /// @warning Doit être appelé depuis le thread détenteur du verrou.
                    /// @warning Appel sans Lock() préalable = comportement indéfini.
                    /// @warning Double Unlock() = comportement indéfini.
                    void Unlock() noexcept;

                    /// @brief Tente de verrouiller avec un timeout maximal.
                    /// @param milliseconds Durée d'attente maximale en millisecondes.
                    /// @return true si le verrou a été acquis, false en cas de timeout.
                    /// @note Utilise une boucle active avec YieldThread() pour la portabilité.
                    /// @note Pour un timeout de 0ms, comportement équivalent à TryLock().
                    /// @note Précision du timeout dépend du scheduler OS (non garantie).
                    [[nodiscard]] nk_bool TryLockFor(nk_uint32 milliseconds) noexcept;

                    // -------------------------------------------------------------
                    // ACCÈS AU HANDLE NATIF (USAGE AVANCÉ)
                    // -------------------------------------------------------------

                    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                        /// @brief Accède au handle SRWLOCK natif Windows.
                        /// @return Référence vers le SRWLOCK sous-jacent.
                        /// @warning Usage réservé aux intégrations bas-niveau.
                        SRWLOCK& GetNativeHandle() noexcept;
                    #else
                        /// @brief Accède au handle pthread_mutex_t natif POSIX.
                        /// @return Référence vers le pthread_mutex_t sous-jacent.
                        /// @warning Usage réservé aux intégrations bas-niveau.
                        pthread_mutex_t& GetNativeHandle() noexcept;
                    #endif
        
                    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                        SRWLOCK& Get() {
                    #else
                        pthread_mutex_t& Get() {
                    #endif
                        return GetNativeHandle();
                    }

                private:

                    // -------------------------------------------------------------
                    // MÉTHODES UTILITAIRES INTERNES (STATIQUES PRIVÉES)
                    // -------------------------------------------------------------

                    /// @brief Récupère le temps monotone courant en millisecondes.
                    /// @return Timestamp en ms depuis le boot système (monotone, non-décrémentable).
                    /// @note Utilisé uniquement par TryLockFor() pour le calcul du timeout.
                    static nk_uint64 GetMonotonicTimeMs() noexcept;

                    /// @brief Cède volontairement le temps CPU au scheduler.
                    /// @note Optimise l'attente active dans TryLockFor() pour réduire la charge CPU.
                    static void YieldThread() noexcept;

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS (DONNÉES INTERNES)
                    // -------------------------------------------------------------

                    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                        SRWLOCK mMutex;  ///< Handle SRWLock natif Windows (8 bytes, aligné)
                    #else
                        pthread_mutex_t mMutex;  ///< Handle pthread_mutex_t natif POSIX
                    #endif

                    nk_bool mInitialized;  ///< Indicateur d'initialisation réussie du mutex

                    // -------------------------------------------------------------
                    // CLASSES AMIES (ACCÈS PRIVILÉGIÉ)
                    // -------------------------------------------------------------

                    /// @brief NkConditionVariable nécessite un accès direct au mutex natif.
                    friend class NkConditionVariable;
            };

            // =================================================================
            // CLASSE : NkRecursiveMutex
            // =================================================================
            // Mutex réentrant permettant le verrouillage multiple par le même thread.
            //
            // Représentation interne :
            //   - Windows : CRITICAL_SECTION avec récursivité native
            //   - POSIX   : pthread_mutex_t avec attribut PTHREAD_MUTEX_RECURSIVE
            //
            // Comportement récursif :
            //   - Chaque Lock() incrémente un compteur interne de récursivité
            //   - Chaque Unlock() décrémente ce compteur
            //   - Le mutex n'est réellement libéré que lorsque le compteur atteint 0
            //
            // Utiliser pour :
            //   - Fonctions récursives nécessitant une protection mutex
            //   - API publique appelant des méthodes internes déjà protégées
            //   - Cas où l'ordre d'appel des verrous n'est pas contrôlable
            //
            // Ne pas utiliser pour :
            //   - Sections critiques standards (préférer NkMutex, 2-3x plus rapide)
            //   - Code sensible aux performances (overhead de comptage + atomic)
            //   - Nouveaux designs : souvent signe de couplage excessif
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkRecursiveMutex {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR
                    // -------------------------------------------------------------

                    /// @brief Constructeur : initialise le mutex réentrant natif.
                    /// @note Configuration automatique des attributs pthread si nécessaire.
                    NkRecursiveMutex() noexcept;

                    /// @brief Destructeur : libère les ressources système.
                    /// @note Vérifie l'état d'initialisation avant destruction.
                    ~NkRecursiveMutex() noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Mutex réentrant non-copiable.
                    NkRecursiveMutex(const NkRecursiveMutex&) = delete;

                    /// @brief Mutex réentrant non-affectable.
                    NkRecursiveMutex& operator=(const NkRecursiveMutex&) = delete;

                    /// @brief Mutex réentrant non-déplaçable.
                    NkRecursiveMutex(NkRecursiveMutex&&) = delete;

                    /// @brief Mutex réentrant non-affectable par déplacement.
                    NkRecursiveMutex& operator=(NkRecursiveMutex&&) = delete;

                    // -------------------------------------------------------------
                    // API LOCK/UNLOCK
                    // -------------------------------------------------------------

                    /// @brief Verrouille le mutex réentrant (bloquant).
                    /// @note Incrémente le compteur de récursivité interne.
                    void Lock() noexcept;

                    /// @brief Tente de verrouiller sans blocage.
                    /// @return true si acquis, false si déjà détenu par un autre thread.
                    [[nodiscard]] nk_bool TryLock() noexcept;

                    /// @brief Déverrouille le mutex (décrémente le compteur de récursivité).
                    /// @note Le mutex n'est réellement libéré que lorsque le compteur atteint 0.
                    void Unlock() noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------

                    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                        CRITICAL_SECTION mMutex;  ///< Handle CRITICAL_SECTION Windows
                    #else
                        pthread_mutex_t mMutex;   ///< Handle pthread_mutex_t récursif
                    #endif

                    nk_bool mInitialized;  ///< Indicateur d'initialisation réussie
            };

            // =================================================================
            // CLASSE : NkScopedLock (RAII guard pattern)
            // =================================================================
            // Guard RAII pour la gestion automatique et sécurisée des verrous de mutex.
            //
            // Pattern Resource Acquisition Is Initialization :
            //   - Acquisition du lock dans le constructeur (immédiate)
            //   - Libération automatique dans le destructeur (garantie même en cas d'exception)
            //   - Sémantique de déplacement pour transfert de propriété entre scopes
            //
            // Avantages :
            //   - Prévention systématique des deadlocks par oubli d'Unlock()
            //   - Code plus lisible : la portée du lock est visuellement délimitée
            //   - Sécurité exceptionnelle : rollback automatique sur throw
            //   - Compatible avec les early returns et les multiples points de sortie
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkScopedLockMutex {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR (INLINE POUR PERFORMANCE)
                    // -------------------------------------------------------------

                    /// @brief Constructeur : verrouille immédiatement le mutex fourni.
                    /// @param mutex Référence vers le mutex à protéger.
                    /// @note Garantie noexcept : Lock() ne lève pas d'exception.
                    explicit NkScopedLockMutex(NkMutex& mutex) noexcept;

                    /// @brief Destructeur : déverrouille le mutex si encore détenu.
                    /// @note Garantie noexcept : Unlock() est toujours sûr.
                    ~NkScopedLockMutex() noexcept;

                    // -------------------------------------------------------------
                    // SÉMANTIQUE DE DÉPLACEMENT (MOVE-ONLY)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de déplacement : transfère la propriété du lock.
                    /// @param other Instance source dont la propriété est transférée.
                    /// @note L'instance source devient invalide (mMutex = nullptr).
                    NkScopedLockMutex(NkScopedLockMutex&& other) noexcept;

                    /// @brief Opérateur d'affectation par déplacement.
                    /// @param other Instance source dont la propriété est transférée.
                    /// @return Référence vers cette instance après transfert.
                    NkScopedLockMutex& operator=(NkScopedLockMutex&& other) noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Copie interdite : un lock ne peut pas être dupliqué.
                    NkScopedLockMutex(const NkScopedLockMutex&) = delete;

                    /// @brief Affectation par copie interdite.
                    NkScopedLockMutex& operator=(const NkScopedLockMutex&) = delete;

                    // -------------------------------------------------------------
                    // MÉTHODES DE CONTRÔLE ET REQUÊTES
                    // -------------------------------------------------------------

                    /// @brief Libère manuellement le mutex avant la destruction du guard.
                    /// @note Rend le guard inactif : le destructeur n'appellera pas Unlock().
                    void Unlock() noexcept;

                    /// @brief Accède au mutex protégé par ce guard.
                    /// @return Pointeur vers le mutex, ou nullptr si le guard a été déplacé.
                    [[nodiscard]] NkMutex* GetMutex() noexcept;

                    /// @brief Accède au mutex protégé (version const).
                    /// @return Pointeur const vers le mutex, ou nullptr si déplacé.
                    [[nodiscard]] const NkMutex* GetMutex() const noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------

                    /// @brief Pointeur vers le mutex géré (nullptr si guard déplacé/inactif).
                    NkMutex* mMutex;
            };

        }
    }

#endif

// =============================================================================
// IMPLÉMENTATIONS INLINE (dans le header pour optimisation)
// =============================================================================

namespace nkentseu {
    namespace threading {

        // ---------------------------------------------------------------------
        // Implémentation : NkScopedLock (méthodes inline critiques)
        // ---------------------------------------------------------------------
        // Ces méthodes sont définies inline dans le header pour permettre
        // l'optimisation par le compilateur et éviter l'overhead d'appel de
        // fonction pour des opérations fréquentes dans les boucles critiques.

        // ---------------------------------------------------------------------
        // Constructeur : acquisition immédiate du lock
        // ---------------------------------------------------------------------
        inline NkScopedLockMutex::NkScopedLockMutex(NkMutex& mutex) noexcept
            : mMutex(&mutex)
        {
            mMutex->Lock();
        }

        // ---------------------------------------------------------------------
        // Destructeur : libération automatique si encore détenu
        // ---------------------------------------------------------------------
        inline NkScopedLockMutex::~NkScopedLockMutex() noexcept
        {
            if (mMutex != nullptr) {
                mMutex->Unlock();
            }
        }

        // ---------------------------------------------------------------------
        // Constructeur de déplacement : transfert de propriété
        // ---------------------------------------------------------------------
        inline NkScopedLockMutex::NkScopedLockMutex(NkScopedLockMutex&& other) noexcept
            : mMutex(other.mMutex)
        {
            other.mMutex = nullptr;
        }

        // ---------------------------------------------------------------------
        // Opérateur de déplacement : transfert avec nettoyage préalable
        // ---------------------------------------------------------------------
        inline NkScopedLockMutex& NkScopedLockMutex::operator=(NkScopedLockMutex&& other) noexcept
        {
            if (this != &other) {
                if (mMutex != nullptr) {
                    mMutex->Unlock();
                }
                mMutex = other.mMutex;
                other.mMutex = nullptr;
            }
            return *this;
        }

        // ---------------------------------------------------------------------
        // Méthode : Unlock manuel avec invalidation du guard
        // ---------------------------------------------------------------------
        inline void NkScopedLockMutex::Unlock() noexcept
        {
            if (mMutex != nullptr) {
                mMutex->Unlock();
                mMutex = nullptr;
            }
        }

        // ---------------------------------------------------------------------
        // Accesseur : GetMutex (version mutable)
        // ---------------------------------------------------------------------
        inline NkMutex* NkScopedLockMutex::GetMutex() noexcept
        {
            return mMutex;
        }

        // ---------------------------------------------------------------------
        // Accesseur : GetMutex (version const)
        // ---------------------------------------------------------------------
        inline const NkMutex* NkScopedLockMutex::GetMutex() const noexcept
        {
            return mMutex;
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Protection basique d'une ressource partagée
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>
    using namespace nkentseu::threading;

    NkMutex dataMutex;
    std::vector<int> sharedBuffer;

    void Producer(int value)
    {
        NkScopedLockMutex lock(dataMutex);
        sharedBuffer.push_back(value);
    }

    int Consumer()
    {
        NkScopedLockMutex lock(dataMutex);
        if (sharedBuffer.empty()) {
            return -1;
        }
        int value = sharedBuffer.back();
        sharedBuffer.pop_back();
        return value;
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Timeout pour éviter les blocages infinis
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>
    #include <NKCore/NkLogger.h>

    NkMutex resourceMutex;

    bool TryAccessResource(nk_uint32 timeoutMs)
    {
        if (!resourceMutex.TryLockFor(timeoutMs)) {
            NK_LOG_WARNING("Timeout acquiring resource lock after {}ms", timeoutMs);
            return false;
        }
        PerformCriticalOperation();
        resourceMutex.Unlock();
        return true;
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Mutex réentrant pour API imbriquée
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>

    class DataProcessor {
    public:
        void ProcessBatch(const std::vector<Data>& items)
        {
            NkScopedLockMutex lock(mMutex);
            for (const auto& item : items) {
                ProcessItem(item);
            }
        }

    private:
        void ProcessItem(const Data& item)
        {
            NkScopedLockMutex lock(mMutex);
            // Transformation...
        }
        NkRecursiveMutex mMutex;
    };

    // ---------------------------------------------------------------------
    // Exemple 4 : Transfert de propriété de NkScopedLockMutex (move semantics)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>

    void ExtendedCriticalSection(NkMutex& mutex)
    {
        NkScopedLockMutex lock(mutex);
        if (NeedsExtendedProcessing()) {
            ContinueProcessing(std::move(lock));
            return;
        }
    }

    void ContinueProcessing(NkScopedLockMutex&& lock)
    {
        PerformExtendedOperation();
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Intégration avec NkConditionVariable
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>
    #include <NKThreading/NkConditionVariable.h>

    class ThreadSafeQueue {
    public:
        void Push(Item item)
        {
            {
                NkScopedLockMutex lock(mMutex);
                mQueue.push(std::move(item));
            }
            mCondition.NotifyOne();
        }
        Item Pop()
        {
            NkScopedLockMutex lock(mMutex);
            mCondition.Wait(lock, [this]() { return !mQueue.empty(); });
            Item item = std::move(mQueue.front());
            mQueue.pop();
            return item;
        }
    private:
        NkMutex mMutex;
        NkConditionVariable mCondition;
        std::queue<Item> mQueue;
    };

    // ---------------------------------------------------------------------
    // Exemple 6 : Configuration CMake pour NKThreading
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Module NKThreading
    add_library(nkthreading
        src/NkMutex.cpp
        src/NkConditionVariable.cpp
        src/NkThreadPool.cpp
    )
    target_include_directories(nkthreading PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(nkthreading PUBLIC
        NKCore::NKCore
        NKPlatform::NKPlatform
    )
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(nkthreading PRIVATE NKENTSEU_THREADING_BUILD_SHARED_LIB)
    else()
        target_compile_definitions(nkthreading PRIVATE NKENTSEU_THREADING_STATIC_LIB)
    endif()

    // ---------------------------------------------------------------------
    // Exemple 7 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : RAII avec NkScopedLockMutex
    void GoodExample(NkMutex& mutex)
    {
        NkScopedLockMutex guard(mutex);
        // Code protégé...
    }

    // ❌ MAUVAIS : Gestion manuelle risque d'oubli
    void BadExample(NkMutex& mutex)
    {
        mutex.Lock();
        // Code protégé...
        mutex.Unlock();
    }

    // ✅ BON : Mutex réentrant uniquement si nécessaire
    class RecursiveAPI {
        NkRecursiveMutex mMutex;
    };

    // ❌ MAUVAIS : Sur-utilisation de mutex réentrant
    class SimpleData {
        NkRecursiveMutex mMutex;  // NkMutex suffirait !
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
