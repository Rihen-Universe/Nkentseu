// =============================================================================
// NKSpeech/NkG2P.cpp — implémentation du G2P rule-based (voir NkG2P.h pour les
// sources). Tout le matching se fait sur des CODEPOINTS Unicode (uint32), jamais
// sur des littéraux de chaîne non-ASCII : portable quel que soit le charset source
// du compilateur (pas de risque de mauvais encodage MSVC/Clang/GCC).
// Zero-STL, namespace nkentseu::ai.
// =============================================================================
#include "NKSpeech/NkG2P.h"
#include "NKContainers/String/Encoding/NkUTF8.h"

namespace nkentseu {
	namespace ai {

		namespace {

			// -----------------------------------------------------------------------
			// Codepoints Unicode utilisés (constantes nommées — voir NkG2P.h SOURCES).
			// -----------------------------------------------------------------------
			constexpr uint32 kCombAcute = 0x0301;	// tone HIGH
			constexpr uint32 kCombGrave = 0x0300;	// tone LOW
			constexpr uint32 kCombCaron = 0x030C;	// tone RISING
			constexpr uint32 kCombCirc = 0x0302;	// tone FALLING
			constexpr uint32 kCombTilde = 0x0303;	// nasalisation

			constexpr uint32 kSchwaLower = 0x0259, kSchwaUpper = 0x018F;			// ə / Ə
			constexpr uint32 kAlphaLower = 0x0251;									// ɑ (pas de majuscule d'usage courant)
			constexpr uint32 kOpenOLower = 0x0254, kOpenOUpper = 0x0186;			// ɔ / Ɔ
			constexpr uint32 kOpenELower = 0x025B, kOpenEUpper = 0x0190;			// ɛ / Ɛ
			constexpr uint32 kUBarLower = 0x0289, kUBarUpper = 0x0244;				// ʉ / Ʉ
			constexpr uint32 kEngLower = 0x014B, kEngUpper = 0x014A;				// ŋ / Ŋ
			constexpr uint32 kApostropheAscii = 0x0027;							// '
			constexpr uint32 kApostropheModifier = 0x02BC;							// ʼ (glottal stop letter)
			constexpr uint32 kCCedilla = 0x00E7;									// ç

			// -----------------------------------------------------------------------
			// Séparateurs de mots : blancs, ponctuation ASCII/typographique, CHIFFRES
			// (numéros de versets collés aux mots dans le NT numérisé, ex. "1Ŋwa'nyə").
			// L'APOSTROPHE N'EST PAS un séparateur (phonème glottal en bbj).
			// -----------------------------------------------------------------------
			bool IsSeparator(uint32 cp) {
				if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r')
					return true;
				if (cp >= '0' && cp <= '9')
					return true;
				switch (cp) {
					case '.': case ',': case ';': case ':': case '!': case '?':
					case '"': case '(': case ')': case '[': case ']': case '{': case '}':
					case '-': case '/': case '\\':
					case 0x2013: case 0x2014: // – —
					case 0x201C: case 0x201D: // “ ”
					case 0x00AB: case 0x00BB: // « »
						return true;
					default:
						return false;
				}
			}

			// Fold ASCII A-Z -> a-z uniquement (les lettres spéciales bbj sont gérées
			// à part, cf. FoldSpecialUpper). Ne touche pas aux codepoints > 127.
			uint32 FoldAsciiLower(uint32 cp) {
				if (cp >= 'A' && cp <= 'Z')
					return cp - 'A' + 'a';
				return cp;
			}

			// Fold des quelques majuscules spéciales bbj rencontrées dans le corpus réel
			// (Ŋkhənyə, Ɛlɔ...). Renvoie cp inchangé si non concerné.
			uint32 FoldSpecialUpper(uint32 cp) {
				if (cp == kEngUpper) return kEngLower;
				if (cp == kSchwaUpper) return kSchwaLower;
				if (cp == kOpenEUpper) return kOpenELower;
				if (cp == kOpenOUpper) return kOpenOLower;
				if (cp == kUBarUpper) return kUBarLower;
				return cp;
			}

			bool IsCombining(uint32 cp) {
				return cp == kCombAcute || cp == kCombGrave || cp == kCombCaron || cp == kCombCirc || cp == kCombTilde;
			}

			// Applique un diacritique combinant à (tone, nasal) courants.
			void ApplyCombining(uint32 cp, NkTone &tone, bool &nasal) {
				if (cp == kCombAcute) tone = NkTone::High;
				else if (cp == kCombGrave) tone = NkTone::Low;
				else if (cp == kCombCaron) tone = NkTone::Rising;
				else if (cp == kCombCirc) tone = NkTone::Falling;
				else if (cp == kCombTilde) nasal = true;
			}

			// Décode le texte UTF-8 en une suite de codepoints (zero-STL, NkVector<uint32>).
			NkVector<uint32> DecodeToCodepoints(const NkString &text) {
				NkVector<uint32> cps;
				const char *data = text.Data();
				const usize n = (usize)text.Size();
				usize pos = 0;
				while (pos < n) {
					uint32 cp = 0;
					const usize consumed = encoding::utf8::NkDecodeChar(data + pos, cp);
					if (consumed == 0) {
						// Byte invalide isolé : on l'ignore et on avance d'un byte pour ne
						// pas boucler indéfiniment (robustesse — ne devrait pas arriver sur
						// un fichier UTF-8 valide comme le corpus du projet).
						++pos;
						continue;
					}
					cps.PushBack(cp);
					pos += consumed;
				}
				return cps;
			}

			// -----------------------------------------------------------------------
			// Table VOYELLES précomposées latines (Unicode NFC) rencontrées dans le
			// corpus réel (analyse de fréquence des codepoints, cf. NkG2P.h SOURCES [4]) :
			// á à é è í ì ó ò ú ù ã õ + quelques majuscules et circonflexes rares.
			// -----------------------------------------------------------------------
			bool MatchPrecomposedVowel(uint32 cp, NkPhonSym &sym, NkTone &tone, bool &nasal) {
				nasal = false;
				switch (cp) {
					case 0x00E1: case 0x00C1: sym = NkPhonSym::V_a; tone = NkTone::High; return true; // á Á
					case 0x00E0: case 0x00C0: sym = NkPhonSym::V_a; tone = NkTone::Low; return true;	// à À
					case 0x00E2: case 0x00C2: sym = NkPhonSym::V_a; tone = NkTone::Falling; return true; // â Â
					case 0x01CE: case 0x01CD: sym = NkPhonSym::V_a; tone = NkTone::Rising; return true; // ǎ Ǎ
					case 0x00E3: sym = NkPhonSym::V_a; tone = NkTone::Mid; nasal = true; return true;	 // ã
					case 0x00E9: case 0x00C9: sym = NkPhonSym::V_e; tone = NkTone::High; return true;	 // é É
					case 0x00E8: case 0x00C8: sym = NkPhonSym::V_e; tone = NkTone::Low; return true;	 // è È
					case 0x00EA: case 0x00CA: sym = NkPhonSym::V_e; tone = NkTone::Falling; return true; // ê Ê
					case 0x00ED: case 0x00CD: sym = NkPhonSym::V_i; tone = NkTone::High; return true;	 // í Í
					case 0x00EC: case 0x00CC: sym = NkPhonSym::V_i; tone = NkTone::Low; return true;	 // ì Ì
					case 0x00EE: case 0x00CE: sym = NkPhonSym::V_i; tone = NkTone::Falling; return true; // î Î
					case 0x00F3: case 0x00D3: sym = NkPhonSym::V_o; tone = NkTone::High; return true;	 // ó Ó
					case 0x00F2: case 0x00D2: sym = NkPhonSym::V_o; tone = NkTone::Low; return true;	 // ò Ò
					case 0x00F4: case 0x00D4: sym = NkPhonSym::V_o; tone = NkTone::Falling; return true; // ô Ô
					case 0x00F5: sym = NkPhonSym::V_o; tone = NkTone::Mid; nasal = true; return true;	 // õ
					case 0x00FA: case 0x00DA: sym = NkPhonSym::V_u; tone = NkTone::High; return true;	 // ú Ú
					case 0x00F9: case 0x00D9: sym = NkPhonSym::V_u; tone = NkTone::Low; return true;	 // ù Ù
					case 0x00FB: case 0x00DB: sym = NkPhonSym::V_u; tone = NkTone::Falling; return true; // û Û
					default: return false;
				}
			}

