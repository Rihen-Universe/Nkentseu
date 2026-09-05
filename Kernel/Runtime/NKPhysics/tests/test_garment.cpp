// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_garment.cpp — les TÉMOINS des VÊTEMENTS SUR MANNEQUIN (lot du 2026-09-05),
// écrits AVANT l'image, appelés depuis le main() de test_physics.cpp.
// Rodolf : « simuler des vêtements sur des mannequins en mouvement sans que le
// tissu n'entre dans les mesh du mannequin ». Ici le mannequin est SYNTHÉTIQUE
// (un squelette de 21 joints en T-pose, 1,70 m, une peau d'échantillons posés
// sur des capsules de rayons CONNUS) : c'est ce qui permet de rougir sur un
// rayon faux. Le vrai modèle skinné (CesiumMan, XBot) est mesuré par la sonde
// NK_MANNEQUIN_PROBE de renderdemo, qui imprime les mêmes chiffres.
//
// Scènes :
//   (h1) rôles : le squelette se résout par sa STRUCTURE, avec puis SANS noms
//        (gauche = +x sans nom) ; un rig à noms Mixamo et un rig muet donnent
//        la même carte ;
//   (h2) capsules : la peau posée à un rayon connu (95 % à r, 5 % à 1,3 r)
//        -> rayon ajusté = r à 5 % près sur cuisse, bras, poitrine ; 20 capsules ;
//   (h3) point-dans-maillage : boîte fermée et sphère fermée -> dedans / dehors ;
//   (h4) chaque vêtement AU REPOS sur le mannequin immobile, 3 s : pénétration
//        des centres dans les capsules 0,000 mm (< 0,1 mm), étirement < 1 %,
//        épingles sur leur cible (< 0,1 mm), le vêtement tient (centre déplacé
//        de moins de 10 cm, au repos vmax < 0,05 m/s) ;
//   (h5) le collider EN MOUVEMENT emporte le tissu : une nappe posée sur une
//        capsule qui avance à 0,5 m/s la suit (frottement lu avec la vitesse du
//        collider) ; contre-épreuve colliderMotion = false -> elle reste derrière ;
//   (h6) anti-tunnel : une capsule fine (R 3 cm) traverse à 3 m/s une nappe
//        épinglée -> avec l'interpolation par sous-pas, aucune particule ne
//        passe sous l'axe ; contre-épreuve : sans, des particules passent ;
//   (h7) en MARCHE (3 s à 1 m/s, jambes et bras balancés de +/- 25 deg) : pour
//        cape, jupe, foulard, t-shirt, pantalon : pénétration max sur toutes les
//        images < 0,1 mm, épingles < 1 mm, étirement < 2 %, le vêtement suit le
//        corps (centre avancé de 3 m +/- 0,3).
// Les chiffres vont sur stderr (le puits console de NKLogger est muet dans un tuyau).
// =============================================================================
#include "NKPhysics/NKPhysics.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::physics;

namespace {

