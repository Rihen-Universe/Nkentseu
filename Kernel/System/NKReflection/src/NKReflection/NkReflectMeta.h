// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkReflectMeta.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-06-24
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Metadonnees d'edition pour les proprietes reflechies (Phase 3). Porte
//   l'enumeration de flags 64 bits (issue de NKECS/Reflect::NkMetaFlag) dans
//   NKReflection, source de verite unique de la reflexion. Ces flags pilotent
//   l'inspecteur de l'editeur (visibilite, edition, range, tooltip, categorie,
//   serialisation, blueprint, reseau...).
//
//   Zero-STL : aucune dependance std::, types primitifs NKCore.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKREFLECTMETA_H
#define NK_REFLECTION_NKREFLECTMETA_H

    #include "NKCore/NkTypes.h"

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // ENUMERATION : NkReflectMeta (flags 64 bits)
            // =================================================================
            /**
             * @enum NkReflectMeta
             * @brief Metadonnees extensibles (drapeaux 64 bits) pour les
             *        proprietes reflechies.
             *
             * Portage de NKECS/Reflect::NkMetaFlag dans NKReflection afin que la
             * couche System (source de verite reflexion) possede les memes
             * qualificatifs d'edition que l'ancien systeme ECS. Les valeurs sont
             * des puissances de 2 combinables via OR bit-a-bit.
             *
             * @note Les codes de bits sont IDENTIQUES a NkMetaFlag (NKECS) pour
             *       permettre un pont 1:1 si necessaire.
             */
            enum NkReflectMeta : nk_uint64 {
                // -- Aucun --------------------------------------------------
                NK_REFLECT_META_NONE      = 0ULL,

                // -- Visibilite ---------------------------------------------
                NK_REFLECT_VISIBLE        = 1ULL << 0,
                NK_REFLECT_HIDE_EDITOR    = 1ULL << 1,
                NK_REFLECT_READONLY       = 1ULL << 2,
                NK_REFLECT_ADVANCED       = 1ULL << 3,

                // -- Serialisation ------------------------------------------
                NK_REFLECT_SERIALIZE      = 1ULL << 4,
                NK_REFLECT_TRANSIENT      = 1ULL << 5,  // = NkMeta_NoSerialize
                NK_REFLECT_SERIALIZE_DEF  = 1ULL << 6,

                // -- Edition ------------------------------------------------
                NK_REFLECT_EDIT_ANYWHERE  = 1ULL << 8,
                NK_REFLECT_EDIT_DEFAULTS  = 1ULL << 9,
                NK_REFLECT_EDIT_FIXEDSIZE = 1ULL << 10,
                NK_REFLECT_NO_EDIT        = 1ULL << 11,

                // -- Blueprint / Scripting ----------------------------------
                NK_REFLECT_BP_READWRITE   = 1ULL << 16,
                NK_REFLECT_BP_READONLY    = 1ULL << 17,
                NK_REFLECT_BP_CALLABLE    = 1ULL << 18,
                NK_REFLECT_BP_PURE        = 1ULL << 19,

                // -- Reseau -------------------------------------------------
                NK_REFLECT_REPLICATED     = 1ULL << 24,
                NK_REFLECT_REP_NOTIFY     = 1ULL << 25,
                NK_REFLECT_REP_SKIPOWNER  = 1ULL << 26,

                // -- UI / Affichage -----------------------------------------
                NK_REFLECT_RANGE          = 1ULL << 32,
                NK_REFLECT_PASSWORD       = 1ULL << 33,
                NK_REFLECT_MULTILINE      = 1ULL << 34,
                NK_REFLECT_COLOR_PICKER   = 1ULL << 35,

                // -- Utilitaires --------------------------------------------
                NK_REFLECT_INSTANCED      = 1ULL << 40,
                NK_REFLECT_DUPLICATE      = 1ULL << 42,
                NK_REFLECT_NEVER_DUP      = 1ULL << 43,

                // -- Reserve utilisateur ------------------------------------
                NK_REFLECT_USER0          = 1ULL << 56,
                NK_REFLECT_USER1          = 1ULL << 57,
                NK_REFLECT_USER2          = 1ULL << 58,
                NK_REFLECT_USER3          = 1ULL << 59,
                NK_REFLECT_USER4          = 1ULL << 60,
                NK_REFLECT_USER5          = 1ULL << 61,
                NK_REFLECT_USER6          = 1ULL << 62,
                NK_REFLECT_USER7          = 1ULL << 63,
            };

            // =================================================================
            // STRUCTURE : NkEditMeta (metadonnees d'edition d'une propriete)
            // =================================================================
            /**
             * @struct NkEditMeta
             * @brief Bloc optionnel de metadonnees d'edition attache a une
             *        NkProperty (range slider, tooltip, categorie d'inspecteur).
             *
             * Tous les champs sont a duree de vie statique (literaux) ou par
             * valeur (range). Permet a l'inspecteur de generer le bon widget.
             */
            struct NkEditMeta {
                nk_float32     rangeMin = 0.0f;     ///< Borne min (slider) si NK_REFLECT_RANGE
                nk_float32     rangeMax = 0.0f;     ///< Borne max (slider) si NK_REFLECT_RANGE
                const nk_char* tooltip  = nullptr;  ///< Texte d'aide au survol
                const nk_char* category = nullptr;  ///< Categorie / regroupement UI
                const nk_char* display  = nullptr;  ///< Nom lisible (optionnel)
            };

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKREFLECTMETA_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
