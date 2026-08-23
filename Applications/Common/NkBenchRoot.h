#pragma once
// -----------------------------------------------------------------------------
// @File    NkBenchRoot.h
// @Brief   ANCRE DE DEPOT pour les bancs console. En-tete PUR : rien a lier.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE, ET POURQUOI IL N EST PAS DANS LE KERNEL
//   Un banc lit des ressources (`Resources/...`) et parfois sa propre reference.
//   Ces chemins se resolvent depuis la RACINE du worktree ; le repertoire de
//   lancement, lui, varie. Un banc lance d ailleurs rend « 0 OK / N FAIL » et
//   RESSEMBLE a un chargeur casse.
//
//   > Le symptome d un chemin non resolu est indiscernable de celui d un
//   > chargeur casse. Un verificateur diagnostiquera un defaut qui n existe pas,
//   > et le rediagnostiquera a chaque passe.
//
//   C est un utilitaire de TEST : il n a rien a faire dans le moteur. Il vit
//   donc a cote des bancs, en en-tete pur, sans cible de build.
//
// ⚠️ POURQUOI PAS LA PARADE PRECEDENTE (essayer "", "../../", "../../../")
//   Elle a ete ecrite trois fois et elle etait FAUSSE dans une des trois copies :
//   elle n ancrait que les ressources, pas la reference du harnais. Mesure :
//   `--check` sortait en 2 depuis la racine et en 0 depuis le dossier du banc,
//   avec le MEME binaire. Et aucune des trois copies ne survivait a un
//   lancement depuis un repertoire hors du depot (mesure : code 1).
//   Trois copies d une meme regle divergent ; une seule ne peut pas.
//
// CE QU ELLE FAIT
//   Remonte jusqu au marqueur `Nkentseu.jenga` depuis le repertoire courant,
//   PUIS depuis le repertoire du binaire (argv[0]) -- ce second essai est le
//   seul qui sauve un lancement hors du depot.
//
// ⚠️ AUCUN REPLI. Si l ancre est introuvable, `NkBenchRootFound()` rend faux et
//   l appelant DOIT s arreter en erreur. Retomber sur des chemins relatifs
//   preserverait le succes en mesurant des fichiers absents -- c est le repli
//   qui ment, la forme qu on a deja payee ailleurs.
//
// USAGE
//   #include "NkBenchRoot.h"
//   int main(int argc, char **argv) {
//       NkBenchRootInit(argc, argv);
//       if (!NkBenchRootFound()) { ...message...; return 3; }
//       ... NkBenchPath("Resources/Models/test/cube.usda") ...
//   }
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"

#include <stdio.h>
#include <string.h>

namespace nkentseu {

	namespace nkbench_detail {

		inline char *Argv0Buffer() {
			static char buf[1024] = {0};
			return buf;
		}

		inline bool &RootFoundFlag() {
			static bool found = false;
			return found;
		}

		inline bool Readable(const char *p) {
			FILE *f = fopen(p, "rb");
			if (!f)
				return false;
			fclose(f);
			return true;
		}

		// Remonte au plus 12 crans depuis `start` (vide, ou termine par '/').
		inline bool SearchUp(const NkString &start, NkString &out) {
			NkString p = start;
			for (int i = 0; i < 12; ++i) {
				if (Readable((p + NkString("Nkentseu.jenga")).Data())) {
					out = p;
					return true;
				}
				p = p + NkString("../");
			}
			return false;
		}

		inline NkString ComputeRoot() {
			NkString r;
			if (SearchUp(NkString(""), r)) {
				RootFoundFlag() = true;
				return r;
			}
			const char *a0 = Argv0Buffer();
			if (a0[0]) {
				char base[1024];
				strncpy(base, a0, sizeof(base) - 1);
				base[sizeof(base) - 1] = 0;
				for (char *ch = base; *ch; ++ch)
					if (*ch == 0x5C) // antislash Windows -> slash, une seule forme a couper
						*ch = '/';
				char *slash = strrchr(base, '/');
				if (slash)
					*(slash + 1) = 0;
				else
					base[0] = 0;
				if (SearchUp(NkString(base), r)) {
					RootFoundFlag() = true;
					return r;
				}
			}
			RootFoundFlag() = false;
			return NkString("");
		}

		inline const NkString &Root() {
			static NkString r = ComputeRoot();
			return r;
		}

	} // namespace nkbench_detail

	// A appeler EN PREMIER dans main(), avant toute mesure : l ancre est resolue
	// une seule fois et argv[0] doit etre connu a ce moment-la.
	inline void NkBenchRootInit(int argc, char **argv) {
		if (argc > 0 && argv && argv[0]) {
			char *buf = nkbench_detail::Argv0Buffer();
			strncpy(buf, argv[0], 1023);
			buf[1023] = 0;
		}
		(void)nkbench_detail::Root(); // force la resolution ici, pas au premier chemin
	}

	inline bool NkBenchRootFound() {
		(void)nkbench_detail::Root();
		return nkbench_detail::RootFoundFlag();
	}

	inline const char *NkBenchArgv0() {
		const char *a = nkbench_detail::Argv0Buffer();
		return a[0] ? a : "(argv[0] absent)";
	}

	inline NkString NkBenchPath(const char *relatifDepuisLaRacine) {
		return nkbench_detail::Root() + NkString(relatifDepuisLaRacine);
	}

} // namespace nkentseu