			// Base voyelle "nue" (spéciale bbj ou ASCII), AVANT application des
			// diacritiques combinants qui suivent éventuellement.
			bool MatchBareVowelBase(uint32 cpFolded, NkPhonSym &sym) {
				switch (cpFolded) {
					case 'a': sym = NkPhonSym::V_a; return true;
					case 'e': sym = NkPhonSym::V_e; return true;
					case 'i': sym = NkPhonSym::V_i; return true;
					case 'o': sym = NkPhonSym::V_o; return true;
					case 'u': sym = NkPhonSym::V_u; return true;
					default: break;
				}
				if (cpFolded == kAlphaLower) { sym = NkPhonSym::V_aa; return true; }
				if (cpFolded == kOpenELower) { sym = NkPhonSym::V_eh; return true; }
				if (cpFolded == kOpenOLower) { sym = NkPhonSym::V_oh; return true; }
				if (cpFolded == kSchwaLower) { sym = NkPhonSym::V_eu; return true; }
				if (cpFolded == kUBarLower) { sym = NkPhonSym::V_uu; return true; }
				return false;
			}

			// Digraphes consonantiques ASCII (2 codepoints déjà foldés en minuscule).
			// Plus long d'abord n'est pas nécessaire ici (tous les digraphes utiles
			// font 2 caractères ASCII) — mais DOIT être essayé avant les lettres seules.
			bool MatchConsonantDigraph(uint32 c0, uint32 c1, NkPhonSym &sym) {
				if (c0 == 'g' && c1 == 'h') { sym = NkPhonSym::C_gh; return true; }	// ɣ
				if (c0 == 'z' && c1 == 'h') { sym = NkPhonSym::C_zh; return true; }	// ʒ
				if (c0 == 's' && c1 == 'h') { sym = NkPhonSym::C_sh; return true; }	// ʃ
				if (c0 == 't' && c1 == 's') { sym = NkPhonSym::C_ts; return true; }	// t͡s
				if (c0 == 'd' && c1 == 'z') { sym = NkPhonSym::C_dz; return true; }	// d͡z
				if (c0 == 'b' && c1 == 'v') { sym = NkPhonSym::C_bv; return true; }	// b͡v
				if (c0 == 'p' && c1 == 'f') { sym = NkPhonSym::C_pf; return true; }	// p͡f
				// Affrication p/b/t/d/k + h -> [pɸ bβ tθ dð kx] (source [1], Wikipedia EN).
				if (c0 == 'p' && c1 == 'h') { sym = NkPhonSym::C_ph_affr; return true; }
				if (c0 == 'b' && c1 == 'h') { sym = NkPhonSym::C_bh_affr; return true; }
				if (c0 == 't' && c1 == 'h') { sym = NkPhonSym::C_th_affr; return true; }
				if (c0 == 'd' && c1 == 'h') { sym = NkPhonSym::C_dh_affr; return true; }
				if (c0 == 'k' && c1 == 'h') { sym = NkPhonSym::C_kh_affr; return true; }
				return false;
			}

			// Consonne simple (lettre ASCII foldée, ou ŋ). Voir NkG2P.h SOURCES pour les
			// graphèmes "c"->/tʃ/, "j"->/dʒ/, "y"->/j/ (déduits, non une citation directe).
			bool MatchConsonantSingle(uint32 cpFolded, NkPhonSym &sym) {
				switch (cpFolded) {
					case 'p': sym = NkPhonSym::C_p; return true;
					case 'b': sym = NkPhonSym::C_b; return true;
					case 't': sym = NkPhonSym::C_t; return true;
					case 'd': sym = NkPhonSym::C_d; return true;
					case 'k': sym = NkPhonSym::C_k; return true;
					case 'g': sym = NkPhonSym::C_g; return true;
					case 'f': sym = NkPhonSym::C_f; return true;
					case 's': sym = NkPhonSym::C_s; return true;
					case 'h': sym = NkPhonSym::C_h; return true;
					case 'm': sym = NkPhonSym::C_m; return true;
					case 'n': sym = NkPhonSym::C_n; return true;
					case 'l': sym = NkPhonSym::C_l; return true;
					case 'w': sym = NkPhonSym::C_w; return true;
					case 'y': sym = NkPhonSym::C_y; return true;	// glide /j/ (Yeso, Yohane...)
					case 'c': sym = NkPhonSym::C_tsh; return true; // /tʃ/ (Cyəpɔ, cwə...)
					case 'j': sym = NkPhonSym::C_dzh; return true; // /dʒ/ (jyə, jap...)
					case 'v': sym = NkPhonSym::C_v; return true;	// allophone (source [1])
					case 'z': sym = NkPhonSym::C_z; return true;	// allophone (source [1])
					case 'r': sym = NkPhonSym::C_r; return true;	// emprunts/noms propres, non natif
					default: break;
				}
				if (cpFolded == kEngLower) { sym = NkPhonSym::C_ng; return true; } // ŋ
				return false;
			}

			// -----------------------------------------------------------------------
			// Helpers PARTAGÉS fr/en (frontières de mot) — voir NkG2P.h SOURCES [6]-[9]
			// pour les règles françaises/anglaises qui les utilisent (e muet, liaison,
			// magic e, th voisé/non voisé...).
			// -----------------------------------------------------------------------

			// Frontière de "mot" pour fr/en : fin de chaîne, séparateur générique, OU
			// apostrophe (élision fr "l'ami" / contraction en "don't" -- l'apostrophe
			// EST une frontière ici, contrairement au bbj où c'est un phonème).
			bool NkIsFrEnWordBoundary(uint32 cp) {
				return IsSeparator(cp) || cp == kApostropheAscii;
			}

			bool NkAtWordEnd(const NkVector<uint32> &cps, usize idx) {
				return idx >= cps.Size() || NkIsFrEnWordBoundary(cps[(nk_size)idx]);
			}

			// Indice (exclusif) de fin du mot courant à partir de `idx` (qui pointe
			// DANS le mot, pas sur un séparateur).
			usize NkWordEndIndex(const NkVector<uint32> &cps, usize idx) {
				const usize n = cps.Size();
				usize j = idx;
				while (j < n && !NkIsFrEnWordBoundary(cps[(nk_size)j]))
					++j;
				return j;
			}

			bool NkIsAsciiVowelLetter(uint32 cFolded) {
				return cFolded == 'a' || cFolded == 'e' || cFolded == 'i' || cFolded == 'o' || cFolded == 'u' ||
					   cFolded == 'y';
			}

