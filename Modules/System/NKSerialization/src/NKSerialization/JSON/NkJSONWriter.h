#pragma once
#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONWRITER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_JSON_NKJSONWRITER_H_INCLUDED
#include "NKSerialization/NkArchive.h"
namespace nkentseu {
class NkJSONWriter {
public:
    static nk_bool WriteArchive(const NkArchive& archive, NkString& outJson,
                                nk_bool pretty = true, nk_int32 indentSpaces = 2);
    static NkString WriteArchive(const NkArchive& archive, nk_bool pretty = true,
                                 nk_int32 indentSpaces = 2);
};
} // namespace nkentseu
#endif
