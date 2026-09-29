//
// NkUnkenyControleurs.cpp
// =============================================================================
// Description :
//   Le systeme des controleurs de personnage (voir l'en-tete). Une seule
//   decision — sauter ? de combien accelerer ? — partagee par le rigide et le
//   mou ; seules la MESURE (ou est le sol) et l'APPLICATION (vitesse du corps
//   rigide, poussee ajoutee aux particules) different.
//
// Caracteristiques :
//   - Le sol vient de la physique, jamais d'un rayon lance au hasard : les
//     contacts du dernier pas (rigide), les contacts tenus sur le pas (mou).
//   - « Sauter seulement au sol » est une REGLE du systeme, pas une politesse
//     du jeu : une demande en l'air attend au plus `tamponSaut`, puis est
//     comptee comme refusee.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Jeu/NkUnkenyControleurs.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			/// Ce que le corps sait de son sol, au debut du pas fixe.
			struct NkMesureSol {
					bool auSol = false;
					NkVec2f normale{0.f, 0.f};
					NkVec2f vitesse{0.f, 0.f};	  ///< du corps (ou de la partie poussee)
					NkVec2f vitesseSol{0.f, 0.f}; ///< de ce qui le porte (plateforme mobile)
			};

			bool NomVide(const char *s) noexcept {
				return s == nullptr || s[0] == '\0';
			}

			/// LA decision, commune aux deux controleurs. Rend la variation de
			/// vitesse horizontale et, si le saut est accorde, `sauter`.
			void Decider(const NkReglagesControle2D &r, NkEtatControle2D &e, const NkActions &actions, const NkMesureSol &m,
						 float32 dt, float32 &dvx, bool &sauter) noexcept {
				// 1. Le sol. Juste apres un saut, le contact du pas precedent peut
				//    encore etre la alors que le corps MONTE : ce n'est pas un
				//    atterrissage (sinon un saut tamponne repartirait aussitot).
				const bool monte = e.sautEnCours && (m.vitesse.y - m.vitesseSol.y) > 0.5f;
				if (m.auSol && !monte) {
					e.auSol = true;
					e.sautEnCours = false;
					e.tempsHorsSol = 0.f;
					e.normaleSol = m.normale;
				} else {
					e.auSol = false;
					e.tempsHorsSol += dt;
					e.normaleSol = NkVec2f(0.f, 0.f);
				}

				// 2. La demande de saut : un FRONT du bouton, vu par le controleur.
				const bool bouton = actions.Enfoncee(r.actionSauter);
				if (bouton && !e.boutonAvant) {
					e.demandeEnAttente = true;
					e.tempsDemande = 0.f;
				} else if (e.demandeEnAttente) {
					e.tempsDemande += dt;
				}
				e.boutonAvant = bouton;

				// 3. Le saut : au sol, ou juste apres avoir QUITTE un bord (coyote) --
				//    jamais apres un saut, sinon ce serait un double saut.
				const bool peut = e.auSol || (!e.sautEnCours && e.tempsHorsSol <= r.coyote);
				sauter = false;
				if (e.demandeEnAttente) {
					if (peut) {
						sauter = true;
						e.demandeEnAttente = false;
						e.sautEnCours = true;
						e.auSol = false;
						e.tempsHorsSol = r.coyote + 1.f;
						e.sauts++;
					} else if (e.tempsDemande > r.tamponSaut) {
						e.demandeEnAttente = false;
						e.sautsRefuses++;
					}
				}

				// 4. La marche : vers la vitesse visee, a acceleration bornee. En
				//    l'air sans axe, l'elan est garde (aucun freinage).
				const float32 axe = actions.Valeur(r.actionX);
				const bool pousse = axe > 0.05f || axe < -0.05f;
				const float32 cible = axe * r.vitesseMax + (e.auSol ? m.vitesseSol.x : 0.f);
				float32 acc = 0.f;
				if (e.auSol) {
					acc = pousse ? r.acceleration : r.freinage;
				} else if (pousse) {
					acc = r.acceleration * math::NkClamp(r.controleAir, 0.f, 1.f);
				}
				dvx = math::NkClamp(cible - m.vitesse.x, -acc * dt, acc * dt);
			}

			void SystemeControleurs(NkScene &scene, float32 dt, void *donnees) {
				if (donnees != nullptr) {
					NkPasControleurs2D(scene, *static_cast<const NkActions *>(donnees), dt);
				}
			}
		} // namespace

		uint32 NkAjouterControleurs2D(NkScene &scene, const NkActions *actions, int32 ordre) {
			if (actions == nullptr) {
				logger.Warn("[unkeny] NkAjouterControleurs2D refuse : table d'actions nulle");
				return 0u;
			}
			// Le void * du planificateur n'a pas de const : le systeme ne fait que LIRE.
			return scene.AjouterSysteme("unkeny.controleurs2D", NkPhaseSysteme::NK_PAS_FIXE, &SystemeControleurs,
										const_cast<NkActions *>(actions), ordre);
		}

		void NkPasControleurs2D(NkScene &scene, const NkActions &actions, float32 dt) {
			if (dt <= 0.f) {
				return;
			}
			// --- Rigides ----------------------------------------------------
			if (physics::NkPhysicsWorld *monde = scene.MondePhysique()) {
				scene.Monde().Query<NkControleRigide2D, NkCorps2D>().ForEach(
					[&](ecs::NkEntityId, NkControleRigide2D &c, NkCorps2D &k) {
						if (!c.reglages.actif) {
							return;
						}
						physics::NkRigidBody *b = monde->GetBody(k.corpsId);
						if (b == nullptr || b->type != physics::NkBodyType::DYNAMIC) {
							return;
						}
						NkMesureSol m;
						m.vitesse = NkVec2f(b->linearVelocity.x, b->linearVelocity.y);
						NkVec2f somme(0.f, 0.f);
						// Sur la pile : pas d'allocation par pas fixe. Seize contacts
						// suffisent a un personnage (au-dela, les suivants sont ignores).
						physics::NkBodyContact contacts[16];
						const uint32 nContacts = monde->BodyContacts(k.corpsId, contacts, 16u);
						for (uint32 i = 0; i < nContacts; ++i) {
							const physics::NkBodyContact &ct = contacts[i];
							if (ct.normal.y < c.reglages.penteMax) {
								continue; // un mur, un plafond : ne porte pas
							}
							if (!m.auSol) {
								// Ce qui porte le PREMIER : sa vitesse emporte le personnage.
								if (const physics::NkRigidBody *sol = monde->GetBody(ct.other)) {
									if (sol->type != physics::NkBodyType::STATIC) {
										m.vitesseSol = NkVec2f(sol->linearVelocity.x, sol->linearVelocity.y);
									}
								}
							}
							m.auSol = true;
							somme += NkVec2f(ct.normal.x, ct.normal.y);
						}
						const float32 l = math::NkSqrt(somme.x * somme.x + somme.y * somme.y);
						m.normale = l > 1.0e-6f ? NkVec2f(somme.x / l, somme.y / l) : NkVec2f(0.f, 0.f);
						float32 dvx = 0.f;
						bool sauter = false;
						Decider(c.reglages, c.etat, actions, m, dt, dvx, sauter);
						b->linearVelocity.x += dvx;
						if (sauter) {
							b->linearVelocity.y = c.reglages.vitesseSaut + (m.vitesseSol.y > 0.f ? m.vitesseSol.y : 0.f);
						}
						if (dvx != 0.f || sauter) {
							b->flags &= ~static_cast<uint32>(physics::NK_BODY_SLEEPING);
							b->sleepTimer = 0.f;
						}
					});
			}

			// --- Corps mous -------------------------------------------------
			physics::NkParticules2D *p = scene.Particules();
			if (p == nullptr) {
				return;
			}
			scene.Monde().Query<NkControleMou2D, NkCorpsMou2D>().ForEach([&](ecs::NkEntityId, NkControleMou2D &c, NkCorpsMou2D &mou) {
				if (!c.reglages.actif) {
					return;
				}
				const int32 ci = p->IndexCorps(mou.corpsId);
				if (ci < 0) {
					return;
				}
				uint32 partiePoussee = 0u;
				uint32 partieSol = 0u;
				if (!NomVide(c.partiePoussee)) {
					partiePoussee = p->TrouverPartie(mou.corpsId, c.partiePoussee);
				}
				if (!NomVide(c.partieSol)) {
					partieSol = p->TrouverPartie(mou.corpsId, c.partieSol);
				}
				// Une partie nommee INTROUVABLE : le corps entier, et on le DIT (une
				// fois) -- une faute de frappe ne doit pas faire croire a un
				// personnage qui marche mal.
				if ((!NomVide(c.partiePoussee) && partiePoussee == 0u) || (!NomVide(c.partieSol) && partieSol == 0u)) {
					static bool dit = false;
					if (!dit) {
						dit = true;
						logger.Warn("[unkeny] NkControleMou2D : partie \"{0}\" / \"{1}\" introuvable, le corps entier la remplace",
									c.partiePoussee, c.partieSol);
					}
				}
				const physics::NkContactCorpsP2D sol = partieSol != 0u ? p->ContactPartie(partieSol, c.reglages.penteMax)
																	   : p->ContactCorps(static_cast<uint32>(ci), c.reglages.penteMax);
				NkMesureSol m;
				m.auSol = sol.Touche();
				m.normale = sol.normale;
				m.vitesse = partiePoussee != 0u ? p->VitessePartie(partiePoussee) : p->VitesseCorps(static_cast<uint32>(ci));
				float32 dvx = 0.f;
				bool sauter = false;
				Decider(c.reglages, c.etat, actions, m, dt, dvx, sauter);
				NkVec2f dv(dvx, 0.f);
				if (sauter && m.vitesse.y < c.reglages.vitesseSaut) {
					dv.y = c.reglages.vitesseSaut - m.vitesse.y;
				}
				// Le redressement d'abord : il ne touche ni la translation ni la forme.
				if (c.reglages.actif && c.redressement > 0.f) {
					p->FreinerRotation(static_cast<uint32>(ci), c.redressement);
				}
				if (dv.x == 0.f && dv.y == 0.f) {
					return;
				}
				// EN AJOUTANT : la gravite, les chocs et la forme continuent de vivre.
				if (partiePoussee != 0u) {
					p->AjouterVitessePartie(partiePoussee, dv);
				} else {
					p->AjouterVitesse(static_cast<uint32>(ci), dv);
				}
			});
		}

		uint32 NkRelierCorpsMous(NkScene &scene, ecs::NkEntityId a, ecs::NkEntityId b, float32 portee) {
			physics::NkParticules2D *p = scene.Particules();
			const NkCorpsMou2D *ma = scene.Monde().Get<NkCorpsMou2D>(a);
			const NkCorpsMou2D *mb = scene.Monde().Get<NkCorpsMou2D>(b);
			if (p == nullptr || ma == nullptr || mb == nullptr) {
				return 0u;
			}
			const int32 ia = p->IndexCorps(ma->corpsId);
			const int32 ib = p->IndexCorps(mb->corpsId);
			if (ia < 0 || ib < 0) {
				return 0u;
			}
			return p->RelierCorps(static_cast<uint32>(ia), static_cast<uint32>(ib), portee);
		}

	} // namespace unkeny
} // namespace nkentseu
