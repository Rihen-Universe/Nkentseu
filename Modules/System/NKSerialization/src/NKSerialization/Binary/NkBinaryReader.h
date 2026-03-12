#pragma once

#ifndef NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
namespace entseu {

class NkBinaryReader {
public:
    static nk_bool ReadArchive(const nk_uint8* data,
                               nk_size size,
                               NkArchive& outArchive,
                               NkString* outError = nullptr);
};

} // namespace entseu

using NkBinaryReader = entseu::NkBinaryReader;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_BINARY_NKBINARYREADER_H_INCLUDED
