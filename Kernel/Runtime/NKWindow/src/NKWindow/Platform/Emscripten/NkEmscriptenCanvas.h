// =============================================================================
// NkEmscriptenCanvas.h — LA taille du canvas Web, calculee UNE seule fois
//
// ⚠️ LE DEFAUT QUE CE FICHIER EXISTE POUR CORRIGER (Rodolf, 2026-09-01)
//   « La resolution de l'ecran n'est pas prise en compte : le texte et les
//   images sont etires. »
//
//   Un `<canvas>` a DEUX tailles, et elles n'ont rien a voir :
//     LE TAMPON DE DESSIN  `canvas.width` / `canvas.height` — le nombre de
//                          pixels que le programme peint.
//     LA TAILLE AFFICHEE   la taille CSS — la place que le navigateur lui
//                          donne dans la page.
//   Le navigateur ETIRE le tampon pour remplir la taille affichee. Quand les
//   deux rapports different, tout est deforme — et rien ne le signale.
//
//   La coquille HTML du depot pose `canvas { width:100%; height:100% }`, donc
//   la taille AFFICHEE est celle de la page. Mais la fenetre forcait le tampon
//   a la taille DEMANDEE par l'application (480x854 pour GemCrush). Un tampon
//   de rapport 0,56 etire sur un cadre de rapport 1,78 : texte et images
//   deformes, exactement comme rapporte.
//
// ⚠️ ET LE CHEMIN D'ENTREE, LUI, COMPENSAIT DEJA
//   `MapCssToCanvas` (NkEmscriptenEventSystem.cpp) convertit correctement les
//   coordonnees CSS vers le tampon. C'est pour cela que les clics tombaient
//   juste pendant que l'image etait deformee — et c'est ce qui rendait le
//   defaut difficile a nommer : « ca reagit bien, mais c'est etire ».
//   Deux chemins qui decrivent la meme geometrie, un seul qui la respecte.
//
// LA REGLE, ET ELLE TIENT EN UNE LIGNE
//   Sur le Web, le tampon de dessin vaut TAILLE CSS x devicePixelRatio.
//   - meme rapport que l'affichage  -> plus aucune deformation ;
//   - x devicePixelRatio            -> net sur un ecran dense, au lieu d'une
//                                      image doublee puis reetiree.
//   La taille demandee par l'application n'est qu'un SECOURS : sur une page,
//   c'est le CSS qui decide de la place, pas le programme.
//
// POURQUOI UN EN-TETE PARTAGE
//   Deux endroits ont besoin de ce calcul — la creation de la fenetre et le
//   rappel de redimensionnement du navigateur, qui vivent dans deux unites de
//   compilation. L'ecrire deux fois, c'est garantir qu'une seule suivra le jour
//   ou la regle change : c'est le defaut que ce depot documente sous « deux
//   chemins qui doivent s'accorder se valident l'un l'autre ».
// =============================================================================
#pragma once

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace nkentseu {
	namespace emscripten_canvas {

		/// Accorde le TAMPON DE DESSIN a la taille AFFICHEE x devicePixelRatio,
		/// et rend le tampon obtenu.
		///
		/// `secoursLargeur` / `secoursHauteur` ne servent que si le navigateur ne
		/// sait pas dire la taille CSS (page pas encore mise en page). Ils ne
		/// doivent JAMAIS l'emporter sur une taille CSS connue : la page decide.
		///
		/// N'ecrit RIEN quand la taille calculee est deja la bonne — reecrire
		/// `canvas.width` EFFACE le contenu du canvas et repart d'un tampon
		/// vierge. Appelee a chaque trame sans ce test, elle produirait un
		/// clignotement permanent.
		inline void AccorderTampon(const char *selecteur, uint32 secoursLargeur, uint32 secoursHauteur,
								   uint32 &sortieLargeur, uint32 &sortieHauteur) noexcept {
			double cssL = 0.0;
			double cssH = 0.0;
			int cible = 0;
			int cibleH = 0;

			if (emscripten_get_element_css_size(selecteur, &cssL, &cssH) == EMSCRIPTEN_RESULT_SUCCESS && cssL > 0.0 &&
				cssH > 0.0) {
				double ratio = emscripten_get_device_pixel_ratio();
				// ⚠️ Un ratio absurde se borne au lieu de se propager. Certains
				// navigateurs rendent 0 avant la premiere mise en page, et un
				// zoom extreme peut monter tres haut : un tampon de 0 pixel ou
				// de 40 000 pixels ne se voit pas ici, il fait echouer la
				// creation du contexte graphique bien plus loin.
				if (!(ratio > 0.0)) {
					ratio = 1.0;
				}
				if (ratio > 4.0) {
					ratio = 4.0;
				}
				cible = static_cast<int>(math::NkRound(cssL * ratio));
				cibleH = static_cast<int>(math::NkRound(cssH * ratio));
			}

			if (cible <= 0 || cibleH <= 0) {
				cible = static_cast<int>(secoursLargeur);
				cibleH = static_cast<int>(secoursHauteur);
			}
			if (cible <= 0) {
				cible = 1;
			}
			if (cibleH <= 0) {
				cibleH = 1;
			}

			int actuelL = 0;
			int actuelH = 0;
			emscripten_get_canvas_element_size(selecteur, &actuelL, &actuelH);
			if (actuelL != cible || actuelH != cibleH) {
				emscripten_set_canvas_element_size(selecteur, cible, cibleH);
			}

			sortieLargeur = static_cast<uint32>(cible);
			sortieHauteur = static_cast<uint32>(cibleH);
		}

	} // namespace emscripten_canvas
} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN
