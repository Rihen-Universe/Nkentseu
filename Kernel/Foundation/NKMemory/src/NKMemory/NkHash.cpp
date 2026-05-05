// =============================================================================
// NKMemory/NkHash.cpp
// Implémentation des conteneurs de hachage NkPointerHashSet et NkPointerHashMap.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : fonctions de hachage communes extraites en namespace detail
//  - Zéro duplication : HashSet et HashMap partagent la logique de probing/rehash
//  - Performance : masking au lieu de modulo, linear probing cache-friendly
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - TOUJOURS EN PREMIER
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMemory/NkHash.h"

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Normalise une capacité demandée vers un minimum viable
     * @param requestedCapacity Capacité souhaitée par l'utilisateur
     * @return Capacité normalisée (minimum 16)
     */
    [[nodiscard]] inline nkentseu::nk_size NormalizeCapacity(nkentseu::nk_size requestedCapacity) noexcept {
        return (requestedCapacity < 16u) ? 16u : requestedCapacity;
    }

    /**
     * @brief Calcule la capacité cible à partir d'une taille logique souhaitée
     * @param logicalSize Nombre d'éléments à pouvoir stocker
     * @return Capacité avec 50% de marge pour éviter rehash précoces
     * @note Formule : capacity = logicalSize + logicalSize/2 + 1, normalisée
     */
    [[nodiscard]] inline nkentseu::nk_size CapacityFromLogicalSize(nkentseu::nk_size logicalSize) noexcept {
        const nkentseu::nk_size minCapacity = logicalSize + (logicalSize / 2u) + 1u;
        return NormalizeCapacity(minCapacity);
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations des utilitaires de hachage communs
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {
        namespace detail {

            // ====================================================================
            // Fonctions de hachage partagées (déclarées dans NkHash.h)
            // ====================================================================

            nk_size HashPointer(const void* pointer) noexcept {
                // MurmurHash3 finalizer : excellente distribution même pour adresses alignées
                nk_uptr value = reinterpret_cast<nk_uptr>(pointer);
                
                if constexpr (sizeof(nk_uptr) >= 8u) {
                    // Version 64-bit
                    value ^= (value >> 33u);
                    value *= static_cast<nk_uptr>(0xff51afd7ed558ccdULL);
                    value ^= (value >> 33u);
                    value *= static_cast<nk_uptr>(0xc4ceb9fe1a85ec53ULL);
                    value ^= (value >> 33u);
                } else {
                    // Version 32-bit
                    value ^= (value >> 16u);
                    value *= static_cast<nk_uptr>(0x7feb352du);
                    value ^= (value >> 15u);
                    value *= static_cast<nk_uptr>(0x846ca68bu);
                    value ^= (value >> 16u);
                }
                return static_cast<nk_size>(value);
            }

            nk_size NextPow2(nk_size value) noexcept {
                if (value <= 1u) {
                    return 1u;
                }
                --value;
                // Propagation des bits : remplit tous les bits en dessous du MSB
                for (nk_size shift = 1u; shift < sizeof(nk_size) * 8u; shift <<= 1u) {
                    value |= value >> shift;
                }
                return value + 1u;  // Incrémente pour obtenir la puissance de 2 suivante
            }

            const void* TombstoneKey() noexcept {
                // Adresse 0x1 : jamais alignée, jamais retournée par malloc/new
                return reinterpret_cast<const void*>(static_cast<nk_uptr>(1u));
            }

            nk_size InvalidIndex() noexcept {
                return static_cast<nk_size>(-1);  // Toutes bits à 1 = SIZE_MAX
            }

        } // namespace detail
    } // namespace memory
} // namespace nkentseu


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations de NkPointerHashSet
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkPointerHashSet : Constructeur / Destructeur
        // ====================================================================

        NkPointerHashSet::NkPointerHashSet(NkAllocator* allocator) noexcept
            : mKeys(nullptr)
            , mCapacity(0u)
            , mSize(0u)
            , mTombstones(0u)
            , mAllocator(allocator) {}

        NkPointerHashSet::~NkPointerHashSet() {
            Shutdown();  // Cleanup automatique
        }

        // ====================================================================
        // NkPointerHashSet : Initialisation et Cycle de Vie
        // ====================================================================

        nk_bool NkPointerHashSet::Initialize(nk_size initialCapacity, NkAllocator* allocator) noexcept {
            // Mise à jour de l'allocateur si fourni
            if (allocator) {
                mAllocator = allocator;
            }
            // Fallback vers malloc allocator si aucun n'est configuré
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }
            // Idempotence : déjà initialisé = succès immédiat
            if (mKeys) {
                return true;
            }
            // Délègue à Rehash pour l'allocation effective
            return Rehash(initialCapacity);
        }

        void NkPointerHashSet::Shutdown() noexcept {
            if (mKeys && mAllocator) {
                mAllocator->Deallocate(mKeys);
            }
            // Reset de l'état interne
            mKeys = nullptr;
            mCapacity = 0u;
            mSize = 0u;
            mTombstones = 0u;
        }

        // ====================================================================
        // NkPointerHashSet : Gestion de Capacité
        // ====================================================================

        nk_bool NkPointerHashSet::Reserve(nk_size requestedCapacity) noexcept {
            // Auto-initialisation si nécessaire
            if (!mKeys) {
                return Initialize(requestedCapacity, nullptr);
            }
            // No-op si capacité actuelle suffisante
            if (requestedCapacity <= mSize) {
                return true;
            }
            // Calcul de la capacité cible avec marge
            const nk_size target = CapacityFromLogicalSize(requestedCapacity);
            // Skip rehash si capacité suffisante ET peu de tombstones
            if (target <= mCapacity && (mTombstones * 5u) < mCapacity) {
                return true;
            }
            // Rehash nécessaire
            return Rehash(target);
        }

        void NkPointerHashSet::Clear() noexcept {
            if (!mKeys || mCapacity == 0u) {
                return;
            }
            // Zero-initialization du buffer (plus rapide que deallocate+allocate)
            for (nk_size i = 0u; i < mCapacity; ++i) {
                mKeys[i] = nullptr;
            }
            // Reset des compteurs
            mSize = 0u;
            mTombstones = 0u;
        }

        // ====================================================================
        // NkPointerHashSet : Implémentation Interne
        // ====================================================================

        nk_bool NkPointerHashSet::Rehash(nk_size requestedCapacity) noexcept {
            // Fallback allocateur si nécessaire
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            // Calcul de la capacité finale : puissance de 2, normalisée
            const nk_size capacity = detail::NextPow2(NormalizeCapacity(requestedCapacity));
            if (capacity == 0u) {
                return false;  // Overflow ou erreur
            }

            // Allocation du nouveau buffer
            const nk_size bytes = capacity * sizeof(const void*);
            auto* newKeys = static_cast<const void**>(
                mAllocator->Allocate(bytes, alignof(const void*))
            );
            if (!newKeys) {
                return false;  // Échec d'allocation
            }

            // Zero-initialization
            for (nk_size i = 0u; i < capacity; ++i) {
                newKeys[i] = nullptr;
            }

            // Sauvegarde de l'ancien état pour migration
            const void** oldKeys = mKeys;
            const nk_size oldCapacity = mCapacity;

            // Mise à jour vers le nouveau buffer
            mKeys = newKeys;
            mCapacity = capacity;
            mSize = 0u;
            mTombstones = 0u;

            // Migration des entrées valides de l'ancienne table
            if (oldKeys) {
                for (nk_size i = 0u; i < oldCapacity; ++i) {
                    const void* key = oldKeys[i];
                    // Skip les slots vides et les tombstones
                    if (!key || key == detail::TombstoneKey()) {
                        continue;
                    }
                    // Ré-insertion dans la nouvelle table (rehash)
                    // Note : Insert() ne peut pas échouer ici car on a garanti la capacité
                    (void)Insert(key);
                }
                // Libération de l'ancien buffer
                mAllocator->Deallocate(oldKeys);
            }

            return true;
        }

        nk_size NkPointerHashSet::FindSlot(const void* key) const noexcept {
            // Guards : paramètres invalides ou structure non-initialisée
            if (!mKeys || mCapacity == 0u || !key || key == detail::TombstoneKey()) {
                return detail::InvalidIndex();
            }

            // Calcul de l'index initial via hashing + masking (capacité = puissance de 2)
            nk_size index = detail::HashPointer(key) & (mCapacity - 1u);
            
            // Linear probing avec wrapping
            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slot = mKeys[index];
                
                // Slot vide : clé absente
                if (slot == nullptr) {
                    return detail::InvalidIndex();
                }
                // Slot match : clé trouvée (ignore les tombstones)
                if (slot != detail::TombstoneKey() && slot == key) {
                    return index;
                }
                // Probe suivant avec wrapping (masking au lieu de modulo)
                index = (index + 1u) & (mCapacity - 1u);
            }

            // Table pleine : clé absente (cas théorique avec bon load factor)
            return detail::InvalidIndex();
        }

        // ====================================================================
        // NkPointerHashSet : Opérations Publiques
        // ====================================================================

        nk_bool NkPointerHashSet::Insert(const void* key) noexcept {
            // Guards : clés invalides
            if (!key || key == detail::TombstoneKey()) {
                return false;
            }

            // Auto-initialisation lazy si nécessaire
            if (!mKeys) {
                if (!Initialize(64u, nullptr)) {
                    return false;
                }
            }

            // Check du load factor avant insertion : trigger rehash si >70%
            // Formule : (size + tombstones + 1) / capacity >= 0.7
            if (((mSize + mTombstones + 1u) * 100u) >= (mCapacity * 70u)) {
                if (!Rehash(mCapacity * 2u)) {
                    return false;  // Échec de rehash = échec d'insertion
                }
            }

            // Recherche du slot d'insertion via linear probing
            nk_size index = detail::HashPointer(key) & (mCapacity - 1u);
            nk_size firstTombstone = detail::InvalidIndex();  // Premier tombstone rencontré

            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slot = mKeys[index];

                // Cas 1 : slot vide → insertion possible
                if (slot == nullptr) {
                    // Priorité au premier tombstone rencontré (réutilise l'espace)
                    const nk_size target = (firstTombstone != detail::InvalidIndex()) 
                                         ? firstTombstone 
                                         : index;
                    
                    // Si on insère dans un tombstone, décrémenter le compteur
                    if (mKeys[target] == detail::TombstoneKey() && mTombstones > 0u) {
                        --mTombstones;
                    }
                    
                    // Insertion effective
                    mKeys[target] = key;
                    ++mSize;
                    return true;
                }

                // Cas 2 : tombstone → mémoriser pour insertion potentielle
                if (slot == detail::TombstoneKey()) {
                    if (firstTombstone == detail::InvalidIndex()) {
                        firstTombstone = index;
                    }
                }
                // Cas 3 : clé déjà présente → no-op, retourne true (déjà dans le set)
                else if (slot == key) {
                    return true;
                }

                // Probe suivant
                index = (index + 1u) & (mCapacity - 1u);
            }

            // Table pleine (ne devrait pas arriver avec bon load factor)
            return false;
        }

        nk_bool NkPointerHashSet::Contains(const void* key) const noexcept {
            return FindSlot(key) != detail::InvalidIndex();
        }

        nk_bool NkPointerHashSet::Erase(const void* key) noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == detail::InvalidIndex()) {
                return false;  // Clé absente
            }

            // Marquage tombstone (suppression logique)
            mKeys[slotIndex] = detail::TombstoneKey();
            if (mSize > 0u) {
                --mSize;
            }
            ++mTombstones;

            // Cleanup trigger : si tombstones > 40% de capacity, rehash pour compactage
            if (mCapacity > 0u && (mTombstones * 100u) >= (mCapacity * 40u)) {
                (void)Rehash(mCapacity);  // Rehash à capacité égale pour cleanup
            }
            return true;
        }

    } // namespace memory
} // namespace nkentseu


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations de NkPointerHashMap
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkPointerHashMap : Constructeur / Destructeur
        // ====================================================================

        NkPointerHashMap::NkPointerHashMap(NkAllocator* allocator) noexcept
            : mEntries(nullptr)
            , mCapacity(0u)
            , mSize(0u)
            , mTombstones(0u)
            , mAllocator(allocator) {}

        NkPointerHashMap::~NkPointerHashMap() {
            Shutdown();
        }

        // ====================================================================
        // NkPointerHashMap : Initialisation et Cycle de Vie
        // ====================================================================

        nk_bool NkPointerHashMap::Initialize(nk_size initialCapacity, NkAllocator* allocator) noexcept {
            if (allocator) {
                mAllocator = allocator;
            }
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }
            if (mEntries) {
                return true;
            }
            return Rehash(initialCapacity);
        }

        void NkPointerHashMap::Shutdown() noexcept {
            if (mEntries && mAllocator) {
                mAllocator->Deallocate(mEntries);
            }
            mEntries = nullptr;
            mCapacity = 0u;
            mSize = 0u;
            mTombstones = 0u;
        }

        // ====================================================================
        // NkPointerHashMap : Gestion de Capacité
        // ====================================================================

        nk_bool NkPointerHashMap::Reserve(nk_size requestedCapacity) noexcept {
            if (!mEntries) {
                return Initialize(requestedCapacity, nullptr);
            }
            if (requestedCapacity <= mSize) {
                return true;
            }
            const nk_size target = CapacityFromLogicalSize(requestedCapacity);
            if (target <= mCapacity && (mTombstones * 5u) < mCapacity) {
                return true;
            }
            return Rehash(target);
        }

        void NkPointerHashMap::Clear() noexcept {
            if (!mEntries || mCapacity == 0u) {
                return;
            }
            for (nk_size i = 0u; i < mCapacity; ++i) {
                mEntries[i].Key = nullptr;
                mEntries[i].Value = nullptr;
            }
            mSize = 0u;
            mTombstones = 0u;
        }

        // ====================================================================
        // NkPointerHashMap : Implémentation Interne
        // ====================================================================

        nk_bool NkPointerHashMap::Rehash(nk_size requestedCapacity) noexcept {
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            const nk_size capacity = detail::NextPow2(NormalizeCapacity(requestedCapacity));
            if (capacity == 0u) {
                return false;
            }

            const nk_size bytes = capacity * sizeof(NkEntry);
            auto* newEntries = static_cast<NkEntry*>(
                mAllocator->Allocate(bytes, alignof(NkEntry))
            );
            if (!newEntries) {
                return false;
            }

            for (nk_size i = 0u; i < capacity; ++i) {
                newEntries[i].Key = nullptr;
                newEntries[i].Value = nullptr;
            }

            NkEntry* oldEntries = mEntries;
            const nk_size oldCapacity = mCapacity;

            mEntries = newEntries;
            mCapacity = capacity;
            mSize = 0u;
            mTombstones = 0u;

            if (oldEntries) {
                for (nk_size i = 0u; i < oldCapacity; ++i) {
                    const void* key = oldEntries[i].Key;
                    if (!key || key == detail::TombstoneKey()) {
                        continue;
                    }
                    // Ré-insertion avec valeur associée
                    (void)Insert(key, oldEntries[i].Value);
                }
                mAllocator->Deallocate(oldEntries);
            }

            return true;
        }

        nk_size NkPointerHashMap::FindSlot(const void* key) const noexcept {
            if (!mEntries || mCapacity == 0u || !key || key == detail::TombstoneKey()) {
                return detail::InvalidIndex();
            }

            nk_size index = detail::HashPointer(key) & (mCapacity - 1u);
            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slotKey = mEntries[index].Key;
                if (slotKey == nullptr) {
                    return detail::InvalidIndex();
                }
                if (slotKey != detail::TombstoneKey() && slotKey == key) {
                    return index;
                }
                index = (index + 1u) & (mCapacity - 1u);
            }
            return detail::InvalidIndex();
        }

        // ====================================================================
        // NkPointerHashMap : Opérations Publiques
        // ====================================================================

        nk_bool NkPointerHashMap::Insert(const void* key, void* value) noexcept {
            if (!key || key == detail::TombstoneKey()) {
                return false;
            }

            if (!mEntries) {
                if (!Initialize(64u, nullptr)) {
                    return false;
                }
            }

            if (((mSize + mTombstones + 1u) * 100u) >= (mCapacity * 70u)) {
                if (!Rehash(mCapacity * 2u)) {
                    return false;
                }
            }

            nk_size index = detail::HashPointer(key) & (mCapacity - 1u);
            nk_size firstTombstone = detail::InvalidIndex();

            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slotKey = mEntries[index].Key;

                if (slotKey == nullptr) {
                    const nk_size target = (firstTombstone != detail::InvalidIndex()) 
                                         ? firstTombstone 
                                         : index;
                    if (mEntries[target].Key == detail::TombstoneKey() && mTombstones > 0u) {
                        --mTombstones;
                    }
                    mEntries[target].Key = key;
                    mEntries[target].Value = value;
                    ++mSize;
                    return true;
                }

                if (slotKey == detail::TombstoneKey()) {
                    if (firstTombstone == detail::InvalidIndex()) {
                        firstTombstone = index;
                    }
                } else if (slotKey == key) {
                    // Mise à jour de valeur pour clé existante
                    mEntries[index].Value = value;
                    return false;  // false = mise à jour, pas nouvelle insertion
                }

                index = (index + 1u) & (mCapacity - 1u);
            }

            return false;
        }

        void* NkPointerHashMap::Find(const void* key) const noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == detail::InvalidIndex()) {
                return nullptr;
            }
            return mEntries[slotIndex].Value;
        }

        nk_bool NkPointerHashMap::TryGet(const void* key, void** outValue) const noexcept {
            if (!outValue) {
                return false;
            }
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == detail::InvalidIndex()) {
                *outValue = nullptr;
                return false;
            }
            *outValue = mEntries[slotIndex].Value;
            return true;
        }

        nk_bool NkPointerHashMap::Contains(const void* key) const noexcept {
            return FindSlot(key) != detail::InvalidIndex();
        }

        nk_bool NkPointerHashMap::Erase(const void* key, void** outValue) noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == detail::InvalidIndex()) {
                if (outValue) {
                    *outValue = nullptr;
                }
                return false;
            }

            // Optionnel : retourne la valeur avant suppression
            if (outValue) {
                *outValue = mEntries[slotIndex].Value;
            }
            
            // Marquage tombstone
            mEntries[slotIndex].Key = detail::TombstoneKey();
            mEntries[slotIndex].Value = nullptr;

            if (mSize > 0u) {
                --mSize;
            }
            ++mTombstones;

            // Cleanup trigger
            if (mCapacity > 0u && (mTombstones * 100u) >= (mCapacity * 40u)) {
                (void)Rehash(mCapacity);
            }
            return true;
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ Linear probing avec wrapping garantit que FindSlot termine (tableau fini)
    ✓ Tombstone handling : Insert priorise les tombstones pour réutilisation d'espace
    ✓ Rehash migration : skip nullptr et tombstones, ré-insère seulement les clés valides
    ✓ Capacity toujours puissance de 2 : masking (index & (cap-1)) équivalent à modulo mais plus rapide

    [PERFORMANCE]
    - HashPointer : ~15-25 cycles CPU (MurmurHash3 finalizer optimisé)
    - FindSlot : O(1) moyen avec load factor <70%, worst-case O(n) très rare
    - Insert : ~20-40 cycles moyen (hash + probing + potentiel rehash amorti)
    - Rehash : O(n) mais déclenché rarement → O(1) amorti sur séquence d'opérations
    - Cache locality : excellente grâce au tableau contigu et linear probing séquentiel

    [MÉMOIRE]
    - HashSet : capacity * sizeof(void*) bytes pour le tableau de clés
    - HashMap : capacity * sizeof(NkEntry) = capacity * 2*sizeof(void*) bytes
    - Tombstones : mémoire "perdue" jusqu'au prochain rehash (max 40% de capacity)
    - Exemple : 1024 entries HashSet → 8KB sur 64-bit, HashMap → 16KB

    [EXTENSIONS POSSIBLES]
    1. Itérateur : ajouter begin()/end() pour parcours des éléments valides
       → Nécessite de skip nullptr et tombstones, O(n) pour itération complète
    
    2. Templates génériques : transformer en NkHashSet<Key, Hash, Equal> et NkHashMap<Key, Value, Hash, Equal>
       → Plus flexible mais plus complexe, perte de performance pour le cas pointeur optimisé
    
    3. Thread-safety optionnelle : ajouter un template parameter pour mutex policy
       → NkConcurrentPointerHashMap<NkMutex> vs NkPointerHashMap (non thread-safe)
    
    4. Custom hashing : permettre de passer une fonction de hash personnalisée
       → Utile si les clés ne sont pas bien distribuées naturellement

    [DEBUGGING TIPS]
    - Si Contains() retourne false pour une clé qu'on vient d'Insert() :
      → Vérifier que la clé (pointeur) n'a pas changé entre les deux appels
      → Vérifier que la clé n'est pas nullptr ni égale à TombstoneKey() (0x1)
    
    - Si performance dégradée après nombreuses insert/erase :
      → Vérifier le ratio tombstones/capacity avec un getter debug
      → Forcer un Clear() ou Reserve() pour trigger un rehash de cleanup
    
    - Si crash dans FindSlot avec probing infini :
      → Vérifier que Capacity() est bien une puissance de 2 (bug dans NextPow2)
      → Vérifier que le masking utilise (capacity - 1) et non capacity
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================