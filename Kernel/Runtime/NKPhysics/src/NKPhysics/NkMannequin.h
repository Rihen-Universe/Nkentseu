#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkMannequin.h — le CORPS d'un personnage skinné, vu par le tissu (2026-09-05).
// Rodolf : « simuler des vêtements sur des mannequins en mouvement sans que le
// tissu n'entre dans les mesh du mannequin ». Ce fichier est l'étage (a) de la
// réponse : des PROXIES -- une capsule par os, AJUSTÉE SUR LA PEAU du modèle au
// repos, posée chaque image par le squelette animé -- c'est ce que Chaos Cloth
// (Unreal) et le Cloth d'Unity font par défaut, à la main ; ici le rayon est lu
// sur les sommets skinnés. L'étage (b), la collision contre le maillage skinné
// lui-même, ne se décide qu'après la mesure : le test POINT-DANS-MAILLAGE
// (parité d'un lancer de rayon) en bas de ce fichier est ce qui la fait, et il
// répond à « n'entre pas dans les mesh », pas seulement « dans les capsules ».
//
// NKPhysics ne voit ni NKRenderer ni NKAnima : le squelette entre en TABLEAUX
// BRUTS (parents, matrices monde de repos, noms), la peau en ÉCHANTILLONS
// (position au repos, quatre os, quatre poids) -- l'appelant (la sonde de la
// démo, plus tard Noge) les copie depuis son format. Même frontière que
// NkSkeletonView du ragdoll.
//
// RÔLES D'UN HUMANOÏDE (NkHumanoidMap) : résolus par la STRUCTURE du squelette
// (hanches = le premier joint à trois enfants ; jambes = les chaînes qui
// descendent ; colonne = celle qui monte ; au premier joint à trois enfants sur
// la colonne, les bras partent de côté et le cou monte), les NOMS ne servant
// qu'à départager gauche et droite (« left / right / _L / _R / .L ») ; sans nom
// utile, la gauche du personnage est +x (convention glTF : il regarde +z).
// Mesuré sur CesiumMan (19 joints, noms « Skeleton_arm_joint_L__4_ ») et XBot /
// YBot (65 joints, « mixamorig:LeftArm ») : les deux se résolvent sans table de
// noms -- une table aurait connu Mixamo et pas CesiumMan.
//
// CAPSULES (NkMannequin::Build) : une par joint j non racine, segment parent(j)
// -> j, portée par le repère de parent(j) (les sommets pesés par un os habitent
// le segment qui part de cet os vers ses enfants). Rayon = QUANTILE (défaut 0,9)
// des distances au segment des sommets dont l'os dominant est parent(j), chaque
// sommet compté pour le plus proche des segments enfants de son os. Le maximum
// gonflerait la capsule d'un torse à la largeur des épaules ; la médiane la
// creuserait ; 0,9 laisse dépasser 10 % des sommets -- et c'est le test
// point-dans-maillage qui dit ensuite combien de particules ça laisse entrer.
// Les doigts sont fusionnés : une capsule par main, de la main au bout du plus
// long doigt (65 joints -> 34 capsules sur XBot, 30 de moins par sous-pas).
//
// Zéro STL (qsort de la libc pour le quantile, comme strcmp ailleurs).
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKCollision/NkColShapes.h"
#include "NKMath/NkMat.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace physics {

		// ── Rôles d'un humanoïde ──────────────────────────────────────────────
		enum NkHumanBone : uint8 {
			NK_HB_HIPS = 0,
			NK_HB_SPINE,	 // premier joint au-dessus des hanches
			NK_HB_CHEST,	 // le joint d'où partent les bras et le cou
			NK_HB_NECK,
			NK_HB_HEAD,
			NK_HB_HEAD_TOP, // feuille au-dessus de la tête (si elle existe)
			NK_HB_L_SHOULDER,
			NK_HB_L_UPPER_ARM,
			NK_HB_L_FOREARM,
			NK_HB_L_HAND,
			NK_HB_R_SHOULDER,
			NK_HB_R_UPPER_ARM,
			NK_HB_R_FOREARM,
			NK_HB_R_HAND,
			NK_HB_L_UPPER_LEG,
			NK_HB_L_LOWER_LEG,
			NK_HB_L_FOOT,
			NK_HB_R_UPPER_LEG,
			NK_HB_R_LOWER_LEG,
			NK_HB_R_FOOT,
			NK_HB_COUNT
		};

		// Le squelette tel que NKPhysics le lit : tableaux bruts, aucune copie.
		struct NkSkeletonBind {
				const int32 *parent = nullptr;			// parent de chaque joint (-1 = racine), en indice de joint
				const math::NkMat4f *world = nullptr;	// pose de REPOS, monde (bind)
				const char *const *names = nullptr;		// noms (nullptr = sans noms)
				uint32 count = 0;
		};

		struct NkHumanoidMap {
				int32 joint[NK_HB_COUNT];
				NkHumanoidMap() {
					for (uint32 k = 0; k < NK_HB_COUNT; ++k)
						joint[k] = -1;
				}
				// Résolution structurelle (en-tête). Vrai si hanches, poitrine, deux cuisses et deux
				// bras (haut du bras) sont trouvés -- le minimum pour un vêtement.
				bool Resolve(const NkSkeletonBind &skel);
				uint32 Found() const noexcept {
					uint32 n = 0;
					for (uint32 k = 0; k < NK_HB_COUNT; ++k)
						if (joint[k] >= 0)
							++n;
					return n;
				}
				bool Has(NkHumanBone b) const noexcept {
					return joint[b] >= 0;
				}
				static const char *RoleName(NkHumanBone b) noexcept;
		};

		// Un sommet de la peau au REPOS (position monde à la pose de repos), ses os et ses poids.
		struct NkSkinSample {
				NkVec3f pos;
				int32 bone[4] = {-1, -1, -1, -1};
				float32 w[4] = {0.f, 0.f, 0.f, 0.f};
		};

		struct NkMannequinParams {
				float32 quantile = 0.9f;	// fraction des sommets de l'os SOUS le rayon (en-tête)
				float32 minRadius = 0.01f;	// m : rayon plancher (os sans sommet, doigts)
				float32 margin = 0.f;		// m : ajouté à chaque rayon (l'aisance se met plutôt sur le vêtement)
				uint32 minSamples = 6;		// sous ce nombre de sommets, le rayon est hérité du parent ou plancher
				bool mergeFingers = true;	// une capsule par main, de la main au bout du plus long doigt
		};

		// Une capsule de repos, dans le repère de l'os qui la porte.
		struct NkMannequinCapsule {
				int32 owner = -1;		 // os dont le repère porte la capsule (parent(j))
				int32 joint = -1;		 // l'extrémité : j
				NkVec3f localA, localB; // extrémités dans le repère de `owner` (repos)
				float32 radius = 0.f;	 // m
				uint32 samples = 0;		 // sommets qui ont servi au rayon
		};

		class NkMannequin {
			public:
				NkMannequinParams params;

				// Capsules par os depuis la peau au repos (en-tête). `map` (optionnelle) sert à
				// fusionner les doigts ; sans elle, chaque segment garde sa capsule.
				bool Build(const NkSkeletonBind &skel, const NkSkinSample *samples, uint32 sampleCount,
						   const NkHumanoidMap *map = nullptr);
				// Pose : les capsules MONDE pour une pose donnée (matrices monde par joint, même ordre).
				// `out` est vidée puis remplie, TOUJOURS dans le même ordre : c'est ce qui permet au
				// tissu d'interpoler entre deux poses (collidersPrev / colliders).
				void Pose(const math::NkMat4f *jointWorld, uint32 jointCount, NkVector<collision::NkShape> &out) const;

				uint32 CapsuleCount() const noexcept {
					return (uint32)mCaps.Size();
				}
				const NkMannequinCapsule &Capsule(uint32 i) const noexcept {
					return mCaps[i];
				}
				// Rayon de repos du segment parent(j) -> j (0 si aucune capsule ne finit en j).
				float32 RadiusTo(int32 j) const noexcept;
				// Rayon max des capsules PORTÉES par l'os b (0 si aucune).
				float32 RadiusOf(int32 b) const noexcept;
				uint32 JointCount() const noexcept {
					return mJointCount;
				}

			private:
				NkVector<NkMannequinCapsule> mCaps;
				uint32 mJointCount = 0;
		};

		// ── Point dans un maillage fermé : parité d'un lancer de rayon ────────
		// Le maillage skinné de l'image (sommets monde, indices), une grille sur (y, z) et un
		// rayon selon +x par requête. C'est LE test qui répond à « le tissu n'entre pas dans
		// les mesh » : une particule dont le rayon +x coupe un nombre IMPAIR de triangles est
		// dedans (Möller-Trumbore de NKCollision, NkRayTriangle3D). Pas d'épaisseur : le
		// centre de la particule est dedans ou pas ; l'épaisseur du tissu se mesure à part.
		class NkMeshInsideTester {
			public:
				// Reconstruit la grille (par image sur un maillage animé). cellsY x cellsZ cellules.
				void Build(const NkVec3f *verts, uint32 vertCount, const uint32 *indices, uint32 triCount,
						   uint32 cellsY = 32, uint32 cellsZ = 32);
				bool Inside(const NkVec3f &p) const noexcept;
				// ⚠ LA PARITÉ EXIGE UNE SURFACE FERMÉE, et un personnage de production n'en est pas
				// une. Mesuré le 2026-09-05 sur XBot et YBot (Mixamo) par un contrôle POSITIF -- le
				// centre de chaque triangle rentré de 1 cm sous sa face, qui est dedans par
				// construction : la parité n'en reconnaît que **47,4 %** (XBot) et **48,8 %**
				// (YBot). Le corps est fait de plusieurs coques ouvertes qui se recouvrent (surface
				// + articulations), et un rayon en traverse deux là où il devrait en traverser une.
				// Le contrôle NÉGATIF, lui, est vert (0 / 512 points à 3 m dits dedans).
				// D'où ce second test : le TRIANGLE LE PLUS PROCHE et le signe de sa normale
				// (pseudonormale ; Ericson, « Real-Time Collision Detection », §5.1.5). Il ne
				// suppose rien de fermé, il coûte O(triangles) par requête -- c'est un instrument de
				// mesure, pas un test par image. `outDepth` (optionnel) rend la profondeur en m.
				bool InsideNearest(const NkVec3f &p, float32 *outDepth = nullptr) const noexcept;
				uint32 CountInsideNearest(const NkVec3f *pts, uint32 count, float32 *outMaxDepth = nullptr) const noexcept;
				// Nombre de points dedans parmi `count` ; `firstInside` (optionnel) = premier indice trouvé.
				uint32 CountInside(const NkVec3f *pts, uint32 count, int32 *firstInside = nullptr) const noexcept;
				uint32 TriangleCount() const noexcept {
					return mTriCount;
				}

			private:
				const NkVec3f *mVerts = nullptr;
				const uint32 *mIdx = nullptr;
				uint32 mTriCount = 0;
				NkVec3f mMin, mMax;
				uint32 mCY = 0, mCZ = 0;
				NkVector<uint32> mCellStart; // CSR : mCY * mCZ + 1
				NkVector<uint32> mCellTri;
		};

	} // namespace physics
} // namespace nkentseu
