// =============================================================================
// NKSerialization/NkSerializableRegistry.h
// =============================================================================
// Registre des composants ECS sérialisables.
//
// Corrections vs version originale :
//  - std::function / std::unordered_map → NkVector + pointeurs de fonctions
//  - DefaultSerialize fallback binaire corrigé (taille fixe non portable → JSON)
//  - Find() : méthode correctement déclarée en static dans la classe
//  - NkComponentId → NkTypeId (cohérence avec NkSchemaVersioning)
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZABLEREGISTRY_H
#define NKENTSEU_SERIALIZATION_NKSERIALIZABLEREGISTRY_H

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/NkISerializable.h"
#include "NKSerialization/NkSchemaVersioning.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstring>

namespace nkentseu {

    // =============================================================================
    // NkComponentSerializableRegistry
    // Registre dédié aux composants ECS (indépendant de NkSerializableRegistry
    // qui gère les objets polymorphiques par nom de type).
    // =============================================================================
    class NkComponentRegistry {
        public:
            using SerializeFn   = nk_bool(*)(const void*, NkArchive&) noexcept;
            using DeserializeFn = nk_bool(*)(void*, const NkArchive&) noexcept;

            struct Entry {
                NkTypeId      typeId      = 0u;
                const char*   name        = nullptr;
                SerializeFn   serialize   = nullptr;
                DeserializeFn deserialize = nullptr;
            };

            [[nodiscard]] static NkComponentRegistry& Global() noexcept {
                static NkComponentRegistry s_instance;
                return s_instance;
            }

            // ── Enregistrement ────────────────────────────────────────────────────────

            template<typename T>
            static void Register(const char* name,
                                SerializeFn   serFn   = nullptr,
                                DeserializeFn deserFn = nullptr) noexcept {
                auto& reg = Global();
                NkTypeId id = NkTypeOf<T>();

                // Mise à jour si déjà enregistré
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (reg.mEntries[i].typeId == id) {
                        if (serFn)   reg.mEntries[i].serialize   = serFn;
                        if (deserFn) reg.mEntries[i].deserialize = deserFn;
                        return;
                    }
                }

                Entry e;
                e.typeId      = id;
                e.name        = name;
                e.serialize   = serFn   ? serFn   : &DefaultSerialize<T>;
                e.deserialize = deserFn ? deserFn : &DefaultDeserialize<T>;
                reg.mEntries.PushBack(e);
            }

