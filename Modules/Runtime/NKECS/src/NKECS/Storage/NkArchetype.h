// =============================================================================
// Storage/NkArchetype.h
// =============================================================================
// Un archétype représente une signature unique de composants.
// Il stocke toutes les entités partageant exactement le même ensemble de types.
//
// Structure interne :
//   - NkComponentMask     : bitset indiquant quels composants sont présents.
//   - NkComponentPool[]   : un pool par type de composant (layout SoA).
//   - entityIds[]         : tableau associant chaque ligne (row) à son NkEntityId.
//
// Cycle de vie des entités et migration :
//   1. Ajout ou retrait d'un composant → calcul du nouvel archétype cible.
//   2. Migration : déplacement des données depuis l'archétype source vers la cible.
//   3. Suppression dans la source par swap‑remove (O(1)).
//   4. Mise à jour du NkEntityIndex (archetypeId + row).
//
// Itération efficace :
//   for each archetype matching query mask :
//       for each row in archetype :
//           process(entityId, components...)
// =============================================================================

#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Storage/NkComponentPool.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkEntityRecord
        // =====================================================================
        // Enregistrement de la position exacte d'une entité vivante dans le stockage.
        // Stocké dans le NkEntityIndex pour un accès O(1) à l'entité.
        struct NkEntityRecord {
            // Identifiant de l'archétype où réside actuellement l'entité.
            NkArchetypeId archetypeId = kInvalidArchetypeId;

            // Index de la ligne dans les pools de cet archétype.
            // La valeur 0xFFFFFFFFu sert de sentinelle "invalide".
            uint32        row         = 0xFFFFFFFFu;
        };

        // =====================================================================
        // NkArchetype
        // =====================================================================
        // Représente un groupe homogène d'entités partageant la même combinaison
        // de composants. Implémente un stockage SoA (Structure of Arrays) pour
        // maximiser la localité spatiale et faciliter les optimisations SIMD.
        class NkArchetype {
            public:
                // -----------------------------------------------------------------
                // Constructeur
                // -----------------------------------------------------------------
                // Construit un archétype avec l'identifiant `id` et le masque `mask`.
                // Pour chaque bit actif dans `mask`, une NkComponentPool est allouée
                // et initialisée avec les métadonnées du type correspondant.
                explicit NkArchetype(NkArchetypeId id, const NkComponentMask& mask) noexcept
                    : mId(id)
                    , mMask(mask) {
                    // Récupération du registre global des types.
                    NkTypeRegistry& reg = NkTypeRegistry::Global();
                    mNumPools = 0;

                    // Parcours de tous les bits actifs du masque.
                    mask.ForEach([&](NkComponentId cid) {
                        // Protection contre un dépassement de capacité (ne devrait pas arriver).
                        if (mNumPools >= kMaxComponentTypes) {
                            return;
                        }

                        // Récupération des métadonnées du composant (taille, alignement, etc.).
                        const ComponentMeta* meta = reg.Get(cid);
                        NKECS_ASSERT(meta != nullptr);

                        // Stockage de l'ID et création de la pool associée.
                        mPoolIds[mNumPools] = cid;
                        mPools[mNumPools]   = NkComponentPool(meta);

                        // Table de correspondance inverse : ID → index dans le tableau local.
                        mPoolIndex[cid]     = mNumPools;

                        // Incrémentation du nombre de pools gérées par cet archétype.
                        ++mNumPools;
                    });
                }

                // -----------------------------------------------------------------
                // Destructeur
                // -----------------------------------------------------------------
                // Par défaut, les pools libèrent automatiquement leur mémoire.
                ~NkArchetype() noexcept = default;

                // -----------------------------------------------------------------
                // Sémantique de copie interdite (ressources uniques)
                // -----------------------------------------------------------------
                NkArchetype(const NkArchetype&) = delete;
                NkArchetype& operator=(const NkArchetype&) = delete;

                // -----------------------------------------------------------------
                // Sémantique de déplacement autorisée (pour les conteneurs)
                // -----------------------------------------------------------------
                NkArchetype(NkArchetype&&) noexcept = default;
                NkArchetype& operator=(NkArchetype&&) noexcept = default;

                // -----------------------------------------------------------------
                // Accesseurs (lecture seule)
                // -----------------------------------------------------------------
                [[nodiscard]] NkArchetypeId          Id()    const noexcept { return mId;    }
                [[nodiscard]] const NkComponentMask& Mask()  const noexcept { return mMask;  }
                [[nodiscard]] uint32                 Count() const noexcept { return mSize;  }
                [[nodiscard]] bool                   Empty() const noexcept { return mSize == 0; }

                // Vérifie si le composant identifié par `cid` fait partie de l'archétype.
                [[nodiscard]] bool Has(NkComponentId cid) const noexcept {
                    return mMask.Has(cid);
                }

                // Retourne un pointeur vers la pool du composant `cid`, ou nullptr s'il est absent.
                [[nodiscard]] NkComponentPool* GetPool(NkComponentId cid) noexcept {
                    const uint32 idx = PoolIndex(cid);
                    return (idx != kInvalidPoolIndex) ? &mPools[idx] : nullptr;
                }

                // Version const du précédent.
                [[nodiscard]] const NkComponentPool* GetPool(NkComponentId cid) const noexcept {
                    const uint32 idx = PoolIndex(cid);
                    return (idx != kInvalidPoolIndex) ? &mPools[idx] : nullptr;
                }

                // Récupère l'identifiant de l'entité située à la ligne `row`.
                [[nodiscard]] NkEntityId GetEntity(uint32 row) const noexcept {
                    NKECS_ASSERT(row < mSize);
                    return mEntityIds[row];
                }

                // Vue en lecture seule sur le tableau des identifiants d'entités.
                [[nodiscard]] NkSpan<const NkEntityId> Entities() const noexcept {
                    return { mEntityIds, mSize };
                }

                // -----------------------------------------------------------------
                // Mutations (ajout / suppression d'entités dans l'archétype)
                // -----------------------------------------------------------------

                // Ajoute une nouvelle entité avec tous ses composants construits par défaut.
                // Retourne l'index de la ligne allouée.
                uint32 AddEntity(NkEntityId id) noexcept {
                    const uint32 row = mSize;

                    // Agrandissement du tableau d'identifiants si nécessaire.
                    if (mSize >= mEntityCapacity) {
                        GrowEntityArray();
                    }

                    // Stockage de l'identifiant à la fin.
                    mEntityIds[mSize] = id;

                    // Construction par défaut dans chaque pool de composants.
                    for (uint32 i = 0; i < mNumPools; ++i) {
                        mPools[i].PushDefault();
                    }

                    // Incrémentation de la taille logique.
                    ++mSize;
                    return row;
                }

                // Supprime une entité en utilisant la technique du swap‑remove (O(1)).
                // L'entité située en dernière position est déplacée vers la ligne `row`.
                // Retourne l'identifiant de l'entité déplacée, ou Invalid si aucune.
                NkEntityId RemoveEntity(uint32 row) noexcept {
                    NKECS_ASSERT(row < mSize);

                    const uint32 last = mSize - 1;
                    NkEntityId swappedEntity = NkEntityId::Invalid();

                    if (row != last) {
                        // Swap‑remove dans chaque pool.
                        for (uint32 i = 0; i < mNumPools; ++i) {
                            mPools[i].SwapRemove(row);
                        }

                        // Mise à jour du tableau d'entités.
                        mEntityIds[row] = mEntityIds[last];
                        swappedEntity   = mEntityIds[row];
                    } else {
                        // Cas où l'entité à supprimer est déjà la dernière.
                        for (uint32 i = 0; i < mNumPools; ++i) {
                            mPools[i].SwapRemove(row);
                        }
                    }

                    // Réduction de la taille logique.
                    --mSize;
                    return swappedEntity;
                }

                // Migre une entité depuis un autre archétype (`src`) vers celui‑ci.
                // Les composants communs sont déplacés ; les nouveaux sont construits par défaut.
                // Retourne la ligne où l'entité a été placée dans cet archétype.
                uint32 MigrateFrom(NkArchetype& src, uint32 srcRow, NkEntityId entityId) noexcept {
                    const uint32 dstRow = mSize;

                    // Agrandissement du tableau d'entités si nécessaire.
                    if (mSize >= mEntityCapacity) {
                        GrowEntityArray();
                    }

                    // Placement de l'identifiant.
                    mEntityIds[mSize] = entityId;

                    // Parcours de toutes les pools du présent archétype.
                    for (uint32 i = 0; i < mNumPools; ++i) {
                        const NkComponentId cid = mPoolIds[i];
                        NkComponentPool* srcPool = src.GetPool(cid);

                        if (srcPool != nullptr) {
                            // Le composant existe dans la source → déplacement.
                            mPools[i].MoveFrom(*srcPool, srcRow);
                        } else {
                            // Nouveau composant → construction par défaut.
                            mPools[i].PushDefault();
                        }
                    }

                    ++mSize;
                    return dstRow;
                }

                // Itère sur chaque ligne de l'archétype, en fournissant le numéro de ligne
                // et l'identifiant de l'entité.
                // Utile pour les opérations qui nécessitent l'ID en plus des composants.
                template<typename Fn>
                void ForEachRow(Fn&& fn) const noexcept {
                    for (uint32 row = 0; row < mSize; ++row) {
                        fn(row, mEntityIds[row]);
                    }
                }

                // -----------------------------------------------------------------
                // Accès typé aux composants
                // -----------------------------------------------------------------

                // Récupère un pointeur vers le composant de type T à la ligne `row`.
                // Retourne nullptr si le composant n'est pas présent dans l'archétype.
                template<typename T>
                [[nodiscard]] T* GetComponent(uint32 row) noexcept {
                    const NkComponentId cid = NkIdOf<T>();
                    NkComponentPool* pool = GetPool(cid);
                    if (pool == nullptr) {
                        return nullptr;
                    }
                    return pool->AtTyped<T>(row);
                }

                // Version const du précédent.
                template<typename T>
                [[nodiscard]] const T* GetComponent(uint32 row) const noexcept {
                    const NkComponentId cid = NkIdOf<T>();
                    const NkComponentPool* pool = GetPool(cid);
                    if (pool == nullptr) {
                        return nullptr;
                    }
                    return pool->AtTyped<T>(row);
                }

                // Retourne un pointeur vers le tableau brut des composants de type T.
                // Permet une itération ultra‑rapide ou des traitements SIMD.
                template<typename T>
                [[nodiscard]] T* ComponentData() noexcept {
                    const NkComponentId cid = NkIdOf<T>();
                    NkComponentPool* pool = GetPool(cid);
                    if (pool == nullptr) {
                        return nullptr;
                    }
                    return pool->Data<T>();
                }

                // Écrit (copie) une valeur dans le composant de type T à la ligne `row`.
                template<typename T>
                void SetComponent(uint32 row, const T& value) noexcept {
                    T* ptr = GetComponent<T>(row);
                    if (ptr != nullptr) {
                        *ptr = value;
                    }
                }

            private:
                // -----------------------------------------------------------------
                // Constante interne : index de pool invalide.
                // -----------------------------------------------------------------
                static constexpr uint32 kInvalidPoolIndex = 0xFFFFFFFFu;

                // Retourne l'index local de la pool correspondant à un ComponentId,
                // ou kInvalidPoolIndex si le composant n'est pas présent.
                [[nodiscard]] uint32 PoolIndex(NkComponentId cid) const noexcept {
                    if (cid >= kMaxComponentTypes) {
                        return kInvalidPoolIndex;
                    }
                    const uint32 idx = mPoolIndex[cid];
                    // Double vérification : l'index doit être valide ET le bit présent dans le masque.
                    return (idx != kInvalidPoolIndex && mMask.Has(cid)) ? idx : kInvalidPoolIndex;
                }

                // Agrandit dynamiquement le tableau `mEntityIds` (stratégie 1.5x).
                void GrowEntityArray() noexcept {
                    const uint32 newCap = (mEntityCapacity < 8u)
                                            ? 8u
                                            : mEntityCapacity + mEntityCapacity / 2u;

                    NkEntityId* newArr = new NkEntityId[newCap];

                    if (mEntityIds != nullptr) {
                        std::memcpy(newArr, mEntityIds, mSize * sizeof(NkEntityId));
                        delete[] mEntityIds;
                    }

                    mEntityIds      = newArr;
                    mEntityCapacity = newCap;
                }

                // -----------------------------------------------------------------
                // Membres de l'archétype
                // -----------------------------------------------------------------
                NkArchetypeId    mId       = kInvalidArchetypeId;  // Identifiant unique
                NkComponentMask  mMask     = {};                   // Signature des composants
                uint32           mSize     = 0;                    // Nombre d'entités vivantes

                // Pools de composants – une par type présent dans l'archétype.
                NkComponentPool  mPools[kMaxComponentTypes]    = {};
                NkComponentId    mPoolIds[kMaxComponentTypes]  = {}; // Tableau parallèle des IDs
                uint32           mPoolIndex[kMaxComponentTypes];     // Mapping ComponentId → index local
                uint32           mNumPools  = 0;                     // Nombre réel de pools

                // Tableau dynamique des identifiants d'entités (alloué séparément).
                NkEntityId*      mEntityIds       = nullptr;
                uint32           mEntityCapacity  = 0;
        };

        // =====================================================================
        // NkEntityIndex
        // =====================================================================
        // Table de correspondance globale entité → (archétype, ligne).
        // Garantit un accès O(1) aux données de n'importe quelle entité.
        //
        // Elle gère également la génération (gen) pour détecter les "dangling
        // entities" : après destruction d'une entité, son ancien NkEntityId
        // devient automatiquement invalide car la génération ne correspond plus.
        // =====================================================================
        class NkEntityIndex {
            public:
                // -----------------------------------------------------------------
                // Entrée de la table
                // -----------------------------------------------------------------
                struct Entry {
                    NkEntityRecord record;   // Position actuelle (archétype + ligne)
                    uint32         gen  = 0; // Génération courante (0 = jamais allouée)
                    bool           alive = false;
                };

                // -----------------------------------------------------------------
                // Allocation d'un nouvel identifiant d'entité
                // -----------------------------------------------------------------
                // Réutilise un index libéré (via free list) ou étend le tableau.
                // Retourne un NkEntityId valide ou Invalid() si plus de place.
                [[nodiscard]] NkEntityId Allocate() noexcept {
                    // Réutilisation d'un index précédemment libéré.
                    if (!mFreeList.Empty()) {
                        const uint32 idx = mFreeList.Back();
                        mFreeList.PopBack();

                        Entry& e = mEntries[idx];
                        e.alive  = true;
                        e.record = {};                // Sera renseigné lors de l'ajout dans un archétype
                        return { idx, e.gen };
                    }

                    // Aucun index libre → on étend le tableau.
                    if (mNumEntries >= kMaxEntities) {
                        return NkEntityId::Invalid(); // Capacité maximale atteinte
                    }

                    const uint32 idx = mNumEntries;
                    ++mNumEntries;

                    // Agrandissement du tableau si nécessaire.
                    GrowIfNeeded();

                    Entry& e = mEntries[idx];
                    e.gen    = 1u;   // Première génération (0 est réservée à "invalide")
                    e.alive  = true;
                    e.record = {};

                    return { idx, e.gen };
                }

                // -----------------------------------------------------------------
                // Libération d'une entité
                // -----------------------------------------------------------------
                // Invalide l'identifiant en incrémentant sa génération et marque
                // l'index comme réutilisable.
                void Free(NkEntityId id) noexcept {
                    if (!IsAlive(id)) {
                        return;
                    }

                    Entry& e = mEntries[id.index];
                    e.alive  = false;
                    ++e.gen;                     // Invalide tous les anciens NkEntityId pointant ici
                    e.record = {};

                    // Ajout de l'index dans la free list pour une future allocation.
                    mFreeList.PushBack(id.index);
                }

                // -----------------------------------------------------------------
                // Test de validité d'un identifiant
                // -----------------------------------------------------------------
                [[nodiscard]] bool IsAlive(NkEntityId id) const noexcept {
                    if (!id.IsValid() || id.index >= mNumEntries) {
                        return false;
                    }
                    const Entry& e = mEntries[id.index];
                    return e.alive && (e.gen == id.gen);
                }

                // -----------------------------------------------------------------
                // Accès à l'enregistrement de position
                // -----------------------------------------------------------------
                [[nodiscard]] NkEntityRecord* GetRecord(NkEntityId id) noexcept {
                    if (!IsAlive(id)) {
                        return nullptr;
                    }
                    return &mEntries[id.index].record;
                }

                [[nodiscard]] const NkEntityRecord* GetRecord(NkEntityId id) const noexcept {
                    if (!IsAlive(id)) {
                        return nullptr;
                    }
                    return &mEntries[id.index].record;
                }

                // -----------------------------------------------------------------
                // Mise à jour de la position d'une entité
                // -----------------------------------------------------------------
                void SetRecord(NkEntityId id, NkArchetypeId archetypeId, uint32 row) noexcept {
                    if (!IsAlive(id)) {
                        return;
                    }
                    Entry& e = mEntries[id.index];
                    e.record.archetypeId = archetypeId;
                    e.record.row         = row;
                }

                // -----------------------------------------------------------------
                // Statistiques
                // -----------------------------------------------------------------
                [[nodiscard]] uint32 AliveCount() const noexcept {
                    return mNumEntries - static_cast<uint32>(mFreeList.Size());
                }

                // -----------------------------------------------------------------
                // Destructeur (libération du tableau dynamique)
                // -----------------------------------------------------------------
                ~NkEntityIndex() noexcept {
                    delete[] mEntries;
                }

            private:
                // -----------------------------------------------------------------
                // FreeList interne
                // -----------------------------------------------------------------
                // Stocke les indices libérés pour une réutilisation rapide.
                // Implémentation simple avec un tableau de taille fixe.
                // La capacité de 256 est suffisante car la réutilisation est immédiate
                // dans la majorité des cas (c'est une file LIFO). Voir discussion
                // complémentaire plus bas.
                struct FreeList {
                    static constexpr uint32 kCapacity = 256;
                    uint32 data[kCapacity] = {};
                    uint32 size = 0;

                    void PushBack(uint32 v) noexcept {
                        if (size < kCapacity) {
                            data[size] = v;
                            ++size;
                        }
                        // Si la limite est atteinte, l'index est simplement perdu.
                        // Cela n'affecte pas la correction, seulement l'optimalité
                        // de la réutilisation (on allouera de nouveaux indices).
                    }

                    [[nodiscard]] uint32 Back() const noexcept {
                        return data[size - 1];
                    }

                    void PopBack() noexcept {
                        if (size > 0) {
                            --size;
                        }
                    }

                    [[nodiscard]] bool Empty() const noexcept {
                        return size == 0;
                    }

                    [[nodiscard]] uint32 Size() const noexcept {
                        return size;
                    }
                };

                // Agrandit le tableau d'entrées (stratégie de croissance 1.5x).
                void GrowIfNeeded() noexcept {
                    if (mNumEntries <= mCapacity) {
                        return;
                    }

                    const uint32 newCap = (mCapacity < 64u)
                                            ? 64u
                                            : mCapacity + mCapacity / 2u;

                    Entry* newArr = new Entry[newCap](); // () assure l'initialisation à zéro

                    if (mEntries != nullptr) {
                        std::memcpy(newArr, mEntries, mCapacity * sizeof(Entry));
                        delete[] mEntries;
                    }

                    mEntries  = newArr;
                    mCapacity = newCap;
                }

                Entry*   mEntries    = nullptr;   // Tableau dynamique des entrées
                uint32   mNumEntries = 0;         // Nombre d'entrées logiques allouées
                uint32   mCapacity   = 0;         // Capacité actuelle du tableau

                FreeList mFreeList;               // Liste des indices libérés
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKARCHETYPE.H
// =============================================================================
//
// Les archétypes sont le fondement du stockage SoA. Ils ne sont généralement
// pas manipulés directement par l'utilisateur, mais par NkWorld et NkArchetypeGraph.
// Ces exemples illustrent leur fonctionnement interne.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création manuelle d'un archétype et ajout d'entités
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetype.h"
    #include "NkECS/Core/NkTypeRegistry.h"

    // Composants de test.
    struct Position { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    NK_COMPONENT(Position);
    NK_COMPONENT(Velocity);

    void ExampleManualArchetype() {
        using namespace nkentseu::ecs;

        // Construction d'un masque contenant Position et Velocity.
        NkComponentMask mask;
        mask.Set(NkIdOf<Position>());
        mask.Set(NkIdOf<Velocity>());

        // Création d'un archétype (en pratique géré par NkArchetypeGraph).
        NkArchetype archetype(0, mask);

        // Ajout de quelques entités.
        NkEntityId e1 = {0, 1};
        NkEntityId e2 = {1, 1};
        uint32 row1 = archetype.AddEntity(e1);
        uint32 row2 = archetype.AddEntity(e2);

        // Accès aux composants et modification.
        Position* p1 = archetype.GetComponent<Position>(row1);
        if (p1) {
            p1->x = 10.0f;
            p1->y = 20.0f;
        }

        Velocity* v2 = archetype.GetComponent<Velocity>(row2);
        if (v2) {
            v2->vx = 1.0f;
        }

        // Itération sur toutes les entités de l'archétype.
        archetype.ForEachRow([&](uint32 row, NkEntityId id) {
            Position* p = archetype.GetComponent<Position>(row);
            Velocity* v = archetype.GetComponent<Velocity>(row);
            if (p && v) {
                p->x += v->vx;  // mise à jour basique
            }
        });

        // Suppression d'une entité (swap‑remove).
        NkEntityId swapped = archetype.RemoveEntity(row1);
        // `swapped` est l'entité qui a été déplacée pour combler le trou.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Migration d'une entité entre archétypes
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetype.h"

    void ExampleMigration() {
        using namespace nkentseu::ecs;

        // Création de deux archétypes : l'un avec Position seul, l'autre avec Position+Velocity.
        NkComponentMask maskPos;
        maskPos.Set(NkIdOf<Position>());

        NkComponentMask maskPosVel;
        maskPosVel.Set(NkIdOf<Position>());
        maskPosVel.Set(NkIdOf<Velocity>());

        NkArchetype archPos(0, maskPos);
        NkArchetype archPosVel(1, maskPosVel);

        // Ajout d'une entité dans le premier archétype.
        NkEntityId e = {0, 1};
        uint32 srcRow = archPos.AddEntity(e);
        Position* p = archPos.GetComponent<Position>(srcRow);
        p->x = 5.0f;

        // Migration vers l'archétype contenant Velocity.
        uint32 dstRow = archPosVel.MigrateFrom(archPos, srcRow, e);

        // Suppression de l'entité de l'archétype source (nécessaire après migration).
        archPos.RemoveEntity(srcRow);

        // Maintenant l'entité a à la fois Position et Velocity.
        Position* p2 = archPosVel.GetComponent<Position>(dstRow);
        Velocity* v2 = archPosVel.GetComponent<Velocity>(dstRow);
        // p2->x vaut 5.0f (conservé), v2 est default-construit.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation de NkEntityIndex pour le suivi des entités
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetype.h"

    void ExampleEntityIndex() {
        using namespace nkentseu::ecs;

        NkEntityIndex index;

        // Allocation de deux entités.
        NkEntityId e1 = index.Allocate();
        NkEntityId e2 = index.Allocate();

        assert(index.IsAlive(e1));
        assert(index.IsAlive(e2));

        // Association d'une position.
        index.SetRecord(e1, 0, 10);  // archetypeId=0, row=10
        index.SetRecord(e2, 1, 5);

        // Récupération de l'enregistrement.
        const NkEntityRecord* rec = index.GetRecord(e1);
        if (rec) {
            printf("Entity is in archetype %u at row %u\n", rec->archetypeId, rec->row);
        }

        // Destruction de e1.
        index.Free(e1);
        assert(!index.IsAlive(e1));

        // Réallocation : l'ancien index peut être réutilisé, mais avec une génération incrémentée.
        NkEntityId e3 = index.Allocate();
        // e3.index peut être égal à e1.index, mais e3.gen > e1.gen.
        // Ainsi, e1 reste invalide car sa génération ne correspond plus.

        // Nombre d'entités vivantes.
        uint32 alive = index.AliveCount(); // = 2 (e2 et e3)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Accès direct aux données pour itération rapide
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetype.h"

    void ExampleDirectDataAccess() {
        using namespace nkentseu::ecs;

        // Supposons un archétype contenant Position.
        NkComponentMask mask;
        mask.Set(NkIdOf<Position>());
        NkArchetype arch(0, mask);

        // Ajout de nombreuses entités...
        for (int i = 0; i < 1000; ++i) {
            arch.AddEntity({static_cast<uint32>(i), 1});
        }

        // Récupération du pointeur vers le tableau brut des Positions.
        Position* positions = arch.ComponentData<Position>();
        uint32 count = arch.Count();

        // Itération ultra-rapide (compatible SIMD).
        for (uint32 i = 0; i < count; ++i) {
            positions[i].x += 1.0f;
        }

        // On peut aussi parcourir en même temps les EntityIds si besoin.
        NkSpan<const NkEntityId> entities = arch.Entities();
        for (uint32 i = 0; i < count; ++i) {
            printf("Entity %u : x=%f\n", entities[i].index, positions[i].x);
        }
    }
*/

// =============================================================================