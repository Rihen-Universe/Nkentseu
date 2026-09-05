#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGarment.h — VÊTEMENTS PROCÉDURAUX sur un squelette (2026-09-05).
// Rodolf : « tu génères des vêtements : robe, jupe, pantalon, chemise, t-shirt,
// foulard, chapeau, cape ». Chaque vêtement est UNE fonction qui prend les
// MESURES du corps (lues sur les os et les capsules du mannequin : largeur
// d'épaules, rayon de taille, longueur de jambe...) et rend un NkCloth déjà
// ÉPINGLÉ aux bons os (épingles à cible : la rangée épinglée suit l'os
// rigidement, le reste est simulé).
//
// CONSTRUCTIONS, du simple au complexe :
//   cape      rectangle derrière le dos, rangée haute épinglée à la POITRINE
//   foulard   anneau autour du cou (tube fermé, évasé sur les épaules), rangée
//             haute épinglée au COU
//   jupe      tube fermé de la taille à l'ourlet (évasé), rangée haute épinglée
//             aux HANCHES, ourlet libre
//   t-shirt   tube du corps (épaules -> hanches, épinglé à la POITRINE) + deux
//             manches courtes (tubes autour du bras), COUSUES au corps : la
//             couture = contraintes de distance entre bords (rangée haute de la
//             manche <-> particules les plus proches du corps), rien d'épinglé
//             sur la manche -- elle tient par la couture et le bras
//   chemise   t-shirt à manches longues (jusqu'au poignet), corps plus long
//   robe      corsage (tube épinglé à la poitrine) + jupe NON épinglée, cousue
//             au bas du corsage : la jupe pend de la couture
//   pantalon  ceinture (tube épinglé aux hanches) + deux jambes (tubes autour
//             de chaque jambe, de la ceinture à la cheville) cousues à la
//             ceinture ; pas de panneau d'entrejambe : les deux tubes se
//             recouvrent entre les cuisses (nommé, non fait : le fond)
//   chapeau   RIGIDE, attaché à la tête (NkHat) : pas de simulation
//
// Un vêtement = plusieurs PANNEAUX dans un seul NkCloth (AppendPanel), donc une
// seule liste de colliders, une seule auto-collision, un seul pas.
//
// Ce qui attend les mannequins de Rodolf (homme et femme) : les mesures sont
// lues sur le squelette, donc un rig Mixamo ou CesiumMan les fournit déjà ; ce
// qui manque est l'ajustement fin (aisance par zone, pinces, ourlets, un vrai
// patron) et les tissus par matière (compliances, densité) -- nommé.
// =============================================================================
#include "NKPhysics/NkCloth.h"
#include "NKPhysics/NkMannequin.h"

namespace nkentseu {
	namespace physics {

		// Les mesures d'un corps au repos, dans le monde. Tout ce que les vêtements lisent.
		struct NkBodyMeasures {
				bool valid = false;
				// repère du corps : haut, droite (de l'épaule gauche vers la droite), avant (up x right)
				NkVec3f up{0.f, 1.f, 0.f}, right{-1.f, 0.f, 0.f}, forward{0.f, 0.f, 1.f};
				// positions de joints (monde, repos)
				NkVec3f hips, chest, neck, head, headTop;
				NkVec3f lShoulder, rShoulder, lUpperArm, rUpperArm, lElbow, rElbow, lHand, rHand;
				NkVec3f lUpLeg, rUpLeg, lKnee, rKnee, lFoot, rFoot;
				// longueurs (m)
				float32 height = 0.f;			// sommet du squelette - bas
				float32 shoulderWidth = 0.f;	// |épaule G - épaule D| (bras si pas d'épaule)
				float32 hipWidth = 0.f;			// |cuisse G - cuisse D|
				float32 torsoLength = 0.f;		// cou - hanches
				float32 legLength = 0.f;		// cuisse - pied
				float32 armLength = 0.f;		// bras - main
				// rayons (m), lus sur les capsules du mannequin
				float32 waistRadius = 0.f, chestRadius = 0.f, neckRadius = 0.f, headRadius = 0.f;
				float32 thighRadius = 0.f, calfRadius = 0.f, upperArmRadius = 0.f, forearmRadius = 0.f;
				float32 pelvisRadius = 0.f; // capsules hanches -> cuisses : ce que le bassin occupe sous les hanches
		};

