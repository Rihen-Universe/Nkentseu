#pragma once
// -----------------------------------------------------------------------------
// @File    NkMeshAnalysis.h
// @Brief   Lecture STRUCTURELLE d'un maillage : ce qu'il faut savoir pour le
//          retopologiser, le reparer, ou en faire une donnee d'apprentissage.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE MODULE
//   Trois besoins convergent sur la meme question — « comment ce maillage est-il
//   fait ? » :
//     • RETOPOLOGIE : ou sont les poles, ou courent les boucles, le modele est-il
//       symetrique, la densite est-elle homogene ;
//     • REPARATION (note « le vrai goulot » par la roadmap NKGen) : trous,
//       non-manifold, sommets isoles, faces degenerees ;
//     • DONNEE D'APPRENTISSAGE : decrire une forme par des CHIFFRES stables, pour
//       qu'un modele apprenne a produire une procedure qui les reproduit.
//   Les trois demandaient la meme analyse. La faire une fois evite trois variantes
//   qui divergeraient.
//
// CE QUI REND CETTE ANALYSE UTILISABLE POUR L'APPRENTISSAGE
//   Toutes les grandeurs sont des INVARIANTS calcules sur l'identite SOUDEE, donc
//   independants de l'ordre des sommets, du format d'import et du nombre de copies
//   de coin. Deux maillages identiques a une renumerotation pres donnent le meme
//   rapport. C'est la condition pour qu'un ecart mesure signifie quelque chose.
//
// CE QU'ELLE NE FAIT PAS
//   Elle ne juge pas la QUALITE (« ce maillage est-il beau ? ») et ne repare rien.
//   Elle DECRIT. Le jugement appartient a l'outil qui la consomme — et melanger
//   les deux produirait un seuil arbitraire enfoui dans une mesure.
//
// CPU PUR, aucun GPU.
// -----------------------------------------------------------------------------

#include "NKRenderer/Mesh/NkEditMesh.h"

namespace nkentseu {
	namespace renderer {

		// Un POLE est un sommet de valence differente de 4 sur un maillage quad.
		// C'est LA grandeur qui compte en retopologie : les boucles d'aretes
		// naissent et meurent aux poles, et leur placement decide de la
		// deformation. Un maillage « propre » n'en a pas zero — il en a peu, et
		// aux bons endroits.
		struct NkMeshPoles {
				uint32 valence3 = 0;	///< pole en E : 3 aretes (le plus courant)
				uint32 valence5 = 0;	///< pole en N : 5 aretes
				uint32 valence6plus = 0; ///< 6 et plus : souvent un defaut
				uint32 regular = 0;		///< valence 4 : le cas nominal d'un quad mesh
				uint32 boundary = 0;	///< sur un bord : la valence n'y signifie pas la meme chose
				uint32 Total() const {
					return valence3 + valence5 + valence6plus;
				}
		};

		// Symetrie detectee, par axe. `ratio` = proportion de sommets ayant un
		// vis-a-vis miroir. On rend un RATIO et non un booleen : la symetrie
		// parfaite n'existe pas sur un maillage importe, et un seuil code en dur
		// ici imposerait un jugement a tous les appelants.
		struct NkMeshSymmetry {
				float32 x = 0.f, y = 0.f, z = 0.f;
				int32 BestAxis() const {
					if (x >= y && x >= z)
						return 0;
					return (y >= z) ? 1 : 2;
				}
				float32 BestRatio() const {
					const int32 a = BestAxis();
					return (a == 0) ? x : (a == 1 ? y : z);
				}
		};

		struct NkMeshStats {
				// ── Comptage, sur identite SOUDEE ────────────────────────────────
				uint32 verts = 0;	  ///< sommets topologiques (copies coincidentes fusionnees)
				uint32 rawVerts = 0;  ///< sommets bruts (avant soudure) — ecart = duplication par coin
				uint32 edges = 0;
				uint32 faces = 0;
				uint32 tris = 0, quads = 0, ngons = 0;

				// ── Sante topologique ────────────────────────────────────────────
				uint32 boundaryEdges = 0;	///< portees par UNE face : le maillage est ouvert la
				uint32 nonManifoldEdges = 0; ///< plus de deux faces : pathologique
				uint32 looseVerts = 0;		///< aucun bord incident : invisibles et nuisibles
				uint32 degenerateFaces = 0;	///< aire quasi nulle

				// ── Forme ────────────────────────────────────────────────────────
				NkMeshPoles poles;
				NkMeshSymmetry symmetry;
				float32 quadRatio = 0.f;	///< quads / faces — 1 = full quad
				float32 edgeMin = 0.f, edgeMax = 0.f, edgeMean = 0.f;
				float32 edgeDeviation = 0.f; ///< ecart-type / moyenne : 0 = densite parfaitement homogene
				float32 area = 0.f;
				NkVec3f bboxMin{}, bboxMax{};

				// CARACTERISTIQUE D'EULER V - E + F. Sur un maillage FERME et
				// manifold, elle vaut 2 - 2g ou g est le genre (nombre d'anses) :
				// 2 pour une sphere ou un cube, 0 pour un tore. C'est la mesure de
				// forme la moins chere et la plus robuste qui existe — elle ne depend
				// d'aucune position, seulement de la topologie.
				int32 euler = 0;
				// Genre DEDUIT, valable seulement si le maillage est ferme et
				// manifold ; -1 sinon. Rendre -1 plutot qu'un chiffre faux : un genre
				// calcule sur un maillage ouvert n'a aucun sens.
				int32 genus = -1;

				bool IsClosed() const {
					return boundaryEdges == 0;
				}
				bool IsManifold() const {
					return nonManifoldEdges == 0;
				}
		};

		class NkMeshAnalysis {
			public:
				// `symEps` : tolerance de la recherche de vis-a-vis miroir, en
				// fraction de la diagonale de la boite englobante. Relative et non
				// absolue : un seuil en unites monde n'aurait pas le meme sens sur un
				// personnage de 2 m et sur une vis de 5 mm.
				static NkMeshStats Analyze(const NkEditMesh &m, float32 symEps = 0.002f);

				// Auto-test headless.
				static bool SelfTest();
		};

	} // namespace renderer
} // namespace nkentseu
