// =============================================================================
// NKSpeech/NkTextNorm.cpp — implémentation de la normalisation de texte du
// front-end TTS (voir NkTextNorm.h pour les sources). Tout le découpage se
// fait sur des CODEPOINTS Unicode (uint32), même style/portabilité que
// NkG2P.cpp. Zero-STL, namespace nkentseu::ai.
// =============================================================================
#include "NKSpeech/NkTextNorm.h"
#include "NKContainers/String/Encoding/NkUTF8.h"

namespace nkentseu {
	namespace ai {

		namespace {

			// -----------------------------------------------------------------------
			// Tables lexicales fr/en (voir NkTextNorm.h SOURCES [1][2][3]).
			// -----------------------------------------------------------------------
			const char *kFrOnes[10] = {"zero", "un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf"};
			const char *kFrTeens[10] = {"dix", "onze", "douze", "treize", "quatorze", "quinze",
										 "seize", "dix-sept", "dix-huit", "dix-neuf"};

			const char *kEnOnes[10] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
			const char *kEnTeens[10] = {"ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
										 "sixteen", "seventeen", "eighteen", "nineteen"};
			const char *kEnTens[10] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
										"eighty", "ninety"};

			// -----------------------------------------------------------------------
			// Fold ASCII A-Z -> a-z (même helper que NkG2P.cpp, dupliqué localement :
			// pas d'utilitaire partagé exporté pour ça dans NKContainers).
			// -----------------------------------------------------------------------
			uint32 FoldAsciiLower(uint32 cp) {
				if (cp >= 'A' && cp <= 'Z')
					return cp - 'A' + 'a';
				return cp;
			}

			bool IsAsciiDigit(uint32 cp) {
				return cp >= '0' && cp <= '9';
			}

			bool IsAsciiLetter(uint32 cp) {
				const uint32 f = FoldAsciiLower(cp);
				return f >= 'a' && f <= 'z';
			}

			bool IsAsciiUpper(uint32 cp) {
				return cp >= 'A' && cp <= 'Z';
			}

			bool IsAsciiWhitespace(uint32 cp) {
				return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r';
			}

			// Ponctuation reconnue comme génératrice de pause (voir NkTextNorm.h
			// SOURCES [6]) : . , ; : ! ?
			bool IsPausePunct(uint32 cp) {
				switch (cp) {
					case '.': case ',': case ';': case ':': case '!': case '?':
						return true;
					default:
						return false;
				}
			}

			// Autre ponctuation/séparateur ASCII/typographique (même liste que
			// NkG2P.cpp IsSeparator, moins les caractères déjà couverts par
			// IsPausePunct) : traitée comme frontière neutre (documenté NkTextNorm.h,
			// section "ce qui n'est pas géré").
			bool IsOtherSeparator(uint32 cp) {
				switch (cp) {
					case '"': case '(': case ')': case '[': case ']': case '{': case '}':
					case '-': case '/': case '\\':
					case 0x2013: case 0x2014: // - --
					case 0x201C: case 0x201D: // " "
					case 0x00AB: case 0x00BB: // <<  >>
						return true;
					default:
						return false;
				}
			}

			// -----------------------------------------------------------------------
			// Nombres cardinaux : formatage 0-99 / 0-999 (voir NkTextNorm.h SOURCES
			// [1][2][3] pour les règles d'accord/écriture détaillées).
			// -----------------------------------------------------------------------

			// fr, variante regionale (SOURCES [dialect]) : "septante"/"huitante"/
			// "nonante" sont des mots DE BASE reguliers (comme "cinquante"), PAS des
			// constructions composees a partir de "vingt"/"dix" -> la dizaine complete
			// se forme exactement comme 20-69 ("-"+unite, ou " et un" si unite==1),
			// jamais de forme a base de "dix"/teens contrairement au francais standard
			// (soixante-dix, quatre-vingt-dix). Jamais de 's' final : contrairement a
			// "quatre-vingts" (qui est litteralement "4 x 20", donc pluralisable),
			// "huitante" n'est PAS une multiplication de "vingt" et reste invariable
			// (deduction logique a partir de la regle d'accord de SOURCES [3],
			// cf. NkTextNorm.h SOURCES [dialect] pour la reference qui confirme la
			// forme reguliere elle-meme).
			NkString RegularTensWord(const char *base, int32 ones) {
				NkString out(base);
				if (ones == 1) {
					out.Append(" et un");
				} else if (ones > 0) {
					out.Append("-").Append(kFrOnes[ones]);
				}
				return out;
			}

			// fr : 0..99 -> mots. `pluralizeEighty` : applique le 's' de
			// "quatre-vingts" (SOURCES [3]) si n == 80 exactement ET que ce groupe de
			// deux chiffres est en position "unités" (rien après dans tout le
			// nombre) — voir Format999Fr pour la propagation de ce booléen.
			// `dialect` (SOURCES [dialect]) : BelgeSuisse reecrit 70-99 en mots
			// reguliers septante/huitante/nonante ; 0-69 est IDENTIQUE aux deux
			// dialectes (aucune variante regionale connue/sourcee en dessous de 70).
			NkString FrTwoDigits(int32 n, bool pluralizeEighty, NkFrNumberDialect dialect) {
				if (n < 10)
					return NkString(kFrOnes[n]);
				if (dialect == NkFrNumberDialect::BelgeSuisse) {
					if (n >= 70 && n <= 79)
						return RegularTensWord("septante", n - 70);
					if (n >= 80 && n <= 89)
						return RegularTensWord("huitante", n - 80);
					if (n >= 90 && n <= 99)
						return RegularTensWord("nonante", n - 90);
					// n < 70 : formation partagee par les deux dialectes, cf. ci-dessous.
				}
				if (n < 20)
					return NkString(kFrTeens[n - 10]);

				const int32 tens = n / 10;
				const int32 ones = n % 10;

				switch (tens) {
					case 2: case 3: case 4: case 5: case 6: {
						static const char *kBase[7] = {"", "", "vingt", "trente", "quarante", "cinquante", "soixante"};
						NkString out(kBase[tens]);
						if (ones == 1) {
							out.Append(" et un"); // vingt et un / trente et un / ... (SOURCES [3])
						} else if (ones > 0) {
							out.Append("-").Append(kFrOnes[ones]);
						}
						return out;
					}
					case 7: {
						// 70-79 = soixante + (10+ones) : soixante-dix, soixante et onze
						// (SEUL cas où "et" relie a un nombre >= 70, SOURCES [3]),
						// soixante-douze...soixante-dix-neuf.
						NkString out("soixante");
						if (ones == 1)
							out.Append(" et ").Append(kFrTeens[ones]); // soixante et onze
						else
							out.Append("-").Append(kFrTeens[ones]);
						return out;
					}
					case 8: {
						// 80-89 = quatre-vingt(s) + ones ; PAS de "et" (SOURCES [3] : "ni
						// quatre-vingt-un ni quatre-vingt-onze avec et").
						if (ones == 0)
							return NkString("quatre-vingt").Append(pluralizeEighty ? "s" : "");
						return NkString("quatre-vingt-").Append(kFrOnes[ones]);
					}
					case 9: {
						// 90-99 = quatre-vingt-dix + ones (0..9 -> teens[ones]).
						return NkString("quatre-vingt-").Append(kFrTeens[ones]);
					}
					default:
						return NkString(kFrOnes[n]); // ne devrait pas arriver (n<100 couvert)
				}
			}

			// fr : 1..999 -> mots. `isUnitsGroup` = true si CE groupe de 3 chiffres
			// est le groupe le moins significatif du nombre entier (aucun mot
			// d'échelle "mille/million/..." après lui) : conditionne le 's' de
			// "cent(s)"/"quatre-vingt(s)" (SOURCES [3] + limite documentée en tête de
			// NkTextNorm.h pour le cas "avant million/milliard").
			NkString Format999Fr(int32 n, bool isUnitsGroup, NkFrNumberDialect dialect) {
				const int32 h = n / 100;
				const int32 r = n % 100;
				NkString out;
				if (h > 0) {
					if (h > 1)
						out.Append(kFrOnes[h]).Append(" ");
					out.Append("cent");
					if (h > 1 && r == 0 && isUnitsGroup)
						out.Append("s"); // "deux cents" (SOURCES [3])
					if (r > 0)
						out.Append(" ").Append(FrTwoDigits(r, isUnitsGroup, dialect));
				} else {
					out = FrTwoDigits(r, isUnitsGroup, dialect);
				}
				return out;
			}

			// en : 0..99 -> mots (SOURCES [1], usage standard, pas de règle d'accord
			// particulière contrairement au français).
			NkString EnTwoDigits(int32 n) {
				if (n < 10)
					return NkString(kEnOnes[n]);
				if (n < 20)
					return NkString(kEnTeens[n - 10]);
				const int32 tens = n / 10;
				const int32 ones = n % 10;
				NkString out(kEnTens[tens]);
				if (ones > 0)
					out.Append("-").Append(kEnOnes[ones]);
				return out;
			}

			// en : 1..999 -> mots. "hundred" toujours précédé du chiffre (y compris
			// "one hundred"), jamais pluralisé en lecture cardinale (SOURCES [1]).
			NkString Format999En(int32 n) {
				const int32 h = n / 100;
				const int32 r = n % 100;
				NkString out;
				if (h > 0) {
					out.Append(kEnOnes[h]).Append(" hundred");
					if (r > 0)
						out.Append(" ").Append(EnTwoDigits(r));
				} else {
					out = EnTwoDigits(r);
				}
				return out;
			}

			// Nombre de groupes de 3 chiffres supportés (unités, mille, million,
			// milliard/billion, billion/trillion -> 10^0 a 10^14, largement au-dela de
			// l'exigence "au moins jusqu'aux milliers", cf. NkTextNorm.h).
			constexpr int32 kMaxGroups = 5;

			// -----------------------------------------------------------------------
			// Mois (fr/en, 1..12) pour ExpandDateLiteral. Sans accent (meme convention
			// ASCII que kFrOnes/kFrTeens : "fevrier" pas "février", "aout" pas "août",
			// "decembre" pas "décembre").
			// -----------------------------------------------------------------------
			const char *kFrMonths[12] = {
				"janvier", "fevrier", "mars", "avril", "mai", "juin",
				"juillet", "aout", "septembre", "octobre", "novembre", "decembre",
			};
			const char *kEnMonths[12] = {
				"january", "february", "march", "april", "may", "june",
				"july", "august", "september", "october", "november", "december",
			};

			// -----------------------------------------------------------------------
			// Validite calendaire reelle (SOURCES [leap-year]) : annee bissextile =
			// divisible par 4, SAUF les seculaires (divisibles par 100), SAUF CELLES
			// divisibles par 400 (exception de l'exception, ex. 2000 est bissextile,
			// 1900 ne l'est pas). Utilise par ExpandDateLiteral pour rejeter les dates
			// impossibles (ex. "30/02/2026", "31/04/2026").
			// -----------------------------------------------------------------------
			bool IsLeapYear(int32 year) {
				if (year % 400 == 0)
					return true;
				if (year % 100 == 0)
					return false;
				return (year % 4) == 0;
			}

			int32 DaysInMonth(int32 month, int32 year) {
				static const int32 kDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
				if (month < 1 || month > 12)
					return 0;
				if (month == 2 && IsLeapYear(year))
					return 29;
				return kDaysInMonth[month - 1];
			}

