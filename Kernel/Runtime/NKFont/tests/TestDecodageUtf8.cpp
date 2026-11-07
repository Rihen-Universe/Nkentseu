// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// @File    TestDecodageUtf8.cpp
// @Brief   LE DECODAGE UTF-8 S'ARRETE AU TERMINATEUR ET A LA BORNE.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (07/11/2026)
// =============================================================================
//  Rodolf : « le text dans quel image choisir clignote ». NKCode coupait un texte octet par octet et mesurait chaque
//  morceau ; un morceau qui finit au milieu de « é » (« R\xC3 » puis le zero) faisait lire a NkFontDecodeUTF8 le ZERO
//  comme suite du caractere, puis avancer APRES le terminateur, dans le tas : une lecture hors limites, une largeur
//  au hasard, une coupure qui change d'une image a l'autre. Il avalait aussi l'octet qui suit un debut orphelin.
//
//  CE QUE LE BANC MESURE, sur le vrai NkFontDecodeUTF8 :
//    - des chaines accentuees (2, 3 et 4 octets) se decodent caractere par caractere, avec et sans borne ;
//    - une suite coupee par le terminateur rend U+FFFD et le curseur S'ARRETE sur le zero ; ce qui suit le zero
//      (des octets de suite laisses en memoire) ne change rien au resultat ;
//    - une suite coupee par la borne (le dernier octet du tableau) rend U+FFFD et le curseur s'arrete sur la borne ;
//    - la chaine vide et la plage vide rendent 0 sans bouger ;
//    - un debut orphelin ne mange pas le caractere suivant ; formes trop longues et demi-codets rendent U+FFFD.
//  Le NEGATIF est dans le meme binaire : la copie exacte de l'ancien decodage passe par les memes mesures, et le
//  banc doit le voir lire apres le zero et avaler l'octet qui suit un debut orphelin.
// =============================================================================

#include "NKFont/NkFont.h"

using namespace nkentseu;

namespace {

	using Decodeur = NkFontCodepoint (*)(const char **, const char *);

