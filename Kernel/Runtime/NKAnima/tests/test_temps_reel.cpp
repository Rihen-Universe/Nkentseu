// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_temps_reel.cpp — LE BANC DE LA PHYSIQUE D'ANIMATION EN TEMPS
// REEL (Realtime/, 06/10/2026).
//
// Chaque critere est MESURE (la valeur est imprimee) et a SA mutation, posee
// dans le MEME binaire par la variable d'environnement NK_RT_MUTATION :
//
//   ressort qui converge sans exploser    spring_explicit
//   IK des pieds sur une marche            footik_off
//   IK des pieds dans un creux (bassin)    footik_no_pelvis
//   bascule ragdoll sans saut de pose      ragdoll_from_bind
//   vitesse gardee a la bascule            ragdoll_no_velocity
//   relevement sans saut                   getup_snap
//   equilibre (centre de masse)            balance_off
//   cout par image                         cost
//
//   NK_RT_MUTATION=footik_off ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
//   -> le critere de la marche doit ROUGIR (et lui seul, a peu pres).
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using namespace nkentseu::anim::rt;
using math::NkMat4f;
using math::NkVec3f;

namespace {

	// ── Les mutations demandees ────────────────────────────────────────────────
	uint32 MutationsDemandees() {
		const char *e = std::getenv("NK_RT_MUTATION");
		if (e == nullptr)
			return 0u;
		struct Nom {
				const char *nom;
				uint32 bit;
		};
		static const Nom kNoms[] = {
			{"spring_explicit", NK_RT_MUT_SPRING_EXPLICIT}, {"footik_no_pelvis", NK_RT_MUT_FOOTIK_NO_PELVIS},
			{"footik_off", NK_RT_MUT_FOOTIK_OFF},			 {"ragdoll_from_bind", NK_RT_MUT_RAGDOLL_FROM_BIND},
			{"ragdoll_no_velocity", NK_RT_MUT_RAGDOLL_NO_VELOCITY}, {"getup_snap", NK_RT_MUT_GETUP_SNAP},
			{"cost", NK_RT_MUT_COST},						 {"balance_off", NK_RT_MUT_BALANCE_OFF},
		};
		uint32 b = 0u;
		for (const Nom &n : kNoms)
			if (std::strstr(e, n.nom) != nullptr)
				b |= n.bit;
		return b;
	}
	struct Mutations {
			Mutations() {
				NkRtSetMutations(MutationsDemandees());
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	// ── Le sol d'essai : un plan et des boites (le « lancer de l'hote ») ──────────
	struct Boite {
			NkVec3f mini, maxi;
	};
	struct SolEssai {
			float32 planY = 0.f;
			Boite boites[8];
			uint32 n = 0;
			void Ajouter(const NkVec3f &a, const NkVec3f &b) {
				boites[n].mini = a;
				boites[n].maxi = b;
				++n;
			}
	};
	float32 Comp(const NkVec3f &v, int k) {
		return k == 0 ? v.x : (k == 1 ? v.y : v.z);
	}
	bool LancerEssai(void *u, const NkVec3f &o, const NkVec3f &d, float32 maxD, NkRtRayHit &hit) {
		const SolEssai &s = *static_cast<const SolEssai *>(u);
		float32 best = maxD;
		bool trouve = false;
		NkVec3f n = V3(0.f, 1.f, 0.f);
		if (std::fabs(d.y) > 1e-6f) {
			const float32 t = (s.planY - o.y) / d.y;
			if (t >= 0.f && t <= best) {
				best = t;
				trouve = true;
			}
		}
		for (uint32 b = 0; b < s.n; ++b) {
			float32 tn = -1e30f, tf = 1e30f;
			int axe = -1;
			bool rate = false;
			for (int k = 0; k < 3 && !rate; ++k) {
				const float32 dk = Comp(d, k), ok = Comp(o, k);
				const float32 lo = Comp(s.boites[b].mini, k), hi = Comp(s.boites[b].maxi, k);
				if (std::fabs(dk) < 1e-9f) {
					rate = ok < lo || ok > hi;
					continue;
				}
				float32 t1 = (lo - ok) / dk, t2 = (hi - ok) / dk;
				if (t1 > t2) {
					const float32 x = t1;
					t1 = t2;
					t2 = x;
				}
				if (t1 > tn) {
					tn = t1;
					axe = k;
				}
				if (t2 < tf)
					tf = t2;
			}
			if (rate || tn > tf || tn < 0.f || tn > best || axe < 0)
				continue;
			best = tn;
			trouve = true;
			const float32 sg = Comp(d, axe) > 0.f ? -1.f : 1.f;
			n = V3(axe == 0 ? sg : 0.f, axe == 1 ? sg : 0.f, axe == 2 ? sg : 0.f);
		}
		if (!trouve)
			return false;
		hit.point = Madd(o, d, best);
		hit.distance = best;
		hit.normal = n; // le plan : +Y ; une boite plus proche : la normale de sa face
		return true;
	}
	NkRtGround Sol(SolEssai &s) {
		NkRtGround g;
		g.raycast = &LancerEssai;
		g.user = &s;
		g.planeY = s.planY;
		return g;
	}

	// ── Le personnage et ses poses ──────────────────────────────────────────────
	struct Perso {
			NkRtTestHumanoid h;
			NkRtCharacter c;
			void Construire(bool tordu) {
				NkRtMakeTestHumanoid(h, tordu);
				(void)c.Setup(h.parents.Data(), h.bindLocal.Data(), h.names.Data(), (uint32)h.parents.Size());
			}
			uint32 N() const {
				return (uint32)h.parents.Size();
			}
	};
	/// Une pose fabriquee en MONDE, par rotations du haut vers le bas.
	struct Pose {
			const NkRtRig *rig = nullptr;
			const NkMat4f *bind = nullptr;
			NkVector<NkMat4f> world, local;
			void Depart(const Perso &p) {
				rig = &p.c.rig;
				bind = p.h.bindLocal.Data();
				world.Resize(p.N());
				local.Resize(p.N());
				rig->LocalToWorld(bind, world.Data());
			}
			void Deplacer(const NkVec3f &d) {
				world[0].m30 += d.x;
				world[0].m31 += d.y;
				world[0].m32 += d.z;
				rig->RefreshDescendants(0, bind, world.Data());
			}
			void Tourner(int32 j, const NkVec3f &axe, float32 angle) {
				RotateAbout(world[(uint32)j], QFromAxisAngle(Normalize(axe, V3(1.f, 0.f, 0.f)), angle),
							Origin(world[(uint32)j]));
				rig->RefreshDescendants((uint32)j, bind, world.Data());
			}
			const NkMat4f *Finir() {
				rig->WorldToLocal(world.Data(), local.Data());
				return local.Data();
			}
	};
	const NkVec3f kX = {1.f, 0.f, 0.f};

	/// La marche sur place qui avance a `v` m/s (le pas : 1 Hz).
	const NkMat4f *Marche(Pose &po, const Perso &p, float32 t, float32 v) {
		po.Depart(p);
		const float32 ph = 6.2831853f * t;
		po.Deplacer(V3(0.f, 0.015f * std::sin(2.f * ph), v * t));
		const NkRtTestHumanoid &h = p.h;
		po.Tourner(h.thighL, kX, -0.40f * std::sin(ph));
		po.Tourner(h.shinL, kX, 0.45f * (std::sin(ph + 1.2f) > 0.f ? std::sin(ph + 1.2f) : 0.f));
		po.Tourner(h.thighR, kX, 0.40f * std::sin(ph));
		po.Tourner(h.shinR, kX, 0.45f * (std::sin(ph + 1.2f + 3.14159f) > 0.f ? std::sin(ph + 1.2f + 3.14159f) : 0.f));
		po.Tourner(h.upperArmL, kX, 0.35f * std::sin(ph));
		po.Tourner(h.upperArmR, kX, -0.35f * std::sin(ph));
		return po.Finir();
	}
	const NkMat4f *Debout(Pose &po, const Perso &p, const NkVec3f &place) {
		po.Depart(p);
		po.Deplacer(place);
		return po.Finir();
	}

	bool Fini(const NkMat4f *w, uint32 n) {
		for (uint32 j = 0; j < n; ++j)
			if (!IsFinite(Origin(w[j])))
				return false;
		return true;
	}
	float32 Deg(float32 r) {
		return r * 57.29578f;
	}

} // namespace

// =============================================================================
// 1. LE RESSORT CONVERGE SANS EXPLOSER
// La tete file a 3 m/s pendant 1 s puis s'arrete net ; 3 s de repos. Deux
// reglages : le defaut a 60 i/s, et un ressort TRES raide (k = 5 000) a 15 i/s --
// celui qu'un integrateur explicite fait diverger.
// =============================================================================
TEST_CASE(NKAnima, RT1_Ressort_converge_sans_exploser) {
	Mutations mut;
	struct Cas {
			const char *nom;
			float32 k, c, dt;
	};
	const Cas kCas[2] = {{"defaut 60 i/s", 90.f, 9.f, 1.f / 60.f}, {"raide k=5000 a 15 i/s", 5000.f, 40.f, 1.f / 15.f}};
	for (const Cas &cas : kCas) {
		Perso p;
		p.Construire(false);
		p.c.useBalance = false;
		p.c.useFootIK = false;
		ASSERT_TRUE_MSG(p.c.springs.ChainCount() == 1u, "la queue de cheval est trouvee par son nom");
		p.c.springs.Params(0).stiffness = cas.k;
		p.c.springs.Params(0).damping = cas.c;
		SolEssai s;
		const NkRtGround g = Sol(s);
		Pose po;
		float32 pic = 0.f, vmax = 0.f, devFin = 0.f, devAvant = 0.f, vFin = 0.f;
		bool fini = true;
		const uint32 nIm = (uint32)std::lround(4.f / cas.dt);
		const uint32 iAvant = (uint32)std::lround(3.5f / cas.dt);
		for (uint32 i = 0; i <= nIm; ++i) {
			const float32 t = (float32)i * cas.dt;
			const float32 x = t < 1.f ? 3.f * t : 3.f;
			p.c.Update(Debout(po, p, V3(x, 0.f, 0.f)), 0.f, g, cas.dt);
			fini = fini && Fini(p.c.World(), p.N());
			const float32 dev = p.c.springs.LastMaxDeviation();
			if (t < 1.3f && dev > pic)
				pic = dev;
			if (p.c.springs.LastMaxSpeed() > vmax)
				vmax = p.c.springs.LastMaxSpeed();
			if (i == iAvant)
				devAvant = dev;
			devFin = dev;
			vFin = p.c.springs.LastMaxSpeed();
		}
		// Les longueurs des os de la queue : gardees.
		float32 pireL = 0.f;
		for (uint32 k = 1; k < 4u; ++k) {
			const uint32 j = (uint32)p.h.tail0 + k;
			const float32 l = Distance(Origin(p.c.World()[j]), Origin(p.c.World()[j - 1u]));
			const float32 l0 = Distance(Origin(p.c.BindWorld()[j]), Origin(p.c.BindWorld()[j - 1u]));
			if (std::fabs(l - l0) > pireL)
				pireL = std::fabs(l - l0);
		}
		std::printf("[RT1 %s] pic %.1f deg | fin %.3f deg (0,5 s avant : %.3f) | vitesse fin %.5f m/s, max %.2f m/s | "
					"os %.2e m\n",
					cas.nom, Deg(pic), Deg(devFin), Deg(devAvant), vFin, vmax, pireL);
		ASSERT_TRUE_MSG(fini, "aucune position non finie");
		if (cas.k < 1000.f)
			ASSERT_TRUE_MSG(Deg(pic) > 5.f, "le secondaire existe : la queue s'ecarte de plus de 5 deg en mouvement");
		ASSERT_TRUE_MSG(vmax < 20.f, "sans exploser : vitesse de pointe < 20 m/s");
		ASSERT_TRUE_MSG(std::fabs(Deg(devFin - devAvant)) < 0.2f, "converge : l'ecart ne bouge plus (< 0,2 deg en 0,5 s)");
		ASSERT_TRUE_MSG(vFin < 0.01f, "converge : la pointe est au repos (< 1 cm/s)");
		ASSERT_TRUE_MSG(pireL < 1e-4f, "les os de la queue gardent leur longueur");
	}
}

// =============================================================================
// 2. L'IK DES PIEDS SUR UNE MARCHE, ET DANS UN CREUX
// Le personnage est anime debout sur un sol plat (y = 0). Le monde : (a) le pied
// gauche sur une marche de 15 cm ; (b) le pied droit dans un creux de 12 cm --
// la jambe tendue ne l'atteint qu'avec le bassin qui descend. Deux squelettes :
// reperes de repos droits, et TORDUS (les solveurs ne supposent rien des reperes).
// =============================================================================
TEST_CASE(NKAnima, RT2_IK_des_pieds_sur_une_marche) {
	Mutations mut;
	for (int tordu = 0; tordu < 2; ++tordu) {
		for (int cas = 0; cas < 2; ++cas) {
			Perso p;
			p.Construire(tordu != 0);
			p.c.useBalance = false;
			p.c.useSprings = false;
			SolEssai s;
			float32 solG = 0.f, solD = 0.f;
			if (cas == 0) {
				s.planY = 0.f;
				s.Ajouter(V3(0.f, -1.f, -1.f), V3(1.f, 0.15f, 1.f));
				solG = 0.15f;
			} else {
				s.planY = -0.12f;
				s.Ajouter(V3(0.f, -1.f, -1.f), V3(1.f, 0.f, 1.f));
				solD = -0.12f;
			}
			const NkRtGround g = Sol(s);
			Pose po;
			for (uint32 i = 0; i < 90u; ++i)
				p.c.Update(Debout(po, p, V3(0.f, 0.f, 0.f)), 0.f, g, 1.f / 60.f);
			const NkMat4f *w = p.c.World();
			const NkRtTestHumanoid &h = p.h;
			const float32 hAnim = Origin(p.c.AnimWorld()[(uint32)h.footL]).y; // la cheville animee, sol plat
			const float32 eG = std::fabs(Origin(w[(uint32)h.footL]).y - (solG + hAnim));
			const float32 eD = std::fabs(Origin(w[(uint32)h.footR]).y - (solD + hAnim));
			// Les longueurs des jambes.
			float32 pireL = 0.f;
			const int32 os[6][2] = {{h.thighL, h.shinL}, {h.shinL, h.footL}, {h.footL, h.toeL},
									{h.thighR, h.shinR}, {h.shinR, h.footR}, {h.footR, h.toeR}};
			for (const auto &o : os) {
				const float32 l = Distance(Origin(w[(uint32)o[0]]), Origin(w[(uint32)o[1]]));
				const float32 l0 = Distance(Origin(p.c.BindWorld()[(uint32)o[0]]), Origin(p.c.BindWorld()[(uint32)o[1]]));
				if (std::fabs(l - l0) > pireL)
					pireL = std::fabs(l - l0);
			}
			// Le genou de la jambe pliee : en AVANT de la droite hanche-cheville.
			const int32 hip = cas == 0 ? h.thighL : h.thighR, knee = cas == 0 ? h.shinL : h.shinR,
						ank = cas == 0 ? h.footL : h.footR;
			const NkVec3f A = Origin(w[(uint32)hip]), B = Origin(w[(uint32)knee]), C = Origin(w[(uint32)ank]);
			const NkVec3f dir = Normalize(Sub(C, A), V3(0.f, -1.f, 0.f));
			const NkVec3f horsLigne = Sub(Sub(B, A), Scale(dir, Dot(Sub(B, A), dir)));
			const float32 avance = horsLigne.z;
			std::printf("[RT2 %s, %s] cheville G %.4f m, D %.4f m de leur sol | bassin %.3f m | jambes %.2e m | "
						"genou en avant de %.3f m\n",
						tordu ? "reperes tordus" : "reperes droits", cas == 0 ? "marche +15 cm" : "creux -12 cm", eG, eD,
						p.c.footIK.PelvisOffset(), pireL, avance);
			ASSERT_TRUE_MSG(eG < 0.01f, "le pied gauche est pose sur SON sol (a 1 cm)");
			ASSERT_TRUE_MSG(eD < 0.01f, "le pied droit est pose sur SON sol (a 1 cm)");
			ASSERT_TRUE_MSG(pireL < 1e-4f, "les os des jambes gardent leur longueur");
			ASSERT_TRUE_MSG(avance > 0.02f, "le genou plie vers l'avant (> 2 cm hors de la ligne)");
			if (cas == 1)
				ASSERT_TRUE_MSG(std::fabs(p.c.footIK.PelvisOffset() + 0.12f) < 0.01f, "le bassin descend du creux (12 cm)");
		}
	}
}

// =============================================================================
// 3. LA BASCULE RAGDOLL SANS SAUT DE POSE, ET LE RELEVEMENT
// Il marche a 1,5 m/s (60 i/s) ; a l'image 60, un choc (ou rien : « il
// s'effondre ») ; pendant le fondu l'hote continue la marche, puis joue « debout ».
// Le ragdoll tombe, se pose, se releve vers l'animation, recale.
// =============================================================================
TEST_CASE(NKAnima, RT3_Bascule_ragdoll_sans_saut_de_pose) {
	Mutations mut;
	for (int choc = 0; choc < 2; ++choc) {
		Perso p;
		p.Construire(false);
		SolEssai s;
		const NkRtGround g = Sol(s);
		Pose po;
		const float32 dt = 1.f / 60.f;
		const uint32 n = p.N();
		NkVector<NkVec3f> avant, avant2;
		avant.Resize(n);
		avant2.Resize(n);
		// Un SAUT : le deplacement d'une image, compare au plus grand des 10 images
		// d'avant (le mouvement en cours, queue de cheval comprise).
		float32 recents[10] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		float32 animMax = 0.f, sautChoc = 0.f, accelHorizMax = 0.f, sautDebut = 0.f, sautFin = 0.f;
		float32 refDebut = 0.f, refFin = 0.f;
		uint32 finRestant = 0; // la fin du relevement : l'image de bascule et les deux suivantes
		float32 bassinMin = 9.f, solMin = 9.f, etirement = 0.f;
		bool releve = false, debutVu = false, finVu = false, fini = true;
		float32 tMarche = 0.f;
		NkVec3f placeDebout = V3(0.f, 0.f, 0.f);
		NkRagdollPhase phaseAvant = NkRagdollPhase::Animated;
		NkVec3f bassinRepos = V3(0.f, 0.f, 0.f);
		for (uint32 i = 0; i < 60u * 9u; ++i) {
			if (i == 60u) {
				const NkVec3f dv = choc ? V3(0.f, 0.8f, -3.5f) : V3(0.f, 0.f, 0.f);
				p.c.Hit((uint32)p.h.chest, dv);
			}
			const NkRagdollPhase ph = p.c.ragdoll.Phase();
			const NkMat4f *anim;
			if (ph == NkRagdollPhase::Animated && i < 60u) {
				tMarche = (float32)i * dt;
				anim = Marche(po, p, tMarche, 1.5f);
			} else if (ph == NkRagdollPhase::Falling) {
				tMarche += dt;
				anim = Marche(po, p, tMarche, 1.5f);
				placeDebout = V3(0.f, 0.f, 1.5f * tMarche);
			} else {
				anim = Debout(po, p, placeDebout);
			}
			p.c.Update(anim, 0.f, g, dt);
			const NkMat4f *w = p.c.World();
			fini = fini && Fini(w, n);
			const NkRagdollPhase apres = p.c.ragdoll.Phase();
			float32 depl = 0.f;
			for (uint32 j = 0; j < n && i > 0u; ++j) {
				const float32 d = Distance(Origin(w[j]), avant[j]);
				if (d > depl)
					depl = d;
			}
			float32 recent = 0.f;
			for (float32 r : recents)
				recent = r > recent ? r : recent;
			recents[i % 10u] = depl;
			if (i == 60u) {
				sautChoc = depl;
				animMax = recent;
			}
			// L'acceleration HORIZONTALE du bassin autour de la bascule (vitesse gardee ?).
			if (i >= 58u && i <= 72u) {
				const NkVec3f x = Origin(w[0]);
				const NkVec3f a = Add(Sub(x, Scale(avant[0], 2.f)), avant2[0]);
				const float32 ah = std::sqrt(a.x * a.x + a.z * a.z);
				if (ah > accelHorizMax)
					accelHorizMax = ah;
			}
			if (apres == NkRagdollPhase::Ragdoll || apres == NkRagdollPhase::Falling) {
				if (Origin(w[0]).y < bassinMin)
					bassinMin = Origin(w[0]).y;
				for (uint32 j = 0; j < n; ++j)
					if (Origin(w[j]).y < solMin)
						solMin = Origin(w[j]).y;
				if (p.c.ragdoll.ragdoll.MaxBoneStretch() > etirement)
					etirement = p.c.ragdoll.ragdoll.MaxBoneStretch();
				bassinRepos = Origin(w[0]);
			}
			if (apres == NkRagdollPhase::GettingUp && phaseAvant == NkRagdollPhase::Ragdoll) {
				sautDebut = depl;
				refDebut = recent;
				debutVu = true;
			}
			if (finRestant > 0u) {
				sautFin = depl > sautFin ? depl : sautFin;
				--finRestant;
			}
			if (apres == NkRagdollPhase::Animated && phaseAvant == NkRagdollPhase::GettingUp) {
				sautFin = depl;
				refFin = recent;
				finRestant = 2u;
				finVu = true;
				releve = true;
			}
			phaseAvant = apres;
			for (uint32 j = 0; j < n; ++j) {
				avant2[j] = avant[j];
				avant[j] = Origin(w[j]);
			}
		}
		const NkVec3f bassinFin = avant[0];
		const float32 recalage = std::sqrt((bassinFin.x - bassinRepos.x) * (bassinFin.x - bassinRepos.x) +
										   (bassinFin.z - bassinRepos.z) * (bassinFin.z - bassinRepos.z));
		std::printf("[RT3 %s] saut a la bascule %.4f m (marche : %.4f m/image) | accel. horiz. bassin max %.4f m/image^2 | "
					"bassin min %.3f m | sol min %.3f m | etirement %.2f %% | releve %s, saut debut %.4f m (avant : %.4f), fin %.4f m (avant : %.4f) | "
					"bassin final y %.3f, a %.3f m du bassin couche\n",
					choc ? "choc" : "effondrement", sautChoc, animMax, accelHorizMax, bassinMin, solMin, 100.f * etirement,
					releve ? "oui" : "NON", sautDebut, refDebut, sautFin, refFin, bassinFin.y, recalage);
		ASSERT_TRUE_MSG(fini, "aucune position non finie");
		ASSERT_TRUE_MSG(sautChoc <= 1.5f * animMax + 0.01f, "pas de saut de pose a la bascule");
		if (!choc)
			ASSERT_TRUE_MSG(accelHorizMax < 0.006f, "la vitesse de la marche est gardee a la bascule (bassin)");
		ASSERT_TRUE_MSG(bassinMin < 0.35f, "le ragdoll tombe (bassin sous 35 cm)");
		ASSERT_TRUE_MSG(solMin > -0.005f, "le ragdoll repose SUR le sol");
		ASSERT_TRUE_MSG(etirement < 0.03f, "les os du ragdoll gardent leur longueur (< 3 %)");
		ASSERT_TRUE_MSG(debutVu && sautDebut <= 1.5f * refDebut + 0.01f, "le relevement part de la pose couchee sans saut");
		ASSERT_TRUE_MSG(finVu && sautFin <= 1.5f * refFin + 0.01f, "le relevement finit sans saut");
		ASSERT_TRUE_MSG(releve && std::fabs(bassinFin.y - 0.95f) < 0.06f, "il est debout a la fin");
		ASSERT_TRUE_MSG(recalage < 0.05f, "il se releve LA OU il est tombe");
	}
}

// =============================================================================
// 4. L'EQUILIBRE : le haut du corps penche en avant, bras tendus ; le centre de
// masse sort du polygone des pieds. Le bassin recule, les pieds restent plantes.
// =============================================================================
TEST_CASE(NKAnima, RT4_Equilibre_centre_de_masse) {
	Mutations mut;
	Perso p;
	p.Construire(false);
	p.c.useSprings = false;
	SolEssai s;
	const NkRtGround g = Sol(s);
	Pose po;
	const NkRtTestHumanoid &h = p.h;
	float32 margeAvant = 0.f;
	for (uint32 i = 0; i < 90u; ++i) {
		po.Depart(p);
		po.Tourner(h.spine, kX, 1.1f);
		po.Tourner(h.chest, kX, 0.35f);
		po.Tourner(h.upperArmL, kX, -1.0f);
		po.Tourner(h.upperArmR, kX, -1.0f);
		p.c.Update(po.Finir(), 0.f, g, 1.f / 60.f);
		if (i == 0u)
			margeAvant = p.c.balance.MarginBefore();
	}
	uint32 ns = 0;
	const NkVec3f *sup = p.c.Support(ns);
	const NkVec3f com = p.c.balance.Mass().ComputeCOM(p.c.World(), (int32)p.N());
	const float32 marge = NkLiveBalance::SignedMargin(com, sup, ns);
	float32 glisse = 0.f;
	for (int32 a : {h.footL, h.footR}) {
		const NkVec3f d = Sub(Origin(p.c.World()[(uint32)a]), Origin(p.c.AnimWorld()[(uint32)a]));
		const float32 l = std::sqrt(d.x * d.x + d.z * d.z);
		if (l > glisse)
			glisse = l;
	}
	std::printf("[RT4] marge du centre de masse : avant %.3f m, apres %.3f m | bassin deplace de %.3f m | pieds %.4f m\n",
				margeAvant, marge, Length(p.c.balance.Shift()), glisse);
	ASSERT_TRUE_MSG(margeAvant < 0.f, "le cas est desequilibre au depart (sinon il ne prouve rien)");
	ASSERT_TRUE_MSG(marge >= 0.015f, "le centre de masse revient au-dessus des appuis (marge >= 1,5 cm)");
	ASSERT_TRUE_MSG(glisse < 0.005f, "les pieds restent plantes (< 5 mm)");
}

// =============================================================================
// 5. LE COUT PAR IMAGE : le personnage complet (equilibre, IK des pieds sur une
// marche, queue de cheval) qui marche ; puis le meme en ragdoll. Chaque image est
// chronometree ; on retient la MEDIANE (une machine chargee -- une compilation a
// cote -- fausse la moyenne, pas la mediane). Budget en Debug, -O0.
// =============================================================================
TEST_CASE(NKAnima, RT5_Cout_par_image) {
	Mutations mut;
	Perso p;
	p.Construire(false);
	SolEssai s;
	s.Ajouter(V3(0.f, -1.f, 2.f), V3(1.f, 0.12f, 30.f));
	const NkRtGround g = Sol(s);
	Pose po;
	const float32 dt = 1.f / 60.f;
	// Les poses de l'animation sont faites AVANT la mesure : on ne chronometre que
	// la physique.
	const uint32 kN = 600u;
	NkVector<NkVector<NkMat4f>> poses;
	poses.Resize(kN);
	for (uint32 i = 0; i < kN; ++i) {
		const NkMat4f *l = Marche(po, p, (float32)i * dt, 1.2f);
		poses[i].Resize(p.N());
		for (uint32 j = 0; j < p.N(); ++j)
			poses[i][j] = l[j];
	}
	using Horloge = std::chrono::steady_clock;
	static double marche[600], ragdoll[600];
	uint32 nm = 0, nr = 0;
	for (uint32 i = 0; i < kN; ++i) {
		const auto t0 = Horloge::now();
		p.c.Update(poses[i].Data(), 0.f, g, dt);
		marche[nm++] = std::chrono::duration<double, std::micro>(Horloge::now() - t0).count();
	}
	p.c.Hit((uint32)p.h.chest, V3(0.f, 0.5f, -3.f));
	for (uint32 i = 0; i < kN; ++i) {
		const NkRagdollPhase ph = p.c.ragdoll.Phase();
		const auto t0 = Horloge::now();
		p.c.Update(poses[kN - 1u].Data(), 0.f, g, dt);
		const double us = std::chrono::duration<double, std::micro>(Horloge::now() - t0).count();
		if (ph == NkRagdollPhase::Falling || ph == NkRagdollPhase::Ragdoll)
			ragdoll[nr++] = us;
	}
	auto Mediane = [](double *v, uint32 n) {
		if (n == 0u)
			return 0.0;
		for (uint32 i = 1; i < n; ++i) { // tri par insertion (600 valeurs au plus)
			const double x = v[i];
			uint32 k = i;
			while (k > 0u && v[k - 1u] > x) {
				v[k] = v[k - 1u];
				--k;
			}
			v[k] = x;
		}
		return v[n / 2u];
	};
	const double usMarche = Mediane(marche, nm), usRagdoll = Mediane(ragdoll, nr);
#if defined(NDEBUG)
	const char *const kConstruction = "Release";
#else
	const char *const kConstruction = "Debug -O0";
#endif
	std::printf("[RT5] cout par image (%u os, %s, mediane) : marche (equilibre + IK des pieds + ressorts) %.1f us | "
				"ragdoll %.1f us (%u images)\n",
				p.N(), kConstruction, usMarche, usRagdoll, nr);
	// Le budget en Debug : 150 us la marche, 300 us le ragdoll (60 i/s = 16 700 us
	// par image : cent personnages tiennent dans un tiers d'image en Release).
	ASSERT_TRUE_MSG(nr > 30u, "le ragdoll a bien ete mesure");
	ASSERT_TRUE_MSG(usMarche < 150.0, "la marche coute moins de 150 us par image (Debug)");
	ASSERT_TRUE_MSG(usRagdoll < 300.0, "le ragdoll coute moins de 300 us par image (Debug)");
}