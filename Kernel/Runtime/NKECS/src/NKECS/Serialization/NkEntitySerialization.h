// =============================================================================
// FICHIER: NKECS/Serialization/NkEntitySerialization.h
// MODULE : NKECS (Runtime)
// =============================================================================
// DESCRIPTION (finition Phase 4/5 du chantier NKReflection) :
//   Sérialisation au NIVEAU ENTITÉ et NIVEAU MONDE. Au-dessus de la
//   sérialisation par composant (NkJsonSerialization.h + pont P4), ce fichier
//   sait :
//     - SerializeEntity   : parcourir le masque de composants d'une entité,
//                           écrire chaque composant réfléchi dans un sous-archive
//                           nommé par le nom du composant.
//     - DeserializeEntity : recréer les composants d'une entité depuis un archive
//                           (retrouve le ComponentId par nom, ajoute le composant
//                           type-erased, appelle le hook deserialize).
//     - SerializeWorld / DeserializeWorld : toutes les entités vivantes,
//                           base d'une sauvegarde de scène (.nkscene).
//
//   PRINCIPE DE PARCOURS DU MASQUE
//   ------------------------------
//   Une entité réside dans un archétype (signature = NkComponentMask). On obtient
//   son NkEntityRecord (archetypeId, row) via NkWorld::EntityIndex().GetRecord,
//   l'archétype via NkWorld::Graph().Get(archetypeId), puis on itère les bits
//   actifs du masque de l'archétype via NkComponentMask::ForEach(fn). Pour chaque
//   ComponentId présent : on récupère le ComponentMeta (NkTypeRegistry), le
//   pointeur brut du composant (archetype->GetPool(cid)->At(row)) et son hook
//   serialize (branché par NkRegisterComponentReflection<T>()).
//
//   AJOUT TYPE-ERASED D'UN COMPOSANT (désérialisation)
//   --------------------------------------------------
//   NkWorld::Add<T> est template. Pour ajouter un composant connu seulement par
//   son ComponentId (lu dans l'archive), on réplique la mécanique d'archétype
//   sans T : transition d'archétype (Graph().AddComponent), migration des pools
//   existantes (NkArchetype::MigrateFrom) et construction par défaut du nouveau
//   composant (NkComponentPool::PushDefault via MigrateFrom). C'est exactement ce
//   que fait AddImpl, mais piloté par cid. Le ComponentMeta du type DOIT déjà
//   exister (le type a été enregistré au moins une fois — garanti par l'appel à
//   NkRegisterComponentReflection<T>() côté contenu, et indispensable pour
//   construire l'archétype). On documente cette précondition.
//
//   Zero-STL : NkArchive / NkString / NKMemory uniquement.
//
//   Auteur : Rihen — 2026-06-25 — Proprietary, free to use and modify.
// =============================================================================

#pragma once

