#pragma once
// -----------------------------------------------------------------------------
// @File    NkGenerateur.h
// @Brief   GENIA -- L'INTERFACE du generateur : UNE seule question,
//          « une image -> un chemin glTF ». Rien d'autre ne traverse.
//
//          POURQUOI UNE INTERFACE : TRELLIS, TripoSR ou un service distant se
//          remplacent SANS toucher au modeleur. « Un producteur qui connait son
//          consommateur en aura bientot deux et servira mal les deux. » Le
//          modeleur ne sait pas comment le glTF est fabrique ; il sait qu'il
//          en recoit un, et il l'importe par NkGLTFLoader comme n'importe quel
//          fichier lache dans le navigateur.
//
//          POURQUOI UN PROCESSUS EXTERNE (NkGenerateurProcessus) : les modeles
//          sont en PyTorch. Python n'est PAS une dependance du build C++ : on
//          lance une ligne de commande, on attend, on verifie que le fichier
//          demande existe. C'est tout ce que le pont sait faire, et c'est
//          voulu -- AUCUNE logique de generation ici.
//
//          DETTE DECLAREE : l'attente est SYNCHRONE (la fenetre ne repond pas
//          pendant la generation, ~10-60 s). C'est le MVP-0 ; la mise sur un
//          fil (NKThreading) viendra quand la chaine aura prouve qu'elle tient.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace nkentseu {
	namespace nk3d {

		/// L'INTERFACE. Une image entre, un chemin glTF sort. `why` nomme le
		/// refus (l'appelant l'affiche, il ne devine pas).
		class NkIGenerateur {
			public:
				virtual ~NkIGenerateur() {}
				/// Produit `outGltfPath` (.glb ou .gltf) depuis `imagePath`. Rend vrai
				/// si le fichier existe a la sortie. Ne rend JAMAIS vrai sans fichier.
				virtual bool Generer(const char *imagePath, const char *outGltfPath, NkString &why) = 0;
				/// ── LA SECONDE PORTE : DU TEXTE VERS UN FICHIER ─────────────────
				/// Demandee par l'agent « texte vers 3D » (navette du 17/09,
				/// `echanges/genia-3d.questions.md` R4.1). Elle s'AJOUTE, elle ne
				/// remplace rien : les deux entrees coexistent.
				///
				/// ⚠️ `outPath` ET NON `outGltfPath`, ET CE N'EST PAS UN DETAIL :
				///    son producteur ecrit du **.obj**, parce qu'AUCUN ecrivain glTF
				///    n'existe dans le depot. Nommer le parametre « gltf » aurait
				///    impose un format que personne ne sait ecrire.
				///
				/// ⚠️ LE DEFAUT EST UN REFUS NOMME, JAMAIS UN REPLI MUET. Une
				///    implementation qui ne sait pas produire depuis du texte le DIT
				///    et n'ecrit rien. Un repli qui rendrait un objet par defaut
				///    ferait apparaitre une forme que personne n'a demandee -- et
				///    l'utilisateur la prendrait pour la reponse.
				virtual bool GenererDepuisTexte(const char *invite, const char *outPath,
												NkString &why) {
					(void)invite;
					(void)outPath;
					why = "ce generateur ne sait pas produire depuis du texte (seule l'image est supportee)";
					return false;
				}
				/// Le nom du generateur, pour le journal (« TripoSR (processus) »).
				virtual const char *Nom() const = 0;
		};

		/// LE GENERATEUR PAR PROCESSUS EXTERNE. Un GABARIT de ligne de commande
		/// avec deux trous, `{image}` et `{out}` ; on le remplit, on lance, on
		/// attend, on verifie le fichier. Le gabarit vient de NK_GENIA_CMD, ou
		/// se compose par defaut de NK_GENIA_PYTHON (sinon `python`) et de
		/// NK_GENIA_SCRIPT (sinon `Tools/Genia/genia_triposr.py`, relatif au
		/// dossier courant -- les temoins se lancent depuis la racine).
		class NkGenerateurProcessus : public NkIGenerateur {
			public:
				NkString gabarit; // ex. "\"C:/.../python.exe\" \"Tools/Genia/genia_triposr.py\" --image \"{image}\" --out \"{out}\""
				/// LE GABARIT DE LA PORTE TEXTE, avec deux trous `{invite}` et `{out}`.
				/// VIDE PAR DEFAUT, et c'est voulu : tant que personne ne l'a rempli,
				/// `GenererDepuisTexte` refuse EN LE DISANT plutot que de lancer la
				/// commande de l'image avec une invite a la place d'un fichier.
				NkString gabaritTexte;
				NkString nom = "TripoSR (processus externe)";

				const char *Nom() const override {
					return nom.CStr();
				}

				/// Remplace CHAQUE occurrence de `trou` par `val` dans `s`.
				static void Remplir(NkString &s, const char *trou, const char *val) {
					const NkString::SizeType lt = (NkString::SizeType)std::strlen(trou);
					NkString::SizeType pos = 0;
					for (;;) {
						NkString::SizeType i = s.Find(trou, pos);
						if (i == NkString::npos)
							break;
						s.Replace(i, lt, val);
						pos = i + (NkString::SizeType)std::strlen(val);
					}
				}

				bool Generer(const char *imagePath, const char *outGltfPath, NkString &why) override {
					why.Clear();
					if (!imagePath || !imagePath[0] || !NkFile::Exists(imagePath)) {
						why = NkString::Format("image introuvable : '%s'", imagePath ? imagePath : "(null)");
						return false;
					}
					if (gabarit.Empty()) {
						why = "aucune commande de generation (NK_GENIA_CMD vide)";
						return false;
					}
					NkString cmd = gabarit;
					Remplir(cmd, "{image}", imagePath);
					Remplir(cmd, "{out}", outGltfPath);
					// Un fichier de sortie qui preexiste rendrait un echec invisible :
					// on l'efface AVANT, pour que « le fichier existe apres » veuille
					// dire « ce lancement l'a ecrit ».
					if (NkFile::Exists(outGltfPath))
						NkFile::Delete(outGltfPath);
					NkLog::Instance().Infof("[genia] lancement : %s", cmd.CStr());
					int32 code = -1;
					NkString sortie;
					if (!Lancer(cmd.CStr(), code, sortie)) {
						why = NkString::Format("le processus n'a pas pu etre lance : %s", cmd.CStr());
						return false;
					}
					if (!NkFile::Exists(outGltfPath)) {
						why = Motif(code, outGltfPath, sortie);
						return false;
					}
					if (code != 0)
						NkLog::Instance().Infof("[genia] le generateur a rendu %d mais le fichier existe : on l'importe", code);
					return true;
				}

				/// ── LA PORTE TEXTE ──────────────────────────────────────────────
				/// Meme mecanique, autre gabarit. Si personne ne l'a rempli, on
				/// REFUSE EN LE DISANT -- et le motif nomme la variable a poser,
				/// parce qu'un refus qui ne dit pas quoi faire oblige a lire le code.
				bool GenererDepuisTexte(const char *invite, const char *outPath, NkString &why) override {
					why.Clear();
					if (!invite || !invite[0]) {
						why = "aucune invite : il n'y a rien a produire";
						return false;
					}
					if (gabaritTexte.Empty()) {
						why = "ce generateur n'a pas de commande texte (posez NK_GENIA_CMD_TEXTE"
							  " ou NK_GENIA_SCRIPT_TEXTE) ; rien n'a ete ecrit";
						return false;
					}
					NkString cmd = gabaritTexte;
					Remplir(cmd, "{invite}", invite);
					Remplir(cmd, "{out}", outPath);
					if (NkFile::Exists(outPath))
						NkFile::Delete(outPath);
					NkLog::Instance().Infof("[genia] lancement (texte) : %s", cmd.CStr());
					int32 code = -1;
					NkString sortie;
					if (!Lancer(cmd.CStr(), code, sortie)) {
						why = NkString::Format("le processus n'a pas pu etre lance : %s", cmd.CStr());
						return false;
					}
					if (!NkFile::Exists(outPath)) {
						why = Motif(code, outPath, sortie);
						return false;
					}
					return true;
				}

			private:
				/// ── LE MOTIF, ET IL VIENT DU PROCESSUS LUI-MEME ─────────────────
				/// ⚠️ C'EST LE DEFAUT QUE LA NAVETTE A SIGNALE : le pont ne lisait
				///    PAS la sortie du sous-processus. Un script qui refusait poliment
				///    ("invite vide", "modele absent") voyait son motif jete, et
				///    l'ecran affichait « n'a pas ecrit le fichier » -- un echec sans
				///    cause, qui envoie chercher au mauvais endroit et finit par
				///    accuser l'utilisateur.
				/// On rend la DERNIERE ligne non vide : c'est la ou un script pose son
				/// erreur. Si la sortie est vide, on le DIT au lieu de laisser croire
				/// qu'on l'a lue.
				static NkString Motif(int32 code, const char *out, const NkString &sortie) {
					NkString derniere;
					const char *s = sortie.CStr();
					const NkString::SizeType n = (NkString::SizeType)std::strlen(s);
					NkString::SizeType fin = n;
					while (fin > 0) {
						// sauter les fins de ligne et les blancs de queue
						while (fin > 0 && (s[fin - 1] == '\n' || s[fin - 1] == '\r' ||
										   s[fin - 1] == ' ' || s[fin - 1] == '\t'))
							--fin;
						if (fin == 0)
							break;
						NkString::SizeType deb = fin;
						while (deb > 0 && s[deb - 1] != '\n' && s[deb - 1] != '\r')
							--deb;
						derniere = NkString(s + deb, fin - deb);
						break;
					}
					if (derniere.Empty())
						return NkString::Format(
							"le generateur a rendu %d, n'a pas ecrit '%s', et n'a RIEN imprime",
							code, out ? out : "(null)");
					return NkString::Format("le generateur a rendu %d et n'a pas ecrit '%s' -- il dit : %s",
											code, out ? out : "(null)", derniere.CStr());
				}

				/// Lance la ligne, ATTEND sa fin, ET LIT CE QU'ELLE ECRIT. Rend faux
				/// si le lancement lui-meme echoue ; `code` recoit le code de sortie,
				/// `sortie` la QUEUE de ce que le processus a imprime (stdout ET stderr
				/// reunis : un script Python ecrit ses erreurs sur l'un ou sur l'autre
				/// selon qu'il les leve ou qu'il les imprime, et le pont n'a pas a le
				/// deviner).
				///
				/// ⚠️ ON LIT PENDANT QUE LE PROCESSUS TOURNE, pas apres. Attendre la
				///    fin en laissant le tube se remplir bloquerait l'enfant sur son
				///    ecriture et nous sur son attente : l'interblocage classique du
				///    tube anonyme, et il ne se verrait qu'avec un script BAVARD.
				///
				/// ⚠️ ON GARDE LA QUEUE, PAS LA TETE (4 Kio). Un generateur qui
				///    imprime sa progression noierait son erreur ; et c'est a la fin
				///    qu'un script pose son motif de refus.
				static bool Lancer(const char *ligne, int32 &code, NkString &sortie) {
					sortie.Clear();
					static const uint32 kGarde = 4096u;
					char queue[kGarde + 1u];
					uint32 nq = 0u;
					auto avaler = [&](const char *buf, uint32 n) {
						for (uint32 i = 0; i < n; ++i) {
							if (nq < kGarde) {
								queue[nq++] = buf[i];
								continue;
							}
							// la queue est pleine : on decale d'un cran (fenetre glissante)
							for (uint32 k = 1; k < kGarde; ++k)
								queue[k - 1] = queue[k];
							queue[kGarde - 1] = buf[i];
						}
					};
#ifdef _WIN32
					SECURITY_ATTRIBUTES sa;
					std::memset(&sa, 0, sizeof(sa));
					sa.nLength = sizeof(sa);
					sa.bInheritHandle = TRUE;
					HANDLE rd = nullptr, wr = nullptr;
					if (!CreatePipe(&rd, &wr, &sa, 0))
						return false;
					// LE BOUT LECTEUR N'EST PAS HERITE : s'il l'etait, l'enfant en
					// garderait une copie ouverte et notre lecture n'atteindrait jamais
					// la fin de fichier -- on attendrait pour toujours un tube que
					// personne ne ferme.
					SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
					// UTF-8 -> UTF-16 : les chemins de Rodolf portent des accents.
					const int n = MultiByteToWideChar(CP_UTF8, 0, ligne, -1, nullptr, 0);
					if (n <= 0) {
						CloseHandle(rd);
						CloseHandle(wr);
						return false;
					}
					wchar_t *w = new wchar_t[(size_t)n];
					MultiByteToWideChar(CP_UTF8, 0, ligne, -1, w, n);
					STARTUPINFOW si;
					PROCESS_INFORMATION pi;
					std::memset(&si, 0, sizeof(si));
					si.cb = sizeof(si);
					si.dwFlags = STARTF_USESTDHANDLES;
					si.hStdOutput = wr;
					si.hStdError = wr;
					si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
					std::memset(&pi, 0, sizeof(pi));
					// CREATE_NO_WINDOW : toujours pas de console qui surgit devant la
					// fenetre du modeleur -- mais sa sortie ne se perd plus pour autant.
					// L'heritage des poignees passe a TRUE : c'est ce qui donne au
					// processus le bout ECRIVAIN du tube.
					const BOOL ok = CreateProcessW(nullptr, w, nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
												   nullptr, nullptr, &si, &pi);
					delete[] w;
					CloseHandle(wr); // le parent n'ecrit pas : sans cette fermeture, pas de fin de fichier
					if (!ok) {
						CloseHandle(rd);
						return false;
					}
					char buf[1024];
					DWORD lu = 0;
					while (ReadFile(rd, buf, (DWORD)sizeof(buf), &lu, nullptr) && lu > 0)
						avaler(buf, (uint32)lu);
					CloseHandle(rd);
					WaitForSingleObject(pi.hProcess, INFINITE);
					DWORD ec = (DWORD)-1;
					GetExitCodeProcess(pi.hProcess, &ec);
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
					code = (int32)ec;
					queue[nq] = 0;
					sortie = NkString(queue);
					return true;
#else
					// `2>&1` : les deux flux dans le meme tube, comme sous Windows.
					NkString avecErr = NkString::Format("%s 2>&1", ligne);
					FILE *f = popen(avecErr.CStr(), "r");
					if (!f)
						return false;
					char buf[1024];
					while (const char *r = std::fgets(buf, (int)sizeof(buf), f))
						avaler(r, (uint32)std::strlen(r));
					const int st = pclose(f);
					code = (int32)st;
					queue[nq] = 0;
					sortie = NkString(queue);
					return true;
#endif
				}
		};

		/// LE GENERATEUR PAR DEFAUT, configure depuis l'environnement, une fois.
		/// C'est le SEUL endroit qui sait quel generateur tourne ; le modeleur
		/// ne voit que NkIGenerateur.
		inline NkIGenerateur &NkGeniaGenerateurParDefaut() {
			static NkGenerateurProcessus sGen;
			static bool sInit = false;
			if (!sInit) {
				sInit = true;
				if (const char *c = std::getenv("NK_GENIA_CMD")) {
					sGen.gabarit = c;
				} else {
					const char *py = std::getenv("NK_GENIA_PYTHON");
					const char *sc = std::getenv("NK_GENIA_SCRIPT");
					sGen.gabarit = NkString::Format("\"%s\" \"%s\" --image \"{image}\" --out \"{out}\"",
													py && py[0] ? py : "python",
													sc && sc[0] ? sc : "Tools/Genia/genia_triposr.py");
				}
				// LA PORTE TEXTE RESTE VIDE TANT QUE PERSONNE NE LA REMPLIT, et on
				// le DIT au journal. Composer un gabarit par defaut a partir du
				// script d'image donnerait une commande qui passerait une invite la
				// ou un fichier est attendu : elle echouerait, et son motif
				// accuserait le texte de l'utilisateur.
				if (const char *ct = std::getenv("NK_GENIA_CMD_TEXTE")) {
					sGen.gabaritTexte = ct;
				} else if (const char *st = std::getenv("NK_GENIA_SCRIPT_TEXTE")) {
					const char *py = std::getenv("NK_GENIA_PYTHON");
					sGen.gabaritTexte = NkString::Format("\"%s\" \"%s\" --invite \"{invite}\" --out \"{out}\"",
														 py && py[0] ? py : "python", st);
				}
				NkLog::Instance().Infof("[genia] generateur : %s ; gabarit : %s", sGen.Nom(), sGen.gabarit.CStr());
				NkLog::Instance().Infof("[genia] porte texte : %s",
										sGen.gabaritTexte.Empty()
											? "AUCUNE (posez NK_GENIA_CMD_TEXTE ou NK_GENIA_SCRIPT_TEXTE)"
											: sGen.gabaritTexte.CStr());
			}
			return sGen;
		}

	} // namespace nk3d
} // namespace nkentseu
