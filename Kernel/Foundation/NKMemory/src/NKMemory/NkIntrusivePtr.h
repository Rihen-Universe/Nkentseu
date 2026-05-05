// =============================================================================
// NKMemory/NkIntrusivePtr.h
// Smart pointer à référence comptée intrusive : l'objet gère son propre compteur.
//
// Design :
//  - Header-only : toutes les implémentations sont inline (requis pour templates)
//  - Réutilisation de NKCore/NkAtomic.h pour le comptage thread-safe (ZÉRO duplication)
//  - Plus léger que NkSharedPtr : pas de bloc de contrôle séparé, -1 allocation
//  - Idéal pour les objets qui DOIVENT être référencés (Entity, Component, Resource)
//  - API compatible avec les patterns de référence counting classiques
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKINTRUSIVEPTR_H
#define NKENTSEU_MEMORY_NKINTRUSIVEPTR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkAtomic.h"           // NkAtomicInt32 pour comptage thread-safe
    #include "NKCore/NkTraits.h"           // traits::NkForward, traits::NkMove
    #include "NKCore/NkTypes.h"            // nk_bool, nk_int32

    // -------------------------------------------------------------------------
    // SECTION 2 : CLASSE DE BASE À RÉFÉRENCE INTRUSIVE
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            /**
             * @class NkIntrusiveRefCounted
             * @brief Base pour objets avec référence comptée intrusive thread-safe
             * @ingroup MemorySmartPointers
             * 
             * Caractéristiques :
             *  - Le compteur de référence est stocké DANS l'objet lui-même (intrusif)
             *  - Thread-safe : opérations atomiques via NkAtomicInt32 (NKCore)
             *  - Destruction automatique via delete quand ref_count == 0
             *  - Plus léger que NkSharedPtr : pas d'allocation séparée pour le bloc de contrôle
             * 
             * @note Hériter publiquement de cette classe pour activer le référence counting
             * @note Le compteur est mutable : permet AddRef/Release sur objets const
             * @note Copy constructor réinitialise le compteur : chaque nouvelle instance commence à 0
             * 
             * @example
             * @code
             * class MyResource : public nkentseu::memory::NkIntrusiveRefCounted {
             * public:
             *     void Load() { /* ... *// }
             * };
             * 
             * // Usage avec NkIntrusivePtr
             * auto ptr = nkentseu::memory::NkMakeIntrusive<MyResource>();
             * ptr->Load();  // Accès via operator->
             * // Destruction automatique quand dernier ptr disparaît
             * @endcode
             */
            class NkIntrusiveRefCounted {
            public:
                // =================================================================
                // @name Constructeurs et règles de copie
                // =================================================================
                
                /**
                 * @brief Constructeur par défaut : compteur initialisé à 0
                 * @note L'objet n'est PAS possédé à la création : le premier NkIntrusivePtr
                 *       doit appeler AddRef() ou utiliser le constructeur avec addRef=true
                 */
                NkIntrusiveRefCounted() noexcept : mRefCount(0) {}

                /**
                 * @brief Copy constructor : réinitialise le compteur pour la nouvelle instance
                 * @note Une copie est un NOUVEL objet : son compteur commence à 0
                 * @note Ne copie PAS le compteur de l'original (sémantique de valeur)
                 */
                NkIntrusiveRefCounted(const NkIntrusiveRefCounted&) noexcept : mRefCount(0) {}

                /**
                 * @brief Copy assignment : no-op, retourne *this
                 * @note Le référence counting ne se "copie" pas : chaque objet gère son propre cycle
                 */
                NkIntrusiveRefCounted& operator=(const NkIntrusiveRefCounted&) noexcept { 
                    return *this; 
                }

                // =================================================================
                // @name Destructeur
                // =================================================================
                
                /**
                 * @brief Destructeur virtuel : requis pour destruction polymorphe sûre
                 * @note Protégé : la destruction doit passer par ReleaseRef(), jamais delete direct
                 */
                virtual ~NkIntrusiveRefCounted() = default;

                // =================================================================
                // @name Gestion des références (thread-safe)
                // =================================================================
                
                /**
                 * @brief Incrémente le compteur de références (thread-safe)
                 * @note Atomique : peut être appelé depuis n'importe quel thread
                 * @note Mutable : peut être appelé sur un objet const
                 */
                void AddRef() const noexcept { 
                    mRefCount.FetchAdd(1); 
                }

                /**
                 * @brief Décrémente le compteur et détruit l'objet si dernier propriétaire
                 * @note Si ref_count passe de 1 à 0 : appelle `delete this`
                 * @note Thread-safe : opération atomique avec mémoire ordering approprié
                 * @warning NE JAMAIS appeler delete directement sur un objet héritant de cette classe
                 * 
                 * @example
                 * @code
                 * MyObject* obj = new MyObject();
                 * obj->AddRef();      // ref_count = 1
                 * obj->ReleaseRef();  // ref_count = 0 -> delete this appelé automatiquement
                 * // NE PAS faire : delete obj; après ReleaseRef() !
                 * @endcode
                 */
                void ReleaseRef() const noexcept {
                    // FetchSub retourne la valeur AVANT la soustraction
                    if (mRefCount.FetchSub(1) == 1) {
                        delete this;  // Destruction automatique via polymorphisme
                    }
                }

                // =================================================================
                // @name Interrogation (thread-safe)
                // =================================================================
                
                /**
                 * @brief Retourne le nombre actuel de références vers cet objet
                 * @note Lecture atomique : valeur snapshot, peut changer juste après l'appel
                 * @note Utile pour le debugging, pas pour la logique métier (race conditions)
                 * @return Nombre de NkIntrusivePtr pointant vers cet objet
                 */
                [[nodiscard]] nk_int32 RefCount() const noexcept { 
                    return mRefCount.Load(); 
                }

            private:
                /** 
                 * @brief Compteur de références atomique
                 * @note Mutable : permet AddRef/Release sur objets const
                 * @note Thread-safe : toutes les opérations sont lock-free quand supporté
                 */
                mutable NkAtomicInt32 mRefCount;
            };

            // -------------------------------------------------------------------------
            // SECTION 3 : SMART POINTER INTRUSIF NkIntrusivePtr<T>
            // -------------------------------------------------------------------------
            /**
             * @class NkIntrusivePtr
             * @brief Smart pointer pour objets héritant de NkIntrusiveRefCounted
             * @tparam T Type de l'objet géré (doit hériter de NkIntrusiveRefCounted)
             * @ingroup MemorySmartPointers
             * 
             * Caractéristiques :
             *  - Ownership partagé via référence counting intrusif
             *  - Thread-safe : incréments/décréments atomiques du compteur
             *  - Plus léger que NkSharedPtr : pas de bloc de contrôle séparé
             *  - Compatible move-only et copyable selon les besoins
             *  - API familière : operator*, operator->, Get(), UseCount()
             * 
             * @note T DOIT hériter publiquement de NkIntrusiveRefCounted
             * @note La destruction de l'objet est gérée automatiquement via ReleaseRef()
             * @note noexcept sur toutes les opérations de transfert/destruction
             * 
             * @example
             * @code
             * class Entity : public nkentseu::memory::NkIntrusiveRefCounted {
             * public:
             *     void Update() { /* ... *// }
             * };
             * 
             * // Création via factory
             * auto entity = nkentseu::memory::NkMakeIntrusive<Entity>();
             * entity->Update();  // Accès direct via operator->
             * 
             * // Partage de référence
             * auto entity2 = entity;  // Copie : incrémente ref_count
             * // Les deux pointeurs partagent le même objet
             * 
             * // Transfert d'ownership
             * void TakeOwnership(nkentseu::memory::NkIntrusivePtr<Entity> ptr) {
             *     ptr->DoWork();
             * } // Destruction automatique à la sortie si dernier propriétaire
             * @endcode
             */
            template <typename T>
            class NkIntrusivePtr {
                // SFINAE-friendly : s'assure que T hérite bien de NkIntrusiveRefCounted
                static_assert(std::is_base_of<NkIntrusiveRefCounted, T>::value,
                    "NkIntrusivePtr<T> requires T to inherit from NkIntrusiveRefCounted");

            public:
                using element_type = T;  ///< Type de l'élément géré (compatibilité générique)

                // =================================================================
                // @name Constructeurs
                // =================================================================
                
                /** @brief Constructeur par défaut : pointeur nul */
                constexpr NkIntrusivePtr() noexcept : mPtr(nullptr) {}
                
                /** @brief Constructeur depuis nullptr (pour tests conditionnels) */
                constexpr NkIntrusivePtr(decltype(nullptr)) noexcept : mPtr(nullptr) {}

                /**
                 * @brief Constructeur depuis un pointeur brut
                 * @param ptr Pointeur vers l'objet à gérer (doit hériter de NkIntrusiveRefCounted)
                 * @param addRef Si true, appelle AddRef() sur ptr (défaut: true)
                 * @note Si addRef=false : suppose que l'appelant a déjà appelé AddRef()
                 * @note Utile pour NkMakeIntrusive qui gère déjà l'AddRef initial
                 */
                explicit NkIntrusivePtr(T* ptr, nk_bool addRef = true) noexcept 
                    : mPtr(ptr) {
                    if (mPtr && addRef) {
                        mPtr->AddRef();  // Thread-safe increment
                    }
                }

                /** @brief Constructeur de copie : incrémente le compteur de références */
                NkIntrusivePtr(const NkIntrusivePtr& other) noexcept : mPtr(other.mPtr) {
                    if (mPtr) {
                        mPtr->AddRef();  // Partage de référence
                    }
                }

                /** @brief Constructeur de mouvement : transfert d'ownership sans toucher au compteur */
                NkIntrusivePtr(NkIntrusivePtr&& other) noexcept : mPtr(other.mPtr) {
                    other.mPtr = nullptr;  // L'autre instance devient vide
                }

                // =================================================================
                // @name Destructeur
                // =================================================================
                
                /**
                 * @brief Destructeur : libère la référence si pointeur valide
                 * @note Appelle ReleaseRef() qui peut détruire l'objet si dernier propriétaire
                 * @note noexcept : la destruction ne doit jamais lever d'exception
                 */
                ~NkIntrusivePtr() noexcept {
                    if (mPtr) {
                        mPtr->ReleaseRef();  // Peut déclencher `delete this`
                    }
                }

                // =================================================================
                // @name Opérateurs d'assignation
                // =================================================================
                
                /**
                 * @brief Assignation par copie : gère proprement ancien et nouveau
                 * @note Utilise l'idiome copy-and-swap pour la sécurité exception
                 * @note Thread-safe : les incréments/décréments sont atomiques
                 */
                NkIntrusivePtr& operator=(const NkIntrusivePtr& other) noexcept {
                    if (this != &other) {
                        // Copy-and-swap : crée une copie temporaire, puis échange
                        NkIntrusivePtr(other).Swap(*this);
                    }
                    return *this;
                }

                /** @brief Assignation par mouvement : transfert efficace */
                NkIntrusivePtr& operator=(NkIntrusivePtr&& other) noexcept {
                    if (this != &other) {
                        NkIntrusivePtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                /** @brief Assignation depuis un pointeur brut */
                NkIntrusivePtr& operator=(T* ptr) noexcept {
                    NkIntrusivePtr(ptr).Swap(*this);
                    return *this;
                }

                /** @brief Assignation depuis nullptr : équivaut à Reset() */
                NkIntrusivePtr& operator=(decltype(nullptr)) noexcept {
                    Reset();
                    return *this;
                }

                // =================================================================
                // @name Gestion manuelle du cycle de vie
                // =================================================================
                
                /**
                 * @brief Réinitialise avec un nouveau pointeur
                 * @param ptr Nouveau pointeur à gérer (défaut: nullptr)
                 * @param addRef Si true, appelle AddRef() sur le nouveau ptr
                 * @note Libère l'ancienne référence avant d'adopter la nouvelle
                 */
                void Reset(T* ptr = nullptr, nk_bool addRef = true) noexcept {
                    NkIntrusivePtr(ptr, addRef).Swap(*this);
                }

                /**
                 * @brief Libère l'ownership sans détruire l'objet
                 * @return Pointeur brut précédemment géré (ou nullptr)
                 * @note Après appel, ce NkIntrusivePtr est vide
                 * @note La responsabilité de ReleaseRef() revient à l'appelant
                 * 
                 * @warning Utilisez avec précaution : risque de fuite si ReleaseRef() oublié
                 */
                [[nodiscard]] T* Release() noexcept {
                    T* ptr = mPtr;
                    mPtr = nullptr;
                    return ptr;
                }

                /**
                 * @brief Échange le contenu avec un autre NkIntrusivePtr
                 * @param other Instance avec laquelle échanger
                 * @note noexcept : opération sûre même en contexte d'exception
                 * @note N'affecte pas les compteurs de référence (juste échange de pointeurs)
                 */
                void Swap(NkIntrusivePtr& other) noexcept {
                    T* tmp = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmp;
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
                
                /** @brief Conversion explicite en booléen pour tests conditionnels */
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }
                
                /**
                 * @brief Retourne le nombre de références vers l'objet géré
                 * @note Thread-safe : lecture atomique du compteur dans l'objet
                 * @note Retourne 0 si ce pointeur est vide
                 * @note Valeur snapshot : peut changer juste après l'appel
                 */
                [[nodiscard]] nk_int32 UseCount() const noexcept { 
                    return mPtr ? mPtr->RefCount() : 0; 
                }

            private:
                T* mPtr;  ///< Pointeur vers l'objet géré (non-possédé : référence partagée)
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : FONCTION FABRIQUE NkMakeIntrusive
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryIntrusiveFactories Fonctions de Création NkIntrusivePtr
             * @brief Helpers pour construire des NkIntrusivePtr de manière sûre
             */

            /**
             * @brief Crée un NkIntrusivePtr<T> avec construction in-place
             * @tparam T Type de l'objet à créer (doit hériter de NkIntrusiveRefCounted)
             * @tparam Args Types des arguments du constructeur de T
             * @param args Arguments forwardés au constructeur de T
             * @return NkIntrusivePtr<T> prenant ownership de l'objet nouvellement créé
             * @ingroup MemoryIntrusiveFactories
             * 
             * @note Utilise `new T(...)` directement : pas d'allocateur personnalisé
             * @note Exception-safe : si le constructeur lève, pas de fuite mémoire
             * @note L'objet est créé avec ref_count=0, puis AddRef() appelé avant retour
             * @note Préférer cette fonction à la construction manuelle pour la sécurité
             * 
             * @example
             * @code
             * class Player : public nkentseu::memory::NkIntrusiveRefCounted {
             * public:
             *     Player(const std::string& name, int level) 
             *         : mName(name), mLevel(level) {}
             * };
             * 
             * auto player = nkentseu::memory::NkMakeIntrusive<Player>("Hero", 42);
             * if (player.IsValid()) {
             *     printf("Player: %s (level %d)\n", player->GetName(), player->GetLevel());
             * }
             * // Destruction automatique quand dernier NkIntrusivePtr disparaît
             * @endcode
             */
            template <typename T, typename... Args>
            [[nodiscard]] NkIntrusivePtr<T> NkMakeIntrusive(Args&&... args) {
                // Allocation directe via new (pas d'allocateur personnalisé pour intrusive)
                T* object = new T(traits::NkForward<Args>(args)...);
                
                if (object) {
                    // L'objet vient d'être créé : ref_count = 0
                    // On appelle AddRef() pour le premier propriétaire (le NkIntrusivePtr retourné)
                    object->AddRef();
                    
                    // Constructeur avec addRef=false car on a déjà appelé AddRef() manuellement
                    // Cela évite un double increment
                    return NkIntrusivePtr<T>(object, false);
                }
                
                // Si new échoue (retourne nullptr en mode no-exception), retourne pointeur vide
                return NkIntrusivePtr<T>(nullptr);
            }

            // -------------------------------------------------------------------------
            // SECTION 5 : HELPERS UTILITAIRES
            // -------------------------------------------------------------------------
            /**
             * @brief Compare deux NkIntrusivePtr pour égalité de pointeur
             * @tparam T Type de l'objet géré
             * @param a Premier pointeur
             * @param b Deuxième pointeur
             * @return true si les deux pointent vers le même objet (ou sont tous deux nullptr)
             * @ingroup MemoryIntrusiveFactories
             */
            template <typename T>
            [[nodiscard]] nk_bool operator==(const NkIntrusivePtr<T>& a, 
                                            const NkIntrusivePtr<T>& b) noexcept {
                return a.Get() == b.Get();
            }

            /** @brief Compare pour inégalité */
            template <typename T>
            [[nodiscard]] nk_bool operator!=(const NkIntrusivePtr<T>& a, 
                                            const NkIntrusivePtr<T>& b) noexcept {
                return !(a == b);
            }

            /**
             * @brief Compare un NkIntrusivePtr avec un pointeur brut
             * @tparam T Type de l'objet géré
             */
            template <typename T>
            [[nodiscard]] nk_bool operator==(const NkIntrusivePtr<T>& ptr, T* raw) noexcept {
                return ptr.Get() == raw;
            }

            template <typename T>
            [[nodiscard]] nk_bool operator==(T* raw, const NkIntrusivePtr<T>& ptr) noexcept {
                return raw == ptr.Get();
            }

            template <typename T>
            [[nodiscard]] nk_bool operator!=(const NkIntrusivePtr<T>& ptr, T* raw) noexcept {
                return !(ptr == raw);
            }

            template <typename T>
            [[nodiscard]] nk_bool operator!=(T* raw, const NkIntrusivePtr<T>& ptr) noexcept {
                return !(raw == ptr);
            }

            /**
             * @brief Compare avec nullptr
             * @tparam T Type de l'objet géré
             */
            template <typename T>
            [[nodiscard]] nk_bool operator==(const NkIntrusivePtr<T>& ptr, decltype(nullptr)) noexcept {
                return ptr.Get() == nullptr;
            }

            template <typename T>
            [[nodiscard]] nk_bool operator==(decltype(nullptr), const NkIntrusivePtr<T>& ptr) noexcept {
                return nullptr == ptr.Get();
            }

            template <typename T>
            [[nodiscard]] nk_bool operator!=(const NkIntrusivePtr<T>& ptr, decltype(nullptr)) noexcept {
                return !(ptr == nullptr);
            }

            template <typename T>
            [[nodiscard]] nk_bool operator!=(decltype(nullptr), const NkIntrusivePtr<T>& ptr) noexcept {
                return !(nullptr == ptr);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKINTRUSIVEPTR_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Classe de base avec référence counting
    class Resource : public nkentseu::memory::NkIntrusiveRefCounted {
    public:
        Resource(const char* name) : mName(name) {}
        void Load() { /* chargement ... *// }
        const char* GetName() const { return mName; }
    private:
        const char* mName;
    };

    // Exemple 2 : Usage basique avec factory
    void Example1() {
        auto res = nkentseu::memory::NkMakeIntrusive<Resource>("texture.png");
        res->Load();
        printf("Loaded: %s (refs=%d)\n", res->GetName(), res.UseCount());  // refs=1
    }  // Destruction automatique : ReleaseRef() -> delete si dernier

    // Exemple 3 : Partage de référence
    void Example2() {
        auto res1 = nkentseu::memory::NkMakeIntrusive<Resource>("data.bin");
        {
            auto res2 = res1;  // Copie : incrémente ref_count à 2
            printf("Refs: %d\n", res1.UseCount());  // Affiche 2
        }  // res2 détruit : ref_count retourne à 1, objet toujours vivant
        printf("Refs: %d\n", res1.UseCount());  // Affiche 1
    }  // res1 détruit : ref_count=0 -> objet détruit

    // Exemple 4 : Stockage dans conteneurs
    std::vector<nkentseu::memory::NkIntrusivePtr<Resource>> resources;
    
    void AddResource(const char* name) {
        resources.push_back(nkentseu::memory::NkMakeIntrusive<Resource>(name));
        // Le shared_ptr est copié dans le vector : ref_count géré automatiquement
    }
    
    void ProcessAll() {
        for (const auto& res : resources) {
            if (res.IsValid()) {
                res->Load();  // Accès via operator->
            }
        }
    }

    // Exemple 5 : Release pour transfert vers API C ou gestion manuelle
    void Example5() {
        auto ptr = nkentseu::memory::NkMakeIntrusive<Resource>("raw.dat");
        
        // Transfert vers une API C qui prend ownership
        C_API_TakeResource(ptr.Release());  // ptr est maintenant nullptr
        // L'API C doit appeler ReleaseRef() quand elle a fini
        
        // OU : reprise de contrôle manuel
        auto* raw = ptr.Release();
        if (raw) {
            raw->Load();  // Utilisation manuelle
            raw->ReleaseRef();  // Libération explicite (obligatoire !)
        }
    }

    // Exemple 6 : Héritage polymorphe
    class Texture : public Resource {
    public:
        Texture(const char* path, int width, int height) 
            : Resource(path), mWidth(width), mHeight(height) {}
        void UploadToGPU() { /* ... *// }
    private:
        int mWidth, mHeight;
    };

    void Example6() {
        nkentseu::memory::NkIntrusivePtr<Resource> base = 
            nkentseu::memory::NkMakeIntrusive<Texture>("icon.png", 256, 256);
        
        // Downcast safe via dynamic_cast
        if (auto* tex = dynamic_cast<Texture*>(base.Get())) {
            tex->UploadToGPU();  // Accès aux méthodes spécifiques
        }
    }
*/

// =============================================================================
// COMPARAISON : NkIntrusivePtr vs NkSharedPtr
// =============================================================================
/*
    | Critère                    | NkIntrusivePtr<T>              | NkSharedPtr<T>              |
    |---------------------------|--------------------------------|-----------------------------|
    | Héritage requis           | Oui : T doit hériter de base  | Non : fonctionne avec tout T |
    | Allocations               | 1 (l'objet lui-même)          | 2 (objet + bloc de contrôle)|
    | Taille mémoire            | +4/8 bytes dans l'objet       | Bloc séparé (~24-32 bytes)  |
    | Performance               | Légèrement plus rapide        | Légèrement plus lent        |
    | Compatibilité types       | Types "maison" uniquement     | Types externes, STL, etc.   |
    | Cycle de références       | Possible (nécessite weak)     | Possible avec NkWeakPtr     |
    | Thread-safety comptage    | Oui (via NkAtomicInt32)       | Oui (via NkAtomicInt32)     |
    | Flexibilité allocateur    | Non (toujours `new`)          | Oui (via NkAllocator)       |

    Quand utiliser NkIntrusivePtr :
    ✓ Vous contrôlez la définition de la classe T
    ✓ Performance critique : éviter l'allocation supplémentaire du bloc de contrôle
    ✓ Objets fréquemment partagés dans un sous-système fermé (ex: ECS, Render Graph)
    ✓ Vous voulez que le compteur fasse partie de l'objet (debug, inspection)

    Quand utiliser NkSharedPtr :
    ✓ T est une classe externe que vous ne pouvez pas modifier
    ✓ Vous avez besoin d'allocateurs personnalisés (pools, arenas)
    ✓ Vous voulez NkWeakPtr sans modifier la classe de base
    ✓ Flexibilité maximale au prix d'un léger surcoût

    Migration NkSharedPtr -> NkIntrusivePtr :
    1. Faire hériter T de NkIntrusiveRefCounted
    2. Remplacer std::shared_ptr<T> par nkentseu::memory::NkIntrusivePtr<T>
    3. Remplacer std::make_shared<T> par nkentseu::memory::NkMakeIntrusive<T>
    4. Supprimer les custom deleters (gérés automatiquement)
    5. Tester : vérifier qu'aucun delete direct n'est fait sur les objets T
*/

// =============================================================================
// THREAD-SAFETY GUARANTEES
// =============================================================================
/*
    [Garanties thread-safe]
    ✓ AddRef() / ReleaseRef() : opérations atomiques via NkAtomicInt32
    ✓ Copy/move de NkIntrusivePtr : manipulation de pointeur + atomiques pour compteur
    ✓ RefCount() / UseCount() : lectures atomiques (snapshot)
    ✓ Destruction : sérialisée par le dernier thread qui décrémente à 0

    [Non garanties thread-safe - responsabilité de l'appelant]
    ✗ Accès concurrent à l'objet pointé via operator* ou operator->
    ✗ Modification de l'objet partagé sans synchronisation externe
    ✗ Itération sur des conteneurs de NkIntrusivePtr sans mutex

    [Bonnes pratiques]
    1. Utiliser mutex NKCore (NkMutex) pour protéger l'accès à l'objet partagé
    2. Privilégier la copie de NkIntrusivePtr (léger) plutôt que le partage d'accès
    3. Pour les caches : envisager un système de weak references si disponible

    [Exemple de pattern thread-safe]
    class ThreadSafeResource : public nkentseu::memory::NkIntrusiveRefCounted {
        mutable nkentseu::core::NkMutex mMutex;
        std::vector<int> mData;
        
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
    auto res = nkentseu::memory::NkMakeIntrusive<ThreadSafeResource>();
    // res peut être partagé entre threads : l'accès à mData est protégé par le mutex interne
*/

// =============================================================================
// DEBUGGING TIPS
// =============================================================================
/*
    [Fuites mémoire avec NkIntrusivePtr]
    
    Symptôme : objets jamais détruits, ref_count > 0 à la fermeture
    
    Causes courantes :
    1. Oubli de ReleaseRef() après Release()
       Solution : Toujours appeler ReleaseRef() si vous avez appelé Release()
       
    2. Cycle de références : A possède B, B possède A
       Solution : Utiliser NkIntrusivePtr pour un sens, raw pointer ou weak pour l'autre
       
    3. Pointeur brut conservé après que le dernier NkIntrusivePtr a disparu
       Solution : Ne jamais conserver de raw pointer long terme, toujours utiliser NkIntrusivePtr

    [Debugging du ref_count]
    
    En mode debug, vous pouvez logger le compteur :
    
        #if defined(NKENTSEU_DEBUG)
            NK_FOUNDATION_LOG_DEBUG("Object %p ref_count=%d", 
                                   static_cast<void*>(this), 
                                   RefCount());
        #endif
    
    Ou ajouter une méthode de dump :
    
        void DumpRefCount(const char* context) const {
            NK_FOUNDATION_LOG_INFO("[%s] %p ref_count=%d", 
                                  context, 
                                  static_cast<const void*>(this),
                                  RefCount());
        }

    [Assertions de sécurité]
    
    Pour catcher les erreurs tôt en debug :
    
        #if defined(NKENTSEU_DEBUG)
            ~NkIntrusiveRefCounted() {
                NK_ASSERT_MSG(mRefCount.Load() == 0, 
                             "Destruction avec ref_count != 0 : fuite ou double-free");
            }
        #endif
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================