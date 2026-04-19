// =============================================================================
// NKSerialization/NkArchive.h
// =============================================================================
// Archive centrale clé/valeur qui sert d'intermediaire universel entre tous
// les formats (JSON, YAML, XML, Binary, NkNative).
//
// Hiérarchie des valeurs :
//   NkArchiveValue  → scalaire (null, bool, int64, uint64, float64, string)
//   NkArchive       → objet plat ordonné de clés → valeurs
//   NkArchiveNode   → valeur pouvant contenir un scalaire, un objet ou un array
//
// NOTE: On maintient deux niveaux d'API pour la compatibilité :
//   1. API plate historique (SetString/GetString…) : clés non-hiérarchiques.
//   2. API hiérarchique (SetNode/GetNode + chemin "parent.child") : pour le
//      système de fichiers d'assets style Unreal.
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // ─── Valeur scalaire ─────────────────────────────────────────────────────────
    enum class NkArchiveValueType : nk_uint8 {
        NK_VALUE_NULL    = 0,
        NK_VALUE_BOOL    = 1,
        NK_VALUE_INT64   = 2,
        NK_VALUE_UINT64  = 3,
        NK_VALUE_FLOAT64 = 4,
        NK_VALUE_STRING  = 5,
    };

    // ─── Discriminant de nœud ────────────────────────────────────────────────────
    enum class NkNodeKind : nk_uint8 {
        Scalar = 0,   // NkArchiveValue
        Object = 1,   // NkArchive imbriqué
        Array  = 2,   // Tableau de NkArchiveNode
    };

    // Forward declarations
    struct NkArchiveValue;
    struct NkArchiveEntry;
    class  NkArchive;
    struct NkArchiveNode;

    // =============================================================================
    // NkArchiveValue — Valeur scalaire portable
    // =============================================================================
    struct NkArchiveValue {
        NkArchiveValueType type = NkArchiveValueType::NK_VALUE_NULL;

        // Stockage unifié : les scalaires petit-endian sont toujours sérialisés
        // sans passer par une conversion string → on garde le texte comme repr
        // canonique pour les formats texte (JSON/YAML/XML) et un union pour
        // les formats binaires.
        union Raw {
            nk_bool   b;
            nk_int64  i;
            nk_uint64 u;
            nk_float64 f;
            Raw() noexcept : u(0) {}
        } raw;

        NkString text;  // Représentation textuelle canonique (toujours à jour)

        // ── Factories ────────────────────────────────────────────────────────────
        [[nodiscard]] static NkArchiveValue Null()    noexcept;
        [[nodiscard]] static NkArchiveValue FromBool(nk_bool v) noexcept;
        [[nodiscard]] static NkArchiveValue FromInt32(nk_int32 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromInt64(nk_int64 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromUInt32(nk_uint32 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromUInt64(nk_uint64 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromFloat32(nk_float32 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromFloat64(nk_float64 v) noexcept;
        [[nodiscard]] static NkArchiveValue FromString(NkStringView v) noexcept;

        // ── Helpers ──────────────────────────────────────────────────────────────
        [[nodiscard]] bool IsNull()    const noexcept { return type == NkArchiveValueType::NK_VALUE_NULL; }
        [[nodiscard]] bool IsBool()    const noexcept { return type == NkArchiveValueType::NK_VALUE_BOOL; }
        [[nodiscard]] bool IsInt()     const noexcept { return type == NkArchiveValueType::NK_VALUE_INT64; }
        [[nodiscard]] bool IsUInt()    const noexcept { return type == NkArchiveValueType::NK_VALUE_UINT64; }
        [[nodiscard]] bool IsFloat()   const noexcept { return type == NkArchiveValueType::NK_VALUE_FLOAT64; }
        [[nodiscard]] bool IsString()  const noexcept { return type == NkArchiveValueType::NK_VALUE_STRING; }
        [[nodiscard]] bool IsNumeric() const noexcept {
            return type == NkArchiveValueType::NK_VALUE_INT64   ||
                type == NkArchiveValueType::NK_VALUE_UINT64  ||
                type == NkArchiveValueType::NK_VALUE_FLOAT64;
        }

        bool operator==(const NkArchiveValue& o) const noexcept {
            return type == o.type && text == o.text;
        }
        bool operator!=(const NkArchiveValue& o) const noexcept { return !(*this == o); }
    };

    // =============================================================================
    // NkArchiveNode — Nœud hiérarchique (scalaire | objet | array)
    // =============================================================================
    struct NkArchiveNode {
            NkNodeKind kind = NkNodeKind::Scalar;

            // Scalaire
            NkArchiveValue value;

            // Objet (NkArchive imbrique — allocation dynamique pour éviter la récursion)
            NkArchive* object = nullptr;  // owning ptr, kind == Object

            // Array de nœuds
            NkVector<NkArchiveNode> array; // kind == Array

            NkArchiveNode() noexcept = default;
            explicit NkArchiveNode(const NkArchiveValue& v) noexcept
                : kind(NkNodeKind::Scalar), value(v) {}

            NkArchiveNode(const NkArchiveNode& o) noexcept;
            NkArchiveNode(NkArchiveNode&& o) noexcept;
            NkArchiveNode& operator=(const NkArchiveNode& o) noexcept;
            NkArchiveNode& operator=(NkArchiveNode&& o) noexcept;
            ~NkArchiveNode() noexcept;

            void SetObject(const NkArchive& arc) noexcept;
            void MakeArray()  noexcept { FreeObject(); kind = NkNodeKind::Array; array.Clear(); }
            void MakeScalar() noexcept { FreeObject(); kind = NkNodeKind::Scalar; array.Clear(); }

            [[nodiscard]] bool IsScalar() const noexcept { return kind == NkNodeKind::Scalar; }
            [[nodiscard]] bool IsObject() const noexcept { return kind == NkNodeKind::Object && object; }
            [[nodiscard]] bool IsArray()  const noexcept { return kind == NkNodeKind::Array; }

        private:
            void FreeObject() noexcept;
    };

    // =============================================================================
    // NkArchiveEntry — Paire clé + nœud
    // =============================================================================
    struct NkArchiveEntry {
        NkString        key;
        NkArchiveNode   node;
    };

    // =============================================================================
    // NkArchive — Table associative ordonnée (clé → nœud)
    // =============================================================================
    class NkArchive {
        public:
            NkArchive() noexcept = default;
            NkArchive(const NkArchive&) noexcept = default;
            NkArchive(NkArchive&&) noexcept = default;
            NkArchive& operator=(const NkArchive&) noexcept = default;
            NkArchive& operator=(NkArchive&&) noexcept = default;
            ~NkArchive() noexcept = default;

            // ── Setters scalaires ────────────────────────────────────────────────────
            nk_bool SetNull    (NkStringView key) noexcept;
            nk_bool SetBool    (NkStringView key, nk_bool v) noexcept;
            nk_bool SetInt32   (NkStringView key, nk_int32 v) noexcept;
            nk_bool SetInt64   (NkStringView key, nk_int64 v) noexcept;
            nk_bool SetUInt32  (NkStringView key, nk_uint32 v) noexcept;
            nk_bool SetUInt64  (NkStringView key, nk_uint64 v) noexcept;
            nk_bool SetFloat32 (NkStringView key, nk_float32 v) noexcept;
            nk_bool SetFloat64 (NkStringView key, nk_float64 v) noexcept;
            nk_bool SetString  (NkStringView key, NkStringView v) noexcept;
            nk_bool SetValue   (NkStringView key, const NkArchiveValue& v) noexcept;

            // ── Getters scalaires ────────────────────────────────────────────────────
            [[nodiscard]] nk_bool GetValue  (NkStringView key, NkArchiveValue& out) const noexcept;
            [[nodiscard]] nk_bool GetBool   (NkStringView key, nk_bool& out)    const noexcept;
            [[nodiscard]] nk_bool GetInt32  (NkStringView key, nk_int32& out)   const noexcept;
            [[nodiscard]] nk_bool GetInt64  (NkStringView key, nk_int64& out)   const noexcept;
            [[nodiscard]] nk_bool GetUInt32 (NkStringView key, nk_uint32& out)  const noexcept;
            [[nodiscard]] nk_bool GetUInt64 (NkStringView key, nk_uint64& out)  const noexcept;
            [[nodiscard]] nk_bool GetFloat32(NkStringView key, nk_float32& out) const noexcept;
            [[nodiscard]] nk_bool GetFloat64(NkStringView key, nk_float64& out) const noexcept;
            [[nodiscard]] nk_bool GetString (NkStringView key, NkString& out)   const noexcept;

            // ── Objets imbriqués ─────────────────────────────────────────────────────
            nk_bool SetObject(NkStringView key, const NkArchive& arc) noexcept;
            [[nodiscard]] nk_bool GetObject(NkStringView key, NkArchive& out) const noexcept;

            // ── Tableaux ─────────────────────────────────────────────────────────────
            nk_bool SetArray(NkStringView key, const NkVector<NkArchiveValue>& arr) noexcept;
            [[nodiscard]] nk_bool GetArray(NkStringView key, NkVector<NkArchiveValue>& out) const noexcept;

            nk_bool SetNodeArray(NkStringView key, const NkVector<NkArchiveNode>& arr) noexcept;
            [[nodiscard]] nk_bool GetNodeArray(NkStringView key, NkVector<NkArchiveNode>& out) const noexcept;

            // ── Nœuds bruts ─────────────────────────────────────────────────────────
            nk_bool SetNode(NkStringView key, const NkArchiveNode& node) noexcept;
            [[nodiscard]] nk_bool GetNode(NkStringView key, NkArchiveNode& out) const noexcept;
            [[nodiscard]] const NkArchiveNode* FindNode(NkStringView key) const noexcept;
            [[nodiscard]]       NkArchiveNode* FindNode(NkStringView key)       noexcept;

            // ── Accesseur par chemin hiérarchique "a.b.c" ────────────────────────────
            nk_bool SetPath(NkStringView path, const NkArchiveNode& node) noexcept;
            [[nodiscard]] const NkArchiveNode* GetPath(NkStringView path) const noexcept;

            // ── Meta-données inline (système asset style Unreal) ─────────────────────
            // Les méta-données sont stockées sous la clé "__meta__" comme objet imbriqué.
            nk_bool SetMeta(NkStringView metaKey, NkStringView value) noexcept;
            [[nodiscard]] nk_bool GetMeta(NkStringView metaKey, NkString& out) const noexcept;

            // ── Gestion ─────────────────────────────────────────────────────────────
            [[nodiscard]] nk_bool Has(NkStringView key)    const noexcept;
            nk_bool Remove(NkStringView key) noexcept;
            void    Clear() noexcept;
            void    Merge(const NkArchive& other, bool overwrite = true) noexcept;

            [[nodiscard]] nk_size  Size()    const noexcept { return mEntries.Size(); }
            [[nodiscard]] nk_bool  Empty()   const noexcept { return mEntries.Empty(); }
            [[nodiscard]] const NkVector<NkArchiveEntry>& Entries() const noexcept { return mEntries; }
            [[nodiscard]]       NkVector<NkArchiveEntry>& Entries()       noexcept { return mEntries; }

        private:
            [[nodiscard]] nk_size FindIndex(NkStringView key) const noexcept;
            [[nodiscard]] static bool IsValidKey(NkStringView key) noexcept;

            NkVector<NkArchiveEntry> mEntries;

            static constexpr nk_size NPOS = static_cast<nk_size>(-1);
    };

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED
