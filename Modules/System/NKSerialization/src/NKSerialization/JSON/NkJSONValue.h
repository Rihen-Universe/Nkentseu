#pragma once
#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H_INCLUDED
#define NKENTSEU_SERIALIZATION_JSON_NKJSONVALUE_H_INCLUDED
#include "NKContainers/String/NkString.h"
namespace nkentseu {
NkString NkJSONEscapeString(NkStringView input) noexcept;
NkString NkJSONUnescapeString(NkStringView input, nk_bool& ok) noexcept;
} // namespace nkentseu
#endif
