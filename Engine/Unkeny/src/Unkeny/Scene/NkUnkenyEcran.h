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

		// =====================================================================
		// LA CAMERA DU JEU SELON L'ECRAN (document 03, §2.6)
		//
		// Le jeu est regarde dans l'editeur a travers un viseur de REFERENCE
		// (sa taille et son zoom, cuits avec le jeu). Sur un autre ecran, une
		// REGLE du projet decide de ce qui se voit :
		//   tout montrer  le cadre de reference entier, et le monde au-dela
		//                 (le comportement d'avant le 01/10 : la valeur 0)
		//   hauteur fixe  la meme hauteur de monde ; la largeur suit l'ecran
		//   largeur fixe  la meme largeur de monde ; la hauteur suit
		//   bandes        tout le cadre, et RIEN au-dela : bandes noires
		//   remplir       l'ecran entier couvert ; le cadre est rogne
		// =====================================================================
		enum class NkRegleCamera : uint8 {
			NK_TOUT_MONTRER = 0,
			NK_HAUTEUR_FIXE,
			NK_LARGEUR_FIXE,
			NK_BANDES,
			NK_REMPLIR,
			NK_COUNT
		};

		/// Le zoom et le viseur de la camera du jeu sur un ecran donne. `vue`
		/// est l'ecran, sauf avec des bandes : le rectangle au rapport de la
		/// reference, centre (le reste est noir).
		struct NkCadrageCamera {
				float32 zoom = 32.f;
				nkgui::NkRect vue{0.f, 0.f, 0.f, 0.f};
		};

		inline NkCadrageCamera NkCadrerCamera(NkRegleCamera regle, float32 refLargeur, float32 refHauteur, float32 refZoom,
											  const nkgui::NkRect &ecran) noexcept {
			NkCadrageCamera c;
			c.zoom = refZoom;
			c.vue = ecran;
			if (refLargeur <= 1.f || refHauteur <= 1.f || ecran.w <= 1.f || ecran.h <= 1.f) {
				return c; // pas de reference : le zoom tel quel
			}
			const float32 kx = ecran.w / refLargeur;
			const float32 ky = ecran.h / refHauteur;
			const float32 kMin = kx < ky ? kx : ky;
			const float32 kMax = kx < ky ? ky : kx;
			switch (regle) {
				case NkRegleCamera::NK_HAUTEUR_FIXE:
					c.zoom = refZoom * ky;
					break;
				case NkRegleCamera::NK_LARGEUR_FIXE:
					c.zoom = refZoom * kx;
					break;
				case NkRegleCamera::NK_REMPLIR:
					c.zoom = refZoom * kMax;
					break;
				case NkRegleCamera::NK_BANDES: {
					c.zoom = refZoom * kMin;
					const float32 w = refLargeur * kMin;
					const float32 h = refHauteur * kMin;
					c.vue = nkgui::NkRect{ecran.x + (ecran.w - w) * 0.5f, ecran.y + (ecran.h - h) * 0.5f, w, h};
					break;
				}
				default:
					c.zoom = refZoom * kMin;
					break;
			}
			return c;
		}

		/// L'ecran du jeu RESTREINT a la vue de sa camera (les bandes) : le
		/// rectangle devient `vue`, les marges ne gardent que ce qui depasse
		/// encore dans la vue. Le HUD s'ancre ainsi DANS l'image du jeu ET dans
		/// la zone sure -- jamais dans une bande noire, ou il serait coupe.
		/// Sans bande (`vue` = l'ecran), l'ecran est rendu tel quel.
		inline NkEcranDuJeu NkEcranDansVue(const NkEcranDuJeu &e, const nkgui::NkRect &vue) noexcept {
			NkEcranDuJeu r = e;
			r.rect = vue;
			auto reste = [](float32 v) { return v > 0.f ? v : 0.f; };
			r.marges.top = reste(e.rect.y + e.marges.top - vue.y);
			r.marges.left = reste(e.rect.x + e.marges.left - vue.x);
			r.marges.bottom = reste((vue.y + vue.h) - (e.rect.y + e.rect.h - e.marges.bottom));
			r.marges.right = reste((vue.x + vue.w) - (e.rect.x + e.rect.w - e.marges.right));
			return r;
		}

		/// Le nom ecrit dans jeu.json et dans le .nkappareil de l'editeur.
		inline const char *NkNomRegleCamera(NkRegleCamera r) noexcept {
			static const char *kNoms[5] = {"tout-montrer", "hauteur-fixe", "largeur-fixe", "bandes", "remplir"};
			const int32 i = static_cast<int32>(r);
			return i >= 0 && i < 5 ? kNoms[i] : kNoms[0];
		}
		/// false (et `r` inchange) pour un nom inconnu.
		inline bool NkRegleCameraDepuisNom(const char *nom, NkRegleCamera &r) noexcept {
			for (int32 i = 0; i < static_cast<int32>(NkRegleCamera::NK_COUNT); ++i) {
				const char *a = NkNomRegleCamera(static_cast<NkRegleCamera>(i));
				const char *b = nom != nullptr ? nom : "";
				while (*a != '\0' && *a == *b) {
					++a;
					++b;
				}
				if (*a == '\0' && *b == '\0') {
					r = static_cast<NkRegleCamera>(i);
					return true;
				}
			}
			return false;
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
