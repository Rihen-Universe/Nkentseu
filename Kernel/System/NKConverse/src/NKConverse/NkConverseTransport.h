#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverseTransport.h
// @Brief   UNE REQUETE HTTP VERS UN FOURNISSEUR DE MODELES, LUE EN FLUX -- par
//          NKNetwork, ou par `curl` quand le depot n'a pas compile HTTPS.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI DEUX CHEMINS (2026-10-01)
//   - http:// (Ollama, LM Studio, llama.cpp server, vLLM sur ce PC ou sur un PC
//     du reseau) : NkHTTPClient, avec le FLUX ajoute le meme jour
//     (`NkHTTPRequest::surCorps`). Rien ne sort de la maison.
//   - https:// (l'API d'Anthropic, un service compatible OpenAI en ligne) :
//     NkHTTPClient SI le depot a ete construit avec NK_ENABLE_TLS=1. Ce n'est
//     pas le defaut, et c'est mesure : `NKNetwork.jenga` dit que mbed-TLS a
//     fait DISPARAITRE la lecture des PDF le 11/08, et l'option est restee a
//     l'arret. Sans elle, toute requete https rendait « HTTPS not compiled in ».
//     Le second chemin est `curl`, livre avec Windows 10 et 11
//     (System32\curl.exe), lance comme un processus -- et LU EN FLUX par son
//     tuyau de sortie.
//
// ⚠️⚠️ LA CLE NE PASSE JAMAIS SUR LA LIGNE DE COMMANDE.
//   NKCode le fait (`curl ... -H "x-api-key: <cle>"`, NkAiPanel.h l. 6217) : la
//   cle est alors LISIBLE dans la table des processus de la machine par tout
//   programme du meme utilisateur. Ici, les en-tetes partent dans un FICHIER
//   TEMPORAIRE (`-H @fichier`, curl >= 7.55) et le corps aussi
//   (`--data-binary @fichier`) ; les deux sont EFFACES des que curl a fini. La
//   ligne de commande ne porte que l'adresse et deux chemins.
//
// LE FLUX
//   `flux(statut, octets, n)` recoit le CORPS au fil de l'eau, deja debarrasse
//   du cadrage `chunked` (NKNetwork) ou des en-tetes (curl -i). Rendre false
//   arrete tout : la requete NKNetwork est coupee, curl est termine.
// -----------------------------------------------------------------------------

#include "NKConverse/NkConverse.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkAtomic.h"
#include "NKNetwork/HTTP/NkHTTPClient.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#if !defined(_WIN32)
	#include <unistd.h>
#endif

namespace nkentseu::converse {

	struct NkEnteteIA {
			NkString cle;
			NkString valeur;
	};

	struct NkRequeteHttpIA {
			bool post = true;
			NkString url;
			NkVector<NkEnteteIA> entetes; ///< ⚠️ peut porter une CLE : jamais journalise
			NkString corps;
			/// Delai d'INACTIVITE en flux, delai total sinon. Le premier appel d'un
			/// modele local le charge depuis le disque : 34,7 s mesurees le 17/09.
			nkentseu::uint32 delaiMs = 300000u;
	};

	struct NkResultatHttpIA {
			nkentseu::uint32 statut = 0u; ///< 0 = personne n'a repondu
			NkString corps;				  ///< le corps ENTIER (aussi en flux)
			NkString erreur;			  ///< ce qui a manque, quand `statut == 0`
			bool viaCurl = false;
			bool annule = false;		  ///< le lecteur du flux a demande l'arret
	};

	using NkFluxHttpIA = nkentseu::NkFunction<bool(nkentseu::uint32, const char *, nkentseu::uint32)>;

	namespace transportdetail {

		inline bool CommencePar(const NkString &s, const char *p) {
			return s.StartsWith(p);
		}

