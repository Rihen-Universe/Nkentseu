#pragma once
// =============================================================================
// NkModelerInput.h — l'etat de l'application et le survol/clic sur les zones.
//
// POURQUOI UNE COUCHE A PART PLUTOT QUE DES `if` DANS LE DESSIN
//   Peindre et reagir sont deux besoins differents. Si chaque fonction de
//   peinture testait elle-meme la souris, il faudrait la lui passer partout, et
//   surtout : la ZONE cliquable finirait par diverger de la zone DESSINEE des la
//   premiere retouche de mise en page. On declare donc les zones UNE fois, au
//   moment ou on les dessine, et on interroge ensuite.
//
// LE PRINCIPE : REGISTRE DE ZONES.
//   Chaque frame, la peinture enregistre ses rectangles sensibles avec une CLE.
//   La couche d'interaction dit ensuite « quelle zone est sous la souris »,
//   « laquelle vient d'etre cliquee ». Zone dessinee et zone cliquable sont donc
//   le MEME rectangle par construction -- impossible qu'elles se decalent.
//
//   C'est le principe de l'interface immediate, et il evite le defaut classique
//   du bouton qui repond a cote parce qu'on a bouge le dessin sans bouger le
//   test.
// =============================================================================

#include "NKGui/Core/NkGuiContext.h"
#include "NKEditorKit/NkShortcutTable.h"

namespace nkentseu {
	namespace nk3d {

		using nkgui::NkRect;

		// ── MODES ───────────────────────────────────────────────────────────────
		// L'ordre est celui du deroulant. Sculpt 2.5D et Sculpt sont DEUX modes et
		// non un seul avec une option : ils n'ont ni les memes outils, ni le meme
		// resultat (le premier travaille en espace ecran, le second sur la
		// geometrie reelle).
		// PATRON = le depliage UV (unwrapping) ; TEXTURE PAINTING = la peinture
		// sur texture. Ajoutes EN FIN d'enum : les indices existants ne bougent
		// pas (onglets, pastilles et en-tetes s'y referent par valeur).
		enum class NkMode : uint8 {
			Object = 0,
			Edit,
			Sculpt25D,
			Sculpt,
			Texturing,
			Patron,
			TexturePaint,
			Count
		};

		inline const char *NkModeName(NkMode m) {
			static const char *const kNames[] = {"Objet",	  "Edition", "Sculpt 2.5D",
												 "Sculpt",	  "Texturing", "Patron",
												 "Texture painting"};
			return (uint8)m < (uint8)NkMode::Count ? kNames[(uint8)m] : "?";
		}

		// Sous-mode de selection, valable uniquement hors mode objet.
		enum class NkSubMode : uint8 { Vertex = 0, Edge, Face };

		// Outil actif : « que fait mon clic ? ». Un seul a la fois.
		// MultiGizmo = le mode COMBINE de la demo (T+R+S en un seul gizmo).
		enum class NkTool : uint8 { Select = 0, Cursor, Move, Rotate, Scale, MultiGizmo };

		// ── INTENTION CLAVIER ───────────────────────────────────────────────────
		// Une TOUCHE ne fait rien elle-meme : elle pose une intention, consommee
		// entre deux images. Deux raisons, et la seconde est la vraie :
		//  - les evenements arrivent hors de la boucle, donc modifier le maillage
		//    depuis un callback reentrerait dans une image en cours de peinture ;
		//  - une intention nommee se relie a un menu, a une palette de commandes et
		//    a un panneau d'outils sans rien dupliquer. Si la touche appelait
		//    directement l'operation, chaque nouveau chemin d'acces recopierait la
		//    meme logique -- et ils divergeraient.
		//
		// ENUMERATION EN AJOUT SEUL si elle est un jour serialisee dans un fichier
		// de raccourcis : renumeroter rendrait faux tous les fichiers existants.
		enum class NkVpAction : uint8 {
			None = 0,
			ToggleEdit,
			SubModeVertex,
			SubModeEdge,
			SubModeFace,
			SelectAll,
			SelectNone,
			ToolMove,
			ToolRotate,
			ToolScale,
			// Transformations MODALES : la touche arme, la souris pilote. Elles
			// sont distinctes des outils -- G ne selectionne pas l'outil
			// deplacement, il DEPLACE tout de suite.
			ModalMove,
			ModalRotate,
			ModalScale,
			ModalAxisX,
			ModalAxisY,
			ModalAxisZ,
			ModalConfirm,
			ModalCancel,
			// Outils de selection par ZONE. Ils s'ARMENT (la touche) puis
			// s'appliquent au glissement : c'est ce qui permet de choisir la forme
			// avant de tracer, au lieu de deviner ce qu'un glissement veut dire.
			ZoneRect,
			ZoneCircle,
			CursorPlace,
			ToggleXray,
			Extrude,
			ExtrudeIndividual,
			Delete,
			Dissolve,
			Merge,
			MakeFace,
			Subdivide,
			LoopCut,
			Inset,
			BevelEdge,
			BevelVertex,
			Undo,
			Redo,
			ViewFront,
			ViewBack,
			ViewRight,
			ViewLeft,
			ViewTop,
			ViewBottom,
			ToggleOrtho,
			FrameAll,
		};

		// ── ETAT DE SESSION ─────────────────────────────────────────────────────
		struct NkModelerState {
				NkMode mode = NkMode::Object;
				NkSubMode subMode = NkSubMode::Face;
				NkTool tool = NkTool::Move;

				// Aimantation : une bascule PAR transformation (cf. UI_SPEC / Unreal).
				bool snapGrid = true, snapAngle = true, snapScale = false;

				// -1 = AUCUNE selection. C'est un etat legitime : celui ou les commandes
				// de scene s'appliquent, et non une valeur sentinelle d'erreur.
				int32 selectedObject = 1;
				int32 selectedAsset = 0;  ///< carte du navigateur
				int32 selectedFolder = 1; ///< dossier du navigateur
				int32 matcap = 0;		  ///< matcap actif, mode edition seulement
				int32 viewLayout = 0;	  ///< disposition des vues (menu de gauche)
				int32 selShape = 0;		  ///< rectangle / cercle / lasso
				int32 modOpenCat = 0;	  ///< categorie de modificateurs survolee
				NkRect modAnchor{};		  ///< ou ancrer la liste a deux niveaux
				int32 activeTab = 0;
				int32 activeFilter = 4; ///< pastille « Tout »