#ifndef NKECS_SERIALIZATION_NKENTITYSERIALIZATION_H
#define NKECS_SERIALIZATION_NKENTITYSERIALIZATION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES (zero-STL, zero-Noge)
    // -------------------------------------------------------------------------

    #include "NKECS/NkECSDefines.h"
    #include "NKECS/Core/NkTypeRegistry.h"
    #include "NKECS/Storage/NkArchetype.h"
    #include "NKECS/Storage/NkArchetypeGraph.h"
    #include "NKECS/World/NkWorld.h"
    #include "NKECS/Serialization/NkJsonSerialization.h"  // SerializeComponentMeta...

    #include "NKSerialization/NkArchive.h"

    #include "NKContainers/String/NkString.h"

    namespace nkentseu { namespace ecs { namespace serialization {

        // =====================================================================
        // 0. HELPERS INTERNES (résolution par nom + accès type-erased)
        // =====================================================================

        /**
         * @brief Retrouve le ComponentId d'un composant à partir de son nom
         *        (tel que stocké dans ComponentMeta.name, posé par NK_COMPONENT).
         * @return Le ComponentId, ou kInvalidComponentId si introuvable.
         *
         * @note Recherche linéaire sur les types enregistrés (registre global).
         *       Acceptable : la (dé)sérialisation de scène n'est pas un chemin
         *       chaud, et le nombre de types de composants reste modeste.
         */
        NKECS_INLINE NkComponentId FindComponentIdByName(const nk_char* name) noexcept {
            if (!name) {
                return kInvalidComponentId;
            }
            NkTypeRegistry& reg = NkTypeRegistry::Global();
            const uint32 count = reg.Count();
            for (uint32 id = 0; id < count; ++id) {
                const ComponentMeta* meta = reg.Get(id);
                if (meta && meta->name && ::nkentseu::NkStringView(meta->name) == ::nkentseu::NkStringView(name)) {
                    return meta->id;
                }
            }
            return kInvalidComponentId;
        }

        /**
         * @brief Pointeur brut (void*) vers l'instance du composant `cid` porté
         *        par l'entité `id` dans le monde `world`.
         * @return Adresse de l'instance, ou nullptr si l'entité ne porte pas ce
         *         composant (ou si c'est un tag de taille nulle).
         */
        NKECS_INLINE void* GetComponentRaw(NkWorld& world, NkEntityId id, NkComponentId cid) noexcept {
            const NkEntityRecord* rec = world.EntityIndex().GetRecord(id);
            if (!rec || rec->archetypeId == kInvalidArchetypeId) {
                return nullptr;
            }
            NkArchetype* arch = world.Graph().Get(rec->archetypeId);
            if (!arch || !arch->Has(cid)) {
                return nullptr;
            }
            NkComponentPool* pool = arch->GetPool(cid);
            if (!pool) {
                return nullptr;
            }
            return pool->At(rec->row);
        }

        /**
         * @brief Ajoute le composant identifié par `cid` à l'entité `id` (type-
         *        erased) et retourne le pointeur brut de la nouvelle instance
         *        (construite par défaut). No-op si déjà présent (retourne
         *        l'existant).
         *
         * @note Précondition : le ComponentMeta de `cid` doit être enregistré
         *       (taille/align/ctor connus) — sinon impossible de construire
         *       l'archétype. Pour les composants réfléchis, c'est garanti par
         *       NkRegisterComponentReflection<T>() (appelé au chargement du type).
         *
         * @note Réplique fidèlement NkWorld::AddImpl mais piloté par cid :
         *       transition d'archétype (Graph().AddComponent / GetOrCreate),
         *       migration des composants communs (MigrateFrom), puis le nouveau
         *       composant est PushDefault par MigrateFrom (construction par défaut).
         */
        NKECS_INLINE void* AddComponentByIdRaw(NkWorld& world, NkEntityId id, NkComponentId cid) noexcept {
            if (cid == kInvalidComponentId) {
                return nullptr;
            }
            // Le meta doit exister (sinon NkArchetype ne saura pas construire la pool).
            const ComponentMeta* meta = NkTypeRegistry::Global().Get(cid);
            if (!meta) {
                return nullptr;
            }

            NkEntityRecord* rec = world.EntityIndex().GetRecord(id);
            if (!rec) {
                return nullptr;  // entité morte / invalide
            }

            NkArchetypeGraph& graph = world.Graph();

            // CAS 1 : composant déjà présent -> retourne l'existant.
            if (rec->archetypeId != kInvalidArchetypeId) {
                NkArchetype* arch = graph.Get(rec->archetypeId);
                if (arch && arch->Has(cid)) {
                    return arch->GetPool(cid) ? arch->GetPool(cid)->At(rec->row) : nullptr;
                }
            }

            // CAS 2 : nouveau composant -> migration d'archétype.
            const NkArchetypeId oldArchId = rec->archetypeId;
            const uint32        oldRow    = rec->row;

            NkComponentMask single;
            single.Set(cid);

            const NkArchetypeId newArchId = (oldArchId == kInvalidArchetypeId)
                ? graph.GetOrCreate(single)
                : graph.AddComponent(oldArchId, cid);

            NkArchetype* newArch = graph.Get(newArchId);
            if (!newArch) {
                return nullptr;
            }

            uint32 newRow = 0;
            if (oldArchId != kInvalidArchetypeId) {
                NkArchetype* oldArch = graph.Get(oldArchId);
                newRow = newArch->MigrateFrom(*oldArch, oldRow, id);

                const NkEntityId swapped = oldArch->RemoveEntity(oldRow);
                if (swapped.IsValid()) {
                    world.EntityIndex().SetRecord(swapped, oldArchId, oldRow);
                }
            } else {
                newRow = newArch->AddEntity(id);
            }

            world.EntityIndex().SetRecord(id, newArchId, newRow);

            NkComponentPool* pool = newArch->GetPool(cid);
            return pool ? pool->At(newRow) : nullptr;
        }

        // =====================================================================
        // 1. SERIALISATION D'UNE ENTITÉ
        // =====================================================================
        /**
         * @brief Sérialise tous les composants RÉFLÉCHIS d'une entité dans `ar`.
         *
         * Pour chaque ComponentId présent dans le masque de l'archétype de
         * l'entité (parcours via NkComponentMask::ForEach), on récupère le
         * ComponentMeta ; s'il porte un hook serialize (composant réfléchi
         * enregistré via NkRegisterComponentReflection<T>()), on sérialise son
         * instance dans un SOUS-ARCHIVE nommé par le nom du composant, puis on
         * attache ce sous-archive à `ar` sous cette clé.
         *
         * @return true si l'entité est vivante et a été parcourue, false sinon.
         *         Les composants sans réflexion sont silencieusement sautés.
         */
        NKECS_INLINE nk_bool SerializeEntity(NkWorld& world, NkEntityId id, ::nkentseu::NkArchive& ar) noexcept {
            const NkEntityRecord* rec = world.EntityIndex().GetRecord(id);
            if (!rec) {
                return false;  // entité morte / invalide
            }
            if (rec->archetypeId == kInvalidArchetypeId) {
                return true;   // entité vivante mais sans composant : archive vide OK
            }
            NkArchetype* arch = world.Graph().Get(rec->archetypeId);
            if (!arch) {
                return false;
            }
            const uint32 row = rec->row;

            arch->Mask().ForEach([&](NkComponentId cid) {
                const ComponentMeta* meta = NkTypeRegistry::Global().Get(cid);
                if (!meta || !meta->serialize) {
                    return;  // composant non réfléchi : sauté
                }
                NkComponentPool* pool = arch->GetPool(cid);
                if (!pool) {
                    return;
                }
                const void* comp = pool->At(row);
                // Les tags (taille nulle) renvoient nullptr : on les ignore ici
                // (rien à sérialiser dans leur contenu). Leur présence pourrait
                // être encodée par une clé vide ; reporté.
                if (!comp) {
                    return;
                }
                ::nkentseu::NkArchive sub;
                meta->serialize(comp, sub);
                ar.SetObject(::nkentseu::NkStringView(meta->name), sub);
            });

            return true;
        }

        // =====================================================================
        // 2. DESERIALISATION D'UNE ENTITÉ
        // =====================================================================
        /**
         * @brief Reconstruit les composants d'une entité depuis `ar`.
         *
         * Pour chaque clé de l'archive (un sous-objet par composant, nommé par le
         * nom du composant) : on retrouve le ComponentId par nom, on ajoute le
         * composant à l'entité (type-erased) et on appelle son hook deserialize
         * sur l'instance fraîchement créée.
         *
         * @return true si l'entité est vivante, false sinon. Les clés ne
         *         correspondant à aucun composant réfléchi connu sont sautées.
         *
         * @note Pré-requis : les TYPES des composants présents dans l'archive
         *       doivent avoir été enregistrés (NkRegisterComponentReflection<T>())
         *       AVANT l'appel, sinon FindComponentIdByName échoue (type inconnu).
         */
        NKECS_INLINE nk_bool DeserializeEntity(NkWorld& world, NkEntityId id, const ::nkentseu::NkArchive& ar) noexcept {
            if (!world.EntityIndex().GetRecord(id)) {
                return false;  // entité morte
            }

            const ::nkentseu::NkVector<::nkentseu::NkArchiveEntry>& entries = ar.Entries();
            for (nk_size i = 0; i < entries.Size(); ++i) {
                const ::nkentseu::NkArchiveEntry& e = entries[i];
                if (!e.node.IsObject()) {
                    continue;  // on n'attend que des sous-objets {composant: {...}}
                }
                const NkComponentId cid = FindComponentIdByName(e.key.CStr());
                if (cid == kInvalidComponentId) {
                    continue;  // type inconnu (pas enregistré) : sauté
                }
                const ComponentMeta* meta = NkTypeRegistry::Global().Get(cid);
                if (!meta || !meta->deserialize) {
                    continue;  // composant non réfléchi : sauté
                }

                // Ajoute (ou récupère) le composant puis remplit ses champs.
                void* comp = AddComponentByIdRaw(world, id, cid);
                if (!comp) {
                    continue;  // tag (taille nulle) ou échec : rien à remplir
                }
                // Le sous-objet est e.node.object (copie via GetObject pour rester
                // dans l'API publique const-correcte).
                ::nkentseu::NkArchive sub;
                if (ar.GetObject(e.key.View(), sub)) {
                    meta->deserialize(comp, sub);
                }
            }
            return true;
        }

        // =====================================================================
        // 3. SERIALISATION DU MONDE (toutes les entités vivantes)
        // =====================================================================
        // Clé "entities" -> tableau d'objets, chaque objet = { "id": <u64>,
        // <composant>: {...}, ... }. L'id (packé) permet de préserver l'identité
        // logique au rechargement (optionnel côté consommateur).

        /**
         * @brief Sérialise toutes les entités vivantes du monde sous la clé
         *        "entities" (tableau d'objets entité).
         * @return true (toujours : un monde vide produit un tableau vide).
         */
        NKECS_INLINE nk_bool SerializeWorld(NkWorld& world, ::nkentseu::NkArchive& ar) noexcept {
            ::nkentseu::NkVector<::nkentseu::NkArchive> entityArchives;

            NkArchetypeGraph& graph = world.Graph();
            const uint32 archCount = graph.Count();
            for (uint32 a = 0; a < archCount; ++a) {
                NkArchetype* arch = graph.Get(a);
                if (!arch || arch->Empty()) {
                    continue;
                }
                arch->ForEachRow([&](uint32 /*row*/, NkEntityId eid) {
                    ::nkentseu::NkArchive entAr;
                    entAr.SetUInt64(::nkentseu::NkStringView("id"), eid.Pack());
                    SerializeEntity(world, eid, entAr);
                    entityArchives.PushBack(entAr);
                });
            }

            ar.SetObjectArray(::nkentseu::NkStringView("entities"), entityArchives);
            return true;
        }

        // =====================================================================
        // 4. DESERIALISATION DU MONDE (recrée les entités)
        // =====================================================================
        /**
         * @brief Recrée les entités du monde depuis la clé "entities".
         *
         * Chaque objet entité est désérialisé dans une NOUVELLE entité (les ids
         * d'origine ne sont pas réimposés : le monde alloue de nouveaux ids — la
         * clé "id" est purement informative ici). La clé "id" est ignorée pour la
         * reconstruction des composants (ce n'est pas un composant réfléchi).
         *
         * @return true (un tableau absent/vid e laisse le monde inchangé).
         */
        NKECS_INLINE nk_bool DeserializeWorld(NkWorld& world, const ::nkentseu::NkArchive& ar) noexcept {
            ::nkentseu::NkVector<::nkentseu::NkArchive> entityArchives;
            if (!ar.GetObjectArray(::nkentseu::NkStringView("entities"), entityArchives)) {
                return true;  // pas d'entités : rien à faire
            }

            for (nk_size i = 0; i < entityArchives.Size(); ++i) {
                const NkEntityId eid = world.CreateEntity();
                DeserializeEntity(world, eid, entityArchives[i]);
            }
            return true;
        }

    }}} // namespace nkentseu::ecs::serialization

#endif // NKECS_SERIALIZATION_NKENTITYSERIALIZATION_H

// ============================================================
// Copyright (c) 2025-2026 Rihen. Tous droits reserves.
// ============================================================
