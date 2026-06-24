// =============================================================================
// FICHIER: NKECS/Serialization/NkJsonSerialization.h
// MODULE : NKECS (Runtime)
// =============================================================================
// DESCRIPTION (reparation Phase 4 du chantier NKReflection) :
//   Sérialisation des COMPOSANTS NKECS via le pont de réflexion générique
//   (NKReflection -> NKSerialization), sans dépendre de la couche Engine (Noge).
//
//   AVANT (cassé) : ce fichier incluait `../Prefab/NkPrefab.h` et
//   `../VisualScript/NkBlueprint.h` — qui n'existent PAS dans NKECS (Kernel) :
//   ils appartiennent à Noge (Engine). Cela violait l'ordre des couches
//   (Kernel -> Engine interdit) et le fichier ne compilait pas en standalone.
//   Il dépendait en plus de `nlohmann/json` et de `<fstream>` (STL), contraires
//   à la règle zero-STL du projet.
//
//   APRÈS (réparé) :
//     - Les includes Noge (Prefab/Blueprint) sont RETIRÉS.
//     - La logique Prefab/Blueprint (to_json/from_json de NkPrefab,
//       NkBlueprintGraph, NkValue, Save/LoadBlueprintToFile) est de la logique
//       d'éditeur de jeu : elle DOIT vivre côté Noge (Engine), réécrite sur
//       l'archive maison (NkArchive) + NkReflectSerializer. Elle a donc été
//       retirée d'ici (cf. note "DÉPLACÉ VERS NOGE" en bas).
//     - À la place, on expose la sérialisation de COMPOSANTS via le pont P4 :
//       chaque composant réfléchi (NkRegisterComponentReflection<T>()) porte
//       dans son ComponentMeta des hooks serialize/deserialize branchés sur
//       NkReflectSerializer. On les appelle ici de façon générique (par type
//       OU par ComponentMeta type-erased).
//
//   Zero-STL : NkArchive / NkString / NKMemory uniquement. La conversion
//   archive <-> JSON texte se fait via NKSerialization/JSON (NkJSONWriter /
//   NkJSONReader) côté appelant.
//
//   Auteur : Rihen — 2026-06-24 — Proprietary, free to use and modify.
// =============================================================================

#pragma once

