// =============================================================================
// NKSerialization/NkArchive.cpp
// =============================================================================
#include "NKSerialization/NkArchive.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace nkentseu {

    // =============================================================================
    // NkArchiveValue — Factories
    // =============================================================================
    NkArchiveValue NkArchiveValue::Null() noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_NULL;
        return v;
    }
    NkArchiveValue NkArchiveValue::FromBool(nk_bool b) noexcept {
        NkArchiveValue v;
        v.type   = NkArchiveValueType::NK_VALUE_BOOL;
        v.raw.b  = b;
        v.text   = b ? NkString("true") : NkString("false");
        return v;
    }
    NkArchiveValue NkArchiveValue::FromInt32(nk_int32 i) noexcept {
        return FromInt64(static_cast<nk_int64>(i));
    }
    NkArchiveValue NkArchiveValue::FromInt64(nk_int64 i) noexcept {
        NkArchiveValue v;
        v.type  = NkArchiveValueType::NK_VALUE_INT64;
        v.raw.i = i;
        v.text  = NkString::Fmtf("%lld", static_cast<long long>(i));
        return v;
    }
    NkArchiveValue NkArchiveValue::FromUInt32(nk_uint32 u) noexcept {
        return FromUInt64(static_cast<nk_uint64>(u));
    }
    NkArchiveValue NkArchiveValue::FromUInt64(nk_uint64 u) noexcept {
        NkArchiveValue v;
        v.type  = NkArchiveValueType::NK_VALUE_UINT64;
        v.raw.u = u;
        v.text  = NkString::Fmtf("%llu", static_cast<unsigned long long>(u));
        return v;
    }
    NkArchiveValue NkArchiveValue::FromFloat32(nk_float32 f) noexcept {
        return FromFloat64(static_cast<nk_float64>(f));
    }
    NkArchiveValue NkArchiveValue::FromFloat64(nk_float64 f) noexcept {
        NkArchiveValue v;
        v.type  = NkArchiveValueType::NK_VALUE_FLOAT64;
        v.raw.f = f;
        v.text  = NkString::Fmtf("%.17g", f);
        return v;
    }
    NkArchiveValue NkArchiveValue::FromString(NkStringView sv) noexcept {
        NkArchiveValue v;
        v.type = NkArchiveValueType::NK_VALUE_STRING;
        v.text = NkString(sv);
        return v;
    }

    // =============================================================================
    // NkArchiveNode — Big 5
    // =============================================================================
    static void CloneNode(NkArchiveNode& dst, const NkArchiveNode& src) noexcept;

    void NkArchiveNode::FreeObject() noexcept {
        delete object;
        object = nullptr;
    }

    NkArchiveNode::~NkArchiveNode() noexcept {
        FreeObject();
    }

    NkArchiveNode::NkArchiveNode(const NkArchiveNode& o) noexcept {
        CloneNode(*this, o);
    }
    NkArchiveNode::NkArchiveNode(NkArchiveNode&& o) noexcept
        : kind(o.kind), value(std::move(o.value)), object(o.object), array(std::move(o.array))
    {
        o.object = nullptr;
        o.kind   = NkNodeKind::Scalar;
    }
    NkArchiveNode& NkArchiveNode::operator=(const NkArchiveNode& o) noexcept {
        if (this != &o) { FreeObject(); array.Clear(); CloneNode(*this, o); }
        return *this;
    }
    NkArchiveNode& NkArchiveNode::operator=(NkArchiveNode&& o) noexcept {
        if (this != &o) {
            FreeObject(); array.Clear();
            kind   = o.kind;
            value  = std::move(o.value);
            object = o.object;
            array  = std::move(o.array);
            o.object = nullptr;
            o.kind   = NkNodeKind::Scalar;
        }
        return *this;
    }

    void NkArchiveNode::SetObject(const NkArchive& arc) noexcept {
        FreeObject();
        array.Clear();
        kind   = NkNodeKind::Object;
        object = new NkArchive(arc);
    }

    static void CloneNode(NkArchiveNode& dst, const NkArchiveNode& src) noexcept {
        dst.kind  = src.kind;
        dst.value = src.value;
        if (src.kind == NkNodeKind::Object && src.object) {
            dst.object = new NkArchive(*src.object);
        }
        if (src.kind == NkNodeKind::Array) {
            dst.array = src.array;
        }
    }

    // =============================================================================
    // NkArchive — helpers internes
    // =============================================================================
    nk_size NkArchive::FindIndex(NkStringView key) const noexcept {
        for (nk_size i = 0; i < mEntries.Size(); ++i) {
            if (mEntries[i].key.Compare(key) == 0) return i;
        }
        return NPOS;
    }

    bool NkArchive::IsValidKey(NkStringView key) noexcept {
        return !key.Empty();
    }

    // =============================================================================
    // NkArchive — SetNode / GetNode
    // =============================================================================
    nk_bool NkArchive::SetNode(NkStringView key, const NkArchiveNode& node) noexcept {
        if (!IsValidKey(key)) return false;
        nk_size idx = FindIndex(key);
        if (idx != NPOS) {
            mEntries[idx].node = node;
            return true;
        }
        NkArchiveEntry e;
        e.key  = NkString(key);
        e.node = node;
        mEntries.PushBack(std::move(e));
        return true;
    }

    nk_bool NkArchive::GetNode(NkStringView key, NkArchiveNode& out) const noexcept {
        nk_size idx = FindIndex(key);
        if (idx == NPOS) return false;
        out = mEntries[idx].node;
        return true;
    }

    const NkArchiveNode* NkArchive::FindNode(NkStringView key) const noexcept {
        nk_size idx = FindIndex(key);
        return (idx != NPOS) ? &mEntries[idx].node : nullptr;
    }
    NkArchiveNode* NkArchive::FindNode(NkStringView key) noexcept {
        nk_size idx = FindIndex(key);
        return (idx != NPOS) ? &mEntries[idx].node : nullptr;
    }

    // =============================================================================
    // NkArchive — Scalaires
    // =============================================================================
    nk_bool NkArchive::SetValue(NkStringView key, const NkArchiveValue& v) noexcept {
        return SetNode(key, NkArchiveNode(v));
    }
    nk_bool NkArchive::SetNull   (NkStringView k) noexcept { return SetValue(k, NkArchiveValue::Null()); }
    nk_bool NkArchive::SetBool   (NkStringView k, nk_bool b)     noexcept { return SetValue(k, NkArchiveValue::FromBool(b)); }
    nk_bool NkArchive::SetInt32  (NkStringView k, nk_int32 i)    noexcept { return SetValue(k, NkArchiveValue::FromInt32(i)); }
    nk_bool NkArchive::SetInt64  (NkStringView k, nk_int64 i)    noexcept { return SetValue(k, NkArchiveValue::FromInt64(i)); }
    nk_bool NkArchive::SetUInt32 (NkStringView k, nk_uint32 u)   noexcept { return SetValue(k, NkArchiveValue::FromUInt32(u)); }
    nk_bool NkArchive::SetUInt64 (NkStringView k, nk_uint64 u)   noexcept { return SetValue(k, NkArchiveValue::FromUInt64(u)); }
    nk_bool NkArchive::SetFloat32(NkStringView k, nk_float32 f)  noexcept { return SetValue(k, NkArchiveValue::FromFloat32(f)); }
    nk_bool NkArchive::SetFloat64(NkStringView k, nk_float64 f)  noexcept { return SetValue(k, NkArchiveValue::FromFloat64(f)); }
    nk_bool NkArchive::SetString (NkStringView k, NkStringView v) noexcept { return SetValue(k, NkArchiveValue::FromString(v)); }

    nk_bool NkArchive::GetValue(NkStringView key, NkArchiveValue& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsScalar()) return false;
        out = n->value;
        return true;
    }

    nk_bool NkArchive::GetBool(NkStringView key, nk_bool& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) return false;
        if (v.type == NkArchiveValueType::NK_VALUE_BOOL) { out = v.raw.b; return true; }
        // Coercition depuis string
        bool parsed = false;
        if (!v.text.ToBool(parsed)) return false;
        out = parsed;
        return true;
    }

    nk_bool NkArchive::GetInt32(NkStringView key, nk_int32& out) const noexcept {
        nk_int64 v = 0;
        if (!GetInt64(key, v)) return false;
        out = static_cast<nk_int32>(v);
        return true;
    }

    nk_bool NkArchive::GetInt64(NkStringView key, nk_int64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) return false;
        if (v.type == NkArchiveValueType::NK_VALUE_INT64)  { out = v.raw.i; return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64) { out = static_cast<nk_int64>(v.raw.u); return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64){ out = static_cast<nk_int64>(v.raw.f); return true; }
        return v.text.ToInt64(out);
    }

    nk_bool NkArchive::GetUInt32(NkStringView key, nk_uint32& out) const noexcept {
        nk_uint64 v = 0;
        if (!GetUInt64(key, v)) return false;
        out = static_cast<nk_uint32>(v);
        return true;
    }

    nk_bool NkArchive::GetUInt64(NkStringView key, nk_uint64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) return false;
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64) { out = v.raw.u; return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_INT64)  { out = static_cast<nk_uint64>(v.raw.i); return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64){ out = static_cast<nk_uint64>(v.raw.f); return true; }
        return v.text.ToUInt64(out);
    }

    nk_bool NkArchive::GetFloat32(NkStringView key, nk_float32& out) const noexcept {
        nk_float64 v = 0.0;
        if (!GetFloat64(key, v)) return false;
        out = static_cast<nk_float32>(v);
        return true;
    }

    nk_bool NkArchive::GetFloat64(NkStringView key, nk_float64& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) return false;
        if (v.type == NkArchiveValueType::NK_VALUE_FLOAT64) { out = v.raw.f; return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_INT64)   { out = static_cast<nk_float64>(v.raw.i); return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_UINT64)  { out = static_cast<nk_float64>(v.raw.u); return true; }
        if (v.type == NkArchiveValueType::NK_VALUE_BOOL || v.type == NkArchiveValueType::NK_VALUE_NULL) return false;
        return v.text.ToDouble(out);
    }

    nk_bool NkArchive::GetString(NkStringView key, NkString& out) const noexcept {
        NkArchiveValue v;
        if (!GetValue(key, v)) return false;
        out = v.text;
        return true;
    }

    // =============================================================================
    // NkArchive — Objets imbriqués
    // =============================================================================
    nk_bool NkArchive::SetObject(NkStringView key, const NkArchive& arc) noexcept {
        if (!IsValidKey(key)) return false;
        nk_size idx = FindIndex(key);
        if (idx != NPOS) {
            mEntries[idx].node.SetObject(arc);
            return true;
        }
        NkArchiveEntry e;
        e.key = NkString(key);
        e.node.SetObject(arc);
        mEntries.PushBack(std::move(e));
        return true;
    }

    nk_bool NkArchive::GetObject(NkStringView key, NkArchive& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsObject()) return false;
        out = *n->object;
        return true;
    }

    // =============================================================================
    // NkArchive — Arrays
    // =============================================================================
    nk_bool NkArchive::SetArray(NkStringView key, const NkVector<NkArchiveValue>& arr) noexcept {
        if (!IsValidKey(key)) return false;
        NkArchiveNode node;
        node.MakeArray();
        for (nk_size i = 0; i < arr.Size(); ++i) {
            NkArchiveNode elem(arr[i]);
            node.array.PushBack(std::move(elem));
        }
        return SetNode(key, node);
    }

    nk_bool NkArchive::GetArray(NkStringView key, NkVector<NkArchiveValue>& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsArray()) return false;
        out.Clear();
        for (nk_size i = 0; i < n->array.Size(); ++i) {
            if (n->array[i].IsScalar()) out.PushBack(n->array[i].value);
        }
        return true;
    }

    nk_bool NkArchive::SetNodeArray(NkStringView key, const NkVector<NkArchiveNode>& arr) noexcept {
        if (!IsValidKey(key)) return false;
        NkArchiveNode node;
        node.MakeArray();
        node.array = arr;
        return SetNode(key, node);
    }

    nk_bool NkArchive::GetNodeArray(NkStringView key, NkVector<NkArchiveNode>& out) const noexcept {
        const NkArchiveNode* n = FindNode(key);
        if (!n || !n->IsArray()) return false;
        out = n->array;
        return true;
    }

    // =============================================================================
    // NkArchive — Chemin hiérarchique "a.b.c"
    // =============================================================================
    nk_bool NkArchive::SetPath(NkStringView path, const NkArchiveNode& node) noexcept {
        if (path.Empty()) return false;

        // Trouver le séparateur
        nk_size dot = static_cast<nk_size>(-1);
        for (nk_size i = 0; i < path.Length(); ++i)
            if (path[i] == '.') { dot = i; break; }

        if (dot == static_cast<nk_size>(-1)) {
            // Feuille
            return SetNode(path, node);
        }

        NkStringView head = path.SubStr(0, dot);
        NkStringView tail = path.SubStr(dot + 1);

        // Récupérer ou créer le sous-objet
        NkArchive child;
        const NkArchiveNode* existing = FindNode(head);
        if (existing && existing->IsObject()) child = *existing->object;

        if (!child.SetPath(tail, node)) return false;
        return SetObject(head, child);
    }

    const NkArchiveNode* NkArchive::GetPath(NkStringView path) const noexcept {
        if (path.Empty()) return nullptr;

        nk_size dot = static_cast<nk_size>(-1);
        for (nk_size i = 0; i < path.Length(); ++i)
            if (path[i] == '.') { dot = i; break; }

        if (dot == static_cast<nk_size>(-1)) return FindNode(path);

        NkStringView head = path.SubStr(0, dot);
        NkStringView tail = path.SubStr(dot + 1);

        const NkArchiveNode* n = FindNode(head);
        if (!n || !n->IsObject()) return nullptr;
        return n->object->GetPath(tail);
    }

    // =============================================================================
    // NkArchive — Meta-données
    // =============================================================================
    nk_bool NkArchive::SetMeta(NkStringView metaKey, NkStringView value) noexcept {
        // Utilise le chemin "__meta__.key"
        NkString path("__meta__.");
        path.Append(metaKey);
        return SetPath(path.View(), NkArchiveNode(NkArchiveValue::FromString(value)));
    }

    nk_bool NkArchive::GetMeta(NkStringView metaKey, NkString& out) const noexcept {
        NkString path("__meta__.");
        path.Append(metaKey);
        const NkArchiveNode* n = GetPath(path.View());
        if (!n || !n->IsScalar()) return false;
        out = n->value.text;
        return true;
    }

    // =============================================================================
    // NkArchive — Gestion
    // =============================================================================
    nk_bool NkArchive::Has(NkStringView key) const noexcept {
        return FindIndex(key) != NPOS;
    }

    nk_bool NkArchive::Remove(NkStringView key) noexcept {
        nk_size idx = FindIndex(key);
        if (idx == NPOS) return false;
        mEntries.Erase(mEntries.begin() + idx);
        return true;
    }

    void NkArchive::Clear() noexcept {
        mEntries.Clear();
    }

    void NkArchive::Merge(const NkArchive& other, bool overwrite) noexcept {
        for (nk_size i = 0; i < other.mEntries.Size(); ++i) {
            const NkArchiveEntry& e = other.mEntries[i];
            if (overwrite || !Has(e.key.View())) {
                SetNode(e.key.View(), e.node);
            }
        }
    }

} // namespace nkentseu
