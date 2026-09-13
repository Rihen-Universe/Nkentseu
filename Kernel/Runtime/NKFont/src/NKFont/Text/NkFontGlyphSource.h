#pragma once
// =============================================================================
// NkFontGlyphSource.h — NKFont REPOND au contrat NkIGlyphSource de NKCore
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// LICENCE : Proprietary - All Rights Reserved (voir LICENSE)
//
// =============================================================================
//  CE QUE CE FICHIER EST, ET CE QU'IL EVITE
// =============================================================================
//  Un ADAPTATEUR, rien de plus : il branche les contours de NKFont
//  (`NkGetGlyphShape`) sur l'interface que NKCore declare et que NKImage
//  consomme. C'est la piece qui permet a NKImage de peindre du <text> SANS
//  dependre de NKFont -- donc sans imposer les polices embarquees a qui ne veut
//  que decoder un PNG (Bare, le Web).
//
//  QUI L'UTILISE : l'application. Une ligne au demarrage --
//      static NkFontGlyphSource glyphes;
//      NkSVGCodec::SetDefaultGlyphSource(&glyphes);
//  -- et tous les <text> des SVG decodes ensuite sont peints. Sans cette ligne,
//  le codec SAUTE le texte ET LE DIT (SkippedAt : « texte : aucune source de
//  glyphes »). Le choix est explicite des deux cotes.
//
//  ⚠️ `font-size` EST LE CADRATIN (em). NkFontAtlas, lui, echelonne par
//  (ascender - descender) : c'est la convention de son atlas, pas celle de CSS.
//  Ici on suit CSS et SVG -- `NkScaleForEmToPixels`. Un meme `font-size` rend
//  donc plus grand qu'un `sizePixels` d'atlas (rapport mesure : 1,25 pour
//  Inter), et c'est la NORME qui a raison pour un fichier destine a etre lu
//  ailleurs.
// =============================================================================

#include "NKCore/Text/NkIGlyphSource.h"
#include "NKFont/Core/NkFontParser.h"
#include "NKFont/Embedded/NkFontEmbedded.h"

namespace nkentseu {

	// =========================================================================
	// NkFontGlyphSource — une source de glyphes servie par les polices EMBARQUEES
	// -------------------------------------------------------------------------
	// Les fontes sont ouvertes A LA DEMANDE et gardees : ouvrir coute une
	// decompression et un parsing de tables, et un document en redemande la meme
	// a chaque glyphe.
	// =========================================================================
	class NkFontGlyphSource : public NkIGlyphSource {
		public:
			NkFontGlyphSource() noexcept = default;
			~NkFontGlyphSource() noexcept override;

			NkFontGlyphSource(const NkFontGlyphSource &) = delete;
			NkFontGlyphSource &operator=(const NkFontGlyphSource &) = delete;

			bool SelectFace(const char *family, int32 weight) noexcept override;
			const char *ActiveFamily() const noexcept override;
			float32 Advance(uint32 codepoint, float32 fontSize) const noexcept override;
			bool Metrics(float32 fontSize, float32 &ascent, float32 &descent) const noexcept override;
			bool Outline(uint32 codepoint, float32 fontSize, float32 penX, float32 baselineY,
						 NkIGlyphSink &sink) const noexcept override;

			/// La graisse REELLEMENT active -- celle de la coupe choisie, pas celle
			/// qui a ete demandee. C'est cette valeur qui dit a l'appelant s'il doit
			/// simuler un gras.
			int32 ActiveWeight() const noexcept override {
				return (mActive >= 0 && mActive < mNbFaces) ? mFaces[mActive].poids : 400;
			}

			/// DECLARE UNE COUPE depuis un fichier (.ttf / .otf). Mesure du
			/// 2026-09-05 : les dix polices embarquees dans NKFont sont TOUTES des
			/// « Regular » -- il n'y a aucune graisse dans le binaire, et le codec
			/// SVG ne pouvait donc que simuler. Les vraies coupes existent sur le
			/// disque (`Resources/Fonts/`) ; c'est a l'application de dire lesquelles
			/// elle veut, plutot qu'a une bibliotheque d'aller les chercher a un
			/// chemin devine.
			/// @return false si le fichier est illisible ou n'est pas une fonte.
			bool AjouterFonteFichier(const char *famille, int32 poids, const char *chemin) noexcept;

		private:
			static constexpr int32 kMaxFaces = 8;

			struct Face {
					char famille[64] = {0};
					int32 poids = 400;		 ///< la graisse de CETTE coupe
					nkft_uint8 *ttf = nullptr;
					bool ttfPropre = false;	 ///< buffer lu d'un fichier (a liberer autrement)
					nkfont::NkFontFaceInfo info;
					bool ok = false;
			};

			Face mFaces[kMaxFaces];
			int32 mNbFaces = 0;
			int32 mActive = -1;
			int32 mWeight = 400;

			/// Ouvre (ou retrouve) la fonte du nom donne, dans la graisse la plus
			/// proche de @p poids. -1 si rien n'est ouvrable.
			int32 Ouvrir(const char *famille, int32 poids) noexcept;
	};

} // namespace nkentseu
