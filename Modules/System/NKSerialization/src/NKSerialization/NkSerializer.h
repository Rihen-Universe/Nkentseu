// =============================================================================
// NKSerialization/NkSerializer.h
// =============================================================================
// Point d'entrée unifié pour tous les formats de sérialisation.
// Corrections par rapport à la version originale :
//   - Variable `outError` non déclarée dans Serialize() → supprimée
//   - static_assert sur NkISerializable déplacé dans les wrappers ad-hoc
//   - NK_AUTO_DETECT : détection par magic bytes + extension
//   - Sécurité : tous les chemins retournent false proprement
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED
#define NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/NkISerializable.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/XML/NkXMLReader.h"
#include "NKSerialization/XML/NkXMLWriter.h"
#include "NKSerialization/YAML/NkYAMLReader.h"
#include "NKSerialization/YAML/NkYAMLWriter.h"
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/Native/NkNativeFormat.h"

#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace nkentseu {

    // =============================================================================
    // NkSerializationFormat
    // =============================================================================
    enum class NkSerializationFormat : nk_uint8 {
        NK_JSON       = 0,
        NK_XML        = 1,
        NK_YAML       = 2,
        NK_BINARY     = 3,
        NK_NATIVE     = 4,
        NK_AUTO_DETECT = 5,
    };

    // ─── Helpers internes ────────────────────────────────────────────────────────
    namespace detail {

        inline NkSerializationFormat DetectFormatFromExtension(const char* path) noexcept {
            if (!path) return NkSerializationFormat::NK_AUTO_DETECT;
            const char* dot = strrchr(path, '.');
            if (!dot) return NkSerializationFormat::NK_AUTO_DETECT;
            ++dot; // skip '.'
            if (strcmp(dot, "json") == 0) return NkSerializationFormat::NK_JSON;
            if (strcmp(dot, "xml")  == 0) return NkSerializationFormat::NK_XML;
            if (strcmp(dot, "yaml") == 0 || strcmp(dot, "yml") == 0) return NkSerializationFormat::NK_YAML;
            if (strcmp(dot, "bin")  == 0) return NkSerializationFormat::NK_BINARY;
            if (strcmp(dot, "nk")   == 0 || strcmp(dot, "nkasset") == 0) return NkSerializationFormat::NK_NATIVE;
            return NkSerializationFormat::NK_AUTO_DETECT;
        }

        inline NkSerializationFormat DetectFormatFromMagic(const nk_uint8* data, nk_size size) noexcept {
            if (size >= 4 && data[0] == 0x4E && data[1] == 0x4B && data[2] == 0x53 && data[3] == 0x31)
                return NkSerializationFormat::NK_NATIVE; // "NKS1"
            if (size >= 4 && data[0] == 0x4E && data[1] == 0x4B && data[2] == 0x53 && data[3] == 0x00)
                return NkSerializationFormat::NK_BINARY;
            if (size > 0 && data[0] == '{')
                return NkSerializationFormat::NK_JSON;
            if (size > 0 && data[0] == '<')
                return NkSerializationFormat::NK_XML;
            return NkSerializationFormat::NK_BINARY; // fallback
        }

        inline bool IsBinaryFormat(NkSerializationFormat fmt) noexcept {
            return fmt == NkSerializationFormat::NK_BINARY || fmt == NkSerializationFormat::NK_NATIVE;
        }

    } // namespace detail

    // =============================================================================
    // API bas niveau : Archive → format → Archive
    // =============================================================================

    // Sérialise un NkArchive vers un format texte
    inline nk_bool SerializeArchiveText(const NkArchive& archive,
                                        NkSerializationFormat format,
                                        NkString& outText,
                                        nk_bool pretty = true,
                                        NkString* outError = nullptr) noexcept {
        switch (format) {
            case NkSerializationFormat::NK_JSON:
                return NkJSONWriter::WriteArchive(archive, outText, pretty);
            case NkSerializationFormat::NK_XML:
                return NkXMLWriter::WriteArchive(archive, outText, pretty);
            case NkSerializationFormat::NK_YAML:
                return NkYAMLWriter::WriteArchive(archive, outText);
            default:
                if (outError) *outError = NkString("Use SerializeArchiveBinary() for binary formats");
                return false;
        }
    }

    // Sérialise un NkArchive vers un format binaire
    inline nk_bool SerializeArchiveBinary(const NkArchive& archive,
                                        NkSerializationFormat format,
                                        NkVector<nk_uint8>& outData,
                                        NkString* outError = nullptr) noexcept {
        switch (format) {
            case NkSerializationFormat::NK_BINARY:
                return NkBinaryWriter::WriteArchive(archive, outData);
            case NkSerializationFormat::NK_NATIVE:
            case NkSerializationFormat::NK_AUTO_DETECT:
                return native::NkNativeWriter::WriteArchive(archive, outData);
            default:
                if (outError) *outError = NkString("Use SerializeArchiveText() for text formats");
                return false;
        }
    }

    // Désérialise un format texte vers un NkArchive
    inline nk_bool DeserializeArchiveText(NkStringView text,
                                        NkSerializationFormat format,
                                        NkArchive& outArchive,
                                        NkString* outError = nullptr) noexcept {
        switch (format) {
            case NkSerializationFormat::NK_JSON:
                return NkJSONReader::ReadArchive(text, outArchive, outError);
            case NkSerializationFormat::NK_XML:
                return NkXMLReader::ReadArchive(text, outArchive, outError);
            case NkSerializationFormat::NK_YAML:
                return NkYAMLReader::ReadArchive(text, outArchive, outError);
            default:
                if (outError) *outError = NkString("Use DeserializeArchiveBinary() for binary formats");
                return false;
        }
    }

    // Désérialise un format binaire vers un NkArchive
    inline nk_bool DeserializeArchiveBinary(const nk_uint8* data, nk_size size,
                                            NkSerializationFormat format,
                                            NkArchive& outArchive,
                                            NkString* outError = nullptr) noexcept {
        if (!data || size == 0) {
            if (outError) *outError = NkString("Empty binary data");
            return false;
        }
        if (format == NkSerializationFormat::NK_AUTO_DETECT)
            format = detail::DetectFormatFromMagic(data, size);

        switch (format) {
            case NkSerializationFormat::NK_BINARY:
                return NkBinaryReader::ReadArchive(data, size, outArchive, outError);
            case NkSerializationFormat::NK_NATIVE:
                return native::NkNativeReader::ReadArchive(data, size, outArchive, outError);
            default:
                if (outError) *outError = NkString("Use DeserializeArchiveText() for text formats");
                return false;
        }
    }

    // =============================================================================
    // API haut niveau : T (NkISerializable) → format
    // =============================================================================

    template<typename T>
    nk_bool Serialize(const T& obj,
                    NkSerializationFormat format,
                    NkString& outText,
                    nk_bool pretty = true,
                    NkString* outError = nullptr) noexcept {
        static_assert(std::is_base_of<NkISerializable, T>::value,
                    "T must inherit from NkISerializable");
        NkArchive archive;
        if (!obj.Serialize(archive)) {
            if (outError) *outError = NkString("Serialize() failed on object");
            return false;
        }
        return SerializeArchiveText(archive, format, outText, pretty, outError);
    }

    template<typename T>
    nk_bool SerializeBinary(const T& obj,
                            NkSerializationFormat format,
                            NkVector<nk_uint8>& outData,
                            NkString* outError = nullptr) noexcept {
        static_assert(std::is_base_of<NkISerializable, T>::value,
                    "T must inherit from NkISerializable");
        NkArchive archive;
        if (!obj.Serialize(archive)) {
            if (outError) *outError = NkString("Serialize() failed on object");
            return false;
        }
        return SerializeArchiveBinary(archive, format, outData, outError);
    }

    template<typename T>
    nk_bool Deserialize(NkStringView text,
                        NkSerializationFormat format,
                        T& obj,
                        NkString* outError = nullptr) noexcept {
        static_assert(std::is_base_of<NkISerializable, T>::value,
                    "T must inherit from NkISerializable");
        NkArchive archive;
        if (!DeserializeArchiveText(text, format, archive, outError)) return false;
        return obj.Deserialize(archive);
    }

    template<typename T>
    nk_bool DeserializeBinary(const nk_uint8* data, nk_size size,
                            NkSerializationFormat format,
                            T& obj,
                            NkString* outError = nullptr) noexcept {
        static_assert(std::is_base_of<NkISerializable, T>::value,
                    "T must inherit from NkISerializable");
        NkArchive archive;
        if (!DeserializeArchiveBinary(data, size, format, archive, outError)) return false;
        return obj.Deserialize(archive);
    }

    // =============================================================================
    // API fichier : SaveToFile / LoadFromFile
    // =============================================================================

    template<typename T>
    nk_bool SaveToFile(const T& obj,
                    const char* path,
                    NkSerializationFormat format = NkSerializationFormat::NK_AUTO_DETECT,
                    nk_bool pretty = true,
                    NkString* outError = nullptr) noexcept {
        static_assert(std::is_base_of<NkISerializable, T>::value,
                    "T must inherit from NkISerializable");

        if (!path) { if (outError) *outError = NkString("Null path"); return false; }

        if (format == NkSerializationFormat::NK_AUTO_DETECT)
            format = detail::DetectFormatFromExtension(path);
        if (format == NkSerializationFormat::NK_AUTO_DETECT)
            format = NkSerializationFormat::NK_NATIVE; // default

        NkArchive archive;
        if (!obj.Serialize(archive)) {
            if (outError) *outError = NkString("Serialize() failed");
            return false;
        }

        if (detail::IsBinaryFormat(format)) {
            NkVector<nk_uint8> data;
            if (!SerializeArchiveBinary(archive, format, data, outError)) return false;

            NkFile file;
            if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                if (outError) *outError = NkString("Cannot open file for writing");
                return false;
            }

            if (file.Write(data.Data(), data.Size()) != data.Size()) {
                if (outError) *outError = NkString("Write error");
                file.Close();
                return false;
            }
            file.Close();
            return true;
        } else {
            NkString text;
            if (!SerializeArchiveText(archive, format, text, pretty, outError)) return false;

            NkFile file;

            if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
                if (outError) *outError = NkString("Cannot open file for writing");
                return false;
            }

            if (file.Write(text.CStr(), text.Length()) != text.Length()) {
                if (outError) *outError = NkString("Write error");
                file.Close();
                return false;
            }
            file.Close();
            return true;
        }
    }

    template<typename T>
    nk_bool LoadFromFile(const char* path, T& obj,
                        NkSerializationFormat format = NkSerializationFormat::NK_AUTO_DETECT, NkString* outError = nullptr) noexcept {
        static_assert(traits::NkIsBaseOf<NkISerializable, T>::value, "T must inherit from NkISerializable");

        if (!path) { if (outError) *outError = NkString("Null path"); return false; }

        if (format == NkSerializationFormat::NK_AUTO_DETECT)
            format = detail::DetectFormatFromExtension(path);

        NkFile file;

        if (!file.Open(path, NkFileMode::NK_READ)) {
            if (outError) *outError = NkString("Cannot open file for reading");
            return false;
        }

        nk_size size = file.Size();

        if (size == 0) { 
            fclose(f);
            file.Close();
            if (outError) *outError = NkString("Empty file"); 
            return false; 
        }

        NkArchive archive;

        if (format == NkSerializationFormat::NK_AUTO_DETECT || detail::IsBinaryFormat(format)) {
            // Lecture binaire → détection par magic si AUTO_DETECT
            NkVector<nk_uint8> data(size);
            if (file.Read(data.Data(), size) != size) {
                file.Close();
                if (outError) *outError = NkString("Read error");
                return false;
            }
            file.Close();

            if (format == NkSerializationFormat::NK_AUTO_DETECT)
                format = detail::DetectFormatFromMagic(data.Data(), size);

            if (detail::IsBinaryFormat(format)) {
                if (!DeserializeArchiveBinary(data.Data(), size, format, archive, outError))
                    return false;
            } else {
                // Magic bytes détectés comme texte (JSON commençant par '{' etc.)
                NkString text(reinterpret_cast<const char*>(data.Data()), size);
                if (!DeserializeArchiveText(text.View(), format, archive, outError)) return false;
            }
        } else {
            NkString text;
            text.Resize(size);
            if (file.Read(text.Data(), size) != size) {
                file.Close();
                if (outError) *outError = NkString("Read error"); 
                return false;
            }
            file.Close();
            if (!DeserializeArchiveText(text.View(), format, archive, outError)) return false;
        }

        return obj.Deserialize(archive);
    }

    // =============================================================================
    // Helper : instanciation polymorphique depuis archive
    // =============================================================================
    inline std::unique_ptr<NkISerializable>
    CreateFromArchive(const NkArchive& archive) noexcept {
        NkString typeName;
        if (!archive.GetString("__type__", typeName)) return nullptr;
        auto obj = NkSerializableRegistry::Global().Create(typeName.CStr());
        if (obj && !obj->Deserialize(archive)) return nullptr;
        return obj;
    }

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZER_H_INCLUDED