			// Compare cps[start,end) (repliage ASCII) à la chaîne C `ascii` (ASCII pur).
			bool NkRangeEqualsAscii(const NkVector<uint32> &cps, usize start, usize end, const char *ascii) {
				usize len = 0;
				while (ascii[len])
					++len;
				if (end - start != len)
					return false;
				for (usize k = 0; k < len; ++k)
					if (FoldAsciiLower(cps[(nk_size)(start + k)]) != (uint32)(unsigned char)ascii[k])
						return false;
				return true;
			}

			// --- Spécifique français : liaison + accents -----------------------------

			// Voyelle ou 'h' (le 'h' français est presque toujours muet -> se comporte
			// comme une voyelle pour la liaison ; l'opposition h muet/h aspiré est une
			// propriété LEXICALE non déductible de la graphie seule -- limite documentée,
			// cf. NkG2P.h SOURCES [7], section « H aspiré »).
			bool NkFrStartsVowelOrH(uint32 cp) {
				const uint32 f = FoldAsciiLower(cp);
				if (NkIsAsciiVowelLetter(f) || f == 'h')
					return true;
				switch (cp) {
					case 0x00E9: case 0x00C9: case 0x00E8: case 0x00C8: case 0x00EA: case 0x00CA:
					case 0x00E0: case 0x00C0: case 0x00F4: case 0x00D4: case 0x00FB: case 0x00DB:
					case 0x00F9: case 0x00D9: case 0x00EE: case 0x00CE:
						return true;
					default:
						return false;
				}
			}

			// Première lettre du mot suivant à partir de l'indice `idx` (qui pointe sur
			// un séparateur) -- pour la décision de liaison. 0 si fin de texte.
			uint32 NkFrPeekNextWordFirstLetter(const NkVector<uint32> &cps, usize idx) {
				const usize n = cps.Size();
				usize j = idx;
				while (j < n && NkIsFrEnWordBoundary(cps[(nk_size)j]))
					++j;
				return (j < n) ? cps[(nk_size)j] : 0u;
			}

		} // namespace

		// =========================================================================
		// bbj (ghomala') — implémentation principale.
		// =========================================================================
		NkVector<NkPhonemeUnit> NkG2P::ToPhonemesBbj(const NkString &text) {
			NkVector<NkPhonemeUnit> out;
			NkVector<uint32> cps = DecodeToCodepoints(text);
			const usize n = cps.Size();

			usize i = 0;
			bool wordOpen = false;
			while (i < n) {
				const uint32 raw = cps[(nk_size)i];

				if (IsSeparator(raw)) {
					if (wordOpen) {
						out.PushBack(NkPhonemeUnit(NkPhonSym::Sil));
						wordOpen = false;
					}
					++i;
					continue;
				}
				wordOpen = true;

				// Occlusive glottale (apostrophe — jamais un digramme/voyelle).
				if (raw == kApostropheAscii || raw == kApostropheModifier) {
					out.PushBack(NkPhonemeUnit(NkPhonSym::C_glottal));
					++i;
					continue;
				}
				// ç -> /s/ (cf. NkG2P.h SOURCES, graphème rare non-bbj dans ce fichier NT).
				if (raw == kCCedilla) {
					out.PushBack(NkPhonemeUnit(NkPhonSym::C_s));
					++i;
					continue;
				}

				// 1) Voyelle précomposée (á, è, õ, ...) : 1 codepoint -> (base, ton, nasal).
				{
					NkPhonSym vs;
					NkTone vt = NkTone::Mid;
					bool vn = false;
					if (MatchPrecomposedVowel(raw, vs, vt, vn)) {
						// Diacritique combinant supplémentaire éventuel (rare, défensif).
						usize j = i + 1;
						while (j < n && IsCombining(cps[(nk_size)j])) {
							ApplyCombining(cps[(nk_size)j], vt, vn);
							++j;
						}
						out.PushBack(NkPhonemeUnit(vs, vt, vn));
						i = j;
						continue;
					}
				}

				const uint32 folded = FoldSpecialUpper(FoldAsciiLower(raw));

				// 2) Voyelle nue (spéciale bbj ə ɑ ɔ ɛ ʉ, ou ASCII a e i o u) + diacritiques
				//    combinants qui suivent (ton + nasalisation). C'est le cas DOMINANT du
				//    corpus réel (combining caron seul = 9350 occurrences, cf. SOURCES [4]).
				{
					NkPhonSym vs;
					if (MatchBareVowelBase(folded, vs)) {
						NkTone vt = NkTone::Mid; // non marqué = ton moyen (source [1][2]).
						bool vn = false;
						usize j = i + 1;
						while (j < n && IsCombining(cps[(nk_size)j])) {
							ApplyCombining(cps[(nk_size)j], vt, vn);
							++j;
						}
						out.PushBack(NkPhonemeUnit(vs, vt, vn));
						i = j;
						continue;
					}
				}

				// 3) Digraphe consonantique ASCII (gh/zh/sh/ts/dz/bv/pf/ph/bh/th/dh/kh).
				//    DOIT être tenté avant la consonne simple (plus long d'abord).
				if (i + 1 < n) {
					const uint32 c1 = FoldAsciiLower(cps[(nk_size)(i + 1)]);
					NkPhonSym ds;
					if (MatchConsonantDigraph(folded, c1, ds)) {
						out.PushBack(NkPhonemeUnit(ds));
						i += 2;
						continue;
					}
				}

				// 4) Consonne simple restante (table).
				{
					NkPhonSym cs;
					if (MatchConsonantSingle(folded, cs)) {
						out.PushBack(NkPhonemeUnit(cs));
						++i;
						continue;
					}
				}

				// 5) Inconnu (contamination du fichier source, ex. lettres latines "x"/"q"
				//    issues d'un en-tête en portugais dans le NT numérisé — cf. NkG2P.h).
				//    Ignoré silencieusement : ne casse pas le pipeline, documenté honnêtement.
				++i;
			}

			return out;
		}

		// =========================================================================
		// fr — règles orthographiques SOURCÉES (voir NkG2P.h SOURCES [6][7]) : e muet
		// final (+ "-es"), voyelles nasales étendues, digraphe -ill-, liaison
		// simplifiée, accents. Non tonal, non exhaustif (limites documentées inline
		// et dans NkG2P.h). Priorité : e muet > nasales > digraphes > -ill- >
		// consonnes finales muettes/liaison > lettres seules.
		// =========================================================================
		NkVector<NkPhonemeUnit> NkG2P::ToPhonemesFr(const NkString &text) {
			NkVector<NkPhonemeUnit> out;
			NkVector<uint32> cps = DecodeToCodepoints(text);
			const usize n = cps.Size();

			auto push = [&out](NkPhonSym s, bool nasal = false) { out.PushBack(NkPhonemeUnit(s, NkTone::None, nasal)); };

			usize i = 0;
			bool wordOpen = false;
			usize wordStart = 0;
			NkPhonSym pendingLiaison = NkPhonSym::Sil;
			bool hasLiaison = false;

			while (i < n) {
				const uint32 raw = cps[(nk_size)i];
				if (NkIsFrEnWordBoundary(raw)) { // séparateur générique OU apostrophe (élision l', d'...)
					if (wordOpen) {
						// Liaison (source [7]) : un mot qui se termine par une consonne
						// normalement muette (marquée ci-dessous par la règle des
						// consonnes finales, ou une nasale finale en -n) SE PRONONCE
						// devant un mot suivant commençant par une voyelle (ou un 'h',
						// presque toujours muet en français). Simplification ASSUMÉE :
						// pas de distinction liaison obligatoire/facultative/interdite
						// (dépend de la syntaxe, hors de portée d'un G2P sans analyse
						// grammaticale) ; h muet/aspiré non distingué (lexical, cf. [7]).
						const uint32 nextFirst = NkFrPeekNextWordFirstLetter(cps, i);
						if (hasLiaison && nextFirst != 0 && NkFrStartsVowelOrH(nextFirst))
							push(pendingLiaison);
						else
							push(NkPhonSym::Sil);
						wordOpen = false;
						hasLiaison = false;
					}
					++i;
					continue;
				}
				if (!wordOpen) {
					wordStart = i;
					hasLiaison = false;
				}
				wordOpen = true;
				const uint32 c0 = FoldAsciiLower(raw);
				const uint32 c1raw = (i + 1 < n) ? cps[(nk_size)(i + 1)] : 0; // non replié (accents non-ASCII)
				const uint32 c1 = FoldAsciiLower(c1raw);
				const uint32 c2 = (i + 2 < n) ? FoldAsciiLower(cps[(nk_size)(i + 2)]) : 0;
				const uint32 c3 = (i + 3 < n) ? FoldAsciiLower(cps[(nk_size)(i + 3)]) : 0;

				// "où" : le SEUL mot courant avec ù, accent purement distinctif
				// (vs "ou" = "or") -> une seule voyelle /u/ (source [7]/orthographe
				// standard). Cas spécial car o+ù = 1 phonème sur 2 codepoints.
				if (c0 == 'o' && (c1raw == 0x00F9 || c1raw == 0x00D9)) { push(NkPhonSym::V_u); i += 2; continue; }

				// ---- e muet final / "-es" final (source [6], "Schwa (e caduc) deletion")
				// Règle : le "e" (et le "s" du pluriel/2e pers.) final non accentué est
				// MUET, SAUF dans les mots grammaticaux monosyllabiques qui gardent le
				// schwa (le, je, de, ce, me, te, se, ne, que). ⚠️ Limite documentée : la
				// suppression du schwa MÉDIAN après une consonne unique en syllabe non
				// accentuée (ex. "appeler" -> [aple], source [6] "medial syllables")
				// N'EST PAS implémentée (nécessiterait un découpage syllabique) — seul
				// le e final est traité ici.
				if (c0 == 'e') {
					const bool finalE = NkAtWordEnd(cps, i + 1);
					const bool finalEs = (c1 == 's') && NkAtWordEnd(cps, i + 2);
					if (finalE || finalEs) {
						static const char *kKeepSchwa[] = {"l", "j", "d", "c", "m", "t", "s", "n", "qu"};
						bool keep = false;
						for (int k = 0; k < 9; ++k)
							if (NkRangeEqualsAscii(cps, wordStart, i, kKeepSchwa[k])) { keep = true; break; }
						if (!keep) {
							i += finalEs ? 2 : 1;
							continue; // e (et 's') muets : rien émis
						}
						// mot monosyllabique -> tombe dans le traitement normal (schwa) plus bas.
					}
				}

				// ---- voyelles nasales étendues (source [6], "Nasal vowels") --------
				// Bloquées si la nasale est DOUBLÉE (nn/mm) ou suivie d'une VOYELLE
				// (ex. "inutile" non nasal vs "intact" nasal ; "inné" bloqué par [nn]
				// — exemple cité par la source [6]). Marque une liaison en -n pour les
				// finales en n (source [7], exemple cité "un ami" -> /œ̃.n‿a.mi/).
				{
					const bool blockedByNext2 = (c1 == 'n' || c1 == 'm') && (c2 == c1 || NkIsAsciiVowelLetter(c2));
					// ain/aim/ein (3 lettres) : voyelle + i + n/m.
					const bool blockedByNext3 = (c3 == c2 || NkIsAsciiVowelLetter(c3));
					if ((c0 == 'a' || c0 == 'e') && c1 == 'i' && (c2 == 'n' || c2 == 'm') && !blockedByNext3) {
						push(NkPhonSym::V_eh, true); // ɛ̃
						if (c2 == 'n' && NkAtWordEnd(cps, i + 3)) { pendingLiaison = NkPhonSym::C_n; hasLiaison = true; }
						i += 3; continue;
					}
					if (!blockedByNext2) {
						if ((c0 == 'a' || c0 == 'e') && c1 == 'n') { // an/en -> ɑ̃
							push(NkPhonSym::V_aa, true);
							if (NkAtWordEnd(cps, i + 2)) { pendingLiaison = NkPhonSym::C_n; hasLiaison = true; }
							i += 2; continue;
						}
						if ((c0 == 'a' || c0 == 'e') && c1 == 'm') { push(NkPhonSym::V_aa, true); i += 2; continue; } // am/em -> ɑ̃
						if (c0 == 'o' && c1 == 'n') { // on -> ɔ̃
							push(NkPhonSym::V_oh, true);
							if (NkAtWordEnd(cps, i + 2)) { pendingLiaison = NkPhonSym::C_n; hasLiaison = true; }
							i += 2; continue;
						}
						if (c0 == 'o' && c1 == 'm') { push(NkPhonSym::V_oh, true); i += 2; continue; } // om -> ɔ̃
						if (c0 == 'i' && c1 == 'n') { // in -> ɛ̃
							push(NkPhonSym::V_eh, true);
							if (NkAtWordEnd(cps, i + 2)) { pendingLiaison = NkPhonSym::C_n; hasLiaison = true; }
							i += 2; continue;
						}
						if (c0 == 'i' && c1 == 'm') { push(NkPhonSym::V_eh, true); i += 2; continue; } // im -> ɛ̃
						if (c0 == 'u' && c1 == 'n') { // un -> œ̃ (approx ə̃, symbole partagé)
							push(NkPhonSym::V_eu, true);
							if (NkAtWordEnd(cps, i + 2)) { pendingLiaison = NkPhonSym::C_n; hasLiaison = true; }
							i += 2; continue;
						}
						if (c0 == 'u' && c1 == 'm') { push(NkPhonSym::V_eu, true); i += 2; continue; } // um -> œ̃
					}
				}

				// Digraphes/trigraphes vocaliques.
				if (c0 == 'e' && c1 == 'a' && c2 == 'u') { push(NkPhonSym::V_o); i += 3; continue; }	// eau -> o
				if (c0 == 'a' && c1 == 'u') { push(NkPhonSym::V_o); i += 2; continue; }				// au -> o
				if (c0 == 'e' && c1 == 'u') { push(NkPhonSym::V_eu); i += 2; continue; }				// eu -> ə/œ (approx)
				if (c0 == 'o' && c1 == 'u') { push(NkPhonSym::V_u); i += 2; continue; }				// ou -> u
				if (c0 == 'o' && c1 == 'i') { push(NkPhonSym::C_w); push(NkPhonSym::V_a); i += 2; continue; } // oi -> wa
				if (c0 == 'a' && c1 == 'i') { push(NkPhonSym::V_eh); i += 2; continue; }				// ai -> ɛ
				if (c0 == 'e' && c1 == 'i') { push(NkPhonSym::V_eh); i += 2; continue; }				// ei -> ɛ

				// Digraphes consonantiques.
				if (c0 == 'c' && c1 == 'h') { push(NkPhonSym::C_sh); i += 2; continue; }				// ch -> ʃ
				if (c0 == 'g' && c1 == 'n') { push(NkPhonSym::C_n); i += 2; continue; }				// gn -> ɲ (approx n)
				if (c0 == 'q' && c1 == 'u') { push(NkPhonSym::C_k); i += 2; continue; }				// qu -> k
				if (c0 == 'p' && c1 == 'h') { push(NkPhonSym::C_f); i += 2; continue; }				// ph -> f

				// ---- digraphe -ill- (source [7bis], règle usuelle + exceptions) -----
				// "ill" = glide /j/ ; après une CONSONNE, un /i/ s'entend aussi avant le
				// glide (ex. "fille" /fij/) ; après une VOYELLE, seul le glide reste
				// (ex. "grenouille" /ɡʁənuj/). EXCEPTIONS lexicales bien connues où
				// "ill" = /il/ (l "normal", pas de glide) : ville, mille, tranquille.
				if (c0 == 'i' && c1 == 'l' && c2 == 'l') {
					const bool exception = NkRangeEqualsAscii(cps, wordStart, i, "v") ||
											NkRangeEqualsAscii(cps, wordStart, i, "m") ||
											NkRangeEqualsAscii(cps, wordStart, i, "tranqu");
					if (exception) {
						push(NkPhonSym::V_i);
						push(NkPhonSym::C_l);
					} else {
						const bool afterVowel = (i > wordStart) && NkIsAsciiVowelLetter(FoldAsciiLower(cps[(nk_size)(i - 1)]));
						if (!afterVowel)
							push(NkPhonSym::V_i);
						push(NkPhonSym::C_y);
					}
					i += 3;
					continue;
				}

				// ---- consonnes finales muettes + liaison (source [7], règle
				// mnémonique "CaReFuL" : c/f/l/r restent prononcées, les autres
				// consonnes finales sont en général muettes) ------------------------
				if (NkAtWordEnd(cps, i + 1)) {
					NkPhonSym liaisonSym = NkPhonSym::Sil;
					bool isSilentFinal = true;
					switch (c0) { // table de correspondance liaison, source [7]
						case 's': case 'x': case 'z': liaisonSym = NkPhonSym::C_z; break;
						case 't': case 'd': liaisonSym = NkPhonSym::C_t; break;
						case 'p': liaisonSym = NkPhonSym::C_p; break;
						case 'g': liaisonSym = NkPhonSym::C_k; break;
						default: isSilentFinal = false; break;
					}
					if (isSilentFinal) {
						pendingLiaison = liaisonSym;
						hasLiaison = true;
						++i;
						continue; // consonne finale muette (sauf liaison) : rien émis ici
					}
				}

				// Lettres seules.
				switch (c0) {
					case 'a': push(NkPhonSym::V_a); break;
					case 'e': push(NkPhonSym::V_eu); break; // e non final : schwa (approx par défaut)
					case 'i': push(NkPhonSym::V_i); break;
					case 'o': push(NkPhonSym::V_o); break;
					case 'u': push(NkPhonSym::V_y); break; // "u" français = /y/ (cf. NkVoiceSynth)
					case 'y': push(NkPhonSym::V_i); break;
					case 'p': push(NkPhonSym::C_p); break;
					case 'b': push(NkPhonSym::C_b); break;
					case 't': push(NkPhonSym::C_t); break;
					case 'd': push(NkPhonSym::C_d); break;
					case 'k': push(NkPhonSym::C_k); break;
					case 'g': push(NkPhonSym::C_g); break;
					case 'f': push(NkPhonSym::C_f); break;
					case 'v': push(NkPhonSym::C_v); break;
					case 's': push(NkPhonSym::C_s); break; // ⚠️ approx : "s" intervocalique -> /z/ non géré (limite documentée)
					case 'z': push(NkPhonSym::C_z); break;
					case 'j': push(NkPhonSym::C_zh); break; // "j" français = ʒ
					case 'm': push(NkPhonSym::C_m); break;
					case 'n': push(NkPhonSym::C_n); break;
					case 'l': push(NkPhonSym::C_l); break;
					case 'r': push(NkPhonSym::C_r); break;
					case 'h': break; // 'h' muet en français : ignoré
					case 0x00E9: push(NkPhonSym::V_e); break;	 // é
					case 0x00E8: case 0x00EA: push(NkPhonSym::V_eh); break; // è ê
					case 0x00E0: push(NkPhonSym::V_a); break;	 // à
					case 0x00E7: push(NkPhonSym::C_s); break;	 // ç
					case 0x00F4: push(NkPhonSym::V_o); break;	 // ô
					case 0x00FB: push(NkPhonSym::V_y); break;	 // û (ex. "sûr" /syʁ/)
					case 0x00F9: push(NkPhonSym::V_u); break;	 // ù isolé (normalement fusionné avec "o", cf. plus haut)
					case 0x00EE: push(NkPhonSym::V_i); break;	 // î
					default: break; // inconnu : ignoré
				}
				++i;
			}
			return out;
		}

