#pragma once

#ifndef NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_XML_NKXMLREADER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
    class NkXMLReader {
        public:
            static nk_bool ReadArchive(NkStringView xml, NkArchive& outArchive, NkString* outError = nullptr);
    };
} // namespace nkentseu
#endif