			// -----------------------------------------------------------------------
			// Détection de littéraux DATE/HEURE/ORDINAL dans le flux de codepoints de
			// Normalize() — appelés AVANT le nombre cardinal simple (voir NkTextNorm.h
			// SOURCES pour les formats couverts/non couverts). Chaque fonction renvoie le
			// nombre de codepoints consommés (0 si non reconnu, `outLiteral` alors
			// inchangé) : ne modifie jamais l'index appelant en cas d'échec.
			// -----------------------------------------------------------------------

			// Date JJ/MM/AAAA (fr) ou MM/DD/AAAA (en, ordre choisi dans
			// ExpandDateLiteral) : d{1,2} '/' d{1,2} '/' (d{4}|d{2}) — année à 4
			// chiffres OU à 2 chiffres (SOURCES [2digit-year] : convention de
			// windowing appliquée par ExpandDateLiteral, pas ici — ce scanner se
			// contente de reconnaître la LONGUEUR valide du groupe). 1 ou 3 chiffres
			// résiduels ne matchent PAS (ambigu, non couvert).
			usize ScanDateLiteral(const NkVector<uint32> &cps, usize n, usize start, NkString &outLiteral) {
				usize p = start;
				usize g1 = p;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]) && (p - g1) < 2)
					++p;
				if (p == g1 || p >= n || cps[(nk_size)p] != '/')
					return 0;
				++p;
				usize g2 = p;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]) && (p - g2) < 2)
					++p;
				if (p == g2 || p >= n || cps[(nk_size)p] != '/')
					return 0;
				++p;
				usize g3 = p;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]) && (p - g3) < 4)
					++p;
				const usize yearLen = p - g3;
				if (yearLen != 2 && yearLen != 4)
					return 0;
				if (p < n && IsAsciiDigit(cps[(nk_size)p]))
					return 0; // pas un chiffre residuel (annee sur plus de 4 chiffres, ou 3 chiffres)

				NkString literal;
				for (usize k = start; k < p; ++k)
					literal.Append((char)cps[(nk_size)k]);
				outLiteral = literal;
				return p - start;
			}

			// Heure d{1,2} ('h'|':') d{2} (ex. "15h30", "15:30", "1h00"). 'h' et ':'
			// acceptés quelle que soit la langue (simple convention de saisie, la
			// LECTURE dépend de `lang` dans ExpandTimeLiteral, pas la reconnaissance).
			usize ScanTimeLiteral(const NkVector<uint32> &cps, usize n, usize start, NkString &outLiteral) {
				usize p = start;
				usize g1 = p;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]) && (p - g1) < 2)
					++p;
				if (p == g1 || p >= n)
					return 0;
				const uint32 sep = cps[(nk_size)p];
				if (sep != 'h' && sep != ':')
					return 0;
				++p;
				usize g2 = p;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]) && (p - g2) < 2)
					++p;
				if ((p - g2) != 2)
					return 0;
				if (p < n && IsAsciiDigit(cps[(nk_size)p]))
					return 0;

				NkString literal;
				for (usize k = start; k < p; ++k)
					literal.Append((char)cps[(nk_size)k]);
				outLiteral = literal;
				return p - start;
			}

			// Ordinal : chiffres + suffixe reconnu, suffixe suivi d'une FRONTIÈRE de mot
			// (jamais une lettre juste après, même mitigation "token entier" que les
			// abréviations SOURCES [5]). fr : "er"/"re" réservés à "1" (SOURCES [ord-fr] —
			// voir NkTextNorm.h), "eme"/"e" pour tout le reste. en : "st"/"nd"/"rd"/"th",
			// acceptés sans validation stricte de cohérence avec le dernier chiffre
			// (simplification tolérante aux fautes de frappe, ex. "3nd" accepté comme
			// "3rd" : seule la VALEUR compte pour la sortie, cf. ExpandOrdinalLiteral).
			usize ScanOrdinalLiteral(const NkVector<uint32> &cps, usize n, usize start, NkTextNormLang lang,
									  NkString &outLiteral) {
				usize p = start;
				while (p < n && IsAsciiDigit(cps[(nk_size)p]))
					++p;
				if (p == start)
					return 0;
				const usize digitsLen = p - start;

				auto folded = [&](usize idx) -> uint32 { return FoldAsciiLower(cps[(nk_size)idx]); };
				auto boundaryAfter = [&](usize idx) -> bool { return idx >= n || !IsAsciiLetter(cps[(nk_size)idx]); };

				usize suffixLen = 0;
				if (lang == NkTextNormLang::Fr) {
					if (p + 3 <= n && folded(p) == 'e' && folded(p + 1) == 'm' && folded(p + 2) == 'e' &&
						boundaryAfter(p + 3)) {
						suffixLen = 3; // "2eme", "21eme"...
					} else if (digitsLen == 1 && cps[(nk_size)start] == '1' && p + 2 <= n &&
							   folded(p) == 'e' && folded(p + 1) == 'r' && boundaryAfter(p + 2)) {
						suffixLen = 2; // "1er" (masculin, SEUL cas valide pour "er", SOURCES [ord-fr])
					} else if (digitsLen == 1 && cps[(nk_size)start] == '1' && p + 2 <= n &&
							   folded(p) == 'r' && folded(p + 1) == 'e' && boundaryAfter(p + 2)) {
						suffixLen = 2; // "1re" (feminin, SOURCES [fem-ord] : produit "premiere")
					} else if (digitsLen == 1 && cps[(nk_size)start] == '1' && p + 3 <= n &&
							   folded(p) == 'e' && folded(p + 1) == 'r' && folded(p + 2) == 'e' &&
							   boundaryAfter(p + 3)) {
						suffixLen = 3; // "1ere" (rendu ASCII de "1ère", meme feminin que "1re")
					} else if (p + 1 <= n && folded(p) == 'e' && boundaryAfter(p + 1)) {
						suffixLen = 1; // "2e", "21e"...
					}
				} else {
					if (p + 2 <= n && boundaryAfter(p + 2)) {
						const uint32 c0 = folded(p);
						const uint32 c1 = folded(p + 1);
						if ((c0 == 's' && c1 == 't') || (c0 == 'n' && c1 == 'd') || (c0 == 'r' && c1 == 'd') ||
							(c0 == 't' && c1 == 'h')) {
							suffixLen = 2;
						}
					}
				}
				if (suffixLen == 0)
					return 0;

				NkString literal;
				for (usize k = start; k < p + suffixLen; ++k)
					literal.Append((char)cps[(nk_size)k]);
				outLiteral = literal;
				return (p + suffixLen) - start;
			}

		} // namespace

		NkString NkTextNorm::NumberToWords(int64 value, NkTextNormLang lang, NkFrNumberDialect dialect) {
			const bool neg = value < 0;
			uint64 mag = neg ? (uint64)(-(value + 1)) + 1u : (uint64)value; // evite l'overflow sur INT64_MIN

			if (mag == 0)
				return NkString(lang == NkTextNormLang::Fr ? "zero" : "zero");

			// Decoupage en groupes de 3 chiffres, du moins au plus significatif.
			int32 groups[kMaxGroups] = {0, 0, 0, 0, 0};
			uint64 rem = mag;
			int32 highest = 0;
			for (int32 g = 0; g < kMaxGroups && rem > 0; ++g) {
				groups[g] = (int32)(rem % 1000);
				rem /= 1000;
				highest = g;
			}

			NkString out;
			if (neg)
				out.Append(lang == NkTextNormLang::Fr ? "moins " : "minus ");

			bool first = true;
			for (int32 g = highest; g >= 0; --g) {
				const int32 n = groups[g];
				if (n == 0)
					continue;
				if (!first)
					out.Append(" ");
				first = false;

				if (lang == NkTextNormLang::Fr) {
					if (g == 0) {
						out.Append(Format999Fr(n, true, dialect));
					} else if (g == 1) {
						// "mille" invariable, jamais precede de "un" (SOURCES [2][3]).
						if (n == 1)
							out.Append("mille");
						else
							out.Append(Format999Fr(n, false, dialect)).Append(" mille");
					} else {
						// "million"/"milliard"/"billion" sont des NOMS (pas des adjectifs
						// numeraux comme "mille") : "cent"/"quatre-vingts" s'accordent donc
						// NORMALEMENT devant eux (isUnitsGroup=true), contrairement au groupe
						// "mille" juste au-dessus qui reste invariable (SOURCES [3bis], fix
						// 2026-07-25 de la limite documentee dans NkTextNorm.h : "quatre-vingts
						// millions", "deux cents millions", mais "quatre-vingt mille").
						static const char *kScaleSing[kMaxGroups] = {"", "mille", "million", "milliard", "billion"};
						static const char *kScalePlur[kMaxGroups] = {"", "mille", "millions", "milliards", "billions"};
						out.Append(Format999Fr(n, true, dialect)).Append(" ").Append(n > 1 ? kScalePlur[g] : kScaleSing[g]);
					}
				} else {
					if (g == 0) {
						out.Append(Format999En(n));
					} else {
						static const char *kScale[kMaxGroups] = {"", "thousand", "million", "billion", "trillion"};
						out.Append(Format999En(n)).Append(" ").Append(kScale[g]);
					}
				}
			}
			return out;
		}

		NkString NkTextNorm::ExpandNumberLiteral(const NkString &literal, NkTextNormLang lang, NkFrNumberDialect dialect) {
			const NkString::SizeType n = literal.Length();
			if (n == 0)
				return NkString();

			NkString::SizeType i = 0;
			bool neg = false;
			if (literal[0] == '-') {
				neg = true;
				i = 1;
			}
			if (i >= n || !(literal[i] >= '0' && literal[i] <= '9'))
				return NkString(); // pas un nombre reconnu

			// Partie entiere.
			NkString::SizeType intStart = i;
			while (i < n && literal[i] >= '0' && literal[i] <= '9')
				++i;
			NkString intPart = literal.SubStr(intStart, i - intStart);

			// Separateur decimal ',' ou '.' (accepte les deux quelle que soit la
			// langue : robustesse, cf. NkTextNorm.h).
			NkString fracPart;
			bool hasFrac = false;
			if (i < n && (literal[i] == ',' || literal[i] == '.')) {
				NkString::SizeType fracStart = i + 1;
				NkString::SizeType j = fracStart;
				while (j < n && literal[j] >= '0' && literal[j] <= '9')
					++j;
				if (j > fracStart) { // au moins un chiffre apres le separateur
					fracPart = literal.SubStr(fracStart, j - fracStart);
					hasFrac = true;
					i = j;
				}
			}
			if (i != n)
				return NkString(); // caracteres residuels non numeriques : pas un nombre simple reconnu

			int64 intValue = 0;
			intPart.ToInt64(intValue);
			if (neg)
				intValue = -intValue;

			NkString out = NumberToWords(intValue, lang, dialect);
			if (hasFrac) {
				out.Append(lang == NkTextNormLang::Fr ? " virgule" : " point");
				// Lecture chiffre par chiffre de la partie decimale (SOURCES [4]).
				const char **ones = (lang == NkTextNormLang::Fr) ? kFrOnes : kEnOnes;
				for (NkString::SizeType k = 0; k < fracPart.Length(); ++k) {
					const int32 d = fracPart[k] - '0';
					out.Append(" ").Append(ones[d]);
				}
			}
			return out;
		}

		NkString NkTextNorm::OrdinalToWords(int64 value, NkTextNormLang lang, bool feminine, NkFrNumberDialect dialect) {
			if (value <= 0)
				return NkString(); // ordinaux uniquement pour value >= 1 (limite honnete, cf. NkTextNorm.h)

			// Le nombre CARDINAL sert de base : seul le DERNIER "mot" (segment apres le
			// dernier espace OU trait d'union, le plus tardif des deux) recoit la
			// transformation ordinale ; tout ce qui precede reste un cardinal inchange
			// (SOURCES [ord-fr]/[ord-en], voir NkTextNorm.h). Repose sur NumberToWords
			// deja teste/source : aucune nouvelle table de nombres. `dialect` (SOURCES
			// [dialect]) ne change que le mot cardinal de base (ex. 70 -> "septante" au
			// lieu de "soixante-dix") : la regle d'elision du 'e' muet avant "-ieme" est
			// generique et s'applique donc telle quelle ("septante" -> "septantieme").
			// `feminine` (SOURCES [fem-ord]) : seul "premier"/"premiere" (value == 1)
			// varie en genre en francais, cf. NkTextNorm.h.
			if (lang == NkTextNormLang::Fr) {
				if (value == 1)
					return NkString(feminine ? "premiere" : "premier"); // irregulier (SOURCES [ord-fr] + [fem-ord])

				NkString card = NumberToWords(value, lang, dialect);
				const NkString::SizeType sp = card.RFind(' ');
				const NkString::SizeType hy = card.RFind('-');
				NkString::SizeType cut;
				if (sp == NkString::npos)
					cut = hy;
				else if (hy == NkString::npos)
					cut = sp;
				else
					cut = (sp > hy) ? sp : hy;

				NkString prefix = (cut == NkString::npos) ? NkString() : card.SubStr(0, cut + 1);
				NkString last = (cut == NkString::npos) ? card : card.SubStr(cut + 1, card.Length() - (cut + 1));

				// Marque plurielle ('s' de "cents"/"quatre-vingts"/"millions"...) : jamais
				// conservee a l'ordinal, seul le nombre CARDINAL la porte (SOURCES [ord-fr]).
				if (last.EndsWith('s'))
					last.Erase(last.Length() - 1, 1);

				NkString suffixed;
				if (last.Compare("un") == 0) {
					suffixed = "unieme"; // "vingt et unieme", jamais "vingt et premier" (SOURCES [ord-fr])
				} else if (last.Compare("cinq") == 0) {
					suffixed = "cinquieme"; // u intercalaire (SOURCES [ord-fr])
				} else if (last.EndsWith('f')) {
					suffixed = last.SubStr(0, last.Length() - 1);
					suffixed.Append("vieme"); // neuf -> neuvieme (f->v, SOURCES [ord-fr])
				} else if (last.EndsWith('e')) {
					suffixed = last.SubStr(0, last.Length() - 1);
					suffixed.Append("ieme"); // e muet elide (SOURCES [ord-fr])
				} else {
					suffixed = last;
					suffixed.Append("ieme");
				}
				prefix.Append(suffixed);
				return prefix;
			} else {
				NkString card = NumberToWords(value, lang);
				const NkString::SizeType sp = card.RFind(' ');
				const NkString::SizeType hy = card.RFind('-');
				NkString::SizeType cut;
				if (sp == NkString::npos)
					cut = hy;
				else if (hy == NkString::npos)
					cut = sp;
				else
					cut = (sp > hy) ? sp : hy;

				NkString prefix = (cut == NkString::npos) ? NkString() : card.SubStr(0, cut + 1);
				NkString last = (cut == NkString::npos) ? card : card.SubStr(cut + 1, card.Length() - (cut + 1));

				// Table d'irreguliers (SOURCES [ord-en]) : couvre aussi les composes qui
				// FINISSENT par ces mots ("twenty-one" -> "twenty-first", "twenty-two" ->
				// "twenty-second"...), puisque seul le dernier segment est transforme.
				struct Irregular {
						const char *card;
						const char *ord;
				};
				static const Irregular kIrregular[] = {
					{"one", "first"}, {"two", "second"}, {"three", "third"},
					{"five", "fifth"}, {"eight", "eighth"}, {"nine", "ninth"}, {"twelve", "twelfth"},
				};
				NkString suffixed;
				bool matched = false;
				for (int32 k = 0; k < (int32)(sizeof(kIrregular) / sizeof(kIrregular[0])); ++k) {
					if (last.Compare(kIrregular[k].card) == 0) {
						suffixed = kIrregular[k].ord;
						matched = true;
						break;
					}
				}
				if (!matched) {
					if (last.EndsWith('y')) {
						suffixed = last.SubStr(0, last.Length() - 1);
						suffixed.Append("ieth"); // twenty -> twentieth (SOURCES [ord-en])
					} else {
						suffixed = last;
						suffixed.Append("th"); // four -> fourth, hundred -> hundredth... (SOURCES [ord-en])
					}
				}
				prefix.Append(suffixed);
				return prefix;
			}
		}

		NkString NkTextNorm::ExpandOrdinalLiteral(const NkString &literal, NkTextNormLang lang, NkFrNumberDialect dialect) {
			// N'utilise QUE la partie CHIFFRES en tete ; le suffixe deja consomme par
			// ScanOrdinalLiteral n'est PAS revalide ici (simplification tolerante aux
			// fautes de frappe, ex. "3nd" -> traite comme "3rd" serait, cf. NkTextNorm.h).
			NkString::SizeType i = 0;
			const NkString::SizeType n = literal.Length();
			while (i < n && literal[i] >= '0' && literal[i] <= '9')
				++i;
			if (i == 0)
				return NkString(); // pas de chiffres en tete : pas un ordinal reconnu

			NkString digits = literal.SubStr(0, i);
			int64 value = 0;
			digits.ToInt64(value);

			// Marqueur feminin "1re"/"1ere" (rendu ASCII de "1ère") -> "premiere"
			// (SOURCES [fem-ord]) : seul le suffixe restant apres les chiffres compte,
			// et seulement pour la valeur 1 (irreguliere, cf. OrdinalToWords).
			bool feminine = false;
			if (lang == NkTextNormLang::Fr && value == 1) {
				NkString suffix = literal.SubStr(i, n - i);
				if (suffix.Compare("re") == 0 || suffix.Compare("ere") == 0)
					feminine = true;
			}
			return OrdinalToWords(value, lang, feminine, dialect);
		}

		NkString NkTextNorm::ExpandDateLiteral(const NkString &literal, NkTextNormLang lang, NkFrNumberDialect dialect,
											   NkTextNormYearReading yearReading) {
			// "D[D]/M[M]/(AAAA|AA)" : ordre JJ/MM (fr) ou MM/DD (en) - SOURCES [date-fr]
			// pour la lecture du jour (cardinal, sauf "premier" le 1er du mois). Annee a
			// 4 OU 2 chiffres (SOURCES [2digit-year], windowing applique ci-dessous).
			const NkString::SizeType n = literal.Length();
			const NkString::SizeType s1 = literal.Find('/');
			if (s1 == NkString::npos)
				return NkString();
			const NkString::SizeType s2 = literal.Find('/', s1 + 1);
			if (s2 == NkString::npos)
				return NkString();

			NkString aStr = literal.SubStr(0, s1);
			NkString bStr = literal.SubStr(s1 + 1, s2 - (s1 + 1));
			NkString cStr = literal.SubStr(s2 + 1, n - (s2 + 1));
			if (aStr.Empty() || bStr.Empty() || (cStr.Length() != 4 && cStr.Length() != 2))
				return NkString();

			int32 aVal = 0, bVal = 0, yearRaw = 0;
			if (!aStr.ToInt(aVal) || !bStr.ToInt(bVal) || !cStr.ToInt(yearRaw))
				return NkString();

			int32 yearVal = yearRaw;
			if (cStr.Length() == 2) {
				// Windowing POSIX/date a 2 chiffres (SOURCES [2digit-year]) : 00-68 ->
				// 2000-2068, 69-99 -> 1969-1999 (pivot 69, meme convention que le standard
				// POSIX/COBOL historique documente pour le bug de l'an 2000).
				yearVal = (yearRaw <= 68) ? (2000 + yearRaw) : (1900 + yearRaw);
			}

			int32 day = 0, month = 0;
			if (lang == NkTextNormLang::Fr) {
				day = aVal;
				month = bVal;
			} else {
				month = aVal;
				day = bVal;
			}
			if (month < 1 || month > 12 || day < 1 || day > 31)
				return NkString();
			// Validite calendaire REELLE (SOURCES [leap-year]) : nombre de jours exact du
			// mois, fevrier variable selon l'annee bissextile (regle gregorienne). Rejet
			// GRACIEUX (chaine vide, meme convention que le reste du fichier pour une
			// entree non reconnue) : pas de crash, pas de log (module sans etat/logger,
			// cf. NkTextNorm.h).
			if (day > DaysInMonth(month, yearVal))
				return NkString();

			NkString out;
			if (lang == NkTextNormLang::Fr) {
				if (day == 1)
					out.Append("premier"); // SOURCES [date-fr] : seul le 1er du mois est ordinal
				else
					out.Append(NumberToWords(day, lang, dialect));
				out.Append(" ").Append(kFrMonths[month - 1]).Append(" ").Append(NumberToWords(yearVal, lang, dialect));
			} else {
				out.Append(kEnMonths[month - 1]).Append(" ").Append(OrdinalToWords(day, lang, false, dialect));
				out.Append(" ").Append(YearToWords(yearVal, lang, yearReading));
			}
			return out;
		}

		NkString NkTextNorm::YearToWords(int32 year, NkTextNormLang lang, NkTextNormYearReading reading) {
			// fr : aucune convention par paires attestee (SOURCES [year-pairs]) ->
			// toujours cardinal plein, quel que soit `reading` (parametre ignore pour
			// fr, documente honnetement plutot que d'inventer une regle).
			if (lang == NkTextNormLang::Fr || reading == NkTextNormYearReading::Full)
				return NumberToWords(year, lang);

			// en, Paired (SOURCES [year-pairs]) : lecture par PAIRES de deux chiffres
			// ("nineteen eighty-four", "twenty twenty-six"), uniquement pour une annee
			// EXACTEMENT 4 chiffres dans [1000,9999] ; hors bornes -> repli cardinal
			// plein (meme fonction NumberToWords, aucune nouvelle table).
			if (year < 1000 || year > 9999)
				return NumberToWords(year, lang);

			const int32 ab = year / 100; // 10..99
			const int32 cd = year % 100; // 0..99
			if (cd == 0) {
				// Annees "rondes" (x900, x000...) : la lecture par paires est
				// incoherente/contestee selon les sources (ex. 2000 se dit "two
				// thousand", jamais "twenty hundred") -> repli honnete sur le cardinal
				// plein plutot que d'inventer une regle non consensuelle (SOURCES
				// [year-pairs], meme politique que le reste du fichier).
				return NumberToWords(year, lang);
			}

			NkString out = EnTwoDigits(ab);
			out.Append(" ");
			if (cd < 10)
				out.Append("oh ").Append(EnTwoDigits(cd)); // ex. 1905 -> "nineteen oh five"
			else
				out.Append(EnTwoDigits(cd));
			return out;
		}

		NkString NkTextNorm::ExpandTimeLiteral(const NkString &literal, NkTextNormLang lang, NkTextNormTimeFormat format) {
			// "HH(h|:)MM" : heures 0-23, minutes 0-59. fr : "minuit"/"midi" pour 0h/12h
			// (SOURCES [minuit-midi]), sinon 24h litteral ("heure" feminin, cf. SOURCES
			// [9]). en : 24h par defaut (`format == H24`), ou 12h AM/PM si
			// `format == H12` (SOURCES [ampm]).
			const NkString::SizeType n = literal.Length();
			NkString::SizeType sep = literal.Find('h');
			if (sep == NkString::npos)
				sep = literal.Find(':');
			if (sep == NkString::npos)
				return NkString();

			NkString hStr = literal.SubStr(0, sep);
			NkString mStr = literal.SubStr(sep + 1, n - (sep + 1));
			if (hStr.Empty() || mStr.Length() != 2)
				return NkString();

			int32 hourVal = 0, minuteVal = 0;
			if (!hStr.ToInt(hourVal) || !mStr.ToInt(minuteVal))
				return NkString();
			if (hourVal < 0 || hourVal > 23 || minuteVal < 0 || minuteVal > 59)
				return NkString();

			NkString out;
			if (lang == NkTextNormLang::Fr) {
				// "heure" est feminin : "une heure" (pas "un heure"), pluriel des 2h
				// (SOURCES [time-fr]). Pas de mot "minutes" (usage standard francais :
				// "quinze heures trente", pas "...trente minutes"). "minuit"/"midi"
				// (SOURCES [minuit-midi]) remplacent "zero heure(s)"/"douze heures" pour
				// 0h et 12h ; `format` est IGNORE en francais (pas de convention AM/PM
				// standard, SOURCES [minuit-midi] : le systeme francophone reste sur les
				// 24h, le 12h/AM-PM est un usage anglophone).
				if (hourVal == 0)
					out.Append("minuit");
				else if (hourVal == 12)
					out.Append("midi");
				else if (hourVal == 1)
					out.Append("une heure");
				else
					out.Append(NumberToWords(hourVal, lang)).Append(" heures");
				if (minuteVal > 0)
					out.Append(" ").Append(NumberToWords(minuteVal, lang));
			} else if (format == NkTextNormTimeFormat::H24) {
				// Convention "digitale" anglaise (SOURCES [time-en]) : minutes < 10 lues
				// chiffre par chiffre apres "oh" (ex. "three oh five"), 0 -> "o clock"
				// (ecrit SANS apostrophe : reste dans le style ASCII simple du fichier),
				// sinon nombre cardinal a 2 chiffres direct ("fifteen thirty").
				out.Append(NumberToWords(hourVal, lang));
				if (minuteVal == 0)
					out.Append(" o clock");
				else if (minuteVal < 10)
					out.Append(" oh ").Append(NumberToWords(minuteVal, lang));
				else
					out.Append(" ").Append(NumberToWords(minuteVal, lang));
			} else {
				// 12h AM/PM (SOURCES [ampm]) : convention standard "12 AM" = minuit,
				// "12 PM" = midi, sinon heure modulo 12 (1-11 le matin/l'apres-midi).
				int32 h12 = hourVal % 12;
				if (h12 == 0)
					h12 = 12;
				const char *suffix = (hourVal < 12) ? "am" : "pm";
				out.Append(NumberToWords(h12, lang));
				if (minuteVal == 0)
					out.Append(" o clock ").Append(suffix);
				else if (minuteVal < 10)
					out.Append(" oh ").Append(NumberToWords(minuteVal, lang)).Append(" ").Append(suffix);
				else
					out.Append(" ").Append(NumberToWords(minuteVal, lang)).Append(" ").Append(suffix);
			}
			return out;
		}

		float32 NkTextNorm::PauseDurationMs(char punct) {
			// Valeurs d'ingenierie (SOURCES [6]) : pauses courtes pour la ponctuation
			// "interne" a la phrase, plus longues en fin de phrase.
			switch (punct) {
				case ',': return 150.0f;
				case ';': return 220.0f;
				case ':': return 220.0f;
				case '.': return 400.0f;
				case '!': return 400.0f;
				case '?': return 400.0f;
				default: return 0.0f;
			}
		}

		NkVector<NkTextNormToken> NkTextNorm::Normalize(const NkString &text, NkTextNormLang lang,
														 NkFrNumberDialect dialect, NkTextNormYearReading yearReading) {
			NkVector<NkTextNormToken> out;

			// Petite table d'abreviations SOURCEES (NkTextNorm.h SOURCES [5]),
			// consultee uniquement sur un TOKEN ENTIER (jamais un prefixe) : evite le
			// piege documente (ex. "stereo"/"drole" confondus avec "St"/"Dr").
			// `isTitle` (SOURCES [abbrev-sbd]) distingue deux CLASSES d'abreviations
			// pour l'heuristique de desambiguisation abreviation/fin de phrase :
			// - titres de civilite (M./Mme/Mlle/Dr/Mr/Mrs) : par DEFINITION toujours
			//   suivis d'un nom propre (leur seule fonction linguistique), donc jamais
			//   une fin de phrase reelle -> le point qui suit ne recoit jamais la pause
			//   longue de fin de phrase.
			// - "etc" : PEUT terminer une phrase ou non -> heuristique sur la casse du
			//   mot suivant (cf. bloc de detection plus bas).
			struct Abbrev { const char *from; const char *to; bool isTitle; };
			static const Abbrev kFrAbbrev[] = {
				{"M", "Monsieur", true}, {"Mme", "Madame", true}, {"Mlle", "Mademoiselle", true},
				{"Dr", "Docteur", true}, {"etc", "et cetera", false},
			};
			static const Abbrev kEnAbbrev[] = {
				{"Mr", "Mister", true}, {"Mrs", "Missus", true}, {"Dr", "Doctor", true}, {"etc", "et cetera", false},
			};
			const Abbrev *abbrevTable = (lang == NkTextNormLang::Fr) ? kFrAbbrev : kEnAbbrev;
			const int32 abbrevCount = (lang == NkTextNormLang::Fr)
										   ? (int32)(sizeof(kFrAbbrev) / sizeof(kFrAbbrev[0]))
										   : (int32)(sizeof(kEnAbbrev) / sizeof(kEnAbbrev[0]));

			// Decodage UTF-8 -> codepoints (meme approche que NkG2P.cpp).
			NkVector<uint32> cps;
			{
				const char *data = text.Data();
				const usize total = (usize)text.Size();
				usize pos = 0;
				while (pos < total) {
					uint32 cp = 0;
					const usize consumed = encoding::utf8::NkDecodeChar(data + pos, cp);
					if (consumed == 0) { ++pos; continue; }
					cps.PushBack(cp);
					pos += consumed;
				}
			}
			const usize n = cps.Size();
			usize i = 0;

			while (i < n) {
				const uint32 raw = cps[(nk_size)i];

				if (IsAsciiWhitespace(raw)) {
					++i;
					continue;
				}

				// --- Nombre (entier ou decimal simple), signe optionnel. -----------
				if (IsAsciiDigit(raw) ||
					(raw == '-' && i + 1 < n && IsAsciiDigit(cps[(nk_size)(i + 1)]) &&
					 (i == 0 || IsAsciiWhitespace(cps[(nk_size)(i - 1)]) || IsPausePunct(cps[(nk_size)(i - 1)]) ||
					  IsOtherSeparator(cps[(nk_size)(i - 1)])))) {
					// --- Date/heure/ordinal LITTERAUX (avant le nombre simple ci-dessous) :
					// tentes UNIQUEMENT sur un chiffre de depart (pas sur '-'), cf.
					// ScanDateLiteral/ScanTimeLiteral/ScanOrdinalLiteral (formats couverts
					// documentes dans NkTextNorm.h). Si aucun ne matche (ou reconnu mais
					// invalide, ex. mois 13), repli SANS consommation sur le nombre simple.
					if (IsAsciiDigit(raw)) {
						NkString dtLit;
						usize consumed = ScanDateLiteral(cps, n, i, dtLit);
						if (consumed > 0) {
							NkString words = NkTextNorm::ExpandDateLiteral(dtLit, lang, dialect, yearReading);
							if (!words.Empty()) {
								out.PushBack(NkTextNormToken::MakeWord(words));
								i += consumed;
								continue;
							}
						} else {
							consumed = ScanTimeLiteral(cps, n, i, dtLit);
							if (consumed > 0) {
								// Pipeline par defaut : 24h (avec le fix minuit/midi fr) — le
								// format 12h/AM-PM n'est expose que via un appel direct a
								// ExpandTimeLiteral, car un litteral "3:05" isole dans un texte
								// ne porte aucun indice fiable pour choisir 12h vs 24h (limite
								// honnete, cf. NkTextNorm.h).
								NkString words = NkTextNorm::ExpandTimeLiteral(dtLit, lang);
								if (!words.Empty()) {
									out.PushBack(NkTextNormToken::MakeWord(words));
									i += consumed;
									continue;
								}
							}
						}
						NkString ordLit;
						consumed = ScanOrdinalLiteral(cps, n, i, lang, ordLit);
						if (consumed > 0) {
							NkString words = NkTextNorm::ExpandOrdinalLiteral(ordLit, lang, dialect);
							if (!words.Empty()) {
								out.PushBack(NkTextNormToken::MakeWord(words));
								i += consumed;
								continue;
							}
						}
					}
					usize start = i;
					if (raw == '-')
						++i;
					while (i < n && IsAsciiDigit(cps[(nk_size)i]))
						++i;
					// Separateur decimal ',' ou '.' flanque de chiffres des deux cotes.
					if (i < n && (cps[(nk_size)i] == ',' || cps[(nk_size)i] == '.') && i + 1 < n &&
						IsAsciiDigit(cps[(nk_size)(i + 1)])) {
						++i;
						while (i < n && IsAsciiDigit(cps[(nk_size)i]))
							++i;
					}
					// Reconstruit le literal ASCII (chiffres/signe/separateur uniquement).
					NkString literal;
					for (usize k = start; k < i; ++k)
						literal.Append((char)cps[(nk_size)k]);
					NkString words = NkTextNorm::ExpandNumberLiteral(literal, lang, dialect);
					if (!words.Empty())
						out.PushBack(NkTextNormToken::MakeWord(words));
					continue;
				}

				// --- Ponctuation generatrice de pause (fusion des runs). -----------
				if (IsPausePunct(raw)) {
					float32 maxMs = PauseDurationMs((char)raw);
					++i;
					while (i < n && (IsPausePunct(cps[(nk_size)i]) || IsAsciiWhitespace(cps[(nk_size)i]))) {
						if (IsPausePunct(cps[(nk_size)i])) {
							const float32 ms = PauseDurationMs((char)cps[(nk_size)i]);
							if (ms > maxMs) maxMs = ms;
						}
						++i;
					}
					out.PushBack(NkTextNormToken::MakePause(maxMs));
					continue;
				}

				// --- Autre separateur neutre (parentheses, tirets, guillemets...). --
				if (IsOtherSeparator(raw)) {
					++i;
					continue;
				}

				// --- Mot (lettres, apostrophe interne conservee pour NkG2P). -------
				if (IsAsciiLetter(raw) || raw > 127) {
					usize start = i;
					while (i < n) {
						const uint32 c = cps[(nk_size)i];
						if (IsAsciiWhitespace(c) || IsAsciiDigit(c) || IsPausePunct(c) || IsOtherSeparator(c))
							break;
						++i;
					}
					// Reconstruit le mot en UTF-8.
					NkString word;
					char buf[8];
					for (usize k = start; k < i; ++k) {
						const usize len = encoding::utf8::NkEncodeChar(cps[(nk_size)k], buf);
						word.Append(buf, (NkString::SizeType)len);
					}
					// Abreviation sourcee ? (comparaison sur le mot ENTIER, cf. SOURCES [5]).
					bool matched = false;
					bool matchedIsTitle = false;
					for (int32 k = 0; k < abbrevCount; ++k) {
						if (word.Compare(abbrevTable[k].from) == 0) {
							out.PushBack(NkTextNormToken::MakeWord(NkString(abbrevTable[k].to)));
							matched = true;
							matchedIsTitle = abbrevTable[k].isTitle;
							break;
						}
					}
					if (!matched) {
						out.PushBack(NkTextNormToken::MakeWord(word));
						continue;
					}

					// --- Desambiguisation abreviation vs fin de phrase (SOURCES
					// [abbrev-sbd]) : ne s'applique que si l'abreviation est IMMEDIATEMENT
					// suivie d'un '.' (le cas "M"/"etc" sans point du tout, ex. en toute
					// fin de token, n'a rien a desambiguiser).
					if (i < n && cps[(nk_size)i] == '.') {
						bool sentenceFinal;
						if (matchedIsTitle) {
							// Titres de civilite : TOUJOURS suivis d'un nom (definition meme
							// de la categorie), donc jamais une fin de phrase reelle par
							// construction -> pause courte (equivalent virgule), pas la pause
							// longue de fin de phrase. COMPROMIS ASSUME : si un titre termine
							// vraiment le texte (ex. "...le Dr." en toute fin), la pause est
							// sous-estimee (courte au lieu de longue) ; accepte car les titres
							// sont, en pratique, presque toujours suivis d'un nom (SOURCES
							// [abbrev-sbd]).
							sentenceFinal = false;
						} else {
							// "etc." et assimiles : heuristique inspiree de la litterature SBD
							// (Kiss & Strunk / Palmer & Hearst, cf. NkTextNorm.h SOURCES
							// [abbrev-sbd]) : le caractere qui suit le point (apres un espace
							// optionnel) tranche. Majuscule -> tres majoritairement une
							// NOUVELLE phrase (≈90% des points sont des fins de phrase selon
							// la statistique du corpus Brown citee par ces sources) -> pause
							// longue. Minuscule (ou rien) -> l'abreviation continue la meme
							// phrase -> pause courte.
							usize p = i + 1;
							while (p < n && IsAsciiWhitespace(cps[(nk_size)p]))
								++p;
							sentenceFinal = (p < n) && IsAsciiUpper(cps[(nk_size)p]);
						}

						const float32 baselineMs = sentenceFinal ? PauseDurationMs('.') : PauseDurationMs(',');
						float32 maxMs = baselineMs;
						++i; // consomme le '.' de l'abreviation.
						while (i < n && (IsPausePunct(cps[(nk_size)i]) || IsAsciiWhitespace(cps[(nk_size)i]))) {
							if (IsPausePunct(cps[(nk_size)i])) {
								const float32 ms = PauseDurationMs((char)cps[(nk_size)i]);
								if (ms > maxMs)
									maxMs = ms;
							}
							++i;
						}
						out.PushBack(NkTextNormToken::MakePause(maxMs));
					}
					continue;
				}

				// Codepoint non gere (residuel) : ignore silencieusement, comme NkG2P.
				++i;
			}

			return out;
		}

		NkString NkTextNorm::NormalizeToText(const NkString &text, NkTextNormLang lang, NkFrNumberDialect dialect,
											  NkTextNormYearReading yearReading) {
			NkVector<NkTextNormToken> tokens = Normalize(text, lang, dialect, yearReading);
			NkString out;
			bool first = true;
			for (nk_size i = 0; i < tokens.Size(); ++i) {
				if (tokens[i].kind != NkTextNormTokenKind::Word)
					continue;
				if (!first)
					out.Append(" ");
				first = false;
				out.Append(tokens[i].text);
			}
			return out;
		}

		// =========================================================================
		// Auto-test headless : nombres reels (entiers/decimaux/negatifs), pauses,
		// abreviations. Voir NkTextNorm.h SOURCES pour la justification des valeurs
		// attendues.
		// =========================================================================
		bool NkTextNorm::SelfTest() {
			bool ok = true;

			auto check = [&ok](bool cond, const char * /*label*/) {
				if (!cond)
					ok = false;
			};

			// --- (1) fr : zero. ---
			check(NkTextNorm::NumberToWords(0, NkTextNormLang::Fr).Compare("zero") == 0, "fr-zero");

			// --- (2) fr : 21 -> "vingt et un" (SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(21, NkTextNormLang::Fr).Compare("vingt et un") == 0, "fr-21");

			// --- (3) fr : 70 -> "soixante-dix" (SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(70, NkTextNormLang::Fr).Compare("soixante-dix") == 0, "fr-70");

			// --- (4) fr : 71 -> "soixante et onze" (SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(71, NkTextNormLang::Fr).Compare("soixante et onze") == 0, "fr-71");

			// --- (5) fr : 80 -> "quatre-vingts" (pluriel, fin de nombre, SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(80, NkTextNormLang::Fr).Compare("quatre-vingts") == 0, "fr-80");

			// --- (6) fr : 81 -> "quatre-vingt-un" (SANS "et", SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(81, NkTextNormLang::Fr).Compare("quatre-vingt-un") == 0, "fr-81");

			// --- (7) fr : 91 -> "quatre-vingt-onze" (SANS "et", SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(91, NkTextNormLang::Fr).Compare("quatre-vingt-onze") == 0, "fr-91");

			// --- (8) fr : 200 -> "deux cents" (pluriel, rien apres, SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(200, NkTextNormLang::Fr).Compare("deux cents") == 0, "fr-200");

			// --- (9) fr : 201 -> "deux cent un" (pas de 's', quelque chose suit). ---
			check(NkTextNorm::NumberToWords(201, NkTextNormLang::Fr).Compare("deux cent un") == 0, "fr-201");

			// --- (10) fr : 123 -> "cent vingt-trois". ---
			check(NkTextNorm::NumberToWords(123, NkTextNormLang::Fr).Compare("cent vingt-trois") == 0, "fr-123");

			// --- (11) fr : 1000 -> "mille" (jamais "un mille", SOURCES [2][3]). ---
			check(NkTextNorm::NumberToWords(1000, NkTextNormLang::Fr).Compare("mille") == 0, "fr-1000");

			// --- (12) fr : 2026 -> "deux mille vingt-six". ---
			check(NkTextNorm::NumberToWords(2026, NkTextNormLang::Fr).Compare("deux mille vingt-six") == 0, "fr-2026");

			// --- (13) fr : 1000000 -> "un million" (contrairement a "mille", SOURCES [2]). ---
			check(NkTextNorm::NumberToWords(1000000, NkTextNormLang::Fr).Compare("un million") == 0, "fr-1000000");

			// --- (14) fr : nombre negatif -> "moins ...". ---
			check(NkTextNorm::NumberToWords(-5, NkTextNormLang::Fr).Compare("moins cinq") == 0, "fr-neg5");

			// --- (15) en : 0. ---
			check(NkTextNorm::NumberToWords(0, NkTextNormLang::En).Compare("zero") == 0, "en-zero");

			// --- (16) en : 123 -> "one hundred twenty-three" (style americain, SOURCES [1]). ---
			check(NkTextNorm::NumberToWords(123, NkTextNormLang::En).Compare("one hundred twenty-three") == 0, "en-123");

			// --- (17) en : 1000 -> "one thousand" ("one" jamais omis, contrairement au fr). ---
			check(NkTextNorm::NumberToWords(1000, NkTextNormLang::En).Compare("one thousand") == 0, "en-1000");

			// --- (18) en : 2026 -> "two thousand twenty-six". ---
			check(NkTextNorm::NumberToWords(2026, NkTextNormLang::En).Compare("two thousand twenty-six") == 0, "en-2026");

			// --- (19) en : 1000000000 -> "one billion" (echelle courte, SOURCES [1]). ---
			check(NkTextNorm::NumberToWords(1000000000LL, NkTextNormLang::En).Compare("one billion") == 0, "en-1e9");

			// --- (20) fr : decimal "3,14" -> "trois virgule un quatre" (SOURCES [4]). ---
			check(NkTextNorm::ExpandNumberLiteral(NkString("3,14"), NkTextNormLang::Fr)
					  .Compare("trois virgule un quatre") == 0,
				  "fr-decimal");

			// --- (21) en : decimal "3.14" -> "three point one four" (SOURCES [4]). ---
			check(NkTextNorm::ExpandNumberLiteral(NkString("3.14"), NkTextNormLang::En)
					  .Compare("three point one four") == 0,
				  "en-decimal");

			// --- (22) fr : "3,05" -> "trois virgule zero cinq" (zero non significatif
			//     preserve, SOURCES [4]). ---
			check(NkTextNorm::ExpandNumberLiteral(NkString("3,05"), NkTextNormLang::Fr)
					  .Compare("trois virgule zero cinq") == 0,
				  "fr-decimal-leading-zero");

			// --- (23) Ponctuation -> pause : "Bonjour, le monde." fr -> tokens
			//     [Word "bonjour"] [Pause 150] [Word "le"] [Word "monde"] [Pause 400]. ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("Bonjour, le monde."), NkTextNormLang::Fr);
				check(toks.Size() == 5, "fr-punct-count");
				if (toks.Size() == 5) {
					check(toks[0].kind == NkTextNormTokenKind::Word && toks[0].text.Compare("Bonjour") == 0, "fr-punct-0");
					check(toks[1].kind == NkTextNormTokenKind::Pause && toks[1].pauseMs == 150.0f, "fr-punct-1");
					check(toks[2].kind == NkTextNormTokenKind::Word && toks[2].text.Compare("le") == 0, "fr-punct-2");
					check(toks[3].kind == NkTextNormTokenKind::Word && toks[3].text.Compare("monde") == 0, "fr-punct-3");
					check(toks[4].kind == NkTextNormTokenKind::Pause && toks[4].pauseMs == 400.0f, "fr-punct-4");
				}
			}

			// --- (24) Ponctuation fusionnee : "Vraiment ?!" -> une seule pause,
			//     duree = max(400,400) = 400 (fusion de runs). ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("Vraiment ?!"), NkTextNormLang::Fr);
				check(toks.Size() == 2, "fr-fusion-count");
				if (toks.Size() == 2)
					check(toks[1].kind == NkTextNormTokenKind::Pause && toks[1].pauseMs == 400.0f, "fr-fusion-pause");
			}

			// --- (25) Abreviation fr : "Dr" -> "Docteur" (SOURCES [5], token entier,
			//     pas de collision avec un mot ordinaire). ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("Dr Martin"), NkTextNormLang::Fr);
				check(toks.Size() == 2, "fr-abbrev-count");
				if (toks.Size() == 2)
					check(toks[0].text.Compare("Docteur") == 0, "fr-abbrev-dr");
			}

			// --- (26) Pas de faux positif : "stereo" n'est PAS confondu avec "St"
			//     (SOURCES [5], mitigation token entier). ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("stereo"), NkTextNormLang::En);
				check(toks.Size() == 1, "en-no-false-positive-count");
				if (toks.Size() == 1)
					check(toks[0].text.Compare("stereo") == 0, "en-no-false-positive");
			}

			// --- (27) NormalizeToText : nombre + mot -> texte a plat pret pour
			//     NkG2P::ToPhonemes (verifie juste l'assemblage, pas le G2P lui-meme,
			//     qui a son propre SelfTest dans NkG2P.cpp). ---
			{
				NkString flat = NkTextNorm::NormalizeToText(NkString("J'ai 21 ans"), NkTextNormLang::Fr);
				check(flat.Compare("J'ai vingt et un ans") == 0, "fr-flatten");
			}

			// =====================================================================
			// Nouveaux cas (2026-07-25) : fix accord cent/quatre-vingts devant
			// million/milliard (SOURCES [3bis]), ordinaux fr/en (SOURCES [7][8]),
			// dates/heures fr/en (SOURCES [9]).
			// =====================================================================

			// --- (28) fr : 80 000 000 -> "quatre-vingts millions" (accord devant un
			//     NOM, SOURCES [3bis], fix de la limite non resolue precedente). ---
			check(NkTextNorm::NumberToWords(80000000LL, NkTextNormLang::Fr).Compare("quatre-vingts millions") == 0,
				  "fr-80M");

			// --- (29) fr : 200 000 000 -> "deux cents millions" (idem pour "cent"). ---
			check(NkTextNorm::NumberToWords(200000000LL, NkTextNormLang::Fr).Compare("deux cents millions") == 0,
				  "fr-200M");

			// --- (30) fr : 280 000 000 -> "deux cent quatre-vingts millions" (cent SANS
			//     's' car suivi d'un reste non nul, quatre-vingts AVEC 's' car dernier
			//     element du groupe million, SOURCES [3bis]). ---
			check(NkTextNorm::NumberToWords(280000000LL, NkTextNormLang::Fr)
					  .Compare("deux cent quatre-vingts millions") == 0,
				  "fr-280M");

			// --- (31) fr : 80 000 -> "quatre-vingt mille" (NON-REGRESSION : "mille" est
			//     un adjectif numeral invariable, PAS un nom -> pas d'accord, SOURCES
			//     [3bis]). ---
			check(NkTextNorm::NumberToWords(80000LL, NkTextNormLang::Fr).Compare("quatre-vingt mille") == 0,
				  "fr-80k");

			// --- (32) fr : 300 000 -> "trois cent mille" (NON-REGRESSION : "cent" reste
			//     invariable devant "mille", SOURCES [3bis]). ---
			check(NkTextNorm::NumberToWords(300000LL, NkTextNormLang::Fr).Compare("trois cent mille") == 0,
				  "fr-300k");

			// --- (33) fr : 800 000 000 -> "huit cents millions". ---
			check(NkTextNorm::NumberToWords(800000000LL, NkTextNormLang::Fr).Compare("huit cents millions") == 0,
				  "fr-800M");

			// --- (34) Ordinal fr : 1 -> "premier" (irregulier, SOURCES [7]). ---
			check(NkTextNorm::OrdinalToWords(1, NkTextNormLang::Fr).Compare("premier") == 0, "ord-fr-1");

			// --- (35) Ordinal fr : 2 -> "deuxieme". ---
			check(NkTextNorm::OrdinalToWords(2, NkTextNormLang::Fr).Compare("deuxieme") == 0, "ord-fr-2");

			// --- (36) Ordinal fr : 5 -> "cinquieme" (u intercalaire, SOURCES [7]). ---
			check(NkTextNorm::OrdinalToWords(5, NkTextNormLang::Fr).Compare("cinquieme") == 0, "ord-fr-5");

			// --- (37) Ordinal fr : 9 -> "neuvieme" (f -> v, SOURCES [7]). ---
			check(NkTextNorm::OrdinalToWords(9, NkTextNormLang::Fr).Compare("neuvieme") == 0, "ord-fr-9");

			// --- (38) Ordinal fr : 21 -> "vingt et unieme" (jamais "vingt et premier",
			//     SOURCES [7]). ---
			check(NkTextNorm::OrdinalToWords(21, NkTextNormLang::Fr).Compare("vingt et unieme") == 0, "ord-fr-21");

			// --- (39) Ordinal fr : 80 -> "quatre-vingtieme" (pas de 's' a l'ordinal). ---
			check(NkTextNorm::OrdinalToWords(80, NkTextNormLang::Fr).Compare("quatre-vingtieme") == 0, "ord-fr-80");

			// --- (40) Ordinal fr : 81 -> "quatre-vingt-unieme". ---
			check(NkTextNorm::OrdinalToWords(81, NkTextNormLang::Fr).Compare("quatre-vingt-unieme") == 0,
				  "ord-fr-81");

			// --- (41) Ordinal fr : 91 -> "quatre-vingt-onzieme". ---
			check(NkTextNorm::OrdinalToWords(91, NkTextNormLang::Fr).Compare("quatre-vingt-onzieme") == 0,
				  "ord-fr-91");

			// --- (42) Ordinal fr : 100 -> "centieme". ---
			check(NkTextNorm::OrdinalToWords(100, NkTextNormLang::Fr).Compare("centieme") == 0, "ord-fr-100");

			// --- (43) Ordinal fr : 200 -> "deux centieme" (marque plurielle du cardinal
			//     PAS repercutee sur l'ordinal, SOURCES [7]). ---
			check(NkTextNorm::OrdinalToWords(200, NkTextNormLang::Fr).Compare("deux centieme") == 0, "ord-fr-200");

			// --- (44) Ordinal fr : 1000 -> "millieme". ---
			check(NkTextNorm::OrdinalToWords(1000, NkTextNormLang::Fr).Compare("millieme") == 0, "ord-fr-1000");

			// --- (45) Ordinal fr : 2026 -> "deux mille vingt-sixieme". ---
			check(NkTextNorm::OrdinalToWords(2026, NkTextNormLang::Fr).Compare("deux mille vingt-sixieme") == 0,
				  "ord-fr-2026");

			// --- (46) Ordinal en : 1 -> "first" (irregulier, SOURCES [8]). ---
			check(NkTextNorm::OrdinalToWords(1, NkTextNormLang::En).Compare("first") == 0, "ord-en-1");

			// --- (47) Ordinal en : 2 -> "second". ---
			check(NkTextNorm::OrdinalToWords(2, NkTextNormLang::En).Compare("second") == 0, "ord-en-2");

			// --- (48) Ordinal en : 3 -> "third". ---
			check(NkTextNorm::OrdinalToWords(3, NkTextNormLang::En).Compare("third") == 0, "ord-en-3");

			// --- (49) Ordinal en : 5 -> "fifth". ---
			check(NkTextNorm::OrdinalToWords(5, NkTextNormLang::En).Compare("fifth") == 0, "ord-en-5");

			// --- (50) Ordinal en : 9 -> "ninth". ---
			check(NkTextNorm::OrdinalToWords(9, NkTextNormLang::En).Compare("ninth") == 0, "ord-en-9");

			// --- (51) Ordinal en : 12 -> "twelfth". ---
			check(NkTextNorm::OrdinalToWords(12, NkTextNormLang::En).Compare("twelfth") == 0, "ord-en-12");

			// --- (52) Ordinal en : 20 -> "twentieth" (-y -> -ieth, SOURCES [8]). ---
			check(NkTextNorm::OrdinalToWords(20, NkTextNormLang::En).Compare("twentieth") == 0, "ord-en-20");

			// --- (53) Ordinal en : 21 -> "twenty-first" (irregulier propage au compose). ---
			check(NkTextNorm::OrdinalToWords(21, NkTextNormLang::En).Compare("twenty-first") == 0, "ord-en-21");

			// --- (54) Ordinal en : 100 -> "one hundredth". ---
			check(NkTextNorm::OrdinalToWords(100, NkTextNormLang::En).Compare("one hundredth") == 0, "ord-en-100");

			// --- (55) ExpandOrdinalLiteral fr : "1er" -> "premier". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1er"), NkTextNormLang::Fr).Compare("premier") == 0,
				  "ordlit-fr-1er");

			// --- (56) ExpandOrdinalLiteral fr : "1re" -> "premiere" (MISE A JOUR
			//     2026-07-25 : le feminin est desormais distingue, cf. SOURCES
			//     [fem-ord] ; cf. aussi le nouveau test "fem-ord-1re" plus bas qui
			//     couvre le meme cas explicitement pour cette mission). ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1re"), NkTextNormLang::Fr).Compare("premiere") == 0,
				  "ordlit-fr-1re");

			// --- (57) ExpandOrdinalLiteral fr : "21e" -> "vingt et unieme". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("21e"), NkTextNormLang::Fr)
					  .Compare("vingt et unieme") == 0,
				  "ordlit-fr-21e");

			// --- (58) ExpandOrdinalLiteral fr : "2eme" -> "deuxieme" (variante
			//     informelle sans accent, cf. ScanOrdinalLiteral). ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("2eme"), NkTextNormLang::Fr).Compare("deuxieme") == 0,
				  "ordlit-fr-2eme");

			// --- (59) ExpandOrdinalLiteral en : "1st" -> "first". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1st"), NkTextNormLang::En).Compare("first") == 0,
				  "ordlit-en-1st");

			// --- (60) ExpandOrdinalLiteral en : "2nd" -> "second". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("2nd"), NkTextNormLang::En).Compare("second") == 0,
				  "ordlit-en-2nd");

			// --- (61) ExpandOrdinalLiteral en : "3rd" -> "third". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("3rd"), NkTextNormLang::En).Compare("third") == 0,
				  "ordlit-en-3rd");

			// --- (62) ExpandOrdinalLiteral en : "21st" -> "twenty-first". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("21st"), NkTextNormLang::En)
					  .Compare("twenty-first") == 0,
				  "ordlit-en-21st");

			// --- (63) ExpandOrdinalLiteral en : "11th" -> "eleventh" (exception
			//     11/12/13 : suffixe non revalide, seule la valeur compte ici). ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("11th"), NkTextNormLang::En).Compare("eleventh") == 0,
				  "ordlit-en-11th");

			// --- (64) ExpandDateLiteral fr : "12/03/2026" (JJ/MM/AAAA) ->
			//     "douze mars deux mille vingt-six" (SOURCES [9]). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("12/03/2026"), NkTextNormLang::Fr)
					  .Compare("douze mars deux mille vingt-six") == 0,
				  "date-fr-12-03-2026");

			// --- (65) ExpandDateLiteral fr : "01/01/2026" -> "premier janvier deux
			//     mille vingt-six" (1er du mois = ordinal, SOURCES [9]). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("01/01/2026"), NkTextNormLang::Fr)
					  .Compare("premier janvier deux mille vingt-six") == 0,
				  "date-fr-premier");

			// --- (66) ExpandDateLiteral en : "03/12/2026" (MM/DD/AAAA) -> "march
			//     twelfth two thousand twenty-six" (jour ORDINAL, SOURCES [9]). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("03/12/2026"), NkTextNormLang::En)
					  .Compare("march twelfth two thousand twenty-six") == 0,
				  "date-en-03-12-2026");

			// --- (67) ExpandDateLiteral : mois invalide (13) -> chaine vide. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("31/13/2026"), NkTextNormLang::Fr).Empty(),
				  "date-invalid-month");

			// --- (68) ExpandDateLiteral : annee sur 2 chiffres -> MISE A JOUR
			//     2026-07-25 : desormais reconnue via le windowing POSIX (SOURCES
			//     [2digit-year], 26 <= 68 -> 2026), cf. aussi les nouveaux tests
			//     "2digit-year-*" plus bas qui couvrent ce point explicitement pour
			//     cette mission. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("12/03/26"), NkTextNormLang::Fr)
					  .Compare("douze mars deux mille vingt-six") == 0,
				  "date-2digit-year");

			// --- (69) ExpandTimeLiteral fr : "15h30" -> "quinze heures trente"
			//     (SOURCES [9]). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("15h30"), NkTextNormLang::Fr)
					  .Compare("quinze heures trente") == 0,
				  "time-fr-15h30");

			// --- (70) ExpandTimeLiteral fr : "1h00" -> "une heure" ("heure" feminin,
			//     minute nulle omise, SOURCES [9]). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("1h00"), NkTextNormLang::Fr).Compare("une heure") == 0,
				  "time-fr-1h00");

			// --- (71) ExpandTimeLiteral fr : "15:05" -> "quinze heures cinq" (accepte
			//     aussi le separateur ':'). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("15:05"), NkTextNormLang::Fr)
					  .Compare("quinze heures cinq") == 0,
				  "time-fr-15h05");

			// --- (72) ExpandTimeLiteral en : "15:30" -> "fifteen thirty". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("15:30"), NkTextNormLang::En)
					  .Compare("fifteen thirty") == 0,
				  "time-en-15-30");

			// --- (73) ExpandTimeLiteral en : "3:05" -> "three oh five" (convention
			//     "oh" pour minute < 10, SOURCES [9]). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("3:05"), NkTextNormLang::En)
					  .Compare("three oh five") == 0,
				  "time-en-3-05");

			// --- (74) ExpandTimeLiteral en : "9:00" -> "nine o clock". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("9:00"), NkTextNormLang::En).Compare("nine o clock") == 0,
				  "time-en-9-00");

			// --- (75) ExpandTimeLiteral : heure invalide (25) -> chaine vide. ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("25:00"), NkTextNormLang::Fr).Empty(), "time-invalid-hour");

			// --- (76) Normalize integration : la date "12/03/2026" est reconnue comme
			//     UN SEUL token (pas 3 nombres separes par le G2P-hostile '/'). ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Le 12/03/2026 il pleut."), NkTextNormLang::Fr);
				check(toks.Size() == 5, "fr-date-integration-count");
				if (toks.Size() == 5)
					check(toks[1].kind == NkTextNormTokenKind::Word &&
							  toks[1].text.Compare("douze mars deux mille vingt-six") == 0,
						  "fr-date-integration-text");
			}

			// --- (77) Normalize integration : l'heure "15h30" est reconnue comme UN
			//     SEUL token. ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Rendez-vous a 15h30."), NkTextNormLang::Fr);
				check(toks.Size() == 5, "fr-time-integration-count");
				if (toks.Size() == 5)
					check(toks[3].kind == NkTextNormTokenKind::Word &&
							  toks[3].text.Compare("quinze heures trente") == 0,
						  "fr-time-integration-text");
			}

			// --- (78) Normalize integration : l'ordinal "21e" est reconnu comme UN
			//     SEUL token developpe ("vingt et unieme"), pas un nombre "21" + un
			//     mot residuel "e". ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("Le 21e siecle."), NkTextNormLang::Fr);
				check(toks.Size() == 4, "fr-ordinal-integration-count");
				if (toks.Size() == 4)
					check(toks[1].kind == NkTextNormTokenKind::Word &&
							  toks[1].text.Compare("vingt et unieme") == 0,
						  "fr-ordinal-integration-text");
			}

			// =====================================================================
			// Nouveaux cas (2026-07-25, Phase 8 "limites comblees") : variante
			// regionale fr (SOURCES [dialect]), desambiguisation abreviation/fin de
			// phrase (SOURCES [abbrev-sbd]), ordinal feminin "1re"/"premiere"
			// (SOURCES [fem-ord]), minuit/midi + 12h/AM-PM (SOURCES
			// [minuit-midi]/[ampm]), annee sur 2 chiffres (SOURCES [2digit-year]),
			// lecture de l'annee par paires en anglais (SOURCES [year-pairs]),
			// validite calendaire reelle (SOURCES [leap-year]).
			// =====================================================================

			// --- (1) Variante regionale fr (SOURCES [dialect]) --------------------

			// --- (79) fr BelgeSuisse : 70 -> "septante". ---
			check(NkTextNorm::NumberToWords(70, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("septante") == 0,
				  "dialect-fr-70");

			// --- (80) fr BelgeSuisse : 71 -> "septante et un" (formation reguliere,
			//     PAS "soixante et onze"). ---
			check(NkTextNorm::NumberToWords(71, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("septante et un") == 0,
				  "dialect-fr-71");

			// --- (81) fr BelgeSuisse : 72 -> "septante-deux". ---
			check(NkTextNorm::NumberToWords(72, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("septante-deux") == 0,
				  "dialect-fr-72");

			// --- (82) fr BelgeSuisse : 80 -> "huitante" (forme des cantons de Vaud/
			//     Valais/Fribourg, jamais pluralisee contrairement a "quatre-vingts"). ---
			check(NkTextNorm::NumberToWords(80, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("huitante") == 0,
				  "dialect-fr-80");

			// --- (83) fr BelgeSuisse : 81 -> "huitante et un". ---
			check(NkTextNorm::NumberToWords(81, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("huitante et un") == 0,
				  "dialect-fr-81");

			// --- (84) fr BelgeSuisse : 90 -> "nonante". ---
			check(NkTextNorm::NumberToWords(90, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("nonante") == 0,
				  "dialect-fr-90");

			// --- (85) fr BelgeSuisse : 99 -> "nonante-neuf". ---
			check(NkTextNorm::NumberToWords(99, NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("nonante-neuf") == 0,
				  "dialect-fr-99");

			// --- (86) NON-REGRESSION : Standard reste le defaut (2 arguments,
			//     comportement inchange, SOURCES [3]). ---
			check(NkTextNorm::NumberToWords(70, NkTextNormLang::Fr).Compare("soixante-dix") == 0,
				  "dialect-fr-standard-default");

			// --- (87) Ordinal dialecte : 70 -> "septantieme" (regle d'elision du 'e'
			//     muet generique, reutilisee sans code specifique). ---
			check(NkTextNorm::OrdinalToWords(70, NkTextNormLang::Fr, false, NkFrNumberDialect::BelgeSuisse)
					  .Compare("septantieme") == 0,
				  "dialect-ord-fr-70");

			// --- (88) ExpandNumberLiteral avec dialecte : "80" fr BelgeSuisse ->
			//     "huitante". ---
			check(NkTextNorm::ExpandNumberLiteral(NkString("80"), NkTextNormLang::Fr, NkFrNumberDialect::BelgeSuisse)
					  .Compare("huitante") == 0,
				  "dialect-expandlit-80");

			// --- (2) Desambiguisation abreviation vs fin de phrase (SOURCES
			//     [abbrev-sbd]) --------------------------------------------------

			// --- (89) fr : titre "M." suivi d'un nom -> pause COURTE (150ms), jamais
			//     la pause longue de fin de phrase (un titre est toujours suivi d'un
			//     nom, cf. SOURCES [abbrev-sbd]). ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("M. Dupont est arrive."), NkTextNormLang::Fr);
				check(toks.Size() == 6, "abbrev-sbd-fr-title-count");
				if (toks.Size() == 6)
					check(toks[1].kind == NkTextNormTokenKind::Pause && toks[1].pauseMs == 150.0f,
						  "abbrev-sbd-fr-title-pause");
			}

			// --- (90) fr : titre "Dr." suivi d'un nom -> meme pause courte. ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Le Dr. Martin viendra."), NkTextNormLang::Fr);
				check(toks.Size() == 6, "abbrev-sbd-fr-dr-count");
				if (toks.Size() == 6)
					check(toks[2].kind == NkTextNormTokenKind::Pause && toks[2].pauseMs == 150.0f,
						  "abbrev-sbd-fr-dr-pause");
			}

			// --- (91) fr : "etc." suivi d'un MOT MAJUSCULE -> heuristique tranche
			//     pour une fin de phrase reelle -> pause LONGUE (400ms), cf. SOURCES
			//     [abbrev-sbd] (~90% des points sont des fins de phrase). ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Il aime les fruits etc. Ensuite il part."), NkTextNormLang::Fr);
				check(toks.Size() == 10, "abbrev-sbd-fr-etc-upper-count");
				if (toks.Size() == 10)
					check(toks[5].kind == NkTextNormTokenKind::Pause && toks[5].pauseMs == 400.0f,
						  "abbrev-sbd-fr-etc-upper-pause");
			}

			// --- (92) fr : "etc." suivi d'un mot MINUSCULE -> l'abreviation continue
			//     la meme phrase -> pause COURTE (150ms). ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(
					NkString("Il a mange des fruits etc. et boit de l'eau."), NkTextNormLang::Fr);
				check(toks.Size() == 12, "abbrev-sbd-fr-etc-lower-count");
				if (toks.Size() == 12)
					check(toks[6].kind == NkTextNormTokenKind::Pause && toks[6].pauseMs == 150.0f,
						  "abbrev-sbd-fr-etc-lower-pause");
			}

			// --- (93) en : titre "Mr." suivi d'un nom -> pause courte. ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Mr. Smith arrived."), NkTextNormLang::En);
				check(toks.Size() == 5, "abbrev-sbd-en-mr-count");
				if (toks.Size() == 5)
					check(toks[1].kind == NkTextNormTokenKind::Pause && toks[1].pauseMs == 150.0f,
						  "abbrev-sbd-en-mr-pause");
			}

			// --- (94) en : titre "Dr." suivi d'un nom -> pause courte. ---
			{
				NkVector<NkTextNormToken> toks =
					NkTextNorm::Normalize(NkString("Dr. Brown left."), NkTextNormLang::En);
				check(toks.Size() == 5, "abbrev-sbd-en-dr-count");
				if (toks.Size() == 5)
					check(toks[1].kind == NkTextNormTokenKind::Pause && toks[1].pauseMs == 150.0f,
						  "abbrev-sbd-en-dr-pause");
			}

			// --- (3) Ordinal feminin "1re"/"premiere" (SOURCES [fem-ord]) ----------

			// --- (95) OrdinalToWords fr : 1, feminine=true -> "premiere". ---
			check(NkTextNorm::OrdinalToWords(1, NkTextNormLang::Fr, true).Compare("premiere") == 0,
				  "fem-ord-direct");

			// --- (96) ExpandOrdinalLiteral fr : "1re" -> "premiere". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1re"), NkTextNormLang::Fr).Compare("premiere") == 0,
				  "fem-ord-1re");

			// --- (97) ExpandOrdinalLiteral fr : "1ere" (rendu ASCII de "1ère") ->
			//     "premiere". ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1ere"), NkTextNormLang::Fr).Compare("premiere") == 0,
				  "fem-ord-1ere");

			// --- (98) NON-REGRESSION : ExpandOrdinalLiteral fr "1er" -> "premier"
			//     (masculin inchange). ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("1er"), NkTextNormLang::Fr).Compare("premier") == 0,
				  "fem-ord-1er-non-regression");

			// --- (99) NON-REGRESSION : le feminin ne s'applique QU'a la valeur 1 -
			//     "21e" reste "vingt et unieme" (epicene, SOURCES [7]). ---
			check(NkTextNorm::ExpandOrdinalLiteral(NkString("21e"), NkTextNormLang::Fr)
					  .Compare("vingt et unieme") == 0,
				  "fem-ord-21e-non-regression");

			// --- (100) OrdinalToWords fr : 2, feminine=true -> "deuxieme" (parametre
			//     feminine SANS EFFET au-dela de 1, epicene, SOURCES [7]/[fem-ord]). ---
			check(NkTextNorm::OrdinalToWords(2, NkTextNormLang::Fr, true).Compare("deuxieme") == 0,
				  "fem-ord-2-epicene");

			// --- (101) Normalize integration : "1re" reconnu comme UN SEUL token
			//     developpe ("premiere"). ---
			{
				NkVector<NkTextNormToken> toks = NkTextNorm::Normalize(NkString("La 1re place."), NkTextNormLang::Fr);
				check(toks.Size() == 4, "fem-ord-integration-count");
				if (toks.Size() == 4)
					check(toks[1].kind == NkTextNormTokenKind::Word && toks[1].text.Compare("premiere") == 0,
						  "fem-ord-integration-text");
			}

			// --- (4) Minuit/midi + 12h/AM-PM (SOURCES [minuit-midi]/[ampm]) --------

			// --- (102) ExpandTimeLiteral fr : "0h00" -> "minuit". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("0h00"), NkTextNormLang::Fr).Compare("minuit") == 0,
				  "minuit-0h00");

			// --- (103) ExpandTimeLiteral fr : "12h00" -> "midi". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("12h00"), NkTextNormLang::Fr).Compare("midi") == 0,
				  "midi-12h00");

			// --- (104) ExpandTimeLiteral fr : "0h15" -> "minuit quinze" (minute non
			//     nulle ajoutee normalement apres "minuit"). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("0h15"), NkTextNormLang::Fr).Compare("minuit quinze") == 0,
				  "minuit-0h15");

			// --- (105) ExpandTimeLiteral fr : "12h30" -> "midi trente". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("12h30"), NkTextNormLang::Fr).Compare("midi trente") == 0,
				  "midi-12h30");

			// --- (106) NON-REGRESSION : "1h00" fr (format H24 explicite, ignore en
			//     francais) reste "une heure". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("1h00"), NkTextNormLang::Fr, NkTextNormTimeFormat::H24)
					  .Compare("une heure") == 0,
				  "time-fr-1h00-explicit-format");

			// --- (107) ExpandTimeLiteral en H12 : "0:00" -> "twelve o clock am"
			//     (minuit = 12 AM, SOURCES [ampm]). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("0:00"), NkTextNormLang::En, NkTextNormTimeFormat::H12)
					  .Compare("twelve o clock am") == 0,
				  "ampm-en-0-00");

			// --- (108) ExpandTimeLiteral en H12 : "12:00" -> "twelve o clock pm"
			//     (midi = 12 PM, SOURCES [ampm]). ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("12:00"), NkTextNormLang::En, NkTextNormTimeFormat::H12)
					  .Compare("twelve o clock pm") == 0,
				  "ampm-en-12-00");

			// --- (109) ExpandTimeLiteral en H12 : "13:05" -> "one oh five pm". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("13:05"), NkTextNormLang::En, NkTextNormTimeFormat::H12)
					  .Compare("one oh five pm") == 0,
				  "ampm-en-13-05");

			// --- (110) ExpandTimeLiteral en H12 : "23:30" -> "eleven thirty pm". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("23:30"), NkTextNormLang::En, NkTextNormTimeFormat::H12)
					  .Compare("eleven thirty pm") == 0,
				  "ampm-en-23-30");

			// --- (111) NON-REGRESSION : "9:00" en H24 explicite reste "nine o
			//     clock". ---
			check(NkTextNorm::ExpandTimeLiteral(NkString("9:00"), NkTextNormLang::En, NkTextNormTimeFormat::H24)
					  .Compare("nine o clock") == 0,
				  "time-en-9-00-explicit-format");

			// --- (5) Annee sur 2 chiffres (SOURCES [2digit-year]) -------------------

			// --- (112) ExpandDateLiteral fr : "12/03/26" -> annee 26 <= 68 -> 2026. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("12/03/26"), NkTextNormLang::Fr)
					  .Compare("douze mars deux mille vingt-six") == 0,
				  "2digit-year-26");

			// --- (113) ExpandDateLiteral fr : "12/03/95" -> annee 95 >= 69 -> 1995. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("12/03/95"), NkTextNormLang::Fr)
					  .Compare("douze mars mille neuf cent quatre-vingt-quinze") == 0,
				  "2digit-year-95");

			// --- (114) ExpandDateLiteral fr : "01/01/00" -> pivot bas -> 2000. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("01/01/00"), NkTextNormLang::Fr)
					  .Compare("premier janvier deux mille") == 0,
				  "2digit-year-00");

			// --- (115) ExpandDateLiteral fr : "01/01/68" -> borne haute du pivot ->
			//     2068 (POSIX : 00-68 -> 20xx). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("01/01/68"), NkTextNormLang::Fr)
					  .Compare("premier janvier deux mille soixante-huit") == 0,
				  "2digit-year-68");

			// --- (116) ExpandDateLiteral fr : "01/01/69" -> borne basse de l'autre
			//     cote du pivot -> 1969 (POSIX : 69-99 -> 19xx). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("01/01/69"), NkTextNormLang::Fr)
					  .Compare("premier janvier mille neuf cent soixante-neuf") == 0,
				  "2digit-year-69");

			// --- (117) ExpandDateLiteral : annee sur 3 chiffres -> non reconnue
			//     (seuls 2 ou 4 chiffres sont valides, cf. ScanDateLiteral). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("12/03/202"), NkTextNormLang::Fr).Empty(),
				  "2digit-year-3digit-rejected");

			// --- (6) Lecture de l'annee par paires en anglais (SOURCES
			//     [year-pairs]) ------------------------------------------------------

			// --- (118) YearToWords en Full : 1984 -> cardinal plein inchange. ---
			check(NkTextNorm::YearToWords(1984, NkTextNormLang::En, NkTextNormYearReading::Full)
					  .Compare("one thousand nine hundred eighty-four") == 0,
				  "year-pairs-full-1984");

			// --- (119) YearToWords en Paired : 1984 -> "nineteen eighty-four"
			//     (exemple canonique de la convention par paires). ---
			check(NkTextNorm::YearToWords(1984, NkTextNormLang::En, NkTextNormYearReading::Paired)
					  .Compare("nineteen eighty-four") == 0,
				  "year-pairs-paired-1984");

			// --- (120) YearToWords en Paired : 2026 -> "twenty twenty-six" (annee
			//     courante du projet, autre exemple canonique de la source). ---
			check(NkTextNorm::YearToWords(2026, NkTextNormLang::En, NkTextNormYearReading::Paired)
					  .Compare("twenty twenty-six") == 0,
				  "year-pairs-paired-2026");

			// --- (121) YearToWords en Paired : 2005 -> "twenty oh five" (minute-like
			//     "oh" pour la 2e paire < 10, meme convention que ExpandTimeLiteral). ---
			check(NkTextNorm::YearToWords(2005, NkTextNormLang::En, NkTextNormYearReading::Paired)
					  .Compare("twenty oh five") == 0,
				  "year-pairs-paired-2005");

			// --- (122) YearToWords en Paired : 2000 (annee "ronde") -> repli sur le
			//     cardinal plein (convention incoherente pour x000, SOURCES
			//     [year-pairs]). ---
			check(NkTextNorm::YearToWords(2000, NkTextNormLang::En, NkTextNormYearReading::Paired)
					  .Compare("two thousand") == 0,
				  "year-pairs-paired-2000-fallback");

			// --- (123) YearToWords en Paired : 999 (hors bornes [1000,9999]) ->
			//     repli sur le cardinal plein. ---
			check(NkTextNorm::YearToWords(999, NkTextNormLang::En, NkTextNormYearReading::Paired)
					  .Compare("nine hundred ninety-nine") == 0,
				  "year-pairs-paired-999-fallback");

			// --- (124) ExpandDateLiteral integration en : "03/12/1984" avec
			//     yearReading=Paired -> "march twelfth nineteen eighty-four". ---
			check(NkTextNorm::ExpandDateLiteral(NkString("03/12/1984"), NkTextNormLang::En,
												 NkFrNumberDialect::Standard, NkTextNormYearReading::Paired)
					  .Compare("march twelfth nineteen eighty-four") == 0,
				  "year-pairs-date-integration");

			// --- (7) Validite calendaire reelle (SOURCES [leap-year]) --------------

			// --- (125) ExpandDateLiteral : "30/02/2026" (30 fevrier, 2026 non
			//     bissextile) -> REJETE (chaine vide). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("30/02/2026"), NkTextNormLang::Fr).Empty(),
				  "leap-year-30-02-2026-invalid");

			// --- (126) ExpandDateLiteral : "29/02/2024" (2024 divisible par 4, PAS
			//     par 100 -> bissextile) -> ACCEPTE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("29/02/2024"), NkTextNormLang::Fr)
					  .Compare("vingt-neuf fevrier deux mille vingt-quatre") == 0,
				  "leap-year-29-02-2024-valid");

			// --- (127) ExpandDateLiteral : "29/02/2023" (2023 non bissextile) ->
			//     REJETE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("29/02/2023"), NkTextNormLang::Fr).Empty(),
				  "leap-year-29-02-2023-invalid");

			// --- (128) ExpandDateLiteral : "29/02/2000" (divisible par 400 ->
			//     bissextile malgre la divisibilite par 100, regle gregorienne
			//     complete, SOURCES [leap-year]) -> ACCEPTE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("29/02/2000"), NkTextNormLang::Fr)
					  .Compare("vingt-neuf fevrier deux mille") == 0,
				  "leap-year-29-02-2000-valid");

			// --- (129) ExpandDateLiteral : "29/02/1900" (divisible par 100 mais PAS
			//     par 400 -> PAS bissextile, exception de l'exception) -> REJETE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("29/02/1900"), NkTextNormLang::Fr).Empty(),
				  "leap-year-29-02-1900-invalid");

			// --- (130) ExpandDateLiteral : "31/04/2026" (avril n'a que 30 jours) ->
			//     desormais REJETE (limite resolue, contrairement au comportement
			//     documente avant cette version). ---
			check(NkTextNorm::ExpandDateLiteral(NkString("31/04/2026"), NkTextNormLang::Fr).Empty(),
				  "leap-year-31-04-2026-invalid");

			// --- (131) ExpandDateLiteral : "30/04/2026" (30 avril, jour limite
			//     valide) -> ACCEPTE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("30/04/2026"), NkTextNormLang::Fr)
					  .Compare("trente avril deux mille vingt-six") == 0,
				  "leap-year-30-04-2026-valid");

			// --- (132) ExpandDateLiteral : "31/12/2026" (31 decembre, jour limite
			//     valide pour un mois de 31 jours) -> ACCEPTE. ---
			check(NkTextNorm::ExpandDateLiteral(NkString("31/12/2026"), NkTextNormLang::Fr)
					  .Compare("trente et un decembre deux mille vingt-six") == 0,
				  "leap-year-31-12-2026-valid");

			return ok;
		}

	} // namespace ai
} // namespace nkentseu
