// =============================================================================
// Storage/NkArchetypeGraph.h
// =============================================================================
// Gère la création, la recherche et la mise en cache des archétypes.
//
// Lorsqu'un composant est ajouté ou retiré d'une entité via `world.Add<T>()`,
// le graphe détermine l'archétype cible correspondant à la nouvelle signature.
// Pour éviter de recalculer ou de recréer des archétypes identiques, un cache
// à deux niveaux est utilisé :
//   1. Cache Mask → ArchetypeId : associe directement une signature à son archétype.
//   2. Cache Edge (srcId + ComponentId + opération) → dstId : transitions fréquentes.
//
// Implémentation :
//   - Tables de hachage internes par adressage ouvert avec sondage linéaire.
//   - Fonction de hachage FNV‑1a (cohérente avec le reste du module).
//   - Aucune dépendance à la STL (évite std::unordered_map).
// =============================================================================

#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Storage/NkArchetype.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkArchetypeGraph
        // =====================================================================
        // Singleton implicite (un seul graphe par NkWorld). Responsable de :
        //   - Allouer de nouveaux archétypes.
        //   - Fournir l'archétype correspondant à un masque de composants.
        //   - Accélérer les transitions (ajout/retrait de composant) via un cache.
        class NkArchetypeGraph {
          public:
            // -----------------------------------------------------------------
            // Constructeur
            // -----------------------------------------------------------------
            // Crée automatiquement l'archétype vide (id = 0) qui ne contient aucun
            // composant. Cet archétype sert de point de départ pour les entités
            // nouvellement créées avant l'ajout de leur premier composant.
            NkArchetypeGraph() noexcept {
                mEmptyArchetypeId = AllocArchetype(NkComponentMask{});
            }

            // -----------------------------------------------------------------
            // Destructeur
            // -----------------------------------------------------------------
            // Libère tous les archétypes alloués dynamiquement.
            ~NkArchetypeGraph() noexcept {
                for (uint32 i = 0; i < mNumArchetypes; ++i) {
                    if (mArchetypes[i] != nullptr) {
                        delete mArchetypes[i];
                        mArchetypes[i] = nullptr;
                    }
                }
            }

            // -----------------------------------------------------------------
            // Sémantique de copie interdite (ressource unique par monde)
            // -----------------------------------------------------------------
            NkArchetypeGraph(const NkArchetypeGraph&) = delete;
            NkArchetypeGraph& operator=(const NkArchetypeGraph&) = delete;

            // -----------------------------------------------------------------
            // Accès aux archétypes par identifiant
            // -----------------------------------------------------------------
            [[nodiscard]] NkArchetype* Get(NkArchetypeId id) noexcept {
                if (id >= mNumArchetypes) {
                    return nullptr;
                }
                return mArchetypes[id];
            }

            [[nodiscard]] const NkArchetype* Get(NkArchetypeId id) const noexcept {
                if (id >= mNumArchetypes) {
                    return nullptr;
                }
                return mArchetypes[id];
            }

            // -----------------------------------------------------------------
            // Propriétés du graphe
            // -----------------------------------------------------------------
            // Retourne l'identifiant de l'archétype vide.
            [[nodiscard]] NkArchetypeId EmptyId() const noexcept {
                return mEmptyArchetypeId;
            }

            // Nombre total d'archétypes créés depuis le démarrage.
            [[nodiscard]] uint32 Count() const noexcept {
                return mNumArchetypes;
            }

            // -----------------------------------------------------------------
            // Recherche / création d'un archétype à partir d'un masque
            // -----------------------------------------------------------------
            // Retourne l'identifiant de l'archétype correspondant au masque `mask`.
            // Si aucun archétype n'existe encore avec cette signature, il est créé
            // et ajouté au cache.
            NkArchetypeId GetOrCreate(const NkComponentMask& mask) noexcept {
                const uint64 hash = mask.Hash();
                const NkArchetypeId cached = MaskLookup(hash, mask);
                if (cached != kInvalidArchetypeId) {
                    return cached;
                }
                return AllocArchetype(mask);
            }

            // -----------------------------------------------------------------
            // Transition : ajout d'un composant
            // -----------------------------------------------------------------
            // Étant donné un archétype source `srcId` et un composant `cid`,
            // retourne l'archétype destination après ajout de ce composant.
            // Le résultat est mis en cache pour les appels ultérieurs.
            NkArchetypeId AddComponent(NkArchetypeId srcId, NkComponentId cid) noexcept {
                // Construction de la clé de cache pour l'arête "add".
                const uint64 edgeKey = EdgeKey(srcId, cid, true);
                const NkArchetypeId cached = EdgeLookup(edgeKey);
                if (cached != kInvalidArchetypeId) {
                    return cached;
                }

                // Récupération de l'archétype source.
                NkArchetype* src = Get(srcId);
                NKECS_ASSERT(src != nullptr);

                // Construction du nouveau masque.
                NkComponentMask newMask = src->Mask();
                newMask.Set(cid);

                // Obtention (ou création) de l'archétype destination.
                const NkArchetypeId dstId = GetOrCreate(newMask);

                // Mise en cache de la transition.
                EdgeInsert(edgeKey, dstId);
                return dstId;
            }

            // -----------------------------------------------------------------
            // Transition : retrait d'un composant
            // -----------------------------------------------------------------
            // Symétrique de AddComponent : retourne l'archétype obtenu après
            // suppression du composant `cid` de l'archétype source.
            NkArchetypeId RemoveComponent(NkArchetypeId srcId, NkComponentId cid) noexcept {
                // Clé pour l'arête "remove".
                const uint64 edgeKey = EdgeKey(srcId, cid, false);
                const NkArchetypeId cached = EdgeLookup(edgeKey);
                if (cached != kInvalidArchetypeId) {
                    return cached;
                }

                NkArchetype* src = Get(srcId);
                NKECS_ASSERT(src != nullptr);

                NkComponentMask newMask = src->Mask();
                newMask.Clear(cid);

                const NkArchetypeId dstId = GetOrCreate(newMask);
                EdgeInsert(edgeKey, dstId);
                return dstId;
            }

            // -----------------------------------------------------------------
            // Itération sur les archétypes correspondant à une requête
            // -----------------------------------------------------------------
            // Parcourt tous les archétypes non vides dont le masque :
            //   - contient tous les bits de `required`,
            //   - ne contient aucun bit de `excluded`.
            // La fonction `fn` est appelée pour chaque archétype satisfaisant.
            template<typename Fn>
            void ForEachMatching(const NkComponentMask& required,
                                 const NkComponentMask& excluded,
                                 Fn&& fn) noexcept {
                for (uint32 i = 0; i < mNumArchetypes; ++i) {
                    NkArchetype* arch = mArchetypes[i];
                    if (arch == nullptr) {
                        continue;
                    }
                    if (arch->Empty()) {
                        continue;
                    }

                    const NkComponentMask& m = arch->Mask();

                    // Vérification des composants requis.
                    if (!m.ContainsAll(required)) {
                        continue;
                    }

                    // Vérification des composants exclus (si la liste n'est pas vide).
                    if (!excluded.IsEmpty() && m.HasAny(excluded)) {
                        continue;
                    }

                    // L'archétype correspond, on appelle le callback.
                    fn(arch);
                }
            }

          private:
            // -----------------------------------------------------------------
            // Allocation d'un nouvel archétype
            // -----------------------------------------------------------------
            // Crée un nouvel objet NkArchetype, l'enregistre dans le tableau
            // `mArchetypes`, l'ajoute au cache Mask → Id, puis retourne son ID.
            NkArchetypeId AllocArchetype(const NkComponentMask& mask) noexcept {
                NKECS_ASSERT(mNumArchetypes < kMaxArchetypes);

                const NkArchetypeId id = mNumArchetypes;
                ++mNumArchetypes;

                mArchetypes[id] = new NkArchetype(id, mask);

                // Ajout au cache Mask → Id.
                MaskInsert(mask.Hash(), mask, id);

                return id;
            }

            // =================================================================
            // Cache Mask → ArchetypeId
            // =================================================================
            // Table de hachage à adressage ouvert avec sondage linéaire.
            // Taille : puissance de deux pour un modulo rapide via masquage.

            // Taille de la table (doit être une puissance de deux).
            static constexpr uint32 kMaskTableSize = kMaxArchetypes * 2u;

            // Entrée du cache Mask.
            struct MaskEntry {
                uint64        hash = 0;                     // Hash du masque (pour comparaison rapide)
                NkComponentMask mask = {};                   // Copie du masque (pour résoudre les collisions)
                NkArchetypeId   id   = kInvalidArchetypeId; // Identifiant de l'archétype associé
                bool            used = false;                // Indique si l'entrée est occupée
            };

            // Recherche un archétype à partir du hash et du masque.
            // Retourne kInvalidArchetypeId si non trouvé.
            NkArchetypeId MaskLookup(uint64 hash, const NkComponentMask& mask) const noexcept {
                uint32 slot = static_cast<uint32>(hash) & (kMaskTableSize - 1u);

                for (uint32 probe = 0; probe < kMaskTableSize; ++probe) {
                    const MaskEntry& e = mMaskTable[slot];
                    if (!e.used) {
                        return kInvalidArchetypeId;
                    }
                    if (e.hash == hash && e.mask == mask) {
                        return e.id;
                    }
                    slot = (slot + 1u) & (kMaskTableSize - 1u);
                }

                return kInvalidArchetypeId;
            }

            // Insère une association (hash, mask) → id dans la table.
            void MaskInsert(uint64 hash, const NkComponentMask& mask, NkArchetypeId id) noexcept {
                uint32 slot = static_cast<uint32>(hash) & (kMaskTableSize - 1u);

                for (uint32 probe = 0; probe < kMaskTableSize; ++probe) {
                    MaskEntry& e = mMaskTable[slot];
                    if (!e.used) {
                        e.hash = hash;
                        e.mask = mask;
                        e.id   = id;
                        e.used = true;
                        return;
                    }
                    slot = (slot + 1u) & (kMaskTableSize - 1u);
                }

                // La table ne devrait jamais être pleine en pratique (dimensionnée pour kMaxArchetypes).
                NKECS_ASSERT(false && "Mask table full");
            }

            // =================================================================
            // Cache Edge (srcId + ComponentId + opération) → dstId
            // =================================================================
            // Accélère les transitions add/remove les plus fréquentes.

            // Taille de la table des arêtes.
            static constexpr uint32 kEdgeTableSize = kMaxArchetypes * 4u;

            // Entrée du cache Edge.
            struct EdgeEntry {
                uint64        key = 0;                     // Clé compactée (srcId, cid, add/remove)
                NkArchetypeId dst = kInvalidArchetypeId;   // Archétype destination
                bool          used = false;
            };

            // Construit une clé de 64 bits à partir des trois paramètres.
            // Bits :
            //   - srcId  : 32 bits (mais NkArchetypeId = uint32, donc ok)
            //   - cid    : 32 bits (NkComponentId)
            //   - add    : 1 bit (0 = remove, 1 = add)
            // On décale pour éviter les collisions entre les champs.
            static uint64 EdgeKey(NkArchetypeId src, NkComponentId cid, bool add) noexcept {
                return (static_cast<uint64>(src) << 33u)
                     | (static_cast<uint64>(cid) << 1u)
                     | (add ? 1ULL : 0ULL);
            }

            // Recherche une transition dans le cache Edge.
            NkArchetypeId EdgeLookup(uint64 key) const noexcept {
                uint32 slot = static_cast<uint32>(key) & (kEdgeTableSize - 1u);

                for (uint32 p = 0; p < kEdgeTableSize; ++p) {
                    const EdgeEntry& e = mEdgeTable[slot];
                    if (!e.used) {
                        return kInvalidArchetypeId;
                    }
                    if (e.key == key) {
                        return e.dst;
                    }
                    slot = (slot + 1u) & (kEdgeTableSize - 1u);
                }

                return kInvalidArchetypeId;
            }

            // Insère une transition dans le cache Edge.
            void EdgeInsert(uint64 key, NkArchetypeId dst) noexcept {
                uint32 slot = static_cast<uint32>(key) & (kEdgeTableSize - 1u);

                for (uint32 p = 0; p < kEdgeTableSize; ++p) {
                    EdgeEntry& e = mEdgeTable[slot];
                    if (!e.used) {
                        e.key  = key;
                        e.dst  = dst;
                        e.used = true;
                        return;
                    }
                    slot = (slot + 1u) & (kEdgeTableSize - 1u);
                }

                NKECS_ASSERT(false && "Edge table full");
            }

            // =================================================================
            // Membres privés
            // =================================================================
            // Tableau des archétypes alloués. La taille maximale est kMaxArchetypes.
            NkArchetype*  mArchetypes[kMaxArchetypes] = {};

            // Nombre d'archétypes actuellement alloués.
            uint32        mNumArchetypes              = 0;

            // Identifiant de l'archétype vide (créé dans le constructeur).
            NkArchetypeId mEmptyArchetypeId           = kInvalidArchetypeId;

            // Table de cache Mask → ArchetypeId.
            MaskEntry     mMaskTable[kMaskTableSize]  = {};

            // Table de cache Edge → dstId.
            EdgeEntry     mEdgeTable[kEdgeTableSize]  = {};
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKARCHETYPEGRAPH.H
// =============================================================================
//
// Le graphe d'archétypes est un composant interne de NkWorld. Il n'est
// généralement pas manipulé directement, mais ces exemples illustrent son
// fonctionnement pour les curieux ou pour l'extension du moteur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création manuelle d'un graphe et obtention d'archétypes
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetypeGraph.h"
    #include "NkECS/Core/NkTypeRegistry.h"

    // Composants de test.
    struct Position { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    NK_COMPONENT(Position);
    NK_COMPONENT(Velocity);

    void ExampleBasicGraph() {
        using namespace nkentseu::ecs;

        // Création du graphe (l'archétype vide est automatiquement créé).
        NkArchetypeGraph graph;

        // Récupération de l'archétype vide.
        NkArchetypeId emptyId = graph.EmptyId();
        const NkArchetype* emptyArch = graph.Get(emptyId);
        printf("Empty archetype has %u entities\n", emptyArch->Count());

        // Construction d'un masque avec Position.
        NkComponentMask maskPos;
        maskPos.Set(NkIdOf<Position>());

        // Obtention de l'archétype correspondant (créé si nécessaire).
        NkArchetypeId posId = graph.GetOrCreate(maskPos);
        NkArchetype* posArch = graph.Get(posId);

        // Ajout d'une entité dans cet archétype.
        NkEntityId e = {0, 1};
        posArch->AddEntity(e);

        // Affichage du nombre total d'archétypes créés.
        printf("Graph contains %u archetypes\n", graph.Count());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des transitions avec cache
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetypeGraph.h"

    void ExampleTransitions() {
        using namespace nkentseu::ecs;

        NkArchetypeGraph graph;

        // Création de l'archétype de base : Position seule.
        NkComponentMask maskPos;
        maskPos.Set(NkIdOf<Position>());
        NkArchetypeId posId = graph.GetOrCreate(maskPos);

        // Transition : ajout de Velocity.
        NkComponentId velId = NkIdOf<Velocity>();
        NkArchetypeId posVelId = graph.AddComponent(posId, velId);

        // La même transition appelée une seconde fois retourne immédiatement
        // le résultat depuis le cache (aucune création).
        NkArchetypeId cachedId = graph.AddComponent(posId, velId);
        assert(cachedId == posVelId);

        // Transition inverse : retrait de Velocity.
        NkArchetypeId backToPosId = graph.RemoveComponent(posVelId, velId);
        assert(backToPosId == posId);  // On retombe sur l'archétype original.

        // Vérification des signatures.
        const NkArchetype* archPosVel = graph.Get(posVelId);
        assert(archPosVel->Mask().Has(velId));
        assert(archPosVel->Mask().Has(NkIdOf<Position>()));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Itération sur les archétypes correspondant à une requête
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetypeGraph.h"

    void ExampleForEachMatching() {
        using namespace nkentseu::ecs;

        NkArchetypeGraph graph;

        // Création de plusieurs archétypes.
        NkComponentMask maskPos;
        maskPos.Set(NkIdOf<Position>());
        NkArchetypeId posId = graph.GetOrCreate(maskPos);

        NkComponentMask maskPosVel;
        maskPosVel.Set(NkIdOf<Position>());
        maskPosVel.Set(NkIdOf<Velocity>());
        NkArchetypeId posVelId = graph.GetOrCreate(maskPosVel);

        // Requête : tous les archétypes ayant Position, mais sans Velocity.
        NkComponentMask required;
        required.Set(NkIdOf<Position>());

        NkComponentMask excluded;
        excluded.Set(NkIdOf<Velocity>());

        // Parcours des archétypes correspondants.
        graph.ForEachMatching(required, excluded, [](NkArchetype* arch) {
            printf("Found archetype %u with %u entities\n", arch->Id(), arch->Count());
            // Ici, seul l'archétype posId sera listé (posVelId est exclu).
        });

        // Requête avec excluded vide : tous les archétypes ayant Position.
        NkComponentMask noExcluded;
        graph.ForEachMatching(required, noExcluded, [](NkArchetype* arch) {
            printf("Archetype %u matches (has Position)\n", arch->Id());
            // posId et posVelId seront listés.
        });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Simulation de l'ajout/retrait de composant d'une entité
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkArchetypeGraph.h"
    #include "NkECS/Storage/NkArchetype.h"

    void SimulateEntityComponentChange() {
        using namespace nkentseu::ecs;

        // Initialisation : graphe et index d'entités (pour la position).
        NkArchetypeGraph graph;
        NkEntityIndex entityIndex;

        // Création d'une entité.
        NkEntityId entity = entityIndex.Allocate();

        // Ajout d'un composant Position.
        NkComponentId posId = NkIdOf<Position>();
        NkArchetypeId srcId = graph.EmptyId();  // l'entité est actuellement "vide"
        NkArchetypeId dstId = graph.AddComponent(srcId, posId);

        // Mise à jour de l'index.
        NkArchetype* dstArch = graph.Get(dstId);
        uint32 row = dstArch->AddEntity(entity);
        entityIndex.SetRecord(entity, dstId, row);

        // Plus tard, on ajoute Velocity.
        NkComponentId velId = NkIdOf<Velocity>();
        NkArchetypeId srcId2 = entityIndex.GetRecord(entity)->archetypeId;
        uint32 srcRow = entityIndex.GetRecord(entity)->row;
        NkArchetypeId dstId2 = graph.AddComponent(srcId2, velId);

        // Migration de l'entité.
        NkArchetype* srcArch = graph.Get(srcId2);
        NkArchetype* newArch = graph.Get(dstId2);
        uint32 newRow = newArch->MigrateFrom(*srcArch, srcRow, entity);
        srcArch->RemoveEntity(srcRow);
        entityIndex.SetRecord(entity, dstId2, newRow);

        // L'entité possède maintenant Position et Velocity.
        assert(newArch->Has(posId) && newArch->Has(velId));
    }
*/

// =============================================================================