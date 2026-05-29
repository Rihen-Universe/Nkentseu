#pragma once
// =============================================================================
// NkFileStream.h
// -----------------------------------------------------------------------------
// Flux fichier multi-plateforme. Implementation desormais bati sur NkFile
// (NKFileSystem) au lieu d'appeler directement Win32/POSIX. Avantage :
//
//   - Heritage automatique du fallback AAssetManager Android : un .ogg dans
//     `assets/` de l'APK est ouvert via AAsset si fopen echoue.
//   - Path handling unifie via NkPath.
//   - Une seule source de verite pour les operations fichier.
//
// L'API publique reste inchangee (NkStream::Open / Read / Write / Seek / ...).
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKCore/NkTypes.h"
#include "NKFileSystem/NkFile.h"
#include "NKStream/NkStream.h"

#include <cstddef>

namespace nkentseu {

    ///////////////////////////////////////////////////////////////////////////////
    //  NkFileStream : flux fichier construit au-dessus de NkFile
    ///////////////////////////////////////////////////////////////////////////////

    class NKSTREAM_API NkFileStream : public NkStream {
        public:
            NkFileStream() = default;
            ~NkFileStream() override { Close(); }

            // ── Mapping NkStream mode -> NkFileMode ──────────────────────────
            // NK_READ_MODE   (0x01) -> NK_READ
            // NK_WRITE_MODE  (0x02) -> NK_WRITE  (+ NK_TRUNCATE par defaut sauf append)
            // NK_APPEND_MODE        -> NK_APPEND
            // On force NK_BINARY pour usage stream (pas de conversion newline).
            // ─────────────────────────────────────────────────────────────────
            bool Open(const char* path, uint32 mode) override {
                mOpenMode = mode;

                NkFileMode fmode = NkFileMode::NK_BINARY;
                const bool wantRead   = (mode & NK_READ_MODE)   != 0;
                const bool wantWrite  = (mode & NK_WRITE_MODE)  != 0;
                const bool wantAppend = (mode & NK_APPEND_MODE) != 0;

                if (wantRead && wantWrite) {
                    fmode = wantAppend
                        ? (NkFileMode::NK_READ | NkFileMode::NK_APPEND | NkFileMode::NK_BINARY)
                        : NkFileMode::NK_READ_WRITE_BINARY;
                } else if (wantRead) {
                    fmode = NkFileMode::NK_READ_BINARY;
                } else if (wantWrite) {
                    fmode = wantAppend ? NkFileMode::NK_APPEND_BINARY
                                       : NkFileMode::NK_WRITE_BINARY;
                }

                return mFile.Open(path, fmode);
            }

            void Close() override {
                if (mFile.IsOpen()) mFile.Close();
            }

            bool IsOpen() const override {
                return mFile.IsOpen();
            }

            // ── Lecture / ecriture raw octet ─────────────────────────────────
            usize ReadRaw(void* buffer, usize byteCount) override {
                return mFile.Read(buffer, byteCount);
            }

            usize WriteRaw(const void* data, usize byteCount) override {
                return mFile.Write(data, byteCount);
            }

            // ── Positionnement ───────────────────────────────────────────────
            bool Seek(usize position) override {
                return mFile.Seek(static_cast<nk_int64>(position), NkSeekOrigin::NK_BEGIN);
            }

            usize Tell() const override {
                return static_cast<usize>(mFile.Tell());
            }

            usize Size() const override {
                return static_cast<usize>(mFile.GetSize());
            }

            bool IsEOF() const override {
                return mFile.IsOpen() ? Tell() >= Size() : true;
            }

            // ── Flush (sur ecriture seulement) ───────────────────────────────
            void Flush() {
                // NkFile gere son propre flush en interne ; l'appel ici est un
                // no-op silencieux. Si necessaire, on pourra ajouter une methode
                // Flush() explicite a NkFile.
            }

            // ── Encodage : reste a la charge de NkFileStream ─────────────────
            // NkFile ne gere pas l'encodage ; on garde la gestion BOM ici pour
            // ne pas casser l'API existante.
            bool SetEncoding(Encoding encoding) override {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                constexpr uint16 UTF16_BOM = 0xFEFF;
                constexpr uint8 UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

                switch (encoding) {
                    case Encoding::NK_UTF16_LE:
                        if (IsWriteMode()) WriteRaw(&UTF16_BOM, sizeof(UTF16_BOM));
                        mEncoding = encoding;
                        return true;
                    case Encoding::NK_UTF8:
                        if (IsWriteMode()) WriteRaw(UTF8_BOM, sizeof(UTF8_BOM));
                        mEncoding = encoding;
                        return true;
                    default:
                        mEncoding = Encoding::NK_SYSTEM_DEFAULT;
                        return false;
                }
            #else
                mEncoding = (encoding == Encoding::NK_SYSTEM_DEFAULT)
                            ? Encoding::NK_UTF8 : encoding;
                return true;
            #endif
            }

            Encoding GetEncoding() const override {
                return mEncoding;
            }

            // ── Accesseur direct au NkFile sous-jacent (interop optionnelle) ─
            NkFile& GetFile()             noexcept { return mFile; }
            const NkFile& GetFile() const noexcept { return mFile; }

        private:
            bool IsWriteMode() const {
                return (mOpenMode & (NK_WRITE_MODE | NK_APPEND_MODE)) != 0;
            }

            NkFile   mFile;                                       ///< Backing file (gere AAsset Android)
            Encoding mEncoding  = Encoding::NK_SYSTEM_DEFAULT;
            uint32   mOpenMode  = 0;
    };

} // namespace nkentseu
