#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Viewport/NkCreaFamilles.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// L'ADAPTATEUR DES FAMILLES (21/09 Q8, deplace le 22/09 Q10.1).
//
// ⚠️ LES CONSTRUCTEURS VIVENT DANS NKRenderer, PAS ICI :
//    `Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkMeshFamilles.h`. Noge,
//    Nogee et NKScena les appellent de la meme facon. Ce qui suit n'est que la
//    porte du modeleur : geometrie -> noeuds de scene.
//
// POURQUOI. La porte chinoise de Rodolf est sortie en 22 planches alignees au
// sol, la villa en 21 boites : l'assemblage libre demande au modele de SAVOIR
// construire, et un 7B ne le sait pas. Pour une famille reconnue, la structure
// est ECRITE ICI une fois pour toutes ; le modele n'a plus qu'a dire « porte,
// style chinois, 2,4 m x 3,2 m, deux battants, detaillee ».
//
// ⚠️ LE DETAIL, PAS SEULEMENT LES VOLUMES (precision de Rodolf : « je ne vois pas
//    de details, des creux »). Chaque constructeur pose, en niveau « detaille »,
//    ce qui fait reconnaitre l'objet de pres, avec les operations qui EXISTENT
//    DEJA dans NkEditMesh : InsetSelectedFaces (profondeur negative = panneau en
//    creux), BevelSelected (chanfreins), SubdivideSelectedFaces (decoupe des
//    panneaux), le modificateur Array (rangees de tuiles), SpinSelected (pieds
//    tournes, poteaux, poignees). Rien n'est recode.
//
// ⚠️ AUCUN TYPE NKRenderer ICI : ce fichier est inclus par la creation, qui vit
//    dans l'unite de main.cpp, ou NKRenderer et NKCanvas ne peuvent pas se
//    rencontrer. Le travail est dans NkCreaFamilles.cpp.
// -----------------------------------------------------------------------------
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nk3d {

		struct NkFamParams {
				char famille[24] = {0}; ///< porte, table, maison (la revolution vit dans la creation)
				char style[24] = {0};	///< porte : simple | chinois ; maison : deux_pans | plat
				float32 largeur = 0.f, hauteur = 0.f, profondeur = 0.f;
				int32 nombre = 0;		///< porte : battants ; maison : etages ; table : pieds (4)
				int32 fenetres = 0;		///< maison : fenetres par facade et par etage
				bool detaille = true;
		};

		struct NkFamPiece {
				int32 noeud = -1;
				char nom[24] = {0};
				char matiere[24] = {0};
				uint32 faces = 0; ///< faces du maillage (n-gons) : la mesure « simple contre detaille »
		};

		/// Construit la famille, pieces en coordonnees OBJET (sol a y = 0, centre en
		/// x = z = 0). Rend le nombre de pieces creees, ou -1 et `pourquoi`.
		int32 NkFamConstruire(const NkFamParams &p, NkFamPiece *out, int32 cap, char *pourquoi, uint32 capPourquoi);

		/// La famille est-elle construite par NkFamConstruire ?
		bool NkFamConnue(const char *famille);

		/// Le nom de la i-eme famille, nullptr au-dela. UNE SEULE LISTE, celle de la
		/// bibliotheque : le lexique de reconnaissance la lit au lieu de la recopier.
		const char *NkFamNom(int32 i);

		/// Le formulaire de la famille, AVEC ses valeurs autorisees.
		bool NkFamFormulaire(const char *famille, char *out, uint32 cap);

		/// Valeur autorisee pour ce champ ? Sinon `remplacement` recoit le defaut
		/// nomme -- on CORRIGE, on ne rejette pas le document entier (defaut du 21/09).
		bool NkFamValeurAutorisee(const char *famille, const char *champ, const char *valeur, char *remplacement,
								  uint32 cap);

	} // namespace nk3d
} // namespace nkentseu
