// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFile.cpp
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation de NkFile.
//   Utilise l'API C standard de fichier (fopen/fread/fwrite/fseek).
// =============================================================================

#include "NKFileSystem/NkFile.h"

// -- En-tetes C standard ------------------------------------------------------
#include <cstdio>
#include <cstring>

// -- En-tetes plateforme ------------------------------------------------------
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/stat.h>
#endif

namespace nkentseu {

    // =========================================================================
    // Prive
    // =========================================================================

    const char* NkFile::GetModeString() const {
        const bool read     = NkHasFlag(mMode, NkFileMode::NK_READ);
        const bool write    = NkHasFlag(mMode, NkFileMode::NK_WRITE);
        const bool append   = NkHasFlag(mMode, NkFileMode::NK_APPEND);
        const bool truncate = NkHasFlag(mMode, NkFileMode::NK_TRUNCATE);
        const bool binary   = NkHasFlag(mMode, NkFileMode::NK_BINARY);

        // 🔴 Validation stricte (évite bugs subtils)
        if (append && truncate)
            return ""; // invalide

        // Cas principaux (logique fopen)
        if (read && !write && !append) {
            if (binary) return "rb";
            return "r";
        }
        else if (write && !read && !append) {
            if (binary) return "wb";
            return "w";
        }
        else if (append && !read && !write) {
            if (binary) return "ab";
            return "a";
        }
        else if (read && write && !append && !truncate) {
            if (binary) return "rb+";
            return "r+";
        }
        else if (read && write && truncate) {
            if (binary) return "wb+";
            return "w+";
        }
        else if (read && append) {
            if (binary) return "ab+";
            return "a+";
        }
            
        return ""; // combinaison non supportée
    }

    // =========================================================================
    // Constructeurs
    // =========================================================================

    NkFile::NkFile()
        : mHandle(nullptr)
        , mPath()
        , mMode(NkFileMode::NK_READ)
        , mIsOpen(false) {
    }

    NkFile::NkFile(const char* path, NkFileMode mode)
        : mHandle(nullptr)
        , mPath(path ? path : "")
        , mMode(mode)
        , mIsOpen(false) {
        Open(path, mode);
    }

    NkFile::NkFile(const NkPath& path, NkFileMode mode)
        : mHandle(nullptr)
        , mPath(path)
        , mMode(mode)
        , mIsOpen(false) {
        Open(path, mode);
    }

    NkFile::~NkFile() {
        Close();
    }

    NkFile::NkFile(NkFile&& other) noexcept
        : mHandle(other.mHandle)
        , mPath(other.mPath)
        , mMode(other.mMode)
        , mIsOpen(other.mIsOpen) {
        other.mHandle = nullptr;
        other.mIsOpen = false;
    }

    NkFile& NkFile::operator=(NkFile&& other) noexcept {
        if (this != &other) {
            Close();
            mHandle = other.mHandle;
            mPath = other.mPath;
            mMode = other.mMode;
            mIsOpen = other.mIsOpen;
            other.mHandle = nullptr;
            other.mIsOpen = false;
        }
        return *this;
    }

    // =========================================================================
    // Ouverture / fermeture
    // =========================================================================

    bool NkFile::Open(const char* path, NkFileMode mode) {
        Close();

        if (!path) {
            mPath = "";
            mMode = mode;
            return false;
        }

        mPath = path;
        mMode = mode;

        FILE* file = fopen(path, GetModeString());
        if (!file) {
            return false;
        }

        mHandle = file;
        mIsOpen = true;
        return true;
    }

    bool NkFile::Open(const NkPath& path, NkFileMode mode) {
        return Open(path.CStr(), mode);
    }

    void NkFile::Close() {
        if (mIsOpen && mHandle) {
            fclose(static_cast<FILE*>(mHandle));
            mHandle = nullptr;
            mIsOpen = false;
        }
    }

    bool NkFile::IsOpen() const {
        return mIsOpen;
    }

    // =========================================================================
    // Lecture
    // =========================================================================

