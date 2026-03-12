#pragma once

#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H_INCLUDED
#define NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"

namespace nkentseu {
namespace entseu {

enum class NkJSONValueType : nk_uint8 {
    NK_JSON_NULL = 0,
    NK_JSON_BOOL,
    NK_JSON_NUMBER,
    NK_JSON_STRING
};

class NkJSONValue {
public:
    NkJSONValue();

    static NkJSONValue Null();
    static NkJSONValue FromBool(nk_bool value);
    static NkJSONValue FromNumber(float64 value);
    static NkJSONValue FromString(NkStringView value);

    NkJSONValueType GetType() const;

    nk_bool IsNull() const;
    nk_bool IsBool() const;
    nk_bool IsNumber() const;
    nk_bool IsString() const;

    nk_bool AsBool(nk_bool defaultValue = false) const;
    float64 AsNumber(float64 defaultValue = 0.0) const;
    const NkString& AsString() const;

private:
    NkJSONValueType mType;
    nk_bool mBool;
    float64 mNumber;
    NkString mString;
};

NkString NkJSONEscapeString(NkStringView input);
NkString NkJSONUnescapeString(NkStringView input, nk_bool& ok);
NkString NkJSONValueToString(const NkJSONValue& value);

} // namespace entseu

using NkJSONValue = entseu::NkJSONValue;
using NkJSONValueType = entseu::NkJSONValueType;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H_INCLUDED