				// ── ETATS DES LISTES DEROULANTES ────────────────────────────────
				// Un indice par combo. Les libelles vivent dans les tables de
				// NkModelerScreens : ici on ne garde que le CHOIX, qui est ce qui
				// appartient a la session.
				int32 projection = 0; ///< 0 perspective, 1..6 vues orthographiques
				int32 shading = 0;	  ///< eclaire / non eclaire / fil de fer / rendu
				// Masque de surimpressions : grille, repere, contours, gizmos, normales,
				// statistiques, filaire, origines. Grille + repere + contours + gizmos
				// par defaut -- ce qu on veut voir en ouvrant, sans le bruit du reste.
				// bits : 1 grille, 2 lignes fines, 4 lignes majeures, 8 axes du plan,
				// 16 contour de selection, 32 HUD texte de la demo (off par defaut :
				// il chevauchait la barre d'outils), 64 CURSEUR 3D.
				// GRILLE, LIGNES FINES ET MAJEURES COUPEES par defaut (Rihen) :
				// c'est le SOL INFINI en damier qui donne le repere au sol --
				// restent les axes et le contour de selection.
				// Le CURSEUR 3D est COCHE par defaut (Rihen) : c'est un repere de
				// travail attendu, qu'on doit pouvoir eteindre sans renoncer a
				// l'outil qui le place.
				uint32 overlayMask = 0x18u | 0x40u;
// Sections DEROULEES du panneau Details, un bit par section (maillage,
				// modificateurs, materiaux, sous-maillages). Les quatre ouvertes au
				// depart : un panneau qui s'ouvre tout replie oblige a quatre clics
				// avant de montrer quoi que ce soit.
				uint32 detailOpen = 0x0Fu;
				int32 materialSlot = 0; ///< emplacement de materiau courant
				// PANNEAUX VISIBLES. Ils ont une taille minimale (cf. NkLayout) : on ne
				// les retrecit donc pas jusqu'a disparition, on les MASQUE franchement.
				// Un panneau reduit a trois pixels n'est ni utilisable ni refermable.
				bool showLeft = true;
				bool showRight = true;
				bool showBrowser = true;
				// Fraction horizontale du clic dans la barre de titre, retenue au moment
				// du clic pour replacer la fenetre sous le curseur quand on la tire
				// depuis l'etat maximise.
				float32 dragFracX = 0.5f;
				// Navigation dans la vue : dernier point connu et glissement en cours. Le
				// « en cours » est indispensable -- sans lui, le premier deplacement apres
				// un appui ferait un bond, parce que le point precedent daterait de la
				// derniere position survolee ailleurs dans l'ecran.
				// Intention en attente et saisie de texte en cours. Le second garde
				// est indispensable : sans lui, taper « e » dans un champ de nom
				// extrude le maillage.
				// Fond de la vue 3D : indice dans la table de prereglages de l'ecran.
				NkRect addAnchor{};	  ///< bouton « Ajouter », pour ancrer son menu
				int32 addOpenCat = 0; ///< categorie ouverte du menu Ajouter
				int32 bgChoice = 0;
				// Fond PERSONNALISE (le picker demande par Rihen) + menus de la vue.
				float32 bgCustom[3] = {0.13f, 0.15f, 0.19f};
				bool bgMenuOpen = false;
				bool bgPickerOpen = false;
				int32 bgDragChannel = -1;
				bool viewMenuOpen = false;
				// Selecteur de matcaps : par CATEGORIE, avec defilement V et H.
				bool matcapOpen = false;
				float32 matcapScrollY = 0.f;
				float32 matcapScrollX = 0.f;
				// Panneau droit unique : glissement de reglage en cours + defilement.
				char propDragKey[24] = {0};
				// PICKER DE COULEUR EN FENETRE MODALE (Rihen). La cle designe le
				// champ qui l'a ouvert ; la couleur vit ICI le temps du dialogue et
				// le champ la recopie a chaque frame, ce qui donne l'apercu en
				// direct. `colorOrig` permet d'annuler pour de bon.
				char colorOpen[40] = {0};
				float32 colorCur[3] = {1.f, 1.f, 1.f};
				float32 colorOrig[3] = {1.f, 1.f, 1.f};
				float32 colorAlpha = 1.f, colorAlphaOrig = 1.f;
				int32 colorTab = 0;		 // 0 = RVB, 1 = TSV
				int32 colorSpace = 1;	 // 0 = lineaire, 1 = perceptuel
				char colorHex[16] = {0}; // saisie hexadecimale
				// La TEINTE survit au geste : au noir comme au blanc, la conversion
				// depuis le RVB la perdrait et le curseur sauterait.
				float32 colorHsv[3] = {0.f, 0.f, 1.f};
				bool colorHsvValid = false;
				float32 colorModalX = 0.f, colorModalY = 0.f;
				bool colorModalPlaced = false;
				bool colorModalDrag = false;
				bool colorClosing = false;
				// LES COMBOS ECRIVENT A LA FRAME SUIVANTE, par un pointeur retenu
				// sur la valeur. Ce pointeur ne doit donc JAMAIS designer une
				// variable locale : elle est detruite avant que la liste ne soit
				// peinte, et le choix se perdait en silence. Les valeurs pilotees
				// par un combo vivent ici.
				int32 shadowQual = -1;	// -1 = pas encore lue depuis le moteur
				int32 shadowDynamic = 1; // 0 = calcul unique, 1 = recalcul continu
				// Capture demandee par le declencheur de la vue, consommee par la
				// boucle principale (qui seule connait la fenetre et le disque) :
				// 1 = « Vue 3D » (scene seule), 2 = « Tutoriel » (toute la
				// fenetre, interface comprise).
				int32 capturePending = 0;
				// Meme mecanique pour la VIDEO du tutoriel : l'interface demande,
				// la boucle principale execute -- elle seule connait la taille
				// reelle de la fenetre, indispensable pour ouvrir le fichier.
				// 1 = demarrer, 2 = arreter en gardant, 3 = abandonner.
				int32 tutoRecPending = 0;
				// Bouton-menu TUTORIEL du footer (Photo / Video) : la capture de
				// TOUTE l'application vit dans la barre de l'application, pas
				// dans la vue 3D.
				bool tutoMenuOpen = false;
				// Le bouton capture est un MENU : ouvert, il devoile les types
				// (Capture / Tutoriel...) et cliquer une entree EXECUTE la
				// capture correspondante (regle de Rihen).
				bool captureMenuOpen = false;
				int32 fogMode = 0;		 // 0 = lineaire, 1 = exponentiel
				// Source de l'ambiance : 0 couleur unie, 1 ciel procedural, 2 HDRI.
				int32 envSource = 0;
				// Modele de ciel : 0 degrade, 1 physique. Ici et pas en local —
				// cf. la regle ci-dessus, apprise une fois de plus le 02/08.
				int32 skyModel = 0;
				// Soleil que le ciel suit : 0 = manuel, i+1 = i-eme directionnelle
				// de la liste construite a la frame. Un RANG, pas un noeud : la
				// correspondance rang -> noeud est refaite a chaque image, et
				// c'est le noeud qui est conserve cote hote.
				int32 skySunSel = 0;
				char hdrPath[256] = {0};
				int32 hdrOk = 0; // 0 rien tente, 1 charge, -1 echec
				float32 colorDragDX = 0.f, colorDragDY = 0.f;
				// Chaque ONGLET DE SCENE garde sa pose de camera : cible, distance,
				// lacet, tangage, ortho. C'est ce qui rend les onglets FONCTIONNELS
				// aujourd'hui ; les objets par scene viendront avec le format projet.
				float32 sceneCamPose[8][6] = {}; // cible xyz, distance, lacet, tangage
				bool sceneCamOrtho[8] = {};
				bool sceneCamSet[8] = {};
				// Tab bar d'ESPACES au-dessus de la vue (Modelisation seul pour
				// l'instant) ; son en-tete s'escamote.
				bool wsBarOpen = true;
				float32 propScroll = 0.f;
				// Les trois SOUS-BLOCS du panneau Proprietes : replies/deplies et
				// chacun son defilement -- leur contenu peut etre tres long.
				// EN TABLEAUX : le panneau accueillera d'AUTRES categories de
				// proprietes (la table kSecs des ecrans) -- huit emplacements.
				// propOpen = ACTIVE par la pastille ; propFold = PLIE par le
				// chevron (l'en-tete reste, seul le contenu est recouvert).
				// UNE SEULE active a la fois (regle de Rihen) : au demarrage, la
				// categorie « Modele » -- c'est celle sur laquelle on travaille.
				bool propOpen[8] = {true};
				bool propFold[8] = {};
				bool AnyPropOpen() const {
					for (int32 i = 0; i < 8; ++i)
						if (propOpen[i])
							return true;
					return false;
				}
				float32 propScroll3[8] = {};
				// Hauteur CHOISIE de chaque section (0 = partage automatique) :
				// la poignee sous la section la regle.
				float32 propSecH[8] = {};
				// ── PANNEAU MATERIAU, facture Blender (capture de Rihen) ────
				// Une LISTE d'emplacements avec sa colonne + / - / menu, une
				// poignee de hauteur, puis la barre du navigateur et enfin les
				// proprietes du materiau SELECTIONNE. La liste remplace la pile
				// de groupes repliables : elle tient dans un coin d'ecran quel
				// que soit le nombre de materiaux.
				int32 projMatSel = 0;		 // ligne selectionnee dans la liste
				float32 projMatListH = 96.f; // hauteur de la liste (poignee)
				float32 projMatScroll = 0.f;
				// Le panneau des matcaps s'ancre au bouton qui l'a ouvert (barre de
				// la vue OU panneau Proprietes).
				NkRect matcapAnchor{};
				// Panneau d'AIMANTATION (cible + pas), ancre a son bouton de la
				// barre -- les pas se reglent DANS le panneau (regle de Rihen),
				// pas dans la barre.
				bool snapMenuOpen = false;
				NkRect snapMenuAnchor{};
				// Panneau de l'EDITION PROPORTIONNELLE (rayon + attenuation),
				// ancre a son chevron dans la barre de la vue.
				bool propMenuOpen = false;
				NkRect propMenuAnchor{};
				// NOMS PERSONNALISES de la hierarchie : 0..85 objets, 86..89
				// lumieres, 90..95 parents. Vide = nom genere. Ils vivent ici tant
				// que la demo n'a pas de champ nom ; le format projet les reprendra.
				char customNames[176][24] = {};
				// Cadenas et proportionnel des lignes de transformation.
				bool lockPos = false, lockRot = false, lockScl = false;
				// PROPAGER AUX ENFANTS : les proprietes communes editees sur un
				// parent s'appliquent aussi a ses descendants compatibles.
				bool matPropagate = false;
				// Verrou et proportionnel de la ligne DIMENSIONS.
				bool lockDim = false, propDim = false;
				// PROPORTIONNEL par ligne de transformation : l'axe touche propage
				// son rapport aux autres (delta quand la base est nulle).
				bool propPos = false, propRot = false, propScale = false;
				// PAS D'AIMANTATION modifiables depuis les proprietes de l'outil.
				float32 snapStepT = 0.5f, snapStepR = 15.f, snapStepS = 0.1f;
				// Groupes ouverts de la hierarchie (bit par groupe).
				uint32 hierOpen = 0xFFFFFFFFu;
				// ARBRE DE PARENTE : pliage par noeud (bit = PLIE), etat du
				// glisser-deposer, et EMPTY actif -- la selection d'un parent
				// SEUL (les empties n'existent pas dans la demo).
				// 5 mots = 160 bits : IL EN FAUT UN PAR NOEUD, et les objets de
				// l'utilisateur sont les noeuds 96 a 159. Avec 3 mots (96 bits),
				// `hierFold[node >> 5]` ecrivait HORS DU TABLEAU pour tout objet
				// cree par l'utilisateur -- donc dans `activeEmpty` (noeuds 96-127)
				// et dans `hierDragNode` (128-159) juste en dessous. Plier un
				// objet corrompait ainsi la selection et bloquait le glisser :
				// c'est la cause du pliage inoperant ET des objets impossibles a
				// selectionner (constate par Rihen).
				uint32 hierFold[5] = {};
				int32 activeEmpty = -1;
				int32 hierDragNode = -1;
				float32 hierDragX = 0.f, hierDragY = 0.f;
				bool hierDragging = false;
				bool hierMouseWasDown = false;
				// MENU CONTEXTUEL de la hierarchie (clic droit sur une ligne).
				// MESSAGE du pied de la hierarchie : explique un refus (verrou...).
				// Un clic sans effet passe sinon pour une panne.
				// PIVOT (origine) : verrou et proportionnel, comme les autres
				// lignes de transformation. Blender ne le laisse bouger qu'en mode
				// Edition ; ici il est aussi accessible en mode Objet, mais on peut
				// le figer (Rihen).
				bool lockPiv = false;
				bool propPiv = false;
				// Couleur de base : meme trio de commandes que les autres lignes.
				bool lockMat = false;
				bool propMat = false;
				bool lockLit = false;
				bool propLit = false;
				// GROUPES du panneau Modele (Transformation, Dimensions, Relations,
				// Materiaux...) : un bit par groupe, mis a 1 quand il est REPLIE.
				// Les elements de nature differente se rangent par groupe (Rihen).
				uint32 grpFold = 0;
				// ── MENU D'UN GROUPE DE PROPRIETES (facture Unity) ───────────
				// Chaque bandeau de groupe porte le meme petit menu a droite :
				// copier / coller / reinitialiser. Un SEUL etat pour toute
				// l'application -- un menu a la fois, et les groupes n'ont rien
				// a declarer pour l'avoir.
				char grpMenuKey[40] = {}; // groupe dont le menu est ouvert
				char grpMenuTitle[40] = {};
				NkRect grpMenuAnchor{};
				// ── PRESSE-PAPIERS DE PROPRIETES ────────────────────────────
				// Le groupe copie (sa cle) et ses VALEURS ; le collage n'est
				// propose que vers un groupe de MEME nature -- coller une
				// Transformation dans un Brouillard ne veut rien dire. Un
				// tampon generique : chaque groupe range ses champs dans
				// l'ordre qu'il veut, il est le seul a les relire.
				char grpClipKey[40] = {};
				float32 grpClipF[16] = {};
				int32 grpClipI[8] = {};
				bool grpClipHas = false;
				// ACTION DEMANDEE par le menu, consommee par le groupe concerne
				// a son tour de peinture : 0 aucune, 1 copier, 2 coller,
				// 3 reinitialiser. Le menu ne CONNAIT pas les proprietes -- il
				// pose l'intention, la categorie proprietaire l'execute.
				char grpActionKey[40] = {};
				int32 grpAction = 0;
				// ── PIPETTE DE RELATION (idee de Rihen, reprise de Blender) ──
				// Plutot que de chercher un objet dans une liste, on le DESIGNE :
				// on arme la pipette, puis on clique l'objet dans la vue ou dans
				// la hierarchie. 0 = inactive, 1 = choisir le parent, 2 = ajouter
				// un enfant. `pickFor` retient l'objet dont on edite les
				// relations, `pickPrev` la selection au moment de l'armement --
				// c'est son changement qui designe la cible.
				int32 relPick = 0;
				int32 relPickFor = -1;
				int32 relPickPrev = -1;
				// ── LISTES DU PANNEAU MODELE ────────────────────────────────
				// Groupes de vertex, shape keys, maps UV, attributs de couleur,
				// attributs : ces natures n'ont pas encore de modele de donnees
				// dans le moteur. On n'en INVENTE donc pas le contenu -- les
				// listes partent VIDES et ne contiennent que ce que
				// l'utilisateur cree lui-meme, rattache a SON objet.
				// kind : 0 groupe de vertex, 1 shape key, 2 map UV,
				//        3 attribut de couleur, 4 attribut.
				struct ListItem {
						uint8 kind = 0;
						int32 owner = -1; ///< noeud proprietaire
						char name[20] = {};
						float32 value = 1.f; ///< force / valeur, selon la nature
						bool on = true;
				};
				ListItem listItems[64] = {};
				int32 listCount = 0;
				char hierNote[96] = {};
				// SURCOUCHE BLOQUANTE : un menu est peint APRES les panneaux,
				// mais ceux-ci ont deja evalue leurs clics -- le clic sur une
				// entree de menu tombait AUSSI sur le widget du dessous (c'est
				// ainsi que « creer un enfant » verrouillait le parent : le clic
				// atteignait son cadenas). On memorise donc l'emprise des menus
				// d'une frame sur l'autre, et les panneaux l'ignorent.
				NkRect uiBlockCur{};
				NkRect uiBlockAcc{};
				bool uiBlockCurOn = false;
				bool uiBlockAccOn = false;
				void UiBlockAdd(const NkRect &b) {
					if (b.w <= 0.f || b.h <= 0.f)
						return;
					if (!uiBlockAccOn) {
						uiBlockAcc = b;
						uiBlockAccOn = true;
						return;
					}
					const float32 x0 = uiBlockAcc.x < b.x ? uiBlockAcc.x : b.x;
					const float32 y0 = uiBlockAcc.y < b.y ? uiBlockAcc.y : b.y;
					const float32 x1 = (uiBlockAcc.x + uiBlockAcc.w) > (b.x + b.w)
										   ? (uiBlockAcc.x + uiBlockAcc.w)
										   : (b.x + b.w);
					const float32 y1 = (uiBlockAcc.y + uiBlockAcc.h) > (b.y + b.h)
										   ? (uiBlockAcc.y + uiBlockAcc.h)
										   : (b.y + b.h);
					uiBlockAcc = {x0, y0, x1 - x0, y1 - y0};
				}
				bool UiBlocks(float32 mx, float32 my) const {
					return uiBlockCurOn && mx >= uiBlockCur.x && my >= uiBlockCur.y &&
						   mx < uiBlockCur.x + uiBlockCur.w &&
						   my < uiBlockCur.y + uiBlockCur.h;
				}
				void UiBlockFlip() { // debut de frame
					uiBlockCur = uiBlockAcc;
					uiBlockCurOn = uiBlockAccOn;
					uiBlockAccOn = false;
				}
				int32 hierMenuNode = -1;
				float32 hierMenuX = 0.f, hierMenuY = 0.f;
				// UNITES DE MESURE de la scene (0 metrique, 1 imperial, 2 aucun).
				int32 unitSystem = 0;
				int32 unitLength = 0;
				float32 unitScale = 1.f;
				// DIALOGUE DE CONFIRMATION de suppression : cibles memorisees a
				// la demande, le dialogue tranche (avec/sans les enfants).
				bool delAskOpen = false;
				bool delHasKids = false;
				int32 delNodeCount = 0;
				int32 delNodes[64];
				// Nom AFFICHE de la source du presse-papiers (pour « X.001 »).
				char clipName[24] = {};
				// Noeud en cours d'AJUSTEMENT de creation (panneau bas-droit).
				int32 addAdjustNode = -1;
				// Mode SOURCE (Couleur/Texture/Mix) de la lumiere en cours : le
				// popup du combo ecrit ici (adresse STABLE), resynchronise au
				// changement de lumiere.
				int32 lightSrcNode = -1;
				// Navigateur : menu contextuel des cartes + presse-papiers.
				int32 browMenuIdx = -1;
				float32 browMenuX = 0.f, browMenuY = 0.f;
				int32 browClip = -1;
				bool browClipCut = false;
				// Rect du navigateur (pose chaque frame) : les raccourcis y sont
				// routes vers les cartes plutot que vers la scene.
				NkRect browserRect{0.f, 0.f, 0.f, 0.f};
				// Glisser-deposer du navigateur (4 sens, facon Unreal).
				int32 browDragIdx = -1;
				float32 browDragX = 0.f, browDragY = 0.f;
				bool browDragging = false;
				bool browMouseWasDown = false;
				// Pliage de l'arbre (bit = plie), origine du drag, et carte
				// Copier/Deplacer du depot gauche -> droite.
				uint32 browFold[8] = {};
				bool browDragFromTree = false;
				int32 browAskIdx = -1;
				int32 browAskDest = -1;
				float32 browAskX = 0.f, browAskY = 0.f;
				bool browMenuCreat = false;
				NkRect viewRect{0.f, 0.f, 0.f, 0.f};
				// CONFLIT d'homonyme en attente (Renommer/Remplacer/Arreter).
				int32 browConfSrc = -1;
				int32 browConfDest = -1;
				bool browConfCopy = false;
				float32 browConfX = 0.f, browConfY = 0.f;
				// FILE des conflits rencontres pendant une fusion (un par un).
				int32 browConfQ[32][2] = {};
				uint32 browConfQCopy = 0;
				int32 browConfQN = 0;
				// HISTORIQUE de navigation (fleches) + fil d'Ariane.
				int32 browHist[64] = {};
				int32 browHistLen = 0;
				int32 browHistPos = 0;
				int32 browPrevFolder = -1;
				bool browHistNav = false;
				int32 lightSrcUi = 0;
				// Une scene AJOUTEE est VIERGE : les objets de la demo y sont masques.
				bool sceneBlank[8] = {};
				// Nature de l'onglet : 0 scene ; sinon 1+kind d'asset (EDITEUR
				// specialise ouvert au double-clic dans le navigateur).
				uint8 sceneTabKind[8] = {};
				int32 sceneTabAsset[8] = {}; // index navigateur + 1
				int32 editPreviewNode = 0;   // noeud+1 de la maquette d'editeur
				uint8 sceneTabId[8] = {};    // document hote STABLE par onglet
				int32 sceneIdNext = 1;       // 0 = scene d'ouverture (demo)
				int32 addParentNode = -1;    // parent impose au prochain Ajouter
				int32 sceneTabIsoNode[8] = {}; // noeud+1 ISOLE dans cet onglet
				uint8 sceneTabIsoHome[8] = {}; // document d'origine du noeud isole
				// CHAQUE SCENE A SES PROPRIETES (Rihen) : valeurs rangees par
				// onglet, echangees a l'activation.
				int32 unitSystemTab[8] = {};
				int32 unitLengthTab[8] = {};
				float32 unitScaleTab[8] = {};
				int32 propClipNode = 0; // noeud+1 source de 'Copier proprietes'
				int32 propCopyTarget = 0; // 0 = toutes les scenes, sinon 1+onglet
				uint8 browserSub[32] = {}; // sous-type des graphes (kind 0)
				bool browMenuGraph = false; // sous-menu Graphe ouvert
				// VUE CAMERA : noeud+1 regarde, et pose libre a restituer.
				int32 camViewNode = 0;
				bool camPickOpen = false;
				float32 camViewSave[6] = {};
				bool camViewSaveOrtho = false;
				bool camViewSaved = false;
				// TYPE de fond de la scene : 0 couleur unie, 1 degrade, 2 texture,
				// 3 HDRI, 4 ciel. Seule la couleur unie est cablee aujourd'hui ; les
				// autres montrent leurs proprietes en annoncant le chantier moteur.
				int32 bgType = 0;
				// LUMINOSITE du fond : assombrit ou eclaircit la couleur choisie
				// (prereglage ou personnalisee) sans en changer la teinte.
				float32 bgBrightness = 1.f;
				int32 matcapDragBar = -1; ///< -1 aucun, 0 verticale, 1 horizontale
				float32 matcapDragOff = 0.f;
				// ── Navigateur de projet : CONTENU CREE PAR L'UTILISATEUR ───────
				// Plus aucune donnee simulee : le navigateur nait vide et se remplit
				// par « + Dossier / + Materiau / + Texture ». Tableaux plats a
				// indices stables, comme partout ailleurs dans cet etat.
				static const int32 kMaxBrowser = 32;
				// Noeud SOURCE d'un asset reutilisable (0 = aucun, sinon noeud+1).
				int32 browserSrcNode[kMaxBrowser] = {};
				int32 browserCount = 0;
				uint8 browserKind[32] = {};	   ///< 0 dossier, 1 materiau, 2 texture
				char browserNames[32][32] = {};
				int32 browserParent[32] = {};  ///< -1 = racine
				int32 browserFolder = -1;	   ///< dossier ouvert (-1 = racine)
				NkVpAction pendingAction = NkVpAction::None;
				bool editingText = false;
				bool xray = false;
				float32 navLastX = 0.f, navLastY = 0.f;
				// Meme raison pour le gizmo : son deplacement se calcule a partir de la
				// position precedente, jamais d'un delta fourni tout fait.
				float32 gizLastX = 0.f, gizLastY = 0.f;
				bool gizWasDown = false;
				// Le geste appartient a la zone ou il a COMMENCE : ces deux drapeaux
				// empechent un glissement ne de l'interface de devenir une entree du
				// gizmo 3D en traversant la vue.
				bool gizGestureInView = false;
				bool gizWasMouseDown = false;
				int32 lastProjection = 0; ///< pour n'appliquer le combo que sur changement
				// Navigation par GLISSEMENT depuis les boutons de la vue et le gizmo :
				// -1 aucun, 0 loupe, 1 main, 2 gizmo de navigation. Le geste continue
				// meme si la souris quitte le bouton -- c'est le bouton ENFONCE qui
				// commande, pas la position.
				// Outil de selection par ZONE en cours : -1 aucun, 0 rectangle,
				// 1 lasso, 2 cercle. Il est ARME par une touche puis s'applique au
				// glissement -- c'est la mecanique de Blender (B, Ctrl+glisser, C).
				int32 zoneTool = -1;
				bool zoneActive = false; ///< glissement en cours
				float32 zoneX0 = 0.f, zoneY0 = 0.f, zoneX1 = 0.f, zoneY1 = 0.f;
				float32 zoneRadius = 40.f; ///< cercle : rayon en pixels (molette)
				static const int32 kMaxLasso = 128;
				int32 lassoCount = 0;
				float32 lasso[128 * 2] = {};
				// EDITION PROPORTIONNELLE : les sommets voisins suivent en
				// s'attenuant. Le rayon est en unites monde.
				bool proportional = false;
				float32 proportionalRadius = 1.f;
				// Recherche : un filtre par nom, commun a la hierarchie et au
				// navigateur. Vide = tout passe.
				char searchHier[32] = {};
				char searchBrowser[32] = {};
				// Pastilles de type du navigateur : un bit par kind de carte.
				// Zero = aucune pastille allumee = tout passe.
				uint32 browFilter = 0u;
				char searchProps[32] = {};
				int32 navDragMode = -1;
				float32 navDragLastX = 0.f, navDragLastY = 0.f;
				bool navDragging = false;
				int32 solidLight = 0; ///< eclairage du mode solide : studio / matcap / plat
				int32 selectMode = 2; ///< sommet / arete / face
				int32 addKind = 0;	  ///< primitive a ajouter
				int32 modKind = 0;	  ///< modificateur a ajouter
				int32 orientation = 0; ///< repere : monde / local / normal / vue
				int32 camSpeed = 2;	   ///< vitesse de deplacement de la camera

				// Sections repliables. Une seule pour l'instant ; il y en aura une par
				// section de panneau, et c'est deja la bonne forme.
				bool showTransform = true;

				// Valeurs de transformation. Elles vivent ici et non dans la peinture :
				// le glissement les modifie, elles doivent survivre a la frame.
				float32 pos[3] = {0.f, 0.f, 0.f};
				float32 rot[3] = {0.f, 0.f, 0.f};
				float32 scl[3] = {1.f, 1.f, 1.f};

				// Dossiers deplies du navigateur.
				bool folderOpen[8] = {true, true, false, false, false, false, false, false};

				// Scenes. UNE seule par defaut -- ouvrir sur deux scenes vides ferait
				// croire que l'une d'elles contient quelque chose.
				char sceneNames[8][32] = {"Scene", "", "", "", "", "", "", ""};
				int32 sceneCount = 1;

				// Noms modifiables de la hierarchie et des dossiers.
				char objectNames[8][32] = {"Scene", "Cube", "Sphere", "Groupe", "Roue", "Axe", "", ""};
				char folderNames[8][32] = {"MonProjet", "Maillages", "Animations", "Materiaux",
										   "Textures", "", "", ""};

				// Etats par objet de la hierarchie. Tableau fixe tant que la scene est
				// simulee ; il deviendra une propriete de l'objet reel.
				bool visible[8] = {true, true, true, true, false, true, true, true};
				bool locked[8] = {false, false, true, false, false, false, false, false};

