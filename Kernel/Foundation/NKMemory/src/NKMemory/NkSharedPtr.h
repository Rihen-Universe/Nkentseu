// =============================================================================
// NKMemory/NkSharedPtr.h
// Smart pointer à référence comptée thread-safe avec support d'allocateur.
//
// Design :
//  - Header-only : implémentations inline requises pour templates
//  - Réutilisation de NKCore/NkAtomic.h pour le comptage atomique (ZÉRO duplication)
//  - Bloc de contrôle séparé : permet NkWeakPtr sans surcoût pour NkSharedPtr
//  - Support explicite d'allocateur pour l'objet ET le bloc de contrôle
//  - API compatible std::shared_ptr avec extensions NKMemory
//  - Thread-safe : opérations atomiques sur les compteurs de référence
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKSHAREDPTR_H
#define NKENTSEU_MEMORY_NKSHAREDPTR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkAllocator.h"      // NkAllocator, New<T>, Delete<T>
    #include "NKMemory/NkUniquePtr.h"      // Pour cohérence d'API (optionnel)
    #include "NKCore/NkAtomic.h"           // NkAtomicInt32 pour comptage thread-safe
    #include "NKCore/NkTraits.h"           // traits::NkForward, traits::NkMove
    #include "NKCore/NkTypes.h"            // nk_bool, nk_size, nk_int32

    // -------------------------------------------------------------------------
    // SECTION 2 : BLOC DE CONTRÔLE ABSTRAIT
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // Forward declaration pour amitié
            template <typename T> class NkWeakPtr;

            /**
             * @class NkSharedControlBlockBase
             * @brief Base polymorphe pour le bloc de contrôle des références
             * @ingroup MemorySmartPointersInternal
             * 
             * Gère :
             *  - Comptage fort (shared_ptr) et faible (weak_ptr) via atomiques
             *  - Destruction polymorphe de l'objet et du bloc de contrôle
             *  - Logique thread-safe de transition 1->0 pour les compteurs
             * 
             * @note Classe abstraite : l'implémentation concrète est dans NkSharedControlBlock<T>
             * @note Non-copyable : chaque instance est unique à un objet géré
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkSharedControlBlockBase {
            public:
                /** @brief Initialise les compteurs : strong=1 (nouveau shared_ptr), weak=1 (le bloc lui-même) */
                NkSharedControlBlockBase() noexcept : mStrong(1), mWeak(1) {}
                
                /** @brief Destructeur virtuel pour destruction polymorphe sûre */
                virtual ~NkSharedControlBlockBase() = default;

                // Non-copyable : état partagé unique
                NkSharedControlBlockBase(const NkSharedControlBlockBase&) = delete;
                NkSharedControlBlockBase& operator=(const NkSharedControlBlockBase&) = delete;

                // =================================================================
                // @name Gestion des références fortes (shared_ptr)
                // =================================================================
                
                /** @brief Incrémente le compteur de références fortes (thread-safe) */
                void AddStrongRef() noexcept { mStrong.FetchAdd(1); }

                /**
                 * @brief Décrémente le compteur fort et détruit l'objet si dernier
                 * @note Si mStrong passe de 1 à 0 : appelle DestroyObject() puis ReleaseWeakRef()
                 * @note Thread-safe : opération atomique avec mémoire ordering approprié
                 */
                void ReleaseStrongRef() noexcept {
                    // FetchSub retourne la valeur AVANT la soustraction
                    if (mStrong.FetchSub(1) == 1) {
                        DestroyObject();      // Détruit l'objet géré (virtuel)
                        ReleaseWeakRef();     // Libère la référence faible implicite du bloc
                    }
                }

                // =================================================================
                // @name Gestion des références faibles (weak_ptr)
                // =================================================================
                
                /** @brief Incrémente le compteur de références faibles (thread-safe) */
                void AddWeakRef() noexcept { mWeak.FetchAdd(1); }

                /**
                 * @brief Décrémente le compteur faible et détruit le bloc si dernier
                 * @note Le compteur weak inclut la référence "implicite" du bloc lui-même
                 * @note Si mWeak passe de 1 à 0 : le bloc de contrôle est dealloqué
                 */
                void ReleaseWeakRef() noexcept {
                    if (mWeak.FetchSub(1) == 1) {
                        DestroyControlBlock();  // Détruit le bloc de contrôle lui-même (virtuel)
                    }
                }

                // =================================================================
                // @name Interrogation d'état (thread-safe)
                // =================================================================
                
                /** @brief Retourne le nombre actuel de shared_ptr pointant vers l'objet */
                [[nodiscard]] nk_int32 StrongCount() const noexcept { return mStrong.Load(); }
                
                /** 
                 * @brief Retourne le nombre de weak_ptr (exclut la référence interne du bloc)
                 * @note weak_count = mWeak.Load() - 1 (la soustraction retire la référence du bloc)
                 */
                [[nodiscard]] nk_int32 WeakCount() const noexcept { return mWeak.Load() - 1; }
                
                /** @brief Teste si l'objet géré a été détruit (strong_count == 0) */
                [[nodiscard]] nk_bool Expired() const noexcept { return mStrong.Load() == 0; }

                /**
                 * @brief Tente d'incrémenter strong_count de manière atomique et conditionnelle
                 * @return true si l'objet est encore valide et le compteur a été incrémenté
                 * @note Utilisé par NkWeakPtr::Lock() pour promotion weak->strong thread-safe
                 * @note Boucle CAS (Compare-And-Swap) pour gérer les conflits concurrents
                 */
                [[nodiscard]] nk_bool TryAddStrongRef() noexcept {
                    nk_int32 expected = mStrong.Load();
                    while (expected > 0) {  // Ne peut promouvoir que si l'objet existe encore
                        const nk_int32 desired = expected + 1;
                        if (mStrong.CompareExchangeWeak(expected, desired)) {
                            return true;  // Succès : compteur incrémenté atomiquement
                        }
                        // Échec : expected mis à jour avec la valeur actuelle, retry
                    }
                    return false;  // Objet déjà expiré : promotion impossible
                }

            protected:
                /**
                 * @brief Détruit l'objet géré (implémentation concrète dans NkSharedControlBlock<T>)
                 * @note Appelé quand strong_count passe à 0
                 * @note Doit être noexcept pour garantir pas d'exception pendant la destruction
                 */
                virtual void DestroyObject() noexcept = 0;
                
                /**
                 * @brief Détruit et dealloque le bloc de contrôle lui-même
                 * @note Appelé quand weak_count passe à 0 (plus aucun shared_ptr ni weak_ptr)
                 * @note Doit utiliser l'allocateur approprié pour la deallocation
                 */
                virtual void DestroyControlBlock() noexcept = 0;

            private:
                NkAtomicInt32 mStrong;  ///< Compteur de références fortes (shared_ptr)
                NkAtomicInt32 mWeak;    ///< Compteur de références faibles (weak_ptr + 1 pour le bloc)
            };

            // -------------------------------------------------------------------------
            // SECTION 3 : BLOC DE CONTRÔLE CONCRET (TEMPLATÉ)
            // -------------------------------------------------------------------------
            /**
             * @class NkSharedControlBlock<T>
             * @brief Implémentation concrète du bloc de contrôle pour un type T
             * @tparam T Type de l'objet géré
             * @ingroup MemorySmartPointersInternal
             * 
             * Stocke :
             *  - Pointeur vers l'objet géré (T*)
             *  - Pointeur vers l'allocateur à utiliser (pour destruction ET deallocation du bloc)
             *  - Flag indiquant si l'objet a été alloué via NkAllocator (vs new standard)
             * 
             * @note Le bloc lui-même est alloué via NkAllocator::Allocate (pas new)
             * @note Placement new utilisé pour construire le bloc dans la mémoire allouée
             */
            template <typename T>
            class NkSharedControlBlock final : public NkSharedControlBlockBase {
            public:
                /**
                 * @brief Constructeur avec paramètres de gestion mémoire
                 * @param ptr Pointeur vers l'objet nouvellement alloué (prend ownership)
                 * @param allocator Allocateur à utiliser pour destruction ET deallocation du bloc
                 * @param useAllocatorForObject true si l'objet doit être détruit via allocator->Delete
                 */
                NkSharedControlBlock(T* ptr, NkAllocator* allocator, nk_bool useAllocatorForObject) noexcept
                    : mPtr(ptr)
                    , mAllocator(allocator ? allocator : &NkGetDefaultAllocator())
                    , mUseAllocatorForObject(useAllocatorForObject) {}

            protected:
                /**
                 * @brief Détruit l'objet géré via l'allocateur approprié
                 * @note Si mUseAllocatorForObject : appelle allocator->Delete<T>(mPtr)
                 * @note Sinon : utilise delete standard (compatibilité avec code legacy)
                 */
                void DestroyObject() noexcept override {
                    if (!mPtr) {
                        return;  // Defensive : devrait ne jamais arriver en usage normal
                    }
                    if (mUseAllocatorForObject && mAllocator) {
                        mAllocator->Delete(mPtr);  // Destructeur + deallocate via NKMemory
                    } else {
                        delete mPtr;  // Fallback standard C++
                    }
                    mPtr = nullptr;  // Prévention dangling pointer (défensif)
                }

                /**
                 * @brief Détruit et dealloque le bloc de contrôle lui-même
                 * @note Utilise placement delete explicite puis deallocate via l'allocateur
                 * @note L'allocateur du bloc peut être différent de celui de l'objet (cas rare)
                 */
                void DestroyControlBlock() noexcept override {
                    NkAllocator* alloc = mAllocator ? mAllocator : &NkGetDefaultAllocator();
                    
                    // Destruction explicite via placement delete (appelle le dtor ~NkSharedControlBlock)
                    this->~NkSharedControlBlock();
                    
                    // Deallocation de la mémoire du bloc via l'allocateur
                    alloc->Deallocate(this);
                }

            private:
                T*             mPtr;                    ///< Pointeur vers l'objet géré
                NkAllocator*   mAllocator;              ///< Allocateur pour destruction/deallocation
                nk_bool        mUseAllocatorForObject;  ///< Flag : utiliser allocator vs delete standard
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE PRINCIPALE NkSharedPtr
            // -------------------------------------------------------------------------
            /**
             * @class NkSharedPtr
             * @brief Smart pointer à référence comptée thread-safe avec allocateur personnalisé
             * @tparam T Type de l'objet géré (peut être incomplet à la déclaration)
             * @ingroup MemorySmartPointers
             * 
             * Caractéristiques :
             *  - Référence comptée thread-safe via NkAtomicInt32 (NKCore)
             *  - Support d'allocateur personnalisé pour l'objet ET le bloc de contrôle
             *  - Compatible avec NkWeakPtr pour références non-possessives
             *  - API compatible std::shared_ptr pour migration facile
             *  - noexcept sur les opérations critiques (copy/move/destroy)
             * 
             * @note Le bloc de contrôle est alloué séparément : surcoût d'une allocation
             * @note Pour éviter l'allocation séparée, envisager NkMakeShared (optimisation future)
             * @note Thread-safe pour les opérations de référence counting, pas pour l'accès à l'objet
             */
            template <typename T>
            class NkSharedPtr {
            public:
                using element_type = T;  ///< Type de l'élément géré (pour compatibilité générique)

                // =================================================================
                // @name Constructeurs
                // =================================================================
                
                /** @brief Constructeur par défaut : pointeur nul, pas de bloc de contrôle */
                constexpr NkSharedPtr() noexcept : mPtr(nullptr), mControl(nullptr) {}
                
                /** @brief Constructeur depuis nullptr (pour tests conditionnels) */
                constexpr NkSharedPtr(decltype(nullptr)) noexcept : mPtr(nullptr), mControl(nullptr) {}

                /**
                 * @brief Constructeur principal depuis un pointeur brut
                 * @param ptr Pointeur vers l'objet à gérer (prend ownership)
                 * @param allocator Allocateur à utiliser (nullptr = allocateur par défaut)
                 * 
                 * @note Alloue le bloc de contrôle via l'allocateur spécifié
                 * @note Si l'allocation du bloc échoue : détruit ptr et retourne un shared_ptr vide
                 * @note Le flag useAllocatorForObject est positionné selon si allocator != nullptr
                 * 
                 * @exception Safety : noexcept sur les opérations de base, mais Allocate peut échouer
                 */
                explicit NkSharedPtr(T* ptr, NkAllocator* allocator = nullptr)
                    : mPtr(ptr), mControl(nullptr) {
                    if (!ptr) {
                        return;  // Rien à gérer : état nul valide
                    }
                    
                    // L'allocateur du bloc de contrôle est celui fourni (ou défaut)
                    NkAllocator* controlAllocator = allocator ? allocator : &NkGetDefaultAllocator();
                    
                    // Allocation du bloc de contrôle avec alignement approprié
                    void* mem = controlAllocator->Allocate(
                        sizeof(NkSharedControlBlock<T>), 
                        alignof(NkSharedControlBlock<T>)
                    );
                    
                    if (!mem) {
                        // Échec d'allocation du bloc : nettoyage pour éviter fuite mémoire
                        if (allocator) {
                            allocator->Delete(ptr);  // Utiliser le même allocateur que prévu
                        } else {
                            delete ptr;  // Fallback standard
                        }
                        mPtr = nullptr;
                        return;
                    }
                    
                    // Construction du bloc via placement new dans la mémoire allouée
                    const nk_bool useAllocatorForObject = (allocator != nullptr);
                    mControl = new (mem) NkSharedControlBlock<T>(ptr, controlAllocator, useAllocatorForObject);
                }

                /** @brief Constructeur de copie : incrémente le compteur fort */
                NkSharedPtr(const NkSharedPtr& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    if (mControl) {
                        mControl->AddStrongRef();  // Thread-safe increment
                    }
                }

                /** @brief Constructeur de mouvement : transfère ownership sans toucher aux compteurs */
                NkSharedPtr(NkSharedPtr&& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    other.mPtr = nullptr;
                    other.mControl = nullptr;  // L'autre instance devient vide
                }

                /**
                 * @brief Constructeur depuis NkWeakPtr : tentative de promotion weak->strong
                 * @param weak Référence faible à promouvoir
                 * @note Thread-safe : utilise TryAddStrongRef() pour éviter les races
                 * @note Si l'objet a déjà été détruit : construit un shared_ptr vide
                 */
                explicit NkSharedPtr(const NkWeakPtr<T>& weak) noexcept
                    : mPtr(nullptr), mControl(nullptr) {
                    if (weak.mControl && weak.mControl->TryAddStrongRef()) {
                        // Promotion réussie : on partage le même bloc de contrôle
                        mControl = weak.mControl;
                        mPtr = weak.mPtr;
                    }
                    // Sinon : reste dans l'état nul (objet expiré)
                }

                // =================================================================
                // @name Destructeur
                // =================================================================
                
                /**
                 * @brief Destructeur : décrémente le compteur fort et nettoie si nécessaire
                 * @note Si dernier shared_ptr : détruit l'objet, puis potentiellement le bloc
                 * @note noexcept : la destruction ne doit jamais lever d'exception
                 */
                ~NkSharedPtr() noexcept {
                    if (mControl) {
                        mControl->ReleaseStrongRef();  // Peut déclencher destruction
                    }
                }

                // =================================================================
                // @name Opérateurs d'assignation
                // =================================================================
                
                /**
                 * @brief Assignation par copie : gère proprement l'ancien et le nouveau
                 * @note Utilise l'idiome copy-and-swap pour la sécurité exception
                 * @note Thread-safe : les incréments/décréments de compteurs sont atomiques
                 */
                NkSharedPtr& operator=(const NkSharedPtr& other) noexcept {
                    if (this != &other) {
                        // Copy-and-swap : crée une copie, puis échange (exception-safe)
                        NkSharedPtr(other).Swap(*this);
                    }
                    return *this;
                }

                /** @brief Assignation par mouvement : transfert efficace sans toucher aux compteurs */
                NkSharedPtr& operator=(NkSharedPtr&& other) noexcept {
                    if (this != &other) {
                        NkSharedPtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                /** @brief Assignation depuis nullptr : équivaut à Reset() */
                NkSharedPtr& operator=(decltype(nullptr)) noexcept {
                    Reset();
                    return *this;
                }

                // =================================================================
                // @name Gestion manuelle du cycle de vie
                // =================================================================
                
                /** @brief Réinitialise à l'état nul, libérant l'objet précédent si présent */
                void Reset() noexcept { NkSharedPtr().Swap(*this); }
                
                /**
                 * @brief Réinitialise avec un nouvel objet
                 * @param ptr Nouveau pointeur à gérer
                 * @param allocator Allocateur à utiliser pour le nouvel objet
                 * @note Équivalent à construire un nouveau shared_ptr puis Swap
                 */
                void Reset(T* ptr, NkAllocator* allocator = nullptr) { 
                    NkSharedPtr(ptr, allocator).Swap(*this); 
                }

                /**
                 * @brief Échange le contenu avec un autre NkSharedPtr
                 * @param other Instance avec laquelle échanger
                 * @note noexcept : opération sûre même en contexte d'exception
                 * @note N'affecte pas les compteurs de référence (juste échange de pointeurs)
                 */
                void Swap(NkSharedPtr& other) noexcept {
                    // Échange des pointeurs vers l'objet
                    T* tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    // Échange des pointeurs vers le bloc de contrôle
                    NkSharedControlBlockBase* tmpCtrl = mControl;
                    mControl = other.mControl;
                    other.mControl = tmpCtrl;
                }

                // =================================================================
                // @name Accès et interrogation
                // =================================================================
                
                /** @brief Retourne le pointeur brut géré (peut être nullptr) */
                [[nodiscard]] T* Get() const noexcept { return mPtr; }
                
                /** @brief Déférencement : accès à l'objet pointé */
                [[nodiscard]] T& operator*() const noexcept { return *mPtr; }
                
                /** @brief Accès membre : équivalent à Get()->member */
                [[nodiscard]] T* operator->() const noexcept { return mPtr; }
                
                /** 
                 * @brief Retourne le nombre de shared_ptr partageant cet objet
                 * @note Thread-safe : lecture atomique du compteur
                 * @note Retourne 0 si ce shared_ptr est vide
                 */
                [[nodiscard]] nk_int32 UseCount() const noexcept {
                    return mControl ? mControl->StrongCount() : 0;
                }
                
                /** @brief Teste si ce shared_ptr est le seul propriétaire (use_count == 1) */
                [[nodiscard]] nk_bool Unique() const noexcept { return UseCount() == 1; }
                
                /** @brief Teste si le pointeur est valide (non-null) */
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }
                
                /** @brief Conversion explicite en booléen pour tests conditionnels */
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

            private:
                // Constructeur privé pour usage interne (ex: NkWeakPtr::Lock)
                NkSharedPtr(T* ptr, NkSharedControlBlockBase* control, nk_bool addRef) noexcept
                    : mPtr(ptr), mControl(control) {
                    if (addRef && mControl) {
                        mControl->AddStrongRef();
                    }
                }

                T*                        mPtr;      ///< Pointeur vers l'objet géré
                NkSharedControlBlockBase* mControl;  ///< Pointeur vers le bloc de contrôle polymorphe

                // Amitié pour accès aux membres privés
                friend class NkWeakPtr<T>;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : CLASSE NkWeakPtr (référence faible)
            // -------------------------------------------------------------------------
            /**
             * @class NkWeakPtr
             * @brief Référence faible non-possessive vers un objet géré par NkSharedPtr
             * @tparam T Type de l'objet observé (doit correspondre au NkSharedPtr source)
             * @ingroup MemorySmartPointers
             * 
             * Caractéristiques :
             *  - N'affecte pas le cycle de vie de l'objet (n'incrémente pas strong_count)
             *  - Permet d'observer un objet sans empêcher sa destruction
             *  - Promotion thread-safe vers NkSharedPtr via Lock()
             *  - Utile pour briser les cycles de référence dans les graphes d'objets
             * 
             * @note Ne fournit pas d'accès direct à l'objet : utiliser Lock() d'abord
             * @note Thread-safe pour les opérations de comptage, pas pour l'accès à l'objet après Lock()
             */
            template <typename T>
            class NkWeakPtr {
            public:
                // =================================================================
                // @name Constructeurs
                // =================================================================
                
                /** @brief Constructeur par défaut : référence vide */
                constexpr NkWeakPtr() noexcept : mPtr(nullptr), mControl(nullptr) {}

                /**
                 * @brief Constructeur depuis NkSharedPtr : observe sans posséder
                 * @param shared Shared pointer à observer
                 * @note Incrémente weak_count, pas strong_count
                 * @note Partage le même bloc de contrôle que le shared_ptr source
                 */
                NkWeakPtr(const NkSharedPtr<T>& shared) noexcept
                    : mPtr(shared.mPtr), mControl(shared.mControl) {
                    if (mControl) {
                        mControl->AddWeakRef();  // Thread-safe increment du compteur faible
                    }
                }

                /** @brief Constructeur de copie : partage l'observation */
                NkWeakPtr(const NkWeakPtr& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    if (mControl) {
                        mControl->AddWeakRef();
                    }
                }

                /** @brief Constructeur de mouvement : transfert d'observation */
                NkWeakPtr(NkWeakPtr&& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    other.mPtr = nullptr;
                    other.mControl = nullptr;
                }

                // =================================================================
                // @name Destructeur
                // =================================================================
                
                /**
                 * @brief Destructeur : décrémente le compteur faible
                 * @note Si dernier weak_ptr ET plus aucun shared_ptr : détruit le bloc de contrôle
                 * @note noexcept : destruction sûre même en contexte d'exception
                 */
                ~NkWeakPtr() noexcept {
                    if (mControl) {
                        mControl->ReleaseWeakRef();
                    }
                }

                // =================================================================
                // @name Opérateurs d'assignation
                // =================================================================
                
                NkWeakPtr& operator=(const NkWeakPtr& other) noexcept {
                    if (this != &other) {
                        NkWeakPtr(other).Swap(*this);
                    }
                    return *this;
                }

                /** @brief Assignation depuis NkSharedPtr : commence à observer cet objet */
                NkWeakPtr& operator=(const NkSharedPtr<T>& shared) noexcept {
                    NkWeakPtr(shared).Swap(*this);
                    return *this;
                }

                NkWeakPtr& operator=(NkWeakPtr&& other) noexcept {
                    if (this != &other) {
                        NkWeakPtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                // =================================================================
                // @name Gestion manuelle
                // =================================================================
                
                /** @brief Arrête d'observer l'objet (décrémente weak_count) */
                void Reset() noexcept { NkWeakPtr().Swap(*this); }

                /** @brief Échange l'observation avec un autre weak_ptr */
                void Swap(NkWeakPtr& other) noexcept {
                    T* tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    NkSharedControlBlockBase* tmpCtrl = mControl;
                    mControl = other.mControl;
                    other.mControl = tmpCtrl;
                }

                // =================================================================
                // @name Interrogation et promotion
                // =================================================================
                
                /** @brief Retourne le nombre de shared_ptr pointant vers l'objet observé */
                [[nodiscard]] nk_int32 UseCount() const noexcept {
                    return mControl ? mControl->StrongCount() : 0;
                }
                
                /** @brief Teste si l'objet observé a été détruit (strong_count == 0) */
                [[nodiscard]] nk_bool Expired() const noexcept {
                    return !mControl || mControl->Expired();
                }
                
                /** @brief Alias pour Expired() : plus explicite dans certains contextes */
                [[nodiscard]] nk_bool IsValid() const noexcept {
                    return !Expired();
                }
                
                /**
                 * @brief Tente de promouvoir cette référence faible en shared_ptr fort
                 * @return NkSharedPtr<T> valide si l'objet existe encore, sinon shared_ptr vide
                 * @note Thread-safe : utilise TryAddStrongRef() pour éviter les races
                 * @note Si promotion réussie : l'objet est garanti vivant pendant la durée du shared_ptr retourné
                 * 
                 * @example
                 * @code
                 * NkWeakPtr<MyClass> weak = /\* ... *\/;
                 * if (auto shared = weak.Lock(); shared.IsValid()) {
                 *     shared->DoSomething();  // Objet garanti vivant ici
                 * }
                 * // Après ce bloc, l'objet peut être détruit si plus de références
                 * @endcode
                 */
                [[nodiscard]] NkSharedPtr<T> Lock() const noexcept {
                    if (!mControl) {
                        return NkSharedPtr<T>();  // Référence vide : promotion impossible
                    }
                    if (!mControl->TryAddStrongRef()) {
                        return NkSharedPtr<T>();  // Objet déjà détruit : échec de promotion
                    }
                    // Promotion réussie : construit un shared_ptr partageant le bloc
                    // addRef=false car TryAddStrongRef a déjà incrémenté le compteur
                    return NkSharedPtr<T>(mPtr, mControl, false);
                }

            private:
                T*                        mPtr;      ///< Pointeur vers l'objet observé (non-possédé)
                NkSharedControlBlockBase* mControl;  ///< Bloc de contrôle partagé avec les shared_ptr

                // Amitié pour accès aux membres privés (construction interne)
                friend class NkSharedPtr<T>;
            };

            // -------------------------------------------------------------------------
            // SECTION 6 : FONCTIONS FABRIQUE (FACTORY FUNCTIONS)
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemorySharedPtrFactories Fonctions de Création NkSharedPtr
             * @brief Helpers pour construire des NkSharedPtr de manière sûre et expressive
             */

            /**
             * @brief Crée un NkSharedPtr<T> avec construction in-place
             * @tparam T Type de l'objet à créer
             * @tparam Args Types des arguments du constructeur de T
             * @param args Arguments forwardés au constructeur de T
             * @return NkSharedPtr<T> prenant ownership de l'objet nouvellement créé
             * @ingroup MemorySharedPtrFactories
             * 
             * @note Utilise l'allocateur par défaut via NkGetDefaultAllocator()
             * @note Exception-safe : si New<T> lève, pas de fuite mémoire
             * @note Allocation séparée de l'objet et du bloc de contrôle (2 allocations)
             * @note Pour une allocation unique (objet + bloc), envisager NkMakeShared optimisé (futur)
             * 
             * @example
             * @code
             * auto obj = NkMakeShared<MyClass>(arg1, arg2);
             * if (obj.IsValid()) {
             *     obj->DoSomething();
             * }
             * // Destruction automatique quand dernier shared_ptr/weak_ptr disparaît
             * @endcode
             */
            template <typename T, typename... Args>
            [[nodiscard]] NkSharedPtr<T> NkMakeShared(Args&&... args) {
                NkAllocator& alloc = NkGetDefaultAllocator();
                T* object = alloc.New<T>(traits::NkForward<Args>(args)...);
                return NkSharedPtr<T>(object, &alloc);
            }

            /**
             * @brief Crée un NkSharedPtr<T> avec allocateur personnalisé
             * @tparam T Type de l'objet à créer
             * @tparam Args Types des arguments du constructeur
             * @param allocator Allocateur à utiliser pour l'objet ET le bloc de contrôle
             * @param args Arguments forwardés au constructeur de T
             * @return NkSharedPtr<T> configuré avec l'allocateur spécifié
             * @ingroup MemorySharedPtrFactories
             * 
             * @note L'allocateur est utilisé pour :
             *   - Allouer l'objet via allocator.New<T>()
             *   - Allouer le bloc de contrôle via allocator.Allocate()
             *   - Détruire l'objet via allocator.Delete<T>()
             *   - Détruire le bloc via allocator.Deallocate()
             * @note Essentiel pour la cohérence mémoire dans les pools/arenas
             */
            template <typename T, typename... Args>
            [[nodiscard]] NkSharedPtr<T> NkMakeSharedWithAllocator(NkAllocator& allocator, Args&&... args) {
                T* object = allocator.New<T>(traits::NkForward<Args>(args)...);
                return NkSharedPtr<T>(object, &allocator);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKSHAREDPTR_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Usage basique avec référence comptée
    void Example1() {
        auto shared1 = nkentseu::memory::NkMakeShared<MyClass>(arg1);
        {
            auto shared2 = shared1;  // Copie : incrémente strong_count à 2
            shared2->DoWork();
        }  // shared2 détruit : strong_count retourne à 1, objet toujours vivant
        
        shared1->DoMoreWork();  // Objet toujours valide
    }  // shared1 détruit : strong_count=0 -> objet détruit

    // Exemple 2 : WeakPtr pour éviter les cycles de référence
    class Node {
    public:
        nkentseu::memory::NkSharedPtr<Node> parent;      // Référence forte vers parent
        nkentseu::memory::NkWeakPtr<Node> child;         // Référence faible vers enfant
        
        void SetupChild(nkentseu::memory::NkSharedPtr<Node> childShared) {
            child = childShared;  // Stocke en weak pour éviter cycle
        }
        
        void AccessChild() {
            if (auto childShared = child.Lock(); childShared.IsValid()) {
                childShared->DoSomething();  // Enfant toujours vivant
            } else {
                // Enfant déjà détruit : gérer le cas
            }
        }
    };

    // Exemple 3 : Allocateur personnalisé pour pool mémoire
    void Example3() {
        nkentseu::memory::NkPoolAllocator pool(sizeof(MyClass), 50);
        
        auto shared = nkentseu::memory::NkMakeSharedWithAllocator<MyClass>(pool, arg1);
        // MyClass est alloué dans le pool, sera libéré dans le pool à la destruction
        // Le bloc de contrôle est aussi alloué dans le même pool (cohérence)
    }

    // Exemple 4 : Thread-safety du comptage (mais pas de l'accès à l'objet)
    std::vector<nkentseu::memory::NkSharedPtr<MyClass>> globalCache;
    nkentseu::core::NkMutex cacheMutex;  // Mutex de NKCore pour protection d'accès

    void ThreadSafeAdd(const nkentseu::memory::NkSharedPtr<MyClass>& item) {
        // Le shared_ptr peut être copié thread-safe (compteurs atomiques)
        // MAIS l'accès au conteneur global nécessite un mutex
        nkentseu::core::NkLockGuard lock(cacheMutex);
        globalCache.push_back(item);  // Copie thread-safe du shared_ptr
    }

    // Exemple 5 : Promotion weak->strong thread-safe
    nkentseu::memory::NkWeakPtr<MyClass> cachedWeak;

    void UseCached() {
        // Promotion thread-safe : si l'objet existe, on obtient un shared_ptr valide
        if (auto cached = cachedWeak.Lock(); cached.IsValid()) {
            cached->Process();  // Objet garanti vivant pendant cet appel
        }
        // Après ce bloc, l'objet peut être détruit si plus de références fortes
    }
*/

// =============================================================================
// COMPARAISON AVEC std::shared_ptr
// =============================================================================
/*
    | Feature                    | std::shared_ptr          | NkSharedPtr              | Avantage NKMemory |
    |---------------------------|-------------------------|-------------------------|-------------------|
    | Allocateur personnalisé   | Via alias constructor   | Via paramètre explicite | Plus simple, type-safe |
    | Compatibilité NKMemory    | Non                     | Oui (New/Delete)        | Intégration native |
    | Atomic operations         | std::atomic (standard)  | NkAtomicInt32 (NKCore)  | Contrôle plateforme, optimisations |
    | Bloc de contrôle          | Implémentation cachée   | Polymorphe, extensible  | Possibilité d'extensions futures |
    | WeakPtr support           | Oui                     | Oui + API explicite     | Lock() avec retour booléen clair |
    | Méthode IsValid()         | Non (operator bool)     | Oui + operator bool     | Plus explicite pour les audits |
    | Dépendances               | <memory> standard       | NKMemory/NKCore         | Contrôle total du build |

    Considérations de performance :
    - NkSharedPtr : 2 allocations (objet + bloc) vs std::shared_ptr (peut faire 1 avec make_shared)
    - Future optimisation : implémenter NkMakeShared avec allocation unique (objet+bloc contigus)
    - Comptage atomique : NkAtomicInt32 peut être optimisé par plateforme (lock-free garanties)

    Migration depuis std::shared_ptr :
    1. Remplacer #include <memory> par #include "NKMemory/NkSharedPtr.h"
    2. Remplacer std:: par nkentseu::memory::
    3. Pour les allocateurs : utiliser NkMakeSharedWithAllocator au lieu de alias constructor
    4. Vérifier que les types utilisés sont compatibles avec NKCore types (nk_int32, etc.)
    5. Tester la thread-safety : NkAtomicInt32 peut avoir des garanties différentes de std::atomic selon la plateforme
*/

// =============================================================================
// THREAD-SAFETY GUARANTEES
// =============================================================================
/*
    [Garanties thread-safe]
    ✓ Copie/assignation de NkSharedPtr/NkWeakPtr : compteurs atomiques
    ✓ Destruction : décrémentation atomique, destruction sérialisée par le dernier thread
    ✓ TryAddStrongRef() / Lock() : CAS loop pour promotion weak->strong sans race
    ✓ UseCount() / Expired() : lectures atomiques (snapshot, pas synchronisation)

    [Non garanties thread-safe - responsabilité de l'appelant]
    ✗ Accès concurrent à l'objet pointé via operator* ou operator->
    ✗ Modification de l'objet partagé sans synchronisation externe
    ✗ Itération sur des conteneurs de shared_ptr sans mutex

    [Bonnes pratiques]
    1. Utiliser mutex NKCore (NkMutex) pour protéger l'accès à l'objet partagé
    2. Privilégier la copie de shared_ptr (léger) plutôt que le partage d'accès
    3. Pour les caches : utiliser NkWeakPtr + Lock() pour éviter de garder l'objet vivant inutilement

    [Exemple de pattern thread-safe]
    class ThreadSafeResource {
        mutable nkentseu::core::NkMutex mMutex;
        mutable std::vector<int> mData;  // Donnée protégée
        
    public:
        void Add(int value) {
            nkentseu::core::NkLockGuard lock(mMutex);
            mData.push_back(value);
        }
        
        std::vector<int> Snapshot() const {
            nkentseu::core::NkLockGuard lock(mMutex);
            return mData;  // Copie thread-safe
        }
    };
    
    // Usage :
    auto resource = nkentseu::memory::NkMakeShared<ThreadSafeResource>();
    // resource peut être partagé entre threads : l'accès à mData est protégé par le mutex interne
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================