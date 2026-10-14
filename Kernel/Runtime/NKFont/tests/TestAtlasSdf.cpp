// =============================================================================
// @File    TestAtlasSdf.cpp
// @Brief   L'ATLAS SDF : CHAQUE GLYPHE PORTE LE SDF DE SA PROPRE IMAGE.
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (14/10/2026) — L'ATLAS SDF CORROMPU
// =============================================================================
//  Signale par un apprenant (exercice « Le SDF, mesure », sprint 5), transmis par
//  la session du site : avec `atlas.sdfMode = true`, l'atlas sort corrompu.
//  La cause : `NkFontAtlas::Build` appelait
//
//      NkMakeSDFFromBitmap(dst, drawW, drawH, sdfDst, sdfSpread);
//
//  avec `dst` et `sdfDst` DANS l'atlas (pas de ligne = la largeur de l'ATLAS),
//  alors que la fonction lit `alpha[py * w + px]` et ecrit `sdfOut[py * w + px]`
//  avec w = la largeur du GLYPHE. Des que le glyphe est moins large que l'atlas
//  -- toujours --, elle lit et ecrit au mauvais endroit.
//
// =============================================================================
//  CE QUE CE BANC MESURE
// =============================================================================
//  (s1) la fonction a pas (`NkMakeSDFFromBitmapStride`) sur une image posee dans
//       un tampon plus large : le meme SDF que sur une copie contigue, et rien
//       d'ecrit hors du rectangle ;
//  (s2) l'atlas SDF, glyphe par glyphe : sa region est EXACTEMENT le SDF calcule
//       sur l'image de ce glyphe tiree d'un atlas ordinaire (meme police, meme
//       taille, meme reglage) -- une egalite a l'octet, pas un seuil ;
//  (s3) LE NEGATIF DANS LE MEME BINAIRE : l'appel d'avant (sans le pas) donne un
//       autre resultat sur au moins un glyphe. Sans lui, (s2) pourrait etre vert
//       parce que la mesure ne sait pas voir le defaut.
// =============================================================================

#include "NKFont/Core/NkFontParser.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKFont/NkFont.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;

namespace {

	/// Le rectangle d'un glyphe dans un atlas (pixels), lu sur ses coordonnees de texture.
	struct Rect {
			int x = 0, y = 0, w = 0, h = 0;
	};
	Rect RectDe(const NkFontGlyph &g, int largeur, int hauteur) {
		Rect r;
		r.x = static_cast<int>(g.u0 * static_cast<float>(largeur) + 0.5f);
		r.y = static_cast<int>(g.v0 * static_cast<float>(hauteur) + 0.5f);
		r.w = static_cast<int>(g.u1 * static_cast<float>(largeur) + 0.5f) - r.x;
		r.h = static_cast<int>(g.v1 * static_cast<float>(hauteur) + 0.5f) - r.y;
		return r;
	}

	/// Un atlas neuf (alpha ou SDF), la police a 18 px, sans sur-echantillonnage (les coordonnees de texture
	/// tombent alors sur des pixels entiers).
	NkFontAtlas *Atlas(const nkft_uint8 *octets, nkft_uint32 taille, bool sdf, NkFont **police) {
		NkFontAtlas *atlas = new NkFontAtlas();
		atlas->sdfMode = sdf;
		atlas->sdfSpread = 4;
		NkFontConfig cfg;
		cfg.oversampleH = 1;
		cfg.oversampleV = 1;
		*police = atlas->AddFontFromMemory(octets, taille, 18.f, &cfg);
		if (!*police || !atlas->Build()) {
			delete atlas;
			return nullptr;
		}
		return atlas;
	}

} // namespace

