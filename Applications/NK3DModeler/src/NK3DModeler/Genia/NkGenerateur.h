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
					if (!Lancer(cmd.CStr(), code)) {
						why = NkString::Format("le processus n'a pas pu etre lance : %s", cmd.CStr());
						return false;
					}
					if (!NkFile::Exists(outGltfPath)) {
						why = NkString::Format("le generateur a rendu %d et n'a pas ecrit '%s' (voir sa sortie)",
											   code, outGltfPath);
						return false;
					}
					if (code != 0)
						NkLog::Instance().Infof("[genia] le generateur a rendu %d mais le fichier existe : on l'importe", code);
					return true;
				}

			private:
				/// Lance la ligne et ATTEND sa fin. Rend faux si le lancement lui-meme
				/// echoue ; `code` recoit le code de sortie du processus.
				static bool Lancer(const char *ligne, int32 &code) {
#ifdef _WIN32
					// UTF-8 -> UTF-16 : les chemins de Rodolf portent des accents.
					const int n = MultiByteToWideChar(CP_UTF8, 0, ligne, -1, nullptr, 0);
					if (n <= 0)
						return false;
					wchar_t *w = new wchar_t[(size_t)n];
					MultiByteToWideChar(CP_UTF8, 0, ligne, -1, w, n);
					STARTUPINFOW si;
					PROCESS_INFORMATION pi;
					std::memset(&si, 0, sizeof(si));
					si.cb = sizeof(si);
					std::memset(&pi, 0, sizeof(pi));
					// CREATE_NO_WINDOW : pas de console qui surgit devant la fenetre du
					// modeleur. La sortie du script va au journal Python (stdout/stderr
					// herites : invisibles ici, lisibles quand on lance depuis un terminal).
					const BOOL ok = CreateProcessW(nullptr, w, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
					delete[] w;
					if (!ok)
						return false;
					WaitForSingleObject(pi.hProcess, INFINITE);
					DWORD ec = (DWORD)-1;
					GetExitCodeProcess(pi.hProcess, &ec);
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
					code = (int32)ec;
					return true;
#else
					const int r = std::system(ligne);
					if (r == -1)
						return false;
					code = (int32)r;
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
				NkLog::Instance().Infof("[genia] generateur : %s ; gabarit : %s", sGen.Nom(), sGen.gabarit.CStr());
			}
			return sGen;
		}

	} // namespace nk3d
} // namespace nkentseu
