#include "NKSerialization/NkArchive.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
namespace entseu {

namespace {
constexpr nk_size NK_ARCHIVE_NPOS = static_cast<nk_size>(-1);

nk_bool NkIsIntegerLike(NkArchiveValueType type) {
    return type == NkArchiveValueType::NK_VALUE_INT64 || type == NkArchiveValueType::NK_VALUE_UINT64;
}

} // namespace

NkArchiveValue NkArchiveValue::Null() {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_NULL;
    out.text.Clear();
    return out;
}

NkArchiveValue NkArchiveValue::FromBool(nk_bool value) {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_BOOL;
    out.text = value ? "true" : "false";
    return out;
}

NkArchiveValue NkArchiveValue::FromInt64(int64 value) {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_INT64;
    out.text = NkString::Fmtf("%lld", static_cast<long long>(value));
    return out;
}

NkArchiveValue NkArchiveValue::FromUInt64(uint64 value) {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_UINT64;
    out.text = NkString::Fmtf("%llu", static_cast<unsigned long long>(value));
    return out;
}

NkArchiveValue NkArchiveValue::FromFloat64(float64 value) {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_FLOAT64;
    out.text = NkString::Fmtf("%.17g", value);
    return out;
}

NkArchiveValue NkArchiveValue::FromString(NkStringView value) {
    NkArchiveValue out;
    out.type = NkArchiveValueType::NK_VALUE_STRING;
    out.text = NkString(value);
    return out;
}

nk_bool NkArchive::SetValue(NkStringView key, const NkArchiveValue& value) {
    if (!IsValidKey(key)) {
        return false;
    }

    const nk_size idx = FindIndex(key);
    if (idx != NK_ARCHIVE_NPOS) {
        mEntries[idx].value = value;
        return true;
    }

    NkArchiveEntry entry;
    entry.key = NkString(key);
    entry.value = value;
    mEntries.PushBack(traits::NkMove(entry));
    return true;
}

nk_bool NkArchive::SetNull(NkStringView key) {
    return SetValue(key, NkArchiveValue::Null());
}

nk_bool NkArchive::SetBool(NkStringView key, nk_bool value) {
    return SetValue(key, NkArchiveValue::FromBool(value));
}

nk_bool NkArchive::SetInt64(NkStringView key, int64 value) {
    return SetValue(key, NkArchiveValue::FromInt64(value));
}

nk_bool NkArchive::SetUInt64(NkStringView key, uint64 value) {
    return SetValue(key, NkArchiveValue::FromUInt64(value));
}

nk_bool NkArchive::SetFloat64(NkStringView key, float64 value) {
    return SetValue(key, NkArchiveValue::FromFloat64(value));
}

nk_bool NkArchive::SetString(NkStringView key, NkStringView value) {
    return SetValue(key, NkArchiveValue::FromString(value));
}

nk_bool NkArchive::GetValue(NkStringView key, NkArchiveValue& outValue) const {
    const nk_size idx = FindIndex(key);
    if (idx == NK_ARCHIVE_NPOS) {
        return false;
    }
    outValue = mEntries[idx].value;
    return true;
}

nk_bool NkArchive::GetBool(NkStringView key, nk_bool& outValue) const {
    NkArchiveValue value;
    if (!GetValue(key, value)) {
        return false;
    }

    if (value.type == NkArchiveValueType::NK_VALUE_BOOL) {
        outValue = (value.text == "true");
        return true;
    }

    bool parsed = false;
    if (!value.text.ToBool(parsed)) {
        return false;
    }
    outValue = parsed;
    return true;
}

nk_bool NkArchive::GetInt64(NkStringView key, int64& outValue) const {
    NkArchiveValue value;
    if (!GetValue(key, value)) {
        return false;
    }

    if (!(value.type == NkArchiveValueType::NK_VALUE_INT64 || NkIsIntegerLike(value.type))) {
        if (value.type != NkArchiveValueType::NK_VALUE_FLOAT64 && value.type != NkArchiveValueType::NK_VALUE_STRING) {
            return false;
        }
    }

    return value.text.ToInt64(outValue);
}

nk_bool NkArchive::GetUInt64(NkStringView key, uint64& outValue) const {
    NkArchiveValue value;
    if (!GetValue(key, value)) {
        return false;
    }

    if (!(value.type == NkArchiveValueType::NK_VALUE_UINT64 || value.type == NkArchiveValueType::NK_VALUE_INT64)) {
        if (value.type != NkArchiveValueType::NK_VALUE_FLOAT64 && value.type != NkArchiveValueType::NK_VALUE_STRING) {
            return false;
        }
    }

    return value.text.ToUInt64(outValue);
}

nk_bool NkArchive::GetFloat64(NkStringView key, float64& outValue) const {
    NkArchiveValue value;
    if (!GetValue(key, value)) {
        return false;
    }

    if (value.type == NkArchiveValueType::NK_VALUE_BOOL || value.type == NkArchiveValueType::NK_VALUE_NULL) {
        return false;
    }

    return value.text.ToDouble(outValue);
}

nk_bool NkArchive::GetString(NkStringView key, NkString& outValue) const {
    NkArchiveValue value;
    if (!GetValue(key, value)) {
        return false;
    }
    outValue = value.text;
    return true;
}

nk_bool NkArchive::Has(NkStringView key) const {
    return FindIndex(key) != NK_ARCHIVE_NPOS;
}

nk_bool NkArchive::Remove(NkStringView key) {
    const nk_size idx = FindIndex(key);
    if (idx == NK_ARCHIVE_NPOS) {
        return false;
    }

    mEntries.Erase(mEntries.begin() + idx);
    return true;
}

void NkArchive::Clear() {
    mEntries.Clear();
}

nk_size NkArchive::Size() const {
    return mEntries.Size();
}

nk_bool NkArchive::Empty() const {
    return mEntries.Empty();
}

const NkVector<NkArchiveEntry>& NkArchive::Entries() const {
    return mEntries;
}

nk_size NkArchive::FindIndex(NkStringView key) const {
    for (nk_size i = 0; i < mEntries.Size(); ++i) {
        if (mEntries[i].key.Compare(key) == 0) {
            return i;
        }
    }
    return NK_ARCHIVE_NPOS;
}

nk_bool NkArchive::IsValidKey(NkStringView key) {
    return !key.Empty();
}

} // namespace entseu
} // namespace nkentseu
