#pragma once

#ifndef NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
    class NkYAMLWriter {
        public:
            static nk_bool WriteArchive(const NkArchive& archive, NkString& outYaml);
            static NkString WriteArchive(const NkArchive& archive);
    };
} // namespace nkentseu

#endif
