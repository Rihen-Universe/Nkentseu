#include "NKSerialization/JSON/NkJSONValue.h"

namespace nkentseu {
namespace entseu {

NkJSONValue::NkJSONValue()
    : mType(NkJSONValueType::NK_JSON_NULL)
    , mBool(false)
    , mNumber(0.0)
    , mString() {
}

NkJSONValue NkJSONValue::Null() {
    return NkJSONValue();
}

NkJSONValue NkJSONValue::FromBool(nk_bool value) {
    NkJSONValue out;
    out.mType = NkJSONValueType::NK_JSON_BOOL;
    out.mBool = value;
    return out;
}

NkJSONValue NkJSONValue::FromNumber(float64 value) {
    NkJSONValue out;
    out.mType = NkJSONValueType::NK_JSON_NUMBER;
    out.mNumber = value;
    return out;
}

NkJSONValue NkJSONValue::FromString(NkStringView value) {
    NkJSONValue out;
    out.mType = NkJSONValueType::NK_JSON_STRING;
    out.mString = NkString(value);
    return out;
}

NkJSONValueType NkJSONValue::GetType() const {
    return mType;
}

nk_bool NkJSONValue::IsNull() const {
    return mType == NkJSONValueType::NK_JSON_NULL;
}

nk_bool NkJSONValue::IsBool() const {
    return mType == NkJSONValueType::NK_JSON_BOOL;
}

nk_bool NkJSONValue::IsNumber() const {
    return mType == NkJSONValueType::NK_JSON_NUMBER;
}

nk_bool NkJSONValue::IsString() const {
    return mType == NkJSONValueType::NK_JSON_STRING;
}

nk_bool NkJSONValue::AsBool(nk_bool defaultValue) const {
    return IsBool() ? mBool : defaultValue;
}

float64 NkJSONValue::AsNumber(float64 defaultValue) const {
    return IsNumber() ? mNumber : defaultValue;
}

const NkString& NkJSONValue::AsString() const {
    return mString;
}

NkString NkJSONEscapeString(NkStringView input) {
    NkString out;
    out.Reserve(input.Length() + 8);

    for (nk_size i = 0; i < input.Length(); ++i) {
        const char ch = input[i];
        switch (ch) {
            case '\\': out.Append("\\\\"); break;
            case '"':  out.Append("\\\""); break;
            case '\n': out.Append("\\n"); break;
            case '\r': out.Append("\\r"); break;
            case '\t': out.Append("\\t"); break;
            case '\b': out.Append("\\b"); break;
            case '\f': out.Append("\\f"); break;
            default:   out.Append(ch); break;
        }
    }
    return out;
}

NkString NkJSONUnescapeString(NkStringView input, nk_bool& ok) {
    ok = true;
    NkString out;
    out.Reserve(input.Length());

    for (nk_size i = 0; i < input.Length(); ++i) {
        const char ch = input[i];
        if (ch != '\\') {
            out.Append(ch);
            continue;
        }

        if (i + 1 >= input.Length()) {
            ok = false;
            return NkString();
        }

        const char esc = input[++i];
        switch (esc) {
            case '\\': out.Append('\\'); break;
            case '"': out.Append('"'); break;
            case 'n': out.Append('\n'); break;
            case 'r': out.Append('\r'); break;
            case 't': out.Append('\t'); break;
            case 'b': out.Append('\b'); break;
            case 'f': out.Append('\f'); break;
            case '/': out.Append('/'); break;
            case 'u':
                // Minimal unicode support for now: keep marker when escape is malformed,
                // otherwise consume 4 hex digits and emit '?'.
                if (i + 4 >= input.Length()) {
                    ok = false;
                    return NkString();
                }
                i += 4;
                out.Append('?');
                break;
            default:
                ok = false;
                return NkString();
        }
    }

    return out;
}

NkString NkJSONValueToString(const NkJSONValue& value) {
    switch (value.GetType()) {
        case NkJSONValueType::NK_JSON_NULL:
            return NkString("null");
        case NkJSONValueType::NK_JSON_BOOL:
            return NkString(value.AsBool() ? "true" : "false");
        case NkJSONValueType::NK_JSON_NUMBER:
            return NkString::Fmtf("%.17g", value.AsNumber());
        case NkJSONValueType::NK_JSON_STRING:
        default:
            return NkString("\"") + NkJSONEscapeString(value.AsString().View()) + "\"";
    }
}

} // namespace entseu
} // namespace nkentseu
