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

		enum class NkLang { None, C, Python, NKSL, Markdown };

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

		// ── Table Type -> MEMBRES (heuristique v2) : scanne les corps `struct|class|union X { … }`
		//    et moissonne, PAR MEMBRE : son nom + son TYPE (type du champ / type de RETOUR de la
		//    méthode) -> résolution des chaînes `a.b().c.`. Capture aussi l'HÉRITAGE
		//    (`struct X : public A, B` -> paires inhType/inhBase), les MÉTHODES HORS-CLASSE
		//    (`Ret Cls::Meth(...)`) et les fonctions LIBRES avec leur retour (gfnName/gfnRet,
		//    pour `foo().`). Sert la complétion contextuelle INSTANTANÉE après '.', '->', '::'
		//    (le compilateur affine ensuite). Arrays parallèles ; accolades suivies hors
		//    chaînes/commentaires. ──
		inline void NkScanTextMembers(const char *text, NkVector<NkString> &owner, NkVector<NkString> &name,
									  NkVector<NkString> &mtype, NkVector<NkString> &inhType,
									  NkVector<NkString> &inhBase, NkVector<NkString> &gfnName,
									  NkVector<NkString> &gfnRet) {
			auto isW = [](char c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
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
			// Type/retour AVANT `nameStart` : saute blancs, *,&, args template, et les
			// qualificatifs (const/static/unsigned…) pour atteindre l'identifiant du type.
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
					// qualificatif : provisoire (unsigned/long seuls = type), continue à remonter
					int32 k2 = 0;
					while (tmp[k2]) {
						out[k2] = tmp[k2];
						++k2;
					}
					out[k2] = 0;
				}
			};

			struct Scope {
					char nm[96];
					int32 depth;
			};

			Scope stack[24];
			int32 sn = 0;
			char pending[96];
			pending[0] = 0;			  // type vu, en attente de son '{'
			int32 basesOn = 0;		  // dans la liste d'héritage `: A, B` (jusqu'au '{')
			int32 depth = 0, blk = 0; // profondeur d'accolades / bloc-commentaire
			const char *p = text;
			char line[1024];
			while (*p && owner.Size() < 30000) {
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
				// Passe B : accolades / ';' / ':' héritage / `struct|class|union NAME`, DANS L'ORDRE.
				for (int32 i = 0; i < cn; ++i) {
					const char c = cl[i];
					if (c == '{') {
						++depth;
						if (pending[0] && sn < 24) { // le corps du type vu s'ouvre ici
							Scope &sc = stack[sn++];
							int32 k = 0;
							while (pending[k] && k < 95) {
								sc.nm[k] = pending[k];
								++k;
							}
							sc.nm[k] = 0;
							sc.depth = depth;
						}
						pending[0] = 0;
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
							inhType.PushBack(NkString(pending));
							inhBase.PushBack(NkString(nm));
						}
						i = e - 1;
					} else if (!basesOn && (c == 's' || c == 'c' || c == 'u') && (i == 0 || !isW(cl[i - 1]))) {
						int32 adv = 0;
						if (i + 6 <= cn && cl[i] == 's' && cl[i + 1] == 't' && cl[i + 2] == 'r' && cl[i + 3] == 'u' &&
							cl[i + 4] == 'c' && cl[i + 5] == 't' && !isW(cl[i + 6]))
							adv = 6;
						else if (i + 5 <= cn && cl[i] == 'c' && cl[i + 1] == 'l' && cl[i + 2] == 'a' &&
								 cl[i + 3] == 's' && cl[i + 4] == 's' && !isW(cl[i + 5]))
							adv = 5;
						else if (i + 5 <= cn && cl[i] == 'u' && cl[i + 1] == 'n' && cl[i + 2] == 'i' &&
								 cl[i + 3] == 'o' && cl[i + 4] == 'n' && !isW(cl[i + 5]))
							adv = 5;
						if (!adv)
							continue;
						int32 s = i + adv;
						while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
							++s;
						int32 e = s;
						while (e < cn && isW(cl[e]))
							++e;
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
					int32 s = 0;
					while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
						++s;
					if (s < cn && cl[s] != '#') {
						// saute accès/using/typedef/friend/template (pas des membres nommés ici)
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
							// méthode : identifiant juste avant la 1re '(' ; champ : avant '=' sinon ';'.
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
								if (e2 > s2 && !(cl[s2] >= '0' && cl[s2] <= '9')) {
									char nm[96];
									int32 k = 0;
									for (int32 t = s2; t < e2 && k < 95; ++t)
										nm[k++] = cl[t];
									nm[k] = 0;
									if (!isKw(nm) && NkSymStrCmp(nm, stack[sn - 1].nm) != 0) { // pas le constructeur
										char rt[96];
										typeBefore(cl, s, s2, rt);
										owner.PushBack(NkString(stack[sn - 1].nm));
										name.PushBack(NkString(nm));
										mtype.PushBack(NkString(rt)); // "" si type introuvable
									}
								}
							}
						}
					}
				} else if (sn == 0 && cn > 0 && owner.Size() < 30000) {
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
											char rt[96];
											typeBefore(cl, s, cs, rt);
											owner.PushBack(NkString(cls));
											name.PushBack(NkString(nm));
											mtype.PushBack(NkString(rt));
										}
									} else {
										char rt[96];
										typeBefore(cl, s, s2, rt);
										if (rt[0] && !isKw(rt) && gfnName.Size() < 20000) {
											gfnName.PushBack(NkString(nm));
											gfnRet.PushBack(NkString(rt));
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
