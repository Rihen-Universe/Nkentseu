#pragma once

#ifndef NKENTSEU_SERIALIZATION_XML_NKXMLWRITER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_XML_NKXMLWRITER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
    class NkXMLWriter {
        public:
            static nk_bool WriteArchive(const NkArchive& archive, NkString& outXml, nk_bool pretty = true, nk_int32 indentSpaces = 2);
            static NkString WriteArchive(const NkArchive& archive, nk_bool pretty = true, nk_int32 indentSpaces = 2);
    };
} // namespace nkentseu
#endif
