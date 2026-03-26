#pragma once
/**
 * @File    NkFTFontFace.h
 * @Brief   Backend FreeType pour NKFont — chargement et rasterisation de polices.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Backend alternatif basé sur FreeType2 pour NKFont.
 *  Utilisé en remplacement temporaire des parsers/rasterizers natifs NKFont
 *  pendant leur développement. Ne supprime pas les classes existantes.
 *
 *  Usage typique :
 * @code
 *   NkFTFontLibrary lib;
 *   if (!lib.Init()) { ... }
 *
 *   NkFTFontFace* face = lib.LoadFont(data, size, 16);
 *   if (!face) { ... }
 *
 *   NkFTGlyph g;
 *   if (face->GetGlyph('A', g) && !g.isEmpty) {
 *       // g.bitmap : pixels Gray8, g.width × g.height
 *       // g.bearingX, g.bearingY, g.xAdvance (en pixels)
 *   }
 *
 *   lib.FreeFont(face);
 *   lib.Destroy();
 * @endcode
 *
 * @Note
 *  Les types FreeType (FT_Library, FT_Face) sont cachés derrière des void*
 *  pour ne pas exposer les headers FreeType dans l'API publique.
 */

#include "NKFont/NkFontExport.h"

#ifndef NKENTSEU_TYPES_DEFINED
#  define NKENTSEU_TYPES_DEFINED
#  include "NKCore/NkTypes.h"
#endif

namespace nkentseu {

    // =========================================================================
    //  NkFTGlyph — glyphe rasterisé par FreeType
    // =========================================================================

    struct NKENTSEU_FONT_API NkFTGlyph {
        const uint8* bitmap;    ///< Pixels Gray8, pitch × height octets (non-owning, valide jusqu'au prochain GetGlyph)
        int32   width;          ///< Largeur en pixels
        int32   height;         ///< Hauteur en pixels
        int32   pitch;          ///< Stride en octets (peut être > width)
        int32   bearingX;       ///< Décalage horizontal depuis la position du curseur (pixels)
        int32   bearingY;       ///< Décalage vertical depuis la ligne de base (pixels, Y vers le haut)
        int32   xAdvance;       ///< Avance horizontale (pixels)
        bool    isEmpty;        ///< Glyphe vide (espace, etc.) — bitmap peut être null
        bool    valid;          ///< false si le glyphe est absent de la police
    };

    // =========================================================================
    //  NkFTFontFace — une police à une taille donnée (wraps FT_Face)
    // =========================================================================

    class NKENTSEU_FONT_API NkFTFontFace {
    public:

        NkFTFontFace()  noexcept = default;
        ~NkFTFontFace() noexcept { Destroy(); }

        NkFTFontFace(const NkFTFontFace&)            = delete;
        NkFTFontFace& operator=(const NkFTFontFace&) = delete;

        // ── Cycle de vie (géré par NkFTFontLibrary) ──────────────────────────

        /**
         * @Brief Crée la face FreeType depuis un buffer de police.
         * @param ftLibrary  Handle FT_Library (void* opaque depuis NkFTFontLibrary).
         * @param data       Buffer du fichier de police (doit rester valide).
         * @param size       Taille du buffer en octets.
         * @param ppem       Taille cible en pixels par em.
         * @return true si succès.
         */
        bool Create(void* ftLibrary, const uint8* data, usize size, uint16 ppem) noexcept;

        /**
         * @Brief Libère la face FreeType.
         */
        void Destroy() noexcept;

        // ── Accès aux glyphes ─────────────────────────────────────────────────

        /**
         * @Brief Rasterise un glyphe par codepoint Unicode.
         *        Les pixels retournés dans out.bitmap sont valides jusqu'au
         *        prochain appel à GetGlyph (buffer interne FreeType réutilisé).
         *        Copiez les pixels si vous en avez besoin plus longtemps.
         * @param codepoint  Codepoint Unicode (ex. 'A' = 65, U+00E9 = 'é').
         * @param out        Résultat du rendu.
         * @return true si succès (false si glyphe absent).
         */
        bool GetGlyph(uint32 codepoint, NkFTGlyph& out) noexcept;

        // ── Métriques globales ────────────────────────────────────────────────

        [[nodiscard]] int32  GetAscender()   const noexcept; ///< Pixels au-dessus de la baseline
        [[nodiscard]] int32  GetDescender()  const noexcept; ///< Pixels en dessous (valeur negative)
        [[nodiscard]] int32  GetLineHeight() const noexcept; ///< Interligne recommandé en pixels

        [[nodiscard]] uint16 GetPPEM()  const noexcept { return mPPEM;         }
        [[nodiscard]] bool   IsValid()  const noexcept { return mFTFace != nullptr; }

    private:
        friend class NkFTFontLibrary;

        void*  mFTFace   = nullptr; ///< FT_Face (opaque)
        uint16 mPPEM     = 0;
        bool   mInUse    = false;
    };

    // =========================================================================
    //  NkFTFontLibrary — gestionnaire FreeType (wraps FT_Library)
    // =========================================================================

    class NKENTSEU_FONT_API NkFTFontLibrary {
    public:

        NkFTFontLibrary()  noexcept = default;
        ~NkFTFontLibrary() noexcept { Destroy(); }

        NkFTFontLibrary(const NkFTFontLibrary&)            = delete;
        NkFTFontLibrary& operator=(const NkFTFontLibrary&) = delete;

        /**
         * @Brief Initialise FreeType2 (FT_Init_FreeType).
         * @return true si succès.
         */
        bool Init() noexcept;

        /**
         * @Brief Libère toutes les faces et détruit FT_Library.
         */
        void Destroy() noexcept;

        // ── Chargement / déchargement ─────────────────────────────────────────

        /**
         * @Brief Charge une police depuis un buffer mémoire.
         * @param data   Buffer du fichier de police (doit rester valide pendant toute la durée de vie de la face).
         * @param size   Taille du buffer en octets.
         * @param ppem   Taille en pixels par em.
         * @return Pointeur vers une NkFTFontFace du pool interne, nullptr si échec ou pool plein.
         */
        [[nodiscard]] NkFTFontFace* LoadFont(
            const uint8* data, usize size, uint16 ppem
        ) noexcept;

        /**
         * @Brief Libère une face précédemment allouée par LoadFont.
         */
        void FreeFont(NkFTFontFace* face) noexcept;

        [[nodiscard]] bool IsValid() const noexcept { return mFTLibrary != nullptr; }

    private:
        void* mFTLibrary = nullptr; ///< FT_Library (opaque)

        static constexpr uint32 kMaxFaces = 32;
        NkFTFontFace mFacePool[kMaxFaces]; ///< Pool fixe — pas d'allocation dynamique
    };

} // namespace nkentseu