		// Lit les mesures. Faux si la carte ne suffit pas (Resolve faux) ; les rayons manquants
		// sont estimés depuis la taille (dits dans `estimatedRadii`).
		bool NkMeasureBody(const NkMannequin &mannequin, const NkHumanoidMap &map, const NkSkeletonBind &skel,
						   NkBodyMeasures &out, uint32 *estimatedRadii = nullptr);

		enum NkGarmentKind : uint8 {
			NK_GARMENT_CAPE = 0,
			NK_GARMENT_FOULARD,
			NK_GARMENT_JUPE,
			NK_GARMENT_TSHIRT,
			NK_GARMENT_CHEMISE,
			NK_GARMENT_ROBE,
			NK_GARMENT_PANTALON,
			NK_GARMENT_COUNT
		};
		const char *NkGarmentName(NkGarmentKind k) noexcept;
		// "cape" / "jupe" / ... -> genre ; faux si inconnu
		bool NkGarmentFromName(const char *name, NkGarmentKind &out) noexcept;

		struct NkGarmentParams {
				float32 spacing = 0.03f;	  // m entre particules
				float32 areaDensity = 0.2f;	  // kg/m² (coton léger 0,15-0,25)
				float32 ease = 0.02f;		  // m d'aisance entre le corps et le tissu
				float32 thickness = 0.006f;	  // rayon d'une particule (NkClothParams::thickness)
				float32 friction = 0.3f;
				float32 damping = 1.f;
				// 32 x 2 : mesuré (h4) / (h7) -- à 16 x 2 une jupe de 20 rangées pendue à sa ceinture
				// s'étire de 4,3 % au repos et 7 % en marche (la charge ne se propage pas assez le
				// long des colonnes, en-tête de NkCloth) ; à 32 x 2 elle passe sous 1 % / 2 %.
				// 32 x 4 : une jupe de 20 rangées pendue à sa ceinture restait à 1,03 % au repos et 4,7 %
				// en marche à 32 x 2 ; le coût est mesuré dans la sonde (échanges, DECISIONS).
				uint32 substeps = 32, iterations = 4;
				bool selfCollision = false;
		};

		// Une épingle : la particule suit `bone` rigidement (offset dans le repère de l'os au repos).
		struct NkGarmentPin {
				uint32 particle = 0;
				int32 bone = -1;
				NkVec3f local;
		};

		class NkGarment {
			public:
				NkGarmentKind kind = NK_GARMENT_CAPE;
				NkCloth cloth;
				NkVector<NkGarmentPin> pins;
				uint32 seams = 0;	// contraintes de couture (entre panneaux)
				uint32 pieces = 0;	// panneaux
				float32 mass = 0.f; // kg

				NkGarment() = default;
				NkGarment(const NkGarment &) = delete;
				NkGarment &operator=(const NkGarment &) = delete;

				// Construit le vêtement sur le corps au repos et l'épingle. Avec le mannequin, chaque
				// panneau est AJUSTÉ HORS DU CORPS avant que ses longueurs de repos soient prises
				// (mesuré : un anneau épinglé posé dans la sphère de la tête ou dans la clavicule
				// donnait 16 / 55 mm de pénétration permanente et 100-290 % d'étirement -- une
				// épingle dans une capsule n'en sort jamais). L'ajustement est DIRECTIONNEL : un anneau
				// est poussé le long de son rayon, une nappe le long de sa normale, jusqu'à sortir --
				// pousser vers la surface la plus proche projetait des voisines sur le même point
				// (mesuré : arêtes de repos 0 mm à l'aisselle et à l'entrejambe, 3 000 % d'étirement).
				// Une manche ne s'ajuste que contre son bras, une jambe contre sa jambe. Les capsules
				// au repos sont aussi copiées dans cloth.colliders.
				// `bodyMesh` (facultatif, la peau au REPOS) : le patron est aussi poussé hors du
				// MAILLAGE, pas seulement hors des capsules. Mesuré le 05/09 sur XBot (28 374
				// sommets) : avec les capsules seules, une jupe garde 136-142 particules sur 1 302
				// À L'INTÉRIEUR du corps en permanence -- la capsule du bassin (quantile 0,9 : 144 mm,
				// quantile 1,0 : 164 mm) est plus mince que le ventre et les fesses, et passer le
				// quantile de 0,9 à 1,0 ne fait tomber le compte que de 142 à 136. Le rayon d'une
				// capsule ne peut pas décrire une section qui n'est pas un disque : c'est le maillage
				// qui tranche.
				bool Build(NkGarmentKind k, const NkBodyMeasures &body, const NkHumanoidMap &map,
						   const NkSkeletonBind &skel, const NkGarmentParams &params,
						   const NkMannequin *mannequin = nullptr, const NkMeshInsideTester *bodyMesh = nullptr);
				// Chaque image, AVANT cloth.Step : les cibles des épingles depuis la pose courante.
				void UpdatePins(const math::NkMat4f *jointWorld, uint32 jointCount);
				// Une fois, au repos : pose les épingles exactement (sans trajet).
				void SnapPins(const math::NkMat4f *jointWorld, uint32 jointCount);

