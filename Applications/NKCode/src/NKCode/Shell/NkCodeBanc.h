#pragma once
// =============================================================================
// NkCodeBanc.h — `NKCode --selftest` : le banc de NKCode, SANS fenetre (02/10).
//
// Ce qu'il eprouve : NKCode ouvre ce qu'on lui donne, comme VS Code -- pas
// seulement des workspaces Jenga (remarque de Rihen, 02/10 : « on doit pouvoir
// ouvrir des fichiers ou des dossiers qui ne sont pas forcement des .jenga ni des
// dossiers qui en contiennent »). Les memes fonctions que l'application : la
// resolution d'un chemin (NkResoudreArgument), le chargement (NkCodeDialogs::
// DoLoad, l'ecran de chargement), le depot sur le lanceur (NkHomeConsommerDepot),
// la veille des .jenga (TickWatch), « Creer un workspace Jenga ici », et la
// construction (DoBuildAction / PollBuild).
//
// PRE-ENREGISTREMENT (chaque temoin a sa mutation, NK_NKCODE_MUTATION=...) :
//   (o1) un dossier SANS aucun .jenga s'ouvre en edition simple, AUCUNE erreur ;
//        Construire / Executer n'agissent pas et DISENT pourquoi    [exige-jenga]
//   (o2) un fichier .txt hors de tout workspace : son dossier en edition simple,
//        le fichier dans l'onglet actif                              [fichier]
//   (o3) un fichier lache sur le LANCEUR (aucun dossier ouvert) : meme chose ;
//        idem par le double-clic de la vue « Ouvrir »                [lanceur]
//   (o4) un .jenga qui n'est PAS un workspace : ouvert comme un fichier (onglet),
//        dossier en edition simple ; la veille ne crie pas « Workspace detecte »
//        toutes les 1,5 s                                  [jenga-fichier], [veille]
//   (o5) « Creer un workspace Jenga ici » : <Dossier>.jenga ecrit, JAMAIS
//        ecrase, LU par Jenga (jenga info), et NKCode le CONSTRUIT : la barre
//        d'etat finit sur « Termine (OK) » (INDETERMINE sans jenga)
//   Le suivi de la construction A CHAQUE IMAGE (barre d'outils, pas seulement le
//   panneau OUTPUT) ne se voit qu'en vrai : captures hors ecran, avant / apres.
// =============================================================================
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Project/NkProcess.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace nkentseu {
	namespace nkcode {

		namespace banc_detail {
			inline int32 &Reussis() {
				static int32 n = 0;
				return n;
			}
			inline int32 &Echecs() {
				static int32 n = 0;
				return n;
			}
			inline void Temoin(bool ok, const char *quoi) {
				std::printf("  [%s] %s\n", ok ? " OK " : "ECHEC", quoi);
				(ok ? Reussis() : Echecs())++;
			}
			inline void Indetermine(const char *quoi, const char *pourquoi) {
				std::printf("  [ ?? ] %s : INDETERMINE (%s)\n", quoi, pourquoi);
			}
			inline void Poser(const char *nom, const NkString &v) {
#if defined(_WIN32)
				::SetEnvironmentVariableA(nom, v.CStr());
				_putenv_s(nom, v.CStr());
#else
				setenv(nom, v.CStr(), 1);
#endif
			}
			inline void Dormir(uint32 ms) {
#if defined(_WIN32)
				::Sleep(ms);
#else
				usleep(ms * 1000u);
#endif
			}
			inline NkString Propre(const NkString &s) {
				NkString r = s;
				char *p = const_cast<char *>(r.CStr());
				for (usize i = 0; i < r.Length(); ++i)
					if (p[i] == '\\')
						p[i] = '/';
				while (r.Length() > 3u && r.CStr()[r.Length() - 1u] == '/')
					r = NkString(r.CStr(), r.Length() - 1u);
				return r;
			}
			inline bool MemeChemin(const NkString &a, const NkString &b) {
				const NkString x = Propre(a), y = Propre(b);
				if (x.Length() != y.Length())
					return false;
				for (usize i = 0; i < x.Length(); ++i) {
					char c = x.CStr()[i], d = y.CStr()[i];
					c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
					d = (d >= 'A' && d <= 'Z') ? (char)(d - 'A' + 'a') : d;
					if (c != d)
						return false;
				}
				return true;
			}
			/// Le fichier de l'onglet actif est-il `chemin` ?
			inline bool OngletActif(NkCodeState &st, const NkString &chemin) {
				return st.HasActive() && MemeChemin(st.files[st.active].path.ToString(), chemin);
			}
			/// L'ecran de chargement jusqu'au bout (sans fenetre) : rend false s'il
			/// finit en ERREUR ou ne finit pas.
			inline bool Charger(NkCodeDialogs &d, NkCodeState &st) {
				for (int32 k = 0; k < 4000 && d.loading.active && !d.loading.finished && !d.loading.error; ++k) {
					d.loading.Tick(&st, 0.05f);
					if (st.HasWorkspace())
						Dormir(5); // `jenga info` tourne vraiment
				}
				const bool ok = d.loading.finished && !d.loading.error;
				d.loading.active = false;
				d.loading.finished = false;
				d.showStart = false;
				return ok;
			}
		} // namespace banc_detail

		/// `NKCode --selftest`. 0 = tout tient.
		inline int NkCodeLancerBanc() {
			using namespace banc_detail;
			std::printf("\nNKCode -- banc : ouvrir ce qu'on lui donne, comme VS Code\n\n");
			if (const char *mu = std::getenv("NK_NKCODE_MUTATION"))
				std::printf("    MUTATION DE BANC NK_NKCODE_MUTATION=%s : un temoin doit ROUGIR\n", mu);
			// Un dossier A CE PROCESSUS ; et le dossier personnel de NKCode (recents,
			// reglages) y est redirige : le banc n'ecrit RIEN chez l'utilisateur.
			const char *tmp = std::getenv("TEMP");
#if defined(_WIN32)
			const unsigned pid = (unsigned)_getpid();
#else
			const unsigned pid = (unsigned)getpid();
#endif
			const NkString racine = Propre(NkString(tmp ? tmp : "/tmp")) + NkPrintf("/nkcode_banc_%u", pid);
			NkDirectory::Delete(racine.CStr(), true);
			NkDirectory::CreateRecursive((racine + "/maison/AppData/Roaming").CStr());
			Poser("USERPROFILE", racine + "/maison");
			Poser("HOME", racine + "/maison");
			Poser("APPDATA", racine + "/maison/AppData/Roaming");
			// Les dossiers d'essai.
			const NkString simple = racine + "/DossierSimple";
			NkDirectory::CreateRecursive((simple + "/src").CStr());
			NkFile::WriteAllText((simple + "/notes.txt").CStr(), "Un dossier sans workspace Jenga.\n");
			NkFile::WriteAllText((simple + "/src/main.cpp").CStr(),
								 "#include <cstdio>\nint main() { std::printf(\"Bonjour\\n\"); return 0; }\n");
			const NkString hors = racine + "/hors";
			NkDirectory::CreateRecursive(hors.CStr());
			const NkString txt = hors + "/lisezmoi.txt";
			NkFile::WriteAllText(txt.CStr(), "Un fichier seul, hors de tout workspace.\n");
			const NkString lache = racine + "/lache";
			NkDirectory::CreateRecursive(lache.CStr());
			const NkString txt2 = lache + "/lache.txt";
			NkFile::WriteAllText(txt2.CStr(), "Lache sur le lanceur.\n");
			const NkString projetSeul = racine + "/ProjetSeul";
			NkDirectory::CreateRecursive(projetSeul.CStr());
			const NkString outil = projetSeul + "/Outil.jenga";
			NkFile::WriteAllText(outil.CStr(), "# Un projet seul : PAS un workspace.\nfrom Jenga import *\n"
											   "with project(\"Outil\"):\n    consoleapp()\n    files([\"src/**.cpp\"])\n");

			NkCodeState *pst = new NkCodeState();
			NkCodeState &st = *pst;
			NkCodeDialogs *pd = new NkCodeDialogs();
			NkCodeDialogs &d = *pd;
			d.st = &st;
			NkHomeState *ph = new NkHomeState();
			ph->st = &st;
			ph->dlg = &d;

			// ── (o1) un dossier sans aucun .jenga ──
			{
				const NkArgOuverture o = d.OuvrirChemin(simple.CStr());
				const bool pasErreur = !d.loading.error;
				const bool charge = Charger(d, st);
				const bool simpleMode = !st.HasWorkspace() && MemeChemin(st.root.ToString(), simple) && o.jenga.Empty();
				st.DoBuildAction("build");
				const bool buildDit = !st.IsBuilding() && st.status == NkString(NkCodeState::MessageSansWorkspace());
				st.status = NkString();
				st.DoRun();
				const bool runDit = st.status == NkString(NkCodeState::MessageSansWorkspace());
				std::printf("    (o1) erreur %d, charge %d, edition simple %d, Construire dit %d, Executer dit %d\n",
							pasErreur ? 0 : 1, charge ? 1 : 0, simpleMode ? 1 : 0, buildDit ? 1 : 0, runDit ? 1 : 0);
				Temoin(pasErreur && charge && simpleMode,
					   "(o1) un dossier SANS .jenga s'ouvre en edition simple, sans erreur");
				Temoin(buildDit && runDit, "(o1) Construire / Executer n'agissent pas et disent pourquoi");
			}

			// ── (o2) un fichier .txt hors de tout workspace ──
			{
				const NkArgOuverture o = d.OuvrirChemin(txt.CStr());
				const bool charge = Charger(d, st);
				const bool ok = charge && !st.HasWorkspace() && MemeChemin(st.root.ToString(), hors) && OngletActif(st, txt);
				std::printf("    (o2) dossier « %s », fichier « %s », onglet actif %d\n", o.dossier.CStr(), o.fichier.CStr(),
							OngletActif(st, txt) ? 1 : 0);
				Temoin(ok, "(o2) un fichier .txt hors workspace : son dossier, et lui dans l'onglet actif");
			}

			// ── (o3) un fichier lache sur le LANCEUR, aucun dossier ouvert ──
			{
				d.showStart = true; // le lanceur, comme au demarrage
				st.osDropPaths.Clear();
				st.osDropPaths.PushBack(txt2);
				const bool pris = NkHomeConsommerDepot(ph);
				const bool charge = pris && Charger(d, st);
				const bool ok = charge && MemeChemin(st.root.ToString(), lache) && OngletActif(st, txt2) && st.osDropPaths.Empty();
				std::printf("    (o3) depot pris %d, dossier « %s », onglet actif %d\n", pris ? 1 : 0, st.root.ToString().CStr(),
							OngletActif(st, txt2) ? 1 : 0);
				Temoin(ok, "(o3) un fichier lache sur le lanceur s'ouvre (son dossier, lui en onglet)");
			}

			// ── (o4) un .jenga qui n'est PAS un workspace ──
			{
				const NkArgOuverture o = d.OuvrirChemin(outil.CStr());
				const bool charge = Charger(d, st);
				const bool fichier = charge && !st.HasWorkspace() && MemeChemin(st.root.ToString(), projetSeul) && OngletActif(st, outil);
				std::printf("    (o4) dossier « %s », fichier « %s », onglet actif %d\n", o.dossier.CStr(), o.fichier.CStr(),
							OngletActif(st, outil) ? 1 : 0);
				Temoin(fichier, "(o4) un .jenga sans workspace s'ouvre comme un FICHIER, dossier en edition simple");
				st.status = NkString();
				for (int32 k = 0; k < 8; ++k) // 4 s de veille (une mesure toutes les 1,5 s)
					st.TickWatch(0.5f);
				const bool calme = !st.HasWorkspace() && !st.status.Contains("detecte");
				std::printf("    (o4) apres 4 s de veille : workspace %d, barre d'etat « %s »\n", st.HasWorkspace() ? 1 : 0,
							st.status.CStr());
				Temoin(calme, "(o4) la veille ne prend pas ce .jenga pour un workspace (pas de « detecte »)");
			}

			// ── (o5) « Creer un workspace Jenga ici », lu par Jenga, construit par NKCode ──
			{
				d.OuvrirChemin(simple.CStr());
				Charger(d, st);
				const NkString ecrit = st.CreerWorkspaceIci();
				const NkString attendu = simple + "/DossierSimple.jenga";
				const NkString texte = NkFile::ReadAllText(attendu.CStr());
				const bool cree = MemeChemin(ecrit, attendu) && NkCodeState::EstWorkspaceJenga(attendu.CStr()) && st.HasWorkspace() &&
								  OngletActif(st, attendu);
				NkFile::WriteAllText(attendu.CStr(), (texte + "# ligne de l'utilisateur\n").CStr());
				const NkString encore = st.CreerWorkspaceIci();
				const bool garde = encore.Empty() && NkFile::ReadAllText(attendu.CStr()).Contains("# ligne de l'utilisateur");
				std::printf("    (o5) ecrit « %s » ; second appel refuse %d\n", ecrit.CStr(), garde ? 1 : 0);
				Temoin(cree, "(o5) « Creer un workspace Jenga ici » ecrit DossierSimple.jenga, NKCode passe en workspace");
				Temoin(garde, "(o5) un second « Creer » ne remplace rien");
				// Jenga le lit-il ?
				NkProcess info;
				NkVector<NkString> lignes;
				info.Start(NkString("jenga info --jenga-file \"") + attendu + "\"");
				for (int32 k = 0; k < 1200 && !info.Done(); ++k)
					Dormir(50);
				info.Drain(lignes);
				bool jengaLa = false, projet = false;
				for (usize i = 0; i < lignes.Size(); ++i) {
					jengaLa = jengaLa || lignes[i].Contains("Jenga") || lignes[i].Contains("Location");
					projet = projet || (lignes[i].Contains("DossierSimple") && lignes[i].Contains("ConsoleApp"));
				}
				if (!jengaLa || info.ExitCode() == 9009) {
					Indetermine("(o5) Jenga lit le workspace cree, NKCode le construit", "jenga introuvable dans le PATH");
				} else {
					Temoin(info.ExitCode() == 0 && projet, "(o5) Jenga LIT le workspace cree (jenga info : DossierSimple, ConsoleApp)");
					// NKCode le construit (son propre chemin : DoBuildAction -> file -> PollBuild).
					// D'abord ses projets, comme la barre d'outils (`jenga info` asynchrone).
					for (int32 k = 0; k < 1200 && !st.InfoParsed(); ++k) {
						st.LoadProjects();
						st.PollProjects();
						Dormir(50);
					}
					st.status = NkString();
					st.DoBuildAction("build");
					const bool lance = st.IsBuilding() || st.status == NkString("Construction...");
					for (int32 k = 0; k < 6000 && (st.IsBuilding() || st.status == NkString("Construction...")); ++k) {
						st.PollBuild();
						Dormir(20);
					}
					st.PollBuild();
					std::printf("    (o5) construction lancee %d ; barre d'etat a la fin : « %s »\n", lance ? 1 : 0, st.status.CStr());
					if (!(st.status == NkString("Termine (OK)")))
						for (usize i = 0; i < st.output.Size() && i < 40u; ++i)
							std::printf("      %s\n", st.output[i].CStr());
#if defined(_WIN32)
					const NkString exe = simple + "/Build/Bin/Debug-Windows/DossierSimple/DossierSimple.exe";
					const bool produit = NkFile::Exists(exe.CStr());
					std::printf("    (o5) %s : %s\n", exe.CStr(), produit ? "produit" : "ABSENT");
#else
					const bool produit = true;
#endif
					Temoin(lance && produit && st.status == NkString("Termine (OK)"),
						   "(o5) NKCode CONSTRUIT le workspace cree : l'exe, et « Termine (OK) » en barre d'etat");
				}
			}

			delete ph;
			delete pd;
			delete pst;
			NkDirectory::Delete(racine.CStr(), true);
			std::printf("\n%s : %d reussis, %d echec\n", Echecs() == 0 ? "BANC NKCODE REUSSI" : "BANC NKCODE EN ECHEC", Reussis(),
						Echecs());
			return Echecs() == 0 ? 0 : 1;
		}

	} // namespace nkcode
} // namespace nkentseu
