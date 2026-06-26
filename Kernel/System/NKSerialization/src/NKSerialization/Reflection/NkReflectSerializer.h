// =============================================================================
// NKSerialization/Reflection/NkReflectSerializer.h
// Pont Reflection <-> Serialization : (de)serialisation AUTOMATIQUE d'un objet
// reflechi (NKReflection) vers/depuis un NkArchive, par iteration des proprietes.
//
// Phase 2 du chantier NKReflection. Permet de sauver/charger un objet sans
// ecrire de Serialize() a la main : on parcourt les NkProperty de son NkClass,
// on lit chaque valeur via NkProperty::GetValueGeneric (NkReflectVariant) puis
// on la pousse dans l'archive selon la categorie du type (int/float/bool/string/
// enum/objet imbrique). La deserialisation est symetrique via SetValueGeneric.
//
// Couverture Phase 2 :
//  - Primitifs (bool, int8..int64, uint8..uint64, float32/64)   : OK
//  - NkString (categorie NK_STRING)                              : OK
//  - Enums (categorie NK_ENUM, traites comme int64)              : OK
//  - Objets imbriques reflechis (NK_CLASS avec NkClass associe)  : OK (recursif)
//  - Heritage (proprietes des classes de base)                  : OK (remontee)
// Repousse Phase 3 :
//  - Conteneurs (NkVector / tableaux) et pointeurs               : non geres
//  - Noms symboliques d'enum (ici stockes en valeur numerique)
//
// Zero-STL : aucune dependance std::, conteneurs/chaines maison, NKMemory.
//
// Auteur : Rihen
// Date   : 2026-06-24
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_REFLECTION_NKREFLECTSERIALIZER_H
#define NKENTSEU_SERIALIZATION_REFLECTION_NKREFLECTSERIALIZER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES
    // -------------------------------------------------------------------------

    #include "NKCore/NkTypes.h"
    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkRegistry.h"

    namespace nkentseu {

        // ---------------------------------------------------------------------
        // CLASSE : NkReflectSerializer
        // ---------------------------------------------------------------------
        /**
         * @class NkReflectSerializer
         * @brief Pont generique Reflection -> Archive et Archive -> Reflection.
         *
         * Methodes statiques sans etat : on passe le NkClass decrivant le type,
         * l'adresse de l'instance et l'archive cible/source. Le serializer
         * parcourt toutes les proprietes (y compris heritees) et transfere les
         * valeurs via NkReflectVariant.
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkReflectSerializer {
            public:

                // -------------------------------------------------------------
                // SERIALISATION : objet reflechi -> archive
                // -------------------------------------------------------------
                /**
                 * @brief Serialise une instance reflechie dans une archive.
                 * @param cls      Meta-classe decrivant l'instance (non nul).
                 * @param instance Adresse de l'objet a serialiser (non nul).
                 * @param ar       Archive de destination (les cles = noms de prop).
                 * @return true si au moins le parcours s'est deroule (false si
                 *         arguments invalides). Les proprietes non gerees sont
                 *         sautees sans faire echouer l'ensemble.
                 *
                 * @note Saute les proprietes marquees NK_TRANSIENT.
                 * @note Les objets imbriques (NK_CLASS) sont ecrits comme
                 *       sous-objets de l'archive (recursion).
                 */
                static nk_bool SerializeReflected(
                    const reflection::NkClass* cls,
                    const void* instance,
                    NkArchive& ar
                );

                // -------------------------------------------------------------
                // DESERIALISATION : archive -> objet reflechi
                // -------------------------------------------------------------
                /**
                 * @brief Remplit une instance reflechie depuis une archive.
                 * @param cls      Meta-classe decrivant l'instance (non nul).
                 * @param instance Adresse de l'objet a remplir (non nul).
                 * @param ar       Archive source.
                 * @return true si arguments valides, false sinon. Les cles
                 *         absentes de l'archive laissent la propriete inchangee.
                 *
                 * @note Saute les proprietes NK_TRANSIENT et NK_READ_ONLY.
                 * @note Coercition numerique best-effort via NkReflectVariant.
                 */
                static nk_bool DeserializeReflected(
                    const reflection::NkClass* cls,
                    void* instance,
                    const NkArchive& ar
                );

                // -------------------------------------------------------------
                // HELPERS TYPES : resolution automatique du NkClass via T
                // -------------------------------------------------------------
                /**
                 * @brief Serialise un objet T en resolvant son NkClass.
                 * @tparam T Type reflechi (doit exposer GetStaticClass() ou etre
                 *           enregistre dans NkRegistry).
                 */
                template<typename T>
                static nk_bool SerializeObject(const T& obj, NkArchive& ar) {
                    const reflection::NkClass* cls = ResolveClass<T>();
                    return SerializeReflected(cls, &obj, ar);
                }

                /**
                 * @brief Deserialise un objet T en resolvant son NkClass.
                 * @tparam T Type reflechi (voir SerializeObject).
                 */
                template<typename T>
                static nk_bool DeserializeObject(T& obj, const NkArchive& ar) {
                    const reflection::NkClass* cls = ResolveClass<T>();
                    return DeserializeReflected(cls, &obj, ar);
                }

            private:

                // -------------------------------------------------------------
                // RESOLUTION DU NkClass POUR UN TYPE T
                // -------------------------------------------------------------
                // Priorite a T::GetStaticClass() (renvoi par la macro
                // NKENTSEU_REFLECT_CLASS, nom litteral correct). A defaut, on
                // tente le registre via typeid (peut echouer si la classe a ete
                // enregistree sous son nom litteral et non typeid). SFINAE.

                template<typename T>
                static auto ResolveClassImpl(int)
                    -> decltype(&T::GetStaticClass()) {
                    return &T::GetStaticClass();
                }

                template<typename T>
                static const reflection::NkClass* ResolveClassImpl(...) {
                    return reflection::NkRegistry::Get().GetClass<T>();
                }

                template<typename T>
                static const reflection::NkClass* ResolveClass() {
                    return ResolveClassImpl<T>(0);
                }
        };

    } // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_REFLECTION_NKREFLECTSERIALIZER_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
