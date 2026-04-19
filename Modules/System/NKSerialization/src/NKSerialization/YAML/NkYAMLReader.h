#pragma once

#ifndef NKENTSEU_SERIALIZATION_YAML_NKYAMLREADER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_YAML_NKYAMLREADER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
    class NkYAMLReader {
        public:
            static nk_bool ReadArchive(NkStringView yaml, NkArchive& outArchive, NkString* outError = nullptr);
    };
} // namespace nkentseu

#endif
