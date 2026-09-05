#pragma once
// -----------------------------------------------------------------------------
// @File    Selecteur.h
// @Brief   ② LE SELECTEUR DE FICHIERS A DEUX VOLETS, cote application : le style
//          (les roles du theme du costume) et LA VIGNETTE (le cache d'images).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// -----------------------------------------------------------------------------
//  CE QUE CE FICHIER FAIT, ET SURTOUT CE QU'IL NE FAIT PAS
//
//  Il ne dessine RIEN. Le selecteur est dans le kit (`NKEditorKit/NkFilePickerNav.h`)
//  parce que Rodolf l'a demande explicitement : « s'il est dans l'application,
//  remonte-le dans NKEditorKit ». Ce qui reste ici est ce qu'AUCUNE autre
//  application ne partage :
//    1. LES ROLES. Le navigateur de contenu ne connait que des jetons de theme.
//       Les resoudre demande la table de l'application ; on lit donc la
//       DECLARATION du composant et on resout son `defaultRole` -- c'est le
//       « premier pas gratuit » : aucune instance, aucune couleur en dur.
//    2. LA VIGNETTE. Le kit ne sait ni lire un PNG ni fabriquer une texture.
//       NkUIDesign, lui, a deja `NkCacheImages` (celui des remplissages image).
//       ⚠️ On ne charge QUE les extensions d'image : demander une vignette pour
//          un `.zip` de 400 Mo ferait passer le selecteur pour un plantage.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkFilePickerNav.h"

namespace nkuidesign {

	// ⚠️ LE STYLE A MONTE DANS LE KIT (⑥, 05/09 soir) : `NkStyleSelecteurDefaut`
	//    resout les memes treize roles depuis la declaration du composant, et
	//    `NkDrawSelecteur` l'appelle pour toute application. Le garder ici en aurait
	//    fait le second endroit ou vivent les memes valeurs -- exactement ce que ce
	//    fichier reproche aux navigateurs recopies.

	/// L'extension designe-t-elle une image que `NkImage` sait relire ? La liste
	/// est celle des codecs du depot, pas une supposition.
	inline bool NkEstUneImage(const char *chemin) {
		static const char *kExt[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif",
									 ".psd", ".hdr", ".pic", ".pnm", ".ppm", ".pgm", ".svg"};
		if (!chemin)
			return false;
		nkentseu::usize n = 0;
		while (chemin[n])
			++n;
		for (nkentseu::usize k = 0; k < sizeof(kExt) / sizeof(kExt[0]); ++k) {
			nkentseu::usize m = 0;
			while (kExt[k][m])
				++m;
			if (m > n)
				continue;
			bool ok = true;
			for (nkentseu::usize i = 0; i < m && ok; ++i) {
				char a = chemin[n - m + i];
				if (a >= 'A' && a <= 'Z')
					a = (char)(a + 32);
				ok = (a == kExt[k][i]);
			}
			if (ok)
				return true;
		}
		return false;
	}

	// ⚠️ LA VIGNETTE ELLE-MEME N'EST PAS ICI : elle a besoin de `NkCacheImages`,
	//    qui vit dans `Panels.h` avec l'etat de l'application. Elle est donc
	//    definie dans `ExportSVG.h`, apres lui (`NkVignetteFichierExport`).
	//    L'inclure a l'envers ferait un cycle -- et un fichier de style n'a rien
	//    a savoir du cache d'images.

} // namespace nkuidesign