		// =========================================================================
		// en — règles orthographiques SOURCÉES (voir NkG2P.h SOURCES [8][9][10][11]) :
		// "magic e" (voyelle allongée + e final muet), "th" voisé/non voisé selon les
		// mots grammaticaux, c/g mous vs durs, wh+o -> /h/. L'anglais reste notoirement
		// irrégulier (nombreuses exceptions lexicales, ex. have/give/love pour le
		// "magic e" — NON gérées, cf. limites documentées inline) — PAS un
		// remplaçant d'un dictionnaire type CMUdict.
		// =========================================================================
		NkVector<NkPhonemeUnit> NkG2P::ToPhonemesEn(const NkString &text) {
			NkVector<NkPhonemeUnit> out;
			NkVector<uint32> cps = DecodeToCodepoints(text);
			const usize n = cps.Size();

			auto push = [&out](NkPhonSym s) { out.PushBack(NkPhonemeUnit(s)); };

			usize i = 0;
			bool wordOpen = false;
			usize wordStart = 0;
			while (i < n) {
				const uint32 raw = cps[(nk_size)i];
				if (NkIsFrEnWordBoundary(raw)) { // apostrophe = contraction en anglais (don't, it's)
					if (wordOpen) { push(NkPhonSym::Sil); wordOpen = false; }
					++i;
					continue;
				}
				if (!wordOpen)
					wordStart = i;
				wordOpen = true;
				const usize wordEnd = NkWordEndIndex(cps, i);
				const uint32 c0 = FoldAsciiLower(raw);
				const uint32 c1 = (i + 1 < n) ? FoldAsciiLower(cps[(nk_size)(i + 1)]) : 0;
				const uint32 c2 = (i + 2 < n) ? FoldAsciiLower(cps[(nk_size)(i + 2)]) : 0;

				// ---- "magic e" (source [8], "Silent e") : Voyelle-Consonne-e final ->
				// la voyelle se dit "longue" (son de son NOM) et le e final est muet.
				// Ex. cake/eɪ, bike/aɪ, note/oʊ, cute/juː. Le jeu de symboles n'a pas de
				// diphtongues dédiées -> approximation par la monophtongue disponible la
				// plus proche (limite documentée, même esprit que le fr "eu -> approx").
				if (NkIsAsciiVowelLetter(c0) && c1 != 0 && !NkIsAsciiVowelLetter(c1) && c2 == 'e' &&
					(i + 3) == wordEnd) {
					NkPhonSym longSym;
					switch (c0) {
						case 'a': longSym = NkPhonSym::V_e; break;	// a_e -> /eɪ/ (approx e), ex. cake
						case 'e': longSym = NkPhonSym::V_i; break;	// e_e -> /iː/ (approx i), ex. mete
						case 'i': longSym = NkPhonSym::V_a; break;	// i_e -> /aɪ/ (approx a), ex. bike
						case 'o': longSym = NkPhonSym::V_o; break;	// o_e -> /oʊ/ (approx o), ex. note
						default: longSym = NkPhonSym::V_uu; break;	// u_e -> /juː/ (approx ʉ, /j/ initial perdu), ex. cute
					}
					push(longSym);
					++i;
					continue;
				}
				// e final muet du "magic e" ci-dessus : repéré en arrière (voyelle puis
				// UNE consonne juste avant ce e) pour ne pas dupliquer la condition à 3
				// caractères de vue.
				if (c0 == 'e' && (i + 1) == wordEnd && i >= wordStart + 2) {
					const uint32 prev1 = FoldAsciiLower(cps[(nk_size)(i - 1)]);
					const uint32 prev2 = FoldAsciiLower(cps[(nk_size)(i - 2)]);
					if (!NkIsAsciiVowelLetter(prev1) && NkIsAsciiVowelLetter(prev2)) {
						++i;
						continue; // e muet ("magic e") : rien émis
					}
				}

				// ---- "th" voisé /ð/ dans un petit ensemble de mots grammaticaux
				// fréquents (source [9]) ; non voisé /θ/ par défaut ailleurs (majorité
				// des mots, y compris tous les mots nouveaux — source [9]).
				if (c0 == 't' && c1 == 'h') {
					static const char *kVoicedTh[] = {"the", "this", "that", "these", "those", "they",
													   "them", "their", "there", "then", "than", "thus", "though"};
					bool voiced = false;
					for (int k = 0; k < 13; ++k)
						if (NkRangeEqualsAscii(cps, wordStart, wordEnd, kVoicedTh[k])) { voiced = true; break; }
					push(voiced ? NkPhonSym::C_dentalDh : NkPhonSym::C_dentalTh);
					i += 2; continue;
				}
				if (c0 == 's' && c1 == 'h') { push(NkPhonSym::C_sh); i += 2; continue; }
				if (c0 == 'c' && c1 == 'h') { push(NkPhonSym::C_tsh); i += 2; continue; }
				if (c0 == 'p' && c1 == 'h') { push(NkPhonSym::C_f); i += 2; continue; }
				// "wh" + 'o' -> /h/ (who, whole, whom, whose, wholly) ; /w/ sinon
				// (which, what, when...) — source [10], "English orthography".
				if (c0 == 'w' && c1 == 'h') { push(c2 == 'o' ? NkPhonSym::C_h : NkPhonSym::C_w); i += 2; continue; }
				if (c0 == 'n' && c1 == 'g') { push(NkPhonSym::C_ng); i += 2; continue; }
				if (c0 == 'q' && c1 == 'u') { push(NkPhonSym::C_k); push(NkPhonSym::C_w); i += 2; continue; }

				// Digraphes vocaliques usuels (approximation grossière — l'anglais a
				// beaucoup de diphtongues que ce jeu de symboles ne rend pas fidèlement).
				if (c0 == 'e' && c1 == 'e') { push(NkPhonSym::V_i); i += 2; continue; }	// ee -> i:
				if (c0 == 'o' && c1 == 'o') { push(NkPhonSym::V_u); i += 2; continue; }	// oo -> u:
				if (c0 == 'a' && c1 == 'y') { push(NkPhonSym::V_e); i += 2; continue; }	// ay -> eɪ (approx e)
				if (c0 == 'o' && c1 == 'w') { push(NkPhonSym::V_o); i += 2; continue; }	// ow -> oʊ (approx o)
				if (c0 == 'o' && c1 == 'u') { push(NkPhonSym::V_aa); i += 2; continue; }	// ou -> aʊ (approx ɑ)

				// ---- c/g mous vs durs (source [11], "Hard and soft C" + pendant "G") :
				// "soft" /s/ ou /dʒ/ avant e/i/y, "hard" /k/ ou /ɡ/ ailleurs. Exceptions
				// lexicales connues NON gérées ici (get/give/girl/gift pour g ; soccer/
				// Celtic pour c) -- limite documentée, cf. source [11].
				if (c0 == 'c') {
					push((c1 == 'e' || c1 == 'i' || c1 == 'y') ? NkPhonSym::C_s : NkPhonSym::C_k);
					++i; continue;
				}
				if (c0 == 'g' && (c1 == 'e' || c1 == 'i' || c1 == 'y')) { push(NkPhonSym::C_dzh); ++i; continue; }

				switch (c0) {
					case 'a': push(NkPhonSym::V_eh); break; // approx (cat) — très grossier
					case 'e': push(NkPhonSym::V_eh); break;
					case 'i': push(NkPhonSym::V_i); break;
					case 'o': push(NkPhonSym::V_oh); break;
					case 'u': push(NkPhonSym::V_u); break;
					case 'y': push(NkPhonSym::V_i); break;
					case 'p': push(NkPhonSym::C_p); break;
					case 'b': push(NkPhonSym::C_b); break;
					case 't': push(NkPhonSym::C_t); break;
					case 'd': push(NkPhonSym::C_d); break;
					case 'k': push(NkPhonSym::C_k); break;
					case 'g': push(NkPhonSym::C_g); break;
					case 'f': push(NkPhonSym::C_f); break;
					case 'v': push(NkPhonSym::C_v); break;
					case 's': push(NkPhonSym::C_s); break;
					case 'z': push(NkPhonSym::C_z); break;
					case 'j': push(NkPhonSym::C_dzh); break;
					case 'm': push(NkPhonSym::C_m); break;
					case 'n': push(NkPhonSym::C_n); break;
					case 'l': push(NkPhonSym::C_l); break;
					case 'r': push(NkPhonSym::C_r); break;
					case 'w': push(NkPhonSym::C_w); break;
					case 'h': push(NkPhonSym::C_h); break;
					case 'x': push(NkPhonSym::C_k); push(NkPhonSym::C_s); break;
					case 'q': push(NkPhonSym::C_k); break;
					default: break;
				}
				++i;
			}
			return out;
		}

		NkVector<NkPhonemeUnit> NkG2P::ToPhonemes(const NkString &text, NkG2PLang lang) {
			switch (lang) {
				case NkG2PLang::Bbj: return ToPhonemesBbj(text);
				case NkG2PLang::En: return ToPhonemesEn(text);
				case NkG2PLang::Fr:
				default: return ToPhonemesFr(text);
			}
		}

		const char *NkG2P::SymbolLabel(NkPhonSym s) {
			switch (s) {
				case NkPhonSym::Sil: return "_";
				case NkPhonSym::V_a: return "a";
				case NkPhonSym::V_aa: return "aa";
				case NkPhonSym::V_e: return "e";
				case NkPhonSym::V_eh: return "eh";
				case NkPhonSym::V_eu: return "eu";
				case NkPhonSym::V_i: return "i";
				case NkPhonSym::V_o: return "o";
				case NkPhonSym::V_oh: return "oh";
				case NkPhonSym::V_u: return "u";
				case NkPhonSym::V_uu: return "uu";
				case NkPhonSym::V_y: return "y";
				case NkPhonSym::C_p: return "p";
				case NkPhonSym::C_b: return "b";
				case NkPhonSym::C_t: return "t";
				case NkPhonSym::C_d: return "d";
				case NkPhonSym::C_k: return "k";
				case NkPhonSym::C_g: return "g";
				case NkPhonSym::C_f: return "f";
				case NkPhonSym::C_s: return "s";
				case NkPhonSym::C_h: return "h";
				case NkPhonSym::C_zh: return "zh";
				case NkPhonSym::C_sh: return "sh";
				case NkPhonSym::C_gh: return "gh";
				case NkPhonSym::C_v: return "v";
				case NkPhonSym::C_z: return "z";
				case NkPhonSym::C_dentalTh: return "th0"; // θ
				case NkPhonSym::C_dentalDh: return "dh0"; // ð
				case NkPhonSym::C_pf: return "pf";
				case NkPhonSym::C_ts: return "ts";
				case NkPhonSym::C_dz: return "dz";
				case NkPhonSym::C_bv: return "bv";
				case NkPhonSym::C_tsh: return "tsh";
				case NkPhonSym::C_dzh: return "dzh";
				case NkPhonSym::C_ph_affr: return "ph^";
				case NkPhonSym::C_bh_affr: return "bh^";
				case NkPhonSym::C_th_affr: return "th^";
				case NkPhonSym::C_dh_affr: return "dh^";
				case NkPhonSym::C_kh_affr: return "kh^";
				case NkPhonSym::C_m: return "m";
				case NkPhonSym::C_n: return "n";
				case NkPhonSym::C_ng: return "ng";
				case NkPhonSym::C_w: return "w";
				case NkPhonSym::C_y: return "y";
				case NkPhonSym::C_l: return "l";
				case NkPhonSym::C_r: return "r";
				case NkPhonSym::C_glottal: return "q"; // ʔ (convention X-SAMPA)
				default: return "?";
			}
		}

		const char *NkG2P::ToneLabel(NkTone t) {
			switch (t) {
				case NkTone::None: return "-";
				case NkTone::Mid: return "M";
				case NkTone::High: return "H";
				case NkTone::Low: return "L";
				case NkTone::Rising: return "R";
				case NkTone::Falling: return "F";
				default: return "?";
			}
		}

		// =========================================================================
		// Auto-test headless : mots RÉELS bbj (NT + lexique lamba-africa.com, cf.
		// NkG2P.h SOURCES [4][5]) -> vérifie voyelles/tons/consonnes attendus.
		// =========================================================================
		bool NkG2P::SelfTest() {
			bool ok = true;

			// --- (1) "dɔ̀mnyə̀" = « impasse » (lamba-africa.com, source [5] vérifiée). ---
			// d-ɔ(bas)-m-n-y-ə(bas). Encodage : d,ɔ,COMBINING GRAVE,m,n,y,ə,COMBINING GRAVE.
			{
				char buf[64];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar('d', buf + p);
				p += encoding::utf8::NkEncodeChar(kOpenOLower, buf + p);
				p += encoding::utf8::NkEncodeChar(kCombGrave, buf + p);
				p += encoding::utf8::NkEncodeChar('m', buf + p);
				p += encoding::utf8::NkEncodeChar('n', buf + p);
				p += encoding::utf8::NkEncodeChar('y', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				p += encoding::utf8::NkEncodeChar(kCombGrave, buf + p);
				NkString word(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(word);
				// Attendu : d, ɔ(Low), m, n, y(=j), ə(Low)  -> 6 unités.
				if (ph.Size() != 6)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_d))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::V_oh && ph[1].tone == NkTone::Low))
					ok = false;
				if (ok && !(ph[2].symbol == NkPhonSym::C_m))
					ok = false;
				if (ok && !(ph[3].symbol == NkPhonSym::C_n))
					ok = false;
				if (ok && !(ph[4].symbol == NkPhonSym::C_y))
					ok = false;
				if (ok && !(ph[5].symbol == NkPhonSym::V_eu && ph[5].tone == NkTone::Low))
					ok = false;
			}

