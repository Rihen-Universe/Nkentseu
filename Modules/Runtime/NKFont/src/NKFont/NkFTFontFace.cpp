/**
 * @File    NkFTFontFace.cpp
 * @Brief   Implémentation du backend FreeType2 pour NKFont.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "NKFont/NkFTFontFace.h"
#include "NKFont/NkFontFace.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FreeType2 — inclus uniquement dans cette unité de traduction
// ─────────────────────────────────────────────────────────────────────────────

#include "ft2build.h"
#include FT_FREETYPE_H

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de cast (garde les types FT hors du header public)
// ─────────────────────────────────────────────────────────────────────────────

static inline FT_Library AsLib(void* p)  noexcept { return static_cast<FT_Library>(p); }
static inline FT_Face    AsFace(void* p) noexcept { return static_cast<FT_Face>(p);    }

// ─────────────────────────────────────────────────────────────────────────────
//  NkFTFontFace
// ─────────────────────────────────────────────────────────────────────────────

namespace nkentseu {

bool NkFTFontFace::Create(
    void* ftLibrary, const uint8* data, usize size, uint16 ppem
) noexcept {
    if (!ftLibrary || !data || size == 0 || ppem == 0) return false;

    FT_Face face = nullptr;
    FT_Error err = FT_New_Memory_Face(
        AsLib(ftLibrary),
        reinterpret_cast<const FT_Byte*>(data),
        static_cast<FT_Long>(size),
        0,
        &face
    );
    if (err != 0 || !face) return false;

    err = FT_Set_Pixel_Sizes(face, 0, ppem);
    if (err != 0) {
        FT_Done_Face(face);
        return false;
    }

    mFTFace = face;
    mPPEM   = ppem;
    mInUse  = true;
    return true;
}

void NkFTFontFace::Destroy() noexcept {
    if (mFTFace) {
        FT_Done_Face(AsFace(mFTFace));
        mFTFace = nullptr;
    }
    mPPEM   = 0;
    mInUse  = false;
}

bool NkFTFontFace::GetGlyph(uint32 codepoint, NkFTGlyph& out) noexcept {
    out = {};
    if (!mFTFace) return false;

    FT_Face face = AsFace(mFTFace);

    FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
    if (glyphIndex == 0) {
        // Glyphe .notdef — on l'indique mais on rend quand même
        out.valid   = false;
        out.isEmpty = true;
        return false;
    }

    FT_Error err = FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);
    if (err != 0) return false;

    FT_GlyphSlot slot = face->glyph;
    FT_Bitmap&   bmp  = slot->bitmap;

    // Remplissage du résultat
    out.valid    = true;
    out.width    = static_cast<int32>(bmp.width);
    out.height   = static_cast<int32>(bmp.rows);
    out.pitch    = static_cast<int32>(bmp.pitch < 0 ? -bmp.pitch : bmp.pitch);
    out.bearingX = slot->bitmap_left;
    out.bearingY = slot->bitmap_top;
    // advance est en 26.6 → shift right 6 pour obtenir des pixels entiers
    out.xAdvance = static_cast<int32>(slot->advance.x >> 6);
    out.isEmpty  = (out.width == 0 || out.height == 0);
    // Le buffer bitmap appartient à FreeType et reste valide jusqu'au prochain FT_Load_Glyph.
    out.bitmap   = out.isEmpty ? nullptr : reinterpret_cast<const uint8*>(bmp.buffer);

    return true;
}

int32 NkFTFontFace::GetAscender() const noexcept {
    if (!mFTFace) return 0;
    FT_Face face = AsFace(mFTFace);
    // Les métriques de la taille courante sont en 26.6
    return static_cast<int32>(face->size->metrics.ascender >> 6);
}

int32 NkFTFontFace::GetDescender() const noexcept {
    if (!mFTFace) return 0;
    FT_Face face = AsFace(mFTFace);
    return static_cast<int32>(face->size->metrics.descender >> 6);
}

int32 NkFTFontFace::GetLineHeight() const noexcept {
    if (!mFTFace) return 0;
    FT_Face face = AsFace(mFTFace);
    return static_cast<int32>(face->size->metrics.height >> 6);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkFTFontLibrary
// ─────────────────────────────────────────────────────────────────────────────

bool NkFTFontLibrary::Init() noexcept {
    if (mFTLibrary) return true;  // déjà initialisé

    FT_Library lib = nullptr;
    FT_Error err = FT_Init_FreeType(&lib);
    if (err != 0 || !lib) return false;

    mFTLibrary = lib;
    return true;
}

void NkFTFontLibrary::Destroy() noexcept {
    // Libère toutes les faces encore actives
    for (uint32 i = 0; i < kMaxFaces; ++i) {
        if (mFacePool[i].mInUse) {
            mFacePool[i].Destroy();
        }
    }

    if (mFTLibrary) {
        FT_Done_FreeType(AsLib(mFTLibrary));
        mFTLibrary = nullptr;
    }
}

NkFTFontFace* NkFTFontLibrary::LoadFont(
    const uint8* data, usize size, uint16 ppem
) noexcept {
    if (!mFTLibrary || !data || size == 0 || ppem == 0) return nullptr;

    // Cherche un slot libre dans le pool
    for (uint32 i = 0; i < kMaxFaces; ++i) {
        if (!mFacePool[i].mInUse) {
            if (mFacePool[i].Create(mFTLibrary, data, size, ppem)) {
                return &mFacePool[i];
            }
            return nullptr;
        }
    }
    return nullptr; // pool plein
}

void NkFTFontLibrary::FreeFont(NkFTFontFace* face) noexcept {
    if (!face) return;
    // Vérifie que la face appartient bien à ce pool
    for (uint32 i = 0; i < kMaxFaces; ++i) {
        if (&mFacePool[i] == face) {
            mFacePool[i].Destroy();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkFontLibrary — méthodes backend FreeType
// ─────────────────────────────────────────────────────────────────────────────

// NkFTFontFace* NkFontLibrary::LoadFontFT(
//     const uint8* data, usize size, uint16 ppem
// ) noexcept {
//     // Initialisation à la demande — FreeType n'est créé que si LoadFontFT est appelé
//     if (!mFTLib.IsValid()) {
//         if (!mFTLib.Init()) return nullptr;
//     }
//     return mFTLib.LoadFont(data, size, ppem);
// }

// void NkFontLibrary::FreeFontFT(NkFTFontFace* face) noexcept {
//     mFTLib.FreeFont(face);
// }

} // namespace nkentseu
