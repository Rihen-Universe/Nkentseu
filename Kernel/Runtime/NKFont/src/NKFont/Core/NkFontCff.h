#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKFont/src/NKFont/Core/NkFontCff.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   CFF COMPLET : la table « CFF » d'un OpenType (OTTO) et le CFF NU
//          d'un PDF (/FontFile3 : Type1C, CIDFontType0C).
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI (06/10). Rodolf, en cours : « les pdf ne sont pas correctement lus,
// elles sont vides ». Ses chapitres (XeLaTeX) n'embarquent que du CFF et du
// TrueType sans cmap Unicode ; NkInitFontFace refusait l'un et l'autre, et
// l'ancien decodeur CFF n'executait ni les subroutines, ni le DICT prive, ni
// les polices CID, et lisait 29/30 (callgsubr, vhcurveto) comme des nombres.
// Resultat mesure : les formes de la page, AUCUN caractere.
//
// Ce module suit Adobe TN #5176 (CFF) et TN #5177 (Type 2 charstrings) :
// INDEX, DICT, DICT prive, subroutines locales/globales (biais), polices CID
// (FDArray, FDSelect 0 et 3), charset (0/1/2 et ISOAdobe), encodage (0/1,
// supplements, Standard), et tous les operateurs de trace, de flex, de hint
// et d'arithmetique, plus « seac » par endchar.
// -----------------------------------------------------------------------------

#include "NkFontParser.h"

namespace nkentseu {
	namespace nkfont {

		/// Lit la table CFF situee a `cffOffset` (longueur `cffLen`) dans
		/// `info->data` et remplit les champs cff* de `info`. Faux si illisible.
		bool NkCffInit(NkFontFaceInfo *info, nkft_uint32 cffOffset, nkft_uint32 cffLen);

		/// Contour du glyphe `glyph` (indice CharStrings), en unites de police.
		bool NkCffGlyphShape(const NkFontFaceInfo *info, NkGlyphId glyph, NkFontVertexBuffer *buf);

		/// Nombre de glyphes du programme.
		nkft_int32 NkCffGlyphCount(const NkFontFaceInfo *info);

		/// Police CID (ROS) : le charset y associe des CID, pas des noms.
		bool NkCffIsCID(const NkFontFaceInfo *info);

		/// CID -> glyphe (polices CID). 0 si le CID n'existe pas.
		NkGlyphId NkCffGlyphFromCID(const NkFontFaceInfo *info, nkft_uint32 cid);

		/// Nom de glyphe -> glyphe (polices nommees). 0 si inconnu.
		NkGlyphId NkCffGlyphFromName(const NkFontFaceInfo *info, const char *name, nkft_int32 len);

		/// Nom du glyphe `glyph` dans `out` (termine par 0) ; rend sa longueur, 0 si
		/// inconnu (police CID, ou hors charset).
		nkft_int32 NkCffGlyphName(const NkFontFaceInfo *info, NkGlyphId glyph, char *out, nkft_int32 cap);

		/// Code de l'encodage INTERNE (Standard ou propre a la police) -> glyphe.
		/// 0 si le code n'y figure pas.
		NkGlyphId NkCffGlyphFromCode(const NkFontFaceInfo *info, nkft_uint32 code);

	} // namespace nkfont
} // namespace nkentseu
