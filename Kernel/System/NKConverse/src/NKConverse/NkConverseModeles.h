#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseModeles.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LES MODELES REELS D'UN FOURNISSEUR, et ce que chacun SAIT faire --
//          pour que le panneau IA n'en ecrive aucun en dur.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI (Rodolf, 21/09 a 06h52) : « etant donne une IA donnee, on doit pouvoir
// choisir le modele a utiliser et si possible avoir des proprietes ». La liste
// du panneau vient d'ICI -- jamais d'un tableau ecrit en dur : un modele
// telecharge demain apparait sans une ligne de code, un modele efface disparait.
//
//   - OLLAMA : /api/tags (ce que rend `ollama list`), puis /api/show par modele
//     pour ses CAPACITES (thinking, vision, tools). Deux requetes au service de
//     la machine, quelques millisecondes.
//   - CLAUDE (le CLI) : ce que `claude --help` DOCUMENTE pour `--model` (les
//     alias et l'exemple de nom complet). ⚠️ RIEN N'EST FACTURE : l'aide est
//     locale. On n'envoie PAS `/model` au CLI -- NKCode le fait, mais c'est une
//     session ; ici la regle est « aucun appel facture », et l'aide suffit a ne
//     rien ecrire en dur.
//
// ⚠️ LES CAPACITES DECIDENT DES PROPRIETES. « Thinking » n'a de sens que pour un
//    modele qui l'annonce (deepseek-r1) ; le panneau GRISE l'interrupteur pour
//    les autres AVEC ce motif, plutot que d'ecrire `think` dans une requete que
//    le service refuserait (400) ou ignorerait.
//
// ⚠️ SYNCHRONE ET PONCTUEL : appele a l'ouverture du panneau ou sur demande,
//    jamais par image.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverseClaude.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKNetwork/HTTP/NkHTTPClient.h"
#include <cstdio>

namespace nkentseu::converse {

	struct NkConverseModeleInfo {
			NkString nom;		 ///< « qwen2.5:7b-instruct »
			NkString parametres; ///< « 7.6B »
			NkString quantif;	 ///< « Q4_K_M »
			NkString famille;	 ///< « qwen2 »
			nkentseu::uint64 octets = 0;
			bool pensee = false; ///< capacite « thinking »
			bool vision = false; ///< capacite « vision »
			bool outils = false; ///< capacite « tools »
			bool alias = false;	 ///< Claude : un alias du CLI (« opus »), pas un nom complet
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  L'EFFORT -> LE BUDGET DE REPONSE
	// ═══════════════════════════════════════════════════════════════════════
	/// ⚠️ UNE SEULE TABLE, lue par tous les hotes : deux traductions du meme
	///    reglage finiraient par valoir deux choses. -1 = aucun plafond ecrit
	///    (le cran le plus haut : le comportement d'avant l'Effort, inchange).
	///    Les crans bas restent assez larges pour un document entier : un
	///    budget qui tronque un .nkuidoc produirait un REFUS de format, et
	///    l'utilisateur accuserait le modele.
	inline nkentseu::int32 NkConverseBudgetEffort(nkentseu::int32 cran, nkentseu::int32 crans) {
		if (crans < 2 || cran >= crans - 1)
			return -1;
		static const nkentseu::int32 kBudget[] = {512, 2048, 8192};
		if (cran < 0)
			cran = 0;
		return cran < 3 ? kBudget[cran] : -1;
	}

