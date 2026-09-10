// =============================================================================
// NkUnkenyVues.h — plusieurs vues d'une meme scene, et les MINIATURES
//
// A QUOI SERT CE FICHIER
//   Regarder la meme scene depuis plusieurs endroits, en meme temps :
//     - un ecran partage a deux joueurs ;
//     - une minicarte dans un coin ;
//     - une vignette de previsualisation dans un editeur ;
//     - une lunette, un retroviseur, une camera de surveillance dans le jeu.
//
//   Toutes reviennent au meme geste : une NkVue2D + un rectangle de destination.
//
// DEUX FACONS DE RENDRE UNE VUE, ET ELLES NE SE VALENT PAS
//   DIRECTE     on dessine la scene dans le viseur, dans la meme liste
//               d'affichage. Rien a allouer, rien a synchroniser. C'est ce
//               qu'il faut pour un ecran partage ou une minicarte.
//   HORS-ECRAN  on dessine dans une NkRenderTexture, et on affiche l'image
//               obtenue. Necessaire quand l'image doit SURVIVRE a la trame :
//               une vignette d'editeur qu'on ne rafraichit qu'au changement,
//               un effet qui relit sa propre image.
//
//   ⚠️ La forme hors-ecran coute une cible de rendu et un changement de cible
//   par trame. Ce n'est pas gratuit, et ce n'est presque jamais necessaire pour
//   une minicarte. On offre les deux, et on dit laquelle prendre.
//
// ⚠️ LES TROIS « CAMERAS » DU DEPOT, parce qu'elles se confondent
//     NkVue2D                  ce qu'on voit d'une scene, et ou       (Unkeny)
//     nkentseu::NkCamera2D     pose d'une camera virtuelle AR         (NKCamera)
//     NkCameraSystem           la camera REELLE de l'appareil         (NKCamera)
//   Une camera REELLE n'est pas une vue : c'est une SOURCE D'IMAGES. Elle
//   n'appartient donc pas a ce fichier — voir NkUnkenyCameraReelle.h.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un cadrage automatique (suivre le joueur) -> ici, en methode de vue
//   - un effet plein ecran                      -> apres le rendu, chez le jeu
// =============================================================================
#pragma once

#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Scene/NkUnkenyCamera.h"

namespace nkentseu {
	namespace unkeny {

		/// Une vue posee sur une scene : ou l'on regarde, et ou l'on dessine.
		struct NkVuePosee {
				NkVue2D vue;
				bool active = true;

				/// Suivi doux d'une cible. `souplesse` a 0 = suivi rigide (la
				/// camera colle) ; 0,1 = elle rattrape en douceur.
				/// ⚠️ Le suivi rigide donne mal au coeur sur un personnage qui
				/// saute ; c'est le premier reglage qu'on regrette de ne pas
				/// avoir expose.
				bool suitCible = false;
				NkVec2f cible{0.f, 0.f};
				float32 souplesse = 0.12f;

				/// Bornes du deplacement, en monde. La camera ne sort pas de la
				/// carte : sans cela on voit le vide au bord des niveaux.
				bool borne = false;
				NkVec2f bornesMin{0.f, 0.f};
				NkVec2f bornesMax{0.f, 0.f};

				void Avancer(float32 dt) noexcept;
		};

		/// Une miniature : une vue rendue HORS ECRAN, dont l'image survit a la
		/// trame. Utile a un editeur (vignette de scene) et aux effets.
		class NkMiniature {
			public:
				bool Creer(renderer::NkIRenderer2D &rendu, uint32 largeur, uint32 hauteur);
				void Liberer();

				bool EstValide() const noexcept {
					return mValide;
				}
				NkVue2D &Vue() noexcept {
					return mVue;
				}

				/// Dessine la scene dans la texture. A appeler AVANT la liste
				/// d'affichage principale : changer de cible de rendu au milieu
				/// d'une trame deja commencee melange les deux images.
				NkStatsRendu Rendre(NkScene &scene, const renderer::NkColor2D &fond);

				renderer::NkRenderTexture &Cible() noexcept {
					return mCible;
				}

				uint32 Largeur() const noexcept {
					return mLargeur;
				}
				uint32 Hauteur() const noexcept {
					return mHauteur;
				}

			private:
				renderer::NkRenderTexture mCible;
				NkVue2D mVue;
				uint32 mLargeur = 0;
				uint32 mHauteur = 0;
				bool mValide = false;
		};

		/// Dessine une scene a travers une vue POSEE, dans la liste d'affichage
		/// courante. C'est la forme DIRECTE — celle qu'il faut par defaut.
		///
		/// ⚠️ Elle pose un rectangle de decoupe sur le viseur : sans lui, une
		/// minicarte dans un coin deborderait sur tout l'ecran, et le defaut ne
		/// se verrait qu'avec une scene assez grande.
		NkStatsRendu NkRendreVue(nkgui::NkGuiDrawList &dl, NkScene &scene, NkVuePosee &vue);

	} // namespace unkeny
} // namespace nkentseu
