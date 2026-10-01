// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurWorkspaceCpp.cpp
// DESCRIPTION: Le workspace Jenga des scripts C++ d'un projet (voir l'en-tete).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurWorkspaceCpp.h"

#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurProcessus.h"

#include "NKFileSystem/NkFile.h"

#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			/// « \ » -> « / », sans barre finale.
			NkString Propre(const char *chemin) {
				NkString r(chemin != nullptr ? chemin : "");
				char *p = const_cast<char *>(r.CStr());
				for (usize i = 0; i < r.Length(); ++i) {
					if (p[i] == '\\') {
						p[i] = '/';
					}
				}
				while (r.Length() > 1u && r.CStr()[r.Length() - 1u] == '/') {
					r = NkString(r.CStr(), r.Length() - 1u);
				}
				return r;
			}
			/// (Banc) MUTATION nommee, `NK_WS_MUTATION` (voir NkEditeurScripts.cpp) :
			/// « illisible » casse la partie generee, « ecrase » reecrit tout le
			/// fichier sans egard pour l'utilisateur. Sans la variable, rien.
			bool Mutation(const char *nom) {
				const char *v = std::getenv("NK_WS_MUTATION");
				return v != nullptr && std::strcmp(v, nom) == 0;
			}
			/// L'indice du debut de la premiere ligne qui COMMENCE par `repere`, a
			/// partir de `depuis` ; -1 sinon.
			int64 Ligne(const NkString &texte, const char *repere, usize depuis) {
				const char *t = texte.CStr();
				const usize n = texte.Length(), r = std::strlen(repere);
				for (usize i = depuis; i + r <= n; ++i) {
					if ((i == 0u || t[i - 1u] == '\n') && std::strncmp(t + i, repere, r) == 0) {
						return static_cast<int64>(i);
					}
				}
				return -1;
			}
		} // namespace

		NkString NkEditeurWorkspaceNom(const char *projet) {
			const NkString p = Propre(projet);
			const char *barre = std::strrchr(p.CStr(), '/');
			return NkIdentifiantJeu(barre != nullptr ? barre + 1 : p.CStr());
		}

		NkString NkEditeurWorkspaceChemin(const char *projet) {
			return Propre(projet) + "/" + NkEditeurWorkspaceNom(projet) + ".jenga";
		}

		NkString NkEditeurWorkspaceDll(const char *projet) {
			// Le nom que Jenga donne a une bibliotheque partagee (Builder.GetTargetPath :
			// le nom du projet et l'extension de la plateforme, sans prefixe « lib »).
#if defined(_WIN32)
			const char *ext = ".dll";
#elif defined(__APPLE__)
			const char *ext = ".dylib";
#else
			const char *ext = ".so";
#endif
			return Propre(projet) + "/" + NK_WS_SORTIE + "/" + NK_WS_PROJET + ext;
		}

		NkString NkEditeurWorkspacePartie(const char *nom, const char *depot) {
			NkString t;
			t += NkString(NK_WS_DEBUT) + " -- partie GENEREE par UnkenyEditor, refaite a chaque ouverture du projet.\n";
			t += "#     N'ecrivez rien ici (ce serait remplace) : la suite du fichier est a vous.\n";
			t += "from Jenga import *\n";
			t += "from Jenga.GlobalToolchains import RegisterJengaGlobalToolchains\n\n";
			t += "# Le depot Nkentseu de l'editeur qui a ouvert le projet : l'en-tete des scripts\n";
			t += "# (Engine/Unkeny/src/Unkeny/Script/NkUnkenyScriptABI.h).\n";
			t += NkString::Format("NK = r\"%s\"\n\n", Propre(depot).CStr());
			t += NkString::Format("with workspace(\"%s\"):\n", nom != nullptr ? nom : "Jeu");
			t += "    RegisterJengaGlobalToolchains()\n";
			t += "    configurations([\"Debug\", \"Release\"])\n";
			t += "    targetoses([TargetOS.WINDOWS, TargetOS.LINUX, TargetOS.MACOS])\n";
			t += "    targetarchs([TargetArch.X86_64, TargetArch.ARM64])\n\n";
			t += "    # Les scripts C++ du projet : UNE bibliotheque partagee, que UnkenyEditor\n";
			t += "    # RECHARGE A CHAUD des qu'elle change (meme pendant Jouer). Le registre des\n";
			t += "    # classes est ecrit par l'editeur (il lit les NK_UNKENY_CLASSE des sources).\n";
			t += NkString::Format("    with project(\"%s\"):\n", NK_WS_PROJET);
			t += Mutation("illisible") ? "        sharedlibrairie()\n" : "        sharedlib()\n";
			t += "        language(\"C++\")\n";
			t += "        cppdialect(\"C++17\")\n";
			t += "        files([\"Contenu/**.cpp\", \"Intermediaire/Scripts/NkUnkRegistre.cpp\"])\n";
			t += "        includedirs([NK + \"/Engine/Unkeny/src\"])\n";
			t += NkString::Format("        objdir(\"%%{wks.location}/%s/Obj/%%{cfg.buildcfg}-%%{cfg.system}\")\n", NK_WS_SORTIE);
			t += NkString::Format("        targetdir(\"%%{wks.location}/%s\")\n", NK_WS_SORTIE);
			t += "        with filter(\"system:Windows\"):\n";
			t += "            # La chaine clang-mingw ; la DLL ne depend que du systeme (les memes\n";
			t += "            # options que la compilation directe de l'editeur).\n";
			t += "            usetoolchain(\"clang-mingw\")\n";
			t += "            ldflags([\"-static-libstdc++\", \"-static-libgcc\", \"-static\"])\n";
			t += NkString(NK_WS_FIN) + " -- fin de la partie generee.\n";
			return t;
		}

		NkString NkEditeurWorkspaceNeuf(const char *nom, const char *depot) {
			const NkString n(nom != nullptr ? nom : "Jeu");
			NkString t;
			t += "#!/usr/bin/env python3\n";
			t += "# -*- coding: utf-8 -*-\n";
			t += "# =============================================================================\n";
			t += NkString::Format("# %s.jenga -- le workspace Jenga des SCRIPTS C++ du projet Unkeny « %s »\n", n.CStr(), n.CStr());
			t += "#\n";
			t += "# Cree par UnkenyEditor, comme Unreal genere la solution d'un projet. Dans\n";
			t += "# l'editeur, un double-clic sur un script C++ ouvre NKCode SUR ce workspace.\n";
			t += "#   - Enregistrer (Ctrl+S) : l'editeur recompile seul et recharge a chaud ;\n";
			t += "#   - Construire (Ctrl+B dans NKCode, ou a la main depuis ce dossier :\n";
			t += NkString::Format("#       jenga build --jenga-file %s.jenga --config Debug --platform Windows-x86_64\n", n.CStr());
			t += NkString::Format("#     ) produit %s/%s.dll : l'editeur la RECHARGE A CHAUD\n", NK_WS_SORTIE, NK_WS_PROJET);
			t += "#     des qu'elle change, meme pendant Jouer.\n";
			t += "#\n";
			t += "# La partie entre « >>> UNKENY » et « <<< UNKENY » est refaite par l'editeur a\n";
			t += "# chaque ouverture du projet. Le RESTE du fichier est a vous : l'editeur n'y\n";
			t += "# touche jamais. Pour qu'il refasse tout, supprimez ce fichier.\n";
			t += "# =============================================================================\n";
			t += NkEditeurWorkspacePartie(nom, depot);
			t += "\n";
			t += "# ── A VOUS ──────────────────────────────────────────────────────────────────\n";
			t += "# Ce qui suit, indente de 4 espaces, est DANS le workspace. Par exemple :\n";
			t += "#     with project(\"Outils\"):\n";
			t += "#         consoleapp()\n";
			t += "#         files([\"Outils/**.cpp\"])\n";
			return t;
		}

		bool NkEditeurWorkspaceRemplacer(const NkString &texte, const NkString &partie, NkString &sortie) {
			const int64 debut = Ligne(texte, NK_WS_DEBUT, 0u);
			if (debut < 0) {
				return false;
			}
			const int64 fin = Ligne(texte, NK_WS_FIN, static_cast<usize>(debut) + 1u);
			if (fin < 0) {
				return false;
			}
			// La ligne du repere de fin, fin de ligne comprise.
			usize apres = static_cast<usize>(fin);
			while (apres < texte.Length() && texte.CStr()[apres] != '\n') {
				++apres;
			}
			if (apres < texte.Length()) {
				++apres;
			}
			sortie = NkString(texte.CStr(), static_cast<usize>(debut)) + partie + NkString(texte.CStr() + apres);
			return true;
		}

		NkWorkspaceCpp NkEditeurAssurerWorkspaceCpp(const char *projet, const char *depot) {
			NkWorkspaceCpp r;
			r.nom = NkEditeurWorkspaceNom(projet);
			r.chemin = NkEditeurWorkspaceChemin(projet);
			const NkString nomFichier = r.nom + ".jenga";
			if (depot == nullptr || *depot == '\0') {
				r.message = NkString::Format("[C++] %s non écrit : dépôt Nkentseu introuvable (posez NK_UNKENY_DEPOT, ou lancez l'éditeur "
											 "depuis le dépôt) -- le workspace doit nommer l'en-tête NkUnkenyScriptABI.h",
											 nomFichier.CStr());
				return r;
			}
			const NkString partie = NkEditeurWorkspacePartie(r.nom.CStr(), depot);
			if (!NkFile::Exists(r.chemin.CStr()) || Mutation("ecrase")) {
				if (!NkFile::WriteAllText(r.chemin.CStr(), NkEditeurWorkspaceNeuf(r.nom.CStr(), depot).CStr())) {
					r.message = NkString("[C++] écriture impossible : ") + r.chemin;
					return r;
				}
				r.etat = NkEtatWorkspace::NK_CREE;
				r.message = NkString::Format("[C++] workspace Jenga des scripts créé : %s (NKCode s'ouvre dessus ; sa DLL est rechargée à chaud)",
											 r.chemin.CStr());
				return r;
			}
			const NkString texte = NkFile::ReadAllText(r.chemin.CStr());
			NkString nouveau;
			if (!NkEditeurWorkspaceRemplacer(texte, partie, nouveau)) {
				r.etat = NkEtatWorkspace::NK_RESPECTE;
				r.message = NkString::Format("[C++] %s n'a pas les repères « %s » / « %s » : l'éditeur n'y touche pas (vos réglages "
											 "restent). Pour qu'il le refasse, supprimez-le ou remettez les deux repères.",
											 nomFichier.CStr(), NK_WS_DEBUT, NK_WS_FIN);
				return r;
			}
			if (nouveau == texte) {
				r.etat = NkEtatWorkspace::NK_A_JOUR;
				return r;
			}
			if (!NkFile::WriteAllText(r.chemin.CStr(), nouveau.CStr())) {
				r.message = NkString("[C++] écriture impossible : ") + r.chemin;
				return r;
			}
			r.etat = NkEtatWorkspace::NK_MIS_A_JOUR;
			r.message = NkString::Format("[C++] %s : partie générée mise à jour (le reste du fichier est gardé tel quel)", nomFichier.CStr());
			return r;
		}

		const char *NkEditeurWorkspacePlateforme() noexcept {
#if defined(_WIN32)
			return "Windows-x86_64";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
			return "macOS-arm64";
#else
			return "macOS-x86_64";
#endif
#else
			return "Linux-x86_64";
#endif
		}

		int32 NkEditeurJengaSurWorkspace(const char *workspace, const char *arguments, NkVector<NkString> &sortie) {
			sortie.Clear();
			const NkString ws = Propre(workspace);
			const char *barre = std::strrchr(ws.CStr(), '/');
			const NkString dossier = barre != nullptr ? NkString(ws.CStr(), static_cast<usize>(barre - ws.CStr())) : NkString();
			NkEditeurProcessus p;
			const NkString cmd = NkString::Format("jenga %s --jenga-file \"%s\"", arguments != nullptr ? arguments : "info", ws.CStr());
			if (!p.Lancer(cmd, dossier)) {
				NkVector<NkString> raison;
				p.Recolter(raison);
				for (usize i = 0; i < raison.Size(); ++i) {
					sortie.PushBack(raison[i]);
				}
				return -1;
			}
			return p.Attendre([](const NkString &ligne, void *d) { static_cast<NkVector<NkString> *>(d)->PushBack(ligne); }, &sortie);
		}

	} // namespace editeur
} // namespace nkentseu