	namespace modelesdetail {
		/// La chaine qui suit `"cle":"` a partir de `depuis`, jusqu'au guillemet
		/// fermant (les noms de modele n'echappent rien).
		inline bool Champ(const NkString &j, NkString::SizeType depuis, NkString::SizeType borne, const char *cle,
						  NkString &out) {
			NkString motif("\"");
			motif.Append(cle);
			motif.Append("\":\"");
			const NkString::SizeType i = j.Find(motif.Data(), depuis);
			if (i == NkString::npos || i >= borne)
				return false;
			const NkString::SizeType a = i + motif.Length();
			NkString::SizeType b = a;
			while (b < j.Length() && j[b] != '"')
				++b;
			out = NkString(j.Data() + a, b - a);
			return true;
		}
		inline nkentseu::uint64 Nombre(const NkString &j, NkString::SizeType depuis, NkString::SizeType borne,
									   const char *cle) {
			NkString motif("\"");
			motif.Append(cle);
			motif.Append("\":");
			const NkString::SizeType i = j.Find(motif.Data(), depuis);
			if (i == NkString::npos || i >= borne)
				return 0u;
			nkentseu::uint64 v = 0u;
			for (NkString::SizeType k = i + motif.Length(); k < j.Length() && j[k] >= '0' && j[k] <= '9'; ++k)
				v = v * 10u + (nkentseu::uint64)(j[k] - '0');
			return v;
		}

		/// Lance `ligne`, rend sa sortie standard (fusionnee avec l'erreur).
		/// Faux = le lancement lui-meme a echoue.
		inline bool Capturer(const char *ligne, NkString &sortie, nkentseu::uint32 delaiMs) {
			sortie = NkString();
#if defined(_WIN32)
			SECURITY_ATTRIBUTES sa;
			memset(&sa, 0, sizeof(sa));
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			HANDLE lire = nullptr, ecrire = nullptr;
			if (!CreatePipe(&lire, &ecrire, &sa, 0))
				return false;
			SetHandleInformation(lire, HANDLE_FLAG_INHERIT, 0);
			const int n = MultiByteToWideChar(CP_UTF8, 0, ligne, -1, nullptr, 0);
			if (n <= 0) {
				CloseHandle(lire);
				CloseHandle(ecrire);
				return false;
			}
			wchar_t *w = new wchar_t[(nkentseu::usize)n];
			MultiByteToWideChar(CP_UTF8, 0, ligne, -1, w, n);
			STARTUPINFOW si;
			PROCESS_INFORMATION pi;
			memset(&si, 0, sizeof(si));
			memset(&pi, 0, sizeof(pi));
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = ecrire;
			si.hStdError = ecrire;
			si.hStdInput = nullptr;
			const BOOL ok = CreateProcessW(nullptr, w, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
										   &si, &pi);
			delete[] w;
			CloseHandle(ecrire);
			if (!ok) {
				CloseHandle(lire);
				return false;
			}
			char buf[4096];
			DWORD lu = 0;
			while (ReadFile(lire, buf, sizeof(buf), &lu, nullptr) && lu > 0)
				sortie.Append(buf, (NkString::SizeType)lu);
			// ⚠️ ON N'ATTEND QUE LE PROCESSUS QU'ON A LANCE, et on ne le tue pas :
			//    une aide qui ne rend pas en `delaiMs` est abandonnee, pas abattue.
			WaitForSingleObject(pi.hProcess, delaiMs);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			CloseHandle(lire);
			return true;
#else
			(void)delaiMs;
			std::FILE *f = popen(ligne, "r");
			if (!f)
				return false;
			char buf[4096];
			nkentseu::usize lu;
			while ((lu = std::fread(buf, 1, sizeof(buf), f)) > 0)
				sortie.Append(buf, (NkString::SizeType)lu);
			pclose(f);
			return true;
#endif
		}
	} // namespace modelesdetail