#ifndef NKECS_SERIALIZATION_NKJSONSERIALIZATION_H
#define NKECS_SERIALIZATION_NKJSONSERIALIZATION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES (zero-STL, zero-Noge)
    // -------------------------------------------------------------------------

    #include "NKECS/NkECSDefines.h"
    #include "NKECS/Core/NkTypeRegistry.h"
    #include "NKECS/Reflect/NkReflectBridge.h"   // pont P4 (NkClass genere + hooks)

    #include "NKSerialization/NkArchive.h"
    #include "NKSerialization/Reflection/NkReflectSerializer.h"

    namespace nkentseu { namespace ecs { namespace serialization {

        // =====================================================================
        // 1. SERIALISATION D'UN COMPOSANT — API TYPE-SAFE (par type T)
        // =====================================================================
        /**
         * @brief Sérialise une instance de composant T vers une archive.
         * @tparam T Type de composant réfléchi (déclaré via NK_REFLECT_BEGIN/END
         *           et enregistré via NkRegisterComponentReflection<T>()).
         * @param comp Instance source.
         * @param ar   Archive de destination (clés = noms de propriétés).
         * @return true si la réflexion du composant est disponible et le parcours
         *         s'est déroulé, false sinon.
         *
         * @note Idempotent : enregistre la réflexion du composant si elle ne l'a
         *       pas encore été (premier appel).
         */
        template<typename T>
        NKECS_INLINE nk_bool SerializeComponent(const T& comp, ::nkentseu::NkArchive& ar) noexcept {
            const ::nkentseu::reflection::NkClass* cls =
                ::nkentseu::ecs::reflect::NkRegisterComponentReflection<T>();
            if (!cls) {
                return false;
            }
            return ::nkentseu::NkReflectSerializer::SerializeReflected(cls, &comp, ar);
        }

        /**
         * @brief Désérialise une archive vers une instance de composant T.
         * @tparam T Type de composant réfléchi (voir SerializeComponent).
         * @param comp Instance à remplir.
         * @param ar   Archive source.
         * @return true si la réflexion est disponible, false sinon.
         */
        template<typename T>
        NKECS_INLINE nk_bool DeserializeComponent(T& comp, const ::nkentseu::NkArchive& ar) noexcept {
            const ::nkentseu::reflection::NkClass* cls =
                ::nkentseu::ecs::reflect::NkRegisterComponentReflection<T>();
            if (!cls) {
                return false;
            }
            return ::nkentseu::NkReflectSerializer::DeserializeReflected(cls, &comp, ar);
        }

        // =====================================================================
        // 2. SERIALISATION D'UN COMPOSANT — API TYPE-ERASED (par ComponentMeta)
        // =====================================================================
        // Utile pour la sérialisation d'une ENTITÉ : on parcourt le masque de
        // composants, on récupère le ComponentMeta de chaque ComponentId et on
        // appelle ses hooks sans connaître T à la compilation. Les hooks sont
        // remplis par NkRegisterComponentReflection<T>() (pont P4).

        /**
         * @brief Sérialise un composant via son ComponentMeta (type-erased).
         * @param meta Métadonnées du composant (doit porter le hook serialize).
         * @param comp Adresse de l'instance de composant.
         * @param ar   Archive de destination.
         * @return true si le hook est branché et a été appelé, false sinon.
         */
        NKECS_INLINE nk_bool SerializeComponentMeta(
            const ComponentMeta* meta, const void* comp, ::nkentseu::NkArchive& ar) noexcept {
            if (!meta || !meta->serialize || !comp) {
                return false;
            }
            meta->serialize(comp, ar);
            return true;
        }

        /**
         * @brief Désérialise un composant via son ComponentMeta (type-erased).
         * @param meta Métadonnées du composant (doit porter le hook deserialize).
         * @param comp Adresse de l'instance de composant à remplir.
         * @param ar   Archive source.
         * @return true si le hook est branché et a été appelé, false sinon.
         */
        NKECS_INLINE nk_bool DeserializeComponentMeta(
            const ComponentMeta* meta, void* comp, const ::nkentseu::NkArchive& ar) noexcept {
            if (!meta || !meta->deserialize || !comp) {
                return false;
            }
            meta->deserialize(comp, ar);
            return true;
        }

        /**
         * @brief Vrai si le composant identifié par `id` expose la (de)sérialisation
         *        réfléchie (hooks branchés via NkRegisterComponentReflection).
         */
        NKECS_INLINE nk_bool ComponentHasReflection(NkComponentId id) noexcept {
            const ComponentMeta* meta = NkTypeRegistry::Global().Get(id);
            return meta && meta->reflectClass && meta->serialize && meta->deserialize;
        }

    }}} // namespace nkentseu::ecs::serialization

#endif // NKECS_SERIALIZATION_NKJSONSERIALIZATION_H

// =============================================================================
// NOTE — CE QUI A ÉTÉ DÉPLACÉ / RETIRÉ (réparation Phase 4)
// =============================================================================
//
// RETIRÉ de ce fichier (violait Kernel -> Engine + STL) :
//   - #include "../Prefab/NkPrefab.h"            (type Noge, Engine)
//   - #include "../VisualScript/NkBlueprint.h"   (type Noge, Engine)
//   - #include <nlohmann/json.hpp>               (STL/3rd-party interdit)
//   - #include <fstream>                          (STL interdit)
//   - to_json/from_json(NkValue, NkPrefab, NkBlueprintGraph)
//   - SaveToFile/LoadFromFile(NkPrefab)
//   - SaveBlueprintToFile/LoadBlueprintFromFile(NkBlueprintGraph)
//
// À DÉPLACER VERS NOGE (Engine) — logique Prefab/Blueprint :
//   La (dé)sérialisation de NkPrefab et NkBlueprintGraph est de la logique
//   d'éditeur de jeu et doit être réécrite côté Noge sur NkArchive +
//   NkJSONWriter/NkJSONReader (zero-STL), en réutilisant pour les COMPOSANTS
//   le pont générique exposé ici (SerializeComponent / SerializeComponentMeta).
//   Emplacement suggéré : Engine/Noge/src/Noge/ECS/Serialization/.
//
// CONSERVÉ / NOUVEAU (component-serialization via réflexion P2/P4) :
//   - SerializeComponent<T> / DeserializeComponent<T>      (type-safe)
//   - SerializeComponentMeta / DeserializeComponentMeta    (type-erased)
//   - ComponentHasReflection(id)
//   Ce fichier compile désormais en standalone sans dépendre de Noge.
//
// ============================================================
// Copyright (c) 2025-2026 Rihen. Tous droits reserves.
// ============================================================
