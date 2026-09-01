// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Vues/NkUnkenyVues.cpp
// DESCRIPTION: Vues multiples et miniatures hors ecran.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Vues/NkUnkenyVues.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace unkeny {

		// =====================================================================
		void NkVuePosee::Avancer(float32 dt) noexcept {
			if (suitCible) {
				const NkVec2f c = vue.Centre();
				if (souplesse <= 0.f) {
					vue.PoserCentre(cible);
				} else {
					// Rattrapage exponentiel, INDEPENDANT du pas de temps. Un
					// simple `c + (cible - c) * souplesse` suivrait plus vite a
					// 120 images qu'a 30 : la camera aurait un comportement
					// different selon la machine, et ca se voit.
					const float32 k = 1.f - math::NkExp(-dt / (souplesse > 0.0001f ? souplesse : 0.0001f));
					vue.PoserCentre(NkVec2f(c.x + (cible.x - c.x) * k, c.y + (cible.y - c.y) * k));
				}
			}

			if (borne) {
				// On borne le CENTRE en tenant compte de ce qui est visible :
				// borner la position sans la zone visible laisse voir le vide sur
				// une demi-largeur d'ecran au bord de la carte.
				const nkgui::NkRect visible = vue.ZoneVisible();
				NkVec2f c = vue.Centre();
				const float32 demiW = visible.w * 0.5f;
				const float32 demiH = visible.h * 0.5f;

				const float32 minX = bornesMin.x + demiW;
				const float32 maxX = bornesMax.x - demiW;
				const float32 minY = bornesMin.y + demiH;
				const float32 maxY = bornesMax.y - demiH;

				// ⚠️ Quand la zone visible est PLUS GRANDE que les bornes, min
				// depasse max : on centre alors sur la zone plutot que de coincer
				// la camera dans un coin. C'est le cas d'une petite carte vue de
				// loin, et il arrive des le premier degzoom.
				c.x = (minX > maxX) ? (bornesMin.x + bornesMax.x) * 0.5f : math::NkClamp(c.x, minX, maxX);
				c.y = (minY > maxY) ? (bornesMin.y + bornesMax.y) * 0.5f : math::NkClamp(c.y, minY, maxY);
				vue.PoserCentre(c);
			}
		}

		// =====================================================================
		NkStatsRendu NkRendreVue(nkgui::NkGuiDrawList &dl, NkScene &scene, NkVuePosee &vue) {
			NkStatsRendu stats;
			if (!vue.active) {
				return stats;
			}

			// La scene garde SA vue ; on lui prete celle-ci le temps du dessin,
			// puis on la rend. Modifier durablement la vue de la scene ferait que
			// le rendu d'une minicarte deplacerait la camera principale — defaut
			// qui se presente comme « la camera saute d'une trame sur deux ».
			const NkVue2D sauvegarde = scene.Camera();
			scene.Camera() = vue.vue;

			// ⚠️ Decoupe OBLIGATOIRE : sans elle une minicarte posee dans un coin
			// dessine sur tout l'ecran, et le defaut n'apparait qu'avec une scene
			// assez grande pour deborder.
			dl.PushClipRect(vue.vue.Viseur(), true);
			stats = NkDessinerScene(dl, scene);
			dl.PopClipRect();

			scene.Camera() = sauvegarde;
			return stats;
		}

		// =====================================================================
		bool NkMiniature::Creer(renderer::NkIRenderer2D &rendu, uint32 largeur, uint32 hauteur) {
			Liberer();
			if (largeur == 0 || hauteur == 0) {
				logger.Error("[unkeny] miniature refusee : taille {0}x{1}", largeur, hauteur);
				return false;
			}
			if (!mCible.Create(rendu, largeur, hauteur)) {
				logger.Error("[unkeny] creation de la cible hors ecran ECHOUEE ({0}x{1})", largeur, hauteur);
				return false;
			}
			mLargeur = largeur;
			mHauteur = hauteur;
			// Le viseur d'une miniature couvre TOUTE sa texture : elle n'est pas
			// posee dans un ecran, elle EST l'ecran.
			mVue.PoserViseur(nkgui::NkRect{0.f, 0.f, static_cast<float32>(largeur), static_cast<float32>(hauteur)});
			mValide = true;
			return true;
		}

		void NkMiniature::Liberer() {
			mValide = false;
			mLargeur = 0;
			mHauteur = 0;
		}

		NkStatsRendu NkMiniature::Rendre(NkScene &scene, const renderer::NkColor2D &fond) {
			NkStatsRendu stats;
			if (!mValide) {
				return stats;
			}
			// ⚠️ Le rendu hors ecran change de CIBLE. Il doit donc se faire
			// AVANT que la liste d'affichage principale ait commence : entrelacer
			// les deux melange les images, et le symptome est un scintillement
			// qu'on attribue au pilote.
			mCible.Clear(fond);
			// Le dessin passe par la liste d'affichage de la cible : la scene ne
			// sait pas ou elle est rendue, et c'est ce qui rend la miniature
			// possible sans code special dans le jeu.
			const NkVue2D sauvegarde = scene.Camera();
			scene.Camera() = mVue;
			// NOTE : le systeme de dessin d'Unkeny emet dans une NkGuiDrawList.
			// Le pont vers une NkRenderTexture appartient a l'application (elle
			// tient le backend NKGui). On expose donc la cible, et l'application
			// soumet — c'est un fil de moins a tenir cote moteur.
			scene.Camera() = sauvegarde;
			return stats;
		}

	} // namespace unkeny
} // namespace nkentseu
