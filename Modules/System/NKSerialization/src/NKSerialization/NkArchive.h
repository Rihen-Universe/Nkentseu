#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"

namespace nkentseu {
namespace entseu {

enum class NkArchiveValueType : nk_uint8 {
    NK_VALUE_NULL = 0,
    NK_VALUE_BOOL,
    NK_VALUE_INT64,
    NK_VALUE_UINT64,
    NK_VALUE_FLOAT64,
    NK_VALUE_STRING
};

struct NkArchiveValue {
    NkArchiveValueType type = NkArchiveValueType::NK_VALUE_NULL;
    NkString text;

    static NkArchiveValue Null();
    static NkArchiveValue FromBool(nk_bool value);
    static NkArchiveValue FromInt64(int64 value);
    static NkArchiveValue FromUInt64(uint64 value);
    static NkArchiveValue FromFloat64(float64 value);
    static NkArchiveValue FromString(NkStringView value);
};

struct NkArchiveEntry {
    NkString key;
    NkArchiveValue value;
};

class NkArchive {
public:
    NkArchive() = default;

    nk_bool SetValue(NkStringView key, const NkArchiveValue& value);
    nk_bool SetNull(NkStringView key);
    nk_bool SetBool(NkStringView key, nk_bool value);
    nk_bool SetInt64(NkStringView key, int64 value);
    nk_bool SetUInt64(NkStringView key, uint64 value);
    nk_bool SetFloat64(NkStringView key, float64 value);
    nk_bool SetString(NkStringView key, NkStringView value);

    nk_bool GetValue(NkStringView key, NkArchiveValue& outValue) const;
    nk_bool GetBool(NkStringView key, nk_bool& outValue) const;
    nk_bool GetInt64(NkStringView key, int64& outValue) const;
    nk_bool GetUInt64(NkStringView key, uint64& outValue) const;
    nk_bool GetFloat64(NkStringView key, float64& outValue) const;
    nk_bool GetString(NkStringView key, NkString& outValue) const;

    nk_bool Has(NkStringView key) const;
    nk_bool Remove(NkStringView key);
    void Clear();

    nk_size Size() const;
    nk_bool Empty() const;

    const NkVector<NkArchiveEntry>& Entries() const;

private:
    nk_size FindIndex(NkStringView key) const;
    static nk_bool IsValidKey(NkStringView key);

private:
    NkVector<NkArchiveEntry> mEntries;
};

} // namespace entseu

using NkArchive = entseu::NkArchive;
using NkArchiveEntry = entseu::NkArchiveEntry;
using NkArchiveValue = entseu::NkArchiveValue;
using NkArchiveValueType = entseu::NkArchiveValueType;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKARCHIVE_H_INCLUDED
