//
// NkUnkenyBancSquelette.cpp
// =============================================================================
// LE BANC DU SQUELETTE 2D (2026-10-02, R30). Lance par `UnkenyEditor --selftest`,
// `Physic2D --selftest` et `UnkenyPlayer --selftest` (le meme code des trois cotes).
//
// PRE-ENREGISTREMENT (ecrit avant le premier build) :
//   (q1)  LA PEAU : un sommet pondere 50/50 entre deux os suit la MOYENNE des deux
//         matrices (calculee ici a la main, sans le code du moteur) ; un sommet
//         sans poids suit l'entite. CONTRE-EPREUVE (q1n) : le meme sommet a 100/0
//         -> le temoin « suit la moyenne » voit ECHEC
//   (q2)  LE FICHIER : .nkskel aller-retour (os, longueurs, emplacements) ; la
//         scene JSON garde le squelette et les poids. CONTRE-EPREUVE (q2n) : un
//         .nkskel tronque est refuse
//   (q3)  JOUER : un clip d'os joue par le clip de proprietes (45 deg a mi-clip),
//         puis par l'ANIMATEUR (un etat qui designe le clip, le melange pose les
//         os). CONTRE-EPREUVE (q3n) : un clip inconnu ne bouge rien
//   (q4)  LES EMPLACEMENTS : une piste a paliers ferme la main a 0,5 s (l'autre
//         attache se montre, son ordre suit)
//   (q5)  LA CHAINE MOLLE : une echarpe de trois os, horizontale au depart, PEND
//         sous la gravite et TRAINE quand le corps file ; elle reste accrochee.
//         CONTRE-EPREUVE (q5n) : sans monde de particules, rien ne pend
//   (q6)  L'IK EN JEU : l'avant-bras va sur la poignee
//   (q7)  AUTO-POIDS (chaleur, distance) : sommes pleines, chaque bout a son os ;
//         le pinceau ajoute ; les doubles d'une couture ont les memes poids
//   (q8)  PHOTO / RESTAURER (Arreter) : plus de corps de chaine, la pose revient
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKECS/World/NkWorld.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "NKPhysics/NkParticules2D.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Anim/NkUnkenyProprietes.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Squelette/NkUnkenySquelette.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gE = 0;
			int32 gR = 0;
			constexpr float32 kPi = 3.14159265358979f;

			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}

			struct NkSceneTas {
					NkScene *s = nullptr;
					NkSceneTas(bool physique, bool particules) {
						s = memory::NkGetDefaultAllocator().New<NkScene>();
						NkSceneConfig cfg;
						cfg.physique = physique;
						cfg.particules = particules;
						s->Init(cfg);
						if (particules && s->Particules() != nullptr) {
							s->Particules()->reglages.limites.actif = false;
						}
					}
					~NkSceneTas() {
						memory::NkGetDefaultAllocator().Delete(s);
					}
					NkScene &operator*() noexcept {
						return *s;
					}
			};

			template <typename T> struct NkTas {
					T *p = nullptr;
					NkTas() {
						p = memory::NkGetDefaultAllocator().New<T>();
					}
					~NkTas() {
						memory::NkGetDefaultAllocator().Delete(p);
					}
					T &operator*() noexcept {
						return *p;
					}
					T *operator->() noexcept {
						return p;
					}
			};

			float32 Dist(const NkVec2f &a, const NkVec2f &b) {
				return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
			}

			/// Le sommet le plus proche de `p` (repos).
			uint32 SommetPres(const NkMaillage2D &m, const NkVec2f &p) {
				uint32 meilleur = 0;
				float32 d = 1e30f;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					const float32 x = Dist(m.positions[i], p);
					if (x < d) {
						d = x;
						meilleur = i;
					}
				}
				return meilleur;
			}

			/// Deux os racines A (-1,0)->(0,0) et B (0,0)->(1,0) sur un rectangle 2 x 1.
			void DeuxOs(NkSquelette2D &s, NkMaillage2D &m) {
				s = NkSquelette2D();
				NkMaillageRectangle2D(m, NkVec2f(2.f, 1.f), NkVec2f(0.5f, 0.5f), 4, 2);
				NkMaillageSansOs2D(m);
				NkSqueletteAjouterOs(s, "A", -1, NkVec2f(-1.f, 0.f), NkVec2f(0.f, 0.f));
				NkSqueletteAjouterOs(s, "B", -1, NkVec2f(0.f, 0.f), NkVec2f(1.f, 0.f));
			}

			/// Le temoin de (q1) : le sommet est-il a la moyenne attendue ?
			bool SuitLaMoyenne(const NkVec2f &vu, const NkVec2f &attendu) {
				return Dist(vu, attendu) < 1e-4f;
			}

			// =================================================================
			void BancPeau() {
				NkSceneTas st(false, false);
				NkScene &s = *st;
				const ecs::NkEntityId e = s.Creer("Peau", NkVec2f(2.f, 1.f));
				NkTas<NkMaillage2D> m;
				NkTas<NkSquelette2D> sq;
				DeuxOs(*sq, *m);
				const uint32 i = SommetPres(*m, NkVec2f(0.f, 0.5f));
				const uint32 libre = SommetPres(*m, NkVec2f(1.f, -0.5f));
				float32 t[NK_SQUELETTE2D_OS_MAX] = {};
				t[0] = 0.5f;
				t[1] = 0.5f;
				NkPoserPoidsSommet2D(*m, i, t);
				// La pose : A tourne de 30 deg autour de sa tete, B glisse de (0,3 ; 0,2).
				sq->os[0].pangle = 30.f * kPi / 180.f;
				sq->os[1].px += 0.3f;
				sq->os[1].py += 0.2f;
				s.Monde().Add<NkMaillage2D>(e, *m);
				s.Monde().Add<NkSquelette2D>(e, *sq);
				// A LA MAIN : A tourne p autour de (-1, 0) ; B le translate.
				const NkVec2f p = m->positions[i];
				const float32 c = std::cos(30.f * kPi / 180.f), sn = std::sin(30.f * kPi / 180.f);
				const NkVec2f r(p.x + 1.f, p.y);
				const NkVec2f pa(-1.f + r.x * c - r.y * sn, r.x * sn + r.y * c);
				const NkVec2f pb(p.x + 0.3f, p.y + 0.2f);
				const NkVec2f attendu(2.f + (pa.x + pb.x) * 0.5f, 1.f + (pa.y + pb.y) * 0.5f);
				const NkTransform2D *tr = s.Monde().Get<NkTransform2D>(e);
				NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
				NkMaillagePositionsMonde(s, e, *tr, *s.Monde().Get<NkMaillage2D>(e), w);
				Temoin(SuitLaMoyenne(w[i], attendu), "(q1) un sommet 50/50 entre deux os suit la MOYENNE des deux", Dist(w[i], attendu));
				Temoin(Dist(w[libre], tr->VersMonde(m->positions[libre])) < 1e-5f, "(q1) un sommet sans poids suit l'entite (comme avant R30)",
					   Dist(w[libre], tr->VersMonde(m->positions[libre])));
				// La prise au clic voit le maillage DEFORME (le meme point de branchement).
				const float32 dedans = NkDistanceMaillage2D(s, e, *tr, *s.Monde().Get<NkMaillage2D>(e), w[i] + NkVec2f(0.f, -0.02f));
				Temoin(dedans <= 0.f, "(q1) la prise au clic suit la peau (le sommet deplace est dedans)", dedans);
				// CONTRE-EPREUVE : 100 / 0 -> le temoin de la moyenne voit ECHEC.
				t[0] = 1.f;
				t[1] = 0.f;
				NkPoserPoidsSommet2D(*s.Monde().Get<NkMaillage2D>(e), i, t);
				NkMaillagePositionsMonde(s, e, *tr, *s.Monde().Get<NkMaillage2D>(e), w);
				Temoin(!SuitLaMoyenne(w[i], attendu), "(q1n) le meme sommet a 100/0 -> le temoin de la moyenne voit ECHEC", Dist(w[i], attendu));
			}

			// =================================================================
			void BancFichier() {
				NkTas<NkMaillage2D> m;
				NkTas<NkSquelette2D> sq;
				DeuxOs(*sq, *m);
				NkSqueletteAjouterOs(*sq, "C", 1, NkVec2f(1.f, 0.f), NkVec2f(1.4f, 0.3f));
				NkSqueletteAjouterEmplacement(*sq, "Main", 2, 0);
				const char *chemin = "unkeny_banc_squelette.nkskel";
				const bool ecrit = NkSauverSquelette2D(*sq, &*m, chemin);
				NkTas<NkSquelette2D> lu;
				const bool relu = ecrit && NkChargerSquelette2D(*lu, &*m, chemin);
				bool pareil = relu && lu->nbOs == sq->nbOs && lu->nbEmplacements == 1u;
				for (uint32 j = 0; pareil && j < sq->nbOs; ++j) {
					pareil = std::strcmp(lu->os[j].nom, sq->os[j].nom) == 0 && lu->os[j].parent == sq->os[j].parent &&
							 std::fabs(lu->os[j].x - sq->os[j].x) < 1e-5f && std::fabs(lu->os[j].angle - sq->os[j].angle) < 1e-5f &&
							 std::fabs(lu->os[j].longueur - sq->os[j].longueur) < 1e-5f;
				}
				pareil = pareil && lu->emplacements[0].os == 2 && lu->emplacements[0].attaches[0] == 0;
				Temoin(pareil, "(q2) .nkskel : os, parents, longueurs, emplacement font l'aller-retour", static_cast<float32>(lu->nbOs));
				// CONTRE-EPREUVE : le fichier TRONQUE est refuse.
				NkVector<uint8> octets = NkFile::ReadAllBytes(chemin);
				NkVector<uint8> moitie;
				for (uint32 k = 0; k < octets.Size() / 2u; ++k) {
					moitie.PushBack(octets[k]);
				}
				NkFile::WriteAllBytes("unkeny_banc_squelette_tronque.nkskel", moitie);
				NkTas<NkSquelette2D> rate;
				Temoin(!NkChargerSquelette2D(*rate, &*m, "unkeny_banc_squelette_tronque.nkskel"),
					   "(q2n) un .nkskel tronque est REFUSE", static_cast<float32>(moitie.Size()));
				NkFile::Delete(chemin);
				NkFile::Delete("unkeny_banc_squelette_tronque.nkskel");
				// La SCENE (JSON) garde le squelette ET les poids de son maillage.
				NkSceneTas a(false, false), b(false, false);
				const ecs::NkEntityId e = (*a).Creer("Bodofia", NkVec2f(0.f, 0.f));
				float32 t[NK_SQUELETTE2D_OS_MAX] = {};
				t[1] = 0.25f;
				t[2] = 0.75f;
				NkPoserPoidsSommet2D(*m, 3, t);
				sq->os[2].pangle = 0.5f;
				(*a).Monde().Add<NkMaillage2D>(e, *m);
				(*a).Monde().Add<NkSquelette2D>(e, *sq);
				NkString json;
				const bool sauve = NkSauverSceneJSON(*a, json);
				NkString erreur;
				const bool charge = sauve && NkChargerSceneJSON(*b, NkStringView(json.CStr(), json.Size()), nullptr, &erreur);
				ecs::NkEntityId f;
				(*b).Monde().Query<NkSquelette2D>().ForEach([&](ecs::NkEntityId id, NkSquelette2D &) { f = id; });
				const NkSquelette2D *s2 = f.IsValid() ? (*b).Monde().Get<NkSquelette2D>(f) : nullptr;
				const NkMaillage2D *m2 = f.IsValid() ? (*b).Monde().Get<NkMaillage2D>(f) : nullptr;
				const bool garde = charge && s2 != nullptr && m2 != nullptr && s2->nbOs == 3u && std::fabs(s2->os[2].pangle - 0.5f) < 1e-5f &&
								   std::fabs(NkPoidsSommet2D(*m2, 3, 2) - 0.75f) < 1e-3f && std::strcmp(s2->emplacements[0].nom, "Main") == 0;
				Temoin(garde, "(q2) la scene JSON garde le squelette (pose, emplacement) et les poids", m2 != nullptr ? NkPoidsSommet2D(*m2, 3, 2) : -1.f);
			}

			/// Un clip « leve le bras A » : 0 deg a 0 s, 90 deg a 1 s.
			anim::NkAnimationClip ClipLeve(const NkSquelette2D &s0, const char *nom) {
				NkTas<NkSquelette2D> s;
				*s = s0;
				anim::NkAnimationClip c;
				c.name = nom;
				c.loop = false;
				NkSqueletteRetourRepos(*s);
				NkSqueletteCle(*s, c, 0.f);
				s->os[0].pangle = kPi * 0.5f;
				NkSqueletteCle(*s, c, 1.f);
				c.duration = 1.f;
				return c;
			}

			// =================================================================
			void BancJouer() {
				NkTas<NkMaillage2D> m;
				NkTas<NkSquelette2D> sq;
				DeuxOs(*sq, *m);
				NkEnregistrerClipProprietes("banc_q_leve", ClipLeve(*sq, "banc_q_leve"));
				{
					NkSceneTas st(false, false);
					NkScene &s = *st;
					const ecs::NkEntityId e = s.Creer("Joueur", NkVec2f(0.f, 0.f));
					s.Monde().Add<NkMaillage2D>(e, *m);
					s.Monde().Add<NkSquelette2D>(e, *sq);
					NkClipProprietes2D cp;
					cp.boucle = false;
					cp.Jouer("banc_q_leve");
					s.Monde().Add<NkClipProprietes2D>(e, cp);
					for (int32 k = 0; k < 30; ++k) {
						s.Pas(1.f / 60.f);
					}
					const float32 a = s.Monde().Get<NkSquelette2D>(e)->os[0].pangle * 180.f / kPi;
					Temoin(std::fabs(a - 45.f) < 3.f, "(q3) Jouer : un clip d'os pose l'os a mi-clip (45 deg a 0,5 s)", a);
				}
				{
					// Par l'ANIMATEUR : un etat qui designe le clip ; le melange pose les os.
					anim::NkAnimController ctl;
					const int32 repos = ctl.base.AddEmptyState("repos");
					const int32 leve = ctl.base.AddEmptyState("leve");
					ctl.base.SetStateClipRef(leve, "banc_q_leve");
					(void)repos;
					const int32 tr = ctl.base.AddTransitionEx(repos, leve, 0.f);
					ctl.base.AddCondition(tr, "lever", anim::NkAnimStateMachine::NkCondKind::BOOL_TRUE);
					NkEnregistrerControleurAnimateur("banc_q_ctl", ctl);
					NkSceneTas st(false, false);
					NkScene &s = *st;
					const ecs::NkEntityId e = s.Creer("Anime", NkVec2f(0.f, 0.f));
					s.Monde().Add<NkMaillage2D>(e, *m);
					s.Monde().Add<NkSquelette2D>(e, *sq);
					NkAnimateur2D an = NkCreerAnimateur2D("banc_q_ctl");
					an.PoserBool("lever", true);
					s.Monde().Add<NkAnimateur2D>(e, an);
					for (int32 k = 0; k < 40; ++k) {
						s.Pas(1.f / 60.f);
					}
					const float32 a = s.Monde().Get<NkSquelette2D>(e)->os[0].pangle * 180.f / kPi;
					Temoin(a > 20.f && a < 80.f, "(q3) l'ANIMATEUR : l'etat « leve » joue le clip d'os (melange de NKAnima)", a);
				}
				{
					// CONTRE-EPREUVE : un clip inconnu ne bouge rien.
					NkSceneTas st(false, false);
					NkScene &s = *st;
					const ecs::NkEntityId e = s.Creer("Muet", NkVec2f(0.f, 0.f));
					s.Monde().Add<NkSquelette2D>(e, *sq);
					NkClipProprietes2D cp;
					cp.Jouer("banc_q_absent");
					s.Monde().Add<NkClipProprietes2D>(e, cp);
					for (int32 k = 0; k < 30; ++k) {
						s.Pas(1.f / 60.f);
					}
					const float32 a = s.Monde().Get<NkSquelette2D>(e)->os[0].pangle * 180.f / kPi;
					Temoin(!(std::fabs(a - 45.f) < 3.f), "(q3n) un clip INCONNU : le temoin des 45 deg voit ECHEC", a);
				}
			}

			// =================================================================
			void BancEmplacements() {
				NkTas<NkMaillage2D> m;
				NkTas<NkSquelette2D> sq;
				DeuxOs(*sq, *m);
				// Deux parties : la main ouverte (gauche) et la main fermee (droite).
				uint8 g[NK_MAILLAGE2D_SOMMETS_MAX] = {}, d[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				for (uint32 i = 0; i < m->nbSommets; ++i) {
					g[i] = m->positions[i].x <= 1e-4f ? 1u : 0u;
				}
				const int32 po = NkMaillageFairePartie2D(*m, g, "Ouverte");
				for (uint32 i = 0; i < m->nbSommets; ++i) {
					d[i] = m->positions[i].x >= -1e-4f && m->partieSommet[i] == 0u ? 1u : 0u;
				}
				const int32 pf = NkMaillageFairePartie2D(*m, d, "Fermee");
				const int32 e0 = NkSqueletteAjouterEmplacement(*sq, "Main", 1, po);
				sq->emplacements[e0].attaches[1] = static_cast<int8>(pf);
				sq->emplacements[e0].ordre = sq->emplacements[e0].ordreRepos = 5;
				int32 ordre[NK_MAILLAGE2D_PARTIES_MAX];
				bool vis[NK_MAILLAGE2D_PARTIES_MAX];
				NkEmplacementsParties2D(&*sq, *m, ordre, vis);
				const bool repos = po > 0 && pf > 0 && vis[po] && !vis[pf] && ordre[po] == 5;
				anim::NkAnimationClip c;
				c.name = "banc_q_main";
				c.duration = 1.f;
				anim::NkAnimationClip::NkPropertyTrack &p =
					c.AddPropertyTrack("", anim::NkSlot2DProperty("Main", false), anim::NkAnimationClip::NkPropertyKind::NK_STEP);
				p.curve.AddKey(0.f, math::NkVec4f(0.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
				p.curve.AddKey(0.5f, math::NkVec4f(1.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
				anim::NkAnimationClip::NkPropertyTrack &o =
					c.AddPropertyTrack("", anim::NkSlot2DProperty("Main", true), anim::NkAnimationClip::NkPropertyKind::NK_STEP);
				o.curve.AddKey(0.f, math::NkVec4f(-2.f, 0.f, 0.f, 0.f), anim::NkInterpMode::NK_STEP);
				NkEnregistrerClipProprietes("banc_q_main", c);
				NkSceneTas st(false, false);
				NkScene &s = *st;
				const ecs::NkEntityId e = s.Creer("Main", NkVec2f(0.f, 0.f));
				s.Monde().Add<NkMaillage2D>(e, *m);
				s.Monde().Add<NkSquelette2D>(e, *sq);
				NkClipProprietes2D cp;
				cp.boucle = false;
				cp.Jouer("banc_q_main");
				s.Monde().Add<NkClipProprietes2D>(e, cp);
				for (int32 k = 0; k < 42; ++k) {
					s.Pas(1.f / 60.f);
				}
				NkEmplacementsParties2D(s.Monde().Get<NkSquelette2D>(e), *s.Monde().Get<NkMaillage2D>(e), ordre, vis);
				Temoin(repos && !vis[po] && vis[pf] && ordre[pf] == -2,
					   "(q4) une piste a paliers ferme la main a 0,5 s (attache et ordre animes)", static_cast<float32>(ordre[pf]));
			}

			/// L'echarpe : un corps (0,0)->(0,1), trois os horizontaux depuis (0,1).
			void Echarpe(NkSquelette2D &s) {
				s = NkSquelette2D();
				NkSqueletteAjouterOs(s, "Corps", -1, NkVec2f(0.f, 0.f), NkVec2f(0.f, 1.f));
				NkSqueletteAjouterOs(s, "Echarpe1", 0, NkVec2f(0.f, 1.f), NkVec2f(0.3f, 1.f));
				NkSqueletteAjouterOs(s, "Echarpe2", 1, NkVec2f(0.3f, 1.f), NkVec2f(0.6f, 1.f));
				NkSqueletteAjouterOs(s, "Echarpe3", 2, NkVec2f(0.6f, 1.f), NkVec2f(0.9f, 1.f));
				NkSqueletteAjouterChaine(s, 1, 3);
			}

			/// Le bout de l'echarpe (monde) et sa tete.
			void BoutsEcharpe(NkScene &s, ecs::NkEntityId e, NkVec2f &tete, NkVec2f &bout, NkVec2f &cou) {
				const NkSquelette2D *sq = s.Monde().Get<NkSquelette2D>(e);
				const NkTransform2D *t = s.Monde().Get<NkTransform2D>(e);
				math::NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
				NkSqueletteMondePose(*sq, monde);
				tete = t->VersMonde(NkSqueletteTete(monde, 1));
				bout = t->VersMonde(NkSqueletteQueue(*sq, monde, 3));
				cou = t->VersMonde(NkSqueletteQueue(*sq, monde, 0));
			}

			// =================================================================
			void BancChaine() {
				NkTas<NkSquelette2D> sq;
				Echarpe(*sq);
				float32 bas = 0.f, retard = 0.f, accroche = 0.f;
				{
					NkSceneTas st(true, true);
					NkScene &s = *st;
					const ecs::NkEntityId e = s.Creer("Bodofia", NkVec2f(0.f, 2.f));
					s.Monde().Add<NkSquelette2D>(e, *sq);
					for (int32 k = 0; k < 120; ++k) {
						s.Pas(1.f / 60.f);
					}
					NkVec2f tete, bout, cou;
					BoutsEcharpe(s, e, tete, bout, cou);
					bas = tete.y - bout.y;
					Temoin(s.Monde().Get<NkSquelette2D>(e)->chaines[0].corps != 0u && bas > 0.5f,
						   "(q5) la chaine molle (XPBD) PEND sous la gravite apres 2 s (et s'y pose)", bas);
					// Le corps file vers la droite : l'echarpe TRAINE derriere.
					for (int32 k = 0; k < 20; ++k) {
						s.Monde().Get<NkTransform2D>(e)->position.x += 0.08f;
						s.Pas(1.f / 60.f);
					}
					BoutsEcharpe(s, e, tete, bout, cou);
					retard = tete.x - bout.x;
					accroche = Dist(tete, cou);
					Temoin(retard > 0.05f, "(q5) le corps file a droite : l'echarpe TRAINE derriere lui", retard);
					Temoin(accroche < 1e-3f, "(q5) elle reste ACCROCHEE au cou (sa tete = le bout du corps)", accroche);
				}
				{
					// CONTRE-EPREUVE : sans monde de particules, rien ne pend.
					NkSceneTas st(true, false);
					NkScene &s = *st;
					const ecs::NkEntityId e = s.Creer("Raide", NkVec2f(0.f, 2.f));
					s.Monde().Add<NkSquelette2D>(e, *sq);
					for (int32 k = 0; k < 120; ++k) {
						s.Pas(1.f / 60.f);
					}
					NkVec2f tete, bout, cou;
					BoutsEcharpe(s, e, tete, bout, cou);
					const float32 b = tete.y - bout.y;
					Temoin(!(b > 0.5f), "(q5n) sans particules : le temoin « pend » voit ECHEC", b);
				}
			}

			// =================================================================
			void BancIK() {
				NkSceneTas st(false, false);
				NkScene &s = *st;
				const ecs::NkEntityId e = s.Creer("Bras", NkVec2f(0.f, 0.f));
				NkTas<NkSquelette2D> sq;
				NkSqueletteAjouterOs(*sq, "Bras", -1, NkVec2f(0.f, 0.f), NkVec2f(1.f, 0.f));
				NkSqueletteAjouterOs(*sq, "AvantBras", 0, NkVec2f(1.f, 0.f), NkVec2f(2.f, 0.f));
				NkSqueletteAjouterOs(*sq, "Poignee", -1, NkVec2f(1.2f, -0.8f), NkVec2f(1.3f, -0.8f));
				sq->ik[0].os = 1;
				sq->ik[0].cible = 2;
				sq->ik[0].coudePositif = false;
				sq->nbIK = 1;
				s.Monde().Add<NkSquelette2D>(e, *sq);
				s.Pas(1.f / 60.f);
				const NkSquelette2D *r = s.Monde().Get<NkSquelette2D>(e);
				math::NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
				NkSqueletteMondePose(*r, monde);
				const float32 d = Dist(NkSqueletteQueue(*r, monde, 1), NkSqueletteTete(monde, 2));
				Temoin(d < 1e-3f, "(q6) IK en jeu : le bout de l'avant-bras va sur la poignee", d);
			}

			// =================================================================
			void BancPoids() {
				NkTas<NkMaillage2D> m;
				NkTas<NkSquelette2D> sq;
				DeuxOs(*sq, *m);
				auto Pleins = [](const NkMaillage2D &x) {
					for (uint32 i = 0; i < x.nbSommets; ++i) {
						uint32 somme = 0;
						for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
							somme += x.poids[i * NK_MAILLAGE2D_OS + k];
						}
						if (somme != NK_MAILLAGE2D_POIDS_PLEIN) {
							return false;
						}
					}
					return true;
				};
				const uint32 gauche = SommetPres(*m, NkVec2f(-1.f, 0.f)), droite = SommetPres(*m, NkVec2f(1.f, 0.f));
				NkAutoPoidsChaleur2D(*sq, *m);
				Temoin(Pleins(*m) && NkPoidsSommet2D(*m, gauche, 0) > 0.8f && NkPoidsSommet2D(*m, droite, 1) > 0.8f,
					   "(q7) auto-poids CHALEUR : sommes pleines, chaque bout a son os", NkPoidsSommet2D(*m, gauche, 0));
				NkAutoPoidsDistance2D(*sq, *m);
				Temoin(Pleins(*m) && NkPoidsSommet2D(*m, gauche, 0) > 0.8f && NkPoidsSommet2D(*m, droite, 1) > 0.8f,
					   "(q7) auto-poids DISTANCE : sommes pleines, chaque bout a son os", NkPoidsSommet2D(*m, droite, 1));
				const float32 avant = NkPoidsSommet2D(*m, gauche, 1);
				NkPeindrePoids2D(*m, 1, NkVec2f(-1.f, 0.f), 0.3f, 0.8f, false);
				Temoin(Pleins(*m) && NkPoidsSommet2D(*m, gauche, 1) > avant + 0.5f, "(q7) le PINCEAU donne de l'os B au sommet de gauche",
					   NkPoidsSommet2D(*m, gauche, 1));
				// Une couture : ses doubles ont les memes poids (elle ne s'ouvre pas en pliant).
				uint8 g[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				for (uint32 i = 0; i < m->nbSommets; ++i) {
					g[i] = m->positions[i].x <= 1e-4f ? 1u : 0u;
				}
				NkMaillageFairePartie2D(*m, g, "Gauche");
				NkAutoPoidsChaleur2D(*sq, *m);
				bool memes = true;
				uint32 doubles = 0;
				for (uint32 i = 0; i < m->nbSommets; ++i) {
					uint8 d[NK_MAILLAGE2D_SOMMETS_MAX];
					const uint32 n = NkDoublesSommet2D(*m, i, d, NK_MAILLAGE2D_SOMMETS_MAX);
					for (uint32 k = 0; k < n; ++k) {
						if (d[k] != i) {
							++doubles;
							memes = memes && std::fabs(NkPoidsSommet2D(*m, i, 0) - NkPoidsSommet2D(*m, d[k], 0)) < 1e-3f;
						}
					}
				}
				Temoin(doubles > 0u && memes, "(q7) les doubles d'une couture recoivent les MEMES poids", static_cast<float32>(doubles));
			}

			// =================================================================
			void BancPhoto() {
				NkSceneTas st(true, true);
				NkScene &s = *st;
				const ecs::NkEntityId e = s.Creer("Photo", NkVec2f(0.f, 2.f));
				NkTas<NkSquelette2D> sq;
				Echarpe(*sq);
				s.Monde().Add<NkSquelette2D>(e, *sq);
				NkTas<NkScene::NkPhoto> photo;
				s.Photographier(*photo);
				for (int32 k = 0; k < 30; ++k) {
					s.Pas(1.f / 60.f);
				}
				const bool joue = s.Monde().Get<NkSquelette2D>(e) != nullptr && s.Monde().Get<NkSquelette2D>(e)->chaines[0].corps != 0u;
				s.Restaurer(*photo);
				ecs::NkEntityId f;
				s.Monde().Query<NkSquelette2D>().ForEach([&](ecs::NkEntityId id, NkSquelette2D &) { f = id; });
				const NkSquelette2D *r = f.IsValid() ? s.Monde().Get<NkSquelette2D>(f) : nullptr;
				const bool rendu = r != nullptr && r->chaines[0].corps == 0u && std::fabs(r->os[2].pangle - sq->os[2].pangle) < 1e-6f;
				Temoin(joue && rendu, "(q8) Arreter (Restaurer) : plus de corps de chaine, la pose revient", r != nullptr ? r->os[2].pangle : -1.f);
			}
		} // namespace

		int32 NkUnkenyLancerBancSquelette() {
			gE = 0;
			gR = 0;
			std::printf("\n== BANC SQUELETTE 2D (R30) ==\n");
			BancPeau();
			BancFichier();
			BancJouer();
			BancEmplacements();
			BancChaine();
			BancIK();
			BancPoids();
			BancPhoto();
			std::printf("== SQUELETTE 2D : %d / %d ==\n", gR, gR + gE);
			return gE == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