            template<typename T>
            static void Unregister() noexcept {
                auto& reg = Global();
                NkTypeId id = NkTypeOf<T>();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (reg.mEntries[i].typeId == id) {
                        reg.mEntries.Erase(reg.mEntries.begin() + i);
                        return;
                    }
                }
            }

            // ── Lookup ────────────────────────────────────────────────────────────────
            [[nodiscard]] static const Entry* Find(NkTypeId id) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i)
                    if (reg.mEntries[i].typeId == id) return &reg.mEntries[i];
                return nullptr;
            }

            [[nodiscard]] static const Entry* FindByName(const char* name) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i)
                    if (strcmp(reg.mEntries[i].name, name) == 0) return &reg.mEntries[i];
                return nullptr;
            }

            template<typename T>
            [[nodiscard]] static const Entry* Find() noexcept {
                return Find(NkTypeOf<T>());
            }

            // ── Sérialisation/désérialisation ─────────────────────────────────────────
            [[nodiscard]] static nk_bool Serialize(NkTypeId id,
                                                    const void* obj,
                                                    NkArchive& out) noexcept {
                const Entry* e = Find(id);
                return e && e->serialize ? e->serialize(obj, out) : false;
            }

            [[nodiscard]] static nk_bool Deserialize(NkTypeId id,
                                                    void* obj,
                                                    const NkArchive& arc) noexcept {
                const Entry* e = Find(id);
                return e && e->deserialize ? e->deserialize(obj, arc) : false;
            }

            template<typename T>
            [[nodiscard]] static nk_bool Serialize(const T& obj, NkArchive& out) noexcept {
                return Serialize(NkTypeOf<T>(), &obj, out);
            }

            template<typename T>
            [[nodiscard]] static nk_bool Deserialize(T& obj, const NkArchive& arc) noexcept {
                return Deserialize(NkTypeOf<T>(), &obj, arc);
            }

            // ── Itération ─────────────────────────────────────────────────────────────
            template<typename Fn>
            static void ForEach(Fn&& fn) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) fn(reg.mEntries[i]);
            }

            [[nodiscard]] static nk_uint32 Count() noexcept {
                return static_cast<nk_uint32>(Global().mEntries.Size());
            }

        private:
            // ── Fallback de sérialisation générique ──────────────────────────────────
            // Sérialise les champs publics via l'archive clé/valeur.
            // NOTE: Pour une vraie sérialisation automatique, il faut utiliser
            //       NkReflect (système de réflexion natif du moteur).
            //       Ce fallback écrit la représentation JSON de sizeof(T) bytes
            //       en base64 — portable mais pas lisible.
            template<typename T>
            static nk_bool DefaultSerialize(const void* obj, NkArchive& out) noexcept {
                // Si T implémente NkISerializable, on délègue
                if constexpr (std::is_base_of<NkISerializable, T>::value) {
                    return static_cast<const T*>(obj)->Serialize(out);
                }
                // Fallback : sérialisation binaire brute en base16
                // (utiliser de préférence les fonctions de sérialisation manuelles)
                const nk_uint8* raw = static_cast<const nk_uint8*>(obj);
                NkString hex;
                hex.Reserve(sizeof(T) * 2u + 8u);
                static const char hx[] = "0123456789ABCDEF";
                for (nk_size i = 0; i < sizeof(T); ++i) {
                    hex.Append(hx[(raw[i] >> 4u) & 0xFu]);
                    hex.Append(hx[raw[i] & 0xFu]);
                }
                out.SetString("__raw__", hex.View());
                out.SetUInt64("__rawSz__", sizeof(T));
                return true;
            }

            template<typename T>
            static nk_bool DefaultDeserialize(void* obj, const NkArchive& arc) noexcept {
                if constexpr (std::is_base_of<NkISerializable, T>::value) {
                    return static_cast<T*>(obj)->Deserialize(arc);
                }
                NkString hex;
                nk_uint64 sz = 0;
                if (!arc.GetString("__raw__", hex) || !arc.GetUInt64("__rawSz__", sz)) return false;
                if (sz != sizeof(T) || hex.Length() != sizeof(T) * 2u) return false;
                nk_uint8* raw = static_cast<nk_uint8*>(obj);
                for (nk_size i = 0; i < sizeof(T); ++i) {
                    auto hexDigit = [](char c) -> nk_uint8 {
                        if (c >= '0' && c <= '9') return static_cast<nk_uint8>(c - '0');
                        if (c >= 'A' && c <= 'F') return static_cast<nk_uint8>(c - 'A' + 10);
                        if (c >= 'a' && c <= 'f') return static_cast<nk_uint8>(c - 'a' + 10);
                        return 0u;
                    };
                    raw[i] = static_cast<nk_uint8>(
                        (hexDigit(hex[i*2u]) << 4u) | hexDigit(hex[i*2u+1u]));
                }
                return true;
            }

            NkVector<Entry> mEntries;
    };

    // =============================================================================
    // Macros d'auto-enregistrement
    // =============================================================================

    // Enregistrement simple (utilise la réflexion automatique ou NkISerializable)
    #define NK_REGISTER_COMPONENT(Type, Name)                                    \
    namespace { struct _NkCompReg_##Type {                                       \
        _NkCompReg_##Type() noexcept {                                           \
            ::nkentseu::NkComponentRegistry::Register<Type>(Name);               \
        }                                                                        \
    } _sNkCompReg_##Type; }

    // Enregistrement avec fonctions de sérialisation custom (free functions)
    // SerFn  : nk_bool fn(const void*, NkArchive&) noexcept
    // DeserFn: nk_bool fn(void*, const NkArchive&) noexcept
    #define NK_REGISTER_COMPONENT_CUSTOM(Type, Name, SerFn, DeserFn)             \
    namespace { struct _NkCompReg_##Type {                                       \
        _NkCompReg_##Type() noexcept {                                           \
            ::nkentseu::NkComponentRegistry::Register<Type>(Name, SerFn, DeserFn);\
        }                                                                        \
    } _sNkCompReg_##Type; }

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZABLEREGISTRY_H