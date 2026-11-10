#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkMatieresProcedurales.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LES MATIERES PHYSIQUES GENEREES PAR NOTRE CODE (09/11/2026, rendu-realiste)
// =============================================================================
//  Rodolf (09/11) : « je veux les rendus realistes ». Une matiere reelle n'est pas
//  un aplat : l'enduit a son grain, la pierre ses joints, le bois son fil et ses
//  lames, le carrelage ses joints et son poli. Ces cartes sont CALCULEES ici, sans
//  aucune image telechargee : bruit de valeurs periodique (donc la texture se
//  repete sans couture), bruit fractal (fbm, somme d'octaves) et bruit cellulaire
//  (Worley 1996 : distance au germe le plus proche, F1, et au second, F2).
//
//  Trois cartes par matiere, au standard glTF que lit le nuanceur PBR :
//    - COULEUR (sRGB) : une MODULATION autour de la teinte de la piece (la teinte
//      de sommet multiplie la carte dans le nuanceur : `albSample *= vColor`), sauf
//      l'herbe du sol qui porte sa propre couleur ;
//    - NORMALE (lineaire, convention OpenGL : +y = sens des v croissants), tiree
//      d'un champ de hauteur par differences centrees ;
//    - ORM (lineaire) : r = occlusion de cavite, g = RUGOSITE, b = METAL. Le
//      materiau porte rugosite = 1 et metal = 1 : la carte dit la valeur vraie
//      (le nuanceur multiplie : `rog = orm.g * uObj.roughness`).
//  Chaque classe dit sa TAILLE DE TUILE en metres : les UV des pieces sont posees
//  en metres par le chargeur (NkRenduRealiste.cpp), divises par cette taille.
//
//  Parametres ecrits (pas devines) : table `NkMatiereDe`. Les valeurs de rugosite
//  suivent les ordres de grandeur publies pour le PBR metal/rugosite (enduit et
//  beton 0,8 a 0,9 ; bois huile 0,5 a 0,6 ; carrelage emaille 0,2 a 0,3 ; metal
//  brosse 0,3 a 0,4 ; verre 0,02).
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		enum NkMatiereClasse : int32 {
			kMatEnduit = 0,	  ///< murs blancs, plafonds : grain fin d'enduit
			kMatBeton,		  ///< marches, dalles, poteaux : pores et nuances
			kMatPierre,		  ///< parements : moellons irreguliers et joints
			kMatBois,		  ///< bardage, menuiserie : fil, cernes, lames
			kMatCarrelage,	  ///< sols interieurs : carreaux de 60 cm, joints de 4 mm
			kMatMetal,		  ///< chassis, garde-corps : metal brosse
			kMatFeuillage,	  ///< plantes : feuilles en cellules
			kMatHerbe,		  ///< le terrain : herbe et terre (couleur propre)
			kMatCeramique,	  ///< sanitaires, faiences : poli
			kMatCuir,		  ///< assises : grain fin
			kMatPeinture,	  ///< teintes sombres ou colorees non classees
			kMatVerre,		  ///< vitrages : pas de carte, transmission par l'opacite
			kMatEmissif,	  ///< bandeaux, spots, ecrans : pas de carte
			// (10/11/2026, pour le film de l'archviz) les etoffes et les placages
			kMatTissu,		  ///< lin, chenille, bouclette, plaid : une TRAME (fils de chaine et de trame, flammes du lin)
			kMatVelours,	  ///< velours : poil fin, moire d'ecrasement ; le lustre fait le reste
			kMatPlacage,	  ///< grands panneaux : fil continu sur 2,4 m, lies assortis en livre, sans lames ni damier
			kMatClasses
		};

		struct NkMatiereParams {
				const char *nom;
				float32 tuileM;	   ///< taille de la tuile en metres (UV = metres / tuileM)
				float32 rugosite;  ///< rugosite moyenne (0 miroir, 1 mat)
				float32 metal;	   ///< 0 dielectrique, 1 metal
				float32 relief;	   ///< intensite de la carte normale (0..2)
				bool cartes;	   ///< false : verre, emissif (valeurs numeriques seules)
				bool couleurPropre; ///< true : la carte porte la couleur (herbe), la teinte est blanche
		};

		/// La table des classes (bornee : une classe hors table rend l'enduit).
		const NkMatiereParams &NkMatiereDe(int32 classe);

		/// Genere (ou retrouve) les trois cartes d'une classe dans `dossier` :
		/// `<nom>_couleur_v<version>.png`, `_normale_`, `_orm_`. `taille` en pixels
		/// (puissance de 2 conseillee). `moyenneLin` recoit la moyenne LINEAIRE de la
		/// carte de couleur (r, g, b) : le chargeur divise la teinte par elle pour
		/// que la piece garde sa teinte moyenne. Rend faux si une ecriture echoue.
		bool NkMatiereGenerer(int32 classe, const char *dossier, int32 taille, float32 *moyenneLin3);

		/// Les chemins des cartes d'une classe (apres NkMatiereGenerer). `canal` :
		/// 0 couleur, 1 normale, 2 orm.
		void NkMatiereChemin(int32 classe, const char *dossier, int32 canal, char *out, int32 cap);

		/// BANC de justesse des generateurs (sans GPU) : periodicite (le bord gauche
		/// prolonge le bord droit), bornes, moyenne et ecart-type par classe, et la
		/// contre-epreuve `mutation` (bruit NON periodique : le banc doit le voir).
		/// Ecrit le compte rendu sur la sortie ; rend le nombre d'echecs.
		int32 NkMatieresBanc(bool mutation);

	} // namespace renderer
} // namespace nkentseu
