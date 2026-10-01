// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurScripts.cpp
// DESCRIPTION: Les scripts dans l'editeur : hote branche sur la scene,
//              compilation et rechargement a chaud du C++, Blueprints suivis.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurScripts.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "Livraison/NkEditeurConstruire.h"
#include "Script/NkBpCatalogue.h"

#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace nkentseu {
	namespace editeur {

		namespace {
			/// « \ » -> « / ».
			NkString Oblique(const NkString &s) {
				NkString r = s;
				char *p = const_cast<char *>(r.CStr());
				for (usize i = 0; i < r.Length(); ++i) {
					if (p[i] == '\\') {
						p[i] = '/';
					}
				}
				return r;
			}
			bool CommencePar(const NkString &texte, const NkString &prefixe) {
				if (prefixe.Length() > texte.Length()) {
					return false;
				}
				for (usize i = 0; i < prefixe.Length(); ++i) {
					char a = texte.CStr()[i], b = prefixe.CStr()[i];
#if defined(_WIN32)
					a = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
					b = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
#endif
					if (a != b) {
						return false;
					}
				}
				return true;
			}
			void Sortie(void *donnees, const char *ligne, bool faute) {
				NkEditeurScripts &s = *static_cast<NkEditeurScripts *>(donnees);
				s.journal.PushBack(NkString(ligne));
				if (faute) {
					s.montrerJournal = true;
				}
			}
			NkString NomDll() {
#if defined(_WIN32)
				return NkString("Scripts.dll");
#elif defined(__APPLE__)
				return NkString("libScripts.dylib");
#else
				return NkString("libScripts.so");
#endif
			}
			NkString Intermediaire(const NkEditeurScripts &s) {
				return s.projet + NK_SCRIPTS_INTERMEDIAIRE + "/";
			}
			/// Les fichiers `motif` du Contenu (recursif), et leur empreinte.
			void Lister(NkEditeurModele &m, const char *motif, NkVector<NkString> &chemins, NkVector<int64> &empreintes) {
				chemins.Clear();
				empreintes.Clear();
				const NkString contenu = NkEditeurDossierContenu(m);
				if (!NkDirectory::Exists(contenu.CStr())) {
					return;
				}
				const NkVector<NkDirectoryEntry> e =
					NkDirectory::GetEntries(contenu.CStr(), motif, NkSearchOption::NK_ALL_DIRECTORIES);
				for (uint32 i = 0; i < e.Size(); ++i) {
					if (!e[i].IsFile) {
						continue;
					}
					chemins.PushBack(Oblique(e[i].FullPath.ToString()));
					empreintes.PushBack(static_cast<int64>(e[i].ModificationTime) * 1000003 + static_cast<int64>(e[i].Size));
				}
			}
			bool Egaux(const NkVector<NkString> &a, const NkVector<int64> &ea, const NkVector<NkString> &b,
					   const NkVector<int64> &eb) {
				if (a.Size() != b.Size()) {
					return false;
				}
				for (uint32 i = 0; i < a.Size(); ++i) {
					if (!(a[i] == b[i]) || ea[i] != eb[i]) {
						return false;
					}
				}
				return true;
			}
			/// « D:/P/Contenu/Scripts/Porte.cpp:12:5: error: x » -> l'erreur, relative.
			bool LireErreur(const NkString &projet, const NkString &ligne, NkErreurScriptCpp &e, bool &avertissement) {
				const char *t = ligne.CStr();
				const char *err = std::strstr(t, ": error: ");
				const char *av = std::strstr(t, ": warning: ");
				const char *marque = err != nullptr ? err : av;
				if (marque == nullptr) {
					return false;
				}
				avertissement = err == nullptr;
				// fichier:ligne:colonne avant la marque. Le lecteur « C: » d'un
				// chemin Windows porte un deux-points : on repart de la fin.
				const char *fin = marque;
				const char *p2 = fin;
				while (p2 > t && p2[-1] != ':') {
					--p2;
				}
				const char *p1 = p2 > t ? p2 - 1 : t;
				while (p1 > t && p1[-1] != ':') {
					--p1;
				}
				if (p1 <= t || p2 <= t) {
					return false;
				}
				e.colonne = std::atoi(p2);
				e.ligne = std::atoi(p1);
				NkString fichier = Oblique(NkString(t, static_cast<usize>(p1 - 1 - t)));
				const NkString base = Oblique(projet);
				if (CommencePar(fichier, base)) {
					fichier = NkString(fichier.CStr() + base.Length());
				}
				e.fichier = fichier;
				e.message = NkString(marque + (avertissement ? 11 : 9));
				return true;
			}
		} // namespace

		// =====================================================================
		void NkEditeurScriptsDemarrer(NkEditeurScripts &s, NkEditeurModele &m, const unkeny::NkActions *actions,
									  const unkeny::NkLiaisons *liaisons) {
			s.hote.Brancher(m.scene, s.registre);
			s.hote.PoserActions(actions, liaisons);
			s.hote.PoserSortie(&Sortie, &s);
			m.scripts = &s;
			s.demarre = true;
			s.ageReleve = 99.f;
		}

		void NkEditeurScriptsArreter(NkEditeurScripts &s) {
			s.hote.Arreter();
			if (s.compilateur.EnCours()) {
				s.compilateur.Arreter();
			}
			s.modules.Decharger();
		}

		NkString NkEditeurScriptsCompilateur() {
			// Le PATH d'abord (la chaine que jenga prend), puis msys2 (ucrt64, clang64).
#if defined(_WIN32)
			const char *path = std::getenv("PATH");
			if (path != nullptr) {
				const char *a = path;
				while (*a != '\0') {
					const char *b = std::strchr(a, ';');
					const NkString dossier = b != nullptr ? NkString(a, static_cast<usize>(b - a)) : NkString(a);
					if (!dossier.Empty()) {
						const NkString c = Oblique(dossier) + "/clang++.exe";
						if (NkFile::Exists(c.CStr())) {
							return c;
						}
					}
					if (b == nullptr) {
						break;
					}
					a = b + 1;
				}
			}
			const char *connus[] = {"C:/msys64/ucrt64/bin/clang++.exe", "C:/msys64/clang64/bin/clang++.exe"};
			for (const char *c : connus) {
				if (NkFile::Exists(c)) {
					return NkString(c);
				}
			}
			return NkString();
#else
			const char *connus[] = {"/usr/bin/clang++", "/usr/local/bin/clang++", "/opt/homebrew/opt/llvm/bin/clang++"};
			for (const char *c : connus) {
				if (NkFile::Exists(c)) {
					return NkString(c);
				}
			}
			return NkString();
#endif
		}

		void NkEditeurScriptsClassesDe(const char *texte, NkVector<NkString> &sortie) {
			if (texte == nullptr) {
				return;
			}
			const char *motifs[] = {"NK_UNKENY_CLASSE_VARIABLES(", "NK_UNKENY_CLASSE("};
			const char *p = texte;
			while (true) {
				const char *trouve = nullptr;
				usize longueur = 0;
				for (const char *m : motifs) {
					const char *q = std::strstr(p, m);
					if (q != nullptr && (trouve == nullptr || q < trouve)) {
						trouve = q;
						longueur = std::strlen(m);
					}
				}
				if (trouve == nullptr) {
					return;
				}
				// La ligne n'est ni un commentaire ni la definition de la macro.
				const char *debut = trouve;
				while (debut > texte && debut[-1] != '\n') {
					--debut;
				}
				bool ignore = false;
				for (const char *c = debut; c < trouve; ++c) {
					if ((c[0] == '/' && c[1] == '/') || c[0] == '#' || c[0] == '*') {
						ignore = true;
						break;
					}
				}
				const char *n = trouve + longueur;
				while (*n == ' ' || *n == '\t') {
					++n;
				}
				const char *f = n;
				while ((*f >= 'a' && *f <= 'z') || (*f >= 'A' && *f <= 'Z') || (*f >= '0' && *f <= '9') || *f == '_') {
					++f;
				}
				if (!ignore && f > n) {
					const NkString nom(n, static_cast<usize>(f - n));
					bool deja = false;
					for (uint32 i = 0; i < sortie.Size(); ++i) {
						deja = deja || sortie[i] == nom;
					}
					if (!deja) {
						sortie.PushBack(nom);
					}
				}
				p = trouve + longueur;
			}
		}

		NkString NkEditeurScriptsRegistre(const NkVector<NkString> &classes) {
			NkString t;
			t += "// GENERE par UnkenyEditor a chaque compilation des scripts : ne pas le modifier.\n";
			t += "// Le registre des classes C++ du projet (document 01, § 5.6) : un point d'entree\n";
			t += "// exporte (DLL rechargee a chaud dans l'editeur), ou nomme (lie en statique\n";
			t += "// dans le jeu construit, NK_UNKENY_SCRIPTS_STATIQUES).\n";
			t += "#include \"Unkeny/Script/NkUnkenyScriptABI.h\"\n\n";
			for (uint32 i = 0; i < classes.Size(); ++i) {
				t += NkString::Format("extern \"C\" const NkUnkClasseV1 *NkUnkClasse_%s(void);\n", classes[i].CStr());
			}
			t += "\n#if defined(NK_UNKENY_SCRIPTS_STATIQUES)\n";
			t += "extern \"C\" const NkUnkModuleV1 *nk_unkeny_module_statique(void) {\n";
			t += "#else\n";
			t += "extern \"C\" NK_UNK_EXPORTE const NkUnkModuleV1 *nk_unkeny_module_v1(void) {\n";
			t += "#endif\n";
			t += NkString::Format("\tstatic const NkUnkClasseV1 *classes[%u];\n", static_cast<unsigned>(classes.Size() + 1u));
			for (uint32 i = 0; i < classes.Size(); ++i) {
				t += NkString::Format("\tclasses[%u] = NkUnkClasse_%s();\n", static_cast<unsigned>(i), classes[i].CStr());
			}
			t += NkString::Format("\tstatic NkUnkModuleV1 module = {sizeof(NkUnkModuleV1), NK_UNK_ABI_MAJEURE, NK_UNK_ABI_MINEURE, %uu, classes};\n",
								  static_cast<unsigned>(classes.Size()));
			t += "\treturn &module;\n}\n";
			return t;
		}

		bool NkEditeurScriptsReleverCpp(NkEditeurScripts &s, NkEditeurModele &m) {
			NkVector<NkString> chemins;
			NkVector<int64> empreintes;
			Lister(m, "*.cpp", chemins, empreintes);
			if (Egaux(chemins, empreintes, s.sources, s.empreintes)) {
				return false;
			}
			s.sources = chemins;
			s.empreintes = empreintes;
			return true;
		}

		bool NkEditeurScriptsCompiler(NkEditeurScripts &s, NkEditeurModele &m) {
			if (s.compilateur.EnCours()) {
				s.recompiler = true;
				return false;
			}
			if (s.sources.Empty()) {
				return false;
			}
			const NkString clang = NkEditeurScriptsCompilateur();
			const NkString depot = NkTrouverDepot();
			const NkString inclure = depot.Empty() ? NkString() : Oblique(depot) + "Engine/Unkeny/src";
			if (clang.Empty() || inclure.Empty() || !NkFile::Exists((inclure + "/Unkeny/Script/NkUnkenyScriptABI.h").CStr())) {
				s.etat = NkEtatCompilation::NK_IMPOSSIBLE;
				s.journal.PushBack(clang.Empty() ? NkString("[C++] clang++ introuvable (PATH, C:/msys64/ucrt64/bin) : les scripts C++ ne peuvent pas être compilés ici")
												 : NkString("[C++] en-tête NkUnkenyScriptABI.h introuvable : le dépôt Nkentseu (Engine/Unkeny/src) est nécessaire pour compiler"));
				s.montrerJournal = true;
				return false;
			}
			// Les classes, lues dans les sources : le registre genere les appelle.
			s.classes.Clear();
			for (uint32 i = 0; i < s.sources.Size(); ++i) {
				const NkString texte = NkFile::ReadAllText(s.sources[i].CStr());
				NkEditeurScriptsClassesDe(texte.CStr(), s.classes);
			}
			const NkString inter = Intermediaire(s);
			NkDirectory::CreateRecursive(inter.CStr());
			const NkString registre = inter + "NkUnkRegistre.cpp";
			NkFile::WriteAllText(registre.CStr(), NkEditeurScriptsRegistre(s.classes).CStr());
			NkString cmd = NkString("\"") + clang + "\" -shared -std=c++17 -O1 -g0 -fno-color-diagnostics";
#if defined(_WIN32)
			// La chaine clang-mingw du depot (config/toolchain.jenga) : tout en
			// statique, la DLL ne depend que du systeme.
			cmd += " --target=x86_64-w64-windows-gnu -static-libstdc++ -static-libgcc -static";
#else
			cmd += " -fPIC";
#endif
			cmd += NkString(" -I\"") + inclure + "\" -o \"" + inter + NomDll() + "\"";
			for (uint32 i = 0; i < s.sources.Size(); ++i) {
				cmd += NkString(" \"") + s.sources[i] + "\"";
			}
			cmd += NkString(" \"") + registre + "\"";
			s.sortie.Clear();
			s.erreurs.Clear();
			if (!s.compilateur.Lancer(cmd, inter)) {
				s.etat = NkEtatCompilation::NK_IMPOSSIBLE;
				s.journal.PushBack(NkString("[C++] le compilateur n'a pas pu être lancé : ") + cmd);
				s.montrerJournal = true;
				return false;
			}
			s.etat = NkEtatCompilation::NK_EN_COURS;
			s.journal.PushBack(NkString::Format("[C++] compilation de %u fichier(s), %u classe(s)…", static_cast<unsigned>(s.sources.Size()),
												static_cast<unsigned>(s.classes.Size())));
			return true;
		}

		void NkEditeurScriptsSuivreCompilation(NkEditeurScripts &s, NkEditeurModele &m, bool attendre) {
			if (s.etat != NkEtatCompilation::NK_EN_COURS) {
				return;
			}
			if (attendre) {
				s.compilateur.Attendre(
					[](const NkString &ligne, void *d) { static_cast<NkEditeurScripts *>(d)->sortie.PushBack(ligne); }, &s);
			} else {
				NkVector<NkString> lignes;
				s.compilateur.Recolter(lignes);
				for (uint32 i = 0; i < lignes.Size(); ++i) {
					s.sortie.PushBack(lignes[i]);
				}
				if (s.compilateur.EnCours()) {
					return;
				}
				s.compilateur.Recolter(lignes);
				for (uint32 i = 0; i < lignes.Size(); ++i) {
					s.sortie.PushBack(lignes[i]);
				}
			}
			++s.compilations;
			const int32 code = s.compilateur.Code();
			uint32 nbErreurs = 0;
			for (uint32 i = 0; i < s.sortie.Size(); ++i) {
				NkErreurScriptCpp e;
				bool avert = false;
				if (LireErreur(s.projet, NkSansAnsi(s.sortie[i].CStr()), e, avert)) {
					if (!avert) {
						s.erreurs.PushBack(e);
						++nbErreurs;
					}
					s.journal.PushBack(NkString::Format("%s:%d:%d: %s: %s", e.fichier.CStr(), e.ligne, e.colonne,
														avert ? "warning" : "error", e.message.CStr()));
				}
			}
			if (code != 0) {
				s.etat = NkEtatCompilation::NK_ECHOUEE;
				if (nbErreurs == 0u) {
					for (uint32 i = 0; i < s.sortie.Size() && i < 12u; ++i) {
						s.journal.PushBack(NkString("[C++] ") + NkSansAnsi(s.sortie[i].CStr()));
					}
				}
				s.journal.PushBack(NkString::Format("[C++] ÉCHEC de la compilation (%u erreur(s)) : l'ancienne version des scripts reste active",
													static_cast<unsigned>(nbErreurs)));
				s.montrerJournal = true;
			} else {
				NkString err;
				const NkString dll = Intermediaire(s) + NomDll();
				if (s.modules.Recharger(dll.CStr(), s.registre, &s.hote, &err)) {
					s.etat = NkEtatCompilation::NK_REUSSIE;
					NkString noms;
					for (uint32 i = 0; i < s.classes.Size(); ++i) {
						noms += i > 0u ? NkString(", ") : NkString();
						noms += s.classes[i];
					}
					s.journal.PushBack(NkString::Format("[C++] scripts recompilés et rechargés à chaud (génération %u) : %s",
														static_cast<unsigned>(s.modules.Generation()), noms.CStr()));
				} else {
					s.etat = NkEtatCompilation::NK_ECHOUEE;
					s.journal.PushBack(NkString("[C++] rechargement refusé : ") + err + " (l'ancienne version reste active)");
					s.montrerJournal = true;
				}
			}
			(void)m;
			if (s.recompiler) {
				s.recompiler = false;
				NkEditeurScriptsCompiler(s, m);
			}
		}

		void NkEditeurScriptsTrame(NkEditeurScripts &s, NkEditeurModele &m, NkEditeurInterface *ui, float32 dt) {
			if (!s.demarre) {
				return;
			}
			// Le PROJET a change (une scene d'un autre dossier) : on repart.
			const NkString projet = Oblique(NkEditeurDossierProjet(m));
			if (!(projet == s.projet)) {
				s.projet = projet;
				s.sources.Clear();
				s.empreintes.Clear();
				s.blueprints.Clear();
				s.empreintesBp.Clear();
				s.ageReleve = 99.f;
				unkeny::NkModulesCpp::NettoyerCopies((Intermediaire(s) + NomDll()).CStr());
			}
			// « Arreter » : les instances partent avec la scene d'avant Jouer.
			if (s.etatPrecedent != NkEtatJeu::NK_EDITION && m.etat == NkEtatJeu::NK_EDITION) {
				s.hote.Arreter();
			}
			s.etatPrecedent = m.etat;
			s.ageReleve += dt;
			if (s.ageReleve >= 0.5f) {
				s.ageReleve = 0.f;
				if (NkEditeurScriptsReleverCpp(s, m)) {
					NkEditeurScriptsCompiler(s, m);
				}
				NkVector<NkString> chemins;
				NkVector<int64> empreintes;
				Lister(m, "*.nkbp", chemins, empreintes);
				bool change = false;
				for (uint32 i = 0; i < chemins.Size(); ++i) {
					const NkString ref = NkEditeurRefScript(m, chemins[i].CStr());
					int32 connu = -1;
					for (uint32 k = 0; k < s.blueprints.Size(); ++k) {
						if (s.blueprints[k] == ref) {
							connu = static_cast<int32>(k);
						}
					}
					if (connu >= 0 && s.empreintesBp[static_cast<uint32>(connu)] == empreintes[i]) {
						continue;
					}
					NkString err;
					s.registre.ChargerBlueprint(ref.CStr(), chemins[i].CStr(), &err);
					if (!err.Empty()) {
						s.journal.PushBack(NkString::Format("[Blueprint] %s : %s", ref.CStr(), err.CStr()));
					} else if (connu >= 0) {
						s.journal.PushBack(NkString::Format("[Blueprint] %s relu", ref.CStr()));
					}
					if (connu >= 0) {
						s.empreintesBp[static_cast<uint32>(connu)] = empreintes[i];
					} else {
						s.blueprints.PushBack(ref);
						s.empreintesBp.PushBack(empreintes[i]);
					}
					change = true;
				}
				if (change) {
					s.hote.MigrerInstances();
				}
			}
			NkEditeurScriptsSuivreCompilation(s, m, false);
			if (ui != nullptr) {
				for (uint32 i = 0; i < s.journal.Size(); ++i) {
					const int32 t = static_cast<int32>(ui->temps);
					ui->journal.PushBack(NkString::Format("[%02d:%02d]  %s", t / 60, t % 60, s.journal[i].CStr()));
					if (ui->journal.Size() > 200u) {
						ui->journal.RemoveAt(0);
					}
				}
				if (s.montrerJournal) {
					ui->ongletTiroir = 1;
					ui->voirTiroir = true;
				}
				s.journal.Clear();
				s.montrerJournal = false;
			}
		}

		// =====================================================================
		NkString NkEditeurRefScript(NkEditeurModele &m, const char *cheminAbsolu) {
			const NkString projet = Oblique(NkEditeurDossierProjet(m));
			const NkString c = Oblique(NkString(cheminAbsolu != nullptr ? cheminAbsolu : ""));
			if (projet.Empty() || projet == "./" || !CommencePar(c, projet)) {
				// Hors d'un projet : le chemin du navigateur (« Contenu/... ») s'il y est.
				const NkString nav = NkEditeurNavigateurDe(m, cheminAbsolu);
				return nav.Empty() ? c : nav;
			}
			return NkString(c.CStr() + projet.Length());
		}

		void NkEditeurScriptsProposes(NkEditeurScripts &s, NkVector<NkString> &sortie) {
			sortie.Clear();
			for (uint32 i = 0; i < s.blueprints.Size(); ++i) {
				sortie.PushBack(s.blueprints[i]);
			}
			for (uint32 i = 0; i < s.registre.Nombre(); ++i) {
				const unkeny::NkDefinitionScript *d = s.registre.DefinitionA(i);
				if (d != nullptr && d->genre == unkeny::NkGenreScript::NK_CPP && d->classe != nullptr) {
					sortie.PushBack(d->nom);
				}
			}
		}

		NkString NkEditeurNouveauScriptCpp(NkEditeurModele &m, const char *dossierNav) {
			const NkString dossier = NkEditeurCheminContenu(m, dossierNav);
			NkDirectory::CreateRecursive(dossier.CStr());
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), "NouveauScript.cpp");
			if (chemin.Empty()) {
				NkEditeurAnnoncer(m, "Nouveau script C++ : écriture impossible");
				return NkString();
			}
			// Le NOM DE LA CLASSE est celui du fichier (un identifiant C++).
			NkString nom = editorkit::NkDisqueNom(chemin.CStr());
			const char *point = std::strrchr(nom.CStr(), '.');
			if (point != nullptr) {
				nom = NkString(nom.CStr(), static_cast<usize>(point - nom.CStr()));
			}
			NkString classe;
			for (usize i = 0; i < nom.Length(); ++i) {
				const char ch = nom.CStr()[i];
				const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
				classe.Append(ok ? ch : '_');
			}
			if (classe.Empty() || (classe.CStr()[0] >= '0' && classe.CStr()[0] <= '9')) {
				classe = NkString("Script_") + classe;
			}
			NkString t;
			t += "// =============================================================================\n";
			t += NkString::Format("// %s.cpp -- un script C++ d'Unkeny\n", classe.CStr());
			t += "//\n";
			t += NkString::Format("// Posez-le sur une entite : Details > Scripts > « + cpp:%s ». A chaque\n", classe.CStr());
			t += "// enregistrement de ce fichier, UnkenyEditor le recompile et le RECHARGE A\n";
			t += "// CHAUD, meme pendant Jouer ; les erreurs arrivent dans le Journal\n";
			t += "// (fichier:ligne). L'etat garde par NK_UNKENY_GARDER survit au rechargement.\n";
			t += "//\n";
			t += "// Ce qu'un script recoit (surchargez ce qui sert) :\n";
			t += "//   Debut()                           une fois, avant tout le reste\n";
			t += "//   Tick(dt)                          chaque image\n";
			t += "//   PasFixe(dt)                       chaque pas de physique (le lieu des forces)\n";
			t += "//   ContactDebut(autre), ContactFin(autre)              chocs entre corps\n";
			t += "//   ZoneEntree(autre, soiEstLaZone), ZoneSortie(...)    collisionneur declencheur\n";
			t += "//   ActionPressee(action, valeur), ActionRelachee(action)   « Sauter »... jamais une touche\n";
			t += "//   Recharge()                        apres un rechargement a chaud\n";
			t += "//\n";
			t += "// Ce qu'il peut faire (les aides de nkunk::Script, toutes par la table de l'hote) :\n";
			t += "//   Soi(), ParNom(\"Porte\"), NomEst(e, \"Joueur\"), Detruire(e), Activer(e, oui)\n";
			t += "//   Position(e), Teleporter(e, p), Rotation(e), PoserRotation(e, r)\n";
			t += "//   Vitesse(e), PoserVitesse(e, v), Impulsion(e, i), Force(e, f)\n";
			t += "//   PoserCouleur(e, 0xRRGGBBAA), PoserVisible(e, oui)\n";
			t += "//   JouerClip(e, i), AnimParametre(e, \"vitesse\", v), AnimDeclencher(e, \"saut\")\n";
			t += "//   JouerEffet(e), ArreterEffet(e)                      les emetteurs (feu, fumee...)\n";
			t += "//   ValeurAction(\"Avancer\"), ActionEnfoncee(\"Sauter\"), JouerSon(\"saut\")\n";
			t += "//   Variable(\"vitesse\", 2.f), PoserVariable(\"n\", 3.f), Afficher(\"texte\"), Temps()\n";
			t += "// =============================================================================\n";
			t += "#include \"Unkeny/Script/NkUnkenyScriptABI.h\"\n\n";
			t += NkString::Format("class %s : public nkunk::Script {\n", classe.CStr());
			t += "\tpublic:\n";
			t += "\t\tvoid Debut() override {\n";
			t += NkString::Format("\t\t\tAfficher(\"Bonjour depuis %s (C++)\");\n", classe.CStr());
			t += "\t\t}\n\n";
			t += "\t\tvoid Tick(float dt) override {\n";
			t += "\t\t\t(void)dt;\n";
			t += "\t\t\tetat.images += 1;\n";
			t += "\t\t}\n\n";
			t += "\t\t// L'etat PRIVE a garder quand ce fichier est recompile pendant Jouer.\n";
			t += "\t\tstruct Etat {\n";
			t += "\t\t\t\tint images = 0;\n";
			t += "\t\t} etat;\n";
			t += "\t\tNK_UNKENY_GARDER(etat)\n";
			t += "};\n\n";
			t += "// Les variables EXPOSEES apparaissent dans les Details (sauvees avec la scene).\n";
			t += NkString::Format("NK_UNKENY_CLASSE_VARIABLES(%s, NK_UNKENY_REEL(\"vitesse\", 2.f))\n", classe.CStr());
			if (!NkFile::WriteAllText(chemin.CStr(), t.CStr())) {
				NkEditeurAnnoncer(m, "Nouveau script C++ : écriture impossible");
				return NkString();
			}
			const NkString nav = NkEditeurNavigateurDe(m, chemin.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Script C++ créé : %s (classe cpp:%s)", nav.CStr(), classe.CStr()).CStr());
			return nav;
		}

		NkString NkEditeurNouveauBlueprint(NkEditeurModele &m, const char *dossierNav) {
			const NkString dossier = NkEditeurCheminContenu(m, dossierNav);
			NkDirectory::CreateRecursive(dossier.CStr());
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), "NouveauBlueprint.nkbp");
			graph::NkNodeGraph g;
			NkBpGrapheBonjour(g);
			NkErreurBp err;
			if (chemin.Empty() || !NkBpEnregistrer(chemin.CStr(), g, err)) {
				NkEditeurAnnoncer(m, NkString::Format("Nouveau Blueprint : %s", err.message.Empty() ? "écriture impossible" : err.message.CStr()).CStr());
				return NkString();
			}
			const NkString nav = NkEditeurNavigateurDe(m, chemin.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Blueprint créé : %s", nav.CStr()).CStr());
			return nav;
		}

		bool NkEditeurOuvrirTexteExterne(const char *chemin) {
			if (chemin == nullptr || !NkFile::Exists(chemin)) {
				return false;
			}
#if defined(_WIN32)
			auto Large = [](const NkString &s, NkVector<wchar_t> &w) {
				const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, nullptr, 0);
				w.Resize(static_cast<usize>(n > 0 ? n : 1));
				::MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, w.Data(), n);
			};
			NkVector<wchar_t> fichier, argument;
			Large(NkString(chemin), fichier);
			Large(NkString("\"") + chemin + "\"", argument);
			// NKCode, s'il est construit a cote du depot.
			const NkString depot = NkTrouverDepot();
			const char *candidats[] = {"Build/Bin/Release-Windows/NKCode/NKCode.exe", "Build/Bin/Debug-Windows/NKCode/NKCode.exe"};
			for (const char *c : candidats) {
				const NkString exe = depot + c;
				if (!depot.Empty() && NkFile::Exists(exe.CStr())) {
					NkVector<wchar_t> w;
					Large(exe, w);
					const HINSTANCE h = ::ShellExecuteW(nullptr, L"open", w.Data(), argument.Data(), nullptr, SW_SHOWNORMAL);
					if (reinterpret_cast<intptr_t>(h) > 32) {
						return true;
					}
				}
			}
			// L'editeur associe aux .cpp par le systeme, sinon le bloc-notes.
			HINSTANCE h = ::ShellExecuteW(nullptr, L"open", fichier.Data(), nullptr, nullptr, SW_SHOWNORMAL);
			if (reinterpret_cast<intptr_t>(h) > 32) {
				return true;
			}
			h = ::ShellExecuteW(nullptr, L"open", L"notepad.exe", argument.Data(), nullptr, SW_SHOWNORMAL);
			return reinterpret_cast<intptr_t>(h) > 32;
#elif defined(__APPLE__)
			const NkString cmd = NkString("open -t \"") + chemin + "\" &";
			return std::system(cmd.CStr()) == 0;
#else
			const NkString cmd = NkString("xdg-open \"") + chemin + "\" >/dev/null 2>&1 &";
			return std::system(cmd.CStr()) == 0;
#endif
		}

	} // namespace editeur
} // namespace nkentseu