    usize NkFile::Read(void* buffer, usize size) {
        if (!mIsOpen || !buffer) {
            return 0;
        }

        return fread(buffer, 1, size, static_cast<FILE*>(mHandle));
    }

    NkString NkFile::ReadLine() {
        if (!mIsOpen) {
            return NkString();
        }

        char buffer[4096];
        if (!fgets(buffer, sizeof(buffer), static_cast<FILE*>(mHandle))) {
            return NkString();
        }

        usize len = strlen(buffer);

        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';

            if (len > 1 && buffer[len - 2] == '\r') {
                buffer[len - 2] = '\0';
            }
        }

        return NkString(buffer);
    }

    NkString NkFile::ReadAll() {
        if (!mIsOpen) {
            return NkString();
        }

        const nk_int64 size = GetSize();
        if (size <= 0) {
            return NkString();
        }

        NkVector<char> buffer;
        buffer.Resize(static_cast<usize>(size) + 1);

        const usize read = Read(buffer.Data(), static_cast<usize>(size));
        buffer[read] = '\0';
        return NkString(buffer.Data());
    }

    NkVector<NkString> NkFile::ReadLines() {
        NkVector<NkString> lines;

        while (!IsEOF()) {
            NkString line = ReadLine();
            if (!line.Empty() || !IsEOF()) {
                lines.PushBack(line);
            }
        }

        return lines;
    }

    // =========================================================================
    // Ecriture
    // =========================================================================

    usize NkFile::Write(const void* data, usize size) {
        if (!mIsOpen || !data) {
            return 0;
        }

        return fwrite(data, 1, size, static_cast<FILE*>(mHandle));
    }

    bool NkFile::WriteLine(const char* text) {
        if (!mIsOpen || !text) {
            return false;
        }

        const usize len = strlen(text);
        if (Write(text, len) != len) {
            return false;
        }

        const char newline[] = "\n";
        if (Write(newline, 1) != 1) {
            return false;
        }

        return true;
    }

    bool NkFile::Write(const NkString& text) {
        return Write(text.CStr(), text.Length()) == text.Length();
    }

    // =========================================================================
    // Position
    // =========================================================================

    nk_int64 NkFile::Tell() const {
        if (!mIsOpen) {
            return -1;
        }

        return static_cast<nk_int64>(ftell(static_cast<FILE*>(mHandle)));
    }

    bool NkFile::Seek(nk_int64 offset, NkSeekOrigin origin) {
        if (!mIsOpen) {
            return false;
        }

        int whence = SEEK_SET;

        switch (origin) {
            case NkSeekOrigin::NK_BEGIN:
                whence = SEEK_SET;
                break;
            case NkSeekOrigin::NK_CURRENT:
                whence = SEEK_CUR;
                break;
            case NkSeekOrigin::NK_END:
                whence = SEEK_END;
                break;
            default:
                whence = SEEK_SET;
                break;
        }

        return fseek(static_cast<FILE*>(mHandle), static_cast<long>(offset), whence) == 0;
    }

    bool NkFile::SeekToBegin() {
        return Seek(0, NkSeekOrigin::NK_BEGIN);
    }

    bool NkFile::SeekToEnd() {
        return Seek(0, NkSeekOrigin::NK_END);
    }

    nk_int64 NkFile::GetSize() const {
        if (!mIsOpen) {
            return -1;
        }

        const nk_int64 current = Tell();
        fseek(static_cast<FILE*>(mHandle), 0, SEEK_END);
        const nk_int64 size = static_cast<nk_int64>(ftell(static_cast<FILE*>(mHandle)));
        fseek(static_cast<FILE*>(mHandle), static_cast<long>(current), SEEK_SET);
        return size;
    }

    // =========================================================================
    // Divers
    // =========================================================================

    void NkFile::Flush() {
        if (mIsOpen) {
            fflush(static_cast<FILE*>(mHandle));
        }
    }

    const NkPath& NkFile::GetPath() const {
        return mPath;
    }

    NkFileMode NkFile::GetMode() const {
        return mMode;
    }

    bool NkFile::IsEOF() const {
        if (!mIsOpen) {
            return true;
        }

        return feof(static_cast<FILE*>(mHandle)) != 0;
    }

    // =========================================================================
    // Utilitaires statiques
    // =========================================================================

    bool NkFile::Exists(const char* path) {
        if (!path) {
            return false;
        }

#ifdef _WIN32
        const DWORD attrs = GetFileAttributesA(path);
        return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
#endif
    }

    bool NkFile::Exists(const NkPath& path) {
        return Exists(path.CStr());
    }

    bool NkFile::Delete(const char* path) {
        if (!path) {
            return false;
        }

        return remove(path) == 0;
    }

    bool NkFile::Delete(const NkPath& path) {
        return Delete(path.CStr());
    }

    bool NkFile::Copy(const char* source, const char* dest, bool overwrite) {
        if (!source || !dest) {
            return false;
        }

        if (!overwrite && Exists(dest)) {
            return false;
        }

        NkFile src(source, NkFileMode::NK_READ_BINARY);
        if (!src.IsOpen()) {
            return false;
        }

        NkFile dst(dest, NkFileMode::NK_WRITE_BINARY);
        if (!dst.IsOpen()) {
            return false;
        }

        char buffer[8192];
        usize read = 0;

        while ((read = src.Read(buffer, sizeof(buffer))) > 0) {
            if (dst.Write(buffer, read) != read) {
                return false;
            }
        }

        return true;
    }

    bool NkFile::Copy(const NkPath& source, const NkPath& dest, bool overwrite) {
        return Copy(source.CStr(), dest.CStr(), overwrite);
    }

    bool NkFile::Move(const char* source, const char* dest) {
        if (!source || !dest) {
            return false;
        }

        return rename(source, dest) == 0;
    }

    bool NkFile::Move(const NkPath& source, const NkPath& dest) {
        return Move(source.CStr(), dest.CStr());
    }

    nk_int64 NkFile::GetFileSize(const char* path) {
        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            return -1;
        }

        return file.GetSize();
    }

    nk_int64 NkFile::GetFileSize(const NkPath& path) {
        return GetFileSize(path.CStr());
    }

    NkString NkFile::ReadAllText(const char* path) {
        NkFile file(path, NkFileMode::NK_READ);
        if (!file.IsOpen()) {
            return NkString();
        }

        return file.ReadAll();
    }

    NkString NkFile::ReadAllText(const NkPath& path) {
        return ReadAllText(path.CStr());
    }

    NkVector<nk_uint8> NkFile::ReadAllBytes(const char* path) {
        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            return NkVector<nk_uint8>();
        }

        const nk_int64 size = file.GetSize();
        if (size <= 0) {
            return NkVector<nk_uint8>();
        }

        NkVector<nk_uint8> data;
        data.Resize(static_cast<usize>(size));
        file.Read(data.Data(), static_cast<usize>(size));
        return data;
    }

    NkVector<nk_uint8> NkFile::ReadAllBytes(const NkPath& path) {
        return ReadAllBytes(path.CStr());
    }

    bool NkFile::WriteAllText(const char* path, const char* text) {
        if (!path || !text) {
            return false;
        }

        NkFile file(path, NkFileMode::NK_WRITE);
        if (!file.IsOpen()) {
            return false;
        }

        const usize len = strlen(text);
        return file.Write(text, len) == len;
    }

    bool NkFile::WriteAllText(const NkPath& path, const NkString& text) {
        return WriteAllText(path.CStr(), text.CStr());
    }

    bool NkFile::WriteAllBytes(const char* path, const NkVector<nk_uint8>& data) {
        if (!path) {
            return false;
        }

        NkFile file(path, NkFileMode::NK_WRITE_BINARY);
        if (!file.IsOpen()) {
            return false;
        }

        return file.Write(data.Data(), data.Size()) == data.Size();
    }

    bool NkFile::WriteAllBytes(const NkPath& path, const NkVector<nk_uint8>& data) {
        return WriteAllBytes(path.CStr(), data);
    }

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
