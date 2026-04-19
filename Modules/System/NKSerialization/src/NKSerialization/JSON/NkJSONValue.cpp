#include "NKSerialization/JSON/NkJSONValue.h"
namespace nkentseu {

NkString NkJSONEscapeString(NkStringView input) noexcept {
    NkString out;
    out.Reserve(input.Length() + 8u);
    for (nk_size i = 0; i < input.Length(); ++i) {
        char c = input[i];
        switch (c) {
            case '\\': out.Append("\\\\"); break;
            case '"':  out.Append("\\\""); break;
            case '\n': out.Append("\\n");  break;
            case '\r': out.Append("\\r");  break;
            case '\t': out.Append("\\t");  break;
            case '\b': out.Append("\\b");  break;
            case '\f': out.Append("\\f");  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20u) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)c);
                    out.Append(buf);
                } else {
                    out.Append(c);
                }
                break;
        }
    }
    return out;
}

NkString NkJSONUnescapeString(NkStringView input, nk_bool& ok) noexcept {
    ok = true;
    NkString out;
    out.Reserve(input.Length());
    for (nk_size i = 0; i < input.Length(); ++i) {
        if (input[i] != '\\') { out.Append(input[i]); continue; }
        if (++i >= input.Length()) { ok = false; return {}; }
        switch (input[i]) {
            case '"':  out.Append('"');  break;
            case '\\': out.Append('\\'); break;
            case '/':  out.Append('/');  break;
            case 'n':  out.Append('\n'); break;
            case 'r':  out.Append('\r'); break;
            case 't':  out.Append('\t'); break;
            case 'b':  out.Append('\b'); break;
            case 'f':  out.Append('\f'); break;
            case 'u':
                if (i + 4u >= input.Length()) { ok = false; return {}; }
                i += 4u; out.Append('?'); break;
            default: ok = false; return {};
        }
    }
    return out;
}

} // namespace nkentseu