			private:
				// `tilt` : composante vers le HAUT de la direction d'ajustement (0 = radial pur ; 1 = à 45°,
				// pour un col qui doit passer AU-DESSUS des épaules sans que deux rangées convergent)
				uint32 Tube(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
							float32 rTop, float32 rBot, float32 length, const NkGarmentParams &p, uint32 &nAround,
							uint32 &nDown, float32 tilt = 0.f);
				// tube À TROUS : keep(i, j, position) dit si la case existe ; `indexOf` reçoit la carte
				template <class Keep>
				uint32 TubeMasked(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
								  float32 rTop, float32 rBot, float32 length, const NkGarmentParams &p, uint32 &nAround,
								  uint32 &nDown, Keep keep, NkVector<int32> &indexOf);
				// coud les particules d'une liste aux plus proches d'une autre (rest = distance courante)
				uint32 SewLists(const NkVector<uint32> &a, const NkVector<uint32> &b);
				// pousse chaque position le long de SA direction jusqu'à sortir des capsules retenues ;
				// au-delà de 15 cm de marche (la direction suit l'axe d'un os : mesuré, un anneau
				// d'épaules marchait jusqu'à la main), repli : projection vers la surface la plus proche
				void FitOutside(NkVec3f *pts, const NkVec3f *dirs, uint32 count) const;
				bool InsideAny(const NkVec3f &p, NkVec3f *outNearest = nullptr) const;
				const NkMeshInsideTester *mFitMesh = nullptr;
				// anneaux d'un tube : positions et directions radiales
				void RingRows(const NkVec3f &top, const NkVec3f &axisDown, const NkVec3f &e1, const NkVec3f &e2,
							  float32 rTop, float32 rBot, float32 length, float32 spacing, uint32 &nAround, uint32 &nDown,
							  NkVector<NkVec3f> &rows, NkVector<NkVec3f> &dirs, float32 tilt = 0.f) const;
				// retient les capsules d'un membre (joint = racine du membre ou un de ses descendants),
				// ou toutes (root < 0), MOINS deux sous-arbres (le tronc ne s'ajuste pas contre les bras :
				// mesuré, la marche radiale d'un anneau d'épaules suivait l'axe du bras jusqu'à la main)
				void FitAgainst(int32 rootJoint, const NkSkeletonBind &skel, int32 excludeA = -1, int32 excludeB = -1);
				NkVector<collision::NkShape> mFitShapes; // capsules de repos (même ordre que le mannequin)
				NkVector<int32> mFitJoint;				// joint d'arrivée de chaque capsule
				NkVector<uint8> mFitOn;					// 1 = retenue pour l'ajustement en cours
				float32 mFitMargin = 0.f;
				void PinRow(uint32 first, uint32 count, int32 bone, const NkSkeletonBind &skel);
				uint32 Sew(uint32 firstA, uint32 countA, uint32 firstB, uint32 countB);
		};

		// Chapeau rigide : un cylindre attaché à la tête.
		struct NkHat {
				int32 bone = -1;
				math::NkMat4f local = math::NkMat4f::Identity(); // repère du chapeau dans celui de la tête (repos)
				float32 radius = 0.f, height = 0.f;
				bool Build(const NkBodyMeasures &body, const NkHumanoidMap &map, const NkSkeletonBind &skel);
				math::NkMat4f World(const math::NkMat4f *jointWorld, uint32 jointCount) const;
		};

	} // namespace physics
} // namespace nkentseu