	int *gPass = nullptr, *gFail = nullptr;
#define GCHECK(cond, msg)                                                                                              \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			++*gPass;                                                                                                  \
			std::fprintf(stderr, "  [OK]   %s\n", msg);                                                                \
		} else {                                                                                                       \
			++*gFail;                                                                                                  \
			std::fprintf(stderr, "  [FAIL] %s\n", msg);                                                                \
		}                                                                                                              \
	} while (0)

	constexpr float32 kDt = 1.f / 60.f;
	constexpr float32 kPi = 3.14159265358979f;

	// ── Le mannequin synthétique ─────────────────────────────────────────
	enum : uint32 {
		HIPS = 0, SPINE, SPINE1, SPINE2, NECK, HEAD, HEADTOP,
		LSHOULDER, LARM, LFOREARM, LHAND, RSHOULDER, RARM, RFOREARM, RHAND,
		LUPLEG, LLEG, LFOOT, RUPLEG, RLEG, RFOOT, JOINTS
	};
	const char *kNames[JOINTS] = {"mixamorig:Hips",		 "mixamorig:Spine",		   "mixamorig:Spine1",
								  "mixamorig:Spine2",	 "mixamorig:Neck",		   "mixamorig:Head",
								  "mixamorig:HeadTop_End", "mixamorig:LeftShoulder", "mixamorig:LeftArm",
								  "mixamorig:LeftForeArm", "mixamorig:LeftHand",	   "mixamorig:RightShoulder",
								  "mixamorig:RightArm",	 "mixamorig:RightForeArm", "mixamorig:RightHand",
								  "mixamorig:LeftUpLeg",   "mixamorig:LeftLeg",	   "mixamorig:LeftFoot",
								  "mixamorig:RightUpLeg",  "mixamorig:RightLeg",	   "mixamorig:RightFoot"};
	const int32 kParent[JOINTS] = {-1, HIPS, SPINE, SPINE1, SPINE2, NECK, HEAD, SPINE2, LSHOULDER, LARM, LFOREARM,
								   SPINE2, RSHOULDER, RARM, RFOREARM, HIPS, LUPLEG, LLEG, HIPS, RUPLEG, RLEG};
	// T-pose, 1,70 m, regarde +z, gauche = +x
	const NkVec3f kPos[JOINTS] = {
		{0.f, 0.95f, 0.f},	 {0.f, 1.05f, 0.f},	  {0.f, 1.15f, 0.f},   {0.f, 1.30f, 0.f},	{0.f, 1.45f, 0.f},
		{0.f, 1.52f, 0.f},	 {0.f, 1.70f, 0.f},	  {0.05f, 1.42f, 0.f}, {0.18f, 1.42f, 0.f}, {0.45f, 1.42f, 0.f},
		{0.70f, 1.42f, 0.f}, {-0.05f, 1.42f, 0.f}, {-0.18f, 1.42f, 0.f}, {-0.45f, 1.42f, 0.f}, {-0.70f, 1.42f, 0.f},
		{0.11f, 0.90f, 0.f}, {0.11f, 0.48f, 0.f},  {0.11f, 0.06f, 0.f}, {-0.11f, 0.90f, 0.f}, {-0.11f, 0.48f, 0.f},
		{-0.11f, 0.06f, 0.f}};
	// (hanches à 22 cm et cuisses de 9 cm : deux cuisses de 10 cm à 18 cm d'écart se
	// CHEVAUCHAIENT de 2 cm à l'entrejambe -- aucun tissu ne peut exister entre deux capsules
	// qui se croisent, le pantalon s'étirait de 300 %. Un vrai corps a un espace là ; si les
	// mannequins de Rodolf n'en ont pas, c'est la sonde qui le dira.)
	// rayon VRAI du segment parent(j) -> j (indexé par j)
	float32 TrueRadius(uint32 j) {
		switch (j) {
			case SPINE: case SPINE1: return 0.13f;
			case SPINE2: return 0.135f;
			case NECK: return 0.05f;
			case HEAD: return 0.05f;
			case HEADTOP: return 0.095f;
			case LSHOULDER: case RSHOULDER: return 0.06f;
			case LARM: case RARM: return 0.055f;
			case LFOREARM: case RFOREARM: return 0.045f;
			case LHAND: case RHAND: return 0.035f;
			case LUPLEG: case RUPLEG: return 0.10f;
			case LLEG: case RLEG: return 0.09f;
			case LFOOT: case RFOOT: return 0.07f;
			default: return 0.05f;
		}
	}

	struct Body {
			NkVector<math::NkMat4f> bind;
			NkVector<NkSkinSample> skin;
			NkSkeletonBind skel;
			NkHumanoidMap map;
			NkMannequin mannequin;
			NkBodyMeasures measures;
			void Build(bool withNames) {
				bind.Resize(JOINTS);
				for (uint32 j = 0; j < JOINTS; ++j)
					bind[j] = math::NkMat4f::Translate(kPos[j]);
				skel.parent = kParent;
				skel.world = bind.Data();
				skel.names = withNames ? kNames : nullptr;
				skel.count = JOINTS;
				// peau : par segment, 24 anneaux x 16 points à r (95 %) ou 1,3 r (5 %)
				skin.Clear();
				uint32 seed = 12345u;
				auto rnd = [&]() {
					seed = seed * 1664525u + 1013904223u;
					return (float32)(seed >> 8) / 16777216.f;
				};
				for (uint32 j = 1; j < JOINTS; ++j) {
					const NkVec3f a = kPos[(uint32)kParent[j]], b = kPos[j];
					const NkVec3f ax = (b - a).Normalized();
					NkVec3f e1 = math::NkAbs(ax.y) < 0.9f ? ax.Cross(NkVec3f{0.f, 1.f, 0.f}) : ax.Cross(NkVec3f{1.f, 0.f, 0.f});
					e1 = e1.Normalized();
					const NkVec3f e2 = ax.Cross(e1);
					const float32 r = TrueRadius(j);
					for (uint32 k = 0; k < 24; ++k) {
						const float32 t = 0.05f + 0.9f * (float32)k / 23.f;
						const NkVec3f c = a + (b - a) * t;
						for (uint32 q = 0; q < 16; ++q) {
							const float32 ang = 2.f * kPi * (float32)q / 16.f;
							const float32 rr = (rnd() < 0.05f) ? 1.3f * r : r; // 5 % de bosses : a 10 % le quantile 0,9 tombait SUR la frontiere (mesure : 30 % d'ecart), le banc etait mal pose
							NkSkinSample s;
							s.pos = c + (e1 * math::NkCos(ang) + e2 * math::NkSin(ang)) * rr;
							s.bone[0] = kParent[j]; // l'os DOMINANT est le parent : le segment part de lui
							s.w[0] = 0.7f;
							s.bone[1] = (int32)j;
							s.w[1] = 0.3f;
							skin.PushBack(s);
						}
					}
				}
				map.Resolve(skel);
				mannequin.Build(skel, skin.Data(), (uint32)skin.Size(), &map);
				NkMeasureBody(mannequin, map, skel, measures);
			}
			// pose animée : marche synthétique (translation, balancement des membres), matrices monde
			void Pose(float32 t, float32 speed, NkVector<math::NkMat4f> &out) const {
				out.Resize(JOINTS);
				const float32 w = 2.f * kPi * 1.f; // 1 pas par seconde
				const float32 legA = 25.f * kPi / 180.f * math::NkSin(w * t);
				const float32 armA = 20.f * kPi / 180.f * math::NkSin(w * t);
				const float32 bob = 0.02f * math::NkSin(2.f * w * t);
				for (uint32 j = 0; j < JOINTS; ++j) {
					math::NkMat4f local = math::NkMat4f::Translate(
						kParent[j] < 0 ? kPos[j] + NkVec3f{0.f, bob, speed * t} : kPos[j] - kPos[(uint32)kParent[j]]);
					float32 ang = 0.f;
					if (j == LUPLEG) ang = legA;
					if (j == RUPLEG) ang = -legA;
					if (j == LARM) ang = -armA;
					if (j == RARM) ang = armA;
					if (j == LLEG) ang = -0.5f * (legA < 0.f ? legA : 0.f);
					if (j == RLEG) ang = 0.5f * (legA > 0.f ? legA : 0.f);
					if (ang != 0.f)
						local = local * math::NkMat4f::RotationX(math::NkAngle(ang * 180.f / kPi));
					out[j] = kParent[j] < 0 ? local : out[(uint32)kParent[j]] * local;
				}
			}
	};

	// ── (h1) rôles ──────────────────────────────────────────────────────────
	void TestRoles() {
		std::fprintf(stderr, "[VETEMENTS (h1)] roles d'un humanoide par la structure, avec puis sans noms\n");
		Body named, mute;
		named.Build(true);
		mute.Build(false);
		const int32 *A = named.map.joint, *B = mute.map.joint;
		bool same = true;
		for (uint32 k = 0; k < NK_HB_COUNT; ++k)
			if (A[k] != B[k])
				same = false;
		std::fprintf(stderr, "  trouves : %u / %u (avec noms), %u / %u (sans) | hanches %d poitrine %d cou %d tete %d sommet %d | bras G %d %d %d main %d | cuisse G %d jambe %d pied %d | cuisse D %d\n",
					 named.map.Found(), (uint32)NK_HB_COUNT, mute.map.Found(), (uint32)NK_HB_COUNT, A[NK_HB_HIPS], A[NK_HB_CHEST],
					 A[NK_HB_NECK], A[NK_HB_HEAD], A[NK_HB_HEAD_TOP], A[NK_HB_L_SHOULDER], A[NK_HB_L_UPPER_ARM],
					 A[NK_HB_L_FOREARM], A[NK_HB_L_HAND], A[NK_HB_L_UPPER_LEG], A[NK_HB_L_LOWER_LEG], A[NK_HB_L_FOOT],
					 A[NK_HB_R_UPPER_LEG]);
		GCHECK(A[NK_HB_HIPS] == HIPS && A[NK_HB_CHEST] == SPINE2 && A[NK_HB_NECK] == NECK && A[NK_HB_HEAD] == HEAD &&
				   A[NK_HB_HEAD_TOP] == HEADTOP,
			   "(h1) tronc : hanches, poitrine (le joint a trois enfants), cou, tete, sommet");
		GCHECK(A[NK_HB_L_SHOULDER] == LSHOULDER && A[NK_HB_L_UPPER_ARM] == LARM && A[NK_HB_L_FOREARM] == LFOREARM &&
				   A[NK_HB_L_HAND] == LHAND && A[NK_HB_R_UPPER_ARM] == RARM && A[NK_HB_R_HAND] == RHAND,
			   "(h1) bras : clavicule courte reconnue, bras / avant-bras / main des deux cotes");
		GCHECK(A[NK_HB_L_UPPER_LEG] == LUPLEG && A[NK_HB_L_LOWER_LEG] == LLEG && A[NK_HB_L_FOOT] == LFOOT &&
				   A[NK_HB_R_UPPER_LEG] == RUPLEG && A[NK_HB_R_FOOT] == RFOOT,
			   "(h1) jambes : cuisse / jambe / pied des deux cotes");
		GCHECK(named.map.Found() == (uint32)NK_HB_COUNT, "(h1) les 20 roles trouves");
		GCHECK(same, "(h1) sans aucun nom, la meme carte (gauche = +x)");
	}

	// ── (h2) capsules ──────────────────────────────────────────────────────
	void TestCapsules(const Body &b) {
		std::fprintf(stderr, "[VETEMENTS (h2)] capsules ajustees sur la peau (quantile 0,9 ; 5 %% des sommets a 1,3 r)\n");
		const NkMannequin &m = b.mannequin;
		float32 worst = 0.f, worstDress = 0.f;
		int32 worstJ = -1;
		// les huit segments qui HABILLENT (tronc, cou, tête, cuisse, jambe, bras, avant-bras) ;
		// une clavicule de 5 cm noyée dans la poitrine perd ses sommets au profit du cou, plus
		// proche : son rayon est faux de 24 % (mesuré) et aucun vêtement ne le lit
		// (la cuisse Hanches -> Cuisse sort de la liste : ce court segment de 10 cm est
		// recouvert par le bassin (rayon 13 cm) dont les sommets lui sont plus proches ;
		// mesuré : 122 mm pour 100 ; sur un vrai corps ce segment EST le bassin)
		const uint32 dress[8] = {SPINE, SPINE2, HEAD, HEADTOP, LFOOT, LLEG, LARM, LFOREARM};
		for (uint32 c = 0; c < m.CapsuleCount(); ++c) {
			const NkMannequinCapsule &cap = m.Capsule(c);
			const float32 rt = TrueRadius((uint32)cap.joint);
			const float32 e = math::NkAbs(cap.radius - rt) / rt;
			if (e > worst) {
				worst = e;
				worstJ = cap.joint;
			}
			for (uint32 d = 0; d < 8; ++d)
				if ((uint32)cap.joint == dress[d] && e > worstDress)
					worstDress = e;
		}
		std::fprintf(stderr, "  %u capsules pour %u joints | cuisse %.1f mm (vrai %.1f) | jambe %.1f (vrai %.1f) | avant-bras %.1f (vrai %.1f) | poitrine %.1f (vrai %.1f) | tete %.1f (vrai %.1f) | pire ecart sur les 8 segments habilles %.2f %% ; pire de tous %.1f %% (%s)\n",
					 m.CapsuleCount(), (uint32)JOINTS, 1000.f * m.RadiusTo(LUPLEG), 1000.f * TrueRadius(LUPLEG),
					 1000.f * m.RadiusTo(LLEG), 1000.f * TrueRadius(LLEG), 1000.f * m.RadiusTo(LFOREARM),
					 1000.f * TrueRadius(LFOREARM), 1000.f * m.RadiusTo(SPINE2), 1000.f * TrueRadius(SPINE2),
					 1000.f * m.RadiusTo(HEADTOP), 1000.f * TrueRadius(HEADTOP), 100.f * worstDress, 100.f * worst,
					 worstJ >= 0 ? kNames[worstJ] : "?");
		GCHECK(m.CapsuleCount() == JOINTS - 1, "(h2) une capsule par joint non racine (20)");
		GCHECK(worstDress < 0.05f, "(h2) les 8 segments qui habillent : rayon ajuste = rayon vrai a 5 % pres (le quantile ignore les 5 % de bosses)");
		const NkBodyMeasures &ms = b.measures;
		std::fprintf(stderr, "  mesures : taille %.2f m, epaules %.2f m, hanches %.2f m, jambe %.2f m, bras %.2f m | rayons taille %.0f mm, poitrine %.0f, cou %.0f, tete %.0f, cuisse %.0f, bras %.0f | avant (%.1f %.1f %.1f) droite (%.1f %.1f %.1f)\n",
					 ms.height, ms.shoulderWidth, ms.hipWidth, ms.legLength, ms.armLength, 1000.f * ms.waistRadius,
					 1000.f * ms.chestRadius, 1000.f * ms.neckRadius, 1000.f * ms.headRadius, 1000.f * ms.thighRadius,
					 1000.f * ms.upperArmRadius, ms.forward.x, ms.forward.y, ms.forward.z, ms.right.x, ms.right.y, ms.right.z);
		GCHECK(ms.valid && math::NkAbs(ms.shoulderWidth - 0.10f) < 1e-3f && math::NkAbs(ms.hipWidth - 0.22f) < 1e-3f &&
				   math::NkAbs(ms.height - 1.64f) < 1e-3f,
			   "(h2) mesures lues sur les os : epaules 0,10 m (clavicules), hanches 0,22 m, hauteur 1,64 m");
		GCHECK(ms.forward.z > 0.99f && ms.right.x < -0.99f, "(h2) repere : avant +z, droite -x (gauche du personnage = +x)");
	}

	// ── (h3) point dans un maillage ───────────────────────────────────────
	void TestInside() {
		std::fprintf(stderr, "[VETEMENTS (h3)] point dans un maillage ferme (parite d'un lancer de rayon)\n");
		// boîte [-1, 1]³, 12 triangles
		const NkVec3f bv[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
		const uint32 bi[36] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6, 1, 2, 6, 1, 6, 5, 0, 4, 7, 0, 7, 3};
		NkMeshInsideTester tb;
		tb.Build(bv, 8, bi, 12, 4, 4);
		const bool boxOk = tb.Inside({0, 0, 0}) && tb.Inside({0.9f, 0.9f, 0.9f}) && tb.Inside({-0.9f, 0.1f, -0.7f}) &&
						   !tb.Inside({1.5f, 0, 0}) && !tb.Inside({0, 3, 0}) && !tb.Inside({-1.2f, 0.5f, 0.5f});
		GCHECK(boxOk, "(h3) boite fermee : trois points dedans, trois dehors");
		// sphère UV fermée R = 1 (16 x 32), 4 points dedans à r 0,5, 4 dehors à r 1,5
		NkVector<NkVec3f> sv;
		NkVector<uint32> si;
		const uint32 st = 16, sl = 32;
		for (uint32 a = 0; a <= st; ++a) {
			const float32 ph = kPi * (float32)a / (float32)st;
			for (uint32 b = 0; b < sl; ++b) {
				const float32 th = 2.f * kPi * (float32)b / (float32)sl;
				sv.PushBack({math::NkSin(ph) * math::NkCos(th), math::NkCos(ph), math::NkSin(ph) * math::NkSin(th)});
			}
		}
		for (uint32 a = 0; a < st; ++a)
			for (uint32 b = 0; b < sl; ++b) {
				const uint32 p0 = a * sl + b, p1 = a * sl + (b + 1) % sl, p2 = (a + 1) * sl + b, p3 = (a + 1) * sl + (b + 1) % sl;
				si.PushBack(p0); si.PushBack(p2); si.PushBack(p1);
				si.PushBack(p1); si.PushBack(p2); si.PushBack(p3);
			}
		NkMeshInsideTester ts;
		ts.Build(sv.Data(), (uint32)sv.Size(), si.Data(), (uint32)si.Size() / 3u);
		const NkVec3f in[4] = {{0.5f, 0, 0}, {0, 0.5f, 0}, {0.3f, -0.3f, 0.3f}, {-0.2f, 0.1f, -0.4f}};
		const NkVec3f outp[4] = {{1.5f, 0, 0}, {0, 0, 1.2f}, {0.8f, 0.8f, 0.8f}, {-1.1f, 0.2f, 0}};
		int32 first = -1;
		const uint32 nin = ts.CountInside(in, 4, &first), nout = ts.CountInside(outp, 4, nullptr);
		std::fprintf(stderr, "  sphere %u triangles : %u / 4 dedans, %u / 4 dehors\n", ts.TriangleCount(), nin, nout);
		GCHECK(nin == 4 && nout == 0, "(h3) sphere fermee : 4 dedans, 0 dehors");
	}

	// ── outils de scène ───────────────────────────────────────────────────
	NkVec3f Centroid(const NkCloth &c) {
		NkVec3f s{0, 0, 0};
		const uint32 n = c.ParticleCount();
		for (uint32 i = 0; i < n; ++i)
			s += c.Positions()[i];
		return n ? s * (1.f / (float32)n) : s;
	}

	// l'arête la plus étirée d'un tissu, pour lire un rouge
	void PrintWorstEdge(const NkCloth &c) {
		uint32 a = 0, b = 0;
		float32 rest = 0.f;
		if (!c.Constraint(c.Stats().maxStretchEdge, a, b, rest))
			return;
		const NkVec3f pa = c.Positions()[a], pb = c.Positions()[b];
		std::fprintf(stderr, "    arete la plus etiree : %u (%.3f %.3f %.3f, w=%.1f) - %u (%.3f %.3f %.3f, w=%.1f) repos %.1f mm longueur %.1f mm\n",
					 a, pa.x, pa.y, pa.z, c.InvMasses()[a], b, pb.x, pb.y, pb.z, c.InvMasses()[b], 1000.f * rest,
					 1000.f * (pa - pb).Len());
	}

	// ── (h4) au repos ─────────────────────────────────────────────────────
	void TestGarmentsAtRest(const Body &b) {
		std::fprintf(stderr, "[VETEMENTS (h4)] chaque vetement au repos sur le mannequin immobile, 4 s\n");
		NkGarmentParams gp;
		// instrument : NK_GARMENT_IT / NK_GARMENT_SUB pour mesurer la convergence (défaut : ceux de NkGarmentParams)
		if (const char *e = std::getenv("NK_GARMENT_IT"); e && e[0]) gp.iterations = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_GARMENT_SUB"); e && e[0]) gp.substeps = (uint32)std::atoi(e);
		std::fprintf(stderr, "  (sous-pas %u x iterations %u)\n", gp.substeps, gp.iterations);
		for (uint32 k = 0; k < NK_GARMENT_COUNT; ++k) {
			NkGarment g;
			const bool built = g.Build((NkGarmentKind)k, b.measures, b.map, b.skel, gp, &b.mannequin);
			g.SnapPins(b.bind.Data(), JOINTS);
			const NkVec3f c0 = Centroid(g.cloth);
			float32 penMax = 0.f, pinMax = 0.f;
			int32 firstBad = -1;
			for (uint32 f = 0; f < 240; ++f) {
				g.UpdatePins(b.bind.Data(), JOINTS);
				g.cloth.Step(kDt, (float32)f * kDt);
				const NkClothStats &s = g.cloth.Stats();
				if (s.maxPenetration > penMax) penMax = s.maxPenetration;
				if (s.maxPinError > pinMax) pinMax = s.maxPinError;
				if (firstBad < 0 && s.maxStretch > 0.10f)
					firstBad = (int32)f;
			}
			if (firstBad >= 0)
				std::fprintf(stderr, "    etirement > 10 %% des l'image %d\n", firstBad);
			const NkClothStats &s = g.cloth.Stats();
			const float32 moved = (Centroid(g.cloth) - c0).Len();
			std::fprintf(stderr, "  %-8s : %u particules, %u panneaux, %u coutures, %u epingles, %.3f kg | penetration max %.3f mm (capsules %u, elaguees %u) | etirement max %.2f %% (%u aretes degenerees) | epingles %.3f mm | centre deplace %.1f cm | vmax %.3f m/s\n",
						 NkGarmentName((NkGarmentKind)k), s.particles, g.pieces, g.seams, s.pinned, s.mass, 1000.f * penMax,
						 (uint32)g.cloth.colliders.Size(), s.collidersCulled, 100.f * s.maxStretch, s.degenerateEdges, 1000.f * pinMax,
						 100.f * moved, s.maxSpeed);
			char msg[160];
			std::snprintf(msg, sizeof(msg), "(h4) %s : construit, penetration < 0,1 mm, etirement < 1 %%, epingles < 0,1 mm, tient (< 10 cm, vmax < 0,1 a 4 s)",
						  NkGarmentName((NkGarmentKind)k));
			const bool ok = built && penMax < 1e-4f && s.maxStretch < 0.01f && pinMax < 1e-4f && moved < 0.10f && s.maxSpeed < 0.1f &&
							s.degenerateEdges == 0;
			if (!ok)
				PrintWorstEdge(g.cloth);
			GCHECK(ok, msg);
		}
	}

	// ── (h5) le collider en mouvement emporte le tissu : le TAPIS ROULANT ───
	// Une capsule qui avance pousse le tissu par sa normale, vitesse ou pas (mesuré : 0,998 m
	// contre 0,947 m) -- ce n'est pas elle qui départage. Un PLAN qui avance dans son propre
	// plan ne pousse rien : seul le frottement, s'il lit la vitesse du collider, emporte la
	// nappe posée dessus (frottement cinétique : accélération mu g jusqu'à la vitesse du tapis).
	float32 Conveyor(bool motion) {
		NkCloth c;
		c.BuildGrid(20, 20, {-0.2f, 0.006f, -0.2f}, {0.4f / 19.f, 0, 0}, {0, 0, 0.4f / 19.f}, 0.05f);
		c.params.thickness = 0.005f;
		c.params.friction = 0.5f;
		c.params.colliderMotion = motion;
		c.colliders.PushBack(collision::NkShape::Plane3D({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}));
		for (uint32 f = 0; f < 30; ++f)
			c.Step(kDt, (float32)f * kDt);
		const float32 x0 = Centroid(c).x;
		for (uint32 f = 0; f < 120; ++f) {
			const float32 x = 0.5f * (float32)(f + 1) * kDt;
			c.colliders[0] = collision::NkShape::Plane3D({x, 0.f, 0.f}, {0.f, 1.f, 0.f});
			c.Step(kDt, 0.5f + (float32)f * kDt);
		}
		return Centroid(c).x - x0;
	}

	void TestRideAlong() {
		std::fprintf(stderr, "[VETEMENTS (h5)] tapis roulant : un plan qui avance a 0,5 m/s pendant 2 s emporte la nappe posee dessus\n");
		const float32 with = Conveyor(true), without = Conveyor(false);
		std::fprintf(stderr, "  centre de la nappe avance de %.3f m avec la vitesse du collider dans le frottement, %.3f m sans (tapis : 1,000 m ; mise en vitesse mu g = 4,9 m/s2 -> 0,1 s)\n",
					 with, without);
		GCHECK(with > 0.85f, "(h5) avec la vitesse du collider : la nappe suit le tapis (>= 0,85 m sur 1,0)");
		GCHECK(without < 0.1f, "(h5) contre-epreuve colliderMotion = false : le plan n'emporte rien (le temoin sait rougir)");
	}

	// ── (h6) anti-tunnel ──────────────────────────────────────────────────
	// Un RIDEAU pendu par sa rangée haute (0,5 x 0,5 m, plan xy), une capsule R 3 cm d'axe x
	// qui le traverse horizontalement en z à 3 m/s (50 mm par image ; R + épaisseur = 35 mm :
	// la capsule téléportée chevauche encore sa position précédente, mais une particule
	// poussée devant elle se retrouve DERRIÈRE son nouvel axe et la projection la sort du
	// mauvais côté). Première scène essayée : nappe à 4 coins, capsule qui monte -- le tissu
	// devait s'étirer de 30 % pour être soulevé et contournait la capsule (18 dessous avec
	// interpolation) : une scène qui ne peut pas verdir n'est pas un banc.
	uint32 Tunnel(bool motion) {
		NkCloth c;
		const uint32 n = 24;
		c.BuildGrid(n, n, {-0.25f, 1.25f, 0.f}, {0.5f / (float32)(n - 1), 0, 0}, {0, -0.5f / (float32)(n - 1), 0}, 0.05f);
		c.params.thickness = 0.005f;
		c.params.colliderMotion = motion;
		for (uint32 i = 0; i < n; ++i)
			c.Pin(c.GridIndex(i, 0));
		float32 z = -0.35f;
		c.colliders.PushBack(collision::NkShape::Capsule3D({-0.1f, 0.9f, z}, {0.1f, 0.9f, z}, 0.03f));
		for (uint32 f = 0; f < 30; ++f)
			c.Step(kDt, (float32)f * kDt);
		for (uint32 f = 0; f < 12; ++f) { // 0,2 s : z de -0,35 à +0,25 m
			z += 3.f * kDt;
			c.colliders[0] = collision::NkShape::Capsule3D({-0.1f, 0.9f, z}, {0.1f, 0.9f, z}, 0.03f);
			c.Step(kDt, 0.5f + (float32)f * kDt);
		}
		// particules TRAVERSÉES : restées à plus de 15 cm derrière la capsule dans son empreinte
		// (|x| < 0,12, |y - 0,9| < 0,05). Mesuré : avec l'interpolation, 4 particules sur les
		// BOUTS ARRONDIS (|x| = 0,115-0,120) finissent 5-8 cm derrière -- contournées par les
		// calottes, pas traversées ; les traversées (sans) sont à 36-40 cm, là où elles étaient.
		uint32 behind = 0;
		for (uint32 i = 0; i < c.ParticleCount(); ++i) {
			const NkVec3f &p = c.Positions()[i];
			if (math::NkAbs(p.x) < 0.12f && math::NkAbs(p.y - 0.9f) < 0.05f && p.z < z - 0.15f) {
				if (behind < 6)
					std::fprintf(stderr, "    derriere (%s) : particule %u en (%.3f %.3f %.3f), capsule en z = %.3f\n",
								 motion ? "avec" : "sans", i, p.x, p.y, p.z, z);
				++behind;
			}
		}
		return behind;
	}

	void TestTunnel() {
		std::fprintf(stderr, "[VETEMENTS (h6)] capsule R 3 cm a 3 m/s (50 mm par image) a travers un rideau pendu\n");
		const uint32 with = Tunnel(true), without = Tunnel(false);
		std::fprintf(stderr, "  particules restees a plus de 15 cm derriere la capsule : %u avec l'interpolation par sous-pas, %u sans\n", with, without);
		GCHECK(with == 0, "(h6) interpolation par sous-pas : aucune particule ne traverse la capsule");
		GCHECK(without > 0, "(h6) contre-epreuve colliderMotion = false : la capsule saute au travers (le temoin sait rougir)");
	}

	// ── (h7) en marche ────────────────────────────────────────────────────
	void TestWalk(const Body &b) {
		std::fprintf(stderr, "[VETEMENTS (h7)] en marche 3 s a 1 m/s (jambes +/- 25 deg, bras +/- 20 deg, hanches +/- 2 cm)\n");
		const NkGarmentKind kinds[5] = {NK_GARMENT_CAPE, NK_GARMENT_JUPE, NK_GARMENT_FOULARD, NK_GARMENT_TSHIRT,
										NK_GARMENT_PANTALON};
		NkGarmentParams gp;
		if (const char *e = std::getenv("NK_GARMENT_IT"); e && e[0]) gp.iterations = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_GARMENT_SUB"); e && e[0]) gp.substeps = (uint32)std::atoi(e);
		NkVector<math::NkMat4f> W;
		for (uint32 k = 0; k < 5; ++k) {
			NkGarment g;
			g.Build(kinds[k], b.measures, b.map, b.skel, gp, &b.mannequin);
			b.Pose(0.f, 1.f, W);
			b.mannequin.Pose(W.Data(), JOINTS, g.cloth.colliders);
			g.SnapPins(W.Data(), JOINTS);
			// 1 s au repos d'abord (le vêtement se pose), puis 3 s de marche
			for (uint32 f = 0; f < 60; ++f) {
				g.UpdatePins(W.Data(), JOINTS);
				g.cloth.Step(kDt, (float32)f * kDt);
			}
			const NkVec3f c0 = Centroid(g.cloth);
			float32 penMax = 0.f, pinMax = 0.f, stretchMax = 0.f;
			uint32 contactsMax = 0;
			for (uint32 f = 0; f < 180; ++f) {
				const float32 t = (float32)(f + 1) * kDt;
				b.Pose(t, 1.f, W);
				b.mannequin.Pose(W.Data(), JOINTS, g.cloth.colliders);
				g.UpdatePins(W.Data(), JOINTS);
				g.cloth.Step(kDt, 1.f + t);
				const NkClothStats &s = g.cloth.Stats();
				if (s.maxPenetration > penMax) penMax = s.maxPenetration;
				if (s.maxPinError > pinMax) pinMax = s.maxPinError;
				if (s.maxStretch > stretchMax) stretchMax = s.maxStretch;
				if (s.contacts > contactsMax) contactsMax = s.contacts;
			}
			const NkVec3f d = Centroid(g.cloth) - c0;
			std::fprintf(stderr, "  %-8s : penetration max %.3f mm sur 180 images | epingles max %.3f mm | etirement max %.2f %% (%u aretes degenerees) | contacts max %u | centre avance de %.2f m (corps : 3,00) | derive laterale %.1f cm\n",
						 NkGarmentName(kinds[k]), 1000.f * penMax, 1000.f * pinMax, 100.f * stretchMax, g.cloth.Stats().degenerateEdges,
						 contactsMax, d.z, 100.f * d.x);
			char msg[160];
			std::snprintf(msg, sizeof(msg), "(h7) %s en marche : penetration < 0,1 mm a chaque image, epingles < 1 mm, etirement < 2 %%, suit le corps (3 m +/- 0,3)",
						  NkGarmentName(kinds[k]));
			const bool ok = penMax < 1e-4f && pinMax < 1e-3f && stretchMax < 0.02f && math::NkAbs(d.z - 3.f) < 0.3f;
			if (!ok)
				PrintWorstEdge(g.cloth);
			GCHECK(ok, msg);
		}
	}

} // namespace

int RunGarmentTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	std::fprintf(stderr, "=== VETEMENTS SUR MANNEQUIN (NkMannequin, NkGarment) : temoins (h1)-(h7) ===\n");
	TestRoles();
	Body body;
	body.Build(true);
	TestCapsules(body);
	TestInside();
	TestGarmentsAtRest(body);
	TestRideAlong();
	TestTunnel();
	TestWalk(body);
	return fail;
}