	// ═══════════════════════════════════════════════════════════════════════
	//  OLLAMA
	// ═══════════════════════════════════════════════════════════════════════
	/// Liste les modeles du service `hote`. Rend faux ET NOMME la raison.
	inline bool NkConverseOllamaModeles(const char *hote, nkentseu::NkVector<NkConverseModeleInfo> &out,
										NkString &motif) {
		out.Clear();
		nkentseu::net::NkHTTPClient http;
		nkentseu::net::NkHTTPClient::Config cfg;
		cfg.defaultTimeoutMs = 3000u;
		http.Configure(cfg);
		NkString url(hote ? hote : "http://127.0.0.1:11434");
		url.Append("/api/tags");
		const nkentseu::net::NkHTTPResponse r = http.Get(url.Data());
		if (r.statusCode != 200u) {
			motif = NkString("Lancez « ollama serve » : le service de modeles ne repond pas (");
			motif.Append(r.error.Length() ? r.error.Data() : "aucune reponse");
			motif.Append(")");
			return false;
		}
		const NkString &j = r.body;
		// Chaque modele commence par `{"name":` ; on borne la lecture de ses
		// champs au debut du suivant -- un champ absent ne se vole pas au voisin.
		nkentseu::NkVector<NkString::SizeType> debuts;
		for (NkString::SizeType i = j.Find("{\"name\":\"", 0); i != NkString::npos; i = j.Find("{\"name\":\"", i + 1))
			debuts.PushBack(i);
		for (nkentseu::usize k = 0; k < debuts.Size(); ++k) {
			const NkString::SizeType a = debuts[k];
			const NkString::SizeType b = (k + 1 < debuts.Size()) ? debuts[k + 1] : j.Length();
			NkConverseModeleInfo m;
			modelesdetail::Champ(j, a, b, "name", m.nom);
			modelesdetail::Champ(j, a, b, "parameter_size", m.parametres);
			modelesdetail::Champ(j, a, b, "quantization_level", m.quantif);
			modelesdetail::Champ(j, a, b, "family", m.famille);
			m.octets = modelesdetail::Nombre(j, a, b, "size");
			// LES CAPACITES, demandees au service -- pas devinees sur le nom.
			NkString urlShow(hote ? hote : "http://127.0.0.1:11434");
			urlShow.Append("/api/show");
			NkString corps("{\"model\":\"");
			corps.Append(m.nom.Data());
			corps.Append("\"}");
			const nkentseu::net::NkHTTPResponse s = http.Post(urlShow.Data(), corps.Data());
			if (s.statusCode == 200u) {
				const NkString::SizeType c = s.body.Find("\"capabilities\":[", 0);
				if (c != NkString::npos) {
					NkString::SizeType f = c;
					while (f < s.body.Length() && s.body[f] != ']')
						++f;
					const NkString caps(s.body.Data() + c, f - c);
					m.pensee = caps.Find("\"thinking\"", 0) != NkString::npos;
					m.vision = caps.Find("\"vision\"", 0) != NkString::npos;
					m.outils = caps.Find("\"tools\"", 0) != NkString::npos;
				}
			}
			if (m.nom.Length())
				out.PushBack(m);
		}
		if (out.Size() == 0) {
			motif = NkString("Aucun modele installe : « ollama pull <modele> » en installe un.");
			return false;
		}
		return true;
	}

