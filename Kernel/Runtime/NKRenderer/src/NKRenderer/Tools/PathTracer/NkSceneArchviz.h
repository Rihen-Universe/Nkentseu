#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkSceneArchviz.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LA DESCRIPTION DE SCENE COMMUNE (09/11/2026, rendu-realiste)
// =============================================================================
//  Le traceur de chemins (NkTraceurChemins) et la reference Cycles lisent la MEME
//  description : on compare alors deux integrateurs, pas deux scenes.
//    - la geometrie : les .obj (et .mtl) exportes a cote des .nkgeo d'un assemblage
//      .nkscene de l'atelier (pieces `forme maillage fichier X.nkgeo origine x y z`) ;
//    - les matieres : `materiaux_scene.json` de l'archviz (bibliotheque nommee ;
//      attribution PAR NOM D'OBJET « document:piece » : d'abord `pieces`, puis la
//      premiere des `regles` [regex document, regex piece, matiere] qui convient) ;
//    - les lumieres : `lumieres_scene.json` (sources point / spot / ruban en lumens
//      et kelvins, liees a leur objet ; ambiances : soleil, ciel en degrade, EV).
//  Le document d'une piece : la ligne `scene X` de `<dossier>/<document>.nkscene`
//  est la ligne `o X` du .obj ; les sources d'un document sont decalees de l'origine
//  de sa piece dans l'assemblage.
//  UNITES : 1 unite = 1 lux x 1e-3 (soleil du soir 2 500 lux = 2,5, la valeur de
//  NK_SOLEIL de l'archviz) ; une source de F lumens a la puissance P = F x 1e-3 ;
//  point : I = P / 4 pi ; spot : I = P / (2 pi (1 - cos demi-angle)) ; ruban :
//  points isotropes tous les 25 cm au plus (flux par metre x longueur).
//  Aucune bibliotheque standard : JSON et expressions regulieres (le sous-ensemble
//  employe par l'archviz : ^ $ | ( ) . [..] \d, sans quantificateur) ecrits ici.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		struct NkScMateriau {
				char nom[48] = {0};
				float32 couleur[3] = {0.8f, 0.8f, 0.8f}; ///< albedo lineaire
				float32 rugosite = 0.8f;
				float32 metal = 0.f;
				float32 transmission = 0.f; ///< 1 : verre (dielectrique lisse, n = 1,5)
				float32 emission = 0.f;		///< radiance emise = couleur x emission (unites de scene)
				int32 classe = -1;			///< carte procedurale (NkMatieresProcedurales), -1 aucune
				float32 tuile = 1.f;		///< metres par tuile de la carte
		};

		struct NkScSource {
				int32 type = 0; ///< 0 point (isotrope), 1 spot
				float32 p[3] = {0.f, 0.f, 0.f};
				float32 dir[3] = {0.f, -1.f, 0.f}; ///< spot : axe
				float32 cosExt = -1.f;			   ///< spot : cosinus du demi-angle
				float32 rayon = 0.02f;			   ///< rayon de la source (ombres douces)
				float32 couleur[3] = {1.f, 1.f, 1.f};
				float32 intensite = 0.f; ///< intensite radiante (unites de scene / sr)
				float32 puissance = 0.f; ///< P = lumens x 1e-3 (echantillonnage)
				char objet[64] = {0};
		};

		struct NkScArchviz {
				// geometrie (monde, metres, Y en haut) ; normales sortantes
				NkVector<float32> P;  ///< 3 par sommet
				NkVector<float32> N;  ///< 3 par sommet
				NkVector<float32> UV; ///< 2 par sommet (metres, projection en boite ; / tuile au rendu)
				NkVector<uint32> tri; ///< 3 indices par triangle
				NkVector<int32> triMat;
				NkVector<NkScMateriau> mats;
				NkVector<NkScSource> sources;
				// l'ambiance
				float32 soleilDir[3] = {0.f, 1.f, 0.f}; ///< VERS le soleil
				float32 soleilCouleur[3] = {1.f, 1.f, 1.f};
				float32 soleilE = 0.f; ///< eclairement (unites de scene) sur un plan face au soleil
				float32 cielZenith[3] = {0.25f, 0.45f, 0.85f};
				float32 cielHorizon[3] = {0.75f, 0.85f, 0.95f};
				float32 cielIntensite = 1.f;
				// (10/11/2026) LE CIEL EN TABLE lat-long (W x H, RGB a plat ; theta depuis le zenith,
				// phi autour de +y depuis +x vers +z : NkHosekCielTable). Present, il remplace le
				// degrade (x cielIntensite).
				NkVector<float32> cielTable;
				int32 cielW = 0, cielH = 0;
				float32 expositionEV = 10.f;
				float32 mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
				// le compte rendu de l'attribution (preuve : le meme compte que la reference Cycles)
				int32 piecesParNom = 0, piecesParRegle = 0, piecesParMtl = 0;
		};

		/// Charge la scene. `assemblage` : le .nkscene ; `matieres` / `lumieres` : les deux JSON ;
		/// `ambiance` : "soir" ou "jour" ; `terrain` : demi-cote du terrain d'herbe (0 : aucun).
		/// Ecrit le compte rendu sur la sortie. Rend faux si la geometrie manque.
		bool NkScArchvizCharger(NkScArchviz &s, const char *assemblage, const char *matieres, const char *lumieres,
								const char *ambiance, float32 terrain);

		/// Recherche d'une expression reguliere (sous-ensemble ^ $ | ( ) . [..] \d) dans un texte
		/// (semantique de re.search). Expose pour son banc.
		bool NkScRegexCherche(const char *motif, const char *texte);

		/// Banc de la regex et du JSON (cas ecrits, attendus ecrits avant) ; rend le nombre d'echecs.
		int32 NkScArchvizBanc();

	} // namespace renderer
} // namespace nkentseu
