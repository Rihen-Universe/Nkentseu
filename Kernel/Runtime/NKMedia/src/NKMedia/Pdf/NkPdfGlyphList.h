//
// NkPdfGlyphList.h — nom de glyphe -> texte UTF-8 (Adobe Glyph List).
//
// Les polices Type 1 des PDF dvips/LaTeX sans /ToUnicode designent leurs
// caracteres par NOM (/space, /alpha, /uni0041...), via /Differences ou la
// table en clair de leur programme. L'AGL, donnee publiee par Adobe
// (BSD-3-Clause), dit ce que chaque nom represente. La table est generee par
// script (voir NkPdfGlyphList.cpp) et n'est jamais retapee a la main.
//
#pragma once

#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Texte UTF-8 du glyphe nomme `name` (longueur `len`, sans la barre
			// oblique). Chaine VIDE si le nom est opaque (sous-ensembles « a35 »)
			// — on se tait plutot que d'inventer. Gere la liste d'Adobe et les
			// noms algorithmiques uniXXXX / uXXXX.
			NkString NkPdfGlyphNameToText(const char *name, int32 len);

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
