// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontParser.h
// DESCRIPTION: Parser TTF/OTF/WOFF + rastériseur scanline + SDF.
//
// Formats supportés :
//   ✅ TTF SimpleGlyph + CompositeGlyph
//   ✅ TTC (TrueType Collection) via faceIndex
//   ✅ OTF avec table glyf (contours TrueType)
//   ✅ OTF/CFF (Type 2 charstrings — interpréteur intégré)
//   ✅ WOFF (décompresseur zlib intégré)
//   ⚠  WOFF2 (Brotli — non supporté, dépendance externe requise)
//   ✅ cmap format 4 (BMP) et format 12 (full Unicode)
//   ✅ kern table format 0
//   ✅ SDF generation (Signed Distance Field)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_CORE_NKFONTPARSER_H
#define NK_NKFONT_CORE_NKFONTPARSER_H

#include "NkFontTypes.h"

namespace nkentseu {
	namespace nkfont {

		// ============================================================
		// Big-endian readers
		// ============================================================

		inline nkft_uint8 NkReadU8(const nkft_uint8 *p) {
			return p[0];
		}

		inline nkft_uint16 NkReadU16(const nkft_uint8 *p) {
			return (nkft_uint16)((p[0] << 8) | p[1]);
		}

		inline nkft_uint32 NkReadU32(const nkft_uint8 *p) {
			return ((nkft_uint32)p[0] << 24) | ((nkft_uint32)p[1] << 16) | ((nkft_uint32)p[2] << 8) |
				   ((nkft_uint32)p[3]);
		}

		inline nkft_int16 NkReadI16(const nkft_uint8 *p) {
			return (nkft_int16)NkReadU16(p);
		}

		inline nkft_int32 NkReadI32(const nkft_uint8 *p) {
			return (nkft_int32)NkReadU32(p);
		}

		// ============================================================
		// NkFontDataSpan
		// ============================================================

		struct NkFontDataSpan {
				const nkft_uint8 *data = nullptr;
				nkft_size size = 0;

				bool IsValid(nkft_size offset, nkft_size len = 1) const {
					return data && (offset + len) <= size;
				}

				const nkft_uint8 *At(nkft_size offset) const {
					return data + offset;
				}
		};

		// ============================================================
		// NkFontFaceInfo
		// ============================================================

		struct NkFontFaceInfo {
				NkFontDataSpan data;

				// Offsets tables TTF
				nkft_uint32 cmap = 0;
				nkft_uint32 loca = 0;
				nkft_uint32 head = 0;
				nkft_uint32 glyf = 0;
				nkft_uint32 hhea = 0;
				nkft_uint32 hmtx = 0;
				nkft_uint32 kern = 0;
				nkft_uint32 gpos = 0;
				nkft_uint32 os2 = 0;
				nkft_uint32 name_ = 0;
				nkft_uint32 cff = 0;

				// Métriques
				nkft_int32 unitsPerEm = 0;
				nkft_int32 indexToLocFormat = 0;
				nkft_int32 ascent = 0;
				nkft_int32 descent = 0;
				nkft_int32 lineGap = 0;
				nkft_int32 numGlyphs = 0;
				nkft_int32 numHMetrics = 0;

				// cmap sélectionnée
				nkft_uint32 cmapTableOffset = 0;
				nkft_int32 cmapFormat = 0;

				// Type
				bool isCFF = false;

				// CFF base (offset dans data du début de la table CFF)
				nkft_uint32 cffBase = 0;

				// ── CFF COMPLET (06/10, NkFontCff.cpp) ─────────────────────────────
				// Rempli par NkCffInit, pour la table « CFF » d'un OpenType (OTTO) comme
				// pour un CFF NU (PDF /FontFile3 : Type1C, CIDFontType0C), qui n'a ni
				// en-tete sfnt, ni cmap, ni hmtx. Offsets ABSOLUS dans `data` ; 0 = absent.
				bool cffNu = false;			   ///< CFF sans enveloppe OpenType
				nkft_uint32 cffLen = 0;		   ///< longueur de la table CFF
				nkft_uint32 cffCharStrings = 0; ///< INDEX CharStrings
				nkft_int32 cffNumGlyphs = 0;
				nkft_uint32 cffGSubrs = 0;	   ///< INDEX des subroutines GLOBALES
				nkft_uint32 cffStrings = 0;	   ///< INDEX des chaines (SID >= 391)
				nkft_uint32 cffPrivate = 0, cffPrivateLen = 0; ///< DICT prive (police non CID)
				nkft_uint32 cffCharset = 0;	   ///< valeur du DICT : 0/1/2 predefini, sinon relatif
				nkft_uint32 cffEncoding = 0;   ///< valeur du DICT : 0/1 predefini, sinon relatif
				bool cffCid = false;		   ///< police CID (ROS) : FDArray + FDSelect
				nkft_uint32 cffFDArray = 0;
				nkft_uint32 cffFDSelect = 0;
				nkft_float32 cffMatrix[6] = {0.001f, 0.f, 0.f, 0.001f, 0.f, 0.f};

				// cmaps NON Unicode, gardees pour les polices SYMBOLIQUES des PDF :
				// (3,0) Microsoft Symbol et (1,0) Macintosh Roman (ISO 32000, 9.6.6.4).
				nkft_uint32 cmapSymOffset = 0;
				nkft_int32 cmapSymFormat = 0;
				nkft_uint32 cmapMacOffset = 0;
				nkft_int32 cmapMacFormat = 0;

				// WOFF : buffer décompressé (à libérer via FreeFontFace)
				nkft_uint8 *woffBuffer = nullptr;
				nkft_uint32 woffBufferSize = 0;
		};

		// ============================================================
		// Vertex
		// ============================================================