	/// L'ANCIEN decodage, copie exacte de NkFontAtlas.cpp avant le 07/11/2026 : le negatif du banc.
	NkFontCodepoint AncienDecodage(const char **str, const char *end) {
		const nkft_uint8 *s = (const nkft_uint8 *)*str;
		const nkft_uint8 *e = (const nkft_uint8 *)end;
		if (!s || (e && s >= e) || !*s)
			return 0;
		NkFontCodepoint cp;
		nkft_uint8 b0 = *s;
		if (b0 < 0x80) {
			cp = b0;
			*str = (const char *)(s + 1);
			return cp;
		} else if ((b0 & 0xE0) == 0xC0) {
			if (e && s + 2 > e) {
				++*str;
				return NKFONT_CODEPOINT_REPLACEMENT;
			}
			cp = ((nkft_uint32)(b0 & 0x1F) << 6) | (s[1] & 0x3F);
			*str = (const char *)(s + 2);
		} else if ((b0 & 0xF0) == 0xE0) {
			if (e && s + 3 > e) {
				++*str;
				return NKFONT_CODEPOINT_REPLACEMENT;
			}
			cp = ((nkft_uint32)(b0 & 0x0F) << 12) | ((nkft_uint32)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
			*str = (const char *)(s + 3);
		} else if ((b0 & 0xF8) == 0xF0) {
			if (e && s + 4 > e) {
				++*str;
				return NKFONT_CODEPOINT_REPLACEMENT;
			}
			cp = ((nkft_uint32)(b0 & 0x07) << 18) | ((nkft_uint32)(s[1] & 0x3F) << 12) |
				 ((nkft_uint32)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
			*str = (const char *)(s + 4);
		} else {
			++*str;
			return NKFONT_CODEPOINT_REPLACEMENT;
		}
		return cp;
	}

	/// Ce qu'une lecture complete a rendu : les points de code (16 au plus) et l'endroit ou le curseur s'est arrete.
	struct Lecture {
			int n = 0;
			NkFontCodepoint cp[16] = {};
			const char *arret = nullptr;
	};

	/// Decode `t` jusqu'au premier 0 rendu (sans borne si `fin` est nul). Un decodage qui ne bouge plus arrete la lecture.
	Lecture Lire(Decodeur d, const char *t, const char *fin) {
		Lecture l;
		const char *p = t;
		while (l.n < 16) {
			const char *avant = p;
			const NkFontCodepoint c = d(&p, fin);
			if (c == 0 || p == avant)
				break;
			l.cp[l.n++] = c;
		}
		l.arret = p;
		return l;
	}

	bool Egal(const Lecture &l, const NkFontCodepoint *attendu, int n) {
		if (l.n != n)
			return false;
		for (int i = 0; i < n; ++i)
			if (l.cp[i] != attendu[i])
				return false;
		return true;
	}

	int Longueur(const char *t) {
		int n = 0;
		while (t[n])
			++n;
		return n;
	}

	constexpr NkFontCodepoint R = NKFONT_CODEPOINT_REPLACEMENT;

	struct Resultats {
			bool accents = false, tronqueZero = false, tronqueBorne = false, vide = false;
			bool orphelin = false, tropLong = false, demiCodet = false, suiteIsolee = false;
	};

	Resultats Mesurer(Decodeur d) {
		Resultats r;
		{ // « Réglages à l'échelle, 5 € et 😀 » : 2, 3 et 4 octets, sans borne puis avec la borne exacte
			const char *t = "R\xC3\xA9glages \xC3\xA0 l'\xC3\xA9"
							"chelle, 5 \xE2\x82\xAC et \xF0\x9F\x98\x80";
			const int L = Longueur(t);
			const NkFontCodepoint debut[] = {'R', 0xE9, 'g', 'l', 'a', 'g', 'e', 's', ' ', 0xE0, ' ', 'l', '\'', 0xE9, 'c', 'h'};
			const Lecture a = Lire(d, t, nullptr); // les 16 premiers
			// la fin, lue a partir de « chelle » : e, l, l, e, virgule, espace, 5, espace, €, espace, e, t, espace, 😀
			const NkFontCodepoint finAttendue[] = {'e', 'l', 'l', 'e', ',', ' ', '5', ' ', 0x20AC, ' ', 'e', 't', ' ', 0x1F600};
			const Lecture b = Lire(d, a.arret, nullptr);
			const Lecture c = Lire(d, a.arret, t + L);
			r.accents = Egal(a, debut, 16) && Egal(b, finAttendue, 14) && b.arret == t + L && Egal(c, finAttendue, 14) && c.arret == t + L;
		}
		{ // coupee par le TERMINATEUR, avec des octets de suite laisses APRES le zero (la memoire que l'ancien lisait)
			const char m2a[] = {'R', '\xC3', '\0', '\xA9', '\xA9', '\xA9', 'X', '\0'};
			const char m2b[] = {'R', '\xC3', '\0', '\x41', '\x42', '\x43', 'Y', '\0'}; // meme chaine, autre memoire apres le zero
			const char m3[] = {'a', '\xE2', '\x82', '\0', '\xAC', '\xAC', '\0'};
			const char m4[] = {'a', '\xF0', '\x9F', '\x98', '\0', '\x80', '\x80', '\0'};
			const NkFontCodepoint att[] = {'R', R}, att34[] = {'a', R};
			const Lecture la = Lire(d, m2a, nullptr), lb = Lire(d, m2b, nullptr);
			const Lecture l3 = Lire(d, m3, nullptr), l4 = Lire(d, m4, nullptr);
			r.tronqueZero = Egal(la, att, 2) && la.arret == m2a + 2 && Egal(lb, att, 2) && lb.arret == m2b + 2 && Egal(l3, att34, 2) &&
							l3.arret == m3 + 3 && Egal(l4, att34, 2) && l4.arret == m4 + 4;
		}
		{ // coupee par la BORNE : le dernier octet du tableau est le debut de « é » (ou les deux premiers de « € »)
			const char b2[2] = {'A', '\xC3'};
			const char b3[2] = {'\xE2', '\x82'};
			const NkFontCodepoint att2[] = {'A', R}, att3[] = {R};
			const Lecture l2 = Lire(d, b2, b2 + 2), l3 = Lire(d, b3, b3 + 2);
			r.tronqueBorne = Egal(l2, att2, 2) && l2.arret == b2 + 2 && Egal(l3, att3, 1) && l3.arret <= b3 + 2;
		}
		{ // la chaine vide, la plage vide, le pointeur nul : 0, sans bouger
			const char *vide = "";
			const char *p = vide;
			const bool a = d(&p, nullptr) == 0 && p == vide;
			const char *t = "A";
			const char *q = t;
			const bool b = d(&q, t) == 0 && q == t;
			const char *nul = nullptr;
			const bool c = d(&nul, nullptr) == 0 && nul == nullptr;
			r.vide = a && b && c;
		}
		{ // un debut orphelin ne mange pas le caractere qui suit
			const NkFontCodepoint att[] = {R, 'A', 'b'};
			r.orphelin = Egal(Lire(d, "\xC3"
									 "Ab",
								   nullptr),
							  att, 3);
		}
		{ // les formes trop longues (« / » ecrit sur 2 et 3 octets) ne deviennent pas « / »
			const Lecture a = Lire(d, "\xC0\xAF", nullptr), b = Lire(d, "\xE0\x80\xAF", nullptr);
			r.tropLong = a.n >= 1 && a.cp[0] == R && (a.n < 2 || a.cp[1] == R) && b.n >= 1 && b.cp[0] == R;
		}
		{ // un demi-codet (D800) n'est pas un caractere
			const Lecture a = Lire(d, "\xED\xA0\x80", nullptr);
			r.demiCodet = a.n == 1 && a.cp[0] == R;
		}
		{ // un octet de suite isole : U+FFFD, puis la lecture reprend
			const NkFontCodepoint att[] = {R, 'A'};
			r.suiteIsolee = Egal(Lire(d, "\x80"
										 "A",
									   nullptr),
								 att, 2);
		}
		return r;
	}

} // namespace

void NkTestsDecodageUtf8(void (*temoin)(bool, const char *)) {
	const Resultats r = Mesurer(&NkFontDecodeUTF8);
	temoin(r.accents, "UTF-8 : une chaine accentuee (2, 3 et 4 octets) se decode caractere par caractere, avec et sans borne");
	temoin(r.tronqueZero, "UTF-8 : une suite coupee par le zero rend U+FFFD et s'arrete SUR le zero (rien n'est lu apres)");
	temoin(r.tronqueBorne, "UTF-8 : une suite coupee par la borne rend U+FFFD et s'arrete sur la borne");
	temoin(r.vide, "UTF-8 : la chaine vide, la plage vide et le pointeur nul rendent 0 sans bouger");
	temoin(r.orphelin, "UTF-8 : un debut orphelin rend U+FFFD et ne mange pas le caractere suivant");
	temoin(r.tropLong && r.demiCodet && r.suiteIsolee, "UTF-8 : formes trop longues, demi-codet et octet de suite isole rendent U+FFFD");
	const Resultats a = Mesurer(&AncienDecodage);
	temoin(a.accents && !a.tronqueZero && !a.orphelin,
		   "NEGATIF : l'ancien decodage lit apres le zero et avale l'octet qui suit un debut orphelin (le banc le voit)");
}
