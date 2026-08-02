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
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NKEditorKit/NkShortcutTable.h"

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
			};
			n = 6;
			return k;
		}
		inline const NkIcon *NkOverlayIcons() {
			static const NkIcon k[] = {NkIcon::SnapGrid, NkIcon::Dot,	  NkIcon::Ruler,
									   NkIcon::Gizmo,	 NkIcon::Square, NkIcon::Journal};
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
			// LES TROIS ORIENTATIONS DU GIZMO de la demo (sa touche virgule) :
			// monde, local, normale. Â« Vue Â» n'existe pas dans le moteur -- la
			// proposer aurait fait un choix sans effet.
			static const char *const k[] = {"Monde", "Local", "Normale"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkOrientIcons() {
			static const NkIcon k[] = {NkIcon::Globe, NkIcon::Mesh, NkIcon::Ruler};
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
				{"Maillage", NkIcon::Cube3D, kMesh, 9},
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
			if (hit.Clicked("win.close")) {
				if (st.dirty)
					st.askClose = true;
				else
					st.running = false;
			}

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
		inline void NkStoreSceneCam(NkModelerState &st, int32 tab) {
			if (tab < 0 || tab >= 8)
				return;
			float32 *cp = st.sceneCamPose[tab];
			bool ortho = false;
			demo::Demo3DHostGetCameraPose(cp, &cp[3], &cp[4], &cp[5], &ortho);
			st.sceneCamOrtho[tab] = ortho;
			st.sceneCamSet[tab] = true;
		}
		// ── ACTIVER UN ONGLET ───────────────────────────────────────────────────
		// Un onglet EDITEUR est une SCENE A PART ENTIERE (Rihen) : la vue se vide
		// et une MAQUETTE de l'asset nait a l'origine, editable avec les memes
		// outils. En quittant l'editeur, la maquette disparait.
		inline void NkActivateTab(NkModelerState &st, int32 tb, bool force = false) {
			if (tb < 0 || tb >= st.sceneCount || (tb == st.activeTab && !force))
				return;
			if (st.sceneTabKind[st.activeTab] == 0 && tb != st.activeTab)
				NkStoreSceneCam(st, st.activeTab); // la scene quittee garde sa vue
			// Quitter un onglet d'ISOLATION : le noeud isole rentre chez lui.
			if (st.sceneTabIsoNode[st.activeTab] > 0 && tb != st.activeTab)
				demo::Demo3DHostMoveTreeScene(
					st.sceneTabIsoNode[st.activeTab] - 1,
					(int32)st.sceneTabIsoHome[st.activeTab]);
			// CHAQUE SCENE A SES PROPRIETES : celles du quitte sont rangees.
			st.unitSystemTab[st.activeTab] = st.unitSystem;
			st.unitLengthTab[st.activeTab] = st.unitLength;
			st.unitScaleTab[st.activeTab] = st.unitScale;
			if (st.editPreviewNode > 0) {
				demo::Demo3DHostDeleteNode(st.editPreviewNode - 1, true);
				st.editPreviewNode = 0;
			}
			st.activeTab = tb;
			// BASCULE DE DOCUMENT : l'hote ne rend et ne liste plus que les
			// noeuds de CE document ; la selection ne traverse jamais.
			demo::Demo3DHostSetActiveScene((int32)st.sceneTabId[tb]);
			// L'hote doit savoir s'il sert un MODEL : la selection en vue 3D
			// n'y a pas la meme regle (mesh par mesh, contre model entier).
			demo::Demo3DHostSetDocIsModel(st.sceneTabKind[tb] == 7);
			demo::Demo3DHostDeselectAll();
			st.unitSystem = st.unitSystemTab[tb];
			st.unitLength = st.unitLengthTab[tb];
			st.unitScale = st.unitScaleTab[tb];
			if (st.unitScale < 0.001f)
				st.unitScale = 1.f; // onglet jamais visite : valeurs par defaut
			const uint8 tk = st.sceneTabKind[tb];
			if (tk == 0) {
				if (st.sceneCamSet[tb])
					demo::Demo3DHostSetCameraPose(st.sceneCamPose[tb],
												  st.sceneCamPose[tb][3],
												  st.sceneCamPose[tb][4],
												  st.sceneCamPose[tb][5],
												  st.sceneCamOrtho[tb]);
				else
					demo::Demo3DHostResetView();
				return; // l'appartenance filtre deja les objets de la scene
			}
			// EDITEUR : scene VIDE + maquette selon la nature de l'asset.
			demo::Demo3DHostResetView(); // document neuf : vue d'ouverture
			// ISOLATION : l'onglet edite le NOEUD LUI-MEME (pas une copie) --
			// deplace dans ce document, rendu a sa scene au retour.
			if (st.sceneTabIsoNode[tb] > 0) {
				const int32 iso = st.sceneTabIsoNode[tb] - 1;
				demo::Demo3DHostMoveTreeScene(iso, (int32)st.sceneTabId[tb]);
				// Le seul parent d'un model est le model : ses maillages
				// reviennent tous a plat sous lui (Rihen).
				demo::Demo3DHostFlattenModel(iso);
				NkModelFirstMesh(st, iso); // le model et sa matiere
				demo::Demo3DHostSelectEmptyNode(iso);
				st.editPreviewNode = 0;
				return;
			}
			const uint8 ek = (uint8)(tk - 1);
			const int32 ai = st.sceneTabAsset[tb] - 1;
			int32 pv = -1;
			if (ek == 6 && ai >= 0 && ai < st.browserCount &&
				st.browserSrcNode[ai] > 0)
				pv = demo::Demo3DHostDuplicateNode(st.browserSrcNode[ai] - 1);
			else if (ek == 6)
				pv = demo::Demo3DHostAddNode(2, 0); // mesh sans source : cube
			if (pv >= 0) {
				// La maquette nait a l'ORIGINE, droite, et selectionnee ; elle
				// garde sa propre echelle (dimensions reelles de l'asset).
				float32 pz[3] = {0.f, 0.f, 0.f}, rz[3] = {0.f, 0.f, 0.f};
				float32 sz[3] = {1.f, 1.f, 1.f}, gp[3], gr[3];
				demo::Demo3DHostEmptyTransform(pv, gp, gr, sz);
				demo::Demo3DHostSetEmptyTransform(pv, pz, rz, sz);
				demo::Demo3DHostSelectEmptyNode(pv);
			}
			st.editPreviewNode = pv + 1;
			if (tk == 7 && pv >= 0) {
				demo::Demo3DHostFlattenModel(pv); // maillages tous freres
				NkModelFirstMesh(st, pv);
			}
		}
		inline void PaintTabsI(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
							   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = S(10.f);
			const float32 h = r.h - 2.f;
			char key[32];
			for (int32 i = 0; i < st.sceneCount; ++i) {
				const float32 tw = p.TextW(st.sceneNames[i]) + S(44.f);
				const NkRect tr{x, r.y + 2.f, tw, h};
				snprintf(key, sizeof(key), "tab.%d", i);
				const bool over = hit.Add(key, tr);
				const bool on = (i == st.activeTab);
				p.Fill(tr, on ? NkRole::PanelHeader : (over ? NkRole::PanelBg : NkRole::InputBg), 3.f);
				if (st.sceneTabKind[i] != 0) {
					// Liseret de NATURE : distinguer d'un oeil les onglets EDITEUR
					// des scenes (memes couleurs que les cartes du navigateur).
					const uint8 k2 = (uint8)(st.sceneTabKind[i] - 1);
					const NkRole r2 = (k2 == 0)	  ? NkRole::TypeMesh
									  : (k2 == 4) ? NkRole::AccentUi
									  : (k2 == 6) ? NkRole::AxisX
									  : (k2 == 2) ? NkRole::TypeMat
												  : NkRole::TypeTex;
					p.Fill({tr.x, tr.y, tr.w, 2.f}, r2); // liseret en HAUT (Rihen)
				}
				snprintf(key, sizeof(key), "tab.name.%d", i);
				EditableText(p, hit, ws, in, key, {x + S(10.f), r.y, tw - S(32.f), r.h}, st.sceneNames[i],
							 on ? NkRole::Text : NkRole::TextMuted, st.sceneNames[i], 32u);
				// La CROIX n'apparait que s'il reste plus d'une scene : fermer la derniere
				// laisserait l'application sans document, etat qu'aucune autre partie de
				// l'interface ne sait representer.
				if (st.sceneCount > 1) {
					snprintf(key, sizeof(key), "tab.close.%d", i);
					const NkRect cr{x + tw - S(24.f), r.y + 2.f, S(20.f), h};
					HoverFill(p, cr, hit.Add(key, cr), 2.f);
					p.IconV(x + tw - S(20.f), r.y, r.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
					if (hit.Clicked(key)) {
						// Fermer un onglet d'ISOLATION -- ACTIF OU NON : le noeud
						// rentre chez lui AVANT le decalage des donnees. Ferme en
						// arriere-plan, son sous-arbre restait echoue dans un
						// document mort -- donc introuvable dans la scene.
						if (st.sceneTabIsoNode[i] > 0)
							demo::Demo3DHostMoveTreeScene(
								st.sceneTabIsoNode[i] - 1,
								(int32)st.sceneTabIsoHome[i]);
						for (int32 k = i; k + 1 < st.sceneCount; ++k) {
							NkWidgetState::Copy(st.sceneNames[k], st.sceneNames[k + 1], 31u);
							// TOUTES les donnees d'onglet suivent le nom.
							st.sceneBlank[k] = st.sceneBlank[k + 1];
							st.sceneTabKind[k] = st.sceneTabKind[k + 1];
							st.sceneTabAsset[k] = st.sceneTabAsset[k + 1];
							st.sceneTabId[k] = st.sceneTabId[k + 1];
							st.sceneTabIsoNode[k] = st.sceneTabIsoNode[k + 1];
							st.sceneTabIsoHome[k] = st.sceneTabIsoHome[k + 1];
							st.unitSystemTab[k] = st.unitSystemTab[k + 1];
							st.unitLengthTab[k] = st.unitLengthTab[k + 1];
							st.unitScaleTab[k] = st.unitScaleTab[k + 1];
							st.sceneCamOrtho[k] = st.sceneCamOrtho[k + 1];
							st.sceneCamSet[k] = st.sceneCamSet[k + 1];
							for (int32 a = 0; a < 6; ++a)
								st.sceneCamPose[k][a] = st.sceneCamPose[k + 1][a];
						}
						st.sceneCount--;
						// L'ACTIF suit : ferme avant lui son index recule ; ferme
						// LUI-MEME, l'environnement du nouvel actif s'applique.
						const bool wasAct = (i == st.activeTab);
						if (i < st.activeTab)
							st.activeTab--;
						if (st.activeTab >= st.sceneCount)
							st.activeTab = st.sceneCount - 1;
						if (wasAct)
							NkActivateTab(st, st.activeTab, true);
						break; // la liste a change : on ne continue pas a la parcourir
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
			}
			const NkRect ar{x + S(4.f), r.y + 2.f, S(24.f), h};
			HoverFill(p, ar, hit.Add("tab.add", ar));
			p.IconV(x + S(8.f), r.y, r.h, NkIcon::Add, NkRole::Text, 12.f);
			if (hit.Clicked("tab.add") && st.sceneCount < 8) {
				// Le nom par defaut est NUMEROTE : deux scenes homonymes seraient
				// indistinguables. Une scene NEUVE nait VIERGE : les objets de la
				// demo appartiennent a la premiere scene.
				const int32 nt = st.sceneCount++;
				snprintf(st.sceneNames[nt], 32, "Scene_%d", nt + 1);
				st.sceneCamSet[nt] = false;
				st.sceneBlank[nt] = true;
				st.sceneTabKind[nt] = 0;
				st.sceneTabAsset[nt] = 0;
				st.sceneTabId[nt] = (uint8)st.sceneIdNext++;
				st.sceneTabIsoNode[nt] = 0;
				st.unitSystemTab[nt] = 0;
				st.unitLengthTab[nt] = 0;
				st.unitScaleTab[nt] = 1.f;
				NkActivateTab(st, nt);
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

			btn("tb.save", NkIcon::Save, "Enregistrer");
			p.VLine(x - S(4.f), r.y + S(7.f), r.h - S(14.f));
			x += S(6.f);

			// â”€â”€ DEROULANT DE MODE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce ne sont pas deux boutons mais UNE liste, qui va s'allonger. Deux
			// boutons auraient cesse de tenir au troisieme mode.
			static const NkIcon kModeIc[5] = {NkIcon::Mesh, NkIcon::Edit, NkIcon::Layers,
											  NkIcon::Ruler, NkIcon::Overlay};
			static const char *const kModeNames[5] = {"Objet", "Edition", "Sculpt 2.5D", "Sculpt",
													  "Texturing"};
			const float32 cbH = S(22.f), cbY = r.y + (r.h - cbH) * 0.5f;
			{
				int32 sel = (int32)st.mode;
				Combo(p, hit, ws, "tb.mode", {x, cbY, S(140.f), cbH}, kModeNames, kModeIc, 5, sel, combo);
				st.mode = (NkMode)sel;
			}
			x += S(148.f);
			p.VLine(x - S(7.f), r.y + S(7.f), r.h - S(14.f));

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
			// MODIFICATEUR : liste a DEUX NIVEAUX. Le bouton montre le modificateur
			// courant avec SON icone ; le clic ouvre les categories, et chaque
			// categorie ouvre ses entrees.
			{
				// LE BOUTON DIT Â« MODIFICATEUR Â», pas le nom du dernier choisi.
				// Afficher Â« Miroir Â» laisse croire a un reglage en cours alors que
				// c'est une commande d'AJOUT -- et le modificateur ajoute, lui, vit
				// dans le panneau Details.
				const NkRect mr{x, cbY, S(136.f), cbH};
				const bool over = hit.Add("tb.mod", mr);
				const bool open = ws.ComboOpen("tb.mod");
				p.Outline(mr, (over || open) ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
				p.IconV(x + S(8.f), cbY, cbH, NkIcon::Layers, NkRole::AccentUi, 13.f);
				p.TextV(x + S(27.f), cbY, cbH, "Modificateur");
				p.IconV(x + mr.w - S(18.f), cbY, cbH, open ? NkIcon::ChevronUp : NkIcon::ChevronDown,
						NkRole::Text, 11.f);
				if (hit.Clicked("tb.mod"))
					ws.ToggleCombo("tb.mod");
				st.modAnchor = mr; // memorise pour la liste, peinte apres tout le reste
				x += S(144.f);
			}

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

		// Ligne « Transmettre » d'un parent : quelles composantes de SA
		// transformation atteignent ses enfants (option par transformation,
		// idee de Rihen).
		inline void NkXmitRow(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r,
							  const NkRect &rr, float32 &yy, int32 node) {
			p.TextV(r.x + kPad, yy, kRowH, "Transmettre", NkRole::TextMuted);
			int32 mask = demo::Demo3DHostNodeXmitMask(node);
			static const char *const kXm[3] = {"Pos", "Rot", "Ech"};
			const float32 bw = (rr.w - S(128.f) - S(8.f)) / 3.f;
			char kx[24];
			for (int32 b = 0; b < 3; ++b) {
				const NkRect br{r.x + S(120.f) + (float32)b * (bw + S(4.f)), yy + S(2.f), bw,
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
		inline void NkBrowDelRec(NkModelerState &st, int32 root2) {
			int32 stk[64];
			int32 sp2 = 0;
			stk[sp2++] = root2;
			while (sp2 > 0) {
				const int32 s2 = stk[--sp2];
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
			if (st.sceneTabIsoNode[st.activeTab] > 0)
				return st.sceneTabIsoNode[st.activeTab] - 1;
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
			// CLIC DROIT DANS LA VUE 3D : le menu du noeud ACTIF.
			if (in.mouseClicked[1] && st.hierMenuNode < 0 &&
				NkHitRegistry::Contains(view, in.mousePos)) {
				const int32 actV = st.activeEmpty >= 0
									   ? st.activeEmpty
									   : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (actV >= 0) {
					st.hierMenuNode = actV;
					st.hierMenuX = in.mousePos.x;
					st.hierMenuY = in.mousePos.y;
				}
			}
			// SUPPRIMER (X ou Suppr : le sous-arbre part avec, regle de Rihen),
			// DUPLIQUER (Maj+D), COPIER / COLLER (Ctrl+C / Ctrl+V). Valables
			// aussi la souris sur la vue 3D -- jamais pendant une saisie.
			if (!ws.editing) {
				bool delK = in.keyInit[(int32)nkgui::NkGuiKey::Delete] ||
							(in.keyInit[(int32)nkgui::NkGuiKey::X] && !in.ctrlDown &&
							 !hit.MouseDown());
				bool dupK = in.keyInit[(int32)nkgui::NkGuiKey::D] && in.shiftDown;
				for (int32 ci = 0; ci < in.charCount; ++ci) {
					const uint32 cp = in.chars[ci];
					// X pendant un glissement = verrou d'axe du gizmo, pas une
					// suppression : la souris doit etre relachee.
					if ((cp == 'x' || cp == 'X') && !in.ctrlDown && !hit.MouseDown())
						delK = true;
					if ((cp == 'd' || cp == 'D') && in.shiftDown)
						dupK = true;
				}
				// Les raccourcis POLLES par l'hote (seule voie fiable pour les
				// lettres, constatee avec Rihen) s'ajoutent aux evenements.
				const int32 sk = demo::Demo3DHostTakeShortcuts();
				delK = delK || (sk & 8) != 0;
				dupK = dupK || (sk & 1) != 0;
				// AU-DESSUS DU NAVIGATEUR, les raccourcis agissent sur LUI.
				if (NkHitRegistry::Contains(st.browserRect, in.mousePos)) {
					if (delK && st.selectedAsset >= 0)
						BrDelRec(st.selectedAsset);
					else if (dupK && st.selectedAsset >= 0)
						BrCopyRec(st.selectedAsset, st.browserParent[st.selectedAsset]);
					if ((in.wantCopy || (sk & 2) != 0 ||
						 (in.keyInit[(int32)nkgui::NkGuiKey::C] && in.ctrlDown)) &&
						st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = false;
					}
					if ((sk & 64) != 0 && st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = true;
					}
					if (in.wantPaste || (sk & 4) != 0 ||
						(in.keyInit[(int32)nkgui::NkGuiKey::V] && in.ctrlDown))
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
				if ((in.wantCopy || (sk & 2) != 0 ||
					 (in.keyInit[(int32)nkgui::NkGuiKey::C] && in.ctrlDown)) &&
					actN >= 0)
					{
						demo::Demo3DHostCopyNode(actN);
						NkHierNodeName(st, actN, st.clipName, sizeof(st.clipName));
					}
				if (in.wantPaste || (sk & 4) != 0 ||
					(in.keyInit[(int32)nkgui::NkGuiKey::V] && in.ctrlDown)) {
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
				hmIt[nH] = "Copier  (Ctrl+C)";
				hmAct[nH++] = 1;
				hmIt[nH] = "Coller  (Ctrl+V)";
				hmAct[nH++] = 2;
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
						if (st.sceneCount < 8) {
							const int32 tb7 = st.sceneCount++;
							NkHierNodeName(st, tn, st.sceneNames[tb7], 32);
							st.sceneTabKind[tb7] = 7;
							st.sceneTabAsset[tb7] = 0;
							st.sceneTabIsoNode[tb7] = tn + 1;
							st.sceneTabIsoHome[tb7] = st.sceneTabId[st.activeTab];
							st.sceneTabId[tb7] = (uint8)st.sceneIdNext++;
							st.sceneCamSet[tb7] = false;
							st.sceneBlank[tb7] = true;
							NkActivateTab(st, tb7);
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
					}
					st.hierMenuNode = -1;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mr, hit.Mouse())) {
					st.hierMenuNode = -1;
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
						st.browserKind[k5] = kNewK[act2 - 10];
						st.browserParent[k5] = destF;
						NkBrowUniqueName(st, kNewK[act2 - 10], destF, kNewN[act2 - 10],
										 st.browserNames[k5], 32);
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
								   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
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

			const float32 colEye = r.x + r.w - S(48.f);
			const float32 colLock = r.x + r.w - S(26.f);
			const float32 colType = r.x + r.w - S(122.f);

			// L'EN-TETE annonce TOUTES les colonnes : nom, type, oeil, cadenas --
			// pour que l'utilisateur sache exactement ce que c'est (Rihen).
			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + S(34.f), y, kRowH, "Nom");
			p.TextV(colType, y, kRowH, "Type", NkRole::TextMuted);
			p.IconV(colEye, y, kRowH, NkIcon::Eye, NkRole::TextMuted, 12.f);
			p.IconV(colLock, y, kRowH, NkIcon::Lock, NkRole::TextMuted, 12.f);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			hit.Add("hier.list", listR);
			p.Clip(listR);
			hit.PushClip(listR); // les lignes defilees hors de vue ne cliquent pas

			char key[40];
			float32 yy = y - st.scrollHier;
			int32 visibleCount = 0;

			// Racine : LA SCENE, renommable -- et elle seule. Dans un editeur de
			// MODEL il n'y a pas de ligne de document : le model EST la racine,
			// et l'afficher en plus donnait deux lignes « Model » de meme nom
			// (constate par Rihen sur sa capture). Une scene, elle, n'est pas un
			// noeud : sa ligne est donc necessaire.
			if (st.sceneTabKind[st.activeTab] != 7) {
				const NkRect rowR{r.x, yy, r.w, kRowH};
				hit.Add("hier.scene", rowR);
				p.IconV(r.x + S(6.f), yy, kRowH, NkIcon::Globe, NkRole::Text, 13.f);
				EditableText(p, hit, ws, in, "hier.scene.name",
							 {r.x + S(24.f), yy, colType - r.x - S(30.f), kRowH},
							 st.sceneNames[st.activeTab], NkRole::Text,
							 st.sceneNames[st.activeTab], 32u);
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
						// ICONES DISTINCTES model / mesh (demande de Rihen) : dans un
						// editeur on doit voir d'un coup d'oeil qui est le conteneur
						// et qui est la matiere.
						NkIcon ico = isEmpty ? NkUserKindIcon(node >= 96 ? ukind : 4)
											 : (isLight ? NkIcon::Light : NkIcon::Mesh);
						if (demo::Demo3DHostNodeIsModel(node))
							ico = NkIcon::Cube3D; // le conteneur
						else if (demo::Demo3DHostNodeIsMesh(node))
							ico = NkIcon::Mesh; // la matiere
						p.IconV(tx, yy, kRowH, ico, fg, 13.f);
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
							if (isEmpty && !lokEff) {
								if (hit.ShiftDown() || hit.CtrlDown()) {
									demo::Demo3DHostToggleEmptyNode(node); // multi successif
								} else {
									demo::Demo3DHostDeselectAll();
									demo::Demo3DHostSelectEmptyNode(node);
								}
								st.activeEmpty = node;
							} else if (isLight) {
								if (!lokEff)
									demo::Demo3DHostSelectLight(li);
								st.activeEmpty = -1;
							} else if (!lokEff) {
								demo::Demo3DHostSelectObject(node,
															 hit.ShiftDown() || hit.CtrlDown());
								st.activeEmpty = -1;
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
				// CLIC DROIT DANS LE VIDE (ou sur la racine) : TOUT le menu
				// Ajouter, au pointeur ; l'objet nait a la racine (Rihen).
				st.addParentNode = -1;
				st.addAnchor = {hit.Mouse().x, hit.Mouse().y - S(26.f), 0.f, 0.f};
				if (!ws.ComboOpen("tb.addmenu"))
					ws.ToggleCombo("tb.addmenu");
			}
			if (hit.Clicked("hier.list") && !st.hierDragging && st.hierMenuNode < 0) {
				demo::Demo3DHostDeselectAll();
				st.activeEmpty = -1;
			}

			// Molette par CONTENANCE : les lignes recouvrent la liste, le survol
			// exact la rendait morte (constate). La barre est COLLEE au bord
			// droit ; seule sa zone de saisie s'arrete avant le splitter.
			hit.WheelIn(listR, st.scrollHier, (float32)visibleCount * kRowH, listH);
			NkScrollDrag(p, hit, st, "hier.scrollbar", listR, (float32)visibleCount * kRowH,
						 st.scrollHier);

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
					// Creuse : le trou reprend le fond de la VUE -- le gizmo flotte
					// au-dessus de la scene.
					p.Ring(ex, ey, over ? ball + 2.f : ball, kHalf[i].role, NkRole::ViewportTop);
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
				const NkRect t0{r.x + S(8.f), r.y + S(2.f), S(122.f), wsBarH - S(4.f)};
				hit.Add("ws.tab.0", t0);
				p.Fill(t0, NkRole::AccentUi, 3.f);
				p.IconV(t0.x + S(6.f), t0.y, t0.h, NkIcon::Mesh, NkRole::TextOnAccent, 12.f);
				p.TextV(t0.x + S(24.f), t0.y, t0.h, "Modelisation", NkRole::TextOnAccent);
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
				// ── VUE CAMERA (Rihen) ──────────────────────────────────────
				// Bascule entre la vue 3D libre et CE QUE VOIT une camera de la
				// scene. Le selecteur liste les cameras du document actif.
				{
					if (st.camViewNode > 0 &&
						(NkHierNodeSkip(st.camViewNode - 1) ||
						 demo::Demo3DHostUserSub(st.camViewNode - 1) != 10)) {
						// la camera regardee a disparu (ou change de document)
						st.camViewNode = 0;
						demo::Demo3DHostSetCameraView(-1);
					}
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
					if (hit.Clicked("view.cam"))
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
							// RETOUR : la pose libre d'avant est restituee.
							if (st.camViewNode > 0 && st.camViewSaved)
								demo::Demo3DHostSetCameraPose(
									st.camViewSave, st.camViewSave[3], st.camViewSave[4],
									st.camViewSave[5], st.camViewSaveOrtho);
							st.camViewNode = 0;
							demo::Demo3DHostSetCameraView(-1);
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
								// On MEMORISE la pose libre avant d'entrer, une
								// seule fois : passer d'une camera a l'autre ne
								// doit pas ecraser le point de vue d'origine.
								if (st.camViewNode == 0) {
									bool oo = false;
									demo::Demo3DHostGetCameraPose(
										st.camViewSave, &st.camViewSave[3],
										&st.camViewSave[4], &st.camViewSave[5], &oo);
									st.camViewSaveOrtho = oo;
									st.camViewSaved = true;
								}
								st.camViewNode = cams[c2] + 1;
								demo::Demo3DHostSetCameraView(cams[c2]);
								st.camPickOpen = false;
							}
						}
						if (hit.AnyClick() && !NkHitRegistry::Contains(lb, hit.Mouse()) &&
							!hit.IsHovered("view.pick"))
							st.camPickOpen = false;
					}
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
			struct Snap {
					NkIcon icon;
					char value[16];
			};
			// Les VALEURS viennent de l'ETAT (modifiables dans les proprietes de
			// l'outil) : codees en dur, elles mentaient des le premier reglage.
			Snap kSnaps[3] = {{NkIcon::SnapGrid, {}}, {NkIcon::SnapAngle, {}},
							  {NkIcon::SnapScale, {}}};
			snprintf(kSnaps[0].value, sizeof(kSnaps[0].value), "%.2g", (float64)st.snapStepT);
			snprintf(kSnaps[1].value, sizeof(kSnaps[1].value), "%.0f deg", (float64)st.snapStepR);
			snprintf(kSnaps[2].value, sizeof(kSnaps[2].value), "%.2g", (float64)st.snapStepS);

			// Largeurs, calculees d'abord pour caler le tout a droite.
			const bool editMode2 = demo::Demo3DHostInEditMode();
			const float32 wSub = editMode2 ? (S(8.f) + 3.f * (btn + 2.f)) : 0.f;
			// OUTILS EN DEUX BLOCS dans le meme cadre : [Selection | Curseur] puis
			// un vide, puis [Deplacer | Rotation | Echelle | Multigizmo] -- la
			// disposition demandee par Rihen (celle de Blender).
			const float32 wTools = S(8.f) + 6.f * (btn + 2.f) + S(10.f);
			float32 wSet = S(8.f) + 2.f * (btn + 2.f);
			for (int32 i = 0; i < 3; ++i)
				wSet += btn + p.TextW(kSnaps[i].value) + S(12.f);
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
				// VITESSE DE CAMERA : son icone etait la camera -- le meme dessin
				// que la projection perspective ET que le bouton de vol. Un dessin
				// dedie, sinon la barre a trois boutons jumeaux.
				static const NkIcon kCamIc[4] = {NkIcon::Speed, NkIcon::Speed, NkIcon::Speed,
												 NkIcon::Speed};
				Combo(p, hit, ws, "vp.cam", {cx, barY + 1.f, btn, barH - 2.f}, cams, kCamIc, nCam,
					  st.camSpeed, combo, true, false, false);
				cx += btn + S(8.f);
				p.VLine(cx - S(4.f), barY + S(6.f), barH - S(12.f));

				bool *const flags[3] = {&st.snapGrid, &st.snapAngle, &st.snapScale};
				static const char *const kKeys[3] = {"vp.snap.0", "vp.snap.1", "vp.snap.2"};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kKeys[i], br);
					const bool on = *flags[i];
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i]))
						*flags[i] = !on;
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, kSnaps[i].icon,
							on ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					cx += btn + 3.f;
					// La valeur reste AFFICHEE quand l'aimantation est coupee, mais
					// attenuee : on veut savoir sur quel pas on retombera.
					p.TextV(cx, barY, barH, kSnaps[i].value, on ? NkRole::Text : NkRole::TextMuted);
					cx += p.TextW(kSnaps[i].value) + S(9.f);
				}
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
			const float32 navH = 4.f * 26.f + 3.f * 5.f;
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
					static const char *const kHdrNames[4] = {"Modele", "Rendu", "Scene",
															 "Modificateur"};
					int32 actSec = -1;
					for (int32 i9 = 0; i9 < 4; ++i9)
						if (st.propOpen[i9]) {
							actSec = i9;
							break;
						}
					char hd[64];
					if (actSec >= 0)
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
			// d'autant.
			NkRect r = rFull;
			r.w -= S(26.f);
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
			// LES QUATRE CATEGORIES de la maquette (Rihen, dessin Banani), avec ses
			// icones : Modele (box), Rendu (sun), Scene (layers), Modificateur
			// (sliders-horizontal). UNE SEULE est active a la fois.
			static const NkPropSec kSecs[] = {{"Modele", NkIcon::Cube3D},
											  {"Rendu", NkIcon::Sun},
											  {"Scene", NkIcon::Layers},
											  {"Modificateur", NkIcon::SlidersH}};
			const int32 kNSec = (int32)(sizeof(kSecs) / sizeof(kSecs[0]));
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
			const NkRect rr{r.x, 0.f, r.w - S(14.f), 0.f}; // colonne du scrollbar reservee
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
				float32 yy = secY - st.propScroll3[sec];

				if (sec == 0) {
					// â”€â”€ L'OBJET : nom + TRANSFORMATION COMPLETE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					const int32 li = demo::Demo3DHostSelectedLight();
					const int32 act = demo::Demo3DHostActiveObject();
					if (st.activeEmpty >= 0) {
						// ── UN EMPTY : nom + transformation. Pas de rendu propre --
						// sa transformation n'agit QUE par ses enfants (le detecteur
						// de parente la repercute au sous-arbre).
						const int32 en = st.activeEmpty;
						NkHierNodeName(st, en, buf, sizeof(buf));
						p.IconV(r.x + kPad, yy, kRowH, NkIcon::Cursor, NkRole::Text, 13.f);
						p.TextV(r.x + kPad + S(18.f), yy, kRowH, buf);
						yy += kRowH;
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
							NkRect rowR = rr;
							rowR.x = r.x + kPad;
							rowR.w = rr.w - 2.f * kPad;
							// MEMES ICONES que l'objet : cadenas, reset, proportionnel
							// (un empty est un objet comme un autre, Rihen).
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Position", st.pos, 0.01f,
											  "prop.epos", st.lockPos ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propPos);
							if (hit.Clicked("prop.epos.ic0"))
								st.lockPos = !st.lockPos;
							if (hit.Clicked("prop.epos.ic2"))
								st.propPos = !st.propPos;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation", st.rot, 0.5f,
											  "prop.erot", st.lockRot ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.1f", NkIcon::SnapScale, st.propRot);
							if (hit.Clicked("prop.erot.ic0"))
								st.lockRot = !st.lockRot;
							if (hit.Clicked("prop.erot.ic2"))
								st.propRot = !st.propRot;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Echelle", st.scl, 0.01f,
											  "prop.escl", st.lockScl ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propScale);
							if (hit.Clicked("prop.escl.ic0"))
								st.lockScl = !st.lockScl;
							if (hit.Clicked("prop.escl.ic2"))
								st.propScale = !st.propScale;
							yy += Vec3RowH();
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
								PaintTransformRow(p, hit, ws, in, rowR, yy, "Dimensions", dimE,
									  0.01f, "prop.edim",
									  st.lockDim ? NkIcon::Lock : NkIcon::Unlock,
									  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propDim);
					if (hit.Clicked("prop.edim.ic0"))
						st.lockDim = !st.lockDim;
					if (hit.Clicked("prop.edim.ic2"))
						st.propDim = !st.propDim;
					yy += Vec3RowH();
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
					if (dimChE)
						demo::Demo3DHostSetNodeBaseSize(en, dimE);
							}
							bool diffE = false;
							for (int32 a = 0; a < 3; ++a)
								if (st.pos[a] != sE[a] || st.rot[a] != sE[3 + a] ||
									st.scl[a] != sE[6 + a])
									diffE = true;
							if (diffE) {
								demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
								for (int32 a = 0; a < 3; ++a) {
									sE[a] = st.pos[a];
									sE[3 + a] = st.rot[a];
									sE[6 + a] = st.scl[a];
								}
							}
							if (ukE >= 1 && ukE <= 3) {
								// MATERIAU du maillage utilisateur : les memes reglages
								// que n'importe quel mesh.
								p.TextV(r.x + kPad, yy, kRowH, "Materiau", NkRole::TextMuted);
								yy += kRowH;
								float32 mtE[3], metE = 0.f, rghE = 0.5f;
								demo::Demo3DHostMeshMaterial(en, mtE, &metE, &rghE);
								const float32 mtE0[3] = {mtE[0], mtE[1], mtE[2]};
								const float32 metE0 = metE, rghE0 = rghE;
								PaintTransformRow(p, hit, ws, in, rowR, yy, "Couleur", mtE,
												  0.005f, "prop.emcol", NkIcon::None,
												  NkIcon::None);
								yy += Vec3RowH();
								p.TextV(r.x + kPad, yy, kRowH, "Metallique", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.emmet",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
										   kRowH - S(4.f)},
										  metE, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								p.TextV(r.x + kPad, yy, kRowH, "Rugosite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.emrgh",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
										   kRowH - S(4.f)},
										  rghE, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (mtE[0] != mtE0[0] || mtE[1] != mtE0[1] || mtE[2] != mtE0[2])
									demo::Demo3DHostSetMeshTint(en, mtE);
								if (metE != metE0 || rghE != rghE0)
									demo::Demo3DHostSetMeshMetalRough(en, metE, rghE);
							}
							if (ukE == 5) {
								// LUMIERE UTILISATEUR : ses proprietes NATIVES.
								float32 ulc[3], uli = 1.f;
								if (demo::Demo3DHostUserLightParams(en, ulc, &uli)) {
									bool ulch = false;
									p.TextV(r.x + kPad, yy, kRowH, "Intensite",
											NkRole::TextMuted);
									ulch |= DragFloat(p, hit, ws, in, "prop.ulint",
													  {r.x + S(120.f), yy + S(3.f),
													   rr.w - S(128.f), kRowH - S(4.f)},
													  uli, 0.05f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									const float32 ulc0[3] = {ulc[0], ulc[1], ulc[2]};
									PaintTransformRow(p, hit, ws, in, rowR, yy, "Couleur",
													  ulc, 0.01f, "prop.ulcol", NkIcon::None,
													  NkIcon::None);
									yy += Vec3RowH();
									if (ulch || ulc[0] != ulc0[0] || ulc[1] != ulc0[1] ||
										ulc[2] != ulc0[2])
										demo::Demo3DHostSetUserLightParams(en, ulc, uli);
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
											p.TextV(r.x + kPad, yy, kRowH, lbl, NkRole::TextMuted);
											DragFloat(p, hit, ws, in, k4,
													  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
													  val, stp, NkRole::AccentUi, fm);
											yy += kRowH;
										};
										if (tyL != 0)
											LRow("Portee", "prop.ulex.rg", rgL, 0.05f, "%.2f");
										if (tyL == 2) {
											LRow("Cone interne", "prop.ulex.ci", inL, 0.2f, "%.1f");
											LRow("Cone externe", "prop.ulex.co", outL, 0.2f, "%.1f");
										}
										if (tyL == 3) {
											LRow("Largeur", "prop.ulex.aw", awL, 0.01f, "%.2f");
											LRow("Hauteur", "prop.ulex.ah", ahL, 0.01f, "%.2f");
										}
										{
											const NkRect cb2{r.x + kPad, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add("prop.ulex.sh", cb2);
											p.Outline(cb2, shL ? NkRole::AccentUi : NkRole::Border,
													  shL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb2.x + S(18.f), yy, kRowH, "Ombres portees",
													NkRole::TextMuted);
											if (hit.Clicked("prop.ulex.sh"))
												shL = !shL;
											yy += kRowH;
										}
										if (rgL != v0[0] || inL != v0[1] || outL != v0[2] || awL != v0[3] ||
											ahL != v0[4] || shL != s0)
											demo::Demo3DHostSetLightEx(en, rgL, inL, outL, awL, ahL, shL);
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
									p.TextV(r.x + kPad, yy, kRowH, "Source", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.ulex.mode",
										  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
										  kCMix, nullptr, 3, st.lightSrcUi, combo);
									yy += kRowH;
									if (st.lightSrcUi != derivedM) {
										if (st.lightSrcUi == 0) {
											demo::Demo3DHostSetLightCookie(en, -1);
										} else {
											demo::Demo3DHostSetLightCookie(en, ck0 < 0 ? 0 : ck0);
											if (st.lightSrcUi == 1) {
												const float32 wc[3] = {1.f, 1.f, 1.f};
												float32 c5[3];
								float32 i5 = 1.f;
								demo::Demo3DHostUserLightParams(en, c5, &i5);
								demo::Demo3DHostSetUserLightParams(en, wc, i5);
											}
										}
									}
									if (st.lightSrcUi > 0) {
										float32 slot = (float32)(ck0 < 0 ? 0 : ck0);
										p.TextV(r.x + kPad, yy, kRowH, "Texture (atlas)", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.ulex.slot",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
												  slot, 0.05f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
										if ((int32)(slot + 0.5f) != ck0)
											demo::Demo3DHostSetLightCookie(en, (int32)(slot + 0.5f));
									}
								}
							}
							{
								// CAMERA : ses proprietes propres (declaratives tant que
								// le rendu au travers de la camera n'est pas branche).
								float32 cf = 50.f, cnr = 0.1f, cfr = 100.f;
								if (demo::Demo3DHostCameraParams(en, &cf, &cnr, &cfr)) {
									const float32 c0[3] = {cf, cnr, cfr};
									p.TextV(r.x + kPad, yy, kRowH, "Camera",
											NkRole::TextMuted);
									yy += kRowH;
									p.TextV(r.x + kPad, yy, kRowH, "Focale (deg)",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cfov",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cf, 0.2f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
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
									if (cf != c0[0] || cnr != c0[1] || cfr != c0[2])
										demo::Demo3DHostSetCameraParams(en, cf, cnr, cfr);
								}
							}
							NkXmitRow(p, hit, r, rr, yy, en);
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
							if (sLLast == li)
								demo::Demo3DHostSetLightPosition(li, lpos);
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
						PaintTransformRow(p, hit, ws, in, rowR, yy, "Couleur", lcol, 0.01f,
										  "prop.lcol", NkIcon::None, NkIcon::None);
						static float32 sLC[4] = {-1.f, 0.f, 0.f, 0.f};
						if (lch || lcol[0] != sLC[1] || lcol[1] != sLC[2] || lcol[2] != sLC[3]) {
							if ((int32)sLC[0] == li) {
								for (int32 a = 0; a < 3; ++a)
									if (lcol[a] < 0.f)
										lcol[a] = 0.f;
								demo::Demo3DHostSetLightParams(li, lcol, lint < 0.f ? 0.f : lint);
								// PROPAGER coche : memes reglages pour les lumieres
								// DESCENDANTES de celle-ci (propriete commune).
								if (st.matPropagate)
									for (int32 l2 = 0; l2 < demo::Demo3DHostLightCount(); ++l2)
										if (l2 != li && NkHierIsDescendant(86 + l2, 86 + li))
											demo::Demo3DHostSetLightParams(l2, lcol,
																		   lint < 0.f ? 0.f : lint);
							}
							sLC[0] = (float32)li;
							sLC[1] = lcol[0];
							sLC[2] = lcol[1];
							sLC[3] = lcol[2];
						}
						yy += Vec3RowH();
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
									ahL != v0[4] || shL != s0)
									demo::Demo3DHostSetLightEx(86 + li, rgL, inL, outL, awL, ahL, shL);
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
								} else {
									demo::Demo3DHostSetLightCookie(86 + li, ck0 < 0 ? 0 : ck0);
									if (st.lightSrcUi == 1) {
										const float32 wc[3] = {1.f, 1.f, 1.f};
										demo::Demo3DHostSetLightParams(li, wc, lint < 0.f ? 0.f : lint);
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
								if ((int32)(slot + 0.5f) != ck0)
									demo::Demo3DHostSetLightCookie(86 + li, (int32)(slot + 0.5f));
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
											  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propPos);
							if (hit.Clicked("prop.pos.ic0"))
								st.lockPos = !st.lockPos;
							if (hit.Clicked("prop.pos.ic2"))
								st.propPos = !st.propPos;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation", st.rot, 0.5f,
											  "prop.rot", st.lockRot ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.1f", NkIcon::SnapScale, st.propRot);
							if (hit.Clicked("prop.rot.ic0"))
								st.lockRot = !st.lockRot;
							if (hit.Clicked("prop.rot.ic2"))
								st.propRot = !st.propRot;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Echelle", st.scl, 0.01f,
											  "prop.scl", st.lockScl ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propScale);
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
									  NkIcon::Refresh, "%.2f", NkIcon::SnapScale, st.propDim);
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
					if (dimCh)
						demo::Demo3DHostSetNodeBaseSize(act, dim);
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
						// ── ORIGINE (PIVOT) ─────────────────────────────────────────
						// Elle merite une place a part (Rihen) : c'est autour d'elle
						// que l'objet TOURNE et se met a l'ECHELLE, lui et tout ce
						// qu'il porte. On peut la poser n'importe ou -- la deplacer
						// ici ne deplace PAS la matiere, seulement le point de pivot.
						if (act >= 90) {
							float32 org[3];
							if (demo::Demo3DHostNodeOrigin(act, org)) {
								const float32 org0[3] = {org[0], org[1], org[2]};
								PaintTransformRow(p, hit, ws, in, r, yy, "Origine", org,
												  0.01f, "props.org", NkIcon::None,
												  NkIcon::None);
								yy += Vec3RowH();
								bool orgCh = false;
								for (int32 a = 0; a < 3; ++a)
									if (org[a] != org0[a])
										orgCh = true;
								if (orgCh)
									demo::Demo3DHostSetNodeOrigin(act, org);
								float32 ctr[3];
								if (demo::Demo3DHostMeshesCenter(act, ctr)) {
									if (Button("props.orgctr", yy,
											   "Origine au centre des maillages",
											   r.x + kPad, rr.w - 2.f * kPad))
										demo::Demo3DHostSetNodeOrigin(act, ctr);
									yy += kRowH;
								}
							}
						}
						// ── MATERIAU DU MAILLAGE (proprietes par TYPE) ──────────────
						// Lecture = valeurs EFFECTIVES de la derniere soumission ;
						// ecriture = SURCHARGE par objet. PROPAGER cochee : la
						// modification s'applique aussi aux maillages DESCENDANTS
						// (propriete commune parent/enfant, regle de Rihen).
						{
							p.TextV(r.x + kPad, yy, kRowH, "Materiau", NkRole::TextMuted);
							yy += kRowH;
							float32 mt[3], met = 0.f, rgh = 0.5f;
							demo::Demo3DHostMeshMaterial(act, mt, &met, &rgh);
							const float32 mt0[3] = {mt[0], mt[1], mt[2]};
							const float32 met0 = met, rgh0 = rgh;
							NkRect rowM = rr;
							rowM.x = r.x + kPad;
							rowM.w = rr.w - 2.f * kPad;
							PaintTransformRow(p, hit, ws, in, rowM, yy, "Couleur", mt, 0.005f,
											  "prop.mcol", NkIcon::None, NkIcon::None);
							yy += Vec3RowH();
							p.TextV(r.x + kPad, yy, kRowH, "Metallique", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.mmet",
									  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
									  met, 0.005f, NkRole::AccentUi, "%.2f");
							yy += kRowH;
							p.TextV(r.x + kPad, yy, kRowH, "Rugosite", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.mrgh",
									  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
									  rgh, 0.005f, NkRole::AccentUi, "%.2f");
							yy += kRowH;
							const bool actParent = demo::Demo3DHostNodeHasChildren(act);
							if (actParent) {
								NkPropagateCheck(p, hit, r, yy, "prop.mprop", st.matPropagate);
								yy += kRowH;
							}
							auto clamp01 = [](float32 v) {
								return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
							};
							const bool tintCh =
								mt[0] != mt0[0] || mt[1] != mt0[1] || mt[2] != mt0[2];
							const bool mrCh = met != met0 || rgh != rgh0;
							if (tintCh || mrCh) {
								for (int32 a = 0; a < 3; ++a)
									mt[a] = clamp01(mt[a]);
								met = clamp01(met);
								rgh = clamp01(rgh);
								if (tintCh)
									demo::Demo3DHostSetMeshTint(act, mt);
								if (mrCh)
									demo::Demo3DHostSetMeshMetalRough(act, met, rgh);
								if (st.matPropagate && actParent) {
									const int32 nO2 = demo::Demo3DHostObjectCount();
									for (int32 n3 = 0; n3 < nO2; ++n3) {
										if (n3 == act || !NkHierIsDescendant(n3, act))
											continue;
										if (tintCh)
											demo::Demo3DHostSetMeshTint(n3, mt);
										if (mrCh)
											demo::Demo3DHostSetMeshMetalRough(n3, met, rgh);
									}
								}
							}
							if (Button("prop.mreset", yy, "Materiau d'origine", r.x + kPad,
									   rr.w - 2.f * kPad)) {
								demo::Demo3DHostResetMeshMat(act);
								if (st.matPropagate && actParent) {
									const int32 nO2 = demo::Demo3DHostObjectCount();
									for (int32 n3 = 0; n3 < nO2; ++n3)
										if (NkHierIsDescendant(n3, act))
											demo::Demo3DHostResetMeshMat(n3);
								}
							}
							yy += kRowH;
						}
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
					// ── RENDU (pastille « sun » de la maquette) ─────────────────
					// Categorie a definir avec Rihen : on annonce ce qu'elle sera
					// plutot que d'y empiler des reglages pris ailleurs.
					p.TextV(r.x + kPad, yy, kRowH, "Parametres de rendu", NkRole::Text);
					yy += kRowH;
					p.TextV(r.x + kPad, yy, kRowH, "-- a definir --", NkRole::TextMuted);
					yy += kRowH;
				} else if (sec == 2) {
					// â”€â”€ LA SCENE : champs GLISSABLES (comme les transformations) â”€
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
							st.projection = 1;
							st.lastProjection = 1;
						}
						yy += kRowH;
					}
					const NkRect fr{r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)};
					{
						p.TextV(r.x + kPad, yy, kRowH, "Taille ortho", NkRole::TextMuted);
						float32 os = demo::Demo3DHostOrthoScale();
						if (DragFloat(p, hit, ws, in, "props.oscale", fr, os, 0.005f,
									  NkRole::AccentUi, "%.2f"))
							demo::Demo3DHostSetOrthoScale(os);
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Distance (0=auto)", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 fv = demo::Demo3DHostViewFar();
						if (DragFloat(p, hit, ws, in, "props.far", fr2, fv, 5.f, NkRole::AccentUi,
									  "%.0f"))
							demo::Demo3DHostSetViewFar(fv < 20.f ? 0.f : fv);
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Etendue grille", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 ge = (float32)demo::Demo3DHostGridExtent();
						if (DragFloat(p, hit, ws, in, "props.grid", fr2, ge, 1.f, NkRole::AccentUi,
									  "%.0f"))
							demo::Demo3DHostSetGridExtent((int32)(ge + 0.5f));
						yy += kRowH;
					}
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
								// scene par son nom, dans l'ordre des onglets.
								static const char *sTgts[9];
								sTgts[0] = "Toutes les scenes";
								for (int32 t7 = 0; t7 < st.sceneCount && t7 < 8; ++t7)
									sTgts[t7 + 1] = st.sceneNames[t7];
								if (st.propCopyTarget > st.sceneCount)
									st.propCopyTarget = 0;
								Combo(p, hit, ws, "props.utgt",
									  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
									   kRowH - S(4.f)},
									  sTgts, nullptr, st.sceneCount + 1,
									  st.propCopyTarget, combo);
								yy += kRowH;
								if (Button("props.ucopy", yy, "Copier les proprietes",
										   r.x + kPad, rr.w - 2.f * kPad)) {
									const int32 t0 =
										st.propCopyTarget <= 0 ? 0 : st.propCopyTarget - 1;
									const int32 t1 = st.propCopyTarget <= 0
														 ? st.sceneCount - 1
														 : st.propCopyTarget - 1;
									for (int32 t7 = t0; t7 <= t1 && t7 < 8; ++t7) {
										st.unitSystemTab[t7] = st.unitSystem;
										st.unitLengthTab[t7] = st.unitLength;
										st.unitScaleTab[t7] = st.unitScale;
									}
								}
								yy += kRowH;
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
				} else if (sec == 3) {
					// ── MODIFICATEUR (pastille « sliders-horizontal ») ──────────
					// La categorie reste A DEFINIR avec Rihen. En attendant, elle
					// HEBERGE les reglages de l'outil actif : ils etaient sur une
					// pastille supprimee par la maquette, et les perdre en silence
					// serait une regression. Ils demenageront a la refonte.
					p.TextV(r.x + kPad, yy, kRowH, "Modificateurs -- a definir",
							NkRole::TextMuted);
					yy += kRowH;
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
						p.TextV(r.x + kPad, yy, kRowH, "Orientation", NkRole::TextMuted);
						int32 nOr = 0;
						const char *const *orients = NkOrientItems(nOr);
						float32 bx = r.x + S(96.f);
						for (int32 i3 = 0; i3 < nOr; ++i3) {
							snprintf(key, sizeof(key), "props.or.%d", i3);
							float32 wq = (rr.w - S(108.f) - 8.f) / 3.f;
							if (wq < S(30.f))
								wq = S(30.f);
							const NkRect br{bx, yy + S(2.f), wq, kRowH - S(4.f)};
							hit.Add(key, br);
							const bool on = (st.orientation == i3);
							if (on)
								p.Fill(br, NkRole::AccentUi, 3.f);
							else
								p.Outline(br, NkRole::Border, NkRole::PanelHeader, 3.f);
							p.Clip(br);
							p.TextV(br.x + S(4.f), yy, kRowH, orients[i3],
									on ? NkRole::TextOnAccent : NkRole::Text);
							p.Unclip();
							if (hit.Clicked(key))
								st.orientation = i3;
							bx += wq + 4.f;
						}
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
					// ── SECOND BLOC : l'outil d'EDITION quand le mode Edit est actif.
					// Deux natures coexistent (deplacer + extruder) : chaque outil a
					// SON bloc, empile avec son titre.
					if (demo::Demo3DHostInEditMode()) {
						yy += S(4.f);
						p.Fill({r.x, yy, rr.w, kRowH}, NkRole::PanelHeader);
						p.TextV(r.x + kPad, yy, kRowH, "Outil d'edition");
						yy += kRowH;
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
					}
				}

				sContentH[sec] = (yy + st.propScroll3[sec]) - secY + S(4.f);
				// Une section dont le contenu TIENT n'avale pas la molette : elle
				// n'a rien a faire defiler, c'est donc la PILE qui doit bouger.
				if (sContentH[sec] > boxH)
					anyWheel |= hit.WheelIn(box, st.propScroll3[sec], sContentH[sec], boxH);
				else
					st.propScroll3[sec] = 0.f;
				hit.PopClip();
				p.Unclip();
				snprintf(key, sizeof(key), "props.sb.%d", sec);
				{
					NkRect inBox = box;
					inBox.w -= S(9.f); // la barre de section s'INSERE ; le bord est a la globale
					NkScrollDrag(p, hit, st, key, inBox, sContentH[sec], st.propScroll3[sec]);
				}
				secY += boxH;
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
				NkScrollDrag(p, hit, st, "props.outer", {r.x, stackTop, r.w, viewH}, stackH,
							 st.propScroll);
			}
			// ── LES PASTILLES : une par section, a droite. BLEUE = active.
			// UNE SEULE A LA FOIS (regle de Rihen) : le panneau montre les
			// proprietes de LA categorie choisie, et rien d'autre. Les ouvrir
			// ensemble revenait a empiler des blocs sans rapport et a rogner la
			// place de chacun -- illisible des que les categories se comptent en
			// dizaines. Recliquer la pastille active replie le panneau.
			{
				p.VLine(r.x + r.w, stackTop, (rFull.y + rFull.h) - stackTop);
				float32 ty = stackTop + S(4.f);
				for (int32 i2 = 0; i2 < kNSec; ++i2) {
					char tk[24];
					snprintf(tk, sizeof(tk), "props.tab.%d", i2);
					const NkRect tb{r.x + r.w + S(3.f), ty, S(20.f), S(24.f)};
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
							st.dirty = true;
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
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
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
			float32 tyy = ty + S(12.f) - assetScroll;
			int32 shown = 0;
			char akey[40];
			const float32 wrapW = r.x + r.w - S(16.f);
			for (int32 i = 0; i < st.browserCount; ++i) {
				if (st.browserKind[i] == 255 || st.browserParent[i] != st.browserFolder)
					continue; // supprimes ignores ; les dossiers ont leur carte
				++shown;
				if (tx + tw > wrapW) { // retour a la ligne
					tx = ax;
					tyy += cardH + S(14.f);
				}
				const uint8 kind = st.browserKind[i]; // 0 blueprint, 2 materiau, 3 texture
				const NkRole role = (kind == 0)   ? NkRole::TypeMesh
									: (kind == 1) ? NkRole::TypeFolder
									: (kind == 4) ? NkRole::AccentUi
									: (kind == 5) ? NkRole::AxisZ
									: (kind == 6) ? NkRole::AxisX
									: (kind == 2) ? NkRole::TypeMat
												  : NkRole::TypeTex;
				const char *kindName = (kind == 0)   ? "Graphe"
									   : (kind == 1) ? "Dossier"
									   : (kind == 4) ? "Dataset IA"
									   : (kind == 5) ? "Scene"
									   : (kind == 0 && st.browserSub[i] == 0) ? "Graphe modelisation"
									   : (kind == 0 && st.browserSub[i] == 1) ? "Graphe texturing"
									   : (kind == 0 && st.browserSub[i] == 2) ? "Graphe materiau"
									   : (kind == 0 && st.browserSub[i] == 3) ? "Graphe motion"
									   : (kind == 6) ? "Model"
									   : (kind == 2) ? "Materiau"
													 : "Texture";

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
					// BLUEPRINT : un cube en volume -- c'est une piece a modeler.
					const float32 hw = 18.f, hh = 16.f, dp = 9.f;
					p.Fill({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, NkRole::PanelHeader);
					p.OutlineSharp({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, role);
					p.Line(cx - hw, cy - hh + dp, cx - hw + dp, cy - hh, role);
					p.Line(cx - hw + dp, cy - hh, cx + hw + dp, cy - hh, role);
					p.Line(cx + hw, cy - hh + dp, cx + hw + dp, cy - hh, role);
					p.Line(cx + hw + dp, cy - hh, cx + hw + dp, cy + hh - dp, role);
					p.Line(cx + hw, cy + hh, cx + hw + dp, cy + hh - dp, role);
				} else if (kind == 2) {
					// Boule de rendu avec reflet : sans reflet, le disque se lit
					// comme une pastille de couleur.
					p.Disc(cx, cy, 22.f, role);
					p.Disc(cx - 8.f, cy - 8.f, 5.f, NkRole::Text);
				} else if (kind == 5) {
					// SCENE : un globe raye -- un monde a ouvrir.
					p.Disc(cx, cy, 22.f, role);
					p.Fill({cx - 22.f, cy - 2.f, 44.f, 4.f}, NkRole::PanelHeader);
					p.Fill({cx - 2.f, cy - 22.f, 4.f, 44.f}, NkRole::PanelHeader);
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
					for (int32 gx = 0; gx < 6; ++gx)
						for (int32 gy = 0; gy < 6; ++gy)
							p.Fill({cx - half + (float32)gx * q, cy - half + (float32)gy * q, q, q},
								   ((gx + gy) & 1) ? role : NkRole::PanelHeader);
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
					EditableText(p, hit, ws, in, akey, {tx + pad, fyy, tw - pad * 2.f, lh + 2.f},
								 st.browserNames[i], NkRole::Text, st.browserNames[i], 32u);
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
					for (int32 t9 = 0; t9 < st.sceneCount; ++t9)
						if (st.sceneTabAsset[t9] == i + 1 && st.sceneTabKind[t9] == tk9)
							tb = t9; // deja ouvert : on l'ACTIVE simplement
					if (tb < 0 && st.sceneCount < 8) {
						tb = st.sceneCount++;
						NkWidgetState::Copy(st.sceneNames[tb], st.browserNames[i], 31u);
						st.sceneTabAsset[tb] = i + 1;
						st.sceneTabKind[tb] = tk9;
						st.sceneTabId[tb] = (uint8)st.sceneIdNext++;
						st.sceneTabIsoNode[tb] = 0;
						st.unitSystemTab[tb] = 0;
						st.unitLengthTab[tb] = 0;
						st.unitScaleTab[tb] = 1.f;
						st.sceneCamSet[tb] = false;
						// contenu REEL au chargement : viendra du format projet
						st.sceneBlank[tb] = true;
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
			if (shown == 0)
				p.TextV(ax, ty + S(12.f), kRowH,
						"Vide -- creez un dossier, un materiau ou une texture", NkRole::TextMuted);

			p.Unclip();
			const NkRect treeArea{r.x, ty, treeW, th};
			const NkRect assetArea{ax - S(10.f), ty + S(33.f), r.w - treeW - S(10.f), th - S(33.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			// La hauteur de contenu etait figee a 125 px, valeur de l'ancienne carte.
			// Les cartes font maintenant 133 px : le defilement s'arretait avant le bas
			// et la derniere rangee restait inaccessible.
			const float32 assetContentH = (tyy + cardH + S(14.f)) - ty + assetScroll;
			hit.Wheel("brow.assets", st.scrollAssets, assetContentH, assetArea.h);
			p.VScroll(treeArea, 5.f * kRowH + S(8.f), treeScroll);
			p.VScroll(assetArea, assetContentH, assetScroll);
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
				{"Nouveau", "", false},		{"Ouvrir...", "", false}, {"Ouvrir recent", "", true},
				{nullptr, "", false},		{"Enregistrer", "", false}, {"Enregistrer sous...", "", false},
				{nullptr, "", false},		{"Importer", "", true},   {"Exporter", "", true},
				{nullptr, "", false},		{"Quitter", "", false},
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
			static const NkMenuDef kMenus[] = {
				{"Fichier", kFile, 11},	  {"Edition", kEdit, 7},	{"Fenetre", kWindow, 8},
				{"Outils", kTools, 6},	  {"Selection", kSelect, 5}, {"Objet", kObject, 6},
				{"Aide", kHelp, 4},
			};
			outCount = 7;
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
					if (st.openMenu == 0 && i == 10) // Fichier > Quitter
						st.askClose = true;
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
							st.dirty = true;
							ws.CloseCombo();
						}
					}
				}
				flatBase += cats[c].count;
			}

			// Un clic HORS des deux panneaux referme. Teste apres toutes les zones :
			// sinon un clic sur une entree refermerait avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("mod.panel") && !hit.IsHovered("mod.sub")
				&& !hit.IsHovered("tb.mod")) {
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
							st.dirty = true;
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
		inline void PaintCloseDialog(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
									 NkHitRegistry &hit) {
			if (!st.askClose)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlg.veil", {0.f, 0.f, W, H});

			const float32 bw = S(420.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlg.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Quitter NK3DModeler ?");
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					"La scene a des modifications non enregistrees.", NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'ordre compte : l'action la plus SURE est a droite, sous la main, et
			// c'est elle qui porte l'accent. Â« Quitter sans enregistrer Â» reste
			// atteignable mais ne se propose pas.
			const B kBtns[3] = {{"dlg.cancel", "Annuler", false},
								{"dlg.discard", "Quitter sans enregistrer", false},
								{"dlg.save", "Enregistrer et quitter", true}};
			float32 bx = box.x + box.w - S(14.f);
			for (int32 i = 2; i >= 0; --i) {
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
				// L'enregistrement reel viendra avec le format de projet. On marque le
				// document propre pour que le chemin soit deja juste.
				st.dirty = false;
				st.running = false;
			}
		}

		// â”€â”€ BARRE D'ETAT â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintStatus(NkModelerPainter &p, const NkRect &r, const NkModelerState &st) {
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
			p.VLine(x, r.y + 6.f, r.h - 12.f);
			x += 10.f;
			p.Fill({x, r.y + (r.h - 20.f) * 0.5f, 240.f, 20.f}, NkRole::InputBg, 2.f);
			p.IconV(x + 6.f, r.y, r.h, NkIcon::Terminal, NkRole::Text, 12.f);
			p.TextV(x + 24.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);
		}

	} // namespace nk3d
} // namespace nkentseu