		/// Un nom de fichier temporaire UNIQUE (processus, compteur, horloge).
		inline NkString FichierTemporaire(const char *suffixe) {
			static nkentseu::NkAtomic<nkentseu::uint32> sCompteur{0u};
			const nkentseu::uint32 n = sCompteur.FetchAdd(1u);
			const char *tmp = std::getenv("TEMP");
			if (!tmp || !*tmp)
				tmp = std::getenv("TMP");
#if defined(_WIN32)
			const unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
			const unsigned long t = static_cast<unsigned long>(GetTickCount());
			if (!tmp || !*tmp)
				tmp = ".";
#else
			const unsigned long pid = static_cast<unsigned long>(getpid());
			const unsigned long t = static_cast<unsigned long>(std::rand());
			if (!tmp || !*tmp)
				tmp = "/tmp";
#endif
			char b[512];
			std::snprintf(b, sizeof(b), "%s/nkconverse_%lu_%lu_%u_%s", tmp, pid, t, static_cast<unsigned>(n),
						  suffixe ? suffixe : "tmp");
			return NkString(b);
		}

		/// L'executable curl : celui de Windows d'abord (il est la depuis 1803),
		/// puis le PATH. Chaine vide = aucun.
		inline NkString CurlExe() {
#if defined(_WIN32)
			if (const char *sr = std::getenv("SystemRoot")) {
				NkString c(sr);
				c.Append("\\System32\\curl.exe");
				if (nkentseu::NkFile::Exists(c.CStr()))
					return c;
			}
			return NkString("curl.exe");
#else
			return NkString("curl");
#endif
		}

