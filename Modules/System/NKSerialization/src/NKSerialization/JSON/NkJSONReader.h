#pragma once

#ifndef NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
namespace entseu {

class NkJSONReader {
public:
    static nk_bool ReadArchive(NkStringView json,
                               NkArchive& outArchive,
                               NkString* outError = nullptr);
};

} // namespace entseu

using NkJSONReader = entseu::NkJSONReader;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_JSON_NKJSONREADER_H_INCLUDED
