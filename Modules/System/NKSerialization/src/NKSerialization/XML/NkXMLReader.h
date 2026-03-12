#pragma once

#ifndef NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
namespace entseu {

class NkXMLReader {
public:
    static nk_bool ReadArchive(NkStringView xml,
                               NkArchive& outArchive,
                               NkString* outError = nullptr);
};

} // namespace entseu

using NkXMLReader = entseu::NkXMLReader;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H_INCLUDED
