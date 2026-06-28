#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiFont.h
// @Brief   Police NKGui — wrapper mince sur NKFont (atlas + glyphes). Phase 3.
// @License Proprietary - Free to use and modify
//
// Charge une police (embarquée ou fichier), construit l'atlas, expose la face
// NKFont + l'atlas alpha8 à uploader par le backend (texId stable).
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"

namespace nkentseu {
    namespace nkgui {

        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiFont {
            NkFontAtlas atlas;                 ///< possède la texture + les glyphes
            NkFont*     face   = nullptr;      ///< face produite (détenue par l'atlas)
            uint32      texId  = 0x4E4B4654u;  ///< 'NKFT' — id stable pour le backend
            uint8*      pixels = nullptr;      ///< atlas alpha8 (détenu par l'atlas)
            int32       atlasW = 0;
            int32       atlasH = 0;
            bool        dirty  = false;        ///< à (ré)uploader côté backend

            NkGuiFont() = default;
            NkGuiFont(const NkGuiFont&)            = delete;
            NkGuiFont& operator=(const NkGuiFont&) = delete;

            // Charge une police embarquée à la taille donnée + construit l'atlas.
            bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx) noexcept;

            // Charge une police depuis un fichier TTF/OTF (ex. police système Windows).
            // Rechargeable : réinitialise l'atlas avant. Conserve `texId` (ré-upload).
            bool LoadFromFile(const char* path, float32 sizePx) noexcept;

            const NkFont* Face()  const noexcept { return face; }
            uint32        TexId() const noexcept { return texId; }
            bool          Valid() const noexcept { return face != nullptr; }

            float32 Ascent()     const noexcept { return face ? face->ascent      : 0.f; }
            float32 Descent()    const noexcept { return face ? face->descent     : 0.f; }
            float32 LineHeight() const noexcept { return face ? face->lineAdvance : 0.f; }
            float32 MeasureWidth(const char* s) const noexcept {
                return (face && s) ? face->CalcTextSizeX(s) : 0.f;
            }
        };

        // Polices de REPLI EXTERNES (fichiers .ttf charges au runtime) : tout glyphe
        // absent des polices principales (Inter/DejaVu) y est cherche. Roles :
        //   broad : large couverture (latin etendu, grec, cyrillique, hebreu, arabe, symboles)
        //   cjk   : ideogrammes (中文/日本語/한국어) — volumineux, opt-in
        //   emoji : emoji monochrome
        // A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
        // de charger les polices. Un chemin nullptr/vide = role desactive.
        NKENTSEU_NKGUI_API void NkSetFallbackFontPaths(const char* broad, const char* cjk, const char* emoji) noexcept;

    } // namespace nkgui
} // namespace nkentseu
