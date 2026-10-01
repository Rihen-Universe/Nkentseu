// =============================================================================
// NkUnkenyEcran.h — l'ECRAN du jeu et sa ZONE SURE, tels que le jeu les voit
//
// A QUOI SERT CE FICHIER (document 03 d'UnkenyEditor, §2.5, 2026-10-01)
//   Un jeu doit savoir OU il peut poser un bouton : pas sous l'encoche, pas
//   sous l'indicateur de geste. Ce fichier lui donne l'ecran (le rectangle ou
//   il dessine) et la zone sure, et de quoi y ANCRER son interface.
//
// ⚠️ UNE SEULE SOURCE PAR SITUATION, ET UNKENY N'EN CALCULE AUCUNE
//   Sur l'appareil, la zone sure est celle de NKWindow (GetSafeAreaInsets),
//   que NKCanvas recopie dans NkLayoutInfo et que le joueur (UnkenyPlayer)
//   transmet. Dans l'editeur, c'est celle de l'appareil SIMULE (meme structure,
//   NkLayoutSimule). Unkeny ne fait que la LIRE : la scene la recoit par
//   PoserEcran, a chaque trame, de celui qui l'affiche.
//
// ⚠️ LES MARGES SONT CELLES DE NKWINDOW : NkSafeAreaInsets, en pixels de
//   l'ecran. Pas un type de plus : le jeu lit la meme chose partout.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKEvent/NkSafeArea.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		/// L'ecran du jeu.
		/// ⚠️ « NkEcranDuJeu », pas « NkEcranJeu » : ce nom-la est deja l'ecran
		/// de MENU ou de PARTIE d'un jeu de plateau (Jeu/NkUnkenySieges.h).
		struct NkEcranDuJeu {
				/// Ou le jeu dessine, en pixels de la FENETRE : la fenetre entiere
				/// sur l'appareil, l'ecran de l'appareil simule dans l'editeur.
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f};
				/// La zone sure, en pixels de `rect` : la structure de NKWindow.
				NkSafeAreaInsets marges;
				/// Pixels de `rect` par point : un decalage ecrit en points garde
				/// sa taille physique d'un ecran a l'autre.
				float32 densite = 1.f;
				/// Vrai quand l'ecran est SIMULE (l'editeur) : pour le dire, jamais
				/// pour calculer autrement.
				bool simule = false;

				bool Valide() const noexcept {
					return rect.w > 0.f && rect.h > 0.f;
				}
		};

		/// Le rectangle SUR : l'ecran moins ses marges, en pixels de la fenetre.
		/// Jamais negatif : des marges plus grandes que l'ecran rendent un
		/// rectangle vide, centre, et non un rectangle a l'envers.
		inline nkgui::NkRect NkZoneSure(const NkEcranDuJeu &e) noexcept {
			float32 w = e.rect.w - e.marges.left - e.marges.right;
			float32 h = e.rect.h - e.marges.top - e.marges.bottom;
			const float32 x = e.rect.x + e.marges.left + (w < 0.f ? w * 0.5f : 0.f);
			const float32 y = e.rect.y + e.marges.top + (h < 0.f ? h * 0.5f : 0.f);
			return nkgui::NkRect{x, y, w < 0.f ? 0.f : w, h < 0.f ? 0.f : h};
		}

		/// Les neuf ancres d'une interface : ligne (haut, milieu, bas) x
		/// colonne (gauche, centre, droite).
		enum class NkAncre : uint8 {
			NK_HAUT_GAUCHE = 0,
			NK_HAUT,
			NK_HAUT_DROITE,
			NK_GAUCHE,
			NK_CENTRE,
			NK_DROITE,
			NK_BAS_GAUCHE,
			NK_BAS,
			NK_BAS_DROITE,
			NK_COUNT
		};

		/// Un rectangle `w` x `h` ancre dans `zone`, ecarte de (`dx`, `dy`) du
		/// bord d'ancrage, VERS L'INTERIEUR (au centre, le decalage pousse vers
		/// la droite et le bas).
		inline nkgui::NkRect NkAncrer(const nkgui::NkRect &zone, NkAncre a, float32 w, float32 h, float32 dx = 0.f,
									  float32 dy = 0.f) noexcept {
			const int32 i = static_cast<int32>(a) % 9;
			const int32 col = i % 3;
			const int32 lig = i / 3;
			const float32 x = col == 0 ? zone.x + dx : (col == 2 ? zone.x + zone.w - w - dx : zone.x + (zone.w - w) * 0.5f + dx);
			const float32 y = lig == 0 ? zone.y + dy : (lig == 2 ? zone.y + zone.h - h - dy : zone.y + (zone.h - h) * 0.5f + dy);
			return nkgui::NkRect{x, y, w, h};
		}

		/// COMPOSANT : une entite d'INTERFACE (HUD) tenue a l'ecran. Chaque
		/// trame, NkAppliquerAncrages (Unkeny/Partie/NkUnkenyZoneSure.h) pose
		/// son transform pour que son sprite tombe a l'ancre, dans la zone sure.
		struct NkAncrageEcran2D {
				uint8 ancre = static_cast<uint8>(NkAncre::NK_HAUT_DROITE); ///< NkAncre
				/// Ecart au bord d'ancrage, en POINTS (x densite = pixels).
				math::NkVec2f decalage{12.f, 12.f};
				/// true : dans la ZONE SURE (texte, boutons). false : au bord de
				/// l'ecran (un fond, une bande decorative qui doit y aller).
				bool zoneSure = true;
		};

	} // namespace unkeny
} // namespace nkentseu