				// Defilements. Etats de SESSION : ils doivent survivre a la frame.
				float32 scrollHier = 0.f, scrollProps = 0.f, scrollDetails = 0.f;
				float32 scrollTree = 0.f, scrollAssets = 0.f;
				// Defilements HORIZONTAUX : les noms longs debordent des qu'un panneau
				// est retreci, et sans eux la fin du nom est simplement perdue.
				float32 scrollHierH = 0.f, scrollPropsH = 0.f, scrollDetailsH = 0.f;
				float32 scrollTreeH = 0.f, scrollAssetsH = 0.f;

				// Menus ouverts. -1 = aucun. L'indice designe l'entree de la barre.
				int32 openMenu = -1;
				int32 hoverMenuItem = -1;
				int32 openSubMenu = -1; ///< sous-menu deploye dans le menu courant

				// ── PROPORTIONS AJUSTABLES ──────────────────────────────────────
				// Les separateurs modifient ces FRACTIONS et non des pixels : a la
				// prochaine ouverture, la disposition se retrouve identique quelle que
				// soit la taille de fenetre. En pixels, une fenetre plus petite
				// ecraserait les panneaux ; en fractions, ils suivent.
				// LARGEUR MINIMALE A LA PREMIERE OUVERTURE (Rihen) : un panneau
				// qui s'ouvre trop large mange la vue 3D, et l'utilisateur doit
				// le retrecir avant de travailler -- l'inverse du bon defaut.
				// Une fraction NULLE laisse Compute() appliquer son plancher
				// (kMinLeftW / kMinRightW) : on obtient donc la largeur minimale
				// sans dupliquer ces valeurs ici. Des que l'utilisateur bouge un
				// separateur, la fraction devient la SIENNE et se conserve --
				// fermer puis rouvrir retrouve sa largeur, pas le minimum.
				float32 leftFrac = 0.f;
				float32 rightFrac = 0.f;
				float32 browserFrac = 0.22f;
				float32 propsFrac = 0.45f; ///< part des proprietes dans la colonne de droite

