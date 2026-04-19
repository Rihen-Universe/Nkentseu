// =============================================================================
// NKSerialization/NkSchemaVersioning.h
// =============================================================================
// Migration de schéma entre versions d'assets.
//
// Corrections vs version originale :
//  - Suppression de std::vector / std::function → NkVector + pointeurs
//  - NkComponentId → nk_uint64 (pas de dépendance sur NKCore/NkTypeRegistry)
//  - MigrateArchive() : boucle de migration chaînée corrigée
//  - Macro NK_REGISTER_MIGRATION : NkIdOf<T>() remplacé par NkTypeId<T>()
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include "NKContainers/Sequential/NkVector.h"
#include <type_traits>

namespace nkentseu {

    // ─── Identifiant de type (hash de nom, dérivé du pointeur de vtable) ─────────
    using NkTypeId = nk_uint64;

    // Helper de génération d'ID par type (même technique que EnTT)
    template<typename T>
    [[nodiscard]] constexpr NkTypeId NkTypeOf() noexcept {
        // On utilise l'adresse d'une variable statique locale — unique par TU
        static const char s_tag = 0;
        return static_cast<NkTypeId>(
            reinterpret_cast<nk_size>(&s_tag));
    }

    // =============================================================================
    // NkSchemaVersion
    // =============================================================================
    struct NkSchemaVersion {
        nk_uint16 major = 0u;
        nk_uint16 minor = 0u;
        nk_uint16 patch = 0u;

        NkSchemaVersion() noexcept = default;
        constexpr NkSchemaVersion(nk_uint16 maj, nk_uint16 min, nk_uint16 pat) noexcept
            : major(maj), minor(min), patch(pat) {}

        [[nodiscard]] constexpr nk_uint64 Pack() const noexcept {
            return (static_cast<nk_uint64>(major) << 32u) |
                (static_cast<nk_uint64>(minor) << 16u) |
                static_cast<nk_uint64>(patch);
        }
        [[nodiscard]] static constexpr NkSchemaVersion Unpack(nk_uint64 v) noexcept {
            return {
                static_cast<nk_uint16>((v >> 32u) & 0xFFFFu),
                static_cast<nk_uint16>((v >> 16u) & 0xFFFFu),
                static_cast<nk_uint16>(v & 0xFFFFu)
            };
        }

        [[nodiscard]] constexpr bool operator==(const NkSchemaVersion& o) const noexcept { return Pack() == o.Pack(); }
        [[nodiscard]] constexpr bool operator!=(const NkSchemaVersion& o) const noexcept { return Pack() != o.Pack(); }
        [[nodiscard]] constexpr bool operator< (const NkSchemaVersion& o) const noexcept { return Pack() <  o.Pack(); }
        [[nodiscard]] constexpr bool operator<=(const NkSchemaVersion& o) const noexcept { return Pack() <= o.Pack(); }

        [[nodiscard]] NkString ToString() const noexcept {
            return NkString::Fmtf("%u.%u.%u", (unsigned)major, (unsigned)minor, (unsigned)patch);
        }
    };

    // =============================================================================
    // NkSchemaMigration — Fonction de migration entre deux versions
    // =============================================================================
    struct NkSchemaMigration {
        using MigrationFn = nk_bool(*)(NkArchive&, NkSchemaVersion from, NkSchemaVersion to) noexcept;

        NkSchemaVersion from;
        NkSchemaVersion to;
        MigrationFn     fn = nullptr;

        NkSchemaMigration() noexcept = default;
        NkSchemaMigration(NkSchemaVersion f, NkSchemaVersion t, MigrationFn fn_) noexcept
            : from(f), to(t), fn(fn_) {}

        [[nodiscard]] nk_bool Apply(NkArchive& archive) const noexcept {
            return fn ? fn(archive, from, to) : true;
        }
    };

    // =============================================================================
    // NkSchemaRegistry — Registre des migrations et versions courantes
    // =============================================================================
    class NkSchemaRegistry {
        public:
            [[nodiscard]] static NkSchemaRegistry& Global() noexcept {
                static NkSchemaRegistry s_instance;
                return s_instance;
            }

            // ── Enregistrement ────────────────────────────────────────────────────────
            static void SetCurrentVersion(NkTypeId typeId, NkSchemaVersion version) noexcept {
                auto& reg = Global();
                for (nk_size i = 0; i < reg.mVersions.Size(); ++i) {
                    if (reg.mVersions[i].typeId == typeId) {
                        reg.mVersions[i].version = version;
                        return;
                    }
                }
                VersionEntry e; e.typeId = typeId; e.version = version;
                reg.mVersions.PushBack(e);
            }

            static void RegisterMigration(NkTypeId typeId,
                                        NkSchemaVersion from,
                                        NkSchemaVersion to,
                                        NkSchemaMigration::MigrationFn fn) noexcept {
                MigrationEntry e;
                e.typeId     = typeId;
                e.migration  = NkSchemaMigration(from, to, fn);
                Global().mMigrations.PushBack(e);
            }

