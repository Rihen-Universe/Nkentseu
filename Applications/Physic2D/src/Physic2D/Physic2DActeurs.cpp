// =============================================================================
// Physic2DActeurs.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physic2DActeurs.h"
#include "NKPhysics/NkParticules2DFabrique.h"

#include <cstdio>

namespace nkentseu {
	namespace physic2d {

		using namespace unkeny;
		using physics::NkPresetP2D;

		namespace {
			NkRessourcesSim gRessources;

			ecs::NkEntityId Statique(NkScene &s, const char *nom, const NkVec2f &centre, const NkVec2f &demi, bool sol) {
				const ecs::NkEntityId e = s.Creer(nom, centre);
				NkCollisionneur2D col;
				col.forme = NkForme2D::NK_BOITE;
				col.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkDecor2D d;
				d.sol = sol;
				s.Monde().Add<NkDecor2D>(e, d);
				NkCorps2D c;
				c.type = NkTypeCorps::NK_STATIQUE;
				c.friction = 0.6f;
				s.AjouterCorps(e, c);
				return e;
			}

			/// Un corps mou pose TEL QUEL (taille propre au niveau, pas celle du
			/// catalogue) : la fabrique de NKPhysics, puis l'entite.
			ecs::NkEntityId Mou(NkScene &s, NkActeur a, int32 indexCorps) {
				if (indexCorps < 0) {
					return ecs::NkEntityId::Invalid();
				}
				char nom[32];
				NkNommerActeurSim(nom, sizeof(nom), a);
				return s.CreerCorpsMou(nom, indexCorps, NkActeurInfo(a).couleur);
			}
		} // namespace

		void NkCreerRessourcesDemo(NkTextures2D *textures, NkSons2D *sons) {
			NkCreerRessourcesSim(gRessources, textures, sons);
		}

		const NkRessourcesSim &NkRessourcesDemo() noexcept {
			return gRessources;
		}

		ecs::NkEntityId NkPoserActeur(NkScene &s, NkActeur a, const NkVec2f &posDemandee) {
			// On pose DANS la boite : un acteur ne nait pas dans un mur.
			const NkVec2f pos(math::NkClamp(posDemandee.x, -NK_DEMI_LARGEUR + 0.7f, NK_DEMI_LARGEUR - 0.7f),
							  math::NkClamp(posDemandee.y, 0.7f, NK_HAUTEUR - 0.7f));
			return NkPoserActeurSim(s, a, pos, &gRessources, NK_HAUTEUR - 0.1f);
		}

		ecs::NkEntityId NkPoserObstacle(NkScene &s, const NkVec2f &a, const NkVec2f &b, float32 rayon) {
			const ecs::NkEntityId e = NkPoserObstacleSim(s, a, b, rayon);
			s.Monde().Add<NkDecor2D>(e, NkDecor2D{});
			return e;
		}

		const char *NkNiveauNom(int32 i) noexcept {
			switch (i) {
				case 0:
					return "Démo · Tout à la fois";
				case 1:
					return "Bac à slime";
				case 2:
					return "Cristaux et chaleur";
				case 3:
					return "Tissus, cordes et ponts";
				default:
					return "Niveau vide";
			}
		}

		void NkChargerNiveau(NkScene &s, int32 i) {
			// Le marqueur de decor doit survivre a Jouer / Arreter : sans lui,
			// le sol restaure devient effacable a la gomme.
			s.PhotographierAussi<NkDecor2D>("physic2d.NkDecor2D");
			NkVector<ecs::NkEntityId> ids;
			s.Entites(ids);
			for (uint32 k = 0; k < ids.Size(); ++k) {
				s.Detruire(ids[k]);
			}
			if (physics::NkParticules2D *p = s.Particules()) {
				p->Vider();
				p->reglages.temperature = 0.f;
				p->reglages.vent = 0.f;
			}
			NkRemettreNomsSim();

			// La boite : sol, murs, plafond. Des corps STATIQUES ordinaires : les
			// rigides et les particules les touchent par le meme chemin.
			const float32 L = NK_DEMI_LARGEUR;
			const float32 H = NK_HAUTEUR;
			Statique(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(L + 1.f, 0.5f), true);
			Statique(s, "Mur_gauche", NkVec2f(-L - 0.5f, H * 0.5f), NkVec2f(0.5f, H * 0.5f + 1.f), true);
			Statique(s, "Mur_droit", NkVec2f(L + 0.5f, H * 0.5f), NkVec2f(0.5f, H * 0.5f + 1.f), true);
			Statique(s, "Plafond", NkVec2f(0.f, H + 0.5f), NkVec2f(L + 1.f, 0.5f), true);

			auto obstacle = [&](float32 ax, float32 ay, float32 bx, float32 by, float32 r) {
				NkPoserObstacle(s, NkVec2f(ax, ay), NkVec2f(bx, by), r);
			};
			auto poser = [&](NkActeur a, float32 x, float32 y) { NkPoserActeur(s, a, NkVec2f(x, y)); };
			physics::NkParticules2D *p = s.Particules();

			switch (i) {
				case 0: {
					obstacle(-11.5f, 7.6f, -6.4f, 5.6f, 0.10f); // rampe
					obstacle(1.5f, 0.f, 1.5f, 1.9f, 0.09f);	   // bac a eau
					obstacle(4.7f, 0.f, 4.7f, 1.9f, 0.09f);
					obstacle(-5.2f, 0.f, -5.2f, 3.3f, 0.12f); // piliers du pont
					obstacle(-1.2f, 0.f, -1.2f, 3.3f, 0.12f);
					obstacle(6.2f, 4.8f, 9.8f, 4.8f, 0.10f); // etagere
					obstacle(-0.4f, 7.6f, -0.4f, 7.6f, 0.22f); // chevilles
					obstacle(0.8f, 7.0f, 0.8f, 7.0f, 0.22f);
					NkPoserPont(s, NkVec2f(-5.2f, 3.45f), NkVec2f(-1.2f, 3.45f));
					if (p != nullptr) {
						Mou(s, NkActeur::NK_EAU, physics::NkCreerFluideP2D(*p, NkVec2f(3.1f, 1.1f), 0.95f, NkPresetP2D::NK_EAU));
					}
					poser(NkActeur::NK_BALLON, -10.5f, 9.f);
					poser(NkActeur::NK_BLOB, -3.3f, 8.f);
					if (p != nullptr) {
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(3.f, 9.f), 0.70f, NkPresetP2D::NK_SLIME));
					}
					poser(NkActeur::NK_GELEE, -3.f, 11.5f);
					poser(NkActeur::NK_CAISSE, 7.6f, 5.6f);
					poser(NkActeur::NK_BALLE, 9.f, 6.f);
					poser(NkActeur::NK_TISSU, 9.2f, 9.4f);
					poser(NkActeur::NK_CRISTAL, -8.6f, 0.6f);
					poser(NkActeur::NK_CORDE, -0.6f, 10.f);
					break;
				}
				case 1: {
					obstacle(-7.f, 4.2f, -3.f, 1.2f, 0.12f); // entonnoir
					obstacle(3.f, 1.2f, 7.f, 4.2f, 0.12f);
					obstacle(-3.f, 1.2f, -3.f, 0.f, 0.12f);
					obstacle(3.f, 1.2f, 3.f, 0.f, 0.12f);
					if (p != nullptr) {
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(-3.5f, 9.f), 0.80f, NkPresetP2D::NK_SLIME));
						Mou(s, NkActeur::NK_BLOB, physics::NkCreerBlobP2D(*p, NkVec2f(0.f, 11.5f), 0.45f, NkPresetP2D::NK_BLOB));
						Mou(s, NkActeur::NK_BLOB, physics::NkCreerBlobP2D(*p, NkVec2f(2.5f, 8.f), 0.40f, NkPresetP2D::NK_BLOB));
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(5.2f, 11.5f), 0.60f, NkPresetP2D::NK_SLIME));
						Mou(s, NkActeur::NK_MIEL, physics::NkCreerFluideP2D(*p, NkVec2f(-8.5f, 6.f), 0.70f, NkPresetP2D::NK_MIEL));
					}
					poser(NkActeur::NK_BALLON, 9.f, 7.f);
					poser(NkActeur::NK_GELEE, 0.f, 6.f);
					poser(NkActeur::NK_CAISSE, -9.f, 1.f);
					break;
				}
				case 2: {
					poser(NkActeur::NK_CRISTAL, -7.f, 0.6f);
					poser(NkActeur::NK_CRISTAL, -7.f, 4.f);
					poser(NkActeur::NK_DISQUE, 0.f, 0.8f);
					poser(NkActeur::NK_DISQUE, 0.f, 5.f);
					poser(NkActeur::NK_CRISTAL, 6.5f, 0.6f);
					obstacle(4.f, 7.f, 9.f, 7.f, 0.10f);
					poser(NkActeur::NK_CAISSE, 6.5f, 9.f);
					break;
				}
				case 3: {
					poser(NkActeur::NK_TISSU, -7.f, 10.f);
					poser(NkActeur::NK_TISSU, -2.5f, 10.f);
					poser(NkActeur::NK_CORDE, 2.5f, 10.5f);
					poser(NkActeur::NK_CORDE, 4.f, 10.5f);
					poser(NkActeur::NK_CORDE, 5.5f, 10.5f);
					obstacle(-9.f, 0.f, -9.f, 3.f, 0.12f);
					obstacle(-3.f, 0.f, -3.f, 3.f, 0.12f);
					NkPoserPont(s, NkVec2f(-9.f, 3.15f), NkVec2f(-3.f, 3.15f));
					poser(NkActeur::NK_BALLON, -6.f, 6.f);
					poser(NkActeur::NK_CAISSE, -7.5f, 7.f);
					poser(NkActeur::NK_GELEE, -4.5f, 7.5f);
					poser(NkActeur::NK_BALLE, 4.f, 6.f);
					break;
				}
				default:
					break;
			}
		}

	} // namespace physic2d
} // namespace nkentseu
