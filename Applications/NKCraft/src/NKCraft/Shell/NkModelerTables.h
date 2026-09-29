#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerTables.h
// @Brief   TABLES et CONSTANTES de l'interface du modeleur : metriques (hauteur
//          de ligne, colonne de libelles), libelles et icones des listes
//          deroulantes (projection, ombrage, orientation, incrustations...), et
//          catalogues des objets a ajouter et des modificateurs.
//
//          Ce fichier ne PEINT rien et ne lit aucun etat : ce sont des donnees.
//          Extrait de NkModelerScreens.h (14 495 lignes) au premier lot de la
//          refonte d'interface -- « subdiviser les gros fichiers » (Rihen,
//          13 aout 2026). Y ajouter une entree ne recompile plus tout l'ecran.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"    // NkRect, NkModelerPainter, S()
#include "NK3DModeler/Shell/NkModelerIcons.h" // NkIcon

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

		// Nom AFFICHE d'un noeud de scene : nom personnalise > nom de la demo >
		// nom d'empty par defaut (les anciens groupes, noeuds 90..95).
		static const char *const kNkEmptyNames[6] = {"Spheres", "Cube central", "Colonnes",
													 "Instances", "Decor", "Lumieres"};

	} // namespace nk3d
} // namespace nkentseu