				// Separateur en cours de glissement. -1 = aucun. On MEMORISE lequel :
				// sans cela, un glissement rapide qui sort du rectangle du separateur
				// le lacherait en pleine course.
				int32 dragSplitter = -1;
				float32 dragStart = 0.f;   ///< position souris au debut du glissement
				float32 dragStartFrac = 0.f;

				// ── PROJET ET ECRAN D'ACCUEIL ───────────────────────────────────
				// L'accueil est affiche TANT QU'AUCUN PROJET N'EST OUVERT. Il ne
				// vit pas dans une fenetre a part : c'est une surcouche opaque
				// posee sur l'application, qui continue de tourner dessous.
				bool welcome = true;
				float32 welcomeScroll = 0.f;
				// Boite « Nouveau projet » : un projet est un DOSSIER + un .nk3dm,
				// il faut donc un emplacement ET un nom, et montrer le chemin qui
				// en resulte -- sinon personne ne sait ou son travail atterrit.
				bool newProjOpen = false;
				char newProjName[64] = {};
				char newProjDir[260] = {};
				char projError[200] = {}; ///< derniere erreur, affichee telle quelle
				// DEMANDE D'ACTION PROJET, consommee APRES la frame -- meme patron
				// que capturePending / tutoRecPending. Les selecteurs de fichiers
				// de l'OS ouvrent une boucle modale : les appeler pendant la
				// peinture reentrerait dans la frame en cours.
				//   1 ouvrir la boite Nouveau · 2 Ouvrir... · 3 Enregistrer
				//   4 Enregistrer sous... · 5 Parcourir (dossier de la boite)
				//   6 Creer (validation de la boite) · 7 ouvrir un recent
				int32 projPending = 0;
				int32 projRecent = -1; ///< indice du recent a ouvrir (action 7)
				// « Enregistrer et quitter » : la sauvegarde a lieu apres la frame,
				// la fermeture doit donc attendre qu'elle ait reussi.
				bool quitAfterSave = false;

