#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkEditOverlayStyle.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  L'APPARENCE DU MODE EDITION — UNE SEULE AUTORITE, PLUSIEURS LECTEURS
// =============================================================================
//  Tailles, couleurs et biais de profondeur des marqueurs de sommet, de face et
//  des aretes du mode Edition.
//
//  ⚠️ POURQUOI CE FICHIER EXISTE. Ces valeurs etaient ecrites DEUX FOIS : dans
//     `Applications/NK3DModeler/.../NkDemo3D.cpp` et dans
//     `Applications/Sandbox/src/Demo/Demo3D.cpp`. Elles etaient IDENTIQUES,
//     par portage verbatim -- et c'est precisement ce qui rendait la
//     divergence invisible : rien ne signalait qu'il y en avait deux. Le jour
//     ou l'une change, l'autre reste, et personne ne voit passer l'ecart.
//     Rodolf a demande de corriger l'apparence « comme Blender » : c'est
//     exactement le jour ou deux copies divergent.
//
//  ⚠️ ET CE N'EST PAS UN THEME. Ces couleurs ne passent par AUCUN role de
//     theme, volontairement : le viseur 3D n'en consulte aucun. Un chantier
//     voisin a perdu une soiree sur un critere de couleur pendant qu'un theme
//     non charge peignait tout avec une sentinelle ; ici, la couleur d'un
//     marqueur est une CONSTANTE du produit, et un test qui la lit lit la
//     verite.
//
//  LA REGLE QUI EMPECHE LA DIVERGENCE, puisqu'on ne peut pas l'imposer au
//  compilateur : **aucun literal de taille ou de couleur de marqueur ne
//  s'ecrit ailleurs que dans ce fichier.** Un site qui en aurait besoin ajoute
//  un champ ici et le lit. La sonde `sonde_marqueurs.ps1` mesure l'apparence
//  RENDUE des deux cotes : si une copie reapparaissait, ses chiffres
//  divergeraient.
// -----------------------------------------------------------------------------

