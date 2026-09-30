//
// NkUnkenyPartie.cpp
// =============================================================================
// Description :
//   La trame et l'image du jeu, communes a l'editeur et au joueur autonome.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Partie/NkUnkenyPartie.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			/// Le sprite d'un decor est CACHE (le decor est dessine par sa forme)
			/// mais il garde la couleur : c'est ainsi que l'editeur pose un sol
			/// plus sombre que les obstacles. 0 = la couleur de decor par defaut.
			uint32 CouleurDuSprite(ecs::NkWorld &monde, ecs::NkEntityId id, void *) {
				const NkSprite2D *s = monde.Get<NkSprite2D>(id);
				if (s == nullptr) {
					return 0u;
				}
				return s->couleur;
			}
		} // namespace

		void NkAvancerPartie(NkScene &scene, float32 dt) {
			if (dt <= 0.f) {
				return;
			}
			scene.Pas(dt < NK_PARTIE_TRAME_MAX ? dt : NK_PARTIE_TRAME_MAX);
		}

		NkStatsRendu NkDessinerPartie(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsRenduParticules &rendu) {
			NkOptionsFormes formes;
			formes.couleurDecor = &CouleurDuSprite;
			NkDessinerFormes(dl, scene, formes);
			const NkStatsRendu stats = NkDessinerScene(dl, scene);
			NkDessinerCorpsMous(dl, scene, rendu);
			return stats;
		}

	} // namespace unkeny
} // namespace nkentseu
