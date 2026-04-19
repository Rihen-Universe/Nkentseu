#pragma once
#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H_INCLUDED
#include "NKSerialization/NkArchive.h"
namespace nkentseu {
class NkJSONReader {
public:
    static nk_bool ReadArchive(NkStringView json, NkArchive& outArchive,
                               NkString* outError = nullptr);
};
} // namespace nkentseu
#endif