/// Les temoins du SDF ; `temoin` est celui de la suite (TestAtlasSansConfig.cpp).
void NkTestsAtlasSdf(void (*temoin)(bool, const char *), const nkft_uint8 *octets, nkft_uint32 taille) {
	std::printf("\n===== NKFont_Tests : l'atlas SDF =====\n");

	// ── (s1) la fonction a pas, sur une image posee dans un tampon plus large ──
	{
		const int pas = 40, gx = 7, gy = 3, gw = 12, gh = 10, etendue = 4;
		static nkft_uint8 source[pas * 20], sortie[pas * 20], copie[gw * gh], reference[gw * gh];
		std::memset(source, 0, sizeof(source));
		std::memset(sortie, 0, sizeof(sortie));
		for (int y = 3; y < 7; ++y) // un carre opaque de 4 x 4 dans l'image du glyphe
			for (int x = 4; x < 8; ++x)
				source[(gy + y) * pas + gx + x] = 255;
		for (int y = 0; y < gh; ++y)
			for (int x = 0; x < gw; ++x)
				copie[y * gw + x] = source[(gy + y) * pas + gx + x];
		nkfont::NkMakeSDFFromBitmap(copie, gw, gh, reference, etendue);
		nkfont::NkMakeSDFFromBitmapStride(source + gy * pas + gx, gw, gh, pas, sortie + gy * pas + gx, pas, etendue);
		bool memes = true, horsIntact = true;
		for (int y = 0; y < 20; ++y)
			for (int x = 0; x < pas; ++x) {
				const bool dedans = x >= gx && x < gx + gw && y >= gy && y < gy + gh;
				if (dedans)
					memes = memes && sortie[y * pas + x] == reference[(y - gy) * gw + (x - gx)];
				else
					horsIntact = horsIntact && sortie[y * pas + x] == 0;
			}
		temoin(memes && horsIntact, "(s1) avec son pas, le SDF d'une image posee dans un tampon large est celui de sa copie contigue, et rien n'est ecrit hors d'elle");
	}

	// ── (s2) et (s3) : l'atlas SDF, glyphe par glyphe ──
	NkFont *pNormale = nullptr, *pSdf = nullptr;
	NkFontAtlas *normal = Atlas(octets, taille, false, &pNormale);
	NkFontAtlas *sdf = Atlas(octets, taille, true, &pSdf);
	if (!normal || !sdf) {
		temoin(false, "(s2) les deux atlas (ordinaire et SDF) se construisent");
		delete normal;
		delete sdf;
		return;
	}
	nkft_uint8 *pxN = nullptr, *pxS = nullptr;
	nkft_int32 lN = 0, hN = 0, lS = 0, hS = 0;
	normal->GetTexDataAsAlpha8(&pxN, &lN, &hN);
	sdf->GetTexDataAsAlpha8(&pxS, &lS, &hS);
	const char *lettres = "ABgWm?";
	int glyphes = 0, justes = 0, differentsAvant = 0;
	for (const char *c = lettres; *c; ++c) {
		const NkFontGlyph *gN = pNormale->FindGlyphNoFallback(static_cast<NkFontCodepoint>(*c));
		const NkFontGlyph *gS = pSdf->FindGlyphNoFallback(static_cast<NkFontCodepoint>(*c));
		if (!gN || !gS || !gN->visible || !gS->visible)
			continue;
		const Rect rN = RectDe(*gN, lN, hN), rS = RectDe(*gS, lS, hS);
		if (rN.w != rS.w || rN.h != rS.h || rN.w <= 0 || rN.h <= 0 || rN.w > 128 || rN.h > 128)
			continue;
		++glyphes;
		static nkft_uint8 image[128 * 128], attendu[128 * 128], avant[128 * 128 * 4];
		for (int y = 0; y < rN.h; ++y)
			for (int x = 0; x < rN.w; ++x)
				image[y * rN.w + x] = pxN[(rN.y + y) * lN + rN.x + x];
		nkfont::NkMakeSDFFromBitmap(image, rN.w, rN.h, attendu, 4 /* l etendue des deux atlas (Atlas) */);
		bool memes = true;
		for (int y = 0; y < rS.h && memes; ++y)
			for (int x = 0; x < rS.w && memes; ++x)
				memes = pxS[(rS.y + y) * lS + rS.x + x] == attendu[y * rS.w + x];
		justes += memes ? 1 : 0;
		// L'APPEL D'AVANT, rejoue sur l'atlas ordinaire : la fonction sans pas, sur un pointeur dans l'atlas.
		// (Elle lit des lignes de la largeur du glyphe dans un tampon dont le pas est la largeur de l'atlas.)
		std::memset(avant, 0, sizeof(avant));
		nkfont::NkMakeSDFFromBitmap(pxN + rN.y * lN + rN.x, rN.w, rN.h, avant, 4 /* l etendue des deux atlas (Atlas) */);
		bool memeAvant = true;
		for (int y = 0; y < rN.h && memeAvant; ++y)
			for (int x = 0; x < rN.w && memeAvant; ++x)
				memeAvant = avant[y * rN.w + x] == attendu[y * rN.w + x];
		differentsAvant += memeAvant ? 0 : 1;
	}
	std::printf("  %d glyphe(s) compare(s), %d egal(aux) au SDF de leur image ; l'appel d'avant en rend %d different(s)\n", glyphes, justes,
				differentsAvant);
	temoin(glyphes >= 4 && justes == glyphes, "(s2) l'atlas SDF porte, pour chaque glyphe, le SDF de SA propre image, a l'octet pres");
	temoin(glyphes >= 4 && differentsAvant > 0, "(s3) NEGATIF : l'appel d'avant (sans le pas) donne un autre SDF sur au moins un glyphe");
	delete normal;
	delete sdf;
}
