#pragma once
#ifndef NKENTSEU_SERIALIZATION_BINARY_NKBINARYWRITER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_BINARY_NKBINARYWRITER_H_INCLUDED
#include "NKSerialization/NkArchive.h"
namespace nkentseu {
class NkBinaryWriter {
public:
    static nk_bool WriteArchive(const NkArchive& archive, NkVector<nk_uint8>& outData);
};
} // namespace nkentseu
#endif