		/// Lance `ligne`, livre sa sortie (stdout + stderr) au fil de l'eau a `lire`.
		/// `lire` rend false : le processus est termine. Rend false si le lancement
		/// lui-meme echoue ; `code` = le code de sortie.
		inline bool LancerEnFlux(const NkString &ligne, const nkentseu::NkFunction<bool(const char *, nkentseu::uint32)> &lire,
								 nkentseu::int32 &code, bool &coupe) {
			code = -1;
			coupe = false;
#if defined(_WIN32)
			SECURITY_ATTRIBUTES sa;
			std::memset(&sa, 0, sizeof(sa));
			sa.nLength = sizeof(sa);
			sa.bInheritHandle = TRUE;
			HANDLE lecture = nullptr, ecriture = nullptr;
			if (!CreatePipe(&lecture, &ecriture, &sa, 0))
				return false;
			SetHandleInformation(lecture, HANDLE_FLAG_INHERIT, 0);
			const int n = MultiByteToWideChar(CP_UTF8, 0, ligne.CStr(), -1, nullptr, 0);
			if (n <= 0) {
				CloseHandle(lecture);
				CloseHandle(ecriture);
				return false;
			}
			wchar_t *w = new wchar_t[static_cast<nkentseu::usize>(n)];
			MultiByteToWideChar(CP_UTF8, 0, ligne.CStr(), -1, w, n);
			STARTUPINFOW si;
			PROCESS_INFORMATION pi;
			std::memset(&si, 0, sizeof(si));
			std::memset(&pi, 0, sizeof(pi));
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = ecriture;
			si.hStdError = ecriture;
			si.hStdInput = nullptr;
			const BOOL ok = CreateProcessW(nullptr, w, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
			delete[] w;
			CloseHandle(ecriture);
			if (!ok) {
				CloseHandle(lecture);
				return false;
			}
			char buf[4096];
			DWORD lu = 0;
			while (ReadFile(lecture, buf, sizeof(buf), &lu, nullptr) && lu > 0) {
				if (!lire(buf, static_cast<nkentseu::uint32>(lu))) {
					coupe = true;
					TerminateProcess(pi.hProcess, 1);
					break;
				}
			}
			WaitForSingleObject(pi.hProcess, 10000);
			DWORD ec = static_cast<DWORD>(-1);
			GetExitCodeProcess(pi.hProcess, &ec);
			code = static_cast<nkentseu::int32>(ec);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			CloseHandle(lecture);
			return true;
#else
			NkString l = ligne;
			l.Append(" 2>&1");
			std::FILE *f = popen(l.CStr(), "r");
			if (!f)
				return false;
			char buf[4096];
			nkentseu::usize lu;
			while ((lu = std::fread(buf, 1, sizeof(buf), f)) > 0) {
				if (!lire(buf, static_cast<nkentseu::uint32>(lu))) {
					coupe = true;
					break;
				}
			}
			code = static_cast<nkentseu::int32>(pclose(f));
			return true;
#endif
		}

		/// Le chemin NKNetwork (http, ou https quand TLS est compile).
		inline void ParNKNetwork(const NkRequeteHttpIA &rq, NkResultatHttpIA &res, const NkFluxHttpIA *flux) {
			nkentseu::net::NkHTTPClient http;
			nkentseu::net::NkHTTPClient::Config cfg;
			cfg.defaultTimeoutMs = rq.delaiMs;
			cfg.userAgent = NkString("NKConverse/1.0");
			http.Configure(cfg);
			nkentseu::net::NkHTTPRequest r;
			r.url = rq.url;
			r.method = rq.post ? nkentseu::net::NkHTTPMethod::NK_HTTP_POST : nkentseu::net::NkHTTPMethod::NK_HTTP_GET;
			r.timeoutMs = rq.delaiMs;
			if (rq.post) {
				r.body = rq.corps;
				r.AddHeader("Content-Type", "application/json");
			}
			for (nkentseu::usize i = 0; i < rq.entetes.Size(); ++i)
				r.AddHeader(rq.entetes[i].cle.CStr(), rq.entetes[i].valeur.CStr());
			if (flux && *flux != nullptr) {
				const NkFluxHttpIA *f = flux;
				r.surCorps = [f](nkentseu::uint32 st, const char *d, nkentseu::uint32 n) { return (*f)(st, d, n); };
			}
			const nkentseu::net::NkHTTPResponse rep = http.Send(r);
			res.statut = rep.statusCode;
			res.corps = rep.body;
			res.erreur = rep.error;
			res.annule = rep.error == "flux arrete par l'appelant";
		}

		/// Le chemin curl : en-tetes et corps dans des FICHIERS (voir l'en-tete).
		inline void ParCurl(const NkRequeteHttpIA &rq, NkResultatHttpIA &res, const NkFluxHttpIA *flux) {
			res.viaCurl = true;
			const NkString fEntetes = FichierTemporaire("entetes.txt");
			const NkString fCorps = FichierTemporaire("corps.json");
			NkString entetes;
			if (rq.post)
				entetes.Append("Content-Type: application/json\n");
			for (nkentseu::usize i = 0; i < rq.entetes.Size(); ++i) {
				entetes.Append(rq.entetes[i].cle);
				entetes.Append(": ");
				entetes.Append(rq.entetes[i].valeur);
				entetes.Append("\n");
			}
			if (!nkentseu::NkFile::WriteAllText(fEntetes.CStr(), entetes.CStr())) {
				res.erreur = NkString("impossible d'ecrire le fichier d'en-tetes temporaire de curl");
				return;
			}
			if (rq.post && !nkentseu::NkFile::WriteAllText(fCorps.CStr(), rq.corps.CStr())) {
				nkentseu::NkFile::Delete(fEntetes.CStr());
				res.erreur = NkString("impossible d'ecrire le corps temporaire de curl");
				return;
			}
			NkString ligne("\"");
			ligne.Append(CurlExe());
			ligne.Append("\" -sS -N -i");
			char d[64];
			std::snprintf(d, sizeof(d), " --connect-timeout 10 --max-time %u", static_cast<unsigned>(rq.delaiMs / 1000u + 30u));
			ligne.Append(d);
			ligne.Append(rq.post ? " -X POST" : " -X GET");
			ligne.Append(" -H @\"");
			ligne.Append(fEntetes);
			ligne.Append("\"");
			if (rq.post) {
				ligne.Append(" --data-binary @\"");
				ligne.Append(fCorps);
				ligne.Append("\"");
			}
			ligne.Append(" \"");
			ligne.Append(rq.url);
			ligne.Append("\"");

			// LA LECTURE : les en-tetes d'abord (sautant un « 100 Continue » ou un
			// « Connection established » de mandataire), le corps ensuite.
			NkString brut;
			nkentseu::usize finEntetes = 0u;
			bool enCorps = false;
			nkentseu::usize livre = 0u;
			const nkentseu::NkFunction<bool(const char *, nkentseu::uint32)> lire = [&](const char *p, nkentseu::uint32 n) {
				brut.Append(p, static_cast<NkString::SizeType>(n));
				while (!enCorps) {
					const NkString::SizeType fin = brut.Find("\r\n\r\n", static_cast<NkString::SizeType>(finEntetes));
					if (fin == NkString::npos)
						return true;
					NkString bloc(brut.CStr() + finEntetes, static_cast<NkString::SizeType>(fin - finEntetes));
					nkentseu::uint32 st = 0u;
					const NkString::SizeType sp = bloc.Find(' ');
					if (CommencePar(bloc, "HTTP/") && sp != NkString::npos)
						for (NkString::SizeType k = sp + 1; k < bloc.Length() && bloc[k] >= '0' && bloc[k] <= '9'; ++k)
							st = st * 10u + static_cast<nkentseu::uint32>(bloc[k] - '0');
					finEntetes = static_cast<nkentseu::usize>(fin + 4u);
					if (st == 0u || (st >= 100u && st < 200u) || bloc.Find("onnection established") != NkString::npos)
						continue; // un bloc intermediaire : le vrai suit
					res.statut = st;
					enCorps = true;
					livre = finEntetes;
				}
				if (enCorps && brut.Length() > livre) {
					const nkentseu::usize debut = livre;
					livre = static_cast<nkentseu::usize>(brut.Length());
					if (flux && *flux != nullptr &&
						!(*flux)(res.statut, brut.CStr() + debut, static_cast<nkentseu::uint32>(livre - debut)))
						return false;
				}
				return true;
			};
			nkentseu::int32 code = -1;
			bool coupe = false;
			const bool lance = LancerEnFlux(ligne, lire, code, coupe);
			nkentseu::NkFile::Delete(fEntetes.CStr()); // ⚠️ la cle ne reste pas sur le disque
			if (rq.post)
				nkentseu::NkFile::Delete(fCorps.CStr());
			res.annule = coupe;
			if (!lance) {
				res.erreur = NkString("curl ne s'est pas lance (ni HTTPS compile, ni curl sur cette machine)");
				return;
			}
			if (enCorps) {
				res.corps = NkString(brut.CStr() + finEntetes, static_cast<NkString::SizeType>(brut.Length() - finEntetes));
				return;
			}
			// Aucun en-tete : tout ce que curl a ecrit est son message d'erreur
			// (« curl: (6) Could not resolve host... »).
			res.statut = 0u;
			res.erreur = brut.Length() ? brut : NkString("curl n'a rien rendu");
			while (res.erreur.Length() && (res.erreur[res.erreur.Length() - 1] == '\n' || res.erreur[res.erreur.Length() - 1] == '\r'))
				res.erreur.PopBack();
			char c[48];
			std::snprintf(c, sizeof(c), " (code %d)", static_cast<int>(code));
			res.erreur.Append(c);
		}

	} // namespace transportdetail

	/// LA requete. https sans TLS compile -> curl, en le disant (`viaCurl`).
	inline bool NkHttpIA(const NkRequeteHttpIA &rq, NkResultatHttpIA &res, const NkFluxHttpIA *flux = nullptr) {
		res = NkResultatHttpIA();
		const bool https = transportdetail::CommencePar(rq.url, "https://");
		transportdetail::ParNKNetwork(rq, res, flux);
		if (https && res.statut == 0u && !res.annule && res.erreur.Find("HTTPS not compiled") != NkString::npos) {
			res = NkResultatHttpIA();
			transportdetail::ParCurl(rq, res, flux);
		}
		return res.statut != 0u;
	}

} // namespace nkentseu::converse
