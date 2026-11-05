// =============================================================================
// @File    TestGlyphesComposes.cpp
// @Brief   LES COURBES D'UN GLYPHE COMPOSE SE TRANSLATENT AVEC LEUR COMPOSANT.
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (05/11/2026)
// =============================================================================
//  Rodolf : « dans l'application, quand tu mets "...", le troisieme point est toujours mal affiche ». La cause
//  n'etait ni la mesure, ni la decoupe : DecodeCompositeGlyph (NkFontParser.cpp) appliquait la matrice du composant
//  au point de controle des courbes SANS y ajouter le decalage (dx, dy), alors qu'il l'ajoutait au sommet. L'ellipse
//  « … » d'Inter est composee de trois points decales ; le premier (decalage nul) etait juste, les deux autres
//  gardaient leur point de controle a l'origine : un point rond devenu une trainee. Les accents (é, ü...) etaient
//  deformes de la meme facon.
//
//  CE QUE LE BANC MESURE : pour chaque courbe de chaque contour, le point de controle reste dans la boite du contour
//  (agrandie de 60 % de sa taille : une courbe quadratique deborde un peu des sommets, jamais d'un point entier).
//  Le NEGATIF est dans le meme binaire : le defaut est REPRODUIT sur une copie du glyphe (le point de controle du
//  dernier contour ramene a l'origine), et la mesure doit le voir.
// =============================================================================

#include "NKFont/Core/NkFontParser.h"
#include "NKFont/Embedded/NkFontEmbedded.h"

using namespace nkentseu;
using namespace nkentseu::nkfont;

namespace {

	/// Nombre de courbes dont le point de controle sort de la boite de son contour.
	int Egares(const NkFontVertexBuffer &b) {
		int egares = 0;
		nkft_uint32 debut = 0;
		while (debut < b.count) {
			nkft_uint32 fin = debut + 1;
			while (fin < b.count && b.verts[fin].type != NK_FONT_VERTEX_MOVE)
				++fin;
			int minx = 32767, miny = 32767, maxx = -32768, maxy = -32768;
			for (nkft_uint32 i = debut; i < fin; ++i) {
				const NkFontVertex &v = b.verts[i];
				minx = v.x < minx ? v.x : minx;
				maxx = v.x > maxx ? v.x : maxx;
				miny = v.y < miny ? v.y : miny;
				maxy = v.y > maxy ? v.y : maxy;
			}
			const int mx = (maxx - minx) * 6 / 10 + 8, my = (maxy - miny) * 6 / 10 + 8;
			for (nkft_uint32 i = debut; i < fin; ++i) {
				const NkFontVertex &v = b.verts[i];
				if (v.type == NK_FONT_VERTEX_CURVE &&
					(v.cx < minx - mx || v.cx > maxx + mx || v.cy < miny - my || v.cy > maxy + my))
					++egares;
			}
			debut = fin;
		}
		return egares;
	}

	int Contours(const NkFontVertexBuffer &b) {
		int n = 0;
		for (nkft_uint32 i = 0; i < b.count; ++i)
			if (b.verts[i].type == NK_FONT_VERTEX_MOVE)
				++n;
		return n;
	}

} // namespace

/// Les temoins ; `temoin` est celui de la suite (TestAtlasSansConfig.cpp).
void NkTestsGlyphesComposes(void (*temoin)(bool, const char *)) {
	const NkEmbeddedFontData *police = NkFontEmbedded::GetData(NkEmbeddedFontId::Inter);
	nkft_uint32 taille = 0;
	nkft_uint8 *octets = police ? NkFontEmbedded::DecompressData(*police, &taille) : nullptr;
	temoin(octets != nullptr && taille > 0, "glyphes composes : Inter se decompresse");
	if (!octets)
		return;
	NkFontFaceInfo info;
	const bool ouverte = NkInitFontFace(&info, octets, taille);
	temoin(ouverte, "glyphes composes : Inter s'ouvre");
	if (ouverte) {
		static NkFontVertexBuffer forme, copie; // 2048 sommets : pas sur la pile
		struct Cas {
				nkft_uint32 cp;
				const char *nom;
		};
		const Cas cas[] = {{0x2026, "l'ellipse « … »"}, {0x00E9, "é"}, {0x00FC, "ü"}, {0x00E0, "à"}, {0x00EA, "ê"}};
		for (const Cas &c : cas) {
			const NkGlyphId g = NkFindGlyphIndex(&info, c.cp);
			const bool lu = g != 0 && NkGetGlyphShape(&info, g, &forme) && forme.count > 0;
			char libelle[160];
			int k = 0;
			for (const char *p = "glyphes composes : les points de controle de "; *p && k < 100; ++p)
				libelle[k++] = *p;
			for (const char *p = c.nom; *p && k < 150; ++p)
				libelle[k++] = *p;
			libelle[k] = '\0';
			temoin(lu && Egares(forme) == 0, libelle);
			if (c.cp == 0x2026) {
				temoin(lu && Contours(forme) >= 3, "glyphes composes : l'ellipse a ses trois points (trois contours)");
				// NEGATIF : le defaut remis sur une copie (point de controle du dernier contour ramene a l'origine)
				copie = forme;
				nkft_uint32 dernier = 0;
				for (nkft_uint32 i = 0; i < copie.count; ++i)
					if (copie.verts[i].type == NK_FONT_VERTEX_MOVE)
						dernier = i;
				for (nkft_uint32 i = dernier; i < copie.count; ++i)
					if (copie.verts[i].type == NK_FONT_VERTEX_CURVE) {
						copie.verts[i].cx = 0;
						copie.verts[i].cy = 0;
					}
				temoin(lu && Egares(copie) > 0, "NEGATIF : le defaut remis (controle a l'origine) est vu par la mesure");
			}
		}
	}
	NkFontEmbedded::FreeDecompressedData(octets);
}
