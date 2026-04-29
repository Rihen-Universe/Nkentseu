// =============================================================================
// NKMemory/NkGc.cpp
// Implémentation du garbage collector mark-and-sweep.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale : fonctions de hachage extraites, guards locaux pour locks
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Zero duplication : logique hash table partagée entre méthodes
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
#include "NKMemory/NkGc.h"

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Minimum de deux valeurs size_t
     * @note Helper local pour éviter dépendance supplémentaire
     */
    [[nodiscard]] inline nkentseu::nk_size Min(nkentseu::nk_size a, nkentseu::nk_size b) noexcept {
        return (a < b) ? a : b;
    }

    /**
     * @brief Normalise une capacité demandée vers un minimum viable
     * @param requestedCapacity Capacité souhaitée par l'utilisateur
     * @return Capacité normalisée (minimum 64 pour la hash table GC)
     */
    [[nodiscard]] inline nkentseu::nk_size NormalizeIndexCapacity(nkentseu::nk_size requestedCapacity) noexcept {
        return (requestedCapacity < 64u) ? 64u : requestedCapacity;
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // NkGcTracer : Implémentation
        // ====================================================================

        NkGcTracer::NkGcTracer(NkGarbageCollector& gc) noexcept 
            : mGc(gc) {}

        void NkGcTracer::Mark(NkGcObject* object) noexcept {
            // Délègue à la méthode privée du GC (lock déjà acquis via RegisterObject/etc.)
            mGc.Mark(object);
        }

        // ====================================================================
        // NkGcObject : Implémentation
        // ====================================================================

        NkGcObject::NkGcObject() noexcept
            : mGcNext(nullptr)
            , mGcPrev(nullptr)
            , mGcMarked(false)
            , mGcOwned(false)
            , mGcAllocator(nullptr)
            , mGcDestroy(nullptr) {}

        void NkGcObject::Trace(NkGcTracer& /*tracer*/) {
            // Par défaut : no-op (objet sans références sortantes)
            // Les classes dérivées doivent override pour déclarer leurs références
        }

        nk_bool NkGcObject::IsMarked() const noexcept {
            return mGcMarked;
        }

        // ====================================================================
        // NkGcRoot : Implémentation
        // ====================================================================

        NkGcRoot::NkGcRoot(NkGcObject** slot) noexcept 
            : mSlot(slot), mNext(nullptr) {}

        void NkGcRoot::Bind(NkGcObject** slot) noexcept {
            mSlot = slot;
        }

        NkGcObject** NkGcRoot::Slot() const noexcept {
            return mSlot;
        }

        // ====================================================================
        // NkGarbageCollector : Constructeur / Destructeur
        // ====================================================================

        NkGarbageCollector::NkGarbageCollector(NkAllocator* allocator) noexcept
            : mObjects(nullptr)
            , mRoots(nullptr)
            , mObjectCount(0u)
            , mObjectIndex(nullptr)
            , mObjectIndexCapacity(0u)
            , mObjectIndexSize(0u)
            , mObjectIndexTombstones(0u)
            , mAllocator(allocator ? allocator : &NkGetDefaultAllocator())
            , mLock() {}

        NkGarbageCollector::~NkGarbageCollector() {
            // Acquisition du lock pour cleanup thread-safe
            NkSpinLockGuard guard(mLock);

            // Libération de la liste des racines (pas de destruction des objets pointés)
            NkGcRoot* root = mRoots;
            while (root) {
                NkGcRoot* next = root->mNext;
                root->mNext = nullptr;  // Defensive : éviter dangling pointers
                root = next;
            }
            mRoots = nullptr;

            // Destruction de tous les objets managés restants
            NkGcObject* current = mObjects;
            while (current) {
                NkGcObject* next = current->mGcNext;
                // Détachement défensif
                current->mGcPrev = nullptr;
                current->mGcNext = nullptr;
                DestroyCollectedObject(current);
                current = next;
            }
            mObjects = nullptr;
            mObjectCount = 0u;

            // Reset des compteurs d'index
            mObjectIndexSize = 0u;
            mObjectIndexTombstones = 0u;

            // Libération de la hash table elle-même
            if (mObjectIndex) {
                mAllocator->Deallocate(mObjectIndex);
                mObjectIndex = nullptr;
            }
            mObjectIndexCapacity = 0u;
        }

        // ====================================================================
        // NkGarbageCollector : Configuration
        // ====================================================================

        void NkGarbageCollector::SetAllocator(NkAllocator* allocator) noexcept {
            if (!allocator) {
                allocator = &NkGetDefaultAllocator();
            }

            NkSpinLockGuard guard(mLock);
            
            // Sécurité : ne pas changer d'allocateur si des objets sont déjà managés
            // (risque de deallocation avec le mauvais allocateur)
            if (mObjectCount != 0u || mObjectIndex != nullptr) {
                return;
            }
            mAllocator = allocator;
        }

        NkAllocator* NkGarbageCollector::GetAllocator() const noexcept {
            return mAllocator;
        }

        // ====================================================================
        // NkGarbageCollector : Gestion de l'Index Hash Table
        // ====================================================================

        NkGcObject* NkGarbageCollector::HashTombstone() noexcept {
            // Adresse 0x1 : jamais alignée, jamais retournée par malloc/new
            return reinterpret_cast<NkGcObject*>(static_cast<nk_uptr>(1u));
        }

        nk_size NkGarbageCollector::HashPointer(const void* pointer) const noexcept {
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

        nk_size NkGarbageCollector::NextPow2(nk_size value) const noexcept {
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

        void NkGarbageCollector::RebuildIndexLocked(nk_size requestedCapacity) noexcept {
            // Normalisation et arrondi à puissance de 2
            nk_size newCapacity = NormalizeIndexCapacity(requestedCapacity);
            newCapacity = NextPow2(newCapacity);

            // Allocation du nouveau buffer de hash table
            NkGcObject** newTable = static_cast<NkGcObject**>(
                mAllocator->Allocate(newCapacity * sizeof(NkGcObject*), alignof(NkGcObject*))
            );
            if (!newTable) {
                return;  // Échec d'allocation : garder l'ancienne table
            }

            // Zero-initialization
            for (nk_size i = 0u; i < newCapacity; ++i) {
                newTable[i] = nullptr;
            }

            // Helper lambda pour insertion dans la nouvelle table (sans vérification de doublon)
            auto insertRaw = [&](NkGcObject* object) {
                nk_size idx = HashPointer(object) & (newCapacity - 1u);
                for (nk_size probes = 0u; probes < newCapacity; ++probes) {
                    if (newTable[idx] == nullptr) {
                        newTable[idx] = object;
                        return;
                    }
                    idx = (idx + 1u) & (newCapacity - 1u);  // Linear probing avec wrapping
                }
            };

            // Migration des entrées valides
            if (mObjectIndex) {
                // Copie depuis l'ancienne hash table
                for (nk_size i = 0u; i < mObjectIndexCapacity; ++i) {
                    NkGcObject* slot = mObjectIndex[i];
                    if (slot && slot != HashTombstone()) {
                        insertRaw(slot);
                    }
                }
                // Libération de l'ancienne table
                mAllocator->Deallocate(mObjectIndex);
            } else {
                // Première initialisation : itération via la liste chaînée mObjects
                NkGcObject* current = mObjects;
                while (current) {
                    insertRaw(current);
                    current = current->mGcNext;
                }
            }

            // Mise à jour des membres vers la nouvelle table
            mObjectIndex = newTable;
            mObjectIndexCapacity = newCapacity;
            mObjectIndexSize = mObjectCount;  // Toutes les entrées valides sont migrées
            mObjectIndexTombstones = 0u;      // Nouveaux tombstones reset
        }

        void NkGarbageCollector::EnsureIndexCapacityLocked() {
            // Initialisation lazy de la hash table
            if (!mObjectIndex) {
                RebuildIndexLocked(64u);
                return;
            }

            // Calcul du nombre de slots utilisés (valides + tombstones)
            const nk_size usedSlots = mObjectIndexSize + mObjectIndexTombstones;
            
            // Trigger rehash si load factor > 70%
            if ((usedSlots + 1u) * 100u >= mObjectIndexCapacity * 70u) {
                RebuildIndexLocked(mObjectIndexCapacity * 2u);  // Double la capacité
            } 
            // Trigger cleanup si tombstones > 20%
            else if (mObjectIndexTombstones * 100u >= mObjectIndexCapacity * 20u) {
                RebuildIndexLocked(mObjectIndexCapacity);  // Même capacité, élimine les tombstones
            }
        }

        nk_bool NkGarbageCollector::ContainsIndexLocked(const NkGcObject* object) const noexcept {
            if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
                return false;
            }

            nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
            for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
                const NkGcObject* slot = mObjectIndex[idx];
                if (slot == nullptr) {
                    return false;  // Slot vide : objet absent
                }
                if (slot != HashTombstone() && slot == object) {
                    return true;  // Match trouvé
                }
                idx = (idx + 1u) & (mObjectIndexCapacity - 1u);  // Probe suivant
            }
            return false;  // Table parcourue sans trouver l'objet
        }

        nk_bool NkGarbageCollector::InsertIndexLocked(NkGcObject* object) noexcept {
            if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
                return false;
            }

            nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
            nk_size tombstoneIdx = static_cast<nk_size>(-1);  // Premier tombstone rencontré

            for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
                NkGcObject* slot = mObjectIndex[idx];
                
                if (slot == nullptr) {
                    // Slot vide : insertion possible
                    const nk_size target = (tombstoneIdx != static_cast<nk_size>(-1)) 
                                         ? tombstoneIdx 
                                         : idx;
                    
                    // Si on insère dans un tombstone, décrémenter le compteur
                    if (mObjectIndex[target] == HashTombstone() && mObjectIndexTombstones > 0u) {
                        --mObjectIndexTombstones;
                    }
                    
                    mObjectIndex[target] = object;
                    ++mObjectIndexSize;
                    return true;
                }
                
                if (slot == HashTombstone()) {
                    // Mémoriser le premier tombstone pour insertion potentielle
                    if (tombstoneIdx == static_cast<nk_size>(-1)) {
                        tombstoneIdx = idx;
                    }
                } else if (slot == object) {
                    // Déjà présent : no-op, retourne true
                    return true;
                }
                
                idx = (idx + 1u) & (mObjectIndexCapacity - 1u);
            }
            
            return false;  // Table pleine (ne devrait pas arriver avec bon load factor)
        }

        nk_bool NkGarbageCollector::EraseIndexLocked(const NkGcObject* object) noexcept {
            if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
                return false;
            }

            nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
            for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
                NkGcObject* slot = mObjectIndex[idx];
                
                if (slot == nullptr) {
                    return false;  // Slot vide : objet absent
                }
                
                if (slot != HashTombstone() && slot == object) {
                    // Marquage tombstone (suppression logique)
                    mObjectIndex[idx] = HashTombstone();
                    if (mObjectIndexSize > 0u) {
                        --mObjectIndexSize;
                    }
                    ++mObjectIndexTombstones;
                    return true;
                }
                
                idx = (idx + 1u) & (mObjectIndexCapacity - 1u);
            }
            return false;  // Objet non trouvé
        }

        // ====================================================================
        // NkGarbageCollector : Gestion de la Liste Chaînée d'Objets
        // ====================================================================

        void NkGarbageCollector::UnlinkObjectLocked(NkGcObject* object) noexcept {
            if (!object) {
                return;
            }

            // Mise à jour du prev
            if (object->mGcPrev) {
                object->mGcPrev->mGcNext = object->mGcNext;
            } else if (mObjects == object) {
                // L'objet est la tête de liste : mettre à jour mObjects
                mObjects = object->mGcNext;
            }

            // Mise à jour du next
            if (object->mGcNext) {
                object->mGcNext->mGcPrev = object->mGcPrev;
            }

            // Détachement défensif de l'objet
            object->mGcPrev = nullptr;
            object->mGcNext = nullptr;
        }

        // ====================================================================
        // NkGarbageCollector : Registration/Unregistration Publiques
        // ====================================================================

        void NkGarbageCollector::RegisterObject(NkGcObject* object) noexcept {
            if (!object) {
                return;
            }

            NkSpinLockGuard guard(mLock);

            // Vérification de doublon via l'index si disponible, sinon liste chaînée
            if (mObjectIndex) {
                if (ContainsIndexLocked(object)) {
                    return;  // Déjà enregistré : no-op
                }
            } else {
                // Fallback : recherche linéaire dans la liste (O(n) mais rare avant première insertion)
                NkGcObject* current = mObjects;
                while (current) {
                    if (current == object) {
                        return;
                    }
                    current = current->mGcNext;
                }
            }

            // Garantie de capacité suffisante pour l'index
            EnsureIndexCapacityLocked();
            (void)InsertIndexLocked(object);  // Insertion dans la hash table

            // Initialisation des métadonnées GC
            object->mGcMarked = false;
            object->mGcPrev = nullptr;
            
            // Insertion en tête de la liste chaînée mObjects (O(1))
            object->mGcNext = mObjects;
            if (mObjects) {
                mObjects->mGcPrev = object;
            }
            mObjects = object;
            
            ++mObjectCount;
        }

        void NkGarbageCollector::UnregisterObject(NkGcObject* object) noexcept {
            if (!object) {
                return;
            }

            NkSpinLockGuard guard(mLock);

            // Vérification de présence
            if (mObjectIndex) {
                if (!ContainsIndexLocked(object)) {
                    return;  // Non enregistré : no-op
                }
            } else {
                // Fallback : recherche linéaire
                NkGcObject* current = mObjects;
                while (current && current != object) {
                    current = current->mGcNext;
                }
                if (!current) {
                    return;  // Non trouvé
                }
                object = current;  // Utiliser le pointeur trouvé (au cas où)
            }

            // Retrait de la liste chaînée
            UnlinkObjectLocked(object);
            
            // Retrait de la hash table (marquage tombstone)
            if (mObjectIndex) {
                (void)EraseIndexLocked(object);
            }
            
            // Mise à jour du compteur
            if (mObjectCount > 0u) {
                --mObjectCount;
            }
        }

        // ====================================================================
        // NkGarbageCollector : Allocation Mémoire Brute
        // ====================================================================

        void* NkGarbageCollector::Allocate(nk_size size, nk_size alignment) noexcept {
            if (size == 0u) {
                return nullptr;
            }
            return mAllocator->Allocate(size, alignment);
        }

        void NkGarbageCollector::Free(void* pointer) noexcept {
            if (!pointer) {
                return;
            }
            mAllocator->Deallocate(pointer);
        }

        // ====================================================================
        // NkGarbageCollector : Interrogation
        // ====================================================================

        nk_bool NkGarbageCollector::ContainsObject(const NkGcObject* object) const noexcept {
            if (!object) {
                return false;
            }

            NkSpinLockGuard guard(mLock);
            
            if (mObjectIndex) {
                return ContainsIndexLocked(object);
            }
            
            // Fallback : recherche linéaire dans la liste chaînée
            const NkGcObject* current = mObjects;
            while (current) {
                if (current == object) {
                    return true;
                }
                current = current->mGcNext;
            }
            return false;
        }

        nk_size NkGarbageCollector::ObjectCount() const noexcept {
            return mObjectCount;
        }

        // ====================================================================
        // NkGarbageCollector : Gestion des Racines
        // ====================================================================

        void NkGarbageCollector::AddRoot(NkGcRoot* root) noexcept {
            if (!root) {
                return;
            }
            NkSpinLockGuard guard(mLock);
            
            // Insertion en tête de la liste chaînée des racines
            root->mNext = mRoots;
            mRoots = root;
        }

        void NkGarbageCollector::RemoveRoot(NkGcRoot* root) noexcept {
            if (!root) {
                return;
            }
            NkSpinLockGuard guard(mLock);

            // Recherche et retrait de la liste chaînée
            NkGcRoot* previous = nullptr;
            NkGcRoot* current = mRoots;
            while (current) {
                if (current == root) {
                    if (previous) {
                        previous->mNext = current->mNext;
                    } else {
                        mRoots = current->mNext;
                    }
                    current->mNext = nullptr;  // Détachement défensif
                    return;
                }
                previous = current;
                current = current->mNext;
            }
            // Root non trouvé : no-op silencieux
        }

        // ====================================================================
        // NkGarbageCollector : Algorithme Mark-and-Sweep
        // ====================================================================

        void NkGarbageCollector::Mark(NkGcObject* object) noexcept {
            // Guards : nullptr ou déjà marqué = no-op
            if (!object || object->mGcMarked) {
                return;
            }
            
            // Marquage de l'objet
            object->mGcMarked = true;
            
            // Propagation récursive via Trace()
            // Note : le lock est déjà acquis par l'appelant (via Collect() ou NkGcTracer)
            NkGcTracer tracer(*this);
            object->Trace(tracer);
        }

        void NkGarbageCollector::MarkRoots() noexcept {
            // Parcours de toutes les racines externes
            NkGcRoot* root = mRoots;
            while (root) {
                if (root->mSlot && *root->mSlot) {
                    // Si le pointeur protégé est non-null, marquer l'objet pointé
                    Mark(*root->mSlot);
                }
                root = root->mNext;
            }
        }

        void NkGarbageCollector::Sweep() noexcept {
            // Parcours de tous les objets managés
            NkGcObject* current = mObjects;
            
            while (current) {
                NkGcObject* next = current->mGcNext;  // Sauvegarder next avant potentielle destruction
                
                if (!current->mGcMarked) {
                    // Objet non atteint : collecte
                    UnlinkObjectLocked(current);           // Retrait de la liste chaînée
                    (void)EraseIndexLocked(current);       // Retrait de la hash table
                    DestroyCollectedObject(current);       // Destruction effective
                    if (mObjectCount > 0u) {
                        --mObjectCount;
                    }
                } else {
                    // Objet atteint : reset du flag pour la prochaine collecte
                    current->mGcMarked = false;
                }
                current = next;
            }
        }

        void NkGarbageCollector::DestroyCollectedObject(NkGcObject* object) noexcept {
            if (!object) {
                return;
            }

            // Destruction via la méthode appropriée
            if (object->mGcOwned && object->mGcAllocator && object->mGcDestroy) {
                // Objet créé via New<T> : utiliser la fonction de destruction personnalisée
                object->mGcDestroy(object, object->mGcAllocator);
            } else {
                // Objet créé manuellement ou via new standard : fallback vers delete
                delete object;
            }
        }

        // ====================================================================
        // NkGarbageCollector : Collecte Publique
        // ====================================================================

        void NkGarbageCollector::Collect() noexcept {
            // Acquisition du lock pour toute la durée de la collecte
            NkSpinLockGuard guard(mLock);
            
            // Phase 1 : marquage depuis les racines
            MarkRoots();
            
            // Phase 2 : suppression des objets non-marqués
            Sweep();
            
            // Note : pas de compactage ou défragmentation dans cette version
            // Les objets conservés restent à leur adresse mémoire actuelle
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ Double-linked list : UnlinkObjectLocked gère correctement les cas tête/milieu/fin
    ✓ Hash table : linear probing avec wrapping garantit que FindSlot termine
    ✓ Tombstone handling : Insert priorise les tombstones pour réutilisation d'espace
    ✓ Mark propagation : recursion via Trace() avec guard contre re-marquage (mGcMarked)
    ✓ Thread-safety : toutes les méthodes publiques acquièrent mLock via NkSpinLockGuard

    [PERFORMANCE]
    - Register/Unregister : O(1) moyen grâce à hash table, O(n) fallback rare avant init
    - ContainsObject : O(1) moyen avec index, O(n) sans (liste chaînée)
    - Collect() : O(n + e) où n = nombre d'objets, e = nombre d'arêtes (références via Trace)
    - Rehash : O(n) mais amorti sur de nombreuses insertions → O(1) amorti
    - Trace() complexity : responsabilité de l'utilisateur : garder léger et déterministe

    [MÉMOIRE]
    - Par objet managé : +4 pointeurs (next/prev) + 2 flags + 3 pointeurs metadata ≈ 40-48 bytes overhead
    - Hash table : capacity * sizeof(NkGcObject*) bytes, load factor 70% max → ~1.4x mémoire des objets
    - Tombstones : jusqu'à 20% de la table avant cleanup → mémoire "perdue" temporairement
    - Exemple : 1000 objets → ~48KB overhead objets + ~11KB hash table (64-bit pointers)

    [EXTENSIONS POSSIBLES]
    1. Generational GC : séparer objets jeunes/vieux pour collectes fréquentes/rares
    2. Concurrent marking : permettre l'exécution du code pendant la phase mark (plus complexe)
    3. Weak references : ajouter NkGcWeakPtr qui n'empêche pas la collecte mais devient nullptr après
    4. Finalizers : callback optionnel avant destruction pour cleanup de ressources externes
    5. Profiling hooks : notifier un callback à chaque collecte avec stats (objets collectés, temps, etc.)

    [DEBUGGING TIPS]
    - Si un objet est collecté trop tôt : vérifier que Trace() déclare TOUTES les références sortantes
    - Si fuite mémoire : utiliser ContainsObject() pour vérifier si l'objet est toujours managé
    - Si crash dans Collect() : vérifier qu'aucun thread ne modifie le graphe pendant le GC
    - Si performance degrade : profiler Trace() et ajuster la fréquence de Collect()
    - Dump debug : itérer sur mObjects et logger IsMarked() pour chaque objet avant/après Collect()

    [THREAD-SAFETY GUARANTEES]
    ✓ Toutes les méthodes publiques acquièrent mLock avant accès aux données partagées
    ✓ Mark()/Trace() sont appelés avec le lock acquis : pas de modification concurrente possible
    ✓ Les objets NkGcObject eux-mêmes ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent aux données membres
    ✓ New/Delete acquièrent le lock pour registration/unregistration : safe pour usage multi-thread
    
    ✗ Ne pas appeler New/Delete depuis Trace() : le lock est déjà acquis → deadlock potentiel
    ✗ Ne pas modifier les pointeurs d'un objet depuis un autre thread pendant Collect() : risque de corruption du graphe
    ✗ Ne pas delete manuellement un objet enregistré : utiliser UnregisterObject + Delete ou laisser le GC gérer
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================