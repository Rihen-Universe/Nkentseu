// =============================================================================
// NKSerialization/Native/NkReflect.h
// =============================================================================
// Système de réflexion statique minimaliste pour la sérialisation automatique
// des structs C++ sans RTTI.
//
// Usage :
//   struct Vec3 {
//       float x, y, z;
//       NK_REFLECT_BEGIN(Vec3)
//           NK_REFLECT_FIELD(x)
//           NK_REFLECT_FIELD(y)
//           NK_REFLECT_FIELD(z)
//       NK_REFLECT_END()
//   };
//
//   // Sérialisation automatique :
//   NkArchive arc;
//   NkReflect::Serialize(vec3, arc);
//   NkReflect::Deserialize(arc, vec3);
//
// Types supportés automatiquement :
//   bool, int8..int64, uint8..uint64, float32, float64, NkString
//   Types imbriqués ayant eux-mêmes NK_REFLECT_*
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include "NKCore/NkTraits.h"
#include <type_traits>

namespace nkentseu {

    // =============================================================================
    // Traits de sérialisation scalaire
    // =============================================================================
    namespace reflect_detail {

        // Détecte si T a une méthode Serialize(NkArchive&)
        template<typename T, typename = void>
        struct HasSerialize : traits::NkFalseType {};
        template<typename T>
        struct HasSerialize<T, traits::NkVoidT<
            decltype(std::declval<const T&>().Serialize(std::declval<NkArchive&>()))>> : traits::NkTrueType {};

        // Détecte si T a les macros NK_REFLECT (via ::NkReflectFields)
        template<typename T, typename = void>
        struct HasReflect : traits::NkFalseType {};
        template<typename T>
        struct HasReflect<T, traits::NkVoidT<typename T::NkReflectTag>> : traits::NkTrueType {};

        // Sérialisation d'un champ scalaire vers NkArchive
        template<typename T>
        void SerializeField(NkStringView key, const T& v, NkArchive& arc) noexcept {
            if constexpr (traits::NkIsSame_v<T, nk_bool> || traits::NkIsSame_v<T, bool>) {
                arc.SetBool(key, static_cast<nk_bool>(v));
            } else if constexpr (traits::NkIsSame_v<T, nk_int8>  || traits::NkIsSame_v<T, nk_int16> ||
                                traits::NkIsSame_v<T, nk_int32> || traits::NkIsSame_v<T, nk_int64>) {
                arc.SetInt64(key, static_cast<nk_int64>(v));
            } else if constexpr (traits::NkIsSame_v<T, nk_uint8>  || traits::NkIsSame_v<T, nk_uint16> ||
                                traits::NkIsSame_v<T, nk_uint32> || traits::NkIsSame_v<T, nk_uint64>) {
                arc.SetUInt64(key, static_cast<nk_uint64>(v));
            } else if constexpr (traits::NkIsSame_v<T, nk_float32>) {
                arc.SetFloat64(key, static_cast<nk_float64>(v));
            } else if constexpr (traits::NkIsSame_v<T, nk_float64>) {
                arc.SetFloat64(key, v);
            } else if constexpr (traits::NkIsSame_v<T, NkString>) {
                arc.SetString(key, v.View());
            } else if constexpr (HasReflect<T>::value) {
                // Type réfléchi imbriqué
                NkArchive childArc;
                T::NkReflectSerialize(v, childArc);
                arc.SetObject(key, childArc);
            } else if constexpr (HasSerialize<T>::value) {
                // NkISerializable
                NkArchive childArc;
                v.Serialize(childArc);
                arc.SetObject(key, childArc);
            }
            // Sinon : type non supporté → ignoré silencieusement
        }

