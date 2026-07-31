#pragma once
// -----------------------------------------------------------------------------
// @File    NkMeshRetopo.h
// @Brief   Retopologie passe 2 : champ de croix + remaillage quad-dominant.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// LA PIECE QUI MANQUAIT ENTRE « ALLEGER » ET « RETOPOLOGIER »
//   La passe 1 (NkMeshDecimate, QEM) allege en gardant la forme, mais rend des
//   TRIANGLES. Un maillage d'animation veut des QUADS dont les aretes SUIVENT la
//   forme — boucles autour des articulations, rangees le long des membres. Ce
//   « sens du courant » a un nom : le champ de croix. En chaque face, une croix
//   de deux directions orthogonales dit « les aretes devraient passer par la ».
//
//   Le champ se calcule en deux temps :
//     1. les faces touchant une ARETE VIVE (angle diedre fort) ou un BORD sont
//        EPINGLEES : leur croix s'aligne sur cette arete. Ce sont les endroits ou
//        la forme IMPOSE la direction ;
//     2. les autres faces heritent par LISSAGE : chaque croix s'aligne sur la
//        moyenne de ses voisines, a un quart de tour pres (une croix revient sur
//        elle-meme tous les 90 degres — c'est ce qui permet a une rangee de
//        rencontrer une colonne sans conflit).
//
//   Ensuite, les paires de triangles voisins sont fusionnees en quads, par ordre
//   de merite : alignement des aretes du quad sur le champ, quasi-planarite,
//   angles proches de 90 degres. Quadify() ne peut pas rendre ce service : elle
//   ne considere que les paires CONSECUTIVES issues d'une triangulation
//   quad-par-quad — apres decimation, il n'y en a plus.
//
// CE QUE CETTE PASSE N'EST PAS, ET C'EST DIT D'AVANCE
//   Ce n'est pas la quadrangulation par grille entiere d'Instant Meshes : pas de
//   garantie 100 % quads, pas de controle des singularites (les points ou 3 ou 5
//   rangees se rencontrent). C'est un remaillage QUAD-DOMINANT guide par le
//   champ — le meme service que « Tris to Quads » de Blender, en mieux oriente.
//   La grille entiere reste l'etape « pro » de la feuille de route NKGen.
// -----------------------------------------------------------------------------

#include "NkMeshDecimate.h"

namespace nkentseu {
	namespace renderer {

		struct NkRetopoParams {
				// Decimation prealable (passe 1). 1.0 = pas de decimation : on ne fait
				// que fusionner les triangles existants en quads.
				float32 targetRatio = 1.f;
				uint32 targetFaces = 0;

				// Au-dela de cet angle diedre, une arete est VIVE : les deux faces sont
				// epinglees sur sa direction. 40 degres = les plis nets d'un objet
				// manufacture, sans epingler le simple galbe.
				float32 featureAngleDeg = 40.f;

				// Iterations de lissage du champ. 20 suffisent largement aux tailles
				// d'edition ; le champ converge vite parce que les epingles tirent.
				int32 fieldSmoothIters = 20;

				// Qualite d'angle minimale d'un quad candidat (1 = parfaitement carre,
				// 0 = degenere). En dessous, la paire reste en triangles : un mauvais
				// quad est PIRE que deux triangles, il se subdivise et s'anime mal.
				float32 minQuadQuality = 0.25f;

				// Angle diedre maximal ENTRE les deux triangles fusionnes. 45 degres :
				// un quad legerement bombe est normal sur une surface courbe, un quad
				// plie en deux n'est pas un quad.
				float32 maxFoldDeg = 45.f;
		};

		struct NkRetopoStats {
				uint32 trisIn = 0;		  ///< triangles en entree de la fusion
				uint32 quadsOut = 0;
				uint32 trisOut = 0;		  ///< triangles restes seuls
				float32 quadRatio = 0.f;  ///< quads / faces de sortie
				float32 alignMean = 0.f;  ///< alignement moyen des aretes de quads sur le champ (0..1)
				uint32 pinnedFaces = 0;	  ///< faces epinglees par une arete vive ou un bord
				NkDecimateStats decim;	  ///< stats de la passe 1 si elle a tourne
		};

		class NkMeshRetopo {
			public:
				// Pipeline complet : decimation QEM (si demandee) puis fusion guidee par
				// le champ. Le maillage est modifie EN PLACE ; les faces non appariees
				// restent des triangles.
				static bool QuadDominant(NkEditMesh &m, const NkRetopoParams &p, NkRetopoStats *out = nullptr);

				// Le champ seul, expose pour les tests et pour les visualisations a
				// venir : une direction par face (le representant de la croix ; les trois
				// autres s'en deduisent par quarts de tour autour de la normale).
				// outPinned[f] = 1 si la face a ete epinglee par une arete vive ou un
				// bord. Ne traite que les faces triangulaires.
				static void ComputeCrossField(const NkEditMesh &m, float32 featureAngleDeg, int32 iters,
											  NkVector<NkVec3f> &outDir, NkVector<uint8> &outPinned);
		};

	} // namespace renderer
} // namespace nkentseu
