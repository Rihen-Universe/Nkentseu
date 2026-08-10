#pragma once
// =============================================================================
// NkModelerScreens.h â€” les zones de l'ecran A, peintes une par une.
//
// Une fonction par zone, dans l'ordre ou elles apparaissent a l'ecran. Chacune
// recoit son rectangle et ne peint QUE dedans : c'est ce qui permettra de les
// deplacer, replier ou remplacer une par une sans toucher aux autres.
//
// Les valeurs affichees sont des EXEMPLES cales sur la maquette (Cube, Sphere,
// 8 sommets, 6 faces...). Elles seront remplacees par la scene reelle zone par
// zone -- c'est la marche a suivre convenue avec Rihen : construire, puis mettre
// a jour en developpant.
// =============================================================================

#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h" // PORTAGE INTEGRAL de --demo=2
#include "NK3DModeler/Viewport/NkOutCompose.h" // formes d'incrustation : dimensions et noms
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NKEditorKit/NkShortcutTable.h"
// La scrollbar STANDARD de Nkentseu (celle de l'editeur de code) : Rihen la veut
// partout, et son en-tete demande explicitement de ne pas la redessiner ailleurs.
#include "NKEditorKit/NkEditorScrollbar.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// Hauteur d'une ligne de liste ou de propriete. Reprise de la maquette :
		// 22 px. En dessous le texte touche les bords, au-dessus la densite chute et
		// il faut faire defiler pour rien.
		// Non const : elles sont multipliees par l'echelle au demarrage (cf. S()).
		inline float32 kRowH = 22.f;
		inline float32 kLabelW = 64.f;  ///< colonne de libelles (resserree : l'ecart
										///< label -> premiere coordonnee etait trop grand)
		inline float32 kPad = 8.f;
		// MARGE INTERNE des panneaux de droite. Le contenu ne doit pas toucher les
		// bords : colle au trait de separation, une ligne de propriete se lit comme
		// la continuation du panneau voisin. Applique en RETRECISSANT le rectangle de
		// travail, une fois, plutot qu'en decalant chaque appel -- sinon il suffit
		// d'en oublier un pour que l'alignement casse.
		inline float32 kInset = 10.f;

		// Declaree ici, definie pres des dialogues : la barre de titre (peinte
		// bien avant) et le menu Fichier doivent pouvoir demander la fermeture
		// par la MEME porte que la croix de l'OS.
		inline void NkRequestClose(NkModelerState &st);

		// Retrecit un rectangle de la marge interne, sans toucher au haut ni au bas :
		// l'en-tete d'onglet et les fonds pleins doivent, eux, aller bord a bord.
		// Applique l'echelle d'interface a toutes les constantes de disposition.
		// Appelee UNE FOIS au demarrage, apres avoir lu le DPI de la fenetre.
		inline void ApplyUiScale(float32 scale) {
			gUiScale = scale;
			kRowH = Px(22.f * scale);
			kLabelW = Px(64.f * scale);
			kPad = Px(8.f * scale);
			kInset = Px(10.f * scale);
		}

		inline NkRect Inset(const NkRect &r) {
			return {r.x + kInset, r.y, r.w - kInset * 2.f, r.h};
		}

		// â”€â”€ CONTENU DES LISTES DE LA VUE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// En TABLES, comme les menus : ajouter une entree ne demande pas de toucher
		// au rendu, et la meme table servira la palette de recherche.
		inline const char *const *NkProjectionItems(int32 &n) {
			// Les six vues orthographiques sont les MEMES que celles du gizmo de
			// navigation : un seul etat, deux facons d'y arriver.
			// ORTHOGONALE est une PROJECTION a part entiere, distincte des six vues
			// d'axe : on peut regarder de biais en orthographique. La confondre avec
			// Â« Dessus Â» -- l'erreur de ma premiere liste -- interdisait justement ce
			// cas, qui est celui du blocking et du travail de proportions.
			static const char *const k[] = {"Perspective", "Orthogonale", "Dessus", "Dessous",
											"Avant",	   "Arriere",	  "Gauche", "Droite"};
			n = 8;
			return k;
		}
		inline const NkIcon *NkProjectionIcons() {
			// La CAMERA pour la perspective (on voit depuis un point), la GRILLE pour
			// les vues orthographiques (aucun point de fuite). Deux dessins distincts,
			// sinon rien ne dit laquelle est active.
			// UNE ICONE PAR ENTREE. Six vues d'axe portant le meme dessin
			// obligeraient a lire le libelle pour les distinguer -- ce qui annule
			// l'interet d'une liste a icones, et rend le bouton de la barre muet
			// des qu'on sort de la perspective.
			//   perspective -> camera        orthogonale -> grille
			//   dessus / dessous -> fleches verticales
			//   avant / arriere  -> fleches horizontales
			//   gauche / droite  -> fleches simples
			static const NkIcon k[] = {NkIcon::Persp,	  NkIcon::OrthoView, NkIcon::ArrowUp,
									   NkIcon::ArrowDown, NkIcon::ViewFront,  NkIcon::ViewBack,
									   NkIcon::ArrowLeft, NkIcon::ArrowRight};
			return k;
		}
		// â”€â”€ MODES D'OMBRAGE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// QUATRE modes disponibles PARTOUT -- solide, materiau, rendu, fil de fer --
		// et on entre en mode EDITION depuis n'importe lequel : le mode d'edition et
		// le mode d'ombrage sont deux axes INDEPENDANTS. C'est ce que Rihen a precise,
		// et c'est ce que fait Blender : on peut editer un maillage en vue rendue.
		//
		// LE MATCAP est le CINQUIEME, et lui n'existe QU'EN EDITION. Sa raison d'etre
		// est justement de montrer la FORME sans texture ni eclairage de scene :
		// c'est ce qui permet de lire les bosses et les creux pendant qu'on modele.
		// L'offrir en mode objet n'aurait pas de sens -- on y regarde le resultat,
		// pas la surface.
		// QUATRE modes d ombrage, disponibles PARTOUT, et on entre en edition depuis
		// n importe lequel : mode d edition et mode d ombrage sont deux axes
		// INDEPENDANTS. Le matcap n en fait plus partie -- c est un reglage du mode
		// solide, cf. NkSolidLightItems.
		inline const char *const *NkShadingItems(int32 &n) {
			// LES SIX MODES REELS de la demo portee (sa touche Z) : l'index de la
			// liste EST le shadingMode de la demo. Une liste qui promettait un mode
			// Â« Materiau Â» inexistant obligeait a mentir au cablage.
			static const char *const k[] = {"Rendu",	"Solide", "Fil de fer",
											"Normales", "UV",	  "Occlusion (AO)"};
			n = 6;
			return k;
		}
		inline const NkIcon *NkShadingIcons() {
			// SIX DESSINS DISTINCTS -- un bouton a icone seule ne dit son etat que
			// si chaque valeur a le sien.
			static const NkIcon k[] = {NkIcon::Light,		// rendu : eclairage de scene
									   NkIcon::Circle,		// solide : sphere nue
									   NkIcon::Wireframe,	// fil de fer
									   NkIcon::ViewNormals, // normales
									   NkIcon::ViewUV,		// uv
									   NkIcon::ViewAO};		// occlusion
			return k;
		}

		// â”€â”€ FORMES DE SELECTION â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// C'est le sous-menu qu'annonce le petit point du bouton de selection.
		// Rectangle par defaut : c'est le geste le plus simple et le plus previsible.
		// Le cercle sert a Â« peindre Â» une selection en balayant, le lasso a cerner
		// une forme irreguliere -- trois besoins reels que le rectangle seul ne
		// couvre pas.
		inline const char *const *NkSelShapeItems(int32 &n) {
			static const char *const k[] = {"Rectangle", "Cercle", "Lasso"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSelShapeIcons() {
			static const NkIcon k[] = {NkIcon::SelRect, NkIcon::SelCircle, NkIcon::SelLasso};
			return k;
		}

		// Les matcaps disponibles. Ils ne s'affichent que si l'ombrage Matcap est
		// choisi : proposer un choix de matcap alors qu'aucun n'est utilise ferait
		// croire a un reglage sans effet.
		inline const char *const *NkMatcapItems(int32 &n) {
			static const char *const k[] = {"Argile", "Metal", "Perle", "Rouge mat", "Bleu doux",
											"Vert atelier", "Peau", "Ceramique"};
			n = 8;
			return k;
		}

		// â”€â”€ MENU DE VUE (l icone a gauche de Perspective) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Il porte ce qui ne merite pas un bouton permanent : disposition des vues,
		// plein ecran, cameras enregistrees, reinitialisation.
		inline const char *const *NkViewMenuItems(int32 &n) {
			// DES ACTIONS, pas un etat : chaque entree FAIT quelque chose de reel.
			// Les dispositions multi-vues promettaient un decoupage du viewport qui
			// n'existe pas encore -- retirees plutot que factices.
			static const char *const k[] = {"Memoriser la camera", "Rappeler la camera",
											"Reinitialiser la vue", "Panneaux : montrer / cacher"};
			n = 4;
			return k;
		}
		inline const NkIcon *NkViewMenuIcons() {
			static const NkIcon k[] = {NkIcon::Camera, NkIcon::Import, NkIcon::Refresh,
									   NkIcon::Drawer};
			return k;
		}
		// â”€â”€ AFFICHAGE : DES CASES, PAS UN CHOIX UNIQUE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Rihen a raison, et UI_SPEC 9bis le disait deja : on veut couramment la
		// grille ET le repere d axes SANS les normales. Mon premier jet proposait
		// trois presets exclusifs (Â« tout / grille seule / rien Â»), ce qui obligeait
		// a choisir la combinaison la moins mauvaise au lieu de composer la sienne.
		//
		// Chaque entree est un bit du masque `st.overlayMask`.
		inline const char *const *NkOverlayItems(int32 &n) {
			// CHAQUE CASE PILOTE UN REGLAGE REEL de la demo portee : la grille
			// infinie et ses trois familles de traits (touches F1..F4), le lisere
			// de selection, et le HUD texte. Les entrees sans effet (Â« Normales Â»,
			// Â« Origines Â»...) sont retirees plutot que decoratives.
			static const char *const k[] = {
				"Grille",				// la grille infinie (F1)
				"Lignes fines",			// subdivisions internes (F2)
				"Lignes majeures",		// lignes principales (F3)
				"Axes du plan",			// X rouge / Z bleu sur le sol (F4)
				"Contour de selection", // le lisere orange
				"HUD de la demo",		// les panneaux texte en surimpression
				"Curseur 3D",			// le viseur rouge et blanc (repere de travail)
			};
			n = 7;
			return k;
		}
		inline const NkIcon *NkOverlayIcons() {
			static const NkIcon k[] = {NkIcon::SnapGrid, NkIcon::Dot,	 NkIcon::Ruler,
									   NkIcon::Gizmo,	 NkIcon::Square, NkIcon::Journal,
									   NkIcon::Cursor};
			return k;
		}

		// â”€â”€ MATCAP : UN REGLAGE DU MODE SOLIDE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Rihen l a corrige a juste titre. J en avais fait un cinquieme mode
		// d ombrage ; c est faux. Le matcap N EST PAS un mode : c est la maniere
		// dont le mode SOLIDE eclaire la surface. Blender fait exactement cela --
		// Â« Solid Â» ouvre un panneau ou l on choisit entre eclairage studio, matcap
		// et couleur plate.
		// Consequence pratique : le selecteur de matcap n apparait que si l ombrage
		// est SOLIDE, et il reste disponible en mode objet comme en edition.
		inline const char *const *NkSolidLightItems(int32 &n) {
			// La SOURCE DE COULEUR des modes non eclaires (Solide / Fil de fer),
			// c'est-a-dire le reglage REEL de la demo (sa touche B) : couleur du
			// materiau, gris d'atelier, ou couleur choisie. L'ancienne liste
			// Â« Studio / Matcap / Plat Â» promettait des eclairages qui n'existent
			// pas -- le matcap est TOUJOURS l'eclairage de ces modes.
			static const char *const k[] = {"Couleur du materiau", "Gris d'atelier",
											"Couleur personnalisee"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSolidLightIcons() {
			static const NkIcon k[] = {NkIcon::Material, NkIcon::Dot, NkIcon::Picker};
			return k;
		}
		inline const char *const *NkOrientItems(int32 &n) {
			// LES SEPT ORIENTATIONS de Blender (capture de Rihen). Gimbal,
			// Curseur et Parent recoivent leur repere de l'hote ; Vue est
			// calculee par le gizmo depuis la camera. Aucune n'est decorative :
			// celles qui n'ont pas de repere retombent sur Local, ce que le
			// libelle ne promet pas autrement.
			static const char *const k[] = {"Monde",  "Local", "Normale", "Gimbal",
											"Vue",	  "Curseur", "Parent"};
			n = 7;
			return k;
		}
		inline const NkIcon *NkOrientIcons() {
			static const NkIcon k[] = {NkIcon::Globe,  NkIcon::Mesh,   NkIcon::Ruler,
									   NkIcon::Rotate, NkIcon::Camera, NkIcon::Cursor,
									   NkIcon::Link2};
			return k;
		}
		inline const char *const *NkCamSpeedItems(int32 &n) {
			// Vitesse de deplacement de la camera, comme le Â« 4 Â» d'Unreal. Sans elle,
			// une scene de 200 metres se parcourt au pas et une piece se traverse d'un
			// coup -- c'est le meme reglage qui rend les deux supportables.
			static const char *const k[] = {"Vitesse 1", "Vitesse 2", "Vitesse 4", "Vitesse 8"};
			n = 4;
			return k;
		}
		inline const char *const *NkSelectModeItems(int32 &n) {
			static const char *const k[] = {"Sommet", "Arete", "Face"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSelectModeIcons() {
			static const NkIcon k[] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
			return k;
		}
		// â”€â”€ AJOUTER, PAR CATEGORIES -- comme Blender â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Â« Ajouter Â» n'est pas une liste de cubes : c'est un menu de CREATION, et
		// chaque categorie repond a une question differente -- une geometrie a
		// modeler, une source de lumiere, un point de vue, un repere d'assemblage.
		// Chaque entree porte le couple (type, primitive) attendu par
		// Viewport3DAddObject : le menu ne connait AUCUNE logique de creation.
		struct NkAddEntry {
				const char *label;
				NkIcon icon;
				int32 type;
				int32 prim;
		};
		struct NkAddCategory {
				const char *name;
				NkIcon icon;
				const NkAddEntry *items;
				int32 count;
		};
		inline const NkAddCategory *NkAddCategories(int32 &n) {
			// Chaque entree porte (nature, sous-type) pour Demo3DHostAddNode :
			// 1 sphere, 2 cube, 3 plan, 4 empty, 5 lumiere, 6 texte, 7 courbe,
			// 8 surface, 9 metaball. Les maillages sans generateur (tore,
			// cylindre, icosphere...) prennent la primitive la plus proche en
			// attendant le generateur moteur ; les natures 6..9 naissent en
			// MARQUEURS types, transformables et parentables.
			static const NkAddEntry kMesh[] = {
				{"Plan", NkIcon::Plane3D, 3, 0},
				// PLAN INFINI : un VRAI maillage (sculptable plus tard en
				// montagnes/terrains), a distinguer du sol systeme du panneau
				// Rendu qui restera statique. Meme nature 3, sous-type dedie.
				{"Plan infini", NkIcon::Plane3D, 3, 3},
				{"Cube", NkIcon::Cube3D, 2, 0},
				{"Cercle", NkIcon::CircleEdge, 10, 0}, // vrai cercle FERME d'aretes
				{"Sphere UV", NkIcon::SphereUV, 1, 0},
				{"IcoSphere", NkIcon::IcoSphere, 1, 1},
				{"Cylindre", NkIcon::Cylinder, 2, 1},
				{"Cone", NkIcon::Cone, 2, 2},
				{"Tore", NkIcon::Torus, 1, 2},
				{"Capsule", NkIcon::Capsule, 1, 3},
			};
			static const NkAddEntry kLight[] = {
				{"Point light", NkIcon::Light, 5, 1},
				{"Soleil", NkIcon::Light, 5, 0},
				{"Spot", NkIcon::Light, 5, 2},
				{"Area", NkIcon::Light, 5, 3},
			};
			static const NkAddEntry kCam[] = {
				{"Camera", NkIcon::Camera, 4, 10},
			};
			// IMAGE (remplace « Reference », regle de Rihen) : reference,
			// arriere-plan et empty image = reperes ; « plan maille » = un vrai
			// plan a texturer.
			static const NkAddEntry kImage[] = {
				{"Reference", NkIcon::ImageRef, 4, 11},
				{"Arriere-plan", NkIcon::ImageRef, 4, 12},
				{"Plan maille", NkIcon::Plane3D, 3, 2},
				{"Empty image", NkIcon::ImageRef, 4, 13},
			};
			static const NkAddEntry kEmpty[] = {
				{"Axes", NkIcon::EmptyAxes, 4, 0},
				{"Fleches", NkIcon::Gizmo, 4, 1},
				{"Fleche simple", NkIcon::Gizmo, 4, 2},
				{"Cercle", NkIcon::CircleEdge, 4, 3},
				{"Cube", NkIcon::Cube3D, 4, 4},
				{"Sphere", NkIcon::SphereUV, 4, 5},
				{"Cone", NkIcon::Cone, 4, 6},
			};
			static const NkAddEntry kText[] = {
				{"Texte", NkIcon::Text3D, 6, 0},
			};
			static const NkAddEntry kCurve[] = {
				{"Bezier", NkIcon::CurveBezier, 7, 0},
				{"Cercle", NkIcon::CurveBezier, 7, 1},
				{"Courbe NURBS", NkIcon::CurveBezier, 7, 2},
				{"Cercle NURBS", NkIcon::CurveBezier, 7, 3},
				{"Chemin", NkIcon::CurveBezier, 7, 4},
				{"Empty hair", NkIcon::CurveBezier, 7, 5},
				{"Fourrure", NkIcon::CurveBezier, 7, 6},
			};
			static const NkAddEntry kSurf[] = {
				{"Courbe NURBS", NkIcon::SurfacePatch, 8, 0},
				{"Cercle NURBS", NkIcon::SurfacePatch, 8, 1},
				{"Surface NURBS", NkIcon::SurfacePatch, 8, 2},
				{"Cylindre NURBS", NkIcon::SurfacePatch, 8, 3},
				{"Sphere NURBS", NkIcon::SurfacePatch, 8, 4},
				{"Tore NURBS", NkIcon::SurfacePatch, 8, 5},
			};
			static const NkAddEntry kMeta[] = {
				{"Ball", NkIcon::Metaball, 9, 0},
				{"Capsule", NkIcon::Capsule, 9, 1},
				{"Plan", NkIcon::Plane3D, 9, 2},
				{"Ellipsoide", NkIcon::Metaball, 9, 3},
				{"Cube", NkIcon::Cube3D, 9, 4},
			};
			static const NkAddCategory kCats[] = {
				{"Maillage", NkIcon::Cube3D, kMesh, 10},
				{"Lumiere", NkIcon::Light, kLight, 4},
				{"Camera", NkIcon::Camera, kCam, 1},
				{"Image", NkIcon::ImageRef, kImage, 4},
				{"Vide", NkIcon::EmptyAxes, kEmpty, 7},
				{"Texte", NkIcon::Text3D, kText, 1},
				{"Courbe", NkIcon::CurveBezier, kCurve, 7},
				{"Surface", NkIcon::SurfacePatch, kSurf, 6},
				{"Metaball", NkIcon::Metaball, kMeta, 5},
			};
			n = 9;
			return kCats;
		}
		// â”€â”€ MODIFICATEURS, CLASSES PAR CATEGORIE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Seize modificateurs dans une liste plate obligent a la parcourir en entier
		// pour trouver le bon, et rien n'indique lesquels font des choses comparables.
		// Les trois categories repondent chacune a une question differente :
		//   GENERER  â€” ajoute de la geometrie qui n'existait pas ;
		//   DEFORMER â€” garde la meme geometrie et la deplace ;
		//   NETTOYER â€” en retire ou la reorganise.
		// C'est le classement de Blender, et il tient parce qu'il repose sur ce que le
		// modificateur FAIT au maillage, pas sur son nom.
		//
		// La liste reprend NkModifierType (NKRenderer). Elle sera GENEREE depuis
		// l'enumeration au branchement : la recopier a la main garantit qu'elle
		// divergera le jour ou un modificateur sera ajoute.
		// Chaque entree porte le TYPE attendu par le moteur (cf. NkModifierType).
		// L'interface classe par usage -- generer / deformer / nettoyer -- et cet
		// ordre n'a aucune raison de coincider avec celui de l'enumeration : le
		// type explicite evite tout calcul d'indice, donc toute divergence le jour
		// ou une categorie gagne une entree.
		struct NkModEntry {
				const char *label;
				NkIcon icon;
				int32 type; ///< NkModifierType du moteur
		};
		struct NkModCategory {
				const char *name;
				NkIcon icon;
				const NkModEntry *items;
				int32 count;
		};

		inline const NkModCategory *NkModifierCategories(int32 &n) {
			// LES DIX-SEPT MODIFICATEURS DU MOTEUR, aucun de plus. La liste
			// precedente annoncait Â« Booleen Â», Â« Enveloppe Â», Â« Remailler Â» et
			// Â« Courbe Â» qui n'existent pas : un menu qui propose ce qui n'existe
			// pas est pire qu'un menu court.
			static const NkModEntry kGen[] = {
				{"Miroir", NkIcon::Ruler, 0},		{"Reseau", NkIcon::SnapGrid, 1},
				{"Subdivision", NkIcon::Layers, 2}, {"Solidifier", NkIcon::Mesh, 3},
				{"Biseau", NkIcon::Scale, 6},		{"Vissage", NkIcon::Rotate, 7},
				{"Construction", NkIcon::Add, 10},	{"Masque", NkIcon::EyeClosed, 11},
			};
			static const NkModEntry kDef[] = {
				{"Projeter", NkIcon::Circle, 12},	 {"Deformation simple", NkIcon::Move, 13},
				{"Lisser", NkIcon::Circle, 14},		 {"Onde", NkIcon::Ruler, 15},
			};
			static const NkModEntry kClean[] = {
				{"Trianguler", NkIcon::Mesh, 4},	  {"Souder", NkIcon::Refresh, 5},
				{"Separer les aretes", NkIcon::Ruler, 8}, {"Decimer", NkIcon::Trash, 9},
				{"Ombrage par angle", NkIcon::Light, 16},
			};
			static const NkModCategory kCats[] = {
				{"Generer", NkIcon::Add, kGen, 8},
				{"Deformer", NkIcon::Move, kDef, 4},
				{"Nettoyer", NkIcon::Trash, kClean, 5},
			};
			n = 3;
			return kCats;
		}

		// Libelle du modificateur choisi, tous categories confondues.
		inline const NkModEntry *NkModifierAt(int32 flat) {
			int32 nc = 0;
			const NkModCategory *cats = NkModifierCategories(nc);
			int32 seen = 0;
			for (int32 c = 0; c < nc; ++c) {
				if (flat < seen + cats[c].count)
					return &cats[c].items[flat - seen];
				seen += cats[c].count;
			}
			return &cats[0].items[0];
		}


		// Fond de survol. UN SEUL endroit, pour que tous les elements survolables
		// reagissent pareil : un survol qui change d'aspect d'un bouton a l'autre se
		// lit comme un defaut d'affichage, pas comme une intention.
		inline void HoverFill(NkModelerPainter &p, const NkRect &r, bool on, float32 rounding = 3.f) {
			if (on)
				p.Fill(r, NkRole::PanelBg, rounding);
		}

		// â”€â”€ BARRE DE MENUS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintMenuBarI(NkModelerPainter &p, const NkRect &r, const char *projectName,
								  NkModelerState &st, NkHitRegistry &hit) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			// Poignee de deplacement : toute la barre, declaree AVANT les menus et
			// les boutons pour qu'ils la recouvrent.
			hit.Add("win.drag", r);

			const float32 logo = S(22.f);
			const float32 ly = r.y + (r.h - logo) * 0.5f;
			p.Fill({S(10.f), ly, logo, logo}, NkRole::AccentUi, 4.f);
			const float32 nkW = p.TextW("NK");
			p.TextV(S(10.f) + (logo - nkW) * 0.5f, ly, logo, "NK", NkRole::TextOnAccent);

			float32 x = S(10.f) + logo + S(16.f);
			static const char *const kMenus[] = {"Fichier", "Edition", "Fenetre", "Outils",
												 "Selection", "Objet", "Aide"};
			static const char *const kMenuKeys[] = {"menu.0", "menu.1", "menu.2", "menu.3",
													"menu.4", "menu.5", "menu.6"};
			for (int32 i = 0; i < 7; ++i) {
				const float32 w = p.TextW(kMenus[i]) + S(18.f);
				const NkRect mr{x - S(9.f), r.y + S(6.f), w, r.h - S(12.f)};
				const bool over = hit.Add(kMenuKeys[i], mr);
				// Un menu OUVERT reste marque meme si la souris est partie : sinon on ne
				// saurait plus quel menu a produit la liste affichee.
				if (st.openMenu == i)
					p.Fill(mr, NkRole::AccentUi, 3.f);
				else
					HoverFill(p, mr, over);
				p.TextV(x, r.y, r.h, kMenus[i], st.openMenu == i ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(kMenuKeys[i]))
					st.openMenu = (st.openMenu == i) ? -1 : i; // deuxieme clic = referme
				x += w;
			}

			const float32 nw = p.TextW(projectName);
			p.TextV(r.w * 0.5f - nw * 0.5f, r.y, r.h, projectName, NkRole::TextMuted);

			const float32 bw = S(30.f), bh = S(22.f);
			const float32 by = r.y + (r.h - bh) * 0.5f;
			// L'ICONE DU BOUTON CENTRAL SUIT L'ETAT DE LA FENETRE : carre quand elle
			// peut etre agrandie, deux carres superposes quand elle peut etre
			// restauree. Garder le meme dessin dans les deux cas obligerait a se
			// souvenir de ce qu'on a fait pour savoir ce que le bouton va faire.
			const NkIcon kWin[3] = {NkIcon::WinMin, st.maximized ? NkIcon::WinRestore : NkIcon::WinMax,
									NkIcon::WinClose};
			static const char *const kWinKeys[3] = {"win.min", "win.max", "win.close"};
			for (int32 i = 0; i < 3; ++i) {
				const float32 bx = r.w - kPad - (float32)(3 - i) * (bw + S(6.f));
				const NkRect br{bx, by, bw, bh};
				const bool over = hit.Add(kWinKeys[i], br);
				const bool close = (i == 2);
				if (close)
					p.Fill(br, over ? NkColor{240, 100, 85, 255} : NkColor{231, 76, 60, 255}, 3.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 3.f);
				p.IconV(bx + (bw - S(13.f)) * 0.5f, by, bh, kWin[i],
						close ? NkRole::TextOnAccent : NkRole::Text, 13.f);
			}
			if (hit.Clicked("win.min"))
				st.wantMinimize = true;
			if (hit.Clicked("win.max"))
				st.wantMaxRestore = true;
			// La FERMETURE passe par la confirmation si le document est modifie.
			// Fermer directement ferait perdre le travail sur un clic mal place, et
			// c'est le genre d'accident qu'on ne pardonne pas a un logiciel.
			if (hit.Clicked("win.close"))
				NkRequestClose(st); // meme politique que la croix de l'OS et Quitter

			// â”€â”€ DEPLACEMENT DE LA FENETRE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// La barre de titre est la poignee, mais seulement la ou elle est VIDE :
			// declarer la zone en PREMIER laisse les menus et les boutons, declares
			// ensuite, la recouvrir. C'est la regle Â« la derniere zone gagne Â» qui
			// fait le tri, sans qu'on ait a lister les exceptions.
			//
			// Le drapeau est pose ici et consomme par la boucle : BeginDragMove BLOQUE
			// (boucle modale de l'OS) et le rappeler pendant la peinture reentrerait
			// dans la frame.
			// On retient OU l'on a saisi la barre, en fraction de largeur : c'est ce
			// qui permet de replacer la fenetre sous le curseur si le glissement part
			// d'un etat maximise (cf. main.cpp).
			if (hit.Clicked("win.drag")) {
				st.wantDragMove = true;
				const float32 w = r.w > 1.f ? r.w : 1.f;
				float32 f = (hit.Mouse().x - r.x) / w;
				st.dragFracX = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
			}
		}

		// â”€â”€ ONGLETS DE DOCUMENT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// â”€â”€ ONGLETS DE SCENE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// UNE SEULE scene a l'ouverture. Demarrer sur deux onglets vides ferait croire
		// que l'un d'eux contient quelque chose, et obligerait a en fermer un avant
		// meme d'avoir commence. Le nom se modifie au DOUBLE-clic, le + en ajoute une.
		// Un MODEL et ses MESH sont deux natures distinctes : le conteneur
		// s'appelle model, sa matiere reste des maillages. Le premier mesh
		// prend un nom independant -- renommer l'un ne renomme pas l'autre.
		inline void NkHierComposeName(NkModelerState &st, const char *base0, int32 newNode);
		inline int32 NkModelFirstMesh(NkModelerState &st, int32 root) {
			const int32 m = demo::Demo3DHostEnsureModelMesh(root);
			if (m >= 0 && m < 176 && st.customNames[m][0] == 0)
				NkHierComposeName(st, "Mesh", m);
			return m;
		}
		// ── MARQUER « MODIFIE » ─────────────────────────────────────────────────
		// POINT DE PASSAGE UNIQUE. L'etat vit a DEUX niveaux qui doivent rester
		// d'accord : la scene (pour le marqueur d'onglet et la protection a la
		// fermeture) et le projet (pour l'invite d'enregistrement en quittant).
		// Les poser separement, c'est garantir qu'un jour l'un des deux sera
		// oublie -- et un marqueur qui ment sur du travail non enregistre coute
		// exactement ce qu'il devait proteger.
		inline void NkMarkDirty(NkModelerState &st) {
			st.dirty = true;
			const int32 dD = st.TabDoc(st.activeTab);
			if (dD >= 0)
				st.docDirty[dD] = true;
			// L'ambiance suit : les objets utilisateur occludent le GI voxel, et
			// « la scene a change » est UNE seule notion -- la pastille et la
			// grille d'occlusion se rafraichissent sur le meme evenement.
			demo::Demo3DHostGIMarkDirty();
		}

		/// Apres un enregistrement REUSSI : plus rien n'est en attente.
		/// L'ARBRE a change : cartes creees, renommees, deplacees, supprimees.
		/// POINT DE PASSAGE UNIQUE, comme NkMarkDirty pour les documents.
		inline void NkMarkTreeDirty(NkModelerState &st) {
			st.treeDirty = true;
			st.dirty = true;
		}

		inline void NkClearDirty(NkModelerState &st) {
			st.dirty = false;
			st.treeDirty = false;
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d)
				st.docDirty[d] = false;
		}

		/// Apres l'enregistrement d'UN SEUL document : lui seul redevient propre.
		/// Le projet reste « modifie » tant qu'un autre document l'est -- sinon
		/// l'invite de fermeture laisserait partir du travail non enregistre, ce
		/// qui est exactement ce que la pastille promet d'eviter.
		inline void NkClearDirtyDoc(NkModelerState &st, int32 d) {
			if (d >= 0 && d < NkModelerState::kMaxDocs)
				st.docDirty[d] = false;
			// L'ARBRE est reecrit a CHAQUE enregistrement, meme quand un seul
			// fichier est ecrit : il porte les chemins, et un fichier ecrit dont
			// l'arbre ignorerait le chemin serait introuvable a la reouverture.
			st.treeDirty = false;
			bool any = false;
			for (int32 i = 0; i < NkModelerState::kMaxDocs && !any; ++i)
				any = st.docUsed[i] && !st.docTransient[i] && st.docDirty[i];
			st.dirty = any;
		}

		/// Carte du navigateur que l'onglet actif ENREGISTRE. Une scene passe par
		/// son document, un editeur d'asset par l'asset qu'il montre : les deux
		/// repondent a la meme question -- « quel fichier suis-je en train de
		/// regarder ? ».
		inline int32 NkActiveCard(const NkModelerState &st) {
			if (st.activeTab < 0 || st.activeTab >= st.sceneCount)
				return -1;
			if (st.sceneTabKind[st.activeTab] != 0) {
				const int32 a = st.sceneTabAsset[st.activeTab] - 1;
				return (a >= 0 && a < st.browserCount) ? a : -1;
			}
			const int32 d = st.TabDoc(st.activeTab);
			if (d < 0)
				return -1;
			const int32 c = st.docCard[d] - 1;
			return (c >= 0 && c < st.browserCount) ? c : -1;
		}

		inline void NkStoreSceneView(NkModelerState &st, int32 tab) {
			const int32 d = st.TabDoc(tab);
			if (d < 0)
				return;
			float32 *cp = st.docCamPose[d];
			bool ortho = false;
			demo::Demo3DHostGetCameraPose(cp, &cp[3], &cp[4], &cp[5], &ortho);
			st.docCamOrtho[d] = ortho;
			st.docCamSet[d] = true;
			// La vue, c'est aussi ce qu'on y AFFICHE (Rihen, 10 aout) : ombrage,
			// surimpressions et fond partent avec le document, pas avec l'onglet.
			NkModelerState::NkDocView &v = st.docView[d];
			v.ombrage = st.shading;
			v.lumiereUnie = st.solidLight;
			v.surimpressions = st.overlayMask;
			v.fond = st.bgChoice;
			v.fondType = st.bgType;
			v.fondLum = st.bgBrightness;
			for (int32 a = 0; a < 3; ++a)
				v.fondPerso[a] = st.bgCustom[a];
			st.docViewSet[d] = true;
		}

		// ── FERMER UN ONGLET ────────────────────────────────────────────────────
		// FERMER UNE VUE NE SUPPRIME RIEN (regle absolue de Rihen, 8 aout 2026).
		// Le document reste dans la table du projet, sa carte reste dans le
		// navigateur, et un double-clic dessus le ROUVRE avec son contenu. Avant,
		// l'onglet ETAIT la scene : sa fermeture l'effacait de l'enregistrement
		// suivant, en laissant ses noeuds orphelins dans le fichier.
		//
		// SEULE EXCEPTION, qui n'en est pas une : un document TRANSITOIRE (maquette
		// d'editeur d'asset, onglet d'isolation) n'a jamais rien ete d'autre que la
		// vue elle-meme. L'asset qu'il montrait, lui, vit dans le navigateur.
		inline void NkActivateTab(NkModelerState &st, int32 tb, bool force);
		inline void NkCloseSceneTab(NkModelerState &st, int32 i) {
			if (i < 0 || i >= st.sceneCount || st.sceneCount <= 1)
				return;
			const int32 d = st.TabDoc(i);
			// LA VUE DE L'ONGLET ACTIF n'est rangee qu'a la bascule : sans ce
			// rangement, fermer l'onglet sur lequel on vient de travailler perdrait
			// le regard qu'on venait d'y poser -- et ses unites.
			if (d >= 0 && i == st.activeTab) {
				if (st.sceneTabKind[i] == 0)
					NkStoreSceneView(st, i);
				st.docUnitSys[d] = st.unitSystem;
				st.docUnitLen[d] = st.unitLength;
				st.docUnitScale[d] = st.unitScale;
			}
			// Le noeud edite QUITTE la vue AVANT qu'elle disparaisse -- ferme en
			// arriere-plan, son sous-arbre restait echoue dans un document mort,
			// donc introuvable. Un ASSET est rearchive (il n'a pas de scene ou
			// rentrer), un noeud ISOLE retourne dans la sienne.
			if (d >= 0 && st.docIsoNode[d] > 0) {
				if (st.docAssetEdit[d])
					demo::Demo3DHostArchiveTree(st.docIsoNode[d] - 1, true);
				else
					demo::Demo3DHostMoveTreeScene(st.docIsoNode[d] - 1,
												  (int32)st.docIsoHome[d]);
			}
			if (d >= 0 && st.docTransient[d])
				st.DocFree(d);
			for (int32 k = i; k + 1 < st.sceneCount; ++k) {
				st.sceneTabKind[k] = st.sceneTabKind[k + 1];
				st.sceneTabAsset[k] = st.sceneTabAsset[k + 1];
				st.sceneTabDoc[k] = st.sceneTabDoc[k + 1];
			}
			st.sceneCount--;
			st.sceneTabDoc[st.sceneCount] = -1;
			// L'ACTIF suit : ferme avant lui son index recule ; ferme LUI-MEME,
			// l'environnement du nouvel actif s'applique.
			const bool wasAct = (i == st.activeTab);
			if (i < st.activeTab)
				st.activeTab--;
			if (st.activeTab >= st.sceneCount)
				st.activeTab = st.sceneCount - 1;
			if (wasAct)
				NkActivateTab(st, st.activeTab, true);
		}

		// ── LE NAVIGATEUR REFLETE LES SCENES DU PROJET ──────────────────────────
		// Chaque document NON TRANSITOIRE obtient (ou met a jour) sa carte
		// « Scene » (kind 5) : c'est par son double-clic qu'on rouvre une scene
		// fermee (demande de Rihen : « on ne voit pas la scene sauvegardee dans le
		// navigateur »).
		//
		// On itere les DOCUMENTS, plus les onglets. Iterer les onglets ne donnait
		// de carte qu'aux scenes OUVERTES : les autres n'apparaissaient nulle part,
		// ce qui les rendait irrecuperables.
		inline void NkBrowserSyncScenes(NkModelerState &st) {
			for (int32 d = 0; d < NkModelerState::kMaxDocs; ++d) {
				if (!st.docUsed[d] || st.docTransient[d])
					continue;
				int32 e = st.docCard[d] - 1;
				if (e < 0 || e >= st.browserCount || st.browserKind[e] != 5 ||
					st.browserDoc[e] != d + 1) {
					if (st.browserCount >= NkModelerState::kMaxBrowser)
						continue; // navigateur plein : la scene reste sans carte
					e = st.browserCount++;
					st.browserKind[e] = 5;
					// A la RACINE : previsible tant que le selecteur de dossier
					// personnalise n'existe pas (chantier suivant) ; la carte se
					// range ensuite par glisser-deposer comme les autres.
					st.browserParent[e] = -1;
					st.browserSrcNode[e] = 0;
					st.browserSub[e] = 0;
					st.browserDoc[e] = d + 1;
					st.docCard[d] = e + 1;
				}
				NkWidgetState::Copy(st.browserNames[e], st.docName[d], 31u);
			}
		}
		// ── ACTIVER UN ONGLET ───────────────────────────────────────────────────
		// Un onglet EDITEUR est une SCENE A PART ENTIERE (Rihen) : la vue se vide
		// et une MAQUETTE de l'asset nait a l'origine, editable avec les memes
		// outils. En quittant l'editeur, la maquette disparait.
		inline void NkActivateTab(NkModelerState &st, int32 tb, bool force = false) {
			if (tb < 0 || tb >= st.sceneCount || (tb == st.activeTab && !force))
				return;
			const int32 dOld = st.TabDoc(st.activeTab);
			const int32 d = st.TabDoc(tb);
			if (d < 0)
				return; // onglet sans document : il n'y a rien a montrer
			if (st.sceneTabKind[st.activeTab] == 0 && tb != st.activeTab)
				NkStoreSceneView(st, st.activeTab); // la scene quittee garde sa vue
			// Quitter une vue d'edition : l'ASSET est rearchive, le noeud ISOLE
			// rentre dans sa scene. Meme regle qu'a la fermeture de l'onglet --
			// c'est le meme geste vu de deux endroits.
			if (dOld >= 0 && st.docIsoNode[dOld] > 0 && tb != st.activeTab) {
				if (st.docAssetEdit[dOld])
					demo::Demo3DHostArchiveTree(st.docIsoNode[dOld] - 1, true);
				else
					demo::Demo3DHostMoveTreeScene(st.docIsoNode[dOld] - 1,
												  (int32)st.docIsoHome[dOld]);
			}
			// CHAQUE SCENE A SES PROPRIETES : celles du quitte sont rangees.
			if (dOld >= 0) {
				st.docUnitSys[dOld] = st.unitSystem;
				st.docUnitLen[dOld] = st.unitLength;
				st.docUnitScale[dOld] = st.unitScale;
			}
			// LES REGLAGES RENDU AUSSI (par scene, Rihen 10 aout) : requete
			// DIFFEREE — le gestionnaire de projet capture l'etat du document
			// quitte puis applique l'instantane de l'active (l'etat vivant est
			// global a la vue, il n'a pas encore bouge au moment du differe).
			// Seules les SCENES possedent ces reglages, pas les editeurs d'asset.
			if (st.sceneTabKind[st.activeTab] == 0 && dOld >= 0)
				st.renduSwitchFrom = dOld;
			if (st.sceneTabKind[tb] == 0)
				st.renduSwitchTo = d;
			if (st.editPreviewNode > 0) {
				demo::Demo3DHostDeleteNode(st.editPreviewNode - 1, true);
				st.editPreviewNode = 0;
			}
			st.activeTab = tb;
			// BASCULE DE DOCUMENT : l'hote ne rend et ne liste plus que les
			// noeuds de CE document ; la selection ne traverse jamais.
			demo::Demo3DHostSetActiveScene((int32)st.docScene[d]);
			// L'hote doit savoir s'il sert un MODEL : la selection en vue 3D
			// n'y a pas la meme regle (mesh par mesh, contre model entier).
			demo::Demo3DHostSetDocIsModel(st.sceneTabKind[tb] == 7);
			demo::Demo3DHostDeselectAll();
			st.unitSystem = st.docUnitSys[d];
			st.unitLength = st.docUnitLen[d];
			st.unitScale = st.docUnitScale[d];
			if (st.unitScale < 0.001f)
				st.unitScale = 1.f; // document jamais visite : valeurs par defaut
			const uint8 tk = st.sceneTabKind[tb];
			if (tk == 0) {
				if (st.docCamSet[d])
					demo::Demo3DHostSetCameraPose(st.docCamPose[d], st.docCamPose[d][3],
												  st.docCamPose[d][4], st.docCamPose[d][5],
												  st.docCamOrtho[d]);
				else
					demo::Demo3DHostResetView();
				// L'AFFICHAGE de la scene revient avec elle : la synchronisation de
				// main.cpp poussera vers l'hote ce qui a change. Un document jamais
				// visite recoit les defauts d'ouverture — pas l'affichage de la
				// scene qu'on quitte.
				{
					const NkModelerState::NkDocView v =
						st.docViewSet[d] ? st.docView[d] : NkModelerState::NkDocView{};
					st.shading = v.ombrage;
					st.solidLight = v.lumiereUnie;
					st.overlayMask = v.surimpressions;
					st.bgChoice = v.fond;
					st.bgType = v.fondType;
					st.bgBrightness = v.fondLum;
					for (int32 a = 0; a < 3; ++a)
						st.bgCustom[a] = v.fondPerso[a];
				}
				return; // l'appartenance filtre deja les objets de la scene
			}
			// EDITEUR : scene VIDE + l'asset lui-meme.
			demo::Demo3DHostResetView(); // document neuf : vue d'ouverture
			const uint8 ek = (uint8)(tk - 1);
			const int32 ai = st.sceneTabAsset[tb] - 1;
			// ── L'ONGLET EDITE LE NOEUD LUI-MEME, JAMAIS UNE COPIE ──────────
			// Deux chemins y menent et ils partagent tout sauf la sortie :
			//   * ISOLATION -- le noeud est un objet de SCENE, il y retourne ;
			//   * ASSET du navigateur -- le noeud est une ARCHIVE, desarchivee le
			//     temps de la vue et rearchivee en sortant.
			// L'editeur d'asset travaillait auparavant sur un DUPLICATA detruit a
			// la fermeture : tout ce qu'on y faisait etait perdu, position comprise
			// (constate par Rihen). Un editeur qui n'edite rien est pire que pas
			// d'editeur du tout.
			if (st.docIsoNode[d] == 0 && ek == 6 && ai >= 0 && ai < st.browserCount) {
				int32 src = st.browserSrcNode[ai] - 1;
				if (src < 0) {
					// Carte creee par « + Model » : elle n'a pas encore de corps.
					// Il nait ICI et devient LE corps de la carte -- sinon chaque
					// ouverture repartait d'un cube neuf.
					src = demo::Demo3DHostAddNode(2, 0);
					if (src >= 0)
						st.browserSrcNode[ai] = src + 1;
				}
				if (src >= 0) {
					demo::Demo3DHostArchiveTree(src, false);
					st.docIsoNode[d] = src + 1;
					st.docAssetEdit[d] = true;
				}
			}
			if (st.docIsoNode[d] > 0) {
				const int32 iso = st.docIsoNode[d] - 1;
				// REVENIR sur l'onglet : l'asset a ete rearchive en le quittant,
				// il faut le remettre dans la vue. Sans cela, rouvrir l'editeur
				// d'un Model montrait une scene vide.
				if (st.docAssetEdit[d])
					demo::Demo3DHostArchiveTree(iso, false);
				demo::Demo3DHostMoveTreeScene(iso, (int32)st.docScene[d]);
				// Le seul parent d'un model est le model : ses maillages
				// reviennent tous a plat sous lui (Rihen).
				demo::Demo3DHostFlattenModel(iso);
				NkModelFirstMesh(st, iso); // le model et sa matiere
				demo::Demo3DHostSelectEmptyNode(iso);
				st.editPreviewNode = 0;
				return;
			}
			// Les autres natures d'asset (materiau, texture, graphe...) n'ont pas
			// encore de corps editable : leur onglet reste une scene vide plutot
			// qu'une maquette qui ferait croire qu'on edite quelque chose.
			st.editPreviewNode = 0;
		}
		inline void PaintTabsI(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
							   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = S(10.f);
			const float32 h = r.h - 2.f;
			char key[32];
			for (int32 i = 0; i < st.sceneCount; ++i) {
				const int32 di = st.TabDoc(i);
				if (di < 0)
					continue; // onglet sans document : anomalie, on ne peint rien
				const float32 tw = p.TextW(st.docName[di]) + S(44.f);
				const NkRect tr{x, r.y + 2.f, tw, h};
				snprintf(key, sizeof(key), "tab.%d", i);
				const bool over = hit.Add(key, tr);
				const bool on = (i == st.activeTab);
				p.Fill(tr, on ? NkRole::PanelHeader : (over ? NkRole::PanelBg : NkRole::InputBg), 3.f);
				if (st.sceneTabKind[i] != 0) {
					// Liseret de NATURE : distinguer d'un oeil les onglets EDITEUR
					// des scenes. La couleur vient du MEME point de passage que les
					// cartes du navigateur -- c'est la meme nature vue a deux
					// endroits, elle ne doit pas pouvoir en avoir deux couleurs.
					const uint8 k2 = (uint8)(st.sceneTabKind[i] - 1);
					const int32 aT = st.sceneTabAsset[i] - 1;
					const uint8 sT = (aT >= 0 && aT < st.browserCount) ? st.browserSub[aT] : 0;
					p.Fill({tr.x, tr.y, tr.w, 2.f}, NkAssetColor(p, k2, sT)); // en HAUT (Rihen)
				}
				snprintf(key, sizeof(key), "tab.name.%d", i);
				if (EditableText(p, hit, ws, in, key, {x + S(10.f), r.y, tw - S(32.f), r.h},
								 st.docName[di], on ? NkRole::Text : NkRole::TextMuted,
								 st.docName[di], 32u)) {
					// RENOMMER L'ONGLET RENOMME SA CARTE, TOUT DE SUITE. Le nom ne
					// se propageait qu'a l'enregistrement (via NkBrowserSyncScenes),
					// si bien que le navigateur affichait l'ancien nom entre-temps
					// (constate par Rihen). Symetrique du renommage depuis la carte :
					// chaque sens agit A LA VALIDATION, jamais en continu -- une
					// recopie a chaque frame ferait qu'un cote ecraserait l'autre.
					const int32 e9 = st.docCard[di] - 1;
					if (st.sceneTabKind[i] == 0 && e9 >= 0 && e9 < st.browserCount &&
						st.browserKind[e9] == 5)
						NkWidgetState::Copy(st.browserNames[e9], st.docName[di], 31u);
				}
				// PASTILLE ET CROIX PARTAGENT LE MEME EMPLACEMENT, mais leurs
				// conditions different -- et c'est important : la pastille « non
				// enregistre » vaut pour TOUTE scene, y compris la DERNIERE, alors
				// que la croix n'apparait que s'il reste plus d'une scene (fermer
				// la derniere laisserait l'application sans document). L'ancienne
				// imbrication mettait la pastille SOUS la condition de la croix :
				// avec une seule scene -- le cas le plus courant -- aucune
				// modification n'etait jamais signalee (constate par Rihen).
				{
					snprintf(key, sizeof(key), "tab.close.%d", i);
					const NkRect cr{x + tw - S(24.f), r.y + 2.f, S(20.f), h};
					const bool canClose = st.sceneCount > 1;
					const bool overClose = canClose && hit.Add(key, cr);
					// MARQUEUR « NON ENREGISTRE » (Rihen) : une PASTILLE prend la
					// place de la croix tant que la souris n'est pas dessus. MEME
					// emplacement, donc la largeur de l'onglet ne bouge pas quand une
					// scene devient modifiee -- des onglets qui changent de taille a
					// la premiere frappe rendraient la barre illisible. Au survol la
					// croix revient : on ferme sans avoir a viser ailleurs.
					if (st.docDirty[di] && !overClose) {
						const float32 d = S(7.f);
						p.Fill({cr.x + (cr.w - d) * 0.5f, cr.y + (cr.h - d) * 0.5f, d, d},
							   on ? NkRole::Text : NkRole::TextMuted, d * 0.5f);
					} else if (canClose) {
						HoverFill(p, cr, overClose, 2.f);
						p.IconV(x + tw - S(20.f), r.y, r.h, NkIcon::WinClose, NkRole::TextMuted,
								10.f);
					}
					if (canClose && hit.Clicked(key)) {
						// LA CROIX FERME, SANS RIEN DEMANDER. Il n'y a plus rien a
						// proteger : le document reste dans le projet et sa carte dans
						// le navigateur. L'ancienne boite « Fermer sans enregistrer »
						// disait vrai a l'epoque ou l'onglet ETAIT la scene ; la
						// garder maintenant ferait redouter une perte qui n'existe
						// plus. La pastille reste : elle dit que le PROJET n'est pas
						// enregistre, ce qui est toujours exact.
						NkCloseSceneTab(st, i);
						break; // la liste a change
					}
				}
				snprintf(key, sizeof(key), "tab.%d", i);
				{
					// Le clic sur le NOM bascule AUSSI l'onglet : la zone du nom
					// recouvre celle de l'onglet et lui VOLAIT le survol --
					// selectionner une scene exigeait de viser les bords (Rihen).
					char nk2[32];
					snprintf(nk2, sizeof(nk2), "tab.name.%d", i);
					if (hit.Clicked(key) || (!ws.IsEditing(nk2) && hit.Clicked(nk2)))
						NkActivateTab(st, i);
				}
				x += tw + 3.f;
				// MARQUEUR DE SEPARATION entre en-tetes d'onglets (regle de
				// Rihen) : sans lui, deux scenes cote a cote se confondaient.
				if (i + 1 < st.sceneCount) {
					p.VLine(x + 1.f, r.y + S(6.f), h - S(8.f));
					x += S(5.f);
				}
			}
			const NkRect ar{x + S(4.f), r.y + 2.f, S(24.f), h};
			HoverFill(p, ar, hit.Add("tab.add", ar));
			p.IconV(x + S(8.f), r.y, r.h, NkIcon::Add, NkRole::Text, 12.f);
			if (hit.Clicked("tab.add") && st.sceneCount < 8) {
				// UN NOUVEAU DOCUMENT, puis une vue dessus. Le nom par defaut est
				// NUMEROTE d'apres le nombre de documents et non d'onglets : numeroter
				// par onglet redonnait « Scene_2 » a une scene creee apres en avoir
				// ferme une -- deux scenes homonymes dans le meme projet.
				const int32 nd = st.DocAlloc();
				if (nd >= 0) {
					int32 used = 0;
					for (int32 q = 0; q < NkModelerState::kMaxDocs; ++q)
						if (st.docUsed[q] && !st.docTransient[q])
							++used;
					snprintf(st.docName[nd], 32, "Scene_%d", (int)used);
					// Une scene NEUVE nait VIERGE : les objets de la demo
					// appartiennent a la premiere scene.
					st.docBlank[nd] = true;
					st.docScene[nd] = (uint8)st.sceneIdNext++;
					const int32 nt = st.sceneCount++;
					st.sceneTabKind[nt] = 0;
					st.sceneTabAsset[nt] = 0;
					st.sceneTabDoc[nt] = nd;
					// LA CARTE NAIT AVEC LA SCENE, pas a l'enregistrement : une scene
					// qu'on ferme avant d'avoir enregistre doit rester retrouvable.
					NkBrowserSyncScenes(st);
					NkActivateTab(st, nt);
				}
			}
		}

		// â”€â”€ BARRE D'OUTILS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// LE MODE EST UN DEROULANT, PLUS UN COMMUTATEUR A DEUX ETATS. Rihen a
		// remarque que Â« Objet Â» et Â« Edition Â» faisaient doublon avec Â« Mode de
		// selection Â» -- et il a raison sur le fond : ce ne sont pas deux boutons,
		// c'est UNE liste de modes, qui va s'allonger (sculpt 2.5D, sculpt reel,
		// texturing, riggging...). Deux boutons cotes a cote auraient cesse de tenir
		// au troisieme mode.
		// Â« Mode de selection Â» reste a cote, et ne fait PAS doublon : il porte le
		// sous-mode sommet / arete / face, qui n'a de sens qu'EN edition.
		inline void PaintToolbar(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, NkComboPending &combo) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			// Editeurs sans design defini : pas de barre d'outils (Rihen).
			if (st.sceneTabKind[st.activeTab] != 0 &&
				st.sceneTabKind[st.activeTab] != 7)
				return;
			float32 x = kPad;
			const float32 ih = S(14.f);

			// Bouton icone + libelle, avec sa zone sensible. Une seule fonction pour
			// que tous reagissent pareil.
			auto btn = [&](const char *key, NkIcon ic, const char *label) -> bool {
				const float32 w = ih + S(5.f) + p.TextW(label) + S(14.f);
				const NkRect br{x - S(7.f), r.y + S(5.f), w, r.h - S(10.f)};
				const bool over = hit.Add(key, br);
				HoverFill(p, br, over);
				p.IconV(x, r.y, r.h, ic, NkRole::Text, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, label);
				x += w;
				return hit.Clicked(key);
			};

			// Le retour du bouton etait JETE : « Enregistrer » peignait son icone
			// et ne faisait RIEN (constate par Rihen). Il passe par le meme
			// differe que le menu Fichier -- une seule voie d'enregistrement.
			//
			// LE BOUTON EST AUSSI UN SIGNAL (demande de Rihen, facon Unreal) :
			// tant qu'il existe du travail non enregistre, il s'affiche en ACCENT
			// -- l'oeil le voit sans chercher la pastille de l'onglet. Peint
			// AVANT le btn() pour que le survol garde son retour visuel.
			if (st.dirty) {
				const float32 wS = ih + S(5.f) + p.TextW("Enregistrer") + S(14.f);
				p.Fill({x - S(7.f), r.y + S(5.f), wS, r.h - S(10.f)}, NkRole::AccentUi, 3.f);
				if (btn("tb.save", NkIcon::Save, "Enregistrer"))
					st.projPending = 3;
			} else {
				// RIEN A ENREGISTRER = BOUTON INERTE (Rihen, 10 aout) : pas de
				// zone cliquable, icone et libelle en sourdine — l'etat se lit
				// d'un coup d'oeil, et un clic sans effet n'existe plus.
				const float32 w = ih + S(5.f) + p.TextW("Enregistrer") + S(14.f);
				p.IconV(x, r.y, r.h, NkIcon::Save, NkRole::TextMuted, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, "Enregistrer", NkRole::TextMuted);
				x += w;
			}
			p.VLine(x - S(4.f), r.y + S(7.f), r.h - S(14.f));
			x += S(6.f);

			// â”€â”€ DEROULANT DE MODE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Le deroulant a QUITTE la barre : les ESPACES au-dessus de la vue
			// portent desormais Objet / Edition / Sculpture 2.5D / Sculpture /
			// Texturing -- un seul endroit pour changer de mode (regle de
			// Rihen, le doublon barre + onglets pretait a confusion).
			const float32 cbH = S(22.f), cbY = r.y + (r.h - cbH) * 0.5f;

			// LE Â« MODE DE SELECTION Â» A ETE RETIRE D ICI. Rihen a demande a quoi
			// servait le Â« Face Â» a cote d Â« Objet Â» : c etait le sous-mode
			// sommet/arete/face, exactement la meme chose que les trois boutons de la
			// barre de la vue. Un doublon que j avais introduit sans le voir.
			// Il reste dans la VUE, ou il est a sa place : c est la qu on selectionne,
			// et c est la qu on doit pouvoir changer de sous-mode sans traverser
			// l ecran.

			// AJOUTER et MODIFICATEUR sont des LISTES, pas des boutons : ils ouvrent un
			// choix. Un bouton simple laisserait croire a une action immediate.
			{
				// Â« Ajouter Â» OUVRE LE MENU PAR CATEGORIES, il ne retient pas de
				// Â« primitive courante Â» : ajouter est une action, pas un reglage.
				const NkRect ar{x, cbY, S(96.f), cbH};
				const bool over = hit.Add("tb.addmenu", ar);
				const bool open = ws.ComboOpen("tb.addmenu");
				// ENCADRE comme tous les deroulants de la barre : sans cadre il se
				// lisait comme une etiquette, pas comme une commande.
				p.Outline(ar, (over || open) ? NkRole::AccentUi : NkRole::Border,
						  open ? NkRole::AccentUi : NkRole::InputBg, 3.f);
				const NkRole fg = open ? NkRole::TextOnAccent : NkRole::Text;
				p.IconV(ar.x + S(8.f), cbY, cbH, NkIcon::Add, fg, 13.f);
				p.TextV(ar.x + S(26.f), cbY, cbH, "Ajouter", fg);
				p.IconV(ar.x + ar.w - S(16.f), cbY, cbH,
						open ? NkIcon::ChevronUp : NkIcon::ChevronDown, fg, 11.f);
				if (hit.Clicked("tb.addmenu")) {
					ws.ToggleCombo("tb.addmenu");
					st.addParentNode = -1; // depuis la barre : naissance a la racine
					st.addAnchor = ar; // le menu est peint apres tout le reste
				}
				x += S(104.f);
			}
			// (Le deroulant MODIFICATEUR a quitte la barre lui aussi : la
			// pastille Modificateur du panneau Proprietes porte deja l'ajout --
			// un seul endroit, regle de Rihen.)

			// Â« Ajouter Â» et Â« Modificateur Â» etaient ecrits DEUX FOIS : une fois en
			// deroulant (ci-dessus, celui qui marche) et une fois en bouton plat ici.
			// Le doublon est retire -- deux commandes identiques cote a cote font
			// douter qu'elles fassent la meme chose.

			// Reglages cale a DROITE : action de session et non de modelisation, la
			// distance visuelle dit cette difference de nature.
			{
				const float32 sw = ih + S(5.f) + p.TextW("Reglages");
				const NkRect br{r.w - kPad - sw - S(7.f), r.y + S(5.f), sw + S(14.f), r.h - S(10.f)};
				HoverFill(p, br, hit.Add("tb.settings", br));
				p.IconV(r.w - kPad - sw, r.y, r.h, NkIcon::Gear, NkRole::Text, 14.f);
				p.TextV(r.w - kPad - sw + ih + S(5.f), r.y, r.h, "Reglages");
			}
		}

		// â”€â”€ EN-TETE DE PANNEAU â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Onglet + croix, comme la maquette. Rendu ici une seule fois : quatre
		// panneaux le partagent, et il n'y a donc qu'un endroit a corriger.
		// La croix de l'en-tete REFERME LE PANNEAU. Elle etait dessinee mais morte,
		// et c'est justement la contrepartie de la largeur minimale : puisqu'on ne
		// peut plus reduire un panneau jusqu'a le faire disparaitre, il faut un geste
		// franc pour le fermer -- et une poignee visible pour le rouvrir.
		//
		// `show` peut etre nul : certains en-tetes (l'onglet d'un panneau qui ne se
		// ferme pas) gardent la croix desactivee plutot que de la faire disparaitre,
		// pour que la rangee d'en-tetes reste alignee.
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title,
									 NkHitRegistry *hit = nullptr, bool *show = nullptr,
									 const char *key = nullptr) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			const NkRect cr{r.x + r.w - 24.f, r.y + 3.f, 20.f, h - 6.f};
			bool over = false;
			if (hit && show && key) {
				over = hit->Add(key, cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				if (hit->Clicked(key))
					*show = false;
			}
			p.IconV(r.x + r.w - 20.f, r.y, h, NkIcon::WinClose,
					over ? NkRole::TextOnAccent : NkRole::Text, 11.f);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
		}

		// â”€â”€ POIGNEE DE REOUVERTURE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Un panneau ferme doit laisser une trace : sans elle, l'utilisateur qui a
		// clique la croix n'a plus aucun moyen de deviner comment revenir en arriere,
		// et le menu n'est pas un moyen de DEVINER -- c'est un moyen de retrouver ce
		// qu'on sait deja exister.
		// Le chevron pointe vers l'endroit ou le panneau reapparaitra.
		inline void PaintPanelHandle(NkModelerPainter &p, const NkRect &r, NkHitRegistry &hit,
									 const char *key, bool &show, NkIcon icon) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const bool over = hit.Add(key, r);
			p.Fill(r, over ? NkRole::AccentUi : NkRole::PanelHeader);
			if (r.w > r.h) {
				p.HLine(r.x, r.y, r.w);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y, r.h, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			} else {
				p.VLine(r.x + (r.x < 4.f ? r.w - 1.f : 0.f), r.y, r.h);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y + r.h * 0.5f - 13.f, 26.f, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			}
			if (over)
				hit.WantCursor(NkCursorWant::Hand);
			if (hit.Clicked(key))
				show = true;
		}

		// Champ de recherche, avec sa loupe. Sans l'icone, un champ vide au texte
		// grise se confond avec une etiquette desactivee.
		// CHAMP DE RECHERCHE REEL : bordure, saisie, effacement, et un filtre que
		// l'appelant applique. L'ancien etait un dessin -- une boite grise avec le
		// mot Â« Rechercher Â» -- qui ne recevait aucun clic et ne filtrait rien.
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y,
								   NkHitRegistry &hit, NkWidgetState &ws,
								   const nkgui::NkGuiInput &in, const char *key, char *buf) {
			const float32 h = 22.f;
			const NkRect fr{r.x + 6.f, y + 4.f, r.w - 12.f, h};
			// BORDURE : sans elle, le champ se confond avec le fond du panneau et
			// rien ne dit qu'on peut y ecrire.
			const bool editing = ws.IsEditing(key);
			p.Outline(fr, editing ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
			p.IconV(fr.x + 6.f, fr.y, h, NkIcon::Search, NkRole::TextMuted, 12.f);
			const NkRect tr{fr.x + 24.f, fr.y, fr.w - 48.f, h};
			if (editing) {
				p.TextV(tr.x, tr.y, h, ws.editBuf);
				const float32 cw = p.TextW(ws.editBuf);
				p.Fill({tr.x + cw + 1.f, tr.y + 3.f, 1.f, h - 6.f}, NkRole::Text);
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 c = in.chars[i];
					if (c >= 32u && c < 127u && ws.editLen < 30u) {
						ws.editBuf[ws.editLen++] = (char)c;
						ws.editBuf[ws.editLen] = 0;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
					ws.editBuf[--ws.editLen] = 0;
				// La recherche s'applique A CHAQUE FRAPPE : attendre Entree pour
				// filtrer une liste n'aurait aucun sens, on cherche justement en
				// voyant le resultat se reduire.
				NkWidgetState::Copy(buf, ws.editBuf, 31u);
				hit.Add(key, fr);
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)
					|| in.KeyPressed(nkgui::NkGuiKey::Escape)
					|| (hit.AnyClick() && !hit.IsHovered(key)))
					ws.EndEdit();
			} else {
				const bool over = hit.Add(key, fr);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (*buf)
					p.TextV(tr.x, tr.y, h, buf, NkRole::Text);
				else
					p.TextV(tr.x, tr.y, h, "Rechercher", NkRole::TextMuted);
				// SIMPLE clic, pas double : un champ de recherche s'ouvre au premier
				// clic. Le double-clic est reserve au renommage, ou il protege d'un
				// renommage accidentel -- ici il n'y a rien a proteger.
				if (hit.Clicked(key))
					ws.BeginEdit(key, buf);
			}
			// Croix d'effacement, seulement s'il y a quelque chose a effacer.
			if (*buf) {
				char ck[48];
				snprintf(ck, sizeof(ck), "%s.clear", key);
				const NkRect cr{fr.x + fr.w - 22.f, fr.y + 3.f, 18.f, h - 6.f};
				HoverFill(p, cr, hit.Add(ck, cr), 2.f);
				p.IconV(cr.x + 3.f, cr.y, cr.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
				if (hit.Clicked(ck)) {
					buf[0] = 0;
					if (editing)
						ws.EndEdit();
				}
			}
			return y + h + 8.f;
		}

		// Filtre insensible a la casse : Â« cu Â» trouve Â« Cube Â». Un filtre sensible
		// obligerait a connaitre la casse exacte de ce qu'on cherche, ce qui est
		// exactement ce qu'on ne sait pas quand on cherche.
		inline bool NkNameMatches(const char *name, const char *filter) {
			if (!filter || !*filter)
				return true;
			if (!name)
				return false;
			for (const char *a = name; *a; ++a) {
				const char *x = a;
				const char *y = filter;
				while (*x && *y) {
					char cx = *x, cy = *y;
					if (cx >= 'A' && cx <= 'Z')
						cx = (char)(cx - 'A' + 'a');
					if (cy >= 'A' && cy <= 'Z')
						cy = (char)(cy - 'A' + 'a');
					if (cx != cy)
						break;
					++x;
					++y;
				}
				if (!*y)
					return true;
			}
			return false;
		}

		// â”€â”€ HIERARCHIE (gauche) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Arbre REPLIABLE, noms MODIFIABLES, deux colonnes d'etat (oeil, cadenas), et
		// un clic dans le VIDE qui deselectionne.
		//
		// Ce dernier point compte plus qu'il n'en a l'air : sans lui, une fois un
		// objet selectionne on ne peut plus revenir a Â« rien de selectionne Â» sans
		// passer par un menu. Or Â« rien Â» est un etat legitime -- c'est celui ou les
		// commandes de scene s'appliquent.
		// La hierarchie liste LA SCENE, plus des lignes inventees. Chaque ligne est
		// un slot du viewport : les indices sont stables (tableau a trous), donc les
		// cles de zones cliquables restent valides d'une image a l'autre.
		inline NkIcon NkObjectIcon(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return NkIcon::Light;
				case nk3d::kVpObjCamera:
					return NkIcon::Camera;
				case nk3d::kVpObjEmpty:
					return NkIcon::Gizmo;
				default:
					return NkIcon::Mesh;
			}
		}

		inline const char *NkObjectTypeName(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return "Lumiere";
				case nk3d::kVpObjCamera:
					return "Camera";
				case nk3d::kVpObjEmpty:
					return "Repere";
				default:
					return "Maillage";
			}
		}

		// â”€â”€ BARRE DE DEFILEMENT SAISISSABLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// La molette marchait ; la barre n'etait qu'un DESSIN (p.VScroll) --
		// Â« le scrollbar n'est pas fonctionnel Â» (Rihen). Le pouce suit la
		// souris tant que le bouton reste enfonce, meme hors de la glissiere :
		// le geste appartient a la barre ou il a commence (propDragKey).
		inline void NkScrollDrag(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const char *key, const NkRect &area, float32 contentH,
								 float32 &offset) {
			if (contentH > area.h && area.h > 0.f) {
				// Le DESSIN reste colle au bord droit ; seule la ZONE DE SAISIE
				// s'arrete 4 px avant lui -- les SPLITTERS de panneau, declares
				// apres tout le reste, possedent ces derniers pixels et voleraient
				// le clic. Plus large que le dessin (6 px) : une cible de 6 px se
				// rate.
				const NkRect track{area.x + area.w - S(16.f), area.y, S(12.f), area.h};
				hit.Add(key, track);
				const bool mine = (strcmp(st.propDragKey, key) == 0);
				if (hit.MouseDown() && (hit.IsHovered(key) || mine)) {
					if (!st.propDragKey[0] && hit.IsHovered(key))
						snprintf(st.propDragKey, sizeof(st.propDragKey), "%s", key);
					if (strcmp(st.propDragKey, key) == 0) {
						float32 th = area.h * (area.h / contentH);
						if (th < 24.f)
							th = 24.f;
						float32 t = (hit.Mouse().y - area.y - th * 0.5f) / (area.h - th);
						if (t < 0.f)
							t = 0.f;
						if (t > 1.f)
							t = 1.f;
						offset = t * (contentH - area.h);
					}
				} else if (mine && !hit.MouseDown()) {
					st.propDragKey[0] = 0;
				}
			}
			p.VScroll(area, contentH, offset);
		}

		// ── LA SEULE BARRE DE DEFILEMENT DE L'APPLICATION ──────────────────────
		// Celle de NKEditorKit, la meme que l'editeur de code et que le panneau
		// des proprietes (Rihen : la hierarchie et les deux cotes du navigateur
		// doivent lui ressembler). La gouttiere est prise DANS la zone, a droite :
		// une barre posee par-dessus le contenu masquerait la derniere colonne.
		// Renvoie la zone utile restante, celle qu'il faut clipper et peindre.
		inline NkRect NkPaintVScroll(NkModelerPainter &p, nkgui::NkGuiContext *guiCtx,
									 const NkRect &area, float32 contentH, float32 &scroll,
									 uint32 id) {
			const float32 sbW = editorkit::NkScrollbarWidth();
			if (area.w <= sbW * 2.f || area.h <= sbW * 2.f)
				return area;
			const NkRect track{area.x + area.w - sbW, area.y, sbW, area.h};
			if (guiCtx) {
				// LE HARNAIS ET LES FLECHES, PLUS SOMBRES (Rihen). Le skin
				// utilisateur de NKEditorKit existe pour ca : on le pose une fois,
				// et TOUTES les barres de l'application suivent -- y compris
				// celle des proprietes, qui doit rester identique aux autres.
				auto &sk = editorkit::NkScrollbarUserSkin();
				if (!sk.custom) {
					sk.custom = true;
					sk.colors.track = NkColor{0, 0, 0, 46};
					sk.colors.thumb = NkColor{44, 49, 58, 255};
					sk.colors.thumbHover = NkColor{62, 69, 80, 255};
					sk.colors.arrowHover = NkColor{26, 30, 38, 255};
				}
				// LE FOND DE LA GOUTTIERE EST OPAQUE : la barre de NKEditorKit
				// peint une piste translucide, si bien que les lignes de selection
				// peintes avant elle transparaissaient au travers (Rihen).
				p.Fill(track, NkRole::PanelBg);
				editorkit::NkVScrollbar(*guiCtx, guiCtx->dl, track, scroll,
										contentH > area.h ? contentH : area.h + 1.f, area.h, id,
										kRowH);
			} else {
				p.VScroll(area, contentH, scroll);
			}
			return {area.x, area.y, area.w - sbW, area.h};
		}

		// Ligne « Transmettre » d'un parent : quelles composantes de SA
		// transformation atteignent ses enfants (option par transformation,
		// idee de Rihen).
		// `r` est le rectangle de TRAVAIL : les trois boutons se calent sur SA
		// largeur. Ils se calculaient auparavant sur la largeur du panneau entier,
		// et debordaient donc du cadre d'un groupe (constate par Rihen).
		inline void NkXmitRow(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
							  const NkRect &rr, float32 &yy, int32 node) {
			(void)rr;
			const float32 labW = S(90.f);
			p.TextV(r.x, yy, kRowH, "Transmettre", NkRole::TextMuted);
			int32 mask = demo::Demo3DHostNodeXmitMask(node);
			static const char *const kXm[3] = {"Pos", "Rot", "Ech"};
			const float32 bw = (r.w - labW - S(8.f)) / 3.f;
			char kx[24];
			for (int32 b = 0; b < 3; ++b) {
				const NkRect br{r.x + labW + (float32)b * (bw + S(4.f)), yy + S(2.f), bw,
								kRowH - S(4.f)};
				snprintf(kx, sizeof(kx), "prop.xmit.%d", b);
				hit.Add(kx, br);
				const bool on2 = ((mask >> b) & 1) != 0;
				if (on2)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					p.Outline(br, NkRole::Border, NkRole::PanelHeader, 3.f);
				const float32 tw = p.TextW(kXm[b]);
				p.TextV(br.x + (br.w - tw) * 0.5f, yy, kRowH, kXm[b],
						on2 ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(kx))
					mask ^= (1 << b);
			}
			demo::Demo3DHostSetNodeXmitMask(node, mask);
			yy += kRowH;
		}
		inline void NkHierNodeName(NkModelerState &st, int32 node, char *out, uint32 cap);
		// Nature d'un noeud utilisateur -> libelle et icone de la hierarchie.
		inline const char *NkUserKindLabel(int32 k) {
			static const char *const kL[11] = {"Empty",  "Maillage", "Maillage",
											   "Maillage", "Empty",	"Lumiere",
											   "Texte",	"Courbe",	"Surface",
											   "Metaball", "Maillage"};
			return (k >= 0 && k <= 10) ? kL[k] : "Empty";
		}
		inline NkIcon NkUserKindIcon(int32 k) {
			switch (k) {
				case 1:
				case 2:
				case 3:
					return NkIcon::Mesh;
				case 5:
					return NkIcon::Light;
				case 6:
					return NkIcon::Text3D;
				case 7:
					return NkIcon::CurveBezier;
				case 8:
					return NkIcon::SurfacePatch;
				case 9:
					return NkIcon::Metaball;
				case 10:
					return NkIcon::CircleEdge;
				default:
					return NkIcon::Cursor;
			}
		}
		// Nom d'un double/colle : « base.NNN » (base = nom AFFICHE de la
		// source, suffixe .NNN existant coupe pour ne pas empiler).
		inline void NkHierComposeName(NkModelerState &st, const char *base0, int32 newNode) {
			if (newNode < 96 || newNode >= 160 || !base0 || !base0[0])
				return;
			char base[24];
			snprintf(base, sizeof(base), "%s", base0);
			const int32 bl = (int32)strlen(base);
			if (bl > 4 && base[bl - 4] == '.' && base[bl - 3] >= '0' && base[bl - 3] <= '9' &&
				base[bl - 2] >= '0' && base[bl - 2] <= '9' && base[bl - 1] >= '0' &&
				base[bl - 1] <= '9')
				base[bl - 4] = 0;
			snprintf(st.customNames[newNode], 24, "%.19s.%03d", base, (newNode - 96) + 1);
		}
		// Nom UNIQUE par (dossier, nature) : Base, Base_02, Base_03... (Rihen).
		inline void NkBrowUniqueName(NkModelerState &st, uint8 kind, int32 parent,
									 const char *base, char *out, uint32 cap) {
			for (int32 n7 = 1; n7 < 1000; ++n7) {
				if (n7 == 1)
					snprintf(out, cap, "%s", base);
				else
					snprintf(out, cap, "%s_%02d", base, n7);
				bool taken = false;
				for (int32 j7 = 0; j7 < st.browserCount; ++j7) {
					if (st.browserNames[j7] == out)
						continue; // soi-meme (nom en cours d'ecriture)
					if (st.browserKind[j7] == kind && st.browserParent[j7] == parent &&
						strcmp(st.browserNames[j7], out) == 0) {
						taken = true;
						break;
					}
				}
				if (!taken)
					return;
			}
		}
		// Homonyme de MEME NATURE dans un dossier (hors src et supprimes).
		inline int32 NkBrowFindSame(NkModelerState &st, int32 dest, uint8 kind,
									const char *name, int32 excl) {
			for (int32 j8 = 0; j8 < st.browserCount; ++j8)
				if (j8 != excl && st.browserKind[j8] == kind &&
					st.browserParent[j8] == dest &&
					strcmp(st.browserNames[j8], name) == 0)
					return j8;
			return -1;
		}
		/// Chemin RELATIF du dossier d'une carte-dossier, termine par « / » --
		/// c'est cette barre finale qui dit « dossier » a la file de suppression.
		inline NkString NkBrowFolderRel(const NkModelerState &st, int32 card) {
			NkString parts[8];
			int32 n = 0, cur = card;
			for (int32 g = 0; g < 8 && cur >= 0 && cur < st.browserCount; ++g) {
				if (st.browserKind[cur] != 1)
					break;
				parts[n++] = st.browserNames[cur];
				cur = st.browserParent[cur];
			}
			NkString out;
			for (int32 i = n - 1; i >= 0; --i) {
				out += parts[i];
				out += '/';
			}
			return out;
		}
		// ── CLASSEMENT DU NAVIGATEUR ────────────────────────────────────────────
		// Comparaison de noms INSENSIBLE A LA CASSE, comme l'explorateur : « arbre »
		// et « Arbre » doivent se suivre, pas se retrouver aux deux bouts de la
		// liste selon la majuscule.
		inline int32 NkBrowNameCmp(const char *a, const char *b) {
			for (int32 i = 0;; ++i) {
				char x = a[i], y = b[i];
				if (x >= 'A' && x <= 'Z')
					x = (char)(x - 'A' + 'a');
				if (y >= 'A' && y <= 'Z')
					y = (char)(y - 'A' + 'a');
				if (x != y)
					return (x < y) ? -1 : 1;
				if (!x)
					return 0;
			}
		}

		/// Vrai si `a` doit passer AVANT `b`.
		inline bool NkBrowBefore(const NkModelerState &st, int32 a, int32 b) {
			// LES DOSSIERS D'ABORD, toujours -- meme en ordre decroissant. Un
			// dossier n'est pas un element de la liste, c'est le chemin vers la
			// suite ; le renvoyer en bas oblige a le chercher.
			const bool fa = st.browserKind[a] == 1, fb = st.browserKind[b] == 1;
			if (fa != fb)
				return fa;
			int32 c = 0;
			if (st.browSort == 1) { // TYPE, puis nom a type egal
				c = (int32)st.browserKind[a] - (int32)st.browserKind[b];
				if (c == 0)
					c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			} else if (st.browSort == 2) { // DATE, puis nom a date egale
				const nk_int64 ta = st.browserTime[a], tb = st.browserTime[b];
				c = (ta < tb) ? -1 : ((ta > tb) ? 1 : 0);
				if (c == 0)
					c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			} else {
				c = NkBrowNameCmp(st.browserNames[a], st.browserNames[b]);
			}
			// Le SENS ne s'applique qu'au critere, jamais a la regle des dossiers.
			return st.browSortDesc ? (c > 0) : (c < 0);
		}

		/// Cartes VISIBLES du dossier courant, dans l'ordre de classement. Tri par
		/// insertion : le navigateur tient trente-deux cartes, un tri savant y
		/// couterait plus en lecture qu'il ne rapporterait en cycles.
		inline int32 NkBrowVisible(const NkModelerState &st, int32 *out, int32 cap) {
			int32 n = 0;
			for (int32 i = 0; i < st.browserCount && n < cap; ++i) {
				if (st.browserKind[i] == 255 || st.browserParent[i] != st.browserFolder)
					continue;
				// Filtre par TYPE. Les DOSSIERS restent toujours visibles : ils sont
				// le chemin vers le reste, pas un resultat de recherche.
				if (st.browFilter != 0u && st.browserKind[i] != 1 &&
					(st.browFilter & (1u << st.browserKind[i])) == 0u)
					continue;
				if (!NkNameMatches(st.browserNames[i], st.searchBrowser))
					continue;
				int32 k = n++;
				while (k > 0 && NkBrowBefore(st, i, out[k - 1])) {
					out[k] = out[k - 1];
					--k;
				}
				out[k] = i;
			}
			return n;
		}

		inline void NkMarkTreeDirty(NkModelerState &st);
		inline void NkBrowDelRec(NkModelerState &st, int32 root2) {
			NkMarkTreeDirty(st);
			int32 stk[64];
			int32 sp2 = 0;
			stk[sp2++] = root2;
			while (sp2 > 0) {
				const int32 s2 = stk[--sp2];
				// LE FICHIER SUIT LA CARTE (Rihen). Il part en CORBEILLE, pas au
				// neant : une suppression de trop doit pouvoir se rattraper -- meme
				// exigence que « fermer un onglet ne supprime rien ».
				if (st.browserFile[s2][0]) {
					st.DelPendPush(st.browserFile[s2]);
					st.browserFile[s2][0] = 0;
				} else if (st.browserKind[s2] == 1) {
					// Un DOSSIER n'a pas de fichier : c'est son repertoire qui part.
					st.DelPendPush(NkBrowFolderRel(st, s2).CStr());
				}
				st.browserKind[s2] = 255;
				for (int32 j4 = 0; j4 < st.browserCount; ++j4)
					if (st.browserParent[j4] == s2 && st.browserKind[j4] != 255 && sp2 < 63)
						stk[sp2++] = j4;
			}
			if (st.browserFolder == root2)
				st.browserFolder = -1;
			if (st.selectedAsset == root2)
				st.selectedAsset = -1;
		}
		// Copie RECURSIVE avec noms uniques par niveau (et souvenir de source).
		inline void NkBrowCopyRecU(NkModelerState &st, int32 src, int32 par) {
			int32 stk[64][2];
			int32 sp2 = 0;
			stk[sp2][0] = src;
			stk[sp2][1] = par;
			++sp2;
			while (sp2 > 0) {
				--sp2;
				const int32 s2 = stk[sp2][0];
				const int32 p2 = stk[sp2][1];
				if (st.browserCount >= NkModelerState::kMaxBrowser)
					break;
				const int32 k4 = st.browserCount++;
				st.browserKind[k4] = st.browserKind[s2];
				st.browserParent[k4] = p2;
				st.browserSrcNode[k4] = st.browserSrcNode[s2];
				NkBrowUniqueName(st, st.browserKind[s2], p2, st.browserNames[s2],
								 st.browserNames[k4], 32);
				if (st.browserKind[s2] == 1)
					for (int32 j4 = 0; j4 < k4; ++j4)
						if (st.browserParent[j4] == s2 && st.browserKind[j4] != 255 &&
							sp2 < 63) {
							stk[sp2][0] = j4;
							stk[sp2][1] = k4;
							++sp2;
						}
			}
		}
		// DEPLACER en REMPLACANT : dossier homonyme = FUSION recursive (le
		// contenu migre et l'identite se reverifie a chaque niveau -- Windows).
		inline void NkBrowMoveReplace(NkModelerState &st, int32 src, int32 dest) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup < 0) {
				st.browserParent[src] = dest;
				return;
			}
			if (st.browserKind[src] == 1) {
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (st.browserKind[c8] != 255 && st.browserParent[c8] == src)
						NkBrowMoveReplace(st, c8, dup);
				st.browserKind[src] = 255; // la coquille vide disparait
				if (st.browserFolder == src)
					st.browserFolder = dup;
			} else {
				// FICHIER homonyme : ON DEMANDE (file du dialogue) -- plus de
				// remplacement silencieux pendant une fusion (Rihen).
				if (st.browConfQN < 32) {
					st.browConfQ[st.browConfQN][0] = src;
					st.browConfQ[st.browConfQN][1] = dest;
					st.browConfQCopy &= ~(1u << st.browConfQN);
					st.browConfQN++;
				}
			}
		}
		// Remplacement EXPLICITE d'un seul element (choix du dialogue).
		inline void NkBrowReplaceOne(NkModelerState &st, int32 src, int32 dest,
									 bool isCopy) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup >= 0)
				NkBrowDelRec(st, dup);
			if (isCopy)
				NkBrowCopyRecU(st, src, dest);
			else
				st.browserParent[src] = dest;
		}
		inline int32 NkBrowCopyOne(NkModelerState &st, int32 src, int32 par) {
			if (st.browserCount >= NkModelerState::kMaxBrowser)
				return -1;
			const int32 k4 = st.browserCount++;
			st.browserKind[k4] = st.browserKind[src];
			st.browserParent[k4] = par;
			st.browserSrcNode[k4] = st.browserSrcNode[src];
			snprintf(st.browserNames[k4], 32, "%s", st.browserNames[src]);
			return k4;
		}
		inline void NkBrowCopyReplace(NkModelerState &st, int32 src, int32 dest) {
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup >= 0 && st.browserKind[src] == 1) {
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (st.browserKind[c8] != 255 && st.browserParent[c8] == src)
						NkBrowCopyReplace(st, c8, dup);
				return;
			}
			if (dup >= 0) {
				// fichier homonyme en COPIE : la file du dialogue tranchera
				if (st.browConfQN < 32) {
					st.browConfQ[st.browConfQN][0] = src;
					st.browConfQ[st.browConfQN][1] = dest;
					st.browConfQCopy |= (1u << st.browConfQN);
					st.browConfQN++;
				}
				return;
			}
			const int32 nk8 = NkBrowCopyOne(st, src, dest);
			if (nk8 >= 0 && st.browserKind[src] == 1)
				for (int32 c8 = 0; c8 < st.browserCount; ++c8)
					if (c8 != nk8 && st.browserKind[c8] != 255 &&
						st.browserParent[c8] == src)
						NkBrowCopyReplace(st, c8, nk8);
		}
		// DEMANDE de transfert : sans homonyme on agit ; sinon le DIALOGUE
		// Renommer / Remplacer / Arreter tranche (regle de Rihen).
		inline void NkBrowRequestTransfer(NkModelerState &st, int32 src, int32 dest,
										  bool isCopy, float32 mx, float32 my) {
			if (src < 0 || st.browserKind[src] == 255)
				return;
			if (!isCopy && st.browserParent[src] == dest)
				return; // deja la
			const int32 dup = NkBrowFindSame(st, dest, st.browserKind[src],
											 st.browserNames[src], src);
			if (dup < 0) {
				if (isCopy)
					NkBrowCopyRecU(st, src, dest);
				else
					st.browserParent[src] = dest;
				return;
			}
			st.browConfSrc = src;
			st.browConfDest = dest;
			st.browConfCopy = isCopy;
			st.browConfX = mx;
			st.browConfY = my;
		}
		// ── REGLE MODEL / MESH (Rihen) ──────────────────────────────────────
		// Model -> Model : parente possible (hierarchie de scene classique).
		// Model -> Mesh  : CONTENANCE (le model contient ses maillages).
		// Mesh  -> Mesh  : JAMAIS de parente -- un maillage est une DONNEE
		// geometrique, pas un noeud de hierarchie. Les maillages d'un meme
		// model sont donc forcement FRERES.
		// Dans un editeur de Model, la seule cible de parente legitime est
		// donc la RACINE du model.
		inline int32 NkModelRootOf(const NkModelerState &st) {
			if (st.sceneTabKind[st.activeTab] != 7)
				return -1; // pas dans un editeur de Model
			const int32 d = st.TabDoc(st.activeTab);
			if (d >= 0 && st.docIsoNode[d] > 0)
				return st.docIsoNode[d] - 1;
			return st.editPreviewNode - 1;
		}
		// Cible de parente AUTORISEE pour un noeud depose sur `dest`.
		// -1 = refuser le depot.
		inline int32 NkParentTargetAllowed(const NkModelerState &st, int32 dest) {
			const int32 mr = NkModelRootOf(st);
			if (mr < 0)
				return dest; // scene : Model -> Model, libre
			return mr;	   // model : tout maillage est FRERE sous la racine
		}
		inline void NkHierNameNewNode(NkModelerState &st, int32 srcNode, int32 newNode) {
			char b[24];
			NkHierNodeName(st, srcNode, b, sizeof(b));
			NkHierComposeName(st, b, newNode);
		}
		// Ligne a SAUTER dans la hierarchie : noeud supprime ou slot libre.
		inline bool NkHierNodeSkip(int32 node) {
			if (demo::Demo3DHostNodeDeleted(node))
				return true;
			if (demo::Demo3DHostNodeScene(node) != demo::Demo3DHostActiveScene())
				return true; // appartient a un autre document (scene/editeur)
			// Les MESH INTERNES d'un model sont sa matiere, pas des objets de
			// scene : ils ne figurent que dans la hierarchie du MODEL (Rihen).
			if (!demo::Demo3DHostDocIsModel() && demo::Demo3DHostNodeIsMesh(node))
				return true;
			return node >= 96 && demo::Demo3DHostUserKind(node) == 0;
		}
		// D'OU VIENT LE VERROU ? Le sien, ou celui du premier ancetre cadenasse.
		// Sert a EXPLIQUER un refus de selection : un clic sans effet passe sinon
		// pour une panne (leçon d'un vrai depannage avec Rihen).
		inline void NkHierLockedName(NkModelerState &st, int32 node, char *out, uint32 cap) {
			char nm[24];
			if (demo::Demo3DHostObjectLocked(node)) {
				NkHierNodeName(st, node, nm, sizeof(nm));
				snprintf(out, cap, "%s est verrouille -- cliquez son cadenas pour l'ouvrir.",
						 nm);
				return;
			}
			int32 cur = demo::Demo3DHostNodeParent(node);
			for (int32 g = 0; g < 96 && cur >= 0; ++g) {
				if (demo::Demo3DHostObjectLocked(cur)) {
					NkHierNodeName(st, cur, nm, sizeof(nm));
					snprintf(out, cap,
							 "Verrouille par son parent %s -- ouvrez SON cadenas.", nm);
					return;
				}
				cur = demo::Demo3DHostNodeParent(cur);
			}
			out[0] = 0;
		}
		// ── L'ICONE D'UN NOEUD, PAR SA NATURE ──────────────────────────────────
		// Un seul endroit la decide, pour que la hierarchie et le panneau de
		// proprietes montrent TOUJOURS le meme dessin (Rihen). C'est aussi ce qui
		// manquait a la camera : son sous-type se perdait, et elle heritait de
		// l'icone des empties.
		inline NkIcon NkNodeIcon(int32 node) {
			if (node >= 86 && node < 90)
				return NkIcon::Light; // lumieres natives de la demo
			if (demo::Demo3DHostNodeIsModel(node))
				return NkIcon::Cube3D; // le conteneur
			if (demo::Demo3DHostNodeIsMesh(node))
				return NkIcon::Mesh; // sa matiere
			const int32 uk = node >= 96 ? demo::Demo3DHostUserKind(node) : 0;
			if (uk == 5)
				return NkIcon::Light;
			if (uk == 4) {
				// Les EMPTIES se distinguent par leur sous-type : la camera a le
				// sien, l'image de reference aussi.
				const int32 us = demo::Demo3DHostUserSub(node);
				if (us == 10)
					return NkIcon::Camera;
				if (us == 11)
					return NkIcon::ImageRef;
				return NkIcon::EmptyAxes;
			}
			if (uk >= 1 && uk <= 3)
				return NkUserKindIcon(uk);
			return node >= 90 ? NkIcon::EmptyAxes : NkIcon::Mesh;
		}
		// Le noeud CONTIENT-IL des maillages ? Alors c'est un MODEL : sa geometrie
		// vit dans ses maillages, pas en lui.
		inline bool NkNodeHasMeshKids(int32 node) {
			const int32 nc = demo::Demo3DHostNodeCount();
			for (int32 c = 0; c < nc; ++c)
				if (demo::Demo3DHostNodeIsMesh(c) &&
					demo::Demo3DHostNodeParent(c) == node &&
					!demo::Demo3DHostNodeDeleted(c))
					return true;
			return false;
		}
		// Le noeud a-t-il des enfants VIVANTS (ni supprimes ni slots libres) ?
		inline bool NkHierHasLiveKids(int32 node) {
			const int32 nc = demo::Demo3DHostNodeCount();
			for (int32 c = 0; c < nc; ++c)
				if (!NkHierNodeSkip(c) && demo::Demo3DHostNodeParent(c) == node)
					return true;
			return false;
		}
		// Le noeud est-il un DESCENDANT de anc dans l'arbre de parente ?
		inline bool NkHierIsDescendant(int32 node, int32 anc) {
			int32 cur = node;
			for (int32 g = 0; g < 96 && cur >= 0; ++g) {
				cur = demo::Demo3DHostNodeParent(cur);
				if (cur == anc)
					return true;
			}
			return false;
		}
		// Case « Propager aux enfants » d'une propriete commune parent/enfant.
		inline bool NkPropagateCheck(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
									 float32 y, const char *key, bool &on) {
			const NkRect cb{r.x + kPad, y + S(5.f), S(12.f), S(12.f)};
			hit.Add(key, cb);
			p.Outline(cb, on ? NkRole::AccentUi : NkRole::Border,
					  on ? NkRole::AccentUi : NkRole::InputBg, 2.f);
			p.TextV(cb.x + S(18.f), y, kRowH, "Propager aux enfants", NkRole::TextMuted);
			if (hit.Clicked(key)) {
				on = !on;
				return true;
			}
			return false;
		}
		// Nom AFFICHE d'un noeud de scene : nom personnalise > nom de la demo >
		// nom d'empty par defaut (les anciens groupes, noeuds 90..95).
		static const char *const kNkEmptyNames[6] = {"Spheres", "Cube central", "Colonnes",
													 "Instances", "Decor", "Lumieres"};
		inline void NkHierNodeName(NkModelerState &st, int32 node, char *out, uint32 cap) {
			if (node >= 0 && node < 160 && st.customNames[node][0]) {
				snprintf(out, cap, "%s", st.customNames[node]);
				return;
			}
			if (node >= 96) {
				// OBJET UTILISATEUR : nom par nature + numero de slot.
				static const char *const kUK[11] = {"Objet", "Sphere", "Cube",
													"Plan",  "Empty",  "Lumiere",
													"Texte", "Courbe", "Surface",
													"Metaball", "Cercle"};
				int32 k2 = demo::Demo3DHostUserKind(node);
				if (k2 < 0 || k2 > 10)
					k2 = 0;
				const char *bn = kUK[k2];
				if (k2 == 3) {
					// les PLANS a sous-type portent leur vrai nom
					const int32 sb = demo::Demo3DHostUserSub(node);
					if (sb == 2)
						bn = "Plan maille";
					else if (sb == 3)
						bn = "Plan infini";
				}
				if (k2 == 4) {
					// les VIDES a sous-type portent leur vrai nom
					const int32 sb = demo::Demo3DHostUserSub(node);
					if (sb == 10)
						bn = "Camera";
					else if (sb == 11)
						bn = "Reference";
					else if (sb == 12)
						bn = "Arriere-plan";
					else if (sb == 13)
						bn = "Image";
				}
				if (k2 == 5) {
					// nom par TYPE de lumiere (Rihen)
					static const char *const kLT[4] = {"Soleil", "Point light", "Spot",
													   "Area"};
					bn = kLT[demo::Demo3DHostUserSub(node) & 3];
				}
				snprintf(out, cap, "%s.%03d", bn, node - 96);
				return;
			}
			if (node >= 90)
				snprintf(out, cap, "%s", kNkEmptyNames[node - 90]);
			else if (node >= 86)
				demo::Demo3DHostLightName(node - 86, out, cap);
			else
				demo::Demo3DHostObjectName(node, out, cap);
		}
		// ── MENUS ET RACCOURCIS DE SCENE (peints PAR-DESSUS tout) ───────────
		// Raccourcis globaux (X/Suppr, Maj+D, Ctrl+C/V), menu contextuel --
		// lignes de la hierarchie ET clic droit dans la vue 3D (Rihen) --,
		// dialogue de confirmation de suppression. Appele en DERNIER par main.
		inline void PaintSceneMenus(NkModelerPainter &p, const NkRect &area, const NkRect &view,
									NkModelerState &st, NkHitRegistry &hit, NkWidgetState &ws,
									const nkgui::NkGuiInput &in) {
			const int32 kNumObj2 = demo::Demo3DHostObjectCount();
			const int32 kFirstLight = kNumObj2;
			const int32 selLight = demo::Demo3DHostSelectedLight();
			const int32 activeObj = demo::Demo3DHostActiveObject();
			char key[40];
			// Operations du NAVIGATEUR, partagees par le menu et les raccourcis.
			auto BrCopyRec = [&](int32 src, int32 par) { NkBrowCopyRecU(st, src, par); };
			auto BrDelRec = [&](int32 root2) { NkBrowDelRec(st, root2); };
			auto BrPaste = [&](int32 dest5) {
				if (st.browClip < 0 || st.browserKind[st.browClip] == 255)
					return;
				if (st.browClipCut)
					for (int32 c4 = dest5; c4 >= 0; c4 = st.browserParent[c4])
						if (c4 == st.browClip)
							return; // pas dans sa propre descendance
				NkBrowRequestTransfer(st, st.browClip, dest5, !st.browClipCut,
									  in.mousePos.x, in.mousePos.y);
				if (st.browClipCut && st.browConfSrc < 0)
					st.browClip = -1; // deplace sans conflit ; sinon le dialogue
			};
			// CLIC DROIT DANS LA VUE 3D : le menu du noeud ACTIF — et, SANS noeud
			// actif, le menu AJOUTER au pointeur (Rihen, 10 aout : le vide de la
			// scene doit proposer copier/coller/dupliquer/supprimer/ajouter —
			// les quatre premiers vivent dans le menu du noeud, l'ajout ici).
			// L'objet nait au curseur 3D, comme depuis la barre.
			if (in.mouseClicked[1] && st.hierMenuNode < 0 &&
				NkHitRegistry::Contains(view, in.mousePos)) {
				const int32 actV = st.activeEmpty >= 0
									   ? st.activeEmpty
									   : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (actV >= 0) {
					st.hierMenuNode = actV;
					st.hierMenuX = in.mousePos.x;
					st.hierMenuY = in.mousePos.y;
				} else {
					st.voidMenuOpen = 1;
					st.voidMenuX = in.mousePos.x;
					st.voidMenuY = in.mousePos.y;
				}
			}
			// SUPPRIMER (X ou Suppr : le sous-arbre part avec, regle de Rihen),
			// DUPLIQUER (Maj+D), COPIER / COLLER (Ctrl+C / Ctrl+V). Valables
			// aussi la souris sur la vue 3D -- jamais pendant une saisie.
			if (!ws.editing) {
				bool delK = in.keyInit[(int32)nkgui::NkGuiKey::Delete] ||
							(in.keyInit[(int32)nkgui::NkGuiKey::X] && !in.ctrlDown &&
							 !hit.MouseDown());
				// UN SEUL POINT DE PASSAGE pour Maj+D : la voie POLLEE de l'hote
				// (sk & 1, ci-dessous). L'evenement clavier declenchait EN PLUS,
				// une frame avant le bit polle -- deux duplications par pression
				// (constate par Rihen ; visible depuis que le callback clavier
				// renseigne Shift pour le presse-papiers).
				bool dupK = false;
				for (int32 ci = 0; ci < in.charCount; ++ci) {
					const uint32 cp = in.chars[ci];
					// X pendant un glissement = verrou d'axe du gizmo, pas une
					// suppression : la souris doit etre relachee.
					if ((cp == 'x' || cp == 'X') && !in.ctrlDown && !hit.MouseDown())
						delK = true;
					// (PLUS de repli caractere pour Maj+D : depuis que les
					// modificateurs sont renseignes par le callback clavier, le
					// caractere « D » arrivait UNE FRAME apres l'evenement et
					// dupliquait une seconde fois -- constate par Rihen.)
				}
				// Les raccourcis POLLES par l'hote (seule voie fiable pour les
				// lettres, constatee avec Rihen) s'ajoutent aux evenements.
				const int32 sk = demo::Demo3DHostTakeShortcuts();
				delK = delK || (sk & 8) != 0;
				dupK = dupK || (sk & 1) != 0;
				// AU-DESSUS DU NAVIGATEUR, les raccourcis agissent sur LUI.
				// UN SEUL POINT DE PASSAGE pour Ctrl+C / Ctrl+V aussi : la voie
				// POLLEE (sk & 2 / sk & 4), comme Maj+D ci-dessus. L'evenement
				// (wantPaste, keyInit) declenchait EN PLUS, une frame avant le
				// bit polle -- DEUX collages par pression, depuis la vue comme
				// depuis la hierarchie (constate par Rihen). Copier deux fois
				// etait invisible (meme presse-papiers) ; coller deux fois
				// creait deux noeuds.
				if (NkHitRegistry::Contains(st.browserRect, in.mousePos)) {
					if (delK && st.selectedAsset >= 0)
						BrDelRec(st.selectedAsset);
					else if (dupK && st.selectedAsset >= 0)
						BrCopyRec(st.selectedAsset, st.browserParent[st.selectedAsset]);
					if ((sk & 2) != 0 && st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = false;
					}
					if ((sk & 64) != 0 && st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = true;
					}
					if ((sk & 4) != 0)
						BrPaste(st.browserFolder);
				} else {
				const int32 actN = st.activeEmpty >= 0
									   ? st.activeEmpty
									   : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (delK && !st.delAskOpen) {
					// CONFIRMATION D'ABORD (Rihen) : on memorise les cibles, le
					// dialogue tranche -- y compris le sort des enfants.
					st.delNodeCount = 0;
					for (int32 n2 = 0; n2 < kNumObj2; ++n2)
						if (demo::Demo3DHostObjectSelected(n2) && st.delNodeCount < 64)
							st.delNodes[st.delNodeCount++] = n2;
					if (selLight >= 0 && st.delNodeCount < 64)
						st.delNodes[st.delNodeCount++] = kFirstLight + selLight;
					if (st.activeEmpty >= 0 && st.delNodeCount < 64)
						st.delNodes[st.delNodeCount++] = st.activeEmpty;
					if (st.delNodeCount > 0) {
						st.delHasKids = false;
						for (int32 di = 0; di < st.delNodeCount; ++di)
							if (NkHierHasLiveKids(st.delNodes[di]))
								st.delHasKids = true;
						st.delAskOpen = true;
					}
				} else if (dupK && actN >= 0) {
					const int32 nn = demo::Demo3DHostDuplicateNode(actN);
					if (nn >= 0) {
						NkHierNameNewNode(st, actN, nn); // « Sol.001 » (Rihen)
						demo::Demo3DHostSelectEmptyNode(nn);
					}
				}
				// Meme point de passage unique que ci-dessus : la voie POLLEE
				// seule, sinon evenement + bit pollent DEUX collages.
				if ((sk & 2) != 0 && actN >= 0) {
					demo::Demo3DHostCopyNode(actN);
					NkHierNodeName(st, actN, st.clipName, sizeof(st.clipName));
				}
				if ((sk & 4) != 0) {
					const int32 nn = demo::Demo3DHostPasteNode();
					if (nn >= 0) {
						NkHierComposeName(st, st.clipName, nn);
						demo::Demo3DHostSelectEmptyNode(nn);
					}
				}
				// Ctrl+P / Maj+P polles : parenter la selection a l'actif,
				// ou tout deparenter -- meme regle que la hierarchie.
				if (sk & (16 | 32)) {
					for (int32 n2 = 0; n2 < 90; ++n2) {
						const bool selN = n2 < kNumObj2
											  ? demo::Demo3DHostObjectSelected(n2)
											  : (selLight == n2 - kFirstLight);
						if (!selN || n2 == actN)
							continue;
						demo::Demo3DHostSetNodeParent(n2,
													  (sk & 16) && actN >= 0 ? actN : -1);
					}
					if ((sk & 32) && actN >= 0 && actN < 90)
						demo::Demo3DHostSetNodeParent(actN, -1);
				}
				}
			}
			// MENU CONTEXTUEL : Dupliquer / Copier / Coller / Supprimer, avec
			// ou sans les enfants (les deux variantes demandees par Rihen).
			if (st.hierMenuNode >= 0) {
				const int32 tnM = st.hierMenuNode;
				const int32 ukM = demo::Demo3DHostUserKind(tnM);
				// Isoler : uniquement ce qui se MODELISE (ni lumiere ni empty).
				const bool isoOk = tnM < 86 || (ukM >= 1 && ukM <= 3) || ukM >= 6;
				const bool promOk = demo::Demo3DHostNodeParent(tnM) >= 0;
				const char *hmIt[12];
				int32 hmAct[12];
				int32 nH = 0;
				hmIt[nH] = "Ajouter un enfant...";
				hmAct[nH++] = 5;
				if (isoOk) {
					hmIt[nH] = "Isoler (editer comme Model)";
					hmAct[nH++] = 6;
				}
				if (promOk) {
					hmIt[nH] = "Promouvoir en parent";
					hmAct[nH++] = 7;
				}
				hmIt[nH] = "Copier proprietes";
				hmAct[nH++] = 8;
				if (st.propClipNode > 0 && st.propClipNode != tnM + 1) {
					hmIt[nH] = "Coller proprietes";
					hmAct[nH++] = 9;
				}
				hmIt[nH] = "Dupliquer  (Maj+D)";
				hmAct[nH++] = 0;
				// LES DEUX VARIANTES (Rihen, 10 aout) : dupliquer/coller en
				// FRERE (comportement historique) ou en ENFANT du noeud clique.
				hmIt[nH] = "Dupliquer comme enfant";
				hmAct[nH++] = 11;
				hmIt[nH] = "Copier  (Ctrl+C)";
				hmAct[nH++] = 1;
				hmIt[nH] = "Coller  (Ctrl+V)";
				hmAct[nH++] = 2;
				hmIt[nH] = "Coller comme enfant";
				hmAct[nH++] = 10;
				hmIt[nH] = "Supprimer...  (X)";
				hmAct[nH++] = 3;
				// LARGEUR = l'entree la plus longue ; vers le HAUT si le bas
				// manquerait (Rihen : tout doit toujours se voir).
				float32 wH = 0.f;
				for (int32 mi = 0; mi < nH; ++mi)
					if (p.TextW(hmIt[mi]) > wH)
						wH = p.TextW(hmIt[mi]);
				NkRect mr{st.hierMenuX, st.hierMenuY, wH + S(28.f),
						  kRowH * (float32)nH};
				if (mr.y + mr.h > area.y + area.h)
					mr.y = st.hierMenuY - mr.h; // vers le HAUT
				if (mr.y < area.y)
					mr.y = area.y;
				st.UiBlockAdd(mr); // les panneaux du dessous ne repondent plus
				p.Outline(mr, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 mact = -1;
				for (int32 mi = 0; mi < nH; ++mi) {
					const NkRect it{mr.x, mr.y + (float32)mi * kRowH, mr.w, kRowH};
					snprintf(key, sizeof(key), "hier.menu.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, hmIt[mi]);
					if (hit.Clicked(key))
						mact = hmAct[mi];
				}
				if (mact >= 0) {
					const int32 tn = st.hierMenuNode;
					if (mact == 0) {
						const int32 nn = demo::Demo3DHostDuplicateNode(tn);
						if (nn >= 0) {
							NkHierNameNewNode(st, tn, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
						}
					} else if (mact == 1) {
						demo::Demo3DHostCopyNode(tn);
						NkHierNodeName(st, tn, st.clipName, sizeof(st.clipName));
					} else if (mact == 2) {
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
						}
					} else if (mact == 3) {
						// CONFIRMATION d'abord -- le dialogue tranche pour les
						// enfants (Rihen).
						st.delNodes[0] = tn;
						st.delNodeCount = 1;
						st.delHasKids = NkHierHasLiveKids(tn);
						st.delAskOpen = true;
					} else if (mact == 5) {
						// AJOUTER UN ENFANT : tout le menu Ajouter ; l'objet
						// clique est le PARENT du nouveau (Rihen).
						// Dans un Model, l'enfant nait FRERE sous la racine.
						st.addParentNode = NkParentTargetAllowed(st, tn);
						st.addAnchor = {st.hierMenuX, st.hierMenuY - S(26.f), 0.f,
										0.f};
						if (!ws.ComboOpen("tb.addmenu"))
							ws.ToggleCombo("tb.addmenu");
					} else if (mact == 6) {
						// ISOLER : un onglet Model edite CE noeud, seul, sans
						// etre gene par le reste de la scene (Rihen).
						const int32 dH = st.TabDoc(st.activeTab);
						if (st.sceneCount < 8 && dH >= 0) {
							// L'ISOLATION est un document TRANSITOIRE : il n'existe
							// que le temps de la vue, et le noeud rentre chez lui
							// quand elle se ferme. Il n'a donc ni carte ni ligne
							// dans le fichier -- ce serait une scene fantome.
							const int32 d7 = st.DocAlloc();
							if (d7 >= 0) {
								st.docTransient[d7] = true;
								NkHierNodeName(st, tn, st.docName[d7], 32);
								st.docIsoNode[d7] = tn + 1;
								st.docIsoHome[d7] = st.docScene[dH];
								st.docScene[d7] = (uint8)st.sceneIdNext++;
								st.docBlank[d7] = true;
								const int32 tb7 = st.sceneCount++;
								st.sceneTabKind[tb7] = 7;
								st.sceneTabAsset[tb7] = 0;
								st.sceneTabDoc[tb7] = d7;
								NkActivateTab(st, tb7);
							}
						}
					} else if (mact == 7) {
						// PROMOUVOIR : il prend la place de son parent ; le
						// parent devient son fils, ses freres ses enfants.
						const int32 pp7 = demo::Demo3DHostNodeParent(tn);
						if (pp7 >= 0) {
							const int32 gp7 = demo::Demo3DHostNodeParent(pp7);
							const int32 nc7 = demo::Demo3DHostNodeCount();
							for (int32 c7 = 0; c7 < nc7; ++c7)
								if (c7 != tn && !NkHierNodeSkip(c7) &&
									demo::Demo3DHostNodeParent(c7) == pp7)
									demo::Demo3DHostSetNodeParent(c7, tn);
							demo::Demo3DHostSetNodeParent(tn, gp7);
							demo::Demo3DHostSetNodeParent(pp7, tn);
						}
					} else if (mact == 8) {
						st.propClipNode = tn + 1; // source des proprietes
					} else if (mact == 9) {
						// COLLER LES PROPRIETES : matiere, dimensions, lumiere --
						// JAMAIS la transform (position/rotation restent siennes).
						const int32 s9 = st.propClipNode - 1;
						float32 t9[3], mt9, rg9;
						if (demo::Demo3DHostMeshMaterial(s9, t9, &mt9, &rg9)) {
							demo::Demo3DHostSetMeshTint(tn, t9);
							demo::Demo3DHostSetMeshMetalRough(tn, mt9, rg9);
						}
						float32 bs9[3];
						demo::Demo3DHostNodeBaseSize(s9, bs9);
						demo::Demo3DHostSetNodeBaseSize(tn, bs9);
						const bool sL9 = demo::Demo3DHostUserKind(s9) == 5 ||
										 (s9 >= 86 && s9 < 90);
						const bool dL9 = demo::Demo3DHostUserKind(tn) == 5 ||
										 (tn >= 86 && tn < 90);
						if (sL9 && dL9) {
							float32 rgE, inE, outE, aw9, ah9;
							bool sh9;
							int32 ty9;
							if (demo::Demo3DHostLightEx(s9, &rgE, &inE, &outE, &aw9,
														&ah9, &sh9, &ty9))
								demo::Demo3DHostSetLightEx(tn, rgE, inE, outE, aw9,
														   ah9, sh9);
							float32 c9[3], it9;
							if (demo::Demo3DHostUserLightParams(s9, c9, &it9))
								demo::Demo3DHostSetUserLightParams(tn, c9, it9);
						}
					} else if (mact == 10) {
						// COLLER COMME ENFANT : le colle nait sous le noeud clique
						// (dans un Model, la cible autorisee est la racine).
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							const int32 tg = NkParentTargetAllowed(st, tn);
							if (tg >= 0 && tg != nn)
								demo::Demo3DHostSetNodeParent(nn, tg);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (mact == 11) {
						// DUPLIQUER COMME ENFANT : la copie devient fils de
						// l'original au lieu de naitre a cote.
						const int32 nn = demo::Demo3DHostDuplicateNode(tn);
						if (nn >= 0) {
							NkHierNameNewNode(st, tn, nn);
							const int32 tg = NkParentTargetAllowed(st, tn);
							if (tg >= 0 && tg != nn)
								demo::Demo3DHostSetNodeParent(nn, tg);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					}
					st.hierMenuNode = -1;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mr, hit.Mouse())) {
					st.hierMenuNode = -1;
				}
			}
			// ── MENU DU VIDE (Rihen, 10 aout) : Ajouter / Copier / Coller /
			// Dupliquer / Supprimer — et d'autres viendront. Ouvert par clic
			// droit hors de tout noeud, dans la vue 3D comme dans la
			// hierarchie. Les actions a cible visent le noeud ACTIF s'il en
			// reste un ; sans cible elles s'affichent en sourdine, cliquables
			// pour rien n'est pas un etat (regle du depot).
			if (st.voidMenuOpen) {
				const int32 actV2 = st.activeEmpty >= 0
										? st.activeEmpty
										: (selLight >= 0 ? kFirstLight + selLight : activeObj);
				const bool canPaste2 = st.clipName[0] != 0;
				const char *vmIt[8];
				int32 vmAct[8];
				bool vmOn[8];
				int32 nV = 0;
				// « Ajouter » est un SOUS-MENU (question de Rihen, 10 aout — oui,
				// c'est mieux) : le survol ouvre la cascade categories -> types
				// (PaintAddObjectMenu), accrochee au bord droit de la ligne,
				// comme « Creer > » du navigateur. Le clic n'a rien a faire.
				vmIt[nV] = "Ajouter              >";
				vmAct[nV] = 0;
				vmOn[nV++] = true;
				vmIt[nV] = "Copier  (Ctrl+C)";
				vmAct[nV] = 1;
				vmOn[nV++] = actV2 >= 0;
				vmIt[nV] = "Coller  (Ctrl+V)";
				vmAct[nV] = 2;
				vmOn[nV++] = canPaste2;
				vmIt[nV] = "Dupliquer  (Maj+D)";
				vmAct[nV] = 3;
				vmOn[nV++] = actV2 >= 0;
				vmIt[nV] = "Supprimer...  (X)";
				vmAct[nV] = 4;
				vmOn[nV++] = actV2 >= 0;
				float32 wV = 0.f;
				for (int32 mi = 0; mi < nV; ++mi)
					if (p.TextW(vmIt[mi]) > wV)
						wV = p.TextW(vmIt[mi]);
				NkRect mrV{st.voidMenuX, st.voidMenuY, wV + S(28.f), kRowH * (float32)nV};
				if (mrV.y + mrV.h > area.y + area.h)
					mrV.y = st.voidMenuY - mrV.h;
				if (mrV.y < area.y)
					mrV.y = area.y;
				st.UiBlockAdd(mrV);
				p.Outline(mrV, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 vact = -1;
				for (int32 mi = 0; mi < nV; ++mi) {
					const NkRect it{mrV.x, mrV.y + (float32)mi * kRowH, mrV.w, kRowH};
					snprintf(key, sizeof(key), "void.menu.%d", mi);
					const bool overV = vmOn[mi] && hit.Add(key, it);
					if (vmOn[mi])
						HoverFill(p, it, overV, 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, vmIt[mi],
							vmOn[mi] ? NkRole::Text : NkRole::TextMuted);
					// SOUS-MENU AJOUTER : il SUIT le survol — il s'ouvre sur sa
					// ligne, se ferme des qu'une autre entree est survolee.
					if (overV && vmAct[mi] == 0 && !ws.ComboOpen("tb.addmenu")) {
						st.addParentNode = -1;
						// La cascade se place a (a.x, a.y + a.h + 2) : h=0 et
						// y = ligne - 2 la posent exactement au niveau de la ligne.
						st.addAnchor = {mrV.x + mrV.w - S(2.f), it.y - 2.f, 0.f, 0.f};
						ws.ToggleCombo("tb.addmenu");
					} else if (overV && vmAct[mi] != 0 && ws.ComboOpen("tb.addmenu")) {
						ws.CloseCombo();
					}
					if (vmOn[mi] && hit.Clicked(key))
						vact = vmAct[mi];
				}
				if (vact >= 0 && vact != 0) {
					if (vact == 1) {
						demo::Demo3DHostCopyNode(actV2);
						NkHierNodeName(st, actV2, st.clipName, sizeof(st.clipName));
					} else if (vact == 2) {
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (vact == 3) {
						const int32 nn = demo::Demo3DHostDuplicateNode(actV2);
						if (nn >= 0) {
							NkHierNameNewNode(st, actV2, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (vact == 4) {
						st.delNodes[0] = actV2;
						st.delNodeCount = 1;
						st.delHasKids = NkHierHasLiveKids(actV2);
						st.delAskOpen = true;
					}
					st.voidMenuOpen = 0;
				} else if (hit.AnyClick() && hit.IsHovered("addm.sub")) {
					// Une CREATION dans la cascade Ajouter ferme tout le menu.
					st.voidMenuOpen = 0;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mrV, hit.Mouse()) &&
						   !hit.IsHovered("addm.panel") && !hit.IsHovered("addm.sub")) {
					// Clic ailleurs : fermer — sauf dans la cascade Ajouter, qui
					// fait partie du menu.
					st.voidMenuOpen = 0;
				}
			}
			// DIALOGUE DE CONFIRMATION : aucune suppression directe (Rihen).
			// Si un parent est vise, l'utilisateur choisit le sort des enfants.
			if (st.delAskOpen) {
				hit.Add("hier.delveil", area); // voile : la liste ne repond plus
				const float32 dw = area.w - S(24.f) < S(250.f) ? area.w - S(24.f) : S(250.f);
				const float32 dh = kRowH * (st.delHasKids ? 5.f : 3.f) + S(8.f);
				const NkRect dr{area.x + (area.w - dw) * 0.5f, area.y + area.h * 0.32f, dw, dh};
				p.Outline(dr, NkRole::AccentUi, NkRole::PanelHeader, 4.f);
				char title[96];
				if (st.delNodeCount == 1) {
					char nm[48];
					NkHierNodeName(st, st.delNodes[0], nm, sizeof(nm));
					snprintf(title, sizeof(title), "Supprimer \"%s\" ?", nm);
				} else {
					snprintf(title, sizeof(title), "Supprimer %d elements ?",
							 st.delNodeCount);
				}
				p.TextV(dr.x + S(10.f), dr.y + S(4.f), kRowH, title);
				float32 dy = dr.y + S(4.f) + kRowH;
				if (st.delHasKids) {
					p.TextV(dr.x + S(10.f), dy, kRowH, "Des enfants en dependent :",
							NkRole::TextMuted);
					dy += kRowH;
				}
				int32 choice = -1; // 0 avec enfants, 1 garder, 2 annuler
				{
					const NkRect b0{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.ok", b0);
					p.Fill(b0, NkRole::AccentUi, 3.f);
					p.TextV(b0.x + S(8.f), dy, kRowH,
							st.delHasKids ? "Supprimer avec les enfants" : "Supprimer",
							NkRole::TextOnAccent);
					if (hit.Clicked("hier.del.ok"))
						choice = 0;
					dy += kRowH;
				}
				if (st.delHasKids) {
					const NkRect b1{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.keep", b1);
					p.Outline(b1, NkRole::Border, NkRole::InputBg, 3.f);
					p.TextV(b1.x + S(8.f), dy, kRowH, "Garder les enfants");
					if (hit.Clicked("hier.del.keep"))
						choice = 1;
					dy += kRowH;
				}
				{
					const NkRect b2{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.cancel", b2);
					p.Outline(b2, NkRole::Border, NkRole::InputBg, 3.f);
					p.TextV(b2.x + S(8.f), dy, kRowH, "Annuler  (Echap)");
					if (hit.Clicked("hier.del.cancel") ||
						in.keyInit[(int32)nkgui::NkGuiKey::Escape])
						choice = 2;
				}
				if (choice == 0 || choice == 1) {
					for (int32 di = 0; di < st.delNodeCount; ++di)
						demo::Demo3DHostDeleteNode(st.delNodes[di], choice == 0);
					demo::Demo3DHostDeselectAll();
				}
				if (choice >= 0) {
					st.delAskOpen = false;
					st.delNodeCount = 0;
				}
			}
			// ── MENU CONTEXTUEL DU NAVIGATEUR : PARTOUT (arbre, grille, carte
			// ou vide), et son contenu s'adapte au presse-papiers (Rihen).
			if (st.browMenuIdx != -1) {
				const bool onCard = st.browMenuIdx >= 0;
				// -4 : combo Creer de la barre -> UNIQUEMENT la liste de creation
				// (le clic droit garde Importer + sous-menu Creer).
				const bool creatOnly = (st.browMenuIdx == -4);
				const bool canPaste =
					st.browClip >= 0 && st.browserKind[st.browClip] != 255;
				const char *bmIt[12];
				int32 bmAct[12];
				int32 nIt = 0;
				if (!creatOnly) {
					bmIt[nIt] = "Creer                >";
					bmAct[nIt++] = 100; // ouvre le SOUS-MENU au survol
					bmIt[nIt] = "Importer...";
					bmAct[nIt++] = 20;
				}
				if (onCard) {
					bmIt[nIt] = "Couper";
					bmAct[nIt++] = 0;
					bmIt[nIt] = "Copier";
					bmAct[nIt++] = 1;
				}
				if (canPaste && !creatOnly) {
					bmIt[nIt] = "Coller";
					bmAct[nIt++] = 2;
				}
				if (onCard) {
					bmIt[nIt] = "Dupliquer";
					bmAct[nIt++] = 3;
					bmIt[nIt] = "Supprimer";
					bmAct[nIt++] = 4;
				}
				// LARGEUR = l'entree la plus longue ; s'ouvre vers le HAUT si le
				// bas manquerait : tout doit toujours se voir (Rihen).
				float32 wM2 = 0.f;
				for (int32 mi = 0; mi < nIt; ++mi)
					if (p.TextW(bmIt[mi]) > wM2)
						wM2 = p.TextW(bmIt[mi]);
				NkRect mr2{st.browMenuX, st.browMenuY, creatOnly ? 0.f : wM2 + S(28.f),
						   kRowH * (float32)nIt};
				if (mr2.y + mr2.h > area.y + area.h)
					mr2.y = st.browMenuY - mr2.h; // vers le HAUT
				if (mr2.y < area.y)
					mr2.y = area.y;
				st.UiBlockAdd(mr2);
				if (!creatOnly)
					p.Outline(mr2, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 act2 = -1;
				float32 creatY = mr2.y;
				for (int32 mi = 0; mi < nIt; ++mi) {
					const NkRect it{mr2.x, mr2.y + (float32)mi * kRowH, mr2.w, kRowH};
					snprintf(key, sizeof(key), "brw.menu.%d", mi);
					const bool overIt = hit.Add(key, it);
					HoverFill(p, it, overIt, 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, bmIt[mi]);
					// le survol OUVRE le sous-menu Creer, un autre item le ferme
					if (overIt)
						st.browMenuCreat = (bmAct[mi] == 100);
					if (bmAct[mi] == 100)
						creatY = it.y;
					if (hit.Clicked(key) && bmAct[mi] != 100)
						act2 = bmAct[mi];
				}
				NkRect sub2{0.f, 0.f, 0.f, 0.f};
				NkRect gr3{0.f, 0.f, 0.f, 0.f};
				if (st.browMenuCreat) {
					// SOUS-MENU Creer : tout ce qui peut naitre ici (Rihen), dont
					// la SCENE et le MESH reutilisable.
					// Le GRAPHE remplace le blueprint : c'est un editeur nodal, et
					// il en existe plusieurs natures (Rihen).
					static const char *const kCr[7] = {"Dossier", "Scene", "Model",
													   "Materiau", "Texture",
													   "Graphe             >",
													   "Dataset"};
					float32 wS2 = 0.f;
					for (int32 mi = 0; mi < 7; ++mi)
						if (p.TextW(kCr[mi]) > wS2)
							wS2 = p.TextW(kCr[mi]);
					sub2 = {mr2.x + mr2.w + 2.f, creatY, wS2 + S(28.f), kRowH * 7.f};
					if (sub2.y + sub2.h > area.y + area.h)
						sub2.y = area.y + area.h - sub2.h;
					if (sub2.x + sub2.w > area.x + area.w)
						sub2.x = mr2.x - sub2.w - 2.f;
					st.UiBlockAdd(sub2);
					p.Outline(sub2, NkRole::Border, NkRole::PanelHeader, 3.f);
					float32 grY = sub2.y;
					for (int32 mi = 0; mi < 7; ++mi) {
						const NkRect it{sub2.x, sub2.y + (float32)mi * kRowH, sub2.w, kRowH};
						snprintf(key, sizeof(key), "brw.sub.%d", mi);
						const bool ovG = hit.Add(key, it);
						HoverFill(p, it, ovG, 0.f);
						p.TextV(it.x + S(10.f), it.y, kRowH, kCr[mi]);
						if (mi == 5) {
							grY = it.y;
							if (ovG)
								st.browMenuGraph = true;
						} else if (ovG) {
							st.browMenuGraph = false;
						}
						if (hit.Clicked(key) && mi != 5)
							act2 = 10 + mi;
					}
					if (st.browMenuGraph) {
						// SOUS-MENU GRAPHE : les natures d'editeur nodal (Rihen).
						static const char *const kGr[4] = {
							"Modelisation procedurale", "Texturing procedural",
							"Materiau", "Motion"};
						float32 wG = 0.f;
						for (int32 gi = 0; gi < 4; ++gi)
							if (p.TextW(kGr[gi]) > wG)
								wG = p.TextW(kGr[gi]);
						gr3 = {sub2.x + sub2.w + 2.f, grY, wG + S(28.f), kRowH * 4.f};
						if (gr3.y + gr3.h > area.y + area.h)
							gr3.y = area.y + area.h - gr3.h;
						if (gr3.x + gr3.w > area.x + area.w)
							gr3.x = sub2.x - gr3.w - 2.f;
						st.UiBlockAdd(gr3);
						p.Outline(gr3, NkRole::Border, NkRole::PanelHeader, 3.f);
						for (int32 gi = 0; gi < 4; ++gi) {
							const NkRect it{gr3.x, gr3.y + (float32)gi * kRowH, gr3.w,
											kRowH};
							snprintf(key, sizeof(key), "brw.gr.%d", gi);
							HoverFill(p, it, hit.Add(key, it), 0.f);
							p.TextV(it.x + S(10.f), it.y, kRowH, kGr[gi]);
							if (hit.Clicked(key))
								act2 = 30 + gi;
						}
					}
				}
				if (act2 >= 0) {
					// TOUTE action de ce menu touche l'arbre (creation, copie,
					// deplacement, suppression) : le marquer ICI, en amont, evite
					// d'avoir a y penser branche par branche -- et c'est justement
					// une branche oubliee qui ferait quitter sans rien demander.
					NkMarkTreeDirty(st);
					const int32 tgt = st.browMenuIdx;
					// le dossier VISE (carte-dossier cliquee) sinon le courant
					const int32 destF =
						(onCard && st.browserKind[tgt] == 1) ? tgt : st.browserFolder;
					if (act2 >= 30 && act2 <= 33 &&
						st.browserCount < NkModelerState::kMaxBrowser) {
						// GRAPHE : un asset nodal, avec sa NATURE en sous-type.
						static const char *const kGrN[4] = {"Graphe_Modelisation",
															"Graphe_Texturing",
															"Graphe_Materiau",
															"Graphe_Motion"};
						const int32 kg = st.browserCount++;
						st.browserKind[kg] = 0;
						st.browserSub[kg] = (uint8)(act2 - 30);
						st.browserParent[kg] = destF;
						NkBrowUniqueName(st, 0, destF, kGrN[act2 - 30],
										 st.browserNames[kg], 32);
					} else if (act2 >= 10 && act2 <= 16 &&
							   st.browserCount < NkModelerState::kMaxBrowser) {
						// dossier, scene, mesh, materiau, texture, blueprint, dataset
						static const uint8 kNewK[7] = {1, 5, 6, 2, 3, 0, 4};
						static const char *const kNewN[7] = {"Dossier", "Scene", "Model",
															 "Materiau", "Texture", "BP",
															 "Dataset"};
						const int32 k5 = st.browserCount++;
						const uint8 nk5 = kNewK[act2 - 10];
						st.browserKind[k5] = nk5;
						st.browserParent[k5] = destF;
						st.browserMat[k5] = 0;
						st.browserDoc[k5] = 0;
						st.browserSrcNode[k5] = 0;
						st.browserFile[k5][0] = 0;
						NkBrowUniqueName(st, nk5, destF, kNewN[act2 - 10],
										 st.browserNames[k5], 32);
						// « TOUT CE QUI EST FICHIER EST UN ASSET REEL » (Rihen).
						// Une carte creee ici recoit SA MATIERE tout de suite : sans
						// cela, « + Materiau » ne posait qu'un nom, et les materiaux
						// du projet formaient un monde separe des cartes.
						if (nk5 == 2) {
							const int32 sl = demo::Demo3DHostProjMatCreate();
							if (sl >= 0) {
								st.browserMat[k5] = sl + 1;
								demo::Demo3DHostProjMatSetName(sl, st.browserNames[k5]);
							}
						} else if (nk5 == 5) {
							// Une SCENE creee ici est un vrai document, sinon son
							// double-clic fabriquerait une scene vide sans lien.
							const int32 dN = st.DocAlloc();
							if (dN >= 0) {
								NkWidgetState::Copy(st.docName[dN], st.browserNames[k5], 31u);
								st.docScene[dN] = (uint8)st.sceneIdNext++;
								st.docBlank[dN] = true;
								st.docCard[dN] = k5 + 1;
								st.browserDoc[k5] = dN + 1;
							}
						}
					} else if (act2 == 0) {
						st.browClip = tgt;
						st.browClipCut = true;
					} else if (act2 == 1) {
						st.browClip = tgt;
						st.browClipCut = false;
					} else if (act2 == 2) {
						// dans le dossier CLIQUE, pas la racine (Rihen)
						BrPaste(destF);
					} else if (act2 == 3) {
						BrCopyRec(tgt, st.browserParent[tgt]);
					} else if (act2 == 4) {
						BrDelRec(tgt);
					}
					st.browMenuIdx = -1;
					st.browMenuCreat = false; // sinon le prochain menu l'ouvrirait
					st.browMenuGraph = false;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mr2, hit.Mouse()) &&
						   !(st.browMenuCreat && NkHitRegistry::Contains(sub2, hit.Mouse())) &&
						   !(st.browMenuGraph && NkHitRegistry::Contains(gr3, hit.Mouse())) &&
						   !hit.IsHovered("brw.creer")) { // pas le clic d'OUVERTURE
					st.browMenuIdx = -1;
					st.browMenuCreat = false;
				}
			}
			// CARTE du depot GAUCHE -> DROITE : Copier / Deplacer / Annuler ;
			// cliquer dans le vide annule aussi (Rihen).
			if (st.browAskIdx >= 0) {
				static const char *const kAsk[3] = {"Deplacer ici", "Copier ici",
													"Annuler"};
				NkRect ar3{st.browAskX, st.browAskY, S(160.f), kRowH * 3.f};
				if (ar3.y + ar3.h > area.y + area.h)
					ar3.y = area.y + area.h - ar3.h;
				p.Outline(ar3, NkRole::AccentUi, NkRole::PanelHeader, 3.f);
				int32 ask2 = -1;
				for (int32 mi = 0; mi < 3; ++mi) {
					const NkRect it{ar3.x, ar3.y + (float32)mi * kRowH, ar3.w, kRowH};
					snprintf(key, sizeof(key), "brw.ask.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, kAsk[mi]);
					if (hit.Clicked(key))
						ask2 = mi;
				}
				if (ask2 == 0) {
					NkBrowRequestTransfer(st, st.browAskIdx, st.browAskDest, false,
										  in.mousePos.x, in.mousePos.y);
				} else if (ask2 == 1) {
					NkBrowRequestTransfer(st, st.browAskIdx, st.browAskDest, true,
										  in.mousePos.x, in.mousePos.y);
				}
				if (ask2 >= 0 ||
					(hit.AnyClick() && !NkHitRegistry::Contains(ar3, hit.Mouse())))
					st.browAskIdx = -1; // le vide ANNULE
			}
			// CONFLIT D'HOMONYME (facon Windows) : Renommer / Remplacer /
			// Arreter -- remplacer deux DOSSIERS homonymes les FUSIONNE
			// recursivement (regle de Rihen).
			if (st.browConfSrc >= 0) {
				NkRect cr3{st.browConfX, st.browConfY, S(220.f), kRowH * 4.f + S(6.f)};
				if (cr3.y + cr3.h > area.y + area.h)
					cr3.y = area.y + area.h - cr3.h;
				if (cr3.x + cr3.w > area.x + area.w)
					cr3.x = area.x + area.w - cr3.w;
				p.Outline(cr3, NkRole::AccentUi, NkRole::PanelHeader, 3.f);
				char t7[64];
				snprintf(t7, sizeof(t7), "\"%s\" existe deja ici",
						 st.browserNames[st.browConfSrc]);
				p.TextV(cr3.x + S(8.f), cr3.y + S(3.f), kRowH, t7);
				static const char *const kCf[3] = {"Renommer", "Remplacer", "Arreter"};
				int32 cAct = -1;
				for (int32 mi = 0; mi < 3; ++mi) {
					const NkRect it{cr3.x, cr3.y + S(3.f) + kRowH * (float32)(mi + 1),
									cr3.w, kRowH};
					snprintf(key, sizeof(key), "brw.conf.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, kCf[mi]);
					if (hit.Clicked(key))
						cAct = mi;
				}
				if (cAct >= 0) {
					const int32 cs = st.browConfSrc;
					const int32 cd = st.browConfDest;
					if (cAct == 0) {
						// RENOMMER : suffixe unique, puis transfert.
						if (st.browConfCopy) {
							NkBrowCopyRecU(st, cs, cd); // les noms y sont uniques
						} else {
							char nn7[32];
							NkBrowUniqueName(st, st.browserKind[cs], cd,
											st.browserNames[cs], nn7, 32);
							snprintf(st.browserNames[cs], 32, "%s", nn7);
							st.browserParent[cs] = cd;
						}
					} else if (cAct == 1) {
						if (st.browserKind[cs] == 1) {
							// dossier : FUSION (les fichiers homonymes rejoignent
							// la file et repassent ici un par un)
							if (st.browConfCopy)
								NkBrowCopyReplace(st, cs, cd);
							else
								NkBrowMoveReplace(st, cs, cd);
						} else {
							NkBrowReplaceOne(st, cs, cd, st.browConfCopy);
						}
					}
					if (cAct != 2 && !st.browConfCopy && st.browClip == cs)
						st.browClip = -1; // le couper est consomme
					st.browConfSrc = -1;
					// la FILE continue : le prochain conflit reprend le dialogue
					if (st.browConfQN > 0) {
						st.browConfQN--;
						st.browConfSrc = st.browConfQ[st.browConfQN][0];
						st.browConfDest = st.browConfQ[st.browConfQN][1];
						st.browConfCopy =
							((st.browConfQCopy >> st.browConfQN) & 1u) != 0u;
					}
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(cr3, hit.Mouse())) {
					st.browConfSrc = -1; // le vide ARRETE
				}
			}
		}
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in,
								   nkgui::NkGuiContext *guiCtx = nullptr) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie", &hit, &st.showLeft,
									  "hier.close");
			// Les editeurs SANS design defini (materiau, texture, blueprint,
			// dataset) n'ont PAS de hierarchie : seuls Scene et Model ont
			// l'interface complete (Rihen).
			{
				const uint8 tkH = st.sceneTabKind[st.activeTab];
				if (tkH != 0 && tkH != 7) {
					p.TextV(r.x + S(12.f), y + S(6.f), kRowH,
							"Indisponible pour cet editeur", NkRole::TextMuted);
					return;
				}
			}
			y = PaintSearch(p, r, y, hit, ws, in, "hier.search", st.searchHier);

			// TROIS COLONNES D'ETAT depuis que le RENDU a la sienne (Rihen) :
			// oeil (visible dans la vue), camera (present dans l'image
			// produite), cadenas (selectionnable). Elles sont distinctes : on
			// travaille souvent avec un repere qui n'a rien a faire dans le
			// rendu final.
			const float32 colEye = r.x + r.w - S(70.f);
			const float32 colCam = r.x + r.w - S(48.f);
			const float32 colLock = r.x + r.w - S(26.f);
			const float32 colType = r.x + r.w - S(144.f);

			// L'EN-TETE annonce TOUTES les colonnes : nom, type, oeil, camera,
			// cadenas -- pour que l'utilisateur sache exactement ce que c'est.
			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + S(34.f), y, kRowH, "Nom");
			p.TextV(colType, y, kRowH, "Type", NkRole::TextMuted);
			p.IconV(colEye, y, kRowH, NkIcon::Eye, NkRole::TextMuted, 12.f);
			p.IconV(colCam, y, kRowH, NkIcon::Camera, NkRole::TextMuted, 12.f);
			p.IconV(colLock, y, kRowH, NkIcon::Lock, NkRole::TextMuted, 12.f);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			// LA GOUTTIERE EST RESERVEE AVANT DE PEINDRE : le contenu s'arrete
			// avant elle. Sans cela, les bandeaux de selection couraient jusqu'au
			// bord et passaient SOUS la barre, qui semblait alors decollee et
			// traversee par le dessin (Rihen).
			const NkRect listInner{r.x, listTop, r.w - editorkit::NkScrollbarWidth(), listH};
			hit.Add("hier.list", listR);
			p.Clip(listInner);
			hit.PushClip(listInner); // les lignes defilees hors de vue ne cliquent pas

			char key[40];
			float32 yy = y - st.scrollHier;
			int32 visibleCount = 0;

			// Racine : LA SCENE, renommable -- et elle seule. Dans un editeur de
			// MODEL il n'y a pas de ligne de document : le model EST la racine,
			// et l'afficher en plus donnait deux lignes « Model » de meme nom
			// (constate par Rihen sur sa capture). Une scene, elle, n'est pas un
			// noeud : sa ligne est donc necessaire.
			const int32 dAct = st.TabDoc(st.activeTab);
			if (st.sceneTabKind[st.activeTab] != 7 && dAct >= 0) {
				const NkRect rowR{r.x, yy, r.w, kRowH};
				hit.Add("hier.scene", rowR);
				p.IconV(r.x + S(6.f), yy, kRowH, NkIcon::Globe, NkRole::Text, 13.f);
				if (EditableText(p, hit, ws, in, "hier.scene.name",
								 {r.x + S(24.f), yy, colType - r.x - S(30.f), kRowH},
								 st.docName[dAct], NkRole::Text, st.docName[dAct], 32u)) {
					// Troisieme voie de renommage (avec l'onglet et la carte) : elle
					// doit propager comme les deux autres, sinon le navigateur garde
					// l'ancien nom.
					const int32 e8 = st.docCard[dAct] - 1;
					if (e8 >= 0 && e8 < st.browserCount && st.browserKind[e8] == 5)
						NkWidgetState::Copy(st.browserNames[e8], st.docName[dAct], 31u);
				}
				p.TextV(colType, yy, kRowH, "Scene", NkRole::TextMuted);
				yy += kRowH;
				++visibleCount;
			}

			// ── ARBRE REEL DE PARENTE ───────────────────────────────────────
			// L'arbre suit la TABLE DE PARENTE de l'hote : les anciens groupes
			// sont devenus de vrais EMPTIES (noeuds 90..95) et TOUT noeud peut
			// etre parent ou enfant, quelle que soit sa nature (regle de Rihen).
			// Le CHEVRON plie/deplie -- et RIEN d'autre : le clic de ligne ne
			// fait que selectionner, et un parent se selectionne SEUL (sa
			// transformation emporte ses enfants, pas sa selection). Glisser une
			// ligne sur une autre PARENTE ; vers le vide de la liste, DEPARENTE.
			const int32 kNumObj2 = demo::Demo3DHostObjectCount();
			const int32 kFirstLight = kNumObj2;
			const int32 kFirstEmpty2 = 90;
			const int32 kNNodes = demo::Demo3DHostNodeCount();
			const int32 activeObj = demo::Demo3DHostActiveObject();
			const int32 selLight = demo::Demo3DHostSelectedLight();
			const bool searching = (st.searchHier[0] != 0);
			// Un objet ou une lumiere redevenus actifs (clic vue ou hierarchie)
			// reprennent la main sur l'empty actif.
			// L'EMPTY ACTIF vit dans l'HOTE (gizmo des empties) : une seule
			// source de verite, la vue et la hierarchie restent d'accord.
			st.activeEmpty = demo::Demo3DHostSelectedEmptyNode();
			if (!hit.MouseDown() && st.hierDragNode < 0)
				st.hierDragging = false; // le relachement est digere, une frame apres
			// SOUS UN MENU, ce panneau ne repond plus : les menus sont peints
			// APRES lui, donc son clic etait deja parti (voir UiBlocks).
			const bool uiBlk = st.UiBlocks(hit.Mouse().x, hit.Mouse().y);
			int32 aliveCount = 0, selCount = 0;
			for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2) {
				if (NkHierNodeSkip(n2))
					continue;
				++aliveCount;
				if (n2 < kNumObj2 ? demo::Demo3DHostObjectSelected(n2)
								  : (selLight == n2 - kFirstLight))
					++selCount;
			}
			char nameBuf[48];
			const bool freshPress = hit.MouseDown() && !st.hierMouseWasDown;
			int32 dropHover = -1;
			// ── MAJ+CLIC = PLAGE (Rihen, 10 aout) : tout ce qui s'affiche entre
			// l'ANCRE (dernier clic sans Maj) et la ligne cliquee. La plage ne
			// s'applique qu'APRES le parcours : l'ordre affiche n'est complet
			// qu'a la fin de la boucle.
			int32 visOrder[256];
			uint8 visIsEmpty[256];
			int32 visLight[256];
			int32 visCount = 0;
			int32 rangeTarget = -1;
			// Pile explicite (racines : les empties d'abord -- les familles --
			// puis tout noeud sans parent) ; en RECHERCHE, liste plate.
			int32 stack[200];
			int32 sdepth[200];
			int32 sp = 0;
			if (searching) {
				for (int32 n2 = kNNodes - 1; n2 >= 0; --n2) {
					if (NkHierNodeSkip(n2))
						continue;
					stack[sp] = n2;
					sdepth[sp] = 0;
					++sp;
				}
			} else {
				int32 roots[200];
				int32 nRoots = 0;
				// EST RACINE : sans parent, OU dont le parent n'est pas listable
				// ici (supprime, ou parti dans un autre document par isolation).
				// Sans cette seconde regle l'orphelin DISPARAISSAIT de l'arbre --
				// impossible a selectionner (constate par Rihen).
				auto isRoot2 = [](int32 n3) {
					const int32 pa = demo::Demo3DHostNodeParent(n3);
					return pa < 0 || NkHierNodeSkip(pa);
				};
				for (int32 n2 = kFirstEmpty2; n2 < kNNodes; ++n2)
					if (!NkHierNodeSkip(n2) && isRoot2(n2))
						roots[nRoots++] = n2;
				for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2)
					if (!NkHierNodeSkip(n2) && isRoot2(n2))
						roots[nRoots++] = n2;
				for (int32 i2 = nRoots - 1; i2 >= 0; --i2) {
					stack[sp] = roots[i2];
					sdepth[sp] = 0;
					++sp;
				}
			}
			while (sp > 0) {
				--sp;
				const int32 node = stack[sp];
				const int32 depth = sdepth[sp];
				const bool isLight = node >= kFirstLight && node < kFirstEmpty2;
				const bool isEmpty = node >= kFirstEmpty2;
				const int32 li = isLight ? node - kFirstLight : -1;
				NkHierNodeName(st, node, nameBuf, sizeof(nameBuf));
				// Enfants REELLEMENT listes : un chevron qui ne deplie rien de
				// visible donne l'impression que le pliage est casse (Rihen).
				const bool hasKids = NkHierHasLiveKids(node);
				// Borne EXPLICITE : le tableau couvre 160 noeuds (5 x 32). Un index
				// hors limites ecrirait dans l'etat voisin (bug deja paye).
				const int32 foldW = (node >> 5) < 5 ? (node >> 5) : 4;
				const bool folded = ((st.hierFold[foldW] >> (node & 31)) & 1u) != 0u;
				bool chevHit = false; // clic tombe sur la fleche : pas de selection
				const bool sel = isEmpty
									 ? (demo::Demo3DHostEmptyNodeSelected(node) ||
										st.activeEmpty == node)
								 : isLight ? (selLight == li)
										   : demo::Demo3DHostObjectSelected(node);
				const bool show = !searching || NkNameMatches(nameBuf, st.searchHier);
				if (show) {
					++visibleCount;
					// Ordre AFFICHE, pour la plage Maj+clic (hors clip de
					// defilement : c'est l'ordre qui compte, pas la visibilite).
					if (visCount < 256) {
						visOrder[visCount] = node;
						visIsEmpty[visCount] = isEmpty ? 1 : 0;
						visLight[visCount] = li;
						++visCount;
					}
					const NkRect rowR{r.x, yy, r.w, kRowH};
					if (yy >= listTop - kRowH && yy < listTop + listH) {
						snprintf(key, sizeof(key), "hier.row.%d", node);
						const bool over = hit.Add(key, rowR);
						if (sel)
							p.Fill(rowR, NkRole::AccentUi);
						else
							HoverFill(p, rowR, over, 0.f);
						const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
						const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
						const float32 ind = searching ? S(6.f) : S(4.f) + (float32)depth * S(14.f);
						// CHEVRON : la SEULE commande de pliage -- le clic de ligne
						// pliait aussi, trop sensible et genant pour renommer (Rihen).
						if (hasKids && !searching) {
							// zone LARGE : le pliage doit etre aise (Rihen)
							// PLIAGE : la FLECHE seule, teste en GEOMETRIE BRUTE --
							// une zone nommee depend de l'ordre de declaration et du
							// registre ; le pliage, lui, doit toujours repondre.
							const NkRect chevR{r.x + ind - S(4.f), yy, S(24.f), kRowH};
							p.IconV(r.x + ind + S(2.f), yy, kRowH,
									folded ? NkIcon::ChevronRight : NkIcon::ChevronDown, fg, 11.f);
							if (in.mouseClicked[0] && !uiBlk && !st.hierDragging &&
								!ws.dragging &&
								NkHitRegistry::Contains(chevR, hit.Mouse())) {
								st.hierFold[foldW] ^= (1u << (node & 31));
								chevHit = true; // ce clic ne selectionne pas
							}
						}
						const float32 tx = r.x + ind + S(18.f);
						// Un OBJET UTILISATEUR de nature maillage garde l'icone maillage.
						const int32 ukind = node >= 96 ? demo::Demo3DHostUserKind(node) : 0;
						const bool isUserMesh = ukind >= 1 && ukind <= 3;
						// L'ICONE VIENT DE LA NATURE DU NOEUD, decidee en un seul
						// endroit (NkNodeIcon) : model, maillage, lumiere, camera,
						// empty... La hierarchie et le panneau de proprietes montrent
						// ainsi toujours le meme dessin (Rihen).
						p.IconV(tx, yy, kRowH, NkNodeIcon(node), fg, 13.f);
						p.Clip({rowR.x, yy, colType - rowR.x - S(8.f), kRowH});
						snprintf(key, sizeof(key), "hier.name.%d", node);
						EditableText(p, hit, ws, in, key,
									 {tx + S(18.f), yy, colType - tx - S(26.f), kRowH}, nameBuf, fg,
									 st.customNames[node], 24u);
						p.Unclip();
						// le TYPE affiche precise la nature de la lumiere (Rihen)
						static const char *const kLTt[4] = {"Soleil", "Point light",
															"Spot", "Area"};
						const char *tyTxt = isEmpty
												? NkUserKindLabel(node >= 96 ? ukind : 4)
												: (isLight ? "Lumiere" : "Maillage");
						if (isLight)
							tyTxt = kLTt[demo::Demo3DHostLightType(li) & 3];
						else if (isEmpty && ukind == 5)
							tyTxt = kLTt[demo::Demo3DHostUserSub(node) & 3];
						// Une CAMERA s'annonce « Camera » : c'est techniquement un
						// empty de sous-type 10, mais l'utilisateur voit une camera
						// dans sa scene, pas un empty (Rihen).
						else if (isEmpty && ukind == 4 &&
								 demo::Demo3DHostUserSub(node) == 10)
							tyTxt = "Camera";
						// MODEL et MESH sont deux natures DISTINCTES : le conteneur
						// s'annonce Model, sa matiere reste des maillages (Rihen).
						else if (demo::Demo3DHostNodeIsModel(node))
							tyTxt = "Model";
						else if (demo::Demo3DHostNodeIsMesh(node))
							tyTxt = "Mesh";
						// Dans un MODEL, lumieres/cameras/empties ne font PAS
						// partie du model : aides purement cosmetiques (Rihen).
						if (st.sceneTabKind[st.activeTab] == 7 &&
							(isLight || (isEmpty && (ukind == 4 || ukind == 5))))
							tyTxt = "Cosmetique";
						p.TextV(colType, yy, kRowH, tyTxt, dim);
						if (!isEmpty && !isLight && sel && node == activeObj)
							p.Fill({colType - S(12.f), yy + kRowH * 0.5f - S(2.f), S(4.f), S(4.f)}, fg);
						// L'OEIL, pour TOUS : cacher un parent cache son sous-arbre
						// (etat propre des enfants conserve, restaure au retour).
						{
							const bool hidden = isLight ? demo::Demo3DHostLightHidden(li)
														: demo::Demo3DHostObjectHidden(node);
							snprintf(key, sizeof(key), "hier.eye.%d", node);
							const NkRect eyeR{colEye - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, eyeR, hit.Add(key, eyeR) && !sel, 2.f);
							p.IconV(colEye, yy, kRowH, hidden ? NkIcon::EyeClosed : NkIcon::Eye,
									hidden ? dim : fg, 12.f);
							if (!uiBlk && hit.Clicked(key)) {
								if (isLight)
									demo::Demo3DHostSetLightHidden(li, !hidden);
								else
									demo::Demo3DHostSetObjectHidden(node, !hidden);
							}
						}
						// LA CAMERA : present dans l'IMAGE PRODUITE. Distinct de
						// l'oeil -- un repere, une lumiere temoin ou un guide
						// restent visibles pour travailler tout en n'ayant rien a
						// faire dans le rendu final (Rihen). Comme le cadenas,
						// l'icone montre l'etat EFFECTIF : une exclusion heritee
						// d'un parent se lit en teinte attenuee, sinon elle
						// paraitrait inexplicable sur l'enfant.
						{
							const bool nrOwn = demo::Demo3DHostNodeNoRender(node);
							const bool nrEff = demo::Demo3DHostNodeNoRenderEff(node);
							snprintf(key, sizeof(key), "hier.cam.%d", node);
							const NkRect camR{colCam - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, camR, hit.Add(key, camR) && !sel, 2.f);
							p.IconV(colCam, yy, kRowH,
									nrEff ? NkIcon::CameraOff : NkIcon::Camera,
									nrOwn ? fg : (nrEff ? NkRole::AccentUi : dim), 12.f);
							if (!uiBlk && hit.Clicked(key))
								demo::Demo3DHostSetNodeNoRender(node, !nrOwn);
						}
						// LE CADENAS, pour TOUS : verrouille = INselectionnable, et
						// cadenasser un parent verrouille son sous-arbre (chaque
						// enfant garde son propre drapeau).
						//
						// L'ICONE MONTRE L'ETAT EFFECTIF, pas le drapeau propre : un
						// enfant dont le parent est cadenasse refuse la selection, et
						// afficher son cadenas OUVERT rendait ce refus incomprehensible
						// (Rihen : « je ne peux selectionner ni le parent ni l'enfant »).
						// Le cadenas HERITE se dessine en teinte attenuee : on voit
						// qu'il vient d'un ancetre et qu'il ne s'ouvre pas ici.
						bool lok = false;
						bool lokEff = false;
						{
							lok = demo::Demo3DHostObjectLocked(node);
							lokEff = demo::Demo3DHostObjectLockedEff(node);
							snprintf(key, sizeof(key), "hier.lock.%d", node);
							const NkRect lockR{colLock - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, lockR, hit.Add(key, lockR) && !sel, 2.f);
							p.IconV(colLock, yy, kRowH,
									lokEff ? NkIcon::Lock : NkIcon::Unlock,
									lok ? fg : (lokEff ? NkRole::AccentUi : dim), 12.f);
							if (!uiBlk && hit.Clicked(key)) {
								// Le clic n'agit que sur SON drapeau : un verrou herite
								// se libere sur l'ancetre qui le porte.
								if (lokEff && !lok)
									NkHierLockedName(st, node, st.hierNote, sizeof(st.hierNote));
								else
									demo::Demo3DHostSetObjectLocked(node, !lok);
							}
						}
						// SELECTION : la ligne ou le nom. Un parent se selectionne
						// SEUL, un cadenasse JAMAIS ; Maj/Ctrl+clic = multi.
						bool wantSel = false;
						if (!uiBlk && !st.hierDragging && !st.delAskOpen && !chevHit) {
							snprintf(key, sizeof(key), "hier.row.%d", node);
							wantSel = hit.Clicked(key);
							snprintf(key, sizeof(key), "hier.name.%d", node);
							wantSel = wantSel || hit.Clicked(key);
						}
						if (wantSel && lokEff) {
							// REFUS EXPLIQUE : sans message, un clic sans effet passe
							// pour une panne. On nomme le verrou -- le sien ou celui
							// de l'ancetre qui le lui impose (Rihen).
							NkHierLockedName(st, node, st.hierNote, sizeof(st.hierNote));
						}
						if (wantSel) {
							// MAJ = PLAGE depuis l'ancre (appliquee apres le
							// parcours) ; CTRL = bascule un a un ; clic nu =
							// selection seule ET pose l'ancre.
							if (hit.ShiftDown() && st.hierAnchor >= 0 &&
								st.hierAnchor != node) {
								rangeTarget = node;
							} else if (isEmpty && !lokEff) {
								if (hit.CtrlDown()) {
									demo::Demo3DHostToggleEmptyNode(node); // multi successif
								} else {
									demo::Demo3DHostDeselectAll();
									demo::Demo3DHostSelectEmptyNode(node);
								}
								st.activeEmpty = node;
								st.hierAnchor = node;
							} else if (isLight) {
								if (!lokEff) {
									demo::Demo3DHostSelectLight(li);
									st.hierAnchor = node;
								}
								st.activeEmpty = -1;
							} else if (!lokEff) {
								demo::Demo3DHostSelectObject(node, hit.CtrlDown());
								st.activeEmpty = -1;
								st.hierAnchor = node;
							}
						}
						// GLISSER-DEPOSER : armement au premier appui sur la ligne ;
						// la cible est la ligne survolee au lacher.
						const nkgui::NkVec2 hm = hit.Mouse();
						if (freshPress && !uiBlk && NkHitRegistry::Contains(rowR, hm) &&
							hm.x < r.x + r.w - S(14.f)) {
							st.hierDragNode = node;
							st.hierDragX = hm.x;
							st.hierDragY = hm.y;
							st.hierDragging = false;
						}
						if (st.hierDragging && st.hierDragNode != node &&
							NkHitRegistry::Contains(rowR, hm)) {
							dropHover = node;
							p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
								   NkRole::AccentUi);
						}
						// MENU CONTEXTUEL au clic droit : TOUTE la largeur de la
						// ligne -- meme au-dessus du nom, de l'oeil, du cadenas
						// ou du chevron (les zones fines volaient le clic).
						if (in.mouseClicked[1] && !uiBlk &&
							NkHitRegistry::Contains(rowR, hm)) {
							st.hierMenuNode = node;
							st.hierMenuX = hm.x;
							st.hierMenuY = hm.y;
						}
					}
					yy += kRowH;
				}
				if (hasKids && !searching && !folded) {
					for (int32 c2 = kNNodes - 1; c2 >= 0; --c2)
						if (!NkHierNodeSkip(c2) && demo::Demo3DHostNodeParent(c2) == node &&
							sp < 196) {
							stack[sp] = c2;
							sdepth[sp] = depth + 1;
							++sp;
						}
				}
			}
			// Lacher du glisser-deposer + fantome sous le curseur.
			if (st.hierDragNode >= 0) {
				const nkgui::NkVec2 dm = hit.Mouse();
				if (hit.MouseDown()) {
					if (!st.hierDragging) {
						const float32 ddx = dm.x - st.hierDragX, ddy = dm.y - st.hierDragY;
						if (ddx * ddx + ddy * ddy > 36.f)
							st.hierDragging = true;
					}
					if (st.hierDragging) {
						NkHierNodeName(st, st.hierDragNode, nameBuf, sizeof(nameBuf));
						p.TextV(dm.x + S(14.f), dm.y - kRowH * 0.5f, kRowH, nameBuf, NkRole::Text);
					}
				} else {
					if (st.hierDragging) {
						if (NkHitRegistry::Contains(st.browserRect, dm)) {
							// DEPOSER dans le NAVIGATEUR : l'objet devient un asset
							// MESH reutilisable, souvenir de sa source (Rihen).
							if (st.browserCount < NkModelerState::kMaxBrowser) {
								const int32 k6 = st.browserCount++;
								st.browserKind[k6] = 6;
								st.browserParent[k6] = st.browserFolder;
								// ARCHIVE hote : l'asset survit a la suppression
								// de l'original dans la scene (retour de Rihen).
								const int32 arc6 =
									demo::Demo3DHostArchiveNode(st.hierDragNode);
								st.browserSrcNode[k6] =
									(arc6 >= 0 ? arc6 : st.hierDragNode) + 1;
								char bnm[32];
								NkHierNodeName(st, st.hierDragNode, bnm, sizeof(bnm));
								NkBrowUniqueName(st, 6, st.browserFolder, bnm,
												 st.browserNames[k6], 32);
							}
						} else if (dropHover >= 0 && dropHover != st.hierDragNode) {
							// Mesh -> Mesh refuse : dans un Model, la seule cible
							// est la racine (les maillages sont FRERES).
							const int32 tg = NkParentTargetAllowed(st, dropHover);
							if (tg >= 0 && tg != st.hierDragNode)
								demo::Demo3DHostSetNodeParent(st.hierDragNode, tg);
						}
						else if (dropHover < 0 && NkHitRegistry::Contains(listR, dm))
							demo::Demo3DHostSetNodeParent(st.hierDragNode, -1);
					}
					// hierDragging reste vrai jusqu'a la frame suivante : le clic
					// de relachement ne doit ni selectionner ni deselectionner.
					st.hierDragNode = -1;
				}
			}
			// ── APPLICATION DE LA PLAGE MAJ+CLIC, l'ordre affiche etant complet.
			// Additive (facon Blender) : elle ETEND la selection sans rien
			// deselectionner. Les lumieres sont sautees : leur selection est
			// UNIQUE (selLight) — chaque ligne volerait l'emplacement a la
			// precedente. Les verrouilles aussi, comme au clic.
			if (rangeTarget >= 0) {
				int32 ia = -1, ib = -1;
				for (int32 v = 0; v < visCount; ++v) {
					if (visOrder[v] == st.hierAnchor)
						ia = v;
					if (visOrder[v] == rangeTarget)
						ib = v;
				}
				if (ia >= 0 && ib >= 0) {
					if (ia > ib) {
						const int32 t = ia;
						ia = ib;
						ib = t;
					}
					for (int32 v = ia; v <= ib; ++v) {
						const int32 n4 = visOrder[v];
						if (demo::Demo3DHostObjectLockedEff(n4))
							continue;
						if (visIsEmpty[v]) {
							if (!demo::Demo3DHostEmptyNodeSelected(n4))
								demo::Demo3DHostToggleEmptyNode(n4);
						} else if (visLight[v] < 0) {
							demo::Demo3DHostSelectObject(n4, true); // additif
						}
					}
					// L'ACTIF suit la ligne cliquee quand c'est un noeud
					// utilisateur — le panneau montre ce qu'on vient de viser.
					if (rangeTarget >= kFirstEmpty2)
						st.activeEmpty = rangeTarget;
				}
			}
			st.hierMouseWasDown = hit.MouseDown();
			// CTRL+P PARENTE la selection a l'ACTIF ; MAJ+P DEPARENTE -- le
			// pendant clavier du glisser-deposer. Jamais pendant une saisie.
			if (!ws.editing) {
				bool wantP = false, wantU = false;
				for (int32 ci = 0; ci < in.charCount; ++ci) {
					const uint32 cp = in.chars[ci];
					if (cp == 'p' || cp == 'P' || cp == 16u) {
						if (in.ctrlDown)
							wantP = true;
						else if (in.shiftDown)
							wantU = true;
					}
				}
				const int32 act = st.activeEmpty >= 0
									  ? st.activeEmpty
									  : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (wantP && act < 0)
					wantP = false;
				if (wantP || wantU) {
					for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2) {
						const bool selN = n2 < kNumObj2
											  ? demo::Demo3DHostObjectSelected(n2)
											  : (selLight == n2 - kFirstLight);
						if (!selN || n2 == act)
							continue;
						demo::Demo3DHostSetNodeParent(n2, wantP ? act : -1);
					}
					if (wantU && act >= 0 && act < kFirstEmpty2)
						demo::Demo3DHostSetNodeParent(act, -1);
				}
			}
			hit.PopClip();
			p.Unclip();

			// UN CLIC DANS LE VIDE DESELECTIONNE.
			if (hit.RightClicked("hier.list") && !uiBlk && st.hierMenuNode < 0) {
				// CLIC DROIT DANS LE VIDE : le MENU DU VIDE (Ajouter / Copier /
				// Coller / Dupliquer / Supprimer — Rihen, 10 aout), le meme que
				// celui de la vue 3D. Il est peint par la vue, par-dessus tout.
				st.voidMenuOpen = 1;
				st.voidMenuX = hit.Mouse().x;
				st.voidMenuY = hit.Mouse().y;
			}
			if (hit.Clicked("hier.list") && !st.hierDragging && st.hierMenuNode < 0) {
				demo::Demo3DHostDeselectAll();
				st.activeEmpty = -1;
			}

			// Molette par CONTENANCE : les lignes recouvrent la liste, le survol
			// exact la rendait morte (constate). La barre est COLLEE au bord
			// droit ; seule sa zone de saisie s'arrete avant le splitter.
			hit.WheelIn(listR, st.scrollHier, (float32)visibleCount * kRowH, listH);
			// LA MEME BARRE QUE LES PROPRIETES (Rihen) : celle de NKEditorKit.
			NkPaintVScroll(p, guiCtx, listR, (float32)visibleCount * kRowH, st.scrollHier,
						   0x48494552u);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			char foot[72];
			snprintf(foot, sizeof(foot), "%d objet(s), %d selectionne(s)", aliveCount, selCount);
			// Un MESSAGE remplace le decompte quand une action vient d'etre
			// refusee : sans lui, un clic sans effet passe pour une panne (Rihen a
			// perdu une seance sur un objet verrouille par megarde).
			if (st.hierNote[0]) {
				p.Clip({r.x, fy, r.w - S(6.f), kRowH});
				p.TextV(r.x + kPad, fy, kRowH, st.hierNote, NkRole::AccentUi);
				p.Unclip();
				// Il s'efface au clic suivant AILLEURS que sur une ligne refusee.
				if (hit.Clicked("hier.list"))
					st.hierNote[0] = 0;
			} else {
				p.TextV(r.x + kPad, fy, kRowH, foot, NkRole::TextMuted);
			}
		}

		// â”€â”€ GIZMO DE NAVIGATION, FACON BLENDER â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Six boules reliees au centre, une par DEMI-AXE. Les positives sont PLEINES
		// et portent leur lettre ; les negatives sont CREUSES et muettes.
		//
		// C'est cette dissymetrie qui fait tout le travail : elle dit d'un coup d'oeil
		// de quel cote on regarde. Un simple trepied a trois branches, comme celui que
		// j'avais dessine, ne distingue pas +X de -X -- on ne sait donc jamais si la
		// scene est vue de face ou de dos.
		//
		// Les boules du FOND sont dessinees en PREMIER : sans cet ordre, un demi-axe
		// qui s'eloigne passerait par-dessus celui qui s'approche, et la profondeur
		// serait inversee.
		inline void PaintNavGizmo(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								  float32 cx, float32 cy, float32 radius) {
			// IL TOURNE AVEC LA VUE, comme celui de Blender. La version precedente
			// avait une projection isometrique FIGEE : elle affichait toujours la
			// meme orientation, donc elle ne disait rien de ce qu'on regardait --
			// c'etait un decor. On projette maintenant les axes du MONDE avec le
			// meme repere que la scene : X, Y et Z se placent exactement la ou ils
			// se trouvent dans l'image.
			// PORTAGE : le repere vient de la CAMERA DE LA DEMO (l'ancienne facade
			// est dormante et repondait un repere fige -- le gizmo restait statique,
			// bug signale par Rihen).
			float32 rgt[3], upv[3], fwd[3];
			demo::Demo3DHostCameraAxes(rgt, upv, fwd);

			struct Half {
					float32 wx, wy, wz;	 ///< direction MONDE du demi-axe
					NkRole role;
					const char *label;
					bool positive;
			};
			static const Half kHalf[6] = {
				{1.f, 0.f, 0.f, NkRole::AxisX, "X", true},
				{-1.f, 0.f, 0.f, NkRole::AxisX, "", false},
				{0.f, 1.f, 0.f, NkRole::AxisY, "Y", true},
				{0.f, -1.f, 0.f, NkRole::AxisY, "", false},
				{0.f, 0.f, 1.f, NkRole::AxisZ, "Z", true},
				{0.f, 0.f, -1.f, NkRole::AxisZ, "", false},
			};
			// Chaque demi-axe amene sa vue : cliquer +X regarde DEPUIS +X.
			// Correspondance avec Viewport3DAxisView : 0 face (-Z monde), 1 droite
			// (+X), 2 dessus (+Y).
			struct View {
					int32 which;
					bool opposite;
			};
			static const View kViews[6] = {
				{1, false}, {1, true},	// +X droite / -X gauche
				{2, false}, {2, true},	// +Y dessus / -Y dessous
				{0, true},	{0, false}, // +Z arriere / -Z face
			};

			const float32 ball = radius * 0.28f;
			struct Proj {
					float32 sx, sy, depth;
			};
			Proj pr[6];
			for (int32 i = 0; i < 6; ++i) {
				const float32 wx = kHalf[i].wx, wy = kHalf[i].wy, wz = kHalf[i].wz;
				// Projection sur le repere camera : droite -> +x ecran, haut -> -y
				// ecran (l'ecran descend), avant -> profondeur.
				pr[i].sx = wx * rgt[0] + wy * rgt[1] + wz * rgt[2];
				pr[i].sy = -(wx * upv[0] + wy * upv[1] + wz * upv[2]);
				// Profondeur : positif = vers l'observateur, donc l'OPPOSE de l'axe
				// Â« avant Â» de la camera, qui pointe vers la scene.
				pr[i].depth = -(wx * fwd[0] + wy * fwd[1] + wz * fwd[2]);
			}

			// Ordre de PROFONDEUR reel : ce qui est derriere se peint d'abord. Un
			// tri complet plutot qu'un tableau fige -- l'orientation change, donc
			// l'ordre aussi.
			int32 order[6] = {0, 1, 2, 3, 4, 5};
			for (int32 a = 0; a < 6; ++a)
				for (int32 b = a + 1; b < 6; ++b)
					if (pr[order[b]].depth < pr[order[a]].depth) {
						const int32 t = order[a];
						order[a] = order[b];
						order[b] = t;
					}

			// ── PASTILLE DE FOND, TRANSLUCIDE (Rihen) ───────────────────────────
			// Peinte AVANT tout le reste, donc sous les tiges et les boules. Elle
			// donne au gizmo une assise : sur une scene claire, les axes clairs se
			// perdaient dans le decor. Assez transparente pour qu'on voie la scene
			// au travers, un peu plus opaque au survol pour dire que le corps est
			// saisissable. Le noir tient sur un fond clair comme sur un fond
			// sombre, ce qu'une couleur de theme ne garantirait pas.
			const bool navHot = hit.IsHovered("nav.body");
			const NkColor navBg{0, 0, 0, (uint8)(navHot ? 82 : 48)};
			// Le TROU des demi-axes negatifs se pose SUR la pastille : un peu plus
			// dense qu'elle, il se lit comme un creux sans jamais devenir opaque.
			const NkColor navHole{0, 0, 0, (uint8)(navHot ? 150 : 120)};
			p.DiscColor(cx, cy, radius * 1.06f, navBg);
			// ROTATION LIBRE : tirer le CORPS du gizmo fait tourner la vue. Les
			// boules restent des raccourcis vers les six vues d'axe, mais elles ne
			// suffisent pas -- Blender permet aussi de le faire pivoter a la main,
			// et c'est souvent le geste le plus rapide pour se replacer.
			// La zone du corps est declaree AVANT les boules : elles la recouvrent,
			// donc viser une boule reste un clic sur la boule.
			{
				const NkRect body{cx - radius, cy - radius, radius * 2.f, radius * 2.f};
				const bool overBody = hit.Add("nav.body", body);
				if (overBody)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("nav.body")) {
					st.navDragMode = 2;
					st.navDragLastX = hit.Mouse().x;
					st.navDragLastY = hit.Mouse().y;
				}
			}

			char key[24];
			for (int32 k = 0; k < 6; ++k) {
				const int32 i = order[k];
				const float32 ex = cx + pr[i].sx * (radius - ball);
				const float32 ey = cy + pr[i].sy * (radius - ball);
				snprintf(key, sizeof(key), "nav.axis.%d", i);
				const NkRect br{ex - ball, ey - ball, ball * 2.f, ball * 2.f};
				const bool over = hit.Add(key, br);
				// La tige ne part que des demi-axes POSITIFS : six tiges feraient une
				// etoile illisible.
				if (kHalf[i].positive)
					p.Line(cx, cy, ex, ey, kHalf[i].role, 2.f);
				if (kHalf[i].positive) {
					p.Disc(ex, ey, over ? ball + 2.f : ball, kHalf[i].role);
					const float32 lw = p.TextW(kHalf[i].label);
					p.Text(ex - lw * 0.5f, ey - p.LineH() * 0.5f, kHalf[i].label,
						   NkRole::TextOnAccent);
				} else {
					// Creuse : le trou se pose sur la PASTILLE, plus la couleur du
					// fond de vue -- opaque, elle perçait un rond plein dedans.
					p.RingColor(ex, ey, over ? ball + 2.f : ball, p.C(kHalf[i].role), navHole);
				}
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked(key))
					demo::Demo3DHostAxisView(kViews[i].which, kViews[i].opposite);
			}
		}

		// â”€â”€ COLONNE DE BOUTONS DE VUE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Zoom, deplacement lateral, camera, bascule orthographique/perspective.
		// VERTICALE et sous le gizmo, comme chez Blender : ce sont des commandes de
		// NAVIGATION, pas d'edition, et les tenir a l'ecart des outils evite de
		// changer d'outil en croyant deplacer la vue.
		inline void PaintViewButtons(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									 float32 x, float32 y) {
			// ── CAPTURE : un BOUTON-MENU (regle de Rihen). Le clic devoile les
			// types, et cliquer une entree EXECUTE directement sa capture :
			//   Capture  -> la vue 3D seule, sans interface ;
			//   Tutoriel -> TOUTE la fenetre, interface comprise.
			// PNG numerotes dans captures/ du projet. La liste s'agrandira.
			// EN HAUT de la colonne : empile en bas, le bouton finissait SOUS
			// la boule de navigation, invisible.
			// ── CADENAS D'ORBITE (vue camera seulement, au-dessus de Capture,
			// regle de Rihen) : actif, la rotation ORBITE la camera autour d'un
			// centre (selection, sinon le point vise) au lieu de tourner sur
			// place -- comme Blender.
			if (demo::Demo3DHostCameraView() >= 0) {
				const float32 d0 = 26.f;
				const NkRect lb{x, y, d0, d0};
				const bool lockOn = demo::Demo3DHostCamOrbitLock();
				const bool overL = hit.Add("view.camlock", lb);
				if (lockOn)
					p.Fill(lb, NkRole::AccentUi, 4.f);
				else
					p.Outline(lb, overL ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 4.f);
				p.IconV(lb.x + (d0 - 14.f) * 0.5f, lb.y, d0,
						lockOn ? NkIcon::Lock : NkIcon::Unlock,
						lockOn ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				if (overL)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("view.camlock"))
					demo::Demo3DHostSetCamOrbitLock(!lockOn);
				y += d0 + 6.f;
			}
			{
				const float32 d0 = 26.f;
				const NkRect cb{x, y, d0, d0};
				const bool overC = hit.Add("view.shotgo", cb);
				if (st.captureMenuOpen)
					p.Fill(cb, NkRole::AccentUi, 4.f);
				else
					p.Outline(cb, overC ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 4.f);
				p.IconV(cb.x + (d0 - 14.f) * 0.5f, cb.y, d0, NkIcon::ImageRef,
						st.captureMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				if (overC)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("view.shotgo"))
					st.captureMenuOpen = !st.captureMenuOpen;
				if (st.captureMenuOpen) {
					// CAPTURE = la VUE 3D seule (regle de Rihen). Photo prend une
					// image, Video ouvre une prise -- le stub Â« a venir Â» n'avait
					// plus lieu d'etre des lors que l'enregistrement existe. Le
					// TUTORIEL, lui, concerne toute l'application : il vit au
					// footer, et les deux peuvent tourner en meme temps.
					const bool vRec = demo::Demo3DHostRecActive();
					const char *kCapM[2] = {"Photo", vRec ? "Arreter la video" : "Video"};
					static const char *const kCapKeys[2] = {"view.shot.vue", "view.shot.vid"};
					for (int32 m = 0; m < 2; ++m) {
						const NkRect mr{x + d0 + 6.f, y + (float32)m * (d0 + 2.f), 128.f, d0};
						const bool ovM = hit.Add(kCapKeys[m], mr);
						p.Fill(mr, ovM ? NkRole::AccentUi : NkRole::PanelHeader, 4.f);
						p.TextV(mr.x + 8.f, mr.y, d0, kCapM[m],
								ovM ? NkRole::TextOnAccent : NkRole::Text);
						if (ovM)
							hit.WantCursor(NkCursorWant::Hand);
						if (hit.Clicked(kCapKeys[m])) {
							if (m == 0)
								st.capturePending = 1; // la vue 3D
							else if (vRec)
								demo::Demo3DHostRecStop(true);
							else
								demo::Demo3DHostRecStart();
							st.captureMenuOpen = false;
						}
					}
				}
				y += d0 + 6.f;
			}
			// QUATRE COMMANDES DE NAVIGATION, cablees sur la DEMO PORTEE :
			//   Loupe  -> glisser = zoom (le meme chemin que sa molette) ;
			//            double-clic = pose d'ouverture ;
			//   Main   -> glisser = deplacement lateral (Â« grab Â») ;
			//   Camera -> bascule editeur <-> vol (sa touche F) ;
			//   Ortho  -> perspective / orthographique (son pave 5).
			struct VB {
					NkIcon ic;
					const char *key;
					const char *tip;
					bool enabled;
			};
			const VB kBtns[4] = {
				{NkIcon::Zoom, "view.frame", "Zoom (glisser) / recadrer (double-clic)", true},
				{NkIcon::Pan, "view.center", "Deplacer la vue (glisser)", true},
				{NkIcon::Camera, "view.cam", "Camera de vol (WASD + clic droit)", true},
				{NkIcon::Ortho, "view.ortho", "Perspective / orthographique", true},
			};
			const float32 d = 26.f;
			// Le RECADRAGE reste accessible : double-clic sur la loupe. Le
			// glissement regle le zoom, le double-clic cadre tout -- deux besoins
			// differents sur le meme bouton, comme dans la plupart des editeurs.
			if (hit.DoubleClicked("view.frame"))
				demo::Demo3DHostResetView();
			for (int32 i = 0; i < 4; ++i) {
				const NkRect br{x, y + (float32)i * (d + 6.f), d, d};
				const bool over = kBtns[i].enabled && hit.Add(kBtns[i].key, br);
				const bool on = (i == 3 && demo::Demo3DHostIsOrtho()) ||
								(i == 2 && demo::Demo3DHostIsFlyCam());
				if (on)
					p.Fill(br, NkRole::AccentUi, 4.f);
				else
					p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 4.f);
				p.IconV(br.x + (d - 14.f) * 0.5f, br.y, d, kBtns[i].ic,
						!kBtns[i].enabled ? NkRole::TextMuted
										  : (on ? NkRole::TextOnAccent : NkRole::Text),
						14.f);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (kBtns[i].enabled && hit.Clicked(kBtns[i].key)) {
					if (i == 0 || i == 1) {
						// LOUPE ET MAIN SONT DES GLISSEMENTS, pas des clics : on
						// attrape le bouton et on tire. C'est ce que fait Blender, et
						// c'est le seul acces a la navigation sur un portable sans
						// molette ni bouton du milieu. Un clic simple ne pourrait
						// exprimer ni la quantite ni la direction.
						st.navDragMode = i;
						st.navDragLastX = hit.Mouse().x;
						st.navDragLastY = hit.Mouse().y;
					} else if (i == 2) {
						demo::Demo3DHostToggleFlyCam();
					} else if (i == 3) {
						const bool o = !demo::Demo3DHostIsOrtho();
						demo::Demo3DHostSetOrtho(o);
						st.projection = o ? 1 : 0;
						st.lastProjection = st.projection;
					}
				}
			}
		}

		// â”€â”€ COULEURS DE FOND â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Les cinq prereglages + la couleur PERSONNALISEE (index 5) reglee au
		// picker. Une seule table : le bouton-temoin, le menu et le picker
		// lisent la meme source.
		inline void NkBgColorOf(const NkModelerState &st, int32 choice, float32 *out) {
			static const float32 kBgCol[5][3] = {{0.05f, 0.05f, 0.07f},
												 {0.01f, 0.01f, 0.012f},
												 {0.24f, 0.24f, 0.25f},
												 {0.62f, 0.63f, 0.65f},
												 {0.05f, 0.07f, 0.13f}};
			if (choice >= 5 || choice < 0) {
				out[0] = st.bgCustom[0];
				out[1] = st.bgCustom[1];
				out[2] = st.bgCustom[2];
			} else {
				out[0] = kBgCol[choice][0];
				out[1] = kBgCol[choice][1];
				out[2] = kBgCol[choice][2];
			}
		}

		// â”€â”€ MENU DE VUE (actions) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintViewMenuPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									   const NkRect &view, float32 barY, float32 barH) {
			if (!st.viewMenuOpen)
				return;
			int32 n = 0;
			const char *const *items = NkViewMenuItems(n);
			const NkIcon *icons = NkViewMenuIcons();
			const float32 itemH = S(24.f);
			float32 w = 0.f;
			for (int32 i = 0; i < n; ++i)
				if (p.TextW(items[i]) > w)
					w = p.TextW(items[i]);
			w += S(10.f) + S(19.f) + S(14.f);
			const NkRect box{view.x + S(10.f), barY + barH + S(4.f), w,
							 itemH * (float32)n + S(6.f)};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.menu.panel", box);
			char k[32];
			for (int32 i = 0; i < n; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "vp.menu.i%d", i);
				const bool over = hit.Add(k, ir);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, icons[i], over ? NkRole::TextOnAccent : NkRole::Text,
						13.f);
				p.TextV(ir.x + S(10.f) + S(19.f), ir.y, itemH, items[i],
						over ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(k)) {
					if (i == 0)
						demo::Demo3DHostStoreCamera();
					else if (i == 1)
						demo::Demo3DHostRecallCamera();
					else if (i == 2)
						demo::Demo3DHostResetView();
					else {
						// Panneaux : tout montrer si l'un manque, sinon tout cacher.
						const bool anyHidden = !st.showLeft || !st.showRight || !st.showBrowser;
						st.showLeft = st.showRight = st.showBrowser = anyHidden;
					}
					st.viewMenuOpen = false;
				}
			}
			if (hit.AnyClick() && !hit.IsHovered("vp.menu.panel") && !hit.IsHovered("vp.menu"))
				st.viewMenuOpen = false;
		}

		// â”€â”€ FOND : prereglages + picker de couleur personnalisee â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintBgPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								 const NkRect &view, float32 barY, float32 barH) {
			if (!st.bgMenuOpen)
				return;
			static const char *const kNames[6] = {"Fond sombre", "Fond noir",	 "Fond gris",
												  "Fond clair",  "Fond bleu nuit", "Personnalisee..."};
			const float32 itemH = S(24.f);
			float32 w = 0.f;
			for (int32 i = 0; i < 6; ++i)
				if (p.TextW(kNames[i]) > w)
					w = p.TextW(kNames[i]);
			w += S(10.f) + S(22.f) + S(26.f) + S(10.f);
			// Ancre sous le 4e bouton de la barre gauche (menu, proj, ombrage, fond).
			const float32 ax = view.x + S(10.f) + S(3.f) + 3.f * (S(28.f) + 2.f);
			const NkRect box{ax, barY + barH + S(4.f), w, itemH * 6.f + S(6.f)};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.bg.panel", box);
			char k[32];
			for (int32 i = 0; i < 6; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "vp.bg.i%d", i);
				const bool over = hit.Add(k, ir);
				const bool cur = (st.bgChoice == i);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				// Pastille de la COULEUR REELLE, picker compris : c'est elle qui
				// permet de choisir sans essayer.
				float32 c[3];
				NkBgColorOf(st, i, c);
				p.Fill({ir.x + S(8.f), ir.y + (itemH - S(12.f)) * 0.5f, S(14.f), S(12.f)},
					   NkColor{(uint8)(c[0] * 255.f), (uint8)(c[1] * 255.f), (uint8)(c[2] * 255.f), 255},
					   2.f);
				p.TextV(ir.x + S(10.f) + S(22.f), ir.y, itemH, kNames[i],
						over ? NkRole::TextOnAccent : NkRole::Text);
				if (cur)
					p.IconV(ir.x + ir.w - S(20.f), ir.y, itemH, NkIcon::Check,
							over ? NkRole::TextOnAccent : NkRole::AccentUi, 13.f);
				if (hit.Clicked(k)) {
					st.bgChoice = i;
					if (i == 5)
						st.bgPickerOpen = true; // la personnalisee OUVRE son picker
					else
						st.bgMenuOpen = st.bgPickerOpen = false;
				}
			}
			// â”€â”€ LE PICKER : trois barres R/V/B + temoin. Un vrai choix de
			// couleur, pas une roue complete -- elle viendra avec le theme.
			if (st.bgPickerOpen) {
				const float32 pw = S(210.f), ph = 3.f * S(24.f) + S(40.f);
				const NkRect pk{box.x + box.w + S(6.f), box.y, pw, ph};
				p.Fill({pk.x + 2.f, pk.y + 2.f, pk.w, pk.h}, NkRole::WindowBg, 4.f);
				p.Outline(pk, NkRole::Border, NkRole::PanelHeader, 4.f);
				hit.Add("vp.bg.picker", pk);
				static const char *const kCh[3] = {"R", "V", "B"};
				for (int32 c2 = 0; c2 < 3; ++c2) {
					const NkRect bar{pk.x + S(24.f), pk.y + S(8.f) + (float32)c2 * S(24.f) + S(4.f),
									 pw - S(36.f), S(12.f)};
					p.TextV(pk.x + S(8.f), bar.y - S(2.f), S(16.f), kCh[c2], NkRole::TextMuted);
					snprintf(k, sizeof(k), "vp.bg.ch%d", c2);
					hit.Add(k, bar);
					p.Fill(bar, NkRole::InputBg, 3.f);
					p.Fill({bar.x, bar.y, bar.w * st.bgCustom[c2], bar.h}, NkRole::AccentUi, 3.f);
					// GLISSEMENT : la barre suit la souris tant que le bouton est
					// tenu, meme sortie de la barre (c'est le canal qui possede le
					// geste, pas la position).
					if (hit.MouseDown() && (hit.IsHovered(k) || st.bgDragChannel == c2)) {
						if (st.bgDragChannel < 0 && hit.IsHovered(k))
							st.bgDragChannel = c2;
						if (st.bgDragChannel == c2) {
							float32 v = (hit.Mouse().x - bar.x) / bar.w;
							if (v < 0.f)
								v = 0.f;
							if (v > 1.f)
								v = 1.f;
							st.bgCustom[c2] = v;
							st.bgChoice = 5;
						}
					}
				}
				if (!hit.MouseDown())
					st.bgDragChannel = -1;
				// Temoin en pied : la couleur composee, en grand.
				p.Fill({pk.x + S(8.f), pk.y + ph - S(26.f), pw - S(16.f), S(18.f)},
					   NkColor{(uint8)(st.bgCustom[0] * 255.f), (uint8)(st.bgCustom[1] * 255.f),
							   (uint8)(st.bgCustom[2] * 255.f), 255},
					   3.f);
			}
			const bool overAll = hit.IsHovered("vp.bg.panel") || hit.IsHovered("vp.bg.picker") ||
								 hit.IsHovered("vp.bg");
			if (hit.AnyClick() && !overAll && st.bgDragChannel < 0)
				st.bgMenuOpen = st.bgPickerOpen = false;
		}

		// â”€â”€ MATCAPS PAR CATEGORIE, avec defilement V et H â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Les 34 boules de la bibliotheque, groupees par famille (les plages
		// suivent kPresets de NkMatcapLibrary.cpp). Le panneau defile dans les
		// deux sens des que le contenu depasse -- demande de Rihen.
		inline void PaintMatcapPopup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st) {
			if (!st.matcapOpen)
				return;
			struct Cat {
					const char *name;
					int32 first, count;
			};
			static const Cat kCats[] = {
				{"Studio", 0, 6},		 {"Controle", 6, 5},  {"Argile", 11, 5}, {"Organique", 16, 4},
				{"Metal et verre", 20, 8}, {"Stylise", 28, 2}, {"Historiques", 30, 4},
			};
			const int32 nCats = (int32)(sizeof(kCats) / sizeof(kCats[0]));
			const int32 total = demo::Demo3DHostMatcapCount();
			const float32 headH = S(20.f), cellH = S(22.f), cellW = S(150.f);
			const int32 cols = 2;
			// Taille du CONTENU (avant defilement).
			float32 contentH = S(6.f);
			for (int32 c = 0; c < nCats; ++c) {
				int32 cnt = kCats[c].count;
				if (kCats[c].first + cnt > total)
					cnt = total > kCats[c].first ? total - kCats[c].first : 0;
				contentH += headH + cellH * (float32)((cnt + cols - 1) / cols);
			}
			const float32 contentW = S(8.f) + cellW * (float32)cols;
			// Boite : ancree au bouton qui l'a ouverte, bornee a la fenetre --
			// elle peut donc s'ouvrir depuis la barre de la vue comme depuis le
			// panneau Proprietes.
			float32 boxW = contentW + S(12.f);
			if (boxW > NkPopupBoundsW() - S(40.f))
				boxW = NkPopupBoundsW() - S(40.f);
			float32 boxH = contentH + S(12.f);
			const float32 maxH = NkPopupBoundsH() - S(90.f);
			if (boxH > maxH)
				boxH = maxH;
			const NkRect box = NkFitPopup(st.matcapAnchor, boxW, boxH);
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("vp.mc.panel", box);
			const bool needV = contentH > box.h - S(8.f);
			const bool needH = contentW > box.w - S(8.f);
			const float32 viewH = box.h - S(8.f) - (needH ? S(8.f) : 0.f);
			const float32 viewW = box.w - S(8.f) - (needV ? S(8.f) : 0.f);
			// Bornes du defilement.
			float32 maxSy = contentH - viewH;
			if (maxSy < 0.f)
				maxSy = 0.f;
			float32 maxSx = contentW - viewW;
			if (maxSx < 0.f)
				maxSx = 0.f;
			// Molette = vertical (Maj = horizontal), comme partout.
			if (hit.IsHovered("vp.mc.panel") && hit.WheelDelta() != 0.f) {
				if (hit.ShiftDown())
					st.matcapScrollX -= hit.WheelDelta() * S(24.f);
				else
					st.matcapScrollY -= hit.WheelDelta() * S(24.f);
			}
			if (st.matcapScrollY < 0.f)
				st.matcapScrollY = 0.f;
			if (st.matcapScrollY > maxSy)
				st.matcapScrollY = maxSy;
			if (st.matcapScrollX < 0.f)
				st.matcapScrollX = 0.f;
			if (st.matcapScrollX > maxSx)
				st.matcapScrollX = maxSx;

			// Contenu (borne a la boite : pas de clip pixel, on SAUTE les lignes
			// hors champ -- suffisant pour des rangees regulieres).
			const int32 cur = demo::Demo3DHostMatcap();
			float32 y = box.y + S(4.f) - st.matcapScrollY;
			const float32 x0 = box.x + S(4.f) - st.matcapScrollX;
			char k[32];
			for (int32 c = 0; c < nCats; ++c) {
				int32 cnt = kCats[c].count;
				if (kCats[c].first + cnt > total)
					cnt = total > kCats[c].first ? total - kCats[c].first : 0;
				if (cnt <= 0)
					continue;
				if (y + headH > box.y && y < box.y + viewH)
					p.TextV(x0 + S(6.f), y, headH, kCats[c].name, NkRole::TextMuted);
				y += headH;
				for (int32 i = 0; i < cnt; ++i) {
					const int32 id = kCats[c].first + i;
					const float32 cx = x0 + (float32)(i % cols) * cellW;
					const float32 cy = y + (float32)(i / cols) * cellH;
					if (cy + cellH < box.y || cy > box.y + viewH) {
						continue;
					}
					const NkRect ir{cx, cy, cellW - S(4.f), cellH - 2.f};
					snprintf(k, sizeof(k), "vp.mc.%d", id);
					const bool over = hit.Add(k, ir);
					const bool sel = (id == cur);
					if (sel)
						p.Fill(ir, NkRole::AccentUi, 3.f);
					else if (over)
						p.Fill(ir, NkRole::PanelHeader, 3.f);
					// La VIGNETTE REELLE de la boule, pas un pictogramme generique.
					p.Image(4300u + (uint32)id,
							{ir.x + S(3.f), ir.y + S(2.f), ir.h - S(4.f), ir.h - S(4.f)});
					p.TextV(ir.x + ir.h + S(4.f), ir.y, ir.h, demo::Demo3DHostMatcapName(id),
							sel ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(k))
						demo::Demo3DHostSetMatcap(id);
				}
				y += cellH * (float32)((cnt + cols - 1) / cols);
			}

			// â”€â”€ ASCENSEURS : VERTICAL puis HORIZONTAL, glissables â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			if (needV) {
				const NkRect track{box.x + box.w - S(8.f), box.y + S(4.f), S(5.f), viewH};
				p.Fill(track, NkRole::InputBg, 2.f);
				const float32 thH = viewH * (viewH / contentH) < S(20.f)
									   ? S(20.f)
									   : viewH * (viewH / contentH);
				const float32 thY = track.y + (track.h - thH) * (maxSy > 0.f ? st.matcapScrollY / maxSy : 0.f);
				const NkRect th{track.x, thY, track.w, thH};
				hit.Add("vp.mc.sbv", th);
				p.Fill(th, NkRole::AccentUi, 2.f);
				if (hit.MouseDown() && (hit.IsHovered("vp.mc.sbv") || st.matcapDragBar == 0)) {
					if (st.matcapDragBar < 0 && hit.IsHovered("vp.mc.sbv")) {
						st.matcapDragBar = 0;
						st.matcapDragOff = hit.Mouse().y - thY;
					}
					if (st.matcapDragBar == 0 && track.h > thH)
						st.matcapScrollY =
							(hit.Mouse().y - st.matcapDragOff - track.y) / (track.h - thH) * maxSy;
				}
			}
			if (needH) {
				const NkRect track{box.x + S(4.f), box.y + box.h - S(8.f), viewW, S(5.f)};
				p.Fill(track, NkRole::InputBg, 2.f);
				const float32 thW = viewW * (viewW / contentW) < S(20.f)
									   ? S(20.f)
									   : viewW * (viewW / contentW);
				const float32 thX = track.x + (track.w - thW) * (maxSx > 0.f ? st.matcapScrollX / maxSx : 0.f);
				const NkRect th{thX, track.y, thW, track.h};
				hit.Add("vp.mc.sbh", th);
				p.Fill(th, NkRole::AccentUi, 2.f);
				if (hit.MouseDown() && (hit.IsHovered("vp.mc.sbh") || st.matcapDragBar == 1)) {
					if (st.matcapDragBar < 0 && hit.IsHovered("vp.mc.sbh")) {
						st.matcapDragBar = 1;
						st.matcapDragOff = hit.Mouse().x - thX;
					}
					if (st.matcapDragBar == 1 && track.w > thW)
						st.matcapScrollX =
							(hit.Mouse().x - st.matcapDragOff - track.x) / (track.w - thW) * maxSx;
				}
			}
			if (!hit.MouseDown())
				st.matcapDragBar = -1;
			// Le panneau s'ouvre depuis DEUX boutons (barre de la vue, panneau
			// Proprietes) : la fermeture au clic exterieur doit les connaitre
			// tous les deux -- sinon le clic d'OUVERTURE du second refermait le
			// panneau dans la meme frame (constate par Rihen).
			if (hit.AnyClick() && !hit.IsHovered("vp.mc.panel") && !hit.IsHovered("vp.matcap") &&
				!hit.IsHovered("props.matcap"))
				st.matcapOpen = false;
		}

		// â”€â”€ VUE 3D (centre) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintViewport(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								  NkHitRegistry &hit, NkWidgetState &ws,
								  const nkgui::NkGuiInput &in, NkComboPending &combo,
								  NkCheckPending &checks, const NkShortcutTable &sc) {
			// Le bloc de surcouche est RE-ARME chaque frame par qui en a besoin
			// (badge vue camera...) : on repart de zero ici, sinon un bloc
			// perime survivrait au changement d'onglet et refuserait des clics
			// sans raison visible.
			hit.SetBlock(NkRect{0.f, 0.f, 0.f, 0.f}, false);
			const bool editMode = (st.mode != NkMode::Object);

			// â”€â”€ TAB BAR D'ESPACES DE TRAVAIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Un seul espace aujourd'hui (Modelisation) ; Sculpt, Texturing et
			// NkAnima s'y rangeront. L'en-tete s'ESCAMOTE d'un clic sur le
			// chevron -- replie, seul un petit chevron discret le rappelle.
			float32 wsBarH = 0.f;
			if (st.wsBarOpen) {
				wsBarH = S(24.f);
				const NkRect wb{r.x, r.y, r.w, wsBarH};
				p.Fill(wb, NkRole::PanelHeader);
				p.HLine(r.x, r.y + wsBarH - 1.f, r.w);
				// LES ESPACES SONT LES MODES (regle de Rihen) : Objet, Edition,
				// Sculpture 2.5D, Sculpture, Texturing -- l'onglet actif EST le
				// mode courant. Le deroulant equivalent de la barre d'outils a
				// ete retire : un seul endroit pour changer de mode.
				static const NkIcon kWsIc[7] = {NkIcon::Mesh,	NkIcon::Edit,
												NkIcon::Layers, NkIcon::Ruler,
												NkIcon::Overlay, NkIcon::ViewUV,
												NkIcon::Picker};
				static const char *const kWsNames[7] = {
					"Objet",	 "Edition", "Sculpture 2.5D",	"Sculpture",
					"Texturing", "Patron",	"Texture painting"};
				float32 tx5 = r.x + S(8.f);
				char wk5[16];
				for (int32 t5 = 0; t5 < 7; ++t5) {
					const float32 tw5 = p.TextW(kWsNames[t5]);
					const NkRect t0{tx5, r.y + S(2.f), tw5 + S(32.f), wsBarH - S(4.f)};
					snprintf(wk5, sizeof(wk5), "ws.tab.%d", t5);
					const bool overT = hit.Add(wk5, t0);
					const bool onT = (int32)st.mode == t5;
					if (onT)
						p.Fill(t0, NkRole::AccentUi, 3.f);
					else if (overT)
						p.Fill(t0, NkRole::InputBg, 3.f);
					p.IconV(t0.x + S(6.f), t0.y, t0.h, kWsIc[t5],
							onT ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
					p.TextV(t0.x + S(24.f), t0.y, t0.h, kWsNames[t5],
							onT ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(wk5))
						st.mode = (NkMode)t5;
					tx5 += t0.w + S(4.f);
					// MARQUEUR DE SEPARATION entre onglets (regle de Rihen) :
					// sans lui, sept libelles cote a cote se lisaient comme une
					// seule phrase.
					if (t5 < 6) {
						p.VLine(tx5, r.y + S(5.f), wsBarH - S(10.f));
						tx5 += S(5.f);
					}
				}
				const NkRect ch{r.x + r.w - S(26.f), r.y + S(2.f), S(20.f), wsBarH - S(4.f)};
				HoverFill(p, ch, hit.Add("ws.hide", ch), 2.f);
				p.IconV(ch.x + S(3.f), ch.y, ch.h, NkIcon::ChevronUp, NkRole::TextMuted, 12.f);
				if (hit.Clicked("ws.hide"))
					st.wsBarOpen = false;
			} else {
				// Replie : une POIGNEE VISIBLE, comme celles des panneaux -- le
				// chevron seul de 20 px etait introuvable (constate par Rihen).
				const NkRect ch{r.x + r.w * 0.5f - S(52.f), r.y + S(2.f), S(104.f), S(16.f)};
				const bool overC = hit.Add("ws.show", ch);
				p.Outline(ch, overC ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				p.IconV(ch.x + S(6.f), ch.y, ch.h, NkIcon::ChevronDown, NkRole::Text, 11.f);
				p.TextV(ch.x + S(22.f), ch.y, ch.h, "Espaces", NkRole::TextMuted);
				if (hit.Clicked("ws.show"))
					st.wsBarOpen = true;
			}
			// LA VUE EST RECADREE sous la barre d'espaces : sans cela elle
			// coupait le haut de l'image (le texte du HUD, constate par Rihen).
			NkRect vr = r;
			vr.y += wsBarH;
			vr.h -= wsBarH;

			// â”€â”€ LA VUE 3D REELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce qui etait peint ici jusqu'a present -- un sol en fuyantes dessine a
			// la main -- etait un DECOR. Il donnait l'impression d'une perspective
			// sans camÃ©ra, sans profondeur et sans le moindre objet : rien de ce
			// qu'on y voyait ne pouvait etre selectionne, tourne ni modifie.
			//
			// A la place, la scene est rendue par NKRenderer dans une cible hors
			// ecran, sur le meme device et le meme command buffer que l'interface,
			// et on pose ici SA TEXTURE. Aucune relecture CPU, aucune seconde pile
			// GPU : l'image ne quitte jamais la carte.
			// Fond de la ZONE IMAGE seulement (vr, pas r) : peindre r entier
			// recouvrait la barre d'espaces peinte juste au-dessus.
			p.Fill(vr, NkRole::ViewportTop); // visible tant que la 3D n'est pas prete
			// ONGLET EDITEUR : seul MODEL s'edite dans un vrai viewport (meme
			// interface qu'une scene, regle de Rihen). Materiau, texture,
			// blueprint et dataset = CONTENU VIDE tant que leur design n'est
			// pas defini (peinture, nodes proceduraux, NKGraphe a venir).
			if (st.sceneTabKind[st.activeTab] != 0 &&
				st.sceneTabKind[st.activeTab] != 7) {
				const uint8 ek = (uint8)(st.sceneTabKind[st.activeTab] - 1);
				const char *en = (ek == 0)	 ? "Editeur de Graphe"
								 : (ek == 2) ? "Editeur de Materiau"
								 : (ek == 3) ? "Editeur de Texture"
								 : (ek == 4) ? "Editeur de Dataset IA"
								 : (ek == 6) ? "Editeur de Model"
											 : "Editeur";
				p.Fill(vr, NkRole::WindowBg);
				const float32 cxE = vr.x + vr.w * 0.5f;
				const float32 cyE = vr.y + vr.h * 0.5f;
				p.TextV(cxE - p.TextW(en) * 0.5f, cyE - S(36.f), kRowH, en,
						NkRole::Text);
				const int32 aiE = st.sceneTabAsset[st.activeTab] - 1;
				if (aiE >= 0 && aiE < st.browserCount)
					p.TextV(cxE - p.TextW(st.browserNames[aiE]) * 0.5f,
							cyE - S(12.f), kRowH, st.browserNames[aiE],
							NkRole::TextMuted);
				p.TextV(cxE - p.TextW("Interface a definir -- NKGraphe, peinture, "
									  "procedural a venir") *
								 0.5f,
						cyE + S(12.f), kRowH,
						"Interface a definir -- NKGraphe, peinture, procedural a venir",
						NkRole::TextMuted);
				st.viewRect = {0.f, 0.f, 0.f, 0.f}; // pas de depot 3D ici
				return;
			}
			// PORTAGE INTEGRAL de --demo=2 : la texture vient desormais de la demo
			// portee (NkDemo3D.cpp), sous le MEME id 4096. L'ancienne vue est
			// dormante ; c'est donc l'hote de la demo qui dit Â« pret Â».
			if (demo::Demo3DHostReady()) {
				p.Image(nk3d::kViewportTexId, vr);
				st.viewRect = vr; // depot d'assets : importer un clone en scene
				// ── PASSE-PARTOUT (Rihen) : en vue camera, ce qui deborde du
				// CADRE de la camera est voile -- couleur/opacite PAR camera
				// (noir 60 % par defaut, panneau de la camera). Cadre 16:9
				// (Full HD) en v1 ; la pastille Output pilotera le format.
				// Habillage UI : il n'apparait PAS dans les captures, qui
				// figent la cible hors ecran en dessous.
				if (st.camViewNode > 0) {
					float32 pp[4];
					demo::Demo3DHostCamPasse(st.camViewNode - 1, pp);
					if (pp[3] > 0.003f && vr.w > 8.f && vr.h > 8.f) {
						// LE CADRE VIENT DE L'HOTE : c'est la MEME verite que le
						// rendu (qui zoome pour que l'image exacte de la camera
						// occupe ce cadre) et que la capture (qui recadre
						// dessus). Le voile epouse donc les VRAIS bords de la
						// camera (exigence de Rihen).
						float32 fr4[4];
						demo::Demo3DHostCameraFrame(fr4);
						const float32 fx = vr.x + fr4[0] * vr.w;
						const float32 fy = vr.y + fr4[1] * vr.h;
						const float32 fw = fr4[2] * vr.w;
						const float32 fh = fr4[3] * vr.h;
						const NkColor pc{(uint8)(pp[0] * 255.f), (uint8)(pp[1] * 255.f),
										 (uint8)(pp[2] * 255.f), (uint8)(pp[3] * 255.f)};
						if (fy > vr.y)
							p.Fill({vr.x, vr.y, vr.w, fy - vr.y}, pc, 0.f);
						if (fy + fh < vr.y + vr.h)
							p.Fill({vr.x, fy + fh, vr.w, vr.y + vr.h - fy - fh}, pc, 0.f);
						if (fx > vr.x)
							p.Fill({vr.x, fy, fx - vr.x, fh}, pc, 0.f);
						if (fx + fw < vr.x + vr.w)
							p.Fill({fx + fw, fy, vr.x + vr.w - fx - fw, fh}, pc, 0.f);
						// Lisere fin du cadre, discret.
						const NkColor fr{255, 255, 255, 70};
						p.Fill({fx, fy, fw, 1.f}, fr, 0.f);
						p.Fill({fx, fy + fh - 1.f, fw, 1.f}, fr, 0.f);
						p.Fill({fx, fy, 1.f, fh}, fr, 0.f);
						p.Fill({fx + fw - 1.f, fy, 1.f, fh}, fr, 0.f);
						// ── GUIDES DE COMPOSITION + ZONES SURES (par camera) ──
						// Traces DANS le cadre : habillage de la vue, jamais dans
						// les captures (elles figent la cible en dessous).
						const int32 gd2 = demo::Demo3DHostCamGuides(st.camViewNode - 1);
						if (gd2) {
							const NkColor glc{255, 255, 255, 55};
							if (gd2 & 1) { // tiers
								p.Fill({fx + fw / 3.f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx + 2.f * fw / 3.f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh / 3.f, fw, 1.f}, glc, 0.f);
								p.Fill({fx, fy + 2.f * fh / 3.f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 2) { // centre
								p.Fill({fx + fw * 0.5f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.5f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 4) { // diagonales
								p.Line(fx, fy, fx + fw, fy + fh, glc, 1.f);
								p.Line(fx + fw, fy, fx, fy + fh, glc, 1.f);
							}
							if (gd2 & 8) { // nombre d'or (0,382 / 0,618)
								p.Fill({fx + fw * 0.382f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx + fw * 0.618f, fy, 1.f, fh}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.382f, fw, 1.f}, glc, 0.f);
								p.Fill({fx, fy + fh * 0.618f, fw, 1.f}, glc, 0.f);
							}
							if (gd2 & 16) { // zones sures : action 3,5 %, titre 10/5 %
								const NkColor zsa{120, 220, 130, 80};
								const NkColor zst{220, 140, 120, 80};
								const float32 mrg[2][2] = {{0.035f, 0.035f}, {0.10f, 0.05f}};
								for (int32 z2 = 0; z2 < 2; ++z2) {
									const NkColor &zc = z2 == 0 ? zsa : zst;
									const float32 sx = fx + fw * mrg[z2][0];
									const float32 sy = fy + fh * mrg[z2][1];
									const float32 sw = fw * (1.f - 2.f * mrg[z2][0]);
									const float32 sh = fh * (1.f - 2.f * mrg[z2][1]);
									p.Fill({sx, sy, sw, 1.f}, zc, 0.f);
									p.Fill({sx, sy + sh - 1.f, sw, 1.f}, zc, 0.f);
									p.Fill({sx, sy, 1.f, sh}, zc, 0.f);
									p.Fill({sx + sw - 1.f, sy, 1.f, sh}, zc, 0.f);
								}
							}
						}
					}
					// ── APERCU DES INCRUSTATIONS (Rihen) ────────────────────
					// Les miniatures posees sur l'image de sortie se voient DANS
					// le cadre, a leur place et a leur forme. Sans cet apercu il
					// faudrait lancer un rendu pour savoir ou elles tombent : on
					// les placerait a l'aveugle. Comme les guides, c'est de
					// l'habillage -- la capture fige la cible en dessous et n'en
					// garde rien.
					// HORS du bloc du passe-partout, et non dedans : un voile a
					// zero pour cent ne doit pas faire disparaitre l'apercu.
					// SEULEMENT DEPUIS LA SOURCE PRINCIPALE (Rihen) : les
					// miniatures appartiennent a la composition de l'image
					// principale. Les montrer alors qu'on regarde par une camera
					// qui n'est PAS cette source -- par exemple celle qui
					// alimente une miniature -- faisait croire qu'elles se
					// composeraient aussi dans cette vue-la. Depuis une source
					// secondaire, on doit voir ce que cette camera voit, rien de
					// plus.
					int32 mainSrc = -1;
					demo::Demo3DHostOutMain(&mainSrc, nullptr, nullptr, nullptr, nullptr,
											nullptr);
					const int32 curCam = st.camViewNode > 0 ? st.camViewNode - 1 : -1;
					if (curCam == mainSrc) {
						float32 fr5[4];
						demo::Demo3DHostCameraFrame(fr5);
						const float32 fx = vr.x + fr5[0] * vr.w;
						const float32 fy = vr.y + fr5[1] * vr.h;
						const float32 fw = fr5[2] * vr.w;
						const float32 fh = fr5[3] * vr.h;
						const int32 nMaxI = demo::Demo3DHostOutInsetMax();
						for (int32 q2 = 0; q2 < nMaxI; ++q2) {
							int32 iSrc = -1, iShape = 0;
							float32 iXY[2] = {0.f, 0.f}, iSz[2] = {0.25f, 0.25f}, iBrd = 2.f,
									iCol[3] = {1.f, 1.f, 1.f}, iOpa = 1.f;
							if (!demo::Demo3DHostOutInset(q2, &iSrc, &iShape, iXY, iSz, &iBrd,
														  iCol, &iOpa))
								continue;
							// MEME regle de cadre que le rendu : chaque forme a
							// ses dimensions, et celles a un seul cote se ferment
							// sur un carre. Une autre formule ici mentirait sur
							// le resultat.
							const float32 iw = fw * iSz[0];
							const float32 ih = (nk3d::NkInsetDimCount(iShape) == 1)
												   ? iw
												   : fh * iSz[1];
							const float32 ix = fx + iXY[0] * fw;
							const float32 iy = fy + iXY[1] * fh;
							const NkColor bc{(uint8)(iCol[0] * 255.f), (uint8)(iCol[1] * 255.f),
											 (uint8)(iCol[2] * 255.f), (uint8)(200.f * iOpa)};
							const NkColor fill{(uint8)(iCol[0] * 255.f), (uint8)(iCol[1] * 255.f),
											   (uint8)(iCol[2] * 255.f), (uint8)(38.f * iOpa)};
							if (iShape == 2 || iShape == 3) {
								// Rond : disque tres pale borde d'un anneau.
								const float32 rr2 = (iw < ih ? iw : ih) * 0.5f;
								p.DiscColor(ix + iw * 0.5f, iy + ih * 0.5f, rr2, fill);
								p.RingColor(ix + iw * 0.5f, iy + ih * 0.5f, rr2, bc, fill);
							} else if (iShape == 5) {
								const float32 cx2 = ix + iw * 0.5f, cy2 = iy + ih * 0.5f;
								p.Line(cx2, iy, ix + iw, cy2, bc, 1.f);
								p.Line(ix + iw, cy2, cx2, iy + ih, bc, 1.f);
								p.Line(cx2, iy + ih, ix, cy2, bc, 1.f);
								p.Line(ix, cy2, cx2, iy, bc, 1.f);
							} else {
								const float32 rnd = (iShape == 4) ? ih * 0.18f : 0.f;
								p.Fill({ix, iy, iw, ih}, fill, rnd);
								p.Fill({ix, iy, iw, 1.f}, bc, 0.f);
								p.Fill({ix, iy + ih - 1.f, iw, 1.f}, bc, 0.f);
								p.Fill({ix, iy, 1.f, ih}, bc, 0.f);
								p.Fill({ix + iw - 1.f, iy, 1.f, ih}, bc, 0.f);
							}
							// Numero et source : c'est ce qui permet de savoir
							// laquelle on regarde quand elles se recouvrent.
							char cn2[24] = {};
							if (iSrc >= 0)
								NkHierNodeName(st, iSrc, cn2, sizeof(cn2));
							char il[48];
							snprintf(il, sizeof(il), "%d - %s", (int)(q2 + 1),
									 iSrc < 0 ? "Vue 3D" : (cn2[0] ? cn2 : "Camera"));
							p.Clip({ix, iy, iw, ih});
							p.TextV(ix + S(6.f), iy + S(2.f), S(18.f), il, NkRole::Text);
							p.Unclip();
						}
					}
				}
				// ── VUE CAMERA (Rihen) ──────────────────────────────────────
				// Bascule entre la vue 3D libre et CE QUE VOIT une camera de la
				// scene. Le selecteur liste les cameras du document actif.
				{
					// SYNC AVEC L'HOTE : la bascule CLAVIER (pave 0 / Ctrl+0)
					// change la vue cote hote -- le libelle du selecteur suit.
					{
						const int32 hostCv = demo::Demo3DHostCameraView();
						if (hostCv + 1 != st.camViewNode)
							st.camViewNode = hostCv >= 0 ? hostCv + 1 : 0;
					}
					if (st.camViewNode > 0 &&
						(NkHierNodeSkip(st.camViewNode - 1) ||
						 demo::Demo3DHostUserSub(st.camViewNode - 1) != 10)) {
						// la camera regardee a disparu (ou change de document) :
						// retour vue libre par le chemin unique (pose restituee)
						st.camViewNode = 0;
						demo::Demo3DHostViewCamera(-1);
					}
					// LEVEE du bloc AVANT de peindre le badge : ses propres clics
					// doivent repondre (Clicked refuse tout clic dans l'emprise
					// bloquee). Il sera re-arme en fin de section avec l'emprise
					// badge + liste.
					hit.SetBlock(NkRect{0.f, 0.f, 0.f, 0.f}, false);
					char vlb[48];
					if (st.camViewNode > 0) {
						char cnm[24];
						NkHierNodeName(st, st.camViewNode - 1, cnm, sizeof(cnm));
						snprintf(vlb, sizeof(vlb), "Vue camera : %s", cnm);
					} else {
						snprintf(vlb, sizeof(vlb), "Vue 3D");
					}
					const float32 vw = p.TextW(vlb) + S(34.f);
					// SOUS la barre d'icones du viewport : posee a la meme place,
					// elle RECOUVRAIT le selecteur (constate a l'ecran).
					const NkRect vb{vr.x + S(8.f), vr.y + S(44.f), vw, S(20.f)};
					const bool ovV = hit.Add("view.pick", vb);
					// Fond PLEIN : pose sur l'image 3D, un simple contour se perd.
					p.Fill(vb, NkColor{0, 0, 0, 150}, 4.f);
					p.Outline(vb, ovV || st.camPickOpen ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 4.f);
					p.IconV(vb.x + S(5.f), vb.y, vb.h, NkIcon::Camera,
							st.camViewNode > 0 ? NkRole::AccentUi : NkRole::TextMuted,
							12.f);
					p.TextV(vb.x + S(22.f), vb.y, vb.h, vlb);
					// LA CLE DU CLIC EST CELLE DU RECT ENREGISTRE (« view.pick »).
					// Elle testait « view.cam » -- la cle du bouton camera de VOL
					// de la colonne de navigation : le selecteur ne s'ouvrait
					// jamais par lui-meme, et le bouton de vol l'ouvrait par
					// accident (LE probleme de camera constate par Rihen).
					if (hit.Clicked("view.pick"))
						st.camPickOpen = !st.camPickOpen;
					if (st.camPickOpen) {
						// Liste : la vue 3D, puis TOUTES les cameras du document.
						int32 cams[16];
						int32 nCam = 0;
						const int32 ncT = demo::Demo3DHostNodeCount();
						for (int32 c1 = 0; c1 < ncT && nCam < 16; ++c1)
							if (!NkHierNodeSkip(c1) &&
								demo::Demo3DHostUserKind(c1) == 4 &&
								demo::Demo3DHostUserSub(c1) == 10)
								cams[nCam++] = c1;
						const NkRect lb{vb.x, vb.y + vb.h + 2.f, vb.w < S(150.f) ? S(150.f)
																			 : vb.w,
										kRowH * (float32)(nCam + 1)};
						p.Fill({lb.x + 2.f, lb.y + 2.f, lb.w, lb.h}, NkColor{0, 0, 0, 90}, 4.f);
						p.Outline(lb, NkRole::AccentUi, NkRole::PanelHeader, 4.f);
						const NkRect i0{lb.x, lb.y, lb.w, kRowH};
						HoverFill(p, i0, hit.Add("view.cam.free", i0), 0.f);
						p.TextV(i0.x + S(10.f), i0.y, kRowH, "Vue 3D");
						if (hit.Clicked("view.cam.free")) {
							// RETOUR vue libre par le CHEMIN UNIQUE de l'hote --
							// c'est lui qui memorise et restitue la pose, pour que
							// selecteur et pave 0 restent d'accord.
							demo::Demo3DHostViewCamera(-1);
							st.camViewNode = 0;
							st.camPickOpen = false;
						}
						for (int32 c2 = 0; c2 < nCam; ++c2) {
							const NkRect ic{lb.x, lb.y + kRowH * (float32)(c2 + 1), lb.w,
											kRowH};
							char ck2[32], cnm2[24];
							snprintf(ck2, sizeof(ck2), "view.cam.%d", cams[c2]);
							HoverFill(p, ic, hit.Add(ck2, ic), 0.f);
							NkHierNodeName(st, cams[c2], cnm2, sizeof(cnm2));
							p.IconV(ic.x + S(8.f), ic.y, kRowH, NkIcon::Camera,
									st.camViewNode == cams[c2] + 1 ? NkRole::AccentUi
																  : NkRole::TextMuted,
									12.f);
							p.TextV(ic.x + S(26.f), ic.y, kRowH, cnm2);
							if (hit.Clicked(ck2)) {
								// CHEMIN UNIQUE de l'hote : il memorise la pose
								// libre une seule fois, regarde cette camera et la
								// rend ACTIVE (facon Blender) -- le pave 0 y
								// reviendra directement.
								demo::Demo3DHostViewCamera(cams[c2]);
								st.camViewNode = cams[c2] + 1;
								st.camPickOpen = false;
							}
						}
						if (hit.AnyClick() && !NkHitRegistry::Contains(lb, hit.Mouse()) &&
							!hit.IsHovered("view.pick"))
							st.camPickOpen = false;
						// SURCOUCHE BLOQUANTE (patron NKCode, mecanisme SetBlock
						// deja porte ici mais jamais arme) : l'emprise badge +
						// liste refuse ses clics au reste de l'application --
						// la scene comprise, qui recevait selection et
						// deselection fantomes a travers la liste (Rihen).
						const float32 bw3 = (lb.w > vb.w ? lb.w : vb.w);
						hit.SetBlock({vb.x, vb.y, bw3, (lb.y + lb.h) - vb.y}, true);
					}
					if (!st.camPickOpen)
						hit.SetBlock(vb, true); // badge seul : son clic ne traverse pas non plus
				}
				if (!st.wsBarOpen) {
					// La poignee « Espaces » se REPEINT par-dessus l'image : elle
					// etait recouverte, donc introuvable barre fermee (Rihen).
					const NkRect ch2{r.x + r.w * 0.5f - S(52.f), r.y + S(2.f), S(104.f),
									 S(16.f)};
					p.Outline(ch2, NkRole::Border, NkRole::PanelHeader, 4.f);
					p.IconV(ch2.x + S(6.f), ch2.y, ch2.h, NkIcon::ChevronDown,
							NkRole::TextMuted, 12.f);
					p.TextV(ch2.x + S(22.f), ch2.y, ch2.h, "Espaces", NkRole::TextMuted);
				}
				// ── AJUSTER LA CREATION (facon Blender), bas-droit de la vue ──
				// Chaque nature a SES champs (regles de Rihen) : sphere =
				// segments/anneaux/rayon ; icosphere = subdivisions/rayon ; tore =
				// deux rayons ; cube = largeur/hauteur/profondeur... Valider par
				// « Appliquer » ou par un clic dans la vue.
				if (st.addAdjustNode >= 0) {
					int32 sgA = 0, rgA = 0;
					float32 axA = 0.15f;
					const bool hasParams =
						demo::Demo3DHostMeshParams(st.addAdjustNode, &sgA, &rgA, &axA);
					if (demo::Demo3DHostNodeDeleted(st.addAdjustNode)) {
						st.addAdjustNode = -1;
					} else {
						const int32 ukA = demo::Demo3DHostUserKind(st.addAdjustNode);
						const int32 sbA = demo::Demo3DHostUserSub(st.addAdjustNode);
						const bool isSph = ukA == 1 && sbA == 0;
						const bool isIco = ukA == 1 && sbA == 1;
						const bool isTor = ukA == 1 && sbA == 2;
						const bool isCap = ukA == 1 && sbA == 3;
						const bool isCyl = ukA == 2 && (sbA == 1 || sbA == 2);
						const bool isPln = ukA == 3;
						const bool isCir = ukA == 10;
						const bool showSeg = hasParams;
						const bool showRing = isSph || isTor || isCap;
						const bool showRay = isSph || isIco || isTor || isCap || isCyl || isCir;
						const bool showHaut = isCap || isCyl;
						const bool showAux = isTor;
						const bool showLHP = !showRay && ukA != 5; // cube, plan, vides...
						const char *segLbl =
							isIco ? "Subdivisions" : (isPln ? "Divisions" : "Segments");
						int32 tyP = -1;
						float32 rgP = 0.f, inP = 0.f, outP = 0.f, awP = 0.f, ahP = 0.f;
						bool shP = true;
						if (ukA == 5)
							demo::Demo3DHostLightEx(st.addAdjustNode, &rgP, &inP, &outP, &awP,
													&ahP, &shP, &tyP);
						const int32 rowsN = 2 + (showSeg ? 1 : 0) + (showRing ? 1 : 0) +
											(showRay ? 1 : 0) + (showHaut ? 1 : 0) +
											(showAux ? 1 : 0) + (showLHP ? (isPln ? 2 : 3) : 0) +
											(ukA == 5 ? 1 + (tyP != 0 ? 1 : 0) +
															(tyP == 2 ? 2 : 0) +
															(tyP == 3 ? 2 : 0)
													  : 0);
						const float32 pw = S(232.f);
						const float32 ph = kRowH * (float32)rowsN + S(10.f);
						const NkRect aj{vr.x + vr.w - pw - S(10.f), vr.y + vr.h - ph - S(10.f),
										pw, ph};
						if (hit.AnyClick() && NkHitRegistry::Contains(vr, hit.Mouse()) &&
							!NkHitRegistry::Contains(aj, hit.Mouse())) {
							st.addAdjustNode = -1; // un clic dans la vue VALIDE
						} else {
							p.Outline(aj, NkRole::Border, NkRole::PanelHeader, 4.f);
							hit.Add("vp.adjust", aj);
							char nmA[32];
							NkHierNodeName(st, st.addAdjustNode, nmA, sizeof(nmA));
							char tA[48];
							snprintf(tA, sizeof(tA), "Creation : %s", nmA);
							p.TextV(aj.x + S(8.f), aj.y + S(3.f), kRowH, tA);
							float32 ay = aj.y + S(3.f) + kRowH;
							auto AdjRow = [&](const char *lbl, const char *key2, float32 &val,
											  float32 step, const char *fmt2) {
								p.TextV(aj.x + S(8.f), ay, kRowH, lbl, NkRole::TextMuted);
								const bool ch2 = DragFloat(
									p, hit, ws, in, key2,
									{aj.x + S(104.f), ay + S(3.f), pw - S(112.f), kRowH - S(4.f)},
									val, step, NkRole::AccentUi, fmt2);
								ay += kRowH;
								return ch2;
							};
							float32 fsg = (float32)sgA, frg = (float32)rgA, fax = axA;
							bool prmCh = false;
							if (showSeg)
								prmCh |= AdjRow(segLbl, "vp.adj.seg", fsg, 0.2f, "%.0f");
							if (showRing)
								prmCh |= AdjRow("Anneaux", "vp.adj.ring", frg, 0.2f, "%.0f");
							float32 epA[3], erA[3], esA[3];
							demo::Demo3DHostEmptyTransform(st.addAdjustNode, epA, erA, esA);
							const float32 es0[3] = {esA[0], esA[1], esA[2]};
							if (showRay) {
								float32 rayA = esA[0];
								if (AdjRow(isTor ? "Rayon externe" : "Rayon", "vp.adj.ray",
										   rayA, 0.01f, "%.2f")) {
									esA[0] = rayA;
									esA[2] = rayA;
									if (isSph || isIco)
										esA[1] = rayA; // une sphere reste une sphere
								}
							}
							if (showHaut)
								AdjRow("Hauteur", "vp.adj.h", esA[1], 0.01f, "%.2f");
							if (showAux)
								prmCh |= AdjRow("Rayon interne", "vp.adj.aux", fax, 0.005f,
												"%.2f");
							if (showLHP) {
								AdjRow("Largeur", "vp.adj.lx", esA[0], 0.01f, "%.2f");
								if (!isPln)
									AdjRow("Hauteur", "vp.adj.ly", esA[1], 0.01f, "%.2f");
								AdjRow("Profondeur", "vp.adj.lz", esA[2], 0.01f, "%.2f");
							}
							if (ukA == 5) {
								// une LUMIERE aussi a ses proprietes DES la creation
								float32 ulc2[3], uli2 = 1.f;
								if (demo::Demo3DHostUserLightParams(st.addAdjustNode, ulc2,
																   &uli2)) {
									const float32 i0 = uli2;
									AdjRow("Puissance", "vp.adj.pw", uli2, 0.05f, "%.2f");
									if (uli2 != i0)
										demo::Demo3DHostSetUserLightParams(st.addAdjustNode,
																		   ulc2, uli2);
								}
								const float32 w0[5] = {rgP, inP, outP, awP, ahP};
								if (tyP != 0)
									AdjRow("Portee", "vp.adj.rg", rgP, 0.05f, "%.2f");
								if (tyP == 2) {
									AdjRow("Cone interne", "vp.adj.ci", inP, 0.2f, "%.1f");
									AdjRow("Cone externe", "vp.adj.co", outP, 0.2f, "%.1f");
								}
								if (tyP == 3) {
									AdjRow("Largeur", "vp.adj.aw", awP, 0.01f, "%.2f");
									AdjRow("Hauteur", "vp.adj.ah", ahP, 0.01f, "%.2f");
								}
								if (rgP != w0[0] || inP != w0[1] || outP != w0[2] ||
									awP != w0[3] || ahP != w0[4])
									demo::Demo3DHostSetLightEx(st.addAdjustNode, rgP, inP,
															   outP, awP, ahP, shP);
							}
							if (hasParams && (prmCh || (int32)(fsg + 0.5f) != sgA ||
											  (int32)(frg + 0.5f) != rgA))
								demo::Demo3DHostSetMeshParams(st.addAdjustNode,
															  (int32)(fsg + 0.5f),
															  (int32)(frg + 0.5f), fax);
							if (esA[0] != es0[0] || esA[1] != es0[1] || esA[2] != es0[2])
								demo::Demo3DHostSetEmptyTransform(st.addAdjustNode, epA, erA,
																  esA);
							const NkRect ab{aj.x + S(8.f), ay + S(2.f), pw - S(16.f),
											kRowH - S(4.f)};
							hit.Add("vp.adj.apply", ab);
							p.Fill(ab, NkRole::AccentUi, 3.f);
							p.TextV(ab.x + S(8.f), ay + S(2.f), kRowH - S(4.f), "Appliquer",
									NkRole::TextOnAccent);
							if (hit.Clicked("vp.adj.apply"))
								st.addAdjustNode = -1;
						}
					}
				}
			} else if (const char *e = demo::Demo3DHostError()) {
				// UN ECHEC SE DIT. Un viewport reste noir ne distingue pas Â« la carte
				// a refuse la cible Â» de Â« la scene est vide Â», et on cherche le
				// probleme du mauvais cote pendant une heure.
				char msg[128];
				snprintf(msg, sizeof(msg), "Vue 3D indisponible : %s", e);
				p.TextV(r.x + S(16.f), r.y, r.h, msg, NkRole::TextMuted);
			}

			// â”€â”€ ZONE DE LA SCENE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// La DEMO PORTEE gere elle-meme orbite, molette, zones de selection,
			// curseur 3D et pick : cette zone ne sert plus qu'au SURVOL -- c'est lui
			// qui autorise ses raccourcis et sa souris (voir Demo3DHostSetView).
			{
				hit.Add("view.nav", vr);
			}

			// â”€â”€ GLISSEMENT DE NAVIGATION EN COURS (loupe, main, gizmo de nav) â”€â”€
			// Il se poursuit MEME SI la souris quitte le bouton : c'est le bouton
			// ENFONCE qui commande, pas la position.
			if (st.navDragMode >= 0) {
				if (!hit.MouseDown()) {
					st.navDragMode = -1;
				} else {
					const math::NkVec2 mm = hit.Mouse();
					const float32 ndx = mm.x - st.navDragLastX;
					const float32 ndy = mm.y - st.navDragLastY;
					if (st.navDragMode == 2)
						demo::Demo3DHostOrbit(ndx, ndy);
					else if (st.navDragMode == 1)
						demo::Demo3DHostPan(ndx, ndy);
					else
						// Tirer vers le HAUT rapproche : la meme convention que la
						// molette (0,02 cran par pixel).
						demo::Demo3DHostZoomWheel(-ndy * 0.02f);
					st.navDragLastX = mm.x;
					st.navDragLastY = mm.y;
					hit.WantCursor(st.navDragMode == 1 ? NkCursorWant::Hand
													   : NkCursorWant::ResizeNS);
				}
			}

			// â”€â”€ BARRE FLOTTANTE GAUCHE : ce qu'on REGARDE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Trois listes deroulantes, chacune avec son icone d'etat. Les menus de
			// commandes vivent dans la barre d'outils principale : les dupliquer ici
			// donnerait deux endroits a tenir a jour pour une seule liste.
			const float32 barH = S(26.f), barY = r.y + wsBarH + S(10.f);
			{
				int32 nP = 0, nS = 0, nO = 0;
				const char *const *proj = NkProjectionItems(nP);
				const char *const *shad = NkShadingItems(nS);
				const char *const *ovl = NkOverlayItems(nO);
				const float32 ib = S(28.f);
				// La COULEUR et le MATCAP ne concernent que les modes non eclaires
				// (Solide, Fil de fer) : c'est la que la demo les applique.
				const bool unlitZone = (st.shading == 1 || st.shading == 2);
				// LE COMPTE EXACT des boutons dimensionne le fond -- il etait a 4
				// pour 5 boutons de base, et le dernier (le matcap) flottait sans
				// fond : le defaut signale par Rihen.
				const int32 nBtn = 5 + (unlitZone ? 2 : 0);
				const float32 groupW = S(6.f) + (float32)nBtn * (ib + 2.f);
				float32 bx = r.x + S(10.f);
				p.Fill({bx, barY, groupW, barH}, NkRole::PanelBg, 5.f);
				bx += S(3.f);

				// MENU DE VUE : des ACTIONS (memoriser/rappeler la camera, reset,
				// panneaux). Pas un Combo -- un menu d'actions n'a pas de Â« valeur
				// courante Â» a afficher.
				{
					const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
					const bool over = hit.Add("vp.menu", br);
					if (over || st.viewMenuOpen)
						p.Fill(br, st.viewMenuOpen ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
					p.IconV(br.x + (ib - S(14.f)) * 0.5f, br.y, br.h, NkIcon::Menu,
							st.viewMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
						   st.viewMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked("vp.menu")) {
						st.viewMenuOpen = !st.viewMenuOpen;
						st.bgMenuOpen = st.bgPickerOpen = st.matcapOpen = false;
					}
				}
				bx += ib + 2.f;
				p.VLine(bx - 1.f, barY + S(5.f), barH - S(10.f));

				Combo(p, hit, ws, "vp.proj", {bx, barY + 2.f, ib, barH - 4.f}, proj, NkProjectionIcons(),
					  nP, st.projection, combo, true, false, false);
				bx += ib + 2.f;
				Combo(p, hit, ws, "vp.shade", {bx, barY + 2.f, ib, barH - 4.f}, shad, NkShadingIcons(), nS,
					  st.shading, combo, true, false, false);
				bx += ib + 2.f;

				// â”€â”€ FOND DE LA VUE : le bouton EST un temoin de couleur â”€â”€â”€â”€â”€â”€â”€â”€â”€
				// Cinq prereglages + Â« Personnalisee Â» qui ouvre un PICKER (trois
				// barres R/V/B) -- demande de Rihen. Le temoin montre la couleur
				// REELLE : aucune icone ne dirait mieux l'etat.
				{
					const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
					const bool over = hit.Add("vp.bg", br);
					if (over || st.bgMenuOpen)
						p.Fill(br, NkRole::PanelHeader, 3.f);
					float32 bc[3];
					NkBgColorOf(st, st.bgChoice, bc);
					p.Fill({br.x + S(5.f), br.y + S(4.f), br.w - S(10.f), br.h - S(8.f)},
						   NkColor{(uint8)(bc[0] * 255.f), (uint8)(bc[1] * 255.f),
								   (uint8)(bc[2] * 255.f), 255},
						   2.f);
					p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
						   st.bgMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked("vp.bg")) {
						st.bgMenuOpen = !st.bgMenuOpen;
						st.viewMenuOpen = st.matcapOpen = false;
						if (!st.bgMenuOpen)
							st.bgPickerOpen = false;
					}
					// Applique CHAQUE image : le moteur a une garde d'egalite, et
					// c'est ce qui rend le picker vivant pendant le glissement.
					// La LUMINOSITE (proprietes de la scene) module la couleur sans
					// en changer la teinte.
					float32 lum[3];
					for (int32 c2 = 0; c2 < 3; ++c2) {
						lum[c2] = bc[c2] * st.bgBrightness;
						if (lum[c2] > 1.f)
							lum[c2] = 1.f;
					}
					demo::Demo3DHostSetBackground(lum[0], lum[1], lum[2]);
				}
				bx += ib + 2.f;

				CheckCombo(p, hit, ws, "vp.ovl", {bx, barY + 2.f, ib, barH - 4.f}, ovl,
						   NkOverlayIcons(), nO, st.overlayMask, NkIcon::Eye, checks);
				bx += ib + 2.f;

				if (unlitZone) {
					// SOURCE DE COULEUR des modes non eclaires (la touche B de la
					// demo) : materiau, gris d'atelier, couleur choisie.
					int32 nL = 0;
					const char *const *lights = NkSolidLightItems(nL);
					Combo(p, hit, ws, "vp.solidlight", {bx, barY + 2.f, ib, barH - 4.f}, lights,
						  NkSolidLightIcons(), nL, st.solidLight, combo, true, false, false);
					bx += ib + 2.f;
					// MATCAP : bouton dedie -> panneau par CATEGORIES avec defilement
					// (34 boules ne tiennent pas dans une liste plate).
					{
						const NkRect br{bx, barY + 2.f, ib, barH - 4.f};
						const bool over = hit.Add("vp.matcap", br);
						if (over || st.matcapOpen)
							p.Fill(br, st.matcapOpen ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
						p.IconV(br.x + (ib - S(14.f)) * 0.5f, br.y, br.h, NkIcon::Matcap,
								st.matcapOpen ? NkRole::TextOnAccent : NkRole::Text, 14.f);
						p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
							   st.matcapOpen ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("vp.matcap")) {
							st.matcapOpen = !st.matcapOpen;
							st.matcapAnchor = br; // le panneau s'ancre a SON bouton
							st.viewMenuOpen = st.bgMenuOpen = st.bgPickerOpen = false;
						}
					}
				}
			}

			// â”€â”€ BARRE FLOTTANTE DROITE : ce qu'on FAIT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// TROIS GROUPES SEPARES PAR DU VIDE, et c'est ce que Rihen demandait :
			//   1. les sous-modes de selection (mode edition seulement) ;
			//   2. les outils -- Â« que fait mon clic ? Â», un seul actif ;
			//   3. les reglages : repere, vitesse de camera, aimantations.
			// Un seul bloc continu obligeait a compter les boutons pour retrouver le
			// sien. L'espace fait le tri sans qu'on ait a lire.
			const float32 btn = S(24.f);
			const float32 grp = S(10.f); // vide entre deux groupes
			// LES ANCIENS TROIS AIMANTS (grille/angle/echelle + leurs valeurs)
			// SONT RETIRES (regle de Rihen) : UN aimant-bascule, UNE cible --
			// et quand la cible est INCREMENT ou GRILLE, les PAS des trois
			// transformations deviennent des champs editables ici meme.
			// (les pas se reglent DANS le panneau d'aimantation, plus rien
			// dans la barre -- regle de Rihen)

			// Largeurs, calculees d'abord pour caler le tout a droite.
			const bool editMode2 = demo::Demo3DHostInEditMode();
			const float32 wSub = editMode2 ? (S(8.f) + 3.f * (btn + 2.f)) : 0.f;
			// OUTILS EN DEUX BLOCS dans le meme cadre : [Selection | Curseur] puis
			// un vide, puis [Deplacer | Rotation | Echelle | Multigizmo] -- la
			// disposition demandee par Rihen (celle de Blender).
			const float32 wTools = S(8.f) + 6.f * (btn + 2.f) + S(10.f);
			// Orientation, pivot, aimant + SON chevron, edition proportionnelle
			// + LE SIEN, vitesse. Sans compter les deux derniers, le groupe
			// restait trop etroit et ils tombaient HORS du cadre, a droite de
			// l'ecran : invisibles dans les deux modes (constate par Rihen).
			const float32 wSet = S(8.f) + 7.f * (btn + 2.f) + S(6.f);
			float32 tx = r.x + r.w - S(10.f) - wSet;

			// Groupe 3 : reglages (a droite).
			{
				p.Fill({tx, barY, wSet, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				int32 nOr = 0, nCam = 0;
				const char *const *orients = NkOrientItems(nOr);
				const char *const *cams = NkCamSpeedItems(nCam);
				Combo(p, hit, ws, "vp.orient", {cx, barY + 1.f, btn, barH - 2.f}, orients, NkOrientIcons(),
					  nOr, st.orientation, combo, true, false, false);
				cx += btn + 2.f;
				// ── POINT DE PIVOT (Blender : « Transform Pivot Point », capture
				// de Rihen) : les cinq modes du gizmo, dans l'ordre du menu de
				// Blender. MEMOIRE-DU-POUSSE : le combo ecrit en fin de frame,
				// et la touche « . » peut changer le pivot cote moteur -- on
				// suit le moteur quand c'est lui qui a bouge, on pousse quand
				// c'est le combo.
				{
					static const char *const kPvItems[5] = {
						"Centre boite englobante", "Curseur 3D",
						"Origines individuelles", "Point median", "Element actif"};
					static const int32 kPvVal[5] = {1, 2, 3, 0, 4}; // menu -> moteur
					static const NkIcon kPvIc[5] = {NkIcon::Cube3D, NkIcon::Cursor,
													NkIcon::Layers, NkIcon::Dot,
													NkIcon::Check};
					const int32 engPv = demo::Demo3DHostPivotMode();
					int32 engMenu = 3;
					for (int32 i = 0; i < 5; ++i)
						if (kPvVal[i] == engPv)
							engMenu = i;
					static int32 sPvSel = 3;
					static int32 sPvEngPrev = -1;
					if (engMenu != sPvEngPrev) {
						sPvSel = engMenu; // le moteur a bouge (touche .) : on suit
						sPvEngPrev = engMenu;
					}
					Combo(p, hit, ws, "vp.pivot", {cx, barY + 1.f, btn, barH - 2.f},
						  kPvItems, kPvIc, 5, sPvSel, combo, true, false, false);
					if (sPvSel != engMenu) {
						demo::Demo3DHostSetPivotMode(kPvVal[sPvSel]);
						sPvEngPrev = sPvSel;
					}
					cx += btn + 2.f;
				}
				// ── L'AIMANT : LA bascule d'aimantation (Blender). Un clic
				// l'active, un re-clic la coupe -- et l'etat se VOIT (fond
				// accent), c'est ce qui manquait (Rihen : « je ne sais pas si
				// c'est active »). Verite moteur relue chaque image ; Ctrl
				// pendant le geste INVERSE, comme Blender.
				{
					const bool snapOn2 = demo::Demo3DHostSnapEnabled();
					const NkRect mb2{cx, barY + 2.f, btn, barH - 4.f};
					const bool ovM2 = hit.Add("vp.magnet", mb2);
					if (snapOn2)
						p.Fill(mb2, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, mb2, ovM2, 3.f);
					// AIMANT EN FER A CHEVAL, pas un quadrillage : le quadrillage
					// disait « grille », or la bascule aimante aussi sur sommets,
					// aretes et faces. Le dessin doit decrire l'action (Rihen).
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::Magnet,
							snapOn2 ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.magnet"))
						demo::Demo3DHostSetSnap(!snapOn2, st.snapStepT, st.snapStepR,
												st.snapStepS);
					cx += btn + 2.f;
				}
				// ── CIBLE D'AIMANTATION : un BOUTON-PANNEAU (Blender). Le
				// panneau porte la liste des cibles ET les pas d'increment --
				// c'est la qu'ils se reglent, pas dans la barre (Rihen).
				{
					const NkRect sb2{cx, barY + 1.f, btn, barH - 2.f};
					const bool ovS2 = hit.Add("vp.snapto", sb2);
					if (st.snapMenuOpen)
						p.Fill(sb2, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, sb2, ovS2, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::ChevronDown,
							st.snapMenuOpen ? NkRole::TextOnAccent : NkRole::TextMuted,
							13.f);
					if (hit.Clicked("vp.snapto")) {
						st.snapMenuOpen = !st.snapMenuOpen;
						st.snapMenuAnchor = sb2;
					}
					cx += btn + S(6.f); // respiration avant la paire suivante
				}
				// ── EDITION PROPORTIONNELLE : sa bascule, puis SON chevron ───
				// Chaque bascule garde son chevron A COTE d'elle -- glissee entre
				// l'aimant et le sien, elle brouillait qui ouvre quoi (Rihen).
				// Elle vaut dans les DEUX modes : sommets voisins en Edition,
				// OBJETS voisins en mode Objet (disposer une foret, incurver une
				// rangee de batiments sans les toucher un a un).
				{
					bool peOn = false;
					float32 peR = 1.f;
					int32 peF = 0;
					demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
					const NkRect pb{cx, barY + 2.f, btn, barH - 4.f};
					const bool ovP = hit.Add("vp.prop", pb);
					if (peOn)
						p.Fill(pb, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, pb, ovP, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::Proportional,
							peOn ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.prop"))
						demo::Demo3DHostSetPropEdit(!peOn, peR, peF);
					cx += btn + 2.f;
					const NkRect pm{cx, barY + 1.f, btn, barH - 2.f};
					const bool ovPM = hit.Add("vp.propmenu", pm);
					if (st.propMenuOpen)
						p.Fill(pm, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, pm, ovPM, 3.f);
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, NkIcon::ChevronDown,
							st.propMenuOpen ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					if (hit.Clicked("vp.propmenu")) {
						st.propMenuOpen = !st.propMenuOpen;
						st.propMenuAnchor = pm;
					}
					cx += btn + 2.f;
				}
				// VITESSE DE CAMERA : son icone etait la camera -- le meme dessin
				// que la projection perspective ET que le bouton de vol. Un dessin
				// dedie, sinon la barre a trois boutons jumeaux.
				static const NkIcon kCamIc[4] = {NkIcon::Speed, NkIcon::Speed, NkIcon::Speed,
												 NkIcon::Speed};
				Combo(p, hit, ws, "vp.cam", {cx, barY + 1.f, btn, barH - 2.f}, cams, kCamIc, nCam,
					  st.camSpeed, combo, true, false, false);
				cx += btn + S(8.f);
				p.VLine(cx - S(4.f), barY + S(6.f), barH - S(12.f));

				// (les pas vivent dans le panneau d'aimantation ci-dessous)
			}
			// ── PANNEAU DE L'EDITION PROPORTIONNELLE : rayon + attenuation ──
			// Meme facture que celui de l'aimantation : ancre a son chevron,
			// bloquant, et ferme au clic exterieur -- l'emprise du bouton
			// exclue, sinon le clic d'ouverture le refermerait dans la meme
			// image (le piege deja paye deux fois).
			if (st.propMenuOpen) {
				bool peOn = false;
				float32 peR = 1.f;
				int32 peF = 0;
				demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
				const float32 r0p = peR;
				const int32 f0p = peF;
				static const char *const kFall[8] = {"Lisse",		 "Sphere",	 "Racine",
													 "Carre inverse", "Net",	 "Lineaire",
													 "Constant",	  "Aleatoire"};
				const float32 rowH3 = S(22.f);
				const float32 pw3 = S(206.f);
				const float32 ph3 = S(10.f) + rowH3 * 10.f;
				float32 px3 = st.propMenuAnchor.x + st.propMenuAnchor.w - pw3;
				if (px3 < r.x + S(4.f))
					px3 = r.x + S(4.f);
				const NkRect pr3{px3, st.propMenuAnchor.y + st.propMenuAnchor.h + S(4.f), pw3,
								 ph3};
				hit.Add("vp.propmenu.box", pr3);
				p.Fill(pr3, NkRole::PanelBg, 4.f);
				p.OutlineSharp(pr3, NkRole::Border);
				float32 yy3 = pr3.y + S(4.f);
				p.TextV(px3 + S(10.f), yy3, rowH3, "Rayon", NkRole::TextMuted);
				DragFloat(p, hit, ws, in, "vp.prop.rad",
						  {px3 + S(84.f), yy3 + S(3.f), pw3 - S(94.f), rowH3 - S(6.f)}, peR,
						  0.02f, NkRole::AccentUi, "%.2f m");
				yy3 += rowH3;
				p.TextV(px3 + S(10.f), yy3, rowH3, "Attenuation", NkRole::TextMuted);
				yy3 += rowH3;
				char fk3[24];
				for (int32 i3 = 0; i3 < 8; ++i3) {
					snprintf(fk3, sizeof(fk3), "vp.prop.f%d", i3);
					const NkRect ir3{px3 + S(4.f), yy3, pw3 - S(8.f), rowH3};
					const bool ov3 = hit.Add(fk3, ir3);
					const bool on3 = (peF == i3);
					if (on3)
						p.Fill(ir3, NkRole::AccentUi, 3.f);
					else if (ov3)
						p.Fill(ir3, NkRole::PanelHeader, 3.f);
					p.TextV(ir3.x + S(10.f), yy3, rowH3, kFall[i3],
							on3 ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(fk3))
						peF = i3;
					yy3 += rowH3;
				}
				if (peR != r0p || peF != f0p)
					demo::Demo3DHostSetPropEdit(peOn, peR, peF);
				hit.SetBlock(pr3, true);
				if (hit.AnyClick() && !NkHitRegistry::Contains(pr3, in.mousePos) &&
					!NkHitRegistry::Contains(st.propMenuAnchor, in.mousePos))
					st.propMenuOpen = false;
			}
			// ── PANNEAU D'AIMANTATION : cibles + pas (Blender, capture de
			// Rihen). Il s'ancre a son bouton, BLOQUE les evenements sous lui
			// (SetBlock, meme patron que le badge vue camera) et se referme au
			// clic exterieur.
			if (st.snapMenuOpen) {
				static const char *const kSnItems[9] = {
					"Increment",	  "Grille",
					"Sommet",		  "Arete",
					"Face",			  "Volume",
					"Centre d'arete", "Arete perpendiculaire",
					"Centre de face"};
				const int32 curTgt = demo::Demo3DHostSnapTarget();
				const bool showSteps = curTgt <= 1;
				// La BASE d'accroche n'a de sens que pour les cibles
				// geometriques : sur Increment et Grille, c'est le pas qui
				// decide, pas un point de l'objet.
				const bool showBase = curTgt >= 2;
				const float32 rowH2 = S(22.f);
				const float32 pw = S(226.f);
				const float32 ph = S(8.f) + rowH2 * (1.f + 9.f) +
								   (showSteps ? S(6.f) + rowH2 * 3.f : 0.f) +
								   (showBase ? S(6.f) + rowH2 * 5.f : 0.f) + S(6.f);
				float32 px = st.snapMenuAnchor.x + st.snapMenuAnchor.w - pw;
				if (px < r.x + S(4.f))
					px = r.x + S(4.f);
				const float32 py = st.snapMenuAnchor.y + st.snapMenuAnchor.h + S(4.f);
				const NkRect pr{px, py, pw, ph};
				// PAS de SetBlock ICI : arme avant nos propres lignes, il
				// bloquait leurs clics (Clicked() refuse tout clic dans
				// l'emprise) -- « je n'arrive pas a selectionner », Rihen. Le
				// bloc s'arme EN FIN de panneau, pour la scene en dessous ; le
				// reset de debut de frame l'a deja leve pour nous.
				hit.Add("vp.snapmenu", pr);
				p.Fill(pr, NkRole::PanelBg, 4.f);
				p.OutlineSharp(pr, NkRole::Border);
				float32 yy2 = py + S(4.f);
				p.TextV(px + S(10.f), yy2, rowH2, "Aimanter sur", NkRole::TextMuted);
				yy2 += rowH2;
				char sk2[24];
				for (int32 i9 = 0; i9 < 9; ++i9) {
					snprintf(sk2, sizeof(sk2), "vp.snapto.%d", i9);
					const NkRect ir9{px + S(4.f), yy2, pw - S(8.f), rowH2};
					const bool ov9 = hit.Add(sk2, ir9);
					const bool on9 = curTgt == i9;
					// Volume et Arete perpendiculaire sont livres : plus de stub.
					const bool stub9 = false;
					if (on9)
						p.Fill(ir9, NkRole::AccentUi, 3.f);
					else if (ov9 && !stub9)
						p.Fill(ir9, NkRole::PanelHeader, 3.f);
					p.TextV(ir9.x + S(8.f), yy2, rowH2, kSnItems[i9],
							stub9 ? NkRole::TextMuted
								  : (on9 ? NkRole::TextOnAccent : NkRole::Text));
					if (hit.Clicked(sk2) && !stub9)
						demo::Demo3DHostSetSnapTarget(i9);
					yy2 += rowH2;
				}
				if (showBase) {
					p.HLine(px + S(6.f), yy2 + S(2.f), pw - S(12.f));
					yy2 += S(6.f);
					p.TextV(px + S(10.f), yy2, rowH2, "Base d'accroche", NkRole::TextMuted);
					yy2 += rowH2;
					// « Centre » et « Median » de Blender fusionnent en
					// « Pivot » : le pivot du gizmo suit deja le mode de pivot
					// de l'application -- deux entrees pour un meme point
					// seraient un faux choix.
					static const char *const kSb[3] = {"Le plus proche", "Pivot",
													   "Objet actif"};
					const int32 curBase = demo::Demo3DHostSnapBase();
					for (int32 b9 = 0; b9 < 3; ++b9) {
						snprintf(sk2, sizeof(sk2), "vp.snapbase.%d", b9);
						const NkRect ir9{px + S(4.f), yy2, pw - S(8.f), rowH2};
						const bool ov9 = hit.Add(sk2, ir9);
						const bool on9 = curBase == b9;
						if (on9)
							p.Fill(ir9, NkRole::AccentUi, 3.f);
						else if (ov9)
							p.Fill(ir9, NkRole::PanelHeader, 3.f);
						p.TextV(ir9.x + S(8.f), yy2, rowH2, kSb[b9],
								on9 ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(sk2))
							demo::Demo3DHostSetSnapBase(b9);
						yy2 += rowH2;
					}
					// ALIGNER LA ROTATION SUR LA CIBLE : n'agit que quand une
					// FACE (ou un centre de face) accroche -- les autres cibles
					// n'ont pas de normale stable.
					{
						const bool al = demo::Demo3DHostSnapAlignRot();
						const NkRect cb{px + S(8.f), yy2 + S(4.f), rowH2 - S(8.f),
										rowH2 - S(8.f)};
						const bool ova = hit.Add("vp.snapalign", cb);
						if (al)
							p.Fill(cb, NkRole::AccentUi, 3.f);
						else
							p.Outline(cb, NkRole::Border,
									  ova ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
						if (al)
							p.IconV(cb.x + S(1.f), yy2, rowH2, NkIcon::Check,
									NkRole::TextOnAccent, 11.f);
						p.TextV(cb.x + cb.w + S(8.f), yy2, rowH2, "Aligner la rotation",
								NkRole::Text);
						if (hit.Clicked("vp.snapalign"))
							demo::Demo3DHostSetSnapAlignRot(!al);
						yy2 += rowH2;
					}
				}
				if (showSteps) {
					p.HLine(px + S(6.f), yy2 + S(2.f), pw - S(12.f));
					yy2 += S(6.f);
					static const char *const kSt2[3] = {"Pas deplacement", "Pas angle",
														"Pas echelle"};
					float32 *const sv2[3] = {&st.snapStepT, &st.snapStepR, &st.snapStepS};
					static const char *const kSf2[3] = {"%.2f", "%.0f", "%.2f"};
					bool ch2 = false;
					for (int32 i9 = 0; i9 < 3; ++i9) {
						p.TextV(px + S(10.f), yy2, rowH2, kSt2[i9], NkRole::TextMuted);
						snprintf(sk2, sizeof(sk2), "vp.snapstep.%d", i9);
						ch2 |= DragFloat(p, hit, ws, in, sk2,
										 {px + S(120.f), yy2 + S(3.f), pw - S(130.f),
										  rowH2 - S(6.f)},
										 *sv2[i9], i9 == 1 ? 0.5f : 0.01f,
										 NkRole::AccentUi, kSf2[i9]);
						yy2 += rowH2;
					}
					if (ch2) {
						if (st.snapStepT < 0.01f)
							st.snapStepT = 0.01f;
						if (st.snapStepR < 1.f)
							st.snapStepR = 1.f;
						if (st.snapStepS < 0.01f)
							st.snapStepS = 0.01f;
						demo::Demo3DHostSetSnap(demo::Demo3DHostSnapEnabled(),
												st.snapStepT, st.snapStepR,
												st.snapStepS);
					}
				}
				// Le bloc s'arme APRES nos widgets : il ne vaut que pour la
				// scene et les zones peintes avant nous.
				hit.SetBlock(pr, true);
				// Un clic HORS de l'EMPRISE du panneau et de son bouton
				// referme -- l'emprise geometrique, pas le survol de zone : au
				// clic sur une ligne, la ligne est la zone du dessus et le
				// panneau n'etait « pas survole ».
				if (hit.AnyClick() && !NkHitRegistry::Contains(pr, in.mousePos) &&
					!hit.IsHovered("vp.snapto"))
					st.snapMenuOpen = false;
			}

			// Groupe 2 : outils -- [Selection | Curseur]  [Deplacer | Rotation |
			// Echelle | Multigizmo].
			tx -= grp + wTools;
			{
				p.Fill({tx, barY, wTools, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);

				// SELECTION : une liste de formes (rectangle / cercle / lasso).
				{
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool on = (st.tool == NkTool::Select);
					int32 nS2 = 0;
					const char *const *shapes = NkSelShapeItems(nS2);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					Combo(p, hit, ws, "vp.selshape", br, shapes, NkSelShapeIcons(), nS2,
						  st.selShape, combo, true, false, false);
					if (hit.Clicked("vp.selshape"))
						st.tool = NkTool::Select; // choisir une forme active l'outil
					cx += btn + 2.f;
				}
				// CURSEUR : le clic gauche pose le curseur 3D de la demo.
				{
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add("vp.t.cursor", br);
					const bool on = (st.tool == NkTool::Cursor);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked("vp.t.cursor"))
						st.tool = NkTool::Cursor;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, NkIcon::Cursor,
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
				// L'ESPACE entre les deux blocs : selection et transformation sont
				// deux familles de gestes, le vide fait le tri sans qu'on lise.
				cx += S(10.f);
				p.VLine(cx - S(5.f), barY + S(6.f), barH - S(12.f));

				struct TB {
						NkIcon ic;
						NkTool tool;
						const char *key;
				};
				static const TB kXf[4] = {
					{NkIcon::Move, NkTool::Move, "vp.t.move"},
					{NkIcon::Rotate, NkTool::Rotate, "vp.t.rot"},
					{NkIcon::Scale, NkTool::Scale, "vp.t.scale"},
					// MULTIGIZMO = le mode COMBINE de la demo : T+R+S en un seul
					// gizmo (sa touche C). Il manquait a la barre.
					{NkIcon::Gizmo, NkTool::MultiGizmo, "vp.t.multi"},
				};
				for (int32 i = 0; i < 4; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kXf[i].key, br);
					const bool on = (st.tool == kXf[i].tool);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kXf[i].key))
						st.tool = kXf[i].tool;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kXf[i].ic,
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// Groupe 1 : sous-modes V/E/F, en mode edition seulement -- cables sur
			// le masque de selection REEL de la demo (ses touches 1/2/3).
			if (editMode2) {
				tx -= grp + wSub;
				p.Fill({tx, barY, wSub, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				const NkIcon kSub[3] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
				static const char *const kKeys[3] = {"vp.sub.0", "vp.sub.1", "vp.sub.2"};
				const int32 mask = demo::Demo3DHostEditSelMask();
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kKeys[i], br);
					const bool on = (mask & (1 << i)) != 0;
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i])) {
						// Maj+clic COMBINE les modes (comme Maj+1/2/3 dans la demo),
						// le clic simple remplace.
						const int32 nm = hit.ShiftDown() ? (mask ^ (1 << i)) : (1 << i);
						demo::Demo3DHostSetEditSelMask(nm);
						st.subMode = (NkSubMode)i;
					}
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kSub[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// â”€â”€ GIZMO DE NAVIGATION + BOUTONS DE VUE, en bas a gauche.
			// Chez Blender ils sont en haut a droite ; ici la place y est prise par
			// les outils, et Rihen a demande de garder le coin bas-gauche.
			// Les boutons de navigation sont AU-DESSUS du gizmo, comme chez Blender :
			// on descend du plus abstrait (l'orientation) vers le plus direct (zoom,
			// deplacement). Les mettre dessous les collait au bord de la fenetre.
			const float32 gz = 34.f;
			// La colonne compte SES rangs : capture + 4 boutons de navigation,
			// et le CADENAS d'orbite s'ajoute en vue camera -- avec la hauteur
			// figee a 5, la rangee en plus chevauchait la boule (Rihen).
			const float32 navRows = demo::Demo3DHostCameraView() >= 0 ? 6.f : 5.f;
			const float32 navH = navRows * 26.f + (navRows - 1.f) * 6.f;
			const float32 navY = r.y + r.h - 22.f - gz * 2.f - 14.f - navH;
			PaintViewButtons(p, hit, st, r.x + 14.f, navY);
			PaintNavGizmo(p, hit, st, r.x + 12.f + gz, r.y + r.h - 22.f - gz, gz);

			// â”€â”€ PANNEAU DE DERNIERE OPERATION. Il FLOTTE au-dessus de la scene et n'est
			// pas encastre dans un bord : il appartient a la vue, pas au cadre.
			if (editMode) {
				const float32 pw = 214.f, ph = 4.f * kRowH + 6.f;
				const float32 px = r.x + 12.f, py = r.y + r.h - ph - 80.f;
				p.Fill({px, py, pw, ph}, NkRole::PanelHeader, 4.f);
				p.IconV(px + 6.f, py, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				p.TextV(px + 22.f, py, kRowH, "Extruder la region");
				float32 ry = py + kRowH;
				static const char *const kL[] = {"Distance", "Decalage"};
				static const char *const kV[] = {"0,25", "0,00"};
				for (int32 i = 0; i < 2; ++i) {
					p.TextV(px + kPad, ry, kRowH, kL[i], NkRole::TextMuted);
					p.Fill({px + 112.f, ry + 3.f, 92.f, 16.f}, NkRole::InputBg, 2.f);
					p.TextV(px + 118.f, ry, kRowH, kV[i]);
					ry += kRowH;
				}
				p.Fill({px + kPad, ry + 5.f, 12.f, 12.f}, NkRole::AccentUi, 2.f);
				p.TextV(px + kPad + 18.f, ry, kRowH, "Decalage pair", NkRole::TextMuted);
			}

			// Le raccourci de l'operation courante est LU dans la table via sa CLE DE
			// COMMANDE, jamais recopie : rebinder la touche changera cet affichage tout
			// seul. La cle est stable, l'index ne l'est pas.
			{
				const char *cmd = editMode ? "edit.extruder" : "objet.deplacer";
				char keys[32];
				if (sc.FormatFor(cmd, keys, sizeof(keys))) {
					char hint[96];
					snprintf(hint, sizeof(hint), "%s   %s", editMode ? "Extruder" : "Deplacer", keys);
					p.TextV(r.x + 12.f, r.y + r.h - 24.f, 20.f, hint, NkRole::TextMuted);
				}
			}

			// â”€â”€ POPUPS DE LA VUE : peints en DERNIER, par-dessus les barres â”€â”€â”€â”€
			PaintViewMenuPopup(p, hit, st, r, barY, barH);
			PaintBgPopup(p, hit, st, r, barY, barH);
			// Le panneau des matcaps est peint depuis main, APRES tous les
			// panneaux : il peut s'ouvrir depuis le panneau Proprietes aussi.
		}

		// â”€â”€ LIGNE DE TRANSFORMATION â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// TROIS CADRES DE LARGEUR IDENTIQUE, puis DEUX COLONNES CARREES pour les
		// icones. La largeur egale n'est pas cosmetique : trois champs de tailles
		// differentes se lisent comme trois choses differentes, alors que X, Y et Z
		// sont la meme grandeur sur trois axes.
		//
		// Les deux colonnes d'icones sont CARREES et RESERVEES meme quand la ligne
		// n'a qu'une icone : sans reservation, les champs de Â« Rotation Â» seraient
		// plus larges que ceux de Â« Position Â», et les trois lignes ne s'aligneraient
		// plus verticalement.
		//
		// LES VALEURS SE MODIFIENT EN GLISSANT, comme dans Blender et Unreal. C'est le
		// geste le plus utilise d'un modeleur : bien plus souvent qu'on ne tape un
		// nombre, on veut Â« un peu plus, un peu moins Â» en regardant le resultat.
		inline void PaintTransformRow(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									  const nkgui::NkGuiInput &in, const NkRect &r, float32 y,
									  const char *label, float32 *v, float32 step, const char *keyBase,
									  NkIcon icon1, NkIcon icon2, const char *fmt = "%.2f",
									  NkIcon icon3 = NkIcon::None, bool icon3On = false,
									  float32 labelW = kLabelW) {
			const float32 rowH = kRowH + S(6.f);
			p.Fill({r.x, y, labelW, rowH}, NkRole::LabelCol);
			p.Clip({r.x, y, labelW, rowH});
			p.TextV(r.x + kPad, y, rowH, label);
			p.Unclip();

			const float32 sq = rowH - S(6.f); // colonne carree : cote = hauteur du champ
			const float32 gap = S(4.f);
			// TROIS cases : cadenas, reinitialiser, PROPORTIONNEL. La troisieme
			// reste vide quand la ligne n'en veut pas -- l'alignement prime.
			const float32 iconsW = sq * 3.f + gap * 2.f;
			const float32 avail = r.w - labelW - S(10.f) - iconsW - gap;
			float32 fw = (avail - gap * 2.f) / 3.f;
			if (fw < S(40.f))
				fw = S(40.f);

			const NkRole axes[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			float32 x = r.x + labelW + S(5.f);
			char key[48];
			// Les champs gardent leur LARGEUR MINIMALE, mais le bloc est CLIPPE
			// avant la colonne des icones : panneau etroit, le champ Z passait
			// SOUS le cadenas (constate par Rihen). Tronque vaut mieux que
			// superpose -- et la colonne d'icones reste toujours cliquable.
			const float32 fieldsEnd = r.x + r.w - S(5.f) - iconsW - gap;
			p.Clip({x, y, fieldsEnd - x, rowH});
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.%d", keyBase, i);
				DragFloat(p, hit, ws, in, key, {x, y + S(3.f), fw, rowH - S(6.f)}, v[i], step, axes[i], fmt);
				x += fw + gap;
			}
			p.Unclip();

			// Les deux carres. Une icone absente laisse sa case VIDE plutot que de
			// decaler la suivante -- l'alignement des trois lignes prime.
			float32 ix = r.x + r.w - S(5.f) - iconsW;
			const NkIcon icons[3] = {icon1, icon2, icon3};
			for (int32 i = 0; i < 3; ++i) {
				if (icons[i] == NkIcon::None) {
					ix += sq + gap;
					continue;
				}
				const NkRect br{ix, y + S(3.f), sq, sq};
				snprintf(key, sizeof(key), "%s.ic%d", keyBase, i);
				const bool over = hit.Add(key, br);
				const bool accent = (i == 2 && icon3On);
				if (accent)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
				p.IconV(br.x + (sq - S(13.f)) * 0.5f, br.y, sq, icons[i],
						accent ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				// Reinitialiser remet la valeur NEUTRE de la grandeur : zero pour une
				// position ou une rotation, UN pour une echelle -- remettre une echelle
				// a zero ferait disparaitre l'objet.
				if (hit.Clicked(key) && icons[i] == NkIcon::Refresh) {
					const float32 neutral = (step > 0.05f) ? 0.f : 1.f;
					v[0] = v[1] = v[2] = neutral;
				}
				ix += sq + gap;
			}
			p.HLine(r.x, y + rowH - 1.f, r.w);
		}

		inline float32 Vec3RowH() {
			return kRowH + S(6.f);
		}

		// ── LIGNE DE COULEUR : LA NUANCE OUVRE LE VRAI PICKER ───────────────────
		// Une couleur se choisit A L'OEIL, pas en tapant trois nombres. La ligne
		// montre donc une NUANCE cliquable ; le clic deplie le carre
		// saturation/valeur et la barre de teinte, avec les trois champs R/V/B
		// dessous pour la valeur exacte. Une seule nuance reste ouverte a la fois
		// (st.colorOpen), comme les pastilles du panneau.
		// Renvoie la hauteur consommee ; met *changed a vrai si la couleur bouge.
		inline float32 PaintColorRow(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st,
									 const NkRect &r, float32 y, const char *label,
									 const char *keyBase, float32 *rgb, bool *changed) {
			const auto sat01 = [](float32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
			const float32 y0 = y;
			const float32 swH = kRowH - S(6.f);
			const float32 labW = S(110.f);
			p.TextV(r.x, y, kRowH, label, NkRole::TextMuted);
			const NkRect sw{r.x + labW, y + S(3.f), r.w - labW, swH};
			char key[48];
			snprintf(key, sizeof(key), "%s.sw", keyBase);
			const bool over = hit.Add(key, sw);
			const NkColor cur{(uint8)(sat01(rgb[0]) * 255.f), (uint8)(sat01(rgb[1]) * 255.f),
							  (uint8)(sat01(rgb[2]) * 255.f), 255};
			p.Fill(sw, cur, 3.f);
			p.OutlineSharp(sw, over ? NkRole::AccentUi : NkRole::Border);
			// LE PICKER S'OUVRE EN FENETRE MODALE (Rihen), peinte par-dessus tout
			// en fin de frame. Tant qu'elle est ouverte pour CE champ, la couleur
			// qu'elle porte est recopiee ici : l'apercu est immediat dans la scene.
			const bool open = strcmp(st.colorOpen, keyBase) == 0;
			if (hit.Clicked(key)) {
				if (open) {
					st.colorOpen[0] = 0;
				} else {
					snprintf(st.colorOpen, sizeof(st.colorOpen), "%s", keyBase);
					for (int32 c = 0; c < 3; ++c)
						st.colorOrig[c] = st.colorCur[c] = rgb[c];
					st.colorAlphaOrig = st.colorAlpha;
					st.colorHsvValid = false;
					st.colorModalPlaced = false;
				}
			} else if (open) {
				for (int32 c = 0; c < 3; ++c)
					if (rgb[c] != st.colorCur[c]) {
						rgb[c] = st.colorCur[c];
						*changed = true;
					}
			}
			if (open)
				p.OutlineSharp(sw, NkRole::AccentUi); // ce champ est celui qu'on regle
			return y + kRowH - y0;
		}

		// ── LA FENETRE MODALE DU PICKER ────────────────────────────────────────
		// Peinte en TOUT DERNIER, comme les menus : une surface modale qui doit
		// repondre a ses propres clics et voiler le reste. Deplacable par sa barre
		// de titre, comme les dialogues de NKEditorKit.
		inline void PaintColorPicker(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st, float32 W,
									 float32 H) {
			if (!st.colorOpen[0])
				return;
			// FERMETURE DIFFEREE D'UNE IMAGE : le champ lit `colorCur` AVANT que
			// cette fenetre ne soit peinte. Vider la cle des le clic sur Annuler
			// laisserait donc l'objet avec la couleur de l'apercu -- l'annulation
			// n'annulerait rien. On rend d'abord la couleur d'origine, on ferme
			// a l'image suivante, quand le champ l'a reprise.
			if (st.colorClosing) {
				st.colorClosing = false;
				st.colorOpen[0] = 0;
				return;
			}
			const auto sat01 = [](float32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
			const float32 dw = S(260.f), titleH = S(26.f);
			// roue + deux rangees d'onglets + quatre canaux + hexa + boutons
			const float32 dh = titleH + (dw - S(56.f)) + S(62.f) + kRowH * 5.f + S(52.f);
			if (!st.colorModalPlaced) {
				st.colorModalX = (W - dw) * 0.5f;
				st.colorModalY = (H - dh) * 0.35f;
				st.colorModalPlaced = true;
			}
			// Le VOILE dit que le reste est suspendu -- et absorbe les clics.
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 110});
			hit.Add("colmod.veil", {0.f, 0.f, W, H});
			NkRect box{st.colorModalX, st.colorModalY, dw, dh};
			// Glissement par la barre de titre, traite AVANT de figer la boite :
			// sinon l'affichage traine d'une image derriere la souris.
			const NkRect tb{box.x, box.y, box.w, titleH};
			if (hit.MouseDown() && !st.colorModalDrag && NkHitRegistry::Contains(tb, hit.Mouse())) {
				st.colorModalDrag = true;
				st.colorDragDX = hit.Mouse().x - box.x;
				st.colorDragDY = hit.Mouse().y - box.y;
			}
			if (st.colorModalDrag) {
				if (!hit.MouseDown()) {
					st.colorModalDrag = false;
				} else {
					st.colorModalX = hit.Mouse().x - st.colorDragDX;
					st.colorModalY = hit.Mouse().y - st.colorDragDY;
					if (st.colorModalX < 0.f)
						st.colorModalX = 0.f;
					if (st.colorModalY < 0.f)
						st.colorModalY = 0.f;
					if (st.colorModalX + dw > W)
						st.colorModalX = W - dw;
					if (st.colorModalY + dh > H)
						st.colorModalY = H - dh;
					box.x = st.colorModalX;
					box.y = st.colorModalY;
				}
			}
			p.Fill({box.x + 3.f, box.y + 4.f, box.w, box.h}, NkColor{0, 0, 0, 110}, 4.f);
			p.Outline(box, NkRole::AccentUi, NkRole::PanelBg, 4.f);
			p.Fill({box.x, box.y, box.w, titleH}, NkRole::PanelHeader, 4.f);
			p.TextV(box.x + S(10.f), box.y, titleH, "Couleur");
			hit.Add("colmod.title", tb);
			float32 y = box.y + titleH + S(8.f);
			// ── ROUE CHROMATIQUE + BARRE DE VALEUR ──────────────────────────
			// La teinte tourne, la saturation s'eloigne du centre, la valeur se
			// regle a part : la disposition retenue par Rihen.
			const float32 wheelD = box.w - S(56.f);
			const float32 rad = wheelD * 0.5f;
			const float32 cxw = box.x + S(12.f) + rad, cyw = y + rad;
			// La TEINTE est memorisee : au noir comme au blanc, la reconvertir
			// depuis le RVB la perdrait et le curseur sauterait.
			if (!st.colorHsvValid) {
				NkRgbToHsv(st.colorCur, st.colorHsv);
				st.colorHsvValid = true;
			} else {
				float32 back[3];
				NkHsvToRgb(st.colorHsv, back);
				if (fabsf(back[0] - st.colorCur[0]) > 0.004f ||
					fabsf(back[1] - st.colorCur[1]) > 0.004f ||
					fabsf(back[2] - st.colorCur[2]) > 0.004f)
					NkRgbToHsv(st.colorCur, st.colorHsv); // change de l'exterieur
			}
			NkColorWheel(p, hit, st.propDragKey, sizeof(st.propDragKey), "colmod.wheel", cxw, cyw,
						 rad, st.colorHsv, st.colorCur);
			{
				// Barre de VALEUR, de la teinte pleine au noir.
				const NkRect vb{box.x + box.w - S(32.f), y, S(18.f), wheelD};
				hit.Add("colmod.val", vb);
				float32 pure[3];
				const float32 pureHsv[3] = {st.colorHsv[0], st.colorHsv[1], 1.f};
				NkHsvToRgb(pureHsv, pure);
				const NkColor cTop{(uint8)(pure[0] * 255.f), (uint8)(pure[1] * 255.f),
								   (uint8)(pure[2] * 255.f), 255};
				const NkColor cBot{0, 0, 0, 255};
				p.RectMultiColor(vb, cTop, cTop, cBot, cBot);
				p.OutlineSharp(vb, NkRole::Border);
				if (hit.MouseDown() && (strcmp(st.propDragKey, "colmod.val") == 0 ||
										(!st.propDragKey[0] && hit.IsHovered("colmod.val")))) {
					snprintf(st.propDragKey, sizeof(st.propDragKey), "colmod.val");
					st.colorHsv[2] = sat01(1.f - (hit.Mouse().y - vb.y) / vb.h);
					NkHsvToRgb(st.colorHsv, st.colorCur);
				}
				const float32 my = vb.y + (1.f - st.colorHsv[2]) * vb.h;
				p.Fill({vb.x - 2.f, my - 1.5f, vb.w + 4.f, 3.f}, NkRole::Text, 1.f);
			}
			y += wheelD + S(10.f);
			// ── ESPACE : LINEAIRE / PERCEPTUEL ──────────────────────────────
			// Le perceptuel corrige le gamma : il ne change QUE l'affichage des
			// nombres, jamais la couleur envoyee au rendu.
			{
				static const char *const kSp[2] = {"Lineaire", "Perceptuel"};
				const float32 hw = (box.w - S(20.f)) * 0.5f;
				char sk[24];
				for (int32 s2 = 0; s2 < 2; ++s2) {
					const NkRect br{box.x + S(10.f) + (float32)s2 * hw, y, hw, S(22.f)};
					snprintf(sk, sizeof(sk), "colmod.sp%d", s2);
					const bool on = (st.colorSpace == s2);
					hit.Add(sk, br);
					p.Fill(br, on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
					p.TextV(br.x + (hw - p.TextW(kSp[s2])) * 0.5f, y, S(22.f), kSp[s2],
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(sk))
						st.colorSpace = s2;
				}
				y += S(26.f);
			}
			// ── RVB / TSV ───────────────────────────────────────────────────
			{
				static const char *const kTb[2] = {"RVB", "TSV"};
				const float32 hw = (box.w - S(20.f)) * 0.5f;
				char tk[24];
				for (int32 t2 = 0; t2 < 2; ++t2) {
					const NkRect br{box.x + S(10.f) + (float32)t2 * hw, y, hw, S(22.f)};
					snprintf(tk, sizeof(tk), "colmod.tb%d", t2);
					const bool on = (st.colorTab == t2);
					hit.Add(tk, br);
					p.Fill(br, on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
					p.TextV(br.x + (hw - p.TextW(kTb[t2])) * 0.5f, y, S(22.f), kTb[t2],
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(tk))
						st.colorTab = t2;
				}
				y += S(26.f);
			}
			// ── LES CANAUX, PUIS L'ALPHA, PUIS L'HEXADECIMAL ────────────────
			{
				static const char *const kRgb[3] = {"Rouge", "Vert", "Bleu"};
				static const char *const kHsvN[3] = {"Teinte", "Saturation", "Valeur"};
				const float32 lblW = S(78.f);
				char k[32];
				for (int32 c = 0; c < 3; ++c) {
					p.TextV(box.x + S(10.f), y, kRowH, st.colorTab == 0 ? kRgb[c] : kHsvN[c],
							NkRole::TextMuted);
					snprintf(k, sizeof(k), "colmod.c%d", c);
					const NkRect fr{box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
									kRowH - S(4.f)};
					if (st.colorTab == 0) {
						const float32 g = st.colorSpace == 1 ? 1.f / 2.2f : 1.f;
						float32 v = powf(sat01(st.colorCur[c]), g);
						if (DragFloat(p, hit, ws, in, k, fr, v, 0.005f, NkRole::AccentUi, "%.3f")) {
							st.colorCur[c] = sat01(powf(sat01(v), 1.f / g));
							st.colorHsvValid = false;
						}
					} else {
						float32 v = c == 0 ? st.colorHsv[0] / 360.f : st.colorHsv[c];
						if (DragFloat(p, hit, ws, in, k, fr, v, 0.005f, NkRole::AccentUi, "%.3f")) {
							st.colorHsv[c] = c == 0 ? sat01(v) * 359.9f : sat01(v);
							NkHsvToRgb(st.colorHsv, st.colorCur);
						}
					}
					y += kRowH;
				}
				p.TextV(box.x + S(10.f), y, kRowH, "Alpha", NkRole::TextMuted);
				float32 al = st.colorAlpha;
				if (DragFloat(p, hit, ws, in, "colmod.a",
							  {box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
							   kRowH - S(4.f)},
							  al, 0.005f, NkRole::AccentUi, "%.3f"))
					st.colorAlpha = sat01(al);
				y += kRowH + S(4.f);
				// L'HEXADECIMAL s'echange en sRGB : c'est la notation que tout le
				// monde copie-colle, elle doit donner la meme couleur qu'ailleurs.
				p.TextV(box.x + S(10.f), y, kRowH, "Hex", NkRole::TextMuted);
				const float32 gH = 1.f / 2.2f;
				if (!ws.IsEditing("colmod.hex"))
					snprintf(st.colorHex, sizeof(st.colorHex), "#%02X%02X%02X%02X",
							 (uint32)(powf(sat01(st.colorCur[0]), gH) * 255.f + 0.5f),
							 (uint32)(powf(sat01(st.colorCur[1]), gH) * 255.f + 0.5f),
							 (uint32)(powf(sat01(st.colorCur[2]), gH) * 255.f + 0.5f),
							 (uint32)(sat01(st.colorAlpha) * 255.f + 0.5f));
				if (EditableText(p, hit, ws, in, "colmod.hex",
								 {box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
								  kRowH - S(4.f)},
								 st.colorHex, NkRole::Text, st.colorHex, sizeof(st.colorHex))) {
					uint32 r2 = 0, g2 = 0, b2 = 0, a2 = 255;
					const char *s3 = st.colorHex;
					while (*s3 == '#' || *s3 == ' ')
						++s3;
					const int32 n3 = (int32)strlen(s3);
					if ((n3 == 6 || n3 == 8) && sscanf(s3, "%2x%2x%2x", &r2, &g2, &b2) == 3) {
						if (n3 == 8)
							sscanf(s3 + 6, "%2x", &a2);
						st.colorCur[0] = powf((float32)r2 / 255.f, 2.2f);
						st.colorCur[1] = powf((float32)g2 / 255.f, 2.2f);
						st.colorCur[2] = powf((float32)b2 / 255.f, 2.2f);
						st.colorAlpha = (float32)a2 / 255.f;
						st.colorHsvValid = false;
					}
				}
				y += kRowH + S(6.f);
			}
			const float32 bw = (box.w - S(30.f)) * 0.5f;
			const NkRect bOk{box.x + S(10.f), y, bw, S(24.f)};
			const NkRect bCa{box.x + S(20.f) + bw, y, bw, S(24.f)};
			hit.Add("colmod.ok", bOk);
			p.Fill(bOk, NkRole::AccentUi, 3.f);
			p.TextV(bOk.x + (bOk.w - p.TextW("Valider")) * 0.5f, y, S(24.f), "Valider",
					NkRole::TextOnAccent);
			hit.Add("colmod.cancel", bCa);
			p.Outline(bCa, NkRole::Border, NkRole::InputBg, 3.f);
			p.TextV(bCa.x + (bCa.w - p.TextW("Annuler")) * 0.5f, y, S(24.f), "Annuler");
			const bool esc = in.KeyPressed(nkgui::NkGuiKey::Escape);
			if (hit.Clicked("colmod.cancel") || esc) {
				// ANNULER REND LA COULEUR DE DEPART, pour de bon : l'apercu en
				// direct l'avait deja appliquee a l'objet.
				for (int32 c = 0; c < 3; ++c)
					st.colorCur[c] = st.colorOrig[c];
				st.colorAlpha = st.colorAlphaOrig;
				st.colorHsvValid = false;
				st.colorClosing = true;
			} else if (hit.Clicked("colmod.ok") || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				st.colorOpen[0] = 0;
			}
			// LA MODALE DECLARE SON EMPRISE, VOILE COMPRIS : plus rien de la
			// couche inferieure n'est atteignable tant qu'elle est ouverte, que
			// le clic passe par le registre ou qu'il soit teste a la main.
			hit.PushOcclusion({0.f, 0.f, W, H}, 100);
			st.UiBlockAdd(box);
		}

		// ── GROUPE DE TRANSFORMATION, AU FORMAT DE LA MAQUETTE ──────────────────
		// Dessin choisi par Rihen (Banani) : le titre sur SA ligne, puis une rangee
		// de trois cellules « barre d'axe coloree + champ », et enfin les trois
		// commandes cadenas / reinitialiser / proportionnel.
		//
		// L'axe ne s'ecrit pas : il se LIT A LA COULEUR de sa barre. Ces trois
		// couleurs sont celles de la maquette (plus claires que celles de la vue
		// 3D) -- c'est ce que Rihen a valide a l'ecran.
		inline float32 NkXformGroupH() {
			return kRowH + S(28.f);
		}

		// ── GROUPE DE PROPRIETES REPLIABLE (bandeau + chevron) ──────────────────
		// Les elements de nature differente se rangent par GROUPE (Rihen) :
		// Transformation, Dimensions, Relations, Materiaux... Le bandeau porte le
		// chevron ; replier un groupe cache SON contenu, pas celui des voisins.
		// Renvoie true si le groupe est DEPLIE (donc s'il faut peindre son
		// contenu). `bit` identifie le groupe dans st.grpFold.
		// MARGE du contenu dans le panneau : il ne colle ni au bord gauche ni a la
		// gouttiere de droite (Rihen) -- un texte qui touche le bord se lit mal et
		// donne l'impression que le panneau est trop petit.
		inline float32 NkPropInset() {
			return S(10.f);
		}
		// Le BLOC qui entoure le contenu d'un groupe : il dit ou le groupe commence
		// et ou il finit, ce qu'un simple bandeau ne suffit pas a montrer quand
		// plusieurs groupes se suivent (Rihen).
		// L'ESPACE ENTRE DEUX GROUPES. Il vaut AUSSI pour un groupe replie (Rihen) :
		// c'est justement quand les bandeaux se suivent qu'ils ont besoin de
		// respirer, sinon ils forment une colonne indistincte.
		inline float32 NkPropGroupGap() {
			return S(8.f);
		}
		// MARGE INTERNE d'un groupe : son contenu ne touche ni le cadre a gauche ni
		// a droite, et respire aussi en haut et en bas (Rihen). Sans elle, les
		// champs semblaient colles aux bords du bloc.
		inline float32 NkGroupPad() {
			return S(8.f);
		}
		// Le rectangle de TRAVAIL a l'interieur d'un groupe.
		inline NkRect NkGroupInner(const NkRect &r) {
			return {r.x + NkGroupPad(), 0.f, r.w - 2.f * NkGroupPad(), 0.f};
		}
		inline void PaintGroupBlock(NkModelerPainter &p, const NkRect &r, float32 yTop,
									float32 yBottom) {
			if (yBottom <= yTop)
				return;
			// CONTOUR SEUL. `Outline` REPEINT le fond : peint apres le contenu, il
			// effacait les champs du groupe -- ils s'affichaient vides (constate
			// par Rihen). Le cadre se trace donc sans remplissage.
			p.OutlineSharp({r.x, yTop, r.w, yBottom - yTop}, NkRole::Border);
		}
		// ── UNE SECTION-LISTE DU PANNEAU MODELE ────────────────────────────────
		// Dessin de la maquette : chaque element porte son marqueur, son nom
		// EDITABLE, sa valeur, puis la rangee de quatre commandes
		// (assigner / deselectionner / ajouter / retirer) ; la section se termine
		// par « + Ajouter ».
		//
		// AUCUNE DONNEE INVENTEE : la liste part vide. Ces natures n'existent pas
		// encore dans le moteur, alors on montre honnetement qu'il n'y a rien, et
		// on garde ce que l'utilisateur cree (regle de Rihen sur les references).
		inline void PaintListSection(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st,
									 const NkRect &r, float32 &y, const char *keyBase,
									 uint8 kind, int32 owner, NkIcon mark, NkRole markRole,
									 const char *newName, const char *emptyNote,
									 bool withValue) {
			const NkRect inR = NkGroupInner(r);
			char key[48];
			int32 shown = 0;
			for (int32 i = 0; i < st.listCount; ++i) {
				if (st.listItems[i].kind != kind || st.listItems[i].owner != owner)
					continue;
				++shown;
				p.IconV(inR.x, y, kRowH, mark, markRole, 10.f);
				snprintf(key, sizeof(key), "%s.n%d", keyBase, i);
				const float32 metaW = withValue ? S(46.f) : 0.f;
				EditableText(p, hit, ws, in, key,
							 {inR.x + S(16.f), y, inR.w - S(16.f) - metaW, kRowH},
							 st.listItems[i].name, NkRole::Text, st.listItems[i].name, 20u);
				if (withValue) {
					char mv[16];
					snprintf(mv, sizeof(mv), "%.0f%%", (double)(st.listItems[i].value * 100.f));
					p.TextV(inR.x + inR.w - metaW, y, kRowH, mv, NkRole::TextMuted);
				}
				y += kRowH;
				// Les quatre commandes de la maquette, a parts egales.
				{
					static const NkIcon kAct[4] = {NkIcon::SquareCheck, NkIcon::Square,
												   NkIcon::PlusCircle, NkIcon::MinusCircle};
					const float32 gap = S(3.f);
					const float32 bw = (inR.w - gap * 3.f) * 0.25f;
					const float32 bh = S(18.f);
					float32 bx = inR.x;
					for (int32 a = 0; a < 4; ++a) {
						snprintf(key, sizeof(key), "%s.a%d.%d", keyBase, i, a);
						const NkRect br{bx, y, bw, bh};
						const bool ov = hit.Add(key, br);
						p.Outline(br, ov ? NkRole::AccentUi : NkRole::Border,
								  NkRole::PanelHeader, 3.f);
						p.IconV(br.x + (bw - S(10.f)) * 0.5f, br.y, bh, kAct[a],
								NkRole::TextMuted, 10.f);
						// RETIRER est la seule action que nous savons deja tenir :
						// les trois autres attendent le modele de donnees.
						if (a == 3 && hit.Clicked(key)) {
							for (int32 k = i; k + 1 < st.listCount; ++k)
								st.listItems[k] = st.listItems[k + 1];
							--st.listCount;
						}
						bx += bw + gap;
					}
					y += bh + S(4.f);
				}
			}
			if (shown == 0) {
				p.TextV(inR.x, y, kRowH, emptyNote, NkRole::TextMuted);
				y += kRowH;
			}
			snprintf(key, sizeof(key), "%s.add", keyBase);
			{
				const NkRect br{inR.x, y, inR.w, S(20.f)};
				const bool ov = hit.Add(key, br);
				p.Outline(br, ov ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				const float32 tw = p.TextW("Ajouter");
				p.IconV(br.x + (br.w - tw) * 0.5f - S(14.f), br.y, br.h, NkIcon::Add,
						NkRole::TextMuted, 10.f);
				p.TextV(br.x + (br.w - tw) * 0.5f, br.y - S(1.f), br.h, "Ajouter",
						NkRole::TextMuted);
				if (hit.Clicked(key) && st.listCount < 64 && owner >= 0) {
					NkModelerState::ListItem &it = st.listItems[st.listCount++];
					it.kind = kind;
					it.owner = owner;
					it.value = 1.f;
					it.on = true;
					snprintf(it.name, sizeof(it.name), "%s_%02d", newName, shown + 1);
				}
				y += br.h;
			}
		}
		inline bool PaintPropGroup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   const NkRect &r, float32 &y, const char *key,
								   const char *title, uint32 bit) {
			const NkRect hr{r.x, y, r.w, kRowH};
			const bool over = hit.Add(key, hr);
			p.Fill(hr, NkRole::PanelHeader);
			if (over)
				p.Fill({hr.x, hr.y + hr.h - S(2.f), hr.w, S(2.f)}, NkRole::AccentUi);
			const bool folded = (st.grpFold & bit) != 0u;
			p.IconV(r.x + S(4.f), y, kRowH,
					folded ? NkIcon::ChevronRight : NkIcon::ChevronDown, NkRole::Text, 11.f);
			p.TextV(r.x + S(20.f), y, kRowH, title);
			// ── LE MENU DU GROUPE, POUR TOUS LES GROUPES (Rihen) ────────────
			// Il est rendu ICI, dans la brique commune : aucun groupe n'a a le
			// reecrire, et un groupe ajoute demain l'aura sans y penser. Sa
			// facture est celle d'Unity : un petit bouton a droite du bandeau
			// qui deroule copier / coller / reinitialiser.
			{
				char mk[52];
				snprintf(mk, sizeof(mk), "%s.menu", key);
				const NkRect mb{hr.x + hr.w - S(22.f), y + S(3.f), S(18.f), kRowH - S(6.f)};
				const bool ovM = hit.Add(mk, mb);
				const bool openM = (strcmp(st.grpMenuKey, key) == 0);
				if (openM)
					p.Fill(mb, NkRole::AccentUi, 3.f);
				else if (ovM)
					p.Fill(mb, NkRole::InputBg, 3.f);
				p.IconV(mb.x + S(3.f), mb.y, mb.h, NkIcon::Menu,
						openM ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				if (hit.Clicked(mk)) {
					if (openM) {
						st.grpMenuKey[0] = 0;
					} else {
						NkWidgetState::Copy(st.grpMenuKey, key, 39u);
						NkWidgetState::Copy(st.grpMenuTitle, title, 39u);
						st.grpMenuAnchor = mb;
					}
				}
				// Le CHEVRON ne doit pas plier quand on visait le menu : la zone
				// du menu est declaree APRES, elle gagne donc le survol.
				if (hit.Clicked(key) && !ovM)
					st.grpFold ^= bit;
			}
			y += kRowH;
			return !folded;
		}
		// ── LE GROUPE RECLAME-T-IL UNE ACTION ? ─────────────────────────────
		// Teste ET CONSOMME : appelee au tour de peinture du groupe, elle rend
		// vrai une seule fois. `act` : 1 copier, 2 coller, 3 reinitialiser.
		inline bool NkGrpWants(NkModelerState &st, const char *key, int32 act) {
			if (st.grpAction != act || strcmp(st.grpActionKey, key) != 0)
				return false;
			st.grpAction = 0; // consommee
			return true;
		}
		// Le presse-papiers porte-t-il des valeurs de CE groupe ?
		inline bool NkGrpCanPaste(const NkModelerState &st, const char *key) {
			return st.grpClipHas && strcmp(st.grpClipKey, key) == 0;
		}
		inline void NkGrpCopyF(NkModelerState &st, const char *key, const float32 *v, int32 n) {
			NkWidgetState::Copy(st.grpClipKey, key, 39u);
			for (int32 i = 0; i < n && i < 16; ++i)
				st.grpClipF[i] = v[i];
			st.grpClipHas = true;
		}
		// ── LE MENU OUVERT D'UN GROUPE, peint EN SURCOUCHE ──────────────────
		// Appele une fois, tout a la fin du panneau : les entrees repondent
		// alors sans que le contenu du dessous ne vole leurs clics. Les actions
		// qui n'ont pas encore de sens sont GRISEES et le disent -- jamais une
		// entree qui fait semblant d'agir.
		inline void PaintPropGroupMenu(NkModelerPainter &p, NkHitRegistry &hit,
									   NkModelerState &st, const nkgui::NkGuiInput &in) {
			if (!st.grpMenuKey[0])
				return;
			// ── SURCOUCHE : COUCHE 50, comme tous les menus ─────────────────
			// Le registre donne le survol a la couche la PLUS HAUTE. Peint sur
			// la couche du panneau, ce menu ne gagnait pas ses propres clics :
			// les entrees ne repondaient pas, donc rien n'etait copie (« coller
			// reste grise ») et le menu ne se refermait jamais (Rihen). C'est le
			// meme dispositif que les menus de scene et les listes deroulees.
			NkHitRegistry::LayerScope menuLayer(hit, 50);
			static const char *const kIt[3] = {"Copier les proprietes",
											   "Coller les proprietes", "Reinitialiser"};
			const float32 rowH2 = S(22.f);
			float32 wI = 0.f;
			for (int32 i = 0; i < 3; ++i)
				if (p.TextW(kIt[i]) > wI)
					wI = p.TextW(kIt[i]);
			const float32 pw = wI + S(26.f), ph = rowH2 * 3.f + S(6.f);
			const NkRect &a = st.grpMenuAnchor;
			NkRect pr{a.x + a.w - pw, a.y + a.h + S(2.f), pw, ph};
			if (pr.x < S(4.f))
				pr.x = S(4.f);
			hit.Add("prop.grpmenu", pr);
			p.Fill(pr, NkRole::PanelBg, 4.f);
			p.OutlineSharp(pr, NkRole::Border);
			float32 yy = pr.y + S(3.f);
			// COLLER n'a de sens que depuis un groupe de MEME nature : coller
			// une Transformation dans un Brouillard ne veut rien dire.
			const bool canPaste = NkGrpCanPaste(st, st.grpMenuKey);
			char ik[32];
			for (int32 i = 0; i < 3; ++i) {
				snprintf(ik, sizeof(ik), "prop.grpmenu.%d", i);
				const NkRect ir{pr.x + S(3.f), yy, pr.w - S(6.f), rowH2};
				const bool en = (i == 0) || (i == 1 && canPaste) || i == 2;
				const bool ov = hit.Add(ik, ir);
				if (ov && en)
					p.Fill(ir, NkRole::PanelHeader, 3.f);
				p.TextV(ir.x + S(8.f), yy, rowH2, kIt[i],
						en ? NkRole::Text : NkRole::TextMuted);
				if (en && hit.Clicked(ik)) {
					if (i == 0)
						NkWidgetState::Copy(st.grpClipKey, st.grpMenuKey, 39u);
					// L'INTENTION est posee ici ; c'est la categorie
					// proprietaire du groupe qui l'execute, elle seule sait ce
					// que « copier » veut dire pour ses champs.
					NkWidgetState::Copy(st.grpActionKey, st.grpMenuKey, 39u);
					st.grpAction = i + 1;
					st.grpMenuKey[0] = 0;
				}
				yy += rowH2;
			}
			// SURCOUCHE BLOQUANTE, par le mecanisme DEJA en place pour les menus
			// de la hierarchie (repris de NKCode) : l'emprise memorisee d'une
			// frame sur l'autre, que les panneaux consultent avant d'accepter un
			// clic. SetBlock ne suffisait pas ici -- ce panneau est peint APRES
			// le contenu, qui avait deja tranche ses propres clics.
			st.UiBlockAdd(pr);
			// LE CLIC D'OUVERTURE NE DOIT PAS REFERMER. Le bandeau est peint
			// AVANT ce menu : dans la meme image, le clic qui vient de poser
			// l'ouverture tombait ici comme un « clic exterieur » et refermait
			// aussitot (constate par Rihen ; meme piege que le panneau matcap).
			// L'emprise du BOUTON d'ancrage est donc exclue.
			if (hit.AnyClick() && !NkHitRegistry::Contains(pr, in.mousePos) &&
				!NkHitRegistry::Contains(st.grpMenuAnchor, in.mousePos))
				st.grpMenuKey[0] = 0;
		}
		// Renvoie la HAUTEUR REELLE consommee : un groupe SANS titre (Dimensions,
		// qui n'a qu'une ligne) ne reserve pas la ligne du titre -- elle laissait
		// un grand vide en haut du bloc (constate par Rihen).
		// `neutral` est la valeur de REMISE A ZERO, donnee explicitement par
		// l'appelant : 0 pour une position, une rotation ou un pivot, 1 pour une
		// echelle. La deduire du pas etait faux -- position et echelle ont le meme
		// pas, et la position revenait donc a 1 (constate par Rihen).
		inline float32 PaintXformGroup(NkModelerPainter &p, NkHitRegistry &hit,
									   NkWidgetState &ws, const nkgui::NkGuiInput &in,
									   const NkRect &r, float32 y, const char *title,
									   float32 *v, float32 step, const char *keyBase,
									   bool &locked, bool &prop, const char *fmt = "%.2f",
									   float32 neutral = 0.f) {
			const bool hasTitle = title && title[0];
			if (hasTitle)
				p.TextV(r.x, y, kRowH, title);
			const float32 ry = hasTitle ? y + kRowH : y;
			const float32 rowH = S(22.f);
			// Boutons carres, a la MEME hauteur que les champs : ils suivent leur
			// taille (Rihen). Champs plus etroits -> boutons plus petits.
			const float32 btn = rowH;
			const float32 gap = S(3.f);
			const float32 iconsW = btn * 3.f + gap * 2.f;
			// LARGEUR PROPORTIONNELLE : les trois cellules se partagent tout
			// l'espace laisse par les commandes, et grandissent ou retrecissent
			// avec le panneau (Rihen). Seul un plancher les protege d'un panneau
			// trop etroit -- au-dela, c'est le clip du champ qui tronque.
			float32 cell = (r.w - iconsW - gap * 3.f) / 3.f;
			if (cell < S(30.f))
				cell = S(30.f);
			// LA COULEUR D'AXE EST DEJA DANS LE CHAMP (liseré gauche du champ) : en
			// remettre une a l'exterieur la disait deux fois (Rihen).
			static const NkRole kAxisRole[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			char key[56];
			float32 x = r.x;
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.%d", keyBase, i);
				DragFloat(p, hit, ws, in, key, {x, ry, cell, rowH}, v[i], step,
						  kAxisRole[i], fmt);
				x += cell + gap;
			}
			// Les trois commandes, dans l'ordre de la maquette : cadenas, remise a
			// zero, proportionnel. Elles s'ALLUMENT quand elles sont actives -- une
			// icone qui ne dit pas son etat oblige a essayer pour savoir.
			const NkIcon ics[3] = {locked ? NkIcon::Lock : NkIcon::Unlock, NkIcon::Refresh,
								   NkIcon::Link2};
			x = r.x + r.w - iconsW;
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.ic%d", keyBase, i);
				const NkRect br{x, ry, btn, rowH};
				const bool over = hit.Add(key, br);
				const bool on = (i == 0 && locked) || (i == 2 && prop);
				p.Outline(br, on ? NkRole::AccentUi
								 : (over ? NkRole::AccentUi : NkRole::Border),
						  on ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
				p.IconV(br.x + (btn - S(11.f)) * 0.5f, br.y, rowH, ics[i],
						on ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				if (hit.Clicked(key)) {
					if (i == 0)
						locked = !locked;
					else if (i == 1)
						v[0] = v[1] = v[2] = neutral; // valeur de repos du groupe
					else
						prop = !prop;
				}
				x += btn + gap;
			}
			// HAUTEUR NETTE, sans marge de fin : l'espacement entre deux lignes
			// appartient a l'appelant, sinon un groupe d'UNE ligne (Dimensions)
			// herite d'un vide en bas que rien ne justifie (Rihen).
			return (hasTitle ? kRowH : 0.f) + rowH;
		}

		// â”€â”€ EN-TETE DE SECTION REPLIABLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// La fleche REPLIE VRAIMENT la section. Une fleche qui ne fait rien est pire
		// qu'une absence de fleche : elle promet une commande et ne la tient pas.
		inline bool SectionHeader(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r, float32 y,
								  const char *key, const char *title, bool &open) {
			const NkRect hr{r.x, y, r.w, kRowH};
			const bool over = hit.Add(key, hr);
			// L'EN-TETE a TOUJOURS son fond propre, distinct du fond des valeurs
			// (demande de Rihen) ; le survol s'annonce par un lisere accent.
			p.Fill(hr, NkRole::PanelHeader);
			if (over)
				p.Fill({hr.x, hr.y + hr.h - S(2.f), hr.w, S(2.f)}, NkRole::AccentUi);
			p.IconV(r.x + S(6.f), y, kRowH, open ? NkIcon::ChevronDown : NkIcon::ChevronRight,
					NkRole::Text, 11.f);
			p.TextV(r.x + S(22.f), y, kRowH, title);
			if (hit.Clicked(key))
				open = !open;
			return open;
		}

		// â”€â”€ PANNEAU DROIT UNIQUE : OBJET / SCENE / OUTIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Trois SOUS-BLOCS repliables, chacun son DEFILEMENT : le contenu d'une
		// section peut etre tres long sans pousser les autres hors de l'ecran.
		// La hauteur se partage entre les sections OUVERTES ; la hauteur de
		// contenu est mesuree a l'image precedente (stable des la deuxieme).
		inline void PaintPropertiesUnified(NkModelerPainter &p, const NkRect &rFull,
										   NkModelerState &st, NkHitRegistry &hit, NkWidgetState &ws,
										   const nkgui::NkGuiInput &in, NkComboPending &combo,
										   nkgui::NkGuiContext *guiCtx = nullptr) {
			p.Fill(rFull, NkRole::PanelBg);
			p.VLine(rFull.x, rFull.y, rFull.h);
			// ── LA LISTE OUVERTE D'UN COMBO BLOQUE CE PANNEAU ───────────────
			// Elle est peinte APRES lui, par-dessus ses widgets : sans cette
			// garde, cliquer une entree atteignait AUSSI le widget situe
			// dessous, et le choix etait aussitot ecrase -- « je n'arrive pas a
			// choisir un format », « ca ne doit pas laisser traverser les
			// evenements » (Rihen). L'emprise est celle memorisee a l'image
			// precedente (patron UiBlockAdd/UiBlocks, deja employe par la
			// hierarchie et le navigateur) ; ce panneau lui manquait, alors
			// qu'il est celui qui porte le plus de listes.
			if (st.UiBlocks(hit.Mouse().x, hit.Mouse().y))
				hit.SetBlock({hit.Mouse().x - 1.f, hit.Mouse().y - 1.f, 2.f, 2.f}, true);
			// Editeurs sans design defini : pas de proprietes (Rihen).
			{
				const uint8 tkP = st.sceneTabKind[st.activeTab];
				if (tkP != 0 && tkP != 7) {
					p.TextV(rFull.x + S(12.f), rFull.y + S(6.f), kRowH,
							"Indisponible pour cet editeur", NkRole::TextMuted);
					return;
				}
			}
			// PLUS DE CROIX : les PASTILLES font l'affichage/masquage -- aucune
			// active, et le panneau n'est plus que sa colonne de pastilles.
			const bool collapsed = !st.AnyPropOpen();
			float32 y;
			if (collapsed) {
				y = rFull.y + S(6.f);
			} else {
				p.Fill({rFull.x, rFull.y, rFull.w, kRowH}, NkRole::PanelHeader);
				// L'EN-TETE PORTE LA CATEGORIE entre parentheses (Rihen) : une
				// seule pastille etant active, un second en-tete « Modele » sous
				// celui-ci repetait l'information et volait une ligne au contenu.
				{
					// SEPT sections de base depuis qu'Output existe : cette table
					// etait restee a six, et l'indice 6 -- Output -- tombait dans
					// la branche « pastille du mode ». L'en-tete n'affichait donc
					// aucun nom pour Output (constate par Rihen). La pastille du
					// MODE est desormais la 8e, comme partout ailleurs.
					static const char *const kHdrNames[7] = {"Modele",	 "Rendu",	 "Scene",
															 "Modificateur", "Materiau", "Outil",
															 "Output"};
					static const char *const kHdrMode[6] = {
						"Edition",	 "Sculpture 2.5D", "Sculpture",
						"Texturing", "Patron",		   "Texture painting"};
					int32 actSec = -1;
					for (int32 i9 = 0; i9 < 8; ++i9)
						if (st.propOpen[i9]) {
							actSec = i9;
							break;
						}
					char hd[64];
					if (actSec == 7 && (int32)st.mode >= 1 && (int32)st.mode <= 6)
						snprintf(hd, sizeof(hd), "Proprietes (%s)",
								 kHdrMode[(int32)st.mode - 1]);
					else if (actSec >= 0 && actSec < 7)
						snprintf(hd, sizeof(hd), "Proprietes (%s)", kHdrNames[actSec]);
					else
						snprintf(hd, sizeof(hd), "Proprietes");
					p.TextV(rFull.x + kPad, rFull.y, kRowH, hd);
				}
				p.HLine(rFull.x, rFull.y + kRowH - 1.f, rFull.w);
				y = rFull.y + kRowH;
			}
			// LA COLONNE DE PASTILLES (idee de Rihen, facon Blender) reserve le
			// bord droit ; tout le reste du panneau travaille dans r, ampute
			// d'autant. Entre les deux, la SCROLLBAR de NKEditorKit -- celle de
			// l'editeur de code -- est TOUJOURS visible (Rihen) : une barre qui
			// n'apparait qu'au besoin fait sauter la mise en page et laisse
			// douter qu'il y ait quelque chose plus bas.
			const float32 kSbW = editorkit::NkScrollbarWidth();
			NkRect r = rFull;
			r.w -= S(26.f) + kSbW;
			char key[40], buf[96];

			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				const NkRect br{x, yB + S(2.f), w, kRowH - S(4.f)};
				const bool over = hit.Add(k2, br);
				p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				const float32 tw = p.TextW(label);
				p.TextV(br.x + (br.w - tw) * 0.5f, yB, kRowH, label);
				return hit.Clicked(k2);
			};

			static float32 sContentH[8] = {200.f, 260.f, 200.f, 200.f,
										   200.f, 200.f, 200.f, 200.f};
			// ── LA TABLE DES CATEGORIES ─────────────────────────────────────
			// EN AJOUTER UNE = une entree ici (titre + icone de pastille) + son
			// contenu dans le switch plus bas. Pastilles, pliage, defilement et
			// hauteurs suivent la table sans autre code.
			struct NkPropSec {
					const char *title;
					NkIcon icon;
			};
			// LES CATEGORIES de la maquette (Rihen, dessin Banani), avec leurs
			// icones : Modele (box), Rendu (sun), Scene (layers), Modificateur
			// (sliders-horizontal), Materiau (sphere). UNE SEULE est active a la
			// fois.
			// OUTIL a sa PROPRE pastille (Rihen) : les reglages de l'outil actif
			// etaient HEBERGES par Modificateur depuis qu'une pastille de la
			// maquette avait disparu -- un hebergement annonce comme provisoire
			// dans le code. Ils rejoignent leur place, comme l'onglet « Tool »
			// de Blender ; Modificateur ne garde que les modificateurs.
			// OUTPUT (Rihen) : ce qui SORT de la scene -- resolution, source,
			// destination, et les incrustations posees sur l'image principale.
			// Rendu dit COMMENT la scene est eclairee, Output dit CE QU'ON EN
			// PRODUIT : deux questions distinctes, deux pastilles.
			// Elle est ajoutee EN FIN de liste, pas apres Rendu ou sa place
			// serait plus logique : les indices de section sont memorises dans
			// st.propOpen et cables en dur ailleurs (kSelOnly, la pastille du
			// mode) -- les decaler casserait ces regles en silence.
			static const NkPropSec kSecsBase[7] = {{"Modele", NkIcon::Cube3D},
												   {"Rendu", NkIcon::Sun},
												   {"Scene", NkIcon::Layers},
												   {"Modificateur", NkIcon::SlidersH},
												   {"Materiau", NkIcon::Material},
												   {"Outil", NkIcon::Move},
												   {"Output", NkIcon::Camera}};
			// ── LA PASTILLE DU MODE (regle de Rihen) : chaque mode hors Objet a
			// SA pastille, unique a lui, qui n'existe QUE dans ce mode -- ses
			// fonctions s'y rempliront progressivement, par categories. Une
			// seule entree (indice 5) dont le visage suit le mode courant.
			static const NkPropSec kModeSecs[6] = {{"Edition", NkIcon::Edit},
												   {"Sculpture 2.5D", NkIcon::Layers},
												   {"Sculpture", NkIcon::Ruler},
												   {"Texturing", NkIcon::Overlay},
												   {"Patron", NkIcon::ViewUV},
												   {"Texture painting", NkIcon::Picker}};
			const int32 modeIdx5 = (int32)st.mode; // 0 = Objet
			NkPropSec kSecs[8];
			for (int32 i2 = 0; i2 < 7; ++i2)
				kSecs[i2] = kSecsBase[i2];
			const int32 kNSec = modeIdx5 > 0 ? 8 : 7;
			if (modeIdx5 > 0)
				kSecs[7] = kModeSecs[modeIdx5 - 1]; // la pastille du MODE, en dernier
			// ENTRER dans un mode ACTIVE sa pastille -- mais SEULEMENT si le
			// panneau etait deja ouvert : ferme, il le reste (Rihen -- «
			// mettre sa pastille mais pas ouvrir le panneau s'il etait ferme »).
			// En SORTIR : la pastille disparait, et si elle etait l'active la
			// main revient a Modele (qui suit sa propre regle de selection).
			{
				static int32 sLastMode5 = -1;
				if (sLastMode5 != modeIdx5) {
					if (sLastMode5 >= 0) {
						bool anyOpen5 = false;
						for (int32 j2 = 0; j2 < 8; ++j2)
							anyOpen5 = anyOpen5 || st.propOpen[j2];
						if (modeIdx5 > 0) {
							if (anyOpen5) {
								for (int32 j2 = 0; j2 < 8; ++j2)
									st.propOpen[j2] = false;
								st.propOpen[7] = true;
							}
						} else if (st.propOpen[7]) {
							st.propOpen[7] = false;
							st.propOpen[0] = true;
						}
					}
					sLastMode5 = modeIdx5;
				}
			}
			if (modeIdx5 == 0 && st.propOpen[7]) {
				st.propOpen[7] = false;
				st.propSecH[7] = 0.f;
				st.propScroll3[7] = 0.f;
			}
			// LA PASTILLE MODELE N'EXISTE QUE POUR UNE SELECTION (regle de
			// Rihen) : sans objet actif elle disparait de la colonne, et si
			// elle etait l'active le panneau se replie -- un panneau Modele
			// sans modele n'aurait rien d'honnete a montrer.
			const bool hasSel5 = st.activeEmpty >= 0 ||
								 demo::Demo3DHostActiveObject() >= 0 ||
								 demo::Demo3DHostSelectedLight() >= 0;
			// MEME REGLE POUR MATERIAU ET MODIFICATEUR (Rihen) : sans objet, il
			// n'y a ni matiere a assigner ni modificateur a poser -- leurs
			// pastilles disparaissent comme celle de Modele, et si l'une etait
			// active le panneau se replie.
			static const int32 kSelOnly[3] = {0, 3, 4}; // Modele, Modificateur, Materiau
			if (!hasSel5)
				for (int32 s7 = 0; s7 < 3; ++s7) {
					const int32 i7 = kSelOnly[s7];
					if (!st.propOpen[i7])
						continue;
					st.propOpen[i7] = false;
					st.propSecH[i7] = 0.f;
					st.propScroll3[i7] = 0.f;
				}
			int32 nOpen = 0, nUnfold = 0;
			for (int32 i2 = 0; i2 < kNSec; ++i2)
				if (st.propOpen[i2]) {
					++nOpen;
					if (!st.propFold[i2])
						++nUnfold;
				}
			// Les en-tetes affiches (sections actives) sont deduits ; la place
			// restante se partage entre les sections DEPLIEES.
			const float32 availH = (r.y + r.h) - y - (float32)(nOpen > 0 ? nOpen : 1) * kRowH;
			// La gouttiere du scrollbar est DEJA retranchee de r (voir plus haut) :
			// en retirer une seconde fois laissait une large bande morte a droite
			// du contenu (constate par Rihen).
			const NkRect rr{r.x, 0.f, r.w, 0.f};
			// DEFILEMENT GLOBAL : la pile entiere glisse de propScroll ; le
			// contenu est mesure au fil de la peinture et la barre du bord la
			// pilote (celles des sections sont inserees).
			const float32 stackTop = y;
			p.Clip({r.x, stackTop, r.w, (r.y + r.h) - stackTop});
			float32 secY = y - st.propScroll;

			bool anyWheel = false;
			for (int32 sec = 0; sec < kNSec; ++sec) {
				// PASTILLE DECOCHEE = SECTION RETIREE de la liste (Rihen) : ni
				// contenu NI en-tete -- la colonne de pastilles est le seul moyen
				// de la faire revenir.
				if (!st.propOpen[sec])
					continue;
				snprintf(key, sizeof(key), "props.sec.%d", sec);
				// Le CHEVRON plie/deplie ; il ne retire jamais la section de la
				// liste (ca, c'est la pastille). Et AUCUN clic d'en-tete ne
				// compte pendant un glissement : relacher la POIGNEE sur
				// l'en-tete voisin basculait le chevron (constate par Rihen).
				// PLUS D'EN-TETE DE CATEGORIE ICI : il repetait le titre du panneau,
				// qui porte deja la categorie entre parentheses (Rihen). Le contenu
				// commence donc directement par ses propres sections, comme sur la
				// maquette. La section reste depliee -- son pliage se ferait au
				// niveau de chaque bloc interne.
				st.propFold[sec] = false;
				// REPARTITION EN DEUX PASSES : les sections plus petites que leur
				// part rendent l'espace, redistribue aux plus grandes -- une
				// section n'a de defilement local que quand l'ESPACE TOTAL manque
				// (une petite section en dessous ne doit pas figer la part des
				// autres, constate par Rihen).
				float32 boxH;
				{
					float32 want[8];
					bool alloc[8] = {};
					float32 given[8] = {};
					// La hauteur CHOISIE a la poignee est INTOUCHABLE : allouee
					// telle quelle, meme si le total deborde -- c'est alors le
					// DEFILEMENT GLOBAL qui prend le relais. La plafonner a la
					// part egale (version precedente) rendait la poignee inerte
					// et eteignait la barre generale (constate par Rihen). Les
					// deux passes ne repartissent que les sections AUTO.
					float32 remaining = availH;
					int32 hungry = 0;
					for (int32 j = 0; j < kNSec; ++j) {
						if (!st.propOpen[j] || st.propFold[j])
							continue;
						if (st.propSecH[j] > 0.f) {
							given[j] = st.propSecH[j];
							alloc[j] = true;
							remaining -= given[j];
						} else {
							want[j] = sContentH[j];
							++hungry;
						}
					}
					if (remaining < 0.f)
						remaining = 0.f;
					for (int32 pass = 0; pass < 3 && hungry > 0; ++pass) {
						const float32 sh = remaining / (float32)hungry;
						bool moved = false;
						for (int32 j = 0; j < kNSec; ++j) {
							if (alloc[j] || !st.propOpen[j] || st.propFold[j])
								continue;
							if (want[j] <= sh) {
								given[j] = want[j];
								alloc[j] = true;
								remaining -= want[j];
								--hungry;
								moved = true;
							}
						}
						if (!moved)
							break;
					}
					const float32 shFinal = hungry > 0 ? remaining / (float32)hungry : 0.f;
					for (int32 j = 0; j < kNSec; ++j)
						if (!alloc[j])
							given[j] = shFinal;
					boxH = given[sec];
				}
				if (boxH < kRowH)
					boxH = kRowH;
				// UNE SEULE SECTION A LA FOIS : elle prend TOUTE la hauteur, et
				// c'est la barre generale -- celle de NKEditorKit, toujours
				// visible -- qui la fait defiler. Lui donner une part et une barre
				// a elle revenait a decouper un seul contenu en deux gestes de
				// defilement (Rihen).
				boxH = (r.y + r.h) - secY;
				if (boxH < kRowH)
					boxH = kRowH;
				st.propScroll3[sec] = 0.f;
				// PAS de plafond calcule sur la position DEFILEE : il creait une
				// retroaction (descendre allongeait la derniere section, donc la
				// pile, donc le defilement...) -- c'etait le CLIGNOTEMENT de la
				// barre generale constate par Rihen. L'exces de hauteur est
				// l'affaire du defilement de page, pas d'un plafond.
				const NkRect box{r.x, secY, r.w, boxH};
				snprintf(key, sizeof(key), "props.body.%d", sec);
				hit.Add(key, box);
				p.Clip(box);
				hit.PushClip(box); // les zones suivent le dessin : rien d'invisible
				// UNE RESPIRATION EN HAUT DE CHAQUE SECTION (Rihen) : Modele,
				// Rendu, Scene et Modificateur. Le premier groupe collait au
				// bandeau du panneau, ce qui donnait l'impression qu'il en faisait
				// partie. Pose ICI, au point de depart commun aux quatre : une
				// marge ajoutee section par section aurait fini par diverger.
				float32 yy = secY - st.propScroll3[sec] + S(8.f);

				if (sec == 0) {
					// ── LA PIPETTE A-T-ELLE DESIGNE QUELQU'UN ? ──────────────────
					// Elle se resout ICI, avant tout le reste : cliquer une cible
					// CHANGE LA SELECTION, donc l'objet que le panneau affiche. Se
					// fier a l'objet courant arrivait toujours trop tard -- le
					// sujet etait deja remplace par la cible (constate par Rihen :
					// « le picker se confond avec le clic de selection »).
					if (st.relPick != 0 && st.relPickFor >= 0 && st.activeEmpty >= 0 &&
						st.activeEmpty != st.relPickPrev &&
						st.activeEmpty != st.relPickFor) {
						const int32 subj = st.relPickFor;
						const int32 tgt = st.activeEmpty;
						if (st.relPick == 1 && !NkHierIsDescendant(tgt, subj)) {
							demo::Demo3DHostSetNodeParent(subj, tgt);
							NkMarkDirty(st);
						} else if (st.relPick == 2 && !NkHierIsDescendant(subj, tgt)) {
							demo::Demo3DHostSetNodeParent(tgt, subj);
							NkMarkDirty(st);
						}
						// On REVIENT sur l'objet qu'on editait : sinon le panneau
						// reste sur la cible et l'on perd le fil de son reglage.
						demo::Demo3DHostDeselectAll();
						demo::Demo3DHostSelectEmptyNode(subj);
						st.activeEmpty = subj;
						st.relPick = 0;
						st.relPickFor = -1;
					}
					// â”€â”€ L'OBJET : nom + TRANSFORMATION COMPLETE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					const int32 li = demo::Demo3DHostSelectedLight();
					const int32 act = demo::Demo3DHostActiveObject();
					if (st.activeEmpty >= 0) {
						// ── UN EMPTY : nom + transformation. Pas de rendu propre --
						// sa transformation n'agit QUE par ses enfants (le detecteur
						// de parente la repercute au sous-arbre).
						const int32 en = st.activeEmpty;
						NkHierNodeName(st, en, buf, sizeof(buf));
						// LE NOM EST UN CHAMP, pas une etiquette (Rihen) : on doit
						// pouvoir renommer ici, avant meme de toucher a la
						// transformation, sans passer par la hierarchie.
						{
							// L'icone dit la NATURE de l'objet, comme dans la
							// hierarchie : model, maillage, lumiere, camera, empty.
							// Elle est POUSSEE vers la droite (Rihen) : elle s'aligne
							// sur la marge du contenu, et le champ demarre apres elle.
							const float32 icoX = r.x + NkPropInset();
							p.IconV(icoX, yy, kRowH, NkNodeIcon(en), NkRole::Text, 13.f);
							const NkRect nmR{icoX + S(22.f), yy + S(2.f),
											 (r.x + rr.w - NkPropInset()) - (icoX + S(22.f)),
											 kRowH - S(4.f)};
							p.Outline(nmR, NkRole::Border, NkRole::InputBg, 3.f);
							if (en >= 0 && en < 176)
								EditableText(p, hit, ws, in, "props.name",
											 {nmR.x + S(4.f), yy, nmR.w - S(8.f), kRowH},
											 buf, NkRole::Text, st.customNames[en], 24u);
							yy += kRowH;
							// RESPIRATION entre le nom et le premier groupe
							// (Transformation) : colles, ils se lisaient comme un
							// seul bloc (Rihen).
							yy += NkPropGroupGap();
						}
						static int32 sELast = -1;
						static float32 sE[9] = {};
						float32 ep[3], er2[3], es2[3];
						if (demo::Demo3DHostEmptyTransform(en, ep, er2, es2)) {
							const bool holdE = ws.dragging || ws.editing;
							if (!holdE || en != sELast) {
								for (int32 a = 0; a < 3; ++a) {
									st.pos[a] = ep[a];
									st.rot[a] = er2[a];
									st.scl[a] = es2[a];
									sE[a] = ep[a];
									sE[3 + a] = er2[a];
									sE[6 + a] = es2[a];
								}
								sELast = en;
							}
							// LE CONTENU EST CENTRE dans le panneau, avec la meme
							// marge a gauche et a droite (Rihen).
							NkRect rowR = rr;
							rowR.x = r.x + NkPropInset();
							rowR.w = rr.w - 2.f * NkPropInset();
							// GROUPE « Transformation » : bandeau repliable, puis
							// Position / Rotation / Echelle / Pivot (Rihen, Banani).
							const bool grpXf = PaintPropGroup(p, hit, st, rowR, yy,
															  "prop.g.xform",
															  "Transformation", 1u);
							const float32 grpXfTop = yy;
							// Le contenu travaille EN RETRAIT du cadre (Rihen).
							const NkRect inR = NkGroupInner(rowR);
							// ── LES ACTIONS DU MENU DE CE GROUPE ───────────────
							// Elles agissent AVANT la peinture des champs, pour que
							// ceux-ci montrent deja le resultat. COPIER prend les
							// neuf valeurs, COLLER les repose sur l'objet courant
							// (c'est le geste demande par Rihen : porter la
							// transformation d'un objet sur un autre), et
							// REINITIALISER rend l'objet a l'origine, sans
							// rotation, a l'echelle 1.
							{
								const float32 cur9[9] = {st.pos[0], st.pos[1], st.pos[2],
														 st.rot[0], st.rot[1], st.rot[2],
														 st.scl[0], st.scl[1], st.scl[2]};
								if (NkGrpWants(st, "prop.g.xform", 1))
									NkGrpCopyF(st, "prop.g.xform", cur9, 9);
								if (NkGrpWants(st, "prop.g.xform", 2) &&
									NkGrpCanPaste(st, "prop.g.xform")) {
									for (int32 a = 0; a < 3; ++a) {
										st.pos[a] = st.grpClipF[a];
										st.rot[a] = st.grpClipF[3 + a];
										st.scl[a] = st.grpClipF[6 + a];
									}
									demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
									NkMarkDirty(st);
								}
								if (NkGrpWants(st, "prop.g.xform", 3)) {
									for (int32 a = 0; a < 3; ++a) {
										st.pos[a] = 0.f;
										st.rot[a] = 0.f;
										st.scl[a] = 1.f;
									}
									demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
									NkMarkDirty(st);
								}
							}
							if (grpXf) {
								yy += NkGroupPad(); // respiration en haut du bloc
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Position",
												st.pos, 0.01f, "prop.epos", st.lockPos,
												st.propPos, "%.2f m", 0.f);
								yy += NkGroupPad(); // entre deux lignes du groupe
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Rotation",
												st.rot, 0.5f, "prop.erot", st.lockRot,
												st.propRot, "%.1f\xC2\xB0", 0.f);
								yy += NkGroupPad();
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Echelle",
												st.scl, 0.01f, "prop.escl", st.lockScl,
												st.propScale, "%.2f", 1.f);
							}
							// ── PIVOT (origine) ────────────────────────────────
							// Blender ne le laisse bouger qu'en mode Edition ; on
							// l'offre AUSSI en mode Objet (Rihen : « on ne sait
							// jamais »), avec son cadenas pour le figer quand on
							// ne veut pas y toucher -- comme les autres lignes.
							// Le deplacer NE DEPLACE PAS la matiere : les enfants
							// reculent d'autant, seul le point de rotation et de
							// mise a l'echelle change.
							if (grpXf) {
								float32 piv[3];
								if (demo::Demo3DHostNodeOrigin(act, piv)) {
									const float32 piv0[3] = {piv[0], piv[1], piv[2]};
									yy += NkGroupPad();
									yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Pivot", piv,
													0.01f, "prop.epiv", st.lockPiv,
													st.propPiv, "%.2f m", 0.f);
									bool pivCh = false;
									for (int32 a = 0; a < 3; ++a)
										if (piv[a] != piv0[a])
											pivCh = true;
									if (pivCh && !st.lockPiv) {
										demo::Demo3DHostSetNodeOrigin(act, piv);
										NkMarkDirty(st);
									}
									float32 ctr[3];
									if (demo::Demo3DHostMeshesCenter(act, ctr)) {
										yy += NkGroupPad();
										if (Button("props.pivctr", yy,
												   "Pivot au centre des maillages", inR.x,
												   inR.w) &&
											!st.lockPiv) {
											demo::Demo3DHostSetNodeOrigin(act, ctr);
											NkMarkDirty(st);
										}
										yy += kRowH - S(4.f);
									}
								}
							}
							// Le BLOC qui entoure le groupe : peint APRES son contenu,
							// puisqu'il faut connaitre ou celui-ci s'arrete. Il se
							// referme sur une marge egale a celle du haut.
							if (grpXf) {
								yy += NkGroupPad();
								PaintGroupBlock(p, rowR, grpXfTop, yy);
							}
							// L'espace suit le groupe REPLIE OU NON (Rihen).
							yy += NkPropGroupGap();
							// Proportionnel et verrous : memes regles que l'objet.
							auto PropagateE = [](float32 *vals, const float32 *base, bool on) {
								if (!on)
									return;
								int32 ch = -1;
								for (int32 a = 0; a < 3; ++a)
									if (vals[a] != base[a])
										ch = a;
								if (ch < 0)
									return;
								if (fabsf(base[ch]) > 1e-6f) {
									const float32 ratio = vals[ch] / base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] * ratio;
								} else {
									const float32 d = vals[ch] - base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] + d;
								}
							};
							PropagateE(st.pos, sE, st.propPos);
							PropagateE(st.rot, sE + 3, st.propRot);
							PropagateE(st.scl, sE + 6, st.propScale);
							for (int32 a = 0; a < 3; ++a) {
								if (st.lockPos)
									st.pos[a] = sE[a];
								if (st.lockRot)
									st.rot[a] = sE[3 + a];
								if (st.lockScl)
									st.scl[a] = sE[6 + a];
							}
							// Un MAILLAGE UTILISATEUR a TOUTES les proprietes d'un
							// mesh (Rihen) : DIMENSIONS ici, materiau plus bas.
							const int32 ukE = demo::Demo3DHostUserKind(en);
							if (ukE >= 1 && ukE <= 3) {
								float32 dimE[3];
								demo::Demo3DHostNodeBaseSize(en, dimE);
								const float32 dimE0[3] = {dimE[0], dimE[1], dimE[2]};
								// GROUPE « Dimensions » : sa propre bande repliable.
								const bool grpDim = PaintPropGroup(p, hit, st, rowR, yy,
																   "prop.g.dim", "Dimensions",
																   2u);
								const float32 grpDimTop = yy;
								// ACTIONS DU MENU : copier / coller les trois cotes,
								// reinitialiser aux dimensions de la nature (un
								// tableau vide, que Demo3DHostNodeBaseSize rendra).
								if (NkGrpWants(st, "prop.g.dim", 1))
									NkGrpCopyF(st, "prop.g.dim", dimE, 3);
								if (NkGrpWants(st, "prop.g.dim", 2) &&
									NkGrpCanPaste(st, "prop.g.dim")) {
									for (int32 a = 0; a < 3; ++a)
										dimE[a] = st.grpClipF[a];
									demo::Demo3DHostSetNodeBaseSize(en, dimE);
									NkMarkDirty(st);
								}
								if (grpDim) {
									yy += NkGroupPad();
									yy += PaintXformGroup(p, hit, ws, in, NkGroupInner(rowR), yy, "",
													dimE, 0.01f, "prop.edim", st.lockDim,
													st.propDim, "%.2f m", 1.f);
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpDimTop, yy);
								}
								yy += NkPropGroupGap(); // meme replie (Rihen)
					// PROPORTIONNEL : l'axe touche impose son RAPPORT aux autres ;
					// sinon chaque dimension ne bouge QUE son axe (Rihen).
					if (st.propDim) {
						int32 chD = -1;
						for (int32 a = 0; a < 3; ++a)
							if (dimE[a] != dimE0[a])
								chD = a;
						if (chD >= 0 && fabsf(dimE0[chD]) > 1e-6f) {
							const float32 rd = dimE[chD] / dimE0[chD];
							for (int32 a = 0; a < 3; ++a)
								if (a != chD)
									dimE[a] = dimE0[a] * rd;
						}
					}
					if (st.lockDim)
						for (int32 a = 0; a < 3; ++a)
							dimE[a] = dimE0[a];
					bool dimChE = false;
					for (int32 a = 0; a < 3; ++a)
						if (dimE[a] != dimE0[a])
							dimChE = true;
					if (dimChE) {
						demo::Demo3DHostSetNodeBaseSize(en, dimE);
						NkMarkDirty(st);
					}
							}
							// ── GROUPE « RELATIONS » ────────────────────────────
							// La maquette montre Parent et Enfant ; Blender y ajoute
							// ce qui rend la parente COMPREHENSIBLE : ce que le
							// parent transmet, et le nombre d'enfants portes. Sans
							// cela on voit un lien sans savoir ce qu'il fait.
							{
								const bool grpRel = PaintPropGroup(p, hit, st, rowR, yy,
																   "prop.g.rel", "Relations",
																   4u);
								const float32 grpRelTop = yy;
								if (grpRel) {
									const NkRect iR = NkGroupInner(rowR);
									yy += NkGroupPad();
									// (La pipette se resout PLUS HAUT, au niveau du panneau :
									// designer une cible change la selection, donc l'objet
									// affiche -- la resoudre ici arrivait trop tard.)
									const float32 valX = iR.x + S(96.f);
									const float32 valW = iR.w - S(96.f);
									// PARENT : une LISTE, pas une etiquette (Rihen) -- on
									// CHANGE de parent depuis elle, « Aucun » detachant
									// l'objet. Les candidats excluent l'objet lui-meme
									// et sa descendance : s'y rattacher ferait un cycle.
									p.TextV(iR.x, yy, kRowH, "Parent", NkRole::TextMuted);
									{
										static char sParName[24][24];
										static const char *sParPtr[24];
										static int32 sParNode[24];
										static int32 sParOwner = -1;
										static int32 sParSel = 0;
										// LE NOEUD DEJA APPLIQUE. La liste est peinte APRES
										// le panneau : le choix de l'utilisateur n'arrive
										// donc qu'a la frame suivante. Comparer a l'etat
										// « avant l'appel » ne voyait jamais rien, et
										// recaler l'indice sur le parent reel effacait le
										// choix avant qu'on le lise -- le piege des listes
										// differees, deja paye une fois.
										static int32 sParApplied = -2;
										int32 np = 0;
										snprintf(sParName[0], sizeof(sParName[0]), "Aucun");
										sParPtr[0] = sParName[0];
										sParNode[0] = -1;
										np = 1;
										const int32 pa = demo::Demo3DHostNodeParent(en);
										int32 curIdx = 0;
										const int32 ncP = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncP && np < 24; ++c9) {
											if (c9 == en || NkHierNodeSkip(c9))
												continue;
											if (NkHierIsDescendant(c9, en))
												continue; // interdit : cycle
											NkHierNodeName(st, c9, sParName[np],
														   sizeof(sParName[0]));
											sParPtr[np] = sParName[np];
											sParNode[np] = c9;
											if (c9 == pa)
												curIdx = np;
											++np;
										}
										if (sParOwner != en) {
											sParOwner = en;
											sParSel = curIdx;
											sParApplied = pa;
										}
										if (sParSel < 0 || sParSel >= np)
											sParSel = curIdx;
										// DEUX SENS, sans se marcher dessus : si la liste
										// designe autre chose que ce qu'on a applique,
										// c'est l'utilisateur qui a choisi -> on reparente.
										// Sinon, si la parente reelle a change AILLEURS
										// (hierarchie, glisser-deposer), la liste s'y
										// recale.
										if (sParNode[sParSel] != sParApplied) {
											demo::Demo3DHostSetNodeParent(en, sParNode[sParSel]);
											NkMarkDirty(st);
											sParApplied = sParNode[sParSel];
										} else if (pa != sParApplied) {
											sParSel = curIdx;
											sParApplied = pa;
										}
										// PIPETTE : designer le parent dans la vue plutot que
										// de le chercher dans la liste (Rihen, Blender).
										const bool pkP = (st.relPick == 1 && st.relPickFor == en);
										const NkRect vb{valX, yy + S(3.f), valW - S(24.f),
														kRowH - S(6.f)};
										Combo(p, hit, ws, "prop.rel.par", vb, sParPtr, nullptr, np,
											  sParSel, combo);
										{
											const NkRect eb{vb.x + vb.w + S(4.f), vb.y, S(20.f),
															vb.h};
											const bool ovE = hit.Add("prop.rel.parpick", eb);
											p.Outline(eb,
													  (pkP || ovE) ? NkRole::AccentUi
																   : NkRole::Border,
													  pkP ? NkRole::AccentUi : NkRole::PanelHeader,
													  3.f);
											p.IconV(eb.x + (eb.w - S(11.f)) * 0.5f, eb.y, eb.h,
													NkIcon::Pipette,
													pkP ? NkRole::TextOnAccent : NkRole::TextMuted,
													11.f);
											if (hit.Clicked("prop.rel.parpick")) {
												st.relPick = pkP ? 0 : 1;
												st.relPickFor = en;
												st.relPickPrev = st.activeEmpty;
											}
										}
									}
									yy += kRowH;
									// ENFANTS : la LISTE, pas un compte (Rihen). Un nombre
									// dit qu'il y en a ; la liste dit LESQUELS -- et la
									// choisir, c'est y aller.
									{
										static char sKidName[16][24];
										static const char *sKidPtr[16];
										static int32 sKidNode[16];
										static int32 sKidOwner = -1;
										static int32 sKidSel = 0;
										int32 nk = 0;
										const int32 ncR = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncR && nk < 16; ++c9)
											if (!NkHierNodeSkip(c9) &&
												demo::Demo3DHostNodeParent(c9) == en) {
												NkHierNodeName(st, c9, sKidName[nk],
															   sizeof(sKidName[0]));
												sKidPtr[nk] = sKidName[nk];
												sKidNode[nk] = c9;
												++nk;
											}
										// LE COMPTE dans le libelle, LA LISTE a cote (Rihen) :
										// on sait d'un coup d'oeil combien il y en a, et
										// lesquels si on ouvre.
										char klab[24];
										snprintf(klab, sizeof(klab), "Enfants (%d)", nk);
										p.TextV(iR.x, yy, kRowH, klab, NkRole::TextMuted);
										const float32 delW = nk ? S(24.f) : 0.f;
										const NkRect vb{valX, yy + S(3.f), valW - delW,
														kRowH - S(6.f)};
										if (nk == 0) {
											p.Outline(vb, NkRole::Border, NkRole::InputBg, 2.f);
											p.TextV(vb.x + S(6.f), yy, kRowH, "Aucun",
													NkRole::TextMuted);
										} else {
											// Changer d'objet remet la liste sur son
											// premier enfant : garder l'ancien indice
											// designerait un enfant qui n'est plus la.
											if (sKidOwner != en) {
												sKidOwner = en;
												sKidSel = 0;
											}
											if (sKidSel < 0 || sKidSel >= nk)
												sKidSel = nk - 1;
											Combo(p, hit, ws, "prop.rel.kids", vb, sKidPtr,
												  nullptr, nk, sKidSel, combo);
											// RETIRER l'enfant choisi : detacher se fait
											// depuis la ou on le designe (Rihen).
											const NkRect db{vb.x + vb.w + S(4.f), vb.y,
															S(20.f), vb.h};
											const bool ovD = hit.Add("prop.rel.kdel", db);
											p.Outline(db, ovD ? NkRole::AccentUi : NkRole::Border,
													  NkRole::PanelHeader, 3.f);
											p.IconV(db.x + (db.w - S(11.f)) * 0.5f, db.y, db.h,
													NkIcon::MinusCircle, NkRole::TextMuted, 11.f);
											if (hit.Clicked("prop.rel.kdel") && sKidSel < nk) {
												demo::Demo3DHostSetNodeParent(sKidNode[sKidSel], -1);
												NkMarkDirty(st);
											}
										}
									}
									yy += kRowH;
									// AJOUTER UN ENFANT depuis ce panneau (Rihen) : la
									// liste propose les objets LIBRES du document -- ceux
									// qui n'ont pas encore de parent et qui ne sont pas
									// dans la descendance de celui-ci.
									{
										static char sAddName[24][24];
										static const char *sAddPtr[24];
										static int32 sAddNode[24];
										static int32 sAddSel = 0;
										// « Aucun » EN TETE ET PAR DEFAUT (Rihen) : tant qu'on
										// n'a designe personne, le bouton n'ajoute rien --
										// sinon un clic distrait rattache le premier venu.
										int32 na = 0;
										snprintf(sAddName[0], sizeof(sAddName[0]), "Aucun");
										sAddPtr[0] = sAddName[0];
										sAddNode[0] = -1;
										na = 1;
										const int32 ncA = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncA && na < 24; ++c9) {
											if (c9 == en || NkHierNodeSkip(c9))
												continue;
											if (demo::Demo3DHostNodeParent(c9) == en)
												continue; // deja enfant
											if (NkHierIsDescendant(en, c9))
												continue; // interdit : cycle
											NkHierNodeName(st, c9, sAddName[na],
														   sizeof(sAddName[0]));
											sAddPtr[na] = sAddName[na];
											sAddNode[na] = c9;
											++na;
										}
										// PAS DE LIBELLE « Ajouter » (Rihen) : le libelle
										// « Enfants » couvre deja la liste ET l'ajout. Cette
										// seconde ligne s'aligne simplement sous la
										// premiere, ce qui la rattache visiblement a elle.
										{
											if (sAddSel < 0 || sAddSel >= na)
												sAddSel = 0;
											// La liste, PUIS la pipette, PUIS le plus : on
											// designe l'objet du regard ou du doigt, et on
											// valide toujours par le meme bouton (Rihen).
											const bool pkC = (st.relPick == 2 && st.relPickFor == en);
											const NkRect ab{valX, yy + S(3.f), valW - S(48.f),
															kRowH - S(6.f)};
											Combo(p, hit, ws, "prop.rel.addk", ab, sAddPtr,
												  nullptr, na, sAddSel, combo);
											{
												const NkRect eb{ab.x + ab.w + S(4.f), ab.y, S(20.f),
																ab.h};
												const bool ovE = hit.Add("prop.rel.addpick", eb);
												p.Outline(eb,
														  (pkC || ovE) ? NkRole::AccentUi
																	   : NkRole::Border,
														  pkC ? NkRole::AccentUi
															  : NkRole::PanelHeader,
														  3.f);
												p.IconV(eb.x + (eb.w - S(11.f)) * 0.5f, eb.y, eb.h,
														NkIcon::Pipette,
														pkC ? NkRole::TextOnAccent
															: NkRole::TextMuted,
														11.f);
												if (hit.Clicked("prop.rel.addpick")) {
													st.relPick = pkC ? 0 : 2;
													st.relPickFor = en;
													st.relPickPrev = st.activeEmpty;
												}
											}
											// Le bouton ne fait rien tant que le choix est
											// « Aucun » : il s'affiche eteint pour le dire,
											// et BLANC des qu'il peut agir.
											const bool canAdd =
												sAddSel > 0 && sAddSel < na && sAddNode[sAddSel] >= 0;
											const NkRect pb{ab.x + ab.w + S(28.f), ab.y, S(20.f),
															ab.h};
											const bool ovP = hit.Add("prop.rel.addb", pb);
											p.Outline(pb,
													  (canAdd && ovP) ? NkRole::AccentUi
																	  : NkRole::Border,
													  NkRole::PanelHeader, 3.f);
											p.IconV(pb.x + (pb.w - S(11.f)) * 0.5f, pb.y, pb.h,
													NkIcon::PlusCircle,
													canAdd ? NkRole::Text : NkRole::TextMuted,
													11.f);
											if (canAdd && hit.Clicked("prop.rel.addb")) {
												demo::Demo3DHostSetNodeParent(sAddNode[sAddSel],
																			  en);
												NkMarkDirty(st);
												sAddSel = 0; // on revient a « Aucun »
											}
										}
										yy += kRowH;
									}
									// TOUS LES ENFANTS D'UN COUP (Rihen) : detacher un a
									// un devient vite penible des qu'ils sont nombreux.
									if (demo::Demo3DHostNodeHasChildren(en)) {
										if (Button("prop.rel.kallout", yy,
												   "Detacher tous les enfants", iR.x, iR.w)) {
											const int32 ncD = demo::Demo3DHostNodeCount();
											for (int32 c9 = 0; c9 < ncD; ++c9)
												if (!NkHierNodeSkip(c9) &&
													demo::Demo3DHostNodeParent(c9) == en) {
													demo::Demo3DHostSetNodeParent(c9, -1);
													NkMarkDirty(st);
												}
										}
										yy += kRowH;
									}
									// CE QUE LE PARENT TRANSMET (apport de Blender) :
									// position, rotation, echelle, chacune coupable.
									// La ligne travaille dans le rectangle INTERIEUR du
									// groupe, sinon ses boutons sortaient du cadre.
									if (demo::Demo3DHostNodeHasChildren(en))
										NkXmitRow(p, hit, iR, iR, yy, en);
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpRelTop, yy);
								}
								yy += NkPropGroupGap();
							}
							bool diffE = false;
							for (int32 a = 0; a < 3; ++a)
								if (st.pos[a] != sE[a] || st.rot[a] != sE[3 + a] ||
									st.scl[a] != sE[6 + a])
									diffE = true;
							if (diffE) {
								demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
								NkMarkDirty(st);
								for (int32 a = 0; a < 3; ++a) {
									sE[a] = st.pos[a];
									sE[3 + a] = st.rot[a];
									sE[6 + a] = st.scl[a];
								}
							}
							// ── GROUPE « MATERIAUX » ────────────────────────────
							// Structure de la maquette et de Blender : l'EMPLACEMENT
							// (son nom, sa pastille de couleur, ses commandes), puis
							// la SURFACE avec ses parametres.
							//
							// On n'expose QUE ce que notre moteur rend vraiment :
							// couleur de base, metallique, rugosite. Le reste du
							// Principled BSDF (emission, occlusion, vernis, diffusion,
							// duvet, anisotropie) existe dans NkPBRParams mais n'a pas
							// encore de surcharge par objet -- l'afficher sans effet
							// serait mentir sur ce que l'outil sait faire.
							// (Le groupe « Materiaux » a DEMENAGE dans la pastille
							// MATERIAU du panneau : bibliotheque du projet, apercus
							// et assignation -- regle de Rihen, un seul endroit.)
							// ── GROUPE « LUMIERE » ──────────────────────────────
							// Structure de Blender (captures de Rihen) : le TYPE en
							// tete, puis couleur et puissance, puis ce qui appartient
							// au type choisi -- portee, cones du spot, taille de
							// l'area -- et enfin l'ombre.
							//
							// Notre moteur n'a ni temperature de couleur, ni
							// exposition, ni normalisation, ni reglages d'influence
							// par canal : ces lignes de Blender ne sont donc PAS
							// reprises, plutot que d'etre affichees sans effet.
							const bool grpLit =
								(ukE == 5) ? PaintPropGroup(p, hit, st, rowR, yy, "prop.g.lit",
															"Lumiere", 16u)
										   : false;
							const float32 grpLitTop = yy;
							if (ukE == 5 && grpLit) {
								const NkRect iR = NkGroupInner(rowR);
								const float32 lvX = iR.x + S(96.f);
								const float32 lvW = iR.w - S(96.f);
								yy += NkGroupPad();
								// LE TYPE, en quatre boutons comme Blender : on voit
								// d'un coup lequel est actif et on en change sans
								// ouvrir de liste.
								{
									static const char *const kLT[4] = {"Soleil", "Point",
																	   "Spot", "Area"};
									static const NkIcon kLI[4] = {NkIcon::Sun, NkIcon::Light,
																  NkIcon::Light, NkIcon::Square};
									const int32 cur = demo::Demo3DHostUserSub(en) & 3;
									const float32 g5 = S(3.f);
									const float32 bw5 = (iR.w - g5 * 3.f) * 0.25f;
									float32 bx5 = iR.x;
									char k5[32];
									for (int32 t5 = 0; t5 < 4; ++t5) {
										snprintf(k5, sizeof(k5), "prop.lit.t%d", t5);
										const NkRect br5{bx5, yy + S(2.f), bw5, kRowH - S(4.f)};
										const bool ov5 = hit.Add(k5, br5);
										const bool on5 = (t5 == cur);
										p.Outline(br5,
												  (on5 || ov5) ? NkRole::AccentUi
															   : NkRole::Border,
												  on5 ? NkRole::AccentUi : NkRole::PanelHeader,
												  3.f);
										p.IconV(br5.x + S(4.f), br5.y, br5.h, kLI[t5],
												on5 ? NkRole::TextOnAccent : NkRole::TextMuted,
												11.f);
										p.TextV(br5.x + S(20.f), yy, kRowH, kLT[t5],
												on5 ? NkRole::TextOnAccent : NkRole::Text);
										if (hit.Clicked(k5) && !on5) {
											demo::Demo3DHostSetUserSub(en, t5);
											NkMarkDirty(st);
										}
										bx5 += bw5 + g5;
									}
									yy += kRowH + S(2.f);
								}
								float32 ulc[3], uli = 1.f;
								if (demo::Demo3DHostUserLightParams(en, ulc, &uli)) {
									bool ulch = false;
									const float32 ulc0[3] = {ulc[0], ulc[1], ulc[2]};
									// LA COULEUR SE CHOISIT A L'OEIL : la nuance ouvre le
									// vrai picker (carre saturation/valeur + teinte), les
									// champs chiffres restent dessous pour la precision.
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Couleur",
														"prop.ulcol", ulc, &ulch);
									yy += NkGroupPad();
									p.TextV(iR.x, yy, kRowH, "Puissance", NkRole::TextMuted);
									ulch |= DragFloat(p, hit, ws, in, "prop.ulint",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  uli, 0.05f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									// TEMPERATURE ET EXPOSITION (Rihen) : elles vivent
									// desormais DANS LE MOTEUR. 0 K = temperature
									// desactivee, 0 stop = exposition neutre : tant
									// qu'on n'y touche pas, rien ne change.
									{
										float32 tK = 0.f, ex = 0.f;
										if (demo::Demo3DHostLightTempExp(en, &tK, &ex)) {
											const float32 tK0 = tK, ex0 = ex;
											p.TextV(iR.x, yy, kRowH, "Temperature",
													NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.ultemp",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  tK, 25.f, NkRole::AccentUi, "%.0f K");
											yy += kRowH;
											p.TextV(iR.x, yy, kRowH, "Exposition",
													NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.ulexp",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  ex, 0.05f, NkRole::AccentUi, "%.2f");
											yy += kRowH;
											if (tK != tK0 || ex != ex0) {
												demo::Demo3DHostSetLightTempExp(en, tK, ex);
												NkMarkDirty(st);
											}
										}
									}
									if (ulch || ulc[0] != ulc0[0] || ulc[1] != ulc0[1] ||
										ulc[2] != ulc0[2]) {
										demo::Demo3DHostSetUserLightParams(en, ulc, uli);
										NkMarkDirty(st);
									}
								}
								{
									// PROPRIETES NATIVES par type : portee, cones du spot, dimensions
									// de l'area, ombres -- visibles ICI et a la creation (Rihen).
									float32 rgL, inL, outL, awL, ahL;
									bool shL = true;
									int32 tyL = -1;
									if (demo::Demo3DHostLightEx(en, &rgL, &inL, &outL, &awL, &ahL, &shL,
																&tyL)) {
										const float32 v0[5] = {rgL, inL, outL, awL, ahL};
										const bool s0 = shL;
										auto LRow = [&](const char *lbl, const char *k4, float32 &val,
														float32 stp, const char *fm) {
											// Les lignes travaillent dans le rectangle
											// INTERIEUR du groupe, comme partout ailleurs.
											p.TextV(iR.x, yy, kRowH, lbl, NkRole::TextMuted);
											DragFloat(p, hit, ws, in, k4,
													  {lvX, yy + S(3.f), lvW, kRowH - S(4.f)},
													  val, stp, NkRole::AccentUi, fm);
											yy += kRowH;
										};
										if (tyL != 0)
											LRow("Portee", "prop.ulex.rg", rgL, 0.05f, "%.2f");
										// LOI D'ATTENUATION (Rihen, 9 aout) : au choix PAR
										// lumiere, comme Unreal. « Physique » = 1/d^2 fenetre —
										// la portee ne fait que couper, l'intensite devient
										// comparable a Blender. « Heritee » = l'ancienne loi,
										// defaut des scenes existantes. Pas pour la
										// directionnelle : elle n'a pas d'attenuation.
										if (tyL != 0) {
											// LE POPUP D'UN COMBO ECRIT EN FIN DE FRAME, PAR
											// POINTEUR (pending.selected). Une LOCALE de pile
											// est morte a ce moment-la : le choix partait dans
											// de la pile invalide et la « Loi » semblait
											// verrouillee (constate par Rihen). Stockage
											// STATIQUE, re-synchronise du hote — et la POUSSEE
											// du choix se fait AVANT la resynchro, sinon elle
											// l'ecraserait.
											static int32 sAttSel = 0;
											static int32 sAttFor = -1;
											const int32 am0 = demo::Demo3DHostLightAttMode(en);
											if (sAttFor == en && sAttSel != am0) {
												demo::Demo3DHostSetLightAttMode(en, sAttSel);
												NkMarkDirty(st);
											} else {
												sAttSel = am0;
											}
											sAttFor = en;
											static const char *const kAtt[2] = {
												"Heritee (portee = niveau)",
												"Physique (1/d2, portee = coupure)"};
											p.TextV(iR.x, yy, kRowH, "Loi", NkRole::TextMuted);
											Combo(p, hit, ws, "prop.ulex.att",
												  {lvX, yy + S(2.f), lvW, kRowH - S(4.f)}, kAtt,
												  nullptr, 2, sAttSel, combo);
											yy += kRowH;
										}
										if (tyL == 2) {
											LRow("Cone interne", "prop.ulex.ci", inL, 0.2f, "%.1f");
											LRow("Cone externe", "prop.ulex.co", outL, 0.2f, "%.1f");
										}
										if (tyL == 3) {
											LRow("Largeur", "prop.ulex.aw", awL, 0.01f, "%.2f");
											LRow("Hauteur", "prop.ulex.ah", ahL, 0.01f, "%.2f");
										}
										{
											const NkRect cb2{iR.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add("prop.ulex.sh", cb2);
											p.Outline(cb2, shL ? NkRole::AccentUi : NkRole::Border,
													  shL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb2.x + S(18.f), yy, kRowH, "Ombres portees",
													NkRole::TextMuted);
											if (hit.Clicked("prop.ulex.sh"))
												shL = !shL;
											yy += kRowH;
										}
										// PROFONDEUR LINEAIRE (omni seulement, Rihen — option
										// LearnOpenGL) : l'atlas recoit dist/portee au lieu de
										// la profondeur projetee -> biais constant, coutures de
										// faces effacees. Cable DIRECTEMENT a l'hote, comme la
										// Loi : c'est un choix, pas un reglage de portee.
										if (tyL == 1) {
											const bool ln0 = demo::Demo3DHostLightShadowLinear(en);
											bool lnL = ln0;
											const NkRect cb3{iR.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add("prop.ulex.shlin", cb3);
											p.Outline(cb3, lnL ? NkRole::AccentUi : NkRole::Border,
													  lnL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb3.x + S(18.f), yy, kRowH,
													"Profondeur lineaire", NkRole::TextMuted);
											if (hit.Clicked("prop.ulex.shlin"))
												lnL = !lnL;
											yy += kRowH;
											if (lnL != ln0) {
												demo::Demo3DHostSetLightShadowLinear(en, lnL);
												NkMarkDirty(st);
											}
										}
										if (rgL != v0[0] || inL != v0[1] || outL != v0[2] || awL != v0[3] ||
											ahL != v0[4] || shL != s0) {
											demo::Demo3DHostSetLightEx(en, rgL, inL, outL, awL, ahL, shL);
											NkMarkDirty(st);
										}
									}
								}
								// COULEUR / TEXTURE / MIX (Rihen) : par defaut couleur pure ;
								// Texture = cookie de l'atlas (couleur forcee au blanc) ; Mix =
								// texture x couleur libre. Deux textures melangees : avec le
								// chargement de fichiers ; le nodal NKGraphe ensuite.
								{
									const int32 ck0 = demo::Demo3DHostLightCookie(en);
									const bool whiteC = [&] {
										float32 c4[3];
										float32 i4 = 1.f;
										return demo::Demo3DHostUserLightParams(en, c4, &i4) &&
											   c4[0] > 0.99f && c4[1] > 0.99f && c4[2] > 0.99f;
									}();
									const int32 derivedM = ck0 < 0 ? 0 : (whiteC ? 1 : 2);
									if (st.lightSrcNode != en) {
										st.lightSrcNode = en;
										st.lightSrcUi = derivedM;
									}
									static const char *const kCMix[3] = {"Couleur", "Texture", "Mix"};
									// DANS le groupe : cette ligne se calait encore sur le
									// panneau entier et debordait donc du cadre (Rihen).
									p.TextV(iR.x, yy, kRowH, "Source", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.ulex.mode",
										  {lvX, yy + S(2.f), lvW, kRowH - S(4.f)},
										  kCMix, nullptr, 3, st.lightSrcUi, combo);
									yy += kRowH;
									if (st.lightSrcUi != derivedM) {
										if (st.lightSrcUi == 0) {
											demo::Demo3DHostSetLightCookie(en, -1);
											NkMarkDirty(st);
										} else {
											demo::Demo3DHostSetLightCookie(en, ck0 < 0 ? 0 : ck0);
											NkMarkDirty(st);
											if (st.lightSrcUi == 1) {
												const float32 wc[3] = {1.f, 1.f, 1.f};
												float32 c5[3];
								float32 i5 = 1.f;
								demo::Demo3DHostUserLightParams(en, c5, &i5);
								demo::Demo3DHostSetUserLightParams(en, wc, i5);
								NkMarkDirty(st);
											}
										}
									}
									if (st.lightSrcUi > 0) {
										float32 slot = (float32)(ck0 < 0 ? 0 : ck0);
										p.TextV(iR.x, yy, kRowH, "Texture (atlas)", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.ulex.slot",
												  {lvX, yy + S(3.f), lvW, kRowH - S(4.f)},
												  slot, 0.05f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
										if ((int32)(slot + 0.5f) != ck0) {
											demo::Demo3DHostSetLightCookie(en, (int32)(slot + 0.5f));
											NkMarkDirty(st);
										}
									}
								}
								yy += NkGroupPad();
								PaintGroupBlock(p, rowR, grpLitTop, yy);
							}
							if (ukE == 5)
								yy += NkPropGroupGap();
							{
								// CAMERA : ses proprietes propres (declaratives tant que
								// le rendu au travers de la camera n'est pas branche).
								float32 cf = 50.f, cnr = 0.1f, cfr = 100.f;
								if (demo::Demo3DHostCameraParams(en, &cf, &cnr, &cfr)) {
									// UN GROUPE A PART ENTIERE, repliable, comme
									// Transformation et Relations (Rihen) -- plus une
									// simple etiquette suivie de rangees flottantes.
									const bool grpCam = PaintPropGroup(p, hit, st, rowR, yy,
																	   "prop.g.cam", "Camera", 3u);
									const float32 grpCamTop = yy;
									if (grpCam) {
									yy += NkGroupPad();
									// MARGES DU GROUPE : les rangees ci-dessous sont
									// ecrites en coordonnees de PANNEAU (r.x + kPad,
									// rr.w - 128...). On SUBSTITUE localement r et rr
									// par des rects derives de l'INTERIEUR du groupe :
									// memes formules, et les labels tombent a iCam.x,
									// les champs s'arretent a iCam.x + iCam.w -- les
									// marges des autres groupes, sans reecrire chaque
									// rangee (les rangees flottantes debordaient du
									// cadre, constate par Rihen).
									const NkRect iCam = NkGroupInner(rowR);
									const NkRect r{iCam.x - kPad, iCam.y, iCam.w, iCam.h};
									const NkRect rr{iCam.x, iCam.y, iCam.w + kPad + S(8.f),
													iCam.h};
									const float32 c0[3] = {cf, cnr, cfr};
									// TYPE (Rihen) : perspective ou orthographique. En
									// ortho, l'ECHELLE Y du noeud regle la demi-hauteur
									// du cadre (regle consignee).
									const bool isO = demo::Demo3DHostCamOrtho(en);
									{
										p.TextV(r.x + kPad, yy, kRowH, "Type", NkRole::TextMuted);
										const float32 bw2 = (rr.w - S(128.f)) * 0.5f - S(2.f);
										const NkRect bp{r.x + S(120.f), yy + S(2.f), bw2,
														kRowH - S(4.f)};
										const NkRect bo{bp.x + bw2 + S(4.f), yy + S(2.f), bw2,
														kRowH - S(4.f)};
										hit.Add("prop.cam.persp", bp);
										hit.Add("prop.cam.ortho", bo);
										p.Fill(bp, !isO ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.Fill(bo, isO ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.TextV(bp.x + (bp.w - p.TextW("Perspective")) * 0.5f, yy,
												kRowH, "Perspective",
												!isO ? NkRole::TextOnAccent : NkRole::TextMuted);
										p.TextV(bo.x + (bo.w - p.TextW("Ortho")) * 0.5f, yy, kRowH,
												"Ortho",
												isO ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked("prop.cam.persp")) {
											demo::Demo3DHostSetCamOrtho(en, false);
											NkMarkDirty(st);
										}
										if (hit.Clicked("prop.cam.ortho")) {
											demo::Demo3DHostSetCamOrtho(en, true);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									// PROPRIETES PAR TYPE (Rihen) : la focale n'a pas de
									// sens en ortho (rayons paralleles) -- elle cede la
									// place a l'ECHELLE ORTHO (demi-hauteur du cadre =
									// echelle du noeud). Clips et passe-partout restent
									// communs.
									if (isO) {
										float32 osc = demo::Demo3DHostCamOrthoScale(en);
										const float32 os0 = osc;
										p.TextV(r.x + kPad, yy, kRowH, "Echelle ortho",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.cortho",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  osc, 0.02f, NkRole::AccentUi, "%.2f");
										if (osc != os0) {
											demo::Demo3DHostSetCamOrthoScale(en, osc);
											NkMarkDirty(st);
										}
										yy += kRowH;
									} else {
									// UNITE DE FOCALE (parite Blender) : degres ou
									// millimetres -- meme grandeur, la conversion passe
									// par le CAPTEUR. La focale ANGULAIRE reste la seule
									// verite cote hote.
									const bool inMM = demo::Demo3DHostCamLensMM(en);
									{
										p.TextV(r.x + kPad, yy, kRowH, "Unite focale",
												NkRole::TextMuted);
										const float32 bw3 = (rr.w - S(128.f)) * 0.5f - S(2.f);
										const NkRect bd{r.x + S(120.f), yy + S(2.f), bw3,
														kRowH - S(4.f)};
										const NkRect bm{bd.x + bw3 + S(4.f), yy + S(2.f), bw3,
														kRowH - S(4.f)};
										hit.Add("prop.cam.udeg", bd);
										hit.Add("prop.cam.umm", bm);
										p.Fill(bd, !inMM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.Fill(bm, inMM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.TextV(bd.x + (bd.w - p.TextW("Degres")) * 0.5f, yy,
												kRowH, "Degres",
												!inMM ? NkRole::TextOnAccent : NkRole::TextMuted);
										p.TextV(bm.x + (bm.w - p.TextW("mm")) * 0.5f, yy, kRowH,
												"mm",
												inMM ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked("prop.cam.udeg")) {
											demo::Demo3DHostSetCamLensMM(en, false);
											NkMarkDirty(st);
										}
										if (hit.Clicked("prop.cam.umm")) {
											demo::Demo3DHostSetCamLensMM(en, true);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									if (inMM) {
										float32 mmv = demo::Demo3DHostCamFocalMM(en);
										const float32 mm0 = mmv;
										p.TextV(r.x + kPad, yy, kRowH, "Focale (mm)",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.cfmm",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  mmv, 0.5f, NkRole::AccentUi, "%.1f");
										yy += kRowH;
										if (mmv != mm0) {
											demo::Demo3DHostSetCamFocalMM(en, mmv);
											NkMarkDirty(st);
										}
										float32 sen = demo::Demo3DHostCamSensor(en);
										const float32 se0 = sen;
										p.TextV(r.x + kPad, yy, kRowH, "Capteur (mm)",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.csen",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  sen, 0.2f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
										if (sen != se0) {
											demo::Demo3DHostSetCamSensor(en, sen);
											NkMarkDirty(st);
										}
									} else {
									p.TextV(r.x + kPad, yy, kRowH, "Focale (deg)",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cfov",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cf, 0.2f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
									}
									}
									p.TextV(r.x + kPad, yy, kRowH, "Clip debut",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cnear",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cnr, 0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									p.TextV(r.x + kPad, yy, kRowH, "Clip fin",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cfar",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cfr, 0.5f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
									if (cf != c0[0] || cnr != c0[1] || cfr != c0[2]) {
										demo::Demo3DHostSetCameraParams(en, cf, cnr, cfr);
										NkMarkDirty(st);
									}
									// ── PASSE-PARTOUT (Rihen) : couleur + opacite
									// du voile hors cadre quand on regarde par
									// cette camera. Noir a 60 % par defaut.
									{
										float32 ppc[4];
										demo::Demo3DHostCamPasse(en, ppc);
										const float32 pa0 = ppc[3];
										bool ppCh = false;
										const NkRect iC{r.x + kPad, 0.f, rr.w - kPad * 2.f, 0.f};
										yy += PaintColorRow(p, hit, ws, in, st, iC, yy,
															"Passe-partout", "prop.campp", ppc,
															&ppCh);
										p.TextV(r.x + kPad, yy, kRowH, "Opacite voile",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.camppa",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  ppc[3], 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										if (ppc[3] < 0.f)
											ppc[3] = 0.f;
										if (ppc[3] > 1.f)
											ppc[3] = 1.f;
										if (ppCh || ppc[3] != pa0) {
											demo::Demo3DHostSetCamPasse(en, ppc);
											NkMarkDirty(st);
										}
									}
									// ── GUIDES DE COMPOSITION + ZONES SURES (Rihen,
									// parite Blender) : traces dans le cadre en vue
									// camera. Bits : 1 tiers, 2 centre, 4 diagonales,
									// 8 nombre d'or, 16 zones sures.
									{
										const int32 gd = demo::Demo3DHostCamGuides(en);
										static const char *const kGd[5] = {
											"Tiers", "Centre", "Diagonales", "Nombre d'or",
											"Zones sures"};
										for (int32 g5 = 0; g5 < 5; ++g5) {
											const int32 bit = 1 << g5;
											const NkRect cbG{r.x + kPad, yy + S(5.f), S(12.f),
															 S(12.f)};
											char gk[24];
											snprintf(gk, sizeof(gk), "prop.cam.gd%d", g5);
											hit.Add(gk, cbG);
											const bool onG = (gd & bit) != 0;
											p.Outline(cbG, onG ? NkRole::AccentUi : NkRole::Border,
													  onG ? NkRole::AccentUi : NkRole::InputBg,
													  2.f);
											p.TextV(cbG.x + S(18.f), yy, kRowH, kGd[g5],
													NkRole::TextMuted);
											if (hit.Clicked(gk)) {
												demo::Demo3DHostSetCamGuides(en, gd ^ bit);
												NkMarkDirty(st);
											}
											yy += kRowH;
										}
									}
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpCamTop, yy);
									} // fin du groupe Camera
									yy += NkPropGroupGap();
								}
							}
							// (« Transmettre » a rejoint le groupe RELATIONS : le
							// laisser aussi ici l'affichait DEUX FOIS -- constate par
							// Rihen sur sa capture.)
						}
					} else if (li >= 0) {
						// UNE LUMIERE A SES PROPRIETES comme un maillage (Rihen) --
						// les vides et cameras suivront avec le modele objet.
						demo::Demo3DHostLightName(li, buf, sizeof(buf));
						p.IconV(r.x + kPad, yy, kRowH, NkIcon::Light, NkRole::Text, 13.f);
						p.TextV(r.x + kPad + S(18.f), yy, kRowH, buf);
						yy += kRowH;
						float32 lpos[3];
						demo::Demo3DHostLightPosition(li, lpos);
						NkRect rowR = rr;
						rowR.x = r.x + kPad; // meme marge des deux cotes
						rowR.w = rr.w - 2.f * kPad;
						PaintTransformRow(p, hit, ws, in, rowR, yy, "Position", lpos, 0.01f,
										  "prop.lpos", NkIcon::None, NkIcon::None);
						static float32 sLPull[3] = {};
						static int32 sLLast = -1;
						if (sLLast != li || !(ws.dragging || ws.editing)) {
							// tirer : la lumiere peut bouger par son widget dans la vue
						}
						if (lpos[0] != sLPull[0] || lpos[1] != sLPull[1] || lpos[2] != sLPull[2]) {
							if (sLLast == li) {
								demo::Demo3DHostSetLightPosition(li, lpos);
								NkMarkDirty(st);
							}
							sLPull[0] = lpos[0];
							sLPull[1] = lpos[1];
							sLPull[2] = lpos[2];
							sLLast = li;
						}
						yy += Vec3RowH();
						// ROTATION (direction du faisceau) : soleil, projecteur,
						// surfacique -- une ponctuelle n'a pas d'orientation.
						if (demo::Demo3DHostLightType(li) != 1) {
							static float32 sLE[4][3];
							static bool sLEInit[4] = {};
							float32 ld[3];
							demo::Demo3DHostLightDir(li, ld);
							if (li >= 0 && li < 4 && !sLEInit[li]) {
								// angles retrouves depuis la direction (rx elevation,
								// rz azimut ; ry libre, sans effet visuel)
								float32 dz = ld[2] < -1.f ? -1.f : (ld[2] > 1.f ? 1.f : ld[2]);
								sLE[li][0] = asinf(-dz) * 57.29578f;
								sLE[li][1] = 0.f;
								sLE[li][2] = atan2f(ld[0], -ld[1]) * 57.29578f;
								sLEInit[li] = true;
							}
							const float32 le0[3] = {sLE[li][0], sLE[li][1], sLE[li][2]};
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation",
											  sLE[li], 0.5f, "prop.lrot", NkIcon::None,
											  NkIcon::None, "%.1f");
							yy += Vec3RowH();
							if (sLE[li][0] != le0[0] || sLE[li][1] != le0[1] ||
								sLE[li][2] != le0[2]) {
								const float32 k2r = 0.017453292f;
								const float32 cxr = cosf(sLE[li][0] * k2r);
								const float32 sxr = sinf(sLE[li][0] * k2r);
								const float32 czr = cosf(sLE[li][2] * k2r);
								const float32 szr = sinf(sLE[li][2] * k2r);
								// direction = Rz(azimut) * Rx(elevation) . (0,-1,0)
								const float32 nd[3] = {cxr * szr, -cxr * czr, -sxr};
								demo::Demo3DHostSetLightDir(li, nd);
								NkMarkDirty(st);
							}
						}
						float32 lcol[3], lint = 1.f;
						demo::Demo3DHostLightParams(li, lcol, &lint);
						bool lch = false;
						p.TextV(r.x + kPad, yy, kRowH, "Intensite", NkRole::TextMuted);
						lch |= DragFloat(p, hit, ws, in, "prop.lint",
										 {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
										 lint, 0.05f, NkRole::AccentUi, "%.2f");
						yy += kRowH;
						{
							// LA NUANCE, pas trois nombres : le picker se deplie sous
							// elle (Rihen). La lumiere suit en direct, le bloc qui suit
							// detecte le changement comme avant.
							NkRect cR = rowR;
							cR.x = r.x + kPad;
							cR.w = rr.w - S(16.f);
							bool cCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, cR, yy, "Couleur",
												"prop.lcol", lcol, &cCh);
							lch |= cCh;
						}
						static float32 sLC[4] = {-1.f, 0.f, 0.f, 0.f};
						if (lch || lcol[0] != sLC[1] || lcol[1] != sLC[2] || lcol[2] != sLC[3]) {
							if ((int32)sLC[0] == li) {
								for (int32 a = 0; a < 3; ++a)
									if (lcol[a] < 0.f)
										lcol[a] = 0.f;
								demo::Demo3DHostSetLightParams(li, lcol, lint < 0.f ? 0.f : lint);
								NkMarkDirty(st);
								// PROPAGER coche : memes reglages pour les lumieres
								// DESCENDANTES de celle-ci (propriete commune).
								if (st.matPropagate)
									for (int32 l2 = 0; l2 < demo::Demo3DHostLightCount(); ++l2)
										if (l2 != li && NkHierIsDescendant(86 + l2, 86 + li)) {
											demo::Demo3DHostSetLightParams(l2, lcol,
																		   lint < 0.f ? 0.f : lint);
											NkMarkDirty(st);
										}
							}
							sLC[0] = (float32)li;
							sLC[1] = lcol[0];
							sLC[2] = lcol[1];
							sLC[3] = lcol[2];
						}
						{
							// PROPRIETES NATIVES par type : portee, cones du spot, dimensions
							// de l'area, ombres -- visibles ICI et a la creation (Rihen).
							float32 rgL, inL, outL, awL, ahL;
							bool shL = true;
							int32 tyL = -1;
							if (demo::Demo3DHostLightEx(86 + li, &rgL, &inL, &outL, &awL, &ahL, &shL,
														&tyL)) {
								const float32 v0[5] = {rgL, inL, outL, awL, ahL};
								const bool s0 = shL;
								auto LRow = [&](const char *lbl, const char *k4, float32 &val,
												float32 stp, const char *fm) {
									p.TextV(r.x + kPad, yy, kRowH, lbl, NkRole::TextMuted);
									DragFloat(p, hit, ws, in, k4,
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
											  val, stp, NkRole::AccentUi, fm);
									yy += kRowH;
								};
								if (tyL != 0)
									LRow("Portee", "prop.lex.rg", rgL, 0.05f, "%.2f");
								if (tyL == 2) {
									LRow("Cone interne", "prop.lex.ci", inL, 0.2f, "%.1f");
									LRow("Cone externe", "prop.lex.co", outL, 0.2f, "%.1f");
								}
								if (tyL == 3) {
									LRow("Largeur", "prop.lex.aw", awL, 0.01f, "%.2f");
									LRow("Hauteur", "prop.lex.ah", ahL, 0.01f, "%.2f");
								}
								{
									const NkRect cb2{r.x + kPad, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.lex.sh", cb2);
									p.Outline(cb2, shL ? NkRole::AccentUi : NkRole::Border,
											  shL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb2.x + S(18.f), yy, kRowH, "Ombres portees",
											NkRole::TextMuted);
									if (hit.Clicked("prop.lex.sh"))
										shL = !shL;
									yy += kRowH;
								}
								if (rgL != v0[0] || inL != v0[1] || outL != v0[2] || awL != v0[3] ||
									ahL != v0[4] || shL != s0) {
									demo::Demo3DHostSetLightEx(86 + li, rgL, inL, outL, awL, ahL, shL);
									NkMarkDirty(st);
								}
							}
						}
						// COULEUR / TEXTURE / MIX (Rihen) : par defaut couleur pure ;
						// Texture = cookie de l'atlas (couleur forcee au blanc) ; Mix =
						// texture x couleur libre. Deux textures melangees : avec le
						// chargement de fichiers ; le nodal NKGraphe ensuite.
						{
							const int32 ck0 = demo::Demo3DHostLightCookie(86 + li);
							const bool whiteC = lcol[0] > 0.99f && lcol[1] > 0.99f && lcol[2] > 0.99f;
							const int32 derivedM = ck0 < 0 ? 0 : (whiteC ? 1 : 2);
							if (st.lightSrcNode != 86 + li) {
								st.lightSrcNode = 86 + li;
								st.lightSrcUi = derivedM;
							}
							static const char *const kCMix[3] = {"Couleur", "Texture", "Mix"};
							p.TextV(r.x + kPad, yy, kRowH, "Source", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.lex.mode",
								  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
								  kCMix, nullptr, 3, st.lightSrcUi, combo);
							yy += kRowH;
							if (st.lightSrcUi != derivedM) {
								if (st.lightSrcUi == 0) {
									demo::Demo3DHostSetLightCookie(86 + li, -1);
									NkMarkDirty(st);
								} else {
									demo::Demo3DHostSetLightCookie(86 + li, ck0 < 0 ? 0 : ck0);
									NkMarkDirty(st);
									if (st.lightSrcUi == 1) {
										const float32 wc[3] = {1.f, 1.f, 1.f};
										demo::Demo3DHostSetLightParams(li, wc, lint < 0.f ? 0.f : lint);
										NkMarkDirty(st);
									}
								}
							}
							if (st.lightSrcUi > 0) {
								float32 slot = (float32)(ck0 < 0 ? 0 : ck0);
								p.TextV(r.x + kPad, yy, kRowH, "Texture (atlas)", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.lex.slot",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
										  slot, 0.05f, NkRole::AccentUi, "%.0f");
								yy += kRowH;
								if ((int32)(slot + 0.5f) != ck0) {
									demo::Demo3DHostSetLightCookie(86 + li, (int32)(slot + 0.5f));
									NkMarkDirty(st);
								}
							}
						}
						if (demo::Demo3DHostNodeHasChildren(86 + li)) {
							NkPropagateCheck(p, hit, r, yy, "prop.lprop", st.matPropagate);
							yy += kRowH;
							NkXmitRow(p, hit, r, rr, yy, 86 + li);
						}
					} else if (act >= 0 && demo::Demo3DHostObjectSelected(act)) {
						demo::Demo3DHostObjectName(act, buf, sizeof(buf));
						p.IconV(r.x + kPad, yy, kRowH, NkIcon::Mesh, NkRole::Text, 13.f);
						p.TextV(r.x + kPad + S(18.f), yy, kRowH, buf);
						yy += kRowH;

						// SYNC : TIRER quand on ne glisse pas (le gizmo et la vue
						// restent maitres), POUSSER au changement des champs. Un axe
						// VERROUILLE revient a la valeur tiree ; l'echelle
						// PROPORTIONNELLE propage le rapport de l'axe touche.
						static int32 sLastAct = -1;
						static float32 sPull[9] = {};
						const bool holding = ws.dragging || ws.editing;
						float32 tp[3], tr2[3], ts2[3];
						if (demo::Demo3DHostObjectTransform(act, tp, tr2, ts2)) {
							if (!holding || act != sLastAct) {
								for (int32 a = 0; a < 3; ++a) {
									st.pos[a] = tp[a];
									st.rot[a] = tr2[a];
									st.scl[a] = ts2[a];
									sPull[a] = tp[a];
									sPull[3 + a] = tr2[a];
									sPull[6 + a] = ts2[a];
								}
								sLastAct = act;
							}
							// CENTRE dans le panneau : meme marge a gauche et a droite
							// (kPad, celle des autres panneaux).
							NkRect rowR = rr;
							rowR.x = r.x + kPad;
							rowR.w = rr.w - 2.f * kPad;
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Position", st.pos, 0.01f,
											  "prop.pos", st.lockPos ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propPos);
							if (hit.Clicked("prop.pos.ic0"))
								st.lockPos = !st.lockPos;
							if (hit.Clicked("prop.pos.ic2"))
								st.propPos = !st.propPos;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation", st.rot, 0.5f,
											  "prop.rot", st.lockRot ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.1f", NkIcon::Link2, st.propRot);
							if (hit.Clicked("prop.rot.ic0"))
								st.lockRot = !st.lockRot;
							if (hit.Clicked("prop.rot.ic2"))
								st.propRot = !st.propRot;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Echelle", st.scl, 0.01f,
											  "prop.scl", st.lockScl ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propScale);
							if (hit.Clicked("prop.scl.ic0"))
								st.lockScl = !st.lockScl;
							if (hit.Clicked("prop.scl.ic2"))
								st.propScale = !st.propScale;
							yy += Vec3RowH();
							// DIMENSIONS (unites monde) : echelle x taille de base de
							// la nature ; les EDITER ajuste l'echelle, comme Blender.
							{
								// DIMENSIONS = taille LOCALE, DECOUPLEE de l'echelle
								// (Rihen) : l'editer redimensionne la geometrie au rendu,
								// l'echelle n'en sait rien -- et reciproquement.
								float32 dim[3];
								demo::Demo3DHostNodeBaseSize(act, dim);
								const float32 dim0[3] = {dim[0], dim[1], dim[2]};
								PaintTransformRow(p, hit, ws, in, rowR, yy, "Dimensions", dim,
									  0.01f, "prop.dim",
									  st.lockDim ? NkIcon::Lock : NkIcon::Unlock,
									  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propDim);
					if (hit.Clicked("prop.dim.ic0"))
						st.lockDim = !st.lockDim;
					if (hit.Clicked("prop.dim.ic2"))
						st.propDim = !st.propDim;
					yy += Vec3RowH();
					// PROPORTIONNEL : l'axe touche impose son RAPPORT aux autres ;
					// sinon chaque dimension ne bouge QUE son axe (Rihen).
					if (st.propDim) {
						int32 chD = -1;
						for (int32 a = 0; a < 3; ++a)
							if (dim[a] != dim0[a])
								chD = a;
						if (chD >= 0 && fabsf(dim0[chD]) > 1e-6f) {
							const float32 rd = dim[chD] / dim0[chD];
							for (int32 a = 0; a < 3; ++a)
								if (a != chD)
									dim[a] = dim0[a] * rd;
						}
					}
					if (st.lockDim)
						for (int32 a = 0; a < 3; ++a)
							dim[a] = dim0[a];
					bool dimCh = false;
					for (int32 a = 0; a < 3; ++a)
						if (dim[a] != dim0[a])
							dimCh = true;
					if (dimCh) {
						demo::Demo3DHostSetNodeBaseSize(act, dim);
						NkMarkDirty(st);
					}
							}

							// PROPORTIONNEL (par ligne, 3e icone) : l'axe touche propage
							// son RAPPORT aux autres -- delta simple quand la base est
							// nulle, un rapport n'y aurait pas de sens.
							auto Propagate = [](float32 *vals, const float32 *base, bool on) {
								if (!on)
									return;
								int32 ch = -1;
								for (int32 a = 0; a < 3; ++a)
									if (vals[a] != base[a])
										ch = a;
								if (ch < 0)
									return;
								if (fabsf(base[ch]) > 1e-6f) {
									const float32 ratio = vals[ch] / base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] * ratio;
								} else {
									const float32 d = vals[ch] - base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] + d;
								}
							};
							Propagate(st.pos, sPull, st.propPos);
							Propagate(st.rot, sPull + 3, st.propRot);
							Propagate(st.scl, sPull + 6, st.propScale);
							// Verrous : la ligne revient a l'etat tire.
							for (int32 a = 0; a < 3; ++a) {
								if (st.lockPos)
									st.pos[a] = sPull[a];
								if (st.lockRot)
									st.rot[a] = sPull[3 + a];
								if (st.lockScl)
									st.scl[a] = sPull[6 + a];
							}
							bool diff = false;
							for (int32 a = 0; a < 3; ++a)
								if (fabsf(st.pos[a] - sPull[a]) > 1e-5f ||
									fabsf(st.rot[a] - sPull[3 + a]) > 1e-5f ||
									fabsf(st.scl[a] - sPull[6 + a]) > 1e-5f)
									diff = true;
							if (diff) {
								// La MEME modification pour TOUTE la selection : le delta
								// tape ici se propage aux autres objets selectionnes.
								float32 dP[3], dR[3], rS[3];
								for (int32 a = 0; a < 3; ++a) {
									dP[a] = st.pos[a] - sPull[a];
									dR[a] = st.rot[a] - sPull[3 + a];
									rS[a] = (sPull[6 + a] > 1e-6f) ? st.scl[a] / sPull[6 + a] : 1.f;
								}
								demo::Demo3DHostSetObjectTransform(act, st.pos, st.rot, st.scl);
								NkMarkDirty(st);
								demo::Demo3DHostApplyDeltaToSelection(dP, dR, rS, act);
								for (int32 a = 0; a < 3; ++a) {
									sPull[a] = st.pos[a];
									sPull[3 + a] = st.rot[a];
									sPull[6 + a] = st.scl[a];
								}
							}
						}
						if (demo::Demo3DHostNodeHasChildren(act))
							NkXmitRow(p, hit, r, rr, yy, act);
						// (Le PIVOT a rejoint le groupe Transformation, avec son cadenas
						// et son bouton de reinitialisation comme les autres lignes.)
						// (Le bloc « Materiau » a DEMENAGE dans la pastille MATERIAU
						// du panneau : bibliotheque du projet, apercus et
						// assignation -- regle de Rihen, un seul endroit.)
						int32 nSel = 0;
						const int32 nO = demo::Demo3DHostObjectCount();
						for (int32 i3 = 0; i3 < nO; ++i3)
							if (demo::Demo3DHostObjectSelected(i3))
								++nSel;
						snprintf(buf, sizeof(buf), "%d objet(s) selectionne(s)", nSel);
						p.TextV(r.x + kPad, yy, kRowH, buf, NkRole::TextMuted);
						yy += kRowH;
						if (Button("props.desel", yy, "Tout deselectionner", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostDeselectAll();
						yy += kRowH;
					} else {
						p.TextV(r.x + kPad, yy, kRowH, "Aucun objet selectionne", NkRole::TextMuted);
						yy += kRowH;
					}
				} else if (sec == 1) {
					// ── RENDU : LES OMBRES ──────────────────────────────────────
					// Elles sont GLOBALES au rendu -- une ombre douce l'est pour
					// toute la scene -- donc leur place est ici, et non sur chaque
					// lumiere, qui ne garde que son interrupteur d'ombre (c'est
					// aussi le partage de Blender entre Rendu et Lumiere).
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── ECLAIRAGE D'AMBIANCE ────────────────────────────────────
					// Ce que la scene recoit de son ENVIRONNEMENT, sans aucune
					// source. A zero, un objet hors de toute lumiere est noir --
					// c'est ce qu'on attend d'un rendu, et ce que fait Blender.
					{
						NkRect aR = rr;
						aR.x = r.x + NkPropInset();
						aR.w = rr.w - 2.f * NkPropInset();
						const float32 aTop = yy;
						if (PaintPropGroup(p, hit, st, aR, yy, "prop.g.amb", "Ambiance", 2u)) {
							const NkRect iA = NkGroupInner(aR);
							yy += NkGroupPad();
							p.TextV(iA.x, yy, kRowH, "Intensite", NkRole::TextMuted);
							float32 amb = demo::Demo3DHostAmbient();
							const float32 amb0 = amb;
							DragFloat(p, hit, ws, in, "prop.amb",
									  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
									   kRowH - S(6.f)},
									  amb, 0.005f, NkRole::AccentUi, "%.3f");
							if (amb != amb0) {
								demo::Demo3DHostSetAmbient(amb);
								NkMarkDirty(st);
							}
							yy += kRowH;
							// LA TEINTE de l'ambiance, avec le meme picker que
							// partout ailleurs : blanc = neutre.
							{
								float32 ac[3];
								demo::Demo3DHostAmbientColor(ac);
								bool acCh = false;
								yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
													"prop.ambcol", ac, &acCh);
								if (acCh) {
									demo::Demo3DHostSetAmbientColor(ac);
									NkMarkDirty(st);
								}
							}
						// ── D'OU VIENT L'AMBIANCE ? ─────────────────────────
							// Couleur unie : un aplat, comme le monde par defaut de
							// Blender. Ciel procedural : trois couleurs dont le
							// moteur deduit l'eclairage -- il est donc DIRECTIONNEL,
							// c'est lui qui eclairait trois faces plus que les
							// autres. Image HDRI : une photo 360 qui apporte la
							// lumiere ET les reflets d'un lieu reel.
							{
								static const char *const kSrc[3] = {"Couleur unie",
																	"Ciel procedural",
																	"Image HDRI"};
								p.TextV(iA.x, yy, kRowH, "Source", NkRole::TextMuted);
								// MEMOIRE DU POUSSE, pas de capture dans la frame :
								// DrawComboPopup ecrit st.envSource en FIN d'image,
								// donc « capturer avant / comparer apres » ne voit
								// jamais le changement (meme panne que le modele de
								// ciel). Le moteur ne stocke qu'un booleen pour trois
								// choix : impossible de relire la verite, on memorise
								// donc ce qu'on a reellement pousse.
								static int32 pushedSrc = -1;
								if (pushedSrc < 0)
									pushedSrc = st.envSource;
								Combo(p, hit, ws, "prop.amb.src",
									  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
									   kRowH - S(4.f)},
									  kSrc, nullptr, 3, st.envSource, combo);
								if (st.envSource != pushedSrc) {
									pushedSrc = st.envSource;
									demo::Demo3DHostSetAmbientUseEnv(st.envSource != 0);
									NkMarkDirty(st);
								}
								yy += kRowH;
							}
							// ── LE CIEL SE VOIT-IL ? ────────────────────────────
							// Reglage SEPARE de la source ci-dessus, et il doit le
							// rester : « d'ou vient la lumiere » et « qu'est-ce
							// qu'on voit derriere » sont deux questions distinctes.
							// On eclaire souvent une scene avec un HDRI sans
							// l'afficher en fond, et on affiche parfois un ciel qui
							// ne pilote pas l'ambiance.
							// Le moteur savait deja le faire (NkRender3D::
							// SetSkyboxEnabled, shader Skybox compile au demarrage) ;
							// simplement, AUCUNE ligne de l'application ne le lui
							// demandait -- le ciel ne pouvait donc jamais apparaitre.
							{
								bool skyOn = demo::Demo3DHostSkyVisible();
								const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.amb.skyvis", cb);
								p.Outline(cb, skyOn ? NkRole::AccentUi : NkRole::Border,
										  skyOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Afficher le ciel",
										NkRole::TextMuted);
								if (hit.Clicked("prop.amb.skyvis")) {
									demo::Demo3DHostSetSkyVisible(!skyOn);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// SA LUMINOSITE, encore un reglage a part. Le shader
								// peignait le ciel en le multipliant par l'intensite
								// d'ambiance (0,05) : il sortait quasi noir et
								// paraissait absent alors qu'il etait bien genere.
								// Visible seulement quand le ciel l'est : un curseur
								// sans effet observable n'apprend rien.
								if (skyOn) {
									float32 si = demo::Demo3DHostSkyIntensity();
									const float32 si0 = si;
									p.TextV(iA.x, yy, kRowH, "Luminosite", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.amb.skyint",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si, 0.02f, NkRole::AccentUi, "%.2f");
									if (si != si0) {
										demo::Demo3DHostSetSkyIntensity(si);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								// REINITIALISER L'AMBIANCE seule : intensite,
								// teinte et luminosite du ciel. Ni le modele de
								// ciel ni les nuages ne bougent -- ils ont leurs
								// propres boutons.
								{
									const NkRect rra{iA.x, yy + S(2.f), iA.w, kRowH - S(4.f)};
									hit.Add("prop.amb.reset", rra);
									p.Outline(rra, NkRole::Border, NkRole::InputBg, 3.f);
									p.TextV(rra.x + (rra.w - p.TextW("Ambiance par defaut")) * 0.5f,
											yy, kRowH, "Ambiance par defaut", NkRole::TextMuted);
									if (hit.Clicked("prop.amb.reset"))
										demo::Demo3DHostResetAmbient();
									yy += kRowH;
								}
							}
							if (st.envSource == 1) {
								// ── QUEL MODELE DE CIEL ? ───────────────────
								// Degrade : trois couleurs, stylise, previsible.
								// Physique : la couleur DECOULE de la position du
								// soleil et de la turbidite de l'air -- le bleu du
								// zenith, le blanchiment vers l'horizon et les
								// teintes du couchant sortent du modele, on ne les
								// regle pas.
								{
									static const char *const kSkyM[6] = {
										"Degrade", "Physique (Preetham)",
										"Atmosphere (Rayleigh + Mie)",
										"Hosek-Wilkie (mesure)",
										"Prague (mesure, couchants)",
										"Soleil alien (temperature)"};
									// LA VALEUR VIT DANS L'ETAT, jamais en local : le
									// combo retient un POINTEUR dessus et n'ecrit
									// qu'a la frame suivante. Avec une locale, le
									// choix se perdait en silence et le modele
									// restait bloque sur « Degrade ».
									//
									// ET ON NE COMPARE PAS A UNE VALEUR CAPTUREE
									// DANS LA FRAME. DrawComboPopup ecrit
									// *selected en FIN d'image, apres les panneaux :
									// a la frame suivante, un `const int32 v0 = st.x`
									// pris avant l'appel vaut DEJA la nouvelle
									// valeur, et `st.x != v0` est toujours faux. Le
									// poussage vers le moteur n'a alors jamais lieu.
									// On memorise donc CE QU'ON A REELLEMENT POUSSE.
									static int32 pushedModel = 0;
									p.TextV(iA.x, yy, kRowH, "Modele", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.sky.model",
										  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
										   kRowH - S(4.f)},
										  kSkyM, nullptr, 6, st.skyModel, combo);
									if (st.skyModel != pushedModel) {
										pushedModel = st.skyModel;
										demo::Demo3DHostSetSkyModel(st.skyModel);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								const int32 skyModel = st.skyModel;
								bool ch = false;
								float32 top[3], hor[3], gnd[3];
								demo::Demo3DHostEnvSky(top, hor, gnd);
								// ── SOLEIL ALIEN : la temperature de l'etoile ──
								// C'est LE reglage du modele : le monde entier
								// change de teinte avec elle, pas seulement le
								// disque. Effet visible en continu (modele cuit
								// avec rafraichissement auto) ; l'eclairage
								// attend « Regenerer », comme partout.
								if (skyModel == 5) {
									float32 tk = demo::Demo3DHostSkyAlienTemp();
									const float32 tk0 = tk;
									p.TextV(iA.x, yy, kRowH, "Etoile (K)", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.alientemp",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  tk, 25.f, NkRole::AccentUi, "%.0f");
									if (tk != tk0) {
										demo::Demo3DHostSetSkyAlienTemp(tk);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								if (skyModel == 0) {
									// LES TROIS COULEURS DU CIEL, modifiables.
									yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Zenith",
														"prop.sky.top", top, &ch);
									yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Horizon",
														"prop.sky.hor", hor, &ch);
								} else {
									// ── CIEL PHYSIQUE ──────────────────────
									// LE SOLEIL SE DONNE EN ELEVATION / AZIMUT,
									// pas en vecteur : c'est ainsi qu'on pense un
									// soleil, et c'est ce qui permet de viser un
									// couchant sans calculer de composantes.
									float32 sd[3], turb = 2.5f, si = 1.f;
									bool disc = true;
									demo::Demo3DHostSkySun(sd, &turb, &disc, &si);
									// dir = propagation ; le vecteur VERS le soleil
									// est son oppose.
									const float32 tx = -sd[0], ty = -sd[1], tz = -sd[2];
									const float32 tl = sqrtf(tx * tx + ty * ty + tz * tz);
									float32 elev = (tl > 1e-6f) ? asinf(ty / tl) * 57.2957795f : 45.f;
									float32 azim = atan2f(tx, tz) * 57.2957795f;
									const float32 e0 = elev, a0 = azim, t0 = turb, i0 = si;
									const bool d0 = disc;
									// ── QUEL SOLEIL LE CIEL SUIT-IL ? ──────
									// Une scene peut en porter PLUSIEURS : on
									// choisit, on ne devine pas. La source est
									// designee par son NOEUD, donc supprimer une
									// autre lampe ne fait pas changer de soleil.
									// « Manuel » garde la main sur elevation et
									// azimut.
									// TOUT CE QUE LE COMBO RETIENT DOIT SURVIVRE A LA
									// FRAME. NkComboPending garde un pointeur sur la
									// valeur ET sur le TABLEAU D'ITEMS ; la liste
									// n'est peinte qu'apres tout le reste, par
									// DrawComboPopup. Un tableau d'items LOCAL est
									// donc detruit avant d'etre lu -- ce qui ne se
									// perd pas en silence comme pour la valeur, mais
									// fait planter net a l'ouverture de la liste.
									// Les noms sont recalcules a chaque image (une
									// lumiere peut etre renommee), le STOCKAGE, lui,
									// est permanent.
									static int32 sunNodes[16];
									static char sunLbl[17][48];
									static const char *sunItems[17];
									int32 raw[16];
									const int32 rawCount = demo::Demo3DHostSunNodes(raw, 16);
									// ON NE PROPOSE QUE LES SOLEILS QUE L'UTILISATEUR
									// VOIT. Le moteur porte aussi les lumieres de la
									// demo (noeuds 86..89), qui n'apparaissent pas
									// dans la hierarchie : les lister ici faisait
									// surgir un soleil dont Rihen n'a jamais entendu
									// parler. On applique donc EXACTEMENT le meme
									// filtre que l'arbre de scene.
									int32 sunCount = 0;
									for (int32 i = 0; i < rawCount; ++i) {
										if (NkHierNodeSkip(raw[i]))
											continue;
										sunNodes[sunCount++] = raw[i];
									}
									snprintf(sunLbl[0], sizeof(sunLbl[0]), "Manuel");
									sunItems[0] = sunLbl[0];
									for (int32 i = 0; i < sunCount; ++i) {
										NkHierNodeName(st, sunNodes[i], sunLbl[i + 1],
													   (uint32)sizeof(sunLbl[0]));
										sunItems[i + 1] = sunLbl[i + 1];
									}
									const int32 curSun = demo::Demo3DHostSkySunSource();
									static int32 pushedSun = 0;
									// L'HOTE A PU LACHER LA SOURCE tout seul (le
									// soleil suivi a ete supprime). On remet alors le
									// rang a « Manuel » -- mais SEULEMENT si aucun
									// choix n'est en attente, sinon on effacerait ce
									// que le combo vient d'ecrire.
									if (curSun < 0 && st.skySunSel > 0 &&
										pushedSun == st.skySunSel) {
										st.skySunSel = 0;
										pushedSun = 0;
									}
									p.TextV(iA.x, yy, kRowH, "Suit", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.sky.sunsrc",
										  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
										   kRowH - S(4.f)},
										  sunItems, nullptr, sunCount + 1, st.skySunSel, combo);
									// Meme regle que pour le modele : on compare a CE
									// QU'ON A POUSSE, pas a une valeur capturee dans
									// la frame. Et on BORNE avant d'indexer : entre le
									// clic et son traitement, une lumiere a pu
									// disparaitre et raccourcir la liste.
									if (st.skySunSel != pushedSun) {
										if (st.skySunSel < 0 || st.skySunSel > sunCount)
											st.skySunSel = 0;
										pushedSun = st.skySunSel;
										demo::Demo3DHostSetSkySunSource(
											st.skySunSel <= 0 ? -1 : sunNodes[st.skySunSel - 1]);
										NkMarkDirty(st);
									}
									yy += kRowH;
									// LIE : elevation et azimut sont IMPOSES par la
									// lumiere. On les AFFICHE quand meme, en lecture
									// seule -- un etat qui se propage doit se voir
									// sous sa forme effective, pas rester un champ
									// modifiable dont personne ne tient compte.
									if (curSun >= 0) {
										char sb[64];
										snprintf(sb, sizeof(sb), "%.1f°  /  %.1f°", (double)elev,
												 (double)azim);
										p.TextV(iA.x, yy, kRowH, "Elev. / Azimut",
												NkRole::TextMuted);
										p.TextV(iA.x + S(110.f), yy, kRowH, sb, NkRole::Text);
										yy += kRowH;
									} else {
										p.TextV(iA.x, yy, kRowH, "Elevation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.elev",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  elev, 0.5f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Azimut", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.azim",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  azim, 1.f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
									}
									// Turbidite : 1 = air de haute montagne,
									// 2-3 = ciel clair, 6-10 = atmosphere chargee.
									p.TextV(iA.x, yy, kRowH, "Turbidite", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.turb",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  turb, 0.05f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									p.TextV(iA.x, yy, kRowH, "Puissance", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.sunint",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si, 0.02f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									{
										float32 sc[3];
										demo::Demo3DHostSkySunColor(sc);
										bool scCh = false;
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															"prop.sky.suncol", sc, &scCh);
										if (scCh) {
											demo::Demo3DHostSetSkySunColor(sc);
											NkMarkDirty(st);
										}
									}
									{
										const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
										hit.Add("prop.sky.disc", cb);
										p.Outline(cb, disc ? NkRole::AccentUi : NkRole::Border,
												  disc ? NkRole::AccentUi : NkRole::InputBg, 2.f);
										p.TextV(cb.x + S(18.f), yy, kRowH, "Disque solaire",
												NkRole::TextMuted);
										if (hit.Clicked("prop.sky.disc"))
											disc = !disc;
										yy += kRowH;
									}
									// LE SOLEIL DU CIEL ECLAIRE-T-IL LA SCENE ?
									// Propose UNIQUEMENT en mode Manuel : quand le
									// ciel suit une lumiere, celle-ci eclaire deja,
									// et en ajouter une seconde doublerait
									// l'eclairement sans que rien ne l'explique.
									// C'est ce qui donne au soleil manuel TOUS les
									// effets d'une directionnelle -- ombres portees
									// comprises -- au lieu d'un simple decor.
									if (curSun < 0) {
										bool lightsOn = demo::Demo3DHostSkySunLightsScene();
										const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
										hit.Add("prop.sky.sunlight", cb);
										p.Outline(cb,
												  lightsOn ? NkRole::AccentUi : NkRole::Border,
												  lightsOn ? NkRole::AccentUi : NkRole::InputBg,
												  2.f);
										p.TextV(cb.x + S(18.f), yy, kRowH, "Eclaire la scene",
												NkRole::TextMuted);
										if (hit.Clicked("prop.sky.sunlight")) {
											demo::Demo3DHostSetSkySunLightsScene(!lightsOn);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									if (elev != e0 || azim != a0 || turb != t0 || si != i0 ||
										disc != d0) {
										const float32 er = elev * 0.0174532925f;
										const float32 ar = azim * 0.0174532925f;
										const float32 nd[3] = {-cosf(er) * sinf(ar), -sinf(er),
															   -cosf(er) * cosf(ar)};
										demo::Demo3DHostSetSkySun(nd, turb, disc, si);
										NkMarkDirty(st);
									}
								}
								// LE SOL sert aux DEUX modeles : le ciel physique
								// n'est pas defini sous l'horizon, on y pose cette
								// couleur.
								yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Sol",
													"prop.sky.gnd", gnd, &ch);
								if (ch) {
									demo::Demo3DHostSetEnvSky(top, hor, gnd);
									NkMarkDirty(st);
								}
								// ── ETOILES ─────────────────────────────────
								// Elles s'effacent SEULES quand le ciel
								// s'eclaire : leur visibilite est l'inverse de
								// la luminosite locale. Un cycle jour/nuit les
								// fera donc apparaitre et disparaitre sans
								// qu'on les pilote -- comme les teintes du
								// couchant decoulent du modele physique.
								// Effet immediat : aucune regeneration.
								{
									float32 si2 = 0.f, sd2 = 200.f;
									demo::Demo3DHostSkyStars(&si2, &sd2);
									const float32 a0 = si2, b0 = sd2;
									p.TextV(iA.x, yy, kRowH, "Etoiles", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.stars",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si2, 0.02f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									if (si2 > 0.001f) {
										p.TextV(iA.x, yy, kRowH, "Densite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.stard",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  sd2, 2.f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
									}
									if (si2 != a0 || sd2 != b0) {
										demo::Demo3DHostSetSkyStars(si2, sd2);
										NkMarkDirty(st);
									}
									// MOUVEMENT : rotation de la voute et etoiles
									// filantes. Propose seulement si les etoiles
									// sont allumees — faire tourner un ciel vide
									// ou y lancer des filantes invisibles n'a
									// aucun sens.
									if (si2 > 0.001f) {
										float32 rot = 0.f, sho = 0.f;
										demo::Demo3DHostSkyStarMotion(&rot, &sho);
										const float32 r0 = rot, s0s = sho;
										p.TextV(iA.x, yy, kRowH, "Rotation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.starrot",
												  {iA.x + S(110.f), yy + S(3.f),
												   iA.w - S(110.f), kRowH - S(6.f)},
												  rot, 0.002f, NkRole::AccentUi, "%.3f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Filantes / min",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.shoot",
												  {iA.x + S(110.f), yy + S(3.f),
												   iA.w - S(110.f), kRowH - S(6.f)},
												  sho, 0.2f, NkRole::AccentUi, "%.1f");
										yy += kRowH;
										if (rot != r0 || sho != s0s) {
											demo::Demo3DHostSetSkyStarMotion(rot, sho);
											NkMarkDirty(st);
										}
									}
								}
								// ── LUNES ───────────────────────────────────
								// PLUSIEURS sont possibles : c'est un nombre, pas
								// un interrupteur. Leur PHASE n'est pas reglable
								// -- elle se deduit du soleil, donc le croissant
								// change tout seul quand il descend.
								{
									int32 mc = demo::Demo3DHostSkyMoonCount();
									const int32 mc0 = mc;
									p.TextV(iA.x, yy, kRowH, "Lunes", NkRole::TextMuted);
									const float32 gm = S(3.f);
									const float32 bwm = (iA.w - S(110.f) - gm * 2.f) / 3.f;
									float32 bxm = iA.x + S(110.f);
									char km[24];
									for (int32 t = 0; t < 3; ++t) {
										snprintf(km, sizeof(km), "prop.sky.moonn%d", t);
										const NkRect br{bxm, yy + S(2.f), bwm, kRowH - S(4.f)};
										const bool on = (t == mc);
										hit.Add(km, br);
										p.Outline(br, on ? NkRole::AccentUi : NkRole::Border,
												  on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
										char lb[8];
										snprintf(lb, sizeof(lb), "%d", t);
										p.TextV(br.x + (br.w - p.TextW(lb)) * 0.5f, yy, kRowH, lb,
												on ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked(km))
											mc = t;
										bxm += bwm + gm;
									}
									yy += kRowH;
									if (mc != mc0) {
										demo::Demo3DHostSetSkyMoonCount(mc);
										NkMarkDirty(st);
									}
									for (int32 m = 0; m < mc; ++m) {
										float32 me = 0.f, ma = 0.f, ms = 0.f, mb = 0.f, mcol[3];
										demo::Demo3DHostSkyMoon(m, &me, &ma, &ms, &mb, mcol);
										const float32 e0m = me, a0m = ma, s0m = ms, b0m = mb;
										bool colCh2 = false;
										char kk[28];
										char lbl[24];
										snprintf(lbl, sizeof(lbl), "Lune %d", m + 1);
										p.TextV(iA.x, yy, kRowH, lbl, NkRole::Text);
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mel%d", m);
										p.TextV(iA.x, yy, kRowH, "Elevation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  me, 0.5f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.maz%d", m);
										p.TextV(iA.x, yy, kRowH, "Azimut", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  ma, 1.f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.msz%d", m);
										p.TextV(iA.x, yy, kRowH, "Taille", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  ms, 0.001f, NkRole::AccentUi, "%.3f");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mbr%d", m);
										p.TextV(iA.x, yy, kRowH, "Luminosite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  mb, 0.02f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mcl%d", m);
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															kk, mcol, &colCh2);
										if (me != e0m || ma != a0m || ms != s0m || mb != b0m || colCh2) {
											demo::Demo3DHostSetSkyMoon(m, me, ma, ms, mb, mcol);
											NkMarkDirty(st);
										}
										// PHASE : deduite du soleil par defaut, donc
										// toujours coherente avec l'eclairage. La
										// forcer est un choix de MISE EN SCENE --
										// legitime pour un plan de film, et declare
										// explicitement plutot que subi.
										{
											bool mph = false;
											float32 mpv = 0.25f;
											demo::Demo3DHostSkyMoonPhase(m, &mph, &mpv);
											const bool h0 = mph;
											const float32 v0m = mpv;
											char kp[28];
											snprintf(kp, sizeof(kp), "prop.sky.mpm%d", m);
											const NkRect cb2{iA.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add(kp, cb2);
											p.Outline(cb2, mph ? NkRole::AccentUi : NkRole::Border,
													  mph ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb2.x + S(18.f), yy, kRowH, "Phase forcee",
													NkRole::TextMuted);
											if (hit.Clicked(kp))
												mph = !mph;
											yy += kRowH;
											if (mph) {
												snprintf(kp, sizeof(kp), "prop.sky.mpv%d", m);
												p.TextV(iA.x, yy, kRowH, "Phase",
														NkRole::TextMuted);
												DragFloat(p, hit, ws, in, kp,
														  {iA.x + S(110.f), yy + S(3.f),
														   iA.w - S(110.f), kRowH - S(6.f)},
														  mpv, 0.01f, NkRole::AccentUi, "%.2f");
												yy += kRowH;
											}
											if (mph != h0 || mpv != v0m) {
												demo::Demo3DHostSetSkyMoonPhase(m, mph, mpv);
												NkMarkDirty(st);
											}
										}
									}
								}
								// ── NUAGES ──────────────────────────────────
								// Couche INDEPENDANTE du modele : elle se pose
								// aussi bien sur un degrade que sur un ciel
								// physique. La « couverture » est un SEUIL, pas
								// une opacite : a 0 il n'y a rien, et les nuages
								// naissent puis grossissent quand on monte -- au
								// lieu d'un voile uniforme qui se contenterait de
								// foncer.
								{
									bool cOn = false;
									float32 cCov = 0.5f, cDen = 1.f, cScl = 2.f, cCol[3];
									demo::Demo3DHostSkyClouds(&cOn, &cCov, &cDen, &cScl, cCol);
									const bool o0 = cOn;
									const float32 v0 = cCov, w0 = cDen, s0c = cScl;
									bool colCh = false;
									const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.sky.clouds", cb);
									p.Outline(cb, cOn ? NkRole::AccentUi : NkRole::Border,
											  cOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb.x + S(18.f), yy, kRowH, "Nuages", NkRole::TextMuted);
									if (hit.Clicked("prop.sky.clouds"))
										cOn = !cOn;
									yy += kRowH;
									if (cOn) {
										p.TextV(iA.x, yy, kRowH, "Couverture", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cov",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cCov, 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Densite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cden",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cDen, 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Echelle", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cscl",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cScl, 0.05f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										// LA VITESSE : les nuages DEFILENT. Elle
										// n'agit que sur le ciel evalue en temps
										// reel, donc son effet est immediat -- elle
										// ne demande aucune regeneration.
										{
											float32 cs = demo::Demo3DHostSkyCloudSpeed();
											const float32 cs0 = cs;
											p.TextV(iA.x, yy, kRowH, "Vitesse", NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.sky.cspd",
													  {iA.x + S(110.f), yy + S(3.f),
													   iA.w - S(110.f), kRowH - S(6.f)},
													  cs, 0.002f, NkRole::AccentUi, "%.3f");
											if (cs != cs0) {
												demo::Demo3DHostSetSkyCloudSpeed(cs);
												NkMarkDirty(st);
											}
											yy += kRowH;
										}
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															"prop.sky.ccol", cCol, &colCh);
									}
									if (cOn != o0 || cCov != v0 || cDen != w0 || cScl != s0c || colCh) {
										demo::Demo3DHostSetSkyClouds(cOn, cCov, cDen, cScl, cCol);
										NkMarkDirty(st);
									}
									if (cOn) {
										// AMBIANCES : des reglages tout faits. Le bouton de
										// l'ambiance EN PLACE est plein (accent) -- des
										// qu'un curseur est retouche, plus aucun ne
										// s'allume, et c'est le bon message : on n'est
										// plus sur un preset.
										static const char *const kAmb[3] = {"Defaut", "Pluie",
																			"Desert"};
										static const char *const kAmbKey[3] = {
											"prop.sky.creset", "prop.sky.cpluie",
											"prop.sky.cdesert"};
										const int32 ambCur = demo::Demo3DHostCloudPreset();
										const float32 gpA = S(4.f);
										const float32 bwA = (iA.w - gpA * 2.f) / 3.f;
										for (int32 a = 0; a < 3; ++a) {
											const NkRect ra{iA.x + (bwA + gpA) * a, yy + S(2.f),
															bwA, kRowH - S(4.f)};
											hit.Add(kAmbKey[a], ra);
											const bool actA = ambCur == a;
											if (actA)
												p.Fill(ra, NkRole::AccentUi, 3.f);
											else
												p.Outline(ra, NkRole::Border, NkRole::InputBg,
														  3.f);
											const char *la = kAmb[a];
											float32 twA = p.TextW(la);
											if (twA <= ra.w - S(2.f))
												p.TextV(ra.x + (ra.w - twA) * 0.5f, yy, kRowH,
														la,
														actA ? NkRole::TextOnAccent
															 : NkRole::TextMuted);
											if (hit.Clicked(kAmbKey[a]))
												demo::Demo3DHostApplyCloudPreset(a);
										}
										yy += kRowH;
									}
								}
								// REGENERER est un calcul CPU (convolutions) : il se
								// demande, il ne se declenche pas a chaque image ni
								// sous le curseur qu'on tire.
								//
								// LE BOUTON DIT S'IL Y A QUELQUE CHOSE A REGENERER.
								// Sans ce retour, on tire un curseur, l'image ne
								// bouge pas, et rien ne dit que c'est normal : le
								// reglage passe pour « sans effet » -- exactement le
								// genre de doute qu'on vient de payer cher ailleurs.
								{
									const bool dirty = demo::Demo3DHostSkyNeedsApply();
									const float32 gp = S(4.f);
									const float32 bw = (iA.w - gp) * 0.62f;
									const NkRect br{iA.x, yy + S(2.f), bw, kRowH - S(4.f)};
									hit.Add("prop.sky.apply", br);
									p.Fill(br, dirty ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
									const char *lbl = dirty ? "Regenerer le ciel *"
															: "Regenerer le ciel";
									p.TextV(br.x + (br.w - p.TextW(lbl)) * 0.5f, yy, kRowH, lbl,
											dirty ? NkRole::TextOnAccent : NkRole::TextMuted);
									if (hit.Clicked("prop.sky.apply"))
										demo::Demo3DHostApplySky();
									// REINITIALISER LE CIEL, sans toucher a
									// l'ambiance ni aux nuages : trois portees
									// separees, pour ne pas perdre l'un en voulant
									// retrouver l'autre.
									const NkRect rr2{iA.x + bw + gp, yy + S(2.f),
													 iA.w - bw - gp, kRowH - S(4.f)};
									hit.Add("prop.sky.reset", rr2);
									p.Outline(rr2, NkRole::Border, NkRole::InputBg, 3.f);
									p.TextV(rr2.x + (rr2.w - p.TextW("Defaut")) * 0.5f, yy, kRowH,
											"Defaut", NkRole::TextMuted);
									if (hit.Clicked("prop.sky.reset")) {
										demo::Demo3DHostResetSky();
										// L'etat d'interface doit suivre la remise a
										// zero : sinon le combo continuerait
										// d'afficher « Physique » alors que le
										// moteur est revenu au degrade.
										st.skyModel = 0;
										st.skySunSel = 0;
									}
									yy += kRowH;
								}
							} else if (st.envSource == 2) {
								p.TextV(iA.x, yy, kRowH, "Fichier", NkRole::TextMuted);
								if (!ws.IsEditing("prop.hdr.path") && !st.hdrPath[0])
									snprintf(st.hdrPath, sizeof(st.hdrPath), "%s",
											 demo::Demo3DHostHdrPath());
								EditableText(p, hit, ws, in, "prop.hdr.path",
											 {iA.x + S(70.f), yy + S(2.f), iA.w - S(70.f),
											  kRowH - S(4.f)},
											 st.hdrPath[0] ? st.hdrPath : "Resources/HDRI/....hdr",
											 st.hdrPath[0] ? NkRole::Text : NkRole::TextMuted,
											 st.hdrPath, sizeof(st.hdrPath));
								yy += kRowH;
								const NkRect br{iA.x, yy + S(2.f), iA.w, kRowH - S(4.f)};
								hit.Add("prop.hdr.load", br);
								p.Fill(br, NkRole::AccentUi, 3.f);
								p.TextV(br.x + (br.w - p.TextW("Charger l'image")) * 0.5f, yy,
										kRowH, "Charger l'image", NkRole::TextOnAccent);
								if (hit.Clicked("prop.hdr.load"))
									st.hdrOk = demo::Demo3DHostLoadHdr(st.hdrPath) ? 1 : -1;
								yy += kRowH;
								if (st.hdrOk != 0) {
									p.TextV(iA.x, yy, kRowH,
											st.hdrOk > 0 ? "Image chargee"
														 : "Echec : fichier introuvable ou format"
														   " non equirectangulaire",
											st.hdrOk > 0 ? NkRole::TextMuted : NkRole::AxisX);
									yy += kRowH;
								}
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, aR, aTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── BROUILLARD ──────────────────────────────────────────────
					{
						// ── SOL INFINI (option, Rihen) : un vrai plan recepteur
						// d'ombres, distinct de la grille -- couleur, hauteur,
						// rugosite. Le plan suit la camera cote hote.
						{
							NkRect gR = rr;
							gR.x = r.x + NkPropInset();
							gR.w = rr.w - 2.f * NkPropInset();
							const float32 gTop = yy;
							if (PaintPropGroup(p, hit, st, gR, yy, "prop.g.floor", "Sol", 4u)) {
								const NkRect iG = NkGroupInner(gR);
								const float32 gvX = iG.x + S(110.f);
								const float32 gvW = iG.w - S(110.f);
								yy += NkGroupPad();
								bool gOn = false;
								float32 gCol[3], gY = 0.f, gRg = 0.9f, gTl = 1.f, gMt = 0.f;
								int32 gPat = 0;
								demo::Demo3DHostFloor(&gOn, gCol, &gY, &gRg, &gPat, &gTl, &gMt);
								const bool g0 = gOn;
								const float32 gy0 = gY, gr0 = gRg, gt0 = gTl, gm0 = gMt;
								const int32 gp0 = gPat;
								bool gcCh = false;
								{
									const NkRect cb{iG.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.floor.on", cb);
									p.Outline(cb, gOn ? NkRole::AccentUi : NkRole::Border,
											  gOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb.x + S(18.f), yy, kRowH, "Actif",
											NkRole::TextMuted);
									if (hit.Clicked("prop.floor.on"))
										gOn = !gOn;
									yy += kRowH;
								}
								yy += PaintColorRow(p, hit, ws, in, st, iG, yy, "Couleur",
													"prop.floorcol", gCol, &gcCh);
								p.TextV(iG.x, yy, kRowH, "Hauteur", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.y",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gY, 0.01f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iG.x, yy, kRowH, "Rugosite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.rg",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gRg, 0.005f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
								// METALLIQUE : 0 = dielectrique (la couleur diffuse,
								// reflets blancs), 1 = metal (plus de diffusion, la
								// couleur TEINTE les reflets).
								p.TextV(iG.x, yy, kRowH, "Metallique", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.mt",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gMt, 0.005f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
								// ── MOTIF (Rihen, references Unreal) : uni, damier,
								// ou carreaux a joints -- et la taille du carreau.
								{
									static const char *const kPat[3] = {"Uni", "Damier",
																		"Carreaux"};
									p.TextV(iG.x, yy, kRowH, "Motif", NkRole::TextMuted);
									const float32 pw3 = gvW / 3.f - S(3.f);
									for (int32 m3 = 0; m3 < 3; ++m3) {
										const NkRect mb{gvX + (pw3 + S(4.f)) * (float32)m3,
														yy + S(2.f), pw3, kRowH - S(4.f)};
										char mk[24];
										snprintf(mk, sizeof(mk), "prop.floor.p%d", m3);
										hit.Add(mk, mb);
										const bool onM = gPat == m3;
										p.Fill(mb, onM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										// LIBELLE CLAMPE (Rihen : retreci, le texte
										// debordait du bouton et chevauchait le
										// voisin) : trop etroit -> l'initiale ;
										// encore trop -> rien, le bouton reste lisible
										// par sa couleur.
										static const char *const kPatS[3] = {"U", "D", "C"};
										const char *lbl3 = kPat[m3];
										float32 tw3 = p.TextW(lbl3);
										if (tw3 > mb.w - S(6.f)) {
											lbl3 = kPatS[m3];
											tw3 = p.TextW(lbl3);
										}
										if (tw3 <= mb.w - S(2.f))
											p.TextV(mb.x + (mb.w - tw3) * 0.5f, yy, kRowH, lbl3,
													onM ? NkRole::TextOnAccent
														: NkRole::TextMuted);
										if (hit.Clicked(mk))
											gPat = m3;
									}
									yy += kRowH;
								}
								if (gPat > 0) {
									p.TextV(iG.x, yy, kRowH, "Carreau", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.floor.tl",
											  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gTl,
											  0.01f, NkRole::AccentUi, "%.2f m");
									yy += kRowH;
								}
								if (gOn != g0 || gY != gy0 || gRg != gr0 || gcCh ||
									gPat != gp0 || gTl != gt0 || gMt != gm0) {
									demo::Demo3DHostSetFloor(gOn, gCol, gY, gRg, gPat, gTl, gMt);
									NkMarkDirty(st);
								}
								yy += NkGroupPad();
								PaintGroupBlock(p, gR, gTop, yy);
							}
							yy += NkPropGroupGap();
						}
						NkRect fR = rr;
						fR.x = r.x + NkPropInset();
						fR.w = rr.w - 2.f * NkPropInset();
						const float32 fTop = yy;
						if (PaintPropGroup(p, hit, st, fR, yy, "prop.g.fog", "Brouillard", 4u)) {
							const NkRect iF = NkGroupInner(fR);
							const float32 fvX = iF.x + S(110.f);
							const float32 fvW = iF.w - S(110.f);
							yy += NkGroupPad();
							bool fOn = false;
							float32 fCol[3], fDen = 0.f, fSta = 0.f, fEnd = 0.f;
							int32 fMode = 0;
							demo::Demo3DHostFog(&fOn, fCol, &fDen, &fSta, &fEnd, &fMode);
							const bool o0 = fOn;
							const float32 d0 = fDen, s0 = fSta, e0 = fEnd;
							const int32 m0 = fMode;
							{
								const NkRect cb{iF.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.fog.on", cb);
								p.Outline(cb, fOn ? NkRole::AccentUi : NkRole::Border,
										  fOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Actif", NkRole::TextMuted);
								if (hit.Clicked("prop.fog.on"))
									fOn = !fOn;
								yy += kRowH;
							}
							static const char *const kFm[2] = {"Lineaire (debut / fin)",
															   "Exponentiel (densite)"};
							p.TextV(iF.x, yy, kRowH, "Loi", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.fog.mode",
								  {fvX, yy + S(2.f), fvW, kRowH - S(4.f)}, kFm, nullptr, 2,
								  st.fogMode, combo);
							fMode = st.fogMode;
							yy += kRowH;
							bool fcCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, iF, yy, "Couleur",
												"prop.fogcol", fCol, &fcCh);
							if (fMode == 1) {
								p.TextV(iF.x, yy, kRowH, "Densite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.den",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fDen, 0.002f,
										  NkRole::AccentUi, "%.3f");
								yy += kRowH;
							} else {
								p.TextV(iF.x, yy, kRowH, "Debut", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.sta",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fSta, 0.25f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iF.x, yy, kRowH, "Fin", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.end",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fEnd, 0.5f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
							}
							// ── NAPPE AU SOL, SOUFFLEE COMME UNE FUMEE (Rihen) ──
							// Epaisseur a zero : le brouillard ne depend que de la
							// distance, comme avant -- les autres champs n'ont
							// alors aucun effet et ne s'affichent pas.
							{
								float32 fgB = 0.f, fgT = 0.f, fgW = 0.f;
								bool fgC = true;
								demo::Demo3DHostFogGround(&fgB, &fgT, &fgW, &fgC);
								const float32 b0g = fgB, t0g = fgT, w0g = fgW;
								const bool c0g = fgC;
								p.TextV(iF.x, yy, kRowH, "Epaisseur sol", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.thk",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgT, 0.1f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								if (fgT > 0.001f) {
									p.TextV(iF.x, yy, kRowH, "Altitude", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.fog.base",
											  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgB,
											  0.1f, NkRole::AccentUi, "%.2f m");
									yy += kRowH;
									p.TextV(iF.x, yy, kRowH, "Souffle", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.fog.wind",
											  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgW,
											  0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									// LIER AU VENT DES NUAGES : la nappe derive au
									// meme pas que la couche nuageuse -- c'est ce
									// qui fait respirer le sol et le ciel ensemble.
									const NkRect cbW{iF.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.fog.wcl", cbW);
									p.Outline(cbW, fgC ? NkRole::AccentUi : NkRole::Border,
											  fgC ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cbW.x + S(18.f), yy, kRowH,
											"Suivre le vent des nuages", NkRole::TextMuted);
									if (hit.Clicked("prop.fog.wcl"))
										fgC = !fgC;
									yy += kRowH;
								}
								if (fgB != b0g || fgT != t0g || fgW != w0g || fgC != c0g) {
									demo::Demo3DHostSetFogGround(fgB, fgT, fgW, fgC);
									NkMarkDirty(st);
								}
							}
							if (fOn != o0 || fDen != d0 || fSta != s0 || fEnd != e0 ||
								fMode != m0 || fcCh) {
								demo::Demo3DHostSetFog(fOn, fCol, fDen, fSta, fEnd, fMode);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, fR, fTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── OCCLUSION AMBIANTE (SSAO) ───────────────────────────────
					// Rihen (9 aout) : le depot sombre au pied des objets « ne
					// donne rien de bon pour une application » -> ETEINTE par
					// defaut, mais REGLABLE ici (rayon monde en metres,
					// intensite). Aucun etat local : la config du renderer fait
					// foi (Demo3DHostSSAO la lit, Set la pousse via SetPostConfig
					// qui reconstruit le graphe a l'aplomb de la frame suivante).
					{
						const bool grpAO = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.ssao",
														  "Occlusion ambiante", 1u);
						const float32 grpAOTop = yy;
						if (grpAO) {
							const NkRect iA = NkGroupInner(rowR);
							const float32 avX = iA.x + S(110.f);
							const float32 avW = iA.w - S(110.f);
							yy += NkGroupPad();
							bool aOn = false;
							float32 aRad = 0.5f, aInt = 1.f;
							demo::Demo3DHostSSAO(&aOn, &aRad, &aInt);
							const bool a0 = aOn;
							const float32 ar0 = aRad, ai0 = aInt;
							{
								const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.ssao.on", cb);
								p.Outline(cb, aOn ? NkRole::AccentUi : NkRole::Border,
										  aOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Actif", NkRole::TextMuted);
								if (hit.Clicked("prop.ssao.on"))
									aOn = !aOn;
								yy += kRowH;
							}
							if (aOn) {
								p.TextV(iA.x, yy, kRowH, "Rayon", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.ssao.rad",
										  {avX, yy + S(3.f), avW, kRowH - S(6.f)}, aRad, 0.02f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iA.x, yy, kRowH, "Intensite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.ssao.int",
										  {avX, yy + S(3.f), avW, kRowH - S(6.f)}, aInt, 0.02f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
							}
							if (aOn != a0 || aRad != ar0 || aInt != ai0) {
								demo::Demo3DHostSetSSAO(aOn, aRad, aInt);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpAOTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── EXPOSITION & BLOOM (2026-08-09) ─────────────────────────
					// Reglages presents dans le moteur depuis le debut, jamais
					// proposes : un spot surpuissant faisait un halo geant sans
					// qu'on puisse ni baisser l'exposition ni relever le seuil.
					{
						const bool grpFx = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.postfx",
														  "Exposition et bloom", 1u);
						const float32 grpFxTop = yy;
						if (grpFx) {
							const NkRect iF2 = NkGroupInner(rowR);
							const float32 fvX2 = iF2.x + S(110.f);
							const float32 fvW2 = iF2.w - S(110.f);
							yy += NkGroupPad();
							float32 fxE = 1.f, fxT = 0.85f, fxS = 1.5f;
							bool fxB = true;
							demo::Demo3DHostPostFx(&fxE, &fxB, &fxT, &fxS);
							const float32 e0 = fxE, t0 = fxT, s0 = fxS;
							const bool b0 = fxB;
							p.TextV(iF2.x, yy, kRowH, "Exposition", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.fx.exp",
									  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxE, 0.01f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							{
								const NkRect cb{iF2.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.fx.bloom", cb);
								p.Outline(cb, fxB ? NkRole::AccentUi : NkRole::Border,
										  fxB ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Bloom", NkRole::TextMuted);
								if (hit.Clicked("prop.fx.bloom"))
									fxB = !fxB;
								yy += kRowH;
							}
							if (fxB) {
								p.TextV(iF2.x, yy, kRowH, "Seuil", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fx.thr",
										  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxT,
										  0.01f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								p.TextV(iF2.x, yy, kRowH, "Intensite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fx.str",
										  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxS,
										  0.01f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
							}
							if (fxE != e0 || fxB != b0 || fxT != t0 || fxS != s0) {
								demo::Demo3DHostSetPostFx(fxE, fxB, fxT, fxS);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpFxTop, yy);
						}
						yy += NkPropGroupGap();
					}
					const bool grpSh = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.shadow",
													  "Ombres", 1u);
					const float32 grpShTop = yy;
					if (grpSh) {
						const NkRect iR = NkGroupInner(rowR);
						const float32 svX = iR.x + S(110.f);
						const float32 svW = iR.w - S(110.f);
						yy += NkGroupPad();
						float32 nb = 0.f, sb = 0.f, sf = 0.f;
						int32 q = 1;
						if (demo::Demo3DHostShadowCfg(&nb, &sb, &sf, &q)) {
							const float32 nb0 = nb, sb0 = sb, sf0 = sf;
							// La qualite vit dans l'etat, pas ici : la liste
							// deroulee ecrit a la frame SUIVANTE, par un pointeur
							// qui designerait alors une variable locale morte --
							// le choix se perdait en silence (Rihen : « le
							// combobox ne fonctionne pas »).
							if (st.shadowQual < 0)
								st.shadowQual = q;
							// VERITE-MOTEUR : q vient d'etre lu du moteur, l'etat
							// vient du combo (ecrit en fin de frame). Des qu'ils
							// divergent, on pousse -- « capturer avant / comparer
							// apres » dans la meme frame ne detectait jamais rien.
							const int32 qEng = q;
							// CINQ crans, comme l'enum moteur : avec 4, « Penombre
								// (PCSS) » etait l'index 3, c'est-a-dire POISSON --
								// le vrai PCSS etait inatteignable.
								// PCSS reel depuis le 9 aout (echantillonnage brut de
								// l'atlas au binding 12) : ombre NETTE au contact,
								// penombre qui grandit avec la distance au bloqueur.
								// En mode Penombre, « Douceur » = taille de la source.
								static const char *const kQ[5] = {"Aucune", "Douce (PCF 3)",
															  "Douce (PCF 5)",
															  "Poisson (grain doux)",
															  "Penombre (PCSS)"};
							p.TextV(iR.x, yy, kRowH, "Qualite", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.sh.q",
								  {svX, yy + S(2.f), svW, kRowH - S(4.f)}, kQ, nullptr, 5,
								  st.shadowQual, combo);
							q = st.shadowQual;
							yy += kRowH;
							// ── STATIQUE OU DYNAMIQUE (Rihen) ───────────────────
							// Statique : l'ombre est calculee une fois puis gardee
							// telle quelle -- c'est gratuit, mais elle ne suit plus
							// rien. Dynamique : elle se refait des que la lumiere ou
							// la scene bouge. Un modeleur veut le second ; le
							// premier sert aux decors qui ne bougent plus.
							{
								static const char *const kDyn[2] = {"Statique (calcul unique)",
																	"Dynamique (suit la scene)"};
								p.TextV(iR.x, yy, kRowH, "Mise a jour", NkRole::TextMuted);
								// VERITE-MOTEUR, comme la qualite ci-dessus : le
								// combo ecrit l'etat en fin de frame, on pousse des
								// que l'etat diverge de ce que dit le moteur.
								const int32 dEng = demo::Demo3DHostShadowDynamic() ? 1 : 0;
								Combo(p, hit, ws, "prop.sh.dyn",
									  {svX, yy + S(2.f), svW, kRowH - S(4.f)}, kDyn, nullptr, 2,
									  st.shadowDynamic, combo);
								if (st.shadowDynamic != dEng) {
									demo::Demo3DHostSetShadowDynamic(st.shadowDynamic != 0);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// EN STATIQUE, l'ombre est figee par choix -- mais on
								// doit pouvoir la refaire QUAND ON LE DECIDE (Rihen :
								// « pour static il faut un bouton pour recalculer »).
								// Une passe complete, puis le cache refige.
								// BLEU des qu'un recalcul serait UTILE (lumiere ou
								// geometrie modifiee depuis le gel), normal sinon --
								// le meme langage que « Regenerer le ciel * ».
								if (st.shadowDynamic == 0) {
									const bool stale = demo::Demo3DHostShadowRecalcPending();
									const NkRect rb{svX, yy + S(2.f), svW, kRowH - S(4.f)};
									hit.Add("prop.sh.recalc", rb);
									p.Fill(rb, stale ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
									const char *rlbl = stale ? "Recalculer l'ombre *"
															 : "Recalculer l'ombre";
									p.TextV(rb.x + (rb.w - p.TextW(rlbl)) * 0.5f, yy, kRowH, rlbl,
											stale ? NkRole::TextOnAccent : NkRole::TextMuted);
									if (hit.Clicked("prop.sh.recalc"))
										demo::Demo3DHostShadowRecalc();
									yy += kRowH;
								}
							}
							p.TextV(iR.x, yy, kRowH, "Douceur", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.soft",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, sf, 0.0005f,
									  NkRole::AccentUi, "%.4f");
							yy += kRowH;
							// LE BIAIS NORMAL est le reglage qui empeche un objet de
							// projeter son ombre SUR LUI-MEME : c'est lui qu'on
							// augmente quand on voit ces bandes sombres a sa surface.
							// En TEXELS de la shadow map (1.5 par defaut) : le pas
							// suit cette echelle -- l'ancien 0.005 datait des metres
							// et demandait cent crans pour produire un effet.
							p.TextV(iR.x, yy, kRowH, "Biais normal", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.nb",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, nb, 0.05f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							p.TextV(iR.x, yy, kRowH, "Biais de pente", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.sb",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, sb, 0.0002f,
									  NkRole::AccentUi, "%.4f");
							yy += kRowH;
							if (nb != nb0 || sb != sb0 || sf != sf0 || q != qEng) {
								demo::Demo3DHostSetShadowCfg(nb, sb, sf, q);
								NkMarkDirty(st);
							}
						} else {
							p.TextV(iR.x, yy, kRowH, "Ombres indisponibles",
									NkRole::TextMuted);
							yy += kRowH;
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpShTop, yy);
					}
					yy += NkPropGroupGap();
				} else if (sec == 2) {
					// ── LA SCENE, EN GROUPES REPLIABLES (Rihen) : meme facture
					// que Transformation, Dimensions ou Sol -- une categorie qui
					// deroule quinze champs a la file ne se lit pas.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					const bool grpView = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.view",
														"Vue", 256u);
					const float32 grpViewTop = yy;
					if (grpView) {
						yy += NkGroupPad();
						// MARGES INTERNES : le contenu de ce groupe est ecrit en
						// « r.x + kPad » et « rr.w » ; on SUBSTITUE ces deux rects
						// par ceux du dedans du cadre, comme le groupe Camera --
						// sans quoi les champs collent au bord (Rihen).
						const NkRect iV = NkGroupInner(rowR);
						const NkRect r{iV.x - kPad, rowR.y, iV.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						// Gabarit de champ DERIVE des rects substitues : calcule
						// avant eux, il gardait la largeur du panneau entier et
						// les champs debordaient du cadre (Rihen).
						NkRect fr{r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)};
					{
						p.TextV(r.x + kPad, yy, kRowH, "Projection", NkRole::TextMuted);
						const bool o = demo::Demo3DHostIsOrtho();
						const float32 bw = (rr.w - S(132.f)) * 0.5f;
						const NkRect b1{r.x + S(120.f), yy + S(2.f), bw, kRowH - S(4.f)};
						hit.Add("props.persp", b1);
						if (!o)
							p.Fill(b1, NkRole::AccentUi, 3.f);
						else
							p.Outline(b1, NkRole::Border, NkRole::PanelHeader, 3.f);
						p.TextV(b1.x + S(8.f), yy, kRowH, "Persp.",
								!o ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("props.persp")) {
							demo::Demo3DHostSetOrtho(false);
							NkMarkDirty(st);
							st.projection = 0;
							st.lastProjection = 0;
						}
						const NkRect b2{r.x + S(120.f) + bw + S(4.f), yy + S(2.f), bw, kRowH - S(4.f)};
						hit.Add("props.ortho", b2);
						if (o)
							p.Fill(b2, NkRole::AccentUi, 3.f);
						else
							p.Outline(b2, NkRole::Border, NkRole::PanelHeader, 3.f);
						p.TextV(b2.x + S(8.f), yy, kRowH, "Ortho.",
								o ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("props.ortho")) {
							demo::Demo3DHostSetOrtho(true);
							NkMarkDirty(st);
							st.projection = 1;
							st.lastProjection = 1;
						}
						yy += kRowH;
					}
					fr.y = yy + S(3.f);
					{
						p.TextV(r.x + kPad, yy, kRowH, "Taille ortho", NkRole::TextMuted);
						float32 os = demo::Demo3DHostOrthoScale();
						if (DragFloat(p, hit, ws, in, "props.oscale", fr, os, 0.005f,
									  NkRole::AccentUi, "%.2f")) {
							demo::Demo3DHostSetOrthoScale(os);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Distance (0=auto)", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 fv = demo::Demo3DHostViewFar();
						if (DragFloat(p, hit, ws, in, "props.far", fr2, fv, 5.f, NkRole::AccentUi,
									  "%.0f")) {
							demo::Demo3DHostSetViewFar(fv < 20.f ? 0.f : fv);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Etendue grille", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 ge = (float32)demo::Demo3DHostGridExtent();
						if (DragFloat(p, hit, ws, in, "props.grid", fr2, ge, 1.f, NkRole::AccentUi,
									  "%.0f")) {
							demo::Demo3DHostSetGridExtent((int32)(ge + 0.5f));
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					// ── ECHELLE EXACTE (cisaillement) ────────────────────────
					// Coupee : l'echelle est projetee sur les axes de l'objet, il
					// ne se deforme jamais de travers (choix d'Unreal, notre
					// defaut). Active : un scale en repere GLOBAL sur un objet
					// TOURNE le cisaille vraiment -- un carre devient un losange,
					// comme dans un logiciel qui garde une matrice complete.
					{
						const bool shOn = demo::Demo3DHostShearScale();
						const NkRect cb{r.x + kPad, yy + S(5.f), S(12.f), S(12.f)};
						hit.Add("props.shear", cb);
						p.Outline(cb, shOn ? NkRole::AccentUi : NkRole::Border,
								  shOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
						p.TextV(cb.x + S(18.f), yy, kRowH, "Echelle exacte (cisaillement)",
								NkRole::TextMuted);
						if (hit.Clicked("props.shear")) {
							demo::Demo3DHostSetShearScale(!shOn);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpViewTop, yy);
					}
					yy += NkPropGroupGap();
					// ── GROUPE « UNITES » ────────────────────────────────────
					const bool grpUnit = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.unit",
														"Unites", 512u);
					const float32 grpUnitTop = yy;
					if (grpUnit) {
						yy += NkGroupPad();
						// Memes marges internes que le groupe « Vue » ci-dessus.
						const NkRect iU = NkGroupInner(rowR);
						const NkRect r{iU.x - kPad, rowR.y, iU.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						NkRect fr{r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)};
					{
						// ── UNITES DE MESURE DE LA SCENE (Rihen) ─────────────────
						// Declaratif pour l'instant : les champs restent en unites
						// scene ; la conversion d'affichage viendra avec le format
						// projet.
						p.TextV(r.x + kPad, yy, kRowH, "Unites", NkRole::TextMuted);
						static const char *const kUSys[3] = {"Metrique", "Imperial", "Aucun"};
						Combo(p, hit, ws, "props.usys",
							  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
							  kUSys, nullptr, 3, st.unitSystem, combo);
						yy += kRowH;
						if (st.unitSystem != 2) {
							p.TextV(r.x + kPad, yy, kRowH, "Longueur", NkRole::TextMuted);
							static const char *const kUMet[3] = {"Metres", "Centimetres",
																 "Millimetres"};
							static const char *const kUImp[2] = {"Pieds", "Pouces"};
							const NkRect ur{r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
											kRowH - S(4.f)};
							if (st.unitSystem == 0) {
								Combo(p, hit, ws, "props.ulen", ur, kUMet, nullptr, 3,
									  st.unitLength, combo);
							} else {
								if (st.unitLength > 1)
									st.unitLength = 0;
								Combo(p, hit, ws, "props.ulen", ur, kUImp, nullptr, 2,
									  st.unitLength, combo);
							}
							yy += kRowH;
						}
						p.TextV(r.x + kPad, yy, kRowH, "Echelle d'unite", NkRole::TextMuted);
						NkRect fru = fr;
						fru.y = yy + S(3.f);
						DragFloat(p, hit, ws, in, "props.uscale", fru, st.unitScale, 0.01f,
								  NkRole::AccentUi, "%.2f");
						if (st.unitScale < 0.001f)
							st.unitScale = 0.001f;
						yy += kRowH;
							// CHAQUE SCENE A SES VALEURS (Rihen). Ici, une vraie
							// interface : un COMBO choisit la scene destinataire (ou
							// toutes), un BOUTON fait la copie.
							{
								p.TextV(r.x + kPad, yy, kRowH, "Copier vers",
										NkRole::TextMuted);
								// Le combo liste « Toutes les scenes » puis chaque
								// scene du PROJET -- pas seulement celles qui ont un
								// onglet ouvert : porter ses unites vers une scene
								// fermee est justement ce qu'on veut pouvoir faire.
								static const char *sTgts[NkModelerState::kMaxDocs + 1];
								static int32 sTgtDoc[NkModelerState::kMaxDocs + 1];
								sTgts[0] = "Toutes les scenes";
								int32 nTgt = 1;
								for (int32 d7 = 0; d7 < NkModelerState::kMaxDocs; ++d7) {
									if (!st.docUsed[d7] || st.docTransient[d7])
										continue;
									sTgtDoc[nTgt] = d7;
									sTgts[nTgt] = st.docName[d7];
									++nTgt;
								}
								if (st.propCopyTarget >= nTgt)
									st.propCopyTarget = 0;
								Combo(p, hit, ws, "props.utgt",
									  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
									   kRowH - S(4.f)},
									  sTgts, nullptr, nTgt, st.propCopyTarget, combo);
								yy += kRowH;
								// (Le bouton « Copier les proprietes » a disparu :
								// l'action vit maintenant dans le MENU du bandeau,
								// commun a tous les groupes -- Rihen. Le combo
								// ci-dessus reste : il dit VERS QUELLE scene.)
								// COPIER, demande par le menu du groupe : ce groupe
								// sait ce que ca veut dire -- porter ses unites vers
								// la scene choisie, ou vers toutes.
								if (NkGrpWants(st, "prop.g.unit", 1)) {
									// « Toutes » parcourt les documents ; sinon le seul
									// designe par le combo (sTgtDoc, rempli juste au-dessus).
									for (int32 k7 = 1; k7 < nTgt; ++k7) {
										if (st.propCopyTarget > 0 && k7 != st.propCopyTarget)
											continue;
										const int32 d8 = sTgtDoc[k7];
										st.docUnitSys[d8] = st.unitSystem;
										st.docUnitLen[d8] = st.unitLength;
										st.docUnitScale[d8] = st.unitScale;
									}
									// ... et dans le presse-papiers, pour un collage
									// vers une autre scene plus tard.
									const float32 u3[3] = {(float32)st.unitSystem,
														   (float32)st.unitLength,
														   st.unitScale};
									NkGrpCopyF(st, "prop.g.unit", u3, 3);
								}
								if (NkGrpWants(st, "prop.g.unit", 2) &&
									NkGrpCanPaste(st, "prop.g.unit")) {
									st.unitSystem = (int32)(st.grpClipF[0] + 0.5f);
									st.unitLength = (int32)(st.grpClipF[1] + 0.5f);
									st.unitScale = st.grpClipF[2];
								}
								if (NkGrpWants(st, "prop.g.unit", 3)) {
									st.unitSystem = 0; // metrique
									st.unitLength = 0; // metres
									st.unitScale = 1.f;
								}
							}
					}
					{
						// MATCAP : un COMBO avec l'APERCU de la boule choisie ; le clic
						// ouvre le panneau par categories, ancre ICI.
						p.TextV(r.x + kPad, yy, kRowH, "Matcap", NkRole::TextMuted);
						const int32 mc = demo::Demo3DHostMatcap();
						const NkRect br{r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)};
						const bool overM = hit.Add("props.matcap", br);
						p.Outline(br, (overM || st.matcapOpen) ? NkRole::AccentUi : NkRole::Border,
								  NkRole::InputBg, 3.f);
						p.Image(4300u + (uint32)mc,
								{br.x + S(3.f), br.y + S(2.f), br.h - S(4.f), br.h - S(4.f)});
						p.TextV(br.x + br.h + S(4.f), yy, kRowH, demo::Demo3DHostMatcapName(mc));
						// Marqueur blanc de combo, comme partout.
						p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
							   NkRole::Text);
						if (hit.Clicked("props.matcap")) {
							st.matcapOpen = !st.matcapOpen;
							st.matcapAnchor = br;
						}
						yy += kRowH;
						// L'APERCU, EN GRAND : une vignette de 22 px dit « il y a une
						// boule », pas « quelle matiere c'est ». Le carre reprend la
						// meme texture, en 72 px.
						p.Image(4300u + (uint32)mc, {r.x + S(120.f), yy + S(2.f), S(72.f), S(72.f)});
						yy += S(78.f);
					}
					{
						// â”€â”€ FOND PAR TYPE : un COMBO, et les proprietes du type choisi
						// juste en dessous (Rihen). Seule la COULEUR UNIE agit
						// aujourd'hui ; degrade, texture, HDRI et ciel montrent leurs
						// proprietes en annoncant le chantier moteur -- certains ne
						// seront visibles qu'en mode Rendu/Materiaux.
						p.TextV(r.x + kPad, yy, kRowH, "Fond", NkRole::TextMuted);
						static const char *const kBgTypes[5] = {"Couleur unie", "Degrade", "Texture",
																"HDRI", "Ciel"};
						Combo(p, hit, ws, "props.bgtype", {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
														   kRowH - S(4.f)},
							  kBgTypes, nullptr, 5, st.bgType, combo);
						yy += kRowH;
						if (st.bgType == 0) {
							p.TextV(r.x + kPad, yy, kRowH, "Couleur", NkRole::TextMuted);
							float32 bx = r.x + S(120.f);
							for (int32 i3 = 0; i3 < 6; ++i3) {
								snprintf(key, sizeof(key), "props.bg.%d", i3);
								const NkRect br{bx, yy + S(4.f), S(18.f), kRowH - S(8.f)};
								hit.Add(key, br);
								float32 c[3];
								NkBgColorOf(st, i3, c);
								p.Fill(br, NkColor{(uint8)(c[0] * 255.f), (uint8)(c[1] * 255.f),
												   (uint8)(c[2] * 255.f), 255},
									   2.f);
								if (st.bgChoice == i3)
									p.OutlineSharp(br, NkRole::AccentUi);
								if (hit.Clicked(key))
									st.bgChoice = i3;
								bx += S(22.f);
							}
						yy += kRowH;
						// LUMINOSITE : plus sombre ou plus claire, quelle que soit
						// la couleur choisie (Rihen).
						p.TextV(r.x + kPad, yy, kRowH, "Luminosite", NkRole::TextMuted);
						if (DragFloat(p, hit, ws, in, "props.bglum",
									  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
									   kRowH - S(4.f)},
									  st.bgBrightness, 0.005f, NkRole::AccentUi, "%.2f")) {
							if (st.bgBrightness < 0.1f)
								st.bgBrightness = 0.1f;
							if (st.bgBrightness > 2.f)
								st.bgBrightness = 2.f;
						}
						yy += kRowH;
						if (st.bgChoice == 5) {
							// LE VRAI PICKER (carre SV + barre de teinte, transpose du
							// ColorPicker4 de NKGui) ; les champs R/V/B restent dessous
							// pour les valeurs exactes. Le fond suit EN DIRECT.
							{
								const NkRect pk{r.x + S(16.f), yy + S(2.f), rr.w - S(28.f), S(120.f)};
								NkColorPickerSV(p, hit, st.propDragKey, sizeof(st.propDragKey),
												"props.bgpick", pk, st.bgCustom);
								yy += S(126.f);
							}
							static const char *const kCh[3] = {"Rouge", "Vert", "Bleu"};
							for (int32 c2 = 0; c2 < 3; ++c2) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, kCh[c2], NkRole::TextMuted);
								snprintf(key, sizeof(key), "props.bgc.%d", c2);
								float32 v2 = st.bgCustom[c2];
								if (DragFloat(p, hit, ws, in, key,
											 {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											  kRowH - S(4.f)},
											 v2, 0.005f, NkRole::AccentUi, "%.2f")) {
									if (v2 < 0.f)
										v2 = 0.f;
									if (v2 > 1.f)
										v2 = 1.f;
									st.bgCustom[c2] = v2;
								}
								yy += kRowH;
							}
						}
						} else if (st.bgType == 1) {
							p.TextV(r.x + kPad, yy, kRowH, "Haut / horizon / bas : trois",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "couleurs -- fond moteur, a venir.",
									NkRole::TextMuted);
							yy += kRowH;
						} else if (st.bgType == 2 || st.bgType == 3) {
							p.TextV(r.x + kPad, yy, kRowH,
									st.bgType == 2 ? "Fichier image : (aucun)" : "Fichier .hdr : (aucun)",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "Visible en mode Rendu -- a venir.",
									NkRole::TextMuted);
							yy += kRowH;
						} else {
							p.TextV(r.x + kPad, yy, kRowH, "Ciel procedural (mode Rendu) --",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "a venir avec le fond moteur.",
									NkRole::TextMuted);
							yy += kRowH;
						}
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpUnitTop, yy);
					}
					yy += NkPropGroupGap();
				} else if (sec == 3) {
					// ── MODIFICATEUR (pastille « sliders-horizontal ») ──────────
					// La categorie reste A DEFINIR avec Rihen. En attendant, elle
					// HEBERGE les reglages de l'outil actif : ils etaient sur une
					// pastille supprimee par la maquette, et les perdre en silence
					// serait une regression. Ils demenageront a la refonte.
					// EN GROUPES REPLIABLES (Rihen), comme les autres categories.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					const bool grpMod = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.mod",
													   "Modificateurs", 1024u);
					const float32 grpModTop = yy;
					if (grpMod) {
						yy += NkGroupPad();
						const NkRect iM = NkGroupInner(rowR);
						// L'AJOUT vit ICI : le deroulant de la barre d'outils est
						// retire (regle de Rihen, la pastille suffit). Meme menu a
						// deux niveaux, ancre a ce bouton.
						{
							const NkRect mb{iM.x, yy + S(2.f), iM.w, kRowH - S(4.f)};
							const bool ovM = hit.Add("props.modadd", mb);
							const bool opM = ws.ComboOpen("tb.mod");
							p.Outline(mb, (ovM || opM) ? NkRole::AccentUi : NkRole::Border,
									  opM ? NkRole::AccentUi : NkRole::InputBg, 3.f);
							const char *lbAdd = "Ajouter un modificateur";
							float32 twA = p.TextW(lbAdd);
							if (twA > mb.w - S(6.f)) {
								lbAdd = "Ajouter";
								twA = p.TextW(lbAdd);
							}
							p.TextV(mb.x + (mb.w - twA) * 0.5f, yy, kRowH, lbAdd,
									opM ? NkRole::TextOnAccent : NkRole::Text);
							if (hit.Clicked("props.modadd")) {
								ws.ToggleCombo("tb.mod");
								st.modAnchor = mb;
							}
							yy += kRowH;
						}
						p.TextV(iM.x, yy, kRowH, "Aucun modificateur pose.",
								NkRole::TextMuted);
						yy += kRowH;
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpModTop, yy);
					}
					yy += NkPropGroupGap();
				} else if (sec == 5) {
					// ── OUTIL (pastille dediee, regle de Rihen) ─────────────────
					// Les reglages de l'outil ACTIF : ce que fait le clic, dans
					// quel repere, avec quelle aimantation. Ils etaient heberges
					// par Modificateur -- un provisoire annonce comme tel dans le
					// code -- et retrouvent ici leur place, comme l'onglet
					// « Tool » de Blender.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── GROUPE « OUTIL ACTIF » : les reglages de l'outil courant.
					const bool grpTool = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.tool",
														"Outil actif", 2048u);
					const float32 grpToolTop = yy;
					if (!grpTool) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						// Memes marges internes que les groupes de la Scene.
						const NkRect iT = NkGroupInner(rowR);
						const NkRect r{iT.x - kPad, rowR.y, iT.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
					// (« Ajouter un modificateur » a rejoint la pastille
					// MODIFICATEUR : il etait parti avec l'outil lors du
					// decoupage, alors qu'il n'a rien a y faire -- Rihen.)
					// ── L'OUTIL -- et PLUSIEURS quand plusieurs coexistent : l'outil
					// de transformation garde son bloc, le mode EDITION empile le
					// sien dessous (deplacer + extruder arrivent ensemble, chacun
					// doit rester lisible). ─────────────────────────────────────────
					static const char *const kToolNames[6] = {"Selection",  "Curseur 3D", "Deplacer",
															  "Rotation",	"Echelle",	  "Multigizmo"};
					p.TextV(r.x + kPad, yy, kRowH, kToolNames[(int32)st.tool]);
					yy += kRowH;
					if (st.tool == NkTool::Select) {
						int32 nS2 = 0;
						const char *const *shapes = NkSelShapeItems(nS2);
						float32 bx = r.x + kPad;
						for (int32 i3 = 0; i3 < nS2; ++i3) {
							snprintf(key, sizeof(key), "props.shape.%d", i3);
							float32 wq = (rr.w - 2.f * kPad - 8.f) / 3.f;
							if (wq < S(30.f))
								wq = S(30.f);
							const NkRect br{bx, yy + S(2.f), wq, kRowH - S(4.f)};
							hit.Add(key, br);
							if (st.selShape == i3)
								p.Fill(br, NkRole::AccentUi, 3.f);
							else
								p.Outline(br, NkRole::Border, NkRole::PanelHeader, 3.f);
							p.Clip(br);
							p.TextV(br.x + S(6.f), yy, kRowH, shapes[i3],
									st.selShape == i3 ? NkRole::TextOnAccent : NkRole::Text);
							p.Unclip();
							if (hit.Clicked(key))
								st.selShape = i3;
							bx += wq + 4.f;
						}
						yy += kRowH;
					} else if (st.tool == NkTool::Cursor) {
						p.TextV(r.x + kPad, yy, kRowH, "Clic gauche : poser le curseur 3D",
								NkRole::TextMuted);
						yy += kRowH;
						if (Button("props.cur0", yy, "Remettre a l'origine", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostResetCursor();
						yy += kRowH;
						// CURSEUR <-> SELECTION, en mode objet comme en edition.
						if (Button("props.cur1", yy, "Curseur -> selection", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostCursorToSelection();
						yy += kRowH;
						if (Button("props.cur2", yy, "Selection -> curseur", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostSelectionToCursor();
						yy += kRowH;
					} else {
						// Orientation : libelles CLIPPES a leur bouton -- en retrecissant
						// le panneau ils debordaient (Deplacer, Rotation, Echelle et le
						// cumule partagent ce bloc).
						// ORIENTATION EN COMBO (Rihen) : depuis qu'elles sont SEPT
						// (Monde, Local, Normale, Gimbal, Vue, Curseur, Parent),
						// une rangee de boutons calculee pour trois debordait du
						// groupe. Une liste tient dans n'importe quelle largeur et
						// nomme les entrees en toutes lettres.
						p.TextV(r.x + kPad, yy, kRowH, "Orientation", NkRole::TextMuted);
						int32 nOr = 0;
						const char *const *orients = NkOrientItems(nOr);
						Combo(p, hit, ws, "props.orient",
							  {r.x + S(96.f), yy + S(2.f), rr.w - S(104.f), kRowH - S(4.f)},
							  orients, NkOrientIcons(), nOr, st.orientation, combo);
						yy += kRowH;
						// AIMANTATION : la bascule ET ses PAS, modifiables ici.
						const bool snapOn = demo::Demo3DHostSnapEnabled();
						if (Button("props.snap", yy,
								   snapOn ? "Aimantation : active" : "Aimantation : coupee",
								   r.x + kPad, rr.w - 2.f * kPad))
							demo::Demo3DHostSetSnap(!snapOn, st.snapStepT, st.snapStepR,
													st.snapStepS);
						yy += kRowH;
						{
							static const char *const kSn[3] = {"Pas deplacement", "Pas angle",
															   "Pas echelle"};
							float32 *sv[3] = {&st.snapStepT, &st.snapStepR, &st.snapStepS};
							static const char *const kFm[3] = {"%.2f", "%.0f", "%.2f"};
							bool snCh = false;
							for (int32 i3 = 0; i3 < 3; ++i3) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, kSn[i3], NkRole::TextMuted);
								snprintf(key, sizeof(key), "props.snap.%d", i3);
								snCh |= DragFloat(p, hit, ws, in, key,
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  *sv[i3], i3 == 1 ? 0.5f : 0.01f,
												  NkRole::AccentUi, kFm[i3]);
								yy += kRowH;
							}
							if (snCh) {
								if (st.snapStepT < 0.01f)
									st.snapStepT = 0.01f;
								if (st.snapStepR < 1.f)
									st.snapStepR = 1.f;
								if (st.snapStepS < 0.01f)
									st.snapStepS = 0.01f;
								demo::Demo3DHostSetSnap(snapOn, st.snapStepT, st.snapStepR,
														st.snapStepS);
							}
						}
						// ── EDITION PROPORTIONNELLE : bascule, rayon, attenuation.
						// Les memes reglages que le chevron de la barre -- une seule
						// verite, lue au moteur -- pour qui travaille au panneau.
						{
							bool peOn = false;
							float32 peR = 1.f;
							int32 peF = 0;
							demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
							const float32 r0 = peR;
							const int32 f0 = peF;
							if (Button("props.pe", yy,
									   peOn ? "Edition proportionnelle : active"
											: "Edition proportionnelle : coupee",
									   r.x + kPad, rr.w - 2.f * kPad))
								demo::Demo3DHostSetPropEdit(!peOn, peR, peF);
							yy += kRowH;
							if (peOn) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Rayon", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pe.r",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
										   kRowH - S(4.f)},
										  peR, 0.02f, NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Attenuation",
										NkRole::TextMuted);
								static const char *const kFal[8] = {
									"Lisse", "Sphere",	 "Racine",	 "Carre inverse",
									"Net",	 "Lineaire", "Constant", "Aleatoire"};
								Combo(p, hit, ws, "props.pe.f",
									  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
									  kFal, nullptr, 8, peF, combo);
								yy += kRowH;
								if (peR < 0.01f)
									peR = 0.01f;
								if (peR != r0 || peF != f0)
									demo::Demo3DHostSetPropEdit(peOn, peR, peF);
							}
						}
						if (st.tool == NkTool::Move || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr0", yy, "Remettre la translation", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(0);
							yy += kRowH;
						}
						if (st.tool == NkTool::Rotate || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr1", yy, "Remettre la rotation", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(1);
							yy += kRowH;
						}
						if (st.tool == NkTool::Scale || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr2", yy, "Remettre l'echelle", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(2);
							yy += kRowH;
						}
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpToolTop, yy);
						yy += NkPropGroupGap();
					}
					// ── GROUPE « OUTIL D'EDITION », en mode Edit seulement.
					if (demo::Demo3DHostInEditMode()) {
						const bool grpEd = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.edt",
														  "Outil d'edition", 4096u);
						const float32 grpEdTop = yy;
						if (grpEd) {
						yy += NkGroupPad();
						// Memes marges internes que les autres groupes.
						const NkRect iE = NkGroupInner(rowR);
						const NkRect r{iE.x - kPad, rowR.y, iE.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 m2 = demo::Demo3DHostEditSelMask();
						snprintf(buf, sizeof(buf), "Sous-mode : %s%s%s", (m2 & 1) ? "Sommets " : "",
								 (m2 & 2) ? "Aretes " : "", (m2 & 4) ? "Faces" : "");
						p.TextV(r.x + kPad, yy, kRowH, buf, NkRole::TextMuted);
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "E extruder   I inserer   Ctrl+B biseauter",
								NkRole::TextMuted);
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Ctrl+R boucle   W subdiviser   K couteau",
								NkRole::TextMuted);
						yy += kRowH;
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpEdTop, yy);
						}
						yy += NkPropGroupGap();
					}
				} else if (sec == 4) {
					// ── MATERIAU, FACTURE BLENDER (capture de Rihen) ────────────
					// Une LISTE d'emplacements avec sa colonne + / - / menu et sa
					// poignee de hauteur ; dessous, la barre du NAVIGATEUR (choisir
					// un materiau existant, le renommer, le delier) ; puis les
					// PROPRIETES du materiau selectionne. La pile de groupes
					// repliables ne tenait pas quand les materiaux se comptaient en
					// dizaines -- une liste tient dans un coin d'ecran.
					// La sauvegarde .nkasset (NkMaterialLibrary) et l'editeur nodal
					// s'appuieront sur ce meme registre.
					const int32 actN = st.activeEmpty >= 0 ? st.activeEmpty
														   : demo::Demo3DHostActiveObject();
					const int32 curOf = actN >= 0 ? demo::Demo3DHostProjMatOf(actN) : -1;
					// LE REGISTRE N'EST JAMAIS VIDE ICI : le materiau de base est
					// cree des l'affichage (regle de Rihen -- un maillage ne vit pas
					// sans materiau, la liste non plus).
					demo::Demo3DHostProjMatEnsureDefault();
					// Table des materiaux vivants : la liste travaille sur des
					// RANGS, le registre sur des indices -- les deux ne coincident
					// pas des qu'un materiau est supprime au milieu.
					static int32 sMatIdx[64];
					static char sMatNm[64][32];
					int32 nMats = 0;
					for (int32 mi = 0; mi < 64 && nMats < 64; ++mi)
						if (demo::Demo3DHostProjMatInfo(mi, sMatNm[nMats], 32u, nullptr,
														nullptr, nullptr)) {
							sMatIdx[nMats] = mi;
							++nMats;
						}
					if (st.projMatSel >= nMats)
						st.projMatSel = nMats > 0 ? nMats - 1 : 0;
					const int32 selMat = nMats > 0 ? sMatIdx[st.projMatSel] : -1;

					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── LA LISTE + SA COLONNE DE BOUTONS ───────────────────────
					{
						const float32 colW = S(22.f);
						const float32 lstH = st.projMatListH;
						const NkRect lst{rowR.x, yy, rowR.w - colW - S(4.f), lstH};
						p.Fill(lst, NkRole::InputBg, 3.f);
						p.OutlineSharp(lst, NkRole::Border);
						p.Clip(lst);
						hit.PushClip(lst);
						const float32 lineH = S(20.f);
						float32 ly = lst.y + S(2.f) - st.projMatScroll;
						char lk[32];
						for (int32 i = 0; i < nMats; ++i) {
							const NkRect lr{lst.x + S(2.f), ly, lst.w - S(4.f), lineH};
							if (ly + lineH > lst.y && ly < lst.y + lst.h) {
								snprintf(lk, sizeof(lk), "props.pm.l.%d", i);
								const bool ovL = hit.Add(lk, lr);
								const bool selL = (i == st.projMatSel);
								if (selL)
									p.Fill(lr, NkRole::AccentUi, 2.f);
								else if (ovL)
									p.Fill(lr, NkRole::PanelHeader, 2.f);
								// Pastille de la COULEUR REELLE, comme la capture.
								float32 alb[3];
								demo::Demo3DHostProjMatInfo(sMatIdx[i], nullptr, 0u, alb,
															nullptr, nullptr);
								const NkColor cw{(uint8)(alb[0] * 255.f),
												 (uint8)(alb[1] * 255.f),
												 (uint8)(alb[2] * 255.f), 255};
								p.Fill({lr.x + S(4.f), lr.y + S(4.f), S(12.f), S(12.f)}, cw,
									   6.f);
								p.TextV(lr.x + S(22.f), lr.y, lineH, sMatNm[i],
										selL ? NkRole::TextOnAccent : NkRole::Text);
								// L'objet ACTIF porte-t-il ce materiau ?
								if (curOf == sMatIdx[i])
									p.IconV(lr.x + lr.w - S(16.f), lr.y, lineH, NkIcon::Check,
											selL ? NkRole::TextOnAccent : NkRole::AccentUi,
											11.f);
								if (hit.Clicked(lk))
									st.projMatSel = i;
							}
							ly += lineH;
						}
						hit.PopClip();
						p.Unclip();
						// DEFILEMENT : la liste garde sa hauteur, son contenu glisse.
						const float32 contH = (float32)nMats * lineH + S(4.f);
						if (hit.IsHovered("props.pm.list") && in.wheel != 0.f)
							st.projMatScroll -= in.wheel * lineH;
						const float32 maxSc = contH > lstH ? contH - lstH : 0.f;
						if (st.projMatScroll > maxSc)
							st.projMatScroll = maxSc;
						if (st.projMatScroll < 0.f)
							st.projMatScroll = 0.f;
						hit.Add("props.pm.list", lst);
						// COLONNE + / - / MENU, a droite de la liste (la capture).
						const float32 bx = lst.x + lst.w + S(4.f);
						{
							const NkRect ab{bx, lst.y, colW, S(20.f)};
							const bool ovA = hit.Add("props.pm.add", ab);
							p.Outline(ab, ovA ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							p.IconV(ab.x + S(5.f), ab.y, ab.h, NkIcon::Add, NkRole::Text, 11.f);
							if (hit.Clicked("props.pm.add")) {
								const int32 ni = demo::Demo3DHostProjMatCreate();
								if (ni >= 0)
									for (int32 i = 0; i < nMats + 1; ++i)
										if (i < 64 && ni == sMatIdx[i])
											st.projMatSel = i;
							}
						}
						{
							// RETIRER : le dernier materiau ne se supprime pas -- un
							// maillage en porte toujours un (regle de Rihen).
							const NkRect rb{bx, lst.y + S(21.f), colW, S(20.f)};
							const bool en = nMats > 1;
							const bool ovR = hit.Add("props.pm.del", rb);
							p.Outline(rb, (ovR && en) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							p.IconV(rb.x + S(5.f), rb.y, rb.h, NkIcon::MinusCircle,
									en ? NkRole::Text : NkRole::TextMuted, 11.f);
							if (en && hit.Clicked("props.pm.del") && selMat >= 0) {
								demo::Demo3DHostProjMatDelete(selMat);
								if (st.projMatSel > 0)
									--st.projMatSel;
							}
						}
						{
							// LE MENU du groupe, comme partout : copier / coller /
							// reinitialiser, dans la brique commune.
							const NkRect mb{bx, lst.y + S(46.f), colW, S(20.f)};
							const bool ovM = hit.Add("props.pm.menu", mb);
							const bool opM = (strcmp(st.grpMenuKey, "prop.g.mat") == 0);
							if (opM)
								p.Fill(mb, NkRole::AccentUi, 3.f);
							else
								p.Outline(mb, ovM ? NkRole::AccentUi : NkRole::Border,
										  NkRole::PanelHeader, 3.f);
							p.IconV(mb.x + S(5.f), mb.y, mb.h, NkIcon::ChevronDown,
									opM ? NkRole::TextOnAccent : NkRole::Text, 11.f);
							if (hit.Clicked("props.pm.menu")) {
								if (opM) {
									st.grpMenuKey[0] = 0;
								} else {
									NkWidgetState::Copy(st.grpMenuKey, "prop.g.mat", 39u);
									NkWidgetState::Copy(st.grpMenuTitle, "Materiau", 39u);
									st.grpMenuAnchor = mb;
								}
							}
						}
						yy += lstH;
						// POIGNEE DE HAUTEUR, sous la liste (les points de la
						// capture) : la liste s'agrandit quand les materiaux se
						// multiplient.
						{
							const NkRect gh{lst.x, yy, lst.w, S(6.f)};
							const bool ovG = hit.Add("props.pm.grip", gh);
							const bool mineG = (strcmp(st.propDragKey, "props.pm.grip") == 0);
							if (ovG || mineG)
								hit.WantCursor(NkCursorWant::ResizeNS);
							p.Fill({gh.x + gh.w * 0.5f - S(9.f), gh.y + S(2.f), S(18.f), S(2.f)},
								   (ovG || mineG) ? NkRole::AccentUi : NkRole::Border);
							if (hit.MouseDown() && (ovG || mineG)) {
								if (!st.propDragKey[0] && ovG)
									NkWidgetState::Copy(st.propDragKey, "props.pm.grip", 39u);
								if (mineG || !st.propDragKey[0]) {
									float32 nh = hit.Mouse().y - lst.y;
									st.projMatListH = nh < S(48.f) ? S(48.f)
																   : (nh > S(320.f) ? S(320.f) : nh);
								}
							}
							yy += S(8.f);
						}
					}
					// ── LA BARRE DU NAVIGATEUR (sous la liste, comme la capture)
					// Icone + combo des materiaux + nom editable + delier.
					if (selMat >= 0) {
						char mnm[32];
						float32 alb[3], rgh = 0.85f, mtl = 0.f;
						demo::Demo3DHostProjMatInfo(selMat, mnm, sizeof(mnm), alb, &rgh, &mtl);
						{
							const NkRect br{rowR.x, yy + S(2.f), rowR.w, kRowH - S(4.f)};
							p.Outline(br, NkRole::Border, NkRole::InputBg, 3.f);
							// ── LE DEROULANT EST UNE ICONE, PAS UN LIBELLE ─────
							// Avec son nom affiche, il empietait sur le champ du
							// NOM juste a cote : on ne distinguait plus les deux
							// (constate par Rihen). Chez Blender l'icone EST le
							// navigateur -- elle ouvre la liste, le champ voisin
							// porte le nom. Le mode « icone seule » du combo se
							// demande en refusant a la fois cadre et chevron.
							static const char *sNavPtr[64];
							static NkIcon sNavIc[64];
							for (int32 i = 0; i < nMats; ++i) {
								sNavPtr[i] = sMatNm[i];
								sNavIc[i] = NkIcon::Material;
							}
							int32 navSel = st.projMatSel;
							Combo(p, hit, ws, "props.pm.nav",
								  {br.x + S(2.f), br.y + S(1.f), S(26.f), br.h - S(2.f)},
								  sNavPtr, sNavIc, nMats, navSel, combo, true, false, false);
							if (navSel != st.projMatSel)
								st.projMatSel = navSel;
							// Un TRAIT separe les deux commandes : l'oeil voit
							// « ouvrir la liste » puis « le nom », pas un bloc.
							p.VLine(br.x + S(30.f), br.y + S(3.f), br.h - S(6.f));
							// NOM editable (double-clic), clippe a son cadre.
							static char sNavName[32] = {};
							const NkRect nmR{br.x + S(35.f), br.y, br.w - S(63.f), br.h};
							p.Clip(nmR);
							if (EditableText(p, hit, ws, in, "props.pm.rename",
											 {nmR.x + S(2.f), nmR.y - S(2.f), nmR.w, kRowH},
											 mnm, NkRole::Text, sNavName, 31u))
								demo::Demo3DHostProjMatSetName(selMat, sNavName);
							p.Unclip();
							// DELIER : l'objet actif n'a plus ce materiau -- il en
							// reprend un, jamais aucun.
							const NkRect xb{br.x + br.w - S(24.f), br.y + S(2.f), S(20.f),
											br.h - S(4.f)};
							const bool ovX = hit.Add("props.pm.unlink", xb);
							const bool enX = (actN >= 0 && curOf == selMat);
							p.Outline(xb, (ovX && enX) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 2.f);
							p.IconV(xb.x + S(4.f), xb.y, xb.h, NkIcon::WinClose,
									enX ? NkRole::Text : NkRole::TextMuted, 10.f);
							if (enX && hit.Clicked("props.pm.unlink"))
								demo::Demo3DHostProjMatAssign(actN, -1);
							yy += kRowH;
						}
						// ASSIGNER : geste du mode EDITION (regle de Rihen).
						if (demo::Demo3DHostInEditMode() && actN >= 0) {
							if (Button("props.pm.asg", yy, "Assigner", rowR.x, rowR.w))
								demo::Demo3DHostProjMatAssign(actN, selMat);
							yy += kRowH;
						}
						yy += NkPropGroupGap();
						// ── LES PROPRIETES DU MATERIAU SELECTIONNE ─────────────
						const bool grpMt = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.matp",
														  "Surface", 8192u);
						const float32 grpMtTop = yy;
						if (grpMt) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							// APERCU + colonne des FORMES (plan, sphere, cube,
							// liquide, cheveux -- structure de Blender).
							{
								const float32 side = S(104.f);
								const float32 btn = S(20.f);
								const float32 pvX = iR.x + (iR.w - side - btn - S(6.f)) * 0.5f;
								p.Image(4400u + (uint32)selMat, {pvX, yy, side, side});
								p.OutlineSharp({pvX, yy, side, side}, NkRole::Border);
								static const NkIcon kShp[5] = {NkIcon::Plane3D, NkIcon::SphereUV,
															   NkIcon::Cube3D, NkIcon::Metaball,
															   NkIcon::CurveBezier};
								const int32 shpCur = demo::Demo3DHostProjMatPrevShape(selMat);
								float32 by = yy;
								for (int32 s5 = 0; s5 < 5; ++s5) {
									snprintf(key, sizeof(key), "props.pm.s%d", s5);
									const NkRect sb{pvX + side + S(6.f), by, btn, btn};
									hit.Add(key, sb);
									if (shpCur == s5)
										p.Fill(sb, NkRole::AccentUi, 3.f);
									else
										p.Outline(sb, NkRole::Border, NkRole::InputBg, 3.f);
									p.IconV(sb.x + S(4.f), by, btn, kShp[s5],
											shpCur == s5 ? NkRole::TextOnAccent
														 : NkRole::TextMuted,
											12.f);
									if (hit.Clicked(key))
										demo::Demo3DHostProjMatSetPrevShape(selMat, s5);
									by += btn + S(1.f);
								}
								yy += side + NkGroupPad();
							}
							const float32 a0 = alb[0], a1 = alb[1], a2 = alb[2];
							const float32 r0 = rgh, m0 = mtl;
							bool colCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Couleur",
												"props.pm.col", alb, &colCh);
							// ── LES QUATRE CANAUX DE TEXTURE ────────────────────
							// Couleur, Normale, ORM, Emissif : le moteur les porte
							// depuis toujours, seule la couleur etait reglable ici.
							// UNE boucle pour les quatre -- quatre blocs recopies
							// auraient diverge au premier correctif.
							// Chaque canal a SON tampon de saisie statique : un
							// tampon partage ferait sauter le texte d'un champ a
							// l'autre pendant la frappe (NkComboPending a deja
							// coute cette lecon).
							{
								static char sPmTex[4][260] = {};
								static const char *const kPmKeys[4] = {
									"props.pm.tex0", "props.pm.tex1", "props.pm.tex2",
									"props.pm.tex3"};
								const int32 nCh = demo::Demo3DHostMatChanCount();
								for (int32 ch = 0; ch < nCh && ch < 4; ++ch) {
									p.TextV(iR.x, yy, kRowH, demo::Demo3DHostMatChanName(ch),
											NkRole::TextMuted);
									const NkRect txR{iR.x + S(110.f), yy + S(2.f),
													 iR.w - S(110.f) - S(22.f), kRowH - S(4.f)};
									p.Outline(txR, NkRole::Border, NkRole::InputBg, 3.f);
									const char *curTx = demo::Demo3DHostProjMatMap(selMat, ch);
									p.Clip(txR);
									const bool txApply = EditableText(
										p, hit, ws, in, kPmKeys[ch],
										{txR.x + S(4.f), yy, txR.w - S(8.f), kRowH},
										curTx[0] ? curTx : "aucune",
										curTx[0] ? NkRole::Text : NkRole::TextMuted,
										sPmTex[ch], 259u);
									p.Unclip();
									if (txApply)
										demo::Demo3DHostProjMatSetMap(selMat, ch, sPmTex[ch]);
									// RETIRER : n'apparait que si le canal porte
									// quelque chose -- un bouton qui n'a rien a
									// enlever n'a rien a faire la.
									if (curTx[0]) {
										char rk[24];
										snprintf(rk, sizeof(rk), "props.pm.texx%d", ch);
										const NkRect xr{iR.x + iR.w - S(20.f), yy + S(3.f),
														S(18.f), kRowH - S(6.f)};
										const bool ovx = hit.Add(rk, xr);
										p.Outline(xr, NkRole::Border,
												  ovx ? NkRole::PanelBg : NkRole::PanelHeader,
												  3.f);
										p.IconV(xr.x + S(3.f), yy, kRowH, NkIcon::Trash,
												NkRole::TextMuted, 11.f);
										if (hit.Clicked(rk))
											demo::Demo3DHostProjMatSetMap(selMat, ch, "-");
									}
									yy += kRowH;
								}
								// INTENSITES : elles ne s'affichent QUE si leur
								// texture existe -- un curseur sans effet est pire
								// qu'un curseur absent (regle du projet).
								float32 nrmS = 1.f, emiS = 1.f;
								demo::Demo3DHostProjMatChanStrength(selMat, &nrmS, &emiS);
								const float32 nrm0 = nrmS, emi0 = emiS;
								if (demo::Demo3DHostProjMatMap(selMat, 1)[0]) {
									p.TextV(iR.x, yy, kRowH, "Relief", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.nrms",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  nrmS, 0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
								}
								// PARALLAX : meme regle que le relief — le curseur
								// n'apparait qu'avec sa carte de hauteur (canal 4).
								if (demo::Demo3DHostProjMatMap(selMat, 4)[0]) {
									float32 par = demo::Demo3DHostProjMatParallax(selMat);
									const float32 par0 = par;
									p.TextV(iR.x, yy, kRowH, "Parallax", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.parx",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  par, 0.001f, NkRole::AccentUi, "%.3f");
									yy += kRowH;
									if (par != par0) {
										demo::Demo3DHostProjMatSetParallax(selMat, par);
										NkMarkDirty(st);
									}
								}
								// L'EMISSIF a une teinte ET une intensite, et la
								// teinte vaut MEME SANS texture : une surface peut
								// emettre une couleur unie.
								float32 emiC[3] = {0.f, 0.f, 0.f};
								demo::Demo3DHostProjMatEmissive(selMat, emiC);
								const float32 e0 = emiC[0], e1 = emiC[1], e2 = emiC[2];
								bool emiCh = false;
								yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Emission",
													"props.pm.emi", emiC, &emiCh);
								p.TextV(iR.x, yy, kRowH, "Intensite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.emis",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  emiS, 0.02f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (nrmS != nrm0 || emiS != emi0)
									demo::Demo3DHostProjMatSetChanStrength(selMat, nrmS, emiS);
								if (emiCh || emiC[0] != e0 || emiC[1] != e1 || emiC[2] != e2)
									demo::Demo3DHostProjMatSetEmissive(selMat, emiC);
							}
							p.TextV(iR.x, yy, kRowH, "Rugosite", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "props.pm.rgh",
									  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
									   kRowH - S(6.f)},
									  rgh, 0.005f, NkRole::AccentUi, "%.2f");
							yy += kRowH;
							p.TextV(iR.x, yy, kRowH, "Metallique", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "props.pm.mtl",
									  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
									   kRowH - S(6.f)},
									  mtl, 0.005f, NkRole::AccentUi, "%.2f");
							yy += kRowH;
							// ── PHYSIQUE DE SURFACE (passation §5, « gain le moins
							// cher ») : le shader calcule vernis et diffusion depuis
							// longtemps, seuls ces curseurs manquaient. La rugosite du
							// vernis n'apparait QUE si le vernis existe — un curseur
							// sans effet est pire qu'un curseur absent (regle du
							// projet). La couleur de diffusion suit l'albedo.
							{
								float32 cc = 0.f, ccR = 0.f, sss = 0.f;
								demo::Demo3DHostProjMatSurface(selMat, &cc, &ccR, &sss);
								const float32 cc0 = cc, ccR0 = ccR, sss0 = sss;
								p.TextV(iR.x, yy, kRowH, "Vernis", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.cc",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  cc, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (cc > 0.f) {
									p.TextV(iR.x, yy, kRowH, "Vernis rugosite",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.ccr",
											  {iR.x + S(110.f), yy + S(3.f),
											   iR.w - S(110.f), kRowH - S(6.f)},
											  ccR, 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
								}
								p.TextV(iR.x, yy, kRowH, "Diffusion", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.sss",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  sss, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (cc != cc0 || ccR != ccR0 || sss != sss0)
									demo::Demo3DHostProjMatSetSurface(selMat, cc, ccR, sss);
							}
							if (colCh || alb[0] != a0 || alb[1] != a1 || alb[2] != a2 ||
								rgh != r0 || mtl != m0)
								demo::Demo3DHostProjMatSetParams(selMat, alb, rgh, mtl);
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpMtTop, yy);
						}
						yy += NkPropGroupGap();
						// LES ACTIONS DU MENU, pour ce materiau.
						if (NkGrpWants(st, "prop.g.mat", 1)) {
							const float32 v5[5] = {alb[0], alb[1], alb[2], rgh, mtl};
							NkGrpCopyF(st, "prop.g.mat", v5, 5);
						}
						if (NkGrpWants(st, "prop.g.mat", 2) &&
							NkGrpCanPaste(st, "prop.g.mat")) {
							const float32 a5[3] = {st.grpClipF[0], st.grpClipF[1],
												   st.grpClipF[2]};
							demo::Demo3DHostProjMatSetParams(selMat, a5, st.grpClipF[3],
															 st.grpClipF[4]);
						}
						if (NkGrpWants(st, "prop.g.mat", 3)) {
							const float32 g5[3] = {0.7f, 0.7f, 0.7f};
							demo::Demo3DHostProjMatSetParams(selMat, g5, 0.85f, 0.f);
							// Un materiau neuf n'a ni vernis ni diffusion.
							demo::Demo3DHostProjMatSetSurface(selMat, 0.f, 0.f, 0.f);
							// REINITIALISER retire les QUATRE canaux : n'en
							// oublier qu'un laisserait un relief ou un emissif
							// invisible dans un materiau cense etre neuf.
							const int32 nCh2 = demo::Demo3DHostMatChanCount();
							for (int32 ch = 0; ch < nCh2; ++ch)
								demo::Demo3DHostProjMatSetMap(selMat, ch, "-");
							const float32 e0[3] = {0.f, 0.f, 0.f};
							demo::Demo3DHostProjMatSetEmissive(selMat, e0);
							demo::Demo3DHostProjMatSetChanStrength(selMat, 1.f, 1.f);
						}
					}
				} else if (sec == 6) {
					// ══ OUTPUT : CE QUI SORT DE LA SCENE (Rihen) ═══════════════
					// Une cible PRINCIPALE et, posees dessus, jusqu'a huit
					// INCRUSTATIONS de formes libres. La resolution ne depend
					// PAS de la taille de la fenetre : sans cela le champ ne
					// serait qu'une decoration.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── Sources disponibles : la vue 3D, puis chaque camera.
					// Construites une fois, elles servent la principale ET les
					// incrustations -- deux listes divergeraient.
					// ── LES LISTES DE COMBO DOIVENT SURVIVRE A LA FRAME ─────
					// NkComboPending garde les pointeurs `items` ET `selected`
					// pour peindre la liste deroulante PLUS TARD, par-dessus le
					// reste. Passer un tableau local revenait donc a lui confier
					// des adresses de pile deja mortes au moment ou elle peint
					// et ou elle ecrit le choix : le combo s'ouvrait, mais
					// choisir ne changeait rien (Rihen : « pourquoi on ne peut
					// pas choisir la source ? »). Tous les combos du projet
					// passent des tableaux statiques et un etat persistant ;
					// ceux-ci s'y conforment.
					int32 camNodes[16];
					const int32 nCam = demo::Demo3DHostSceneCameras(camNodes, 16);
					static char srcBuf[18][40];
					static const char *srcNames[18];
					// Plus d'entree « camera active » (Rihen) : elle est deja celle
					// qu'on voit, et une source qui se deplace toute seule rendait
					// imprevisible ce qu'on s'appretait a produire.
					snprintf(srcBuf[0], sizeof(srcBuf[0]), "Vue 3D");
					srcNames[0] = srcBuf[0];
					for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5) {
						// NkHierNodeName, PAS Demo3DHostObjectName : les NOEUDS
						// et les OBJETS sont deux espaces d'indices distincts.
						// Passer un numero de noeud a la fonction des objets
						// nommait un tout autre element de la scene -- une
						// camera s'annoncait « mur gi » (constate par Rihen).
						char cn[32] = {};
						NkHierNodeName(st, camNodes[c5], cn, sizeof(cn));
						// (Le depot du nom vers l'hote a ete deplace dans la
						// synchronisation generale de main.cpp : ici, il aurait
						// fallu avoir ouvert ce panneau au moins une fois pour
						// que les fichiers portent le bon nom -- une condition
						// qu'on ne devine pas.)
						snprintf(srcBuf[c5 + 1], sizeof(srcBuf[0]), "%s",
								 cn[0] ? cn : "Camera");
						srcNames[c5 + 1] = srcBuf[c5 + 1];
					}
					const int32 nSrc = nCam + 1;
					// Noeud <-> rang dans la liste. La liste bouge quand on
					// ajoute une camera ; le noeud, lui, ne bouge pas -- c'est
					// donc LUI qu'on memorise, et le rang se recalcule.
					// Rang 0 = vue 3D (-1), ensuite les cameras nommees. C'est le
					// NOEUD qu'on memorise, jamais le rang : ajouter une camera
					// decale la liste.
					auto srcIndexOf = [&](int32 node) {
						if (node < 0)
							return 0;
						for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5)
							if (camNodes[c5] == node)
								return c5 + 1;
						return 0;
					};
					auto srcNodeOf = [&](int32 idx) {
						return (idx <= 0 || idx - 1 >= nCam) ? -1 : camNodes[idx - 1];
					};
					// La source principale, lue AVANT de batir la liste des
					// miniatures : c'est elle qu'on en retire.
					int32 oSrcCur = -1;
					demo::Demo3DHostOutMain(&oSrcCur, nullptr, nullptr, nullptr, nullptr,
											nullptr);
					// ── LISTE DES MINIATURES : SANS LA SOURCE PRINCIPALE ────
					// Une vue n'est pas a la fois principale et miniature
					// (Rihen), donc la principale ne doit meme pas etre
					// PROPOSABLE ici -- seul un echange, ou le fait qu'elle
					// cesse d'etre principale, l'y ramene. Une entree qu'on ne
					// peut pas choisir n'a rien a faire dans une liste.
					static char insBuf[17][40];
					static const char *insNames[17];
					int32 insNodes[17];
					int32 nIns = 0;
					if (oSrcCur != -1) {
						snprintf(insBuf[nIns], sizeof(insBuf[0]), "Vue 3D");
						insNames[nIns] = insBuf[nIns];
						insNodes[nIns++] = -1;
					}
					for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5) {
						if (camNodes[c5] == oSrcCur)
							continue;
						char cn[32] = {};
						NkHierNodeName(st, camNodes[c5], cn, sizeof(cn));
						snprintf(insBuf[nIns], sizeof(insBuf[0]), "%s", cn[0] ? cn : "Camera");
						insNames[nIns] = insBuf[nIns];
						insNodes[nIns++] = camNodes[c5];
					}
					auto insIndexOf = [&](int32 node) {
						for (int32 i5 = 0; i5 < nIns; ++i5)
							if (insNodes[i5] == node)
								return i5;
						return 0;
					};
					auto insNodeOf = [&](int32 idx) {
						return (idx >= 0 && idx < nIns) ? insNodes[idx] : -1;
					};

					int32 oSrc = -1, oW = 1920, oH = 1080, oScale = 100, oFmt = 0;
					bool oTrans = false;
					demo::Demo3DHostOutMain(&oSrc, &oW, &oH, &oScale, &oFmt, &oTrans);
					const int32 oSrc0 = oSrc, oW0 = oW, oH0 = oH, oScale0 = oScale;
					const int32 oFmt0 = oFmt;
					const bool oTrans0 = oTrans;

					// ── GROUPE « SORTIE PRINCIPALE » ────────────────────────
					const bool gOut = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.out",
													 "Sortie principale", 8192u);
					const float32 gOutTop = yy;
					if (!gOut) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iO = NkGroupInner(rowR);
						const NkRect r{iO.x - kPad, rowR.y, iO.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx = r.x + S(104.f);
						const float32 fw = rr.w - S(112.f);
						p.TextV(r.x + kPad, yy, kRowH, "Source", NkRole::TextMuted);
						{
							// La selection vit dans un STATIC : la liste
							// deroulante y ecrit apres la fin de ce bloc (voir
							// la note sur NkComboPending plus haut).
							// IL FAUT DISTINGUER DEUX CHANGEMENTS. Comparer le
							// static a la verite moteur ne suffit pas : quand
							// c'est le MOTEUR qui a bouge -- un echange
							// principale/miniature, le pave 0 -- l'ecart se lit
							// comme un choix de l'utilisateur, et on reapplique
							// l'ancienne valeur. L'echange etait ainsi annule a
							// l'image suivante (constate par Rihen). On memorise
							// donc la derniere valeur VUE du moteur : s'il a
							// change, il gagne ; sinon seul le combo parle.
							static int32 sSrcSel = 0, sSrcSeen = -999;
							const int32 cur = srcIndexOf(oSrc);
							if (cur != sSrcSeen) {
								sSrcSel = cur;
								sSrcSeen = cur;
							} else if (sSrcSel != cur && sSrcSel >= 0 && sSrcSel < nSrc) {
								oSrc = srcNodeOf(sSrcSel);
								sSrcSeen = sSrcSel;
							}
							Combo(p, hit, ws, "out.src", {fx, yy + S(2.f), fw, kRowH - S(4.f)},
								  srcNames, nullptr, nSrc, sSrcSel, combo);
						}
						yy += kRowH;
						// ── FORMAT : UNE LISTE, DONT UNE ENTREE « LIBRE » ───────
						// Huit boutons de preset prenaient deux rangees et
						// laissaient croire qu'on pouvait a la fois choisir un
						// format ET taper autre chose (Rihen). Une liste dit
						// l'etat sans ambiguite : sur un format nomme, les deux
						// champs sont GRISES et affichent ce qu'il impose ; sur
						// « Libre », ils s'editent.
						struct OPre {
								const char *n;
								int32 w, h;
						};
						static const OPre kPre[9] = {
							{"Libre", 0, 0},		  {"HD 1280 x 720", 1280, 720},
							{"Full HD 1920 x 1080", 1920, 1080},
							{"2K 2560 x 1440", 2560, 1440},
							{"4K 3840 x 2160", 3840, 2160},
							{"Carre 1080", 1080, 1080},
							{"Vertical 1080 x 1920", 1080, 1920},
							{"Cinema 2048 x 858", 2048, 858},
							{"Web 1200 x 630", 1200, 630}};
						static const char *preNames[9];
						for (int32 q = 0; q < 9; ++q)
							preNames[q] = kPre[q].n;
						// LE FORMAT NOMME SE DEDUIT de la resolution -- c'est elle
						// la verite, et taper 1920x1080 affiche « Full HD » tout
						// seul. « LIBRE » NE SE DEDUIT DE RIEN : il a sa propre
						// memoire cote moteur, sans quoi le choisir sur une
						// resolution qui vaut un format connu etait annule a
						// l'image suivante (Rihen).
						bool freeSz = demo::Demo3DHostOutFreeSize();
						int32 match = 0;
						for (int32 q = 1; q < 9; ++q)
							if (oW == kPre[q].w && oH == kPre[q].h) {
								match = q;
								break;
							}
						// Une resolution qui ne correspond a aucun format EST
						// libre : l'etat suit, il ne peut pas dire autre chose.
						if (match == 0)
							freeSz = true;
						int32 preCur = freeSz ? 0 : match;
						p.TextV(r.x + kPad, yy, kRowH, "Format", NkRole::TextMuted);
						{
							static int32 sPreSel = 0, sPreSeen = -999;
							if (preCur != sPreSeen) {
								sPreSel = preCur;
								sPreSeen = preCur;
							} else if (sPreSel != preCur) {
								if (sPreSel <= 0) {
									freeSz = true; // la resolution ne bouge pas
								} else if (sPreSel < 9) {
									freeSz = false;
									oW = kPre[sPreSel].w;
									oH = kPre[sPreSel].h;
								}
								sPreSeen = sPreSel;
								preCur = sPreSel <= 0 ? 0 : sPreSel;
							}
							Combo(p, hit, ws, "out.pre", {fx, yy + S(2.f), fw, kRowH - S(4.f)},
								  preNames, nullptr, 9, sPreSel, combo);
						}
						if (freeSz != demo::Demo3DHostOutFreeSize()) {
							demo::Demo3DHostSetOutFreeSize(freeSz);
							NkMarkDirty(st);
						}
						yy += kRowH;
						// RESOLUTION : deux champs cote a cote, comme Blender.
						// Il n'existe pas de DragInt : le glissement se fait en
						// reel puis s'arrondit -- une seule mecanique de champ
						// dans toute l'interface, donc un seul comportement a
						// apprendre.
						p.TextV(r.x + kPad, yy, kRowH, "Resolution",
								preCur == 0 ? NkRole::TextMuted : NkRole::TextMuted);
						{
							const float32 half = (fw - S(6.f)) * 0.5f;
							if (preCur == 0) {
								float32 fwv = (float32)oW, fhv = (float32)oH;
								DragFloat(p, hit, ws, in, "out.rw",
										  {fx, yy + S(3.f), half, kRowH - S(6.f)}, fwv, 1.f,
										  NkRole::AxisX, "%.0f");
								DragFloat(p, hit, ws, in, "out.rh",
										  {fx + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
										  fhv, 1.f, NkRole::AxisY, "%.0f");
								oW = (int32)(fwv + 0.5f);
								oH = (int32)(fhv + 0.5f);
							} else {
								// GRISES : le format nomme impose ces valeurs.
								// On les MONTRE quand meme -- un champ vide ne
								// dirait pas ce qui va sortir.
								char rb[24];
								const NkRect b1{fx, yy + S(3.f), half, kRowH - S(6.f)};
								const NkRect b2{fx + half + S(6.f), yy + S(3.f), half,
												kRowH - S(6.f)};
								p.Outline(b1, NkRole::Border, NkRole::PanelBg, 3.f);
								p.Outline(b2, NkRole::Border, NkRole::PanelBg, 3.f);
								snprintf(rb, sizeof(rb), "%d", (int)oW);
								p.TextV(b1.x + S(6.f), b1.y, b1.h, rb, NkRole::TextMuted);
								snprintf(rb, sizeof(rb), "%d", (int)oH);
								p.TextV(b2.x + S(6.f), b2.y, b2.h, rb, NkRole::TextMuted);
							}
						}
						yy += kRowH;
						// (Les huit boutons de preset ont ete remplaces par la
						// liste « Format » ci-dessus, dont chaque entree porte
						// sa taille reelle -- Rihen.)
						p.TextV(r.x + kPad, yy, kRowH, "Echelle", NkRole::TextMuted);
						{
							float32 fsc = (float32)oScale;
							DragFloat(p, hit, ws, in, "out.scl",
									  {fx, yy + S(3.f), fw, kRowH - S(6.f)}, fsc, 1.f,
									  NkRole::AccentUi, "%.0f %%");
							oScale = (int32)(fsc + 0.5f);
						}
						yy += kRowH;
						// CE QUI SERA REELLEMENT PRODUIT, en toutes lettres : la
						// resolution seule ne le dit pas des que l'echelle n'est
						// plus a 100 %.
						{
							int32 ew = 0, eh = 0;
							demo::Demo3DHostOutEffectiveSize(&ew, &eh);
							char eb[64];
							snprintf(eb, sizeof(eb), "Image produite : %d x %d px", ew, eh);
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, eb, NkRole::TextMuted);
							yy += kRowH;
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gOutTop, yy);
						yy += NkPropGroupGap();
					}
					// (Le commit de ces valeurs a ete deplace APRES le groupe
					// Destination : le FORMAT et le FOND TRANSPARENT s'y editent,
					// et un commit place ici les testait AVANT qu'ils ne changent
					// -- la case « Fond transparent » ne se cochait donc jamais,
					// et le format choisi n'atteignait pas le moteur. Un commit
					// doit venir apres TOUTES les ecritures de ses variables.)

					// ── GROUPE « DESTINATION » ──────────────────────────────
					const bool gDst = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outdst",
													 "Destination", 16384u);
					const float32 gDstTop = yy;
					if (!gDst) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iD = NkGroupInner(rowR);
						const NkRect r{iD.x - kPad, rowR.y, iD.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx = r.x + S(104.f);
						const float32 fw = rr.w - S(112.f);
						p.TextV(r.x + kPad, yy, kRowH, "Dossier", NkRole::TextMuted);
						{
							char dbuf[260] = {};
							if (EditableText(p, hit, ws, in, "out.dir",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostOutDir(), NkRole::Text, dbuf,
											 sizeof(dbuf))) {
								demo::Demo3DHostSetOutDir(dbuf);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Nom (rendu)", NkRole::TextMuted);
						{
							char nbuf[64] = {};
							if (EditableText(p, hit, ws, in, "out.name",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostOutName(), NkRole::Text, nbuf,
											 sizeof(nbuf))) {
								demo::Demo3DHostSetOutName(nbuf);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						// LES TROIS NOMS ENSEMBLE (Rihen) : rendu, capture de la
						// vue et tutoriel partagent TOUTES les proprietes de
						// sortie -- dossier, format, qualite -- et ne different
						// que par leur nom de base. Les separer dans un groupe a
						// part laissait croire a des reglages independants.
						p.TextV(r.x + kPad, yy, kRowH, "Nom (vue)", NkRole::TextMuted);
						{
							char cb1[64] = {};
							if (EditableText(p, hit, ws, in, "out.capv",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostCaptureName(1), NkRole::Text, cb1,
											 sizeof(cb1))) {
								demo::Demo3DHostSetCaptureName(1, cb1);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Nom (tutoriel)", NkRole::TextMuted);
						{
							char cb2[64] = {};
							if (EditableText(p, hit, ws, in, "out.capt",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostCaptureName(2), NkRole::Text, cb2,
											 sizeof(cb2))) {
								demo::Demo3DHostSetCaptureName(2, cb2);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						// FORMAT : ceux que le moteur d'images sait REELLEMENT
						// ecrire. WebP et SVG y sont declares mais annonces
						// « non implemente » -- les proposer aurait produit des
						// fichiers vides.
						{
							const int32 nF = demo::Demo3DHostOutFormatCount();
							static char fmtBuf[12][20];
							static const char *fmtNames[12];
							for (int32 f5 = 0; f5 < nF && f5 < 12; ++f5) {
								snprintf(fmtBuf[f5], sizeof(fmtBuf[0]), "%s",
										 demo::Demo3DHostOutFormatName(f5));
								fmtNames[f5] = fmtBuf[f5];
							}
							p.TextV(r.x + kPad, yy, kRowH, "Format", NkRole::TextMuted);
							{
								static int32 sFmtSel = 0;
								if (sFmtSel != oFmt && sFmtSel >= 0 && sFmtSel < nF)
									oFmt = sFmtSel;
								else
									sFmtSel = oFmt;
								Combo(p, hit, ws, "out.fmt",
									  {fx, yy + S(2.f), fw, kRowH - S(4.f)}, fmtNames, nullptr,
									  nF < 12 ? nF : 12, sFmtSel, combo);
							}
							yy += kRowH;
							// L'EXTENSION EN CLAIR : c'est elle qui decide de
							// l'encodeur, autant la montrer.
							{
								char eb2[48];
								snprintf(eb2, sizeof(eb2), "Extension : .%s",
										 demo::Demo3DHostOutFormatExt(oFmt));
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, eb2, NkRole::TextMuted);
								yy += kRowH;
							}
							// FOND TRANSPARENT : seulement si le format porte
							// l'alpha. Le proposer pour un JPEG donnerait un fond
							// NOIR sans le dire -- une case qui ment est pire
							// qu'une case absente.
							if (demo::Demo3DHostOutFormatAlpha(oFmt)) {
								const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovt = hit.Add("out.transp", cb);
								if (oTrans)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovt ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (oTrans)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH, "Fond transparent");
								p.Unclip();
								if (hit.Clicked("out.transp"))
									oTrans = !oTrans;
								yy += kRowH;
								if (oTrans) {
									yy += p.TextWrap(r.x + kPad + S(24.f), yy,
													 rr.w - 2.f * kPad - S(24.f),
													 "Le ciel et le fond sont coupes : seule la "
													 "scene est ecrite.",
													 NkRole::TextMuted);
									yy += S(4.f);
								}
							}
							// QUALITE : seulement pour un format qui perd de
							// l'information. L'afficher pour le PNG ferait
							// croire qu'elle y change quelque chose.
							if (demo::Demo3DHostOutFormatLossy(oFmt)) {
								p.TextV(r.x + kPad, yy, kRowH, "Qualite", NkRole::TextMuted);
								float32 q5 = (float32)demo::Demo3DHostOutQuality();
								if (DragFloat(p, hit, ws, in, "out.qual",
											  {fx, yy + S(3.f), fw, kRowH - S(6.f)}, q5, 1.f,
											  NkRole::AccentUi, "%.0f")) {
									demo::Demo3DHostSetOutQuality((int32)(q5 + 0.5f));
									NkMarkDirty(st);
								}
								yy += kRowH;
							}
						}
						{
							char lb[300];
							const char *lp = demo::Demo3DHostOutLastPath();
							if (lp && lp[0]) {
								snprintf(lb, sizeof(lb), "Dernier : %s%s", lp,
										 demo::Demo3DHostOutLastOk() ? "" : "  (ECHEC)");
								p.Clip({r.x + kPad, yy, rr.w - 2.f * kPad, kRowH});
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, lb, NkRole::TextMuted);
								p.Unclip();
								yy += kRowH;
							}
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gDstTop, yy);
						yy += NkPropGroupGap();
					}
					// COMMIT APRES TOUTES LES ECRITURES : source, resolution et
					// echelle viennent de « Sortie principale », format, qualite
					// et fond transparent de « Destination ». Les commiter entre
					// les deux groupes ne voyait que la moitie des changements.
					if (oSrc != oSrc0 || oW != oW0 || oH != oH0 || oScale != oScale0 ||
						oFmt != oFmt0 || oTrans != oTrans0) {
						demo::Demo3DHostSetOutMain(oSrc, oW, oH, oScale, oFmt, oTrans);
						NkMarkDirty(st);
					}

					// ── GROUPE « TYPES DE RENDU » (Rihen) ───────────────────
					// Chaque type coche produit SON image, incrustations
					// comprises, et le fichier porte son suffixe. Rien de coche
					// = le mode courant de la vue, et lui seul : cocher ne doit
					// pas etre un prealable pour rendre ce qu'on a sous les
					// yeux.
					const bool gMod = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outmod",
													 "Types de rendu", 65536u);
					const float32 gModTop = yy;
					if (!gMod) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iM = NkGroupInner(rowR);
						const NkRect r{iM.x - kPad, rowR.y, iM.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 nM = demo::Demo3DHostOutModeCount();
						int32 mask = demo::Demo3DHostOutModes();
						const int32 mask0 = mask;
						for (int32 m5 = 0; m5 < nM; ++m5) {
							char mk[24];
							snprintf(mk, sizeof(mk), "out.mode.%d", m5);
							const bool on5 = (mask & (1 << m5)) != 0;
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovm = hit.Add(mk, cb);
							if (on5)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovm ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (on5)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									demo::Demo3DHostOutModeName(m5));
							if (hit.Clicked(mk))
								mask ^= (1 << m5);
							yy += kRowH;
						}
						if (mask != mask0) {
							demo::Demo3DHostSetOutModes(mask);
							NkMarkDirty(st);
						}
						{
							int32 nOnM = 0;
							for (int32 m5 = 0; m5 < nM; ++m5)
								if (mask & (1 << m5))
									++nOnM;
							char mb[96];
							if (nOnM == 0)
								snprintf(mb, sizeof(mb),
										 "Aucun coche : le mode courant de la vue, une image.");
							else
								snprintf(mb, sizeof(mb), "%d image(s), une par type coche.",
										 (int)nOnM);
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad, mb,
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gModTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « AIDES DANS LE RENDU » (Rihen) ──────────────
					// Coupees par defaut : une lumiere n'existe dans une image
					// que par son effet, pas par son symbole. Mais on veut
					// parfois montrer justement la grille ou le cadre d'une
					// camera -- une planche pedagogique, une explication. Le
					// masquage est donc une OPTION.
					// « Tutoriel » n'y est pas soumis : il photographie la
					// fenetre telle qu'elle est, c'est tout son propos.
					const bool gAid = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outaid",
													 "Aides dans le rendu", 524288u);
					const float32 gAidTop = yy;
					if (!gAid) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iA = NkGroupInner(rowR);
						const NkRect r{iA.x - kPad, rowR.y, iA.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 nA = demo::Demo3DHostOutAidCount();
						int32 aids = demo::Demo3DHostOutAids();
						const int32 aids0 = aids;
						for (int32 a5 = 0; a5 < nA; ++a5) {
							char ak[24];
							snprintf(ak, sizeof(ak), "out.aid.%d", a5);
							const bool on6 = (aids & (1 << a5)) != 0;
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ova = hit.Add(ak, cb);
							if (on6)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ova ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (on6)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									demo::Demo3DHostOutAidName(a5));
							if (hit.Clicked(ak))
								aids ^= (1 << a5);
							yy += kRowH;
						}
						if (aids != aids0) {
							demo::Demo3DHostSetOutAids(aids);
							NkMarkDirty(st);
						}
						// BLOC DE TEXTE QUI VA A LA LIGNE : il epouse la largeur du
						// groupe au lieu de deborder, et sa hauteur est celle qu'il
						// occupe reellement -- retrecir le panneau ajoute une ligne
						// au lieu de tronquer la phrase.
						yy += S(4.f);
						yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
										 "Decochee, l'aide est coupee au rendu. Cochee, elle "
										 "laisse passer ce que l'affichage montre.",
										 NkRole::TextMuted);
						yy += S(4.f);
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gAidTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « VIDEO » ────────────────────────────────────
					// La configuration se REGLE et se conserve des maintenant ;
					// le rendu viendra plus tard (decision de Rihen). Rien ici
					// ne pretend l'executer : pas de bouton « Rendre la video »
					// qui ne rendrait rien -- une commande factice est ce que
					// le principe « fonctionnalites a la naissance » interdit.
					const bool gVid = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outvid",
													 "Video", 131072u);
					const float32 gVidTop = yy;
					if (!gVid) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iV = NkGroupInner(rowR);
						const NkRect r{iV.x - kPad, rowR.y, iV.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx3 = r.x + S(104.f);
						const float32 fw3 = rr.w - S(112.f);
						bool vOn = false;
						int32 vFps = 25, vA = 1, vB = 250, vCod = 0;
						demo::Demo3DHostOutVideo(&vOn, &vFps, &vA, &vB, &vCod);
						const bool vOn0 = vOn;
						const int32 f0v = vFps, a0v = vA, b0v = vB, c0v = vCod;
						p.TextV(r.x + kPad, yy, kRowH, "Images / s", NkRole::TextMuted);
						{
							float32 ff = (float32)vFps;
							DragFloat(p, hit, ws, in, "out.fps",
									  {fx3, yy + S(3.f), fw3, kRowH - S(6.f)}, ff, 0.5f,
									  NkRole::AccentUi, "%.0f");
							vFps = (int32)(ff + 0.5f);
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Plage", NkRole::TextMuted);
						{
							const float32 half = (fw3 - S(6.f)) * 0.5f;
							float32 fa = (float32)vA, fb = (float32)vB;
							DragFloat(p, hit, ws, in, "out.fa",
									  {fx3, yy + S(3.f), half, kRowH - S(6.f)}, fa, 1.f,
									  NkRole::AxisX, "%.0f");
							DragFloat(p, hit, ws, in, "out.fb",
									  {fx3 + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
									  fb, 1.f, NkRole::AxisY, "%.0f");
							vA = (int32)(fa + 0.5f);
							vB = (int32)(fb + 0.5f);
						}
						yy += kRowH;
						// ── CONTENEUR PUIS CODEC (Rihen, comme Blender) ─────
						// Une seule liste melangeait les deux et cachait que le
						// meme MJPEG sert dans AVI et dans MOV, et que l'AVI sait
						// aussi ecrire du non compresse. Le conteneur dit le
						// FICHIER, le codec dit COMMENT les images y entrent.
						p.TextV(r.x + kPad, yy, kRowH, "Conteneur", NkRole::TextMuted);
						{
							static const char *sCont[8] = {};
							static int32 sContN = 0;
							if (sContN == 0) {
								sContN = demo::Demo3DHostOutVidContCount();
								if (sContN > 8)
									sContN = 8;
								for (int32 c = 0; c < sContN; ++c)
									sCont[c] = demo::Demo3DHostOutVidContName(c);
							}
							static int32 sContSel = 0;
							if (sContSel != vCod && sContSel >= 0 && sContSel < sContN)
								vCod = sContSel;
							else
								sContSel = vCod;
							Combo(p, hit, ws, "out.cont",
								  {fx3, yy + S(2.f), fw3, kRowH - S(4.f)}, sCont, nullptr,
								  sContN, sContSel, combo);
							yy += kRowH;
						}
						// LE CODEC DEPEND DU CONTENEUR : proposer H.264 sous AVI
						// promettrait un fichier que NKMedia ne sait pas ecrire.
						// La liste se reconstruit donc a chaque changement.
						{
							p.TextV(r.x + kPad, yy, kRowH, "Codec", NkRole::TextMuted);
							static const char *sCod[6] = {};
							static int32 sCodN = 0, sCodFor = -1;
							if (sCodFor != vCod) {
								sCodFor = vCod;
								sCodN = demo::Demo3DHostOutVidCodCount(vCod);
								if (sCodN > 6)
									sCodN = 6;
								for (int32 k = 0; k < sCodN; ++k)
									sCod[k] = demo::Demo3DHostOutVidCodName(vCod, k);
							}
							const int32 cod0 = demo::Demo3DHostOutVidCod();
							static int32 sCodSel = 0;
							if (sCodSel != cod0 && sCodSel >= 0 && sCodSel < sCodN) {
								demo::Demo3DHostSetOutVidCod(sCodSel);
								NkMarkDirty(st);
							} else {
								sCodSel = cod0;
							}
							Combo(p, hit, ws, "out.codec",
								  {fx3, yy + S(2.f), fw3, kRowH - S(4.f)}, sCod, nullptr, sCodN,
								  sCodSel, combo);
							yy += kRowH;
						}
						// QUALITE PROPRE A LA VIDEO (Rihen) : elle etait partagee
						// avec l'image fixe, si bien que soigner un rendu JPEG
						// alourdissait toutes les prises -- deux usages, deux
						// reglages. Sans objet pour la suite d'images PNG, qui est
						// sans perte : on ne montre pas un levier sans effet.
						if (vCod != 0) {
							p.TextV(r.x + kPad, yy, kRowH, "Qualite video", NkRole::TextMuted);
							float32 vq = (float32)demo::Demo3DHostOutVideoQuality();
							const float32 vq0 = vq;
							DragFloat(p, hit, ws, in, "out.vq",
									  {fx3, yy + S(3.f), fw3, kRowH - S(6.f)}, vq, 0.5f,
									  NkRole::AccentUi, "%.0f");
							if (vq < 1.f)
								vq = 1.f;
							if (vq > 100.f)
								vq = 100.f;
							if ((int32)(vq + 0.5f) != (int32)(vq0 + 0.5f)) {
								demo::Demo3DHostSetOutVideoQuality((int32)(vq + 0.5f));
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(
								r.x + kPad, yy, rr.w - 2.f * kPad,
								vCod == 4
									? "Convertie en QP H.264 (borne 12-48). Le MP4 est encode "
									  "APRES la prise : on filme en images, l'encodeur travaille "
									  "ensuite -- sans quoi la video sortait onze fois trop "
									  "rapide."
									: "Compression image par image : chaque image du fichier est "
									  "independante des autres.",
								NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── LE CURSEUR DANS LA VIDEO DE TUTORIEL (Rihen) ────
						// La capture de fenetre de l'OS ne contient PAS le
						// pointeur : sans ce dessin, la video montre des menus
						// qui s'ouvrent tout seuls.
						{
							const bool cu = demo::Demo3DHostOutCursor();
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovc = hit.Add("out.cursor", cb);
							if (cu)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovc ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (cu)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									"Curseur et sa trace (tutoriel)");
							p.Unclip();
							if (hit.Clicked("out.cursor")) {
								demo::Demo3DHostSetOutCursor(!cu);
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(r.x + kPad + S(24.f), yy,
											 rr.w - 2.f * kPad - S(24.f),
											 "Ne concerne que la video de la fenetre entiere : "
											 "une image fixe n'a pas de trajectoire a montrer.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── CONSERVER LES IMAGES QOI (Rihen) ────────────────
						// Decoche par defaut : le dossier nom_qoi_numero
						// s'efface une fois la video construite. Coche, il
						// reste -- source de montage sans perte.
						{
							const bool kq = demo::Demo3DHostOutKeepQoi();
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovq = hit.Add("out.keepqoi", cb);
							if (kq)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovq ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (kq)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									"Conserver les images QOI");
							p.Unclip();
							if (hit.Clicked("out.keepqoi")) {
								demo::Demo3DHostSetOutKeepQoi(!kq);
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(r.x + kPad + S(24.f), yy,
											 rr.w - 2.f * kPad - S(24.f),
											 "La prise filme en images QOI sans perte, la video "
											 "se construit a l'arret. Cochee, le dossier "
											 "nom_qoi_numero reste apres l'encodage.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── ENREGISTRER LA SESSION (Rihen) ──────────────────
						// On filme ce qui SE PASSE dans la vue. Pas de plage
						// d'images : sans timeline, elle produirait la meme
						// image repetee -- le rendu d'animation viendra pour
						// les captures quand la timeline existera.
						{
							const bool rec = demo::Demo3DHostRecActive();
							const float32 bw3 = (rr.w - 2.f * kPad - S(6.f)) * 0.5f;
							if (!rec) {
								// EN SATURATION : le choix appartient a
								// l'utilisateur (Rihen). Reglable seulement hors
								// prise -- la file est dimensionnee au demarrage.
								{
									const bool gr = demo::Demo3DHostRecGrow();
									const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
													kRowH - S(8.f)};
									const bool ovg = hit.Add("out.rec.grow", cb);
									if (gr)
										p.Fill(cb, NkRole::AccentUi, 3.f);
									else
										p.Outline(cb, NkRole::Border,
												  ovg ? NkRole::PanelHeader : NkRole::PanelBg,
												  3.f);
									if (gr)
										p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
												NkRole::TextOnAccent, 11.f);
									p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
									p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
											"Ne perdre aucune image");
									p.Unclip();
									if (hit.Clicked("out.rec.grow")) {
										demo::Demo3DHostSetRecGrow(!gr);
										NkMarkDirty(st);
									}
									yy += kRowH;
									yy += p.TextWrap(r.x + kPad + S(24.f), yy,
													 rr.w - 2.f * kPad - S(24.f),
													 gr ? "L'application attend l'encodeur quand "
														  "il prend du retard."
														: "Les images en trop sont sautees : "
														  "l'application reste fluide.",
													 NkRole::TextMuted);
									yy += S(6.f);
								}
								if (Button("out.rec.go", yy, "Enregistrer la vue",
										   r.x + kPad, rr.w - 2.f * kPad))
									demo::Demo3DHostRecStart();
								yy += kRowH;
							} else {
								// PAUSE / REPRENDRE : on suspend sans fermer le
								// fichier -- le montage n'aura aucune trace du
								// temps arrete (Rihen).
								const bool pz = demo::Demo3DHostRecPaused();
								if (Button("out.rec.pause", yy,
										   pz ? "Reprendre" : "Pause", r.x + kPad, bw3))
									demo::Demo3DHostRecPause(!pz);
								if (Button("out.rec.stop", yy, "Arreter et garder",
										   r.x + kPad + bw3 + S(6.f), bw3))
									demo::Demo3DHostRecStop(true);
								yy += kRowH;
								// ABANDONNER : ferme ET efface. Sans lui, une
								// prise ratee laisserait un fichier a supprimer
								// a la main, et on hesiterait a enregistrer.
								if (Button("out.rec.drop", yy, "Abandonner (rien n'est garde)",
										   r.x + kPad, rr.w - 2.f * kPad))
									demo::Demo3DHostRecStop(false);
								yy += kRowH;
								const int32 nf = demo::Demo3DHostRecFrames();
								const int32 nd = demo::Demo3DHostRecDropped();
								const float32 secs =
									(vFps > 0) ? (float32)nf / (float32)vFps : 0.f;
								char rb2[128];
								if (nd > 0)
									snprintf(rb2, sizeof(rb2),
											 "%d images -- %.1f s  (%d sautee(s))", (int)nf,
											 (double)secs, (int)nd);
								else
									snprintf(rb2, sizeof(rb2), "%d images -- %.1f s%s", (int)nf,
											 (double)secs, pz ? "  [en pause]" : "");
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, rb2,
										pz ? NkRole::TextMuted : NkRole::AccentUi);
								yy += kRowH;
								p.Clip({r.x + kPad, yy, rr.w - 2.f * kPad, kRowH});
								p.TextV(r.x + kPad + S(8.f), yy, kRowH,
										demo::Demo3DHostRecPath(), NkRole::TextMuted);
								p.Unclip();
								yy += kRowH;
							}
						}
						// ── CADENCE DE CAPTURE (Rihen) ──────────────────────
						// Une image fixe coute trois passes ; une video ne peut
						// pas se le permettre, et c'est la LECTURE des pixels qui
						// coute -- elle synchronise le processeur sur la carte.
						// Chacun de ces leviers a sa contrepartie, d'ou des
						// cases plutot qu'un comportement impose.
						yy += S(4.f);
						{
							const int32 nF2 = demo::Demo3DHostOutFastCount();
							int32 fm = demo::Demo3DHostOutFastMask();
							const int32 fm0 = fm;
							for (int32 a6 = 0; a6 < nF2; ++a6) {
								char fk2[24];
								snprintf(fk2, sizeof(fk2), "out.fast.%d", a6);
								const bool on7 = (fm & (1 << a6)) != 0;
								const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovf = hit.Add(fk2, cb);
								if (on7)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovf ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (on7)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
										demo::Demo3DHostOutFastName(a6));
								p.Unclip();
								if (hit.Clicked(fk2))
									fm ^= (1 << a6);
								yy += kRowH;
							}
							if (fm != fm0) {
								demo::Demo3DHostSetOutFastMask(fm);
								NkMarkDirty(st);
							}
						}
						yy += S(3.f);
						yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
										 "L'enregistrement filme la vue a sa resolution, sans "
										 "redimensionner sa cible : une seule lecture par image. "
										 "La plage ci-dessus servira au rendu d'animation, quand "
										 "la timeline existera.",
										 NkRole::TextMuted);
						yy += S(6.f);
						if (vOn != vOn0 || vFps != f0v || vA != a0v || vB != b0v || vCod != c0v) {
							demo::Demo3DHostSetOutVideo(vOn, vFps, vA, vB, vCod);
							NkMarkDirty(st);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gVidTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « INCRUSTATIONS » ────────────────────────────
					// Les cibles secondaires posees SUR la principale. Position
					// et taille sont des FRACTIONS : changer la resolution ne
					// deplace donc aucune incrustation.
					const bool gIns = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outins",
													 "Incrustations", 32768u);
					const float32 gInsTop = yy;
					if (!gIns) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iI = NkGroupInner(rowR);
						const NkRect r{iI.x - kPad, rowR.y, iI.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 maxIns = demo::Demo3DHostOutInsetMax();
						int32 nUsed = 0;
						for (int32 k5 = 0; k5 < maxIns; ++k5)
							if (demo::Demo3DHostOutInset(k5, nullptr, nullptr, nullptr, nullptr,
														 nullptr, nullptr, nullptr))
								++nUsed;
						if (nUsed == 0) {
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
											 "Aucune incrustation : l'image sort telle quelle.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// Formes, nommees par le moteur : une seule liste, donc
						// aucun risque de dire « Cercle » et d'en rendre un autre.
						const int32 nShape = demo::Demo3DHostOutInsetShapeCount();
						static char shpBuf[8][24];
						static const char *shpNames[8];
						// Une selection PAR incrustation, persistante : la liste
						// deroulante y ecrit apres la fin de la boucle. Et une
						// memoire de ce qu'on a VU du moteur, pour ne pas
						// prendre un echange principale/miniature pour un choix
						// de l'utilisateur -- voir la note du combo de source.
						static int32 sInsSrcSel[8] = {};
						static int32 sInsSrcSeen[8] = {-999, -999, -999, -999,
													   -999, -999, -999, -999};
						static int32 sInsShpSel[8] = {};
						static int32 sInsShpSeen[8] = {-999, -999, -999, -999,
													   -999, -999, -999, -999};
						for (int32 s5 = 0; s5 < nShape && s5 < 8; ++s5) {
							snprintf(shpBuf[s5], sizeof(shpBuf[0]), "%s",
									 demo::Demo3DHostOutInsetShapeName(s5));
							shpNames[s5] = shpBuf[s5];
						}
						for (int32 k5 = 0; k5 < maxIns; ++k5) {
							int32 iSrc = -1, iShape = 0;
							float32 iXY[2] = {0.f, 0.f}, iSz[2] = {0.25f, 0.25f}, iBrd = 2.f,
									iCol[3] = {1.f, 1.f, 1.f}, iOpa = 1.f;
							if (!demo::Demo3DHostOutInset(k5, &iSrc, &iShape, iXY, iSz, &iBrd,
														  iCol, &iOpa))
								continue;
							const int32 s0 = iSrc, sh0 = iShape;
							const float32 x0 = iXY[0], y0 = iXY[1], sz0 = iSz[0], sz1 = iSz[1],
										  b0 = iBrd, o0 = iOpa;
							char key2[32], lbl[40];
							snprintf(lbl, sizeof(lbl), "Incrustation %d", (int)(k5 + 1));
							// En-tete de l'incrustation + sa suppression.
							{
								const NkRect hr{r.x + kPad, yy, rr.w - 2.f * kPad, kRowH};
								p.Fill(hr, NkRole::PanelHeader, 3.f);
								p.TextV(hr.x + S(8.f), yy, kRowH, lbl);
								snprintf(key2, sizeof(key2), "out.ins.del.%d", k5);
								const NkRect db{hr.x + hr.w - S(24.f), yy + S(3.f), S(20.f),
												kRowH - S(6.f)};
								const bool ovd = hit.Add(key2, db);
								HoverFill(p, db, ovd, 3.f);
								p.IconV(db.x + S(3.f), yy, kRowH, NkIcon::Trash,
										ovd ? NkRole::AxisX : NkRole::TextMuted, 12.f);
								if (hit.Clicked(key2)) {
									demo::Demo3DHostOutInsetDelete(k5);
									yy += kRowH;
									continue;
								}
								yy += kRowH;
							}
							const float32 fx2 = r.x + S(104.f);
							const float32 fw2 = rr.w - S(112.f);
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Source", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.src.%d", k5);
							if (k5 < 8) {
								// Le MOTEUR gagne quand c'est lui qui a bouge --
								// un echange principale/miniature, par exemple --
								// sinon l'ecart se lisait comme un choix de
								// l'utilisateur et l'echange etait annule a
								// l'image suivante (Rihen). Voir la note du combo
								// de source principale. La liste, elle, exclut la
								// source principale.
								const int32 cur2 = insIndexOf(iSrc);
								if (cur2 != sInsSrcSeen[k5]) {
									sInsSrcSel[k5] = cur2;
									sInsSrcSeen[k5] = cur2;
								} else if (sInsSrcSel[k5] != cur2 && sInsSrcSel[k5] >= 0 &&
										   sInsSrcSel[k5] < nIns) {
									iSrc = insNodeOf(sInsSrcSel[k5]);
									sInsSrcSeen[k5] = sInsSrcSel[k5];
								}
								Combo(p, hit, ws, key2, {fx2, yy + S(2.f), fw2, kRowH - S(4.f)},
									  insNames, nullptr, nIns, sInsSrcSel[k5], combo);
							}
							yy += kRowH;
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Forme", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.shp.%d", k5);
							if (k5 < 8) {
								if (iShape != sInsShpSeen[k5]) {
									sInsShpSel[k5] = iShape;
									sInsShpSeen[k5] = iShape;
								} else if (sInsShpSel[k5] != iShape && sInsShpSel[k5] >= 0 &&
										   sInsShpSel[k5] < nShape) {
									iShape = sInsShpSel[k5];
									sInsShpSeen[k5] = sInsShpSel[k5];
								}
								Combo(p, hit, ws, key2, {fx2, yy + S(2.f), fw2, kRowH - S(4.f)},
									  shpNames, nullptr, nShape < 8 ? nShape : 8, sInsShpSel[k5],
									  combo);
							}
							yy += kRowH;
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Position", NkRole::TextMuted);
							{
								const float32 half = (fw2 - S(6.f)) * 0.5f;
								snprintf(key2, sizeof(key2), "out.ins.x.%d", k5);
								DragFloat(p, hit, ws, in, key2,
										  {fx2, yy + S(3.f), half, kRowH - S(6.f)}, iXY[0], 0.002f,
										  NkRole::AxisX, "%.2f");
								snprintf(key2, sizeof(key2), "out.ins.y.%d", k5);
								DragFloat(p, hit, ws, in, key2,
										  {fx2 + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
										  iXY[1], 0.002f, NkRole::AxisY, "%.2f");
							}
							yy += kRowH;
							// DIMENSIONS PROPRES A LA FORME (Rihen) : un carre a un
							// COTE, un cercle un DIAMETRE, les autres une largeur
							// et une hauteur. Afficher deux champs pour un cercle
							// reviendrait a en montrer un qui ne sert a rien.
							{
								const int32 nDim = nk3d::NkInsetDimCount(iShape);
								p.TextV(r.x + kPad + S(8.f), yy, kRowH,
										nk3d::NkInsetDimName(iShape, 0), NkRole::TextMuted);
								if (nDim == 1) {
									snprintf(key2, sizeof(key2), "out.ins.sz.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[0],
											  0.002f, NkRole::AccentUi, "%.2f");
									iSz[1] = iSz[0];
									yy += kRowH;
								} else {
									snprintf(key2, sizeof(key2), "out.ins.sz.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[0],
											  0.002f, NkRole::AxisX, "%.2f");
									yy += kRowH;
									p.TextV(r.x + kPad + S(8.f), yy, kRowH,
											nk3d::NkInsetDimName(iShape, 1), NkRole::TextMuted);
									snprintf(key2, sizeof(key2), "out.ins.szh.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[1],
											  0.002f, NkRole::AxisY, "%.2f");
									yy += kRowH;
								}
							}
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Lisere", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.br.%d", k5);
							DragFloat(p, hit, ws, in, key2,
									  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iBrd, 0.1f,
									  NkRole::AccentUi, "%.0f px");
							yy += kRowH;
							// COULEUR DU LISERE : la nuance cliquable du projet, qui
							// ouvre le vrai selecteur. Une pastille qui se contente
							// d'afficher une couleur sans pouvoir la changer serait
							// une commande factice -- exactement ce que le principe
							// « fonctionnalites a la naissance » interdit.
							{
								snprintf(key2, sizeof(key2), "out.ins.bc.%d", k5);
								bool cch = false;
								const NkRect crow{r.x + kPad + S(8.f), yy,
												  rr.w - 2.f * kPad - S(8.f), kRowH};
								yy += PaintColorRow(p, hit, ws, in, st, crow, yy, "Couleur",
													key2, iCol, &cch);
								if (cch) {
									demo::Demo3DHostSetOutInset(k5, iSrc, iShape, iXY, iSz, iBrd,
																iCol, iOpa);
									NkMarkDirty(st);
								}
							}
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Opacite", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.op.%d", k5);
							DragFloat(p, hit, ws, in, key2,
									  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iOpa, 0.005f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							// FICHIER PROPRE : la miniature est AUSSI ecrite
							// seule, telle quelle -- sans masque de forme ni
							// lisere, qui appartiennent a la composition.
							{
								snprintf(key2, sizeof(key2), "out.ins.own.%d", k5);
								const bool own = demo::Demo3DHostOutInsetOwnFile(k5);
								const NkRect cb{r.x + kPad + S(8.f), yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovo = hit.Add(key2, cb);
								if (own)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovo ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (own)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH, "Aussi en fichier propre");
								p.Unclip();
								if (hit.Clicked(key2)) {
									demo::Demo3DHostSetOutInsetOwnFile(k5, !own);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// SOUS-OPTION : elle n'existe que si le fichier
								// propre est demande (Rihen). Une case qui ne
								// gouverne rien n'a pas a etre montree.
								if (own) {
									snprintf(key2, sizeof(key2), "out.ins.shp2.%d", k5);
									const bool shp = demo::Demo3DHostOutInsetOwnShaped(k5);
									const NkRect cb2{r.x + kPad + S(26.f), yy + S(4.f),
													 kRowH - S(8.f), kRowH - S(8.f)};
									const bool ovs = hit.Add(key2, cb2);
									if (shp)
										p.Fill(cb2, NkRole::AccentUi, 3.f);
									else
										p.Outline(cb2, NkRole::Border,
												  ovs ? NkRole::PanelHeader : NkRole::PanelBg,
												  3.f);
									if (shp)
										p.IconV(cb2.x + S(1.f), yy, kRowH, NkIcon::Check,
												NkRole::TextOnAccent, 11.f);
									p.Clip({cb2.x + cb2.w + S(8.f), yy, rr.w - S(80.f), kRowH});
									p.TextV(cb2.x + cb2.w + S(8.f), yy, kRowH,
											"en gardant la forme", NkRole::TextMuted);
									p.Unclip();
									if (hit.Clicked(key2)) {
										demo::Demo3DHostSetOutInsetOwnShaped(k5, !shp);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
							}
							yy += S(4.f);
							if (iSrc != s0 || iShape != sh0 || iXY[0] != x0 || iXY[1] != y0 ||
								iSz[0] != sz0 || iSz[1] != sz1 || iBrd != b0 || iOpa != o0) {
								demo::Demo3DHostSetOutInset(k5, iSrc, iShape, iXY, iSz, iBrd, iCol,
															iOpa);
								NkMarkDirty(st);
							}
						}
						if (nUsed < maxIns) {
							if (Button("out.ins.add", yy, "Ajouter une incrustation",
									   r.x + kPad, rr.w - 2.f * kPad))
								demo::Demo3DHostOutInsetAdd();
							yy += kRowH;
						} else {
							char fb[64];
							snprintf(fb, sizeof(fb), "Maximum atteint (%d incrustations).",
									 (int)maxIns);
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad, fb,
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gInsTop, yy);
						yy += NkPropGroupGap();
					}

					// ── LE RENDU ────────────────────────────────────────────
					// Hors des groupes : c'est l'ACTE, pas un reglage. Il
					// s'etale sur plusieurs images, donc le bouton dit ce qui
					// se passe au lieu de paraitre sans effet.
					{
						const bool busy = demo::Demo3DHostOutBusy();
						const NkRect br{rowR.x + kPad, yy + S(2.f), rowR.w - 2.f * kPad,
										kRowH + S(4.f)};
						const bool ovr = hit.Add("out.render", br);
						p.Fill(br, busy ? NkRole::PanelHeader : NkRole::AccentUi, 4.f);
						if (ovr && !busy)
							p.OutlineSharp(br, NkRole::Text);
						const char *bt = busy ? "Rendu en cours..." : "Rendre l'image";
						const float32 tw3 = p.TextW(bt);
						p.TextV(br.x + (br.w - tw3) * 0.5f, yy + S(2.f), kRowH + S(4.f), bt,
								busy ? NkRole::TextMuted : NkRole::TextOnAccent);
						if (!busy && hit.Clicked("out.render"))
							demo::Demo3DHostRenderOutput();
						yy += kRowH + S(10.f);
					}
				} else if (sec == 7) {
					// ── LA PASTILLE DU MODE : unique a chaque mode, ses
					// fonctions arrivent PROGRESSIVEMENT par categories (regle
					// de Rihen). Aujourd'hui : l'EDITION porte ses premieres
					// categories ; les autres modes annoncent honnetement ce
					// qui vient -- aucune commande factice.
					// (Indice 7 depuis qu'Output occupe le 6 : la pastille du
					// mode reste TOUJOURS la derniere de la colonne.)
					const int32 m5 = (int32)st.mode;
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					if (m5 == 1) {
						// ── EDITION ─────────────────────────────────────────
						const bool gSel = PaintPropGroup(p, hit, st, rowR, yy,
														 "prop.g.edsel", "Selection",
														 0x1000u);
						const float32 gSelTop = yy;
						if (gSel) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							const int32 m2 = demo::Demo3DHostEditSelMask();
							snprintf(buf, sizeof(buf), "Sous-mode : %s%s%s",
									 (m2 & 1) ? "Sommets " : "", (m2 & 2) ? "Aretes " : "",
									 (m2 & 4) ? "Faces" : "");
							p.TextV(iR.x, yy, kRowH, buf, NkRole::TextMuted);
							yy += kRowH;
							p.TextV(iR.x, yy, kRowH, "1 / 2 / 3 pour changer",
									NkRole::TextMuted);
							yy += kRowH + NkGroupPad();
							PaintGroupBlock(p, rowR, gSelTop, yy);
						}
						yy += NkPropGroupGap();
						const bool gTools = PaintPropGroup(p, hit, st, rowR, yy,
														   "prop.g.edtools", "Outils",
														   0x2000u);
						const float32 gToolsTop = yy;
						if (gTools) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							static const char *const kEdT[5] = {
								"E  --  extruder", "I  --  inserer une face",
								"Ctrl+B  --  biseauter", "Ctrl+R  --  boucle de coupe",
								"K  --  couteau   W  --  subdiviser"};
							for (int32 t6 = 0; t6 < 5; ++t6) {
								p.TextV(iR.x, yy, kRowH, kEdT[t6], NkRole::TextMuted);
								yy += kRowH;
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, gToolsTop, yy);
						}
						yy += NkPropGroupGap();
						const bool gGeo = PaintPropGroup(p, hit, st, rowR, yy,
														 "prop.g.edgeo", "Geometrie",
														 0x4000u);
						const float32 gGeoTop = yy;
						if (gGeo) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							p.TextV(iR.x, yy, kRowH,
									"Fusion, separation, symetrie -- a venir.",
									NkRole::TextMuted);
							yy += kRowH + NkGroupPad();
							PaintGroupBlock(p, rowR, gGeoTop, yy);
						}
					} else {
						static const char *const kSoon[5] = {
							"Sculpture 2.5D : brosses de relief -- a venir.",
							"Sculpture : brosses volumiques -- a venir.",
							"Texturing : peinture et calques -- a venir.",
							"Patron : depliage UV (unwrapping) -- a venir.",
							"Texture painting : peinture sur texture -- a venir."};
						if (m5 >= 2 && m5 <= 6) {
							// Blocs qui vont a la ligne : ces phrases depassaient
							// la largeur du panneau des qu'on le retrecissait.
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rowR.w - 2.f * kPad, kSoon[m5 - 2],
											 NkRole::TextMuted);
							yy += S(4.f);
							yy += p.TextWrap(r.x + kPad, yy, rowR.w - 2.f * kPad,
											 "Ses categories apparaitront ici, dans cette "
											 "pastille.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
					}
				}

				// La hauteur du contenu sert desormais a la SEULE barre generale :
				// la molette s'y applique donc directement, sans defilement local.
				sContentH[sec] = yy - secY + S(4.f);
				hit.PopClip();
				p.Unclip();
				// PLUS DE BARRE PAR SECTION (Rihen) : une seule pastille est active,
				// donc une seule section occupe le panneau -- c'est la barre
				// GENERALE qui doit la faire defiler. Une seconde barre a
				// l'interieur decoupait le defilement en deux gestes pour un seul
				// contenu.
				// La pile avance de ce que le contenu occupe REELLEMENT : c'est lui
				// que la barre generale doit pouvoir parcourir, pas la hauteur du
				// cadre (qui vaut maintenant tout le panneau).
				secY += sContentH[sec];
				// ── POIGNEE DE HAUTEUR : agrandir/retrecir CETTE section ────
				// Le geste appartient a la poignee ou il a commence (propDragKey),
				// comme la barre de defilement.
				// La poignee n'existe que si une AUTRE section OUVERTE suit : au
				// bas de la DERNIERE elle flottait en lisere fantome sur le vide
				// (capture de Rihen) -- la derniere remplit toujours l'espace.
				bool hasNextOpen = false;
				for (int32 j2 = sec + 1; j2 < kNSec; ++j2)
					if (st.propOpen[j2] && !st.propFold[j2])
						hasNextOpen = true;
				if (!hasNextOpen)
					st.propSecH[sec] = 0.f;
				if (hasNextOpen) {
					snprintf(key, sizeof(key), "props.div.%d", sec);
					// Entierement DANS le bas de la boite : elle mordait sur
					// l'en-tete suivant, et viser l'un declenchait l'autre.
					const NkRect dv{r.x, secY - S(6.f), r.w - S(14.f), S(6.f)};
					const bool overD = hit.Add(key, dv);
					const bool mineD = (strcmp(st.propDragKey, key) == 0);
					if (overD || mineD) {
						hit.WantCursor(NkCursorWant::ResizeNS);
						p.Fill({r.x, secY - S(2.f), r.w, S(3.f)}, NkRole::AccentUi);
					}
					if (hit.MouseDown() && (overD || mineD)) {
						if (!st.propDragKey[0] && overD)
							snprintf(st.propDragKey, sizeof(st.propDragKey), "%s", key);
						if (strcmp(st.propDragKey, key) == 0) {
							float32 nh = hit.Mouse().y - box.y;
							if (nh < kRowH * 2.f)
								nh = kRowH * 2.f;
							st.propSecH[sec] = nh;
						}
					}
				}
			}
			p.Unclip();
			{
				const float32 stackH = (secY + st.propScroll) - stackTop;
				const float32 viewH = (r.y + r.h) - stackTop;
				const float32 maxOff = stackH > viewH ? stackH - viewH : 0.f;
				if (st.propScroll > maxOff)
					st.propScroll = maxOff;
				// Molette de PAGE : quand aucune section ne l'a consommee (souris
				// sur un en-tete, une poignee, ou du vide), c'est la pile entiere
				// qui defile.
				if (!anyWheel)
					hit.WheelIn({r.x, stackTop, r.w, viewH}, st.propScroll, stackH, viewH);
				// LA SCROLLBAR STANDARD de NKEditorKit -- la meme que l'editeur de
				// code (Rihen). Elle occupe sa gouttiere entre le contenu et la
				// colonne de pastilles, et reste VISIBLE meme quand tout tient a
				// l'ecran : une barre qui va et vient fait sauter la mise en page.
				// AUCUNE pastille active = pas de contenu, donc PAS DE BARRE : le
				// panneau se reduit a sa colonne de pastilles (Rihen). Une
				// gouttiere seule, sans rien a faire defiler, n'annonce rien.
				if (guiCtx && !collapsed) {
					const NkRect sbTrack{r.x + r.w, stackTop, kSbW, viewH};
					editorkit::NkVScrollbar(*guiCtx, guiCtx->dl, sbTrack, st.propScroll,
											stackH > viewH ? stackH : viewH + 1.f, viewH,
											0x4E4B5000u, kRowH);
				} else {
					NkScrollDrag(p, hit, st, "props.outer", {r.x, stackTop, r.w, viewH},
								 stackH, st.propScroll);
				}
			}
			// ── LES PASTILLES : une par section, a droite. BLEUE = active.
			// UNE SEULE A LA FOIS (regle de Rihen) : le panneau montre les
			// proprietes de LA categorie choisie, et rien d'autre. Les ouvrir
			// ensemble revenait a empiler des blocs sans rapport et a rogner la
			// place de chacun -- illisible des que les categories se comptent en
			// dizaines. Recliquer la pastille active replie le panneau.
			{
				// La colonne commence APRES la gouttiere de la scrollbar : posee au
				// meme x, elle recouvrait la barre (constate par Rihen). Le trait
				// separateur se place de meme, entre la barre et les pastilles.
				const float32 tabX = r.x + r.w + kSbW;
				p.VLine(tabX, stackTop, (rFull.y + rFull.h) - stackTop);
				float32 ty = stackTop + S(4.f);
				for (int32 i2 = 0; i2 < kNSec; ++i2) {
					// Modele, Modificateur et Materiau n'apparaissent que pour une
					// selection (regle de Rihen) : sans objet, ils n'auraient rien
					// d'honnete a montrer.
					if ((i2 == 0 || i2 == 3 || i2 == 4) && !hasSel5)
						continue;
					char tk[24];
					snprintf(tk, sizeof(tk), "props.tab.%d", i2);
					const NkRect tb{tabX + S(3.f), ty, S(20.f), S(24.f)};
					const bool on = st.propOpen[i2];
					const bool overT = hit.Add(tk, tb);
					if (on)
						p.Fill(tb, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, tb, overT, 3.f);
					p.IconV(tb.x + (tb.w - S(14.f)) * 0.5f, tb.y, tb.h, kSecs[i2].icon,
							on ? NkRole::TextOnAccent : NkRole::TextMuted, 14.f);
					if (hit.Clicked(tk)) {
						// EXCLUSIVE : choisir une categorie eteint les autres, et
						// recliquer l'active replie le panneau. Une section fermee
						// oublie son agrandissement et son defilement -- ils ne
						// doivent plus peser sur la mise en page.
						for (int32 j2 = 0; j2 < kNSec; ++j2) {
							if (j2 == i2)
								continue;
							if (st.propOpen[j2]) {
								st.propOpen[j2] = false;
								st.propSecH[j2] = 0.f;
								st.propScroll3[j2] = 0.f;
							}
						}
						st.propOpen[i2] = !on;
						if (on) {
							st.propSecH[i2] = 0.f;
							st.propScroll3[i2] = 0.f;
						}
					}
					ty += S(28.f);
				}
			}
			// LE MENU DE GROUPE, EN DERNIER : peint par-dessus tout le panneau,
			// il repond donc a ses propres clics (les zones declarees en
			// dernier gagnent le survol).
			PaintPropGroupMenu(p, hit, st, in);
			if (!hit.MouseDown())
				st.propDragKey[0] = 0; // fin de glissement : la barre lache le geste
		}

		// â”€â”€ PROPRIETES (droite, haut) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintProperties(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
									NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			float32 y = PaintPanelTab(p, full, "Proprietes", &hit, &st.showRight, "prop.close");
			const NkRect r = Inset(full);
			y = PaintSearch(p, r, y, hit, ws, in, "props.search", st.searchProps);

			// LES CINQ PASTILLES Â« General / Objet / Rendu / Physique / Tout Â» ONT
			// ETE RETIREES. Rihen a demande a quoi elles servaient : a rien. Elles
			// venaient de la maquette et annonÃ§aient quatre familles de reglages
			// dont trois n'existent pas -- il n'y a ni materiau d'objet, ni physique.
			// Une barre de filtres qui ne filtre rien apprend a ne plus lire les
			// filtres, y compris ceux qui marcheront un jour.
			//
			// Elles reviendront quand il y aura vraiment plusieurs familles a
			// separer, et elles porteront alors le meme mecanisme que les sections
			// de Details : un bit par famille, teste a la peinture.
			p.HLine(full.x, y, full.w);
			y += 1.f;

			const float32 listTop = y;
			// Meme decoupe que Details : sinon la section Transformation remonte sur
			// l'en-tete Â« Proprietes Â» et deborde sur Details en dessous.
			const NkRect clipR{full.x, listTop, full.w, full.y + full.h - listTop};
			p.Clip(clipR);
			y -= st.scrollProps;

			if (SectionHeader(p, hit, r, y, "prop.sec.transform", "Transformation", st.showTransform)) {
				y += kRowH;
				// LES CHAMPS SONT CEUX DE L'OBJET ACTIF, dans les deux sens : on lit
				// sa transformation avant de peindre (le gizmo a pu la changer), on la
				// reecrit apres (les champs ont pu etre edites). Sans objet, la
				// section le dit au lieu d'afficher des zeros editables qui ne
				// commandent rien.
				const int32 act = nk3d::Viewport3DActiveObject();
				if (act >= 0 && nk3d::Viewport3DObjectAlive(act)) {
					nk3d::Viewport3DGetObjectTransform(act, st.pos, st.rot, st.scl);
					PaintTransformRow(p, hit, ws, in, r, y, "Position", st.pos, 0.01f, "prop.pos",
									  NkIcon::Refresh, NkIcon::Lock);
					y += Vec3RowH();
					PaintTransformRow(p, hit, ws, in, r, y, "Rotation", st.rot, 0.5f, "prop.rot",
									  NkIcon::Refresh, NkIcon::None, "%.1f");
					y += Vec3RowH();
					PaintTransformRow(p, hit, ws, in, r, y, "Echelle", st.scl, 0.01f, "prop.scl",
									  NkIcon::Refresh, NkIcon::Lock, "%.3f");
					y += Vec3RowH();
					nk3d::Viewport3DSetObjectTransform(act, st.pos, st.rot, st.scl);
					// â”€â”€ EDITION PROPORTIONNELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					// Les sommets voisins suivent en s'attenuant, dans un rayon
					// donne. C'est l'outil qui distingue une deformation organique
					// d'un deplacement de sommets : sans lui, bouger un point d'un
					// visage laisse un pic.
					{
						p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
						p.TextV(r.x + kPad, y, kRowH, "Proportionnel");
						const NkRect cb{r.x + kLabelW + 8.f, y + 5.f, 14.f, 14.f};
						const bool over = hit.Add("prop.prop", {r.x + kLabelW, y, 30.f, kRowH});
						if (st.proportional) {
							p.Fill(cb, NkRole::AccentUi, 2.f);
							p.IconV(cb.x + 1.f, cb.y - 1.f, 16.f, NkIcon::Check,
									NkRole::TextOnAccent, 12.f);
						} else {
							p.Outline(cb, over ? NkRole::AccentUi : NkRole::Border,
									  NkRole::InputBg, 2.f);
						}
						if (hit.Clicked("prop.prop"))
							st.proportional = !st.proportional;
						// Le RAYON n'apparait que si l'option est active : un reglage
						// visible mais sans effet fait douter de tout le panneau.
						if (st.proportional) {
							const NkRect rr{r.x + kLabelW + 34.f, y + 2.f, r.w - kLabelW - 42.f,
											kRowH - 4.f};
							DragFloat(p, hit, ws, in, "prop.proprad", rr, st.proportionalRadius,
									  0.01f, NkRole::AccentUi, "%.2f");
							if (st.proportionalRadius < 0.01f)
								st.proportionalRadius = 0.01f;
						}
						p.HLine(r.x, y + kRowH - 1.f, r.w);
						y += kRowH;
					}
				} else {
					p.TextV(r.x + kPad, y, kRowH, "Aucun objet selectionne", NkRole::TextMuted);
					y += kRowH;
				}
			} else {
				y += kRowH;
			}

			p.Unclip();
			const NkRect area = clipR;
			hit.Add("props.list", area);
			hit.Wheel("props.list", st.scrollProps, y - listTop + st.scrollProps, area.h);
			p.VScroll(area, y - listTop + st.scrollProps, st.scrollProps);
		}

		// â”€â”€ DETAILS (droite, bas) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// LE DEROULANT DE MODIFICATEURS EST CELUI DE LA MAQUETTE, et il est ici
		// VOLONTAIREMENT tel quel. J'avais dessine une PILE (activation,
		// reordonnancement, application, retrait) parce que c'est ce qu'il faut
		// fonctionnellement : l'ordre est signifiant, un miroir apres une subdivision
		// ne donne pas le meme maillage qu'avant. Mais la consigne est de coller a la
		// maquette d'abord et de mettre a jour en developpant -- la pile reviendra
		// quand les modificateurs seront reellement branches sur NkModifierParams,
		// avec l'accord de Rihen sur son dessin.
		// Sections repliables du panneau Details. L'index sert de bit dans
		// `st.detailOpen` : un booleen par section eparpillerait l'etat et
		// obligerait a passer un tableau de pointeurs a chaque appel.
		enum NkDetailSection : uint32 {
			NkDetailMesh = 0,
			NkDetailModifiers = 1,
			NkDetailMaterial = 2,
			NkDetailSubMesh = 3,
		};

		// En-tete de section repliable. Retourne l'etat OUVERT/FERME apres le clic,
		// pour que l'appelant saute le corps sans le recalculer.
		inline bool DetailHeader(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r, float32 &y,
								 NkModelerState &st, uint32 bit, const char *label) {
			const NkRect hr{r.x, y, r.w, kRowH};
			char key[40];
			snprintf(key, sizeof(key), "det.sec.%u", bit);
			const bool over = hit.Add(key, hr);
			HoverFill(p, hr, over, 0.f);
			const bool open = (st.detailOpen & (1u << bit)) != 0u;
			p.IconV(r.x + 6.f, y, kRowH, open ? NkIcon::ChevronDown : NkIcon::ChevronRight,
					NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, label);
			y += kRowH;
			// LE CHEVRON REPLIE VRAIMENT. Il etait dessine mais mort : un chevron qui
			// ne fait rien apprend a l'utilisateur a ne plus essayer de cliquer, et
			// c'est une lecon qu'il applique ensuite aux chevrons qui, eux, marchent.
			// Toute la ligne est cliquable, pas seulement la fleche -- viser 11 px de
			// haut n'a aucun interet quand la ligne entiere est sans ambiguite.
			if (hit.Clicked(key))
				st.detailOpen ^= (1u << bit);
			return open;
		}

		inline void PaintDetails(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			const float32 scroll = st.scrollDetails;
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			p.HLine(full.x, full.y, full.w);
			// Proprietes et Details partagent UNE colonne : les fermer ferme donc la
			// colonne entiere, et la poignee de droite les ramene toutes les deux. Les
			// separer demanderait deux poignees pour un gain nul -- on ne travaille pas
			// avec les proprietes sans les details.
			float32 y = PaintPanelTab(p, full, "Details (Cube)", &hit, &st.showRight, "det.close");
			const NkRect r = Inset(full);
			const float32 listTop = y;
			// LE CONTENU EST DECOUPE au rectangle qui reste sous l'en-tete. Sans cela
			// Â« Maillage Â» remonte par-dessus l'onglet Â« Details Â» des le premier cran
			// de molette, et les sections du bas debordent sur le navigateur. C'etait
			// visible et c'est corrige ici plutot qu'en bornant le defilement : borner
			// ne changerait rien, le debordement vient du DESSIN.
			const NkRect clipR{full.x, listTop, full.w, full.y + full.h - listTop};
			p.Clip(clipR);
			y -= scroll;

			// â”€â”€ MAILLAGE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			if (DetailHeader(p, hit, r, y, st, NkDetailMesh, "Maillage")) {
				// LES ARETES MANQUAIENT. Sur un maillage a demi-aretes elles ne sont
				// pas une curiosite : c'est la seule des trois quantites qui trahit un
				// maillage non-manifold (une arete portee par trois faces) et c'est
				// aussi le sous-mode d'edition du milieu. En afficher deux sur trois
				// laissait croire que le compte d'aretes n'existait pas.
				static const char *const kL[] = {"Sommets", "Aretes", "Faces", "Triangles"};
				// PLUS DE VALEURS EN DUR. Elles viennent du NkEditMesh lui-meme : un
				// panneau qui affiche Â« 8 sommets Â» quoi qu'il arrive est pire qu'un
				// panneau vide, parce qu'on le croit.
				uint32 nv = 0, ne = 0, nf = 0, nt = 0;
				nk3d::Viewport3DStats(nv, ne, nf, nt);
				char vbuf[4][24];
				snprintf(vbuf[0], sizeof(vbuf[0]), "%u", nv);
				snprintf(vbuf[1], sizeof(vbuf[1]), "%u", ne);
				snprintf(vbuf[2], sizeof(vbuf[2]), "%u", nf);
				snprintf(vbuf[3], sizeof(vbuf[3]), "%u", nt);
				const char *kV[4] = {vbuf[0], vbuf[1], vbuf[2], vbuf[3]};
				for (int32 i = 0; i < 4; ++i) {
					p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
					p.TextV(r.x + kPad, y, kRowH, kL[i]);
					p.TextV(r.x + kLabelW + kPad, y, kRowH, kV[i], NkRole::TextMuted);
					p.HLine(r.x, y + kRowH - 1.f, r.w);
					y += kRowH;
				}
			}

			// â”€â”€ MODIFICATEURS : LA PILE REELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce qui etait peint ici -- Â« Selectionner un modificateur Â» -- etait une
			// facade. La pile vit sur l'objet actif, elle est NON DESTRUCTIVE (la
			// cage editee reste la base) et l'ORDRE Y EST SIGNIFIANT : miroir puis
			// reseau ne donne pas la meme chose que reseau puis miroir. D'ou les
			// fleches de reordonnancement, qui ne sont pas un confort.
			//
			// L'affichage est GENERIQUE : chaque modificateur publie la liste de ses
			// parametres (libelle, type, bornes) et on la parcourt. Recopier a la
			// main dix-sept jeux de reglages aurait diverge au premier ajout moteur.
			if (DetailHeader(p, hit, r, y, st, NkDetailModifiers, "Modificateurs")) {
				const uint32 nMods = nk3d::Viewport3DModifierCount();
				if (nMods == 0u) {
					p.TextV(r.x + kPad, y, kRowH, "Aucun -- barre d'outils > Modificateur",
							NkRole::TextMuted);
					y += kRowH;
				}
				char mkey[48];
				for (uint32 m = 0; m < nMods; ++m) {
					const int32 type = nk3d::Viewport3DModifierTypeAt(m);
					const bool on = nk3d::Viewport3DModifierEnabled(m);
					const NkRect hr{r.x, y, r.w, kRowH};
					p.Fill(hr, NkRole::PanelHeader);

					// L'OEIL desactive sans supprimer : comparer avec et sans est le
					// geste le plus courant d'une pile.
					snprintf(mkey, sizeof(mkey), "det.mod.eye.%u", m);
					const NkRect er{r.x + 4.f, y + 3.f, 20.f, kRowH - 6.f};
					HoverFill(p, er, hit.Add(mkey, er), 2.f);
					p.IconV(r.x + 6.f, y, kRowH, on ? NkIcon::Eye : NkIcon::EyeClosed,
							on ? NkRole::Text : NkRole::TextMuted, 12.f);
					if (hit.Clicked(mkey))
						nk3d::Viewport3DSetModifierEnabled(m, !on);

					p.TextV(r.x + 26.f, y, kRowH, nk3d::Viewport3DModifierTypeName(type),
							on ? NkRole::Text : NkRole::TextMuted);

					// Monter / descendre / appliquer / retirer, cales a droite.
					float32 bx = r.x + r.w - 22.f;
					struct MB {
							NkIcon ic;
							int32 act; ///< 0 retirer, 1 appliquer, 2 descendre, 3 monter
					};
					static const MB kMB[4] = {{NkIcon::Trash, 0},
											  {NkIcon::Check, 1},
											  {NkIcon::ChevronDown, 2},
											  {NkIcon::ChevronUp, 3}};
					for (int32 b = 0; b < 4; ++b) {
						snprintf(mkey, sizeof(mkey), "det.mod.b%d.%u", b, m);
						const NkRect br{bx - 2.f, y + 3.f, 20.f, kRowH - 6.f};
						HoverFill(p, br, hit.Add(mkey, br), 2.f);
						p.IconV(bx, y, kRowH, kMB[b].ic, NkRole::TextMuted, 11.f);
						if (hit.Clicked(mkey)) {
							switch (kMB[b].act) {
								case 0:
									nk3d::Viewport3DRemoveModifier(m);
									break;
								case 1:
									// DESTRUCTIF : cuit le modificateur dans le maillage
									// et le retire de la pile. C'est tout son objet, et
									// Ctrl+Z le defait (un instantane est pris).
									nk3d::Viewport3DApplyModifier(m);
									break;
								case 2:
									nk3d::Viewport3DMoveModifier(m, false);
									break;
								default:
									nk3d::Viewport3DMoveModifier(m, true);
									break;
							}
							NkMarkDirty(st);
						}
						bx -= 22.f;
					}
					y += kRowH;

					// â”€â”€ Parametres, decrits par le modificateur lui-meme â”€â”€â”€â”€â”€
					const uint32 nP = nk3d::Viewport3DModifierParamCount(m);
					for (uint32 pi = 0; pi < nP; ++pi) {
						const char *plabel = "";
						int32 ptype = 2;
						float32 pmin = 0.f, pmax = 0.f;
						if (!nk3d::Viewport3DModifierParamInfo(m, pi, &plabel, &ptype, &pmin, &pmax))
							continue;
						p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
						p.TextV(r.x + kPad + 8.f, y, kRowH, plabel);
						float32 v = nk3d::Viewport3DGetModifierParam(m, pi);
						snprintf(mkey, sizeof(mkey), "det.mod.p%u.%u", m, pi);
						const NkRect fr{r.x + kLabelW + 4.f, y + 2.f, r.w - kLabelW - 12.f,
										kRowH - 4.f};
						if (ptype == 0) {
							// BOOLEEN : une case, pas un champ numerique.
							const NkRect cb{fr.x + 2.f, fr.y + 2.f, 14.f, 14.f};
							const bool over2 = hit.Add(mkey, {fr.x, fr.y, 22.f, fr.h});
							if (v != 0.f) {
								p.Fill(cb, NkRole::AccentUi, 2.f);
								p.IconV(cb.x + 1.f, cb.y - 1.f, 16.f, NkIcon::Check,
										NkRole::TextOnAccent, 12.f);
							} else {
								p.Outline(cb, over2 ? NkRole::AccentUi : NkRole::Border,
										  NkRole::InputBg, 2.f);
							}
							if (hit.Clicked(mkey))
								nk3d::Viewport3DSetModifierParam(m, pi, v != 0.f ? 0.f : 1.f);
						} else {
								// Les ENTIERS se tirent par pas de 1, les flottants par
							// centiemes : un pas unique rendrait les uns inatteignables
							// et les autres inutilisables.
							const float32 step = (ptype == 1) ? 1.f : 0.01f;
							const char *fmt = (ptype == 1) ? "%.0f" : "%.3f";
							float32 nv = v;
							DragFloat(p, hit, ws, in, mkey, fr, nv, step, NkRole::AccentUi, fmt);
							if (nv != v) {
								if (pmin != pmax) { // bornes publiees : on les respecte
									if (nv < pmin)
										nv = pmin;
									if (nv > pmax)
										nv = pmax;
								}
								nk3d::Viewport3DSetModifierParam(m, pi, nv);
							}
						}
						p.HLine(r.x, y + kRowH - 1.f, r.w);
						y += kRowH;
					}
					y += 4.f;
				}
			}

			// â”€â”€ MATERIAUX : PLUSIEURS PAR MODELE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Rihen le rappelle et c'est structurant : un modele n'a pas UN materiau,
			// il a une LISTE D'EMPLACEMENTS. Chaque face du maillage porte l'indice de
			// l'emplacement qui la peint ; l'ensemble des faces qui partagent un indice
			// forme un SOUS-MAILLAGE. C'est exactement le modele de Blender, et c'est
			// aussi ce qu'attend le rendu : un tampon de dessin par emplacement.
			//
			// Consequence sur le format du maillage : `NkEditMesh` doit porter un
			// `materialSlot` PAR FACE, pas une couleur par objet. Consequence sur
			// l'edition : selectionner des faces (ou des sommets, dont on deduit les
			// faces) puis Â« Assigner Â» ecrit cet indice -- ce qui CREE le sous-maillage
			// sans qu'aucune geometrie soit dupliquee ni separee.
			//
			// Ce panneau n'est pour l'instant qu'une facade : les emplacements sont en
			// dur et Â« Assigner Â» n'ecrit rien. Le cablage vient avec la vue 3D, quand
			// il y aura une vraie selection a assigner.
			if (DetailHeader(p, hit, r, y, st, NkDetailMaterial, "Materiaux")) {
				// LES DEUX MATERIAUX ET LES DEUX SOUS-MAILLAGES QUI S'AFFICHAIENT ICI
				// ETAIENT SIMULES. Rihen a demande d'ou ils sortaient : de nulle part,
				// c'etait une maquette. Un panneau qui invente son contenu est pire
				// qu'un panneau vide -- on lui fait confiance.
				//
				// Le modele reste celui annonce : un modele porte une LISTE
				// d'emplacements, chaque face du maillage porte l'indice de celui qui
				// la peint, et l'ensemble des faces d'un meme indice forme un
				// sous-maillage. Les emplacements viendront des materiaux crees dans
				// le navigateur de projet ; tant qu'aucun n'existe, il n'y a rien a
				// montrer et on le dit.
				p.TextV(r.x + kPad, y, kRowH, "Aucun emplacement", NkRole::TextMuted);
				y += kRowH;
				const float32 bw = (r.w - 24.f) / 3.f;
				struct Btn {
						const char *label;
						NkIcon ic;
				};
				static const Btn kB[] = {
					{"Ajouter", NkIcon::Add}, {"Retirer", NkIcon::Trash}, {"Assigner", NkIcon::Check}};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{r.x + 8.f + (float32)i * bw, y + 3.f, bw - 4.f, kRowH - 6.f};
					// TOUT est grise tant qu'aucun materiau n'existe : Â« Assigner Â»
					// demande en plus une selection de faces, donc le mode edition.
					const bool off = true;
					const NkRole fg = off ? NkRole::TextMuted : NkRole::Text;
					p.Outline(br, NkRole::Border, NkRole::InputBg, 2.f);
					p.IconV(br.x + 5.f, br.y, br.h, kB[i].ic, fg, 12.f);
					p.TextV(br.x + 21.f, br.y, br.h, kB[i].label, fg);
				}
				y += kRowH + 4.f;
			}

			// â”€â”€ SOUS-MAILLAGES â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce que les emplacements DECOUPENT. Un sous-maillage n'est pas un objet
			// separe : c'est un groupe de faces du meme maillage. Il n'y en a donc
			// aucun tant qu'aucun emplacement n'a ete assigne.
			if (DetailHeader(p, hit, r, y, st, NkDetailSubMesh, "Sous-maillages")) {
				p.TextV(r.x + kPad, y, kRowH, "Aucun", NkRole::TextMuted);
				y += kRowH;
			}

			p.Unclip();
			const NkRect area = clipR;
			hit.Add("det.list", area);
			hit.Wheel("det.list", st.scrollDetails, y - listTop + scroll, area.h);
			p.VScroll(area, y - listTop + scroll, scroll);
		}

		// â”€â”€ NAVIGATEUR DE PROJET (bas) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// `sortCombo` porte le deroulant de CLASSEMENT. Il est fourni par la boucle
		// principale, comme pour les autres combos : un popup se peint APRES tout le
		// reste, il ne peut donc pas se declarer ici.
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in,
								 nkgui::NkGuiContext *guiCtx = nullptr,
								 NkComboPending *sortCombo = nullptr) {
			const float32 treeScroll = st.scrollTree;
			const float32 assetScroll = st.scrollAssets;
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 28.f;
			const float32 ih = p.IconSize();
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + kPad + p.TextW("Navigateur de projet") + 10.f;
			{
				// Meme croix que les panneaux lateraux, meme effet : elle referme, et la
				// poignee du bas ramene le navigateur.
				const NkRect cr{x - 3.f, r.y + 4.f, 20.f, topH - 8.f};
				const bool over = hit.Add("brw.close", cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				p.IconV(x, r.y, topH, NkIcon::WinClose, over ? NkRole::TextOnAccent : NkRole::Text,
						11.f);
				if (hit.Clicked("brw.close"))
					st.showBrowser = false;
			}
			x += 22.f;
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			struct B {
					NkIcon ic;
					const char *label;
			};
			// UN COMBO « Creer » (le meme sous-menu que le clic droit) remplace
			// la rangee de boutons, plus « Importer » (Rihen).
			{
				const float32 bw = 18.f + p.TextW("Creer") + 24.f;
				const NkRect br{x - 4.f, r.y + 3.f, bw, topH - 6.f};
				HoverFill(p, br, hit.Add("brw.creer", br), 2.f);
				p.IconV(x, r.y, topH, NkIcon::Add, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, topH, "Creer");
				p.IconV(x + 18.f + p.TextW("Creer") + 6.f, r.y, topH, NkIcon::ChevronDown,
						NkRole::TextMuted, 11.f);
				if (hit.Clicked("brw.creer")) {
					st.browMenuIdx = -4; // liste Creer SEULE (pas d'Importer)
					st.browMenuCreat = true;
					st.browMenuX = br.x;
					st.browMenuY = br.y + br.h + 2.f;
				}
				x += bw + 8.f;
				const float32 bw2 = 18.f + p.TextW("Importer") + 10.f;
				const NkRect br2{x - 4.f, r.y + 3.f, bw2, topH - 6.f};
				HoverFill(p, br2, hit.Add("brw.imp", br2), 2.f);
				p.IconV(x, r.y, topH, NkIcon::Import, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, topH, "Importer");
				x += bw2 + 8.f;
			}
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			// HISTORIQUE : chaque changement de dossier s'enregistre (sauf via
			// les fleches elles-memes).
			if (st.browHistLen == 0) {
				st.browHist[0] = -1;
				st.browHistLen = 1;
				st.browHistPos = 0;
			}
			if (st.browserFolder != st.browPrevFolder) {
				if (!st.browHistNav && st.browHistPos < 63) {
					st.browHistLen = st.browHistPos + 1;
					st.browHist[st.browHistLen++] = st.browserFolder;
					st.browHistPos = st.browHistLen - 1;
				}
				st.browHistNav = false;
				st.browPrevFolder = st.browserFolder;
			}
			const bool canB = st.browHistPos > 0;
			const bool canF = st.browHistPos + 1 < st.browHistLen;
			hit.Add("brw.back", {x - 4.f, r.y + 3.f, 22.f, topH - 6.f});
			p.IconV(x, r.y, topH, NkIcon::ArrowLeft,
					canB ? NkRole::Text : NkRole::TextMuted, 13.f);
			if (canB && hit.Clicked("brw.back")) {
				st.browHistPos--;
				st.browHistNav = true;
				int32 tg9 = st.browHist[st.browHistPos];
				if (tg9 >= 0 && st.browserKind[tg9] == 255)
					tg9 = -1; // dossier disparu entre-temps
				st.browserFolder = tg9;
			}
			hit.Add("brw.fwd", {x + 18.f, r.y + 3.f, 22.f, topH - 6.f});
			p.IconV(x + 22.f, r.y, topH, NkIcon::ArrowRight,
					canF ? NkRole::Text : NkRole::TextMuted, 13.f);
			if (canF && hit.Clicked("brw.fwd")) {
				st.browHistPos++;
				st.browHistNav = true;
				int32 tg9 = st.browHist[st.browHistPos];
				if (tg9 >= 0 && st.browserKind[tg9] == 255)
					tg9 = -1; // dossier disparu entre-temps
				st.browserFolder = tg9;
			}
			// FIL D'ARIANE dynamique et cliquable (chemin REEL du dossier).
			{
				int32 chain[16];
				int32 nCh = 0;
				for (int32 c9 = st.browserFolder; c9 >= 0 && nCh < 16;
					 c9 = st.browserParent[c9])
					chain[nCh++] = c9;
				float32 bx = x + 50.f;
				hit.Add("brw.crumb.root",
						{bx, r.y + 3.f, p.TextW("Contenu") + 6.f, topH - 6.f});
				p.TextV(bx, r.y, topH, "Contenu",
						st.browserFolder < 0 ? NkRole::Text : NkRole::TextMuted);
				if (hit.Clicked("brw.crumb.root"))
					st.browserFolder = -1;
				bx += p.TextW("Contenu") + 8.f;
				for (int32 c9 = nCh - 1; c9 >= 0; --c9) {
					p.TextV(bx, r.y, topH, ">", NkRole::TextMuted);
					bx += p.TextW(">") + 6.f;
					const int32 fi = chain[c9];
					char ck[24];
					snprintf(ck, sizeof(ck), "brw.crumb.%d", fi);
					const float32 wN = p.TextW(st.browserNames[fi]);
					hit.Add(ck, {bx, r.y + 3.f, wN + 6.f, topH - 6.f});
					p.TextV(bx, r.y, topH, st.browserNames[fi],
							fi == st.browserFolder ? NkRole::Text : NkRole::TextMuted);
					if (hit.Clicked(ck))
						st.browserFolder = fi;
					bx += wN + 10.f;
				}
			}
			p.HLine(r.x, r.y + topH - 1.f, r.w);

			// Arbre de dossiers : LES DOSSIERS CREES PAR L'UTILISATEUR, plus une
			// racine fixe. Ils structurent le PROJET et non le disque : ils seront
			// enregistres DANS le fichier de projet.
			st.browserRect = r; // routage des raccourcis (voir PaintSceneMenus)
			// Un menu ou une carte de depot OUVERT bloque tout le navigateur en
			// dessous : les clics ne TRAVERSENT plus (constate par Rihen).
			// ... y compris un menu ouvert AILLEURS (hierarchie, Ajouter) qui
			// deborde sur le navigateur : lui aussi est peint apres ce panneau.
			const bool uiModal = (st.browMenuIdx != -1) || (st.browAskIdx >= 0) ||
								 st.UiBlocks(hit.Mouse().x, hit.Mouse().y);
			const float32 treeW = r.w * 0.18f;
			const float32 ty = r.y + topH;
			const float32 th = r.h - topH;
			p.Clip({r.x, ty, r.w, th});
			p.Fill({r.x, ty, treeW, th}, NkRole::WindowBg);
			hit.Add("brow.tree", {r.x, ty, treeW, th});
			if (hit.RightClicked("brow.tree")) {
				st.browMenuIdx = -2; // fond : Nouveau dossier / Coller
				st.browMenuX = hit.Mouse().x;
				st.browMenuY = hit.Mouse().y;
			}
			p.VLine(r.x + treeW, ty, th);
			// Le FOND de la grille est une zone : cliquer dans le vide
			// DESELECTIONNE (les cartes, declarees apres, gardent leurs clics).
			hit.Add("brow.grid", {r.x + treeW, ty, r.w - treeW, th});
			if (!uiModal && hit.Clicked("brow.grid"))
				st.selectedAsset = -1;
			if (hit.RightClicked("brow.grid")) {
				st.browMenuIdx = -2;
				st.browMenuX = hit.Mouse().x;
				st.browMenuY = hit.Mouse().y;
			}
			int32 dropTo = -999; // -1 racine, i dossier, -100 fond de grille
			int32 chevB2 = -1;   // dossier dont la FLECHE vient d'etre cliquee
			const bool freshB = hit.MouseDown() && !st.browMouseWasDown && !uiModal;
			const nkgui::NkVec2 bm = hit.Mouse();
			int32 folderCount = 0;
			{
				float32 dy = ty + S(4.f) - treeScroll;
				char fkey[40];
				// Racine Â« Contenu Â».
				{
					const NkRect rowR{r.x, dy, treeW, kRowH};
					const bool over = hit.Add("brow.root", rowR);
					const bool on = (st.browserFolder < 0);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					p.IconV(r.x + S(6.f), dy, kRowH, NkIcon::Drawer,
							on ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					p.TextV(r.x + S(24.f), dy, kRowH, "Contenu",
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (!uiModal && hit.Clicked("brow.root"))
						st.browserFolder = -1;
					if (st.browDragging && NkHitRegistry::Contains(rowR, bm)) {
						dropTo = -1; // vers la RACINE
						p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
							   NkRole::AccentUi);
					}
					dy += kRowH;
				}
				// ARBRE RECURSIF : chaque sous-dossier s'indente sous son parent,
				// comme la hierarchie (regle de Rihen, facon Unreal).
				int32 tstk[64];
				int32 tdep[64];
				int32 tsp = 0;
				for (int32 i = st.browserCount - 1; i >= 0; --i)
					if (st.browserKind[i] == 1 &&
						(st.browserParent[i] < 0 ||
						 st.browserKind[st.browserParent[i]] != 1)) {
						tstk[tsp] = i;
						tdep[tsp] = 0;
						++tsp;
					}
				while (tsp > 0) {
					--tsp;
					const int32 i = tstk[tsp];
					const int32 dep = tdep[tsp];
					++folderCount;
					const float32 ind = (float32)dep * S(14.f);
					const bool on = (st.browserFolder == i);
					const NkRect rowR{r.x, dy, treeW, kRowH};
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					const bool over = hit.Add(fkey, rowR);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					// CHEVRON si le dossier a des SOUS-DOSSIERS ; icone COLOREE si
					// plein, eteinte si vide (regles de Rihen).
					bool hasSub = false, hasAny = false;
					for (int32 c7 = 0; c7 < st.browserCount; ++c7)
						if (st.browserParent[c7] == i && st.browserKind[c7] != 255) {
							hasAny = true;
							if (st.browserKind[c7] == 1)
								hasSub = true;
						}
					const bool foldB = ((st.browFold[i >> 5] >> (i & 31)) & 1u) != 0u;
					if (hasSub) {
						snprintf(fkey, sizeof(fkey), "brow.chev.%d", i);
						// PLIAGE : la FLECHE seule, en geometrie brute (voir la
						// hierarchie -- meme raison).
						const NkRect chevB{r.x + ind - S(4.f), dy, S(24.f), kRowH};
						p.IconV(r.x + S(2.f) + ind, dy, kRowH,
								foldB ? NkIcon::ChevronRight : NkIcon::ChevronDown,
								on ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
						if (in.mouseClicked[0] && !uiModal &&
							NkHitRegistry::Contains(chevB, bm)) {
							st.browFold[i >> 5] ^= (1u << (i & 31));
							chevB2 = i; // ce clic n'entre pas dans le dossier
						}
					}
					p.IconV(r.x + S(18.f) + ind, dy, kRowH,
							on ? NkIcon::FolderOpen : NkIcon::Folder,
							hasAny ? NkRole::TypeFolder : NkRole::TextMuted, 13.f);
					snprintf(fkey, sizeof(fkey), "brow.dirname.%d", i);
					EditableText(p, hit, ws, in, fkey,
								 {r.x + S(36.f) + ind, dy, treeW - S(42.f) - ind, kRowH},
								 st.browserNames[i],
								 on ? NkRole::TextOnAccent : NkRole::Text, st.browserNames[i], 32u);
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					if (!uiModal && hit.Clicked(fkey) && chevB2 != i)
						st.browserFolder = i;
					if (in.mouseClicked[1] && NkHitRegistry::Contains(rowR, bm)) {
						st.browMenuIdx = i;
						st.browMenuX = bm.x;
						st.browMenuY = bm.y;
					}
					// GLISSER-DEPOSER : armement sur la ligne, cible au survol.
					if (freshB && NkHitRegistry::Contains(rowR, bm)) {
						st.browDragIdx = i;
						st.browDragX = bm.x;
						st.browDragY = bm.y;
						st.browDragging = false;
						st.browDragFromTree = true;
					}
					if (st.browDragging && st.browDragIdx != i &&
						NkHitRegistry::Contains(rowR, bm)) {
						dropTo = i;
						p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
							   NkRole::AccentUi);
					}
					dy += kRowH;
					if (!foldB)
					for (int32 c6 = st.browserCount - 1; c6 >= 0; --c6)
						if (st.browserKind[c6] == 1 && st.browserParent[c6] == i && tsp < 62) {
							tstk[tsp] = c6;
							tdep[tsp] = dep + 1;
							++tsp;
						}
				}
				if (folderCount == 0)
					p.TextV(r.x + S(8.f), dy, kRowH, "Aucun dossier", NkRole::TextMuted);
			}

			// â”€â”€ CARTES : le contenu du dossier courant â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Plus AUCUNE donnee simulee : chaque carte est un materiau ou une
			// texture cree par l'utilisateur, rattache au dossier ouvert. Le style
			// (ombre, damier, bande de type, pied a deux lignes) est celui valide
			// avec Rihen sur les cartes precedentes.
			const float32 ax = r.x + treeW + S(14.f);
			const float32 tw = 96.f;
			const float32 pvH = 96.f;
			const float32 barH2 = 3.f;
			const float32 footH = 34.f;
			const float32 cardH = pvH + barH2 + footH;
			float32 tx = ax;
			float32 tyy = ty + S(38.f) - assetScroll; // sous le bandeau de recherche
			int32 shown = 0;
			char akey[40];
			const float32 wrapW = r.x + r.w - S(16.f);
			// L'ORDRE D'AFFICHAGE VIENT DU CLASSEMENT, plus de l'ordre de creation
			// (Rihen : « on doit avoir un vrai systeme pour classer et organiser un
			// espace comme l'explorateur de fichiers »). Le filtrage et la recherche
			// vivent avec lui, dans NkBrowVisible : deux endroits qui decident ce qui
			// est visible finiraient par ne plus etre d'accord.
			int32 vis[NkModelerState::kMaxBrowser];
			const int32 visN = NkBrowVisible(st, vis, NkModelerState::kMaxBrowser);
			for (int32 vi = 0; vi < visN; ++vi) {
				const int32 i = vis[vi];
				++shown;
				if (tx + tw > wrapW) { // retour a la ligne
					tx = ax;
					tyy += cardH + S(14.f);
				}
				const uint8 kind = st.browserKind[i];
				// COULEUR ET NOM viennent du point de passage unique (NkModelerUI.h) :
				// la pastille de filtre et le liseret d'onglet lisent la MEME table.
				const NkColor role = NkAssetColor(p, kind, st.browserSub[i]);
				const char *kindName = NkAssetKindName(kind, st.browserSub[i]);

				// Ombre portee legere, comme Unreal.
				p.Fill({tx + 2.f, tyy + 3.f, tw, cardH}, NkColor{0, 0, 0, 90}, 3.f);
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				const bool selCard = (st.selectedAsset == i);
				hit.Add(akey, {tx, tyy, tw, cardH});
				if (selCard)
					p.Fill({tx - 2.f, tyy - 2.f, tw + 4.f, cardH + 4.f}, NkRole::AccentUi, 3.f);

				// Damier de fond : il dit Â« ce fond est vide Â».
				const float32 c = 8.f;
				p.Fill({tx, tyy, tw, pvH}, NkRole::InputBg);
				for (int32 gx = 0; gx * c < tw; ++gx)
					for (int32 gy = 0; gy * c < pvH; ++gy)
						if (((gx + gy) & 1) == 0) {
							const float32 cw = ((float32)(gx + 1) * c > tw) ? tw - (float32)gx * c : c;
							const float32 ch =
								((float32)(gy + 1) * c > pvH) ? pvH - (float32)gy * c : c;
							p.Fill({tx + (float32)gx * c, tyy + (float32)gy * c, cw, ch},
								   NkRole::WindowBg);
						}
				const float32 cx = tx + tw * 0.5f, cy = tyy + pvH * 0.5f;
				// ── MINIATURES : POINT D'ACCROCHE UNIQUE (Rihen, 8 aout 2026) ──
				// TOUS les fichiers -- procedural ou non -- montreront bientot une
				// MINIATURE plutot qu'un glyphe : soit une vignette, soit le
				// RESULTAT de l'asset.
				//
				// REGLE POSEE PAR RIHEN, a ne pas retrouver a la dure plus tard :
				// la miniature vient de LA VUE, c'est-a-dire de ce que la vue 3D
				// regarde -- PAS d'un noeud camera de la scene. Une camera est un
				// objet que l'utilisateur place pour un rendu ; la vignette d'un
				// asset doit montrer l'asset tel qu'on le voit en le travaillant,
				// et un projet sans camera doit avoir ses vignettes quand meme.
				//
				// Tout le dessin ci-dessous est le REPLI : il n'a lieu que tant
				// qu'aucune miniature n'existe pour cette carte. C'est ici, et
				// nulle part ailleurs, qu'elle viendra s'inserer -- un second
				// endroit qui dessinerait un apercu divergerait au premier
				// changement de cadrage.
				if (kind == 1) {
					// DOSSIER : chemise avec rabat ; PLEINE ou VIDE selon son
					// contenu (previsualisation par contenu, regle de Rihen).
					bool fullF = false;
					for (int32 j3 = 0; j3 < st.browserCount; ++j3)
						if (st.browserKind[j3] != 255 && st.browserParent[j3] == i) {
							fullF = true;
							break;
						}
					p.Fill({cx - 22.f, cy - 18.f, 18.f, 7.f}, role); // rabat
					if (fullF) {
						// des feuilles depassent : le dossier est habite
						p.Fill({cx - 14.f, cy - 14.f, 30.f, 4.f}, NkRole::Text);
						p.Fill({cx - 17.f, cy - 9.f, 36.f, 4.f}, NkRole::TextMuted);
						p.Fill({cx - 22.f, cy - 11.f, 44.f, 28.f}, role);
					} else {
						// vide : le corps n'est qu'un contour
						p.Fill({cx - 22.f, cy - 11.f, 44.f, 28.f}, NkRole::PanelHeader);
						p.OutlineSharp({cx - 22.f, cy - 11.f, 44.f, 28.f}, role);
					}
				} else if (kind == 0) {
					// PROCEDURAL : DEUX NOEUDS RELIES, pas un cube (Rihen). Un
					// fichier procedural ne montre pas un objet, il montre la
					// RECETTE qui le fabrique -- l'ancien cube le faisait passer
					// pour un Model, c'est-a-dire pour son resultat.
					// Les broches sont dessinees : c'est ce qui distingue un
					// graphe de deux rectangles quelconques a cette taille.
					const float32 nw = 20.f, nh = 13.f;
					const float32 ax0 = cx - 23.f, ay0 = cy - 16.f;
					const float32 bx0 = cx + 3.f, by0 = cy + 3.f;
					p.Fill({ax0, ay0, nw, nh}, NkRole::PanelHeader);
					p.Fill({ax0, ay0, nw, 4.f}, role); // bandeau d'en-tete
					p.OutlineSharp({ax0, ay0, nw, nh}, role);
					p.Fill({bx0, by0, nw, nh}, NkRole::PanelHeader);
					p.Fill({bx0, by0, nw, 4.f}, role);
					p.OutlineSharp({bx0, by0, nw, nh}, role);
					// Le FIL : sortie droite du premier -> entree gauche du second.
					const float32 ox = ax0 + nw, oy = ay0 + nh - 4.f;
					const float32 ix = bx0, iy = by0 + 4.f;
					p.Disc(ox, oy, 3.f, role);
					p.Disc(ix, iy, 3.f, role);
					p.Line(ox, oy, ox + 6.f, oy, role);
					p.Line(ox + 6.f, oy, ix - 6.f, iy, role);
					p.Line(ix - 6.f, iy, ix, iy, role);
				} else if (kind == 2) {
					// Boule de rendu avec reflet : sans reflet, le disque se lit
					// comme une pastille de couleur.
					p.Disc(cx, cy, 22.f, role);
					p.Disc(cx - 8.f, cy - 8.f, 5.f, NkRole::Text);
				} else if (kind == 5) {
					// SCENE : la MINIATURE REELLE si elle existe (elle vient de
					// LA VUE, cf. la regle ci-dessus) ; sinon le globe raye --
					// un monde a ouvrir.
					const int32 dTh = st.browserDoc[i] - 1;
					if (dTh >= 0 && dTh < NkModelerState::kMaxDocs &&
						st.docThumb[dTh] == 1 && st.docThumbW[dTh] > 0 &&
						st.docThumbH[dTh] > 0) {
						// AJUSTEE SANS DEFORMER, centree sur le damier : la vue
						// est large, la carte presque carree — deformer rendrait
						// toutes les vignettes menteuses.
						const float32 iw = (float32)st.docThumbW[dTh];
						const float32 ih = (float32)st.docThumbH[dTh];
						const float32 sc =
							((tw / iw) < (pvH / ih)) ? (tw / iw) : (pvH / ih);
						const float32 dw = iw * sc, dh = ih * sc;
						p.Image(4500u + (uint32)dTh, {tx + (tw - dw) * 0.5f,
													  tyy + (pvH - dh) * 0.5f, dw, dh});
					} else {
						p.Disc(cx, cy, 22.f, role);
						p.Fill({cx - 22.f, cy - 2.f, 44.f, 4.f}, NkRole::PanelHeader);
						p.Fill({cx - 2.f, cy - 22.f, 4.f, 44.f}, NkRole::PanelHeader);
					}
				} else if (kind == 6) {
					// MESH reutilisable : cube plein -- une piece prete a cloner.
					p.Fill({cx - 16.f, cy - 14.f, 32.f, 28.f}, role);
					p.Line(cx - 16.f, cy - 14.f, cx - 8.f, cy - 22.f, NkRole::Text);
					p.Line(cx - 8.f, cy - 22.f, cx + 24.f, cy - 22.f, NkRole::Text);
					p.Line(cx + 16.f, cy - 14.f, cx + 24.f, cy - 22.f, NkRole::Text);
					p.Line(cx + 24.f, cy - 22.f, cx + 24.f, cy + 6.f, NkRole::Text);
					p.Line(cx + 16.f, cy + 14.f, cx + 24.f, cy + 6.f, NkRole::Text);
				} else if (kind == 4) {
					// DATASET IA : un document ligne (paires d'entrainement JSONL).
					p.Fill({cx - 16.f, cy - 20.f, 32.f, 40.f}, NkRole::PanelHeader);
					p.OutlineSharp({cx - 16.f, cy - 20.f, 32.f, 40.f}, role);
					for (int32 ln = 0; ln < 4; ++ln)
						p.Fill({cx - 11.f, cy - 13.f + (float32)ln * 8.f,
								(ln == 3) ? 12.f : 22.f, 3.f},
							   role);
				} else {
					const float32 q = 6.f, half = q * 3.f;
					const NkColor damier = p.C(NkRole::PanelHeader);
					for (int32 gx = 0; gx < 6; ++gx)
						for (int32 gy = 0; gy < 6; ++gy)
							p.Fill({cx - half + (float32)gx * q, cy - half + (float32)gy * q, q, q},
								   ((gx + gy) & 1) ? role : damier);
				}

				// Bande de type, puis nom et type DANS la carte, tronques.
				p.Fill({tx, tyy + pvH, tw, barH2}, role);
				p.Fill({tx, tyy + pvH + barH2, tw, footH}, NkRole::PanelHeader);
				{
					const float32 fy = tyy + pvH + barH2;
					const float32 lh = p.LineH();
					const float32 pad = 6.f;
					float32 fyy = fy + (footH - (lh * 2.f + 2.f)) * 0.5f;
					snprintf(akey, sizeof(akey), "brow.cardname.%d", i);
					// Le nom est CLIPPE a la carte : il debordait sur la carte
					// voisine des qu'il etait long (constate par Rihen).
					p.Clip({tx + pad, fyy, tw - pad * 2.f, lh + 2.f});
					if (EditableText(p, hit, ws, in, akey, {tx + pad, fyy, tw - pad * 2.f, lh + 2.f},
									 st.browserNames[i], NkRole::Text, st.browserNames[i], 32u)) {
						// RENOMMER LA CARTE D'UNE SCENE RENOMME SON DOCUMENT (Rihen :
						// « renommer dans le navigateur ne l'a pas fait dans
						// l'onglet »). Uniquement AU MOMENT de la validation : une
						// copie a chaque frame ecraserait, elle, le renommage fait
						// depuis l'onglet -- les deux sens doivent coexister. Le nom
						// va au DOCUMENT, donc l'onglet qui le montre suit tout seul.
						if (st.browserKind[i] == 5) {
							const int32 d9 = st.browserDoc[i] - 1;
							if (d9 >= 0 && d9 < NkModelerState::kMaxDocs && st.docUsed[d9])
								NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
						}
					}
					p.Unclip();
					fyy += lh + 2.f;
					p.TextClipped(tx + pad, fyy, tw - pad * 2.f, kindName, NkRole::TextMuted);
				}
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				if (!uiModal && kind == 1 && hit.DoubleClicked(akey))
					st.browserFolder = i; // double-clic : ENTRER dans le dossier
				if (!uiModal && kind != 1 && hit.DoubleClicked(akey)) {
					// OUVRIR l'asset : une SCENE s'ajoute a la barre d'onglets
					// avec son contenu ; les autres natures ouvrent leur EDITEUR
					// specialise dans un onglet dedie (Rihen).
					const uint8 tk9 = (uint8)(kind == 5 ? 0 : 1 + kind);
					int32 tb = -1;
					// UNE SCENE se retrouve par son DOCUMENT : c'est lui qui porte le
					// contenu, et c'est ce lien qui manquait -- le double-clic
					// fabriquait un document neuf, donc une scene VIDE a la place de
					// celle qu'on croyait rouvrir.
					const int32 dCard = (kind == 5) ? (st.browserDoc[i] - 1) : -1;
					for (int32 t9 = 0; t9 < st.sceneCount; ++t9) {
						if (kind == 5) {
							if (st.sceneTabKind[t9] == 0 && st.TabDoc(t9) == dCard && dCard >= 0)
								tb = t9;
						} else if (st.sceneTabAsset[t9] == i + 1 && st.sceneTabKind[t9] == tk9)
							tb = t9; // deja ouvert : on l'ACTIVE simplement
					}
					if (tb < 0 && st.sceneCount < 8) {
						int32 d9 = dCard;
						if (kind == 5) {
							// Carte de scene sans document (fichier d'une version
							// anterieure) : on lui en donne un, vide, plutot que de
							// refuser -- mais VIDE, et il le restera : rien n'a ete
							// enregistre pour elle.
							if (d9 < 0 || d9 >= NkModelerState::kMaxDocs || !st.docUsed[d9]) {
								d9 = st.DocAlloc();
								if (d9 >= 0) {
									NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
									st.docScene[d9] = (uint8)st.sceneIdNext++;
									st.docBlank[d9] = true;
									st.docCard[d9] = i + 1;
									st.browserDoc[i] = d9 + 1;
								}
							}
						} else {
							// EDITEUR d'asset : sa maquette est TRANSITOIRE, elle
							// meurt avec la vue. L'asset, lui, est la carte.
							d9 = st.DocAlloc();
							if (d9 >= 0) {
								st.docTransient[d9] = true;
								NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
								st.docScene[d9] = (uint8)st.sceneIdNext++;
								st.docBlank[d9] = true;
							}
						}
						// Table des documents pleine : on n'ouvre RIEN plutot qu'un
						// onglet qui ne montrerait aucune scene.
						if (d9 >= 0) {
							tb = st.sceneCount++;
							st.sceneTabAsset[tb] = i + 1;
							st.sceneTabKind[tb] = tk9;
							st.sceneTabDoc[tb] = d9;
						}
					}
					if (tb >= 0)
						NkActivateTab(st, tb);
				}
				if (in.mouseClicked[1] &&
					NkHitRegistry::Contains({tx, tyy, tw, cardH}, bm)) {
					st.browMenuIdx = i;
					st.browMenuX = hit.Mouse().x;
					st.browMenuY = hit.Mouse().y;
				}
				{
					// GLISSER-DEPOSER : la carte s'arme au premier appui ; une
					// carte-DOSSIER est une cible de depot.
					const NkRect cardR{tx, tyy, tw, cardH};
					if (freshB && NkHitRegistry::Contains(cardR, bm)) {
						st.browDragIdx = i;
						st.browDragX = bm.x;
						st.browDragY = bm.y;
						st.browDragging = false;
						st.browDragFromTree = false;
					}
					if (st.browDragging && st.browDragIdx != i && kind == 1 &&
						NkHitRegistry::Contains(cardR, bm)) {
						dropTo = i;
						p.OutlineSharp(cardR, NkRole::AccentUi);
					}
				}
				if (!uiModal && hit.Clicked(akey))
					st.selectedAsset = i;
				tx += tw + S(12.f);
			}
			if (shown == 0) {
				// Dire POURQUOI c'est vide : un filtre actif n'est pas un dossier
				// vide, et laisser croire l'inverse ferait chercher un bug.
				const bool filtering = st.searchBrowser[0] || st.browFilter != 0u;
				p.TextV(ax, ty + S(40.f), kRowH,
						filtering ? "Aucun element ne correspond a la recherche"
								  : "Vide -- creez un dossier, un materiau ou une texture",
						NkRole::TextMuted);
			}

			// ── RECHERCHE + PASTILLES DE TYPE, sur son bandeau (maquette) ───────
			// Peinte APRES les cartes, donc PAR-DESSUS : au defilement, elles
			// remontaient sur le bandeau et passaient devant (constate par
			// Rihen). Un bandeau fixe doit couvrir ce qui defile, et il gagne du
			// meme coup les clics sur la bande qu'il occupe.
			{
				const float32 fy = ty + S(6.f);
				const float32 fh = S(22.f);
				p.Fill({r.x + treeW + 1.f, ty, r.w - treeW - 1.f, S(34.f)}, NkRole::PanelHeader);
				p.HLine(r.x + treeW + 1.f, ty + S(34.f), r.w - treeW - 1.f);
				// Le champ de la hierarchie : filtrage a la frappe, invite gardee
				// HORS du buffer, croix d'effacement.
				PaintSearch(p, {ax - S(6.f), 0.f, S(192.f), 0.f}, fy - S(4.f), hit, ws, in,
							"brow.search", st.searchBrowser);
				float32 px = ax + S(196.f);
				// Le nom et la couleur viennent du point de passage unique : une
				// pastille qui aurait garde sa propre table finirait par annoncer
				// une couleur que les cartes n'emploient plus.
				static const uint8 kChipKind[5] = {6, 0, 2, 3, 5};
				char ck[32];
				for (int32 ci = 0; ci < 5; ++ci) {
					const uint8 ckd = kChipKind[ci];
					// Le filtre « procedural » couvre les QUATRE graphes : sa
					// pastille porte donc le nom de la famille, pas d'un sous-type.
					const char *cname = (ckd == 0) ? "Procedural" : NkAssetKindName(ckd, 0);
					const NkColor ccol = NkAssetColor(p, ckd, 0);
					const float32 cw = p.TextW(cname) + S(28.f);
					if (px + cw > r.x + r.w - S(10.f))
						break;
					const NkRect cr{px, fy, cw, fh};
					snprintf(ck, sizeof(ck), "brow.chip.%d", ci);
					const bool ov = hit.Add(ck, cr);
					const bool on = (st.browFilter & (1u << ckd)) != 0;
					p.Fill(cr, on ? NkRole::InputBg : NkRole::PanelBg, 11.f);
					if (on)
						p.OutlineSharp(cr, ccol);
					else
						p.OutlineSharp(cr, ov ? NkRole::AccentUi : NkRole::Border);
					// La PUCE porte la couleur de la famille : c'est elle qui les
					// distingue d'un coup d'oeil, comme sur la maquette.
					p.Fill({cr.x + S(9.f), cr.y + fh * 0.5f - S(3.f), S(6.f), S(6.f)}, ccol,
						   3.f);
					p.TextV(cr.x + S(20.f), cr.y, fh, cname,
							on ? NkRole::Text : NkRole::TextMuted);
					if (hit.Clicked(ck))
						st.browFilter ^= (1u << ckd);
					px += cw + S(6.f);
				}
				// ── CLASSEMENT, cale a DROITE ───────────────────────────────
				// Meme place que dans un explorateur : le tri est un reglage de la
				// vue, pas un filtre -- le coller aux pastilles les ferait passer
				// pour une pastille de plus.
				{
					static const char *const kSortN[3] = {"Nom", "Type", "Date"};
					const char *cur = kSortN[(st.browSort >= 0 && st.browSort < 3)
												 ? st.browSort
												 : 0];
					const float32 aw = fh; // bouton de sens, carre
					const float32 sw = p.TextW("Type") + S(34.f);
					const float32 sx = r.x + r.w - S(10.f) - aw - S(4.f) - sw;
					if (sx > px + S(8.f) && sortCombo) {
						p.TextV(sx - p.TextW("Trier") - S(8.f), fy, fh, "Trier",
								NkRole::TextMuted);
						Combo(p, hit, ws, "brow.sort", {sx, fy, sw, fh}, kSortN, nullptr, 3,
							  st.browSort, *sortCombo);
						const NkRect ar2{sx + sw + S(4.f), fy, aw, fh};
						const bool ov2 = hit.Add("brow.sortdir", ar2);
						p.Outline(ar2, NkRole::Border, ov2 ? NkRole::PanelBg : NkRole::InputBg,
								  4.f);
						// Le CHEVRON dit le sens : vers le bas = croissant, comme la
						// fleche d'un en-tete de colonne d'explorateur.
						p.IconV(ar2.x, ar2.y, ar2.h,
								st.browSortDesc ? NkIcon::ChevronUp : NkIcon::ChevronDown,
								NkRole::Text, 11.f);
						if (hit.Clicked("brow.sortdir"))
							st.browSortDesc = !st.browSortDesc;
					}
					(void)cur;
				}
			}

			p.Unclip();
			const NkRect treeArea{r.x, ty, treeW, th};
			// La zone des cartes va JUSQU'AU BORD DROIT du panneau : elle
			// s'arretait 6 px avant, et sa barre paraissait flotter (Rihen).
			const NkRect assetArea{ax - S(10.f), ty + S(34.f), r.x + r.w - (ax - S(10.f)),
								   th - S(34.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			// La hauteur de contenu etait figee a 125 px, valeur de l'ancienne carte.
			// Les cartes font maintenant 133 px : le defilement s'arretait avant le bas
			// et la derniere rangee restait inaccessible.
			const float32 assetContentH = (tyy + cardH + S(14.f)) - ty + assetScroll;
			hit.Wheel("brow.assets", st.scrollAssets, assetContentH, assetArea.h);
			// LES DEUX COTES DU NAVIGATEUR portent la meme barre que les
			// proprietes (Rihen). La grille commence sous le bandeau de
			// recherche : sa gouttiere aussi, sinon la barre le recouvrirait.
			NkPaintVScroll(p, guiCtx, treeArea, 5.f * kRowH + S(8.f), st.scrollTree, 0x42524F57u);
			NkPaintVScroll(p, guiCtx, assetArea, assetContentH, st.scrollAssets, 0x42415353u);
			// LACHER du glisser-deposer (4 sens, garde anti-cycle) + fantome.
			if (st.browDragIdx >= 0) {
				if (hit.MouseDown()) {
					if (!st.browDragging) {
						const float32 dxb = bm.x - st.browDragX, dyb = bm.y - st.browDragY;
						if (dxb * dxb + dyb * dyb > 36.f)
							st.browDragging = true;
					}
					if (st.browDragging)
						p.TextV(bm.x + S(14.f), bm.y - kRowH * 0.5f, kRowH,
								st.browserNames[st.browDragIdx], NkRole::Text);
				} else {
					if (st.browDragging) {
						// Sur la VUE : importer un CLONE dans la scene (Rihen).
						if (!NkHitRegistry::Contains(st.browserRect, bm) &&
							NkHitRegistry::Contains(st.viewRect, bm) &&
							st.browserKind[st.browDragIdx] == 6) {
							int32 nn6 = -1;
							if (st.browserSrcNode[st.browDragIdx] > 0)
								nn6 = demo::Demo3DHostDuplicateNode(
									st.browserSrcNode[st.browDragIdx] - 1);
							if (nn6 < 0) // asset sans source : cube par defaut
								nn6 = demo::Demo3DHostAddNode(2, 0);
							if (nn6 >= 0)
								demo::Demo3DHostSelectEmptyNode(nn6);
							st.browDragIdx = -1;
						}
						if (st.browDragIdx >= 0 && dropTo == -999 &&
							NkHitRegistry::Contains({r.x + treeW, ty, r.w - treeW, th}, bm))
							dropTo = -100; // fond de grille = dossier COURANT
						if (dropTo != -999) {
							const int32 dest = (dropTo == -100) ? st.browserFolder : dropTo;
							bool ok5 = (dest != st.browDragIdx);
							for (int32 c5 = dest; c5 >= 0 && ok5; c5 = st.browserParent[c5])
								if (c5 == st.browDragIdx)
									ok5 = false;
							if (ok5) {
								const bool destTree =
									NkHitRegistry::Contains({r.x, ty, treeW, th}, bm);
								if (st.browDragFromTree != destTree) {
									// TRAVERSEE gauche <-> droite, dans les DEUX sens :
									// GAUCHE -> DROITE : la carte Copier/Deplacer decide
									// (Rihen) -- cliquer dans le vide annulera.
									st.browAskIdx = st.browDragIdx;
									st.browAskDest = dest;
									st.browAskX = bm.x;
									st.browAskY = bm.y;
								} else {
									NkBrowRequestTransfer(st, st.browDragIdx, dest, false,
														  bm.x, bm.y);
								}
							}
						}
					}
					st.browDragIdx = -1;
				}
			}
			if (!hit.MouseDown() && st.browDragIdx < 0)
				st.browDragging = false;
			st.browMouseWasDown = hit.MouseDown();

			char cnt[32];
			snprintf(cnt, sizeof(cnt), "%d element(s)", shown);
			const float32 ew = p.TextW(cnt);
			p.TextV(r.x + r.w - ew - kPad, r.y + r.h - kRowH, kRowH, cnt, NkRole::TextMuted);
			(void)ih;
		}

		// â”€â”€ CONTENU DES MENUS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Une TABLE plutot que du code : ajouter une entree ne demande pas de
		// toucher au rendu, et la meme table servira la palette de recherche et le
		// menu contextuel -- une liste ecrite deux fois finit par diverger.
		//
		// `command` est la CLE STABLE de NkShortcutTable quand l'entree en a une :
		// c'est elle qui permet d'afficher le raccourci sans le recopier. Vide pour
		// les entrees qui n'en ont pas encore.
		struct NkMenuItem {
				const char *label;   ///< nullptr = separateur
				const char *command; ///< cle NkShortcutTable, ou ""
				bool submenu;		 ///< ouvre un sous-menu
		};
		struct NkMenuDef {
				const char *title;
				const NkMenuItem *items;
				int32 count;
		};

		inline const NkMenuDef *NkMenus(int32 &outCount) {
			static const NkMenuItem kFile[] = {
				{"Nouveau", "", false},
				{"Ouvrir...", "", false},
				{"Ouvrir recent", "", true},
				{nullptr, "", false},
				// Le RACCOURCI est lu dans la table, jamais recopie ici : rebinder
				// une touche met l'affichage a jour tout seul.
				{"Enregistrer", "app.enregistrer", false},
				{"Enregistrer tout", "app.enregistrer_tout", false},
				{"Enregistrer sous...", "", false},
				{nullptr, "", false},
				{"Importer", "", true},
				{"Exporter", "", true},
				{nullptr, "", false},
				{"Quitter", "", false},
			};
			static const NkMenuItem kEdit[] = {
				{"Annuler", "app.annuler", false}, {"Refaire", "app.refaire", false},
				{nullptr, "", false},			   {"Dupliquer", "objet.dupliquer", false},
				{"Supprimer", "objet.supprimer", false}, {nullptr, "", false},
				{"Preferences...", "", false},
			};
			static const NkMenuItem kWindow[] = {
				{"Hierarchie", "", false},		{"Proprietes", "", false}, {"Details", "", false},
				{"Navigateur de projet", "", false}, {nullptr, "", false},
				{"Panneau d'outils", "app.panneau_outils", false}, {nullptr, "", false},
				{"Plein ecran", "", false},
			};
			static const NkMenuItem kTools[] = {
				{"Rechercher une commande", "app.palette", false}, {nullptr, "", false},
				{"Retopologier", "", false},   {"Decimer...", "", false},
				{nullptr, "", false},		   {"Extensions", "", true},
			};
			static const NkMenuItem kSelect[] = {
				{"Tout selectionner", "", false}, {"Tout deselectionner", "", false},
				{"Inverser", "", false},		  {nullptr, "", false},
				{"Par type", "", true},
			};
			static const NkMenuItem kObject[] = {
				{"Deplacer", "objet.deplacer", false}, {"Tourner", "objet.tourner", false},
				{"Redimensionner", "objet.echelle", false}, {nullptr, "", false},
				{"Ajouter un modificateur", "", true}, {"Appliquer tout", "", false},
			};
			static const NkMenuItem kHelp[] = {
				{"Documentation", "", false}, {"Raccourcis clavier", "", false},
				{nullptr, "", false},		  {"A propos", "", false},
			};
			// LE NOMBRE D'ENTREES SE DEDUIT DE LA TABLE, il ne se recopie pas.
			// Il etait ecrit a la main : ajouter « Enregistrer tout » a kFile sans
			// toucher le 11 a fait DISPARAITRE « Quitter » (constate par Rihen).
			// C'est la quatrieme fois que ce depot paie une table recopiee ailleurs
			// (les combos de la sortie, kVidExt fige a 3, kHdrNames reste a six) --
			// ici la duplication disparait pour de bon.
#define NK_MENU(nom, tab) {nom, tab, (int32)(sizeof(tab) / sizeof((tab)[0]))}
			static const NkMenuDef kMenus[] = {
				NK_MENU("Fichier", kFile),	   NK_MENU("Edition", kEdit),
				NK_MENU("Fenetre", kWindow),   NK_MENU("Outils", kTools),
				NK_MENU("Selection", kSelect), NK_MENU("Objet", kObject),
				NK_MENU("Aide", kHelp),
			};
#undef NK_MENU
			outCount = (int32)(sizeof(kMenus) / sizeof(kMenus[0]));
			return kMenus;
		}

		// â”€â”€ DEROULEMENT D'UN MENU â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Peint APRES tout le reste : un menu doit recouvrir les panneaux, et le
		// registre de zones donne la priorite a ce qui est declare en dernier.
		inline void PaintOpenMenu(NkModelerPainter &p, const NkRect &bar, NkModelerState &st,
								  NkHitRegistry &hit, const NkShortcutTable &sc) {
			if (st.openMenu < 0)
				return;
			int32 nMenus = 0;
			const NkMenuDef *menus = NkMenus(nMenus);
			if (st.openMenu >= nMenus)
				return;
			const NkMenuDef &m = menus[st.openMenu];

			// Position : sous l'entree de barre correspondante. On recalcule les
			// largeurs comme la barre pour retomber exactement dessous -- une
			// constante recopiee se decalerait a la premiere traduction.
			float32 x = S(10.f) + S(24.f) + S(16.f);
			for (int32 i = 0; i < st.openMenu; ++i)
				x += p.TextW(menus[i].title) + S(18.f);
			x -= S(9.f);

			const float32 itemH = S(24.f), sepH = S(7.f);
			float32 w = S(150.f);
			char keys[32];
			for (int32 i = 0; i < m.count; ++i) {
				if (!m.items[i].label)
					continue;
				float32 need = p.TextW(m.items[i].label) + S(70.f);
				if (m.items[i].command && *m.items[i].command
					&& sc.FormatFor(m.items[i].command, keys, sizeof(keys)))
					need += p.TextW(keys);
				if (need > w)
					w = need;
			}
			float32 h = S(8.f);
			for (int32 i = 0; i < m.count; ++i)
				h += m.items[i].label ? itemH : sepH;

			const NkRect box{x, bar.y + bar.h, w, h};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f); // ombre portee
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("menu.panel", box); // avale les clics qui tombent dans le menu

			float32 y = box.y + S(4.f);
			for (int32 i = 0; i < m.count; ++i) {
				if (!m.items[i].label) {
					// Separateur : il ne s'etend PAS bord a bord, il est retrait de la
					// marge du texte -- sinon il coupe le menu en deux blocs qui ont
					// l'air sans rapport.
					p.HLine(box.x + S(8.f), y + sepH * 0.5f, box.w - S(16.f));
					y += sepH;
					continue;
				}
				const NkRect ir{box.x + 2.f, y, box.w - 4.f, itemH};
				snprintf(keys, sizeof(keys), "menu.item.%d", i);
				const bool over = hit.Add(keys, ir);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.TextV(ir.x + S(12.f), y, itemH, m.items[i].label,
						over ? NkRole::TextOnAccent : NkRole::Text);
				// Le RACCOURCI est lu dans la table, jamais recopie : rebinder une
				// touche met l'affichage a jour tout seul.
				if (m.items[i].command && *m.items[i].command
					&& sc.FormatFor(m.items[i].command, keys, sizeof(keys))) {
					const float32 kw = p.TextW(keys);
					p.TextV(ir.x + ir.w - kw - S(12.f), y, itemH, keys,
							over ? NkRole::TextOnAccent : NkRole::TextMuted);
				}
				if (m.items[i].submenu)
					p.IconV(ir.x + ir.w - S(18.f), y, itemH, NkIcon::ChevronRight,
							over ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				snprintf(keys, sizeof(keys), "menu.item.%d", i);
				if (hit.Clicked(keys) && !m.items[i].submenu) {
					// Une entree SANS sous-menu referme le menu. Une entree AVEC en
					// ouvrirait un second -- non ecrit tant qu'aucune n'a de contenu
					// reel, plutot qu'un panneau vide qui ferait croire a un bug.
					//
					// MENU FICHIER : les indices sont ceux de kFile, plus haut. Ils
					// sont ecrits ICI et nulle part ailleurs ; inserer une entree
					// dans la table oblige a relire ce bloc -- ce qui est voulu, un
					// decalage silencieux serait bien pire.
					//   0 Nouveau · 1 Ouvrir... · 2 Ouvrir recent (sous-menu, non
					//   ecrit) · 4 Enregistrer · 5 Enregistrer tout · 6 Enregistrer
					//   sous... · 11 Quitter
					// Les demandes partent en DIFFERE (`projPending`) : le selecteur
					// de fichiers de l'OS ouvre une boucle modale, l'appeler pendant
					// la peinture reentrerait dans la frame en cours.
					if (st.openMenu == 0) {
						if (i == 0)
							st.projPending = 1;
						else if (i == 1)
							st.projPending = 2;
						else if (i == 4)
							st.projPending = 3;
						else if (i == 5)
							st.projPending = 8;
						else if (i == 6)
							st.projPending = 4;
						else if (i == 11)
							NkRequestClose(st);
					}
					st.openMenu = -1;
				}
				y += itemH;
			}

			// UN CLIC AILLEURS REFERME. Teste en dernier, apres que toutes les zones
			// du menu ont ete declarees : sans cet ordre, le clic sur une entree
			// refermerait aussi le menu avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("menu.panel")) {
				bool onItem = false;
				for (int32 i = 0; i < m.count && !onItem; ++i) {
					snprintf(keys, sizeof(keys), "menu.item.%d", i);
					onItem = hit.IsHovered(keys);
				}
				bool onBar = false;
				for (int32 i = 0; i < nMenus && !onBar; ++i) {
					snprintf(keys, sizeof(keys), "menu.%d", i);
					onBar = hit.IsHovered(keys);
				}
				if (!onItem && !onBar)
					st.openMenu = -1;
			}
		}

		// â”€â”€ LISTE DES MODIFICATEURS, A DEUX NIVEAUX â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Peinte APRES tout le reste : elle doit recouvrir les panneaux, et le
		// registre donne la priorite a la derniere zone declaree.
		//
		// LA CATEGORIE S'OUVRE AU SURVOL et non au clic. Un clic serait un geste de
		// plus pour atteindre une entree qui, elle, en demande deja un -- et rien ne
		// justifie de valider le fait de Â« regarder Â» une categorie.
		inline void PaintModifierMenu(NkModelerPainter &p, NkModelerState &st, NkHitRegistry &hit,
									  NkWidgetState &ws, float32 W, float32 H) {
			if (!ws.ComboOpen("tb.mod"))
				return;
			int32 nc = 0;
			const NkModCategory *cats = NkModifierCategories(nc);
			const NkRect &a = st.modAnchor;
			const float32 itemH = S(24.f);
			const float32 catW = S(150.f);
			// LE MENU RESTE DANS LA FENETRE. Il etait pose sous son ancre sans aucune
			// verification : avec neuf categories et un sous-menu de huit entrees, le
			// bas du panneau sortait par le bas de l'ecran et les dernieres entrees
			// etaient inatteignables. On remonte le panneau plutot que de le faire
			// defiler -- un menu qui defile demande deux gestes la ou un seul suffit.
			float32 boxY = a.y + a.h + 2.f;
			const float32 boxH = itemH * (float32)nc + S(6.f);
			if (boxY + boxH > H - S(4.f))
				boxY = H - S(4.f) - boxH;
			if (boxY < S(4.f))
				boxY = S(4.f);
			float32 boxX = a.x;
			if (boxX + catW > W - S(4.f))
				boxX = W - S(4.f) - catW;
			if (boxX < S(4.f))
				boxX = S(4.f);
			const NkRect box{boxX, boxY, catW, boxH};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("mod.panel", box);

			char key[32];
			int32 flatBase = 0;
			for (int32 c = 0; c < nc; ++c) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)c * itemH, box.w - 4.f, itemH};
				snprintf(key, sizeof(key), "mod.cat.%d", c);
				const bool over = hit.Add(key, ir);
				if (over)
					st.modOpenCat = c; // survol = ouverture, sans clic
				const bool active = (st.modOpenCat == c);
				if (active)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, cats[c].icon,
						active ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				p.TextV(ir.x + S(29.f), ir.y, itemH, cats[c].name,
						active ? NkRole::TextOnAccent : NkRole::Text);
				p.IconV(ir.x + ir.w - S(18.f), ir.y, itemH, NkIcon::ChevronRight,
						active ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);

				if (active) {
					// Le sous-menu s'ouvre A DROITE, aligne sur SA categorie : aligne sur
					// le haut du panneau, il faudrait chercher a quelle categorie il se
					// rapporte des qu'on en survole une du bas.
					const float32 subW = S(170.f);
					const float32 subH = itemH * (float32)cats[c].count + S(6.f);
					// Le sous-menu subit la meme contrainte, et bascule A GAUCHE du
					// panneau s'il n'y a plus la place a droite.
					float32 subX = box.x + box.w + 3.f;
					if (subX + subW > W - S(4.f))
						subX = box.x - subW - 3.f;
					if (subX < S(4.f))
						subX = S(4.f);
					float32 subY = ir.y - S(3.f);
					if (subY + subH > H - S(4.f))
						subY = H - S(4.f) - subH;
					if (subY < S(4.f))
						subY = S(4.f);
					const NkRect sub{subX, subY, subW, subH};
					p.Fill({sub.x + 2.f, sub.y + 2.f, sub.w, sub.h}, NkRole::WindowBg, 4.f);
					p.Outline(sub, NkRole::Border, NkRole::PanelHeader, 4.f);
					hit.Add("mod.sub", sub);
					for (int32 i = 0; i < cats[c].count; ++i) {
						const NkRect er{sub.x + 2.f, sub.y + S(3.f) + (float32)i * itemH, sub.w - 4.f,
										itemH};
						snprintf(key, sizeof(key), "mod.item.%d.%d", c, i);
						const bool o2 = hit.Add(key, er);
						const bool cur = (st.modKind == flatBase + i);
						if (o2)
							p.Fill(er, NkRole::AccentUi, 3.f);
						p.IconV(er.x + S(10.f), er.y, itemH, cats[c].items[i].icon,
								o2 ? NkRole::TextOnAccent : NkRole::Text, 13.f);
						p.TextV(er.x + S(29.f), er.y, itemH, cats[c].items[i].label,
								o2 ? NkRole::TextOnAccent : NkRole::Text);
						// AUCUNE COCHE ICI. Une coche dit Â« ceci est l'option retenue Â» ;
						// or ce menu ne retient rien, il AJOUTE. Le modificateur ajoute
						// se lit dans le panneau Details, la ou il vit.
						(void)cur;
						if (hit.Clicked(key)) {
							st.modKind = flatBase + i;
							// Chaque entree porte SON type moteur : le menu ne calcule
							// aucun indice, donc rien ne diverge quand une categorie
							// gagne une entree.
							nk3d::Viewport3DAddModifier(cats[c].items[i].type);
							NkMarkDirty(st);
							ws.CloseCombo();
						}
					}
				}
				flatBase += cats[c].count;
			}

			// Un clic HORS des deux panneaux referme. Teste apres toutes les zones :
			// sinon un clic sur une entree refermerait avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("mod.panel") && !hit.IsHovered("mod.sub")
				&& !hit.IsHovered("tb.mod") && !hit.IsHovered("props.modadd")) {
				bool inside = false;
				for (int32 c = 0; c < nc && !inside; ++c) {
					snprintf(key, sizeof(key), "mod.cat.%d", c);
					inside = hit.IsHovered(key);
					for (int32 i = 0; i < cats[c].count && !inside; ++i) {
						snprintf(key, sizeof(key), "mod.item.%d.%d", c, i);
						inside = hit.IsHovered(key);
					}
				}
				if (!inside)
					ws.CloseCombo();
			}
		}

		// â”€â”€ MENU Â« AJOUTER Â», DEUX NIVEAUX â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Meme mecanique que le menu des modificateurs : survol d'une categorie =
		// ouverture de son sous-menu, clic sur une entree = creation + fermeture.
		inline void PaintAddObjectMenu(NkModelerPainter &p, NkModelerState &st, NkHitRegistry &hit,
									   NkWidgetState &ws, float32 W, float32 H) {
			if (!ws.ComboOpen("tb.addmenu"))
				return;
			int32 nc = 0;
			const NkAddCategory *cats = NkAddCategories(nc);
			const NkRect &a = st.addAnchor;
			const float32 itemH = S(24.f);
			const float32 catW = S(140.f);
			float32 boxY = a.y + a.h + 2.f;
			const float32 boxH = itemH * (float32)nc + S(6.f);
			if (boxY + boxH > H - S(4.f))
				boxY = H - S(4.f) - boxH;
			float32 boxX = a.x;
			if (boxX + catW > W - S(4.f))
				boxX = W - S(4.f) - catW;
			const NkRect box{boxX, boxY, catW, boxH};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("addm.panel", box);
			// Ce menu recouvre souvent la hierarchie : sans declarer son emprise,
			// le clic sur une entree atteignait AUSSI la ligne du dessous (c'est
			// ainsi que « Ajouter un enfant » verrouillait le parent).
			st.UiBlockAdd(box);

			char key[32];
			for (int32 c = 0; c < nc; ++c) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)c * itemH, box.w - 4.f, itemH};
				snprintf(key, sizeof(key), "addm.cat.%d", c);
				const bool over = hit.Add(key, ir);
				if (over)
					st.addOpenCat = c; // survol = ouverture, sans clic
				const bool active = (st.addOpenCat == c);
				if (active)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, cats[c].icon,
						active ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				p.TextV(ir.x + S(29.f), ir.y, itemH, cats[c].name,
						active ? NkRole::TextOnAccent : NkRole::Text);
				p.IconV(ir.x + ir.w - S(18.f), ir.y, itemH, NkIcon::ChevronRight,
						active ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);

				if (active) {
					const float32 subW = S(150.f);
					const float32 subH = itemH * (float32)cats[c].count + S(6.f);
					float32 subX = box.x + box.w + 3.f;
					if (subX + subW > W - S(4.f))
						subX = box.x - subW - 3.f;
					float32 subY = ir.y - S(3.f);
					if (subY + subH > H - S(4.f))
						subY = H - S(4.f) - subH;
					const NkRect sub{subX, subY, subW, subH};
					p.Fill({sub.x + 2.f, sub.y + 2.f, sub.w, sub.h}, NkRole::WindowBg, 4.f);
					p.Outline(sub, NkRole::Border, NkRole::PanelHeader, 4.f);
					hit.Add("addm.sub", sub);
					st.UiBlockAdd(sub);
					for (int32 i = 0; i < cats[c].count; ++i) {
						const NkRect er{sub.x + 2.f, sub.y + S(3.f) + (float32)i * itemH,
										sub.w - 4.f, itemH};
						snprintf(key, sizeof(key), "addm.item.%d.%d", c, i);
						const bool o2 = hit.Add(key, er);
						if (o2)
							p.Fill(er, NkRole::AccentUi, 3.f);
						p.IconV(er.x + S(10.f), er.y, itemH, cats[c].items[i].icon,
								o2 ? NkRole::TextOnAccent : NkRole::Text, 13.f);
						p.TextV(er.x + S(29.f), er.y, itemH, cats[c].items[i].label,
								o2 ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(key)) {
							// CREATION dans l'HOTE : noeud utilisateur nomme d'apres
							// l'entree (« Cube.001 »), selectionne immediatement.
							const int32 nn = demo::Demo3DHostAddNode(cats[c].items[i].type,
																	 cats[c].items[i].prim);
							if (nn >= 0) {
								NkHierComposeName(st, cats[c].items[i].label, nn);
								const int32 t8 = cats[c].items[i].type;
								const int32 root8 = NkModelRootOf(st);
								if (root8 >= 0) {
									// DANS UN MODEL, la regle du model PRIME sur le
									// parent demande : un maillage devient un MESH du
									// model, frere des autres sous sa racine. Lumieres,
									// cameras et empties restent des aides COSMETIQUES.
									//
									// Cette regle etait auparavant dans la branche
									// « sinon » : passer par « Ajouter un enfant »
									// posait donc le parent SANS marquer le maillage --
									// il ne revenait alors pas dans la scene et
									// n'entrait pas dans le lisere du model (Rihen).
									if (t8 != 4 && t8 != 5) {
										demo::Demo3DHostSetNodeParent(nn, root8);
										demo::Demo3DHostSetNodeIsMesh(nn, true);
									}
								} else if (st.addParentNode >= 0) {
									// hors model : clic droit sur un OBJET, il devient
									// le parent du nouveau (Rihen)
									demo::Demo3DHostSetNodeParent(nn, st.addParentNode);
								}
								demo::Demo3DHostSelectEmptyNode(nn);
								// pour TOUT element du menu, sans distinction (Rihen)
								st.addAdjustNode = nn;
							}
							NkMarkDirty(st);
							ws.CloseCombo();
						}
					}
				}
			}

			// Un clic HORS des deux panneaux referme.
			if (hit.AnyClick() && !hit.IsHovered("addm.panel") && !hit.IsHovered("addm.sub") &&
				!hit.IsHovered("tb.addmenu")) {
				bool inside = false;
				for (int32 c = 0; c < nc && !inside; ++c) {
					snprintf(key, sizeof(key), "addm.cat.%d", c);
					inside = hit.IsHovered(key);
					for (int32 i = 0; i < cats[c].count && !inside; ++i) {
						snprintf(key, sizeof(key), "addm.item.%d.%d", c, i);
						inside = hit.IsHovered(key);
					}
				}
				if (!inside)
					ws.CloseCombo();
			}
		}

		// â”€â”€ SEPARATEURS GLISSABLES â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Ils modifient des FRACTIONS et non des pixels : la disposition se retrouve
		// identique a la reouverture quelle que soit la taille de fenetre.
		//
		// La zone sensible est PLUS LARGE que le trait dessine (6 px contre 1) :
		// viser un trait d'un pixel est un exercice d'adresse, pas une interaction.
		// C'est ce que font tous les gestionnaires de fenetres.
		inline void PaintSplitters(NkModelerPainter &p, const NkLayout &lay, float32 W, float32 H,
								   NkModelerState &st, NkHitRegistry &hit) {
			const float32 grab = S(6.f);
			struct Sp {
					const char *key;
					NkRect zone;
					bool vertical; ///< true = trait vertical, on glisse en X
					float32 *frac;
					float32 span; ///< dimension de reference pour convertir px -> fraction
					float32 sign; ///< sens : +1 si la fraction croit avec la position
			};
			const Sp sps[4] = {
				{"split.left", {lay.left.x + lay.left.w - grab * 0.5f, lay.left.y, grab, lay.left.h},
				 true, &st.leftFrac, W, 1.f},
				{"split.right", {lay.right.x - grab * 0.5f, lay.right.y, grab, lay.right.h}, true,
				 &st.rightFrac, W, -1.f},
				{"split.browser", {0.f, lay.browser.y - grab * 0.5f, W, grab}, false, &st.browserFrac, H,
				 -1.f},
				{"split.props",
				 {lay.propsR.x, lay.propsR.y + lay.propsR.h - grab * 0.5f, lay.propsR.w, grab}, false,
				 &st.propsFrac, lay.right.h, 1.f},
			};

			// Un separateur borde un panneau : si le panneau est masque, il n'y a rien
			// a redimensionner. Le laisser actif donnerait un trait invisible qui
			// change le curseur et modifie une fraction qu'on ne voit pas.
			// Le splitter vue|proprietes MEURT quand le panneau est replie sur
			// ses pastilles (sa zone restait dans l'espace rendu a la vue --
			// le « cote gauche fantome » constate par Rihen). Et le separateur
			// interne Proprietes/Details est OBSOLETE depuis le panneau unique :
			// c'etait la « tete » invisible des sous-blocs.
			const bool alive[4] = {st.showLeft, st.showRight && st.AnyPropOpen(),
								   st.showBrowser, false};
			for (int32 i = 0; i < 4; ++i) {
				if (!alive[i])
					continue;
				const bool over = hit.Add(sps[i].key, sps[i].zone);
				const bool active = (st.dragSplitter == i);
				if (over || active) {
					hit.WantCursor(sps[i].vertical ? NkCursorWant::ResizeWE : NkCursorWant::ResizeNS);
					// Le trait s'ECLAIRE au survol : sans retour visuel, on ne sait pas
					// qu'on a attrape le bon endroit avant d'avoir deja tire.
					p.Fill(sps[i].zone, active ? NkRole::AccentUi : NkRole::Border);
				}
				if (hit.Clicked(sps[i].key)) {
					st.dragSplitter = i;
					st.dragStart = sps[i].vertical ? hit.Mouse().x : hit.Mouse().y;
					st.dragStartFrac = *sps[i].frac;
				}
			}

			// Le glissement se poursuit MEME SI la souris quitte la zone : c'est le
			// bouton enfonce qui commande, pas la position. Sans cela, un glissement
			// rapide lacherait le separateur en pleine course.
			if (st.dragSplitter >= 0) {
				if (!hit.MouseDown()) {
					st.dragSplitter = -1;
				} else {
					const Sp &sp = sps[st.dragSplitter];
					const float32 now = sp.vertical ? hit.Mouse().x : hit.Mouse().y;
					float32 f = st.dragStartFrac + sp.sign * (now - st.dragStart) / sp.span;
					if (f < 0.08f)
						f = 0.08f;
					if (f > 0.60f)
						f = 0.60f;
					*sp.frac = f;
				}
			}
		}

		// â”€â”€ CONFIRMATION DE FERMETURE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// N'apparait QUE si le document a des modifications non enregistrees.
		// Demander confirmation pour un document propre serait une friction inutile,
		// et l'utilisateur finirait par valider sans lire -- ce qui rend la question
		// dangereuse le jour ou elle compte vraiment.
		// ── DEMANDE DE FERMETURE : UNE SEULE POLITIQUE ─────────────────────────────
		// Tous les chemins (croix dessinee, menu Quitter, croix de l'OS) passent
		// ici. Une prise ou un encodage en cours PRIME sur le document modifie :
		// perdre une video de plusieurs minutes est pire que perdre un clic.
		inline void NkRequestClose(NkModelerState &st) {
			const bool busy = demo::Demo3DHostRecActive() || demo::Demo3DHostRecTutoActive() ||
							  demo::Demo3DHostRecEncoding() || demo::Demo3DHostRecTutoEncoding();
			if (busy)
				st.askCloseRec = true;
			// AUCUN PROJET OUVERT (ecran d'accueil) : il n'y a rien a perdre, donc
			// rien a demander. Poser la question « modifications non enregistrees »
			// devant un ecran de demarrage serait incomprehensible.
			// LA CONFIRMATION EST TOUJOURS POSEE quand un projet est ouvert (Rihen) :
			// fermer un modeleur d'un clic mal place est un accident cher, meme
			// quand tout est enregistre -- il faut rouvrir le projet, refaire sa
			// disposition, retrouver sa vue. Seul l'ECRAN D'ACCUEIL sort sans
			// demander : il n'y a alors ni travail ni disposition a perdre, et la
			// question y serait incomprehensible.
			else if (!st.welcome)
				st.askClose = true;
			else
				st.running = false;
		}

		// ── FERMER PENDANT UNE PRISE OU UN ENCODAGE ────────────────────────────────
		// Trois issues (Rihen) : abandonner, finir puis fermer, ou continuer en
		// arriere-plan -- la fenetre se cache, le processus vit jusqu'a la fin.
		inline void PaintCloseRecDialog(NkModelerPainter &p, float32 W, float32 H,
										NkModelerState &st, NkHitRegistry &hit) {
			if (!st.askCloseRec)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlgr.veil", {0.f, 0.f, W, H});

			const bool recording = demo::Demo3DHostRecActive() || demo::Demo3DHostRecTutoActive();
			// QUATRE actions ne tiennent pas sur une rangee : empilees pleine
			// largeur, elles ne peuvent PAS deborder du cadre, et quatre choix
			// se lisent mieux en liste qu'en file (constate par Rihen : les
			// boutons sortaient de la boite).
			const float32 bw = S(470.f), bh = S(238.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlgr.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f),
					recording ? "Un enregistrement est en cours" : "Un encodage est en cours");
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					recording ? "Quitter maintenant perdrait la prise."
							  : "Quitter maintenant laisserait la video inachevee.",
					NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'action la plus SURE porte l'accent et vient en PREMIER : finir
			// le travail. Abandonner reste atteignable mais ne se propose pas.
			const B kBtns[4] = {{"dlgr.finish", "Terminer puis quitter", true},
								{"dlgr.daemon", "Continuer en arriere-plan", false},
								{"dlgr.drop", "Abandonner et quitter", false},
								{"dlgr.cancel", "Annuler", false}};
			float32 by = box.y + S(76.f);
			for (int32 i = 0; i < 4; ++i) {
				const NkRect br{box.x + S(20.f), by, bw - S(40.f), S(28.f)};
				const bool over = hit.Add(kBtns[i].key, br);
				if (kBtns[i].primary)
					p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 4.f);
				const float32 lw = p.TextW(kBtns[i].label);
				p.TextV(br.x + (br.w - lw) * 0.5f, br.y, br.h, kBtns[i].label,
						kBtns[i].primary ? NkRole::TextOnAccent : NkRole::Text);
				by += S(36.f);
			}

			if (hit.Clicked("dlgr.cancel"))
				st.askCloseRec = false;
			if (hit.Clicked("dlgr.drop")) {
				// Les prises s'abandonnent proprement ; un encodage en cours est
				// simplement laisse : il n'efface son dossier QOI qu'une fois
				// complet, donc rien n'est perdu sur disque.
				if (demo::Demo3DHostRecActive())
					demo::Demo3DHostRecStop(false);
				if (demo::Demo3DHostRecTutoActive())
					demo::Demo3DHostRecTutoStop(false);
				st.running = false;
			}
			const bool finish = hit.Clicked("dlgr.finish");
			const bool daemon = hit.Clicked("dlgr.daemon");
			if (finish || daemon) {
				// On GARDE les prises : leur arret declenche l'encodage, et la
				// boucle fermera l'application quand tout sera fini.
				if (demo::Demo3DHostRecActive())
					demo::Demo3DHostRecStop(true);
				if (demo::Demo3DHostRecTutoActive())
					demo::Demo3DHostRecTutoStop(true);
				st.closeAfterEncode = daemon ? 2 : 1;
				if (daemon)
					st.wantHideWindow = true;
				st.askCloseRec = false;
			}
		}

		// ── NOTIFICATION DE FIN D'ENCODAGE ─────────────────────────────────────────
		// Toujours affichee (Rihen) : la passe finale peut durer des minutes, sa
		// fin merite un signal franc -- le meme langage visuel que la fermeture.
		inline void PaintEncodeDoneDialog(NkModelerPainter &p, float32 W, float32 H,
										  NkModelerState &st, NkHitRegistry &hit) {
			if (!st.encodeDone)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlge.veil", {0.f, 0.f, W, H});
			const float32 bw = S(470.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlge.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Video terminee");
			p.Clip({box.x + S(20.f), box.y + S(44.f), bw - S(40.f), S(22.f)});
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f), st.encodeDonePath,
					NkRole::TextMuted);
			p.Unclip();
			const char *ok = "D'accord";
			const float32 w = p.TextW(ok) + S(28.f);
			const NkRect br{box.x + box.w - w - S(14.f), box.y + bh - S(44.f), w, S(28.f)};
			const bool over = hit.Add("dlge.ok", br);
			p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
			p.TextV(br.x + (w - p.TextW(ok)) * 0.5f, br.y, br.h, ok, NkRole::TextOnAccent);
			if (hit.Clicked("dlge.ok"))
				st.encodeDone = false;
		}

		inline void PaintCloseDialog(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
									 NkHitRegistry &hit) {
			// La croix de l'OS arrive ICI : meme politique que la croix dessinee.
			if (st.wantClose) {
				st.wantClose = false;
				NkRequestClose(st);
			}
			// (La boite « Fermer la scene ? » a DISPARU, et c'est le but du
			// chantier : fermer un onglet ne ferme qu'une VUE, le document reste
			// dans le projet et sa carte dans le navigateur. Demander confirmation
			// laisserait croire qu'il y a quelque chose a perdre.)

			if (!st.askClose)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlg.veil", {0.f, 0.f, W, H});

			const float32 bw = S(420.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlg.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Quitter NK3DModeler ?");
			// LA BOITE S'OUVRE TOUJOURS (Rihen), mais elle ne raconte pas la meme
			// chose selon ce qu'il reste a perdre : annoncer « modifications non
			// enregistrees » alors que tout est ecrit ferait douter d'un travail
			// pourtant deja sur le disque.
			const bool unsaved = st.dirty;
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					unsaved ? "Des modifications ne sont pas enregistrees."
							: "Tout est enregistre.",
					NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'ordre compte : l'action la plus SURE est a droite, sous la main, et
			// c'est elle qui porte l'accent. Â« Quitter sans enregistrer Â» reste
			// atteignable mais ne se propose pas.
			// RIEN A ENREGISTRER : deux boutons seulement. Proposer « Enregistrer et
			// quitter » quand il n'y a rien a ecrire ferait croire qu'il reste du
			// travail en attente, et « Quitter SANS enregistrer » serait alarmant
			// pour une sortie parfaitement propre.
			const B kBtns[3] = {
				{"dlg.cancel", "Annuler", false},
				{"dlg.discard", unsaved ? "Quitter sans enregistrer" : "Quitter", !unsaved},
				{"dlg.save", "Enregistrer et quitter", true}};
			const int32 nBtn = unsaved ? 3 : 2;
			float32 bx = box.x + box.w - S(14.f);
			for (int32 i = nBtn - 1; i >= 0; --i) {
				const float32 w = p.TextW(kBtns[i].label) + S(24.f);
				const NkRect br{bx - w, box.y + bh - S(44.f), w, S(28.f)};
				const bool over = hit.Add(kBtns[i].key, br);
				if (kBtns[i].primary)
					p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 4.f);
				const float32 lw = p.TextW(kBtns[i].label);
				p.TextV(br.x + (w - lw) * 0.5f, br.y, br.h, kBtns[i].label,
						kBtns[i].primary ? NkRole::TextOnAccent : NkRole::Text);
				bx -= w + S(10.f);
			}

			if (hit.Clicked("dlg.cancel"))
				st.askClose = false;
			if (hit.Clicked("dlg.discard"))
				st.running = false;
			if (hit.Clicked("dlg.save")) {
				// L'enregistrement est demande en DIFFERE (il peut ouvrir
				// « Enregistrer sous... » si le projet n'a pas encore de chemin) et
				// la fermeture attend son resultat. Fermer tout de suite fermerait
				// AVANT d'avoir ecrit.
				//
				// La scene EST desormais enregistree avec le projet -- sauf la
				// geometrie editee sommet par sommet et les modificateurs, que
				// l'ecran d'accueil enumere.
				st.askClose = false;
				st.projPending = 3;
				st.quitAfterSave = true;
			}
		}

		// â”€â”€ BARRE D'ETAT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintStatus(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
								NkModelerState &st) {
			char stats[128];
			if (st.mode == NkMode::Object)
				snprintf(stats, sizeof(stats), "Objets 6 - selectionne : %s - 60 ips",
						 st.selectedObject == 1 ? "Cube" : "-");
			else
				snprintf(stats, sizeof(stats), "Sommets 8 - Aretes 12 - Faces 6 - %s - 60 ips",
						 NkModeName(st.mode));
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y, r.w);
			float32 x = r.x + kPad;
			struct B {
					NkIcon ic;
					const char *label;
			};
			static const B kBtns[] = {{NkIcon::Drawer, "Tiroir"}, {NkIcon::Journal, "Journal"}};
			for (int32 i = 0; i < 2; ++i) {
				p.IconV(x, r.y, r.h, kBtns[i].ic, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, r.h, kBtns[i].label);
				x += 18.f + p.TextW(kBtns[i].label) + 16.f;
			}
			// TUTORIEL AU FOOTER (regle de Rihen) : il enregistre TOUTE
			// l'application, sa place est donc dans la barre de l'application --
			// la capture de la vue 3D, elle, reste dans la vue. Bouton-menu :
			// Photo prend une image, Video ouvre une prise que la barre
			// d'enregistrement ci-dessous pilote. Le menu s'ouvre VERS LE HAUT.
			{
				const float32 tw = p.TextW("Tutoriel");
				const NkRect tb{x, r.y + 4.f, 18.f + tw + 10.f, r.h - 8.f};
				const bool ovT = hit.Add("status.tuto", tb);
				if (st.tutoMenuOpen)
					p.Fill(tb, NkRole::AccentUi, 3.f);
				else
					p.Outline(tb, ovT ? NkRole::AccentUi : NkRole::Border,
							  NkRole::PanelHeader, 3.f);
				p.IconV(tb.x + 4.f, r.y, r.h, NkIcon::ImageRef,
						st.tutoMenuOpen ? NkRole::TextOnAccent : NkRole::Text, 12.f);
				p.TextV(tb.x + 20.f, r.y, r.h, "Tutoriel",
						st.tutoMenuOpen ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked("status.tuto"))
					st.tutoMenuOpen = !st.tutoMenuOpen;
				if (st.tutoMenuOpen) {
					// « Video » n'est plus un stub : elle enregistre TOUTE la
					// fenetre, en parallele d'un eventuel enregistrement de la
					// vue -- deux points de vue d'une meme session.
					const bool tRec = demo::Demo3DHostRecTutoActive();
					static const char *const kTuKeys[2] = {"status.tuto.ph", "status.tuto.vid"};
					const char *kTuM[2] = {"Photo", tRec ? "Arreter la video" : "Video"};
					const float32 mh = S(24.f);
					for (int32 m = 0; m < 2; ++m) {
						const NkRect mr{tb.x, r.y - (float32)(m + 1) * (mh + 2.f), S(140.f), mh};
						const bool ovM = hit.Add(kTuKeys[m], mr);
						p.Fill(mr, ovM ? NkRole::AccentUi : NkRole::PanelHeader, 4.f);
						p.TextV(mr.x + 8.f, mr.y, mh, kTuM[m],
								ovM ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(kTuKeys[m])) {
							if (m == 0)
								st.capturePending = 2; // TOUTE la fenetre, en image
							else
								// La TAILLE de la fenetre n'est connue que de la
								// boucle principale : on demande, elle execute --
								// meme patron que capturePending.
								st.tutoRecPending = tRec ? 2 : 1;
							st.tutoMenuOpen = false;
						}
					}
					// un clic ailleurs referme
					if (hit.AnyClick() && !hit.IsHovered("status.tuto") &&
						!hit.IsHovered(kTuKeys[0]) && !hit.IsHovered(kTuKeys[1]))
						st.tutoMenuOpen = false;
				}
				x += tb.w + 12.f;
			}
			p.VLine(x, r.y + 6.f, r.h - 12.f);
			x += 10.f;
			p.Fill({x, r.y + (r.h - 20.f) * 0.5f, 240.f, 20.f}, NkRole::InputBg, 2.f);
			p.IconV(x + 6.f, r.y, r.h, NkIcon::Terminal, NkRole::Text, 12.f);
			p.TextV(x + 24.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);

			// BARRE D'ENREGISTREMENT -- visible SEULEMENT pendant une prise. Une
			// prise en cours est un etat qu'on oublie : elle doit se signaler
			// d'elle-meme, dire DEPUIS QUAND elle tourne, et offrir les trois
			// seules decisions possibles -- suspendre, garder, jeter. Les deux
			// prises (la vue seule / toute la fenetre) sont independantes :
			// deux barres cote a cote, pas un etat partage.
			float32 rx = r.x + r.w - w - kPad - 14.f;
			{
				bool vOn = false;
				int32 fps = 24, vFirst = 0, vLast = 0, vCodec = 0;
				demo::Demo3DHostOutVideo(&vOn, &fps, &vFirst, &vLast, &vCodec);
				if (fps <= 0)
					fps = 24;
				struct Rec {
						const char *name;
						bool active, paused;
						int32 frames, dropped;
						bool enc; ///< passe finale du MP4 : la prise est finie
						int32 encDone, encTotal;
				};
				Rec kRecs[2] = {
					{"Vue", demo::Demo3DHostRecActive(), demo::Demo3DHostRecPaused(),
					 demo::Demo3DHostRecFrames(), demo::Demo3DHostRecDropped(),
					 demo::Demo3DHostRecEncoding(), 0, 0},
					{"Tutoriel", demo::Demo3DHostRecTutoActive(),
					 demo::Demo3DHostRecTutoPaused(), demo::Demo3DHostRecTutoFrames(),
					 demo::Demo3DHostRecTutoDropped(), demo::Demo3DHostRecTutoEncoding(), 0, 0},
				};
				demo::Demo3DHostRecEncodeProgress(&kRecs[0].encDone, &kRecs[0].encTotal);
				demo::Demo3DHostRecTutoEncodeProgress(&kRecs[1].encDone, &kRecs[1].encTotal);
				// Cles DISTINCTES par prise : deux barres partageant une cle se
				// voleraient les clics.
				static const char *const kRecKeys[2][3] = {
					{"status.rec.v.pause", "status.rec.v.stop", "status.rec.v.drop"},
					{"status.rec.t.pause", "status.rec.t.stop", "status.rec.t.drop"},
				};
				for (int32 k = 1; k >= 0; --k) { // de droite a gauche
					const Rec &R = kRecs[k];
					if (!R.active && !R.enc)
						continue;
					char lbl[96];
					const int32 sec = R.frames / fps;
					// PENDANT L'ENCODAGE la prise est finie : plus de temps qui
					// court, mais un travail qui avance. Le taire ferait passer
					// plusieurs minutes de calcul pour un blocage.
					if (R.enc)
						snprintf(lbl, sizeof(lbl), "%s  encodage %d/%d", R.name, R.encDone,
								 R.encTotal);
					// Les images SAUTEES ne se taisent pas : un enregistrement
					// troue doit se voir pendant qu'on peut encore recommencer.
					else if (R.dropped > 0)
						snprintf(lbl, sizeof(lbl), "%s  %02d:%02d  (%d sautees)", R.name,
								 sec / 60, sec % 60, R.dropped);
					else
						snprintf(lbl, sizeof(lbl), "%s  %02d:%02d", R.name, sec / 60,
								 sec % 60);
					// TROIS ICONES, pas trois libelles (Rihen) : le transport a un
					// dessin universel -- deux barres, un carre, une corbeille se
					// lisent sans mot, et la barre reste etroite dans le pied de
					// page. L'info-bulle porte la phrase complete.
					const NkIcon kAct[3] = {R.paused ? NkIcon::MediaPlay : NkIcon::MediaPause,
											NkIcon::MediaStop, NkIcon::Trash};
					// PENDANT L'ENCODAGE, AUCUN BOUTON : suspendre ou abandonner
					// une passe qui ne fait que relire des images deja prises
					// n'a pas de sens -- un bouton sans effet vaut moins que pas
					// de bouton.
					const int32 nAct = R.enc ? 0 : 3;
					const float32 abw = r.h - 8.f - 4.f; // boutons CARRES
					float32 total =
						12.f + p.TextW(lbl) + 16.f + (float32)nAct * (abw + 4.f);
					const NkRect bar{rx - total, r.y + 4.f, total, r.h - 8.f};
					p.Fill(bar, NkRole::InputBg, 3.f);
					// Pastille : pleine quand ca tourne, grise en pause --
					// l'etat se lit sans avoir a lire le bouton.
					const float32 dr = 7.f;
					const NkRect dot{bar.x + 6.f, bar.y + (bar.h - dr) * 0.5f, dr, dr};
					// La pastille dit L'ETAT : rouge on filme, gris suspendu,
					// ambre l'encodage travaille. Rester rouge apres l'arret
					// ferait croire que la prise court encore.
					p.Fill(dot,
						   R.enc ? NkColor{230, 160, 40, 255}
								 : (R.paused ? NkColor{140, 140, 140, 255}
											 : NkColor{231, 76, 60, 255}),
						   dr * 0.5f);
					float32 bx = bar.x + 6.f + dr + 6.f;
					p.TextV(bx, r.y, r.h, lbl, R.paused ? NkRole::TextMuted : NkRole::Text);
					bx += p.TextW(lbl) + 10.f;
					for (int32 a = 0; a < nAct; ++a) {
						const NkRect ab{bx, bar.y + 2.f, abw, bar.h - 4.f};
						const bool ov = hit.Add(kRecKeys[k][a], ab);
						// ABANDON efface la prise : il porte le rouge, pour qu'on
						// ne le clique pas en croyant arreter.
						if (a == 2)
							p.Fill(ab, ov ? NkColor{231, 76, 60, 255} : NkColor{231, 76, 60, 90},
								   3.f);
						else
							p.Fill(ab, ov ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
						p.IconV(ab.x + (ab.w - 12.f) * 0.5f, ab.y, ab.h, kAct[a],
								(ov || a == 2) ? NkRole::TextOnAccent : NkRole::Text, 12.f);
						if (ov)
							hit.WantCursor(NkCursorWant::Hand);
						if (hit.Clicked(kRecKeys[k][a])) {
							if (k == 0) {
								if (a == 0)
									demo::Demo3DHostRecPause(!R.paused);
								else
									demo::Demo3DHostRecStop(a == 1);
							} else {
								if (a == 0)
									demo::Demo3DHostRecTutoPause(!R.paused);
								else
									demo::Demo3DHostRecTutoStop(a == 1);
							}
						}
						bx += abw + 4.f;
					}
					rx = bar.x - 10.f;
				}
			}
		}

	} // namespace nk3d
} // namespace nkentseu