        template<typename T>
        void DeserializeField(NkStringView key, T& v, const NkArchive& arc) noexcept {
            if constexpr (traits::NkIsSame_v<T, nk_bool> || traits::NkIsSame_v<T, bool>) {
                nk_bool b = false; arc.GetBool(key, b); v = static_cast<T>(b);
            } else if constexpr (traits::NkIsSame_v<T, nk_int8>  || traits::NkIsSame_v<T, nk_int16> ||
                                traits::NkIsSame_v<T, nk_int32> || traits::NkIsSame_v<T, nk_int64>) {
                nk_int64 i = 0; arc.GetInt64(key, i); v = static_cast<T>(i);
            } else if constexpr (traits::NkIsSame_v<T, nk_uint8>  || traits::NkIsSame_v<T, nk_uint16> ||
                                traits::NkIsSame_v<T, nk_uint32> || traits::NkIsSame_v<T, nk_uint64>) {
                nk_uint64 u = 0; arc.GetUInt64(key, u); v = static_cast<T>(u);
            } else if constexpr (traits::NkIsSame_v<T, nk_float32>) {
                nk_float64 f = 0.0; arc.GetFloat64(key, f); v = static_cast<nk_float32>(f);
            } else if constexpr (traits::NkIsSame_v<T, nk_float64>) {
                arc.GetFloat64(key, v);
            } else if constexpr (traits::NkIsSame_v<T, NkString>) {
                arc.GetString(key, v);
            } else if constexpr (HasReflect<T>::value) {
                NkArchive childArc;
                if (arc.GetObject(key, childArc)) T::NkReflectDeserialize(childArc, v);
            } else if constexpr (HasSerialize<T>::value) {
                NkArchive childArc;
                if (arc.GetObject(key, childArc)) v.Deserialize(childArc);
            }
        }

    } // namespace reflect_detail

    // =============================================================================
    // Namespace public NkReflect
    // =============================================================================
    namespace NkReflect {

        template<typename T>
        nk_bool Serialize(const T& obj, NkArchive& arc) noexcept {
            if constexpr (reflect_detail::HasReflect<T>::value) {
                T::NkReflectSerialize(obj, arc);
                return true;
            } else if constexpr (reflect_detail::HasSerialize<T>::value) {
                return obj.Serialize(arc);
            }
            return false;
        }

        template<typename T>
        nk_bool Deserialize(const NkArchive& arc, T& obj) noexcept {
            if constexpr (reflect_detail::HasReflect<T>::value) {
                T::NkReflectDeserialize(arc, obj);
                return true;
            } else if constexpr (reflect_detail::HasSerialize<T>::value) {
                return obj.Deserialize(arc);
            }
            return false;
        }

    } // namespace NkReflect

    // =============================================================================
    // Macros de déclaration de réflexion
    // =============================================================================

    // À placer dans le corps de la struct (public)
    #define NK_REFLECT_BEGIN(TypeName)                                              \
        using NkReflectTag = void;                                                  \
        static void NkReflectSerialize(const TypeName& _self, ::nkentseu::NkArchive& _arc) noexcept { \
            (void)_self; (void)_arc;

    #define NK_REFLECT_FIELD(FieldName)                                             \
            ::nkentseu::reflect_detail::SerializeField(#FieldName, _self.FieldName, _arc);

    #define NK_REFLECT_FIELD_AS(FieldName, KeyName)                                 \
            ::nkentseu::reflect_detail::SerializeField(KeyName, _self.FieldName, _arc);

    #define NK_REFLECT_END_SERIALIZE()                                              \
        }                                                                           \
        static void NkReflectDeserialize(const ::nkentseu::NkArchive& _arc, TypeName& _self) noexcept { \
            (void)_arc; (void)_self;

    #define NK_REFLECT_FIELD_D(FieldName)                                           \
            ::nkentseu::reflect_detail::DeserializeField(#FieldName, _self.FieldName, _arc);

    #define NK_REFLECT_FIELD_AS_D(FieldName, KeyName)                               \
            ::nkentseu::reflect_detail::DeserializeField(KeyName, _self.FieldName, _arc);

    #define NK_REFLECT_END()                                                        \
        }

    // Macro tout-en-un (les champs doivent être listés deux fois pour ser+deser)
    // Usage :
    //   NK_REFLECT_STRUCT(Vec3,
    //       NK_REFLECT_FIELD(x) NK_REFLECT_FIELD(y) NK_REFLECT_FIELD(z),
    //       NK_REFLECT_FIELD_D(x) NK_REFLECT_FIELD_D(y) NK_REFLECT_FIELD_D(z))
    #define NK_REFLECT_STRUCT(TypeName, SerFields, DeserFields)                     \
        NK_REFLECT_BEGIN(TypeName)                                                  \
        SerFields                                                                   \
        NK_REFLECT_END_SERIALIZE()                                                  \
        DeserFields                                                                 \
        NK_REFLECT_END()

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NATIVE_NKREFLECT_H_INCLUDED