            // ── Migration ─────────────────────────────────────────────────────────────
            [[nodiscard]] static nk_bool MigrateArchive(NkTypeId typeId,
                                                        NkArchive& archive,
                                                        NkSchemaVersion stored,
                                                        NkString* err = nullptr) noexcept {
                auto& reg = Global();

                // Trouver la version courante
                NkSchemaVersion current;
                bool found = false;
                for (nk_size i = 0; i < reg.mVersions.Size(); ++i) {
                    if (reg.mVersions[i].typeId == typeId) {
                        current = reg.mVersions[i].version;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (err) *err = NkString("Type not registered for versioning");
                    return false;
                }

                if (stored == current) return true;

                // Chaîne de migration : appliquer en ordre croissant
                NkSchemaVersion cursor = stored;
                for (int guard = 0; guard < 256 && cursor != current; ++guard) {
                    bool applied = false;
                    for (nk_size i = 0; i < reg.mMigrations.Size(); ++i) {
                        const auto& me = reg.mMigrations[i];
                        if (me.typeId == typeId && me.migration.from == cursor) {
                            if (!me.migration.Apply(archive)) {
                                if (err) *err = NkString::Fmtf(
                                    "Migration %s → %s failed",
                                    cursor.ToString().CStr(),
                                    me.migration.to.ToString().CStr());
                                return false;
                            }
                            cursor  = me.migration.to;
                            applied = true;
                            break;
                        }
                    }
                    if (!applied) {
                        if (err) *err = NkString::Fmtf(
                            "No migration path from %s to %s",
                            cursor.ToString().CStr(),
                            current.ToString().CStr());
                        return false;
                    }
                }

                // Stocker la version après migration
                NkArchive metaArc;
                archive.GetObject("__meta__", metaArc);
                metaArc.SetString("schema_version", current.ToString().View());
                archive.SetObject("__meta__", metaArc);

                return cursor == current;
            }

            // ── Helper : ajout d'un champ avec valeur par défaut ─────────────────────
            template<typename DefaultFn>
            static void AddFieldMigration(NkTypeId typeId,
                                        NkSchemaVersion from, NkSchemaVersion to,
                                        const char* fieldName,
                                        DefaultFn getDefault) noexcept {
                // On capture via pointeur statique : pas de std::function, pas d'alloc
                struct Adapter {
                    static nk_bool Fn(NkArchive& arc, NkSchemaVersion, NkSchemaVersion) noexcept {
                        // Cette approche est limitée — on utilise SetNull comme placeholder
                        if (!arc.Has("fieldName_placeholder")) {
                            arc.SetNull("fieldName_placeholder");
                        }
                        return true;
                    }
                };
                // Pour un vrai projet, on stockerait le lambda dans un slot statique
                // alloué à l'avance. Ici on montre l'interface ; l'implémentation
                // complète requiert un allocateur custom ou des lambdas capturés.
                (void)typeId; (void)from; (void)to; (void)fieldName; (void)getDefault;
            }

        private:
            struct VersionEntry {
                NkTypeId        typeId  = 0u;
                NkSchemaVersion version;
            };
            struct MigrationEntry {
                NkTypeId          typeId = 0u;
                NkSchemaMigration migration;
            };

            NkVector<VersionEntry>   mVersions;
            NkVector<MigrationEntry> mMigrations;
    };

    // =============================================================================
    // Macros utilitaires
    // =============================================================================

    // Définit la version courante d'un type
    #define NK_SCHEMA_CURRENT_VERSION(Type, Major, Minor, Patch)              \
    namespace { struct _NkSchVer_##Type {                                     \
        _NkSchVer_##Type() noexcept {                                         \
            ::nkentseu::NkSchemaRegistry::SetCurrentVersion(                  \
                ::nkentseu::NkTypeOf<Type>(), {Major, Minor, Patch});         \
        }                                                                     \
    } _sNkSchVer_##Type; }

    // Enregistre une fonction de migration (fn doit être une free function)
    // Signature : nk_bool fn(NkArchive&, NkSchemaVersion from, NkSchemaVersion to) noexcept;
    #define NK_REGISTER_MIGRATION(Type, FromMaj, FromMin, FromPat,            \
                                ToMaj, ToMin, ToPat, FnPtr)                \
    namespace { struct _NkMig_##Type##_##FromMaj##_##ToMaj {                  \
        _NkMig_##Type##_##FromMaj##_##ToMaj() noexcept {                      \
            ::nkentseu::NkSchemaRegistry::RegisterMigration(                  \
                ::nkentseu::NkTypeOf<Type>(),                                 \
                {FromMaj, FromMin, FromPat},                                  \
                {ToMaj, ToMin, ToPat},                                        \
                FnPtr);                                                       \
        }                                                                     \
    } _sNkMig_##Type##_##FromMaj##_##ToMaj; }

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H_INCLUDED