		struct NkFontVertex {
				nkft_int16 x, y, cx, cy, cx1, cy1;
				nkft_uint8 type, padding;
		};

		static constexpr nkft_uint8 NK_FONT_VERTEX_MOVE = 0;
		static constexpr nkft_uint8 NK_FONT_VERTEX_LINE = 1;
		static constexpr nkft_uint8 NK_FONT_VERTEX_CURVE = 2; ///< Bézier quadratique
		static constexpr nkft_uint8 NK_FONT_VERTEX_CUBIC = 3; ///< Bézier cubique (CFF)

		static constexpr nkft_uint32 NK_FONT_MAX_VERTICES = 2048u;

		struct NkFontVertexBuffer {
				NkFontVertex verts[NK_FONT_MAX_VERTICES];
				nkft_uint32 count = 0;

				bool Push(NkFontVertex v) {
					if (count >= NK_FONT_MAX_VERTICES)
						return false;
					verts[count++] = v;
					return true;
				}

				void Clear() {
					count = 0;
				}
		};

		// ============================================================
		// API principale
		// ============================================================

		/**
		 * @brief Initialise depuis un buffer TTF/OTF/WOFF.
		 * WOFF est décompressé automatiquement (buffer alloué dans info->woffBuffer).
		 * Appelez FreeFontFace() pour libérer la mémoire WOFF.
		 */
		///
		/// `cmapUnicodeRequise` (06/10) : vrai par defaut -- une police SANS table
		/// Unicode est refusee, comme avant (l'atlas d'interface se rabat alors sur
		/// une autre). Faux : le lecteur PDF, qui adresse les glyphes lui-meme
		/// (CID, nom, code) ; sont alors aussi acceptes un TrueType sans cmap
		/// Unicode (sous-ensembles embarques) et un CFF NU (/FontFile3).
		bool NkInitFontFace(NkFontFaceInfo *info, const nkft_uint8 *data, nkft_size size, nkft_int32 faceIndex = 0,
							bool cmapUnicodeRequise = true);

		/// (06/10) Glyphe d'une police SYMBOLIQUE (PDF, ISO 32000 9.6.6.4) : cmap
		/// (3,0) avec le code puis 0xF000/0xF100/0xF200 + code, sinon (1,0). 0 si rien.
		NkGlyphId NkFindGlyphIndexSymbolique(const NkFontFaceInfo *info, nkft_uint32 code);

		/**
		 * @brief Libère les ressources allouées par InitFontFace (buffer WOFF).
		 */
		void NkFreeFontFace(NkFontFaceInfo *info);

		NkGlyphId NkFindGlyphIndex(const NkFontFaceInfo *info, NkFontCodepoint cp);

		nkft_float32 NkScaleForPixelHeight(const NkFontFaceInfo *info, nkft_float32 px);
		nkft_float32 NkScaleForEmToPixels(const NkFontFaceInfo *info, nkft_float32 px);

		void NkGetGlyphHMetrics(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_int32 *aw, nkft_int32 *lsb);

		bool NkGetGlyphBox(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_int32 *x0, nkft_int32 *y0, nkft_int32 *x1,
						   nkft_int32 *y1);

		nkft_int32 NkGetGlyphKernAdvance(const NkFontFaceInfo *info, NkGlyphId g1, NkGlyphId g2);

		/**
		 * @brief Décode les contours d'un glyphe (TTF simple/composite, ou CFF Type 2).
		 */
		bool NkGetGlyphShape(const NkFontFaceInfo *info, NkGlyphId glyph, NkFontVertexBuffer *buf);

		void NkGetFontVMetrics(const NkFontFaceInfo *info, nkft_int32 *a, nkft_int32 *d, nkft_int32 *lg);

		bool NkGetFontName(const NkFontFaceInfo *info, nkft_int32 nameID, char *buf, nkft_int32 bufLen);

		// ============================================================
		// Rastériseur
		// ============================================================

		void NkGetGlyphBitmapBox(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_float32 sx, nkft_float32 sy,
								 nkft_float32 shx, nkft_float32 shy, nkft_int32 *ix0, nkft_int32 *iy0, nkft_int32 *ix1,
								 nkft_int32 *iy1);

		void NkMakeGlyphBitmap(const NkFontFaceInfo *info, nkft_uint8 *output, nkft_int32 outW, nkft_int32 outH,
							   nkft_int32 outStride, nkft_float32 sx, nkft_float32 sy, nkft_float32 shx,
							   nkft_float32 shy, NkGlyphId glyph);

		// ============================================================
		// SDF — Signed Distance Field
		// ============================================================

		/**
		 * @brief Génère une texture SDF depuis un bitmap alpha 8 bits.
		 *
		 * Chaque pixel SDF contient la distance signée au contour du glyphe,
		 * normalisée dans [0, 255] :
		 *   > 127 = intérieur du glyphe
		 *   = 127 = exactement sur le contour
		 *   < 127 = extérieur
		 *
		 * Le shader doit lire : float alpha = smoothstep(0.5 - smooth, 0.5 + smooth, sdf/255.0);
		 *
		 * @param alpha    Bitmap alpha 8 bits source (1 octet/pixel).
		 * @param w, h     Dimensions du bitmap.
		 * @param sdfOut   Buffer de sortie (même taille que alpha).
		 * @param spread   Rayon de la zone de transition en pixels (typ. 4-8).
		 */
		void NkMakeSDFFromBitmap(const nkft_uint8 *alpha, nkft_int32 w, nkft_int32 h, nkft_uint8 *sdfOut,
								 nkft_int32 spread = 6);

	} // namespace nkfont
} // namespace nkentseu

#endif // NK_NKFONT_CORE_NKFONTPARSER_H

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================