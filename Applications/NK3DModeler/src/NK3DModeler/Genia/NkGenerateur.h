#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
#include "NKFileSystem/NkPath.h"
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
		/// attend, on verifie le fichier. Le gabarit vient de NK_GENIA_CMD, ou se
		/// compose de NK_GENIA_PYTHON et NK_GENIA_SCRIPT, ou -- a defaut -- se
		/// DEDUIT DE L'EMPLACEMENT DE L'EXECUTABLE (cf. NkGeniaArbreDepuisExe).
		/// ⚠️ Plus jamais `python` nu ni de chemin relatif au dossier courant :
		///    mesure du 20/09, les deux moities etaient fausses et le bouton ne
		///    pouvait pas aboutir. Et si la deduction echoue, le gabarit reste
		///    VIDE avec son motif -- on ne compose pas une commande qu'on sait fausse.
		class NkGenerateurProcessus : public NkIGenerateur {
			public:
				NkString gabarit; // ex. "\"C:/.../python.exe\" \"Tools/Genia/genia_triposr.py\" --image \"{image}\" --out \"{out}\""
				/// LE GABARIT DE LA PORTE TEXTE, avec deux trous `{invite}` et `{out}`.
				/// VIDE PAR DEFAUT, et c'est voulu : tant que personne ne l'a rempli,
				/// `GenererDepuisTexte` refuse EN LE DISANT plutot que de lancer la
				/// commande de l'image avec une invite a la place d'un fichier.
				NkString gabaritTexte;
				NkString nom = "TripoSR (processus externe)";
				/// ── POURQUOI UN MOTIF D'ABSENCE, ET PAS UN GABARIT DE SECOURS ───
				/// Quand le gabarit ne peut pas se composer, ce champ dit CE QU'ON A
				/// CHERCHE ET OU. Sans lui, le refus se resumait a « NK_GENIA_CMD
				/// vide », qui nomme une variable que l'utilisateur n'a jamais eu a
				/// poser -- donc un refus qui envoie chercher au mauvais endroit.
				/// ⚠️ ET SURTOUT : pas de repli sur un `python` nu. Un interpreteur
				///    de secours qui n'a pas torch echouerait PLUS LOIN et PLUS
				///    OBSCUREMENT ; le refus nomme est le seul bon cote de l'etat
				///    d'avant, et il se garde.
				NkString motifGabaritVide;

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
						why = motifGabaritVide.Empty()
								  ? NkString("aucune commande de generation (NK_GENIA_CMD vide)")
								  : motifGabaritVide;
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

		/// ── L'ARBRE, DEDUIT DE L'EXECUTABLE, PAR UNE ANCRE NOMMEE ───────────────
		///
		/// 🔴 MESURE DU 20/09/2026 : le gabarit par defaut designait `python` nu et
		///    `Tools/Genia/genia_triposr.py` RELATIF au dossier courant. Les deux
		///    moities etaient fausses, et chacune suffisait a tout arreter :
		///      depuis le dossier de l'exe -> can't open file '...\NK3DModeler\Tools\...'
		///      depuis la racine de l'arbre -> REFUS : No module named 'torch'
		///    (le `python` du PATH de cette machine est PyManager/pythoncore-3.14).
		///    Et AUCUN lanceur du depot ne posait NK_GENIA_CMD : le bouton
		///    « Generer » etait atteignable et ne pouvait pas aboutir.
		///
		/// ⚠️ CORRIGER UNE SEULE MOITIE LAISSE L'AUTRE VERTE LA OU L'ON REGARDE :
		///    rendre le script absolu fait passer le lancement depuis le dossier de
		///    l'exe... jusqu'a l'import de torch. Les deux se corrigent ensemble ou
		///    le defaut se deplace.
		///
		/// L'ancre est STRUCTURELLE, pas un compte de `..` : l'executable vit
		/// toujours sous `<arbre>/Build/...`. On remonte jusqu'au premier ancetre
		/// nomme `Build` ; son parent est l'arbre, et le parent de l'arbre porte
		/// `genia-tools/` -- exactement ce que `genia_triposr.py` calcule de son
		/// cote (`OUTILS = dirname(RACINE)/genia-tools`). Rend faux si l'ancre
		/// n'existe pas, et alors on ne devine rien.
		inline bool NkGeniaArbreDepuisExe(NkString &arbre, NkString &parent) {
			char exe[1024];
			{
				const NkString e = NkPath::GetExecutableDirectory().ToString();
				snprintf(exe, sizeof(exe), "%s", e.CStr());
				for (char *p = exe; *p; ++p)
					if (*p == '\\')
						*p = '/';
			}
			size_t fin = std::strlen(exe);
			while (fin > 0) {
				while (fin > 0 && exe[fin - 1] == '/')
					--fin;
				size_t deb = fin;
				while (deb > 0 && exe[deb - 1] != '/')
					--deb;
				if (fin - deb == 5 && std::strncmp(exe + deb, "Build", 5) == 0) {
					if (deb == 0)
						return false;
					char b[1024];
					snprintf(b, sizeof(b), "%.*s", (int)(deb - 1), exe); // <arbre>
					arbre = NkString(b);
					size_t a = deb - 1;
					while (a > 0 && exe[a - 1] != '/')
						--a;
					if (a == 0)
						return false;
					snprintf(b, sizeof(b), "%.*s", (int)(a - 1), exe); // parent de l'arbre
					parent = NkString(b);
					return true;
				}
				fin = deb;
			}
			return false;
		}

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
					// Ce que l'environnement impose gagne ; ce qu'il ne dit pas se
					// DEDUIT de l'executable, jamais du dossier courant.
					NkString pyChoisi = (py && py[0]) ? NkString(py) : NkString();
					NkString scChoisi = (sc && sc[0]) ? NkString(sc) : NkString();
					NkString arbre, parent, cherche;
					if ((pyChoisi.Empty() || scChoisi.Empty()) &&
						NkGeniaArbreDepuisExe(arbre, parent)) {
						if (scChoisi.Empty()) {
							NkString s = arbre;
							s.Append("/Tools/Genia/genia_triposr.py");
							if (NkFile::Exists(s.CStr()))
								scChoisi = s;
							else
								cherche = NkString::Format("script introuvable : '%s'", s.CStr());
						}
						if (pyChoisi.Empty()) {
							// Le meme venv sous ses deux dispositions d'OS. Ce n'est pas
							// un repli : c'est le MEME artefact a deux endroits selon le
							// systeme. Un `python` du PATH, lui, serait un AUTRE
							// interpreteur -- et c'est ce qui a echoue.
							const char *sous[2] = {"/genia-tools/venv/Scripts/python.exe",
												   "/genia-tools/venv/bin/python"};
							for (int i = 0; i < 2 && pyChoisi.Empty(); ++i) {
								NkString p = parent;
								p.Append(sous[i]);
								if (NkFile::Exists(p.CStr()))
									pyChoisi = p;
							}
							if (pyChoisi.Empty())
								cherche = NkString::Format(
									"interpreteur introuvable : '%s%s' (ni .../bin/python)",
									parent.CStr(), sous[0]);
						}
					}
					if (!pyChoisi.Empty() && !scChoisi.Empty()) {
						sGen.gabarit =
							NkString::Format("\"%s\" \"%s\" --image \"{image}\" --out \"{out}\"",
											 pyChoisi.CStr(), scChoisi.CStr());
						// (21/09, Q6) LA PORTE TEXTE SE DEDUIT DE LA MEME FACON, et
						// seulement si SON script existe : genia_texte_3d.py, a cote de
						// genia_triposr.py (texte -> image locale -> detourage ->
						// TripoSR). Ce n'est pas le script d'image recycle -- la faute
						// que le commentaire plus bas interdit.
						if (!std::getenv("NK_GENIA_CMD_TEXTE") && !std::getenv("NK_GENIA_SCRIPT_TEXTE")) {
							NkString st2 = scChoisi;
							const NkString::SizeType barre = st2.FindLastOf("/\\");
							if (barre != NkString::npos) {
								st2 = st2.SubStr(0, barre + 1);
								st2.Append("genia_texte_3d.py");
								if (NkFile::Exists(st2.CStr()))
									sGen.gabaritTexte = NkString::Format(
										"\"%s\" \"%s\" --invite \"{invite}\" --out \"{out}\"", pyChoisi.CStr(),
										st2.CStr());
							}
						}
					} else {
						// ⚠️ ON NE COMPOSE PAS UNE COMMANDE QU'ON SAIT FAUSSE. Mettre
						//    `python` ici rendrait un echec PLUS LOIN (import torch) dont
						//    le motif n'aiderait personne. Le refus nomme ce qu'on a
						//    cherche et ou, et la variable qui le contourne.
						sGen.gabarit.Clear();
						sGen.motifGabaritVide = NkString::Format(
							"aucun generateur utilisable -- %s. Posez NK_GENIA_CMD, ou "
							"NK_GENIA_PYTHON et NK_GENIA_SCRIPT.",
							cherche.Empty() ? "l'arbre n'a pas pu etre deduit de l'executable"
											: cherche.CStr());
					}
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
				// Le journal dit le gabarit RETENU, ou le motif de son absence : un
				// « gabarit : (vide) » enverrait chercher dans le code.
				NkLog::Instance().Infof("[genia] generateur : %s ; gabarit : %s", sGen.Nom(),
										sGen.gabarit.Empty() ? sGen.motifGabaritVide.CStr()
															 : sGen.gabarit.CStr());
				NkLog::Instance().Infof("[genia] porte texte : %s",
										sGen.gabaritTexte.Empty()
											? "AUCUNE (posez NK_GENIA_CMD_TEXTE ou NK_GENIA_SCRIPT_TEXTE)"
											: sGen.gabaritTexte.CStr());
			}
			return sGen;
		}

	} // namespace nk3d
} // namespace nkentseu