			// --- (2) "lɛtə̌" = « solide » (lamba-africa.com, source [5] vérifiée). ---
			// l-ɛ-t-ə(caron=Rising).
			{
				char buf[64];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar('l', buf + p);
				p += encoding::utf8::NkEncodeChar(kOpenELower, buf + p);
				p += encoding::utf8::NkEncodeChar('t', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				p += encoding::utf8::NkEncodeChar(kCombCaron, buf + p);
				NkString word(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(word);
				if (ph.Size() != 4)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_l))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::V_eh && ph[1].tone == NkTone::Mid))
					ok = false;
				if (ok && !(ph[2].symbol == NkPhonSym::C_t))
					ok = false;
				if (ok && !(ph[3].symbol == NkPhonSym::V_eu && ph[3].tone == NkTone::Rising))
					ok = false;
			}

			// --- (3) "Ŋkhənyə" (début de Matio 1:1, « généalogie » — corpus NT réel,
			//     source [4]) : Ŋ-kh(affriqué)-ə-n-y-ə. Teste majuscule Ŋ + digraphe "kh".
			{
				char buf[64];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar(kEngUpper, buf + p); // Ŋ
				p += encoding::utf8::NkEncodeChar('k', buf + p);
				p += encoding::utf8::NkEncodeChar('h', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				p += encoding::utf8::NkEncodeChar('n', buf + p);
				p += encoding::utf8::NkEncodeChar('y', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				NkString word(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(word);
				// Attendu : ŋ, kh(affriqué), ə, n, y, ə -> 6 unités.
				if (ph.Size() != 6)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_ng))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::C_kh_affr))
					ok = false;
				if (ok && !(ph[2].symbol == NkPhonSym::V_eu))
					ok = false;
				if (ok && !(ph[3].symbol == NkPhonSym::C_n))
					ok = false;
				if (ok && !(ph[4].symbol == NkPhonSym::C_y))
					ok = false;
				if (ok && !(ph[5].symbol == NkPhonSym::V_eu))
					ok = false;
			}

			// --- (4) apostrophe finale -> occlusive glottale (ex. "gʉ'", forme réelle
			//     du NT, cf. corpus, source [4]). g-ʉ-ʔ.
			{
				char buf[64];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar('g', buf + p);
				p += encoding::utf8::NkEncodeChar(kUBarLower, buf + p);
				p += encoding::utf8::NkEncodeChar(kApostropheAscii, buf + p);
				NkString word(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(word);
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_g))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::V_uu))
					ok = false;
				if (ok && !(ph[2].symbol == NkPhonSym::C_glottal))
					ok = false;
			}

			// --- (5) deux mots séparés -> un silence (Sil) inséré entre les deux. ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(NkString("Si a"));
				// "Si" -> S i ; " " -> Sil ; "a" -> a  => 4 unités.
				if (ph.Size() != 4)
					ok = false;
				if (ok && !(ph[2].symbol == NkPhonSym::Sil))
					ok = false;
			}

			// --- (6) fr : "chat" -> sh a (le "t" final est MUET, règle des consonnes
			//     finales "CaReFuL" -- source [7] ; prononciation réelle /ʃa/, mot
			//     extrêmement courant, cf. tout dictionnaire fr, ex. Wiktionnaire). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("chat"));
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_sh && ph[1].symbol == NkPhonSym::V_a))
					ok = false;
			}

			// --- (7) en : "think" -> dentalTh i n k (mot lexical -> th NON voisé,
			//     source [9] ; inchangé par l'ajout de la règle th voisé/non voisé). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("think"));
				if (ph.Size() != 4)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_dentalTh))
					ok = false;
			}

			// --- (8) fr : "petite" -> p ə t i t (e muet FINAL seulement, le e médian
			//     reste schwa -- source [6] ; prononciation réelle /pətit/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("petite"));
				if (ph.Size() != 5)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_p && ph[1].symbol == NkPhonSym::V_eu &&
							ph[2].symbol == NkPhonSym::C_t && ph[3].symbol == NkPhonSym::V_i &&
							ph[4].symbol == NkPhonSym::C_t))
					ok = false;
			}

			// --- (9) fr : "chantes" -> ʃ ɑ̃ t ("-es" final muet + nasale an -> source
			//     [6] ; prononciation réelle /ʃɑ̃t/, pas de contamination par un "s"
			//     intervocalique puisque le mot n'en contient pas). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("chantes"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_sh && ph[1].symbol == NkPhonSym::V_aa && ph[1].nasalized &&
							ph[2].symbol == NkPhonSym::C_t))
					ok = false;
			}

			// --- (10) fr : "ville" -> v i l (EXCEPTION lexicale au digraphe -ill-,
			//     source [7bis] ; prononciation réelle /vil/, pas /vij/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("ville"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_v && ph[1].symbol == NkPhonSym::V_i &&
							ph[2].symbol == NkPhonSym::C_l))
					ok = false;
			}

			// --- (11) fr : "fille" -> f i y (digraphe -ill- RÉGULIER après consonne,
			//     source [7bis] ; prononciation réelle /fij/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("fille"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_f && ph[1].symbol == NkPhonSym::V_i &&
							ph[2].symbol == NkPhonSym::C_y))
					ok = false;
			}

			// --- (12) fr : "où" -> u (accent grave purement distinctif vs "ou" = "or",
			//     SEUL mot courant avec ù -- orthographe standard, prononciation /u/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("o\xC3\xB9")); // "où" en UTF-8 (o + ù U+00F9)
				if (ph.Size() != 1)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::V_u))
					ok = false;
			}

			// --- (13) fr : "un ami" -> liaison en -n (source [7], EXEMPLE CITÉ TEL
			//     QUEL par Wikipédia EN "Liaison (French)" : "un ami" -> /œ̃.n‿a.mi/).
			//     Sans la liaison on aurait un Sil entre les deux mots ; avec, un C_n. ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("un ami"));
				if (ph.Size() != 5)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::V_eu && ph[0].nasalized && ph[1].symbol == NkPhonSym::C_n &&
							ph[2].symbol == NkPhonSym::V_a && ph[3].symbol == NkPhonSym::C_m &&
							ph[4].symbol == NkPhonSym::V_i))
					ok = false;
			}

			// --- (14) fr : "le" -> l ə (mot grammatical monosyllabique -> le e final
			//     GARDE son schwa, source [6] ; prononciation réelle /lə/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesFr(NkString("le"));
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_l && ph[1].symbol == NkPhonSym::V_eu))
					ok = false;
			}

			// --- (15) en : "the" -> dentalDh ə (mot grammatical -> th VOISÉ, source
			//     [9] ; prononciation réelle /ðə/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("the"));
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_dentalDh))
					ok = false;
			}

			// --- (16)-(19) en : "magic e" (source [8]) -- cake/bike/note/cute : voyelle
			//     allongée (approximée par la monophtongue disponible la plus proche) +
			//     e final muet. Prononciations réelles : /keɪk/ /baɪk/ /noʊt/ /kjuːt/. ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("cake"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_k && ph[1].symbol == NkPhonSym::V_e &&
							ph[2].symbol == NkPhonSym::C_k))
					ok = false;
			}
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("bike"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_b && ph[1].symbol == NkPhonSym::V_a &&
							ph[2].symbol == NkPhonSym::C_k))
					ok = false;
			}
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("note"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_n && ph[1].symbol == NkPhonSym::V_o &&
							ph[2].symbol == NkPhonSym::C_t))
					ok = false;
			}
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("cute"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_k && ph[1].symbol == NkPhonSym::V_uu &&
							ph[2].symbol == NkPhonSym::C_t))
					ok = false;
			}

			// --- (20) en : "ice" -> a s ("magic e" (i_e -> approx a) + "c" MOU devant
			//     'e', source [11] ; prononciation réelle /aɪs/). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("ice"));
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::V_a && ph[1].symbol == NkPhonSym::C_s))
					ok = false;
			}

			// --- (21) en : "which" -> w i tsh (wh->w + ch->tsh, digraphes déjà
			//     existants, non-régression après le refactor). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("which"));
				if (ph.Size() != 3)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_w && ph[1].symbol == NkPhonSym::V_i &&
							ph[2].symbol == NkPhonSym::C_tsh))
					ok = false;
			}

			// --- (22) en : "who" -> h oh (EXCEPTION wh+o -> /h/, source [10] ;
			//     prononciation réelle /huː/, approx grossière sur la voyelle). ---
			{
				NkVector<NkPhonemeUnit> ph = ToPhonemesEn(NkString("who"));
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_h))
					ok = false;
			}

			// --- (23)/(24) bbj : "Pə" / "pə́" -- 1er et 2e mot du texte d'exemple bbj
			//     d'Omniglot (« Ghomala' language and alphabet », omniglot.com/writing/
			//     ghomala.htm, section « Sample text » : « Pə pə́ gɔ́m Ghɔmáláʼ... » =
			//     « Let us speak the Ghɔmáláʼ language... »), lui-même sourcé à
			//     https://www.ghomalaonline.com/bbj/mcùŋ/jɛ-ghɔmáláʼ (dictionnaire
			//     Ghomala'-Français en ligne cité par Omniglot). NOUVELLE source par
			//     rapport aux 4 sources bbj déjà utilisées (lamba-africa.com + corpus NT
			//     + Wikipedia/AGLC) -- récupérée en HTML brut (entités numériques
			//     &#x0259;/&#x0301; = CODEPOINTS EXACTS U+0259 SCHWA / U+0301 COMBINING
			//     ACUTE, vérifiées octet à octet, PAS une paraphrase) pour éviter tout
			//     risque de mauvaise transcription des diacritiques. Paire minimale
			//     PARFAITE pour tester la détection de ton seule (même mot, ton
			//     différent) : "Pə" (ton NON MARQUÉ = Mid) vs "pə́" (accent aigu = High).
			{
				char buf[16];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar('p', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				NkString wPe(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(wPe);
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_p))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::V_eu && ph[1].tone == NkTone::Mid))
					ok = false;
			}
			{
				char buf[16];
				usize p = 0;
				p += encoding::utf8::NkEncodeChar('p', buf + p);
				p += encoding::utf8::NkEncodeChar(kSchwaLower, buf + p);
				p += encoding::utf8::NkEncodeChar(kCombAcute, buf + p);
				NkString wPeHigh(buf, (NkString::SizeType)p);

				NkVector<NkPhonemeUnit> ph = ToPhonemesBbj(wPeHigh);
				if (ph.Size() != 2)
					ok = false;
				if (ok && !(ph[0].symbol == NkPhonSym::C_p))
					ok = false;
				if (ok && !(ph[1].symbol == NkPhonSym::V_eu && ph[1].tone == NkTone::High))
					ok = false;
			}

			return ok;
		}

	} // namespace ai
} // namespace nkentseu
