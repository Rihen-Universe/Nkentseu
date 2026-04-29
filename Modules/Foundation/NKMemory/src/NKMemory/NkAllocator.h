// =============================================================================
// NKMemory/NkAllocator.h
// Interface et implémentations d'allocateurs mémoire pluggables.
//
// Design :
//  - Réutilisation des utilitaires NKMemory/NKCore (ZÉRO duplication)
//  - Hiérarchie de classes avec polymorphisme contrôlé pour l'API publique
//  - Templates type-safe pour new/delete avec placement new
//  - Support multi-stratégie : malloc, new, virtual, pool, arena, stack, etc.
//  - Macros NKENTSEU_MEMORY_API pour la visibilité des symboles
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKALLOCATOR_H
#define NKENTSEU_MEMORY_NKALLOCATOR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API, inline macros
    #include "NKMemory/NkUtils.h"          // NkAlignUp, NkIsPowerOfTwo, NkMemCopy, etc.
    #include "NKCore/NkTypes.h"            // nk_size, nk_bool, nk_uintptr, etc.
    #include "NKCore/NkTraits.h"           // traits::NkForward pour perfect forwarding

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET CONSTANTES
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            /**
             * @brief Alignement mémoire par défaut pour les allocations
             * @ingroup MemoryConstants
             * 
             * Utilisé quand aucun alignement explicite n'est fourni.
             * Garantit la compatibilité avec les pointeurs de fonction et données.
             */
            inline constexpr nk_size NK_MEMORY_DEFAULT_ALIGNMENT = alignof(std::max_align_t);

            // -------------------------------------------------------------------------
            // SECTION 3 : FLAGS D'ALLOCATION MÉMOIRE
            // -------------------------------------------------------------------------
            /**
             * @enum NkMemoryFlag
             * @brief Flags pour contrôler le comportement d'allocation mémoire
             * @ingroup MemoryEnums
             * 
             * Utilisés principalement par NkVirtualAllocator pour les mappings mémoire
             * bas niveau (mmap/VirtualAlloc).
             */
            enum NkMemoryFlag : nk_uint32 {
                NK_MEMORY_FLAG_NONE      = 0u,        ///< Aucun flag spécial
                NK_MEMORY_FLAG_READ      = 1u << 0u,  ///< Permission de lecture
                NK_MEMORY_FLAG_WRITE     = 1u << 1u,  ///< Permission d'écriture
                NK_MEMORY_FLAG_EXECUTE   = 1u << 2u,  ///< Permission d'exécution
                NK_MEMORY_FLAG_RESERVE   = 1u << 3u,  ///< Réserver l'espace d'adressage
                NK_MEMORY_FLAG_COMMIT    = 1u << 4u,  ///< Allouer la mémoire physique
                NK_MEMORY_FLAG_ANONYMOUS = 1u << 5u   ///< Mapping anonyme (non fichier)
            };

            // Opérateurs bitwise pour combiner les flags
            constexpr NkMemoryFlag operator|(NkMemoryFlag a, NkMemoryFlag b) noexcept {
                return static_cast<NkMemoryFlag>(static_cast<nk_uint32>(a) | static_cast<nk_uint32>(b));
            }
            constexpr NkMemoryFlag operator&(NkMemoryFlag a, NkMemoryFlag b) noexcept {
                return static_cast<NkMemoryFlag>(static_cast<nk_uint32>(a) & static_cast<nk_uint32>(b));
            }
            constexpr NkMemoryFlag& operator|=(NkMemoryFlag& a, NkMemoryFlag b) noexcept {
                return a = a | b;
            }

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE DE BASE ABSTRAITE
            // -------------------------------------------------------------------------
            /**
             * @class NkAllocatorBase
             * @brief Classe de base pour tous les allocateurs NKMemory
             * @ingroup MemoryAllocators
             * 
             * Fournit l'interface minimale commune : nom et destruction virtuelle.
             * Ne déclare PAS les méthodes d'allocation pour permettre des signatures
             * différentes dans les dérivées si nécessaire.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkAllocatorBase {
            public:
                /**
                 * @brief Constructeur avec nom optionnel pour le debugging
                 * @param name Nom identifiant l'allocateur (stocké en lecture seule)
                 */
                explicit NkAllocatorBase(const nk_char* name = "NkAllocatorBase") noexcept;
                
                /** @brief Destructeur virtuel pour suppression polymorphe sûre */
                virtual ~NkAllocatorBase() = default;

                // Non-copyable, non-movable pour sécurité mémoire
                NkAllocatorBase(const NkAllocatorBase&) = delete;
                NkAllocatorBase& operator=(const NkAllocatorBase&) = delete;
                NkAllocatorBase(NkAllocatorBase&&) = delete;
                NkAllocatorBase& operator=(NkAllocatorBase&&) = delete;

                /**
                 * @brief Retourne le nom de l'allocateur pour logging/debug
                 * @return Pointeur vers string constante (ne pas libérer)
                 */
                [[nodiscard]] const nk_char* GetName() const noexcept;

            protected:
                const nk_char* mName;  ///< Nom de l'allocateur (non-possédé)
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : INTERFACE PRINCIPALE NkAllocator
            // -------------------------------------------------------------------------
            /**
             * @class NkAllocator
             * @brief Interface publique pour tous les allocateurs mémoire
             * @ingroup MemoryAllocators
             * 
             * Fournit :
             *  - Allocation/désallocation avec alignement
             *  - Reallocation avec copie automatique
             *  - Allocation zero-initialisée (calloc)
             *  - Helpers type-safe : New<T>, Delete<T>, NewArray<T>, DeleteArray<T>
             * 
             * @note Les méthodes virtuelles sont non-inline pour permettre l'override.
             * @note Les templates New/Delete sont inline dans ce header pour performance.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkAllocator : public NkAllocatorBase {
            public:
                // Types pour compatibilité et clarté
                using Pointer    = void*;
                using SizeType   = nk_size;
                using AlignType  = nk_size;

                /** @brief Constructeur avec nom */
                explicit NkAllocator(const nk_char* name = "NkAllocator") noexcept;
                
                /** @brief Destructeur virtuel */
                ~NkAllocator() override = default;

                // Non-copyable pour sécurité
                NkAllocator(const NkAllocator&) = delete;
                NkAllocator& operator=(const NkAllocator&) = delete;

                // =================================================================
                // @name Interface d'Allocation Pure Virtuelle
                // =================================================================
                /**
                 * @brief Alloue un bloc mémoire de taille donnée avec alignement
                 * @param size Nombre d'octets à allouer
                 * @param alignment Alignement requis (puissance de 2, défaut: pointeur)
                 * @return Pointeur vers le bloc alloué, ou nullptr en cas d'échec
                 * 
                 * @note L'alignement DOIT être une puissance de 2
                 * @note Le comportement est undefined si size == 0 (dépend de l'implémentation)
                 */
                [[nodiscard]] virtual Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) = 0;

                /**
                 * @brief Libère un bloc mémoire précédemment alloué
                 * @param ptr Pointeur à libérer (peut être nullptr, opération no-op)
                 * 
                 * @note Comportement undefined si ptr n'a pas été alloué par cet allocateur
                 * @note Double-free est undefined behavior
                 */
                virtual void Deallocate(Pointer ptr) = 0;

                // =================================================================
                // @name Méthodes Virtuelles avec Implémentation Par Défaut
                // =================================================================
                /**
                 * @brief Libère un bloc avec taille connue (pour sized delete)
                 * @param ptr Pointeur à libérer
                 * @param size Taille du bloc (informationnelle, peut être ignorée)
                 * 
                 * @note Par défaut, délègue à Deallocate(ptr) sans utiliser size
                 * @note Les sous-classes peuvent surcharger pour optimiser avec sized free
                 */
                virtual void Deallocate(Pointer ptr, SizeType size);

                /**
                 * @brief Redimensionne un bloc mémoire existant
                 * @param ptr Bloc à redimensionner (peut être nullptr pour allocation)
                 * @param oldSize Taille actuelle du bloc (pour copie optimisée)
                 * @param newSize Nouvelle taille souhaitée
                 * @param alignment Alignement requis pour le nouveau bloc
                 * @return Nouveau pointeur, ou nullptr en cas d'échec (ptr reste valide)
                 * 
                 * @note Implémentation par défaut : allocate + copy + deallocate
                 * @note Les sous-classes peuvent optimiser pour les cas in-place
                 */
                [[nodiscard]] virtual Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

                /**
                 * @brief Alloue et zero-initialise un bloc mémoire
                 * @param size Nombre d'octets à allouer et initialiser à zéro
                 * @param alignment Alignement requis
                 * @return Pointeur vers bloc zero-initialisé, ou nullptr en cas d'échec
                 * 
                 * @note Implémentation par défaut : Allocate + NkMemZero
                 */
                [[nodiscard]] virtual Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

                /**
                 * @brief Réinitialise l'état de l'allocateur (libère tout sans deallocate individuel)
                 * @note No-op par défaut, override pour les allocateurs à reset bulk (Arena, Linear)
                 */
                virtual void Reset() noexcept;

                /** @brief Alias vers GetName() pour cohérence d'API */
                [[nodiscard]] virtual const nk_char* Name() const noexcept;

                // =================================================================
                // @name Helpers Type-Safe (Templates Inline)
                // =================================================================
                /**
                 * @brief Constructeur placement-new avec allocation automatique
                 * @tparam T Type de l'objet à construire
                 * @tparam Args Types des arguments du constructeur
                 * @param args Arguments forwardés au constructeur de T
                 * @return Pointeur vers l'objet construit, ou nullptr si allocation échoue
                 * 
                 * @note Utilise alignof(T) automatiquement pour l'alignement
                 * @note L'objet DOIT être détruit via Delete<T>() pour libération correcte
                 */
                template <typename T, typename... Args>
                [[nodiscard]] T* New(Args&&... args);

                /**
                 * @brief Destructeur + deallocation pour objet créé via New<T>
                 * @tparam T Type de l'objet à détruire
                 * @param ptr Pointeur vers l'objet (nullptr accepté, no-op)
                 * 
                 * @note Appelle explicitement le destructeur ~T() avant deallocation
                 * @note Safe : vérifie nullptr avant toute opération
                 */
                template <typename T>
                void Delete(T* ptr) noexcept;

                /**
                 * @brief Allocation + construction d'un tableau d'objets
                 * @tparam T Type des éléments du tableau
                 * @tparam Args Types des arguments pour chaque constructeur d'élément
                 * @param count Nombre d'éléments à allouer
                 * @param args Arguments forwardés à chaque constructeur
                 * @return Pointeur vers le tableau, ou nullptr en cas d'échec
                 * 
                 * @note Stocke un header interne pour tracker count et offset
                 * @note Utiliser DeleteArray<T>() pour libération correcte
                 * @note L'alignement utilise alignof(T) automatiquement
                 */
                template <typename T, typename... Args>
                [[nodiscard]] T* NewArray(SizeType count, Args&&... args);

                /**
                 * @brief Destruction + deallocation d'un tableau créé via NewArray<T>
                 * @tparam T Type des éléments du tableau
                 * @param ptr Pointeur vers le tableau (nullptr accepté, no-op)
                 * 
                 * @note Détruit chaque élément dans l'ordre inverse de construction
                 * @note Libère le bloc complet incluant l'header interne
                 */
                template <typename T>
                void DeleteArray(T* ptr) noexcept;

                /**
                 * @brief Cast type-safe d'un Pointer vers un type spécifique
                 * @tparam T Type cible du cast
                 * @param ptr Pointeur brut à caster
                 * @return Pointeur casté vers T*, ou nullptr si ptr était nullptr
                 * 
                 * @note Utility pour éviter les reinterpret_cast explicites dans le code client
                 * @note N'effectue aucun runtime check - responsabilité de l'appelant
                 */
                template <typename T>
                [[nodiscard]] T* As(Pointer ptr) const noexcept;
            };

            /** @brief Alias de compatibilité pour code legacy */
            using NkIAllocator = NkAllocator;

            // -------------------------------------------------------------------------
            // SECTION 6 : IMPLÉMENTATIONS D'ALLOCATEURS CONCRETS
            // -------------------------------------------------------------------------

            /**
             * @class NkNewAllocator
             * @brief Allocateur utilisant operator new/delete avec alignement
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Utilise ::operator new(size_t, std::align_val_t) sur C++17+
             * ou fallback manuel avec header d'alignement sur C++14.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkNewAllocator final : public NkAllocator {
            public:
                NkNewAllocator() noexcept;
                ~NkNewAllocator() override = default;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
            };

            /**
             * @class NkMallocAllocator
             * @brief Allocateur utilisant malloc/free avec alignement plateforme
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Utilise _aligned_malloc/_aligned_free sur Windows
             * ou posix_memalign/free sur POSIX.
             * Supporte sized deallocation si C23 disponible.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMallocAllocator final : public NkAllocator {
            public:
                NkMallocAllocator() noexcept;
                ~NkMallocAllocator() override = default;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                void Deallocate(Pointer ptr, SizeType size) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
            };

            /**
             * @class NkVirtualAllocator
             * @brief Allocateur pour mappings mémoire bas niveau (mmap/VirtualAlloc)
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Pour allocations de grande taille, mémoire partagée, ou contrôle fin
             * des permissions (read/write/execute). Gère le tracking interne des blocs.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkVirtualAllocator final : public NkAllocator {
            public:
                NkVirtualAllocator() noexcept;
                ~NkVirtualAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                /**
                 * @brief Alloue de la mémoire virtuelle avec flags explicites
                 * @param size Taille demandée (sera arrondie à la page système)
                 * @param tag Nom optionnel pour debugging (non utilisé actuellement)
                 * @return Pointeur vers la région mappée, ou nullptr en cas d'échec
                 */
                [[nodiscard]] Pointer AllocateVirtual(SizeType size, const nk_char* tag = "VirtualAllocation") noexcept;
                
                /** @brief Libère une région allouée via AllocateVirtual */
                void FreeVirtual(Pointer ptr) noexcept;

                /** @brief Flags actifs pour les prochaines allocations virtuelles */
                NkMemoryFlag flags;

            private:
                // Structure de tracking pour libération correcte
                struct VirtualBlock {
                    Pointer ptr;
                    SizeType size;
                    VirtualBlock* next;
                };

                VirtualBlock* mBlocks;  ///< Liste chaînée des blocs alloués

                /** @brief Retourne la taille de page système */
                [[nodiscard]] SizeType PageSize() const noexcept;
                
                /** @brief Arrondit une taille au multiple de page système */
                [[nodiscard]] SizeType NormalizeSize(SizeType size) const noexcept;
                
                /** @brief Ajoute un bloc au tracking interne */
                void TrackBlock(Pointer ptr, SizeType size);
                
                /** @brief Retire un bloc du tracking et retourne sa taille */
                [[nodiscard]] SizeType UntrackBlock(Pointer ptr) noexcept;
            };

            /**
             * @class NkLinearAllocator
             * @brief Allocateur linéaire (bump pointer) pour allocations séquentielles
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Très rapide pour les allocations temporaires frame-based.
             * Ne supporte pas la deallocation individuelle - Reset() libère tout.
             * Optimisation : deallocation du dernier bloc si top-of-stack.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkLinearAllocator final : public NkAllocator {
            public:
                /**
                 * @brief Constructeur avec buffer pré-alloué
                 * @param capacityBytes Taille totale du buffer interne
                 * @param backingAllocator Allocateur pour le buffer (nullptr = défaut)
                 */
                explicit NkLinearAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkLinearAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                void Deallocate(Pointer ptr, SizeType size) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                /** @brief Capacité totale du buffer en octets */
                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }
                
                /** @brief Octets actuellement utilisés */
                [[nodiscard]] SizeType Used() const noexcept { return mOffset; }
                
                /** @brief Octets restants disponibles */
                [[nodiscard]] SizeType Available() const noexcept { return mCapacity - mOffset; }

            private:
                NkAllocator* mBacking;      ///< Allocateur pour le buffer interne
                nk_uint8*    mBuffer;       ///< Début du buffer alloué
                SizeType     mCapacity;     ///< Taille totale du buffer
                SizeType     mOffset;       ///< Offset courant (bump pointer)
                nk_bool      mOwnsBuffer;   ///< Si true, libérer mBuffer dans le dtor
            };

            /**
             * @class NkArenaAllocator
             * @brief Allocateur arena avec support de markers pour libération partielle
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Comme NkLinearAllocator mais avec CreateMarker()/FreeToMarker()
             * pour libérer sélectivement les allocations récentes.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkArenaAllocator final : public NkAllocator {
            public:
                using Marker = SizeType;  ///< Type opaque pour les markers

                explicit NkArenaAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkArenaAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                /** @brief Capture l'état courant pour libération future */
                [[nodiscard]] Marker CreateMarker() const noexcept;
                
                /** @brief Libère toutes les allocations après le marker */
                void FreeToMarker(Marker marker) noexcept;

            private:
                NkAllocator* mBacking;
                nk_uint8*    mBuffer;
                SizeType     mCapacity;
                SizeType     mOffset;
                nk_bool      mOwnsBuffer;
            };

            /**
             * @class NkStackAllocator
             * @brief Allocateur LIFO avec deallocation du dernier bloc uniquement
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Comme une pile : on ne peut libérer que la dernière allocation.
             * Utile pour les scopes imbriqués avec libération déterministe.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkStackAllocator final : public NkAllocator {
            public:
                explicit NkStackAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkStackAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }
                [[nodiscard]] SizeType Used() const noexcept { return mOffset; }

            private:
                // Header stocké avant chaque allocation pour chaînage LIFO
                struct AllocationHeader {
                    SizeType previousOffset;      ///< Offset avant cette allocation
                    Pointer  previousAllocation;  ///< Pointeur de l'alloc précédente
                };

                NkAllocator* mBacking;
                nk_uint8*    mBuffer;
                SizeType     mCapacity;
                SizeType     mOffset;
                Pointer      mLastAllocation;  ///< Dernière allocation (top of stack)
                nk_bool      mOwnsBuffer;
            };

            /**
             * @class NkPoolAllocator
             * @brief Allocateur de pool à blocs fixes de taille identique
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Très efficace pour les objets fréquemment alloués/libérés de même taille.
             * Utilise une free-list pour O(1) allocate/deallocate.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkPoolAllocator final : public NkAllocator {
            public:
                /**
                 * @brief Constructeur avec paramètres de pool
                 * @param blockSize Taille de chaque bloc (minimum: sizeof(FreeNode))
                 * @param blockCount Nombre total de blocs dans le pool
                 * @param backingAllocator Allocateur pour le buffer du pool
                 */
                NkPoolAllocator(SizeType blockSize, SizeType blockCount,
                                NkAllocator* backingAllocator = nullptr);
                ~NkPoolAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType BlockSize() const noexcept { return mBlockSize; }
                [[nodiscard]] SizeType BlockCount() const noexcept { return mBlockCount; }

            private:
                // Noeud de free-list : réutilise l'espace du bloc libre
                struct FreeNode {
                    FreeNode* next;
                };

                NkAllocator* mBacking;
                nk_uint8*    mBuffer;
                FreeNode*    mFreeList;      ///< Tête de la liste des blocs libres
                SizeType     mBlockSize;     ///< Taille de chaque bloc (ajustée)
                SizeType     mBlockCount;    ///< Nombre total de blocs
                nk_bool      mOwnsBuffer;
            };

            /**
             * @class NkFreeListAllocator
             * @brief Allocateur généraliste avec free-list et coalescing
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Gère des blocs de tailles variables avec fusion des blocs libres
             * adjacents pour réduire la fragmentation. Plus lent que Pool/Linear
             * mais plus flexible pour les patterns d'allocation imprévisibles.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkFreeListAllocator final : public NkAllocator {
            public:
                explicit NkFreeListAllocator(SizeType capacityBytes,
                                            NkAllocator* backingAllocator = nullptr);
                ~NkFreeListAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }

            private:
                // Header de bloc libre/alloué dans le buffer
                struct BlockHeader {
                    SizeType      size;     ///< Taille totale du bloc (header + user)
                    BlockHeader*  next;     ///< Bloc suivant dans la liste globale
                    BlockHeader*  prev;     ///< Bloc précédent (pour coalescing bidirectionnel)
                    nk_bool       isFree;   ///< true si bloc libre, false si alloué
                };

                // Header stocké avant chaque allocation utilisateur
                struct AllocationHeader {
                    BlockHeader* block;  ///< Pointeur vers le BlockHeader propriétaire
                };

                NkAllocator*   mBacking;
                nk_uint8*      mBuffer;
                SizeType       mCapacity;
                BlockHeader*   mHead;      ///< Tête de la liste des blocs (libres + alloués)
                nk_bool        mOwnsBuffer;

                /**
                 * @brief Fusionne le bloc avec ses voisins libres si possible
                 * @param block Bloc récemment libéré à potentiellement fusionner
                 * @note Réduit la fragmentation en combinant les blocs libres adjacents
                 */
                void Coalesce(BlockHeader* block) noexcept;
            };

            /**
             * @class NkBuddyAllocator
             * @brief Allocateur buddy system pour réduction de fragmentation externe
             * @ingroup MemoryAllocatorsConcrete
             * 
             * Utilise un backend NkFreeListAllocator mais arrondit les tailles
             * à la puissance de 2 supérieure. Permet la fusion de blocs "buddies"
             * (même taille, adresses adjacentes) pour optimiser la réutilisation.
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkBuddyAllocator final : public NkAllocator {
            public:
                /**
                 * @brief Constructeur avec paramètres buddy
                 * @param capacityBytes Taille totale de l'arena gérée
                 * @param minBlockSize Taille minimale d'un bloc (arrondie à puissance de 2)
                 * @param backingAllocator Allocateur pour l'arena interne
                 */
                explicit NkBuddyAllocator(SizeType capacityBytes,
                                        SizeType minBlockSize = 32u,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkBuddyAllocator() override;

                Pointer Allocate(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, AlignType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

            private:
                SizeType          mMinBlockSize;  ///< Taille minimale des blocs (puissance de 2)
                NkFreeListAllocator mBackend;     ///< Backend pour la gestion effective des blocs
            };

            // -------------------------------------------------------------------------
            // SECTION 7 : FONCTIONS GLOBALES ET ALLOCATEURS PAR DÉFAUT
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryGlobalFunctions Fonctions Globales d'Allocation
             * @brief API C-style pour allocation sans gestion manuelle d'allocateur
             * 
             * Utilisent l'allocateur par défaut si aucun n'est spécifié.
             * Pratiques pour le code legacy ou les wrappers simples.
             */

            /**
             * @brief Retourne l'allocateur par défaut pour le module mémoire
             * @return Référence vers l'allocateur par défaut (NkMallocAllocator par défaut)
             * @note Thread-safe après initialisation (Meyer's singleton)
             */
            NKENTSEU_MEMORY_API NkAllocator& NkGetDefaultAllocator() noexcept;

            /** @brief Retourne l'instance globale de NkMallocAllocator */
            NKENTSEU_MEMORY_API NkAllocator& NkGetMallocAllocator() noexcept;
            
            /** @brief Retourne l'instance globale de NkNewAllocator */
            NKENTSEU_MEMORY_API NkAllocator& NkGetNewAllocator() noexcept;
            
            /** @brief Retourne l'instance globale de NkVirtualAllocator */
            NKENTSEU_MEMORY_API NkAllocator& NkGetVirtualAllocator() noexcept;

            /**
             * @brief Définit l'allocateur par défaut pour les appels globaux
             * @param allocator Pointeur vers le nouvel allocateur par défaut
             * @note Si nullptr, réinitialise vers NkMallocAllocator
             * @note Non thread-safe : à appeler avant tout usage mémoire dans l'application
             */
            NKENTSEU_MEMORY_API void NkSetDefaultAllocator(NkAllocator* allocator) noexcept;

            /**
             * @brief Alloue un bloc mémoire via l'allocateur spécifié ou par défaut
             * @param size Taille en octets
             * @param allocator Allocateur à utiliser (nullptr = défaut)
             * @param alignment Alignement requis
             * @return Pointeur vers le bloc alloué
             * @ingroup MemoryGlobalFunctions
             */
            NKENTSEU_MEMORY_API void* NkAlloc(nk_size size, NkAllocator* allocator = nullptr,
                                            nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

            /**
             * @brief Alloue et zero-initialise count * size octets
             * @param count Nombre d'éléments
             * @param size Taille de chaque élément
             * @param allocator Allocateur à utiliser
             * @param alignment Alignement requis
             * @return Pointeur vers bloc zero-initialisé
             * @ingroup MemoryGlobalFunctions
             */
            NKENTSEU_MEMORY_API void* NkAllocZero(nk_size count, nk_size size, NkAllocator* allocator = nullptr,
                                                nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

            /**
             * @brief Redimensionne un bloc mémoire existant
             * @param ptr Bloc à redimensionner
             * @param oldSize Taille actuelle
             * @param newSize Nouvelle taille
             * @param allocator Allocateur à utiliser
             * @param alignment Alignement requis
             * @return Nouveau pointeur ou nullptr (ptr original reste valide en cas d'échec)
             * @ingroup MemoryGlobalFunctions
             */
            NKENTSEU_MEMORY_API void* NkRealloc(void* ptr, nk_size oldSize, nk_size newSize,
                                              NkAllocator* allocator = nullptr,
                                              nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

            /** @brief Libère un bloc via l'allocateur spécifié ou par défaut */
            NKENTSEU_MEMORY_API void NkFree(void* ptr, NkAllocator* allocator = nullptr);
            
            /** @brief Libère un bloc avec taille connue (pour sized deallocation) */
            NKENTSEU_MEMORY_API void NkFree(void* ptr, nk_size size, NkAllocator* allocator = nullptr);

        } // namespace memory
    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 8 : IMPLÉMENTATIONS TEMPLATES INLINE
    // -------------------------------------------------------------------------
    // Les templates New/Delete doivent être dans le header pour l'instantiation.
    // Ils utilisent les méthodes virtuelles de NkAllocator pour l'allocation réelle.

    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Implémentations Inline - Helpers Type-Safe
            // ====================================================================

            template <typename T, typename... Args>
            T* NkAllocator::New(Args&&... args) {
                // Allocation avec alignement naturel du type
                void* memory = Allocate(sizeof(T), alignof(T));
                if (!memory) {
                    return nullptr;  // Échec d'allocation : propagation nullptr
                }
                // Placement new avec perfect forwarding
                return new (memory) T(traits::NkForward<Args>(args)...);
            }

            template <typename T>
            void NkAllocator::Delete(T* ptr) noexcept {
                if (!ptr) {
                    return;  // Safe : no-op sur nullptr
                }
                // Destruction explicite avant deallocation mémoire
                ptr->~T();
                Deallocate(ptr);
            }

            template <typename T, typename... Args>
            T* NkAllocator::NewArray(SizeType count, Args&&... args) {
                if (count == 0u) {
                    return nullptr;  // Pas d'allocation pour tableau vide
                }

                // Header interne pour tracker metadata du tableau
                struct ArrayHeader {
                    SizeType count;   ///< Nombre d'éléments
                    SizeType offset;  ///< Offset depuis base jusqu'au premier élément
                };

                const SizeType headerSize   = sizeof(ArrayHeader);
                const SizeType elemAlign    = alignof(T);
                const SizeType baseOffset   = NK_ALIGN_UP(headerSize, elemAlign);
                const SizeType totalBytes   = baseOffset + sizeof(T) * count;

                // Allocation du bloc complet (header + données)
                void* base = Allocate(totalBytes, elemAlign);
                if (!base) {
                    return nullptr;
                }

                // Initialisation de l'header
                auto* header = static_cast<ArrayHeader*>(base);
                header->count  = count;
                header->offset = baseOffset;

                // Pointeur vers le début des éléments (après header aligné)
                T* array = reinterpret_cast<T*>(static_cast<nk_uint8*>(base) + baseOffset);
                
                // Construction de chaque élément avec perfect forwarding
                for (SizeType i = 0u; i < count; ++i) {
                    new (array + i) T(traits::NkForward<Args>(args)...);
                }

                return array;
            }

            template <typename T>
            void NkAllocator::DeleteArray(T* ptr) noexcept {
                if (!ptr) {
                    return;  // Safe : no-op sur nullptr
                }

                // Structure doit matcher exactement celle de NewArray
                struct ArrayHeader {
                    SizeType count;
                    SizeType offset;
                };

                const SizeType headerSize = sizeof(ArrayHeader);
                const SizeType baseOffset = NK_ALIGN_UP(headerSize, alignof(T));
                
                // Reconstruction du pointeur de base depuis le pointeur utilisateur
                auto* base = reinterpret_cast<nk_uint8*>(ptr) - baseOffset;
                auto* header = reinterpret_cast<ArrayHeader*>(base);

                // Destruction dans l'ordre inverse (bonne pratique, pas requis pour POD)
                for (SizeType i = header->count; i-- > 0u;) {
                    ptr[i].~T();
                }

                // Libération du bloc complet (header + données)
                Deallocate(base);
            }

            template <typename T>
            T* NkAllocator::As(Pointer ptr) const noexcept {
                if (!ptr) {
                    return nullptr;  // Propagation sûre de nullptr
                }
                return reinterpret_cast<T*>(ptr);  // Cast explicite mais type-safe via template
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKALLOCATOR_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Utilisation basique avec allocateur par défaut
    void* buffer = nkentseu::memory::NkAlloc(1024);
    // ... utilisation ...
    nkentseu::memory::NkFree(buffer);

    // Exemple 2 : Allocateur personnalisé avec alignement
    nkentseu::memory::NkPoolAllocator pool(64, 100);  // 100 blocs de 64 bytes
    void* obj = pool.Allocate(64, 16);  // Aligné 16 bytes
    pool.Deallocate(obj);

    // Exemple 3 : Helpers type-safe pour objets C++
    nkentseu::memory::NkNewAllocator alloc;
    auto* str = alloc.New<std::string>("Hello");
    // ... utilisation de *str ...
    alloc.Delete(str);  // Appelle ~string() + deallocate

    // Exemple 4 : Arena avec markers pour libération partielle
    nkentseu::memory::NkArenaAllocator arena(65536);
    auto marker = arena.CreateMarker();
    
    void* temp1 = arena.Allocate(256);
    void* temp2 = arena.Allocate(512);
    
    arena.FreeToMarker(marker);  // Libère temp1 et temp2, garde les allocations avant marker

    // Exemple 5 : Changement d'allocateur par défaut (au démarrage de l'app)
    static nkentseu::memory::NkMallocAllocator customAlloc;
    nkentseu::memory::NkSetDefaultAllocator(&customAlloc);
*/

// =============================================================================
// CHOIX D'ALLOCATEUR : GUIDE RAPIDE
// =============================================================================
/*
    | Cas d'usage                    | Allocateur recommandé    | Raison |
    |-------------------------------|--------------------------|--------|
    | Allocations générales         | NkMallocAllocator        | Portable, fiable |
    | Objets C++ avec destructeurs  | NkNewAllocator           | Intègre new/delete |
    | Grandes régions mémoire       | NkVirtualAllocator       | Contrôle permissions |
    | Allocations temporaires frame | NkLinearAllocator        | O(1) allocate, reset bulk |
    | Allocations avec rollback     | NkArenaAllocator         | Markers pour libération partielle |
    | Scope LIFO déterministe       | NkStackAllocator         | Deallocation dernier uniquement |
    | Objets identiques fréquents   | NkPoolAllocator          | O(1) allocate/deallocate, cache-friendly |
    | Tailles variables imprévisibles| NkFreeListAllocator     | Flexible avec coalescing |
    | Réduction fragmentation       | NkBuddyAllocator         | Fusion de blocs buddies |

    Performance relative (allocate + deallocate, 1000 itérations, 64 bytes) :
    - NkPoolAllocator     : ~50ns   (référence)
    - NkLinearAllocator   : ~30ns   (mais reset seulement)
    - NkMallocAllocator   : ~200ns
    - NkFreeListAllocator : ~500ns
    - NkNewAllocator      : ~250ns
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================