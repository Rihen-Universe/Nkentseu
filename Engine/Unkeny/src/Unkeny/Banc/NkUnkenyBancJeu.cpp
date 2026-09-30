// =============================================================================
// NkUnkenyBancJeu.cpp — le banc du JEU : corps mous dans les contacts,
// controleurs de personnage, jalon « Gelee » (2026-09-29)
//
// Lance par NkUnkenyLancerBanc (a la fin), donc par `--selftest` de Physic2D et
// d'UnkenyEditor sans toucher a ces applications. Il imprime son propre bilan :
// celui du banc d'origine (37 temoins) reste comparable d'une version a l'autre.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (z1)  une gelee traverse une ZONE (collisionneur declencheur) : dans
//         NkScene::Contacts, UN debut (a = la zone, b = la gelee) puis UNE fin ;
//         (z1n) une zone a cote : aucun contact de zone avec la gelee
//   (z2)  une gelee tombe sur une caisse statique : un DEBUT solide qui nomme
//         les DEUX entites
//   (z3)  une gelee attachee a une caisse : apres Photographier / Restaurer,
//         l'attache designe le NOUVEL id du corps et tient (ecart <= 1 cm) ;
//         la paire en contact ne donne ni FIN ni DEBUT parasite
//   (c1)  controleur rigide, marcher : vx atteint 5 m/s a 5 %, jamais plus de
//         5 % au-dessus ; il reste au sol
//   (c2)  sauter au sol : vy >= 0,9 x vitesseSaut ; (c2n) redemande en l'air :
//         refusee (1 saut, 1 refus)
//   (c3)  coyote : 0,05 s apres avoir quitte un bord, le saut est accorde ;
//         (c3n) 0,3 s apres, refuse
//   (c4)  tampon : un saut demande juste avant d'atterrir part a
//         l'atterrissage ; (c4n) tampon nul : refuse
//   (c5)  controle en l'air : a 0, l'axe ne change pas vx en l'air (< 0,05) ;
//         a 1, si (> 1 m/s)
//   (c6)  plateforme mobile : un personnage immobile sur une plateforme
//         cinematique a 1 m/s part avec elle (vx a 10 %)
//   (c7)  controleur MOU : il marche (vx >= 0,8 x vitesseMax) SANS decoller
//         (la gravite n'est pas annulee) ; saute au sol ; refuse en l'air
//   (c8)  controleur mou a PARTIES : « pieds » disent le sol, le corps saute
//   (w1)  les deux controleurs vont et reviennent par .nkscene, reglage par
//         reglage ; (w1n) une scene ecrite AVANT eux (texte fige ici) se relit
//         telle quelle, sans controleur
//   (w2)  la photo garde l'ETAT du controleur (sauts, saut en cours)
//   (w3)  parties et attaches vont et reviennent par .nkscene ; l'attache est
//         rebranchee sur le corps REFAIT
//   (G1)  LE JALON « GELEE » : un heros mou, pilote par des actions posees par le
//         banc (droite tenue, saut devant chaque trou, et des sauts demandes EN
//         L'AIR expres), traverse trois plates-formes et deux trous et atteint
//         la zone de fin en moins de 12 s ;
//         (G1b) il ne saute jamais en l'air (controle INDEPENDANT du
//         controleur : au moment de chaque saut accorde, la physique dit qu'il
//         touchait un sol, ou l'avait quitte depuis moins que le coyote) ;
//         (G1c) il ne traverse jamais le sol (aucune particule a plus de 5 cm
//         dans une plate-forme ; il ne tombe jamais sous y = -1) ;
//         (G1n) sans les sauts, il tombe dans le premier trou
//   (G2)  le meme niveau avec un heros RIGIDE : il arrive aussi
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"
#include "Unkeny/Jeu/NkUnkenyNiveauGelee.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

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

			// Les actions du jeu de ce banc : les actions STANDARD (celles de l'editeur
			// et du joueur autonome, Entree/NkUnkenyActionsStandard.h).
			enum : int32 { AXE_X = NK_ACTION_AVANCER, SAUTER = NK_ACTION_SAUTER };

			void Scene(NkScene &s) {
				NkSceneConfig cfg;
				cfg.physique = true;
				cfg.particules = true;
				s.Init(cfg);
			}

			ecs::NkEntityId Boite(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &demi, NkTypeCorps type = NkTypeCorps::NK_STATIQUE,
								  bool zone = false) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				NkCollisionneur2D col;
				col.demiTaille = demi;
				col.declencheur = zone;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D k;
				k.type = type;
				s.AjouterCorps(e, k);
				return e;
			}

			ecs::NkEntityId Gelee(NkScene &s, const NkVec2f &c) {
				const int32 ci = physics::NkCreerGeleeP2D(*s.Particules(), c, 5, 5, 0.13f);
				return s.CreerCorpsMou("Gelee", ci, 0x7CE64CFFu);
			}

			ecs::NkEntityId Rigide(NkScene &s, const NkVec2f &c) {
				const ecs::NkEntityId e = s.Creer("Heros", c);
				NkCollisionneur2D col;
				col.forme = NkForme2D::NK_BOITE;
				col.demiTaille = NkVec2f(0.25f, 0.4f);
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D k;
				k.masse = 60.f;
				k.rotationBloquee = true;
				k.friction = 0.f; // le controleur freine ; un frottement au sol le ferait trebucher
				s.AjouterCorps(e, k);
				return e;
			}

			physics::NkRigidBody *Corps(NkScene &s, ecs::NkEntityId e) {
				const NkCorps2D *k = s.Monde().Get<NkCorps2D>(e);
				return k != nullptr ? s.MondePhysique()->GetBody(k->corpsId) : nullptr;
			}

			/// Une trame de jeu : les actions de la trame, puis la scene.
			void Trame(NkScene &s, NkActions &a, float32 axe, bool sauter) {
				a.NouvelleTrame();
				a.Poser(AXE_X, axe);
				a.Poser(SAUTER, sauter ? 1.f : 0.f);
				s.Pas(1.f / 60.f);
			}

			int32 CompterContacts(const NkScene &s, ecs::NkEntityId a, ecs::NkEntityId b, NkPhaseContact phase, bool zone) {
				int32 n = 0;
				for (uint32 i = 0; i < s.Contacts().Size(); ++i) {
					const NkContact2D &c = s.Contacts()[i];
					if (c.phase != phase || c.declencheur != zone) {
						continue;
					}
					const bool paire = zone ? (c.a == a && c.b == b) : ((c.a == a && c.b == b) || (c.a == b && c.b == a));
					n += paire ? 1 : 0;
				}
				return n;
			}

			NkVec2f Centre(NkScene &s, ecs::NkEntityId e) {
				const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(e);
				const int32 ci = m != nullptr ? s.Particules()->IndexCorps(m->corpsId) : -1;
				return ci >= 0 ? s.Particules()->CentreCorps(static_cast<uint32>(ci)) : NkVec2f(-99.f, -99.f);
			}

			int32 Index(NkScene &s, ecs::NkEntityId e) {
				const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(e);
				return m != nullptr ? s.Particules()->IndexCorps(m->corpsId) : -1;
			}

			// --- (z) les corps mous dans les contacts de la scene --------------
			void TemoinsContacts() {
				for (int32 k = 0; k < 2; ++k) {
					const bool traverse = k == 0;
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId zone = Boite(s, "Fin", NkVec2f(traverse ? 0.f : 4.f, 2.f), NkVec2f(1.f, 0.3f), NkTypeCorps::NK_STATIQUE, true);
					const ecs::NkEntityId g = Gelee(s, NkVec2f(0.f, 4.f));
					int32 debuts = 0;
					int32 fins = 0;
					int32 zonesMou = 0;
					for (int32 i = 0; i < 150; ++i) {
						s.Pas(1.f / 60.f);
						debuts += CompterContacts(s, zone, g, NkPhaseContact::NK_DEBUT, true);
						fins += CompterContacts(s, zone, g, NkPhaseContact::NK_FIN, true);
						for (uint32 c = 0; c < s.Contacts().Size(); ++c) {
							zonesMou += (s.Contacts()[c].declencheur && s.Contacts()[c].b == g) ? 1 : 0;
						}
					}
					if (traverse) {
						Temoin(debuts == 1 && fins == 1, "(z1) gelee dans une zone : UN debut (a = zone) puis UNE fin", static_cast<float32>(debuts));
					} else {
						Temoin(zonesMou == 0, "(z1n) zone a cote : aucun contact de zone avec la gelee", static_cast<float32>(zonesMou));
					}
				}
				{
					NkScene s;
					Scene(s);
					const ecs::NkEntityId caisse = Boite(s, "Caisse", NkVec2f(0.f, 0.5f), NkVec2f(1.5f, 0.5f));
					const ecs::NkEntityId g = Gelee(s, NkVec2f(0.f, 2.f));
					int32 debuts = 0;
					for (int32 i = 0; i < 90; ++i) {
						s.Pas(1.f / 60.f);
						debuts += CompterContacts(s, g, caisse, NkPhaseContact::NK_DEBUT, false);
					}
					Temoin(debuts >= 1, "(z2) gelee sur une caisse : DEBUT solide, les deux entites", static_cast<float32>(debuts));
				}
				{
					// ⚠️ Premiere version : la gelee attachee par UNE particule au flanc
					// de la caisse, et posee au sol a cote -- ecart 17 cm apres la photo :
					// 25 kg de gelee pendus a une particule, qui tirait la caisse de
					// 10 kg ; l'ecart ne disait rien de la photo. Ici la gelee est POSEE
					// sur la caisse, attachee par le bas, et c'est la caisse qu'on pousse
					// apres la photo : la gelee doit suivre.
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId caisse = Boite(s, "Caisse", NkVec2f(0.f, 0.3f), NkVec2f(0.6f, 0.3f), NkTypeCorps::NK_DYNAMIQUE);
					s.Monde().Get<NkCorps2D>(caisse)->masse = 40.f;
					s.Monde().Get<NkCorps2D>(caisse)->friction = 0.f;
					s.ActualiserCorps(caisse);
					const ecs::NkEntityId g = Gelee(s, NkVec2f(0.f, 0.6f + 0.26f + 0.07f));
					for (int32 i = 0; i < 30; ++i) {
						s.Pas(1.f / 60.f); // qu'elle se pose sur la caisse
					}
					const int32 ci = Index(s, g);
					const physics::NkBodyId ancien = s.Monde().Get<NkCorps2D>(caisse)->corpsId;
					const uint32 n = s.Particules()->AttacherZone(static_cast<uint32>(ci), NkVec2f(0.f, 0.67f), 0.2f, *s.MondePhysique(), ancien);
					for (int32 i = 0; i < 30; ++i) {
						s.Pas(1.f / 60.f);
					}
					NkScene::NkPhoto photo;
					s.Photographier(photo);
					s.Restaurer(photo);
					NkVector<ecs::NkEntityId> ids;
					s.Entites(ids);
					uint32 nouveau = 0;
					ecs::NkEntityId caisse2 = ecs::NkEntityId::Invalid();
					ecs::NkEntityId g2 = ecs::NkEntityId::Invalid();
					for (uint32 i = 0; i < ids.Size(); ++i) {
						const NkCorps2D *k = s.Monde().Get<NkCorps2D>(ids[i]);
						if (k != nullptr && k->type == NkTypeCorps::NK_DYNAMIQUE) {
							nouveau = k->corpsId;
							caisse2 = ids[i];
						}
						g2 = s.Monde().Has<NkCorpsMou2D>(ids[i]) ? ids[i] : g2;
					}
					bool rebranchee = s.Particules()->attaches.Size() == n && n > 0u;
					for (uint32 i = 0; i < s.Particules()->attaches.Size(); ++i) {
						rebranchee = rebranchee && s.Particules()->attaches[i].rigide == nouveau;
					}
					const float32 x0 = Centre(s, g2).x;
					int32 parasites = 0;
					float32 ecart = 0.f;
					for (int32 i = 0; i < 40; ++i) {
						s.PoserVitesse(caisse2, NkVec2f(1.f, 0.f));
						s.Pas(1.f / 60.f);
						// La gelee est posee sur la caisse : cette paire DURE, la photo ne
						// doit pas la faire finir.
						parasites += CompterContacts(s, g2, caisse2, NkPhaseContact::NK_FIN, false);
						for (uint32 a = 0; a < s.Particules()->attaches.Size(); ++a) {
							ecart = math::NkMax(ecart, s.Particules()->attaches[a].ecart);
						}
					}
					const float32 suit = Centre(s, g2).x - x0;
					Temoin(rebranchee && nouveau != ancien && ecart <= 0.01f && suit > 0.4f,
						   "(z3) apres la photo, l'attache suit le NOUVEL id et entraine la gelee (ecart m)", ecart);
					Temoin(parasites == 0, "(z3) et aucune FIN parasite de la paire en cours", static_cast<float32>(parasites));
				}
			}

			// --- (c) les controleurs -------------------------------------------
			void TemoinsRigide() {
				// (c1) marcher ; (c2) sauter ; (c2n) en l'air
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(40.f, 0.5f));
					const ecs::NkEntityId h = Rigide(s, NkVec2f(-10.f, 0.4f));
					s.Monde().Add<NkControleRigide2D>(h, NkControleRigide2D{});
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 0.f, false); // se poser
					}
					float32 vMax = 0.f;
					for (int32 i = 0; i < 90; ++i) {
						Trame(s, a, 1.f, false);
						vMax = math::NkMax(vMax, Corps(s, h)->linearVelocity.x);
					}
					const physics::NkRigidBody *b = Corps(s, h);
					const NkControleRigide2D *c = s.Monde().Get<NkControleRigide2D>(h);
					Temoin(Absf(b->linearVelocity.x - 5.f) < 0.25f && vMax <= 5.25f && c->etat.auSol && Absf(b->position.y - 0.4f) < 0.05f,
						   "(c1) rigide : marche a 5 m/s (5 %), sans depasser, au sol (vx)", b->linearVelocity.x);
					Trame(s, a, 1.f, true);
					Trame(s, a, 1.f, true);
					const float32 vy = Corps(s, h)->linearVelocity.y;
					Temoin(vy >= 0.9f * 6.f && c->etat.sauts == 1u, "(c2) rigide : saut au sol (vy m/s)", vy);
					for (int32 i = 0; i < 18; ++i) {
						Trame(s, a, 1.f, false);
					}
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 1.f, i < 3); // redemande EN L'AIR, puis on attend la fin du tampon
					}
					c = s.Monde().Get<NkControleRigide2D>(h);
					Temoin(c->etat.sauts == 1u && c->etat.sautsRefuses == 1u, "(c2n) rigide : redemande en l'air refusee (refus)",
						   static_cast<float32>(c->etat.sautsRefuses));
				}
				// (c3) (c3n) coyote : on marche vers le bord et on saute N pas apres
				// l'avoir quitte.
				for (int32 k = 0; k < 2; ++k) {
					const int32 attente = k == 0 ? 3 : 18;
					NkScene s;
					Scene(s);
					Boite(s, "Bord", NkVec2f(-5.f, -0.5f), NkVec2f(5.f, 0.5f)); // s'arrete en x = 0
					const ecs::NkEntityId h = Rigide(s, NkVec2f(-1.f, 0.4f));
					s.Monde().Add<NkControleRigide2D>(h, NkControleRigide2D{});
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 0.f, false);
					}
					int32 depuis = -1;
					for (int32 i = 0; i < 120 && depuis < attente; ++i) {
						Trame(s, a, 0.5f, false);
						const NkControleRigide2D *c = s.Monde().Get<NkControleRigide2D>(h);
						if (depuis >= 0 || !c->etat.auSol) {
							++depuis;
						}
					}
					Trame(s, a, 0.5f, true);
					for (int32 i = 0; i < 12; ++i) {
						Trame(s, a, 0.5f, false);
					}
					const NkControleRigide2D *c = s.Monde().Get<NkControleRigide2D>(h);
					if (k == 0) {
						Temoin(c->etat.sauts == 1u, "(c3) coyote : 0,05 s apres le bord, saut accorde (sauts)", static_cast<float32>(c->etat.sauts));
					} else {
						Temoin(c->etat.sauts == 0u && c->etat.sautsRefuses == 1u, "(c3n) 0,3 s apres le bord : refuse (refus)",
							   static_cast<float32>(c->etat.sautsRefuses));
					}
				}
				// (c4) (c4n) tampon : demande juste avant d'atterrir.
				for (int32 k = 0; k < 2; ++k) {
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId h = Rigide(s, NkVec2f(0.f, 2.f));
					NkControleRigide2D ctl;
					ctl.reglages.tamponSaut = k == 0 ? 0.15f : 0.f;
					s.Monde().Add<NkControleRigide2D>(h, ctl);
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					bool demande = false;
					for (int32 i = 0; i < 90; ++i) {
						const physics::NkRigidBody *b = Corps(s, h);
						// A 12 cm du sol en tombant : la demande part AVANT le contact.
						const bool maintenant = !demande && b->linearVelocity.y < -1.f && b->position.y - 0.4f < 0.12f;
						demande = demande || maintenant;
						Trame(s, a, 0.f, maintenant);
					}
					const NkControleRigide2D *c = s.Monde().Get<NkControleRigide2D>(h);
					if (k == 0) {
						Temoin(demande && c->etat.sauts == 1u, "(c4) tampon : la demande d'avant l'atterrissage part au sol (sauts)",
							   static_cast<float32>(c->etat.sauts));
					} else {
						Temoin(demande && c->etat.sauts == 0u && c->etat.sautsRefuses == 1u, "(c4n) tampon nul : refusee (refus)",
							   static_cast<float32>(c->etat.sautsRefuses));
					}
				}
				// (c5) controle en l'air
				for (int32 k = 0; k < 2; ++k) {
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId h = Rigide(s, NkVec2f(0.f, 0.4f));
					NkControleRigide2D ctl;
					ctl.reglages.controleAir = k == 0 ? 0.f : 1.f;
					s.Monde().Add<NkControleRigide2D>(h, ctl);
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 0.f, false);
					}
					Trame(s, a, 0.f, true);
					for (int32 i = 0; i < 5; ++i) {
						Trame(s, a, 0.f, false);
					}
					for (int32 i = 0; i < 18; ++i) {
						Trame(s, a, 1.f, false);
					}
					const float32 vx = Corps(s, h)->linearVelocity.x;
					if (k == 0) {
						Temoin(Absf(vx) < 0.05f, "(c5) controle en l'air a 0 : l'axe ne change rien (vx)", vx);
					} else {
						Temoin(vx > 1.f, "(c5) controle en l'air a 1 : il tourne (vx)", vx);
					}
				}
				// (c6) plateforme mobile
				{
					NkScene s;
					Scene(s);
					const ecs::NkEntityId pf = Boite(s, "Plateforme", NkVec2f(0.f, -0.25f), NkVec2f(3.f, 0.25f), NkTypeCorps::NK_CINEMATIQUE);
					const ecs::NkEntityId h = Rigide(s, NkVec2f(0.f, 0.4f));
					s.Monde().Add<NkControleRigide2D>(h, NkControleRigide2D{});
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 0.f, false);
					}
					s.PoserVitesse(pf, NkVec2f(1.f, 0.f));
					for (int32 i = 0; i < 60; ++i) {
						Trame(s, a, 0.f, false);
					}
					const float32 vx = Corps(s, h)->linearVelocity.x;
					Temoin(Absf(vx - 1.f) < 0.1f, "(c6) plateforme mobile : le personnage part avec elle (vx)", vx);
				}
			}

			void TemoinsMou() {
				// (c7)
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(40.f, 0.5f));
					const ecs::NkEntityId g = Gelee(s, NkVec2f(-10.f, 0.4f));
					NkControleMou2D ctl;
					ctl.reglages.vitesseMax = 3.f;
					s.Monde().Add<NkControleMou2D>(g, ctl);
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 40; ++i) {
						Trame(s, a, 0.f, false);
					}
					const float32 y0 = Centre(s, g).y;
					float32 yMax = y0;
					for (int32 i = 0; i < 120; ++i) {
						Trame(s, a, 1.f, false);
						yMax = math::NkMax(yMax, Centre(s, g).y);
					}
					const int32 ci = Index(s, g);
					const float32 vx = s.Particules()->VitesseCorps(static_cast<uint32>(ci)).x;
					Temoin(vx >= 0.8f * 3.f && yMax - y0 < 0.15f, "(c7) mou : marche (vx) sans decoller -- la gravite tient", vx);
					Trame(s, a, 1.f, true);
					for (int32 i = 0; i < 15; ++i) {
						Trame(s, a, 1.f, false);
					}
					const float32 monte = Centre(s, g).y - y0;
					const NkControleMou2D *c = s.Monde().Get<NkControleMou2D>(g);
					Temoin(c->etat.sauts == 1u && monte > 0.5f, "(c7) mou : saute au sol (hauteur m)", monte);
					for (int32 i = 0; i < 12; ++i) {
						Trame(s, a, 1.f, i < 2);
					}
					c = s.Monde().Get<NkControleMou2D>(g);
					Temoin(c->etat.sauts == 1u && c->etat.sautsRefuses == 1u, "(c7) mou : redemande en l'air refusee (refus)",
						   static_cast<float32>(c->etat.sautsRefuses));
				}
				// (c8) les pieds disent le sol
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId g = Gelee(s, NkVec2f(0.f, 0.4f));
					const int32 ci = Index(s, g);
					const uint32 bas[5] = {0u, 1u, 2u, 3u, 4u};
					s.Particules()->CreerPartie(static_cast<uint32>(ci), "pieds", bas, 5u);
					NkControleMou2D ctl;
					std::snprintf(ctl.partieSol, sizeof(ctl.partieSol), "%s", "pieds");
					s.Monde().Add<NkControleMou2D>(g, ctl);
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 40; ++i) {
						Trame(s, a, 0.f, false);
					}
					const bool auSol = s.Monde().Get<NkControleMou2D>(g)->etat.auSol;
					Trame(s, a, 0.f, true);
					for (int32 i = 0; i < 5; ++i) {
						Trame(s, a, 0.f, false);
					}
					const NkControleMou2D *c = s.Monde().Get<NkControleMou2D>(g);
					Temoin(auSol && c->etat.sauts == 1u, "(c8) mou a parties : les « pieds » disent le sol, il saute", static_cast<float32>(c->etat.sauts));
				}
			}

			// --- (w) sauvegarde et photo --------------------------------------
			void TemoinsSauvegarde() {
				// (w1)
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId h = Rigide(s, NkVec2f(-3.f, 0.4f));
					NkControleRigide2D cr;
					cr.reglages.actionX = 4;
					cr.reglages.actionSauter = 7;
					cr.reglages.vitesseMax = 7.25f;
					cr.reglages.coyote = 0.125f;
					cr.reglages.controleAir = 0.3f;
					s.Monde().Add<NkControleRigide2D>(h, cr);
					const ecs::NkEntityId g = Gelee(s, NkVec2f(3.f, 0.4f));
					NkControleMou2D cm;
					cm.reglages.vitesseSaut = 4.5f;
					cm.reglages.tamponSaut = 0.2f;
					cm.reglages.actif = false;
					std::snprintf(cm.partiePoussee, sizeof(cm.partiePoussee), "%s", "tete");
					std::snprintf(cm.partieSol, sizeof(cm.partieSol), "%s", "pieds");
					s.Monde().Add<NkControleMou2D>(g, cm);
					NkString json;
					NkSauverSceneJSON(s, json, nullptr);
					NkScene t;
					NkString err;
					const bool lu = NkChargerSceneJSON(t, json.View(), nullptr, &err);
					const NkControleRigide2D *r = nullptr;
					const NkControleMou2D *m = nullptr;
					NkVector<ecs::NkEntityId> ids;
					t.Entites(ids);
					for (uint32 i = 0; i < ids.Size(); ++i) {
						r = r != nullptr ? r : t.Monde().Get<NkControleRigide2D>(ids[i]);
						m = m != nullptr ? m : t.Monde().Get<NkControleMou2D>(ids[i]);
					}
					const bool justeR = r != nullptr && r->reglages.actionX == 4 && r->reglages.actionSauter == 7 && r->reglages.vitesseMax == 7.25f &&
										r->reglages.coyote == 0.125f && r->reglages.controleAir == 0.3f;
					const bool justeM = m != nullptr && m->reglages.vitesseSaut == 4.5f && m->reglages.tamponSaut == 0.2f && !m->reglages.actif &&
										std::strcmp(m->partiePoussee, "tete") == 0 && std::strcmp(m->partieSol, "pieds") == 0;
					if (!lu) {
						std::printf("    erreur : %s\n", err.CStr());
					}
					Temoin(lu && justeR && justeM, "(w1) les deux controleurs vont et reviennent par .nkscene", 0.f);
				}
				// (w1n) une scene ecrite AVANT les controleurs, figee ici au format du
				// 29/09/2026 (NkUnkenySauvegarde.cpp au commit 07a5a050a) : un sol
				// statique et une caisse, sans "id" dans "corps", sans controleur.
				{
					const char *ancienne =
						"{\"format\": \"unkeny.scene\", \"version\": 1,"
						" \"config\": {\"physique\": true, \"particules\": false, \"gravite\": \"0 -9.81\", \"pasFixe\": 0.0166666675,"
						" \"pasMaxParTrame\": 5},"
						" \"camera\": \"0 0 32 0\","
						" \"entites\": ["
						"  {\"nom\": \"Sol\", \"transform\": \"0 -0.5 0 1 1\","
						"   \"collisionneur\": {\"forme\": 1, \"demi\": \"20 0.5\", \"rayon\": 0.5, \"decalage\": \"0 0\", \"couche\": 1,"
						"    \"masque\": 4294967295, \"declencheur\": false},"
						"   \"corps\": {\"type\": 0, \"masse\": 1, \"amortLineaire\": 0, \"amortAngulaire\": 0.05, \"echelleGravite\": 1,"
						"    \"rotationBloquee\": false, \"friction\": 0.5, \"rebond\": 0}},"
						"  {\"nom\": \"Caisse\", \"transform\": \"0 2 0 1 1\","
						"   \"collisionneur\": {\"forme\": 1, \"demi\": \"0.3 0.3\", \"rayon\": 0.5, \"decalage\": \"0 0\", \"couche\": 1,"
						"    \"masque\": 4294967295, \"declencheur\": false},"
						"   \"corps\": {\"type\": 2, \"masse\": 12, \"amortLineaire\": 0, \"amortAngulaire\": 0.05, \"echelleGravite\": 1,"
						"    \"rotationBloquee\": false, \"friction\": 0.7, \"rebond\": 0}}"
						" ]}";
					NkScene t;
					NkString err;
					const bool lu = NkChargerSceneJSON(t, ancienne, nullptr, &err);
					NkVector<ecs::NkEntityId> ids;
					t.Entites(ids);
					uint32 controles = 0;
					uint32 corps = 0;
					for (uint32 i = 0; i < ids.Size(); ++i) {
						controles += t.Monde().Has<NkControleRigide2D>(ids[i]) ? 1u : 0u;
						controles += t.Monde().Has<NkControleMou2D>(ids[i]) ? 1u : 0u;
						corps += t.Monde().Has<NkCorps2D>(ids[i]) ? 1u : 0u;
					}
					for (int32 i = 0; i < 60; ++i) {
						t.Pas(1.f / 60.f);
					}
					float32 yCaisse = -99.f;
					for (uint32 i = 0; i < ids.Size(); ++i) {
						const NkCorps2D *k = t.Monde().Get<NkCorps2D>(ids[i]);
						if (k != nullptr && k->type == NkTypeCorps::NK_DYNAMIQUE) {
							yCaisse = t.Monde().Get<NkTransform2D>(ids[i])->position.y;
						}
					}
					if (!lu) {
						std::printf("    erreur : %s\n", err.CStr());
					}
					Temoin(lu && ids.Size() == 2u && corps == 2u && controles == 0u && Absf(yCaisse - 0.3f) < 0.05f,
						   "(w1n) une scene d'AVANT se relit telle quelle (caisse posee y m)", yCaisse);
				}
				// (w2) la photo garde l'etat
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId h = Rigide(s, NkVec2f(0.f, 0.4f));
					s.Monde().Add<NkControleRigide2D>(h, NkControleRigide2D{});
					NkActions a;
					NkAjouterControleurs2D(s, &a);
					for (int32 i = 0; i < 20; ++i) {
						Trame(s, a, 0.f, false);
					}
					Trame(s, a, 0.f, true);
					Trame(s, a, 0.f, false);
					NkScene::NkPhoto photo;
					s.Photographier(photo);
					s.Restaurer(photo);
					NkVector<ecs::NkEntityId> ids;
					s.Entites(ids);
					const NkControleRigide2D *c = nullptr;
					for (uint32 i = 0; i < ids.Size(); ++i) {
						c = c != nullptr ? c : s.Monde().Get<NkControleRigide2D>(ids[i]);
					}
					Temoin(c != nullptr && c->etat.sauts == 1u && c->etat.sautEnCours, "(w2) la photo garde l'etat du controleur (sauts)",
						   c != nullptr ? static_cast<float32>(c->etat.sauts) : -1.f);
				}
				// (w3) parties et attaches par fichier
				{
					NkScene s;
					Scene(s);
					Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
					const ecs::NkEntityId caisse = Boite(s, "Caisse", NkVec2f(0.f, 0.3f), NkVec2f(0.4f, 0.3f), NkTypeCorps::NK_DYNAMIQUE);
					// ⚠️ La caisse est REFAITE ici (son id passe de 2 a 3) : sans cela, la
					// scene relue redonnait le meme id 2 par hasard, et le temoin restait
					// vert meme sans rebranchement (mesure par contre-epreuve).
					s.ActualiserCorps(caisse);
					const ecs::NkEntityId g = Gelee(s, NkVec2f(0.72f, 0.4f));
					const int32 ci = Index(s, g);
					const uint32 bas[5] = {0u, 1u, 2u, 3u, 4u};
					s.Particules()->CreerPartie(static_cast<uint32>(ci), "pieds", bas, 5u);
					const uint32 n = s.Particules()->AttacherZone(static_cast<uint32>(ci), NkVec2f(0.47f, 0.4f), 0.12f, *s.MondePhysique(),
																  s.Monde().Get<NkCorps2D>(caisse)->corpsId);
					NkString json;
					NkSauverSceneJSON(s, json, nullptr);
					NkScene t;
					NkString err;
					const bool lu = NkChargerSceneJSON(t, json.View(), nullptr, &err);
					uint32 nouveau = 0;
					NkVector<ecs::NkEntityId> ids;
					t.Entites(ids);
					for (uint32 i = 0; i < ids.Size(); ++i) {
						const NkCorps2D *k = t.Monde().Get<NkCorps2D>(ids[i]);
						if (k != nullptr && k->type == NkTypeCorps::NK_DYNAMIQUE) {
							nouveau = k->corpsId;
						}
					}
					const physics::NkParticules2D *p = t.Particules();
					bool juste = lu && p != nullptr && p->attaches.Size() == n && n > 0u && p->parties.Size() == 1u &&
								 std::strcmp(p->parties[0].nom, "pieds") == 0 && p->parties[0].nombre == 5u;
					for (uint32 i = 0; juste && i < p->attaches.Size(); ++i) {
						juste = p->attaches[i].rigide == nouveau;
					}
					if (!lu) {
						std::printf("    erreur : %s\n", err.CStr());
					}
					Temoin(juste, "(w3) parties et attaches par fichier, attache rebranchee (attaches)", static_cast<float32>(n));
				}
			}

			// --- (G) le jalon « Gelee » ---------------------------------------
			// Le niveau : trois plates-formes, deux trous de 1,8 m, la seconde
			// plate-forme 0,5 m plus haut ; la zone de fin au bout. Il vit dans
			// Jeu/NkUnkenyNiveauGelee.h depuis le 30/09 : l'editeur et le joueur
			// autonome rejouent CE niveau (Espace doit y faire sauter le heros).
			using Niveau = NkNiveauGelee;

			/// Le « joueur » du banc : il ne lit QUE la position du heros (comme un
			/// joueur lit l'ecran), jamais l'etat du controleur. Il saute devant
			/// chaque trou, et appuie expres EN L'AIR au-dessus des trous.
			void Joueur(float32 x, bool avecSauts, float32 &axe, bool &sauter) {
				axe = 1.f;
				sauter = false;
				if (!avecSauts) {
					return;
				}
				const bool devantTrou1 = x > 3.3f && x < 3.7f;
				const bool devantTrou2 = x > 9.3f && x < 9.7f;
				const bool auDessusTrou = (x > 4.6f && x < 4.8f) || (x > 10.6f && x < 10.8f); // en l'air, expres
				sauter = devantTrou1 || devantTrou2 || auDessusTrou;
			}

			struct Bilan {
					bool arrive = false;
					float32 temps = 0.f;
					int32 sautsEnLAir = 0;
					float32 pireEnfoncement = 0.f;
					float32 yMin = 99.f;
					uint32 sauts = 0;
					uint32 refus = 0;
			};

			/// Touche-t-il un sol, selon la PHYSIQUE (pas le controleur) ?
			bool AuSolPhysique(NkScene &s, ecs::NkEntityId h, bool mou) {
				if (mou) {
					const int32 ci = Index(s, h);
					return ci >= 0 && s.Particules()->ContactCorps(static_cast<uint32>(ci), 0.7f).Touche();
				}
				physics::NkBodyContact cs[16];
				const uint32 n = s.MondePhysique()->BodyContacts(s.Monde().Get<NkCorps2D>(h)->corpsId, cs, 16u);
				for (uint32 i = 0; i < n; ++i) {
					if (cs[i].normal.y >= 0.7f) {
						return true;
					}
				}
				return false;
			}

			Bilan Partie(bool mou, bool avecSauts) {
				Bilan b;
				NkScene s;
				Scene(s);
				Niveau n;
				NkConstruireNiveauGelee(s, n, mou);
				const ecs::NkEntityId h = n.heros;
				NkActions a;
				NkAjouterControleurs2D(s, &a);
				float32 horsSol = 0.f;
				uint32 sautsAvant = 0;
				for (int32 i = 0; i < 12 * 60 && !b.arrive; ++i) {
					const float32 x = mou ? Centre(s, h).x : s.Monde().Get<NkTransform2D>(h)->position.x;
					float32 axe = 0.f;
					bool sauter = false;
					Joueur(x, avecSauts, axe, sauter);
					// Ce que la PHYSIQUE dit du sol AVANT la trame : c'est avec ce sol-la
					// que le controleur decide.
					const bool solAvant = AuSolPhysique(s, h, mou);
					horsSol = solAvant ? 0.f : horsSol + 1.f / 60.f;
					Trame(s, a, axe, sauter);
					const NkEtatControle2D &e = mou ? s.Monde().Get<NkControleMou2D>(h)->etat : s.Monde().Get<NkControleRigide2D>(h)->etat;
					if (e.sauts > sautsAvant) {
						// Un saut accorde : au sol, ou dans la tolerance de bord (0,1 s,
						// plus un pas de marge pour l'ordre des mesures).
						if (!solAvant && horsSol > 0.1f + 1.f / 60.f) {
							++b.sautsEnLAir;
						}
						sautsAvant = e.sauts;
					}
					// Le sol n'est jamais traverse.
					if (mou) {
						const int32 ci = Index(s, h);
						if (ci >= 0) {
							const physics::NkCorpsP2D &c = s.Particules()->corps[static_cast<uint32>(ci)];
							for (uint32 q = c.debut; q < c.debut + c.nombre; ++q) {
								const NkVec2f &pos = s.Particules()->particules[q].pos;
								b.yMin = math::NkMin(b.yMin, pos.y);
								for (int32 k = 0; k < 3; ++k) {
									if (pos.x > n.mn[k].x && pos.x < n.mx[k].x && pos.y > n.mn[k].y && pos.y < n.mx[k].y) {
										b.pireEnfoncement = math::NkMax(b.pireEnfoncement, n.mx[k].y - pos.y);
									}
								}
							}
						}
					} else {
						const NkVec2f pos = s.Monde().Get<NkTransform2D>(h)->position;
						b.yMin = math::NkMin(b.yMin, pos.y - 0.4f);
						for (int32 k = 0; k < 3; ++k) {
							if (pos.x > n.mn[k].x && pos.x < n.mx[k].x && pos.y - 0.4f < n.mx[k].y) {
								b.pireEnfoncement = math::NkMax(b.pireEnfoncement, n.mx[k].y - (pos.y - 0.4f));
							}
						}
					}
					for (uint32 c = 0; c < s.Contacts().Size(); ++c) {
						const NkContact2D &ct = s.Contacts()[c];
						if (ct.declencheur && ct.phase == NkPhaseContact::NK_DEBUT && ct.a == n.fin && ct.b == h) {
							b.arrive = true;
							b.temps = static_cast<float32>(i + 1) / 60.f;
						}
					}
					if (b.yMin < -1.f) {
						break; // tombe dans un trou : la partie est perdue
					}
				}
				const NkEtatControle2D &e = mou ? s.Monde().Get<NkControleMou2D>(h)->etat : s.Monde().Get<NkControleRigide2D>(h)->etat;
				b.sauts = e.sauts;
				b.refus = e.sautsRefuses;
				return b;
			}

			void TemoinsGelee() {
				const Bilan g = Partie(true, true);
				std::printf("    gelee : %s en %.2f s, %u sauts, %u refus, enfoncement max %.3f m, y min %.2f m\n", g.arrive ? "arrivee" : "PERDUE",
							static_cast<double>(g.temps), g.sauts, g.refus, static_cast<double>(g.pireEnfoncement), static_cast<double>(g.yMin));
				Temoin(g.arrive && g.temps < 12.f && g.sauts >= 2u, "(G1) GELEE : le heros mou atteint la fin (s)", g.temps);
				Temoin(g.sautsEnLAir == 0 && g.refus >= 1u, "(G1b) il ne saute jamais en l'air (sauts en l'air ; refus > 0)",
					   static_cast<float32>(g.sautsEnLAir));
				Temoin(g.pireEnfoncement <= 0.05f && g.yMin > -1.f, "(G1c) il ne traverse pas le sol (enfoncement max m)", g.pireEnfoncement);
				const Bilan sans = Partie(true, false);
				Temoin(!sans.arrive && sans.yMin < -1.f, "(G1n) sans sauter, il tombe dans le premier trou (y min)", sans.yMin);
				const Bilan r = Partie(false, true);
				std::printf("    rigide : %s en %.2f s, %u sauts, %u refus, %d en l air, enfoncement max %.3f m\n", r.arrive ? "arrive" : "PERDU", static_cast<double>(r.temps), r.sauts, r.refus, r.sautsEnLAir, static_cast<double>(r.pireEnfoncement));
				// ⚠️ Mesure : le rigide s'enfonce de 6,5 cm A L'ATTERRISSAGE (5 m/s en
				// chute, pas fixe 1/60 : 8 cm parcourus en un pas, repris par la
				// correction de position du solveur). C'est le solveur rigide DISCRET
				// (pas de CCD 2D, U9), pas le controleur : le seuil de G2 est « ne
				// traverse pas » (moins d'une demi-hauteur, 0,2 m), la valeur imprimee.
				Temoin(r.arrive && r.sautsEnLAir == 0 && r.pireEnfoncement < 0.2f, "(G2) le meme niveau, heros RIGIDE : il arrive (s)", r.temps);
			}
		} // namespace

		int32 NkUnkenyLancerBancJeu() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("\nUnkeny — banc du JEU (corps mous dans les contacts, controleurs, jalon Gelee)\n\n");
			TemoinsContacts();
			TemoinsRigide();
			TemoinsMou();
			TemoinsSauvegarde();
			TemoinsGelee();
			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC UNKENY (JEU) REUSSI" : "BANC UNKENY (JEU) EN ECHEC", gReussis, gEchecs,
						gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