				// ── DOCUMENT ────────────────────────────────────────────────────
				// `dirty` decide s'il faut demander confirmation a la fermeture. Il
				// passe a vrai des qu'une action modifie la scene.
				bool dirty = true;
				bool askClose = false; ///< boite de confirmation affichee
				// ── FERMETURE PENDANT UNE PRISE OU UN ENCODAGE (Rihen) ─────────
				// Fermer tuerait un travail video en silence. On demande :
				// abandonner, finir puis fermer, ou continuer fenetre fermee.
				bool askCloseRec = false;
				// 0 = non ; 1 = fermer quand l'encodage finit ; 2 = daemon (la
				// fenetre est cachee, le processus vit jusqu'a la fin).
				int32 closeAfterEncode = 0;
				bool wantHideWindow = false; ///< consomme par la boucle
				// Demande de fermeture venue de l'OS (croix systeme) : consommee
				// par la peinture, qui applique les memes regles que la croix
				// dessinee -- deux chemins, une seule politique.
				bool wantClose = false;
				// ── NOTIFICATION DE FIN D'ENCODAGE (Rihen : « toujours ») ──────
				// Le meme genre de boite que la confirmation de fermeture : la
				// passe finale peut durer des minutes, sa fin merite un signal
				// franc, pas une ligne de journal.
				bool encodeDone = false;
				char encodeDonePath[300] = {};
				bool maximized = false;
				// Consommes par la boucle APRES la frame : BeginDragMove et Maximize
				// bloquent (boucle modale de l'OS), les appeler pendant la peinture
				// reentrerait dans la frame.
				bool wantDragMove = false;
				bool wantMinimize = false;
				bool wantMaxRestore = false;