#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		/// L'APPARENCE DES MARQUEURS ET DES ARETES DU MODE EDITION.
		/// Les demi-cotes sont en PIXELS ECRAN : le marqueur garde la meme taille
		/// apparente quelle que soit la distance, comme dans Blender.
		struct NkEditOverlayStyle {
				// ── SOMMETS : trois etats, trois tailles ────────────────────────
				// Blender distingue non selectionne / selectionne / ACTIF, et il
				// les distingue par la taille AUTANT que par la couleur : sur un
				// fond charge, la couleur seule ne suffit pas.
				static constexpr float32 kSommetDemi = 1.5f;		///< non selectionne
				static constexpr float32 kSommetDemiSel = 1.8f;		///< selectionne
				static constexpr float32 kSommetDemiActif = 2.0f;	///< actif (le dernier clique)

				// ── CENTRES DE FACE : memes etats, un cran plus discret au repos
				static constexpr float32 kFaceDemi = 1.4f;
				static constexpr float32 kFaceDemiSel = 1.8f;
				static constexpr float32 kFaceDemiActif = 2.0f;

				/// Le lisere sombre, dessine DESSOUS et donc plus grand du meme
				/// montant quel que soit l'etat. Il existe pour que le marqueur
				/// reste lisible sur une surface claire comme sur une sombre.
				static constexpr float32 kLisereSupp = 0.7f;

				// ── LE BIAIS DE PROFONDEUR ─────────────────────────────────────
				// ⚠️ IL REPOND A UN DEFAUT MESURE, PAS A UN GOUT. Un marqueur est
				//    un quad FACE-CAMERA centre EXACTEMENT sur le sommet : la
				//    moitie du quad plonge donc DANS le volume et perd le test de
				//    profondeur. Mesure du 17/09 : rayon X eteint, il ne survit
				//    qu'a 33 % dans le modeleur et 50 % dans renderdemo -- il est
				//    coupe net, et la suite est le gris de la surface. C'est ce
				//    que Rodolf decrit comme « pas la meme forme ».
				//
				//    Le biais de rasterisation (-1,5) ne peut rien : il decale la
				//    profondeur DU FRAGMENT, pas la POSITION du quad.
				//
				//    On decale donc le quad VERS LA CAMERA, le long du rayon de
				//    vue. Deux proprietes en decoulent, et ce sont elles qui font
				//    que c'est un correctif et non une triche :
				//      - le decalage est une FRACTION de la demi-taille MONDE du
				//        quad, donc il grandit avec la distance exactement comme
				//        lui : le marqueur ne « flotte » pas de loin et ne reste
				//        pas enfonce de pres ;
				//      - il est le long du RAYON DE VUE, donc la projection ecran
				//        ne bouge pas : le marqueur reste centre sur son sommet.
				//
				// ⚠️ ET IL RESTE PETIT, VOLONTAIREMENT. Un decalage large ferait
				//    apparaitre des sommets situes DERRIERE l'objet -- ce ne
				//    serait plus un decalage mais une desactivation du test de
				//    profondeur, et l'utilisateur selectionnerait des sommets
				//    qu'il ne voit pas. C'est le negatif obligatoire de la
				//    mesure.
				// ⚠️ 2,2 ET PAS PLUS, ET C'EST MESURE. Rayon X eteint, adresse par
				//    adresse (18/09) :
				//        decalage 0,0  ->  5 ENTIERS sur 7
				//        decalage 2,2  ->  6 ENTIERS sur 7
				//        decalage 6,0  ->  6 ENTIERS sur 7   (le triple : RIEN de plus)
				//    Au-dela de 2,2 le decalage n'apporte plus rien, et il ne fait
				//    qu'augmenter le risque de faire surgir un coin occulte. La valeur
				//    retenue est donc la PLUS PETITE qui atteint le plafond, pas une
				//    marge prise a l'aveugle.
				//    ⚠️ LE SEPTIEME MARQUEUR N'EST PAS UN PROBLEME DE PROFONDEUR : tripler
				//       le decalage ne le fait pas apparaitre. Sa cause est ailleurs, et
				//       elle n'est pas encore trouvee.
				static constexpr float32 kDecalVersCamera = 2.2f;

				// ── COULEURS ───────────────────────────────────────────────────
				// Palette Blender Edit Mode : cage quasi NOIRE, selection ORANGE
				// VIF, actif BLANC.
				static NkVec4f Sommet() { return NkVec4f{0.02f, 0.02f, 0.03f, 1.f}; }
				static NkVec4f SommetSel() { return NkVec4f{1.f, 0.55f, 0.05f, 1.f}; }
				static NkVec4f Actif() { return NkVec4f{1.f, 1.f, 1.f, 1.f}; }
				static NkVec4f Lisere() { return NkVec4f{0.f, 0.f, 0.f, 0.9f}; }

				static NkVec4f Arete() { return NkVec4f{0.015f, 0.015f, 0.02f, 1.f}; }
				static NkVec4f AreteSel() { return NkVec4f{1.f, 0.70f, 0.13f, 1.f}; }

				/// Le remplissage d'une face selectionnee : TRANSLUCIDE, pour que
				/// la surface reste lisible dessous. C'est son alpha qui le
				/// distingue d'un marqueur a l'ecran -- et c'est pour ca qu'un
				/// test de couleur ne se transporte pas d'un dorsal a l'autre :
				/// un melange translucide ne donne pas les memes pixels en
				/// OpenGL et en DirectX. Mesure du 18/09.
				static NkVec4f FaceRemplissage() { return NkVec4f{1.f, 0.55f, 0.06f, 0.36f}; }

				/// La demi-taille MONDE d'un marqueur, a la profondeur `d`, pour
				/// une demi-taille ECRAN `demiPx`. `thY` est la demi-tangente du
				/// champ vertical, `hauteurCible` la hauteur de la cible EN
				/// PIXELS.
				/// ⚠️ `hauteurCible` est celle de la CIBLE DE RENDU, pas de la
				///    fenetre : le modeleur rend son viseur hors ecran.
				static float32 DemiMonde(float32 demiPx, float32 thY, float32 hauteurCible, float32 d) {
					if (hauteurCible < 1.f)
						hauteurCible = 1.f;
					if (d < 1e-3f)
						d = 1e-3f;
					return demiPx * ((2.f * thY) / hauteurCible) * d;
				}
		};

	} // namespace renderer
} // namespace nkentseu
