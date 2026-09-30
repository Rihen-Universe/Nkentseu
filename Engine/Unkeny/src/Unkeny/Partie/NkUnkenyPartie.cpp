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

		NkStatsRendu NkDessinerPartie(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsRenduParticules &rendu,
									  NkStatsEclairage2D *eclairage, int32 *particules) {
			NkOptionsFormes formes;
			formes.couleurDecor = &CouleurDuSprite;
			NkDessinerFormes(dl, scene, formes);
			const NkStatsRendu stats = NkDessinerScene(dl, scene);
			NkDessinerCorpsMous(dl, scene, rendu);
			// ⚠️ L'ORDRE PORTE LE SENS (2026-09-30) : la fumee (alpha) AVANT la
			//    carte, que la nuit l'assombrisse ; le feu (additif) APRES, qu'il
			//    brille. Eclairage eteint et aucune particule : aucun de ces trois
			//    appels n'ecrit dans la liste (temoin L1 du banc lumiere).
			int32 n = NkDessinerEffets(dl, scene, NkPasseEffets2D::NK_EFFETS_ECLAIRES);
			const NkStatsEclairage2D st = NkDessinerEclairage(dl, scene);
			n += NkDessinerEffets(dl, scene, NkPasseEffets2D::NK_EFFETS_EMISSIFS);
			if (eclairage != nullptr) {
				*eclairage = st;
			}
			if (particules != nullptr) {
				*particules = n;
			}
			return stats;
		}

	} // namespace unkeny
} // namespace nkentseu