	/// LE MODELE EST-IL CHARGE, ET OU ? (/api/ps, ce que rend `ollama ps`).
	/// `octets` = sa taille en memoire, `vram` = la part en memoire video. Faux =
	/// pas en memoire (il le sera a la prochaine demande, apres chargement) ;
	/// `motif` non vide = le service n'a pas repondu.
	inline bool NkConverseOllamaCharge(const char *hote, const char *nom, nkentseu::uint64 &octets,
									   nkentseu::uint64 &vram, NkString &motif) {
		octets = vram = 0u;
		motif = NkString();
		nkentseu::net::NkHTTPClient http;
		nkentseu::net::NkHTTPClient::Config cfg;
		cfg.defaultTimeoutMs = 2000u;
		http.Configure(cfg);
		NkString url(hote ? hote : "http://127.0.0.1:11434");
		url.Append("/api/ps");
		const nkentseu::net::NkHTTPResponse r = http.Get(url.Data());
		if (r.statusCode != 200u) {
			motif = NkString("le service de modeles ne repond pas");
			return false;
		}
		NkString motifNom("{\"name\":\"");
		motifNom.Append(nom ? nom : "");
		motifNom.Append("\"");
		const NkString::SizeType i = r.body.Find(motifNom.Data(), 0);
		if (i == NkString::npos)
			return false;
		NkString::SizeType b = r.body.Find("{\"name\":\"", i + 1);
		if (b == NkString::npos)
			b = r.body.Length();
		octets = modelesdetail::Nombre(r.body, i, b, "size");
		vram = modelesdetail::Nombre(r.body, i, b, "size_vram");
		return true;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  CLAUDE (le CLI)
	// ═══════════════════════════════════════════════════════════════════════
	/// Les noms que `claude --help` documente pour `--model`. Rend faux ET NOMME
	/// la raison (CLI absent, aide sans l'option).
	inline bool NkConverseClaudeModeles(nkentseu::NkVector<NkConverseModeleInfo> &out, NkString &motif) {
		out.Clear();
		const NkString &exe = NkClaudeExe();
		if (exe.Length() == 0) {
			motif = NkString("CLI « claude » introuvable : npm i -g @anthropic-ai/claude-code");
			return false;
		}
		NkString ligne("\"");
		ligne.Append(exe.Data());
		ligne.Append("\" --help");
		NkString aide;
		if (!modelesdetail::Capturer(ligne.Data(), aide, 20000u)) {
			motif = NkString("le CLI « claude » ne s'est pas lance");
			return false;
		}
		// Le bloc de `--model` : de son drapeau a l'option suivante (une ligne
		// qui commence par « -- » ou « -x, » apres l'indentation).
		const NkString::SizeType d = aide.Find("--model <", 0);
		if (d == NkString::npos) {
			motif = NkString("l'aide du CLI ne documente pas --model");
			return false;
		}
		NkString::SizeType f = d + 1;
		for (;;) {
			const NkString::SizeType nl = aide.Find("\n", f);
			if (nl == NkString::npos) {
				f = aide.Length();
				break;
			}
			NkString::SizeType k = nl + 1;
			while (k < aide.Length() && aide[k] == ' ')
				++k;
			if (k < aide.Length() && aide[k] == '-' && k - (nl + 1) < 8u) {
				f = nl;
				break;
			}
			f = nl + 1;
		}
		// Les mots entre apostrophes : « 'fable', 'opus', or 'sonnet' » et
		// « 'claude-fable-5' ».
		for (NkString::SizeType i = d; i < f; ++i) {
			if (aide[i] != '\'')
				continue;
			NkString::SizeType j = i + 1;
			while (j < f && aide[j] != '\'' && aide[j] != ' ' && aide[j] != '\n')
				++j;
			if (j < f && aide[j] == '\'' && j > i + 1) {
				NkConverseModeleInfo m;
				m.nom = NkString(aide.Data() + i + 1, j - i - 1);
				m.alias = m.nom.Find("-", 0) == NkString::npos;
				bool deja = false;
				for (nkentseu::usize q = 0; q < out.Size(); ++q)
					deja = deja || out[q].nom == m.nom;
				if (!deja)
					out.PushBack(m);
				i = j;
			}
		}
		if (out.Size() == 0) {
			motif = NkString("l'aide du CLI ne cite aucun modele pour --model");
			return false;
		}
		return true;
	}

	/// UNE LIGNE DE DESCRIPTION, comme « Select a model » : taille, lieu, et a
	/// quoi il sert -- tire des capacites et de la famille, jamais invente.
	inline NkString NkConverseDecrireModele(const NkConverseModeleInfo &m, bool distant = false) {
		char b[192];
		if (distant) {
			snprintf(b, sizeof(b), "%s · distant · selon l'aide du CLI claude",
					 m.alias ? "alias : la derniere version de la famille" : "nom complet d'un modele");
			return NkString(b);
		}
		const double go = (double)m.octets / 1.0e9;
		const char *usage = m.vision ? "images et texte" : (m.pensee ? "raisonne avant de repondre" : "texte");
		if (m.famille.Find("bert", 0) != NkString::npos)
			usage = "plongements (pas de conversation)";
		snprintf(b, sizeof(b), "%s%s%s · %.1f Go · local · %s%s", m.parametres.Length() ? m.parametres.Data() : "?",
				 m.quantif.Length() ? " " : "", m.quantif.Length() ? m.quantif.Data() : "", go, usage,
				 m.outils ? " · outils" : "");
		return NkString(b);
	}

} // namespace nkentseu::converse
