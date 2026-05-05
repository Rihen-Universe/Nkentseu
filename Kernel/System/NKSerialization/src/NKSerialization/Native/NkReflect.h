// =============================================================================
// NKSerialization/Native/NkReflect.h
// Système de réflexion statique minimaliste pour sérialisation automatique.
//
// Design :
//  - Réflexion compile-time via macros et SFINAE (aucun RTTI runtime)
//  - Sérialisation automatique des structs C++ via NK_REFLECT_* macros
//  - Support des types scalaires, strings, et types imbriqués réfléchis
//  - Intégration transparente avec NkISerializable pour polymorphisme
//  - Export contrôlé via NKENTSEU_SERIALIZATION_API (header-only, inline)
//  - Aucune allocation dynamique : tout passe par NkArchive intermédiaire
//
// Types supportés automatiquement :
//  - Scalaires : bool, int8..int64, uint8..uint64, float32, float64
//  - Strings : NkString (UTF-8)
//  - Types imbriqués : tout type avec NK_REFLECT_BEGIN/END
//  - Polymorphique : tout type dérivant de NkISerializable
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H
#define NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKSerialization/NkISerializable.h"
    #include "NKCore/NkTraits.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/String/NkStringView.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // NAMESPACE : reflect_detail
        // DESCRIPTION : Implémentation interne des traits et helpers de réflexion
        // =============================================================================
        /**
         * @namespace reflect_detail
         * @brief Détails d'implémentation du système de réflexion statique
         * @ingroup SerializationInternals
         *
         * Ce namespace contient :
         *  - Traits SFINAE pour détection de capacités de sérialisation
         *  - Fonctions templates SerializeField/DeserializeField pour types scalaires
         *  - Logique de dispatch compile-time via if constexpr
         *
         * @note Ne pas utiliser directement — passer par les macros NK_REFLECT_*
         *       ou les fonctions publiques NkReflect::Serialize/Deserialize.
         */
        namespace reflect_detail {


            // -----------------------------------------------------------------
            // TRAITS : Détection de capacités de sérialisation
            // -----------------------------------------------------------------
            /**
             * @struct HasSerialize
             * @brief Trait SFINAE pour détecter la méthode Serialize(NkArchive&) const
             * @ingroup SerializationTraits
             *
             * Détecte si un type T possède une méthode :
             * @code nk_bool Serialize(NkArchive&) const @endcode
             *
             * Utilisé pour intégrer automatiquement les types NkISerializable
             * dans le système de réflexion sans dépendance circulaire.
             *
             * @note Basé sur SFINAE via decltype et std::declval (C++11+)
             * @note Retourne NkTrueType ou NkFalseType pour branchement compile-time
             */
            template<typename T, typename = void>
            struct HasSerialize : traits::NkFalseType {};

            template<typename T>
            struct HasSerialize<T, traits::NkVoidT<
                decltype(std::declval<const T&>().Serialize(std::declval<NkArchive&>()))>>
                : traits::NkTrueType {};

            /**
             * @struct HasReflect
             * @brief Trait SFINAE pour détecter les macros NK_REFLECT_*
             * @ingroup SerializationTraits
             *
             * Détecte si un type T a été déclaré avec NK_REFLECT_BEGIN/END
             * en vérifiant la présence du tag interne NkReflectTag.
             *
             * @note Le tag NkReflectTag est un type vide ajouté par la macro
             *       pour éviter les collisions de noms avec d'autres systèmes.
             * @note Retourne NkTrueType ou NkFalseType pour branchement compile-time
             */
            template<typename T, typename = void>
            struct HasReflect : traits::NkFalseType {};

            template<typename T>
            struct HasReflect<T, traits::NkVoidT<typename T::NkReflectTag>>
                : traits::NkTrueType {};


            // -----------------------------------------------------------------
            // FONCTION : SerializeField
            // DESCRIPTION : Sérialise un champ scalaire ou composé dans un archive
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un champ de type T dans un NkArchive
             * @tparam T Type du champ à sérialiser (déduit automatiquement)
             * @param key Nom de la clé pour ce champ dans l'archive
             * @param v Référence const vers la valeur à sérialiser
             * @param arc Archive de destination pour le stockage
             * @note noexcept : garantie de non-levée d'exception
             *
             * Dispatch compile-time via if constexpr :
             *  - Types scalaires natifs : conversion vers SetInt64/SetFloat64/etc.
             *  - NkString : stockage direct via SetString()
             *  - Types avec HasReflect<T> : sérialisation récursive via NkReflectSerialize()
             *  - Types avec HasSerialize<T> : appel de Serialize() via NkISerializable
             *  - Autres types : ignorés silencieusement (extensibilité future)
             *
             * @note Les conversions de type (ex: int32→int64) sont explicites pour éviter
             *       les warnings de narrowing et garantir la portabilité.
             *
             * @example
             * @code
             * struct Vec3 { float x, y, z; };
             * // Après NK_REFLECT_BEGIN(Vec3)...NK_REFLECT_FIELD(x)...
             * // SerializeField("x", vec.x, archive) → archive.SetFloat64("x", vec.x)
             * @endcode
             */
            template<typename T>
            void SerializeField(NkStringView key, const T& v, NkArchive& arc) noexcept {
                if constexpr (traits::NkIsSame_v<T, nk_bool> || traits::NkIsSame_v<T, bool>) {
                    arc.SetBool(key, static_cast<nk_bool>(v));
                }
                else if constexpr (traits::NkIsSame_v<T, nk_int8>  || traits::NkIsSame_v<T, nk_int16> ||
                                   traits::NkIsSame_v<T, nk_int32> || traits::NkIsSame_v<T, nk_int64>) {
                    arc.SetInt64(key, static_cast<nk_int64>(v));
                }
                else if constexpr (traits::NkIsSame_v<T, nk_uint8>  || traits::NkIsSame_v<T, nk_uint16> ||
                                   traits::NkIsSame_v<T, nk_uint32> || traits::NkIsSame_v<T, nk_uint64>) {
                    arc.SetUInt64(key, static_cast<nk_uint64>(v));
                }
                else if constexpr (traits::NkIsSame_v<T, nk_float32>) {
                    arc.SetFloat64(key, static_cast<nk_float64>(v));
                }
                else if constexpr (traits::NkIsSame_v<T, nk_float64>) {
                    arc.SetFloat64(key, v);
                }
                else if constexpr (traits::NkIsSame_v<T, NkString>) {
                    arc.SetString(key, v.View());
                }
                else if constexpr (HasReflect<T>::value) {
                    NkArchive childArc;
                    T::NkReflectSerialize(v, childArc);
                    arc.SetObject(key, childArc);
                }
                else if constexpr (HasSerialize<T>::value) {
                    NkArchive childArc;
                    v.Serialize(childArc);
                    arc.SetObject(key, childArc);
                }
                // Sinon : type non supporté → ignoré silencieusement (extensibilité)
            }

            /**
             * @brief Désérialise un champ de type T depuis un NkArchive
             * @tparam T Type du champ à désérialiser (déduit automatiquement)
             * @param key Nom de la clé pour ce champ dans l'archive
             * @param v Référence mutable vers la valeur à peupler
             * @param arc Archive source contenant les données
             * @note noexcept : garantie de non-levée d'exception
             *
             * Dispatch compile-time via if constexpr (symétrique à SerializeField) :
             *  - Types scalaires : lecture via Get*() avec conversion explicite
             *  - NkString : lecture directe via GetString()
             *  - Types avec HasReflect<T> : désérialisation récursive via NkReflectDeserialize()
             *  - Types avec HasSerialize<T> : appel de Deserialize() via NkISerializable
             *  - Autres types : ignorés silencieusement
             *
             * @note Les conversions de type gèrent les échecs de parsing via les retours bool
             *       des méthodes Get*() de NkArchive — valeurs par défaut en cas d'erreur.
             *
             * @example
             * @code
             * struct Vec3 { float x, y, z; };
             * // Après NK_REFLECT_BEGIN(Vec3)...NK_REFLECT_FIELD_D(x)...
             * // DeserializeField("x", vec.x, archive) → archive.GetFloat64("x", vec.x)
             * @endcode
             */
            template<typename T>
            void DeserializeField(NkStringView key, T& v, const NkArchive& arc) noexcept {
                if constexpr (traits::NkIsSame_v<T, nk_bool> || traits::NkIsSame_v<T, bool>) {
                    nk_bool b = false;
                    arc.GetBool(key, b);
                    v = static_cast<T>(b);
                }
                else if constexpr (traits::NkIsSame_v<T, nk_int8>  || traits::NkIsSame_v<T, nk_int16> ||
                                   traits::NkIsSame_v<T, nk_int32> || traits::NkIsSame_v<T, nk_int64>) {
                    nk_int64 i = 0;
                    arc.GetInt64(key, i);
                    v = static_cast<T>(i);
                }
                else if constexpr (traits::NkIsSame_v<T, nk_uint8>  || traits::NkIsSame_v<T, nk_uint16> ||
                                   traits::NkIsSame_v<T, nk_uint32> || traits::NkIsSame_v<T, nk_uint64>) {
                    nk_uint64 u = 0;
                    arc.GetUInt64(key, u);
                    v = static_cast<T>(u);
                }
                else if constexpr (traits::NkIsSame_v<T, nk_float32>) {
                    nk_float64 f = 0.0;
                    arc.GetFloat64(key, f);
                    v = static_cast<nk_float32>(f);
                }
                else if constexpr (traits::NkIsSame_v<T, nk_float64>) {
                    arc.GetFloat64(key, v);
                }
                else if constexpr (traits::NkIsSame_v<T, NkString>) {
                    arc.GetString(key, v);
                }
                else if constexpr (HasReflect<T>::value) {
                    NkArchive childArc;
                    if (arc.GetObject(key, childArc)) {
                        T::NkReflectDeserialize(childArc, v);
                    }
                }
                else if constexpr (HasSerialize<T>::value) {
                    NkArchive childArc;
                    if (arc.GetObject(key, childArc)) {
                        v.Deserialize(childArc);
                    }
                }
                // Sinon : type non supporté → ignoré silencieusement (extensibilité)
            }


        } // namespace reflect_detail


        // =============================================================================
        // NAMESPACE : NkReflect
        // DESCRIPTION : API publique pour sérialisation/désérialisation réfléchie
        // =============================================================================
        /**
         * @namespace NkReflect
         * @brief Point d'entrée public pour la sérialisation automatique via réflexion
         * @ingroup SerializationComponents
         *
         * Fonctions fournies :
         *  - Serialize<T>(obj, archive) : écrit l'état d'un objet réfléchi dans un archive
         *  - Deserialize<T>(archive, obj) : lit l'état d'un archive dans un objet réfléchi
         *
         * Dispatch automatique :
         *  - Si T a NK_REFLECT_* : appel de T::NkReflectSerialize/Deserialize
         *  - Si T a Serialize/Deserialize : appel via interface NkISerializable
         *  - Sinon : retour false (type non sérialisable)
         *
         * @note Ces fonctions sont inline et header-only — aucune définition .cpp requise.
         * @note noexcept : garanties de non-levée d'exception pour sécurité runtime.
         *
         * @example Sérialisation/désérialisation d'un type réfléchi
         * @code
         * struct Player {
         *     nk_uint32 level;
         *     nk_float32 health;
         *     NK_REFLECT_BEGIN(Player)
         *         NK_REFLECT_FIELD(level)
         *         NK_REFLECT_FIELD(health)
         *     NK_REFLECT_END()
         * };
         *
         * Player p{42, 100.f};
         * nkentseu::NkArchive arc;
         *
         * // Sérialisation
         * if (nkentseu::NkReflect::Serialize(p, arc)) {
         *     // arc contient maintenant level=42, health=100.0
         * }
         *
         * // Désérialisation
         * Player loaded;
         * if (nkentseu::NkReflect::Deserialize(arc, loaded)) {
         *     // loaded.level == 42, loaded.health == 100.f
         * }
         * @endcode
         */
        namespace NkReflect {


            // -----------------------------------------------------------------
            // FONCTION : Serialize
            // DESCRIPTION : Sérialise un objet réfléchi dans un archive
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un objet de type T dans un NkArchive
             * @tparam T Type de l'objet à sérialiser (doit être réfléchi ou NkISerializable)
             * @param obj Référence const vers l'objet à sérialiser
             * @param arc Archive de destination pour le stockage
             * @return true si la sérialisation a réussi, false si T n'est pas sérialisable
             * @note noexcept : garantie de non-levée d'exception
             * @ingroup NkReflectAPI
             *
             * Comportement :
             *  - Si HasReflect<T> : appelle T::NkReflectSerialize(obj, arc)
             *  - Si HasSerialize<T> : appelle obj.Serialize(arc) via NkISerializable
             *  - Sinon : retourne false (type non supporté)
             *
             * @note Aucune allocation dynamique — tout passe par l'archive fourni.
             * @note Thread-safe : pas d'état interne mutable.
             *
             * @example Voir l'exemple dans la documentation du namespace NkReflect.
             */
            template<typename T>
            nk_bool Serialize(const T& obj, NkArchive& arc) noexcept {
                if constexpr (reflect_detail::HasReflect<T>::value) {
                    T::NkReflectSerialize(obj, arc);
                    return true;
                }
                else if constexpr (reflect_detail::HasSerialize<T>::value) {
                    return obj.Serialize(arc);
                }
                return false;
            }

            /**
             * @brief Désérialise un archive dans un objet de type T
             * @tparam T Type de l'objet à peupler (doit être réfléchi ou NkISerializable)
             * @param arc Archive source contenant les données
             * @param obj Référence mutable vers l'objet à peupler
             * @return true si la désérialisation a réussi, false si T n'est pas sérialisable
             * @note noexcept : garantie de non-levée d'exception
             * @ingroup NkReflectAPI
             *
             * Comportement :
             *  - Si HasReflect<T> : appelle T::NkReflectDeserialize(arc, obj)
             *  - Si HasSerialize<T> : appelle obj.Deserialize(arc) via NkISerializable
             *  - Sinon : retourne false (type non supporté)
             *
             * @note Aucune allocation dynamique — tout passe par l'archive fourni.
             * @note Thread-safe : pas d'état interne mutable.
             *
             * @example Voir l'exemple dans la documentation du namespace NkReflect.
             */
            template<typename T>
            nk_bool Deserialize(const NkArchive& arc, T& obj) noexcept {
                if constexpr (reflect_detail::HasReflect<T>::value) {
                    T::NkReflectDeserialize(arc, obj);
                    return true;
                }
                else if constexpr (reflect_detail::HasSerialize<T>::value) {
                    return obj.Deserialize(arc);
                }
                return false;
            }


        } // namespace NkReflect


        // =============================================================================
        // MACROS : Déclaration de réflexion pour structs C++
        // =============================================================================
        /**
         * @defgroup NkReflectMacros Macros de Déclaration de Réflexion
         * @brief Macros pour déclarer la sérialisation automatique d'un struct
         *
         * Ces macros génèrent le code boilerplate pour :
         *  - Tag d'identification NkReflectTag (détection compile-time)
         *  - Méthodes statiques NkReflectSerialize/Deserialize
         *  - Appels à reflect_detail::SerializeField/DeserializeField pour chaque champ
         *
         * Styles d'usage :
         *  - NK_REFLECT_BEGIN/END : déclaration manuelle (plus de contrôle)
         *  - NK_REFLECT_STRUCT : déclaration concise (tout-en-un)
         *
         * @note Ces macros doivent être placées dans la section public du struct.
         * @note Les noms de champs sont stringifiés automatiquement (#FieldName).
         * @note NK_REFLECT_FIELD_AS permet de renommer la clé dans l'archive.
         */

        /**
         * @def NK_REFLECT_BEGIN(TypeName)
         * @brief Début de la déclaration de réflexion pour un type
         * @param TypeName Nom du type/struct à rendre sérialisable
         * @ingroup NkReflectMacros
         *
         * Génère :
         *  - using NkReflectTag = void; (tag de détection)
         *  - Début de la méthode statique NkReflectSerialize
         *
         * @note Doit être suivi de NK_REFLECT_FIELD* puis NK_REFLECT_END_SERIALIZE()
         *       puis NK_REFLECT_FIELD_D* puis NK_REFLECT_END()
         *
         * @example
         * @code
         * struct Vec3 {
         *     float x, y, z;
         *     NK_REFLECT_BEGIN(Vec3)
         *         NK_REFLECT_FIELD(x)
         *         NK_REFLECT_FIELD(y)
         *         NK_REFLECT_FIELD(z)
         *     NK_REFLECT_END_SERIALIZE()
         *         NK_REFLECT_FIELD_D(x)
         *         NK_REFLECT_FIELD_D(y)
         *         NK_REFLECT_FIELD_D(z)
         *     NK_REFLECT_END()
         * };
         * @endcode
         */
        #define NK_REFLECT_BEGIN(TypeName)                                              \
            using NkReflectTag = void;                                                  \
            static void NkReflectSerialize(const TypeName& _self, ::nkentseu::NkArchive& _arc) noexcept { \
                (void)_self; (void)_arc;

        /**
         * @def NK_REFLECT_FIELD(FieldName)
         * @brief Déclare un champ pour la sérialisation (nom automatique)
         * @param FieldName Nom du champ dans le struct (sera stringifié)
         * @ingroup NkReflectMacros
         *
         * Génère un appel à reflect_detail::SerializeField avec :
         *  - Clé : #FieldName (stringification du token)
         *  - Valeur : _self.FieldName
         *  - Archive : _arc (paramètre de la méthode générée)
         *
         * @note Doit être utilisé entre NK_REFLECT_BEGIN et NK_REFLECT_END_SERIALIZE
         *
         * @example NK_REFLECT_FIELD(health) → archive.SetFloat64("health", _self.health)
         */
        #define NK_REFLECT_FIELD(FieldName)                                             \
            ::nkentseu::reflect_detail::SerializeField(#FieldName, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_FIELD_AS(FieldName, KeyName)
         * @brief Déclare un champ pour la sérialisation avec nom personnalisé
         * @param FieldName Nom du champ dans le struct
         * @param KeyName Nom de la clé dans l'archive (string littéral)
         * @ingroup NkReflectMacros
         *
         * Comme NK_REFLECT_FIELD mais permet de renommer la clé dans l'archive.
         * Utile pour la compatibilité avec des formats externes ou des conventions.
         *
         * @example
         * @code
         * // Dans le struct :
         * NK_REFLECT_FIELD_AS(playerHealth, "health")
         * // Dans l'archive : clé = "health", pas "playerHealth"
         * @endcode
         */
        #define NK_REFLECT_FIELD_AS(FieldName, KeyName)                                 \
            ::nkentseu::reflect_detail::SerializeField(KeyName, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_END_SERIALIZE()
         * @brief Fin de la section sérialisation, début de la section désérialisation
         * @ingroup NkReflectMacros
         *
         * Ferme la méthode NkReflectSerialize et ouvre NkReflectDeserialize.
         * Doit être suivi de NK_REFLECT_FIELD_D* puis NK_REFLECT_END().
         *
         * @note Les champs doivent être listés deux fois (ser + deser) pour flexibilité.
         */
        #define NK_REFLECT_END_SERIALIZE()                                              \
            }                                                                           \
            static void NkReflectDeserialize(const ::nkentseu::NkArchive& _arc, TypeName& _self) noexcept { \
                (void)_arc; (void)_self;

        /**
         * @def NK_REFLECT_FIELD_D(FieldName)
         * @brief Déclare un champ pour la désérialisation (nom automatique)
         * @param FieldName Nom du champ dans le struct (sera stringifié)
         * @ingroup NkReflectMacros
         *
         * Génère un appel à reflect_detail::DeserializeField avec :
         *  - Clé : #FieldName (stringification du token)
         *  - Valeur : _self.FieldName (référence mutable)
         *  - Archive : _arc (paramètre de la méthode générée)
         *
         * @note Doit être utilisé entre NK_REFLECT_END_SERIALIZE et NK_REFLECT_END
         *
         * @example NK_REFLECT_FIELD_D(health) → archive.GetFloat64("health", _self.health)
         */
        #define NK_REFLECT_FIELD_D(FieldName)                                           \
            ::nkentseu::reflect_detail::DeserializeField(#FieldName, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_FIELD_AS_D(FieldName, KeyName)
         * @brief Déclare un champ pour la désérialisation avec nom personnalisé
         * @param FieldName Nom du champ dans le struct
         * @param KeyName Nom de la clé dans l'archive (string littéral)
         * @ingroup NkReflectMacros
         *
         * Comme NK_REFLECT_FIELD_D mais avec clé personnalisée.
         * Doit correspondre au KeyName utilisé dans NK_REFLECT_FIELD_AS.
         *
         * @example
         * @code
         * // Dans le struct :
         * NK_REFLECT_FIELD_AS(playerHealth, "health")      // Serialize
         * NK_REFLECT_FIELD_AS_D(playerHealth, "health")    // Deserialize
         * @endcode
         */
        #define NK_REFLECT_FIELD_AS_D(FieldName, KeyName)                               \
            ::nkentseu::reflect_detail::DeserializeField(KeyName, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_FIELD_BASE(FieldName)
         * @brief Déclare un champ à la fois pour la sérialisation ET la désérialisation (nom automatique)
         * @param FieldName Nom du champ dans le struct (sera stringifié pour les deux opérations)
         * @ingroup NkReflectMacros
         *
         * Combine NK_REFLECT_FIELD et NK_REFLECT_FIELD_D en une seule macro.
         * Génère un appel à SerializeField ET un appel à DeserializeField avec la même clé.
         *
         * La clé utilisée dans l'archive est #FieldName (stringification du token).
         * Utile quand les noms de champ C++ correspondent exactement aux noms dans l'archive.
         *
         * @note Doit être utilisé entre NK_REFLECT_BEGIN et NK_REFLECT_END.
         *       Remplace complètement la paire NK_REFLECT_FIELD + NK_REFLECT_FIELD_D.
         *       Ne pas utiliser avec NK_REFLECT_END_SERIALIZE (qui n'est plus nécessaire).
         *
         * @warning Cette macro ne permet pas d'avoir des noms différents entre
         *          la sérialisation et la désérialisation. Pour cela, utilisez
         *          NK_REFLECT_FIELD_BASE_AS à la place.
         *
         * @example
         * @code
         * NK_REFLECT_BEGIN(Player)
         *     NK_REFLECT_FIELD_BASE(health)   // archive.SetFloat64("health", ...)
         *                                      // archive.GetFloat64("health", ...)
         *     NK_REFLECT_FIELD_BASE(mana)     // archive.SetFloat64("mana", ...)
         *                                      // archive.GetFloat64("mana", ...)
         * NK_REFLECT_END()
         * @endcode
         *
         * @see NK_REFLECT_FIELD pour la version sérialisation uniquement
         * @see NK_REFLECT_FIELD_D pour la version désérialisation uniquement
         * @see NK_REFLECT_FIELD_BASE_AS pour la version avec noms personnalisés
         */
        #define NK_REFLECT_FIELD_BASE(FieldName)                                               \
            ::nkentseu::reflect_detail::SerializeField(#FieldName, _self.FieldName, _arc);     \
            ::nkentseu::reflect_detail::DeserializeField(#FieldName, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_FIELD_BASE_AS(FieldName, KeyNameSerialize, KeyNameDeserialize)
         * @brief Déclare un champ pour sérialisation ET désérialisation avec noms personnalisés distincts
         * @param FieldName Nom du champ dans le struct (identifiant C++)
         * @param KeyNameSerialize Nom de la clé pour la SÉRIALISATION dans l'archive (string littéral)
         * @param KeyNameDeserialize Nom de la clé pour la DÉSÉRIALISATION dans l'archive (string littéral)
         * @ingroup NkReflectMacros
         *
         * Combine NK_REFLECT_FIELD_AS et NK_REFLECT_FIELD_AS_D en une seule macro.
         * Génère un appel à SerializeField avec KeyNameSerialize ET un appel à
         * DeserializeField avec KeyNameDeserialize.
         *
         * Cette macro est la plus flexible : elle permet d'avoir des noms de clés
         * DIFFÉRENTS entre la sérialisation et la désérialisation, ce qui est utile pour :
         *  - Migration de format : lire l'ancien nom, écrire le nouveau
         *  - Compatibilité multi-versions : accepter plusieurs noms en entrée
         *  - Conventions externes : s'adapter à des formats imposés
         *
         * @note Doit être utilisé entre NK_REFLECT_BEGIN et NK_REFLECT_END.
         *       Remplace complètement la paire NK_REFLECT_FIELD_AS + NK_REFLECT_FIELD_AS_D.
         *       Ne pas utiliser avec NK_REFLECT_END_SERIALIZE (qui n'est plus nécessaire).
         *
         * @warning Les deux clés DOIVENT être des strings littéraux (entre guillemets).
         *          Si les clés sont identiques, préférez NK_REFLECT_FIELD_BASE pour la
         *          simplicité et la clarté du code.
         *
         * @example
         * @code
         * // Exemple 1 : Migration de format (ancien nom → nouveau nom)
         * NK_REFLECT_FIELD_BASE_AS(health, "hp", "healthPoints")
         * // Sérialise avec la clé "hp"
         * // Désérialise avec la clé "healthPoints"
         *
         * // Exemple 2 : Compatibilité ascendante (lecture de l'ancien format)
         * NK_REFLECT_FIELD_BASE_AS(playerName, "name", "player_name")
         * // Écrit avec "name", lit avec "player_name"
         * @endcode
         *
         * @see NK_REFLECT_FIELD_AS pour la version sérialisation uniquement avec clé personnalisée
         * @see NK_REFLECT_FIELD_AS_D pour la version désérialisation uniquement avec clé personnalisée
         * @see NK_REFLECT_FIELD_BASE pour la version avec clé automatique identique
         */
        #define NK_REFLECT_FIELD_BASE_AS(FieldName, KeyNameSerialize, KeyNameDeserialize)       \
            ::nkentseu::reflect_detail::SerializeField(KeyNameSerialize, _self.FieldName, _arc); \
            ::nkentseu::reflect_detail::DeserializeField(KeyNameDeserialize, _self.FieldName, _arc);

        /**
         * @def NK_REFLECT_END()
         * @brief Fin de la déclaration de réflexion
         * @ingroup NkReflectMacros
         *
         * Ferme la méthode NkReflectDeserialize.
         * Doit être le dernier macro après tous les NK_REFLECT_FIELD_D*.
         */
        #define NK_REFLECT_END()                                                        \
            }

        /**
         * @def NK_REFLECT_STRUCT(TypeName, SerFields, DeserFields)
         * @brief Macro tout-en-un pour déclaration concise de réflexion
         * @param TypeName Nom du type/struct à rendre sérialisable
         * @param SerFields Liste de NK_REFLECT_FIELD* pour la sérialisation
         * @param DeserFields Liste de NK_REFLECT_FIELD_D* pour la désérialisation
         * @ingroup NkReflectMacros
         *
         * Combinaison de NK_REFLECT_BEGIN/END_SERIALIZE/END en une seule macro.
         * Utile pour les structs simples où tous les champs ont le même nom en archive.
         *
         * @note Les champs doivent être listés deux fois (SerFields + DeserFields).
         * @note Pour des noms personnalisés, préférer la forme longue avec NK_REFLECT_FIELD_AS.
         *
         * @example
         * @code
         * struct Vec3 {
         *     float x, y, z;
         *     NK_REFLECT_STRUCT(Vec3,
         *         NK_REFLECT_FIELD(x) NK_REFLECT_FIELD(y) NK_REFLECT_FIELD(z),
         *         NK_REFLECT_FIELD_D(x) NK_REFLECT_FIELD_D(y) NK_REFLECT_FIELD_D(z))
         * };
         * @endcode
         */
        #define NK_REFLECT_STRUCT(TypeName, SerFields, DeserFields)                     \
            NK_REFLECT_BEGIN(TypeName)                                                  \
            SerFields                                                                   \
            NK_REFLECT_END_SERIALIZE()                                                  \
            DeserFields                                                                 \
            NK_REFLECT_END()


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKREFLECT.H
// =============================================================================
// Ce fichier fournit le système de réflexion statique pour sérialisation automatique.
// Il combine macros, SFINAE et if constexpr pour du code compile-time sans RTTI.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Struct simple avec champs scalaires
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkReflect.h>

    struct PlayerStats {
        nk_uint32 level;
        nk_float32 health;
        nk_float32 mana;
        nk_bool isAlive;

        // Déclaration de réflexion concise
        NK_REFLECT_STRUCT(PlayerStats,
            NK_REFLECT_FIELD(level)
            NK_REFLECT_FIELD(health)
            NK_REFLECT_FIELD(mana)
            NK_REFLECT_FIELD(isAlive),
            NK_REFLECT_FIELD_D(level)
            NK_REFLECT_FIELD_D(health)
            NK_REFLECT_FIELD_D(mana)
            NK_REFLECT_FIELD_D(isAlive))
    };

    // Usage :
    PlayerStats stats{42, 100.f, 50.f, true};
    nkentseu::NkArchive arc;

    // Sérialisation automatique
    if (nkentseu::NkReflect::Serialize(stats, arc)) {
        // arc contient maintenant : level=42, health=100.0, mana=50.0, isAlive=true
    }

    // Désérialisation automatique
    PlayerStats loaded;
    if (nkentseu::NkReflect::Deserialize(arc, loaded)) {
        // loaded.level == 42, loaded.health == 100.f, etc.
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Struct avec noms de clés personnalisés
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkReflect.h>

    struct NetworkConfig {
        nk_uint16 port;
        nk_bool enableEncryption;
        nk_float64 timeoutSeconds;

        // Noms personnalisés dans l'archive pour compatibilité externe
        NK_REFLECT_BEGIN(NetworkConfig)
            NK_REFLECT_FIELD_AS(port, "server_port")           // "port" → "server_port"
            NK_REFLECT_FIELD_AS(enableEncryption, "use_tls")   // "enableEncryption" → "use_tls"
            NK_REFLECT_FIELD_AS(timeoutSeconds, "timeout")     // "timeoutSeconds" → "timeout"
        NK_REFLECT_END_SERIALIZE()
            NK_REFLECT_FIELD_AS_D(port, "server_port")
            NK_REFLECT_FIELD_AS_D(enableEncryption, "use_tls")
            NK_REFLECT_FIELD_AS_D(timeoutSeconds, "timeout")
        NK_REFLECT_END()
    };

    // Résultat dans l'archive :
    // {
    //   "server_port": 8080,
    //   "use_tls": true,
    //   "timeout": 30.0
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Struct avec types imbriqués réfléchis
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkReflect.h>

    struct Vec3 {
        nk_float32 x, y, z;
        NK_REFLECT_STRUCT(Vec3,
            NK_REFLECT_FIELD(x) NK_REFLECT_FIELD(y) NK_REFLECT_FIELD(z),
            NK_REFLECT_FIELD_D(x) NK_REFLECT_FIELD_D(y) NK_REFLECT_FIELD_D(z))
    };

    struct Transform {
        Vec3 position;
        Vec3 rotation;
        nk_float32 scale;

        NK_REFLECT_STRUCT(Transform,
            NK_REFLECT_FIELD(position)
            NK_REFLECT_FIELD(rotation)
            NK_REFLECT_FIELD(scale),
            NK_REFLECT_FIELD_D(position)
            NK_REFLECT_FIELD_D(rotation)
            NK_REFLECT_FIELD_D(scale))
    };

    // Sérialisation récursive automatique :
    // Transform → position(Vec3) + rotation(Vec3) + scale(float)
    // L'archive contiendra une structure imbriquée :
    // {
    //   "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
    //   "rotation": { "x": 0.0, "y": 0.0, "z": 0.0 },
    //   "scale": 1.0
    // }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Intégration avec NkISerializable pour polymorphisme
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkReflect.h>
    #include <NKSerialization/NkISerializable.h>

    // Type polymorphique avec interface NkISerializable
    class GameObject : public nkentseu::NkISerializable {
    public:
        nk_uint64 entityId;
        nkentseu::NkString name;

        nk_bool Serialize(nkentseu::NkArchive& arc) const override {
            return arc.SetUInt64("entityId", entityId)
                && arc.SetString("name", name.View());
        }

        nk_bool Deserialize(const nkentseu::NkArchive& arc) override {
            return arc.GetUInt64("entityId", entityId)
                && arc.GetString("name", name);
        }
    };

    // Type réfléchi contenant un NkISerializable
    struct Scene {
        GameObject* mainCharacter;  // Pointeur — sérialisation manuelle requise
        nk_uint32 objectCount;

        NK_REFLECT_BEGIN(Scene)
            NK_REFLECT_FIELD(objectCount)
            // mainCharacter n'est pas réfléchi automatiquement (pointeur)
            // Doit être géré manuellement dans Serialize/Deserialize si besoin
        NK_REFLECT_END_SERIALIZE()
            NK_REFLECT_FIELD_D(objectCount)
        NK_REFLECT_END()

        // Sérialisation manuelle pour les pointeurs
        void SerializeExtra(nkentseu::NkArchive& arc) const {
            if (mainCharacter) {
                nkentseu::NkArchive charArc;
                mainCharacter->Serialize(charArc);
                arc.SetObject("mainCharacter", charArc);
            }
        }
    };

    // Usage combiné :
    Scene scene;
    scene.objectCount = 42;
    // scene.mainCharacter = new GameObject{...};

    nkentseu::NkArchive arc;
    nkentseu::NkReflect::Serialize(scene, arc);  // Sérialise objectCount
    scene.SerializeExtra(arc);                    // Sérialise mainCharacter manuellement
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Types non supportés et extensibilité
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/Native/NkReflect.h>

    struct CustomType {
        // Type personnalisé non supporté nativement
        void* internalData;
        nk_uint32 dataSize;
    };

    struct Config {
        nk_uint32 version;
        CustomType custom;  // Ce champ sera ignoré silencieusement

        NK_REFLECT_STRUCT(Config,
            NK_REFLECT_FIELD(version)
            // NK_REFLECT_FIELD(custom)  // ← Ne pas ajouter : type non supporté
            ,
            NK_REFLECT_FIELD_D(version)
            // NK_REFLECT_FIELD_D(custom)
            )
    };

    // Pour supporter CustomType :
    // Option 1 : Ajouter une spécialisation de reflect_detail::SerializeField
    // Option 2 : Implémenter NkISerializable pour CustomType
    // Option 3 : Utiliser NK_REFLECT_FIELD_AS avec une fonction de conversion custom

    // Exemple Option 2 :
    class CustomTypeSerializable : public nkentseu::NkISerializable {
    public:
        void* internalData;
        nk_uint32 dataSize;

        nk_bool Serialize(nkentseu::NkArchive& arc) const override {
            // Sérialisation custom de internalData...
            return true;
        }

        nk_bool Deserialize(const nkentseu::NkArchive& arc) override {
            // Désérialisation custom de internalData...
            return true;
        }
    };
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. ORDRE DES CHAMPS :
       - Les champs sont sérialisés dans l'ordre de déclaration des macros
       - Pour la désérialisation, l'ordre n'a pas d'importance (accès par clé)
       - Conserver l'ordre ser/deser identique pour lisibilité et debugging

    2. TYPES SUPPORTÉS :
       - Scalaires natifs : bool, int8..64, uint8..64, float32, float64
       - Strings : NkString uniquement (pas std::string)
       - Types imbriqués : doivent avoir NK_REFLECT_* ou implémenter NkISerializable
       - Pointeurs/tableaux : non supportés automatiquement — gestion manuelle requise

    3. NOMS DE CLÉS :
       - Par défaut : nom du champ C++ (stringifié via #FieldName)
       - Personnalisable via NK_REFLECT_FIELD_AS pour compatibilité externe
       - Les clés doivent être uniques dans un même niveau d'archive

    4. PERFORMANCE :
       - Tout le dispatch est compile-time via if constexpr — zéro overhead runtime
       - Aucune allocation dynamique dans les helpers — tout passe par NkArchive
       - Les macros génèrent du code inline — pas de sauts de fonction inutiles

    5. EXTENSIBILITÉ :
       - Pour ajouter un type scalaire : ajouter une branche else if constexpr
         dans reflect_detail::SerializeField/DeserializeField
       - Pour ajouter un type composé : implémenter NkISerializable ou NK_REFLECT_*
       - Pour des conversions custom : spécialiser les helpers ou utiliser FIELD_AS

    6. DEBUGGING :
       - Les erreurs de type sont détectées à la compilation (SFINAE)
       - Les échecs de parsing retournent des valeurs par défaut (pas d'exception)
       - Utiliser NkArchive::Has() pour vérifier la présence d'un champ avant lecture

    7. THREAD-SAFETY :
       - Toutes les fonctions sont stateless — thread-safe par conception
       - L'archive NkArchive passé en paramètre n'est pas thread-safe — synchronisation externe requise si partagé
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================