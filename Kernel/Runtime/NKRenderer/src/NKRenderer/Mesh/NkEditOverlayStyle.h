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
				// ⚠️ 2,0 EST MESURE DANS BLENDER, PAS CHOISI. Les amas de sommets de la
				//    capture 053421 (non selectionnes) et de 054024 (selectionnes) font
				//    tous deux environ 4 x 4 px, soit un demi-cote de 2,0.
				//    ⚠️ ET LES DEUX ETATS ONT LA MEME TAILLE : mon commentaire precedent
				//       affirmait que Blender distingue les trois etats par la taille
				//       AUTANT que par la couleur. Rien dans les captures ne le soutient.
				//       Il les distingue par la COULEUR. Je retire l'affirmation.
				static constexpr float32 kSommetDemi = 2.0f;		///< non selectionne (mesure)
				static constexpr float32 kSommetDemiSel = 2.0f;		///< selectionne (mesure)
				/// ⚠️ NON MESURE dans Blender : aucune capture ne montre un sommet ACTIF
				///    isole dont on puisse prendre la taille. On le garde un cran plus
				///    gros pour qu'il se distingue meme quand tout est selectionne, et
				///    c'est un CHOIX assume, pas un releve.
				static constexpr float32 kSommetDemiActif = 2.4f;	///< actif (NON MESURE)

				// ── CENTRES DE FACE : memes etats, un cran plus discret au repos
				static constexpr float32 kFaceDemi = 1.4f;
				static constexpr float32 kFaceDemiSel = 1.8f;
				static constexpr float32 kFaceDemiActif = 2.0f;

				/// ⚠️ ZERO : BLENDER N'EN DESSINE PAS. Mesure du 18/09 sur 053421 et
				///    054024 -- le sommet est un carre PLEIN, noir ou orange, et rien
				///    autour. Notre lisere sombre (alpha 0,9, 0,7 px de plus de chaque
				///    cote) etait une invention maison. Elle avait sa raison -- rester
				///    lisible sur fond clair ET sombre -- mais le critere est Blender.
				///    ⚠️ CE QUE CA PEUT COUTER, ET JE L'ECRIS PLUTOT QUE DE L'IGNORER :
				///       un sommet NOIR sur une surface sombre devient difficile a voir.
				///       Blender s'en tire parce que son fond de viseur est un gris
				///       moyen (63,63,63) et son cube un gris clair. Si notre viseur
				///       est plus sombre, le probleme reapparaitra -- et c'est a
				///       Rodolf de trancher SUR L'IMAGE, pas a moi de le prevenir en
				///       gardant un ornement qu'il n'a pas demande.
				/// Zero = aucun lisere n'est trace (le trace est saute, pas dessine
				/// a taille nulle : un quad de taille nulle coute quand meme deux
				/// triangles).
				static constexpr float32 kLisereSupp = 0.0f;

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
				/// NOIR PUR, mesure : (0,0,0) dans 053421. Nous avions (5,5,8).
				static NkVec4f Sommet() { return NkVec4f{0.f, 0.f, 0.f, 1.f}; }

				/// ⚠️ CE NOMBRE EST UNE CALIBRATION MESUREE, PAS LA COULEUR DE BLENDER.
				///    La cible relevee dans Blender est (255, 121, 0) A L'ECRAN. On ne
				///    peut PAS la poser telle quelle : ce qu'on ecrit ici traverse
				///    l'encodage et le post-traitement, et ne sort pas tel quel.
				///    Deux mesures, sur renderdemo a camera figee (18/09) :
				///        vert pose 0,4745  ->  202 a l'ecran
				///        vert pose 0,1550  ->  119-129 a l'ecran, dont (227,121,1) pile
				///    Aucune formule ne les relie proprement -- ni gamma 2,2, ni sRGB pur :
				///    le marqueur passe par le meme post-traitement que la scene. La
				///    valeur a donc ete trouvee PAR ITERATION MESUREE, en deux passes.
				///
				/// ⚠️ ET LE ROUGE PLAFONNE. Pose a 1,0, il sort a ~231, jamais 255 : la
				///    cible (255, ...) est HORS D'ATTEINTE avec ce post-traitement. Ce
				///    n'est pas une approximation acceptee a la legere, c'est une limite
				///    MESUREE de notre chaine, et elle se dit plutot que de laisser
				///    croire a une correspondance exacte.
				///    Resultat obtenu : (227, 121, 1) contre (255, 121, 0) vise.
				///
				/// ⚠️ LA MEME REGLE VAUT POUR LE REMPLISSAGE DE FACE, et pour la meme
				///    raison : on ne recopie pas un nombre lu dans une capture d'un
				///    autre moteur. On pose, on rend, on mesure, on corrige.
				static NkVec4f SommetSel() { return NkVec4f{1.f, 0.155f, 0.f, 1.f}; }
				static NkVec4f Actif() { return NkVec4f{1.f, 1.f, 1.f, 1.f}; }
				static NkVec4f Lisere() { return NkVec4f{0.f, 0.f, 0.f, 0.9f}; }

				static NkVec4f Arete() { return NkVec4f{0.f, 0.f, 0.f, 1.f}; }

				/// ⚠️ EXACTEMENT LA MEME QUE `SommetSel()`, ET C'EST LE POINT.
				///    Blender emploie UNE SEULE couleur de selection pour le sommet et
				///    pour l'arete. Nous en avions DEUX -- (255,140,13) et (255,178,33) --
				///    sans qu'aucune ligne n'explique pourquoi. Deux teintes proches et
				///    non identiques se lisent comme un defaut de rendu, pas comme une
				///    intention.
				///    Elle DELEGUE au lieu de recopier : deux literals egaux aujourd'hui
				///    divergeraient au premier reglage.
				static NkVec4f AreteSel() { return SommetSel(); }

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
