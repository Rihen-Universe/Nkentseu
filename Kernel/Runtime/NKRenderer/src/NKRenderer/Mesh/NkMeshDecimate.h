#pragma once
// -----------------------------------------------------------------------------
// @File    NkMeshDecimate.h
// @Brief   Decimation QEM (quadriques d'erreur) par contraction d'aretes.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// POURQUOI QEM ALORS QUE LA DECIMATION PAR CLUSTERING EXISTE DEJA
//   `gen::DecimateClustering` (NKGen) regroupe les sommets par cellule d'une
//   grille et prend leur moyenne. C'est robuste et rapide, mais la grille ignore
//   la FORME : une arete vive traversant une cellule est moyennee comme le reste,
//   donc arrondie. Sur un cube, les huit coins fondent.
//
//   QEM mesure, pour chaque contraction envisagee, DE COMBIEN la surface
//   s'ecarterait des plans d'origine. Contracter au milieu d'une face plate coute
//   zero ; contracter un coin coute cher. En traitant les contractions de la
//   moins chere a la plus chere, on retire d'abord ce qui ne porte pas la forme.
//   C'est la difference entre « alleger » et « alleger en gardant la silhouette ».
//
// TROIS REFUS, ET CHACUN CORRIGE UN DEFAUT QU'ON NE VOIT PAS AU COMPTE DE FACES
//   1. CONDITION DE LIEN — une contraction peut rendre le maillage non manifold
//      (deux nappes se touchant par un sommet). Le compte de faces serait juste,
//      la topologie ruinee, et l'anomalie n'apparaitrait qu'a la subdivision ou
//      au depliage UV.
//   2. RETOURNEMENT DE FACE — deplacer un sommet peut inverser une face voisine.
//      La surface se replie sur elle-meme : trous noirs au rendu, normales
//      inversees. Le cout QEM seul ne l'interdit pas.
//   3. BORD — sans contrainte, un bord ouvert se retracte vers l'interieur, la
//      silhouette rentre. On ajoute des plans virtuels perpendiculaires le long
//      des bords pour les retenir.
//
// CE QUE CETTE PASSE NE FAIT PAS, ET C'EST VOULU
//   Elle produit des TRIANGLES, pas un maillage quad a edge-flow. C'est l'etape
//   d'ALLEGEMENT du pipeline, pas la retopologie « pro » : celle-ci suppose un
//   champ de croix aligne sur les aretes vives (facon Instant Meshes), qui reste
//   ⬜ dans la roadmap NKGen. Confondre les deux serait promettre ce qui n'est
//   pas la.
//
//   Elle ne conserve PAS les UV. Le maillage est reconstruit depuis les positions
//   et les normales sont recalculees. Transferer les UV suppose une projection du
//   dense vers le leger, qui est l'etape 3 du pipeline (depliage + bake).
// -----------------------------------------------------------------------------

#include "NkEditMesh.h"

namespace nkentseu {
	namespace renderer {

		struct NkDecimateParams {
				// Cible. `targetFaces` l'emporte s'il est non nul ; sinon on garde
				// `targetRatio` des triangles de depart.
				float32 targetRatio = 0.5f;
				uint32 targetFaces = 0;

				// Retient les bords ouverts par des plans virtuels. A laisser vrai :
				// sans cela, la silhouette d'un maillage ouvert se retracte.
				bool preserveBoundary = true;

				// Refuse toute contraction qui ferait tourner une face de plus que cet
				// angle. 90 degres = on refuse le retournement franc sans interdire les
				// reorientations legitimes.
				float32 maxNormalFlipDeg = 90.f;

				// Refuse les contractions qui rendraient le maillage non manifold
				// (condition de lien). A ne desactiver que pour etudier son effet.
				bool preserveTopology = true;

				// Plafond d'erreur : une contraction plus couteuse n'est jamais faite,
				// meme si la cible n'est pas atteinte. 0 = pas de plafond. C'est ce qui
				// permet de dire « allege tant que tu ne deformes pas » plutot que
				// « atteins ce compte coute que coute ».
				float32 maxError = 0.f;
		};

		struct NkDecimateStats {
				uint32 trisBefore = 0;
				uint32 trisAfter = 0;
				uint32 vertsBefore = 0;
				uint32 vertsAfter = 0;
				uint32 collapses = 0;
				uint32 rejectedLink = 0; ///< refusees par la condition de lien
				uint32 rejectedFlip = 0; ///< refusees pour retournement de face
				uint32 rejectedCost = 0; ///< refusees par le plafond d'erreur
				float32 maxCost = 0.f;	 ///< cout de la contraction la plus chere acceptee
				bool reachedTarget = false;
		};

		class NkMeshDecimate {
			public:
				// Decime `m` EN PLACE. Le maillage est triangule puis reconstruit ; les
				// n-gons d'entree sont donc perdus (une contraction d'arete n'a pas de
				// definition simple sur un n-gon quelconque).
				static bool DecimateQEM(NkEditMesh &m, const NkDecimateParams &p, NkDecimateStats *out = nullptr);

				// Erreur de forme : distance moyenne et maximale des sommets de `ref`
				// a la surface de `m`. Sert a COMPARER deux decimations -- un compte de
				// triangles ne dit rien de ce qui a ete perdu.
				static void ShapeError(const NkEditMesh &m, const NkEditMesh &ref, float32 &outMean,
									   float32 &outMax);
		};

	} // namespace renderer
} // namespace nkentseu
