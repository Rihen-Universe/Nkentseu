#pragma once
// =============================================================================
// NkDiagParse.h — extraction d'un diagnostic depuis une ligne de compilateur.
//
// Une SEULE implementation, deux usages :
//   - la gouttiere de l'editeur, qui ne garde que les diagnostics du fichier
//     affiche (NkCodeState::ParseDiagLine) ;
//   - le panneau Problemes, qui les veut TOUS, avec leur fichier.
//
// Ces deux besoins vivaient dans le meme code, filtre sur un fichier unique et
// ecrivant directement dans un document. Le panneau ne pouvait pas s'en servir,
// et en ecrire un second aurait garanti la derive : deux analyseurs pour un
// format qui, lui, ne change pas.
//
// Formats reconnus (les seuls que produisent nos chaines de compilation) :
//   clang/gcc   chemin:LIGNE:COL: error|fatal error|warning: message
//   MSVC        chemin(LIGNE,COL): error C1234: message
//               chemin(LIGNE): warning C4996: message
// « note: » et « remark: » sont volontairement ignores : ce sont des lignes de
// CONTEXTE rattachees au diagnostic precedent, pas des problemes en soi. Les
// lister doublerait le panneau sans rien apprendre.
// =============================================================================
#include "NKCode/Project/NkText.h" // NkFindSub
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace nkcode {

		// Severite d'un diagnostic. Les valeurs suivent celles deja utilisees par
		// la gouttiere de l'editeur (0 = avertissement, 1 = erreur) : les changer
		// ici changerait silencieusement la couleur des marqueurs.
		enum class NkDiagSev : uint8 { Warning = 0, Error = 1 };

		struct NkDiagInfo {
				NkString file; // tel qu'ecrit par le compilateur (absolu ou relatif au CWD du build)
				int32 line = 0;	  // 1-base, comme le compilateur l'ecrit
				int32 col = 0;	  // 1-base ; 0 quand le format n'en donne pas
				NkDiagSev sev = NkDiagSev::Error;
				NkString msg;
		};

		// Analyse UNE ligne. Rend false si elle ne porte aucun diagnostic — ce qui
		// est le cas de l'immense majorite des lignes d'un transcript de build.
		inline bool NkParseDiagLine(const char *p, NkDiagInfo &out) {
			if (!p || !*p)
				return false;
			auto isD = [](char c) { return c >= '0' && c <= '9'; };

			// ── clang / gcc ──────────────────────────────────────────────────────
			// On cherche le motif « :N:N: » plutot que de decouper sur ':' : sous
			// Windows le chemin en contient deja un (« C: »), et un decoupage naif
			// prend la lettre de lecteur pour un numero de ligne.
			for (int32 i = 0; p[i]; ++i) {
				if (p[i] != ':' || !isD(p[i + 1]))
					continue;
				int32 j = i + 1, line = 0;
				while (isD(p[j])) {
					line = line * 10 + (p[j] - '0');
					++j;
				}
				if (p[j] != ':' || !isD(p[j + 1]))
					continue;
				int32 k = j + 1, col = 0;
				while (isD(p[k])) {
					col = col * 10 + (p[k] - '0');
					++k;
				}
				if (p[k] != ':' || p[k + 1] != ' ')
					continue;
				const char *sevp = p + k + 2;
				NkDiagSev sev;
				if (NkFindSub(sevp, "error:") == sevp || NkFindSub(sevp, "fatal error:") == sevp)
					sev = NkDiagSev::Error;
				else if (NkFindSub(sevp, "warning:") == sevp)
					sev = NkDiagSev::Warning;
				else
					return false; // note:/remark: -> ligne de contexte, pas un probleme
				const char *msg = sevp;
				while (*msg && *msg != ':')
					++msg;
				if (*msg == ':') {
					++msg;
					while (*msg == ' ')
						++msg;
				}
				out.file = NkString(p, static_cast<usize>(i));
				out.line = line;
				out.col = col;
				out.sev = sev;
				out.msg = NkString(*msg ? msg : sevp);
				return line > 0;
			}

			// ── MSVC ─────────────────────────────────────────────────────────────
			const char *pe = NkFindSub(p, "): error ");
			NkDiagSev sev = NkDiagSev::Error;
			if (!pe) {
				pe = NkFindSub(p, "): warning ");
				sev = NkDiagSev::Warning;
			}
			if (!pe)
				return false;
			// Remonte jusqu'a la parenthese ouvrante qui precede : « chemin(L,C) ».
			int32 op = -1;
			for (int32 i = static_cast<int32>(pe - p); i >= 0; --i)
				if (p[i] == '(') {
					op = i;
					break;
				}
			if (op < 0)
				return false;
			int32 q = op + 1, line = 0, col = 0;
			while (isD(p[q])) {
				line = line * 10 + (p[q] - '0');
				++q;
			}
			if (p[q] == ',') {
				++q;
				while (isD(p[q])) {
					col = col * 10 + (p[q] - '0');
					++q;
				}
			}
			// Le message vient apres le code d'erreur : « error C1234: message ».
			const char *msg = pe + 2; // saute « ): »
			while (*msg == ' ')
				++msg;
			const char *colon = msg;
			while (*colon && *colon != ':')
				++colon;
			if (*colon == ':') {
				msg = colon + 1;
				while (*msg == ' ')
					++msg;
			}
			out.file = NkString(p, static_cast<usize>(op));
			out.line = line;
			out.col = col;
			out.sev = sev;
			out.msg = NkString(msg);
			return line > 0;
		}

	} // namespace nkcode
} // namespace nkentseu