				bool running = true;
		};

		// Forme de curseur demandee pour la frame. L'application la reporte a la
		// fenetre APRES la peinture : une zone dessinee tard peut ainsi corriger la
		// demande d'une zone dessinee tot, exactement comme pour le survol.
		enum class NkCursorWant : uint8 { Arrow = 0, Hand, ResizeWE, ResizeNS };

		// ── REGISTRE DE ZONES ───────────────────────────────────────────────────
		// Cle STABLE en texte plutot qu'un identifiant numerique : on lit
		// « hier.row.2 » dans un journal, pas « 4711 ». Le cout d'une comparaison de
		// chaine est negligeable devant le nombre de zones d'un ecran.
		class NkHitRegistry {
			public:
				// 1024 et non 256 : une ligne de hierarchie declare 5 zones et
				// une carte 3 -- le registre SATURAIT en silence, et tout ce qui
				// etait declare tard (les chevrons de pliage) devenait mort.
				static const uint32 kMax = 1024;

				void Begin(const nkgui::NkGuiInput &in) {
					mCount = 0;
					mMouse = in.mousePos;
					mDown = in.mouseDown[0];
					mClicked = in.mouseClicked[0];
					mRightClicked = in.mouseClicked[1];
					mWheel = in.wheel;
					// Bouton du MILIEU et modificateurs : la navigation 3D en depend, et
					// elle n'est pas un clic mais un GLISSEMENT -- c'est l'etat enfonce
					// qui compte, pas la transition.
					mMiddleDown = in.mouseDown[2];
					mDouble = in.mouseDoubleClicked[0];
					mShift = in.shiftDown;
					mCtrl = in.ctrlDown;
					mAlt = in.altDown;
					mHover[0] = 0;
					mHoverLayer = -1;
					mLayer = 0;
					mCursor = NkCursorWant::Arrow;
				}

				// Rend les evenements au registre SANS vider les zones deja
				// declarees. Sert quand une modale a prive les panneaux de tout
				// clic : les surcouches, peintes ensuite, doivent les retrouver.
				void Rearm(const nkgui::NkGuiInput &in) {
					mDown = in.mouseDown[0];
					mClicked = in.mouseClicked[0];
					mRightClicked = in.mouseClicked[1];
					mWheel = in.wheel;
					mMiddleDown = in.mouseDown[2];
					mDouble = in.mouseDoubleClicked[0];
				}

				// ── ROUTEUR D'OCCLUSION, SUR LE MODELE DE NKCODE ────────────────
				// Une surface flottante declare son EMPRISE et sa COUCHE pendant
				// qu'elle se dessine ; la liste lue est celle de la frame
				// PRECEDENTE, donc stable et INDEPENDANTE de l'ordre de dessin --
				// c'est ce qui manquait a la garde precedente, qui dependait de
				// l'endroit ou on la levait. Un point recouvert par une couche
				// strictement superieure a celle en cours devient inatteignable,
				// et cela vaut aussi bien pour les zones du registre que pour le
				// code qui teste la souris lui-meme, via Reachable().
				static const int32 kOcclMax = 16;
				void PushOcclusion(const NkRect &r, int32 layer) {
					if (mOcclNewCount < kOcclMax) {
						mOcclNew[mOcclNewCount] = r;
						mOcclNewLayer[mOcclNewCount] = layer;
						++mOcclNewCount;
					}
				}
				// A appeler une fois par frame : l'emprise accumulee devient celle
				// que tout le monde consulte.
				void FlipOcclusions() {
					for (int32 i = 0; i < mOcclNewCount; ++i) {
						mOccl[i] = mOcclNew[i];
						mOcclLayer[i] = mOcclNewLayer[i];
					}
					mOcclCount = mOcclNewCount;
					mOcclNewCount = 0;
				}
				bool Reachable(const NkVec2 &pt) const {
					for (int32 i = 0; i < mOcclCount; ++i)
						if (mOcclLayer[i] > mLayer && Contains(mOccl[i], pt))
							return false;
					return true;
				}
				// Le point survole est-il atteignable par la couche en cours ?
				bool ReachableAtMouse() const {
					return Reachable(mMouse);
				}

				// ── COUCHES : L'ETANCHEITE, UNE FOIS POUR TOUTES ────────────────
				// Un menu, un sous-menu, une fenetre modale sont peints tantot
				// AVANT tantot APRES les panneaux qu'ils recouvrent. Se fier a
				// l'ordre de declaration -- « la derniere zone gagne » -- rendait
				// donc l'etancheite dependante de l'ordre de peinture : une barre
				// declaree apres un menu lui volait ses clics, et un panneau
				// declare apres une modale la traversait.
				// Desormais chaque zone porte une COUCHE. Le survol revient a la
				// couche la plus HAUTE qui contient la souris, et a couche egale
				// a la derniere declaree. Ni les menus ni les modales n'ont plus
				// besoin de garde particuliere : elles montent de couche, et tout
				// ce qui est dessous devient aveugle sous elles.
				//   0   = panneaux
				//   50  = menus, listes deroulantes, sous-menus
				//   100 = fenetres modales
				void SetLayer(int32 layer) {
					mLayer = layer;
				}
				int32 Layer() const {
					return mLayer;
				}
				// Sentinelle : pose une couche a la construction, la rend a la
				// destruction. Impossible d'oublier de redescendre.
				struct LayerScope {
						NkHitRegistry &h;
						int32 prev;
						LayerScope(NkHitRegistry &r, int32 layer) : h(r), prev(r.Layer()) {
							h.SetLayer(layer);
						}
						~LayerScope() {
							h.SetLayer(prev);
						}
				};

