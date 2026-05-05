// =============================================================================
// cli/tools/nkecs-convert/main.cpp
// =============================================================================
// Outil de conversion entre formats de sérialisation.
// Usage: nkecs-convert <input> <output>
//   Formats détectés automatiquement depuis l'extension.
// =============================================================================
#include "NKSerialization/NkAllHeaders.h"
#include <cstdio>
#include <cstring>

using namespace nkentseu;

static NkVector<nk_uint8> ReadFileBinary(const char* path, bool& ok) {
    ok = false;
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    nk_size sz = static_cast<nk_size>(ftell(f));
    fseek(f, 0, SEEK_SET);
    NkVector<nk_uint8> buf(sz);
    ok = (fread(buf.Data(), 1u, sz, f) == sz);
    fclose(f);
    return buf;
}

static NkString ReadFileText(const char* path, bool& ok) {
    ok = false;
    FILE* f = fopen(path, "r");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    nk_size sz = static_cast<nk_size>(ftell(f));
    fseek(f, 0, SEEK_SET);
    NkString s; s.Resize(sz);
    ok = (fread(s.Data(), 1u, sz, f) == sz);
    fclose(f);
    return s;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: nkecs-convert <input> <output>\n");
        fprintf(stderr, "Supported extensions: .json .xml .yaml .yml .bin .nk .nkasset\n");
        return 1;
    }

    const char* inPath  = argv[1];
    const char* outPath = argv[2];

    // Détecter les formats
    NkSerializationFormat inFmt  = detail::DetectFormatFromExtension(inPath);
    NkSerializationFormat outFmt = detail::DetectFormatFromExtension(outPath);

    if (inFmt == NkSerializationFormat::NK_AUTO_DETECT) {
        fprintf(stderr, "Unknown input format for: %s\n", inPath);
        return 1;
    }
    if (outFmt == NkSerializationFormat::NK_AUTO_DETECT) {
        fprintf(stderr, "Unknown output format for: %s\n", outPath);
        return 1;
    }

    // Charger l'archive d'entrée
    NkArchive archive;
    NkString err;

    const bool inBinary  = detail::IsBinaryFormat(inFmt);
    const bool outBinary = detail::IsBinaryFormat(outFmt);

    if (inBinary) {
        bool ok = false;
        NkVector<nk_uint8> data = ReadFileBinary(inPath, ok);
        if (!ok) { fprintf(stderr, "Cannot read: %s\n", inPath); return 1; }
        if (!DeserializeArchiveBinary(data.Data(), data.Size(), inFmt, archive, &err)) {
            fprintf(stderr, "Parse error: %s\n", err.CStr());
            return 1;
        }
    } else {
        bool ok = false;
        NkString text = ReadFileText(inPath, ok);
        if (!ok) { fprintf(stderr, "Cannot read: %s\n", inPath); return 1; }
        if (!DeserializeArchiveText(text.View(), inFmt, archive, &err)) {
            fprintf(stderr, "Parse error: %s\n", err.CStr());
            return 1;
        }
    }

    // Écrire la sortie
    FILE* out = fopen(outPath, outBinary ? "wb" : "w");
    if (!out) { fprintf(stderr, "Cannot create: %s\n", outPath); return 1; }

    bool writeOk = false;
    if (outBinary) {
        NkVector<nk_uint8> data;
        if (!SerializeArchiveBinary(archive, outFmt, data, &err)) {
            fclose(out);
            fprintf(stderr, "Encode error: %s\n", err.CStr());
            return 1;
        }
        writeOk = (fwrite(data.Data(), 1u, data.Size(), out) == data.Size());
    } else {
        NkString text;
        if (!SerializeArchiveText(archive, outFmt, text, true, &err)) {
            fclose(out);
            fprintf(stderr, "Encode error: %s\n", err.CStr());
            return 1;
        }
        writeOk = (fwrite(text.CStr(), 1u, text.Length(), out) == text.Length());
    }
    fclose(out);

    if (!writeOk) { fprintf(stderr, "Write error\n"); return 1; }

    printf("Converted: %s → %s\n", inPath, outPath);
    printf("Entries: %zu\n", archive.Size());
    return 0;
}
