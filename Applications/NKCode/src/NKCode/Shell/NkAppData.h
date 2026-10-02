#pragma once
// =============================================================================
// NkAppData.h — OU SONT LES DONNEES LIVREES DE NKCODE (`data/` : polices,
// textures, logo, icones, icons.cfg, langues), quel que soit le dossier de
// lancement.
//
// ⚠️ LA PANNE (2026-10-01) : lance depuis `Build/Bin/Debug-Windows/NKCode/`
//    (double-clic, raccourci), NKCode n'y trouvait pas `data/` — le `build` ne
//    l'y copie pas, seul le paquet le fait (`dependfiles(["data => data"])`).
//    Resultat : icones vides et un carre bleu a la place du logo.
//
// Trois variantes, essayees par NkPath::LocateResource dans le dossier courant,
// puis le dossier de l'exe, puis en remontant leurs parents :
//   « Applications/NKCode/data/... » (le depot), « data/... » (le paquet : il
//   passe donc AVANT la racine du depot trouvee en remontant), « NKCode/data/... ».
// =============================================================================
#include "NKFileSystem/NkPath.h"
#include <cstdio>

namespace nkentseu {
	namespace nkcode {

		/// Le chemin ABSOLU de `data/<sousChemin>` (« fonts », « textures »,
		/// « icons.cfg », « lang »). Vide si introuvable : le journal liste alors
		/// les dossiers essayes (sauf `avertir == false`).
		inline NkString NkCodeData(const char *sousChemin, bool avertir = true) {
			const NkString depot = NkString("Applications/NKCode/data/") + sousChemin;
			const NkString paquet = NkString("data/") + sousChemin;
			const NkString voisin = NkString("NKCode/data/") + sousChemin;
			return NkPath::LocateResource({depot.CStr(), paquet.CStr(), voisin.CStr()}, avertir);
		}

		/// Le meme, en DOSSIER pret a prefixer un nom de fichier (barre finale), ou
		/// vide si introuvable.
		inline NkString NkCodeDataDir(const char *sousDossier, bool avertir = true) {
			NkString d = NkCodeData(sousDossier, avertir);
			if (!d.Empty())
				d += '/';
			return d;
		}

		/// `NKCode --ressources` : OU sont les donnees livrees, SANS fenetre --
		/// le temoin d'un lancement depuis n'importe quel dossier. Rend 0 si
		/// toutes sont trouvees, 1 sinon (le journal dit alors ou elles ont ete
		/// cherchees).
		inline int NkCodeVerifierDonnees() {
			static const char *const kDonnees[] = {"fonts/NotoSans-Regular.ttf", "textures/logo/nkcode_icon.png",
												   "textures/icon/Accueil.png", "icons.cfg", "lang"};
			int manquantes = 0;
			for (const char *d : kDonnees) {
				const NkString p = NkCodeData(d);
				std::printf("[NKCode] data/%-30s -> %s\n", d, p.Empty() ? "INTROUVABLE" : p.CStr());
				if (p.Empty())
					++manquantes;
			}
			std::printf("[NKCode] ressources : %s\n", manquantes == 0 ? "TOUTES TROUVEES" : "MANQUANTES");
			std::fflush(stdout);
			return manquantes == 0 ? 0 : 1;
		}

	} // namespace nkcode
} // namespace nkentseu