				// Declare une zone sensible. Renvoie true si la souris est dessus --
				// ce qui permet d'ecrire directement `if (Add(...)) dessineSurvol();`.
				// ── CLIP DES ZONES ──────────────────────────────────────────────
				// La zone CLIQUABLE suit la zone DESSINEE : quand la peinture est
				// clippee (listes, sections a defilement), les zones le sont aussi.
				// Sans cela, un champ defile hors de vue restait saisissable -- les
				// « elements invisibles qu'on deplace » constates par Rihen.
				void PushClip(const NkRect &r) {
					mClip = r;
					mHasClip = true;
				}
				void PopClip() {
				mHasClip = false;
			}
				bool Add(const char *key, const NkRect &r) {
					if (mCount >= kMax || !key)
					return false;
					NkRect rr = r;
					if (mHasClip) {
						const float32 x0 = rr.x > mClip.x ? rr.x : mClip.x;
						const float32 y0 = rr.y > mClip.y ? rr.y : mClip.y;
						const float32 x1r = rr.x + rr.w < mClip.x + mClip.w ? rr.x + rr.w
																			 : mClip.x + mClip.w;
						const float32 y1r = rr.y + rr.h < mClip.y + mClip.h ? rr.y + rr.h
																			 : mClip.y + mClip.h;
						if (x1r <= x0 || y1r <= y0)
							return false; // entierement hors du clip : pas de zone
						rr = {x0, y0, x1r - x0, y1r - y0};
					}
					Copy(mKeys[mCount], key);
					mRects[mCount] = rr;
					mCount++;
					// Sous une surface d'une couche superieure : cette zone n'est
					// tout simplement pas atteignable.
					const bool inside = Contains(rr, mMouse) && Reachable(mMouse);
					// LA COUCHE LA PLUS HAUTE GAGNE, et a couche egale la derniere
					// declaree -- on peint du fond vers le dessus. Une zone posee
					// sous une surcouche ne prend donc jamais le survol, quel que
					// soit l'ordre dans lequel les deux ont ete declarees.
					if (inside && mLayer >= mHoverLayer) {
						Copy(mHover, key);
						mHoverLayer = mLayer;
					}
					// On ne renvoie « survole » que si la zone a REELLEMENT gagne :
					// sinon un bouton sous une modale s'allumerait au passage de la
					// souris alors qu'il ne repond pas.
					return inside && Eq(mHover, key);
				}

				bool IsHovered(const char *key) const {
					return Eq(mHover, key);
				}
				const char *Hovered() const {
					return mHover;
				}
				// ── SURCOUCHE BLOQUANTE, UNE FOIS POUR TOUTE L'APPLICATION ──────
				// Menus et listes deroulantes sont peints APRES les panneaux : quand
				// on clique dedans, le panneau du dessous a deja decide de SON clic.
				// Plutot que de garder chaque widget un a un -- et d'en oublier a
				// chaque ajout -- le registre refuse ICI tout clic tombant dans
				// l'emprise declaree. Les surcouches, elles, sont peintes apres
				// qu'on l'a levee, et repondent donc normalement.
				void SetBlock(const NkRect &b, bool on) {
					mBlock = b;
					mBlockOn = on;
				}
				bool BlockedAtMouse() const {
					return mBlockOn && Contains(mBlock, mMouse);
				}
				bool Clicked(const char *key) const {
					return mClicked && !BlockedAtMouse() && Eq(mHover, key);
				}
				bool RightClicked(const char *key) const {
					return mRightClicked && !BlockedAtMouse() && Eq(mHover, key);
				}
				bool Down(const char *key) const {
					return mDown && Eq(mHover, key);
				}
				// Bouton ENFONCE, independamment de la zone survolee : c'est ce qu'il
				// faut pour poursuivre un glissement quand la souris a quitte la zone.
				bool MouseDown() const {
					return mDown;
				}
				bool AnyClick() const {
					return mClicked;
				}

				// Molette appliquee a la zone survolee. Le decalage est BORNE ici et non
				// chez l'appelant : sans borne, on peut faire defiler un panneau court
				// dans le vide et croire qu'il est vide.
				bool Wheel(const char *key, float32 &offset, float32 contentH, float32 viewH) const {
					if (mWheel == 0.f || !Eq(mHover, key))
						return false;
					const float32 maxOff = contentH > viewH ? contentH - viewH : 0.f;
					offset -= mWheel * 40.f;
					if (offset < 0.f)
						offset = 0.f;
					if (offset > maxOff)
						offset = maxOff;
					return true;
				}

				// Molette par CONTENANCE, pas par survol exact : une liste est
				// RECOUVERTE par ses lignes -- le survol pointe la ligne, jamais la
				// liste, et exiger le survol exact rendait la molette morte des que
				// la liste etait pleine (constate par Rihen dans la hierarchie).
				bool WheelIn(const NkRect &area, float32 &offset, float32 contentH,
							 float32 viewH) const {
					if (mWheel == 0.f || mMouse.x < area.x || mMouse.y < area.y ||
						mMouse.x >= area.x + area.w || mMouse.y >= area.y + area.h)
						return false;
					const float32 maxOff = contentH > viewH ? contentH - viewH : 0.f;
					offset -= mWheel * 40.f;
					if (offset < 0.f)
						offset = 0.f;
					if (offset > maxOff)
						offset = maxOff;
					return true;
				}

				// Curseur voulu. La DERNIERE demande gagne, meme regle que le survol :
				// c'est ce qui est peint par-dessus qui commande.
				void WantCursor(NkCursorWant c) {
					mCursor = c;
				}
				NkCursorWant Cursor() const {
					return mCursor;
				}

				nkgui::NkVec2 Mouse() const {
					return mMouse;
				}

				// DOUBLE-CLIC sur une zone : NKGui fusionne la detection interne et
				// celle de l'OS, on ne fait que la relayer a la zone survolee.
				bool DoubleClicked(const char *key) const {
					return mDouble && Eq(mHover, key);
				}

				bool MiddleDown() const {
					return mMiddleDown;
				}
				bool ShiftDown() const {
					return mShift;
				}
				bool CtrlDown() const {
					return mCtrl;
				}
				bool AltDown() const {
					return mAlt;
				}
				// Molette BRUTE, sans zone associee. `Wheel(key, ...)` fait defiler une
				// liste et borne son resultat ; ici on veut la valeur pour zoomer, et
				// la borner n'aurait aucun sens.
				float32 WheelDelta() const {
					return mWheel;
				}

				static bool Contains(const NkRect &r, const nkgui::NkVec2 &p) {
					return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
				}

			private:
				static void Copy(char *dst, const char *src) {
					uint32 i = 0;
					for (; src[i] && i < 47; ++i)
						dst[i] = src[i];
					dst[i] = 0;
				}
				static bool Eq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				NkRect mBlock{};
				bool mBlockOn = false;
				char mKeys[kMax][48] = {};
				NkRect mRects[kMax] = {};
				uint32 mCount = 0;
				char mHover[48] = {};
				int32 mLayer = 0;		// couche des zones declarees maintenant
				int32 mHoverLayer = -1; // couche de la zone qui tient le survol
				NkRect mOccl[kOcclMax] = {};	// emprises de la frame precedente
				int32 mOcclLayer[kOcclMax] = {};
				int32 mOcclCount = 0;
				NkRect mOcclNew[kOcclMax] = {}; // celles qu'on accumule maintenant
				int32 mOcclNewLayer[kOcclMax] = {};
				int32 mOcclNewCount = 0;
				NkRect mClip{};
				bool mHasClip = false;
				nkgui::NkVec2 mMouse{0.f, 0.f};
				bool mDown = false, mClicked = false, mRightClicked = false;
				float32 mWheel = 0.f;
				bool mMiddleDown = false;
				bool mDouble = false;
				bool mShift = false, mCtrl = false, mAlt = false;
				NkCursorWant mCursor = NkCursorWant::Arrow;
		};

	} // namespace nk3d
} // namespace nkentseu
