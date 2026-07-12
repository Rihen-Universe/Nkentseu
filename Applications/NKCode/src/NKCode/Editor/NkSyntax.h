#pragma once
// =============================================================================
// NkSyntax.h — Coloration syntaxique (style Visual Studio / VSCode Dark+).
//   Tokenise UNE ligne et emet des plages colorees [begin,end) via un callback :
//   mots-cles, types, chaines, commentaires (// et /*..*/ multi-lignes), nombres,
//   preprocesseur. Langages : C/C++ et Python (sinon texte brut).
//   Sans allocation : gap-filling (les zones non colorees -> couleur texte).
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		enum class NkLang { None, C, Python, NKSL, Markdown, Generic };

		// ═══ Langages GÉNÉRIQUES data-driven (CSS, JS, Lua, Rust…) ══════════════
		// Un NkLangSpec décrit un langage par ses éléments lexicaux ; le chemin
		// Generic de TokenizeLine s'en sert. Les listes de mots-clés peuvent être
		// importées des projets libres (grammaires TextMate/Kate/highlight.js).
		struct NkLangSpec {
				NkString name;
				NkVector<NkString> exts;		///< ".css", ".scss"… (minuscules, avec point)
				NkString lineComment;			///< "//", "#", "--", ";" ("" = aucun)
				NkString blockOpen, blockClose; ///< "/*","*/" ou "<!--","-->" ("" = aucun)
				NkString strChars;				///< délimiteurs de chaînes, ex. "\"'`"
				NkVector<NkString> keywords;	///< triés (NkSymSortDedup) -> NkSymHas
				NkVector<NkString> types;		///< triés
		};

		inline NkVector<NkLangSpec> &NkSynSpecs() {
			static NkVector<NkLangSpec> s;
			return s;
		}

		// Spec ACTIVE pour le rendu en cours (posée par NkLangFromExt sur le thread
		// UI ; le thread d'index de symboles n'appelle jamais TokenizeLine(Generic)).
		inline const NkLangSpec *&NkSynCurSpec() {
			static const NkLangSpec *p = nullptr;
			return p;
		}

		inline int32 NkSynSpecFromExt(const char *ext) {
			if (!ext || !*ext)
				return -1;
			auto low = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
			NkVector<NkLangSpec> &sp = NkSynSpecs();
			for (usize i = 0; i < sp.Size(); ++i)
				for (usize e = 0; e < sp[i].exts.Size(); ++e) {
					const char *a = ext;
					const char *b = sp[i].exts[e].CStr();
					bool eq = true;
					while (*a && *b) {
						if (low(*a) != low(*b)) {
							eq = false;
							break;
						}
						++a;
						++b;
					}
					if (eq && !*a && !*b)
						return static_cast<int32>(i);
				}
			return -1;
		}

		inline bool NkSymW(char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		}

		inline int32 NkSymStrCmp(const char *a, const char *b) {
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return (int32)(unsigned char)*a - (int32)(unsigned char)*b;
		}

		// Tri FUSION sur INDICES (O(n log n), stable, une seule copie des chaînes) +
		// dédoublonnage en UNE passe. L'ancien tri par insertion O(n²) + Erase répétés
		// explosait sur l'index du workspace entier (dizaines de milliers d'entrées).
		inline void NkSymSortDedup(NkVector<NkString> &v) {
			const int32 n = static_cast<int32>(v.Size());
			if (n < 2)
				return;
			NkVector<int32> idx, tmp;
			idx.Resize(n);
			tmp.Resize(n);
			for (int32 i = 0; i < n; ++i)
				idx[i] = i;
			for (int32 w = 1; w < n; w <<= 1) {
				for (int32 lo = 0; lo < n; lo += (w << 1)) {
					int32 mid = lo + w;
					if (mid > n)
						mid = n;
					int32 hi = lo + (w << 1);
					if (hi > n)
						hi = n;
					int32 a = lo, b = mid, o = lo;
					while (a < mid && b < hi)
						tmp[o++] = (NkSymStrCmp(v[idx[a]].CStr(), v[idx[b]].CStr()) <= 0) ? idx[a++] : idx[b++];
					while (a < mid)
						tmp[o++] = idx[a++];
					while (b < hi)
						tmp[o++] = idx[b++];
				}
				for (int32 i = 0; i < n; ++i)
					idx[i] = tmp[i];
			}
			NkVector<NkString> out; // une seule copie de chaque chaîne retenue
			for (int32 i = 0; i < n; ++i) {
				if (i > 0 && NkSymStrCmp(v[idx[i]].CStr(), v[idx[i - 1]].CStr()) == 0)
					continue; // doublon
				out.PushBack(v[idx[i]]);
			}
			v = out;
		}

		// Extrait les TYPES/FONCTIONS DÉFINIS sur une ligne (heuristique) -> append (non triés).
		inline void NkExtractSymsLine(const char *p, int32 n, bool isC, NkVector<NkString> &types,
									  NkVector<NkString> &funcs) {
			int32 i = 0;
			while (i < n && (p[i] == ' ' || p[i] == '\t'))
				++i;
			if (i >= n || p[i] == '#')
				return;
			auto starts = [&](const char *w) {
				int32 k = 0;
				for (; w[k]; ++k)
					if (i + k >= n || p[i + k] != w[k])
						return false;
				return true;
			};
			auto pushR = [&](int32 s, int32 e, NkVector<NkString> &out) {
				if (e > s && (p[s] < '0' || p[s] > '9')) {
					char nm[128];
					int32 k = 0;
					for (int32 t = s; t < e && k < 127; ++t)
						nm[k++] = p[t];
					nm[k] = 0;
					out.PushBack(NkString(nm));
				}
			};
			auto addFrom = [&](int32 s, NkVector<NkString> &out) {
				while (s < n && (p[s] == ' ' || p[s] == '\t'))
					++s;
				int32 e = s;
				while (e < n && NkSymW(p[e]))
					++e;
				// `class NKX_API Nom` : DEUX identifiants -> macro d export + VRAI nom (le 2d).
				{
					int32 s2 = e;
					while (s2 < n && (p[s2] == ' ' || p[s2] == '\t'))
						++s2;
					int32 e2 = s2;
					while (e2 < n && NkSymW(p[e2]))
						++e2;
					if (e2 > s2 && !(p[s2] >= '0' && p[s2] <= '9')) {
						s = s2;
						e = e2;
					}
				}
				pushR(s, e, out);
			};
			if (starts("struct "))
				addFrom(i + 7, types);
			else if (starts("class "))
				addFrom(i + 6, types);
			else if (starts("enum class "))
				addFrom(i + 11, types);
			else if (starts("enum "))
				addFrom(i + 5, types);
			else if (starts("union "))
				addFrom(i + 6, types);
			else if (starts("namespace "))
				addFrom(i + 10, types);
			else if (isC && starts("using "))
				addFrom(i + 6, types);
			else if (isC && starts("typedef ")) {
				int32 e = n;
				while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t' || p[e - 1] == ';'))
					--e;
				int32 s = e;
				while (s > 0 && NkSymW(p[s - 1]))
					--s;
				pushR(s, e, types);
			}
			if (!isC && starts("def "))
				addFrom(i + 4, funcs);
			if (isC) {
				int32 e = n;
				while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t'))
					--e;
				const bool endsBrace = (e > 0 && p[e - 1] == '{'), endsParen = (e > 0 && p[e - 1] == ')');
				bool hasSemi = false;
				for (int32 t = 0; t < n; ++t)
					if (p[t] == ';') {
						hasSemi = true;
						break;
					}
				if (endsBrace || (endsParen && !hasSemi)) {
					int32 par = -1;
					for (int32 t = i; t < n; ++t) {
						if (p[t] == '(') {
							par = t;
							break;
						}
						if (p[t] == ';' || p[t] == '=')
							break;
					}
					if (par > i) {
						int32 ee = par;
						while (ee > i && p[ee - 1] == ' ')
							--ee;
						int32 ss = ee;
						while (ss > i && NkSymW(p[ss - 1]))
							--ss;
						char nm[128];
						int32 k = 0;
						for (int32 t = ss; t < ee && k < 127; ++t)
							nm[k++] = p[t];
						nm[k] = 0;
						if (ee > ss && NkSymStrCmp(nm, "if") && NkSymStrCmp(nm, "for") && NkSymStrCmp(nm, "while") &&
							NkSymStrCmp(nm, "switch") && NkSymStrCmp(nm, "catch") && NkSymStrCmp(nm, "return") &&
							NkSymStrCmp(nm, "sizeof"))
							funcs.PushBack(NkString(nm));
					}
				}
			}
		}

		// Scanne un texte entier (lignes séparées par \n) -> append types/funcs.
		inline void NkScanTextSymbols(const char *text, bool isC, NkVector<NkString> &types,
									  NkVector<NkString> &funcs) {
			const char *p = text;
			char buf[1024];
			while (*p) {
				int32 m = 0;
				while (*p && *p != '\n' && *p != '\r' && m < 1020)
					buf[m++] = *p++;
				buf[m] = 0;
				while (*p == '\n' || *p == '\r')
					++p;
				NkExtractSymsLine(buf, m, isC, types, funcs);
			}
		}

		// ── Tables de symboles (heuristique v3) : TOUT ce que le hover/complétion consomme. ──
		//    memSig / gfnSig = PROTOTYPE complet capturé sur la ligne de déclaration ;
		//    mcr* = macros `#define` (objet OU fonction) avec paramètres + corps.
		struct NkSymTables {
				NkVector<NkString> memOwner, memName, memType, memSig; // membres (type = retour pour une méthode)
				NkVector<NkString> inhOwner, inhBase;				   // héritage : Owner -> Base
				NkVector<NkString> gfnName, gfnRet, gfnSig;			   // fonctions libres
				NkVector<NkString> mcrName, mcrArgs, mcrBody;		   // macros (#define)
				NkVector<NkString> tyName, tyKind; // registre des TYPES : nom -> "struct|class|union|enum|namespace"

				void Clear() {
					memOwner.Clear();
					memName.Clear();
					memType.Clear();
					memSig.Clear();
					inhOwner.Clear();
					inhBase.Clear();
					gfnName.Clear();
					gfnRet.Clear();
					gfnSig.Clear();
					mcrName.Clear();
					mcrArgs.Clear();
					mcrBody.Clear();
					tyName.Clear();
					tyKind.Clear();
				}
		};

		// Scanne `text` -> append dans `tab` : corps `struct|class|union X { … }` (membres avec
		// nom + TYPE + SIGNATURE complète), HÉRITAGE (`: public A, B`), méthodes HORS-CLASSE
		// (`Ret Cls::Meth(...)`), fonctions LIBRES (retour + prototype) et MACROS `#define`.
		// Sert le hover et la complétion contextuelle instantanée (le compilateur affine).
		inline void NkScanTextMembers(const char *text, NkSymTables &tab) {
			auto isW = [](char c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
			};
			auto isPrim = [](const char *s) {
				static const char *pr[] = {"void",	   "int",	 "float",  "double", "char",	"bool",	   "auto",
										   "unsigned", "signed", "long",   "short",	 "int8",	"int16",   "int32",
										   "int64",	   "uint8",	 "uint16", "uint32", "uint64",	"float32", "float64",
										   "usize",	   "isize",	 "true",   "false",	 "nullptr", nullptr};
				for (int32 k = 0; pr[k]; ++k)
					if (NkSymStrCmp(s, pr[k]) == 0)
						return true;
				return false;
			};
			auto isKw = [](const char *s) {
				static const char *kw[] = {"if",		"for",		"while",	 "switch", "return",  "sizeof",
										   "public",	"private",	"protected", "using",  "typedef", "friend",
										   "static",	"template", "operator",	 "else",   "do",	  "new",
										   "delete",	"const",	"struct",	 "class",  "union",	  "enum",
										   "namespace", "case",		"goto",		 nullptr};
				for (int32 k = 0; kw[k]; ++k) {
					int32 m = 0;
					while (kw[k][m] && s[m] == kw[k][m])
						++m;
					if (!kw[k][m] && !s[m])
						return true;
				}
				return false;
			};
			// Type/retour AVANT `nameStart` : saute blancs, *,&, args template, qualificatifs.
			auto typeBefore = [&isW, &isKw](const char *cl, int32 limS, int32 nameStart, char *out) {
				out[0] = 0;
				int32 q = nameStart;
				for (int32 pass = 0; pass < 3; ++pass) {
					while (q > limS && (cl[q - 1] == ' ' || cl[q - 1] == '\t'))
						--q;
					while (q > limS && (cl[q - 1] == '*' || cl[q - 1] == '&'))
						--q;
					while (q > limS && (cl[q - 1] == ' ' || cl[q - 1] == '\t'))
						--q;
					if (q > limS && cl[q - 1] == '>') { // args template `NkVector<...>`
						int32 dep = 1;
						--q;
						while (q > limS && dep > 0) {
							--q;
							if (cl[q] == '>')
								++dep;
							else if (cl[q] == '<')
								--dep;
						}
						while (q > limS && (cl[q - 1] == ' ' || cl[q - 1] == '\t'))
							--q;
					}
					int32 e = q;
					while (q > limS && isW(cl[q - 1]))
						--q;
					if (e <= q || (cl[q] >= '0' && cl[q] <= '9'))
						return;
					char tmp[96];
					int32 k = 0;
					for (int32 t = q; t < e && k < 95; ++t)
						tmp[k++] = cl[t];
					tmp[k] = 0;
					static const char *quals[] = {"const",	  "static",	  "inline", "virtual", "constexpr",
												  "mutable",  "unsigned", "signed", "long",	   "short",
												  "typename", "explicit", nullptr};
					bool qual = false;
					for (int32 s2 = 0; quals[s2]; ++s2)
						if (NkSymStrCmp(tmp, quals[s2]) == 0) {
							qual = true;
							break;
						}
					if (!qual) {
						if (!isKw(tmp)) {
							int32 k2 = 0;
							while (tmp[k2]) {
								out[k2] = tmp[k2];
								++k2;
							}
							out[k2] = 0;
						}
						return;
					}
					int32 k2 = 0;
					while (tmp[k2]) {
						out[k2] = tmp[k2];
						++k2;
					}
					out[k2] = 0;
				}
			};
			// SIGNATURE : texte [s, fin] de la déclaration — jusqu'à la ')' appariée de `par`,
			// sinon jusqu'à `stop` (champ : '='/';'). Rognée à droite, cap 200.
			auto sigOf = [](const char *cl, int32 cn, int32 s, int32 par, int32 stop, char *out) {
				int32 send = stop;
				if (par >= 0) {
					int32 dep = 1, i2 = par + 1;
					while (i2 < cn && dep > 0) {
						if (cl[i2] == '(')
							++dep;
						else if (cl[i2] == ')')
							--dep;
						++i2;
					}
					send = i2;
				}
				if (send > cn)
					send = cn;
				int32 k = 0;
				for (int32 t = s; t < send && k < 198; ++t)
					out[k++] = cl[t];
				while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '\t'))
					--k;
				if (par >= 0 && send > cn - 1 && k >= 197) { // tronquée
					out[k - 1] = '.';
				}
				out[k] = 0;
			};

			struct Scope {
					char nm[96];
					int32 depth;
					char kind; // s=struct c=class u=union e=enum
			};

			Scope stack[24];
			int32 sn = 0;
			char pending[96];
			pending[0] = 0;			  // type vu, en attente de son '{'
			char pendingKind = 0;	  // s/c/u/e/n (n = namespace : PAS un scope membre)
			int32 basesOn = 0;		  // dans la liste d'héritage `: A, B` (jusqu'au '{')
			int32 depth = 0, blk = 0; // profondeur d'accolades / bloc-commentaire
			const char *p = text;
			char line[1024];
			while (*p && tab.memOwner.Size() < 120000) {
				int32 m = 0;
				while (*p && *p != '\n' && *p != '\r' && m < 1020)
					line[m++] = *p++;
				line[m] = 0;
				while (*p == '\n' || *p == '\r')
					++p;
				const int32 depthAtStart = depth;
				// Passe A : nettoie la ligne (chaînes/commentaires retirés), met à jour l'état bloc.
				char cl[1024];
				int32 cn = 0;
				for (int32 i = 0; i < m;) {
					const char c = line[i];
					if (blk) {
						if (c == '*' && i + 1 < m && line[i + 1] == '/') {
							blk = 0;
							i += 2;
							continue;
						}
						++i;
						continue;
					}
					if (c == '/' && i + 1 < m && line[i + 1] == '/')
						break;
					if (c == '/' && i + 1 < m && line[i + 1] == '*') {
						blk = 1;
						i += 2;
						continue;
					}
					if (c == '"' || c == '\'') {
						const char q = c;
						++i;
						while (i < m) {
							if (line[i] == '\\') {
								i += 2;
								continue;
							}
							if (line[i] == q) {
								++i;
								break;
							}
							++i;
						}
						cl[cn++] = q; // marqueur neutre
						continue;
					}
					cl[cn++] = c;
					++i;
				}
				cl[cn] = 0;
				// ── MACROS `#define` : parse la ligne BRUTE (le corps garde ses chaînes). ──
				{
					int32 s0 = 0;
					while (s0 < m && (line[s0] == ' ' || line[s0] == '\t'))
						++s0;
					if (s0 < m && line[s0] == '#') {
						int32 i2 = s0 + 1;
						while (i2 < m && (line[i2] == ' ' || line[i2] == '\t'))
							++i2;
						const char *kw = "define";
						int32 t = 0;
						while (kw[t] && i2 + t < m && line[i2 + t] == kw[t])
							++t;
						if (!kw[t] && i2 + t < m && (line[i2 + t] == ' ' || line[i2 + t] == '\t')) {
							i2 += t;
							while (i2 < m && (line[i2] == ' ' || line[i2] == '\t'))
								++i2;
							int32 e2 = i2;
							while (e2 < m && isW(line[e2]))
								++e2;
							if (e2 > i2 && !(line[i2] >= '0' && line[i2] <= '9') && tab.mcrName.Size() < 40000) {
								char nm[96];
								int32 k = 0;
								for (int32 t2 = i2; t2 < e2 && k < 95; ++t2)
									nm[k++] = line[t2];
								nm[k] = 0;
								NkString args; // "(a, b)" si macro-FONCTION (parenthèse collée au nom)
								int32 b = e2;
								if (b < m && line[b] == '(') {
									int32 dep = 0;
									while (b < m) {
										args += line[b];
										if (line[b] == '(')
											++dep;
										else if (line[b] == ')') {
											--dep;
											++b;
											break;
										}
										++b;
									}
								}
								while (b < m && (line[b] == ' ' || line[b] == '\t'))
									++b;
								NkString body;
								for (int32 t2 = b; t2 < m; ++t2)
									body += line[t2];
								if (m > 0 && line[m - 1] == '\\')
									body += " ..."; // suite sur la ligne suivante (non suivie)
								tab.mcrName.PushBack(NkString(nm));
								tab.mcrArgs.PushBack(args);
								tab.mcrBody.PushBack(body);
							}
						}
					}
				}
				// Passe B : accolades / ';' / ':' héritage / `struct|class|union NAME`, DANS L'ORDRE.
				for (int32 i = 0; i < cn; ++i) {
					const char c = cl[i];
					if (c == '{') {
						++depth;
						if (pending[0]) { // le corps du type/namespace vu s'ouvre ici
							// registre nom -> genre RÉEL (struct/class/union/enum/namespace)
							const char *kw = pendingKind == 's'	  ? "struct"
											 : pendingKind == 'c' ? "class"
											 : pendingKind == 'u' ? "union"
											 : pendingKind == 'e' ? "enum"
											 : pendingKind == 'n' ? "namespace"
																  : "type";
							tab.tyName.PushBack(NkString(pending));
							tab.tyKind.PushBack(NkString(kw));
							if (pendingKind != 'n' && sn < 24) { // namespace : pas un scope MEMBRE
								Scope &sc = stack[sn++];
								int32 k = 0;
								while (pending[k] && k < 95) {
									sc.nm[k] = pending[k];
									++k;
								}
								sc.nm[k] = 0;
								sc.depth = depth;
								sc.kind = pendingKind;
							}
						}
						pending[0] = 0;
						pendingKind = 0;
						basesOn = 0;
					} else if (c == '}') {
						if (sn > 0 && stack[sn - 1].depth == depth)
							--sn; // fin du corps du type
						if (depth > 0)
							--depth;
					} else if (c == ';') {
						pending[0] = 0; // forward declaration -> annule
						basesOn = 0;
					} else if (c == ':') {
						if (i + 1 < cn && cl[i + 1] == ':') { // '::' -> qualification, pas héritage
							++i;
							continue;
						}
						if (pending[0] && !basesOn)
							basesOn = 1; // début de la liste de bases `: public A, B`
					} else if (basesOn && isW(c) && !(c >= '0' && c <= '9') && (i == 0 || !isW(cl[i - 1]))) {
						int32 e = i;
						while (e < cn && isW(cl[e]))
							++e;
						char nm[96];
						int32 k = 0;
						for (int32 t = i; t < e && k < 95; ++t)
							nm[k++] = cl[t];
						nm[k] = 0;
						const bool ns = (e + 1 < cn && cl[e] == ':' && cl[e + 1] == ':'); // espace de noms
						const bool acc = NkSymStrCmp(nm, "public") == 0 || NkSymStrCmp(nm, "private") == 0 ||
										 NkSymStrCmp(nm, "protected") == 0 || NkSymStrCmp(nm, "virtual") == 0 ||
										 NkSymStrCmp(nm, "final") == 0;
						if (!ns && !acc && pending[0]) {
							tab.inhOwner.PushBack(NkString(pending));
							tab.inhBase.PushBack(NkString(nm));
						}
						i = e - 1;
					} else if (!basesOn && (c == 's' || c == 'c' || c == 'u' || c == 'e' || c == 'n') &&
							   (i == 0 || !isW(cl[i - 1]))) {
						int32 adv = 0;
						char kk = 0;
						if (i + 6 <= cn && cl[i] == 's' && cl[i + 1] == 't' && cl[i + 2] == 'r' && cl[i + 3] == 'u' &&
							cl[i + 4] == 'c' && cl[i + 5] == 't' && !isW(cl[i + 6])) {
							adv = 6;
							kk = 's';
						} else if (i + 5 <= cn && cl[i] == 'c' && cl[i + 1] == 'l' && cl[i + 2] == 'a' &&
								   cl[i + 3] == 's' && cl[i + 4] == 's' && !isW(cl[i + 5])) {
							adv = 5;
							kk = 'c';
						} else if (i + 5 <= cn && cl[i] == 'u' && cl[i + 1] == 'n' && cl[i + 2] == 'i' &&
								   cl[i + 3] == 'o' && cl[i + 4] == 'n' && !isW(cl[i + 5])) {
							adv = 5;
							kk = 'u';
						} else if (i + 4 <= cn && cl[i] == 'e' && cl[i + 1] == 'n' && cl[i + 2] == 'u' &&
								   cl[i + 3] == 'm' && !isW(cl[i + 4])) {
							adv = 4; // `enum` (le `class` optionnel qui suit est sauté par la boucle)
							kk = 'e';
						} else if (i + 9 <= cn && cl[i] == 'n' && cl[i + 1] == 'a' && cl[i + 2] == 'm' &&
								   cl[i + 3] == 'e' && cl[i + 4] == 's' && cl[i + 5] == 'p' && cl[i + 6] == 'a' &&
								   cl[i + 7] == 'c' && cl[i + 8] == 'e' && !isW(cl[i + 9])) {
							adv = 9;
							kk = 'n';
						}
						if (!adv)
							continue;
						if (kk == 'e') { // saute l'éventuel `class`/`struct` de `enum class X`
							int32 s0 = i + adv;
							while (s0 < cn && (cl[s0] == ' ' || cl[s0] == '\t'))
								++s0;
							if (s0 + 5 <= cn && cl[s0] == 'c' && cl[s0 + 1] == 'l' && cl[s0 + 2] == 'a' &&
								cl[s0 + 3] == 's' && cl[s0 + 4] == 's' && !isW(cl[s0 + 5]))
								adv = (s0 + 5) - i;
							else if (s0 + 6 <= cn && cl[s0] == 's' && cl[s0 + 1] == 't' && cl[s0 + 2] == 'r' &&
									 cl[s0 + 3] == 'u' && cl[s0 + 4] == 'c' && cl[s0 + 5] == 't' && !isW(cl[s0 + 6]))
								adv = (s0 + 6) - i;
						}
						pendingKind = kk;
						int32 s = i + adv;
						while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
							++s;
						int32 e = s;
						while (e < cn && isW(cl[e]))
							++e;
						// `class NKWINDOW_API Nom` : DEUX identifiants -> le 1er est une macro
						// d export, le VRAI nom du type est le second.
						{
							int32 s2 = e;
							while (s2 < cn && (cl[s2] == ' ' || cl[s2] == '\t'))
								++s2;
							int32 e2 = s2;
							while (e2 < cn && isW(cl[e2]))
								++e2;
							if (e2 > s2 && !(cl[s2] >= '0' && cl[s2] <= '9')) {
								s = s2;
								e = e2;
							}
						}
						if (e > s && !(cl[s] >= '0' && cl[s] <= '9')) {
							int32 k = 0;
							for (int32 t = s; t < e && k < 95; ++t)
								pending[k++] = cl[t];
							pending[k] = 0;
						}
						i = e - 1; // reprend après le nom
					}
				}
				// Moisson des MEMBRES : ligne au niveau DIRECT du corps du type courant.
				if (sn > 0 && depthAtStart == stack[sn - 1].depth && cn > 0) {
					if (stack[sn - 1].kind == 0x65) { // ENUM : chaque ligne liste des ENUMERATEURS
						int32 i2 = 0;
						while (i2 < cn) {
							while (i2 < cn && !isW(cl[i2]))
								++i2;
							int32 e = i2;
							while (e < cn && isW(cl[e]))
								++e;
							if (e > i2 && !(cl[i2] >= 0x30 && cl[i2] <= 0x39)) {
								char nm[96];
								int32 k = 0;
								for (int32 t = i2; t < e && k < 95; ++t)
									nm[k++] = cl[t];
								nm[k] = 0;
								if (!isKw(nm) && !isPrim(nm)) {
									tab.memOwner.PushBack(NkString(stack[sn - 1].nm));
									tab.memName.PushBack(NkString(nm));
									tab.memType.PushBack(NkString(""));
									tab.memSig.PushBack(NkString(""));
								}
							}
							while (e < cn && cl[e] != 0x2C) // saute la valeur jusqu a la virgule
								++e;
							i2 = e + 1;
						}
					} else {
						int32 s = 0;
						while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
							++s;
						if (s < cn && cl[s] != '#') {
							bool skip = false;
							const char *skips[] = {"public",  "private", "protected", "using",
												   "typedef", "friend",	 "template",  nullptr};
							for (int32 k = 0; skips[k]; ++k) {
								int32 t = 0;
								while (skips[k][t] && s + t < cn && cl[s + t] == skips[k][t])
									++t;
								if (!skips[k][t] && (s + t >= cn || !isW(cl[s + t]))) {
									skip = true;
									break;
								}
							}
							if (!skip) {
								int32 par = -1, eq = -1, sc = -1;
								for (int32 i = s; i < cn; ++i) {
									if (cl[i] == '(' && par < 0)
										par = i;
									else if (cl[i] == '=' && eq < 0)
										eq = i;
									else if (cl[i] == ';' && sc < 0)
										sc = i;
								}
								int32 anchor = (par >= 0) ? par : (eq >= 0 ? eq : sc);
								if (anchor > s) {
									// champ POINTEUR DE FONCTION `Ret (*Nom)(args)` : le vrai nom est apres `(*`.
									int32 fpS = -1, fpE = -1;
									if (par >= 0 && par + 1 < cn && cl[par + 1] == 0x2A) {
										int32 s3 = par + 2;
										while (s3 < cn && (cl[s3] == 0x20 || cl[s3] == 0x09))
											++s3;
										int32 e3 = s3;
										while (e3 < cn && isW(cl[e3]))
											++e3;
										if (e3 > s3 && !(cl[s3] >= 0x30 && cl[s3] <= 0x39)) {
											fpS = s3;
											fpE = e3;
										}
									}
									int32 e2 = anchor;
									while (e2 > s && (cl[e2 - 1] == ' ' || cl[e2 - 1] == '\t'))
										--e2;
									if (e2 > s && cl[e2 - 1] == ']') { // tableau : saute [n]
										while (e2 > s && cl[e2 - 1] != '[')
											--e2;
										if (e2 > s)
											--e2;
										while (e2 > s && (cl[e2 - 1] == ' ' || cl[e2 - 1] == '\t'))
											--e2;
									}
									int32 s2 = e2;
									while (s2 > s && isW(cl[s2 - 1]))
										--s2;
									if (fpS >= 0) { // nom reel du pointeur de fonction
										s2 = fpS;
										e2 = fpE;
									}
									if (e2 > s2 && !(cl[s2] >= '0' && cl[s2] <= '9')) {
										char nm[96];
										int32 k = 0;
										for (int32 t = s2; t < e2 && k < 95; ++t)
											nm[k++] = cl[t];
										nm[k] = 0;
										if (!isKw(nm) && !isPrim(nm) &&
											NkSymStrCmp(nm, stack[sn - 1].nm) != 0) { // pas le constructeur/primitif
											char rt[96], sg[200];
											typeBefore(cl, s, s2, rt);
											sigOf(cl, cn, s, par, anchor, sg);
											tab.memOwner.PushBack(NkString(stack[sn - 1].nm));
											tab.memName.PushBack(NkString(nm));
											tab.memType.PushBack(NkString(rt)); // "" si type introuvable
											tab.memSig.PushBack(NkString(sg));
										}
									}
								}
							}
						}
					}
				} else if (sn == 0 && cn > 0 && tab.memOwner.Size() < 120000) {
					// HORS corps de type : signatures de fonctions -> retour (pour `foo().`) et
					// méthodes hors-classe `Ret Cls::Meth(...)` -> membre de Cls avec son retour.
					int32 s = 0;
					while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
						++s;
					if (s < cn && cl[s] != '#') {
						int32 par = -1;
						for (int32 i2 = s; i2 < cn; ++i2) {
							if (cl[i2] == '(') {
								par = i2;
								break;
							}
							if (cl[i2] == '=')
								break; // affectation -> pas une signature
						}
						if (par > s) {
							int32 e2 = par;
							while (e2 > s && (cl[e2 - 1] == ' ' || cl[e2 - 1] == '\t'))
								--e2;
							int32 s2 = e2;
							while (s2 > s && isW(cl[s2 - 1]))
								--s2;
							if (e2 > s2 && !(cl[s2] >= '0' && cl[s2] <= '9')) {
								char nm[96];
								int32 k = 0;
								for (int32 t = s2; t < e2 && k < 95; ++t)
									nm[k++] = cl[t];
								nm[k] = 0;
								if (!isKw(nm)) {
									if (s2 >= s + 2 && cl[s2 - 1] == ':' && cl[s2 - 2] == ':') {
										int32 ce = s2 - 2, cs = ce;
										while (cs > s && isW(cl[cs - 1]))
											--cs;
										if (ce > cs && !(cl[cs] >= '0' && cl[cs] <= '9')) {
											char cls[96];
											int32 k2 = 0;
											for (int32 t = cs; t < ce && k2 < 95; ++t)
												cls[k2++] = cl[t];
											cls[k2] = 0;
											char rt[96], sg[200];
											typeBefore(cl, s, cs, rt);
											sigOf(cl, cn, s, par, par, sg);
											tab.memOwner.PushBack(NkString(cls));
											tab.memName.PushBack(NkString(nm));
											tab.memType.PushBack(NkString(rt));
											tab.memSig.PushBack(NkString(sg));
										}
									} else {
										char rt[96];
										typeBefore(cl, s, s2, rt);
										if (rt[0] && !isKw(rt) && !isPrim(nm) && tab.gfnName.Size() < 60000) {
											char sg[200];
											sigOf(cl, cn, s, par, par, sg);
											tab.gfnName.PushBack(NkString(nm));
											tab.gfnRet.PushBack(NkString(rt));
											tab.gfnSig.PushBack(NkString(sg));
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// Recherche binaire de la sous-chaîne (p,len) dans un vecteur TRIÉ de symboles.
		// Sert à la coloration SÉMANTIQUE (types/fonctions réellement définis dans le fichier).
		inline bool NkSymHas(const NkVector<NkString> *v, const char *p, int32 len) {
			if (!v || v->Empty() || len <= 0)
				return false;
			int32 lo = 0, hi = static_cast<int32>(v->Size()) - 1;
			while (lo <= hi) {
				const int32 mid = (lo + hi) >> 1;
				const char *s = (*v)[mid].CStr();
				int32 c = 0;
				for (int32 k = 0; k < len; ++k) {
					if (s[k] == 0) {
						c = 1;
						break;
					}
					const unsigned char a = (unsigned char)p[k], b = (unsigned char)s[k];
					if (a != b) {
						c = (a < b) ? -1 : 1;
						break;
					}
				}
				if (c == 0)
					c = (s[len] == 0) ? 0 : -1; // égalité seulement si `s` fait exactement `len`
				if (c == 0)
					return true;
				if (c < 0)
					hi = mid - 1;
				else
					lo = mid + 1;
			}
			return false;
		}

		// Couleurs de coloration = celles du contexte NKGui (editables via Preferences >
		// Langages). Alias pour conserver l'API existante (NkSynColors).
		using NkSynColors = nkentseu::nkgui::NkGuiSyntax;

		namespace synd {
			inline bool IsAlpha(char c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
			}

			inline bool IsDigit(char c) {
				return c >= '0' && c <= '9';
			}

			inline bool IsWord(char c) {
				return IsAlpha(c) || IsDigit(c);
			}

			inline bool WordEq(const char *s, int32 n, const char *kw) {
				int32 i = 0;
				for (; i < n && kw[i]; ++i)
					if (s[i] != kw[i])
						return false;
				return i == n && kw[i] == '\0';
			}

			inline bool InList(const char *s, int32 n, const char *const *list) {
				for (int32 i = 0; list[i]; ++i)
					if (WordEq(s, n, list[i]))
						return true;
				return false;
			}

			inline const char *const *CKeywords() {
				static const char *k[] = {"if",			"else",			"for",
										  "while",		"do",			"switch",
										  "case",		"default",		"break",
										  "continue",	"return",		"goto",
										  "sizeof",		"typedef",		"struct",
										  "class",		"union",		"enum",
										  "namespace",	"using",		"template",
										  "typename",	"public",		"private",
										  "protected",	"virtual",		"override",
										  "final",		"const",		"constexpr",
										  "static",		"inline",		"extern",
										  "volatile",	"mutable",		"new",
										  "delete",		"this",			"nullptr",
										  "true",		"false",		"operator",
										  "friend",		"explicit",		"auto",
										  "noexcept",	"try",			"catch",
										  "throw",		"static_cast",	"reinterpret_cast",
										  "const_cast", "dynamic_cast", "decltype",
										  "include",	"define",		"pragma",
										  nullptr};
				return k;
			}

			inline const char *const *CTypes() {
				static const char *t[] = {
					"void", "bool", "char", "short", "int", "long", "float", "double", "unsigned", "signed", "wchar_t",
					"size_t", "usize", "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
					"float32", "float64", "NkString", "NkVector",
					// Types shader (NKSL/GLSL-like) :
					"vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4", "bvec2", "bvec3",
					"bvec4", "mat2", "mat3", "mat4", "float2", "float3", "float4", "int2", "int3", "int4", "half",
					"half2", "half3", "half4", "sampler2D", "sampler3D", "samplerCube", "texture2D", "Texture2D",
					"SamplerState", nullptr};
				return t;
			}

			inline const char *const *NkslKeywords() {
				static const char *k[] = {"if",			 "else",	"for",	  "while",	  "do",		   "switch",
										  "case",		 "default", "break",  "continue", "return",	   "struct",
										  "const",		 "static",	"inline", "true",	  "false",	   "void",
										  "in",			 "out",		"inout",  "uniform",  "attribute", "varying",
										  "layout",		 "buffer",	"flat",	  "smooth",	  "discard",   "precision",
										  "highp",		 "mediump", "lowp",	  "cbuffer",  "register",  "numthreads",
										  "groupshared", "stage",	"shader", "vertex",	  "fragment",  "compute",
										  "include",	 "define",	"pragma", "version",  nullptr};
				return k;
			}

			inline const char *const *PyKeywords() {
				static const char *k[] = {
					"def",	 "class", "if",	   "elif",	 "else",	"for",	"while", "return",	 "import",	 "from",
					"as",	 "with",  "try",   "except", "finally", "pass", "break", "continue", "lambda",	 "None",
					"True",	 "False", "and",   "or",	 "not",		"in",	"is",	 "global",	 "nonlocal", "yield",
					"await", "async", "raise", "del",	 "assert",	"with", nullptr};
				return k;
			}
		} // namespace synd

		inline NkLang NkLangFromExt(const char *ext) {
			using namespace synd;
			auto eq = [&](const char *a) {
				int32 n = 0;
				while (ext[n])
					++n;
				return WordEq(ext, n, a);
			};
			if (eq(".cpp") || eq(".cc") || eq(".cxx") || eq(".h") || eq(".hpp") || eq(".hxx") || eq(".c") ||
				eq(".inl") || eq(".ino"))
				return NkLang::C;
			if (eq(".py") || eq(".jenga") || eq(".pyi"))
				return NkLang::Python; // .jenga = Python
			if (eq(".nksl") || eq(".glsl") || eq(".hlsl") || eq(".vert") || eq(".frag") || eq(".comp") || eq(".shader"))
				return NkLang::NKSL;
			if (eq(".md") || eq(".markdown"))
				return NkLang::Markdown;
			// Langages GÉNÉRIQUES data-driven (CSS, JS, Lua…) : pose la spec active.
			const int32 gi = NkSynSpecFromExt(ext);
			if (gi >= 0) {
				NkSynCurSpec() = &NkSynSpecs()[static_cast<usize>(gi)];
				return NkLang::Generic;
			}
			return NkLang::None;
		}

		// Tokenise [L, L+n). `inBlock` = on est dans un /*..*/ ouvert. emit(a,b,color)
		// pour CHAQUE plage (les trous sont comblés en couleur texte). Retourne le nouvel
		// etat de bloc-commentaire. Emit signature : void(int32, int32, const NkColor&).
		// État de bloc multi-lignes propagé entre lignes : 0 aucun, 1 = /*..*/ C (ou ``` markdown),
		// 2 = docstring Python """, 3 = docstring Python '''. Retourne le nouvel état.
		template <class Emit>
		inline int32
		TokenizeLine(NkLang lang, const char *L, int32 n, int32 st, const NkSynColors &C, Emit emit,
					 const NkVector<NkString> *knownTypes = nullptr, const NkVector<NkString> *knownFuncs = nullptr,
					 const NkVector<NkString> *projTypes = nullptr, const NkVector<NkString> *projFuncs = nullptr) {
			using namespace synd;
			if (lang == NkLang::None) {
				if (n > 0)
					emit(0, n, C.text);
				return 0;
			}

			// ── Markdown (titres, blocs ``` , code inline `...`) ─────────────────
			if (lang == NkLang::Markdown) {
				int32 last = 0;
				auto flush = [&](int32 upTo) {
					if (upTo > last)
						emit(last, upTo, C.text);
				};
				int32 s = 0;
				while (s < n && (L[s] == ' ' || L[s] == '\t'))
					++s;
				const bool fence = (s + 2 < n && L[s] == '`' && L[s + 1] == '`' && L[s + 2] == '`');
				if (st == 1) { // dans un bloc de code ```
					if (fence) {
						if (n > 0)
							emit(0, n, C.comment);
						return 0;
					}
					if (n > 0)
						emit(0, n, C.mdcode);
					return 1;
				}
				if (fence) {
					emit(0, n, C.comment);
					return 1;
				} // ouvre un bloc
				if (s < n && L[s] == '#') {
					emit(0, n, C.heading);
					return 0;
				} // titre #
				for (int32 i = 0; i < n;) { // code inline `...`
					if (L[i] == '`') {
						int32 j = i + 1;
						while (j < n && L[j] != '`')
							++j;
						j = (j < n) ? j + 1 : n;
						flush(i);
						emit(i, j, C.mdcode);
						last = j;
						i = j;
						continue;
					}
					++i;
				}
				flush(n);
				return 0;
			}

			// ── GÉNÉRIQUE data-driven : commentaires/chaînes/nombres/mots-clés
			//    décrits par la spec active (CSS, JS, Lua, Rust, YAML…). ──
			if (lang == NkLang::Generic) {
				const NkLangSpec *sp = NkSynCurSpec();
				if (!sp) {
					if (n > 0)
						emit(0, n, C.text);
					return 0;
				}
				int32 lastG = 0;
				auto flushG = [&](int32 upTo) {
					if (upTo > lastG)
						emit(lastG, upTo, C.text);
				};
				auto spanG = [&](int32 a, int32 b, const NkColor &col) {
					flushG(a);
					emit(a, b, col);
					lastG = b;
				};
				auto startsAt = [&](int32 k, const NkString &s) {
					const char *q = s.CStr();
					for (int32 j = 0; q[j]; ++j)
						if (k + j >= n || L[k + j] != q[j])
							return false;
					return s.Length() > 0;
				};
				int32 i = 0;
				if (st == 1 && sp->blockClose.Length() > 0) { // bloc entamé à la ligne d'avant
					int32 k = 0;
					while (k < n && !startsAt(k, sp->blockClose))
						++k;
					if (k < n) {
						spanG(0, k + static_cast<int32>(sp->blockClose.Length()), C.comment);
						i = lastG;
						st = 0;
					} else {
						if (n > 0)
							emit(0, n, C.comment);
						return 1;
					}
				}
				while (i < n) {
					const char c = L[i];
					if (sp->lineComment.Length() > 0 && startsAt(i, sp->lineComment)) {
						spanG(i, n, C.comment);
						i = n;
						break;
					}
					if (sp->blockOpen.Length() > 0 && startsAt(i, sp->blockOpen)) {
						int32 k = i + static_cast<int32>(sp->blockOpen.Length());
						while (k < n && !startsAt(k, sp->blockClose))
							++k;
						if (k < n) {
							spanG(i, k + static_cast<int32>(sp->blockClose.Length()), C.comment);
							i = lastG;
							continue;
						}
						spanG(i, n, C.comment);
						flushG(n);
						return 1; // bloc ouvert -> continue à la ligne suivante
					}
					bool isStr = false;
					for (const char *q = sp->strChars.CStr(); *q; ++q)
						if (c == *q) {
							isStr = true;
							break;
						}
					if (isStr) { // chaîne délimitée (échappement '\')
						int32 k = i + 1;
						while (k < n && L[k] != c) {
							if (L[k] == '\\' && k + 1 < n)
								++k;
							++k;
						}
						k = (k < n) ? k + 1 : n;
						spanG(i, k, C.string);
						i = k;
						continue;
					}
					if (c >= '0' && c <= '9') { // nombre (entier/hex/float simple)
						int32 k = i + 1;
						while (k < n && (NkSymW(L[k]) || L[k] == '.'))
							++k;
						spanG(i, k, C.number);
						i = k;
						continue;
					}
					if (NkSymW(c) && !(c >= '0' && c <= '9')) { // mot -> keyword/type ?
						int32 k = i + 1;
						while (k < n && NkSymW(L[k]))
							++k;
						if (NkSymHas(&sp->keywords, L + i, k - i))
							spanG(i, k, C.keyword);
						else if (NkSymHas(&sp->types, L + i, k - i))
							spanG(i, k, C.type);
						i = k;
						continue;
					}
					++i;
				}
				flushG(n);
				return 0;
			}

			int32 last = 0; // fin de la derniere plage emise
			auto flush = [&](int32 upTo) {
				if (upTo > last)
					emit(last, upTo, C.text);
			};
			auto span = [&](int32 a, int32 b, const NkColor &col) {
				flush(a);
				emit(a, b, col);
				last = b;
			};
			const bool isC = (lang == NkLang::C || lang == NkLang::NKSL); // NKSL = famille C

			int32 i = 0;
			// ── Continuation d'un bloc ouvert à la ligne précédente ──
			if (st == 1) { // /*..*/ C entamé avant
				int32 k = i;
				while (k + 1 < n && !(L[k] == '*' && L[k + 1] == '/'))
					++k;
				if (k + 1 < n) {
					span(0, k + 2, C.comment);
					i = k + 2;
					st = 0;
				} else {
					span(0, n, C.comment);
					return 1;
				}
			} else if (st == 2 || st == 3) { // docstring Python """ ou ''' entamé avant
				const char q = (st == 2) ? '"' : '\'';
				int32 k = i;
				while (k + 2 < n && !(L[k] == q && L[k + 1] == q && L[k + 2] == q))
					++k;
				if (k + 2 < n) {
					span(0, k + 3, C.comment);
					i = k + 3;
					st = 0;
				} else {
					span(0, n, C.comment);
					return st;
				}
			}
			int32 pendingType =
				0; // 1 = prochain identifiant est un TYPE (après struct/class/enum…), 2 = FONCTION (après def)
			auto wordIs = [&](int32 a, int32 wn, const char *w) {
				int32 k = 0;
				for (; k < wn && w[k]; ++k)
					if (L[a + k] != w[k])
						return false;
				return k == wn && !w[k];
			};
			auto isOper = [](char ch) {
				return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '=' || ch == '<' ||
					   ch == '>' || ch == '!' || ch == '&' || ch == '|' || ch == '^' || ch == '~' || ch == '?' ||
					   ch == ':';
			};
			for (; i < n;) {
				const char c = L[i];
				if (isC && c == '/' && i + 1 < n && L[i + 1] == '/') {
					span(i, n, C.comment);
					i = n;
					break;
				}
				if (isC && c == '/' && i + 1 < n && L[i + 1] == '*') {
					int32 k = i + 2;
					while (k + 1 < n && !(L[k] == '*' && L[k + 1] == '/'))
						++k;
					if (k + 1 < n) {
						span(i, k + 2, C.comment);
						i = k + 2;
					} else {
						span(i, n, C.comment);
						return 1;
					} // bloc ouvert -> lignes suivantes
					continue;
				}
				if (!isC && c == '#') {
					span(i, n, C.comment);
					i = n;
					break;
				} // Python : commentaire
				if (isC && c == '#') { // C : preprocesseur
					bool firstTok = true;
					for (int32 j = 0; j < i; ++j)
						if (L[j] != ' ' && L[j] != '\t') {
							firstTok = false;
							break;
						}
					if (firstTok) {
						// Nom de la directive (après le # et d'éventuels espaces).
						int32 d = i + 1;
						while (d < n && (L[d] == ' ' || L[d] == '\t'))
							++d;
						int32 de = d;
						while (de < n && IsAlpha(L[de]))
							++de;
						auto wordEq = [&](int32 a, int32 b, const char *w) {
							int32 k = 0;
							for (; a + k < b && w[k]; ++k)
								if (L[a + k] != w[k])
									return false;
							return (a + k == b && !w[k]);
						};
						const bool inc =
							wordEq(d, de, "include") || wordEq(d, de, "import") || wordEq(d, de, "include_next");
						if (inc) {
							span(i, de, C.preproc); // "#include" en préproc
							int32 p = de;
							while (p < n && (L[p] == ' ' || L[p] == '\t'))
								++p;
							if (p < n && (L[p] == '<' || L[p] == '"')) { // <chemin> ou "chemin" -> string
								const char cl2 = (L[p] == '<') ? '>' : '"';
								int32 q = p + 1;
								while (q < n && L[q] != cl2)
									++q;
								q = (q < n) ? q + 1 : n;
								span(p, q, C.string);
								i = q;
								continue; // reste de ligne tokenisé normalement
							}
							i = de;
							continue;
						}
						span(i, n, C.preproc);
						i = n;
						break; // autres directives : ligne entière
					}
				}
				// Python : docstring/bloc triple-guillemets """ ou ''' (coloré comme COMMENTAIRE).
				if (!isC && (c == '"' || c == '\'') && i + 2 < n && L[i + 1] == c && L[i + 2] == c) {
					const char q = c;
					int32 k = i + 3;
					while (k + 2 < n && !(L[k] == q && L[k + 1] == q && L[k + 2] == q))
						++k;
					if (k + 2 < n) {
						span(i, k + 3, C.comment);
						i = k + 3;
						continue;
					} else {
						span(i, n, C.comment);
						return (q == '"') ? 2 : 3;
					} // ouvert -> lignes suivantes
				}
				if (c == '"' || c == '\'') { // chaine / caractere (mono-ligne)
					int32 j = i + 1;
					while (j < n && L[j] != c) {
						if (L[j] == '\\' && j + 1 < n)
							++j;
						++j;
					}
					j = (j < n) ? j + 1 : n;
					span(i, j, C.string);
					i = j;
					continue;
				}
				if (!isC && c == '@') { // Python : décorateur @nom
					int32 j = i + 1;
					while (j < n && (IsWord(L[j]) || L[j] == '.'))
						++j;
					span(i, j, C.function);
					i = j;
					continue;
				}
				if (IsDigit(c)) { // nombre (hex/bin/float/suffixes gérés par IsWord)
					int32 j = i + 1;
					while (j < n && (IsWord(L[j]) || L[j] == '.'))
						++j;
					span(i, j, C.number);
					i = j;
					continue;
				}
				if (IsAlpha(c)) { // identifiant / mot-cle / type / fonction / constante
					int32 j = i + 1;
					while (j < n && IsWord(L[j]))
						++j;
					const int32 wn = j - i;
					const char *const *kws = (lang == NkLang::C)	  ? CKeywords()
											 : (lang == NkLang::NKSL) ? NkslKeywords()
																	  : PyKeywords();
					int32 pk = j;
					while (pk < n && (L[pk] == ' ' || L[pk] == '\t'))
						++pk;
					bool call = (pk < n && L[pk] == '(');		  // suivi de '(' -> appel de fonction
					if (!call && isC && pk < n && L[pk] == '<') { // appel template : Foo<...>(
						int32 depth = 1, q = pk + 1;
						for (; q < n && depth > 0; ++q) {
							const char cc = L[q];
							if (cc == '<')
								++depth;
							else if (cc == '>')
								--depth;
							else if (cc == ';' || cc == '{' || cc == '}')
								break;
						}
						if (depth == 0) {
							while (q < n && (L[q] == ' ' || L[q] == '\t'))
								++q;
							if (q < n && L[q] == '(')
								call = true;
						}
					}
					bool allCaps = (wn >= 2), hasAlpha = false; // MAJUSCULES (>=2) -> constante/macro
					for (int32 t = i; t < j; ++t) {
						const char ch = L[t];
						if (ch >= 'a' && ch <= 'z') {
							allCaps = false;
							break;
						}
						if (ch >= 'A' && ch <= 'Z')
							hasAlpha = true;
					}
					allCaps = allCaps && hasAlpha;
					const bool cst = wordIs(i, wn, "true") || wordIs(i, wn, "false") || wordIs(i, wn, "nullptr") ||
									 wordIs(i, wn, "NULL") || wordIs(i, wn, "None") || wordIs(i, wn, "True") ||
									 wordIs(i, wn, "False");
					// Contexte VARIABLE/MEMBRE : `obj.x` / `obj->x` (membre) ou `x.` (objet suivi de '.').
					// On NE colore PAS un membre/objet comme type/fonction GLOBALE (sinon une variable
					// `window`/`cfg` prend la couleur d'une fonction homonyme trouvée ailleurs dans le projet).
					int32 pb = i;
					while (pb > 0 && (L[pb - 1] == ' ' || L[pb - 1] == '\t'))
						--pb;
					const bool member =
						pb > 0 && (L[pb - 1] == '.' || (pb >= 2 && L[pb - 1] == '>' && L[pb - 2] == '-'));
					const bool objFollow = (pk < n && L[pk] == '.');
					if (InList(L + i, wn, kws)) {
						span(i, j, C.keyword);
						if (wordIs(i, wn, "struct") || wordIs(i, wn, "class") || wordIs(i, wn, "enum") ||
							wordIs(i, wn, "union") || wordIs(i, wn, "namespace") || wordIs(i, wn, "typedef") ||
							wordIs(i, wn, "using"))
							pendingType = 1;
						else if (!isC && wordIs(i, wn, "def"))
							pendingType = 2;
						else
							pendingType = 0;
					} else if (cst)
						span(i, j, C.constant);
					else if (!member && !objFollow && (L[i] >= 'A' && L[i] <= 'Z') &&
							 (NkSymHas(knownTypes, L + i, wn) || NkSymHas(projTypes, L + i, wn)))
						span(i, j, C.type); // TYPE (PascalCase, jamais un membre/objet -> évite les collisions de noms
											// minuscules io/cfg)
					else if (pendingType == 1) {
						span(i, j, C.type);
						pendingType = 0;
					} else if (!member && NkSymHas(knownFuncs, L + i, wn))
						span(i, j, C.function); // FONCTION du FICHIER (les fonctions PROJET ne colorent qu'au site
												// d'appel `name(`)
					else if (pendingType == 2) {
						span(i, j, C.function);
						pendingType = 0;
					} else if (isC && InList(L + i, wn, CTypes()))
						span(i, j, C.type);
					else if (allCaps)
						span(i, j, C.constant);
					else if (call)
						span(i, j, C.function);
					// sinon : identifiant ordinaire -> couleur texte (comblé par flush)
					i = j;
					continue;
				}
				if (isOper(c)) { // opérateurs + - * / = < > & | etc.
					int32 j = i + 1;
					while (j < n && isOper(L[j]))
						++j;
					span(i, j, C.oper);
					i = j;
					continue;
				}
				++i; // ponctuation restante (,;(){}[]. ) -> texte
			}
			flush(n);
			return 0;
		}

	} // namespace nkcode
} // namespace nkentseu
