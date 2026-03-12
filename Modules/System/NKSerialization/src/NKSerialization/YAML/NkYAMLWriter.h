#pragma once

#ifndef NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H_INCLUDED

#include "NKSerialization/NkArchive.h"

namespace nkentseu {
namespace entseu {

class NkYAMLWriter {
public:
    static nk_bool WriteArchive(const NkArchive& archive, NkString& outYaml);
    static NkString WriteArchive(const NkArchive& archive);
};

} // namespace entseu

using NkYAMLWriter = entseu::NkYAMLWriter;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_YAML_NKYAMLWRITER_H_INCLUDED
