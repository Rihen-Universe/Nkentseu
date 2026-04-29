// =============================================================================
// NKMemory/NkUniquePtr.h
// Smart pointer unique ownership avec support d'allocateur personnalisé.
//
// Design :
//  - Header-only : toutes les implémentations sont inline (requis pour templates)
//  - Réutilisation des allocateurs NKMemory (ZÉRO duplication de logique)
//  - API compatible std::unique_ptr avec extensions NKMemory
//  - Support explicite pour tableaux (T[]) avec DeleteArray
//  - noexcept sur toutes les opérations de destruction/transfert
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKUNIQUEPTR_H
#define NKENTSEU_MEMORY_NKUNIQUEPTR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkAllocator.h"      // NkAllocator, New<T>, Delete<T>
    #include "NKCore/NkTraits.h"           // traits::NkForward, traits::NkMove
    #include "NKCore/NkTypes.h"            // nk_bool, nk_size

    // -------------------------------------------------------------------------
    // SECTION 2 : DELETERS PAR DÉFAUT
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            /**
             * @struct NkDefaultDelete
             * @brief Deleter par défaut pour objets uniques (non-tableaux)
             * @tparam T Type de l'objet géré
             * @ingroup MemorySmartPointers
             * 
             * Appelle NkAllocator::Delete<T>() pour destruction + deallocation.
             * Utilise l'allocateur fourni ou l'allocateur par défaut.
             */
            template <typename T>
            struct NkDefaultDelete {
                NkAllocator* allocator;  ///< Allocateur à utiliser (nullptr = défaut)

                /**
                 * @brief Constructeur avec allocateur optionnel
                 * @param alloc Pointeur vers l'allocateur (peut être nullptr)
                 */
                explicit NkDefaultDelete(NkAllocator* alloc = nullptr) noexcept 
                    : allocator(alloc) {}

                /**
                 * @brief Opérateur d'appel : détruit et libère le pointeur
                 * @param ptr Pointeur à libérer (nullptr accepté, no-op)
                 * 
                 * @note Appelle ~T() puis deallocate via l'allocateur
                 * @note noexcept : garantit pas d'exception pendant la destruction
                 */
                void operator()(T* ptr) const noexcept {
                    if (!ptr) {
                        return;  // Safe : no-op sur nullptr
                    }
                    NkAllocator* alloc = allocator ? allocator : &NkGetDefaultAllocator();
                    alloc->Delete(ptr);  // Delete<T> : destructeur + deallocate
                }
            };

            /**
             * @struct NkDefaultDelete<T[]>
             * @brief Spécialisation du deleter pour tableaux
             * @tparam T Type des éléments du tableau
             * @ingroup MemorySmartPointers
             * 
             * Appelle NkAllocator::DeleteArray<T>() pour destruction élément par élément.
             * Gère automatiquement l'header interne stocké par NewArray.
             */
            template <typename T>
            struct NkDefaultDelete<T[]> {
                NkAllocator* allocator;

                explicit NkDefaultDelete(NkAllocator* alloc = nullptr) noexcept 
                    : allocator(alloc) {}

                /**
                 * @brief Détruit un tableau élément par élément puis libère
                 * @param ptr Pointeur vers le premier élément du tableau
                 * 
                 * @note Utilise DeleteArray<T> qui lit l'header interne pour le count
                 * @note L'ordre de destruction est inverse de la construction (bonne pratique)
                 */
                void operator()(T* ptr) const noexcept {
                    if (!ptr) {
                        return;
                    }
                    NkAllocator* alloc = allocator ? allocator : &NkGetDefaultAllocator();
                    alloc->DeleteArray(ptr);  // DeleteArray<T> : boucle de destructeurs + free
                }
            };

            // -------------------------------------------------------------------------
            // SECTION 3 : CLASSE PRINCIPALE NkUniquePtr (objet unique)
            // -------------------------------------------------------------------------
            /**
             * @class NkUniquePtr
             * @brief Smart pointer à ownership unique avec allocateur personnalisé
             * @tparam T Type de l'objet géré
             * @tparam Deleter Type du foncteur de destruction (défaut: NkDefaultDelete<T>)
             * @ingroup MemorySmartPointers
             * 
             * Caractéristiques :
             *  - Ownership exclusif : non-copyable, movable uniquement
             *  - Destruction automatique via deleter à la fin de portée
             *  - Support d'allocateur personnalisé via le deleter
             *  - API compatible avec std::unique_ptr pour migration facile
             *  - noexcept sur toutes les opérations de transfert/destruction
             * 
             * @note Pour les tableaux, utiliser la spécialisation NkUniquePtr<T[]>
             * @note Le deleter est stocké par valeur : préférer les types stateless ou petits
             */
            template <typename T, typename Deleter = NkDefaultDelete<T>>
            class NkUniquePtr {
            public:
                // Types standards pour compatibilité générique
                using element_type = T;
                using pointer      = T*;
                using deleter_type = Deleter;

                // =================================================================
                // @name Constructeurs
                // =================================================================
                
                /** @brief Constructeur par défaut : pointeur nul */
                constexpr NkUniquePtr() noexcept : mPtr(nullptr), mDeleter() {}

                /**
                 * @brief Constructeur explicite depuis un pointeur brut
                 * @param ptr Pointeur à prendre en ownership (peut être nullptr)
                 * @note Le deleter par défaut est utilisé (allocateur = nullptr)
                 */
                explicit NkUniquePtr(pointer ptr) noexcept 
                    : mPtr(ptr), mDeleter() {}

                /**
                 * @brief Constructeur avec deleter personnalisé
                 * @param ptr Pointeur à prendre en ownership
                 * @param deleter Instance du foncteur de destruction à utiliser
                 */
                NkUniquePtr(pointer ptr, const Deleter& deleter) noexcept 
                    : mPtr(ptr), mDeleter(deleter) {}

                // =================================================================
                // @name Règles de mouvement (non-copyable)
                // =================================================================
                
                /** @brief Constructeur de mouvement */
                NkUniquePtr(NkUniquePtr&& other) noexcept
                    : mPtr(other.Release()), mDeleter(static_cast<Deleter&&>(other.mDeleter)) {}

                /** @brief Opérateur d'assignation par mouvement */
                NkUniquePtr& operator=(NkUniquePtr&& other) noexcept {
                    if (this != &other) {
                        Reset(other.Release());
                        mDeleter = static_cast<Deleter&&>(other.mDeleter);
                    }
                    return *this;
                }

                // Suppression des copies : ownership exclusif
                NkUniquePtr(const NkUniquePtr&) = delete;
                NkUniquePtr& operator=(const NkUniquePtr&) = delete;

                // =================================================================
                // @name Destructeur
                // =================================================================
                
                /**
                 * @brief Destructeur : libère automatiquement la ressource
                 * @note Appelle Reset() qui invoque le deleter si mPtr != nullptr
                 * @note noexcept : la destruction ne doit jamais lever d'exception
                 */
                ~NkUniquePtr() noexcept { Reset(); }

                // =================================================================
                // @name Accès et interrogation
                // =================================================================
                
                /** @brief Retourne le pointeur brut géré (peut être nullptr) */
                [[nodiscard]] pointer Get() const noexcept { return mPtr; }

                /** @brief Retourne une référence au deleter (pour inspection/modification) */
                [[nodiscard]] Deleter& GetDeleter() noexcept { return mDeleter; }
                [[nodiscard]] const Deleter& GetDeleter() const noexcept { return mDeleter; }

                /** @brief Teste si le pointeur est valide (non-null) */
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }

                /** @brief Conversion explicite en booléen pour tests conditionnels */
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

                /** @brief Déférencement : accès à l'objet pointé */
                [[nodiscard]] element_type& operator*() const noexcept { return *mPtr; }
                
                /** @brief Accès membre : équivalent à Get()->member */
                [[nodiscard]] pointer operator->() const noexcept { return mPtr; }

                // =================================================================
                // @name Gestion manuelle du cycle de vie
                // =================================================================
                
                /**
                 * @brief Libère l'ownership sans détruire l'objet
                 * @return Pointeur brut précédemment géré (ou nullptr)
                 * @note Après appel, ce NkUniquePtr est vide (Get() == nullptr)
                 * @note La responsabilité de libération revient à l'appelant
                 */
                [[nodiscard]] pointer Release() noexcept {
                    pointer old = mPtr;
                    mPtr = nullptr;
                    return old;
                }

                /**
                 * @brief Remplace l'objet géré, détruisant l'ancien si présent
                 * @param ptr Nouveau pointeur à gérer (défaut: nullptr pour simple reset)
                 * @note Si un objet était déjà géré, le deleter est invoqué avant remplacement
                 */
                void Reset(pointer ptr = nullptr) noexcept {
                    pointer old = mPtr;
                    mPtr = ptr;
                    if (old) {
                        mDeleter(old);  // Destruction via le deleter configuré
                    }
                }

                /**
                 * @brief Échange le contenu avec un autre NkUniquePtr
                 * @param other Autre instance avec laquelle échanger
                 * @note noexcept : opération sûre même en cas d'exception ailleurs
                 */
                void Swap(NkUniquePtr& other) noexcept {
                    // Échange des pointeurs
                    pointer tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    // Échange des deleters (peut avoir un état)
                    Deleter tmpDel = static_cast<Deleter&&>(mDeleter);
                    mDeleter = static_cast<Deleter&&>(other.mDeleter);
                    other.mDeleter = static_cast<Deleter&&>(tmpDel);
                }

            private:
                pointer  mPtr;      ///< Pointeur vers l'objet géré
                Deleter  mDeleter;  ///< Foncteur de destruction (peut contenir l'allocateur)
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : SPÉCIALISATION POUR TABLEAUX NkUniquePtr<T[]>
            // -------------------------------------------------------------------------
            /**
             * @class NkUniquePtr<T[]>
             * @brief Spécialisation pour la gestion de tableaux dynamiques
             * @tparam T Type des éléments du tableau
             * @tparam Deleter Type du foncteur de destruction
             * @ingroup MemorySmartPointers
             * 
             * Différences avec la version objet unique :
             *  - operator[] au lieu de operator* et operator->
             *  - Utilise NkDefaultDelete<T[]> qui appelle DeleteArray
             *  - Gère automatiquement l'header interne de NewArray
             * 
             * @note Ne pas utiliser pour des tableaux de taille fixe connue à la compilation
             * @note Pour les tableaux, préférer NkMakeUniqueArray<T>() pour la construction
             */
            template <typename T, typename Deleter>
            class NkUniquePtr<T[], Deleter> {
            public:
                using element_type = T;
                using pointer      = T*;
                using deleter_type = Deleter;

                // =================================================================
                // @name Constructeurs (identiques à la version non-tableau)
                // =================================================================
                constexpr NkUniquePtr() noexcept : mPtr(nullptr), mDeleter() {}
                explicit NkUniquePtr(pointer ptr) noexcept : mPtr(ptr), mDeleter() {}
                NkUniquePtr(pointer ptr, const Deleter& deleter) noexcept 
                    : mPtr(ptr), mDeleter(deleter) {}

                NkUniquePtr(NkUniquePtr&& other) noexcept
                    : mPtr(other.Release()), mDeleter(static_cast<Deleter&&>(other.mDeleter)) {}

                NkUniquePtr& operator=(NkUniquePtr&& other) noexcept {
                    if (this != &other) {
                        Reset(other.Release());
                        mDeleter = static_cast<Deleter&&>(other.mDeleter);
                    }
                    return *this;
                }

                ~NkUniquePtr() noexcept { Reset(); }

                NkUniquePtr(const NkUniquePtr&) = delete;
                NkUniquePtr& operator=(const NkUniquePtr&) = delete;

                // =================================================================
                // @name Accès spécifiques aux tableaux
                // =================================================================
                [[nodiscard]] pointer Get() const noexcept { return mPtr; }
                [[nodiscard]] Deleter& GetDeleter() noexcept { return mDeleter; }
                [[nodiscard]] const Deleter& GetDeleter() const noexcept { return mDeleter; }
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

                /**
                 * @brief Accès par index aux éléments du tableau
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence vers l'élément à l'index spécifié
                 * @note Aucune vérification de bounds : responsabilité de l'appelant
                 */
                [[nodiscard]] element_type& operator[](nk_size index) const noexcept { 
                    return mPtr[index]; 
                }

                // =================================================================
                // @name Gestion du cycle de vie (identique à la version non-tableau)
                // =================================================================
                [[nodiscard]] pointer Release() noexcept {
                    pointer old = mPtr;
                    mPtr = nullptr;
                    return old;
                }

                void Reset(pointer ptr = nullptr) noexcept {
                    pointer old = mPtr;
                    mPtr = ptr;
                    if (old) {
                        mDeleter(old);  // Appelle DeleteArray<T> via NkDefaultDelete<T[]>
                    }
                }

                void Swap(NkUniquePtr& other) noexcept {
                    pointer tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    Deleter tmpDel = static_cast<Deleter&&>(mDeleter);
                    mDeleter = static_cast<Deleter&&>(other.mDeleter);
                    other.mDeleter = static_cast<Deleter&&>(tmpDel);
                }

            private:
                pointer  mPtr;
                Deleter  mDeleter;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : FONCTIONS FABRIQUE (FACTORY FUNCTIONS)
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryUniquePtrFactories Fonctions de Création NkUniquePtr
             * @brief Helpers pour construire des NkUniquePtr de manière sûre et expressive
             */

            /**
             * @brief Crée un NkUniquePtr<T> avec construction in-place
             * @tparam T Type de l'objet à créer
             * @tparam Args Types des arguments du constructeur de T
             * @param args Arguments forwardés au constructeur de T
             * @return NkUniquePtr<T> prenant ownership de l'objet nouvellement créé
             * @ingroup MemoryUniquePtrFactories
             * 
             * @note Utilise l'allocateur par défaut via NkGetDefaultAllocator()
             * @note Exception-safe : si New<T> lève, pas de fuite mémoire
             * @note Préférer cette fonction à la construction manuelle pour la sécurité
             * 
             * @example
             * @code
             * auto obj = NkMakeUnique<MyClass>(arg1, arg2);  // MyClass(arg1, arg2)
             * if (obj.IsValid()) {
             *     obj->DoSomething();
             * }
             * // Destruction automatique à la fin de portée
             * @endcode
             */
            template <typename T, typename... Args>
            [[nodiscard]] NkUniquePtr<T> NkMakeUnique(Args&&... args) {
                NkAllocator& alloc = NkGetDefaultAllocator();
                T* object = alloc.New<T>(traits::NkForward<Args>(args)...);
                return NkUniquePtr<T>(object, NkDefaultDelete<T>(&alloc));
            }

            /**
             * @brief Crée un NkUniquePtr<T> avec allocateur personnalisé
             * @tparam T Type de l'objet à créer
             * @tparam Args Types des arguments du constructeur
             * @param allocator Allocateur à utiliser pour l'allocation/deallocation
             * @param args Arguments forwardés au constructeur de T
             * @return NkUniquePtr<T> configuré avec l'allocateur spécifié
             * @ingroup MemoryUniquePtrFactories
             * 
             * @note L'allocateur est capturé par le deleter pour la deallocation future
             * @note Essentiel quand l'objet doit être libéré dans un pool/arena spécifique
             */
            template <typename T, typename... Args>
            [[nodiscard]] NkUniquePtr<T> NkMakeUniqueWithAllocator(NkAllocator& allocator, Args&&... args) {
                T* object = allocator.New<T>(traits::NkForward<Args>(args)...);
                return NkUniquePtr<T>(object, NkDefaultDelete<T>(&allocator));
            }

            /**
             * @brief Crée un NkUniquePtr<T[]> pour un tableau d'objets
             * @tparam T Type des éléments du tableau
             * @param count Nombre d'éléments à allouer
             * @return NkUniquePtr<T[]> gérant le tableau nouvellement créé
             * @ingroup MemoryUniquePtrFactories
             * 
             * @note Chaque élément est construit avec le constructeur par défaut de T
             * @note Pour construire avec arguments, utiliser NewArray<T>(count, args...) manuellement
             * @note Utilise l'allocateur par défaut
             * 
             * @example
             * @code
             * auto array = NkMakeUniqueArray<int>(100);  // Tableau de 100 ints
             * for (nk_size i = 0; i < 100; ++i) {
             *     array[i] = static_cast<int>(i);
             * }
             * @endcode
             */
            template <typename T>
            [[nodiscard]] NkUniquePtr<T[]> NkMakeUniqueArray(nk_size count) {
                NkAllocator& alloc = NkGetDefaultAllocator();
                T* object = alloc.NewArray<T>(count);
                return NkUniquePtr<T[]>(object, NkDefaultDelete<T[]>(&alloc));
            }

            /**
             * @brief Crée un NkUniquePtr<T[]> avec allocateur personnalisé
             * @tparam T Type des éléments du tableau
             * @param allocator Allocateur à utiliser
             * @param count Nombre d'éléments à allouer
             * @return NkUniquePtr<T[]> configuré avec l'allocateur spécifié
             * @ingroup MemoryUniquePtrFactories
             */
            template <typename T>
            [[nodiscard]] NkUniquePtr<T[]> NkMakeUniqueArrayWithAllocator(NkAllocator& allocator, nk_size count) {
                T* object = allocator.NewArray<T>(count);
                return NkUniquePtr<T[]>(object, NkDefaultDelete<T[]>(&allocator));
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKUNIQUEPTR_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Usage basique avec allocateur par défaut
    void Example1() {
        auto ptr = nkentseu::memory::NkMakeUnique<std::string>("Hello");
        if (ptr.IsValid()) {
            printf("%s\n", ptr->c_str());  // Affiche "Hello"
        }
        // Destruction automatique : ~string() + deallocate
    }

    // Exemple 2 : Allocateur personnalisé (ex: pool allocator)
    void Example2() {
        nkentseu::memory::NkPoolAllocator pool(64, 100);  // Pool de 100 objets de 64B
        
        auto obj = nkentseu::memory::NkMakeUniqueWithAllocator<MyClass>(pool, arg1, arg2);
        // MyClass est alloué dans le pool, sera libéré dans le pool à la destruction
    }

    // Exemple 3 : Transfert d'ownership (move semantics)
    nkentseu::memory::NkUniquePtr<MyClass> CreateObject() {
        return nkentseu::memory::NkMakeUnique<MyClass>(/\* args *\/);
    }

    void ConsumeObject(nkentseu::memory::NkUniquePtr<MyClass> ptr) {
        ptr->DoWork();
        // Destruction à la sortie de la fonction
    }

    void Example3() {
        ConsumeObject(CreateObject());  // Transfert direct, pas de copie
    }

    // Exemple 4 : Gestion de tableau
    void Example4() {
        auto array = nkentseu::memory::NkMakeUniqueArray<float>(256);
        for (nk_size i = 0; i < 256; ++i) {
            array[i] = static_cast<float>(i) * 0.5f;
        }
        // Destruction : appel des destructeurs (no-op pour float) + deallocate
    }

    // Exemple 5 : Release pour transfert vers API C ou gestion manuelle
    void Example5() {
        auto ptr = nkentseu::memory::NkMakeUnique<Resource>();
        
        // Transfert vers une API C qui prend ownership
        C_API_TakeOwnership(ptr.Release());  // ptr est maintenant nullptr
        
        // OU : reprise de contrôle manuel
        auto* raw = ptr.Release();
        if (raw) {
            // ... utilisation manuelle ...
            nkentseu::memory::NkGetDefaultAllocator().Delete(raw);  // Libération explicite
        }
    }

    // Exemple 6 : Deleter personnalisé pour ressources non-mémoire
    struct FileDeleter {
        void operator()(FILE* f) const noexcept {
            if (f) {
                fclose(f);  // Fermeture de fichier, pas de deallocation mémoire
            }
        }
    };

    void Example6() {
        FILE* f = fopen("data.txt", "r");
        nkentseu::memory::NkUniquePtr<FILE, FileDeleter> filePtr(f, FileDeleter());
        // fclose appelé automatiquement à la destruction, même en cas d'exception
    }
*/

// =============================================================================
// COMPARAISON AVEC std::unique_ptr
// =============================================================================
/*
    | Feature                    | std::unique_ptr          | NkUniquePtr              | Avantage NKMemory |
    |---------------------------|-------------------------|-------------------------|-------------------|
    | Allocateur personnalisé   | Via custom deleter      | Via NkAllocator intégré | Plus simple, type-safe |
    | Compatibilité NKMemory    | Non                     | Oui (New/Delete)        | Intégration native |
    | noexcept garanties        | Partielles              | Toutes opérations clés  | Meilleure sécurité exception |
    | Support tableaux T[]      | Oui (spécialisation)    | Oui + DeleteArray       | Gère header interne automatiquement |
    | Méthode IsValid()         | Non (operator bool)     | Oui + operator bool     | Plus explicite pour les audits |
    | Dépendances               | <memory> standard       | NKMemory/NKCore         | Contrôle total du build |

    Migration depuis std::unique_ptr :
    1. Remplacer #include <memory> par #include "NKMemory/NkUniquePtr.h"
    2. Remplacer std:: par nkentseu::memory::
    3. Pour les allocateurs : utiliser NkMakeUniqueWithAllocator au lieu de custom deleter manuel
    4. Vérifier que les types utilisés sont compatibles avec NKCore types (nk_size, etc.)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================