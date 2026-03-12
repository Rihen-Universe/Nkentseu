#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/XML/NkXMLReader.h"
#include "NKSerialization/XML/NkXMLWriter.h"
#include "NKSerialization/YAML/NkYAMLReader.h"
#include "NKSerialization/YAML/NkYAMLWriter.h"
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"

namespace nkentseu {
namespace entseu {

enum class NkSerializationFormat : nk_uint8 {
    NK_JSON = 0,
    NK_XML,
    NK_YAML
};

inline nk_bool NkSerializeArchive(const NkArchive& archive,
                                  NkSerializationFormat format,
                                  NkString& outText,
                                  nk_bool pretty = true) {
    switch (format) {
        case NkSerializationFormat::NK_JSON:
            return NkJSONWriter::WriteArchive(archive, outText, pretty);
        case NkSerializationFormat::NK_XML:
            return NkXMLWriter::WriteArchive(archive, outText, pretty);
        case NkSerializationFormat::NK_YAML:
            return NkYAMLWriter::WriteArchive(archive, outText);
        default:
            return false;
    }
}

inline nk_bool NkDeserializeArchive(NkStringView text,
                                    NkSerializationFormat format,
                                    NkArchive& outArchive,
                                    NkString* outError = nullptr) {
    switch (format) {
        case NkSerializationFormat::NK_JSON:
            return NkJSONReader::ReadArchive(text, outArchive, outError);
        case NkSerializationFormat::NK_XML:
            return NkXMLReader::ReadArchive(text, outArchive, outError);
        case NkSerializationFormat::NK_YAML:
            return NkYAMLReader::ReadArchive(text, outArchive, outError);
        default:
            if (outError) {
                *outError = "Unsupported serialization format";
            }
            return false;
    }
}

inline nk_bool NkSerializeArchiveBinary(const NkArchive& archive, NkVector<nk_uint8>& outData) {
    return NkBinaryWriter::WriteArchive(archive, outData);
}

inline nk_bool NkDeserializeArchiveBinary(const nk_uint8* data,
                                          nk_size size,
                                          NkArchive& outArchive,
                                          NkString* outError = nullptr) {
    return NkBinaryReader::ReadArchive(data, size, outArchive, outError);
}

} // namespace entseu

using NkSerializationFormat = entseu::NkSerializationFormat;
using entseu::NkDeserializeArchive;
using entseu::NkDeserializeArchiveBinary;
using entseu::NkSerializeArchive;
using entseu::NkSerializeArchiveBinary;

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED
