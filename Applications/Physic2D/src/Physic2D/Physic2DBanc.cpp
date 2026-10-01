// =============================================================================
// Physic2DBanc.cpp — `Physic2D --selftest` : l'INTEGRATION dans Unkeny
//
// Le solveur a ses temoins dans NKPhysics (tests/test_particules2d.cpp). Ici on
// eprouve ce que la demo ajoute par-dessus : la scene Unkeny qui porte des
// entites de corps mous COUPLES a de vrais corps rigides, leur synchronisation,
// la photo Jouer / Arreter, et la destruction.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (u1)  une scene `particules` seule cree AUSSI le monde physique
//   (u2)  une caisse rigide posee sur le sol statique y repose (y = demi-cote)
//   (u3)  un blob lache sur la caisse s'arrete DESSUS (son centre au-dessus) ;
//         (u3n) couplage coupe : il passe au travers, son centre finit dessous
//   (u4)  le NkTransform2D d'un corps mou suit le centre de sa matiere
//   (u5)  Photographier puis Restaurer rend les positions de la photo (blob ET
//         caisse), le meme nombre d'entites ; (u5n) sans Restaurer, elles ont bouge
//   (u5d) [ajoute le 2026-09-29, apres qu'une image l'a montre] le composant du
//         JEU NkDecor2D survit a Restaurer : le sol reste un sol
//   (u6)  Detruire l'entite supprime le corps de particules
//   (u7)  un corps dont toute la matiere est gommee perd son entite au pas suivant
//   (u8)  la masse et le materiau de NkCorps2D arrivent au solveur rigide
//   (u9)  chaque niveau tourne 5 s sans NaN ni particule perdue sous le sol
//   (u10) [ajoute le 2026-09-29] chaque niveau, joue 1 s, enregistre en JSON puis
//         relu dans une autre scene : memes particules, decor intact (4 sols),
//         et il tourne encore 2 s sans NaN
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physic2DBanc.h"
#include "Unkeny/Banc/NkUnkenyBancTas.h" // scenes de banc sur le tas (pile macOS)
#include "Physic2D/Physic2DActeurs.h"
#include "NKPhysics/NkParticules2DFabrique.h"

#include <chrono>
#include <cstdio>

namespace nkentseu {
	namespace physic2d {

		using namespace unkeny;

		namespace {
			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}

			float32 Absf(float32 v) {
				return v < 0.f ? -v : v;
			}

			bool Scene(NkScene &s, bool physique, bool particules) {
				NkSceneConfig cfg;
				cfg.physique = physique;
				cfg.particules = particules;
				cfg.gravite = NkVec2f(0.f, -9.81f);
				return s.Init(cfg);
			}

			void Avancer(NkScene &s, float32 secondes) {
				const int32 n = static_cast<int32>(secondes * 60.f + 0.5f);
				for (int32 k = 0; k < n; ++k) {
					s.Pas(1.f / 60.f);
				}
			}

			float32 YCorps(NkScene &s, ecs::NkEntityId e) {
				const NkTransform2D *t = s.Monde().Get<NkTransform2D>(e);
				return t != nullptr ? t->position.y : -1.0e9f;
			}

		} // namespace

		int32 NkPhysic2DLancerBanc() {
			std::printf("Physic2D — banc de l'integration Unkeny (NKECS + NKPhysics + NkParticules2D)\n\n");

			{
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, false, true);
				Temoin(s.MondePhysique() != nullptr && s.Particules() != nullptr,
					   "(u1) particules seules : le monde physique est cree aussi", 1.f);
			}

			// (u2) (u3) (u4) (u8)
			for (int32 k = 0; k < 2; ++k) {
				const bool couple = k == 0;
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, true, true);
				NkChargerNiveau(s, NK_NB_NIVEAUX - 1);
				const ecs::NkEntityId caisse = NkPoserActeur(s, NkActeur::NK_CAISSE, NkVec2f(0.f, 0.8f));
				Avancer(s, 2.f);
				if (couple) {
					Temoin(Absf(YCorps(s, caisse) - 0.33f) < 0.03f, "(u2) caisse rigide posee sur le sol statique (y m)", YCorps(s, caisse));
					const NkCorps2D *c = s.Monde().Get<NkCorps2D>(caisse);
					const physics::NkRigidBody *b = s.MondePhysique()->GetBody(c->corpsId);
					Temoin(Absf(b->invMass - 1.f / 20.f) < 1.0e-5f && Absf(b->material.dynamicFriction - 0.6f) < 1.0e-5f &&
							   Absf(b->material.restitution - 0.1f) < 1.0e-5f,
						   "(u8) masse 20 kg, friction 0,6, rebond 0,1 transmis au solveur", 1.f / b->invMass);
				}
				const ecs::NkEntityId blob = NkPoserActeur(s, NkActeur::NK_BLOB, NkVec2f(0.f, 2.5f));
				if (!couple) {
					const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(blob);
					s.Particules()->corps[static_cast<uint32>(s.Particules()->IndexCorps(m->corpsId))].couplageRigide = false;
				}
				Avancer(s, 4.f);
				// Le CENTRE du blob, pas son point le plus bas : un blob de 84 cm sur
				// une caisse de 66 cm deborde et coule sur les cotes jusqu'au sol,
				// sans pour autant traverser la caisse.
				const float32 centreY = YCorps(s, blob);
				const float32 dessus = YCorps(s, caisse) + 0.33f;
				if (couple) {
					Temoin(centreY > dessus, "(u3) blob lache sur la caisse : il reste dessus (centre du blob m)", centreY);
					const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(blob);
					const NkVec2f centre = s.Particules()->CentreCorps(static_cast<uint32>(s.Particules()->IndexCorps(m->corpsId)));
					const NkTransform2D *t = s.Monde().Get<NkTransform2D>(blob);
					const float32 ecart = Absf(t->position.x - centre.x) + Absf(t->position.y - centre.y);
					Temoin(ecart < 1.0e-4f, "(u4) le transform du corps mou suit sa matiere (ecart m)", ecart);
				} else {
					// Sans couplage, ni la caisse ni le sol rigide ne le retiennent : il tombe.
					Temoin(centreY < dessus - 0.3f, "(u3n) couplage coupe : il traverse (centre du blob m)", centreY);
				}
			}

			// (u5) photo
			{
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, true, true);
				NkChargerNiveau(s, NK_NB_NIVEAUX - 1);
				NkPoserActeur(s, NkActeur::NK_CAISSE, NkVec2f(-2.f, 3.f));
				const ecs::NkEntityId blob = NkPoserActeur(s, NkActeur::NK_BLOB, NkVec2f(2.f, 3.f));
				Avancer(s, 0.3f);
				NK_BANC_SUR_TAS(NkScene::NkPhoto, photo);
				s.Photographier(photo);
				const float32 yBlob = YCorps(s, blob);
				NkVector<ecs::NkEntityId> avant;
				s.Entites(avant);
				Avancer(s, 2.f);
				const float32 yBouge = YCorps(s, blob);
				Temoin(Absf(yBouge - yBlob) > 0.1f, "(u5n) sans Restaurer, le blob a bouge (m)", yBouge - yBlob);
				s.Restaurer(photo);
				NkVector<ecs::NkEntityId> apres;
				s.Entites(apres);
				// Retrouver le blob et la caisse restaures.
				float32 yB = -1.f, yC = -1.f, yCPhoto = -2.f;
				for (uint32 i = 0; i < photo.entites.Size(); ++i) {
					if (photo.entites[i].aCorps && !photo.entites[i].aMou && photo.entites[i].corps.type == NkTypeCorps::NK_DYNAMIQUE) {
						yCPhoto = photo.entites[i].transform.position.y;
					}
				}
				for (uint32 i = 0; i < apres.Size(); ++i) {
					if (s.Monde().Has<NkCorpsMou2D>(apres[i])) {
						yB = YCorps(s, apres[i]);
					}
					const NkCorps2D *c = s.Monde().Get<NkCorps2D>(apres[i]);
					if (c != nullptr && c->type == NkTypeCorps::NK_DYNAMIQUE) {
						yC = s.MondePhysique()->GetBody(c->corpsId)->position.y;
					}
				}
				Temoin(Absf(yB - yBlob) < 1.0e-4f && Absf(yC - yCPhoto) < 1.0e-4f && apres.Size() == avant.Size(),
					   "(u5) Restaurer rend la photo (ecart de hauteur du blob m)", yB - yBlob);
				int32 sols = 0;
				for (uint32 i = 0; i < apres.Size(); ++i) {
					const NkDecor2D *d = s.Monde().Get<NkDecor2D>(apres[i]);
					sols += (d != nullptr && d->sol) ? 1 : 0;
				}
				Temoin(sols == 4, "(u5d) le decor du jeu survit a Restaurer (sol, 2 murs, plafond)", static_cast<float32>(sols));
				Avancer(s, 0.5f);
				Temoin(YCorps(s, apres[0]) == YCorps(s, apres[0]) && s.Particules()->particules.Size() > 0,
					   "(u5) la scene restauree se simule", static_cast<float32>(s.Particules()->particules.Size()));
			}

			// (u6) (u7)
			{
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, true, true);
				NkChargerNiveau(s, NK_NB_NIVEAUX - 1);
				const ecs::NkEntityId a = NkPoserActeur(s, NkActeur::NK_GELEE, NkVec2f(-3.f, 2.f));
				const uint32 idA = s.Monde().Get<NkCorpsMou2D>(a)->corpsId;
				s.Detruire(a);
				Temoin(s.Particules()->IndexCorps(idA) < 0, "(u6) Detruire l'entite supprime la matiere", 0.f);

				const ecs::NkEntityId b = NkPoserActeur(s, NkActeur::NK_EAU, NkVec2f(3.f, 2.f));
				s.Particules()->Gommer(NkVec2f(3.f, 2.f), 1.f);
				Avancer(s, 1.f / 60.f);
				Temoin(!s.Monde().IsAlive(b), "(u7) toute la matiere gommee : l'entite disparait", 0.f);
			}

			// (u9) niveaux
			for (int32 niv = 0; niv < NK_NB_NIVEAUX; ++niv) {
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, true, true);
				NkChargerNiveau(s, niv);
				const uint32 n0 = static_cast<uint32>(s.Particules()->particules.Size());
				const auto t0 = std::chrono::steady_clock::now();
				Avancer(s, 5.f);
				const float32 ms = static_cast<float32>(
					std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 300.0);
				bool sain = s.Particules()->particules.Size() == n0;
				for (uint32 i = 0; i < s.Particules()->particules.Size(); ++i) {
					const NkVec2f &q = s.Particules()->particules[i].pos;
					sain = sain && q.x == q.x && q.y > -0.05f;
				}
				char quoi[96];
				std::snprintf(quoi, sizeof(quoi), "(u9) %s : 5 s sains (ms par pas)", NkNiveauNom(niv));
				Temoin(sain, quoi, ms);
			}

			// (u10) sauvegarde de chaque niveau
			for (int32 niv = 0; niv < NK_NB_NIVEAUX; ++niv) {
				NK_BANC_SUR_TAS(NkScene, s);
				Scene(s, true, true);
				NkChargerNiveau(s, niv);
				Avancer(s, 1.f);
				NkString json;
				const bool ecrit = NkSauverSceneJSON(s, json);
				NK_BANC_SUR_TAS(NkScene, r);
				r.PhotographierAussi<NkDecor2D>("physic2d.NkDecor2D");
				NkString err;
				const bool lu = ecrit && NkChargerSceneJSON(r, json.View(), nullptr, &err);
				bool sain = lu && r.Particules() != nullptr && r.Particules()->particules.Size() == s.Particules()->particules.Size();
				NkVector<ecs::NkEntityId> ids;
				r.Entites(ids);
				int32 sols = 0;
				for (uint32 i = 0; i < ids.Size(); ++i) {
					const NkDecor2D *d = r.Monde().Get<NkDecor2D>(ids[i]);
					sols += (d != nullptr && d->sol) ? 1 : 0;
				}
				if (sain) {
					Avancer(r, 2.f);
					for (uint32 i = 0; i < r.Particules()->particules.Size(); ++i) {
						const NkVec2f &q = r.Particules()->particules[i].pos;
						sain = sain && q.x == q.x && q.y > -0.05f;
					}
				}
				char quoi[96];
				std::snprintf(quoi, sizeof(quoi), "(u10) %s : enregistre, relu, rejoue (ko)", NkNiveauNom(niv));
				Temoin(sain && sols == 4, quoi, static_cast<float32>(json.Length()) / 1024.f);
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC REUSSI" : "BANC EN ECHEC", gReussis, gEchecs,
						gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace physic2d
} // namespace nkentseu
