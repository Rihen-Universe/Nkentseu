// =============================================================================
// NkDemo3D.cpp — PORTAGE INTEGRAL de renderdemo --demo=2 (Demo3D.cpp copie
// verbatim). Les adaptations sont balisees « PORTAGE NK3DModeler » : souris
// traduite fenetre->vue, gardes d'entree, frame rejouee (l'editeur possede
// le device et le command buffer). L'hote est en fin de fichier.
// Source : Applications/Sandbox/src/Demo/Demo3D.cpp — Demo 2
//
// Demo minimaliste 3D :
//   - Config ForGame (RENDER3D + RENDER2D + TEXT + SHADOW + POST_PROCESS + OVERLAY)
//   - Camera 3D orbite autour de l'origine
//   - Mesh primitives : sol (plane) + 4x4 sphere grid + cube central
//   - 1 lumiere directionnelle + 2 lumieres ponctuelles colorees
//
// Demontre le path complet : NkScene/Lights/DrawCalls -> Render3D::Submit
//                            -> RenderGraph -> Flush.
// =============================================================================
#include "NkDemoCommon.h"
#include "NKRHI/Commands/NkICommandBuffer.h" // portage : le graphe s'execute dans le cmd de l'editeur
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialCollection.h" // Upload() du BeginFrame rejoue
#include "NKGui/NkGuiRHIBackend.h" // hote : la cible hors ecran devient une texture d'interface
#include "NKWindow/Core/NkWESystem.h" // NkEvents()
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"	   // NkMouseWheelVerticalEvent, NkMouseButton
#include "NKEvent/NkEventDispatcher.h" // NkInput (IsMouseDown / MouseDeltaX/Y / IsKeyDown)
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Core/NkCameraController.h" // NkOrbitCameraController3D / NkFlyCameraController3D
#include "NKRenderer/Core/NkGizmo.h"			// NkGizmo3D (gizmo éditeur réutilisable)
#include "NKRenderer/Materials/NkMatcapLibrary.h" // noms des 30 matcaps (source unique)
#include "NKRenderer/Core/NkLightGizmo.h"   // widgets des lumieres (facon Blender)
#include "NKImage/NKImage.h"					// Phase H : test ecriture PNG procedural
#include "NKContainers/Associative/NkHashMap.h" // dedup arêtes Edit Mode
#include "NKRenderer/Mesh/NkEditMesh.h"			// structure demi-arête n-gon
#include "NKFileSystem/NkFile.h"				// save/load session d'édition (journal de commandes)
#include "NKTime/NkChrono.h"					// mesure du coût des aperçus modaux (NK_MODAL_PERF)
#include "NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h" // NK_GI_TEST : GI à un rebond
#include <cstdio>
#include <cstdlib> // getenv (override NK_GI_TEST)

namespace nkentseu {
	namespace demo {

		// ══ PORTAGE NK3DModeler ═══════════════════════════════════════════
		// La demo vivait PLEIN-FENETRE dans renderdemo ; ici elle vit dans le
		// panneau « vue 3D ». Ces cinq variables sont TOUTE la difference :
		// l'origine de la vue (traduction souris), sa taille, deux gardes
		// d'entree (saisie de texte en cours, souris hors du panneau) et le
		// command buffer de l'editeur pour la frame courante.
		static float32 nkvpOffX = 0.f, nkvpOffY = 0.f; // origine de la vue (px fenetre)
		static float32 nkvpW = 0.f, nkvpH = 0.f;	   // taille de la vue
		static bool nkvpInputOn = true;				   // faux pendant une saisie de texte
		static bool nkvpHover = false;				   // souris au-dessus de la vue
		static void *nkvpCmd = nullptr;				   // cmd de l'editeur (frame courante)
		static bool nkvpHudOn = true;				   // HUD texte de la demo (surimpression)
		// OEIL et CADENAS de la hierarchie : visibilite et verrou PAR OBJET.
		// La visibilite gate les soumissions de la demo ; le verrou bloque la
		// selection depuis la hierarchie et l'ecriture de transformation.
		static bool nkvpObjHidden[160] = {};
		static bool nkvpObjLocked[160] = {};
		static bool nkvpLightHidden[8] = {};
		static float32 nkvpFarOverride = 0.f;  // 0 = auto (dist*20+100) ; sinon la
											   // DISTANCE DE VUE choisie, independante
											   // des cameras de la scene
		static float32 nkvpOrthoScale = 0.55f; // demi-hauteur ortho = dist * ce facteur
		static int32 nkvpGridExtent = 20;	   // demi-etendue de la grille ortho (unites)
		static bool nkvpGridOn = true; // la VOLONTE de grille (la case du shell), meme quand
									   // l'ortho coupe la grille infinie du moteur
		static bool nkvpAxesOn = true; // axes debug +-1000 (bascule « Axes du plan »)
		static bool nkvpCursorTool = false;			   // outil CURSEUR : clic gauche = poser le curseur 3D
		static bool nkvpGizmoHidden = false;		   // outils Selection/Curseur : pas de poignees
		// ── PARENTE DE SCENE ────────────────────────────────────────────────
		// TOUT NOEUD peut etre parent ou enfant (regle de Rihen) : objets
		// (0..85), lumieres (86..89) et EMPTIES (90..95) -- les anciens groupes
		// d'affichage, devenus de vrais objets vides, invisibles au rendu.
		// -1 = racine. La transformation d'un parent est REPERCUTEE a son
		// sous-arbre par le detecteur de frame (HostHierarchyFrame) ; la
		// selection d'un parent ne selectionne PAS ses enfants.
		static constexpr int32 kNkvpMaxNodes = 160;
		static constexpr int32 kNkvpFirstEmpty = 90;
		static int32 nkvpParentOf[kNkvpMaxNodes];
		// MASQUE DE TRANSMISSION par parent : bit 1 position, bit 2 rotation,
		// bit 4 echelle. Une composante eteinte n'est PLUS propagee aux
		// enfants (option par transformation, idee de Rihen).
		static uint8 nkvpXmit[kNkvpMaxNodes];
		static bool nkvpParentInit = false;
		// Transform PROPRE des empties (ils n'existent pas dans la demo).
		// Transforms en TABLEAUX des noeuds 90..159 : 0..5 = empties, 6..69 =
		// OBJETS UTILISATEUR -- une seule plage, toute la machinerie (gizmo,
		// panneau, detecteur) est partagee.
		static float32 nkvpEmptyPos[70][3] = {};
		static float32 nkvpEmptyRotDeg[70][3] = {};
		static float32 nkvpEmptyScl[70][3] = {{1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}};
		// OBJETS UTILISATEUR : nature du slot (0 libre, 1 sphere, 2 cube,
		// 3 plan, 4 empty).
		static constexpr int32 kNkvpFirstUser = 96;
		static constexpr int32 kNkvpMaxUser = 64;
		static uint8 nkvpUserKind[kNkvpMaxUser] = {};
		// SUPPRESSION par drapeau (heritee par la visibilite effective).
		static bool nkvpDeleted[kNkvpMaxNodes] = {};
		// PRESSE-PAPIERS de noeud (copier/coller).
		static bool nkvpClipSet = false;
		static uint8 nkvpClipKind = 0;
		static float32 nkvpClipTRS[9];
		static float32 nkvpClipMat[5];
		// TAILLE LOCALE surchargee (ligne Dimensions) : decouplee de
		// l'echelle -- le FACTEUR ne s'applique qu'au rendu.
		static bool nkvpBaseSet[kNkvpMaxNodes] = {};
		static float32 nkvpBaseSize[kNkvpMaxNodes][3];
		static float32 nkvpDimFactor[kNkvpMaxNodes][3];
		// RACCOURCIS DE SCENE par POLLING (les evenements clavier ne livrent
		// pas les lettres au shell) : bits 1 dupliquer, 2 copier, 4 coller,
		// 8 supprimer, 16 parenter, 32 deparenter -- consommes une fois.
		static renderer::NkLightDesc nkvpUserLight[kNkvpMaxUser];
		static renderer::NkLightDesc nkvpClipLight;
		static int32 nkvpShortcutBits = 0;
		static uint8 nkvpShortcutPrev = 0;
		static void HostHierarchyFrame(); // detecteur (defini pres des accesseurs)
		static void HostParentEnsureInit();
		static void HostDecompose(const NkMat4f &M, NkVec3f &pos, NkVec3f &rotDeg, NkVec3f &scl);
		// VISIBILITE et VERROU EFFECTIFS : le sien OU celui d'un ancetre --
		// cacher/cadenasser un parent emporte tout son sous-arbre, mais chaque
		// enfant GARDE son propre drapeau, restaure au retour (regle de Rihen).
		static bool HostNodeHiddenOwn(int32 n) {
			if (n < 0 || n >= kNkvpMaxNodes)
				return false;
			if (nkvpDeleted[n])
				return true; // supprime = plus jamais rendu
			if (n >= 86 && n < 90)
				return nkvpLightHidden[n - 86];
			return nkvpObjHidden[n];
		}
		static bool HostHiddenEff(int32 n) {
			for (int32 g = 0; g < kNkvpMaxNodes && n >= 0; ++g) {
				if (HostNodeHiddenOwn(n))
					return true;
				n = nkvpParentOf[n];
			}
			return false;
		}
		static bool HostLockedEff(int32 n) {
			for (int32 g = 0; g < kNkvpMaxNodes && n >= 0; ++g) {
				if (n < 160 && nkvpObjLocked[n])
					return true;
				n = nkvpParentOf[n];
			}
			return false;
		}
		// ── SURCHARGES MATERIAU PAR OBJET (panneau Modele) ──────────────────
		// bit 1 teinte, bit 2 metallique, bit 4 rugosite. Le cache memorise les
		// valeurs EFFECTIVES vues a la soumission pour que le panneau les lise
		// sans connaitre les constantes internes de la demo.
		static uint8 nkvpMatMask[kNkvpMaxNodes] = {};
		static float32 nkvpMatTint[kNkvpMaxNodes][3];
		static float32 nkvpMatMetal[kNkvpMaxNodes];
		static float32 nkvpMatRough[kNkvpMaxNodes];
		static float32 nkvpMatCache[kNkvpMaxNodes][5];
		template <typename TDC>
		static void HostMatHook(int32 i, TDC &dc) {
			if (i < 0 || i >= kNkvpMaxNodes)
				return;
			if (nkvpMatMask[i] & 1) {
				dc.tint.x = nkvpMatTint[i][0];
				dc.tint.y = nkvpMatTint[i][1];
				dc.tint.z = nkvpMatTint[i][2];
			}
			if (nkvpMatMask[i] & 2)
				dc.metallic = nkvpMatMetal[i];
			if (nkvpMatMask[i] & 4)
				dc.roughness = nkvpMatRough[i];
			nkvpMatCache[i][0] = dc.tint.x;
			nkvpMatCache[i][1] = dc.tint.y;
			nkvpMatCache[i][2] = dc.tint.z;
			nkvpMatCache[i][3] = dc.metallic;
			nkvpMatCache[i][4] = dc.roughness;
			// DIMENSIONS decouplees de l'echelle : le facteur de taille locale
			// s'applique au RENDU seulement (l'echelle du panneau n'en sait
			// rien, et reciproquement -- regle de Rihen).
			if (nkvpBaseSet[i])
				dc.transform = dc.transform *
							   NkMat4f::Scale({nkvpDimFactor[i][0], nkvpDimFactor[i][1],
											   nkvpDimFactor[i][2]});
		}

		struct Demo3DState {
				NkMeshHandle meshSphere;
				NkMeshHandle meshPlane;
				NkMeshHandle meshCube;
				// ── NK_GI_TEST : mur mobile pour éprouver le GI à un rebond ──────
				// Bornes de base du mur ; `giWallOffset` s'y ajoute et le GI est
				// recalculé à chaque déplacement — c'est la démonstration que
				// l'indirect suit la géométrie au lieu d'être pré-cuit.
				static constexpr float32 kGIWallMin[3] = {-1.6f, 0.f, 2.2f};
				static constexpr float32 kGIWallMax[3] = {1.6f, 2.6f, 2.8f};
				bool giTest = false;
				bool giOn = true;
				bool giAuto = false;
				bool giDirty = true;
				float32 giIntensity = 1.f;
				float32 giPhase = 0.f;
				NkVec3f giWallOffset = {0.f, 0.f, 0.f};
				// Transform effective du mur (clavier + gizmo) réellement utilisée par
				// le dernier calcul de GI : sert à détecter qu'il a bougé.
				NkMat4f giWallXform = NkMat4f::Identity();
				float32 giBuildMs = 0.f;
				float32 giInjectMs = 0.f;
				NkTexHandle cookieWindow; // E.6 : cookie 2D pour le spot
				NkTexHandle cookieCube;	  // E.6b : cookie cube pour le point red
				// NkVSM v2 : panneau "feuillage" alpha-teste (validation ombre trouee).
				NkMaterial *maskedMat = nullptr;
				NkTexHandle maskedTex;
				float32 angle = 0.f;	  // orbite camera
				// Mode d'affichage viewport (touche Z, façon Blender) : 0=RENDERED (PBR éclairé),
				// 1=SOLID (unlit, phare caméra), 2=WIREFRAME (unlit + fil de fer).
				int32 shadingMode = 0;
				// Source de COULEUR en modes unlit (SOLID/WIREFRAME) et en EDIT MODE, façon
				// option "Color" du mode Solid de Blender. 0=MATERIAL (couleur du matériau),
				// 1=GRIS uniforme (défaut), 2=CUSTOM (couleur choisie). Touche B pour cycler.
				int32 unlitColorMode = 1;
				NkVec3f unlitGray = {0.38f, 0.39f, 0.42f}; // gris moyen-foncé façon Blender
				NkVec3f unlitCustom = {0.45f, 0.62f, 0.85f};
				// Vues axiales (pavé num 1/3/7 = front/right/top…) : façon Blender, on passe en
				// PROJECTION ORTHO. Une orbite libre (clic-milieu) rebascule en perspective.
				bool orthoView = false;
				// Option (touche G off? non — touche dédiée) : montrer la grille en vue axiale ortho.
				bool viewGridInOrtho = true;
				// Panel debug : index PCF mode courant pour cycle (P)
				int32 pcfIdx = (int32)NkPCFMode::PCF5x5;
				// Phase H : true si le PNG a ete charge avec succes (vs fallback procedural).
				bool phaseHLoadOk = false;
				// [ / ] maintenus : ajustent le bias en continu (taux x dt) via l'update
				// frame (le poll clavier NkInput est casse sur Win32 -> on tracke l'etat
				// presse/relache a la main).
				bool biasUpHeld = false;   // ] enfonce
				bool biasDownHeld = false; // [ enfonce
				// ── Caméras réutilisables du moteur (NkCameraController.h) ──
				// ÉDITEUR (Blender) : orbit = milieu ; pan = Shift+milieu ; zoom = molette.
				renderer::NkOrbitCameraController3D editorCam;
				// SIMULATION (jeu/archviz) : fly/FPS (WASD + regard clic-droit).
				renderer::NkFlyCameraController3D simCam;
				bool useSimCam = false;	  // F = bascule éditeur/simulation
				float64 wheelAccum = 0.0; // molette accumulée (callback -> frame)
				// ── Sélection + gizmo (composant moteur réutilisable NkGizmo3D) ──
				bool pickPending = false;	// front montant du clic gauche (callback)
				// DIAGNOSTIC DE SELECTION (NK_PICK_DIAG=1) : journalise, a chaque clic, le nombre
				// de candidats sommet/arete sous le curseur, l'arbitrage gizmo/maillage et l'element
				// elu -> permet de PROUVER (et non deviner) pourquoi un element visible n'etait pas
				// selectionnable sous un angle donne.
				bool pickDiag = false;
				// NK_PICK_AT="x,y" : clic de selection FORCE a ces pixels (capture headless, sans souris).
				bool pickForcePending = false;
				// NK_PICK_SCAN=1 : audit chiffre de selectabilite (ancienne vs nouvelle regle).
				bool pickScanPending = false;
				float32 pickForceX = 0.f, pickForceY = 0.f;
				int32 pickX = 0, pickY = 0; // position écran du clic (pixels)
				// Delta souris RÉEL par frame = (pos courante - pos précédente). NE PAS utiliser
				// NkInput.MouseDelta*() : ce delta d'événement n'est PAS remis à 0 sans mouvement
				// (valeur périmée conservée) -> le gizmo continuait de transformer souris immobile.
				float32 lastMouseX = 0.f, lastMouseY = 0.f;
				bool mouseTracked = false; // 1re frame : pas de delta
				// Indices des cibles : 16 sphères, 1 cube, 2 colonnes, 64 instanciés,
				// puis 3 éléments de décor longtemps NON sélectionnables — le sol (83),
				// le panneau feuillage alpha-testé (84) et le mur rouge du GI (85).
				// Ils étaient dessinés avec une transform EN DUR : aucun index gizmo, donc
				// ni clic ni déplacement possibles.
				static const int32 kIdxFloor = 83, kIdxFoliage = 84, kIdxGIWall = 85;
				static const int32 kNumObj = 16 + 1 + 2 + 64 + 3;
				renderer::NkGizmo3D gizmo; // sélection multiple + translate/rotate/scale/combiné
				// Mesh ÉDITÉ propre à un objet (persiste l'édition) : si valide, l'objet est
				// rendu avec CE mesh au lieu de sa primitive partagée. Rempli à la SORTIE d'edit.
					// ── LUMIERES PERSISTANTES (L1 etape 1) ──────────────────────────────
				// VERROU LEVE ICI. Les 4 lumieres etaient reconstruites EN DUR a chaque
				// frame dans Demo3D_Frame : tout deplacement au gizmo aurait ete ecrase a
				// l'image suivante. Coder le pick avant cette etape aurait donne une
				// manipulation qui marche a l'ecran et se reinitialise — pire que rien.
				// Elles vivent desormais dans l'ETAT : initialisees une fois, modifiables,
				// et seule leur animation reste optionnelle.
				static const int32 kNumLights = 4;
				renderer::NkLightDesc lights[kNumLights];
				bool lightsInit = false;
				// Le spot tournait via ctx.totalTime. On garde l'animation par DEFAUT (la
				// demo montre la projection dynamique du cookie), mais des que
				// l'utilisateur touche au spot elle DOIT s'arreter : sinon sa position
				// serait recalculee et son deplacement perdu a la frame suivante.
				bool spotAnimated = true;
				float32 spotAngle = 0.f; // phase courante, avancee seulement si animee
				NkMeshHandle objMesh[kNumObj]{};
				// AUTORITE TOPOLOGIQUE de l'objet edite : la structure demi-arete n-gon
				// TELLE QUELLE. Sans elle, on re-derivait la topologie depuis les TRIANGLES
				// du mesh de rendu (BuildFromIndexed + quadify), heuristique qui ne
				// reconstitue que les paires de triangles COPLANAIRES : une face creee avec F
				// (4 sommets non parfaitement coplanaires) ou tout n-gon a plus de 4 cotes
				// etait donc perdu -> sa diagonale de triangulation reapparaissait en fil de
				// fer. On conserve desormais la VRAIE topologie a la sortie d'edition, et on
				// la reprend telle quelle a la re-entree.
				renderer::NkEditMesh objHE[kNumObj];
				bool objHasHE[kNumObj] = {};
				// ── WIREFRAME N-GON (mode d'affichage fil de fer sans diagonales) ────
				// Le rasteriseur ne connait que des TRIANGLES : en fil de fer il trace donc la
				// diagonale de chaque quad. On construit a la place un BATCH PERSISTANT d'aretes
				// n-gon : les aretes d'une PRIMITIVE sont calculees UNE SEULE FOIS (BuildFromIndexed
				// + quadify + aretes uniques) puis re-emises, transformees, pour chacune de ses
				// instances. Le buffer GPU n'est reecrit que pour les objets dont la transform a
				// change (ici : le cube central anime) -> cout par frame quasi nul.
				NkMat4f objXform[kNumObj];        // transform MONDE capturee a la soumission
				NkVector<NkVec3f> wireSphere;     // aretes LOCALES de la primitive sphere (partagees)
				NkVector<NkVec3f> wireCube;       // aretes LOCALES de la primitive cube (partagees)
				NkVector<NkVec3f> wireCustom[kNumObj]; // aretes d'un objet au maillage EDITE
				NkVector<float32> wireVerts;      // batch MONDE : 7 float par vertex
				uint32 wireOff[kNumObj] = {};     // tranche de chaque objet (en vertices)
				uint32 wireCnt[kNumObj] = {};
				NkMat4f wireXform[kNumObj];       // transform utilisee lors du dernier remplissage
				uint32 wireTotalV = 0;
				bool wireDirty = true;            // reconstruire toute la table
				bool wireDiag = false;            // NK_WIRE_DIAG=1 : compare topologie exacte / re-devinee
				int32 wireStamp = -12345;         // signature topologique (objets edites)
				// ── Volet 2 : EDIT MODE mesh (édition façon Blender, sur l'OBJET SÉLECTIONNÉ) ──
				// TAB entre en édition de l'objet actif (gizmo.ActiveIndex). On CLONE ses
				// données CPU (modèle Blender : le CPU est l'autorité) en un mesh dynamique
				// propre -> éditer une sphère ne déforme pas les autres. Les vertices sont
				// édités en espace LOCAL ; l'ancre (transform monde de l'objet) sert au
				// rendu, au pick et aux marqueurs.
				NkMeshHandle editMesh;					 // clone dynamique du mesh édité
				NkVector<renderer::NkVertex3D> editRest; // vertices LOCAUX de repos (autorité CPU)
				NkVector<renderer::NkVertex3D> editLive; // rest + delta (re-upload pendant drag)
				NkVector<uint32> editIdx;				 // topologie (triangles)
				NkVector<uint32> editEdges;				 // arêtes UNIQUES (paires) pour la cage — bâti à l'entrée
				renderer::NkEditMesh editHE;			 // AUTORITÉ topologie (n-gon demi-arête)
				renderer::NkEditHistory editHistory;	 // undo/redo (mémento : snapshots de editHE)
				renderer::NkEditMesh editDragSnap;		 // pré-état capturé au DÉBUT d'un drag de sommets
				bool editDragSnapValid = false;
				renderer::NkMeshEditRecorder editRecorder; // journal des commandes (rejeu / modificateurs / IA)
				renderer::NkEditMesh editBase;			   // maillage de BASE (à l'entrée) pour le rejeu
				renderer::NkModifierStack editModifiers; // pile de modificateurs NON-DESTRUCTIFS (Mirror/Array/Subsurf)
				int32 editReplayStep = -1;				 // P : rejeu PAS-À-PAS (-1=off, 0=base, k=base+k commandes)
				bool editSavePending = false;			 // F5 : sauver la session (journal) sur disque
				bool editLoadPending = false;			 // F6 : charger une session + rejouer
				int32 editAddModPending = -1;			 // ajouter le modificateur de ce type
				// TYPE COURANT du menu « ajouter ». Il y a maintenant 17 modificateurs :
				// une touche par type serait ingerable, et Blender ne fait pas cela non
				// plus (il ouvre une liste). On cycle donc le type avec F8/F9 et on
				// l'ajoute avec F7 — F8/F9 gardent ainsi un role voisin de l'ancien.
				int32 editModTypePick = 0;
				bool editClearModPending = false;		 // F10 : vider la pile de modificateurs
				int32 editActiveMod = -1;				 // index du modificateur en cours de réglage
				// PARAMÈTRE courant du modificateur actif. Le réglage ne passe plus par un
				// `if (type == Mirror) … else if (type == Array) …` : il parcourt la TABLE
				// publiée par le modificateur. Ajouter un type de modificateur n'oblige donc
				// plus à revenir modifier la démo — et c'est la même table qu'un éditeur de
				// courbes utilisera pour proposer « quoi animer ».
				int32 editActiveParam = 0;
				int32 editModStackOp = 0;   // 1=monter 2=descendre 3=activer/désactiver
											// 4=dupliquer 5=retirer 6=appliquer
				bool editModParamCyclePending = false;
				int32 editModAdjustPending = 0;			 // [ / ] : ajuster (-1 / +1) le param principal
				bool editModCyclePending = false;		 // \ : changer de modificateur actif
				NkVector<renderer::NkEmId> editTriFace;	 // map triangle de rendu -> face n-gon (pick)
				NkVector<uint8> vertSel;				 // 1 = vertex sélectionné (taille = nb vertices)
				NkMat4f editAnchor = NkMat4f::Identity(); // transform monde de l'objet
				NkMat4f editAnchorInv = NkMat4f::Identity();
				// AABB LOCALE du mesh de RENDU édité (inclut le résultat des modificateurs).
				// Recalculée à chaque Demo3D_SyncFromHE ; transformée par editAnchor au moment
				// du draw call pour produire l'AABB MONDE attendue par NkRender3D::Submit.
				NkVec3f editLocalMin = {-1.f, -1.f, -1.f};
				NkVec3f editLocalMax = {1.f, 1.f, 1.f};
				int32 editObjIdx = -1;						 // objet en cours d'édition (index gizmo)
				NkVec3f editObjTint = {0.75f, 0.78f, 0.85f}; // matériau capturé de l'objet
				float32 editObjMetallic = 0.f;
				float32 editObjRoughness = 0.7f;
				int32 editSelMask = 1;			  // bits : 1=VERTEX 2=EDGE 4=FACE (touches 1/2/3 ; Shift+ = combiner)
				int32 editActiveVert = -1;		  // sommet ACTIF (dernier sélectionné) = rendu BLANC façon Blender
				// ── ÉLÉMENT ACTIF EN ARÊTE ET EN FACE ───────────────────────────────
				// Blender distingue TROIS états, pas deux : non sélectionné (noir),
				// sélectionné (orange) et ACTIF (blanc). L'actif n'est pas cosmétique :
				// c'est lui que visent « pivot = élément actif », les opérations « depuis
				// l'actif » et le futur Merge At Last. Le sommet actif existait déjà ;
				// l'arête et la face, non — en mode ARÊTE ou FACE on ne pouvait donc pas
				// savoir laquelle de plusieurs sélections servirait de référence.
				// L'arête est mémorisée par ses DEUX SOMMETS et non par un indice dans
				// `editEdges` : ce tableau est reconstruit à chaque changement de
				// topologie, un indice y deviendrait silencieusement faux.
				int32 editActiveEdgeA = -1, editActiveEdgeB = -1;
				int32 editActiveFace = -1; // identifiant de face n-gon (demi-arêtes)
				bool editXray = false;			  // Alt+Z : voir/sélectionner à travers (façon Blender)
				bool editMode = false;			  // TAB : bascule objet <-> édition
				bool editTogglePending = false;	  // TAB traité côté frame (accès meshSys)
				bool editWasDragging = false;	  // pour baker le delta en fin de drag
				bool editOverlayDirty = true;	  // reconstruire les buffers overlay (cage/points/faces)
				bool editExtrudePending = false;  // E : extrude région (traité côté frame)
				bool editDeletePending = false;	  // X : supprime faces (traité côté frame)
				bool editMergePending = false;	  // M : soude les vertices sélectionnés
				bool editMakeFacePending = false; // F : crée une face (n-gon) depuis la sélection
				bool editSubdivPending = false;	  // W : subdivise les faces sélectionnées
				bool editLoopCutPending = false;  // Ctrl+R : boucle d'arêtes (loop cut)
			int32 loopCuts = 1;				  // Ctrl+Shift+R : nb de boucles insérées (1..5)
				// ── BEVEL / CHANFREIN (façon Blender) ───────────────────────────
				// Ctrl+B = bevel d'ARÊTE · Ctrl+Shift+B = bevel de SOMMET.
				// Alt+B = cycle les SEGMENTS (1/2/3/4/6) · Alt+Shift+B = cycle la LARGEUR.
				// 0 = rien · 1 = bevel d'arête · 2 = bevel de sommet.
				int32 editBevelPending = 0;
				int32 bevelSegments = 1;   // 1 = chanfrein plat, N = arrondi
				float32 bevelOffset = 0.f; // 0 = AUTO (6 % de la diagonale de bbox)
				// ── INSET FACES (I) ─────────────────────────────────────────────
				// I = inset · Shift+I = bascule INDIVIDUEL / RÉGION · Alt+I = cycle la
				// profondeur (0 / creux / bossage), comme les propriétés d'outil Blender.
				bool editInsetPending = false;
				bool insetIndividual = true;
				float32 insetThickness = 0.f; // 0 = AUTO (8 % de la diagonale de bbox)
				float32 insetDepth = 0.f;
				// ── EDGE SPLIT / RIP (V) ────────────────────────────────────────
				// V = dé-soude les arêtes sélectionnées (déchirure). Shift+V = cycle
				// l'écartement (AUTO 1 % / 10 % / 25 % de la diagonale de bbox).
				bool editSplitPending = false;
				float32 splitGap = 0.f; // 0 = AUTO
				// ── SPIN / RÉVOLUTION (J) ───────────────────────────────────────
				// J = spin autour du CURSEUR 3D (comme Blender), axe vertical par défaut.
				// Shift+J = cycle les pas (6/12/24/32) · Alt+J = cycle l'angle (360/180/90).
				// ── DISSOLVE (Ctrl+X) ───────────────────────────────────────────
				// Contextuel comme Blender : dissout des SOMMETS / ARÊTES / FACES selon le
				// mode de sélection actif (1/2/3). 0 = rien, sinon 1 + mode (1..3).
				int32 editDissolvePending = 0;
				bool editSpinPending = false;
				int32 spinSteps = 12;
				float32 spinAngleDeg = 360.f;
				int32 spinAxis = 1; // 0=X 1=Y 2=Z
				bool spinDuplicate = false;
				// ── OUTILS DE SÉLECTION façon Blender ────────────────────────────
				// selTool : 0=aucun · 1=RECTANGLE (armé par B) · 2=LASSO (Ctrl+glisser) ·
				// 3=CERCLE (C, modal : on « peint » en maintenant le clic).
				int32 selTool = 0;
				bool selDragging = false;		  // tracé en cours
				// Front montant du clic gauche en MODE OBJET. Le mode édition a son propre
				// détecteur ; en mode objet il n'y en avait pas, faute d'outil qui en ait eu
				// besoin jusqu'ici (le gizmo gère son clic lui-même).
				bool prevLeftDownObj = false;
				float32 selX0 = 0.f, selY0 = 0.f; // origine du tracé (pixels écran)
				float32 selX1 = 0.f, selY1 = 0.f; // point courant
				NkVector<NkVec2f> selLasso;		  // contour libre (lasso)
				float32 selCircleR = 40.f;		  // rayon du cercle (pixels)
				int32 selMode = 0;				  // 0=remplacer · 1=ajouter (Shift) · 2=retirer (Ctrl)
				float32 lastWheel = 0.f;		  // molette de la frame (rayon du cercle)
				bool editUndoPending = false;	  // Ctrl+Z : annuler (traité côté frame)
				bool editRedoPending = false;	  // Ctrl+Shift+Z / Ctrl+Y : rétablir
				bool editReplayPending = false;	  // P : rejoue le journal depuis la base
				// Couteau/bisect (K) : trace une ligne (2 clics) -> plan de coupe.
				bool knifeArmed = false;
				bool knifeHasP0 = false;
				NkVec2f knifeP0 = {0.f, 0.f};
				bool editBisectPending = false; // couteau : plan prêt -> couper (côté frame)
				NkVec3f bisectPt = {0.f, 0.f, 0.f}, bisectN = {0.f, 1.f, 0.f};
				// Propriétés des outils (façon Blender) — réglées par Shift+touche, affichées au HUD.
				int32 subdivCuts = 1;			// nb d'itérations de subdivision (Shift+W)
				bool extrudeIndividual = false; // Extrude : région (0) vs faces individuelles (1) (Shift+E)
				int32 mergeMode = 0;			// 0=CENTER 1=FIRST 2=LAST (Shift+M)
				// ── OPERATIONS MODALES INTERACTIVES (façon Blender) ──────────────────
				// CADRE GENERIQUE, un seul mecanisme pour TOUTES les operations : on lance l'op,
				// on la PREVISUALISE en temps reel en bougeant la souris/la molette, puis clic
				// gauche = confirmer / Echap ou clic droit = annuler (retour a l'etat initial).
				// Principe : `modalSnap` est l'etat AVANT l'operation ; a chaque changement de
				// parametre on RESTAURE ce snapshot et on ré-applique l'op avec les parametres
				// courants -> l'apercu est toujours exact, jamais cumulatif. La confirmation
				// repart du meme snapshot et passe par Demo3D_ApplyCmd -> UN SEUL commit d'undo
				// et UNE SEULE entree de journal pour toute l'operation.
				//   modalOp : 0=aucune · 1=BEVEL ARETE · 2=BEVEL SOMMET · 3=INSET · 4=LOOP CUT ·
				//             5=SPIN · 6=EXTRUDE
				int32 modalOp = 0;
				int32 modalStartPending = 0;      // demande de lancement (callback clavier -> frame)
				bool modalCancelPending = false;  // Echap / clic droit
				renderer::NkEditMesh modalSnap;   // etat AVANT l'operation (source de tout apercu)
				NkVector<uint8> modalSelSnap;     // selection AVANT l'operation
				float32 modalVal = 0.f;           // parametre CONTINU pilote a la SOURIS
				float32 modalBase = 0.f;          // valeur au moment du lancement (ancre du drag)
				int32 modalSeg = 1;               // parametre ENTIER pilote a la MOLETTE
				float32 modalStartX = 0.f;        // position souris au lancement (pixels)
				float32 modalScale = 0.01f;       // conversion pixels -> unites du parametre
				bool modalDirty = true;           // les parametres ont change -> re-appliquer
				int32 modalLoopA = -1, modalLoopB = -1; // LOOP CUT : arete survolee (apercu de l'anneau)
				NkVector<uint32> modalSnapEdges; // aretes UNIQUES du snapshot (survol du loop cut)
				// TO SPHERE : centre (espace MAILLAGE) fige au lancement = pivot courant.
				NkVec3f modalCenterLocal = {0.f, 0.f, 0.f};
				int32 modalFrames = 0;           // frames ecoulees depuis le lancement
				bool modalEnvConfirm = false;    // NK_MODAL_CONFIRM=1 : confirme automatiquement
				// Pilote headless : NK_MODAL_OP / NK_MODAL_VAL / NK_MODAL_SEG forcent les
				// parametres au lancement (les ops modales n'ont pas de souris en capture).
				bool modalEnvHasVal = false, modalEnvHasSeg = false;
				float32 modalEnvVal = 0.f;
				int32 modalEnvSeg = 1;
				// ── CURSEUR VIRTUEL DE L'OP MODALE ──────────────────────────────────
				// Tant que l'op tourne, la souris lui est EXCLUSIVE (cf. le verrou unique dans
				// Demo3D_Frame) : on integre nous-memes ses deltas dans un curseur VIRTUEL.
				// Avantage decisif : le meme curseur accepte des deltas SYNTHETIQUES injectes
				// par NK_MODAL_DRAG -> le geste souris devient verifiable en headless.
				float32 modalCurX = 0.f, modalCurY = 0.f;
				// Dernier etat REELLEMENT applique a l'apercu : tant qu'il n'a pas bouge, on ne
				// reconstruit RIEN (le critere est la VALEUR, pas « la souris a bouge »).
				float32 modalAppliedVal = 0.f;
				int32 modalAppliedSeg = 0;
				int32 modalAppliedLoopA = -2, modalAppliedLoopB = -2;
				bool modalHasApplied = false;
				uint32 modalSnapVerts = 0, modalSnapFaces = 0; // compteurs AVANT l'op (preuve d'annulation)
				// Injection d'evenements souris synthetiques (headless) :
				//   NK_MODAL_DRAG="dx,dy" · NK_MODAL_DRAG_FRAMES=<n> · NK_MODAL_WHEEL=<crans>
				//   NK_MODAL_CANCEL=1
				bool modalInjDrag = false;
				float32 modalInjDX = 0.f, modalInjDY = 0.f;
				int32 modalInjFrames = 8;
				int32 modalInjWheel = 0;
				bool modalInjWheelDone = false;
				bool modalInjCancel = false;
				// Mesures (NK_MODAL_PERF=1) : cout des apercus, chemin COMPLET vs POSITIONS.
				bool modalPerf = false;
				bool modalForceFull = false; // NK_MODAL_FORCEFULL=1 : desactive le chemin allege
				int32 modalPvFull = 0, modalPvPos = 0;
				float64 modalPvFullMs = 0.0, modalPvPosMs = 0.0;
				renderer::NkGizmo3D editGizmo;	// 1 seule cible = PIVOT courant de la sélection
				// ── OMBRAGE FLAT / SMOOTH (façon Blender « Shade Flat / Shade Smooth ») ──
				// Shift+F = FLAT · Shift+S = SMOOTH. S'applique aux FACES SÉLECTIONNÉES si
				// la sélection en contient (mixte autorisé, comme Blender), sinon à TOUT le
				// maillage. 0 = rien à faire, 1 = passer en SMOOTH, 2 = passer en FLAT.
				int32 editShadePending = 0;
				// Dernier ombrage posé sur l'OBJET ENTIER (aucune face sélectionnée) : les
				// opérations qui RECONSTRUISENT la topologie (subdivide, loop cut, bisect…)
				// repartent de faces neuves (donc FLAT) — on ré-applique alors cet état pour
				// qu'un objet déclaré « smooth » le reste après une subdivision.
				bool editShadeSmoothAll = false;
				// ── CURSEUR 3D (façon Blender) ──────────────────────────────────────
				// Position MONDE, pivot possible (PIVOT_CURSOR), dessiné en permanence.
				// Placement : Shift + clic DROIT (raycast sous le curseur souris).
				NkVec3f cursor3D = {0.f, 0.f, 0.f};
				// Lumieres soumises cette frame (copie) : sert a dessiner leurs widgets
				// APRES le rendu de scene, et a les rendre selectionnables plus tard.
				NkVector<NkLightDesc> frameLights;
				int32 lightSel = -1;   // index de lumiere selectionnee (-1 = aucune)
				bool showLightGizmos = true;
				// GIZMO DEDIE AUX LUMIERES — un second NkGizmo3D plutot qu'un partage
				// avec celui des objets : les deux ensembles n'ont ni les memes indices,
				// ni les memes manipulations autorisees, et un clic doit trancher entre
				// « j'attrape une lumiere » et « j'attrape un objet ». Deux instances
				// rendent cette arbitration explicite au lieu de la cacher dans un
				// espace d'indices partage.
				renderer::NkGizmo3D lightGizmo;
				renderer::NkGizmo3D emptyGizmo; // poignees des EMPTIES (parente)
				bool emptyDragPrev = false;
				// `lights[]` reste la BASE, jamais ecrite par le gizmo ; l'effet du
				// gizmo est recompose a chaque frame par Demo3D_LightEffective. C'est
				// exactement le contrat des objets (base figee + Apply), et c'est ce qui
				// evite la derive : accumuler dans la base ferait grossir les erreurs
				// d'arrondi a chaque frame de drag.
				bool lightDragPrev = false;
				bool cursorPlacePending = false; // Shift+clic droit -> replacer le curseur 3D
				float32 cursorPX = 0.f, cursorPY = 0.f;
				// PILOTE HEADLESS : force l'application de la transform de groupe en Edit Mode
				// SANS souris (NK_EDIT_SCALE) -> capture de la différence entre pivots.
				bool editForceXform = false;
				// Le mesh GPU d'affichage est-il aligné 1:1 sur editRest/editLive ? (faux dès
				// qu'un modificateur tourne ou que l'ombrage FLAT a dédoublé des coins.)
				bool editDisplay1to1 = true;
				// Nombre de sommets du mesh GPU d'AFFICHAGE tel qu'il a ete CREE (dernier
				// Demo3D_SyncFromHE complet). Le chemin allege « positions seules » n'est
				// legitime que si la nouvelle triangulation d'affichage a EXACTEMENT ce
				// nombre de sommets -> meme topologie, seuls les sommets ont bouge.
				uint32 editDisplayVC = 0;
				NkVector<renderer::NkVertex3D> editDispScratch; // tampon reutilise (zero alloc/frame)
				NkVector<uint32> editDispScratchIdx;
				NkVector<renderer::NkEmId> editDispScratchTF;
		};

		// Transform de BASE (repos, sans animation) d'un objet de la démo par son index
		// gizmo — MÊME disposition que la boucle de soumission. Sert d'ancre à l'Edit Mode.
		static NkMat4f Demo3D_ObjBase(int32 idx) {
			if (idx >= 0 && idx < 16) {
				int32 row = idx / 4, col = idx % 4;
				return NkMat4f::Translate({(col - 1.5f) * 1.2f, 0.5f, (row - 1.5f) * 1.2f}) *
					   NkMat4f::Scale({0.45f, 0.45f, 0.45f});
			}
			if (idx == 16)
				return NkMat4f::Translate({0.f, 0.5f, 0.f}) * NkMat4f::Scale({0.6f, 0.6f, 0.6f}); // cube central (figé)
			if (idx == 17)
				return NkMat4f::Translate({-4.f, 1.f, -2.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}); // colonne 0
			if (idx == 18)
				return NkMat4f::Translate({1.f, 1.f, 4.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}); // colonne 1
			if (idx >= 19 && idx < 83) {
				int32 g = idx - 19, gz = g / 8, gx = g % 8;
				return NkMat4f::Translate({(gx - 3.5f) * 0.55f, 1.6f, (gz - 3.5f) * 0.55f - 4.5f}) *
					   NkMat4f::Scale({0.18f, 0.18f, 0.18f});
			}
			// Décor devenu sélectionnable — MÊMES transforms que les draw calls.
			if (idx == Demo3DState::kIdxFloor)
				return NkMat4f::Scale({40.f, 1.f, 40.f}); // sol 80x80
			if (idx == Demo3DState::kIdxFoliage)
				return NkMat4f::Translate({4.f, 1.6f, -1.f}) * NkMat4f::RotationY(NkAngle::FromRad(0.6f)) *
					   NkMat4f::Scale({1.6f, 1.2f, 0.03f}); // panneau alpha-testé
			if (idx == Demo3DState::kIdxGIWall) {
				const NkVec3f c{(Demo3DState::kGIWallMin[0] + Demo3DState::kGIWallMax[0]) * 0.5f,
								(Demo3DState::kGIWallMin[1] + Demo3DState::kGIWallMax[1]) * 0.5f,
								(Demo3DState::kGIWallMin[2] + Demo3DState::kGIWallMax[2]) * 0.5f};
				return NkMat4f::Translate(c) * NkMat4f::Scale({Demo3DState::kGIWallMax[0] - Demo3DState::kGIWallMin[0],
															   Demo3DState::kGIWallMax[1] - Demo3DState::kGIWallMin[1],
															   Demo3DState::kGIWallMax[2] - Demo3DState::kGIWallMin[2]});
			}
			return NkMat4f::Identity();
		}

		// Matériau (tint/metallic/roughness) d'un objet de la démo par son index — MÊMES
		// valeurs que la boucle de soumission. Sert à rendre le mesh ÉDITÉ avec le matériau
		// d'origine de l'objet (en RENDERED : couleur PBR de l'objet, pas un gris fixe).
		static void Demo3D_ObjMaterial(int32 idx, NkVec3f &tint, float32 &metallic, float32 &roughness) {
			if (idx >= 0 && idx < 16) {
				int32 row = idx / 4, col = idx % 4;
				tint = {(float32)col / 3.f, (float32)row / 3.f, 0.7f};
				metallic = (float32)col / 3.f;
				roughness = 0.05f + (float32)row / 3.f * 0.95f;
				return;
			}
			if (idx == 16) {
				tint = {1.f, 0.8f, 0.3f};
				metallic = 1.f;
				roughness = 0.15f;
				return;
			} // cube or
			if (idx == 17 || idx == 18) {
				tint = {0.7f, 0.7f, 0.7f};
				metallic = 0.f;
				roughness = 0.6f;
				return;
			}
			if (idx >= 19 && idx < 83) {
				int32 g = idx - 19, gz = g / 8, gx = g % 8;
				tint = {(float32)gx / 7.f, 0.6f, (float32)gz / 7.f};
				metallic = 0.f;
				roughness = 0.6f;
				return;
			}
			if (idx == Demo3DState::kIdxFloor) {
				tint = {0.12f, 0.12f, 0.13f};
				metallic = 0.f;
				roughness = 0.92f;
				return;
			}
			if (idx == Demo3DState::kIdxGIWall) {
				tint = {0.9f, 0.05f, 0.05f};
				metallic = 0.f;
				roughness = 0.85f;
				return;
			}
			tint = {0.75f, 0.78f, 0.85f};
			metallic = 0.f;
			roughness = 0.7f;
		}

		// ── ARETES N-GON D'UNE PRIMITIVE (cache local, calcule UNE fois) ─────────────
		// Reconstruit l'autorite demi-arete depuis les donnees CPU du mesh, QUADIFIE (ce
		// qui fusionne les paires de triangles en quads et fait disparaitre la diagonale),
		// puis extrait les aretes UNIQUES en espace LOCAL. Ces aretes servent ensuite a
		// TOUTES les instances de la primitive (une seule construction pour 16 spheres).
		static void Demo3D_NgonEdgesOf(renderer::NkMeshSystem *ms, NkMeshHandle h, NkVector<NkVec3f> &out) {
			out.Clear();
			if (!ms || !h.IsValid() || !ms->HasCPUData(h))
				return;
			const uint32 vc = ms->GetVertexCount(h), ic = ms->GetIndexCount(h);
			const auto *sv = (const renderer::NkVertex3D *)ms->GetVertices(h);
			const uint32 *si = ms->GetIndices(h);
			if (!sv || !si || vc == 0 || ic == 0)
				return;
			renderer::NkEditMesh em;
			em.BuildFromIndexed(sv, vc, si, ic, /*quadify*/ true);
			NkVector<uint32> eu;
			em.GetUniqueEdges(eu);
			out.Reserve((uint32)eu.Size());
			for (uint32 k = 0; k + 1 < (uint32)eu.Size(); k += 2) {
				out.PushBack(em.verts[eu[k]].pos);
				out.PushBack(em.verts[eu[k + 1]].pos);
			}
		}
		
		// Aretes n-gon EXACTES d'une structure demi-arete (aucune heuristique) : c'est la
		// topologie d'edition elle-meme, donc une face a N cotes donne N aretes, jamais la
		// diagonale de sa triangulation.
		static void Demo3D_NgonEdgesOfHE(const renderer::NkEditMesh &em, NkVector<NkVec3f> &out) {
			out.Clear();
			NkVector<uint32> eu;
			const_cast<renderer::NkEditMesh &>(em).GetUniqueEdges(eu);
			out.Reserve((uint32)eu.Size());
			for (uint32 k = 0; k + 1 < (uint32)eu.Size(); k += 2) {
				out.PushBack(em.verts[eu[k]].pos);
				out.PushBack(em.verts[eu[k + 1]].pos);
			}
		}

		// Aretes locales a utiliser pour un objet : son maillage EDITE s'il en a un,
		// sinon le cache partage de sa primitive (sphere pour 0..15, cube ailleurs).
		// PRIORITE ABSOLUE a la topologie demi-arete conservee (objHE) : c'est la seule
		// source qui connait les VRAIES faces n-gon. Le repli BuildFromIndexed+quadify ne
		// sert plus qu'aux primitives partagees (qui, elles, sont bien des quads plans).
		static const NkVector<NkVec3f> *Demo3D_WireSrc(Demo3DState *st, renderer::NkMeshSystem *ms, int32 i) {
			if (st->objMesh[i].IsValid()) {
				if (st->wireCustom[i].Empty()) {
					if (st->objHasHE[i]) {
						Demo3D_NgonEdgesOfHE(st->objHE[i], st->wireCustom[i]);
						// DIAGNOSTIC (NK_WIRE_DIAG=1) : montre noir sur blanc l'ecart entre la
						// VRAIE topologie n-gon et ce que l'ancienne re-derivation par
						// triangles+quadify retrouvait. Chaque arete en trop est une diagonale
						// de triangulation qui apparaissait a l'ecran en fil de fer.
						if (st->wireDiag) {
							NkVector<NkVec3f> guess;
							Demo3D_NgonEdgesOf(ms, st->objMesh[i], guess);
							logger.Info("[WireDiag] objet #{0} : {1} aretes n-gon EXACTES (topologie demi-arete) "
										"vs {2} aretes re-devinees (triangles + quadify) -> {3} diagonale(s) "
										"parasite(s) evitee(s)\n",
										i, (int32)(st->wireCustom[i].Size() / 2), (int32)(guess.Size() / 2),
										(int32)((guess.Size() - st->wireCustom[i].Size()) / 2));
						}
					} else
						Demo3D_NgonEdgesOf(ms, st->objMesh[i], st->wireCustom[i]);
				}
				return &st->wireCustom[i];
			}
			return (i < 16) ? &st->wireSphere : &st->wireCube;
		}
		
		// ── Couleur du fil de fer (mode d'affichage WIREFRAME), PARAMÉTRABLE ──────
		// Défaut calé sur Blender : en mode objet, le fil de fer y est un gris
		// SOMBRE, pas un blanc cassé. L'ancienne valeur {0.82,0.85,0.92} faisait
		// saturer les maillages denses (une sphère = ~2000 arêtes) en une masse
		// blanche illisible.
		// Réglable sans recompiler :
		//   NK_WIRE_COLOR="r,g,b"  composantes 0..1 (ex. "0.05,0.05,0.06" = quasi noir)
		//   NK_WIRE_ALPHA=<a>      opacité 0..1 (défaut 1)
		static NkVec4f gWireColor = {-1.f, 0.f, 0.f, 1.f}; // x<0 => pas encore résolu

		static const NkVec4f &Demo3D_WireColor() {
			if (gWireColor.x >= 0.f)
				return gWireColor;
			gWireColor = {0.28f, 0.29f, 0.33f, 1.f}; // gris sombre façon Blender
			if (const char *c = getenv("NK_WIRE_COLOR")) {
				float32 rgb[3] = {gWireColor.x, gWireColor.y, gWireColor.z};
				int32 k = 0;
				const char *p = c;
				while (k < 3 && *p) {
					rgb[k++] = (float32)atof(p);
					while (*p && *p != ',')
						p++;
					if (*p == ',')
						p++;
				}
				gWireColor.x = rgb[0];
				gWireColor.y = rgb[1];
				gWireColor.z = rgb[2];
			}
			if (const char *a = getenv("NK_WIRE_ALPHA"))
				gWireColor.w = (float32)atof(a);
			return gWireColor;
		}

		// Remplit la tranche d'un objet dans le batch MONDE (7 float par vertex).
		static void Demo3D_WireFillSlice(Demo3DState *st, int32 i, const NkVector<NkVec3f> *src) {
			const NkVec4f col = Demo3D_WireColor();
			const NkMat4f &X = st->objXform[i];
			float32 *dst = st->wireVerts.Data() + (uint64)st->wireOff[i] * 7;
			const uint32 n = st->wireCnt[i];
			for (uint32 k = 0; k < n; k++) {
				const NkVec3f w = X * (*src)[k];
				dst[k * 7 + 0] = w.x;
				dst[k * 7 + 1] = w.y;
				dst[k * 7 + 2] = w.z;
				dst[k * 7 + 3] = col.x;
				dst[k * 7 + 4] = col.y;
				dst[k * 7 + 5] = col.z;
				dst[k * 7 + 6] = col.w;
			}
		}
		
		// Synchronise le batch d'aretes n-gon avec la scene. Reconstruction COMPLETE
		// seulement quand la topologie change (un objet adopte un maillage edite, ou on
		// entre/sort d'edition) ; sinon on ne reecrit que les tranches des objets qui ont
		// REELLEMENT bouge (ici le seul cube central anime).
		static void Demo3D_SyncWireBatch(Demo3DState *st, renderer::NkRender3D *r3d, renderer::NkMeshSystem *ms) {
			int32 stamp = (st->editMode ? st->editObjIdx : -1) * 131 + 17;
			for (int32 i = 0; i < Demo3DState::kNumObj; i++)
				if (st->objMesh[i].IsValid())
					stamp += (i + 1) * 7;
			if (stamp != st->wireStamp) {
				st->wireStamp = stamp;
				st->wireDirty = true;
				for (int32 i = 0; i < Demo3DState::kNumObj; i++)
					if (!st->objMesh[i].IsValid())
						st->wireCustom[i].Clear();
			}
			if (st->wireSphere.Empty())
				Demo3D_NgonEdgesOf(ms, st->meshSphere, st->wireSphere);
			if (st->wireCube.Empty())
				Demo3D_NgonEdgesOf(ms, st->meshCube, st->wireCube);
			if (st->wireDirty) {
				uint32 off = 0;
				for (int32 i = 0; i < Demo3DState::kNumObj; i++) {
					st->wireOff[i] = off;
					st->wireCnt[i] = 0;
					if (st->editMode && st->editObjIdx == i)
						continue; // objet en edition : sa cage n-gon est deja dessinee par l'overlay
					const NkVector<NkVec3f> *src = Demo3D_WireSrc(st, ms, i);
					st->wireCnt[i] = (uint32)src->Size();
					off += st->wireCnt[i];
				}
				st->wireTotalV = off;
				st->wireVerts.Resize(off * 7);
				for (int32 i = 0; i < Demo3DState::kNumObj; i++) {
					if (!st->wireCnt[i])
						continue;
					Demo3D_WireFillSlice(st, i, Demo3D_WireSrc(st, ms, i));
					st->wireXform[i] = st->objXform[i];
				}
				r3d->SetNgonWireLines(st->wireVerts.Data(), off);
				st->wireDirty = false;
				logger.Info("[Demo3D] Wireframe n-gon : batch de {0} aretes ({1} vertices) pour {2} objets\n",
							off / 2, off, (int32)Demo3DState::kNumObj);
				return;
			}
			for (int32 i = 0; i < Demo3DState::kNumObj; i++) {
				if (!st->wireCnt[i] || st->wireXform[i] == st->objXform[i])
					continue;
				Demo3D_WireFillSlice(st, i, Demo3D_WireSrc(st, ms, i));
				st->wireXform[i] = st->objXform[i];
				r3d->UpdateNgonWireLines(st->wireVerts.Data() + (uint64)st->wireOff[i] * 7, st->wireOff[i],
										 st->wireCnt[i]);
			}
		}
		
		// ── Outils d'édition de topologie (Phase C) ──────────────────────────────────
		// Recalcule les normales par vertex = moyenne (pondérée par l'aire) des normales
		// de face. À appeler après toute déformation / changement de topologie.
		// ── Édition sur structure HALF-EDGE / n-gon (AUTORITÉ = editHE) ───────────────
		// Régénère le mesh de RENDU (triangulation de editHE) + cage + map tri->face n-gon,
		// et recrée le mesh GPU dynamique.
		static void Demo3D_SyncFromHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			// OMBRAGE : les commandes qui RECONSTRUISENT la topologie (subdivide, loop cut,
			// bisect…) recréent des faces neuves, donc FLAT par défaut. Si l'objet avait été
			// déclaré SMOOTH dans son ensemble, on ré-applique cet état ici -> un objet lissé
			// le reste après une subdivision (comportement attendu façon Blender).
			// (Un ombrage MIXTE posé face par face n'est PAS reconstitué après une op de
			// topologie : limite assumée — le flag vit sur la face, pas sur une couche
			// persistante indexée autrement.)
			if (st->editShadeSmoothAll && !st->editHE.AnyFaceSmooth())
				st->editHE.SetShadeSmooth(true, false);
			// BASE (editHE) -> pick + cage + overlay + sélection : on édite TOUJOURS la base,
			// même quand des modificateurs sont affichés (façon cage Blender : la cage = la base).
			st->editHE.Triangulate(st->editRest, st->editIdx, st->editTriFace);
			st->editLive = st->editRest;
			st->editActiveVert = -1; // la topologie a pu changer -> l'index actif n'est plus fiable
			st->editActiveEdgeA = st->editActiveEdgeB = -1;
			st->editActiveFace = -1;
			st->editHE.GetUniqueEdges(st->editEdges);
			if ((uint32)st->vertSel.Size() != st->editHE.VertCount())
				st->vertSel.Resize(st->editHE.VertCount());
			// AFFICHAGE SOLIDE = base, OU résultat des modificateurs (non-destructif : editHE
			// n'est jamais modifiée ; on triangule le résultat juste pour le mesh GPU affiché).
			// La triangulation d'AFFICHAGE est OMBRAGE-CONSCIENTE (TriangulateShaded) : les
			// coins des faces FLAT qui se disputent un sommet partagé y sont dédoublés, sans
			// quoi une sphère (dont les sommets sont partagés entre faces) ne pourrait JAMAIS
			// paraître facettée. La cage / le pick / la sélection, eux, restent sur la
			// triangulation 1:1 ci-dessus.
			renderer::NkEditMesh evalMesh;
			NkVector<renderer::NkVertex3D> evV;
			NkVector<uint32> evI;
			NkVector<renderer::NkEmId> evTF;
			if (!st->editModifiers.Empty()) {
				st->editModifiers.Evaluate(st->editHE, evalMesh);
				evalMesh.TriangulateShaded(evV, evI, evTF);
			} else {
				st->editHE.TriangulateShaded(evV, evI, evTF);
			}
			const renderer::NkVertex3D *dvData = evV.Data();
			uint32 dvCount = (uint32)evV.Size();
			const uint32 *diData = evI.Data();
			uint32 diCount = (uint32)evI.Size();
			// Le mesh d'affichage est-il encore aligné 1:1 sur la cage éditable ? Si oui, le
			// drag peut pousser directement editLive dans le VBO (chemin rapide) ; sinon
			// (modificateurs, ou coins dédoublés par l'ombrage FLAT) le solide n'est recalculé
			// qu'en FIN de drag — la cage, elle, suit toujours en temps réel.
			st->editDisplay1to1 = st->editModifiers.Empty() && (dvCount == (uint32)st->editRest.Size());
			st->editDisplayVC = dvCount; // reference du chemin allege « positions seules »
			// AABB LOCALE des vertices RÉELLEMENT affichés (base OU sortie des modificateurs).
			// Indispensable : NkRender3D::Submit fait son frustum culling sur dc.aabb, qui doit
			// être en espace MONDE -> on garde ici la boîte LOCALE et on la transforme par
			// l'ancre au moment du draw call.
			{
				NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
				for (uint32 i = 0; i < dvCount; i++) {
					const NkVec3f &p = dvData[i].pos;
					mn.x = NkMin(mn.x, p.x);
					mn.y = NkMin(mn.y, p.y);
					mn.z = NkMin(mn.z, p.z);
					mx.x = NkMax(mx.x, p.x);
					mx.y = NkMax(mx.y, p.y);
					mx.z = NkMax(mx.z, p.z);
				}
				if (dvCount == 0) {
					mn = {-1.f, -1.f, -1.f};
					mx = {1.f, 1.f, 1.f};
				}
				st->editLocalMin = mn;
				st->editLocalMax = mx;
			}
			if (st->editMesh.IsValid())
				ms->Release(st->editMesh);
			renderer::NkMeshDesc d =
				renderer::NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), dvData, dvCount, diData, diCount);
			d.dynamic = true;
			d.debugName = "Demo3D_EditMesh";
			st->editMesh = ms->Create(d);
			st->editOverlayDirty = true;
			// ── BATCH D'ARETES N-GON : PERIME DES QUE LA TOPOLOGIE CHANGE ──────────────
			// Le batch met en cache les aretes PAR OBJET. Un maillage edite a l'execution
			// (F, extrude, bevel, loop cut…) change de topologie : le cache doit tomber,
			// sinon le fil de fer affiche l'ancienne topologie (ou, apres persistance dans
			// l'objet, une topologie re-devinee depuis les triangles -> diagonales).
			// (Pas de `wireDirty = true` ici : tant qu'on EDITE, l'objet est EXCLU du batch —
			// sa cage n-gon est dessinee par l'overlay d'edition, deja reconstruit. Forcer
			// une reconstruction complete du batch a chaque apercu modal couterait cher pour
			// rien. La reconstruction est declenchee a la SORTIE d'edition, ou l'objet
			// reintegre le batch avec sa topologie n-gon reelle.)
			if (st->editObjIdx >= 0 && st->editObjIdx < Demo3DState::kNumObj)
				st->wireCustom[st->editObjIdx].Clear();
			// DIAGNOSTIC : etat topologique APRES chaque operation (le seul moment ou l'on
			// peut voir si une face n-gon a bien ete creee, ou si elle a ete triangulee).
			if (st->wireDiag) {
				uint32 nTri = 0, nQuad = 0, nNgon = 0;
				for (uint32 f = 0; f < (uint32)st->editHE.faces.Size(); f++) {
					if (!st->editHE.faces[f].alive)
						continue;
					const uint32 sz = st->editHE.FaceSize(f);
					if (sz == 3)
						nTri++;
					else if (sz == 4)
						nQuad++;
					else if (sz > 4)
						nNgon++;
				}
				logger.Info("[WireDiag] editHE apres sync : {0} tri / {1} quad / {2} n-gon | {3} aretes uniques "
							"(cage/fil de fer) | {4} triangles de rendu\n",
							nTri, nQuad, nNgon, (uint32)(st->editEdges.Size() / 2),
							(uint32)(st->editIdx.Size() / 3));
			}
		}

		// ── RE-SYNCHRO ALLÉGÉE : « LES SOMMETS ONT BOUGÉ, PAS LA TOPOLOGIE » ────────
		// Demo3D_SyncFromHE reconstruit TOUT et, surtout, DÉTRUIT puis RECRÉE le mesh GPU
		// (Release + Create) à chaque appel. Sur un aperçu modal, c'est une allocation
		// GPU PAR FRAME. Or les opérations purement GÉOMÉTRIQUES (To Sphere,
		// Shrink/Fatten, SLIDE de loop cut à nombre de coupes constant) laissent la
		// topologie INTACTE : mêmes faces, mêmes arêtes, même triangulation. On garde
		// alors le mesh GPU et on ne ré-uploade que les SOMMETS (UpdateVertices).
		// Économies : pas de Release/Create GPU, pas de GetUniqueEdges, pas de
		// redimensionnement de la sélection.
		// Retourne false si l'hypothèse ne tient pas (le nombre de sommets d'affichage a
		// changé) -> l'appelant doit repasser par le chemin complet.
		static bool Demo3D_SyncPosOnlyFromHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (!ms || !st->editMesh.IsValid() || st->editDisplayVC == 0)
				return false;
			if (st->editHE.VertCount() != (uint32)st->editRest.Size())
				return false; // la topologie a change -> chemin complet
			if (!st->editModifiers.Empty())
				return false; // avec modificateurs, la sortie n'est pas garantie stable
			// Cage / pick / overlay : la triangulation 1:1 est reconstruite (CPU pur, aucune
			// allocation GPU) — le contrat outV[i] == verts[i] garantit les mêmes tailles.
			st->editHE.Triangulate(st->editRest, st->editIdx, st->editTriFace);
			st->editLive = st->editRest;
			// Mesh d'AFFICHAGE : même triangulation ombrage-consciente, dans un tampon
			// réutilisé (aucune allocation par frame une fois chaud).
			st->editHE.TriangulateShaded(st->editDispScratch, st->editDispScratchIdx, st->editDispScratchTF);
			const uint32 dvCount = (uint32)st->editDispScratch.Size();
			if (dvCount != st->editDisplayVC)
				return false; // pas la même topologie d'affichage -> chemin complet
			ms->UpdateVertices(st->editMesh, st->editDispScratch.Data(), dvCount);
			st->editOverlayDirty = true;
			// Topologie identique, mais les SOMMETS ont bougé : le cache d'arêtes n-gon de
			// l'objet édité devient périmé (il sera recalculé à la sortie d'édition, seul
			// moment où l'objet réintègre le batch).
			if (st->editObjIdx >= 0 && st->editObjIdx < Demo3DState::kNumObj)
				st->wireCustom[st->editObjIdx].Clear();
			return true;
		}

		// Face n-gon d'un triangle de rendu — map EXACTE (produite par Triangulate).
		static renderer::NkEmId Demo3D_FaceOfTri(Demo3DState *st, uint32 triIdx) {
			return (triIdx < (uint32)st->editTriFace.Size()) ? st->editTriFace[triIdx] : renderer::NK_EM_INVALID;
		}

		// ── OCCLUSION REELLE (point unique, partagé par le PICK et le SURVOL) ───────
		// Le point MONDE `w` est-il caché par une face du maillage édité ? Lance un rayon
		// caméra -> point et cherche un triangle strictement DEVANT.
		//   • `skipA` / `skipB` : indices de sommets à IGNORER (les triangles qui touchent le
		//     candidat lui-même) — sans quoi un sommet de SILHOUETTE serait occulté par sa
		//     propre face et deviendrait incliquable. Passer -1/-1 quand le point n'est pas
		//     un sommet (milieu d'arête survolée) : la tolérance relative suffit alors.
		//   • Tolérance RELATIVE à la distance : les copies COINCIDENTES du même coin (les
		//     primitives dupliquent leurs sommets par face) touchent le candidat à tt ≈ dist
		//     et ne doivent pas compter comme occultantes.
		//   • X-ray ON : rien n'occulte (on voit et on sélectionne à travers, façon Blender).
		static bool Demo3D_PointOccluded(const Demo3DState *st, const NkVec3f &camPos, const NkVec3f &w, int32 skipA,
										 int32 skipB) {
			if (st->editXray)
				return false;
			NkVec3f dv = w - camPos;
			const float32 dist = dv.Len();
			if (dist < 1e-5f)
				return false;
			dv = dv * (1.f / dist);
			const float32 eps = NkMax(1e-4f, 3e-3f * dist);
			const uint32 rc = (uint32)st->editRest.Size();
			for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
				const int32 i0 = (int32)st->editIdx[t], i1 = (int32)st->editIdx[t + 1], i2 = (int32)st->editIdx[t + 2];
				if (i0 == skipA || i1 == skipA || i2 == skipA || i0 == skipB || i1 == skipB || i2 == skipB)
					continue;
				if ((uint32)i0 >= rc || (uint32)i1 >= rc || (uint32)i2 >= rc)
					continue;
				const NkVec3f v0 = st->editAnchor * st->editRest[i0].pos;
				const NkVec3f v1 = st->editAnchor * st->editRest[i1].pos;
				const NkVec3f v2 = st->editAnchor * st->editRest[i2].pos;
				NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = dv.Cross(e2);
				float32 aa = e1.Dot(h);
				if (fabsf(aa) < 1e-7f)
					continue;
				const float32 f = 1.f / aa;
				NkVec3f sv = camPos - v0;
				const float32 u = f * sv.Dot(h);
				if (u < 0.f || u > 1.f)
					continue;
				NkVec3f q = sv.Cross(e1);
				const float32 vv = f * dv.Dot(q);
				if (vv < 0.f || u + vv > 1.f)
					continue;
				const float32 tt = f * e2.Dot(q);
				if (tt > 1e-4f && tt < dist - eps)
					return true;
			}
			return false;
		}

		// ── P2 — ORIENTATION « NORMAL » DU GIZMO EN EDIT MODE (façon Blender) ────────
		// Calcule le repère de l'élément sélectionné et le pousse dans le gizmo :
		//   Z = normale, X = tangente (une arête de l'élément), Y = Z x X.
		//   • FACE   -> Z = normale de la face,       tangente = 1re arête de la face
		//   • ARÊTE  -> Z = moyenne des normales des faces adjacentes, tangente = direction
		//   • SOMMET -> Z = normale du sommet (moyenne des faces incidentes), tangente = une
		//               arête incidente
		//   • sélection MULTIPLE -> MOYENNE des normales (comme Blender).
		// Priorité FACE > ARÊTE > SOMMET selon le masque actif, avec repli si le niveau
		// prioritaire n'a rien de sélectionné. Tout est converti en espace MONDE (le gizmo
		// raisonne en monde) via l'ancre de l'objet édité.
		static void Demo3D_UpdateEditNormalFrame(Demo3DState *st) {
			auto &M = st->editHE;
			const NkVec3f org = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
			auto dirW = [&](NkVec3f d) { return (st->editAnchor * d) - org; };
			auto vsel = [&](uint32 i) { return i < (uint32)st->vertSel.Size() && st->vertSel[i] != 0; };
			NkVec3f n{0.f, 0.f, 0.f}, t{0.f, 0.f, 0.f};
			bool haveT = false;
			NkVector<renderer::NkEmId> fvn;
			// 1) FACES entièrement sélectionnées.
			if (st->editSelMask & 4) {
				for (uint32 f = 0; f < (uint32)M.faces.Size(); f++) {
					if (!M.faces[f].alive || !M.FaceIsSelected(f))
						continue;
					n = n + M.faces[f].normal;
					if (!haveT) {
						fvn.Clear();
						M.GetFaceVerts(f, fvn);
						if (fvn.Size() >= 2) {
							t = M.verts[fvn[1]].pos - M.verts[fvn[0]].pos;
							haveT = true;
						}
					}
				}
			}
			// 2) ARÊTES sélectionnées : normale = moyenne des faces adjacentes.
			if (n.Len() < 1e-6f) {
				for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
					const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
					if (!vsel(a) || !vsel(b))
						continue;
					renderer::NkEmId f0, f1;
					const uint32 nf = M.EdgeFaces(a, b, f0, f1);
					if (nf >= 1)
						n = n + M.faces[f0].normal;
					if (nf >= 2)
						n = n + M.faces[f1].normal;
					if (!haveT) {
						t = M.verts[b].pos - M.verts[a].pos;
						haveT = true;
					}
				}
			}
			// 3) SOMMETS sélectionnés : normale du sommet + une arête incidente.
			if (n.Len() < 1e-6f) {
				for (uint32 i = 0; i < M.VertCount(); i++) {
					if (!vsel(i))
						continue;
					n = n + M.verts[i].normal;
					if (!haveT) {
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
							if (a == i || b == i) {
								t = M.verts[b].pos - M.verts[a].pos;
								haveT = true;
								break;
							}
						}
					}
				}
			}
			if (n.Len() < 1e-6f) {
				st->editGizmo.ClearNormalFrame(); // rien d'exploitable -> repli Local/Global
				return;
			}
			if (!haveT)
				t = NkVec3f{1.f, 0.f, 0.f};
			st->editGizmo.SetNormalFrame(dirW(n), dirW(t));
		}

		// ── Pont sélection UI <-> COUCHE DE COMMANDES (editHE = moteur, ops paramétrées) ──
		// Les opérations d'édition (extrude/delete/merge/makeface/subdivide/loopcut/bisect)
		// vivent désormais dans NkEditMesh (moteur, sans dépendance UI/GPU) : base pour l'undo/
		// redo, les modificateurs et l'espace d'actions IA (NKAI). Ces wrappers poussent/
		// récupèrent la sélection du démo (st->vertSel) autour de l'appel, puis
		// Demo3D_SyncFromHE régénère le rendu (triangulation + cage + mesh GPU).
		static void Demo3D_PushSel(Demo3DState *st) {
			// SetVertSelection au lieu d'écrire `sel` à la main : c'est ce qui fait
			// vivre l'ORDRE DE SÉLECTION (Vert::selOrder), dont dépendent Merge At
			// First / At Last. L'ordre se déduit des TRANSITIONS, donc pousser le
			// tableau ENTIER à chaque synchronisation — ce que fait cette fonction —
			// n'écrase rien : seuls les changements réels consomment un rang.
			st->editHE.SetVertSelection(st->vertSel.Data(), (uint32)st->vertSel.Size());
			// Les primitives dupliquent leurs sommets PAR FACE : un coin cliqué n'est qu'UNE
			// des N copies coïncidentes. Sans propagation, la face/l'arête voisine ne se voit
			// pas sélectionnée ET la copie retenue peut appartenir à une face qui tourne le
			// dos à la caméra -> le marqueur orange serait masqué et « rien n'aurait l'air
			// sélectionné ». On étend donc la sélection à tous les sommets coïncidents.
			st->editHE.PropagateSelectionToCoincident();
		}

		static void Demo3D_PullSel(Demo3DState *st) {
			const uint32 n = st->editHE.VertCount();
			if ((uint32)st->vertSel.Size() != n)
				st->vertSel.Resize(n);
			for (uint32 i = 0; i < n; ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
		}

		// Normalise la sélection de l'UI après un pick (ou une sélection scriptée) : passe par
		// l'AUTORITÉ (editHE) pour l'étendre aux sommets coïncidents, puis la relit.
		static void Demo3D_NormalizeSel(Demo3DState *st) {
			Demo3D_PushSel(st);
			Demo3D_PullSel(st);
		}

		// ── LUMIERE EFFECTIVE = base + transform du gizmo ───────────────────────────
		// La manipulation d'une lumiere n'est PAS celle d'un objet : chaque poignee
		// doit correspondre a un parametre qui existe reellement. NkLightGizmo dit
		// lesquels (CanTranslate / CanRotate / ScaleMeaning) et on s'y tient :
		//   • directionnelle : sa position n'entre dans aucun calcul d'eclairage, donc
		//     la deplacer ne changerait strictement rien a l'image. On l'ignore.
		//   • ponctuelle : rayonnement isotrope, la tourner ne change rien non plus.
		//   • echelle : elle ne « grossit » pas une lumiere, elle regle sa PORTEE
		//     (point/spot) ou ses DIMENSIONS (area). Une directionnelle n'a ni l'une
		//     ni l'autre.
		// Appliquer une transformation sans effet serait pire qu'inutile : le widget
		// bougerait, l'image non — l'outil mentirait sur son propre resultat.
		static renderer::NkLightDesc Demo3D_LightEffective(const Demo3DState *st, int32 li) {
			renderer::NkLightDesc L = st->lights[li];
			using LG = renderer::NkLightGizmo;
			// Base = simple translation a la position de reference. Le gizmo accumule
			// par-dessus, la base ne bouge jamais.
			const NkMat4f base = NkMat4f::Translate(L.position);
			const NkMat4f m = st->lightGizmo.Apply(li, base);
			const NkVec3f o = m * NkVec3f{0.f, 0.f, 0.f};
			// TRANSLATION ET ROTATION TOUJOURS APPLIQUEES. Premiere version : elles
			// etaient filtrees par CanTranslate/CanRotate, au motif qu'une manipulation
			// sans effet « mentirait sur le resultat ». C'etait une erreur de modele.
			// Dans Blender une lampe EST un objet de la scene : on deplace un soleil pour
			// le RANGER (le sortir du champ, l'aligner sur un repere) meme si sa position
			// n'entre dans aucun calcul d'eclairage, et on tourne une ponctuelle parce que
			// tourner un objet est un geste universel. Refuser cassait deux choses :
			// l'uniformite du geste, et le placement du WIDGET lui-meme, qui restait
			// coince a sa position d'origine. Ce que le type change, c'est l'EFFET sur
			// l'image — dit une fois dans le journal — pas le DROIT de manipuler.
			L.position = o;
			{
				// Direction transformee par la seule partie LINEAIRE : on soustrait
				// l'origine transformee, ce qui annule la translation quelle que soit la
				// composition de la matrice (pas besoin d'une API MulDir dediee).
				const NkVec3f d0 = L.direction.Normalized();
				const NkVec3f t = (m * d0) - o;
				const float32 tl = t.Len();
				if (tl > 1e-5f)
					L.direction = t * (1.f / tl);
			}
			// Facteur d'echelle lu sur un axe : longueur de l'image de X. Uniforme ici,
			// car portee et dimensions se reglent proportionnellement.
			const NkVec3f ax = (m * NkVec3f{1.f, 0.f, 0.f}) - o;
			const float32 k = ax.Len();
			if (k > 1e-4f) {
				switch (LG::ScaleMeaning(L.type)) {
					case renderer::NkLightScaleMeaning::Range: L.range *= k; break;
					case renderer::NkLightScaleMeaning::Dimensions:
						L.areaWidth *= k;
						L.areaHeight *= k;
						break;
					default: break; // None : la directionnelle n'a rien a redimensionner
				}
			}
			return L;
		}

		// ── PROJECTION MONDE -> ÉCRAN, AUTONOME ─────────────────────────────────────
		// Même convention que le gizmo, mais SANS dépendre des variables locales du bloc
		// d'édition : c'est ce qui permet de servir le mode OBJET autant que le mode
		// ÉDITION avec un seul code. Auparavant la projection était une lambda capturant
		// le contexte d'édition, ce qui enfermait de fait les outils de sélection par
		// zone dans le mode édition.
		struct Demo3D_ScreenProj {
				NkVec3f camPos{}, fwd{}, rgt{}, upv{};
				float32 thX = 1.f, thY = 1.f, vw = 1.f, vh = 1.f;

				static Demo3D_ScreenProj Make(NkVec3f pos, NkVec3f target, float32 fovYDeg, float32 w, float32 h) {
					Demo3D_ScreenProj p;
					p.camPos = pos;
					p.fwd = (target - pos).Normalized();
					p.rgt = p.fwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
					p.upv = p.rgt.Cross(p.fwd).Normalized();
					p.thY = tanf(fovYDeg * 0.5f * 3.14159265f / 180.f);
					p.thX = p.thY * (h > 0.f ? (w / h) : 1.f);
					p.vw = w;
					p.vh = h;
					return p;
				}

				// false si le point est DERRIÈRE la caméra : sans ce test, un objet dans le
				// dos se projetterait à l'écran par symétrie et serait sélectionné par une
				// zone qui ne le contient pas visuellement.
				bool operator()(NkVec3f P, float32 &px, float32 &py) const {
					const NkVec3f v = P - camPos;
					const float32 zc = v.Dot(fwd);
					if (zc <= 1e-3f)
						return false;
					const float32 nx = v.Dot(rgt) / (zc * thX), ny = v.Dot(upv) / (zc * thY);
					px = (nx * 0.5f + 0.5f) * vw;
					py = (0.5f - ny * 0.5f) * vh;
					return true;
				}
		};

		// ── SÉLECTION PAR ZONE, MODE OBJET ──────────────────────────────────────────
		// Pendant de Demo3D_SelectInZone (qui opère sur les sommets du maillage édité).
		// Ici la cible est l'OBJET : on teste le centre de son transform monde.
		// mode : 0 = remplacer · 1 = ajouter (Shift) · 2 = retirer (Ctrl).
		// LIMITE ASSUMÉE : test sur le CENTRE, pas sur la silhouette. Un objet très
		// étendu dont le centre est hors zone ne sera pas pris, alors que Blender le
		// prendrait dès qu'un de ses pixels est dans la zone. Traiter la silhouette
		// demanderait de lire le masque de sélection — à faire si la gêne se manifeste.
		template <class InZone>
		static void Demo3D_SelectObjectsInZone(Demo3DState *st, int32 mode, InZone inZone,
											   const Demo3D_ScreenProj &proj) {
			if (mode == 0)
				st->gizmo.ClearSelection();
			for (int32 i = 0; i < (int32)Demo3DState::kNumObj && i < renderer::NkGizmo3D::kMax; i++) {
				// Centre monde = colonne de translation du transform de la frame.
				const NkMat4f &X = st->objXform[i];
				const NkVec3f c = X * NkVec3f{0.f, 0.f, 0.f};
				float32 px = 0.f, py = 0.f;
				if (!proj(c, px, py))
					continue;
				if (!inZone(px, py))
					continue;
				if (mode == 2) {
					// ToggleSelection sur un objet DÉJÀ sélectionné = le retirer, en
					// réaffectant l'actif si c'est lui qu'on enlève.
					if (st->gizmo.IsSelected(i))
						st->gizmo.ToggleSelection(i);
				} else
					st->gizmo.AddToSelection(i);
			}
		}

		// ── SÉLECTION PAR ZONE ÉCRAN (rectangle / lasso / cercle) ────────────────────
		// Cœur COMMUN aux 3 outils : seul le PRÉDICAT « ce point écran est-il dans la zone »
		// change. Parcourt les éléments selon le MODE ACTIF (V/E/F) :
		//   • VERTEX : le sommet est dans la zone ;
		//   • EDGE   : le MILIEU de l'arête est dans la zone (approximation Blender usuelle) ;
		//   • FACE   : le CENTRE de la face est dans la zone.
		// mode : 0 = remplacer · 1 = ajouter (Shift) · 2 = retirer (Ctrl).
		// Hors X-ray, seuls les éléments TOURNÉS VERS LA CAMÉRA sont touchés (Blender ne
		// sélectionne pas à travers le maillage sans X-ray).
		template <class InZone, class Project>
		static void Demo3D_SelectInZone(Demo3DState *st, int32 mode, NkVec3f camPos, InZone inZone, Project project) {
			const uint32 nv = (uint32)st->editRest.Size();
			if ((uint32)st->vertSel.Size() < nv)
				st->vertSel.Resize(nv);
			if (mode == 0)
				for (uint32 i = 0; i < nv; i++)
					st->vertSel[i] = 0;
			const NkVec3f orgW = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
			auto wpos = [&](uint32 i) { return st->editAnchor * st->editRest[i].pos; };
			// FILTRE « DOS-CAMERA » ASSOUPLI : un element de SILHOUETTE a une normale
			// ~perpendiculaire a la vue (produit scalaire NORMALISE ~= 0) ; avec le test
			// strict `> 0` il basculait au hasard du bruit numerique et disparaissait des
			// selections rectangle/lasso/cercle alors qu'il est parfaitement visible. On
			// tolere desormais une legere inclinaison vers l'arriere (-0.2), ce qui couvre
			// toute la bande de silhouette sans laisser passer les faces franchement
			// tournees vers l'arriere.
			auto faces = [&](NkVec3f w, NkVec3f nLocal) {
				if (st->editXray)
					return true;
				NkVec3f nW = (st->editAnchor * nLocal) - orgW;
				NkVec3f toCam = camPos - w;
				const float32 ln = nW.Len(), lv = toCam.Len();
				if (ln < 1e-6f || lv < 1e-6f)
					return true;
				return (nW.Dot(toCam) / (ln * lv)) > -0.2f;
			};
			auto hit = [&](NkVec3f w) {
				float32 px, py;
				return project(w, px, py) && inZone(px, py);
			};
			const uint8 on = (mode == 2) ? (uint8)0 : (uint8)1;
			auto apply = [&](uint32 vi) {
				if (vi < (uint32)st->vertSel.Size())
					st->vertSel[vi] = on;
			};
			if (st->editSelMask & 1) { // VERTEX
				for (uint32 i = 0; i < nv; i++)
					if (faces(wpos(i), st->editRest[i].normal) && hit(wpos(i)))
						apply(i);
			}
			if (st->editSelMask & 2) { // EDGE (par le milieu)
				for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
					const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
					if (a >= nv || b >= nv)
						continue;
					const NkVec3f wa = wpos(a), wb = wpos(b), mid = (wa + wb) * 0.5f;
					if (!faces(mid, st->editRest[a].normal) && !faces(mid, st->editRest[b].normal))
						continue;
					if (hit(mid)) {
						apply(a);
						apply(b);
					}
				}
			}
			if (st->editSelMask & 4) { // FACE (par le centre)
				NkVector<renderer::NkEmId> fvz;
				for (uint32 f = 0; f < (uint32)st->editHE.faces.Size(); f++) {
					if (!st->editHE.faces[f].alive)
						continue;
					fvz.Clear();
					st->editHE.GetFaceVerts(f, fvz);
					if (fvz.Size() < 3)
						continue;
					NkVec3f c{0.f, 0.f, 0.f};
					for (uint32 k = 0; k < (uint32)fvz.Size(); k++)
						c = c + wpos(fvz[k]);
					c = c * (1.f / (float32)fvz.Size());
					if (!faces(c, st->editHE.faces[f].normal))
						continue;
					if (hit(c))
						for (uint32 k = 0; k < (uint32)fvz.Size(); k++)
							apply(fvz[k]);
				}
			}
			Demo3D_NormalizeSel(st);
			st->editOverlayDirty = true;
		}

		// Point dans polygone (lancer de rayon horizontal, règle pair/impair) — lasso.
		static bool Demo3D_PointInPoly(const NkVector<NkVec2f> &poly, float32 px, float32 py) {
			bool in = false;
			const uint32 n = (uint32)poly.Size();
			for (uint32 i = 0, j = n - 1; i < n; j = i++) {
				const NkVec2f &A = poly[i], &B = poly[j];
				if (((A.y > py) != (B.y > py)) &&
					(px < (B.x - A.x) * (py - A.y) / ((B.y - A.y) != 0.f ? (B.y - A.y) : 1e-6f) + A.x))
					in = !in;
			}
			return in;
		}

		// ── BOUCLE D'ARÊTES / DE FACES (Alt+clic façon Blender) ─────────────────────
		// Rendue possible par la SOUDURE topologique (twins entre faces voisines).
		// add=false -> remplace la sélection ; add=true (Shift+Alt) -> l'ajoute.
		static void Demo3D_SelectLoop(Demo3DState *st, uint32 a, uint32 b, bool faceLoop, bool add) {
			const uint32 nv = (uint32)st->vertSel.Size();
			if (!add)
				for (uint32 i = 0; i < nv; i++)
					st->vertSel[i] = 0;
			if (faceLoop) {
				NkVector<renderer::NkEmId> loopF;
				st->editHE.GetFaceLoop(a, b, loopF);
				NkVector<renderer::NkEmId> fvl;
				for (uint32 k = 0; k < (uint32)loopF.Size(); k++) {
					fvl.Clear();
					st->editHE.GetFaceVerts(loopF[k], fvl);
					for (uint32 j = 0; j < (uint32)fvl.Size(); j++)
						if (fvl[j] < nv)
							st->vertSel[fvl[j]] = 1;
				}
				logger.Info("[Demo3D] Alt+clic : boucle de FACES -> {0} faces\n", (uint32)loopF.Size());
			} else {
				NkVector<uint32> loopE;
				st->editHE.GetEdgeLoop(a, b, loopE);
				for (uint32 k = 0; k + 1 < (uint32)loopE.Size(); k += 2) {
					if (loopE[k] < nv)
						st->vertSel[loopE[k]] = 1;
					if (loopE[k + 1] < nv)
						st->vertSel[loopE[k + 1]] = 1;
				}
				logger.Info("[Demo3D] Alt+clic : boucle d'ARETES -> {0} aretes\n", (uint32)(loopE.Size() / 2));
			}
			Demo3D_NormalizeSel(st);
			st->editOverlayDirty = true;
		}

		// Applique UNE commande d'édition (DONNÉE) : capture la sélection courante dans la
		// commande, snapshot le pré-état pour l'undo, exécute cmd.Apply, et SEULEMENT si la
		// géométrie a changé -> enregistre l'undo + PUSH la commande dans le journal (rejeu /
		// modificateurs / données IA) + resync. Point d'entrée unique des éditions.
		static bool Demo3D_ApplyCmd(Demo3DState *st, renderer::NkMeshSystem *ms, renderer::NkMeshEditCommand cmd) {
			cmd.selection.Clear(); // sélection = donnée rejouable
			for (uint32 i = 0; i < (uint32)st->vertSel.Size(); ++i)
				if (st->vertSel[i])
					cmd.selection.PushBack(i);
			Demo3D_PushSel(st);
			renderer::NkEditMesh snapshot = st->editHE; // pré-état (avec sélection live)
			if (!cmd.Apply(st->editHE))
				return false; // no-op -> ni undo ni enregistrement
			st->editHistory.Commit(snapshot);
			st->editRecorder.Push(cmd); // journalise la commande
			st->editReplayStep = -1;	// une édition sort du mode rejeu
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
			return true;
		}

		// EXTRUDE (E) — COMPORTEMENT BLENDER EXACT :
		//   • extrude selon le MODE DE SÉLECTION ACTIF : FACE (bit 4) > EDGE (bit 2) > VERTEX
		//     (bit 1) — face = cap + quads latéraux, arête = quad reliant, sommet = arête fil ;
		//   • OFFSET ZÉRO : la nouvelle géométrie naît EXACTEMENT sur l'originale ;
		//   • elle devient la SÉLECTION ; elle n'est PAS déplacée.
		// L'utilisateur la déplace ensuite lui-même au gizmo (G/R/S), le long de la normale
		// (orientation Normal câblée, cf. Demo3D_UpdateEditNormalFrame) ou contrainte X/Y/Z.
		// Aucun auto-move au runtime. (Shift+E bascule région <-> individuel, faces seulement.)
		static void Demo3D_ExtrudeHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			if (st->editSelMask & 4)
				c.op = renderer::NkMeshEditOp::Extrude; // FACES
			else if (st->editSelMask & 2)
				c.op = renderer::NkMeshEditOp::ExtrudeEdges; // ARÊTES
			else
				c.op = renderer::NkMeshEditOp::ExtrudeVerts; // SOMMETS
			c.extrude.individual = st->extrudeIndividual;
			c.extrude.offset = 0.f; // Blender : zéro déplacement, l'utilisateur bouge ensuite
			// Repli : en mode FACE sans aucune face entièrement sélectionnée, l'extrusion de
			// faces est un no-op -> on retombe sur l'extrusion d'arêtes puis de sommets, comme
			// Blender qui extrude « ce qui est sélectionné ».
			if (Demo3D_ApplyCmd(st, ms, c))
				return;
			if (c.op == renderer::NkMeshEditOp::Extrude) {
				c.op = renderer::NkMeshEditOp::ExtrudeEdges;
				if (Demo3D_ApplyCmd(st, ms, c))
					return;
			}
			if (c.op != renderer::NkMeshEditOp::ExtrudeVerts) {
				c.op = renderer::NkMeshEditOp::ExtrudeVerts;
				Demo3D_ApplyCmd(st, ms, c);
			}
		}

		// DELETE (X) : supprime les faces sélectionnées, compacte — cf. NkEditMesh.
		static void Demo3D_DeleteHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Delete;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// MERGE (M) : soude les sommets sélectionnés (center/first/last, Shift+M) — cf. NkEditMesh.
		static void Demo3D_MergeHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Merge;
			c.merge.mode = (int32)st->mergeMode;
			// AT CURSOR : le curseur 3D vit en MONDE, le merge opere en espace
			// MAILLAGE -> on le ramene par l'ancre d'edition (meme convention que
			// le spin et le pivot curseur).
			c.merge.point = st->editAnchorInv * st->cursor3D;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// CREATE FACE (F) : une face n-gon depuis les sommets sélectionnés — cf. NkEditMesh.
		static void Demo3D_MakeFaceHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::MakeFace;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// SUBDIVIDE (W) : Catmull-Clark, faces sélectionnées ou TOUT. subdivCuts passes en UNE
		// commande (donc UN seul undo) — cf. NkEditMesh.
		static void Demo3D_SubdivideHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Subdivide;
			c.subdiv.cuts = st->subdivCuts;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// LOOP CUT (Ctrl+R) : anneau de quads depuis l'arête sélectionnée — cf. NkEditMesh.
		// st->loopCuts = nombre de boucles insérées (Ctrl+Shift+R cycle 1..5, façon molette).
		static void Demo3D_LoopCutHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::LoopCut;
			c.loopcut.cuts = st->loopCuts;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// BEVEL / CHANFREIN (Ctrl+B arête · Ctrl+Shift+B sommet) — cf. NkEditMesh.
		// Les paramètres (largeur, segments) vivent dans l'état de la démo, façon « propriétés
		// d'outil » Blender : Alt+B cycle les segments, Alt+Shift+B cycle la largeur.
		static void Demo3D_BevelHE(Demo3DState *st, renderer::NkMeshSystem *ms, bool vertexMode) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Bevel;
			c.bevel.offset = st->bevelOffset;
			c.bevel.segments = st->bevelSegments;
			c.bevel.vertexOnly = vertexMode;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// INSET FACES (I) — face plus petite à l'intérieur des faces sélectionnées.
		static void Demo3D_InsetHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Inset;
			c.inset.thickness = st->insetThickness;
			c.inset.depth = st->insetDepth;
			c.inset.individual = st->insetIndividual;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// EDGE SPLIT (V) — dé-soude les arêtes sélectionnées (déchirure) — cf. NkEditMesh.
		static void Demo3D_EdgeSplitHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::EdgeSplit;
			c.esplit.gap = st->splitGap;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// DISSOLVE (Ctrl+X) — retire l'élément SANS trouer (fusion en n-gon), contrairement
		// à X = supprimer. Le mode suit la sélection active : FACE (bit 4) > EDGE (bit 2) >
		// VERTEX (bit 1), exactement comme le Ctrl+X contextuel de Blender.
		static void Demo3D_DissolveHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Dissolve;
			c.dissolve.mode = (st->editSelMask & 4) ? 2 : ((st->editSelMask & 2) ? 1 : 0);
			Demo3D_ApplyCmd(st, ms, c);
		}

		// SPIN / RÉVOLUTION (J) — le profil sélectionné tourne autour du CURSEUR 3D.
		// Centre et axe sont donnés en MONDE (le curseur 3D l'est), la matrice editAnchor
		// (modèle->monde) permet à NkEditMesh de les ramener en local — même schéma que
		// le bisect. Façon Blender : le curseur 3D est le centre par défaut du spin.
		static void Demo3D_SpinHE(Demo3DState *st, renderer::NkMeshSystem *ms) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Spin;
			c.spin.center = st->cursor3D;
			c.spin.axis = (st->spinAxis == 0)   ? NkVec3f{1.f, 0.f, 0.f}
						  : (st->spinAxis == 2) ? NkVec3f{0.f, 0.f, 1.f}
												: NkVec3f{0.f, 1.f, 0.f};
			c.spin.angle = st->spinAngleDeg * 0.01745329f;
			c.spin.steps = st->spinSteps;
			c.spin.duplicate = st->spinDuplicate;
			c.spinXform = st->editAnchor;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// KNIFE / BISECT (K) : coupe par un PLAN (tracé écran -> monde). Le plan est en espace
		// MONDE : on passe editAnchor (modèle->monde) à la commande — cf. NkEditMesh.
		static void Demo3D_BisectHE(Demo3DState *st, renderer::NkMeshSystem *ms, NkVec3f pPoint, NkVec3f pNormal) {
			renderer::NkMeshEditCommand c;
			c.op = renderer::NkMeshEditOp::Bisect;
			c.planePoint = pPoint;
			c.planeNormal = pNormal;
			c.bisectXform = st->editAnchor;
			Demo3D_ApplyCmd(st, ms, c);
		}

		// ══════════════════════════════════════════════════════════════════════════════
		// CADRE MODAL GENERIQUE (façon Blender) — un seul mecanisme pour toutes les ops
		// ══════════════════════════════════════════════════════════════════════════════
		static const char *Demo3D_ModalName(int32 op) {
			switch (op) {
				case 1:
					return "BEVEL ARETE";
				case 2:
					return "BEVEL SOMMET";
				case 3:
					return "INSET";
				case 4:
					return "LOOP CUT";
				case 5:
					return "SPIN";
				case 6:
					return "EXTRUDE";
				case 7:
					return "TO SPHERE";
				case 8:
					return "SHRINK/FATTEN";
			}
			return "-";
		}
		
		// L'operation a-t-elle un effet avec les parametres courants ? (un bevel/inset de
		// largeur nulle ne doit RIEN faire : la commande interpreterait 0 comme « AUTO ».)
		static bool Demo3D_ModalHasEffect(const Demo3DState *st) {
			if (st->modalOp == 1 || st->modalOp == 2 || st->modalOp == 3 || st->modalOp == 7)
				return st->modalVal > 1e-4f;
			if (st->modalOp == 8)
				return fabsf(st->modalVal) > 1e-6f; // deplacement SIGNE (gonfler / retrecir)
			return st->modalOp != 0;
		}
		
		// Construit la COMMANDE correspondant a l'etat modal courant. Point unique :
		// l'apercu et la confirmation utilisent exactement la meme commande.
		static renderer::NkMeshEditCommand Demo3D_ModalCmd(Demo3DState *st) {
			renderer::NkMeshEditCommand c;
			switch (st->modalOp) {
				case 1:
				case 2:
					c.op = renderer::NkMeshEditOp::Bevel;
					c.bevel.offset = st->modalVal;
					c.bevel.segments = st->modalSeg;
					c.bevel.vertexOnly = (st->modalOp == 2);
					break;
				case 3:
					c.op = renderer::NkMeshEditOp::Inset;
					c.inset.thickness = st->modalVal;
					c.inset.depth = st->insetDepth;
					c.inset.individual = st->insetIndividual;
					break;
				case 4:
					// LOOP CUT : molette = NOMBRE de coupes, souris = SLIDE (glissement des
					// coupes le long de l'anneau, facon Blender).
					c.op = renderer::NkMeshEditOp::LoopCut;
					c.loopcut.cuts = st->modalSeg;
					c.loopcut.slide = st->modalVal;
					break;
				case 5:
					c.op = renderer::NkMeshEditOp::Spin;
					c.spin.center = st->cursor3D;
					c.spin.axis = (st->spinAxis == 0)   ? NkVec3f{1.f, 0.f, 0.f}
								: (st->spinAxis == 2) ? NkVec3f{0.f, 0.f, 1.f}
													: NkVec3f{0.f, 1.f, 0.f};
					c.spin.angle = st->modalVal * 0.01745329f;
					c.spin.steps = st->modalSeg;
					c.spin.duplicate = st->spinDuplicate;
					c.spinXform = st->editAnchor;
					break;
				case 6:
					c.op = (st->editSelMask & 4)   ? renderer::NkMeshEditOp::Extrude
							 : (st->editSelMask & 2) ? renderer::NkMeshEditOp::ExtrudeEdges
													 : renderer::NkMeshEditOp::ExtrudeVerts;
					c.extrude.individual = st->extrudeIndividual;
					c.extrude.offset = st->modalVal;
					break;
				case 7:
					// TO SPHERE : centre = PIVOT courant ramene en espace maillage (les 5 modes
					// de pivot sont donc reutilises tels quels) ; « origines individuelles »
					// bascule la spherisation par ilot.
					c.op = renderer::NkMeshEditOp::ToSphere;
					c.tosphere.center = st->modalCenterLocal;
					c.tosphere.factor = st->modalVal;
					c.tosphere.individual =
						(st->editGizmo.PivotMode() == renderer::NkGizmo3D::PIVOT_INDIVIDUAL);
					break;
				case 8:
					c.op = renderer::NkMeshEditOp::ShrinkFatten;
					c.shrinkfatten.offset = st->modalVal;
					break;
			}
			return c;
		}
		
		// Restaure le snapshot + la selection de depart. LOOP CUT : la selection est
		// remplacee par l'ARETE SURVOLEE -> l'anneau previsualise suit la souris, comme
		// le trait jaune de Blender avant confirmation.
		static void Demo3D_ModalRestore(Demo3DState *st) {
			st->editHE = st->modalSnap;
			st->vertSel = st->modalSelSnap;
			if (st->modalOp == 4 && st->modalLoopA >= 0 && st->modalLoopB >= 0) {
				for (uint32 i = 0; i < (uint32)st->vertSel.Size(); i++)
					st->vertSel[i] = 0;
				if ((uint32)st->modalLoopA < (uint32)st->vertSel.Size())
					st->vertSel[st->modalLoopA] = 1;
				if ((uint32)st->modalLoopB < (uint32)st->vertSel.Size())
					st->vertSel[st->modalLoopB] = 1;
			}
		}
		
		// L'op modale ne fait-elle QUE bouger des sommets (topologie inchangee) ? Si oui,
		// l'apercu peut emprunter le chemin allege « positions seules » (aucune
		// reconstruction du mesh GPU). Vrai pour To Sphere et Shrink/Fatten a topologie
		// figee, et pour le SLIDE du loop cut tant que le NOMBRE de coupes et l'arete
		// survolee ne changent pas (la topologie du resultat n'en depend que de ca).
		static bool Demo3D_ModalPosOnlyOk(const Demo3DState *st) {
			if (st->modalForceFull || !st->modalHasApplied)
				return false;
			if (st->modalOp == 7 || st->modalOp == 8)
				return true;
			if (st->modalOp == 4)
				return st->modalSeg == st->modalAppliedSeg && st->modalLoopA == st->modalAppliedLoopA &&
					   st->modalLoopB == st->modalAppliedLoopB;
			return false;
		}

		// APERCU : re-applique l'operation DEPUIS LE SNAPSHOT avec les parametres
		// courants, SANS toucher a l'historique ni au journal (rien n'est encore
		// confirme). Appele UNIQUEMENT quand un parametre a REELLEMENT change.
		static void Demo3D_ModalPreview(Demo3DState *st, renderer::NkMeshSystem *ms) {
			NkChrono pvChrono;
			const bool posOnly = Demo3D_ModalPosOnlyOk(st);
			Demo3D_ModalRestore(st);
			Demo3D_PushSel(st);
			if (Demo3D_ModalHasEffect(st)) {
				renderer::NkMeshEditCommand c = Demo3D_ModalCmd(st);
				// NkMeshEditCommand::Apply REPOSE la selection depuis `c.selection` (la
				// commande est une donnee rejouable). Sans la remplir, l'apercu appliquait
				// l'operation sur une selection VIDE -> aucun effet visible.
				c.selection.Clear();
				for (uint32 i = 0; i < (uint32)st->vertSel.Size(); ++i)
					if (st->vertSel[i])
						c.selection.PushBack(i);
				c.Apply(st->editHE);
			}
			Demo3D_PullSel(st);
			// CHEMIN ALLEGE : la topologie n'a pas bouge -> on ne ré-uploade que les sommets
			// (aucun Release/Create du mesh GPU). Repli automatique sur le chemin complet si
			// l'hypothese ne tient pas.
			bool usedPos = false;
			if (posOnly)
				usedPos = Demo3D_SyncPosOnlyFromHE(st, ms);
			if (!usedPos)
				Demo3D_SyncFromHE(st, ms);
			st->editOverlayDirty = true;
			// Etat REELLEMENT applique : sert de reference pour ne rien reconstruire tant
			// qu'aucun parametre n'a change.
			st->modalAppliedVal = st->modalVal;
			st->modalAppliedSeg = st->modalSeg;
			st->modalAppliedLoopA = st->modalLoopA;
			st->modalAppliedLoopB = st->modalLoopB;
			st->modalHasApplied = true;
			if (st->modalPerf) {
				const float64 ms2 = pvChrono.Elapsed().ToMilliseconds();
				if (usedPos) {
					st->modalPvPos++;
					st->modalPvPosMs += ms2;
				} else {
					st->modalPvFull++;
					st->modalPvFullMs += ms2;
				}
				logger.Info("[ModalPerf] apercu {0} : {1} ms (valeur={2} segments={3})\n",
							usedPos ? "POSITIONS" : "COMPLET", (float32)ms2, st->modalVal, st->modalSeg);
			}
			logger.Info("[Demo3D] MODAL {0} APERCU {1} (valeur={2} segments={3}) -> {4} sommets / {5} faces\n",
					Demo3D_ModalName(st->modalOp), usedPos ? "[positions]" : "[complet]", st->modalVal,
					st->modalSeg, (int32)st->editHE.VertCount(), (int32)st->editHE.FaceCount());
		}
		
		// CONFIRMATION (clic gauche) : on repart du snapshot et on rejoue l'operation par
		// le chemin NORMAL (Demo3D_ApplyCmd) -> UN SEUL undo et UNE SEULE commande
		// journalisee pour toute la manipulation modale.
		static void Demo3D_ModalConfirm(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (st->modalOp == 0)
				return;
			const int32 op = st->modalOp;
			const float32 val = st->modalVal;
			const int32 seg = st->modalSeg;
			const bool eff = Demo3D_ModalHasEffect(st);
			renderer::NkMeshEditCommand c = Demo3D_ModalCmd(st);
			Demo3D_ModalRestore(st);
			st->modalOp = 0;
			st->modalLoopA = st->modalLoopB = -1;
			if (eff)
				Demo3D_ApplyCmd(st, ms, c);
			else
				Demo3D_SyncFromHE(st, ms);
			st->editOverlayDirty = true;
			logger.Info("[Demo3D] MODAL {0} CONFIRME (valeur={1} segments={2}) -> {3} sommets / {4} faces\n",
						Demo3D_ModalName(op), val, seg, (int32)st->editHE.VertCount(), (int32)st->editHE.FaceCount());
			if (st->modalPerf) {
				logger.Info("[ModalPerf] bilan : {0} apercus COMPLETS ({1} ms total, {2} ms/apercu) · {3} "
							"apercus POSITIONS ({4} ms total, {5} ms/apercu)\n",
							st->modalPvFull, (float32)st->modalPvFullMs,
							(float32)(st->modalPvFull ? st->modalPvFullMs / st->modalPvFull : 0.0), st->modalPvPos,
							(float32)st->modalPvPosMs,
							(float32)(st->modalPvPos ? st->modalPvPosMs / st->modalPvPos : 0.0));
			}
		}
		
		// ANNULATION (Echap / clic droit) : retour EXACT a l'etat initial.
		static void Demo3D_ModalCancel(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (st->modalOp == 0)
				return;
			const int32 op = st->modalOp;
			st->modalLoopA = st->modalLoopB = -1;
			st->modalOp = 0;
			st->editHE = st->modalSnap;
			st->vertSel = st->modalSelSnap;
			Demo3D_PushSel(st);
			Demo3D_SyncFromHE(st, ms);
			st->editOverlayDirty = true;
			// PREUVE d'annulation exacte : on compare les compteurs AVANT/APRES (c'est ce que
			// verifie la capture headless NK_MODAL_CANCEL=1).
			const uint32 nvAfter = st->editHE.VertCount(), nfAfter = st->editHE.FaceCount();
			logger.Info("[Demo3D] MODAL {0} ANNULE -> etat initial restaure : {1} sommets / {2} faces "
						"(avant l'op : {3} / {4}) -> {5}\n",
						Demo3D_ModalName(op), (int32)nvAfter, (int32)nfAfter, (int32)st->modalSnapVerts,
						(int32)st->modalSnapFaces,
						(nvAfter == st->modalSnapVerts && nfAfter == st->modalSnapFaces) ? "IDENTIQUE"
																						 : "DIFFERENT !");
			if (st->modalPerf) {
				logger.Info("[ModalPerf] bilan : {0} apercus COMPLETS ({1} ms total, {2} ms/apercu) · {3} "
							"apercus POSITIONS ({4} ms total, {5} ms/apercu)\n",
							st->modalPvFull, (float32)st->modalPvFullMs,
							(float32)(st->modalPvFull ? st->modalPvFullMs / st->modalPvFull : 0.0), st->modalPvPos,
							(float32)st->modalPvPosMs,
							(float32)(st->modalPvPos ? st->modalPvPosMs / st->modalPvPos : 0.0));
			}
		}
		
		// LANCEMENT : capture le snapshot, initialise les parametres et l'ancre souris.
		static void Demo3D_ModalStart(Demo3DState *st, int32 op, renderer::NkMeshSystem *ms) {
			if (st->modalOp != 0)
				Demo3D_ModalCancel(st, ms); // une seule operation modale a la fois
			st->modalOp = op;
			st->modalSnap = st->editHE;
			st->modalSelSnap = st->vertSel;
			st->modalSnap.GetUniqueEdges(st->modalSnapEdges);
			st->modalFrames = 0;
			st->modalLoopA = st->modalLoopB = -1;
			st->modalSnapVerts = st->modalSnap.VertCount();
			st->modalSnapFaces = st->modalSnap.FaceCount();
			st->modalStartX = ((float32)NkInput.MouseX() - nkvpOffX);
			// CURSEUR VIRTUEL : point de depart = position reelle de la souris. Les deltas
			// (reels OU injectes par NK_MODAL_DRAG) s'y accumulent tant que l'op tourne.
			st->modalCurX = st->modalStartX;
			st->modalCurY = ((float32)NkInput.MouseY() - nkvpOffY);
			st->modalHasApplied = false;
			st->modalAppliedLoopA = st->modalAppliedLoopB = -2;
			st->modalInjWheelDone = false;
			st->modalPvFull = st->modalPvPos = 0;
			st->modalPvFullMs = st->modalPvPosMs = 0.0;
			// Echelle pixels -> unites du parametre : ~400 px de course couvrent la
			// diagonale de la boite englobante -> le reglage « tombe juste » quelle que
			// soit la taille du modele.
			NkVec3f bmin{1e30f, 1e30f, 1e30f}, bmax{-1e30f, -1e30f, -1e30f};
			for (uint32 i = 0; i < st->modalSnap.VertCount(); i++) {
				const NkVec3f &q = st->modalSnap.verts[i].pos;
				bmin.x = NkMin(bmin.x, q.x);
				bmin.y = NkMin(bmin.y, q.y);
				bmin.z = NkMin(bmin.z, q.z);
				bmax.x = NkMax(bmax.x, q.x);
				bmax.y = NkMax(bmax.y, q.y);
				bmax.z = NkMax(bmax.z, q.z);
			}
			const float32 diag = (st->modalSnap.VertCount() > 0) ? (bmax - bmin).Len() : 1.f;
			switch (op) {
				case 1:
				case 2:
					st->modalVal = 0.f;
					st->modalSeg = st->bevelSegments;
					st->modalScale = diag / 400.f;
					break;
				case 3:
					st->modalVal = 0.f;
					st->modalSeg = 1;
					st->modalScale = diag / 400.f;
					break;
				case 4:
					// LOOP CUT : la souris pilote le SLIDE (glissement des coupes le long de
					// l'anneau, facon Blender), la molette le NOMBRE de coupes. 0 = position
					// mediane ; ±1 = coupes rabattues sur l'une des deux boucles bordantes.
					st->modalVal = 0.f;
					st->modalSeg = st->loopCuts;
					st->modalScale = 1.f / 300.f; // 300 px de course = glissement complet
					break;
				case 5:
					st->modalVal = st->spinAngleDeg;
					st->modalSeg = st->spinSteps;
					st->modalScale = 1.f; // 1 degre par pixel
					break;
				case 6:
					st->modalVal = 0.f; // Blender : la geometrie nait sur place, puis on tire
					st->modalSeg = 1;
					st->modalScale = diag / 400.f;
					break;
				case 7:
					st->modalVal = 0.f;   // facteur 0 = inchange ; 1 = sphere parfaite
					st->modalSeg = 1;
					st->modalScale = 1.f / 300.f; // 300 px de course = facteur 1
					break;
				case 8:
					st->modalVal = 0.f;   // deplacement SIGNE le long des normales
					st->modalSeg = 1;
					st->modalScale = diag / 400.f;
					break;
			}
			if (st->modalEnvHasVal)
				st->modalVal = st->modalEnvVal; // pilote headless (pas de souris en capture)
			// L'ANCRE du drag est la valeur de depart : la souris ajoute son deplacement a
			// partir de la (sinon la valeur forcee serait aussitot ecrasee a 0).
			st->modalBase = st->modalVal;
			if (st->modalEnvHasSeg)
				st->modalSeg = st->modalEnvSeg;
			st->modalDirty = true;
			logger.Info("[Demo3D] MODAL {0} lance : souris=valeur · molette=segments · clic gauche=confirmer · "
						"Echap/clic droit=annuler (valeur={1} segments={2})\n",
						Demo3D_ModalName(op), st->modalVal, st->modalSeg);
		}
		
		// REJEU (P) : reconstruit editHE depuis le maillage de BASE (capturé à l'entrée) et
		// rejoue TOUTES les commandes enregistrées. Preuve que la couche de commandes est
		// scriptable (fondation modificateurs non-destructifs + données d'imitation IA).
		// REJEU PAS-À-PAS (P) : chaque appui applique UNE commande de plus depuis la base, pour
		// VOIR le modèle se reconstruire étape par étape (comme regarder rejouer la session —
		// exactement l'observation qu'aurait une IA d'apprentissage). Boucle : après la dernière
		// commande, un appui de plus revient à la base.
		static void Demo3D_ReplayEdits(Demo3DState *st, renderer::NkMeshSystem *ms) {
			const int32 total = (int32)st->editRecorder.Count();
			if (st->editReplayStep < 0)
				st->editReplayStep = 0; // 1er appui -> base (0 commande)
			else
				st->editReplayStep++; // appui suivant -> une commande de plus
			if (st->editReplayStep > total)
				st->editReplayStep = 0; // après la fin -> reboucle à la base

			st->editHE = st->editBase; // repart de la base
			int32 applied = 0;
			for (int32 i = 0; i < st->editReplayStep && i < total; ++i) // rejoue les k premières commandes
				if (st->editRecorder.At((uint32)i).Apply(st->editHE))
					++applied;
			st->vertSel.Clear();
			st->vertSel.Resize(st->editHE.VertCount());
			for (uint32 i = 0; i < st->editHE.VertCount(); ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
			Demo3D_SyncFromHE(st, ms);
			logger.Info("[Demo3D] Rejeu pas-a-pas: etape {0}/{1} ({2} appliquees) -> {3} faces\n", st->editReplayStep,
						total, applied, (int32)st->editHE.FaceCount());
		}

		// SAUVER (F5) : sérialise le journal de commandes -> fichier binaire .nmec (persistance
		// de la session : donnée d'imitation IA + modificateur sauvegardable).
		static void Demo3D_SaveSession(Demo3DState *st) {
			NkVector<uint8> bytes;
			st->editRecorder.Serialize(bytes);
			const char *path = "edit_session.nkmec";
			const bool ok = NkFile::WriteAllBytes(path, bytes);
			logger.Info("[Demo3D] Sauvegarde '{0}' : {1} commandes, {2} octets -> {3}\n", path,
						st->editRecorder.Count(), (int32)bytes.Size(), ok ? "OK" : "ECHEC");
		}

		// CHARGER (F6) : lit le fichier -> désérialise dans le journal -> rejoue depuis la base
		// (reconstruit le modèle édité). Le maillage de base courant doit correspondre.
		static void Demo3D_LoadSession(Demo3DState *st, renderer::NkMeshSystem *ms) {
			const char *path = "edit_session.nkmec";
			NkVector<uint8> bytes = NkFile::ReadAllBytes(path);
			if (bytes.Empty()) {
				logger.Warn("[Demo3D] Chargement '{0}' : fichier vide/absent\n", path);
				return;
			}
			if (!st->editRecorder.Deserialize(bytes.Data(), (uint32)bytes.Size())) {
				logger.Warn("[Demo3D] Chargement '{0}' : format invalide\n", path);
				return;
			}
			st->editHE = st->editBase;
			const uint32 applied = st->editRecorder.ReplayOnto(st->editHE);
			st->editReplayStep = -1;
			st->vertSel.Clear();
			st->vertSel.Resize(st->editHE.VertCount());
			for (uint32 i = 0; i < st->editHE.VertCount(); ++i)
				st->vertSel[i] = st->editHE.verts[i].sel;
			Demo3D_SyncFromHE(st, ms);
			logger.Info("[Demo3D] Session chargee '{0}' : {1} commandes rejouees -> {2} faces\n", path, applied,
						(int32)st->editHE.FaceCount());
		}

		// MODIFICATEURS — réglage des paramètres du modificateur ACTIF ([ / ]) et changement
		// du modificateur actif (\). Chaque changement ré-évalue la pile -> résultat live.
		static void Demo3D_AdjustMod(Demo3DState *st, renderer::NkMeshSystem *ms, int32 dir) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0)
				return;
			if (st->editActiveMod < 0 || st->editActiveMod >= cnt)
				st->editActiveMod = cnt - 1;
			renderer::NkMeshModifier &m = st->editModifiers.modifiers[st->editActiveMod];
			const uint32 pc = m.ParamCount();
			if (pc == 0)
				return;
			if (st->editActiveParam < 0 || st->editActiveParam >= (int32)pc)
				st->editActiveParam = 0;
			const renderer::NkModParam *p = m.ParamAt((uint32)st->editActiveParam);
			if (!p)
				return;
			// PAS D'INCRÉMENT PAR TYPE. Un booléen bascule, un entier avance de 1, un
			// réel avance d'un centième de sa plage (ou de 0,05 si la plage est libre).
			// Sans cette règle, un même geste ferait passer un compteur de 1 à 2 et une
			// distance de 0,001 à 1,001 — l'un utilisable, l'autre pas.
			if (p->type == renderer::NkModParamType::Vec3) {
				NkVec3f v{0.f, 0.f, 0.f};
				m.GetParamVec3(p->name, v);
				const float32 stepv = 0.25f * (float32)dir;
				v = v + NkVec3f{stepv, 0.f, 0.f}; // décalage principal = X (cf. Array)
				m.SetParamVec3(p->name, v);
				logger.Info("[Demo3D] Modif[{0}] {1} = ({2}, {3}, {4})\n", st->editActiveMod, p->label, v.x, v.y, v.z);
			} else {
				float32 v = 0.f;
				m.GetParam(p->name, v);
				float32 step = 1.f;
				if (p->type == renderer::NkModParamType::Bool)
					v = (v >= 0.5f) ? 0.f : 1.f;
				else {
					if (p->type == renderer::NkModParamType::Float)
						step = (p->maxV > p->minV) ? (p->maxV - p->minV) * 0.01f : 0.05f;
					v += step * (float32)dir;
				}
				m.SetParam(p->name, v);
				m.GetParam(p->name, v); // relire : la valeur a pu être écrêtée
				logger.Info("[Demo3D] Modif[{0}] {1} ({2}) = {3}\n", st->editActiveMod, p->label, p->name, v);
			}
			Demo3D_SyncFromHE(st, ms);
		}

		// Passe au PARAMÈTRE suivant du modificateur actif (touche ; ). Générique :
		// la liste vient du modificateur, pas d'un switch dans la démo.
		static void Demo3D_CycleModParam(Demo3DState *st) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0 || st->editActiveMod < 0 || st->editActiveMod >= cnt)
				return;
			const renderer::NkMeshModifier &m = st->editModifiers.modifiers[st->editActiveMod];
			const uint32 pc = m.ParamCount();
			if (pc == 0)
				return;
			st->editActiveParam = (st->editActiveParam + 1) % (int32)pc;
			const renderer::NkModParam *p = m.ParamAt((uint32)st->editActiveParam);
			float32 v = 0.f;
			if (p) {
				m.GetParam(p->name, v);
				logger.Info("[Demo3D] Modif[{0}] parametre actif = {1} ({2}) = {3}\n", st->editActiveMod, p->label,
							p->name, v);
			}
		}

		// GESTION DE LA PILE — monter/descendre/activer/dupliquer/retirer/appliquer.
		// L'ORDRE de la pile est un paramètre de RÉSULTAT : miroir puis tableau ne
		// donne pas la même chose que tableau puis miroir. Pouvoir déplacer une entrée
		// n'est donc pas un confort d'interface.
		static void Demo3D_ModStackOp(Demo3DState *st, renderer::NkMeshSystem *ms, int32 op) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0) {
				logger.Info("[Demo3D] Pile de modificateurs vide.\n");
				return;
			}
			if (st->editActiveMod < 0 || st->editActiveMod >= cnt)
				st->editActiveMod = cnt - 1;
			const uint32 i = (uint32)st->editActiveMod;
			bool ok = false;
			switch (op) {
				case 1:
					ok = st->editModifiers.MoveUp(i);
					if (ok)
						st->editActiveMod--;
					break;
				case 2:
					ok = st->editModifiers.MoveDown(i);
					if (ok)
						st->editActiveMod++;
					break;
				case 3: ok = st->editModifiers.SetEnabled(i, !st->editModifiers.modifiers[i].enabled); break;
				case 4:
					ok = st->editModifiers.Duplicate(i);
					if (ok)
						st->editActiveMod = (int32)i + 1; // la copie devient active
					break;
				case 5:
					ok = st->editModifiers.Remove(i);
					if (ok && st->editActiveMod >= (int32)st->editModifiers.Count())
						st->editActiveMod = (int32)st->editModifiers.Count() - 1;
					break;
				case 6: {
					// APPLIQUER : cuit dans le maillage ÉDITABLE et retire de la pile.
					// L'opération est DESTRUCTIVE — d'où le snapshot d'annulation AVANT,
					// sans lequel un clic malheureux serait irréversible.
					st->editHistory.Commit(st->editHE);
					bool notFirst = false;
					ok = st->editModifiers.ApplyToBase(i, st->editHE, &notFirst);
					if (ok && notFirst)
						logger.Info("[Demo3D] ⚠ modificateur appliqué alors qu'il n'était PAS le premier : le "
									"resultat ne correspond pas a ce qui etait affiche (les precedents n'ont pas "
									"ete cuits). Comportement de Blender.\n");
					if (ok && st->editActiveMod >= (int32)st->editModifiers.Count())
						st->editActiveMod = (int32)st->editModifiers.Count() - 1;
					break;
				}
				default: break;
			}
			if (!ok) {
				logger.Info("[Demo3D] Operation de pile sans effet (bord de pile ?).\n");
				return;
			}
			st->editActiveParam = 0;
			Demo3D_SyncFromHE(st, ms);
			// État complet de la pile après l'opération : c'est ce qui permet de
			// vérifier l'ordre sans capture d'écran.
			for (uint32 k = 0; k < st->editModifiers.Count(); ++k) {
				const renderer::NkMeshModifier &mm = st->editModifiers.modifiers[k];
				logger.Info("[Demo3D][PILE] [{0}] {1} id={2} {3}{4}\n", k, renderer::NkModifierTypeName(mm.type),
							mm.id, mm.enabled ? "actif" : "ETEINT",
							((int32)k == st->editActiveMod) ? "  <- selectionne" : "");
			}
		}

		static void Demo3D_CycleActiveMod(Demo3DState *st) {
			const int32 cnt = (int32)st->editModifiers.Count();
			if (cnt == 0) {
				st->editActiveMod = -1;
				return;
			}
			st->editActiveMod = (st->editActiveMod + 1) % cnt;
			const char *nm[3] = {"Mirror", "Array", "Subsurf"};
			logger.Info("[Demo3D] Modificateur actif = [{0}] {1}\n", st->editActiveMod,
						nm[(int32)st->editModifiers.modifiers[st->editActiveMod].type]);
		}

		// UNDO / REDO : restaure un snapshot de editHE, resynchronise sélection + rendu.
		static void Demo3D_UndoEdit(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (!st->editHistory.Undo(st->editHE))
				return;
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
		}

		static void Demo3D_RedoEdit(Demo3DState *st, renderer::NkMeshSystem *ms) {
			if (!st->editHistory.Redo(st->editHE))
				return;
			Demo3D_PullSel(st);
			Demo3D_SyncFromHE(st, ms);
		}

		// E.6b : cubemap procedurale 128x128x6 pour point light.
		// Chaque face = pattern "X" : 2 bandes diagonales lumineuses sur fond noir.
		// Tres contraste pour etre clairement visible meme avec autres lumieres.
		static NkTexHandle CreateLanternCubeCookie(NkTextureLibrary *texLib, NkIDevice *dev) {
			const uint32 S = 128;
			NkTextureCreateDesc d;
			d.width = S;
			d.height = S;
			d.depth = 1;
			d.format = NkGPUFormat::NK_RGBA8_UNORM;
			d.isCubemap = true;
			d.mipLevels = 1;
			d.debugName = "Demo3D_LanternCube";
			NkTexHandle tex = texLib->Create(d);
			if (!tex.IsValid())
				return tex;

			std::vector<uint8_t> pixels(S * S * 4);
			for (uint32 face = 0; face < 6; face++) {
				for (uint32 y = 0; y < S; y++) {
					for (uint32 x = 0; x < S; x++) {
						// X pattern : bright si proche d'une diagonale (epaisseur 8 px)
						float dx = float(x) - S * 0.5f;
						float dy = float(y) - S * 0.5f;
						float diag1 = std::abs(dx - dy); // diagonale slash
						float diag2 = std::abs(dx + dy); // diagonale antislash
						bool onCross = diag1 < 8.f || diag2 < 8.f;
						// Trou central toujours brillant (faisceau "principal")
						float r = std::sqrt(dx * dx + dy * dy);
						bool centerHole = r < S * 0.10f;
						uint8_t v = (onCross || centerHole) ? 255 : 0; // contraste max
						uint32 idx = (y * S + x) * 4;
						pixels[idx + 0] = v;
						pixels[idx + 1] = v;
						pixels[idx + 2] = v;
						pixels[idx + 3] = 255;
					}
				}
				dev->WriteTextureRegion(texLib->GetRHIHandle(tex), pixels.data(), 0, 0, 0, S, S, 1, 0, face);
			}
			return tex;
		}

		// Genere un cookie procedural 256x256 RGBA : motif "window bars".
		// Barres + bordure noires (~12% de transmittance), centre/cellules blanches.
		static NkTexHandle CreateWindowCookie(NkTextureLibrary *texLib) {
			const uint32 W = 256, H = 256;
			std::vector<uint8_t> pixels(W * H * 4);
			for (uint32 y = 0; y < H; y++) {
				for (uint32 x = 0; x < W; x++) {
					bool barV = (x % 64) < 8;
					bool barH = (y % 64) < 8;
					bool border = (x < 8 || x >= W - 8 || y < 8 || y >= H - 8);
					uint8_t v = (barV || barH || border) ? 30 : 255;
					uint32 idx = (y * W + x) * 4;
					pixels[idx + 0] = v;
					pixels[idx + 1] = v;
					pixels[idx + 2] = v;
					pixels[idx + 3] = 255;
				}
			}
			NkTextureCreateDesc d;
			d.pixels = pixels.data();
			d.width = W;
			d.height = H;
			d.mipLevels = 1;
			d.format = NkGPUFormat::NK_RGBA8_UNORM;
			d.debugName = "Demo3D_WindowCookie";
			return texLib->Create(d);
		}

		// Phase H : test de la pipeline texture file-based.
		//
		// Genere (si absent) un fichier PNG procedural "test_pattern.png" 256x256
		// contenant un motif damier color (4 quadrants RGB + bordures), puis le
		// charge via NkTextureLibrary::Load(). Demontre toute la chaine :
		//   1. NkImage::Alloc + SavePNG  -> ecriture disque
		//   2. NkTextureLibrary::Load    -> decode PNG + upload GPU + mip chain
		//   3. retourne un NkTexHandle utilisable comme tout autre texture.
		//
		// Le fichier est genere dans Resources/NKRenderer/Textures/Defaults/
		// (relativement au CWD ou au repo). On essaie d'abord ce chemin, sinon
		// fallback sur le CWD. La premiere execution genere le fichier ; les
		// suivantes le lisent. Si ecriture echoue (permissions / dir inexistant),
		// on retourne false et l'init utilise le cookie procedural.
		static const char *kPhaseHTestPathPrimary = "Resources/NKRenderer/Textures/Defaults/test_pattern.png";
		static const char *kPhaseHTestPathFallback = "test_pattern.png";

		static bool GenerateTestPatternPNG(const char *outPath) {
			// Verifie d'abord s'il existe deja (skip si present).
			if (FILE *f = ::fopen(outPath, "rb")) {
				::fclose(f);
				return true;
			}

			const int32 W = 256, H = 256;
			NkImage *img = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
			if (!img || !img->IsValid()) {
				if (img)
					img->Free();
				return false;
			}

			uint8_t *px = img->Pixels();
			const int32 stride = img->Stride();
			for (int32 y = 0; y < H; ++y) {
				uint8_t *row = px + (size_t)y * stride;
				for (int32 x = 0; x < W; ++x) {
					// 4 quadrants : rouge / vert / bleu / jaune.
					bool right = (x >= W / 2);
					bool bottom = (y >= H / 2);
					uint8_t r = (!right && !bottom) || (right && bottom) ? 255 : 0;
					uint8_t g = (right && !bottom) || (right && bottom) ? 255 : 0;
					uint8_t b = (!right && bottom) ? 255 : 0;
					// Damier 16x16 pour valider qu'on lit bien le PNG (et pas un
					// buffer noir/blanc).
					bool ck = ((x / 16) ^ (y / 16)) & 1;
					if (ck) {
						r = (uint8_t)(r * 0.6f);
						g = (uint8_t)(g * 0.6f);
						b = (uint8_t)(b * 0.6f);
					}
					// Bordure noire 4px pour visualiser les bords.
					bool border = (x < 4 || x >= W - 4 || y < 4 || y >= H - 4);
					if (border) {
						r = g = b = 0;
					}
					row[x * 4 + 0] = r;
					row[x * 4 + 1] = g;
					row[x * 4 + 2] = b;
					row[x * 4 + 3] = 255;
				}
			}
			bool ok = img->SavePNG(outPath);
			img->Free();
			return ok;
		}

		// Helper d'affichage : nom court de PCFMode
		static const char *PcfModeName(NkPCFMode m) {
			switch (m) {
				case NkPCFMode::NONE:
					return "NONE";
				case NkPCFMode::PCF3x3:
					return "PCF3x3";
				case NkPCFMode::PCF5x5:
					return "PCF5x5";
				case NkPCFMode::POISSON:
					return "POISSON";
				case NkPCFMode::PCSS:
					return "PCSS";
			}
			return "?";
		}

		bool Demo3D_Init(DemoCtx &ctx) {
			auto *st = new Demo3DState();
			ctx.userData = st;

			auto *meshSys = ctx.renderer->GetMeshSystem();
			st->meshSphere = meshSys->GetSphere();
			st->meshPlane = meshSys->GetPlane();
			st->meshCube = meshSys->GetCube();
			// Volet 2 : le mesh éditable n'est plus une grille test — il est CLONÉ à la
			// volée depuis l'objet sélectionné à l'entrée en Edit Mode (TAB), cf. la
			// section « EDIT MODE » dans la frame. Les primitives (sphère/cube) gardent
			// leurs données CPU (NkMeshDesc::keepCPU par défaut) -> clonage sans readback.

			// ── AIMANTATION (« snap ») — RÉGLÉE POUR LES DEUX MODES ────────────────
			// Auparavant seul le gizmo OBJET était configuré ; le gizmo d'ÉDITION
			// gardait les défauts de la classe. Ils coïncidaient, donc l'aimantation
			// paraissait identique dans les deux modes — mais par accident : changer
			// le pas ici n'aurait rien changé en édition. Les deux sont désormais
			// réglés au même endroit, ce qui rend la coïncidence VOULUE.
			//   translate (unités monde) · rotation (degrés) · échelle (delta).
			for (renderer::NkGizmo3D *gz : {&st->gizmo, &st->editGizmo}) {
				gz->SetSnapSteps(/*translate*/ 0.5f, /*rotation°*/ 15.f, /*échelle*/ 0.1f);
				// Bascule PERSISTANTE façon Blender (Shift+Tab), que Ctrl inverse.
				// Éteinte par défaut : Ctrl aimante, comme avant.
				gz->SetSnapEnabled(false);
				// Grille ABSOLUE : NK_SNAP_ABS=1. Par défaut incrément RELATIF, qui est
				// aussi le défaut de Blender.
				if (const char *sa = getenv("NK_SNAP_ABS"))
					gz->SetSnapAbsolute(sa[0] && sa[0] != '0');
				if (const char *so = getenv("NK_SNAP_ON"))
					gz->SetSnapEnabled(so[0] && so[0] != '0');
				if (const char *ss = getenv("NK_SNAP_STEP"))
					gz->SetSnapTranslate((float32)atof(ss));
			}

			// ── Phase E.6 : creation procedurale des cookies + bind ──────────────
			auto *texLib = ctx.renderer->GetTextures();
			auto *r3d = ctx.renderer->GetRender3D();
			auto *device = ctx.renderer->GetDevice();

			// ── NkVSM v2 : caster ALPHA-TESTED de validation ─────────────────────
			// Panneau "feuillage" : disques de matiere (alpha=255) sur fond TROUE
			// (alpha=0). SetCastShadowAlphaTest(true) -> l'ombre projetee au sol
			// doit montrer les TROUS entre les disques (pipeline Shadow_AlphaTest)
			// et non le rectangle plein. NB : la passe COULEUR reste opaque (le
			// PBR ne fait pas d'alpha-blend) — c'est l'OMBRE qui valide la feature.
			if (texLib && ctx.renderer->GetMaterials()) {
				const uint32 TW = 128, TH = 128;
				NkVector<uint8> px;
				px.Resize((usize)TW * TH * 4u);
				for (uint32 y = 0; y < TH; y++) {
					for (uint32 x = 0; x < TW; x++) {
						const int32 lx = (int32)(x % 32) - 16;
						const int32 ly = (int32)(y % 32) - 16;
						const bool inDisc = (lx * lx + ly * ly) <= 12 * 12;
						uint8 *p = &px[((usize)y * TW + x) * 4u];
						p[0] = 46;
						p[1] = inDisc ? 168 : 60;
						p[2] = 52;
						p[3] = inDisc ? 255 : 0;
					}
				}
				NkTextureCreateDesc td;
				td.pixels = px.Data();
				td.width = TW;
				td.height = TH;
				td.srgb = true;
				td.debugName = "Demo3D_MaskedLeaf";
				st->maskedTex = texLib->Create(td);
				// NB : le template PBR est enregistre sous "Default_PBR" — on passe
				// par l'overload TYPE (pas de nom en dur qui casse silencieusement).
				st->maskedMat = NkMaterial::Create(ctx.renderer->GetMaterials(), NkMaterialType::NK_PBR_METALLIC);
				if (!st->maskedMat)
					logger.Errorf("[Demo3D] Creation material panneau masked KO\n");
				if (st->maskedMat && st->maskedTex.IsValid())
					st->maskedMat->SetAlbedoMap(st->maskedTex)->SetRoughness(0.8f)->SetCastShadowAlphaTest(true);
			}
			if (texLib && r3d) {
				// Phase H : test de la pipeline file-based.
				// On genere un PNG "test_pattern.png" puis on le charge via
				// NkTextureLibrary::Load(). Si la chaine fonctionne, on l'utilise
				// comme cookie spot (plus visible que le procedural en RAM car
				// sample par texture(...) GLSL avec mips). Fallback : cookie
				// procedural (CreateWindowCookie) si Load echoue.
				const char *pngPath = nullptr;
				if (GenerateTestPatternPNG(kPhaseHTestPathPrimary)) {
					pngPath = kPhaseHTestPathPrimary;
				} else if (GenerateTestPatternPNG(kPhaseHTestPathFallback)) {
					pngPath = kPhaseHTestPathFallback;
				}

				if (pngPath) {
					NkLoadOptions opts;
					// Cookie : on veut le PNG en valeur lineaire (pas de gamma)
					// pour que la modulation lumiere*cookie soit correcte. On
					// pourrait passer srgb=true pour un albedo PBR classique.
					opts.srgb = false;
					opts.genMipmaps = true;	  // mips utiles pour cookie
					opts.useClampEdge = true; // cookie : pas de tiling
					opts.debugName = "Demo3D_PhaseH_TestPattern";
					st->cookieWindow = texLib->Load(NkString(pngPath), opts);
					if (st->cookieWindow.IsValid() && st->cookieWindow.id != texLib->GetError().id) {
						logger.Info("[Demo3D][Phase H] PNG charge OK depuis '{0}'\n", pngPath);
						st->phaseHLoadOk = true;
					} else {
						logger.Errorf("[Demo3D][Phase H] Echec Load PNG, fallback procedural\n");
						st->cookieWindow = CreateWindowCookie(texLib);
					}
				} else {
					logger.Errorf("[Demo3D][Phase H] Impossible d'ecrire le PNG de test, fallback procedural\n");
					st->cookieWindow = CreateWindowCookie(texLib);
				}

				if (st->cookieWindow.IsValid()) {
					r3d->SetLightCookie3D(0, texLib->GetRHIHandle(st->cookieWindow));
					logger.Info("[Demo3D] Cookie 2D bind au slot 0\n");
				}
				// E.6b : cube cookie pour le point light rouge (procedural, OK).
				if (device) {
					st->cookieCube = CreateLanternCubeCookie(texLib, device);
					if (st->cookieCube.IsValid()) {
						r3d->SetLightCookieCube3D(0, texLib->GetRHIHandle(st->cookieCube));
						logger.Info("[Demo3D] Lantern cube cookie cree + bind au slot 0\n");
					}
				}
			}

			// ── Shortcuts clavier pour tweak des params shadow en live ──
			// [ / ] : shadowBias -+ 0.0005
			// , / . : sceneRadius -+ 1.0
			// P     : cycle PCF mode
			// R     : reset to defaults
			// V     : toggle VSync (utile pour mesurer le vrai FPS GPU)
			auto *shadowSys = ctx.renderer->GetShadow();
			auto *renderer = ctx.renderer;
			// Molette souris -> zoom caméra (accumulée ici, appliquée dans Demo3D_Frame).
			NkEvents().AddEventCallback<NkMouseWheelVerticalEvent>(
				[st](NkMouseWheelVerticalEvent *e) {
					// PORTAGE NK3DModeler : molette reservee a la vue survolee.
					if (nkvpInputOn && nkvpHover)
						st->wheelAccum += e->GetDeltaY();
				});
			// Clic GAUCHE -> demande de sélection (ray-pick, traité dans Demo3D_Frame).
			NkEvents().AddEventCallback<NkMouseButtonPressEvent>([st](NkMouseButtonPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
				// PORTAGE NK3DModeler : l'outil CURSEUR de la barre fait du clic
				// gauche un placement de curseur 3D (comme l'outil de Blender). Le
				// Shift+clic droit de la demo reste valable en parallele.
				if (nkvpCursorTool && e->GetButton() == NkMouseButton::NK_MB_LEFT) {
					st->cursorPlacePending = true;
					st->cursorPX = (float32)e->GetX() - nkvpOffX;
					st->cursorPY = (float32)e->GetY() - nkvpOffY;
					return;
				}
				if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
					st->pickPending = true;
					st->pickX = e->GetX() - (int32)nkvpOffX;
					st->pickY = e->GetY() - (int32)nkvpOffY;
				}
				// CURSEUR 3D façon Blender : Shift + clic DROIT le place sous la souris
				// (raycast sur la surface visible, repli sur le plan du sol y=0). Le clic
				// droit SEUL reste libre (regard de la caméra fly).
				// Clic DROIT pendant une operation MODALE : annulation (convention Blender).
				if (e->GetButton() == NkMouseButton::NK_MB_RIGHT && st->modalOp != 0 &&
					!(NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT)))
					st->modalCancelPending = true;
				if (e->GetButton() == NkMouseButton::NK_MB_RIGHT &&
					(NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT))) {
					st->cursorPlacePending = true;
					st->cursorPX = (float32)e->GetX() - nkvpOffX;
					st->cursorPY = (float32)e->GetY() - nkvpOffY;
				}
			});
			// F : bascule caméra ÉDITEUR (orbit) <-> SIMULATION (fly). En EDIT MODE, F crée
			// une face (n-gon) depuis la sélection -> ne pas basculer la caméra.
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
				if (e->GetKey() == NkKey::NK_F && !st->editMode) {
					st->useSimCam = !st->useSimCam;
					logger.Info("[Demo3D] Camera = {0}\n", st->useSimCam
															   ? "SIMULATION (fly: WASD+clic droit)"
															   : "EDITEUR (orbit: milieu/Shift+milieu/molette)");
				}
			});
			// Pavé numérique façon Blender : 1=FRONT (Ctrl=BACK) · 3=RIGHT (Ctrl=LEFT) ·
			// 7=TOP (Ctrl=BOTTOM). Snap de la caméra éditeur.
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
				auto &c = st->editorCam;
				const NkVec3f t = c.GetTarget();
				const float32 d = c.GetDistance();
				const float32 P = 1.55f; // ~90° (clamp pitch)
				const bool ctrl = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
				const NkKey k = e->GetKey();
				bool axisView = false;
				if (k == NkKey::NK_NUMPAD_1) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 1.5708f, 0.f);
						logger.Info("[Demo3D] Vue FRONT (ortho)\n");
					} else {
						c.SetCenter(t, d, -1.5708f, 0.f);
						logger.Info("[Demo3D] Vue BACK (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_3) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 0.f, 0.f);
						logger.Info("[Demo3D] Vue RIGHT (ortho)\n");
					} else {
						c.SetCenter(t, d, 3.1416f, 0.f);
						logger.Info("[Demo3D] Vue LEFT (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_7) {
					axisView = true;
					if (!ctrl) {
						c.SetCenter(t, d, 0.f, P);
						logger.Info("[Demo3D] Vue TOP (ortho)\n");
					} else {
						c.SetCenter(t, d, 0.f, -P);
						logger.Info("[Demo3D] Vue BOTTOM (ortho)\n");
					}
				} else if (k == NkKey::NK_NUMPAD_5) {
					st->orthoView = !st->orthoView;
					logger.Info("[Demo3D] Projection = {0}\n", st->orthoView ? "ORTHO" : "PERSPECTIVE");
				} // pavé 5 = toggle ortho/persp
				// ── NK_GI_TEST : pilotage du mur rouge et A/B de l'indirect ──────
				// Touches actives UNIQUEMENT sous l'override, pour ne rien voler au
				// keymap habituel de la démo.
				else if (st->giTest && (k == NkKey::NK_NUMPAD_4 || k == NkKey::NK_NUMPAD_6 ||
										k == NkKey::NK_NUMPAD_8 || k == NkKey::NK_NUMPAD_2)) {
					const float32 step = 0.35f;
					if (k == NkKey::NK_NUMPAD_4)
						st->giWallOffset.x -= step;
					else if (k == NkKey::NK_NUMPAD_6)
						st->giWallOffset.x += step;
					else if (k == NkKey::NK_NUMPAD_8)
						st->giWallOffset.z -= step;
					else
						st->giWallOffset.z += step;
					st->giDirty = true; // le GI est recalculé : l'indirect SUIT le mur
				} else if (st->giTest && k == NkKey::NK_NUMPAD_0) {
					st->giOn = !st->giOn;
					st->giDirty = true;
					logger.Info("[Demo3D] GI {0}\n", st->giOn ? "ON" : "OFF");
				} else if (st->giTest && k == NkKey::NK_NUMPAD_9) {
					st->giAuto = !st->giAuto;
					logger.Info("[Demo3D] va-et-vient auto du mur : {0}\n", st->giAuto ? "ON" : "OFF");
				}
				if (axisView)
					st->orthoView = true; // vue axiale -> ortho auto (façon Blender)
			});
			// Réglages viewport/debug sur F-keys (hors keymap Blender essentiel) :
			//   F1=grille on/off · F2/F3/F4=grille internes/majeures/axes · F11/F12=opacité plan -/+
			//   V=VSync
			NkEvents().AddEventCallback<NkKeyPressEvent>([renderer, st](NkKeyPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
				const NkKey k = e->GetKey();
				if (k == NkKey::NK_V) {
					static bool vsync = true;
					vsync = !vsync;
					renderer->SetVSync(vsync);
					logger.Info("[Demo3D] VSync = {0}\n", vsync);
				}
				if (auto *r3d = renderer->GetRender3D()) {
					// Z (hors drag, SANS Alt) = cycle mode d'affichage façon Blender :
					// RENDERED -> SOLID -> WIREFRAME. (En drag, Z = verrou d'axe ;
					// Alt+Z = toggle X-ray en Edit Mode, géré dans le keymap.)
					const bool altHeld = NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
					const bool ctrlHeld = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
					// !ctrl : Ctrl+Z est réservé à l'UNDO en edit mode (ne pas cycler l'affichage).
					if (k == NkKey::NK_Z && !altHeld && !ctrlHeld && !st->gizmo.IsDragging() &&
						!st->editGizmo.IsDragging()) {
						st->shadingMode = (st->shadingMode + 1) % 6;
						const char *sm[6] = {"RENDERED", "SOLID", "WIREFRAME", "NORMAL", "UV", "AO"};
						// viewMode shader : 0=PBR éclairé, 1=matcap unlit, 2=normal, 3=uv, 4=ao.
						const int32 vm[6] = {0, 1, 1, 2, 3, 4};
						r3d->SetWireframe(st->shadingMode == 2); // wireframe = rasterizer fil de fer
						r3d->SetViewMode(vm[st->shadingMode]);
						logger.Info("[Demo3D] Affichage = {0}\n", sm[st->shadingMode]);
					}
					// B : cycle la SOURCE DE COULEUR en edit/unlit (façon "Color" du mode
					// Solid de Blender) : MATERIAL -> GRIS -> CUSTOM.
					if (k == NkKey::NK_B) {
						st->unlitColorMode = (st->unlitColorMode + 1) % 3;
						const char *cm[3] = {"MATERIAL", "GRIS", "CUSTOM"};
						logger.Info("[Demo3D] Couleur unlit/edit = {0}\n", cm[st->unlitColorMode]);
					}
					// M : cycle le preset MatCap (effet en mode SOLID/WIREFRAME).
					// En EDIT MODE, M = Merge (soudure) -> ne pas cycler le matcap.
					if (k == NkKey::NK_M && !st->editMode) {
						r3d->SetMatcap(r3d->Matcap() + 1);
						// Nom lu dans NkMatcapLibrary : source unique, pas de liste dupliquee
						// ici qui se desynchroniserait du contenu reel de l'atlas.
						logger.Info("[Demo3D] MatCap = {0} ({1}/{2})\n",
									renderer::NkMatcapLibrary::Name(r3d->Matcap()), r3d->Matcap() + 1,
									(int32)renderer::NkRender3D::kMatcapCount);
					}
					auto &g = r3d->GetInfiniteGridParams();
					if (k == NkKey::NK_F1) {
						// PORTAGE NK3DModeler : F1 bascule la VOLONTE (nkvpGridOn), pas
						// seulement l'etat moteur -- en ortho ce dernier est deja coupe.
						bool on = !nkvpGridOn;
						nkvpGridOn = on;
						r3d->SetInfiniteGridEnabled(on);
						logger.Info("[Demo3D] Grille = {0}\n", on);
					}
					if (k == NkKey::NK_F2) {
						g.showMinor = !g.showMinor;
						logger.Info("[Demo3D] Grille internes = {0}\n", g.showMinor);
					}
					if (k == NkKey::NK_F3) {
						g.showMajor = !g.showMajor;
						logger.Info("[Demo3D] Grille majeures = {0}\n", g.showMajor);
					}
					if (k == NkKey::NK_F4) {
						g.showAxes = !g.showAxes;
						logger.Info("[Demo3D] Grille axes = {0}\n", g.showAxes);
					}
					if (k == NkKey::NK_F11) {
						g.cellColor.w = NkMax(0.0f, g.cellColor.w - 0.05f);
						logger.Info("[Demo3D] Opacite plan = {0}\n", g.cellColor.w);
					}
					if (k == NkKey::NK_F12) {
						g.cellColor.w = NkMin(1.0f, g.cellColor.w + 0.05f);
						logger.Info("[Demo3D] Opacite plan = {0}\n", g.cellColor.w);
					}
					// F5/F6 = sauver/charger la SESSION d'édition -> ne marchent qu'en EDIT MODE.
					if ((k == NkKey::NK_F5 || k == NkKey::NK_F6) && !st->editMode)
						logger.Info("[Demo3D] {0} : entre d'abord en EDIT MODE (Tab) sur un objet selectionne\n",
									k == NkKey::NK_F5 ? "F5 (sauver session)" : "F6 (charger session)");
				}
			});
			// ── KEYMAP GIZMO façon Blender ────────────────────────────────────────
			//   G / R / S = translate / rotate / scale (hors drag)  ·  C = combiné  ·  TAB = cycle
			//   Alt+G / Alt+R / Alt+S = efface translation / rotation / échelle des sélectionnés
			//   A = tout sélectionner  ·  Alt+A = tout désélectionner  ·  , = orientation (G/L/N)
			//   (pendant un drag : X/Y/Z = verrou d'axe, Ctrl = snap)
			NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
				const NkKey k = e->GetKey();
				const bool alt = NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
				const char *mn[4] = {"TRANSLATE", "ROTATE", "SCALE", "COMBINE (T+R+S)"};
				using GZ = renderer::NkGizmo3D;
				// SHIFT+TAB : bascule l'AIMANTATION, comme Blender — et comme lui, Ctrl
				// l'INVERSE ensuite le temps d'un geste. Testé AVANT le TAB nu, sinon la
				// combinaison entrerait en mode édition. Réglée sur les DEUX gizmos : une
				// aimantation qui ne vaudrait que dans un mode serait pire qu'aucune.
				if (k == NkKey::NK_TAB &&
					(NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT))) {
					const bool on = !st->gizmo.IsSnapEnabled();
					st->gizmo.SetSnapEnabled(on);
					st->editGizmo.SetSnapEnabled(on);
					logger.Info("[Demo3D] Aimantation = {0} (pas {1}, grille {2}) — Ctrl inverse\n",
								on ? "ON" : "off", st->gizmo.SnapTranslate(),
								st->gizmo.IsSnapAbsolute() ? "ABSOLUE" : "increment");
					return;
				}
				// TAB : bascule OBJET <-> EDIT MODE. Traité côté frame (accès meshSys pour
				// cloner le mesh de l'objet sélectionné). Façon Blender.
				if (k == NkKey::NK_TAB) {
					st->editTogglePending = true;
					return;
				}
				const bool shiftG = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
				// ── OMBRAGE FLAT / SMOOTH (Blender : menu Object > Shade Flat/Smooth, ou
				//    Mesh > Shading en Edit Mode). Blender n'a PAS de raccourci direct : on
				//    prend Shift+S = SMOOTH et Shift+F = FLAT (S et F seules restent l'échelle
				//    et « créer une face »). Traité côté frame (accès meshSys pour resync).
				// !alt : Shift+ALT+S est reserve a TO SPHERE (edition spherique, cf. plus bas)
				// -> sans ce garde, l'ombrage smooth avalerait la combinaison.
				if (shiftG && !alt && (k == NkKey::NK_S || k == NkKey::NK_F)) {
					if (!st->editMode) {
						// LIMITE ASSUMÉE : hors édition, les objets partagent la MÊME primitive
						// GPU — changer leur ombrage la modifierait pour tous. On passe donc par
						// l'Edit Mode (le maillage y est cloné) ; le résultat PERSISTE ensuite en
						// mode objet, l'objet adoptant son mesh édité à la sortie (TAB).
						logger.Info("[Demo3D] Ombrage : entre en EDIT MODE (Tab) sur l'objet, puis "
									"Shift+S (smooth) / Shift+F (flat)\n");
					} else
						st->editShadePending = (k == NkKey::NK_S) ? 1 : 2;
					return;
				}
				// ── POINT DE PIVOT (Blender : touche `.`) — cycle les 5 modes ────────
				//    Alt+`.` = remet le CURSEUR 3D à l'origine du monde.
				//    (Le placement du curseur 3D = Shift + clic DROIT, cf. callback souris.)
				if (k == NkKey::NK_PERIOD) {
					if (alt) {
						st->cursor3D = {0.f, 0.f, 0.f};
						logger.Info("[Demo3D] Curseur 3D remis a l'origine\n");
					} else {
						// Les DEUX gizmos (objet + édition) partagent le mode : un seul réglage
						// utilisateur, valable en mode OBJET comme en mode ÉDITION.
						st->gizmo.CyclePivot();
						st->editGizmo.SetPivotMode(st->gizmo.PivotMode());
						logger.Info("[Demo3D] Point de pivot = {0}\n", st->gizmo.PivotName());
					}
					return;
				}
				// En EDIT MODE : touches 1/2/3 = sous-mode sélection VERTEX / EDGE / FACE.
				if (st->editMode) {
					// 1/2/3 = mode SEUL (vertex/arête/face) ; Shift+1/2/3 = COMBINER (toggle),
					// façon Blender (on peut avoir plusieurs modes actifs à la fois).
					{
						const bool shiftK = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						int32 bit = (k == NkKey::NK_NUM1)	? 1
									: (k == NkKey::NK_NUM2) ? 2
									: (k == NkKey::NK_NUM3) ? 4
															: 0;
						if (bit) {
							if (shiftK)
								st->editSelMask ^= bit;
							else
								st->editSelMask = bit;
							if (st->editSelMask == 0)
								st->editSelMask = bit; // toujours >=1 mode actif
							st->editOverlayDirty = true;
							logger.Info("[Demo3D] Edit modes = {0}{1}{2}\n", (st->editSelMask & 1) ? "V" : "-",
										(st->editSelMask & 2) ? "E" : "-", (st->editSelMask & 4) ? "F" : "-");
							return;
						}
					}
					// ── BEVEL / CHANFREIN (façon Blender) ──────────────────────────
					// Ctrl+B = bevel d'ARÊTE · Ctrl+Shift+B = bevel de SOMMET (Blender à
					// l'identique). B SEULE reste la sélection RECTANGLE : on intercepte donc
					// AVANT elle, et on ajoute Alt+B / Alt+Shift+B pour régler les propriétés
					// de l'outil (segments / largeur), à la place de la molette modale.
					if (k == NkKey::NK_B) {
						const bool shiftB = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						const bool ctrlB = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
						if (ctrlB) {
							// MODAL (façon Blender) : lance l'operation en APERCU ; la souris regle la
							// largeur, la molette les segments, clic gauche confirme, Echap/clic droit annule.
							st->modalStartPending = shiftB ? 2 : 1;
							return;
						}
						if (alt) {
							if (shiftB) {
								// Largeur : AUTO (0) -> 3 % -> 6 % -> 10 % -> AUTO. Les valeurs sont
								// des fractions de la diagonale de bbox, résolues côté NkEditMesh.
								const float32 cyc[4] = {0.f, 0.05f, 0.12f, 0.25f};
								int32 i = 0;
								for (int32 j = 0; j < 4; j++)
									if (st->bevelOffset == cyc[j])
										i = j;
								st->bevelOffset = cyc[(i + 1) % 4];
								logger.Info("[Demo3D] Bevel largeur = {0}\n",
											st->bevelOffset <= 0.f ? "AUTO (6% bbox)" : "manuelle");
							} else {
								const int32 cyc[5] = {1, 2, 3, 4, 6};
								int32 i = 0;
								for (int32 j = 0; j < 5; j++)
									if (st->bevelSegments == cyc[j])
										i = j;
								st->bevelSegments = cyc[(i + 1) % 5];
								logger.Info("[Demo3D] Bevel segments = {0}\n", st->bevelSegments);
							}
							return;
						}
					}
					// ── OUTILS DE SÉLECTION (façon Blender) ────────────────────────
					// B = arme la sélection RECTANGLE (le prochain glisser trace la boîte).
					// C = bascule le mode CERCLE (on « peint » en maintenant le clic ;
					//     molette = rayon). Échap = quitte l'outil courant.
					// (Le LASSO n'a pas de touche : Ctrl + glisser, comme dans Blender.)
					if (k == NkKey::NK_B) {
						st->selTool = (st->selTool == 1) ? 0 : 1;
						st->selDragging = false;
						logger.Info("[Demo3D] Selection RECTANGLE : {0}\n", st->selTool == 1 ? "ARMEE (glisser)" : "off");
						return;
					}
					if (k == NkKey::NK_C) {
						st->selTool = (st->selTool == 3) ? 0 : 3;
						st->selDragging = false;
						logger.Info("[Demo3D] Selection CERCLE : {0}\n",
									st->selTool == 3 ? "ON (clic=peindre, molette=rayon)" : "off");
						return;
					}
					// Echap pendant une operation MODALE : annulation (retour a l'etat initial).
					if (k == NkKey::NK_ESCAPE && st->modalOp != 0) {
						st->modalCancelPending = true;
						return;
					}
					if (k == NkKey::NK_ESCAPE && st->selTool != 0) {
						st->selTool = 0;
						st->selDragging = false;
						st->selLasso.Clear();
						logger.Info("[Demo3D] Outil de selection : annule\n");
						return;
					}
					// Alt+Z : toggle X-RAY (voir/sélectionner à travers le mesh), façon Blender.
					if (k == NkKey::NK_Z && alt) {
						st->editXray = !st->editXray;
						st->editOverlayDirty = true;
						logger.Info("[Demo3D] X-ray = {0}\n", st->editXray);
						return;
					}
					// Ctrl+Z = ANNULER · Ctrl+Shift+Z / Ctrl+Y = RÉTABLIR (historique d'édition).
					// Traité côté frame (accès meshSys pour resync). Façon Blender.
					{
						const bool ctrlZ = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
						const bool shiftZ = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						if (ctrlZ && k == NkKey::NK_Z) {
							if (shiftZ)
								st->editRedoPending = true;
							else
								st->editUndoPending = true;
							return;
						}
						if (ctrlZ && k == NkKey::NK_Y) {
							st->editRedoPending = true;
							return;
						}
					}
					// P : REJOUE le journal des commandes depuis le maillage de base (preuve que
					// la couche de commandes est scriptable -> modificateurs + données IA).
					if (k == NkKey::NK_P) {
						st->editReplayPending = true;
						return;
					}
					// F5 / F6 : SAUVER / CHARGER la session (journal sérialisé sur disque).
					if (k == NkKey::NK_F5) {
						st->editSavePending = true;
						return;
					}
					if (k == NkKey::NK_F6) {
						st->editLoadPending = true;
						return;
					}
					// F7 = AJOUTER le modificateur du type courant · F8/F9 = changer de type
					// · F10 = vider la pile. Le type courant est journalise a chaque
					// changement : sans cela, avec 17 entrees, on ne saurait pas ce qu'on
					// s'apprete a ajouter.
					if (k == NkKey::NK_F7) {
						st->editAddModPending = st->editModTypePick;
						return;
					}
					if (k == NkKey::NK_F8 || k == NkKey::NK_F9) {
						const int32 last = (int32)renderer::NkModifierType::SmoothByAngle;
						st->editModTypePick += (k == NkKey::NK_F9) ? 1 : -1;
						if (st->editModTypePick < 0)
							st->editModTypePick = last;
						if (st->editModTypePick > last)
							st->editModTypePick = 0;
						uint32 np = 0;
						renderer::NkModifierParams((renderer::NkModifierType)st->editModTypePick, np);
						logger.Info("[Demo3D] Type a ajouter (F7) = [{0}] {1} — {2} parametres\n", st->editModTypePick,
									renderer::NkModifierTypeName((renderer::NkModifierType)st->editModTypePick), np);
						return;
					}
					if (k == NkKey::NK_F10) {
						st->editClearModPending = true;
						return;
					}
					// Réglage du modificateur ACTIF : ] augmente · [ diminue le param principal ·
					// \ change de modificateur actif (Mirror=axe, Array=copies, Subsurf=niveaux).
					if (k == NkKey::NK_RBRACKET) {
						st->editModAdjustPending = +1;
						return;
					}
					if (k == NkKey::NK_LBRACKET) {
						st->editModAdjustPending = -1;
						return;
					}
					// ── GESTION DE LA PILE (Blender : panneau Modificateurs) ─────────
					// L'ORDRE de la pile est un parametre de RESULTAT — miroir puis tableau
					// ne donne pas la meme chose que tableau puis miroir — donc deplacer une
					// entree n'est pas un confort d'interface. Toutes ces touches sont sous
					// SHIFT pour ne pas voler les raccourcis d'edition existants :
					//   Shift+\   parametre suivant du modificateur actif
					//   Shift+Haut / Shift+Bas   monter / descendre dans la pile
					//   Shift+E   activer/desactiver     Shift+D   dupliquer
					//   Shift+Suppr  retirer             Shift+Entree  APPLIQUER (destructif)
					if (k == NkKey::NK_BACKSLASH && shiftG) {
						st->editModParamCyclePending = true;
						return;
					}
					if (shiftG && (k == NkKey::NK_UP || k == NkKey::NK_DOWN)) {
						st->editModStackOp = (k == NkKey::NK_UP) ? 1 : 2;
						return;
					}
					if (shiftG && k == NkKey::NK_E) {
						st->editModStackOp = 3;
						return;
					}
					if (shiftG && k == NkKey::NK_D) {
						st->editModStackOp = 4;
						return;
					}
					if (shiftG && k == NkKey::NK_DELETE) {
						st->editModStackOp = 5;
						return;
					}
					if (shiftG && k == NkKey::NK_ENTER) {
						st->editModStackOp = 6; // APPLIQUER : destructif, snapshot d'annulation pose
						return;
					}
					if (k == NkKey::NK_BACKSLASH) {
						st->editModCyclePending = true;
						return;
					}
					// A / Alt+A : tout sélectionner / désélectionner (les VERTICES).
					if (k == NkKey::NK_A) {
						const uint8 v = alt ? 0 : 1;
						for (uint32 i = 0; i < (uint32)st->vertSel.Size(); i++)
							st->vertSel[i] = v;
						st->editOverlayDirty = true;
						return;
					}
					// Outils topologie (hors drag). Shift+touche = règle la PROPRIÉTÉ de l'outil
					// (façon Blender) ; touche seule = applique.
					if (!st->editGizmo.IsDragging()) {
						const bool shiftK = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
						const bool ctrlK = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
						// Ctrl+R = LOOP CUT (boucle d'arêtes) depuis l'arête sélectionnée.
						// Ctrl+Shift+R = règle le NOMBRE de coupes (1..5), façon molette Blender.
						if (k == NkKey::NK_R && ctrlK) {
							if (shiftK) {
								st->loopCuts = (st->loopCuts % 5) + 1;
								logger.Info("[Demo3D] Loop cut : {0} coupe(s)\n", st->loopCuts);
							} else
								st->modalStartPending = 4; // MODAL : anneau previsualise au survol, molette = nb de coupes
							return;
						}
						if (k == NkKey::NK_E) {
							if (shiftK) {
								st->extrudeIndividual = !st->extrudeIndividual;
								logger.Info("[Demo3D] Extrude = {0}\n",
											st->extrudeIndividual ? "INDIVIDUEL" : "REGION");
							} else
								st->modalStartPending = 6; // MODAL : la souris tire l'extrusion le long de la normale
							return;
						}
						if (k == NkKey::NK_X) {
							// Ctrl+X = DISSOLVE contextuel (Blender) : fusionne au lieu de trouer.
							// X seule reste « supprimer les faces » (déjà en place).
							if (ctrlK)
								st->editDissolvePending = 1;
							else
								st->editDeletePending = true;
							return;
						}
						if (k == NkKey::NK_M) {
							if (shiftK) {
								st->mergeMode = (st->mergeMode + 1) % 6; // 6 modes facon Blender
								const char *mm[6] = {"CENTER", "FIRST", "LAST", "AT CURSOR", "COLLAPSE", "BY DISTANCE"};
								logger.Info("[Demo3D] Merge = {0}\n", mm[st->mergeMode]);
							} else
								st->editMergePending = true;
							return;
						}
						if (k == NkKey::NK_F) {
							st->editMakeFacePending = true;
							return;
						}
						// J = SPIN / RÉVOLUTION. Blender n'a PAS de raccourci par défaut pour
						// Spin (menu Mesh > Spin) : on prend J, libre chez nous. Shift+J = pas,
						// Alt+J = angle. Le centre est le CURSEUR 3D, comme dans Blender.
						if (k == NkKey::NK_J) {
							if (shiftK) {
								const int32 cyc[4] = {6, 12, 24, 32};
								int32 i = 0;
								for (int32 j = 0; j < 4; j++)
									if (st->spinSteps == cyc[j])
										i = j;
								st->spinSteps = cyc[(i + 1) % 4];
								logger.Info("[Demo3D] Spin pas = {0}\n", st->spinSteps);
							} else if (alt) {
								const float32 cyc[3] = {360.f, 180.f, 90.f};
								int32 i = 0;
								for (int32 j = 0; j < 3; j++)
									if (st->spinAngleDeg == cyc[j])
										i = j;
								st->spinAngleDeg = cyc[(i + 1) % 3];
								logger.Info("[Demo3D] Spin angle = {0} deg\n", st->spinAngleDeg);
							} else
								st->modalStartPending = 5; // MODAL : souris = angle, molette = nb de pas
							return;
						}
						// V = EDGE SPLIT (Blender met « rip » sur V ; V était libre chez nous,
						// on y place la dé-soudure d'arêtes, qui en est la variante topologique).
						// Shift+V = cycle l'écartement de la déchirure.
						if (k == NkKey::NK_V) {
							if (shiftK) {
								const float32 cyc[3] = {0.f, 0.15f, 0.35f};
								int32 i = 0;
								for (int32 j = 0; j < 3; j++)
									if (st->splitGap == cyc[j])
										i = j;
								st->splitGap = cyc[(i + 1) % 3];
								logger.Info("[Demo3D] Edge split ecart = {0}\n",
											st->splitGap <= 0.f ? "AUTO (1% bbox)" : "large");
							} else
								st->editSplitPending = true;
							return;
						}
						// I = INSET FACES (Blender à l'identique — I était libre chez nous).
						// Shift+I = bascule INDIVIDUEL / RÉGION · Alt+I = cycle la profondeur.
						if (k == NkKey::NK_I) {
							if (shiftK) {
								st->insetIndividual = !st->insetIndividual;
								logger.Info("[Demo3D] Inset = {0}\n", st->insetIndividual ? "INDIVIDUEL" : "REGION");
							} else if (alt) {
								const float32 cyc[3] = {0.f, -0.2f, 0.2f}; // plat · creux · bossage
								int32 i = 0;
								for (int32 j = 0; j < 3; j++)
									if (st->insetDepth == cyc[j])
										i = j;
								st->insetDepth = cyc[(i + 1) % 3];
								logger.Info("[Demo3D] Inset profondeur = {0}\n", st->insetDepth);
							} else
								st->modalStartPending = 3; // MODAL : la souris regle l'epaisseur
							return;
						}
						// ── EDITION SPHERIQUE / RADIALE (MODALES) ─────────────────────
						// Shift+Alt+S = TO SPHERE (raccourci IDENTIQUE a Blender).
						// Ctrl+Alt+S  = SHRINK/FATTEN. Blender utilise Alt+S, mais Alt+S est
						// DEJA PRIS ici par « effacer l'echelle du gizmo » (et Shift+S par
						// l'ombrage smooth) : on decale donc sur Ctrl+Alt+S plutot que de
						// casser un raccourci existant. Les deux passent par le MEME cadre
						// modal que bevel/inset/loop cut : aucun code specifique.
						if (k == NkKey::NK_S && alt && (shiftK || ctrlK)) {
							st->modalStartPending = shiftK ? 7 : 8;
							return;
						}
						if (k == NkKey::NK_W) {
							if (shiftK) {
								st->subdivCuts = (st->subdivCuts % 4) + 1;
								logger.Info("[Demo3D] Subdiv coupes = {0}\n", st->subdivCuts);
							} else
								st->editSubdivPending = true;
							return;
						}
						// K : arme le COUTEAU/BISECT (les 2 prochains clics tracent la ligne de coupe).
						if (k == NkKey::NK_K) {
							st->knifeArmed = !st->knifeArmed;
							st->knifeHasP0 = false;
							logger.Info("[Demo3D] Couteau = {0}\n", st->knifeArmed ? "ARME (clic 2 points)" : "off");
							return;
						}
					}
				}
				// Gizmo ACTIF selon le mode : objet, vertices, ou LUMIERE.
				// Une lumiere selectionnee capte G/R/S exactement comme un objet — c'est
				// le modele Blender, ou une lampe EST un objet de la scene. Mais toutes
				// les manipulations n'ont pas d'effet sur elle, d'ou le filtre ci-dessous.
				const bool lightActive =
					(!st->editMode && st->lightSel >= 0 && st->lightSel < Demo3DState::kNumLights);
				renderer::NkGizmo3D &G =
					st->editMode ? st->editGizmo : (lightActive ? st->lightGizmo : st->gizmo);
				if (G.IsDragging())
					return; // en plein drag : X/Y/Z = verrou (pas de switch)
				// G/R/S NE SONT JAMAIS REFUSES sur une lumiere. Comme dans Blender, une
				// lampe EST un objet de la scene : elle se deplace, tourne et se
				// redimensionne toujours. Le type ne restreint pas le GESTE, il determine
				// seulement son EFFET sur l'image — et c'est cela qu'on dit, au lieu
				// d'ignorer la touche.
				// Premiere version : la touche etait REFUSEE quand la manipulation n'avait
				// pas d'effet visible. Erreur de modele : on deplace un soleil pour le
				// RANGER dans la scene, pas pour changer l'eclairage, et refuser empechait
				// aussi de bouger son WIDGET. Uniformite du geste d'abord.
				if (lightActive && !alt) {
					using LGZ = renderer::NkLightGizmo;
					const renderer::NkLightDesc &SL = st->lights[st->lightSel];
					if (k == NkKey::NK_G && !LGZ::CanTranslate(SL.type))
						logger.Info("[Demo3D][LUMIERE] deplacement autorise, mais l'eclairage ne changera pas : "
									"une directionnelle n'utilise que sa direction.\n");
					if (k == NkKey::NK_R && !LGZ::CanRotate(SL.type))
						logger.Info("[Demo3D][LUMIERE] rotation autorisee, mais l'eclairage ne changera pas : "
									"une ponctuelle rayonne dans toutes les directions.\n");
					// L'echelle d'une lumiere ne l'agrandit pas : elle regle sa PORTEE
					// (point/spot) ou ses DIMENSIONS (area). Le dire evite de chercher
					// pourquoi le widget ne « grossit » pas comme un objet.
					if (k == NkKey::NK_S) {
						const renderer::NkLightScaleMeaning sm = LGZ::ScaleMeaning(SL.type);
						logger.Info("[Demo3D][LUMIERE] echelle -> {0}\n",
									sm == renderer::NkLightScaleMeaning::Range		  ? "portee"
									: sm == renderer::NkLightScaleMeaning::Dimensions ? "dimensions"
																				  : "aucun parametre (directionnelle)");
					}
				}
				if (k == NkKey::NK_G) {
					if (alt)
						G.ClearSelectedTranslate();
					else
						G.SetMode(GZ::MODE_TRANSLATE);
				}
				if (k == NkKey::NK_R) {
					if (alt)
						G.ClearSelectedRotation();
					else
						G.SetMode(GZ::MODE_ROTATE);
				}
				if (k == NkKey::NK_S) {
					if (alt)
						G.ClearSelectedScale();
					else
						G.SetMode(GZ::MODE_SCALE);
				}
				if (k == NkKey::NK_C && !NkInput.IsKeyDown(NkKey::NK_LCTRL) &&
					!NkInput.IsKeyDown(NkKey::NK_RCTRL)) // Ctrl+C = COPIER, pas le gizmo
					G.SetMode(GZ::MODE_COMBINE);
				if (k == NkKey::NK_A) {
					if (alt)
						G.ClearSelection();
					else
						G.SelectAll();
				}
				if (k == NkKey::NK_COMMA) {
					G.CycleOrientation();
					const char *o[3] = {"GLOBAL", "LOCAL", "NORMAL"};
					logger.Info("[Demo3D] Orientation = {0}\n", o[G.Orientation()]);
				}
				if (k == NkKey::NK_G || k == NkKey::NK_R || k == NkKey::NK_S ||
					(k == NkKey::NK_C && !NkInput.IsKeyDown(NkKey::NK_LCTRL) &&
					 !NkInput.IsKeyDown(NkKey::NK_RCTRL)))
					logger.Info("[Demo3D] Gizmo mode = {0}\n", mn[G.Mode()]);
			});
			if (shadowSys) {
				// ── Scène CLOSE : AUTO-FIT de la cascade directionnelle aux casters ──
				// La cascade est ajustée chaque frame aux bornes RÉELLES de tous les casters
				// (sphères + grille 8x8 de cubes + poteaux) : centre ancré au monde (pas de
				// swimming) + rayon au plus serré (couverture complète SANS gaspiller la
				// résolution). Un rayon fixe trop grand perdait les petites ombres ; un rayon
				// suivant la caméra les faisait glisser/disparaître. L'auto-fit résout les deux.
				shadowSys->GetConfig().autoFitDirectional = true;
				NkEvents().AddEventCallback<NkKeyPressEvent>([shadowSys, st](NkKeyPressEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
					auto &cfg = shadowSys->GetConfig();
					// Debug ombres sur F-keys (libère [ ] P N M R pour le keymap Blender) :
					//   F5/F6 = bias -/+ (maintenu = continu) · F7 = cycle PCF ·
					//   F8/F9 = softness -/+ · F10 = reset ombres.
					switch (e->GetKey()) {
						case NkKey::NK_F5:
							if (!st->biasDownHeld)
								cfg.shadowBias = NkMax(0.0001f, cfg.shadowBias - 0.0005f);
							st->biasDownHeld = true;
							break;
						case NkKey::NK_F6:
							if (!st->biasUpHeld)
								cfg.shadowBias += 0.0005f;
							st->biasUpHeld = true;
							break;
						case NkKey::NK_F7:
							st->pcfIdx = (st->pcfIdx + 1) % 5;
							cfg.quality = (NkVSMShadowQuality)st->pcfIdx;
							break;
						case NkKey::NK_F8:
							cfg.softness = NkMax(0.0005f, cfg.softness - 0.001f);
							break;
						case NkKey::NK_F9:
							cfg.softness = NkMin(0.020f, cfg.softness + 0.001f);
							break;
						case NkKey::NK_F10:
							cfg.shadowBias = 0.001f;
							cfg.softness = 0.003f;
							cfg.quality = NkVSMShadowQuality::PCF5x5;
							st->pcfIdx = (int32)NkVSMShadowQuality::PCF5x5;
							break;
						default:
							break;
					}
				});
				// Relache F5 / F6 -> stoppe l'evolution continue du bias.
				NkEvents().AddEventCallback<NkKeyReleaseEvent>([st](NkKeyReleaseEvent *e) {
				// PORTAGE NK3DModeler : la vue n'ecoute que si elle est concernee.
				if (!nkvpInputOn || !nkvpHover)
					return;
					if (e->GetKey() == NkKey::NK_F5)
						st->biasDownHeld = false;
					if (e->GetKey() == NkKey::NK_F6)
						st->biasUpHeld = false;
				});
			}

			// ── Grille infinie style Blender (remplace la DrawDebugGrid finie) ──────
			// Intérieur des cellules gris SEMI-transparent (cellColor.w) : on voit à
			// travers mais les lignes restent visibles. Axes X rouge / Z bleu sur le plan.
			if (auto *r3d = ctx.renderer->GetRender3D()) {
				r3d->SetInfiniteGridEnabled(true);
				auto &g = r3d->GetInfiniteGridParams();
				g.cellSize = 1.0f;
				g.majorEvery = 10.0f;
				g.fadeEnd = 10.0f; // FACTEUR de portée : rayon net ~ hauteur_cam * 10 (proportionnel)
				g.planeY = 0.01f;  // 1 cm au-dessus du sol solide -> grille visible, pas de z-fight
				g.lineColor = {0.42f, 0.45f, 0.52f,
							   1.0f}; // gris moyen : bien visible sur fond sombre MAIS sous le seuil du bloom
				g.cellColor = {0.09f, 0.10f, 0.12f, 0.18f}; // intérieur = PLAN INFINI (.w=opacité ; 0=transparent)
				g.axisXColor = {1.0f, 0.0f, 0.0f, 1.0f};	// X rouge PLEIN
				g.axisZColor = {0.0f, 0.0f, 1.0f, 1.0f};	// Z bleu PLEIN
				// Axes du SHADER grille DÉSACTIVÉS : on dessine les 3 axes X/Y/Z en lignes 3D
				// réelles (DrawDebugLine, cf. Frame). Raison : l'axe Y en projection écran dans
				// le FS avait des artefacts (quittait l'origine / pas parallèle aux verticales
				// en perspective). Une vraie ligne 3D est correcte partout (perspective, ancrée,
				// top/bottom) ET cohérente en épaisseur pour les 3.
				g.showAxes = false;
			}

			// ── Caméras réutilisables du moteur ──────────────────────────────────
			// Éditeur (Blender) : orbit autour de (0,0.5,0). Simulation (fly) : recul sur -Z.
			// NK_CAM_DIST : recule la caméra orbit (rayon) pour les captures de test. Défaut 6.5.
			float32 camRadius = 6.5f;
			if (const char *cd = getenv("NK_CAM_DIST")) {
				float32 v = (float32)atof(cd);
				if (v > 0.5f)
					camRadius = v;
			}
			// NK_CAM_YAW / NK_CAM_PITCH (radians) : pose d'orbite déterministe pour les
			// captures — indispensable pour reproduire un bug « dépendant de l'angle de vue ».
			float32 camYaw = 0.7f, camPitch = 0.4f;
			if (const char *cy = getenv("NK_CAM_YAW"))
				camYaw = (float32)atof(cy);
			if (const char *cp = getenv("NK_CAM_PITCH"))
				camPitch = (float32)atof(cp);
			// NK_CAM_TARGET="x,y,z" : recentre l'orbite sur un point donné (défaut (0,0.5,0)).
			// Indispensable pour capturer un objet ÉLOIGNÉ de l'origine (ex. colonne #18 en
			// (1,1,4)) au même cadrage que l'utilisateur, donc pour reproduire les bugs
			// dépendants de la position de l'objet.
			NkVec3f camCenter{0.f, 0.5f, 0.f};
			if (const char *ct = getenv("NK_CAM_TARGET")) {
				float32 tv[3] = {camCenter.x, camCenter.y, camCenter.z};
				int32 k = 0;
				const char *p = ct;
				while (k < 3 && *p) {
					tv[k++] = (float32)atof(p);
					while (*p && *p != ',')
						p++;
					if (*p == ',')
						p++;
				}
				camCenter = {tv[0], tv[1], tv[2]};
			}
			st->editorCam.SetCenter(camCenter, camRadius, camYaw, camPitch);
			st->simCam.SetPose({0.f, 1.5f, 6.f}, -1.5708f, -0.15f);

			{
				const char *giEnv = getenv("NK_GI_TEST");
				st->giTest = (giEnv && giEnv[0] && giEnv[0] != '0');
				const char *giInt = getenv("NK_GI_INTENSITY");
				if (giInt && giInt[0])
					st->giIntensity = (float32)atof(giInt);
				// NK_GI_AUTO : démarre le va-et-vient sans clavier — sert à mesurer
				// le coût du GI en mouvement dans une capture automatisée.
				const char *giAuto = getenv("NK_GI_AUTO");
				st->giAuto = (giAuto && giAuto[0] && giAuto[0] != '0');
				if (st->giTest) {
					logger.Info("[Demo3D] === NK_GI_TEST : GI a un rebond (mur rouge mobile) ===\n");
					logger.Info("[Demo3D] PAVE NUM 4/6 : deplacer le mur en X | 8/2 : en Z\n");
					logger.Info("[Demo3D] PAVE NUM 0 : GI on/off (A/B)   9 : va-et-vient auto\n");
					logger.Info("[Demo3D] La lumiere rouge de la scene est RETIREE : tout rouge = rebond\n");
				}
			}

			logger.Info("[Demo3D] Init OK — meshes : sphere={0} plane={1} cube={2}\n", (uint64)st->meshSphere.id,
						(uint64)st->meshPlane.id, (uint64)st->meshCube.id);
			return true;
		}

		// ── NK_GI_TEST : (re)construction de la grille de GI ─────────────────────
		// Re-voxelise le sol + le mur à sa position COURANTE, puis réinjecte
		// l'éclairage. Appelée uniquement quand quelque chose a bougé : l'injection
		// v1 est en CPU, donc on ne la paie pas à chaque frame pour rien. Les deux
		// coûts sont relevés pour être affichés à l'écran — c'est ce qui permet de
		// juger si l'indirect peut suivre une scène animée.
		// Transform de base COMPLÈTE d'un objet : celle de Demo3D_ObjBase, plus le
		// décalage clavier du mur du GI. Toute la chaîne (cible du gizmo, ancre d'Edit
		// Mode, draw, occluder) doit passer par ICI — sinon l'un d'eux travaille sur
		// une position que les autres ignorent, et la cage d'édition se retrouve
		// décalée par rapport à l'objet affiché.
		static NkMat4f Demo3D_ObjBaseFull(const Demo3DState *st, int32 idx) {
			if (st && idx == Demo3DState::kIdxGIWall)
				return NkMat4f::Translate(st->giWallOffset) * Demo3D_ObjBase(idx);
			return Demo3D_ObjBase(idx);
		}

		// AABB MONDE d'un cube unitaire (±0,5) transformé : les 8 coins puis min/max.
		// Sert à faire suivre l'occluder du GI quand le mur est déplacé AU GIZMO
		// (translation, mais aussi rotation ou mise à l'échelle).
		static void Demo3D_XformAABB(const NkMat4f &m, NkVec3f &outMin, NkVec3f &outMax) {
			outMin = {1e30f, 1e30f, 1e30f};
			outMax = {-1e30f, -1e30f, -1e30f};
			for (int32 c = 0; c < 8; c++) {
				const NkVec3f l{(c & 1) ? 0.5f : -0.5f, (c & 2) ? 0.5f : -0.5f, (c & 4) ? 0.5f : -0.5f};
				const NkVec3f p = m * l;
				outMin.x = NkMin(outMin.x, p.x);
				outMin.y = NkMin(outMin.y, p.y);
				outMin.z = NkMin(outMin.z, p.z);
				outMax.x = NkMax(outMax.x, p.x);
				outMax.y = NkMax(outMax.y, p.y);
				outMax.z = NkMax(outMax.z, p.z);
			}
		}

		static void Demo3D_RebuildGI(Demo3DState *st, NkRenderer *renderer, const NkVector<NkLightDesc> &lights) {
			auto *vao = renderer ? renderer->GetVoxelAO() : nullptr;
			if (!vao)
				return;
			vao->Clear();
			NkVoxelOccluder floorOcc;
			floorOcc.minWorld = {-8.f, -0.6f, -8.f};
			floorOcc.maxWorld = {8.f, 0.05f, 8.f};
			floorOcc.opacity = 1.f;
			floorOcc.albedo = {0.5f, 0.5f, 0.5f};
			vao->RegisterOccluder(floorOcc);
			// L'occluder épouse la transform EFFECTIVE du mur (clavier + gizmo) : la
			// lumière rebondit donc toujours exactement sur le mur qu'on voit bouger.
			NkVoxelOccluder wall;
			Demo3D_XformAABB(st->giWallXform, wall.minWorld, wall.maxWorld);
			wall.opacity = 1.f;
			wall.albedo = {0.9f, 0.05f, 0.05f};
			vao->RegisterOccluder(wall);
			vao->Build();
			// GI éteint = injection de zéro : l'opacité (donc l'AO) reste, seul
			// l'indirect disparaît. L'A/B ne change donc QUE ce qu'on veut mesurer.
			vao->SetGIIntensity(st->giOn ? st->giIntensity : 0.f);
			vao->InjectLighting(lights);
			st->giBuildMs = vao->GetLastBuildMs();
			st->giInjectMs = vao->GetLastInjectMs();
		}

		// ── ENTREE EN EDITION SUR UN OBJET ──────────────────────────────────────
		// Extraite telle quelle du corps de frame — MEME code, MEME ordre. C'est le
		// premier pas, et le seul risque, du chantier MULTI-OBJETS : tant que
		// l'entree etait ecrite en ligne au milieu de la frame, il etait impossible
		// de la rejouer pour un SECOND objet sans la dupliquer. Le comportement doit
		// rester strictement identique — verifie par NK_EDIT_IDENTITY (0 partout) et
		// par capture avant/apres.
		//
		// Elle remplit l'etat d'edition COURANT. L'etape suivante consistera a la
		// faire ecrire dans un EMPLACEMENT (Demo3DEditSlot) plutot que directement
		// dans st, puis a garder un emplacement par objet edite.
		static void Demo3D_EnterEditOnObject(Demo3DState *st, renderer::NkMeshSystem *ms,
											 renderer::NkRender3D *r3d, int32 sel) {
			(void)r3d;
			if (sel < 0) {
				logger.Info("[Demo3D] Sélectionne un objet (clic) avant TAB.\n");
			} else {
				// Source = le mesh DÉJÀ édité de l'objet s'il existe (on continue
				// l'édition), sinon la primitive partagée.
				// ⚠️ La règle « <16 = sphère, sinon CUBE » supposait que tout objet
				// non-sphère était un cube. Le sol est un PLAN : on entrait donc en
				// édition sur une cage de cube étirée à 80×1×80, sans rapport avec
				// la géométrie réellement affichée — d'où un « cube invisible » qu'on
				// déplaçait et qui laissait le sol sur place.
				NkMeshHandle prim = st->meshCube;
				if (sel < 16)
					prim = st->meshSphere;
				else if (sel == Demo3DState::kIdxFloor)
					prim = st->meshPlane;
				const bool hadEdit = st->objMesh[sel].IsValid();
				const NkMeshHandle src = hadEdit ? st->objMesh[sel] : prim;
				if (!ms || !ms->HasCPUData(src)) {
					logger.Warn("[Demo3D] Mesh sans copie CPU (keepCPU) — édition impossible.\n");
				} else {
					const uint32 vc = ms->GetVertexCount(src);
					const uint32 ic = ms->GetIndexCount(src);
					const auto *sv = (const renderer::NkVertex3D *)ms->GetVertices(src);
					const uint32 *si = ms->GetIndices(src);
					// AUTORITÉ half-edge n-gon. Si l'objet a DÉJÀ été édité, on reprend sa
					// topologie EXACTE (objHE) : re-dériver depuis les triangles avec
					// quadify perdrait toute face n-gon non reconstituable (face créée
					// avec F non parfaitement plane, n-gon à plus de 4 côtés…) et
					// ferait réapparaître les diagonales de triangulation en fil de fer.
					// Sinon seulement : reconstruction heuristique depuis la primitive.
					if (hadEdit && st->objHasHE[sel] && st->objHE[sel].VertCount() > 0) {
						st->editHE = st->objHE[sel];
						logger.Info("[Demo3D] EDIT MODE : topologie n-gon reprise telle quelle "
									"({0} faces) — aucune re-derivation depuis les triangles\n",
									(int32)st->editHE.FaceCount());
					} else
						st->editHE.BuildFromIndexed(sv, vc, si, ic, /*quadify*/ true);
					// ── NK_EDIT_IDENTITY=1 : contrôle d'ALLER-RETOUR ──────────────
					// Entrer en édition puis en ressortir SANS rien modifier doit être
					// une IDENTITÉ. Ce contrôle compare le maillage re-trianguté au
					// maillage SOURCE, attribut par attribut. Il vaut mieux qu'une
					// comparaison de captures : celle-ci est polluée par le gizmo et le
					// liseré de sélection, qui ne s'affichent QUE dans l'image « après »
					// — j'ai d'abord conclu à tort à un changement de matériau sur cette
					// base. Ici, aucune surcouche ne peut fausser le verdict.
					if (getenv("NK_EDIT_IDENTITY")) {
						NkVector<NkVertex3D> rv;
						NkVector<uint32> ri;
						NkVector<renderer::NkEmId> rtf;
						st->editHE.Triangulate(rv, ri, rtf);
						const uint32 n = (vc < (uint32)rv.Size()) ? vc : (uint32)rv.Size();
						uint32 dPos = 0, dNrm = 0, dTan = 0, dUV = 0, dUV2 = 0, dCol = 0;
						float32 maxPos = 0.f, maxTan = 0.f;
						for (uint32 k = 0; k < n; k++) {
							const NkVec3f dp = rv[k].pos - sv[k].pos;
							const NkVec3f dt = rv[k].tangent - sv[k].tangent;
							const float32 lp = dp.Len(), lt = dt.Len();
							if (lp > 1e-5f) {
								dPos++;
								if (lp > maxPos)
									maxPos = lp;
							}
							if ((rv[k].normal - sv[k].normal).Len() > 1e-4f)
								dNrm++;
							if (lt > 1e-4f) {
								dTan++;
								if (lt > maxTan)
									maxTan = lt;
							}
							if ((rv[k].uv - sv[k].uv).Len() > 1e-5f)
								dUV++;
							if ((rv[k].uv2 - sv[k].uv2).Len() > 1e-5f)
								dUV2++;
							if (rv[k].color != sv[k].color)
								dCol++;
						}
						logger.Info("[Demo3D][IDENTITE] objet #{0} : {1} sommets compares | "
									"pos={2} (max {3}) normal={4} tangent={5} (max {6}) "
									"uv={7} uv2={8} color={9}  <- 0 partout = aller-retour NEUTRE\n",
									sel, n, dPos, maxPos, dNrm, dTan, maxTan, dUV, dUV2, dCol);
					}
					st->editHistory.Clear();   // nouvel objet en édition -> historique neuf
					st->editRecorder.Clear();  // journal des commandes neuf
					st->editModifiers.Clear(); // pile de modificateurs neuve
					st->editActiveMod = -1;
					st->editBase = st->editHE; // maillage de BASE pour le rejeu (modificateurs/IA)
					st->editReplayStep = -1;
					st->vertSel.Clear();
					st->vertSel.Resize(st->editHE.VertCount());
					for (uint32 i = 0; i < st->editHE.VertCount(); i++)
						st->vertSel[i] = 0;
					st->editMesh = {};
					Demo3D_SyncFromHE(st, ms);
					// Le mesh objet a été cloné dans editHE -> on le libère ; il sera
					// ré-adopté (mis à jour) à la sortie d'édition.
					if (hadEdit) {
						ms->Release(st->objMesh[sel]);
						st->objMesh[sel] = {};
					}
					// Ancre = transform MONDE de l'objet (base repos + delta gizmo).
					st->editAnchor = st->gizmo.Apply(sel, Demo3D_ObjBaseFull(st, sel));
					st->editAnchorInv = st->editAnchor.Inverse();
					st->editObjIdx = sel;
					// Capture le matériau de l'objet -> le mesh édité garde sa couleur PBR.
					Demo3D_ObjMaterial(sel, st->editObjTint, st->editObjMetallic, st->editObjRoughness);
					st->editGizmo.ClearSelection();
					st->editWasDragging = false;
					st->editOverlayDirty = true;
					st->editMode = true;
					// Compte flat/smooth : l'ombrage est DEDUIT des normales source par
					// BuildFromIndexed. Le tracer ici permet de verifier sans capture
					// qu'un aller-retour en edition ne rabat pas tout le modele en FLAT.
					uint32 nSmooth = 0, nFlat = 0;
					for (uint32 fi = 0; fi < (uint32)st->editHE.faces.Size(); fi++) {
						if (!st->editHE.faces[fi].alive)
							continue;
						if (st->editHE.faces[fi].smooth)
							nSmooth++;
						else
							nFlat++;
					}
					logger.Info("[Demo3D] EDIT MODE objet #{0} ({1} vertices, ombrage : {2} faces "
								"smooth / {3} faces flat).\n",
								sel, vc, nSmooth, nFlat);
				}
			}
		}

		void Demo3D_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (Demo3DState *)ctx.userData;
			// Delta souris RÉEL de la frame = (courant - précédent) -> vaut 0 sans mouvement
			// (contrairement à NkInput.MouseDelta*() périmé). Alimente les 2 gizmos (objet + edit).
			const float32 curMouseX = ((float32)NkInput.MouseX() - nkvpOffX);
			const float32 curMouseY = ((float32)NkInput.MouseY() - nkvpOffY);
			float32 frameMDX = 0.f, frameMDY = 0.f;
			if (st->mouseTracked) {
				frameMDX = curMouseX - st->lastMouseX;
				frameMDY = curMouseY - st->lastMouseY;
			}
			st->lastMouseX = curMouseX;
			st->lastMouseY = curMouseY;
			st->mouseTracked = true;
			// NK_GRID_CLEAN=1 : capture « grille SEULE » -> pas de sol opaque, pas d'axes debug
			// 3D épais ; on montre la VRAIE grille infinie (lignes fines AA + axes sol fins du
			// shader X=rouge/Z=bleu). Sert à juger le shader infinitegrid à l'œil.
			static int gridClean = -1;
			if (gridClean == -1) {
				const char *v = getenv("NK_GRID_CLEAN");
				gridClean = (v && v[0] && v[0] != '0') ? 1 : 0;
			}
			// DIAG (gated NK_FIX_CAM) : fige la caméra + le temps pour comparer DX12/VK
			// au MÊME angle/pose. Pose déterministe identique sur les 2 backends.
			static int fixcam = -1;
			if (fixcam == -1) {
				const char *v = getenv("NK_FIX_CAM");
				fixcam = (v && v[0] && v[0] != '0') ? 1 : 0;
			}
			// NK_FIX_CAM : fige la CAMÉRA uniquement (angle constant), le temps continue
			// -> le spot bouge toujours. Isole "flicker vient de la caméra" vs "de l'ombre".
			if (fixcam) {
				st->angle = 0.6f;
			} else {
				st->angle += dt * 0.45f;
			}

			// [ / ] maintenus : evolution CONTINUE du bias (taux x dt). Permet de
			// balayer rapidement la plage sans marteler la touche.
			if (st->biasUpHeld || st->biasDownHeld) {
				if (auto *ssys = ctx.renderer->GetShadow()) {
					auto &cfg = ssys->GetConfig();
					const float32 kBiasRate = 0.02f; // unites de bias par seconde
					if (st->biasUpHeld)
						cfg.shadowBias += kBiasRate * dt;
					if (st->biasDownHeld)
						cfg.shadowBias = NkMax(0.0001f, cfg.shadowBias - kBiasRate * dt);
				}
			}

			// PORTAGE NK3DModeler : l'EDITEUR possede la frame device et le
			// command buffer — pas de BeginFrame ici. On rejoue ce qu'il ferait
			// pour NOTRE renderer : reset du pool d'UBO objets et upload des
			// materiaux. (Meme contrainte et meme modele que NkViewport3D.cpp.)
			auto *r3d = ctx.renderer->GetRender3D();
			if (!r3d)
				return;
			r3d->ResetFrame();
			if (auto *mc = ctx.renderer->GetMaterialCollection())
				mc->Upload();

			// ── PILOTE HEADLESS D'EDIT MODE (captures agents/CI, sans souris) ────────────
			// NK_EDIT_MODE=1 : entre en EDIT MODE sur un objet (NK_GIZMO_OBJ, défaut 16 = cube
			// central), sélectionne un sous-ensemble façon démo, et applique éventuellement UNE
			// opération pour une capture avant/après :
			//   NK_EDIT_SELMASK=<1..7> : modes de sél. RENDUS (1=V 2=E 4=F ; défaut 1=vertex).
			//   NK_EDIT_SEL=top|all|face|edge|vert : sous-ensemble sélectionné (défaut top =
			//       faces +Y ; face/edge/vert = UN SEUL élément, idéal pour juger le rendu).
			//   NK_EDIT_OP=extrude|subdivide|loopcut|none : op déclenchée (défaut none).
			//   NK_EDIT_CUTS=<n>        : nombre de coupes du loop cut (défaut 1).
			//   NK_EDIT_ORIENT=g|l|n    : orientation du gizmo d'édition (global/local/normal).
			//   NK_SHADING=<0..5>       : mode d'affichage (0=RENDERED 1=SOLID …), comme en objet.
			// ⚠ NK_EDIT_MOVE n'est PAS le comportement de l'extrude : c'est un ARTEFACT DE
			//   CAPTURE. E crée la géométrie à offset ZÉRO (donc invisible en image fixe) ;
			//   NK_EDIT_MOVE la déplace APRÈS coup, uniquement pour la rendre visible.
			// ── PILOTE HEADLESS : POINT DE PIVOT + CURSEUR 3D (captures agents/CI) ──────
			//   NK_PIVOT=<0..4> ou <m|b|c|i|a> : median · boîte englobante · curseur 3D ·
			//        origines individuelles · élément actif. Vaut pour les DEUX gizmos.
			//   NK_CURSOR3D="x,y,z"            : place le curseur 3D (monde).
			{
				static bool gPivotInit = false;
				if (!gPivotInit) {
					gPivotInit = true;
					if (const char *pv = getenv("NK_PIVOT")) {
						const char c0 = pv[0];
						int32 pm = 0;
						if (c0 >= '0' && c0 <= '4')
							pm = c0 - '0';
						else if (c0 == 'b' || c0 == 'B')
							pm = renderer::NkGizmo3D::PIVOT_BBOX;
						else if (c0 == 'c' || c0 == 'C')
							pm = renderer::NkGizmo3D::PIVOT_CURSOR;
						else if (c0 == 'i' || c0 == 'I')
							pm = renderer::NkGizmo3D::PIVOT_INDIVIDUAL;
						else if (c0 == 'a' || c0 == 'A')
							pm = renderer::NkGizmo3D::PIVOT_ACTIVE;
						st->gizmo.SetPivotMode(pm);
						st->editGizmo.SetPivotMode(pm);
						logger.Info("[Demo3D] NK_PIVOT -> {0}\n", renderer::NkGizmo3D::PivotName(pm));
					}
					// ── PILOTE HEADLESS DES OPERATIONS MODALES ───────────────────────────
					// Une op modale se pilote normalement a la SOURIS : en capture (headless) il
					// n'y en a pas. Ces variables forcent donc les parametres au lancement, ce qui
					// permet de PROUVER l'apercu temps reel sur une image fixe.
					//   NK_MODAL_OP=bevel|bevelvert|inset|loopcut|spin|extrude
					//   NK_MODAL_VAL=<parametre continu>  · NK_MODAL_SEG=<parametre entier>
					//   NK_MODAL_CONFIRM=1 : confirme automatiquement (preuve du commit d'undo unique)
					if (const char *mv = getenv("NK_MODAL_VAL")) {
						st->modalEnvHasVal = true;
						st->modalEnvVal = (float32)atof(mv);
					}
					if (const char *mg = getenv("NK_MODAL_SEG")) {
						st->modalEnvHasSeg = true;
						st->modalEnvSeg = atoi(mg);
					}
					if (getenv("NK_MODAL_CONFIRM"))
						st->modalEnvConfirm = true;
					// ── INJECTION D'EVENEMENTS SOURIS SYNTHETIQUES (verification headless) ──
					// La souris est CAPTUREE par l'op modale : on injecte donc le MEME signal
					// qu'un vrai geste, dans le MEME curseur virtuel, et on prouve en capture
					// que l'apercu evolue avec le drag, que la molette change les segments et
					// que l'annulation restaure EXACTEMENT l'etat initial.
					//   NK_MODAL_DRAG="dx,dy"   deplacement TOTAL, etale sur N frames
					//   NK_MODAL_DRAG_FRAMES=N  nombre de frames du drag (defaut 8)
					//   NK_MODAL_WHEEL=<crans>  crans de molette (parametre ENTIER)
					//   NK_MODAL_CANCEL=1       Echap synthetique a la fin du drag
					if (const char *md = getenv("NK_MODAL_DRAG")) {
						float32 dv2[2] = {0.f, 0.f};
						int32 kk2 = 0;
						const char *p2 = md;
						while (kk2 < 2 && *p2) {
							dv2[kk2++] = (float32)atof(p2);
							while (*p2 && *p2 != ',')
								p2++;
							if (*p2 == ',')
								p2++;
						}
						st->modalInjDrag = true;
						st->modalInjDX = dv2[0];
						st->modalInjDY = dv2[1];
						logger.Info("[Demo3D] NK_MODAL_DRAG -> drag synthetique ({0}, {1}) px\n", dv2[0], dv2[1]);
					}
					if (const char *mf = getenv("NK_MODAL_DRAG_FRAMES")) {
						st->modalInjFrames = atoi(mf);
						if (st->modalInjFrames < 1)
							st->modalInjFrames = 1;
					}
					if (const char *mw = getenv("NK_MODAL_WHEEL"))
						st->modalInjWheel = atoi(mw);
					if (getenv("NK_MODAL_CANCEL"))
						st->modalInjCancel = true;
					if (getenv("NK_MODAL_PERF")) {
						st->modalPerf = true;
						logger.Info("[Demo3D] NK_MODAL_PERF actif : cout de chaque apercu modal mesure\n");
					}
					if (getenv("NK_MODAL_FORCEFULL")) {
						st->modalForceFull = true;
						logger.Info(
							"[Demo3D] NK_MODAL_FORCEFULL actif : chemin allege DESACTIVE (mesure de reference)\n");
					}
					if (getenv("NK_WIRE_DIAG")) {
						st->wireDiag = true;
						logger.Info("[Demo3D] NK_WIRE_DIAG actif : comparaison topologie n-gon exacte / re-devinee\n");
					}
					if (getenv("NK_PICK_DIAG")) {
						st->pickDiag = true;
						logger.Info("[Demo3D] NK_PICK_DIAG actif : diagnostic de selection au clic\n");
					}
					if (const char *cu = getenv("NK_CURSOR3D")) {
						float32 cv[3] = {0.f, 0.f, 0.f};
						int32 kk = 0;
						const char *p = cu;
						while (kk < 3 && *p) {
							cv[kk++] = (float32)atof(p);
							while (*p && *p != ',')
								p++;
							if (*p == ',')
								p++;
						}
						st->cursor3D = {cv[0], cv[1], cv[2]};
						logger.Info("[Demo3D] NK_CURSOR3D -> ({0}, {1}, {2})\n", cv[0], cv[1], cv[2]);
					}
				}
			}
			// Le curseur 3D est une donnée de la DÉMO : on le pousse dans les deux gizmos
			// (il y sert de pivot en mode PIVOT_CURSOR).
			st->gizmo.Set3DCursor(st->cursor3D);
			st->editGizmo.Set3DCursor(st->cursor3D);

			// Séquence multi-frames : frame0 entrer -> frame1 sélectionner -> frame2 (op).
			{
				static int32 gEditDrv = -2;
				if (gEditDrv == -2)
					gEditDrv = getenv("NK_EDIT_MODE") ? 0 : -1;
				if (gEditDrv == 0) {
					int32 obj = 16;
					if (const char *go = getenv("NK_GIZMO_OBJ"))
						obj = atoi(go);
					st->gizmo.Select(obj);
					st->gizmo.SetMode(0);		  // gizmo d'édition en TRANSLATE (flèches pleines)
					st->editTogglePending = true; // consommé juste après -> entre en édition ce frame
					if (const char *sm = getenv("NK_EDIT_SELMASK")) {
						int32 m = atoi(sm) & 7;
						if (m)
							st->editSelMask = m;
					}
					if (const char *sh = getenv("NK_SHADING")) {
						st->shadingMode = atoi(sh) % 6;
						const int32 vm[6] = {0, 1, 1, 2, 3, 4};
						r3d->SetWireframe(st->shadingMode == 2);
						r3d->SetViewMode(vm[st->shadingMode]);
					}
					if (const char *cu = getenv("NK_EDIT_CUTS")) {
						const int32 c = atoi(cu);
						if (c >= 1)
							st->loopCuts = c;
					}
					// Paramètres du BEVEL pour les captures headless (Ctrl+B / Ctrl+Shift+B).
					//   NK_BEVEL_OFF=<largeur locale>  (0 / absent => AUTO 6 % de la bbox)
					//   NK_BEVEL_SEG=<1..16>           (1 = chanfrein plat, N = arrondi)
					if (const char *bo = getenv("NK_BEVEL_OFF"))
						st->bevelOffset = (float32)atof(bo);
					if (const char *bs = getenv("NK_BEVEL_SEG")) {
						const int32 s = atoi(bs);
						if (s >= 1)
							st->bevelSegments = s;
					}
					// Paramètres de l'INSET (touche I).
					//   NK_INSET_THICK=<épaisseur> (0/absent => AUTO 8 % de la bbox)
					//   NK_INSET_DEPTH=<profondeur le long de la normale, signé>
					//   NK_INSET_MODE=individual|region
					if (const char *it = getenv("NK_INSET_THICK"))
						st->insetThickness = (float32)atof(it);
					if (const char *id = getenv("NK_INSET_DEPTH"))
						st->insetDepth = (float32)atof(id);
					if (const char *im = getenv("NK_INSET_MODE"))
						st->insetIndividual = (im[0] != 'r' && im[0] != 'R');
					// NK_SPLIT_GAP=<ecart> : écartement de la déchirure (0 => AUTO 1 % bbox).
					if (const char *sg = getenv("NK_SPLIT_GAP"))
						st->splitGap = (float32)atof(sg);
					// SPIN : NK_SPIN_AXIS=x|y|z · NK_SPIN_ANGLE=<deg> · NK_SPIN_STEPS=<n> ·
					// NK_SPIN_DUP=1 (copies isolées). Le CENTRE est le curseur 3D (NK_CURSOR3D).
					if (const char *sa = getenv("NK_SPIN_AXIS"))
						st->spinAxis = (sa[0] == 'x' || sa[0] == 'X') ? 0 : ((sa[0] == 'z' || sa[0] == 'Z') ? 2 : 1);
					if (const char *sang = getenv("NK_SPIN_ANGLE"))
						st->spinAngleDeg = (float32)atof(sang);
					if (const char *sst = getenv("NK_SPIN_STEPS")) {
						const int32 n = atoi(sst);
						if (n >= 1)
							st->spinSteps = n;
					}
					if (const char *sd = getenv("NK_SPIN_DUP"))
						st->spinDuplicate = (sd[0] != '0');
					if (const char *or_ = getenv("NK_EDIT_ORIENT"))
						st->editGizmo.SetOrientationByName(or_);
					gEditDrv = 1;
				} else if (gEditDrv == 1 && st->editMode) {
					// Sélection démo : faces +Y ("top"), TOUT, ou UN SEUL élément (face/edge/vert).
					const char *sel = getenv("NK_EDIT_SEL");
					const char s0 = sel ? sel[0] : 't';
					const bool selAll = (s0 == 'a' || s0 == 'A');
					const bool selNone = (s0 == 'n' || s0 == 'N'); // capture « rien sélectionné »
					const bool selOneFace = (s0 == 'f' || s0 == 'F');
					const bool selOneEdge = (s0 == 'e' || s0 == 'E');
					const bool selOneVert = (s0 == 'v' || s0 == 'V');
					// NK_EDIT_PRESUB=<n> : SUBDIVISE tout le maillage n fois AVANT la sélection
					// et l'opération -> permet des captures « arête/sommet INTÉRIEUR » (dissolve)
					// sans avoir à enchaîner deux opérations dans le pilote headless.
					if (const char *psb = getenv("NK_EDIT_PRESUB")) {
						const int32 n = atoi(psb);
						if (n >= 1) {
							st->editHE.SelectNone();
							renderer::NkSubdivideParams sp;
							sp.cuts = n;
							st->editHE.SubdivideSelectedFaces(sp);
							if (auto *msP = ctx.renderer->GetMeshSystem())
								Demo3D_SyncFromHE(st, msP);
							st->editHistory.Clear();
							st->editBase = st->editHE;
						}
					}
					const uint32 vc = st->editHE.VertCount();
					for (uint32 i = 0; i < vc && i < (uint32)st->vertSel.Size(); i++)
						st->vertSel[i] = 0;
					st->editActiveVert = -1;
					st->editActiveEdgeA = st->editActiveEdgeB = -1;
					st->editActiveFace = -1;
					const uint32 fcnt = (uint32)st->editHE.faces.Size();
					NkVector<renderer::NkEmId> fvv;
					int32 nsel = 0;
					// DIAGNOSTIC TOPOLOGIE (P0) : compte les faces par nombre de côtés et les
					// arêtes UNIQUES du n-gon. Un cube quadifié doit donner 6 quads / 24 arêtes
					// (12 arêtes géométriques dédoublées car les sommets sont dupliqués par
					// face dans la primitive) et AUCUN triangle -> pas de diagonale possible.
					{
						uint32 nTri = 0, nQuad = 0, nNgon = 0, nWire = 0;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							const uint32 sz = st->editHE.FaceSize(f);
							if (sz == 2)
								nWire++;
							else if (sz == 3)
								nTri++;
							else if (sz == 4)
								nQuad++;
							else if (sz > 4)
								nNgon++;
						}
						logger.Info("[Demo3D] Topologie n-gon : {0} tri / {1} quad / {2} n-gon / {3} fil "
									"| {4} aretes uniques (cage) | {5} triangles de rendu\n",
									nTri, nQuad, nNgon, nWire, (uint32)(st->editEdges.Size() / 2),
									(uint32)(st->editIdx.Size() / 3));
					}
					// NK_EDIT_SELIDX="i,j,k,..." : selectionne des sommets PAR INDICE. Sert a
					// rejouer en headless le geste « je prends N sommets puis F » (creation de
					// face n-gon), impossible a exprimer avec les selections thematiques.
					const char *selIdx = getenv("NK_EDIT_SELIDX");
					if (selIdx) {
						const char *pp = selIdx;
						while (*pp) {
							const int32 vi = atoi(pp);
							if (vi >= 0 && (uint32)vi < (uint32)st->vertSel.Size()) {
								if (!st->vertSel[vi])
									nsel++;
								st->vertSel[vi] = 1;
								st->editActiveVert = vi;
							}
							while (*pp && *pp != ',')
								pp++;
							if (*pp == ',')
								pp++;
						}
						logger.Info("[Demo3D] NK_EDIT_SELIDX -> {0} sommets selectionnes par indice\n", nsel);
					} else if (selNone) {
						// rien à faire : tout est déjà désélectionné ci-dessus
					} else if (selOneFace || selOneEdge || selOneVert) {
						// UN SEUL élément : on prend la face la mieux alignée sur une DIRECTION
						// cible (NK_EDIT_FACEDIR, défaut +Y = face du dessus, bien visible), ou
						// sa 1re arête / son 1er sommet. « xz » vise une face OBLIQUE — utile
						// pour juger l'orientation « Normal » du gizmo sur une sphère.
						NkVec3f want{0.f, 1.f, 0.f};
						if (const char *fd = getenv("NK_EDIT_FACEDIR")) {
							if (fd[0] == 'x')
								want = (fd[1] == 'z') ? NkVec3f{0.707f, 0.f, 0.707f} : NkVec3f{1.f, 0.f, 0.f};
							else if (fd[0] == 'z')
								want = NkVec3f{0.f, 0.f, 1.f};
						}
						int32 best = -1;
						float32 bestY = -2.f;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive || st->editHE.FaceSize(f) < 3)
								continue;
							const float32 y = st->editHE.faces[f].normal.Dot(want);
							if (y > bestY) {
								bestY = y;
								best = (int32)f;
							}
						}
						if (best >= 0) {
							fvv.Clear();
							st->editHE.GetFaceVerts((renderer::NkEmId)best, fvv);
							const uint32 fn = (uint32)fvv.Size();
							// La caméra de capture regarde le coin +X/+Z : on choisit le sommet /
							// l'arête qui maximise (x+z) pour que l'élément sélectionné soit BIEN
							// VISIBLE au centre de l'image (sinon on tombe sur le coin du fond).
							auto score = [&](uint32 vi) {
								return st->editHE.verts[vi].pos.x + st->editHE.verts[vi].pos.z;
							};
							uint32 k0 = 0;
							if (selOneVert) {
								float32 bs = -1e30f;
								for (uint32 k = 0; k < fn; k++)
									if (score(fvv[k]) > bs) {
										bs = score(fvv[k]);
										k0 = k;
									}
							} else if (selOneEdge) {
								float32 bs = -1e30f;
								for (uint32 k = 0; k < fn; k++) {
									const float32 s2 = score(fvv[k]) + score(fvv[(k + 1) % fn]);
									if (s2 > bs) {
										bs = s2;
										k0 = k;
									}
								}
							}
							const uint32 take = selOneVert ? 1u : (selOneEdge ? 2u : fn);
							for (uint32 k = 0; k < take && k < fn; k++) {
								const uint32 vi = fvv[(k0 + k) % fn];
								if (vi < (uint32)st->vertSel.Size()) {
									st->vertSel[vi] = 1;
									st->editActiveVert = (int32)vi;
									nsel++;
								}
							}
						}
					} else {
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							if (!selAll && st->editHE.faces[f].normal.y < 0.5f)
								continue; // "top" = faces tournées vers le haut (+Y)
							fvv.Clear();
							st->editHE.GetFaceVerts(f, fvv);
							for (uint32 k = 0; k < (uint32)fvv.Size(); k++) {
								const uint32 vi = fvv[k];
								if (vi < (uint32)st->vertSel.Size()) {
									if (!st->vertSel[vi])
										nsel++;
									st->vertSel[vi] = 1;
									st->editActiveVert = (int32)vi;
								}
							}
						}
					}
					st->editOverlayDirty = true;
					logger.Info("[Demo3D] NK_EDIT_MODE: selection {0} -> {1} sommets (mask={2})\n",
								selAll		 ? "all"
								: selNone	 ? "none"
								: selOneFace ? "face"
								: selOneEdge ? "edge"
								: selOneVert ? "vert"
											 : "top",
								nsel, st->editSelMask);
					// La sélection scriptée ci-dessus ne touche qu'UNE copie de chaque sommet
					// coïncident : on la normalise (comme après un pick souris) pour que les
					// arêtes de la cage soient reconnues comme sélectionnées.
					Demo3D_NormalizeSel(st);
					// NK_SEL_LOOP=edge|face : applique une SÉLECTION DE BOUCLE (équivalent
					// Alt+clic) depuis la 1re arête sélectionnée -> preuve en capture headless
					// que le parcours de boucle fonctionne (il dépend de la soudure).
					if (const char *sl = getenv("NK_SEL_LOOP")) {
						int32 ea = -1, eb = -1;
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size() && ea < 0; e += 2) {
							const uint32 a2 = st->editEdges[e], b2 = st->editEdges[e + 1];
							if (a2 < (uint32)st->vertSel.Size() && b2 < (uint32)st->vertSel.Size() &&
								st->vertSel[a2] && st->vertSel[b2]) {
								ea = (int32)a2;
								eb = (int32)b2;
							}
						}
						if (ea >= 0)
							Demo3D_SelectLoop(st, (uint32)ea, (uint32)eb, (sl[0] == 'f' || sl[0] == 'F'), false);
					}
					// NK_SHADE=flat|smooth : ombrage (Shade Flat / Shade Smooth). S'applique
					// aux FACES SÉLECTIONNÉES s'il y en a, sinon à TOUT le maillage — même
					// règle que Shift+F / Shift+S.
					if (const char *shd = getenv("NK_SHADE"))
						st->editShadePending = (shd[0] == 's' || shd[0] == 'S') ? 1 : 2;
					gEditDrv = 2;
				} else if (gEditDrv == 2) {
					if (const char *op = getenv("NK_EDIT_OP")) {
						// Comparaison de préfixe (pas de <string.h> ici) : les nouvelles ops ont
						// des noms longs, l'initiale ne suffit plus à les distinguer.
						auto isOp = [](const char *s, const char *pre) -> bool {
							while (*pre) {
								char a = *s++, b = *pre++;
								if (a >= 'A' && a <= 'Z')
									a = (char)(a - 'A' + 'a');
								if (a != b)
									return false;
							}
							return true;
						};
						if (isOp(op, "bevelvert")) {
							st->editBevelPending = 2; // Bevel de SOMMET
						} else if (isOp(op, "bevel")) {
							st->editBevelPending = 1; // Bevel d'ARÊTE
						} else if (isOp(op, "inset")) {
							st->editInsetPending = true; // Inset faces
						} else if (isOp(op, "edgesplit") || isOp(op, "split")) {
							st->editSplitPending = true; // Edge split / rip
						} else if (isOp(op, "spin")) {
							st->editSpinPending = true; // Spin / révolution
						} else if (isOp(op, "dissolve")) {
							st->editDissolvePending = 1; // Dissolve contextuel
						} else if (isOp(op, "makeface") || isOp(op, "face")) {
							st->editMakeFacePending = true; // F : face (n-gon) depuis la selection
						} else if (op[0] == 'e' || op[0] == 'E')
							st->editExtrudePending = true; // Extrude
						else if (op[0] == 's' || op[0] == 'S')
							st->editSubdivPending = true; // Subdivide
						else if (op[0] == 'l' || op[0] == 'L')
							st->editLoopCutPending = true; // Loop cut
					}
					// NK_MODAL_OP : lance l'operation en mode MODAL (apercu non confirme), au
					// lieu de l'appliquer directement comme NK_EDIT_OP.
					if (const char *mo = getenv("NK_MODAL_OP")) {
						auto isM = [](const char *a, const char *pre) -> bool {
							while (*pre) {
								char x = *a++, y = *pre++;
								if (x >= 'A' && x <= 'Z')
									x = (char)(x - 'A' + 'a');
								if (x != y)
									return false;
							}
							return true;
						};
						if (isM(mo, "bevelvert"))
							st->modalStartPending = 2;
						else if (isM(mo, "bevel"))
							st->modalStartPending = 1;
						else if (isM(mo, "inset"))
							st->modalStartPending = 3;
						else if (isM(mo, "loopcut"))
							st->modalStartPending = 4;
						else if (isM(mo, "spin"))
							st->modalStartPending = 5;
						else if (isM(mo, "extrude"))
							st->modalStartPending = 6;
						else if (isM(mo, "tosphere") || isM(mo, "sphere"))
							st->modalStartPending = 7;
						else if (isM(mo, "shrink") || isM(mo, "fatten"))
							st->modalStartPending = 8;
					}
					gEditDrv = 3;
				} else if (gEditDrv == 3 && st->editMode) {
					// NK_EDIT_MOVE=<dy> : après l'op, déplace la sélection le long de +Y (local)
					// -> illustre le « grab » qui SUIT l'extrude façon Blender (et rend le
					// résultat nettement visible en capture). Applique directement sur l'autorité
					// editHE puis resynchronise (undo hors périmètre de ce pilote de capture).
					if (const char *mv = getenv("NK_EDIT_MOVE")) {
						const float32 dy = (float32)atof(mv);
						auto *msMv = ctx.renderer->GetMeshSystem();
						const uint32 hv = st->editHE.VertCount();
						for (uint32 i = 0; i < hv && i < (uint32)st->vertSel.Size(); i++)
							if (st->vertSel[i])
								st->editHE.verts[i].pos.y += dy;
						st->editHE.RecomputeNormals();
						if (msMv)
							Demo3D_SyncFromHE(st, msMv);
					}
					// NK_EDIT_SCALE=<d> : applique une ÉCHELLE de groupe (delta, ex. -0.5 =
					// rétrécir de moitié) par le MÊME chemin que le drag du gizmo d'édition,
					// SANS souris. Combiné à NK_PIVOT, c'est LA preuve visuelle de la
					// différence entre points de pivot (Median vs Origines individuelles…).
					if (const char *sc = getenv("NK_EDIT_SCALE")) {
						const float32 d = (float32)atof(sc);
						st->editGizmo.SetSelectedTransform({0.f, 0.f, 0.f}, NkMat4f::Identity(), {d, d, d});
						st->editForceXform = true; // applique la transform de groupe chaque frame
					}
					// NK_PICK_AT="x,y" : arme un CLIC DE SELECTION a ces pixels ecran. Sert a
					// prouver en headless (sans souris) que le pick attrape bien un sommet/une
					// arete VISIBLE sous un angle de camera donne (NK_CAM_YAW/NK_CAM_PITCH).
					if (getenv("NK_PICK_SCAN"))
						st->pickScanPending = true;
					if (const char *pa = getenv("NK_PICK_AT")) {
						float32 pv[2] = {0.f, 0.f};
						int32 kk = 0;
						const char *pp = pa;
						while (kk < 2 && *pp) {
							pv[kk++] = (float32)atof(pp);
							while (*pp && *pp != ',')
								pp++;
							if (*pp == ',')
								pp++;
						}
						st->pickForceX = pv[0];
						st->pickForceY = pv[1];
						st->pickForcePending = true;
					}
					gEditDrv = 4;
				} else if (gEditDrv == 4) {
					// NK_EDIT_EXIT=1 : ressort d'EDIT MODE (TAB) -> l'objet ADOPTE le mesh
					// édité et se rend SANS la cage : capture « objet propre » qui prouve que
					// le résultat (ex. ombrage smooth) persiste hors édition.
					if (getenv("NK_EDIT_EXIT") && !st->editForceXform)
						st->editTogglePending = true;
					gEditDrv = 5;
				}
			}

			// ── Volet 2 : TAB traité ici (accès meshSys) : entre/sort d'EDIT MODE ──
			// Entrée : CLONE les données CPU de l'objet sélectionné (modèle Blender) en
			// un mesh dynamique propre + capture son ancre (transform monde). Sortie :
			// désactive l'édition (le mesh cloné garde son dernier état).
			if (st->editTogglePending) {
				st->editTogglePending = false;
				auto *ms = ctx.renderer->GetMeshSystem();
				if (st->editMode) {
					// Sortie d'édition : on PERSISTE le mesh édité DANS l'objet -> il garde
					// sa nouvelle forme en mode objet (au lieu de retomber sur la primitive
					// partagée). Transfert de propriété (pas de Release).
					if (st->editObjIdx >= 0 && st->editObjIdx < Demo3DState::kNumObj) {
						if (st->objMesh[st->editObjIdx].IsValid() && ms)
							ms->Release(st->objMesh[st->editObjIdx]);
						st->objMesh[st->editObjIdx] = st->editMesh; // l'objet adopte le mesh édité
						st->editMesh = {};
						// L'objet adopte AUSSI la topologie n-gon REELLE (pas seulement les
						// triangles) : le fil de fer et la re-entree en edition repartent de la
						// verite, plus d'une topologie re-devinee par quadify.
						st->objHE[st->editObjIdx] = st->editHE;
						st->objHasHE[st->editObjIdx] = true;
						st->wireCustom[st->editObjIdx].Clear();
						st->wireDirty = true;
						st->wireStamp = -12345; // force la reconstruction complete du batch
					}
					st->editMode = false;
					st->editObjIdx = -1;
					r3d->ClearEditOverlay();
				} else {
					const int32 sel = st->gizmo.ActiveIndex();
					if (sel < 0)
						logger.Info("[Demo3D] Sélectionne un objet (clic) avant TAB.\n");
					else
						Demo3D_EnterEditOnObject(st, ms, r3d, sel);
				}
			}

			// ── Caméra : ÉDITEUR (orbit/pan/zoom, Blender) ou SIMULATION (fly), via
			//    les contrôleurs RÉUTILISABLES du moteur. F bascule. NK_FIX_CAM fige.
			//    Éditeur : orbit=clic MILIEU, pan=Shift+MILIEU, zoom=molette.
			//    Simulation : regard=clic DROIT, déplacement=WASD + E/Q (Shift=rapide).
			NkCamera3DData camData;
			camData.up = {0.f, 1.f, 0.f};
			camData.fovY = 60.f;
			camData.aspect = (float32)ctx.width / (float32)ctx.height;
			// PORTAGE NK3DModeler : plans near/far DYNAMIQUES, comme l'ancienne
			// vue du modeleur (c'est Rihen qui a pointe vers elle). Figes a
			// 0,1/100, la precision du tampon de profondeur s'ecrase des que la
			// camera s'eloigne, et la grille infinie se deteriore -- visible
			// surtout en projection ORTHOGRAPHIQUE.
			{
				const float32 cdist = st->useSimCam ? 8.f : st->editorCam.GetDistance();
				camData.nearPlane = NkMax(0.01f, cdist * 0.005f);
				// La DISTANCE DE VUE se regle dans les proprietes de la scene,
				// independamment des cameras de scene (objets ajoutables n fois).
				camData.farPlane = (nkvpFarOverride > 0.f) ? nkvpFarOverride : cdist * 20.f + 100.f;
			}
			NkCamera3D cam(camData);

			const float32 wheelRaw = (float32)st->wheelAccum;
			st->wheelAccum = 0.0;

			// ══════════════════════════════════════════════════════════════════════════
			// ║  VERROU SOURIS DES OPERATIONS MODALES — POINT DE GARDE UNIQUE          ║
			// ══════════════════════════════════════════════════════════════════════════
			// Demande de l'auteur, comportement BLENDER : « quand une commande est donnee,
			// la souris n'a plus le meme effet tant que ce n'est pas valide ». Tant qu'un
			// operateur modal tourne, la souris lui est EXCLUSIVE.
			// PRINCIPE (un seul endroit, surtout pas un cas par cas) : on NEUTRALISE ICI les
			// canaux d'entree souris partages, et on met les valeurs BRUTES de cote pour le
			// seul consommateur autorise (le bloc modal). Tous les consommateurs en aval
			// (camera editeur/simu, gizmo objet, gizmo d'edition, outils de selection
			// rectangle/lasso/cercle, curseur 3D, couteau) lisent frameMDX/frameMDY/wheel
			// et deviennent donc INERTES sans qu'aucun d'eux n'ait a connaitre le mode modal.
			// Les CLICS, eux, sont consommes en fin de bloc modal (clickNow/gin.leftPressed/
			// gin.leftDown remis a false) — le seul chemin par lequel un clic peut atteindre
			// la selection ou une poignee de gizmo.
			// A la sortie (confirmation OU annulation), modalOp repasse a 0 : rien n'est
			// memorise, tout redevient strictement normal des la frame suivante.
			const bool modalLock = (st->modalOp != 0);
			const float32 modalMDX = frameMDX; // deltas BRUTS : reserves a l'op modale
			const float32 modalMDY = frameMDY;
			const float32 modalWheelRaw = wheelRaw;
			if (modalLock) {
				frameMDX = 0.f; // -> aucune orbite / pan / regard, aucun drag de gizmo
				frameMDY = 0.f;
				st->cursorPlacePending = false; // Shift+clic droit ne replace pas le curseur 3D
				st->knifeArmed = false;			// le couteau ne peut pas s'armer pendant l'op
				st->knifeHasP0 = false;
				if (st->selTool != 0 || st->selDragging) { // aucun outil de selection par zone
					st->selTool = 0;
					st->selDragging = false;
					st->selLasso.Clear();
				}
				if (!st->editMode)
					st->modalOp = 0; // filet de securite : plus d'edition -> plus d'op modale
			}
			// La molette : ZOOM camera en temps normal, parametre ENTIER de l'op quand une op
			// modale tourne (et RAYON du cercle en outil de selection cercle).
			const float32 wheel = modalLock ? 0.f : wheelRaw;
			// La molette alimente le RAYON du cercle de selection dans LES DEUX modes :
			// l'outil cercle existe desormais aussi en mode objet, et sans cette ligne son
			// rayon y serait fige (la molette repartirait au zoom camera).
			st->lastWheel = ((st->selTool == 3) || (st->editMode && modalLock)) ? modalWheelRaw : 0.f;
			if (!fixcam) {
				// FIX drift caméra : delta RECALCULÉ par frame (frameMDX/MDY = pos courante -
				// pos précédente, = 0 sans mouvement), et NON NkInput.MouseDelta*() qui reste
				// FIGÉ à sa dernière valeur non nulle quand la souris s'arrête -> sinon
				// clic-milieu/droit maintenu SANS bouger faisait dériver orbit/pan/look.
				// (Même correctif que les 2 gizmos, cf. gin.mouseDX = frameMDX plus bas.)
				const float32 mdx = frameMDX;
				const float32 mdy = frameMDY;
				const bool shift = NkInput.IsKeyDown(NkKey::NK_LSHIFT);
				if (st->useSimCam) {
					if (NkInput.IsMouseDown(NkMouseButton::NK_MB_RIGHT))
						st->simCam.Look(mdx, -mdy);
					const float32 spd = (shift ? 12.f : 4.f) * dt;
					float32 fwd = 0.f, rgt = 0.f, up = 0.f;
					if (NkInput.IsKeyDown(NkKey::NK_W) || NkInput.IsKeyDown(NkKey::NK_UP))
						fwd += spd;
					if (NkInput.IsKeyDown(NkKey::NK_S) || NkInput.IsKeyDown(NkKey::NK_DOWN))
						fwd -= spd;
					if (NkInput.IsKeyDown(NkKey::NK_D) || NkInput.IsKeyDown(NkKey::NK_RIGHT))
						rgt += spd;
					if (NkInput.IsKeyDown(NkKey::NK_A) || NkInput.IsKeyDown(NkKey::NK_LEFT))
						rgt -= spd;
					if (NkInput.IsKeyDown(NkKey::NK_E))
						up += spd;
					if (NkInput.IsKeyDown(NkKey::NK_Q))
						up -= spd;
					st->simCam.Move(fwd, rgt, up);
					if (wheel != 0.f)
						st->simCam.Move(wheel * 0.6f, 0.f, 0.f); // molette = avancer
					st->simCam.Apply(cam);
				} else {
					const bool ctrl = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
					// FIX 2 : pivot d'orbite = CENTROÏDE de la sélection (façon Blender
					// « orbit around selection »). Le centroïde vient du gizmo actif (objet
					// hors édit mode, vertices en édit mode) — barycentre calculé au dernier
					// Update() (GetPivot()). Sans sélection -> pivot inchangé (cible courante).
					bool haveSelPivot = false;
					NkVec3f selPivot = {0.f, 0.f, 0.f};
					if (st->editMode) {
						if (st->editGizmo.HasSelection()) {
							selPivot = st->editGizmo.GetPivot();
							haveSelPivot = true;
						}
					} else if (st->gizmo.HasSelection()) {
						selPivot = st->gizmo.GetPivot();
						haveSelPivot = true;
					}
					if (NkInput.IsMouseDown(NkMouseButton::NK_MB_MIDDLE)) {
						if (shift)
							st->editorCam.Pan(-mdx, -mdy); // "grab" façon Blender : on tire la scène (axes inversés)
						else {
							if (mdx != 0.f || mdy != 0.f)
								st->orthoView = false; // orbite libre -> perspective (Blender)
							// Orbite RIGIDE autour du centroïde de la sélection (position ET
							// cible tournent ENSEMBLE) : aucun re-visée du pivot -> AUCUN saut
							// au premier orbit. Sans sélection : orbite normale autour de la
							// cible courante. Le pan (ci-dessus) reste intact (jamais re-pivoté).
							if (haveSelPivot)
								st->editorCam.OrbitAroundPivot(selPivot, mdx, mdy);
							else
								st->editorCam.Rotate(mdx, mdy);
						}
					}
					// Molette façon Blender : seule = ZOOM ; Shift+molette = PAN VERTICAL ;
					// Ctrl+molette = PAN HORIZONTAL. (mdx/mdy pixels ~10-20 ; une crantée de
					// molette ~1 -> multiplier pour un pan comparable.)
					// (En mode CERCLE de sélection, la molette est réservée au rayon ; pendant
					// une op MODALE, `wheel` vaut deja 0 — cf. le verrou souris unique.)
					if (wheel != 0.f && st->selTool != 3) { // l'outil CERCLE capte la molette (rayon)
						const float32 step = wheel * 22.f;
						if (shift)
							st->editorCam.Pan(0.f, step); // vertical
						else if (ctrl)
							st->editorCam.Pan(step, 0.f); // horizontal
						else
							st->editorCam.Zoom(wheel); // zoom
					}
					// Nav éditeur façon Blender = souris uniquement (molette milieu / Shift+milieu /
					// molette). Pas de WASD ici -> G/R/S/A restent libres pour le gizmo/sélection.
					st->editorCam.Apply(cam);
					// Projection ORTHO en vue axiale (façon Blender) : orthoSize dérivé de la
					// distance pour un cadrage comparable à la perspective (demi-hauteur ≈ d·tan(fov/2)).
					if (st->orthoView) {
						const float32 dist = (cam.GetPosition() - cam.GetTarget()).Len();
						cam.SetOrtho(true, dist * nkvpOrthoScale);
						// PORTAGE NK3DModeler : la grille INFINIE est un shader concu
						// pour la PERSPECTIVE (il reconstruit un rayon par pixel) ; en
						// projection orthographique il degenere en eventail de rayures
						// -- constate a l'ecran. En ortho on la COUPE et on trace une
						// grille FINIE en lignes de debogage, comme Blender.
						r3d->SetInfiniteGridEnabled(false);
						if (st->viewGridInOrtho && nkvpGridOn) {
							const int32 N = nkvpGridExtent;
							// LEGEREMENT decalee du plan qu'elle habille : a la meme
							// hauteur que le sol, les lignes z-fightent et « se
							// chevauchent » (constate par Rihen).
							const float32 h = 0.015f;
							const NkVec4f minor{0.30f, 0.31f, 0.34f, 0.55f};
							const NkVec4f major{0.42f, 0.43f, 0.47f, 0.85f};
							// LA GRILLE FAIT FACE AU REGARD **sur les vues d'axe
							// seulement** (Rihen) : de face/arriere -> plan XY, de
							// gauche/droite -> plan ZY. Partout ailleurs -- orbite
							// libre, meme orthographique -- elle reste AU SOL, comme
							// la grille infinie qu'elle remplace. Le seuil 0,99 dit
							// « pratiquement aligne sur l'axe ».
							NkVec3f vdir = cam.GetTarget() - cam.GetPosition();
							const float32 vl =
								sqrtf(vdir.x * vdir.x + vdir.y * vdir.y + vdir.z * vdir.z);
							if (vl > 1e-6f)
								vdir = {vdir.x / vl, vdir.y / vl, vdir.z / vl};
							const float32 ax = fabsf(vdir.x), az = fabsf(vdir.z);
							const int32 plane = (az > 0.99f) ? 1 : ((ax > 0.99f) ? 2 : 0);
							for (int32 gi = -N; gi <= N; ++gi) {
								// La ligne CENTRALE repasserait sur les axes X/Y/Z deja
								// traces (la « seconde ligne » constatee) : on la saute
								// tant que les axes sont affiches.
								if (gi == 0 && nkvpAxesOn)
									continue;
								const NkVec4f &c = (gi % 10 == 0) ? major : minor;
								const float32 f = (float32)gi;
								if (plane == 0) { // sol XZ : defaut + vues dessus/dessous
									r3d->DrawDebugLine({f, h, (float32)-N}, {f, h, (float32)N}, c);
									r3d->DrawDebugLine({(float32)-N, h, f}, {(float32)N, h, f}, c);
								} else if (plane == 1) { // face/arriere : plan XY
									r3d->DrawDebugLine({f, (float32)-N, 0.f}, {f, (float32)N, 0.f}, c);
									r3d->DrawDebugLine({(float32)-N, f, 0.f}, {(float32)N, f, 0.f}, c);
								} else { // gauche/droite : plan ZY
									r3d->DrawDebugLine({0.f, (float32)-N, f}, {0.f, (float32)N, f}, c);
									r3d->DrawDebugLine({0.f, f, (float32)-N}, {0.f, f, (float32)N}, c);
								}
							}
						}
					} else {
						cam.SetOrtho(false);
						r3d->SetInfiniteGridEnabled(nkvpGridOn);
					}
					// NOTE : une premiere version coupait ici SSAO/SSR/bloom en ortho
					// (SetPostConfig a chaud). RETIREE : basculer la config de post en
					// cours de route melangeait des tampons perimes avec la frame
					// courante — c'est le « dedoublement des elements, deux vues en
					// une » constate par Rihen en passant persp/ortho par les vues.
				}
			} else {
				st->editorCam.Apply(cam); // NK_FIX_CAM : pose figée déterministe
			}

			// ── Lights ───────────────────────────────────────────────────────────
			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.time = ctx.totalTime;

			// ── INITIALISATION UNIQUE DES LUMIERES ──────────────────────────────
			// Auparavant ces 4 descripteurs etaient reecrits a CHAQUE frame : la scene
			// etait donc figee par construction, et aucune manipulation n'aurait pu
			// survivre. On ne les ecrit plus qu'une fois ; ensuite l'etat fait foi.
			if (!st->lightsInit) {
				st->lightsInit = true;

				// [0] Soleil directionnel
				NkLightDesc &sun = st->lights[0];
				sun = NkLightDesc{};
				sun.type = NkLightType::NK_DIRECTIONAL;
				sun.direction = {-0.4f, -1.f, -0.3f};
				sun.color = {1.f, 0.95f, 0.85f};
				sun.intensity = 3.f;
				sun.castShadow = true;
				sun.shadowStatic = true; // NkVSM v1 cache : le soleil ne bouge pas

				// [1] Ponctuelle rouge — cube cookie « lantern » (E.6b). Boostee
				// (intensite 12, portee 10) pour que le motif en X reste lisible face
				// au soleil et au spot. Legerement haute pour ne pas etre dans le sol.
				NkLightDesc &redLight = st->lights[1];
				redLight = NkLightDesc{};
				redLight.type = NkLightType::NK_POINT;
				redLight.position = {3.f, 2.5f, 0.f};
				redLight.color = {1.f, 0.2f, 0.1f};
				redLight.intensity = 12.f;
				redLight.range = 10.f;
				redLight.cookieIdx = 0;
				redLight.castShadow = true;
				redLight.shadowStatic = true;

				// [2] Fill bleue
				NkLightDesc &blue = st->lights[2];
				blue = NkLightDesc{};
				blue.type = NkLightType::NK_POINT;
				blue.position = {-2.f, 1.f, 1.f};
				blue.color = {0.2f, 0.5f, 1.f};
				blue.intensity = 2.5f;
				blue.range = 8.f;
				blue.castShadow = true;
				blue.shadowStatic = true;

				// [3] Spot avec cookie procedural « barreaux » projete au sol.
				NkLightDesc &spot = st->lights[3];
				spot = NkLightDesc{};
				spot.type = NkLightType::NK_SPOT;
				spot.position = {3.f, 4.f, 0.f};
				spot.direction = (NkVec3f{0.f, 0.f, 0.f} - spot.position).Normalized();
				spot.color = {1.f, 0.95f, 0.85f};
				spot.intensity = 8.f;
				spot.range = 10.f;
				spot.innerAngle = 18.f;
				spot.outerAngle = 28.f;
				spot.cookieIdx = 0;
				spot.castShadow = true;
			}

			// ANIMATION DU SPOT — desormais OPTIONNELLE et pilotee par une phase
			// PROPRE, pas par ctx.totalTime. Deux raisons : (1) des que l'utilisateur
			// deplacera le spot il faudra couper l'animation sans figer le temps
			// global ; (2) lire l'horloge globale rendait la scene non reproductible
			// entre deux captures, ce qui fausse toute comparaison avant/apres.
			// NK_LIGHT_ANIM=0 fige l'animation (captures deterministes).
			{
				static int32 sAnimEnv = -1;
				if (sAnimEnv == -1) {
					const char *v = getenv("NK_LIGHT_ANIM");
					sAnimEnv = (v && v[0] == '0') ? 0 : 1;
				}
				if (sAnimEnv && st->spotAnimated && !fixcam) {
					st->spotAngle += dt * 0.3f;
					NkLightDesc &spot = st->lights[3];
					spot.position = {3.f * cosf(st->spotAngle), 4.f, 3.f * sinf(st->spotAngle)};
					spot.direction = (NkVec3f{0.f, 0.f, 0.f} - spot.position).Normalized();
				}
			}

			// Soumission : la source de verite est l'ETAT, compose avec le gizmo.
			// Passer `lights[li]` directement rendrait la manipulation invisible dans
			// l'image — le widget bougerait, l'eclairage non.
			for (int32 li = 0; li < Demo3DState::kNumLights; li++)
				if (!HostHiddenEff(86 + li)) // oeil ferme (le sien ou celui d'un ancetre)
					sctx.lights.PushBack(Demo3D_LightEffective(st, li));
			// LUMIERES UTILISATEUR (dupliquees/collees) : descripteur NATIF
			// conserve, position pilotee par les tableaux + le gizmo.
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				if (nkvpUserKind[u] != 5)
					continue;
				const int32 un = kNkvpFirstUser + u;
				if (HostHiddenEff(un))
					continue;
				renderer::NkLightDesc L2 = nkvpUserLight[u];
				const int32 e = un - 90;
				const NkVec3f tr = st->emptyGizmo.TranslateOf(e);
				L2.position = {nkvpEmptyPos[e][0] + tr.x, nkvpEmptyPos[e][1] + tr.y,
							   nkvpEmptyPos[e][2] + tr.z};
				sctx.lights.PushBack(L2);
			}




			sctx.ambientIntensity = 0.15f;

			// ── WIDGETS DES LUMIERES ────────────────────────────────────────────
			// Une lumiere eclaire mais ne se voit pas : sans marqueur elle n'est ni
			// cliquable ni manipulable. On memorise les descripteurs soumis pour les
			// dessiner en surcouche apres le rendu (le widget doit passer PAR-DESSUS
			// la scene, sinon il disparait dans la geometrie).
			st->frameLights.Clear();
			for (uint32 li = 0; li < (uint32)sctx.lights.Size(); li++)
				st->frameLights.PushBack(sctx.lights[li]);

			// ── NK_GI_TEST : GI à un rebond, avec mur ROUGE MOBILE ───────────────
			// Cette scène contient déjà une lumière rouge : elle rendrait le test
			// ambigu (le rouge observé viendrait-il du rebond ou d'elle ?). On la
			// retire donc sous l'override, pour qu'AUCUNE source rouge n'existe et
			// que toute teinte rouge ne puisse venir que du rebond sur le mur.
			if (st->giTest) {
				for (uint32 li = 0; li < (uint32)sctx.lights.Size();) {
					const NkVec3f &c = sctx.lights[li].color;
					if (c.x > 0.5f && c.y < 0.35f && c.z < 0.35f)
						sctx.lights.Erase(sctx.lights.Begin() + li);
					else
						li++;
				}
				// Va-et-vient automatique : le mur balaie l'axe X et le GI suit.
				if (st->giAuto) {
					st->giPhase += dt;
					const float32 nx = math::NkSin(st->giPhase * 0.6f) * 2.2f;
					if (math::NkAbs(nx - st->giWallOffset.x) > 0.02f) {
						st->giWallOffset.x = nx;
						st->giDirty = true;
					}
				}
				// Transform effective du mur = décalage clavier PUIS décalage gizmo.
				// (On appelle gizmo.Apply directement : la lambda userXform n'est
				// définie que plus bas, après BeginScene. Même résultat, le gizmo
				// n'étant mis à jour qu'en fin de frame.)
				const NkMat4f wallXform =
					st->gizmo.Apply(Demo3DState::kIdxGIWall, Demo3D_ObjBaseFull(st, Demo3DState::kIdxGIWall));
				// Déplacé au gizmo ? On compare la transform à celle du dernier calcul :
				// c'est ce qui fait que TIRER LE MUR À LA SOURIS met le GI à jour.
				for (int32 e = 0; e < 16 && !st->giDirty; e++) {
					if (math::NkAbs(((const float32 *)&wallXform)[e] - ((const float32 *)&st->giWallXform)[e]) > 1e-4f)
						st->giDirty = true;
				}
				if (st->giDirty) {
					st->giWallXform = wallXform;
					Demo3D_RebuildGI(st, ctx.renderer, sctx.lights);
					st->giDirty = false;
				}
			}

			r3d->BeginScene(sctx);

			// Transform utilisateur (décalage gizmo) appliqué à un objet : délégué au
			// composant NkGizmo3D (source unique : draw calls ET pick/marqueur passent par lui).
			// FIX 1 (contour de sélection qui suit l'objet) : on MÉMORISE la matrice EXACTE
			// utilisée pour dessiner l'objet actif, afin de la réutiliser telle quelle pour le
			// masque de contour (SubmitSelection). Sinon le contour recalculait userXform APRÈS
			// gizmo.Update() (qui applique le drag) alors que l'objet a été dessiné AVANT ->
			// l'objet et son liseré utilisaient deux états de transform décalés d'une frame
			// pendant translate/scale/rotate (et, pour le cube central animé, la base figée
			// Demo3D_ObjBase au lieu de sa matrice animée). Capturer la matrice dessinée garantit
			// que l'objet ET le contour partagent EXACTEMENT la même transform.
			const int32 selDrawIdx = st->gizmo.ActiveIndex();
			NkMat4f selDrawXform = NkMat4f::Identity();
			bool selDrawValid = false;
			auto userXform = [&](int32 idx, const NkMat4f &base) {
				const NkMat4f m = st->gizmo.Apply(idx, base);
				if (idx == selDrawIdx) {
					selDrawXform = m;
					selDrawValid = true;
				}
				return m;
			};

			// Couleur EFFECTIVE d'un objet : la couleur uniforme (gris/custom) est une
			// propriété du MODE D'AFFICHAGE SOLID/WIREFRAME UNIQUEMENT (façon Blender), PAS
			// de l'edit mode. L'edit mode fonctionne dans N'IMPORTE QUEL mode d'affichage
			// (RENDERED garde le matériau PBR, NORMAL/UV/AO gardent leur canal, etc.).
			const bool grayActive = (st->unlitColorMode != 0) && (st->shadingMode == 1 || st->shadingMode == 2);
			auto effTint = [st, grayActive](NkVec3f matTint) -> NkVec3f {
				if (!grayActive)
					return matTint;
				return (st->unlitColorMode == 2) ? st->unlitCustom : st->unlitGray;
			};
			// Mesh EFFECTIF d'un objet : son mesh édité persistant s'il existe, sinon la
			// primitive partagée. Permet à l'édition de survivre au retour en mode objet.
			auto meshFor = [st](int32 idx, NkMeshHandle prim) -> NkMeshHandle {
				return (idx >= 0 && idx < Demo3DState::kNumObj && st->objMesh[idx].IsValid()) ? st->objMesh[idx] : prim;
			};

			// ── Sol ──────────────────────────────────────────────────────────────
			// RETIRÉ : la grille infinie sert désormais de sol de référence (façon Blender/
			// Unreal). Un sol solide coplanaire au plan y=0 de la grille provoquait du
			// z-fighting sur GL/DX (grille qui semblait NON coplanaire / inclinée). Sans sol,
			// plus aucune surface coplanaire -> grille propre sur tous les backends.
			// NB : sans sol, pas de récepteur d'ombres au sol dans cette démo (les casters
			// castent quand même dans l'atlas). Pour ré-afficher les ombres au sol, remettre
			// un sol ET décaler la grille (planeY) ou la rendre en depth-bias constant.
			// NK_GRID_CLEAN : on saute le sol opaque + on active les axes fins du shader
			// (X rouge / Z bleu) pour voir clairement la grille infinie.
			if (gridClean) {
				static bool gclInit = false;
				if (!gclInit) {
					gclInit = true;
					auto &g = r3d->GetInfiniteGridParams();
					g.showAxes = true;						   // axes sol FINS AA du shader
					g.axisXColor = {0.90f, 0.20f, 0.25f, 1.f}; // X rouge
					g.axisZColor = {0.20f, 0.45f, 0.90f, 1.f}; // Z bleu
					g.lineColor = {0.55f, 0.58f, 0.66f, 1.0f}; // lignes un peu plus lisibles
					g.cellColor = {0.10f, 0.11f, 0.13f, 0.0f}; // pas de remplissage opaque
				}
			}
			// Objet en cours d'édition : sa cage remplace son rendu normal, comme pour
			// tous les autres objets — sinon la primitive d'origine reste affichée PAR
			// DESSUS la cage, et on croit déplacer une forme fantôme.
			if (!gridClean && !(st->editMode && st->editObjIdx == Demo3DState::kIdxFloor)) {
				NkDrawCall3D dc;
				// meshFor : si le sol a été ÉDITÉ, c'est SON mesh qui est rendu — sinon
				// l'extrusion faite en Edit Mode ne serait visible qu'en Edit Mode.
				dc.mesh = meshFor(Demo3DState::kIdxFloor, st->meshPlane);
				// Passe par userXform : le sol suit le gizmo comme n'importe quel objet.
				dc.transform = userXform(Demo3DState::kIdxFloor, Demo3D_ObjBaseFull(st, Demo3DState::kIdxFloor));
				dc.aabb = {{-40, 0, -40}, {40, 0, 40}};
				dc.castShadow = false; // reçoit les ombres (pas caster)
				dc.tint = effTint({0.12f, 0.12f, 0.13f});
				dc.metallic = 0.f;
				dc.roughness = 0.92f;
				HostMatHook(Demo3DState::kIdxFloor, dc);
				if (!HostHiddenEff(Demo3DState::kIdxFloor))
					r3d->Submit(dc);
			}

			// ── NK_GI_TEST : le mur rouge, RENDU à la position qui sert au GI ────
			// Même AABB que l'occluder injecté (source unique kGIWallMin/Max +
			// offset) : la lumière rebondit exactement sur le mur qu'on voit.
			if (st->giTest && !(st->editMode && st->editObjIdx == Demo3DState::kIdxGIWall)) {
				// EXACTEMENT la transform qui a servi à l'occluder (clavier + gizmo).
				const NkMat4f wm =
					userXform(Demo3DState::kIdxGIWall, Demo3D_ObjBaseFull(st, Demo3DState::kIdxGIWall));
				NkVec3f mn, mx;
				Demo3D_XformAABB(wm, mn, mx);
				NkDrawCall3D dc;
				dc.mesh = meshFor(Demo3DState::kIdxGIWall, st->meshCube);
				dc.transform = wm;
				dc.aabb = {mn, mx};
				dc.tint = effTint({0.9f, 0.05f, 0.05f});
				dc.metallic = 0.f;
				dc.roughness = 0.85f;
				HostMatHook(Demo3DState::kIdxGIWall, dc);
				if (!HostHiddenEff(Demo3DState::kIdxGIWall))
					r3d->Submit(dc);
			}

			// ── Grille 4x4 de spheres : grille PBR canonique ─────────────────────
			// Colonnes -> metallic (0..1) : voir l'effet F0 changer
			// Lignes   -> roughness (~0..1) : voir le blur GGX changer
			// La sphere top-left (col=0, row=0) est dielectric mirror -> reflet net
			// du sky. Top-right (col=3, row=0) est metal poli -> reflet teinte par
			// l'albedo. Bottom-row : surfaces rugueuses, ambient diffus dominant.
			for (int row = 0; row < 4; row++) {
				for (int col = 0; col < 4; col++) {
					float32 x = (col - 1.5f) * 1.2f;
					float32 z = (row - 1.5f) * 1.2f;

					NkDrawCall3D dc;
					dc.mesh = meshFor(row * 4 + col, st->meshSphere);
					dc.transform = userXform(row * 4 + col, // idx pick = row*4+col
											 NkMat4f::Translate({x, 0.5f, z}) * NkMat4f::Scale({0.45f, 0.45f, 0.45f}));
					st->objXform[row * 4 + col] = dc.transform;
					dc.aabb = {{x - 0.25f, 0.25f, z - 0.25f}, {x + 0.25f, 0.75f, z + 0.25f}};
					dc.tint = effTint({(float32)col / 3.f, (float32)row / 3.f, 0.7f});
					dc.metallic = (float32)col / 3.f;				   // 0, 0.33, 0.66, 1
					dc.roughness = 0.05f + (float32)row / 3.f * 0.95f; // 0.05 .. 1
					HostMatHook(row * 4 + col, dc);
					// En Edit Mode, l'objet édité est remplacé par son clone (plus bas).
					if (!(st->editMode && st->editObjIdx == row * 4 + col) &&
						!HostHiddenEff(row * 4 + col))
						r3d->Submit(dc);
				}
			}

			// ── DÉMO GPU INSTANCING : grille 8x8 de cubes via SubmitInstanced ─────
			// Avec NK_INSTANCING_GPU=1 -> 1 SEUL draw (gl_InstanceID). Sans -> chemin
			// object-UBO (N draws, correct). Dans les deux cas, 64 cubes en grille
			// derrière la scène : s'ils sont bien répartis -> instancing OK.
			{
				NkDrawCallInstanced inst;
				inst.mesh = st->meshCube;
				for (int gz = 0; gz < 8; gz++) {
					for (int gx = 0; gx < 8; gx++) {
						const int32 idx = 19 + gz * 8 + gx;
						const float32 x = (gx - 3.5f) * 0.55f;
						const float32 z = (gz - 3.5f) * 0.55f - 4.5f; // décalé derrière le sol
						const NkMat4f xf =
							userXform(idx, NkMat4f::Translate({x, 1.6f, z}) * NkMat4f::Scale({0.18f, 0.18f, 0.18f}));
						st->objXform[idx] = xf;
						const NkVec3f tint = effTint({(float32)gx / 7.f, 0.6f, (float32)gz / 7.f});
						if (st->editMode && st->editObjIdx == idx)
							continue;
						if (HostHiddenEff(idx))
							continue; // oeil ferme dans la hierarchie					  // édité -> via editMesh
						if (st->objMesh[idx].IsValid()) { // édité persisté -> draw séparé
							NkDrawCall3D dc;
							dc.mesh = st->objMesh[idx];
							dc.transform = xf;
							dc.aabb = {{x - 0.15f, 1.4f, z - 0.15f}, {x + 0.15f, 1.8f, z + 0.15f}};
							dc.tint = tint;
							dc.metallic = 0.f;
							dc.roughness = 0.6f;
							HostMatHook(idx, dc);
							r3d->Submit(dc);
						} else {
							NkMat4f xfI = xf;
							if (nkvpBaseSet[idx])
								xfI = xfI * NkMat4f::Scale({nkvpDimFactor[idx][0],
															nkvpDimFactor[idx][1],
															nkvpDimFactor[idx][2]});
							inst.transforms.PushBack(xfI);
							// Surcharge de TEINTE par instance (le chemin instancie n'a
							// pas de metallique/rugosite par objet).
							NkVec3f instTint = tint;
							if (nkvpMatMask[idx] & 1) {
								instTint.x = nkvpMatTint[idx][0];
								instTint.y = nkvpMatTint[idx][1];
								instTint.z = nkvpMatTint[idx][2];
							}
							inst.tints.PushBack(instTint);
							nkvpMatCache[idx][0] = instTint.x;
							nkvpMatCache[idx][1] = instTint.y;
							nkvpMatCache[idx][2] = instTint.z;
							nkvpMatCache[idx][3] = 0.f;
							nkvpMatCache[idx][4] = 0.6f;
						}
					}
				}
				inst.aabb = {{-3.f, 1.f, -9.f}, {3.f, 2.5f, 0.f}};
				if (!inst.transforms.Empty())
					r3d->SubmitInstanced(inst);
			}

			// ── Cube central rotatif : metal or poli (gold metallic, low rough) ──
			// Transform calculé UNE fois -> SOURCE UNIQUE partagée par le draw call ET le
			// marqueur de sélection (plus bas). Le marqueur applique cette même matrice à ses
			// coins -> il suit position + rotation + échelle SANS recalcul ni duplication.
			NkMat4f cubeXform = NkMat4f::Translate({0, 0.5f + sinf(ctx.totalTime * 1.5f) * 0.2f, 0}) *
								NkMat4f::RotationY(NkAngle::FromRad(ctx.totalTime * 0.8f)) *
								NkMat4f::Scale({0.6f, 0.6f, 0.6f});
			{
				NkDrawCall3D dc;
				dc.mesh = meshFor(16, st->meshCube);
				dc.transform = userXform(16, cubeXform); // idx pick cube central = 16
				st->objXform[16] = dc.transform;
				dc.aabb = {{-0.35f, 0.1f, -0.35f}, {0.35f, 0.9f, 0.35f}};
				dc.tint = effTint({1.f, 0.8f, 0.3f}); // gold albedo (ou gris en edit/unlit)
				dc.metallic = 1.f;
				dc.roughness = 0.15f;
				HostMatHook(16, dc);
				if (!(st->editMode && st->editObjIdx == 16) && !HostHiddenEff(16))
					r3d->Submit(dc);
			}

			// ── Colonnes bloquantes pour visualiser les ombres point/spot ────────
			// NkVSM v0 : ces colonnes castent des ombres pour TOUTES les lights
			// (sun + red + blue + spot) -> on doit voir 4 ombres differentes
			// projetees sur le sol pour chaque colonne.
			// Position colonnes :
			//   - col0 a (-4, 1, -2) : devant la red light pour ombre rouge
			//   - col1 a (1, 1, 4)   : derriere les spheres pour ombre spot
			for (int c = 0; c < 2; c++) {
				float32 cx = (c == 0) ? -4.f : 1.f;
				float32 cz = (c == 0) ? -2.f : 4.f;
				NkDrawCall3D dc;
				dc.mesh = meshFor(17 + c, st->meshCube);
				dc.transform = userXform(17 + c, // idx pick colonnes = 17,18
										 NkMat4f::Translate({cx, 1.f, cz}) *
											 NkMat4f::Scale({0.3f, 2.f, 0.3f})); // colonne 2m haute
				st->objXform[17 + c] = dc.transform;
				dc.aabb = {{cx - 0.2f, 0.f, cz - 0.2f}, {cx + 0.2f, 2.f, cz + 0.2f}};
				dc.tint = effTint({0.7f, 0.7f, 0.7f});
				dc.metallic = 0.f;
				dc.roughness = 0.6f;
				HostMatHook(17 + c, dc);
				dc.castShadow = true;
				if (!(st->editMode && st->editObjIdx == 17 + c) && !HostHiddenEff(17 + c))
					r3d->Submit(dc);
			}

			// ── OBJETS UTILISATEUR (duplication / collage / menu Ajouter) ────────
			// Adosses aux maillages de la demo ; transform = tableaux + decalages
			// du gizmo (effectif -> ils suivent la poignee en direct).
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				const uint8 uk = nkvpUserKind[u];
				if (uk == 0 || uk == 4)
					continue; // libre ou empty : rien a rendre ici
				const int32 un = kNkvpFirstUser + u;
				if (HostHiddenEff(un))
					continue;
				const int32 e = un - 90;
				const float32 kD2Ru = 0.017453292f;
				const NkVec3f utr = st->emptyGizmo.TranslateOf(e);
				const NkVec3f uos = st->emptyGizmo.ScaleOf(e);
				const NkMat4f uR =
					st->emptyGizmo.RotationOf(e) *
					(NkMat4f::RotationZ(NkAngle::FromRad(nkvpEmptyRotDeg[e][2] * kD2Ru)) *
					 NkMat4f::RotationY(NkAngle::FromRad(nkvpEmptyRotDeg[e][1] * kD2Ru)) *
					 NkMat4f::RotationX(NkAngle::FromRad(nkvpEmptyRotDeg[e][0] * kD2Ru)));
				NkDrawCall3D dc;
				dc.mesh = uk == 1 ? st->meshSphere : (uk == 2 ? st->meshCube : st->meshPlane);
				const NkVec3f uc{nkvpEmptyPos[e][0] + utr.x, nkvpEmptyPos[e][1] + utr.y,
								 nkvpEmptyPos[e][2] + utr.z};
				dc.transform = NkMat4f::Translate(uc) * uR *
							   NkMat4f::Scale({nkvpEmptyScl[e][0] * (1.f + uos.x),
											   nkvpEmptyScl[e][1] * (1.f + uos.y),
											   nkvpEmptyScl[e][2] * (1.f + uos.z)});
				float32 uh = fabsf(nkvpEmptyScl[e][0]);
				if (fabsf(nkvpEmptyScl[e][1]) > uh)
					uh = fabsf(nkvpEmptyScl[e][1]);
				if (fabsf(nkvpEmptyScl[e][2]) > uh)
					uh = fabsf(nkvpEmptyScl[e][2]);
				uh = uh * 1.2f + 0.2f;
				dc.aabb = {{uc.x - uh, uc.y - uh, uc.z - uh}, {uc.x + uh, uc.y + uh, uc.z + uh}};
				dc.castShadow = true;
				dc.tint = effTint({0.7f, 0.7f, 0.72f});
				dc.metallic = 0.f;
				dc.roughness = 0.6f;
				HostMatHook(un, dc);
				r3d->Submit(dc);
			}

			// ── NkVSM v2 : panneau feuillage ALPHA-TESTED (ombre trouee) ──────────
			// Panneau vertical au-dessus du sol, entre le soleil et le sol : son
			// ombre doit montrer les trous entre les disques (Shadow_AlphaTest).
			if (st->maskedMat && !(st->editMode && st->editObjIdx == Demo3DState::kIdxFoliage) &&
				!HostHiddenEff(Demo3DState::kIdxFoliage)) {
				NkDrawCall3D dc;
				dc.mesh = meshFor(Demo3DState::kIdxFoliage, st->meshCube);
				dc.transform = userXform(Demo3DState::kIdxFoliage, Demo3D_ObjBaseFull(st, Demo3DState::kIdxFoliage));
				dc.aabb = {{4.f - 1.7f, 0.3f, -1.f - 1.7f}, {4.f + 1.7f, 2.9f, -1.f + 1.7f}};
				dc.material = st->maskedMat->GetInstHandle();
				dc.castShadow = true;
				r3d->Submit(dc);
			}

			// ── WIREFRAME N-GON : synchronise le batch d'aretes (mode d'affichage 2) ──
			// Appele APRES toutes les soumissions : st->objXform[] contient les transforms
			// MONDE reellement utilisees pour le rendu, donc le fil de fer colle pile aux
			// objets (y compris le cube central anime). Hors mode wireframe, rien n'est fait.
			{
				const bool wireMode = (st->shadingMode == 2);
				r3d->SetNgonWireframe(wireMode);
				if (wireMode) {
					if (auto *msW = ctx.renderer->GetMeshSystem())
						Demo3D_SyncWireBatch(st, r3d, msW);
				} else
					st->wireDirty = true; // on rebatira au prochain passage en fil de fer
			}

			// ── Volet 2 : EDIT MODE — gizmo centroïde + pick VERTEX/EDGE/FACE + marqueurs ──
			// Modèle Blender : la sélection est un ENSEMBLE de vertices ; le gizmo a UNE
			// seule cible = leur centroïde ; le drag applique la transform de groupe (G,
			// autour du centroïde) à tous les vertices sélectionnés. 1/2/3 changent ce que
			// le clic sélectionne (vertex / arête / face). Marqueurs = taille écran (fins).
			// Outils topologie sur le HALF-EDGE (n-gon) : opèrent sur editHE puis régénèrent
			// le mesh de rendu (Demo3D_SyncFromHE). AVANT le bloc édition (peut le faire sortir).
			if (st->editMode && st->editMesh.IsValid()) {
				auto *meshSysT = ctx.renderer->GetMeshSystem();
				if (meshSysT) {
					// Undo/redo d'abord (historique de editHE).
					if (st->editUndoPending) {
						st->editUndoPending = false;
						Demo3D_UndoEdit(st, meshSysT);
						logger.Info("[Demo3D] Undo -> {0} faces (undo={1} redo={2})\n", (int32)st->editHE.FaceCount(),
									st->editHistory.UndoCount(), st->editHistory.RedoCount());
					}
					if (st->editRedoPending) {
						st->editRedoPending = false;
						Demo3D_RedoEdit(st, meshSysT);
						logger.Info("[Demo3D] Redo -> {0} faces (undo={1} redo={2})\n", (int32)st->editHE.FaceCount(),
									st->editHistory.UndoCount(), st->editHistory.RedoCount());
					}
					if (st->editReplayPending) {
						st->editReplayPending = false;
						Demo3D_ReplayEdits(st, meshSysT);
					}
					if (st->editSavePending) {
						st->editSavePending = false;
						Demo3D_SaveSession(st);
					}
					if (st->editLoadPending) {
						st->editLoadPending = false;
						Demo3D_LoadSession(st, meshSysT);
					}
					// NK_MOD_ADD="t[,t…]" : empile ces types UNE FOIS au demarrage. Sans ce
				// levier, une pile de modificateurs ne serait verifiable qu'a la main —
				// donc pas en capture, donc pas de facon reproductible.
				static bool modAddDone = false;
				if (!modAddDone && st->editMode) {
					if (const char *ma = getenv("NK_MOD_ADD")) {
						modAddDone = true;
						const char *p = ma;
						while (*p) {
							renderer::NkMeshModifier mod;
							mod.type = (renderer::NkModifierType)atoi(p);
							const uint32 id = st->editModifiers.Add(mod);
							logger.Info("[Demo3D][PILE] NK_MOD_ADD -> {0} (id={1})\n",
										renderer::NkModifierTypeName(mod.type), id);
							while (*p && *p != ',')
								p++;
							if (*p == ',')
								p++;
						}
						st->editActiveMod = (int32)st->editModifiers.Count() - 1;
						Demo3D_SyncFromHE(st, meshSysT);
					}
				}
				// Modificateurs (F7 ajouter le type courant, F8/F9 changer de type, F10 vider).
					if (st->editAddModPending >= 0) {
						renderer::NkMeshModifier mod;
						mod.type = (renderer::NkModifierType)st->editAddModPending;
						st->editModifiers.Add(mod);
						st->editAddModPending = -1;
						st->editActiveMod = (int32)st->editModifiers.Count() - 1; // le nouveau = actif (réglable [ ])
						Demo3D_SyncFromHE(st, meshSysT);
						logger.Info("[Demo3D] + Modificateur {0} (id={1}) — pile={2} · reglage [ / ] · "
									"parametre suivant Shift+\\ · actif \\ · monter/descendre Shift+Haut/Bas · "
									"activer Shift+E · dupliquer Shift+D · retirer Shift+Suppr · APPLIQUER "
									"Shift+Entree · vider F10\n",
									renderer::NkModifierTypeName(mod.type),
									st->editModifiers.modifiers[st->editModifiers.Count() - 1].id,
									st->editModifiers.Count());
					}
					if (st->editClearModPending) {
						st->editClearModPending = false;
						st->editModifiers.Clear();
						st->editActiveMod = -1;
						Demo3D_SyncFromHE(st, meshSysT);
						logger.Info("[Demo3D] Modificateurs vides -> retour a la base editable\n");
					}
					if (st->editModAdjustPending != 0) {
						Demo3D_AdjustMod(st, meshSysT, st->editModAdjustPending);
						st->editModAdjustPending = 0;
					}
					if (st->editModCyclePending) {
						st->editModCyclePending = false;
						st->editActiveParam = 0; // nouveau modificateur -> premier parametre
						Demo3D_CycleActiveMod(st);
					}
					if (st->editModParamCyclePending) {
						st->editModParamCyclePending = false;
						Demo3D_CycleModParam(st);
					}
					if (st->editModStackOp != 0) {
						const int32 op = st->editModStackOp;
						st->editModStackOp = 0;
						Demo3D_ModStackOp(st, meshSysT, op);
					}
					// ── OMBRAGE FLAT / SMOOTH (Shift+F / Shift+S) ─────────────────
					// Pousse la sélection dans l'autorité (une face est « sélectionnée »
					// quand TOUS ses sommets le sont), pose Face::smooth, recalcule les
					// normales, puis régénère le mesh de rendu. Aucune topologie touchée
					// -> la cage n-gon et les marqueurs d'édition sont intacts.
					if (st->editShadePending != 0) {
						const bool wantSmooth = (st->editShadePending == 1);
						st->editShadePending = 0;
						Demo3D_NormalizeSel(st);
						// Y a-t-il une face ENTIÈREMENT sélectionnée ? Si oui -> ombrage
						// PARTIEL (mixte, façon Blender) ; sinon -> objet entier.
						bool anyFaceSel = false;
						for (uint32 f = 0; f < (uint32)st->editHE.faces.Size() && !anyFaceSel; f++)
							if (st->editHE.faces[f].alive && st->editHE.FaceIsSelected(f))
								anyFaceSel = true;
						st->editHE.SetShadeSmooth(wantSmooth, /*selectedOnly*/ true);
						if (!anyFaceSel)
							st->editShadeSmoothAll = wantSmooth; // état « objet » mémorisé
						Demo3D_SyncFromHE(st, meshSysT);
						logger.Info("[Demo3D] Shade {0} ({1}) -> {2} face(s) lissee(s)\n",
									wantSmooth ? "SMOOTH" : "FLAT", anyFaceSel ? "faces selectionnees" : "objet entier",
									st->editHE.AnyFaceSmooth() ? (st->editHE.AllFacesSmooth() ? "toutes" : "une partie")
															   : "aucune");
					}
					// Lancement d'une OPERATION MODALE (le callback clavier n'a pas acces au
					// systeme de meshes : il pose une demande, traitee ici).
					if (st->modalStartPending != 0) {
						const int32 mop = st->modalStartPending;
						st->modalStartPending = 0;
						Demo3D_ModalStart(st, mop, meshSysT);
					}
					if (st->editExtrudePending) {
						st->editExtrudePending = false;
						Demo3D_ExtrudeHE(st, meshSysT);
						logger.Info("[Demo3D] Extrude -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editMakeFacePending) {
						st->editMakeFacePending = false;
						Demo3D_MakeFaceHE(st, meshSysT);
						logger.Info("[Demo3D] Create face -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editSubdivPending) {
						st->editSubdivPending = false;
						Demo3D_SubdivideHE(st, meshSysT); // subdivCuts passes en 1 commande (1 undo)
						logger.Info("[Demo3D] Subdivide x{0} -> {1} faces\n", st->subdivCuts,
									(int32)st->editHE.FaceCount());
					}
					if (st->editLoopCutPending) {
						st->editLoopCutPending = false;
						Demo3D_LoopCutHE(st, meshSysT);
						logger.Info("[Demo3D] Loop cut -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editDissolvePending != 0) {
						st->editDissolvePending = 0;
						const int32 v0 = (int32)st->editHE.VertCount(), f0 = (int32)st->editHE.FaceCount();
						NkVector<uint32> e0;
						st->editHE.GetUniqueEdges(e0);
						const char *dm = (st->editSelMask & 4) ? "FACES" : ((st->editSelMask & 2) ? "ARETES" : "SOMMETS");
						Demo3D_DissolveHE(st, meshSysT);
						NkVector<uint32> e1;
						st->editHE.GetUniqueEdges(e1);
						logger.Info("[Demo3D] Dissolve {0} : {1} sommets/{2} aretes/{3} faces -> "
									"{4} sommets/{5} aretes/{6} faces (fusion en n-gon, PAS un trou)\n",
									dm, v0, (int32)(e0.Size() / 2), f0, (int32)st->editHE.VertCount(),
									(int32)(e1.Size() / 2), (int32)st->editHE.FaceCount());
					}
					if (st->editSpinPending) {
						st->editSpinPending = false;
						const int32 v0 = (int32)st->editHE.VertCount(), f0 = (int32)st->editHE.FaceCount();
						Demo3D_SpinHE(st, meshSysT);
						const char *axn[3] = {"X", "Y", "Z"};
						logger.Info("[Demo3D] Spin (axe={0} angle={1}deg pas={2} {3}) : {4} sommets/{5} faces "
									"-> {6} sommets/{7} faces\n",
									axn[st->spinAxis % 3], st->spinAngleDeg, st->spinSteps,
									st->spinDuplicate ? "duplique" : "relie", v0, f0,
									(int32)st->editHE.VertCount(), (int32)st->editHE.FaceCount());
					}
					if (st->editSplitPending) {
						st->editSplitPending = false;
						const int32 v0 = (int32)st->editHE.VertCount(), f0 = (int32)st->editHE.FaceCount();
						Demo3D_EdgeSplitHE(st, meshSysT);
						logger.Info("[Demo3D] Edge split (ecart={0}) : {1} sommets/{2} faces -> {3} sommets/{4} faces\n",
									st->splitGap, v0, f0, (int32)st->editHE.VertCount(),
									(int32)st->editHE.FaceCount());
					}
					if (st->editInsetPending) {
						st->editInsetPending = false;
						const int32 v0 = (int32)st->editHE.VertCount(), f0 = (int32)st->editHE.FaceCount();
						Demo3D_InsetHE(st, meshSysT);
						logger.Info("[Demo3D] Inset {0} (epaisseur={1} profondeur={2}) : {3} sommets/{4} faces "
									"-> {5} sommets/{6} faces\n",
									st->insetIndividual ? "INDIVIDUEL" : "REGION", st->insetThickness, st->insetDepth,
									v0, f0, (int32)st->editHE.VertCount(), (int32)st->editHE.FaceCount());
					}
					if (st->editBevelPending != 0) {
						const bool vmode = (st->editBevelPending == 2);
						st->editBevelPending = 0;
						const int32 v0 = (int32)st->editHE.VertCount(), f0 = (int32)st->editHE.FaceCount();
						Demo3D_BevelHE(st, meshSysT, vmode);
						logger.Info("[Demo3D] Bevel {0} (largeur={1} segments={2}) : {3} sommets/{4} faces "
									"-> {5} sommets/{6} faces\n",
									vmode ? "SOMMET" : "ARETE", st->bevelOffset, st->bevelSegments, v0, f0,
									(int32)st->editHE.VertCount(), (int32)st->editHE.FaceCount());
					}
					if (st->editBisectPending) {
						st->editBisectPending = false;
						Demo3D_BisectHE(st, meshSysT, st->bisectPt, st->bisectN);
						logger.Info("[Demo3D] Bisect -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
					if (st->editMergePending) {
						st->editMergePending = false;
						Demo3D_MergeHE(st, meshSysT);
						logger.Info("[Demo3D] Merge -> {0} vertices\n", (int32)st->editHE.VertCount());
					}
					if (st->editDeletePending) {
						st->editDeletePending = false;
						Demo3D_DeleteHE(st, meshSysT);
						if (st->editHE.FaceCount() == 0 || st->editIdx.Empty()) {
							if (st->editMesh.IsValid())
								meshSysT->Release(st->editMesh);
							st->editMesh = {};
							st->editMode = false;
							st->editObjIdx = -1;
							r3d->ClearEditOverlay();
							logger.Info("[Demo3D] Delete : mesh vide -> sortie edit mode\n");
						} else
							logger.Info("[Demo3D] Delete faces -> {0} faces\n", (int32)st->editHE.FaceCount());
					}
				}
			}

			if (st->editMode && st->editMesh.IsValid()) {
				auto *meshSysF = ctx.renderer->GetMeshSystem();
				// NON const : une operation MODALE peut changer le nombre de sommets EN COURS
				// de frame (apercu applique / annule) -> on le reevalue juste apres.
				int32 nv = (int32)st->editRest.Size();

				// Projection monde->écran (même convention que le gizmo).
				const NkVec3f camPos = cam.GetPosition(), camTgt = cam.GetTarget();
				const NkVec3f fwd = (camTgt - camPos).Normalized();
				const NkVec3f rgt = fwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
				const NkVec3f upv = rgt.Cross(fwd).Normalized();
				const float32 thY = tanf(60.f * 0.5f * 3.14159265f / 180.f);
				const float32 thX = thY * ((float32)ctx.width / (float32)ctx.height);
				const float32 VW = (float32)ctx.width, VH = (float32)ctx.height;
				auto project = [&](NkVec3f P, float32 &px, float32 &py) -> bool {
					NkVec3f v = P - camPos;
					float32 zc = v.Dot(fwd);
					if (zc <= 1e-3f)
						return false;
					float32 nx = v.Dot(rgt) / (zc * thX), ny = v.Dot(upv) / (zc * thY);
					px = (nx * 0.5f + 0.5f) * VW;
					py = (0.5f - ny * 0.5f) * VH;
					return true;
				};
				auto worldV = [&](int32 i) { return st->editAnchor * st->editRest[i].pos; };
				// ── AUDIT DE SELECTABILITE (NK_PICK_SCAN=1) ──────────────────────────
				// PREUVE INSTRUMENTEE, independante de toute coordonnee souris : pour CHAQUE
				// sommet du maillage on simule un clic PILE DESSUS (rayon curseur passant par sa
				// propre position ecran) et on compare deux verdicts de visibilite :
				//   ANCIEN : le sommet est garde s'il est dans la MOITIE AVANT du segment
				//            [entree, sortie] du rayon dans le maillage (depth < (tNear+tFar)/2) ;
				//   NOUVEAU : occlusion REELLE (une face cache-t-elle le sommet ?), triangles
				//            incidents ignores.
				// Le compteur « visible mais rejete » chiffre exactement le bug rapporte :
				// « meme directement visible, cliquer un sommet/une arete reste complexe sous
				// certains angles ». Il doit tomber a 0 avec la nouvelle regle.
				if (st->pickScanPending) {
					st->pickScanPending = false;
					auto rayTri = [&](NkVec3f ro, NkVec3f rd, int32 i0, int32 i1, int32 i2, float32 &tt) -> bool {
						const NkVec3f v0 = worldV(i0), v1 = worldV(i1), v2 = worldV(i2);
						NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = rd.Cross(e2);
						const float32 aa = e1.Dot(h);
						if (fabsf(aa) < 1e-7f)
							return false;
						const float32 f = 1.f / aa;
						NkVec3f sv = ro - v0;
						const float32 u = f * sv.Dot(h);
						if (u < 0.f || u > 1.f)
							return false;
						NkVec3f q = sv.Cross(e1);
						const float32 vv = f * rd.Dot(q);
						if (vv < 0.f || u + vv > 1.f)
							return false;
						tt = f * e2.Dot(q);
						return tt > 1e-4f;
					};
					int32 nProj = 0, nOldKeep = 0, nNewKeep = 0, nVisibleButRejected = 0, nOldGhost = 0;
					// Un utilisateur ne clique JAMAIS au pixel exact du sommet : il vise a quelques
					// pixels pres. Or l'ancienne regle calculait son plan median sur le rayon du
					// CURSEUR, pas sur celui du sommet -> c'est la que tout se joue. On simule donc
					// un clic decale de kOffPx pixels dans les 4 directions (toujours dans le rayon
					// de pick de 14 px), ce qui reproduit fidelement le geste reel.
					const float32 kOffPx = 8.f;
					const float32 offs[4][2] = {{kOffPx, 0.f}, {-kOffPx, 0.f}, {0.f, kOffPx}, {0.f, -kOffPx}};
					for (int32 i = 0; i < nv; i++) {
						const NkVec3f w = worldV(i);
						float32 px, py;
						if (!project(w, px, py))
							continue;
						if (px < 0.f || py < 0.f || px >= VW || py >= VH)
							continue;
						nProj++;
						// Verdict NOUVEAU : occlusion REELLE (triangles incidents ignores).
						bool occ = false;
						NkVec3f dv = w - camPos;
						const float32 dist = dv.Len();
						if (dist > 1e-5f) {
							dv = dv * (1.f / dist);
							const float32 eps = NkMax(1e-4f, 3e-3f * dist);
							for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size() && !occ; t += 3) {
								const int32 a0 = (int32)st->editIdx[t], a1 = (int32)st->editIdx[t + 1],
											a2 = (int32)st->editIdx[t + 2];
								if (a0 == i || a1 == i || a2 == i)
									continue;
								float32 tt;
								if (rayTri(camPos, dv, a0, a1, a2, tt) && tt < dist - eps)
									occ = true;
							}
						}
						const bool newKeep = !occ;
						if (newKeep)
							nNewKeep++;
						// Verdict ANCIEN : plan median du rayon CURSEUR, pour 4 visees decalees.
						bool oldKeepAll = true, oldKeepAny = false;
						for (int32 o = 0; o < 4; o++) {
							const float32 cx = px + offs[o][0], cy = py + offs[o][1];
							const float32 nx = cx / VW * 2.f - 1.f, ny = 1.f - cy / VH * 2.f;
							NkVec3f rd = fwd + rgt * (nx * thX) + upv * (ny * thY);
							const float32 rl = rd.Len();
							if (rl > 1e-6f)
								rd = rd * (1.f / rl);
							float32 tNear = 1e30f, tFar = -1e30f;
							for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
								float32 tt;
								if (!rayTri(camPos, rd, (int32)st->editIdx[t], (int32)st->editIdx[t + 1],
											(int32)st->editIdx[t + 2], tt))
									continue;
								if (tt < tNear)
									tNear = tt;
								if (tt > tFar)
									tFar = tt;
							}
							const float32 depthMid = (tNear < 1e29f) ? 0.5f * (tNear + tFar) : 1e30f;
							const bool ok = (depthMid >= 1e29f) || (dist < depthMid + 1e-3f);
							if (ok)
								oldKeepAny = true;
							else
								oldKeepAll = false;
						}
						if (oldKeepAll)
							nOldKeep++;
						if (newKeep && !oldKeepAll)
							nVisibleButRejected++;
						if (!newKeep && oldKeepAny)
							nOldGhost++;
					}
					logger.Info("[PickScan] {0} sommets a l'ecran | VISIBLES (occlusion reelle) : {1} · "
								"fiables avec l'ANCIENNE regle (4 visees a 8 px) : {2}\n",
								nProj, nNewKeep, nOldKeep);
					logger.Info("[PickScan] VISIBLES MAIS PERDUS par l'ancienne regle sous au moins une "
								"visee (= le bug rapporte) : {0} · caches pourtant acceptes (faux positifs) : {1}\n",
								nVisibleButRejected, nOldGhost);
				}
				// (L'occlusion au pick est gérée par un test de PROFONDEUR par rayon curseur,
				//  plus bas dans le bloc de sélection — indépendant de l'orientation caméra.)

				// Centroïde monde + BOÎTE ENGLOBANTE monde de la sélection courante.
				NkVec3f cen = {0.f, 0.f, 0.f};
				NkVec3f selMin{1e30f, 1e30f, 1e30f}, selMax{-1e30f, -1e30f, -1e30f};
				int32 selCnt = 0;
				for (int32 i = 0; i < nv; i++)
					if (st->vertSel[i]) {
						const NkVec3f w = worldV(i);
						cen = cen + w;
						selMin.x = NkMin(selMin.x, w.x);
						selMin.y = NkMin(selMin.y, w.y);
						selMin.z = NkMin(selMin.z, w.z);
						selMax.x = NkMax(selMax.x, w.x);
						selMax.y = NkMax(selMax.y, w.y);
						selMax.z = NkMax(selMax.z, w.z);
						selCnt++;
					}
				if (selCnt > 0)
					cen = cen * (1.f / (float32)selCnt);

				// ── POINT DE PIVOT (façon Blender) en EDIT MODE ───────────────────────
				// Le gizmo d'édition n'a qu'UNE cible : on lui donne directement le pivot
				// choisi comme base, si bien que son pivot interne (barycentre d'une seule
				// cible) EST le pivot demandé — rotation, échelle ET position d'affichage
				// suivent donc tous le mode courant, sans cas particulier.
				//   MEDIAN     : barycentre des sommets sélectionnés (défaut).
				//   BBOX       : centre de la boîte englobante de la sélection.
				//   CURSOR     : curseur 3D.
				//   INDIVIDUAL : gizmo au barycentre (comme Blender) ; la transform, elle,
				//                est appliquée par ÉLÉMENT (cf. plus bas).
				//   ACTIVE     : sommet ACTIF (dernier sélectionné).
				const int32 editPivotMode = st->editGizmo.PivotMode();
				NkVec3f pivotW = cen;
				if (selCnt > 0 && editPivotMode == renderer::NkGizmo3D::PIVOT_BBOX)
					pivotW = (selMin + selMax) * 0.5f;
				else if (editPivotMode == renderer::NkGizmo3D::PIVOT_CURSOR)
					pivotW = st->cursor3D;
				else if (editPivotMode == renderer::NkGizmo3D::PIVOT_ACTIVE && st->editActiveVert >= 0 &&
						 st->editActiveVert < nv)
					pivotW = worldV(st->editActiveVert);

				// Cible unique du gizmo = le PIVOT courant.
				renderer::NkGizmoTarget vt[1];
				vt[0] = {NkMat4f::Translate(pivotW), {0.001f, 0.001f, 0.001f}, 0.0001f};
				const int32 gcount = (selCnt > 0) ? 1 : 0;

				st->editGizmo.SetCamera(camPos, camTgt, 60.f, VW, VH);
				// P2 : repère « Normal » (Z = normale de l'élément sélectionné) recalculé
				// chaque frame HORS drag — pendant un drag on fige le repère, sinon les axes
				// tourneraient sous la souris au fur et à mesure que la surface se déforme.
				if (!st->editGizmo.IsDragging())
					Demo3D_UpdateEditNormalFrame(st);
				renderer::NkGizmoInput gin;
				gin.mouseX = ((float32)NkInput.MouseX() - nkvpOffX);
				gin.mouseY = ((float32)NkInput.MouseY() - nkvpOffY);
				// Delta RECALCULÉ par frame (pos - posPrécédente), pas MouseDeltaX() qui
				// reste figé à sa dernière valeur non nulle quand la souris s'arrête —
				// sinon le gizmo d'édition dérive à souris immobile (même bug que l'objet).
				gin.mouseDX = frameMDX;
				gin.mouseDY = frameMDY;
				// ── ARBITRAGE GIZMO <-> MAILLAGE (façon Blender) ─────────────────────
				// Le gizmo etait PRIORITAIRE sur le clic : ses poignees de PLAN et ses
				// RUBANS DE ROTATION sont de grandes courbes qui traversent le maillage,
				// elles avalaient donc des clics destines a un sommet/arete visible juste
				// dessous. Desormais on compare les DISTANCES ECRAN : le gizmo ne prend le
				// clic que s'il est REELLEMENT plus proche du curseur que le meilleur
				// candidat sommet/arete. (Les FACES ne participent pas a l'arbitrage : leur
				// « distance » serait 0 des que le curseur est dans la silhouette, et le
				// gizmo deviendrait inattrapable.)
				bool clickNow = st->pickPending;
				st->pickPending = false;
				// Pilote headless : NK_PICK_AT="x,y" force un clic de selection a ces pixels.
				if (st->pickForcePending) {
					st->pickForcePending = false;
					gin.mouseX = st->pickForceX;
					gin.mouseY = st->pickForceY;
					clickNow = true;
					logger.Info("[Demo3D] NK_PICK_AT -> clic force en ({0}, {1})\n", gin.mouseX, gin.mouseY);
				}
				float32 meshPx = 1e30f;
				if (clickNow && !st->editGizmo.IsDragging()) {
					if (st->editSelMask & 1)
						for (int32 i = 0; i < nv; i++) {
							float32 px, py;
							if (project(worldV(i), px, py)) {
								const float32 d = sqrtf((px - gin.mouseX) * (px - gin.mouseX) +
														(py - gin.mouseY) * (py - gin.mouseY));
								if (d < 14.f && d < meshPx)
									meshPx = d;
							}
						}
					if (st->editSelMask & 2)
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const uint32 ia = st->editEdges[e], ib = st->editEdges[e + 1];
							if (ia >= (uint32)nv || ib >= (uint32)nv)
								continue;
							float32 ax, ay, bx, by;
							if (!project(worldV((int32)ia), ax, ay) || !project(worldV((int32)ib), bx, by))
								continue;
							const float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
							float32 tt = (l2 > 1e-6f) ? ((gin.mouseX - ax) * dx + (gin.mouseY - ay) * dy) / l2 : 0.f;
							tt = tt < 0.f ? 0.f : (tt > 1.f ? 1.f : tt);
							const float32 qx = ax + tt * dx, qy = ay + tt * dy;
							const float32 d = sqrtf((gin.mouseX - qx) * (gin.mouseX - qx) +
													(gin.mouseY - qy) * (gin.mouseY - qy));
							if (d < 12.f && d < meshPx)
								meshPx = d;
						}
				}
				const float32 gizPx = st->editGizmo.HandlePickDistPx(gin.mouseX, gin.mouseY);
				const bool meshWinsClick = (meshPx < gizPx);
				if (st->pickDiag && clickNow)
					logger.Info("[PickDiag] arbitrage : maillage={0} px · gizmo={1} px -> {2}\n", meshPx, gizPx,
								meshWinsClick ? "MAILLAGE" : "GIZMO");
				gin.leftPressed = clickNow && !meshWinsClick;
				gin.leftDown = NkInput.IsMouseDown(NkMouseButton::NK_MB_LEFT);
				// ══ OPERATION MODALE EN COURS : APERCU TEMPS REEL (façon Blender) ══════
				// Un seul et meme mecanisme pour toutes les ops : on RESTAURE le snapshot puis
				// on ré-applique la commande avec les parametres courants. Souris = parametre
				// continu · molette = parametre entier · clic gauche = confirmer · Echap/clic
				// droit = annuler. Rien n'entre dans l'historique tant que ce n'est pas confirme.
				if (st->modalOp != 0) {
					// TO SPHERE : le CENTRE est le PIVOT courant, fige au lancement (comme
					// Blender) et ramene en espace maillage — sinon il deriverait a chaque
					// apercu puisque la geometrie bouge.
					if (st->modalFrames == 0)
						st->modalCenterLocal = st->editAnchorInv * pivotW;
					st->modalFrames++;
					// ── CURSEUR VIRTUEL : deltas souris REELS + deltas SYNTHETIQUES ────────
					// La souris est capturee par l'op : ses deltas bruts (modalMDX/MDY) ne vont
					// nulle part ailleurs. NK_MODAL_DRAG="dx,dy" injecte le meme signal, etale
					// sur NK_MODAL_DRAG_FRAMES frames -> le geste devient rejouable en capture.
					float32 injDX = 0.f, injDY = 0.f;
					if (st->modalInjDrag && st->modalFrames <= st->modalInjFrames) {
						injDX = st->modalInjDX / (float32)st->modalInjFrames;
						injDY = st->modalInjDY / (float32)st->modalInjFrames;
					}
					st->modalCurX += modalMDX + injDX;
					st->modalCurY += modalMDY + injDY;
					// MOLETTE -> parametre ENTIER (segments du bevel, coupes du loop cut, pas du spin).
					// NK_MODAL_WHEEL=<crans> injecte le meme signal une seule fois.
					int32 wheelNotches = 0;
					if (st->lastWheel != 0.f) {
						wheelNotches = (st->lastWheel > 0.f) ? 1 : -1;
						st->lastWheel = 0.f;
					}
					if (st->modalInjWheel != 0 && !st->modalInjWheelDone && st->modalFrames >= 2) {
						st->modalInjWheelDone = true;
						wheelNotches += st->modalInjWheel;
						logger.Info("[Demo3D] NK_MODAL_WHEEL -> {0} cran(s) de molette injectes\n",
									st->modalInjWheel);
					}
					if (wheelNotches != 0) {
						const int32 lo = (st->modalOp == 5) ? 3 : 1;
						const int32 hi = (st->modalOp == 5) ? 64 : 16;
						const int32 before = st->modalSeg;
						st->modalSeg = NkMax(lo, NkMin(hi, st->modalSeg + wheelNotches));
						if (st->modalSeg != before)
							st->modalDirty = true;
					}
					// SOURIS -> parametre CONTINU (largeur du bevel, epaisseur de l'inset, angle
					// du spin, hauteur de l'extrusion, SLIDE du loop cut). Course horizontale du
					// CURSEUR VIRTUEL depuis le lancement.
					if (st->modalScale != 0.f) {
						float32 nvv = st->modalBase + (st->modalCurX - st->modalStartX) * st->modalScale;
						if (st->modalOp == 5)
							nvv = NkMax(1.f, NkMin(360.f, nvv));
						else if (st->modalOp == 7)
							nvv = NkMax(0.f, NkMin(2.f, nvv)); // facteur To Sphere (Blender autorise > 1)
						else if (st->modalOp == 4)
							nvv = NkMax(-1.f, NkMin(1.f, nvv)); // SLIDE du loop cut : -1 .. +1
						else if (st->modalOp != 6 && st->modalOp != 8)
							nvv = NkMax(0.f, nvv); // largeur/epaisseur : jamais negative
						// (extrude et shrink/fatten sont SIGNES : on ne borne pas)
						if (fabsf(nvv - st->modalVal) > 1e-6f) {
							st->modalVal = nvv;
							st->modalDirty = true;
						}
					}
					// LOOP CUT : ANNEAU SOUS LE CURSEUR — l'apercu suit la souris AVANT
					// confirmation (c'etait la demande initiale). Le survol est teste sur la
					// topologie du SNAPSHOT, pas sur l'apercu (qui change a chaque frame).
					// OCCLUSION : sans test, le survol pouvait elire une arete CACHEE derriere le
					// maillage (elle est proche a l'ECRAN, mais invisible). On reutilise le test
					// d'occlusion reel du pick au clic (Demo3D_PointOccluded) sur le point de
					// l'arete le plus proche du curseur. Cout maitrise : on ne teste que les
					// candidats qui AMELIORENT le meilleur score courant (quelques-uns).
					if (st->modalOp == 4 &&
						(modalMDX != 0.f || modalMDY != 0.f || injDX != 0.f || injDY != 0.f || st->modalLoopA < 0)) {
						const float32 hx = st->modalCurX, hy = st->modalCurY;
						int32 ha = -1, hb = -1;
						float32 hbest = 1e30f;
						int32 occTests = 0, occRejects = 0;
						const uint32 snv = st->modalSnap.VertCount();
						for (uint32 e = 0; e + 1 < (uint32)st->modalSnapEdges.Size(); e += 2) {
							const uint32 ia = st->modalSnapEdges[e], ib = st->modalSnapEdges[e + 1];
							if (ia >= snv || ib >= snv)
								continue;
							const NkVec3f wa = st->editAnchor * st->modalSnap.verts[ia].pos;
							const NkVec3f wb = st->editAnchor * st->modalSnap.verts[ib].pos;
							float32 ax, ay, bx, by;
							if (!project(wa, ax, ay) || !project(wb, bx, by))
								continue;
							const float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
							float32 tt = (l2 > 1e-6f) ? ((hx - ax) * dx + (hy - ay) * dy) / l2 : 0.f;
							tt = tt < 0.f ? 0.f : (tt > 1.f ? 1.f : tt);
							const float32 qx = ax + tt * dx, qy = ay + tt * dy;
							const float32 d = sqrtf((hx - qx) * (hx - qx) + (hy - qy) * (hy - qy));
							if (d >= hbest)
								continue;
							// Point 3D correspondant au point ECRAN le plus proche : c'est LUI
							// qu'on teste (les extremites peuvent etre visibles alors que le
							// milieu de l'arete est masque, et reciproquement).
							const NkVec3f wq = wa + (wb - wa) * tt;
							occTests++;
							if (Demo3D_PointOccluded(st, camPos, wq, -1, -1)) {
								occRejects++;
								continue; // arete CACHEE : on ne la previsualise pas
							}
							hbest = d;
							ha = (int32)ia;
							hb = (int32)ib;
						}
						if (st->pickDiag && occTests > 0)
							logger.Info("[PickDiag] survol loop cut : {0} candidats testes, {1} rejetes (caches)\n",
										occTests, occRejects);
						if (ha >= 0 && (ha != st->modalLoopA || hb != st->modalLoopB)) {
							st->modalLoopA = ha;
							st->modalLoopB = hb;
							st->modalDirty = true;
						}
					}
					// APERCU : UNIQUEMENT si un parametre a REELLEMENT change depuis la derniere
					// application (la comparaison porte sur la VALEUR, pas sur « la souris a
					// bouge ») -> aucune reconstruction inutile du mesh GPU.
					if (st->modalDirty && st->modalHasApplied && st->modalVal == st->modalAppliedVal &&
						st->modalSeg == st->modalAppliedSeg && st->modalLoopA == st->modalAppliedLoopA &&
						st->modalLoopB == st->modalAppliedLoopB)
						st->modalDirty = false; // rien n'a bouge : on ne reconstruit RIEN
					if (st->modalDirty) {
						st->modalDirty = false;
						Demo3D_ModalPreview(st, meshSysF);
					}
					// Pilote headless : le drag injecte doit s'etre DEROULE avant de confirmer /
					// d'annuler, sinon on ne prouverait rien de l'evolution de l'apercu.
					const int32 injEnd = st->modalInjDrag ? (st->modalInjFrames + 2) : 3;
					const bool autoConfirm = (st->modalEnvConfirm && st->modalFrames > injEnd);
					if (st->modalInjCancel && st->modalFrames > injEnd) {
						st->modalInjCancel = false;
						st->modalCancelPending = true;
						logger.Info("[Demo3D] NK_MODAL_CANCEL -> Echap synthetique envoye a l'op modale\n");
					}
					if (st->modalCancelPending) {
						st->modalCancelPending = false;
						Demo3D_ModalCancel(st, meshSysF);
					} else if (clickNow || autoConfirm) {
						Demo3D_ModalConfirm(st, meshSysF);
					}
					// L'operation modale CONSOMME la souris : ni pick, ni poignee de gizmo, ni
					// outil de selection. (Les DELTAS et la MOLETTE ont deja ete neutralises par
					// le verrou unique en amont ; ici on neutralise le CLIC, dernier canal.)
					clickNow = false;
					gin.leftPressed = false;
					gin.leftDown = false;
					nv = (int32)st->editRest.Size(); // la topologie vient peut-etre de changer
				}
				gin.shiftDown = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
				gin.ctrlDown = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
				gin.lockAxis = -1;
				if (st->editGizmo.IsDragging()) {
					if (NkInput.IsKeyDown(NkKey::NK_X))
						gin.lockAxis = 0;
					else if (NkInput.IsKeyDown(NkKey::NK_Y))
						gin.lockAxis = 1;
					else if (NkInput.IsKeyDown(NkKey::NK_Z))
						gin.lockAxis = 2;
				}
				const bool wasDrag = st->editGizmo.IsDragging();
				st->editGizmo.Update(vt, gcount, gin);
				const bool grabbedHandle = (!wasDrag && st->editGizmo.IsDragging());

				// Clic qui n'a PAS attrapé une poignée -> pick VERTEX/EDGE/FACE en espace écran.
				// COUTEAU/BISECT armé : les 2 clics tracent la ligne -> plan de coupe.
				if (clickNow && !grabbedHandle && st->knifeArmed) {
					NkVec2f pc{gin.mouseX, gin.mouseY};
					if (!st->knifeHasP0) {
						st->knifeP0 = pc;
						st->knifeHasP0 = true;
					} else {
						auto rayOf = [&](NkVec2f s) -> NkVec3f {
							float32 nx = s.x / VW * 2.f - 1.f, ny = 1.f - s.y / VH * 2.f;
							NkVec3f d = fwd + rgt * (nx * thX) + upv * (ny * thY);
							float32 l = d.Len();
							return (l > 1e-6f) ? d * (1.f / l) : fwd;
						};
						NkVec3f d0 = rayOf(st->knifeP0), d1 = rayOf(pc), nrm = d0.Cross(d1);
						float32 l = nrm.Len();
						if (l > 1e-4f) {
							st->bisectPt = camPos;
							st->bisectN = nrm * (1.f / l);
							st->editBisectPending = true;
						}
						st->knifeArmed = false;
						st->knifeHasP0 = false;
					}
					st->editOverlayDirty = true;
				}
				// ── OUTILS DE SÉLECTION PAR ZONE (rectangle / lasso / cercle) ─────────
				// Traités AVANT le pick ponctuel : tant qu'un outil est actif, il consomme
				// le clic. Modificateurs façon Blender : Shift = ajouter, Ctrl = retirer.
				bool zoneToolConsumed = false;
				const bool altDown = NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
				if (st->selTool == 3) { // CERCLE modal : on peint tant que le bouton est tenu
					st->selX1 = gin.mouseX;
					st->selY1 = gin.mouseY;
					if (st->lastWheel != 0.f) {
						st->selCircleR += st->lastWheel * 6.f;
						st->selCircleR = NkMax(6.f, NkMin(400.f, st->selCircleR));
					}
					if (gin.leftDown) {
						const float32 cx = gin.mouseX, cy = gin.mouseY, r2 = st->selCircleR * st->selCircleR;
						Demo3D_SelectInZone(
							st, gin.ctrlDown ? 2 : 1, camPos,
							[&](float32 px, float32 py) {
								return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r2;
							},
							project);
					}
					zoneToolConsumed = true;
				} else if (clickNow && !grabbedHandle && !st->knifeArmed && !altDown &&
						   (st->selTool == 1 || gin.ctrlDown)) {
					// Démarre un tracé : RECTANGLE si armé par B, sinon LASSO (Ctrl+glisser).
					st->selDragging = true;
					st->selTool = (st->selTool == 1) ? 1 : 2;
					st->selX0 = st->selX1 = gin.mouseX;
					st->selY0 = st->selY1 = gin.mouseY;
					st->selLasso.Clear();
					st->selLasso.PushBack(NkVec2f{gin.mouseX, gin.mouseY});
					// Rectangle : Shift=ajouter, Ctrl=retirer. Lasso (déjà Ctrl) : Shift=retirer.
					st->selMode = (st->selTool == 1) ? (gin.shiftDown ? 1 : (gin.ctrlDown ? 2 : 0))
													 : (gin.shiftDown ? 2 : 1);
					zoneToolConsumed = true;
				}
				if (st->selDragging) {
					st->selX1 = gin.mouseX;
					st->selY1 = gin.mouseY;
					if (st->selTool == 2) {
						// Lasso : on n'ajoute un point que s'il s'éloigne (contour léger).
						const NkVec2f &lp = st->selLasso[(uint32)st->selLasso.Size() - 1];
						if (fabsf(lp.x - gin.mouseX) + fabsf(lp.y - gin.mouseY) > 3.f)
							st->selLasso.PushBack(NkVec2f{gin.mouseX, gin.mouseY});
					}
					if (!gin.leftDown) { // relache -> on applique la zone
						// Meme garde qu'en mode objet : un rectangle de moins de
						// 4 px est un CLIC, pas une zone a appliquer.
						const bool tinyEd = fabsf(st->selX1 - st->selX0) < 4.f &&
											fabsf(st->selY1 - st->selY0) < 4.f;
						if (tinyEd && st->selTool == 1) {
							// clic simple : le pick ponctuel fera foi
						} else if (st->selTool == 1) {
							const float32 x0 = NkMin(st->selX0, st->selX1), x1 = NkMax(st->selX0, st->selX1);
							const float32 y0 = NkMin(st->selY0, st->selY1), y1 = NkMax(st->selY0, st->selY1);
							Demo3D_SelectInZone(
								st, st->selMode, camPos,
								[&](float32 px, float32 py) { return px >= x0 && px <= x1 && py >= y0 && py <= y1; },
								project);
							logger.Info("[Demo3D] Selection RECTANGLE appliquee\n");
						} else if (st->selTool == 2 && st->selLasso.Size() >= 3) {
							Demo3D_SelectInZone(
								st, st->selMode, camPos,
								[&](float32 px, float32 py) { return Demo3D_PointInPoly(st->selLasso, px, py); },
								project);
							logger.Info("[Demo3D] Selection LASSO appliquee ({0} points)\n",
										(uint32)st->selLasso.Size());
						}
						st->selDragging = false;
						st->selTool = 0; // one-shot, comme Blender
						st->selLasso.Clear();
					}
					zoneToolConsumed = true;
				}

				// Pick sur la BASE (editRest/editIdx = editHE), même sous modificateurs -> on
				// sélectionne/édite la cage de base et le résultat modifié se recalcule.
				if (clickNow && !grabbedHandle && !st->knifeArmed && !zoneToolConsumed) {
					st->editOverlayDirty = true; // la sélection va changer -> reconstruire l'overlay
					const float32 mx = gin.mouseX, my = gin.mouseY;
					// TOGGLE façon Blender : on mémorise l'état AVANT le nettoyage pour savoir
					// si l'élément cliqué était DÉJÀ sélectionné -> dans ce cas le clic le
					// DÉSÉLECTIONNE (au lieu de le re-sélectionner). Shift+clic = toggle sans
					// vider le reste de la sélection.
					NkVector<uint8> prevSel = st->vertSel;
					auto wasSel = [&](uint32 i) { return i < (uint32)prevSel.Size() && prevSel[i] != 0; };
					if (!gin.shiftDown)
						for (int32 i = 0; i < nv; i++)
							st->vertSel[i] = 0;
					// Rayon curseur -> profondeurs d'ENTRÉE (near) et de SORTIE (far) dans le
					// mesh. Un sommet est "devant" (visible) s'il est dans la moitié NEAR
					// (depth < milieu). Test de PROFONDEUR (pas de normale) -> INDÉPENDANT de
					// l'orientation caméra (corrige "impossible de sélectionner selon l'angle").
					// ── VISIBILITE : OCCLUSION REELLE (remplace l'heuristique « moitie near ») ──
					// CAUSE RACINE du « sous certains angles, impossible de cliquer un sommet/une
					// arete pourtant visible » : on calculait les profondeurs d'ENTREE (tNear) et de
					// SORTIE (tFar) du rayon curseur dans le maillage, puis on ne gardait que les
					// elements situes dans la MOITIE AVANT (depth < (tNear+tFar)/2). Cette
					// heuristique n'est vraie que si la surface est ~perpendiculaire a la vue : sur
					// une face RASANTE (ou un maillage fin, ou un rayon qui frole la silhouette ->
					// tNear ~= tFar), la profondeur varie enormement d'un bout a l'autre de la face
					// et des sommets PARFAITEMENT VISIBLES tombaient derriere le plan median ->
					// rejetes. Idem pour le filtre « dos-camera » par NORMALE : un sommet de
					// SILHOUETTE a une normale ~perpendiculaire a la vue (produit scalaire ~= 0) et
					// basculait au hasard du bruit numerique.
					// MAINTENANT : le rayon curseur ne sert plus qu'au pick de FACE ; la visibilite
					// d'un candidat sommet/arete est testee par un VRAI lancer de rayon
					// camera -> candidat (les triangles INCIDENTS au candidat sont ignores, sans
					// quoi un sommet de silhouette serait occulte par sa propre face). Comme le test
					// ne tourne que sur les quelques candidats SOUS LE CURSEUR, il reste bon marche.
					const float32 rNdcX = mx / VW * 2.f - 1.f, rNdcY = 1.f - my / VH * 2.f;
					NkVec3f rDir = fwd + rgt * (rNdcX * thX) + upv * (rNdcY * thY);
					{
						float32 l = rDir.Len();
						if (l > 1e-6f)
							rDir = rDir * (1.f / l);
					}
					float32 tNear = 1e30f;
					// tFar / depthMid ne servent PLUS a filtrer : ils ne sont conserves que pour le
					// DIAGNOSTIC (NK_PICK_DIAG), afin de montrer noir sur blanc ce que l'ANCIENNE
					// heuristique « moitie near » aurait rejete.
					float32 tFar = -1e30f;
					int32 nearestTri = -1;
					for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
						const NkVec3f v0 = worldV(st->editIdx[t]), v1 = worldV(st->editIdx[t + 1]),
										v2 = worldV(st->editIdx[t + 2]);
						NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = rDir.Cross(e2);
						float32 aa = e1.Dot(h);
						if (fabsf(aa) < 1e-7f)
							continue;
						float32 f = 1.f / aa;
						NkVec3f sv = camPos - v0;
						float32 u = f * sv.Dot(h);
						if (u < 0.f || u > 1.f)
							continue;
						NkVec3f q = sv.Cross(e1);
						float32 vv = f * rDir.Dot(q);
						if (vv < 0.f || u + vv > 1.f)
							continue;
						float32 tt = f * e2.Dot(q);
						if (tt > 1e-4f) {
							if (tt < tNear) {
								tNear = tt;
								nearestTri = (int32)t;
							}
							if (tt > tFar)
								tFar = tt;
						}
					}
					// Le point `w` est-il cache par une face ? `sa`/`sb` = indices a IGNORER
					// (les triangles qui touchent le candidat lui-meme). IMPLEMENTATION UNIQUE
					// (Demo3D_PointOccluded), partagee avec le SURVOL de l'anneau de loop cut.
					auto occluded = [&](NkVec3f w, int32 sa, int32 sb) -> bool {
						return Demo3D_PointOccluded(st, camPos, w, sa, sb);
					};
					// ── CANDIDATS : le plus proche du CURSEUR gagne (Blender), la PROFONDEUR ne
					//    sert qu'a departager les quasi-ex aequo (fenetre de 1.5 px).
					// AVANT : n'importe quel element dans le rayon l'emportait pourvu qu'il soit le
					// plus proche de la camera -> un sommet a 13 px battait celui pile sous le
					// curseur. Desormais l'ordre est : distance ecran, puis profondeur.
					static const int32 kMaxCand = 32;
					const float32 kTiePx = 1.5f;
					int32 cI[kMaxCand], cJ[kMaxCand];
					float32 cD[kMaxCand], cZ[kMaxCand];
					NkVec3f cP[kMaxCand];
					int32 nC = 0;
					// Insertion TRIEE par distance ecran croissante (tableau borne : on garde les
					// kMaxCand meilleurs). Zero STL, zero allocation.
					auto pushCand = [&](int32 ia, int32 ib, float32 d, float32 z, NkVec3f pw) {
						if (nC == kMaxCand && d >= cD[kMaxCand - 1])
							return;
						int32 pos = (nC < kMaxCand) ? nC : (kMaxCand - 1);
						if (nC < kMaxCand)
							nC++;
						while (pos > 0 && cD[pos - 1] > d) {
							cI[pos] = cI[pos - 1];
							cJ[pos] = cJ[pos - 1];
							cD[pos] = cD[pos - 1];
							cZ[pos] = cZ[pos - 1];
							cP[pos] = cP[pos - 1];
							pos--;
						}
						cI[pos] = ia;
						cJ[pos] = ib;
						cD[pos] = d;
						cZ[pos] = z;
						cP[pos] = pw;
					};
					// Parcourt les candidats tries et retient le PREMIER non occulte, puis, dans
					// la fenetre d'egalite, celui qui est le plus PROCHE DE LA CAMERA.
					auto electCand = [&](int32 &oa, int32 &ob) {
						oa = -1;
						ob = -1;
						float32 winD = 1e30f, winZ = 1e30f;
						for (int32 k = 0; k < nC; k++) {
							if (oa >= 0 && cD[k] > winD + kTiePx)
								break;
							if (occluded(cP[k], cI[k], cJ[k]))
								continue;
							if (oa < 0 || cZ[k] < winZ) {
								if (oa < 0)
									winD = cD[k];
								winZ = cZ[k];
								oa = cI[k];
								ob = cJ[k];
							}
						}
					};
					// VERTEX : rayon de pick en PIXELS ECRAN (constant, independant du zoom).
					const float32 kVertPx = 14.f;
					int32 bestV = -1, bestVdummy = -1;
					if (st->editSelMask & 1) {
						nC = 0;
						for (int32 i = 0; i < nv; i++) {
							NkVec3f w = worldV(i);
							float32 px, py;
							if (!project(w, px, py))
								continue;
							const float32 d = sqrtf((px - mx) * (px - mx) + (py - my) * (py - my));
							if (d < kVertPx)
								pushCand(i, i, d, (w - camPos).Len(), w);
						}
						if (st->pickDiag)
							logger.Info("[PickDiag] VERTEX : {0} candidat(s) dans {1} px\n", nC, kVertPx);
						electCand(bestV, bestVdummy);
					}
					// EDGE : distance ecran au SEGMENT ; le point teste pour l'occlusion est le
					// point de l'arete le plus proche du curseur (et non son milieu : sur une
					// arete longue et rasante, le milieu peut etre cache alors que le bout
					// cliquable est parfaitement visible).
					const float32 kEdgePx = 12.f;
					int32 bestEa = -1, bestEb = -1;
					if (st->editSelMask & 2) {
						nC = 0;
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const int32 ia = (int32)st->editEdges[e], ib = (int32)st->editEdges[e + 1];
							if (ia >= nv || ib >= nv)
								continue;
							const NkVec3f wa = worldV(ia), wb = worldV(ib);
							float32 ax, ay, bx, by;
							if (!project(wa, ax, ay) || !project(wb, bx, by))
								continue;
							const float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
							float32 tt = (l2 > 1e-6f) ? ((mx - ax) * dx + (my - ay) * dy) / l2 : 0.f;
							tt = tt < 0.f ? 0.f : (tt > 1.f ? 1.f : tt);
							const float32 qx = ax + tt * dx, qy = ay + tt * dy;
							const float32 d = sqrtf((mx - qx) * (mx - qx) + (my - qy) * (my - qy));
							if (d >= kEdgePx)
								continue;
							const NkVec3f pw = wa + (wb - wa) * tt;
							pushCand(ia, ib, d, (pw - camPos).Len(), pw);
						}
						if (st->pickDiag)
							logger.Info("[PickDiag] EDGE : {0} candidat(s) dans {1} px\n", nC, kEdgePx);
						electCand(bestEa, bestEb);
					}
					// FACE : le triangle le plus PROCHE touche par le rayon curseur (nearestTri).
					const int32 bestFt = (st->editSelMask & 4) ? nearestTri : -1;
					// PREUVE : verdict de l'ANCIENNE regle (« moitie near ») sur l'element elu.
					if (st->pickDiag) {
						const float32 depthMid = (tNear < 1e29f) ? 0.5f * (tNear + tFar) : 1e30f;
						if (bestV >= 0) {
							const float32 dep = (worldV(bestV) - camPos).Len();
							logger.Info("[PickDiag] sommet elu #{0} : profondeur={1} · plan median ancien={2} -> ANCIENNE REGLE = {3}\n",
									bestV, dep, depthMid, (dep < depthMid + 1e-3f) ? "gardait" : "REJETAIT (bug)");
						}
						if (bestEa >= 0) {
							const NkVec3f midw = (worldV(bestEa) + worldV(bestEb)) * 0.5f;
							const float32 dep = (midw - camPos).Len();
							logger.Info("[PickDiag] arete elue ({0},{1}) : profondeur milieu={2} · plan median ancien={3} -> ANCIENNE REGLE = {4}\n",
									bestEa, bestEb, dep, depthMid, (dep < depthMid + 1e-3f) ? "gardait" : "REJETAIT (bug)");
						}
					}
					if (st->pickDiag)
						logger.Info("[PickDiag] clic ({0},{1}) -> vertex={2} arete=({3},{4}) face(tri)={5}\n", mx,
									my, bestV, bestEa, bestEb, bestFt);
					// Élection PAR PRIORITÉ façon Blender : vertex (près d'un sommet) > arête
					// (près d'une arête) > face (rayon). Chacun n'est retenu que dans son seuil.
					// ── ALT+CLIC : BOUCLE (edge loop / face loop) façon Blender ──────
					// Alt+clic sur une ARÊTE -> toute la boucle d'arêtes ; sur une FACE ->
					// l'anneau de faces. Shift+Alt+clic ajoute à la sélection existante.
					// Ce parcours n'est possible que grâce à la SOUDURE topologique.
					bool loopDone = false;
					if (altDown && (bestEa >= 0 || bestFt >= 0)) {
						uint32 la = 0, lb = 0;
						bool faceLoop = false, ok = false;
						if (bestEa >= 0) { // une arête est sous le curseur -> edge loop
							la = (uint32)bestEa;
							lb = (uint32)bestEb;
							ok = true;
						} else { // sinon la face touchée -> anneau de faces
							renderer::NkEmId f = Demo3D_FaceOfTri(st, (uint32)bestFt / 3u);
							NkVector<renderer::NkEmId> fvl;
							if (f != renderer::NK_EM_INVALID)
								st->editHE.GetFaceVerts(f, fvl);
							if (fvl.Size() >= 2) {
								la = fvl[0];
								lb = fvl[1];
								faceLoop = true;
								ok = true;
							}
						}
						if (ok) {
							Demo3D_SelectLoop(st, la, lb, faceLoop, gin.shiftDown);
							loopDone = true;
						}
					}
					if (loopDone) {
						// sélection déjà posée + normalisée par Demo3D_SelectLoop
					} else if (bestV >= 0) {
						// Déjà sélectionné -> DÉSÉLECTION (toggle), sinon sélection.
						const uint8 on = wasSel((uint32)bestV) ? (uint8)0 : (uint8)1;
						st->vertSel[bestV] = on;
						st->editActiveVert = on ? bestV : -1; // actif = dernier sélectionné (blanc)
						// Un clic sur un SOMMET périme l'arête et la face actives : garder
						// un ancien élément actif d'un autre type ferait cohabiter deux
						// « références » blanches et on ne saurait plus laquelle fait foi.
						st->editActiveEdgeA = st->editActiveEdgeB = -1;
						st->editActiveFace = -1;
					} else if (bestEa >= 0) {
						// Une arête est « déjà sélectionnée » si ses DEUX extrémités l'étaient.
						const uint8 on = (wasSel((uint32)bestEa) && wasSel((uint32)bestEb)) ? (uint8)0 : (uint8)1;
						st->vertSel[bestEa] = on;
						st->vertSel[bestEb] = on;
						st->editActiveVert = on ? bestEb : -1;
						st->editActiveEdgeA = on ? bestEa : -1;
						st->editActiveEdgeB = on ? bestEb : -1;
						st->editActiveFace = -1;
					} else if (bestFt >= 0) {
						// FACE N-GON : triangle touché -> sa face n-gon (quadify) -> tous ses sommets.
						// Face « déjà sélectionnée » = TOUS ses sommets l'étaient (même convention
						// que le remplissage orange) -> le clic la vide.
						renderer::NkEmId f = Demo3D_FaceOfTri(st, (uint32)bestFt / 3u);
						NkVector<renderer::NkEmId> fv;
						if (f != renderer::NK_EM_INVALID) {
							st->editHE.GetFaceVerts(f, fv);
						} else {
							fv.PushBack(st->editIdx[bestFt]);
							fv.PushBack(st->editIdx[bestFt + 1]);
							fv.PushBack(st->editIdx[bestFt + 2]);
						}
						bool allWere = fv.Size() > 0;
						for (uint32 k = 0; k < (uint32)fv.Size(); k++)
							if (!wasSel(fv[k]))
								allWere = false;
						const uint8 on = allWere ? (uint8)0 : (uint8)1;
						for (uint32 k = 0; k < (uint32)fv.Size(); k++) {
							st->vertSel[fv[k]] = on;
							if (on)
								st->editActiveVert = (int32)fv[k];
						}
						if (!on)
							st->editActiveVert = -1;
						// La FACE active est l'identifiant n-gon, pas le triangle touché :
						// une face quadrangulaire couvre deux triangles, et retenir le
						// triangle ferait clignoter l'actif selon la moitié cliquée.
						st->editActiveFace = (on && f != renderer::NK_EM_INVALID) ? (int32)f : -1;
						st->editActiveEdgeA = st->editActiveEdgeB = -1;
					}
					// Sélection étendue à tous les sommets COÏNCIDENTS : sans ça, le coin retenu
					// peut appartenir à une face qui tourne le dos à la caméra -> son marqueur
					// serait masqué et le sommet paraîtrait NON sélectionné (régression vue par
					// l'auteur). Après propagation, au moins une copie visible passe en orange.
					Demo3D_NormalizeSel(st);
					// L'ACTIF (marqueur BLANC) doit lui aussi être une copie VISIBLE : on le
					// recale sur le sommet coïncident dont la normale fait face à la caméra.
					if (st->editActiveVert >= 0 && st->editActiveVert < nv) {
						const NkVec3f aw = worldV(st->editActiveVert);
						const NkVec3f orgA = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
						for (int32 i = 0; i < nv; i++) {
							if (!st->vertSel[i] || (worldV(i) - aw).Len() > 1e-4f)
								continue;
							const NkVec3f nW = (st->editAnchor * st->editRest[i].normal) - orgA;
							if (nW.Dot(camPos - aw) > 0.f) {
								st->editActiveVert = i;
								break;
							}
						}
					}
					// L'ACTIF ne doit jamais rester « fantôme » : s'il vient d'être désélectionné
					// (ou n'existe plus), on retombe sur le dernier sommet encore sélectionné.
					if (st->editActiveVert < 0 || st->editActiveVert >= nv || !st->vertSel[st->editActiveVert]) {
						st->editActiveVert = -1;
						for (int32 i = nv - 1; i >= 0; i--)
							if (st->vertSel[i]) {
								st->editActiveVert = i;
								break;
							}
					}
				}
				// Garder la cible-0 sélectionnée pour que les poignées s'affichent.
				if (selCnt > 0 && !st->editGizmo.IsDragging())
					st->editGizmo.SelectAll();

				// Début de drag -> snapshot du pré-état (positions AVANT déplacement) pour l'undo.
				if (!st->editWasDragging && st->editGizmo.IsDragging() && selCnt > 0) {
					Demo3D_PushSel(st);
					st->editDragSnap = st->editHE;
					st->editDragSnapValid = true;
				}
				// Drag : applique la transform de groupe aux verts sélectionnés, AUTOUR DU
				// POINT DE PIVOT courant (façon Blender). ApplyAbout() recompose le décalage
				// utilisateur (translation + rotation + échelle) autour d'un point MONDE
				// arbitraire -> un seul chemin pour les 5 modes.
				if ((st->editGizmo.IsDragging() || st->editForceXform) && selCnt > 0) {
					// ORIGINES INDIVIDUELLES : chaque FACE entièrement sélectionnée est
					// transformée autour de SON PROPRE barycentre. Un sommet partagé par
					// plusieurs faces sélectionnées prend la MOYENNE de leurs centres (cas
					// des coins soudés) ; un sommet sélectionné n'appartenant à AUCUNE face
					// entièrement sélectionnée (sélection de sommets/arêtes isolés) retombe
					// sur le pivot commun — limite honnête, Blender y utilise ses « îlots ».
					const bool individual = (editPivotMode == renderer::NkGizmo3D::PIVOT_INDIVIDUAL);
					NkVector<NkVec3f> indOrg;
					NkVector<uint32> indCnt;
					if (individual) {
						indOrg.Resize((uint32)nv);
						indCnt.Resize((uint32)nv);
						for (int32 i = 0; i < nv; i++) {
							indOrg[(uint32)i] = {0.f, 0.f, 0.f};
							indCnt[(uint32)i] = 0;
						}
						NkVector<renderer::NkEmId> fvi;
						for (uint32 f = 0; f < (uint32)st->editHE.faces.Size(); f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							fvi.Clear();
							st->editHE.GetFaceVerts(f, fvi);
							const uint32 fn = (uint32)fvi.Size();
							if (fn < 2)
								continue;
							bool allSel = true;
							for (uint32 k = 0; k < fn && allSel; k++)
								if (fvi[k] >= (uint32)st->vertSel.Size() || !st->vertSel[fvi[k]])
									allSel = false;
							if (!allSel)
								continue;
							NkVec3f fc{0.f, 0.f, 0.f};
							for (uint32 k = 0; k < fn; k++)
								fc = fc + worldV((int32)fvi[k]);
							fc = fc * (1.f / (float32)fn);
							for (uint32 k = 0; k < fn; k++) {
								const uint32 vi = fvi[k];
								if (vi < (uint32)nv) {
									indOrg[vi] = indOrg[vi] + fc;
									indCnt[vi]++;
								}
							}
						}
					}
					const NkMat4f Gcommon = st->editGizmo.ApplyAbout(0, pivotW);
					for (int32 i = 0; i < nv; i++) {
						st->editLive[i] = st->editRest[i];
						if (!st->vertSel[i])
							continue;
						if (individual && indCnt[(uint32)i] > 0) {
							const NkVec3f O = indOrg[(uint32)i] * (1.f / (float32)indCnt[(uint32)i]);
							st->editLive[i].pos = st->editAnchorInv * (st->editGizmo.ApplyAbout(0, O) * worldV(i));
						} else
							st->editLive[i].pos = st->editAnchorInv * (Gcommon * worldV(i));
					}
					// Update rapide du mesh solide seulement s'il est 1:1 avec la cage (ni
					// modificateur, ni coins dédoublés par l'ombrage FLAT) : sinon le solide a un
					// AUTRE nombre de sommets -> on laisse la cage bouger et le solide se recale en
					// fin de drag. L'overlay (cage) suit toujours via editLive.
					if (meshSysF && st->editDisplay1to1)
						meshSysF->UpdateVertices(st->editMesh, st->editLive.Data(), (uint32)nv);
					st->editOverlayDirty = true; // positions changées -> overlay suit le mesh
				}
				// Fin de drag -> baker les positions dans l'AUTORITÉ editHE + RECALCUL des
				// normales (la surface a changé -> l'éclairage suit). Topologie inchangée
				// -> pas de re-triangulation, juste positions+normales.
				if (st->editWasDragging && !st->editGizmo.IsDragging()) {
					const uint32 hv = st->editHE.VertCount();
					for (int32 i = 0; i < nv; i++) {
						st->editRest[i] = st->editLive[i];
						if ((uint32)i < hv)
							st->editHE.verts[i].pos = st->editLive[i].pos;
					}
					st->editHE.RecomputeNormals();
					for (int32 i = 0; i < nv && (uint32)i < hv; i++)
						st->editRest[i].normal = st->editHE.verts[i].normal;
					st->editLive = st->editRest;
					// Mesh d'affichage 1:1 : update rapide. Sinon (modificateurs / coins dédoublés
					// par l'ombrage FLAT) : régénérer -> le résultat affiché se recale sur les
					// nouvelles positions de la base.
					if (st->editDisplay1to1) {
						if (meshSysF)
							meshSysF->UpdateVertices(st->editMesh, st->editLive.Data(), (uint32)nv);
					} else if (meshSysF) {
						Demo3D_SyncFromHE(st, meshSysF);
					}
					st->editGizmo.ResetSelected();
					st->editOverlayDirty = true;
					// Enregistre le déplacement dans l'historique (pré-état snapshoté au début)
					// ET comme commande Move (donnée rejouable : delta par sommet déplacé).
					if (st->editDragSnapValid) {
						st->editHistory.Commit(st->editDragSnap);
						renderer::NkMeshEditCommand mc;
						mc.op = renderer::NkMeshEditOp::Move;
						const uint32 hv = st->editHE.VertCount(), bv = st->editDragSnap.VertCount();
						for (uint32 i = 0; i < hv && i < bv; ++i) {
							NkVec3f d = st->editHE.verts[i].pos - st->editDragSnap.verts[i].pos;
							if (d.x != 0.f || d.y != 0.f || d.z != 0.f) {
								mc.selection.PushBack(i);
								mc.moveDeltas.PushBack(d);
							}
						}
						if (mc.selection.Size() > 0)
							st->editRecorder.Push(mc);
						st->editDragSnapValid = false;
					}
				}
				st->editWasDragging = st->editGizmo.IsDragging();

				// Dessin du mesh édité à son ancre (les vertices sont en espace LOCAL).
				// ⚠ dc.aabb doit être en espace MONDE : NkRender3D::Submit() frustum-cull le
				// draw call avec camera.IsAABBVisible(dc.aabb) SANS lui appliquer dc.transform
				// (comme tous les autres draw calls de la démo, qui passent déjà du monde).
				// Une AABB LOCALE ici décrivait une boîte au voisinage de l'ORIGINE : sur un
				// objet éloigné (colonne #18 en (1,1,4)) le mesh était cullé dès que l'origine
				// sortait du champ -> cage + marqueurs visibles (dessinés en debug, non cullés)
				// mais AUCUNE surface solide. On transforme donc les 8 coins de l'AABB locale
				// par l'ancre, fusionnés avec la position live des sommets (déformation en cours
				// de drag, où le mesh GPU est mis à jour sans repasser par Demo3D_SyncFromHE).
				{
					NkDrawCall3D dc;
					dc.mesh = st->editMesh;
					dc.transform = st->editAnchor;
					NkVec3f amin{1e30f, 1e30f, 1e30f}, amax{-1e30f, -1e30f, -1e30f};
					auto growW = [&](NkVec3f w) {
						amin.x = NkMin(amin.x, w.x);
						amin.y = NkMin(amin.y, w.y);
						amin.z = NkMin(amin.z, w.z);
						amax.x = NkMax(amax.x, w.x);
						amax.y = NkMax(amax.y, w.y);
						amax.z = NkMax(amax.z, w.z);
					};
					const NkVec3f lmn = st->editLocalMin, lmx = st->editLocalMax;
					for (int32 c = 0; c < 8; c++)
						growW(st->editAnchor * NkVec3f{(c & 1) ? lmx.x : lmn.x, (c & 2) ? lmx.y : lmn.y,
													   (c & 4) ? lmx.z : lmn.z});
					for (int32 i = 0; i < nv; i++)
						growW(st->editAnchor * st->editLive[i].pos);
					dc.aabb = {amin, amax};
					dc.tint = effTint(st->editObjTint); // materiau de l'objet (gris en SOLID/WIREFRAME)
					dc.metallic = st->editObjMetallic;
					dc.roughness = st->editObjRoughness;
					HostMatHook(st->editObjIdx, dc);
					r3d->Submit(dc);
				}

				// ── Overlay d'édition PERSISTANT (batch GPU) ───────────────────────────
				// Reconstruit SEULEMENT quand ça change (entrée/sélection/drag/mode/xray).
				// L'orbite caméra ne reconstruit RIEN : le GPU redessine les buffers gardés
				// -> fluide même sur mesh dense. Occlusion = depth-test (X-ray OFF) ; offset
				// le long de la NORMALE (indépendant caméra) pour vaincre le z-fighting.
				if (st->editOverlayDirty) {
					st->editOverlayDirty = false;
					auto liveW = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					auto normW = [&](int32 i) -> NkVec3f {
						NkVec3f nW = (st->editAnchor * (st->editLive[i].pos + st->editLive[i].normal)) - liveW(i);
						float32 l = nW.Len();
						return (l > 1e-6f) ? nW * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
					};
					// Rayon monde (dimensionne points + offset).
					NkVec3f bmin{1e30f, 1e30f, 1e30f}, bmax{-1e30f, -1e30f, -1e30f};
					for (int32 i = 0; i < nv; i++) {
						NkVec3f w = liveW(i);
						bmin.x = NkMin(bmin.x, w.x);
						bmin.y = NkMin(bmin.y, w.y);
						bmin.z = NkMin(bmin.z, w.z);
						bmax.x = NkMax(bmax.x, w.x);
						bmax.y = NkMax(bmax.y, w.y);
						bmax.z = NkMax(bmax.z, w.z);
					}
					float32 rad = (bmax - bmin).Len() * 0.5f;
					if (rad < 1e-4f)
						rad = 1.f;
					const float32 dotS = rad * 0.012f; // demi-taille des points (monde)
					// Plus de décalage le long de la normale : cage/points/faces sont
					// COPLANAIRES à la surface. Le depth-bias des pipelines (lignes + fill)
					// évite le z-fighting -> tout COLLE au modèle (pas de flottement).
					auto pushV = [&](NkVector<float> &A, NkVec3f p, NkVec4f c) {
						A.PushBack(p.x);
						A.PushBack(p.y);
						A.PushBack(p.z);
						A.PushBack(c.x);
						A.PushBack(c.y);
						A.PushBack(c.z);
						A.PushBack(c.w);
					};
					// Palette Blender Edit Mode : cage NOIRE fine, sélection JAUNE-ORANGE VIF.
					const NkVec4f cageCol{0.015f, 0.015f, 0.02f, 1.f}; // arête non sélectionnée
					const NkVec4f selEdgeCol{1.f, 0.70f, 0.13f, 1.f};  // arête sélectionnée (vif)
					const NkVec4f actVertCol{1.f, 1.f, 1.f, 1.f};	   // extrémité = sommet ACTIF
					// ── P0 — CAGE = ARÊTES RÉELLES DU N-GON ──────────────────────────────
					// st->editEdges vient de NkEditMesh::GetUniqueEdges (topologie demi-arête,
					// chaque arête UNE SEULE FOIS, arêtes internes dissoutes par Quadify
					// exclues). On ne dessine JAMAIS les arêtes des triangles de RENDU : sur
					// un quad la diagonale de triangulation n'existe pas (cube = 12 arêtes,
					// pas 18), exactement comme Blender.
					// ⚠ PLUS AUCUN décalage géométrique de la cage. Il valait rad * 0.0035 le long de
					// la radiale depuis le centre de la bbox, donc PROPORTIONNEL À LA TAILLE de
					// l'objet : sur la colonne #18 (bbox monde 0.3 x 2 x 0.3 -> rayon 2.05) il faisait
					// 0.0072 alors que la demi-épaisseur n'est que de 0.3 — soit 2,4 % de décollement
					// latéral : la cage ne suivait donc pas exactement la silhouette. Pire, marqueurs de
					// sommets et remplissage de face sont tracés SANS ce décalage -> les trois ne
					// coïncidaient pas. Le z-fighting est déjà traité côté pipeline par le depth-bias des
					// passes overlay (« DebugLine » / « DebugTriFill » : depthBiasConst = depthBiasSlope
					// = -1.5). Cage, marqueurs et fill sont maintenant STRICTEMENT coplanaires à la
					// surface -> tout COLLE au modèle, exactement comme dans Blender.
					auto liftW = [&](int32 i) { return liveW(i); };
					(void)normW;
					NkVector<float> L;
					L.Reserve((uint32)st->editEdges.Size() * 7);
					// Passe 0 = arêtes non sélectionnées, passe 1 = sélectionnées (tracées
					// APRÈS -> elles gagnent le z-fight et restent franches).
					for (int32 pass = 0; pass < 2; pass++) {
						for (uint32 e = 0; e + 1 < (uint32)st->editEdges.Size(); e += 2) {
							const uint32 a = st->editEdges[e], b = st->editEdges[e + 1];
							if (a >= (uint32)st->vertSel.Size() || b >= (uint32)st->vertSel.Size())
								continue;
							// ── COULEUR PAR EXTRÉMITÉ (interpolation, façon Blender) ──────
							// Le buffer de lignes porte une couleur PAR SOMMET (pos3 + rgba4) :
							// le GPU interpole donc le long de l'arête, sans surcoût ni
							// découpage en segments. Effet : une arête dont UNE SEULE extrémité
							// est sélectionnée apparaît en DÉGRADÉ orange -> noir (« arête
							// semi-sélectionnée ») ; avec ses DEUX extrémités sélectionnées elle
							// est entièrement orange (cas « arête sélectionnée » du flushing) ;
							// sans aucune, elle reste noire. Le sommet ACTIF tire vers le BLANC.
							const bool selA = (st->vertSel[a] != 0), selB = (st->vertSel[b] != 0);
							const bool any = (selA || selB);
							if (any != (pass == 1))
								continue; // passe 0 = arêtes neutres, passe 1 = arêtes touchées
							// ARÊTE ACTIVE : blanche sur TOUTE sa longueur, pas seulement à
							// l'extrémité. Un dégradé n'aurait pas dit « cette arête est la
							// référence » mais « cette extrémité est le sommet actif » — deux
							// informations différentes qu'il ne faut pas confondre.
							const bool actE =
								(st->editActiveEdgeA >= 0 &&
								 (((int32)a == st->editActiveEdgeA && (int32)b == st->editActiveEdgeB) ||
								  ((int32)a == st->editActiveEdgeB && (int32)b == st->editActiveEdgeA)));
							auto vcol = [&](uint32 v, bool s) {
								if (actE)
									return actVertCol; // arête ACTIVE -> blanche entièrement
								if (s && (int32)v == st->editActiveVert)
									return actVertCol; // extrémité ACTIVE -> blanc
								return s ? selEdgeCol : cageCol;
							};
							pushV(L, liftW((int32)a), vcol(a, selA));
							pushV(L, liftW((int32)b), vcol(b, selB));
						}
					}
					r3d->SetEditOverlayLines(L.Empty() ? nullptr : L.Data(), (uint32)(L.Size() / 7));
					// POINTS : marqueurs de vertices en SPRITE ÉCRAN-CONSTANT (taille en pixels
					// fixe quel que soit le zoom, façon Blender). Chaque point = un quad dont
					// chaque sommet porte {centre monde, coin en PIXELS, couleur} (9 floats) ;
					// le vertex shader billboarde en espace écran. Mode VERTEX actif seulement.
					(void)dotS;
					// Les marqueurs de VERTICES / centres de face NE passent PLUS par l'overlay
					// point-sprite (rendu « carré CREUX / crochet d'angle » peu fiable, surtout en
					// DX12). Ils sont dessinés en QUADS PLEINS face-caméra via DrawDebugTriangle (MÊME
					// chemin overlay no-depth que le gizmo solide -> correct OpenGL ET DX12), plus bas,
					// chaque frame, hors du batch persistant. On vide donc le batch de points.
					r3d->SetEditOverlayPoints(nullptr, 0);
					// Le REMPLISSAGE des faces sélectionnées ne passe plus par ce batch
					// persistant : il est tracé chaque frame en DrawDebugTriangle (même
					// chemin que les marqueurs, validé DX12 + GL) — cf. bloc ci-dessous.
					r3d->SetEditOverlayTris(nullptr, 0);
					r3d->SetEditOverlayXray(st->editXray);
				}
				// ── DIAG (NK_DIAG_BIGTRI=1) : détecte les triangles d'overlay qui EXPLOSENT
				// à l'écran (« carrés blancs »). Projette les 3 sommets et loggue tout
				// triangle dont la bbox écran dépasse le seuil (px) ou dont un sommet est
				// derrière le plan near. Purement diagnostique, coût nul si l'env est absent.
				static int32 gDiagBig = -2;
				if (gDiagBig == -2) {
					const char *dv = getenv("NK_DIAG_BIGTRI");
					gDiagBig = (dv && dv[0] && dv[0] != '0') ? atoi(dv) : 0;
				}
				auto diagTri = [&](const char *tag, NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
					if (!gDiagBig)
						return;
					float32 mnx = 1e30f, mny = 1e30f, mxx = -1e30f, mxy = -1e30f;
					int32 behind = 0;
					const NkVec3f P[3] = {a, b, c};
					for (int32 k = 0; k < 3; k++) {
						const NkVec3f v = P[k] - camPos;
						const float32 zc = v.Dot(fwd);
						if (zc <= 1e-3f) {
							behind++;
							continue;
						}
						const float32 sx = (0.5f + (v.Dot(rgt) / (zc * thX)) * 0.5f) * VW;
						const float32 sy = (0.5f - (v.Dot(upv) / (zc * thY)) * 0.5f) * VH;
						mnx = NkMin(mnx, sx);
						mny = NkMin(mny, sy);
						mxx = NkMax(mxx, sx);
						mxy = NkMax(mxy, sy);
					}
					const float32 w = (mxx > mnx) ? (mxx - mnx) : 0.f, h = (mxy > mny) ? (mxy - mny) : 0.f;
					if (behind > 0 || w > (float32)gDiagBig || h > (float32)gDiagBig)
						logger.Info("[DIAG] {0} behind={1} bbox={2}x{3} col=({4},{5},{6},{7}) a=({8},{9},{10})\n", tag,
									behind, w, h, col.x, col.y, col.z, col.w, a.x, a.y, a.z);
				};
				// ── P1 — REMPLISSAGE ORANGE TRANSLUCIDE DES FACES SÉLECTIONNÉES ────────
				// Façon Blender : TOUTE la surface de la face sélectionnée est teintée (pas
				// un simple point au centre). On triangule la face N-GON en éventail sur sa
				// VRAIE boucle (editHE), pas sur les triangles de rendu, et on trace via
				// DrawDebugTriangle. overlay=false -> pipeline « DebugTriFill » (LESS_EQUAL +
				// biais négatif, sans écriture de profondeur) : le fill colle à la surface et
				// reste occlus par la géométrie devant. X-ray -> overlay=true (voir à travers).
				// Une face est sélectionnée si TOUS ses sommets le sont (convention Blender) :
				// le fill apparaît donc aussi en mode VERTEX/EDGE, comme dans Blender.
				{
					auto liveWf = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					const NkVec4f faceFill{1.f, 0.55f, 0.06f, 0.36f};
					const uint32 fcntF = (uint32)st->editHE.faces.Size();
					NkVector<renderer::NkEmId> fvf;
					for (uint32 f = 0; f < fcntF; f++) {
						if (!st->editHE.faces[f].alive)
							continue;
						fvf.Clear();
						st->editHE.GetFaceVerts(f, fvf);
						const uint32 fn = (uint32)fvf.Size();
						if (fn < 3)
							continue; // arête fil : pas de surface
						bool allSel = true;
						for (uint32 k = 0; k < fn && allSel; k++) {
							const uint32 vi = fvf[k];
							if (vi >= (uint32)st->vertSel.Size() || !st->vertSel[vi] ||
								vi >= (uint32)st->editLive.Size())
								allSel = false;
						}
						if (!allSel)
							continue;
						const NkVec3f p0 = liveWf((int32)fvf[0]);
						for (uint32 k = 1; k + 1 < fn; k++) { // éventail sur la boucle n-gon
							diagTri("facefill", p0, liveWf((int32)fvf[k]), liveWf((int32)fvf[k + 1]), faceFill);
							r3d->DrawDebugTriangle(p0, liveWf((int32)fvf[k]), liveWf((int32)fvf[k + 1]), faceFill,
												   0.f, st->editXray);
						}
					}
				}
				// ── Marqueurs VERTEX / centre-de-FACE façon Blender : petits QUADS PLEINS ──────
				// Carré PLEIN (2 triangles) face-caméra, taille ÉCRAN-CONSTANTE (~3 px de côté),
				// via DrawDebugTriangle en overlay no-depth (même chemin que le gizmo solide ->
				// rendu identique OpenGL / DX12). Non sél. = sombre, sél. = orange, actif = blanc ;
				// PLEIN dans tous les cas (fin liseré sombre dessous pour la lisibilité). Rendu
				// chaque frame (suit la caméra pour rester écran-constant).
				{
					auto liveWv = [&](int32 i) { return st->editAnchor * st->editLive[i].pos; };
					// Les marqueurs sont tracés SANS depth-test (fiabilité DX12) : sans filtre,
					// on verrait aussi ceux du DOS du modèle « à travers ». Blender ne les montre
					// qu'en X-ray. Filtre d'orientation : un marqueur dont la normale tourne le
					// dos à la caméra est caché (X-ray OFF), gardé (X-ray ON).
					const NkVec3f orgW = st->editAnchor * NkVec3f{0.f, 0.f, 0.f};
					auto facingCam = [&](NkVec3f w, NkVec3f nLocal) {
						if (st->editXray)
							return true;
						const NkVec3f nW = (st->editAnchor * nLocal) - orgW;
						return nW.Dot(camPos - w) > 0.f;
					};
					// px (demi-côté écran) -> demi-taille MONDE à la profondeur du point.
					const float32 pxToWorld = (2.f * thY) / VH;
					auto fillQuad = [&](NkVec3f w, float32 halfPx, NkVec4f col) {
						float32 d = (w - camPos).Dot(fwd);
						if (d < 1e-3f)
							d = 1e-3f;
						const float32 h = halfPx * pxToWorld * d;
						const NkVec3f rx = rgt * h, uy = upv * h;
						const NkVec3f c00 = w - rx - uy, c10 = w + rx - uy, c11 = w + rx + uy, c01 = w - rx + uy;
						// ⚠ OVERLAY vs DEPTH : les marqueurs suivent le X-RAY, exactement comme
						// le remplissage de face. X-ray OFF -> depth-test : le marqueur est
						// OCCLUS par ce qui est devant (autre objet de la scène, ou le modèle
						// lui-même). X-ray ON -> no-depth : on voit à travers (façon Blender).
						// Le GIZMO, lui, reste no-depth dans TOUS les cas (dessiné plus bas).
						diagTri("marker", c00, c10, c11, col);
						r3d->DrawDebugTriangle(c00, c10, c11, col, 0.f, st->editXray);
						r3d->DrawDebugTriangle(c00, c11, c01, col, 0.f, st->editXray);
					};
					// Carré PLEIN + fin liseré sombre dessous (les 2 sont PLEINS -> jamais creux).
					const NkVec4f rim{0.f, 0.f, 0.f, 0.9f};
					auto dot = [&](NkVec3f w, float32 core, NkVec4f col) {
						fillQuad(w, core + 0.7f, rim); // liseré sombre (dessous)
						fillQuad(w, core, col);		   // coeur PLEIN (dessus)
					};
					// VERTICES (mode VERTEX) : ~3 px de côté (half ~1.5), discret.
					// Code couleur Blender : NOIR = non sélectionné, ORANGE = sélectionné,
					// BLANC = sommet ACTIF (dernier sélectionné).
					if (st->editSelMask & 1) {
						for (int32 i = 0; i < nv; i++) {
							NkVec3f w = liveWv(i);
							if (!facingCam(w, st->editLive[i].normal))
								continue; // sommet du dos -> caché (sauf X-ray), façon Blender
							if (i == st->editActiveVert)
								dot(w, 2.0f, NkVec4f{1.f, 1.f, 1.f, 1.f}); // actif = BLANC
							else if (i < (int32)st->vertSel.Size() && st->vertSel[i])
								dot(w, 1.8f, NkVec4f{1.f, 0.55f, 0.05f, 1.f}); // sél. = ORANGE
							else
								dot(w, 1.5f, NkVec4f{0.02f, 0.02f, 0.03f, 1.f}); // non sél. = NOIR
						}
					}
					// CENTRES DE FACE (mode FACE) : petit carré plein au barycentre de chaque face.
					if (st->editSelMask & 4) {
						const uint32 fcnt = (uint32)st->editHE.faces.Size();
						NkVector<renderer::NkEmId> fvd;
						for (uint32 f = 0; f < fcnt; f++) {
							if (!st->editHE.faces[f].alive)
								continue;
							fvd.Clear();
							st->editHE.GetFaceVerts(f, fvd);
							const uint32 fn = (uint32)fvd.Size();
							if (fn < 3)
								continue;
							NkVec3f cW{0.f, 0.f, 0.f};
							bool allSel = true;
							for (uint32 k = 0; k < fn; k++) {
								const uint32 vi = fvd[k];
								if (vi < (uint32)st->editLive.Size())
									cW = cW + liveWv((int32)vi);
								if (vi >= (uint32)st->vertSel.Size() || !st->vertSel[vi])
									allSel = false;
							}
							cW = cW * (1.f / (float32)fn);
							if (!facingCam(cW, st->editHE.faces[f].normal))
								continue; // face du dos -> point caché (sauf X-ray)
							if ((int32)f == st->editActiveFace) {
								// FACE ACTIVE : centre BLANC et contour blanc. Le seul point
								// central ne suffisait pas — sur un n-gon large, un point de
								// 4 px au barycentre ne dit pas QUELLE face est active quand
								// plusieurs se touchent. Le contour lève l'ambiguïté.
								dot(cW, 2.0f, NkVec4f{1.f, 1.f, 1.f, 1.f});
								for (uint32 k = 0; k < fn; k++) {
									const uint32 v0 = fvd[k], v1 = fvd[(k + 1) % fn];
									if (v0 >= (uint32)st->editLive.Size() || v1 >= (uint32)st->editLive.Size())
										continue;
									r3d->DrawDebugLine(liveWv((int32)v0), liveWv((int32)v1),
													   NkVec4f{1.f, 1.f, 1.f, 1.f}, 0.f, st->editXray);
								}
							} else if (allSel)
								dot(cW, 1.8f, NkVec4f{1.f, 0.55f, 0.05f, 1.f}); // face sél. = ORANGE
							else
								dot(cW, 1.4f, NkVec4f{0.02f, 0.02f, 0.03f, 1.f}); // face = point NOIR
						}
					}
				}
				// Poignées du gizmo (OVERLAY) — rendu chaque frame (peu de lignes, négligeable).
				// Rendu PLEIN (façon Blender solide) IDENTIQUE au gizmo objet : 2 callbacks
				// (drawLine pour tiges/liserés fins + drawTri pour formes PLEINES : cônes/cubes/
				// rubans). Le 2e callback active la surcharge Draw(drawLine, drawTri) du gizmo —
				// mêmes couleurs d'axe (X rouge, Y vert, Z bleu) et mêmes formes que l'objet.
				if (!nkvpGizmoHidden)
					st->editGizmo.Draw(
					[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
					[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
						diagTri("gizmo", a, b, c, col);
						r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
					});
			}

			// ── Gizmo éditeur (composant réutilisable NkGizmo3D) ────────────────────
			// Table des CIBLES (transform de BASE + demi-extent mesh + rayon de pick),
			// MÊME ordre/indices que les draw calls. Le gizmo compose le décalage
			// utilisateur lui-même (Apply), gère pick + drag + dessin, et rend en OVERLAY.
			{
				renderer::NkGizmoTarget targets[Demo3DState::kNumObj];
				int32 n = 0;
				const NkVec3f H = {0.5f, 0.5f, 0.5f};
				auto *msG = ctx.renderer->GetMeshSystem();
				// Pour un objet ÉDITÉ, le marqueur OBB épouse l'AABB LOCALE réelle du mesh
				// modifié (centre + demi-extent), au lieu du demi-extent fixe de la primitive.
				// Demi-extent réel d'un mesh (données CPU) : indispensable pour que le
				// marqueur ÉPOUSE la forme. Un plan est PLAT — lui donner le demi-extent
				// cubique {0.5,0.5,0.5} par défaut dessinait une boîte en volume autour
				// d'un sol d'épaisseur nulle, au lieu d'un contour posé dessus.
				auto meshHalf = [&](NkMeshHandle mh, NkVec3f &half, NkVec3f &center) -> bool {
					if (!mh.IsValid() || !msG || !msG->HasCPUData(mh))
						return false;
					const auto *vv = (const renderer::NkVertex3D *)msG->GetVertices(mh);
					const uint32 vc = msG->GetVertexCount(mh);
					if (!vv || vc == 0)
						return false;
					NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
					for (uint32 i = 0; i < vc; i++) {
						const NkVec3f p = vv[i].pos;
						mn.x = NkMin(mn.x, p.x);
						mn.y = NkMin(mn.y, p.y);
						mn.z = NkMin(mn.z, p.z);
						mx.x = NkMax(mx.x, p.x);
						mx.y = NkMax(mx.y, p.y);
						mx.z = NkMax(mx.z, p.z);
					}
					center = (mn + mx) * 0.5f;
					half = (mx - mn) * 0.5f;
					return true;
				};
				// Variante qui connaît la PRIMITIVE de l'objet : le mesh édité prime, sinon
				// on mesure la primitive elle-même (sphère, cube, plan) au lieu de supposer
				// un cube. `prim` invalide = ancien comportement (demi-extent H).
				auto fitTargetMesh = [&](int32 idx, const NkMat4f &base, float32 pickR,
										 NkMeshHandle prim) -> renderer::NkGizmoTarget {
					NkVec3f h, c;
					if (idx >= 0 && idx < Demo3DState::kNumObj && st->objMesh[idx].IsValid() &&
						meshHalf(st->objMesh[idx], h, c))
						return {base * NkMat4f::Translate(c), h, pickR};
					if (meshHalf(prim, h, c))
						return {base * NkMat4f::Translate(c), h, pickR};
					return {base, H, pickR};
				};
				auto fitTarget = [&](int32 idx, const NkMat4f &base, float32 pickR) -> renderer::NkGizmoTarget {
					if (idx >= 0 && idx < Demo3DState::kNumObj && st->objMesh[idx].IsValid() && msG &&
						msG->HasCPUData(st->objMesh[idx])) {
						const auto *vv = (const renderer::NkVertex3D *)msG->GetVertices(st->objMesh[idx]);
						const uint32 vc = msG->GetVertexCount(st->objMesh[idx]);
						NkVec3f mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
						for (uint32 i = 0; i < vc; i++) {
							NkVec3f p = vv[i].pos;
							mn.x = NkMin(mn.x, p.x);
							mn.y = NkMin(mn.y, p.y);
							mn.z = NkMin(mn.z, p.z);
							mx.x = NkMax(mx.x, p.x);
							mx.y = NkMax(mx.y, p.y);
							mx.z = NkMax(mx.z, p.z);
						}
						if (vc > 0) {
							NkVec3f c = (mn + mx) * 0.5f, h = (mx - mn) * 0.5f;
							return {base * NkMat4f::Translate(c), h, pickR};
						}
					}
					return {base, H, pickR};
				};
				for (int row = 0; row < 4; row++)
					for (int col = 0; col < 4; col++) // 16 sphères
						targets[n++] = fitTarget(row * 4 + col,
												 NkMat4f::Translate({(col - 1.5f) * 1.2f, 0.5f, (row - 1.5f) * 1.2f}) *
													 NkMat4f::Scale({0.45f, 0.45f, 0.45f}),
												 0.35f);
				targets[n++] = fitTarget(16, cubeXform, 0.45f); // cube central
				targets[n++] = fitTarget(17, NkMat4f::Translate({-4.f, 1.f, -2.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}),
										 1.3f); // colonne 0
				targets[n++] = fitTarget(18, NkMat4f::Translate({1.f, 1.f, 4.f}) * NkMat4f::Scale({0.3f, 2.f, 0.3f}),
										 1.3f); // colonne 1
				for (int gz = 0; gz < 8; gz++)
					for (int gx = 0; gx < 8; gx++) // 64 cubes INSTANCIÉS
						targets[n++] =
							fitTarget(19 + gz * 8 + gx,
									  NkMat4f::Translate({(gx - 3.5f) * 0.55f, 1.6f, (gz - 3.5f) * 0.55f - 4.5f}) *
										  NkMat4f::Scale({0.18f, 0.18f, 0.18f}),
									  0.2f);

				// ── Décor sélectionnable (sol, feuillage, mur du GI) ─────────────
				// Le pick du gizmo teste désormais la BOÎTE de l'objet (cf. NkGizmo.h) :
				// un sol de 80×80 ne capte donc que les clics qui tombent réellement
				// dessus, au lieu de voler ceux des objets posés sur lui — ce qui était
				// impossible avec une sphère de pick.
				// Chaque cible reçoit SA primitive : le sol est un PLAN, son marqueur est
				// donc plat et posé dessus, pas une boîte qui l'enveloppe.
				targets[n++] =
					fitTargetMesh(Demo3DState::kIdxFloor, Demo3D_ObjBaseFull(st, Demo3DState::kIdxFloor), 0.5f, st->meshPlane);
				targets[n++] = fitTargetMesh(Demo3DState::kIdxFoliage, Demo3D_ObjBaseFull(st, Demo3DState::kIdxFoliage), 1.2f,
											 st->meshCube);
				targets[n++] = fitTargetMesh(Demo3DState::kIdxGIWall,
											 Demo3D_ObjBaseFull(st, Demo3DState::kIdxGIWall), 1.4f, st->meshCube);

				// Gizmo OBJET actif UNIQUEMENT hors Edit Mode (sinon c'est le gizmo vertices).
				if (!st->editMode) {
					// NK_GIZMO_SHOW=<mode> : sélectionne le cube central + fixe le mode
					// (0=Translate 1=Rotate 2=Scale 3=Combine) pour une CAPTURE headless du
					// gizmo (sans souris). One-shot. Sans cette variable : comportement normal.
					static bool gizShowInit = false;
					if (!gizShowInit) {
						gizShowInit = true;
						if (const char *gv = getenv("NK_GIZMO_SHOW")) {
							// NK_GIZMO_OBJ=<idx> : objet à sélectionner (défaut 14 = sphère mate
							// avant, hors halo de la lumière centrale -> gizmo bien lisible).
							int32 gobj = 14;
							if (const char *go = getenv("NK_GIZMO_OBJ"))
								gobj = atoi(go);
							st->gizmo.Select(gobj);
							st->gizmo.SetMode(atoi(gv) & 3);
						}
						// NK_GIZMO_MULTI="a,b,c" : sélectionne PLUSIEURS objets pour une
						// capture headless. Sans ce levier, impossible de tester en capture
						// que le liseré marque bien TOUS les sélectionnés et pas seulement
						// l'actif — c'était précisément le bug. Le premier index devient
						// l'objet ACTIF (Select vide la sélection), les suivants s'ajoutent.
						if (const char *gm = getenv("NK_GIZMO_MULTI")) {
							const char *p = gm;
							bool first = true;
							while (*p) {
								const int32 id = atoi(p);
								if (first) {
									st->gizmo.Select(id);
									first = false;
								} else
									st->gizmo.AddToSelection(id);
								while (*p && *p != ',')
									p++;
								if (*p == ',')
									p++;
							}
						}
						// NK_SEL_TEST_XFORM=1 : non-régression du FIX 1 (contour qui suit
						// l'objet). Sélectionne un objet (NK_GIZMO_OBJ, défaut 14) et lui
						// applique une transform NON-IDENTITÉ FIGÉE (translation + rotation Y
						// connues) par le MÊME chemin interne que le drag gizmo
						// (SetSelectedTransform -> mTr/mRot). En capture, le liseré orange DOIT
						// épouser l'objet à sa position TRANSFORMÉE (pas à l'origine).
						if (getenv("NK_SEL_TEST_XFORM")) {
							int32 tobj = 14;
							if (const char *go = getenv("NK_GIZMO_OBJ"))
								tobj = atoi(go);
							st->gizmo.Select(tobj);
							st->gizmo.SetSelectedTransform({1.4f, 0.6f, 0.f},
														   NkMat4f::RotationY(NkAngle::FromRad(0.9f)),
														   {0.f, 0.f, 0.f});
						}
						// NK_GIZMO_OBB=0 : masque la cage OBB de sélection (gizmo SEUL, épuré).
						if (const char *ob = getenv("NK_GIZMO_OBB"))
							st->gizmo.SetDrawObjectBounds(!(ob[0] == '0'));
						// NK_SELECT_OUTLINE=1 : liseré silhouette façon Blender (OPTION
						// distincte de l'AABB) — fin contour orange qui épouse le maillage
						// sélectionné, via post-process de détection de bord. Couleur/épaisseur
						// surchargeables (NK_OUTLINE_THICK). L'AABB reste dispo en parallèle.
						// NB : le liseré silhouette est désormais ACTIF PAR DÉFAUT (indicateur
						// de sélection par défaut, à la place de l'AABB). NK_SELECT_OUTLINE=0
						// le désactive ; NK_OUTLINE_THICK règle l'épaisseur.
						if (const char *so = getenv("NK_SELECT_OUTLINE")) {
							float32 thick = 3.f;
							if (const char *ot = getenv("NK_OUTLINE_THICK"))
								thick = (float32)atof(ot);
							r3d->SetSelectionOutline(!(so[0] == '0'), {1.f, 0.45f, 0.05f, 1.f}, thick);
						} else if (const char *ot = getenv("NK_OUTLINE_THICK")) {
							r3d->SetSelectionOutline(true, {1.f, 0.45f, 0.05f, 1.f}, (float32)atof(ot));
						}
						// NK_SHADING=<0..5> : force le mode d'affichage (0=RENDERED, 1=SOLID
						// unlit, 2=WIREFRAME, 3=NORMAL, 4=UV, 5=AO) pour une capture headless.
						if (const char *sh = getenv("NK_SHADING")) {
							st->shadingMode = atoi(sh) % 6;
							const int32 vm[6] = {0, 1, 1, 2, 3, 4};
							r3d->SetWireframe(st->shadingMode == 2);
							r3d->SetViewMode(vm[st->shadingMode]);
						}
						// NK_MATCAP=<0..29> : choisit la matcap dans l'atlas des 30, sans
						// avoir a marteler M. Indispensable pour capturer une matcap donnee
						// de facon reproductible (comparaison avant/apres).
						if (const char *mcv = getenv("NK_MATCAP"))
							r3d->SetMatcap(atoi(mcv));
					}
					st->gizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width,
										(float32)ctx.height);
					renderer::NkGizmoInput gin;
					gin.mouseX = ((float32)NkInput.MouseX() - nkvpOffX);
					gin.mouseY = ((float32)NkInput.MouseY() - nkvpOffY);
					gin.mouseDX = frameMDX;
					gin.mouseDY = frameMDY;
					gin.leftPressed = st->pickPending;
					st->pickPending = false;
					gin.leftDown = NkInput.IsMouseDown(NkMouseButton::NK_MB_LEFT);
					gin.shiftDown = NkInput.IsKeyDown(NkKey::NK_LSHIFT) || NkInput.IsKeyDown(NkKey::NK_RSHIFT);
					gin.ctrlDown = NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL); // SNAP
					gin.lockAxis = -1;
					if (st->gizmo.IsDragging()) {
						if (NkInput.IsKeyDown(NkKey::NK_X))
							gin.lockAxis = 0;
						else if (NkInput.IsKeyDown(NkKey::NK_Y))
							gin.lockAxis = 1;
						else if (NkInput.IsKeyDown(NkKey::NK_Z))
							gin.lockAxis = 2;
					}
					// NK_SEL_AT="x,y" : force UNE FOIS un clic de sélection OBJET à ces
					// pixels et journalise l'index attrapé. Le levier NK_PICK_AT existant
					// ne pilote que le pick de SOMMETS en Edit Mode ; sans équivalent ici,
					// la sélection d'objets ne pouvait être vérifiée qu'à la souris — donc
					// pas de façon reproductible.
					static bool selAtDone = false;
					if (!selAtDone) {
						if (const char *sa = getenv("NK_SEL_AT")) {
							selAtDone = true;
							float32 sv[2] = {0.f, 0.f};
							int32 sk = 0;
							const char *sp = sa;
							while (sk < 2 && *sp) {
								sv[sk++] = (float32)atof(sp);
								while (*sp && *sp != ',')
									sp++;
								if (*sp == ',')
									sp++;
							}
							gin.mouseX = sv[0];
							gin.mouseY = sv[1];
							gin.leftPressed = true;
						}
					}
					// ── PICK ET MANIPULATION DES LUMIERES ────────────────────────────
					// Passe AVANT le gizmo des objets, parce qu'un clic doit etre attribue
					// a un seul destinataire : si les deux le traitaient, deplacer une
					// lumiere selectionnerait aussi l'objet qui se trouve derriere.
					//
					// ARBITRATION — la meme que pour les objets dans son PRINCIPE (le plus
					// proche du curseur gagne), mais sur la DISTANCE ECRAN et non sur un
					// rayon 3D. Un widget de lumiere n'a pas de volume : sa taille a l'ecran
					// est constante par construction, donc un test geometrique rendrait une
					// lumiere lointaine impossible a cliquer alors qu'elle reste aussi
					// grosse a l'ecran qu'une proche. On compare donc des PIXELS.
					bool lightClaimedClick = false;
					{
						using LG = renderer::NkLightGizmo;
						const Demo3D_ScreenProj lproj = Demo3D_ScreenProj::Make(
							cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width, (float32)ctx.height);
						st->lightGizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width,
												 (float32)ctx.height);
						// Cibles du gizmo : extent et rayon de pick NULS, volontairement. Le
						// pick 3D interne de NkGizmo3D ne doit JAMAIS attraper une lumiere —
						// c'est le test ecran ci-dessous qui tranche. On garde malgre tout
						// Update() pour ses poignees, son pivot et son drag.
						renderer::NkGizmoTarget ltg[Demo3DState::kNumLights];
						for (int32 li = 0; li < Demo3DState::kNumLights; li++) {
							ltg[li].base = NkMat4f::Translate(st->lights[li].position);
							ltg[li].localHalf = {0.f, 0.f, 0.f};
							ltg[li].pickRadius = 0.f;
						}
						renderer::NkGizmoInput lin = gin;
						// NK_LIGHT_PICK_AT="x,y" : force UNE FOIS un clic a ces pixels et
						// journalise la lumiere attrapee. Sans ce levier, le pick des lumieres
						// ne serait verifiable qu'a la souris, donc pas en capture.
						static bool lpAtDone = false;
						if (!lpAtDone) {
							if (const char *lp = getenv("NK_LIGHT_PICK_AT")) {
								lpAtDone = true;
								float32 lv[2] = {0.f, 0.f};
								int32 lk = 0;
								const char *pl = lp;
								while (lk < 2 && *pl) {
									lv[lk++] = (float32)atof(pl);
									while (*pl && *pl != ',')
										pl++;
									if (*pl == ',')
										pl++;
								}
								gin.mouseX = lv[0];
								gin.mouseY = lv[1];
								gin.leftPressed = true;
								lin.mouseX = lv[0];
								lin.mouseY = lv[1];
								lin.leftPressed = true;
							}
						}
						if (gin.leftPressed && !st->lightGizmo.IsDragging() && st->showLightGizmos) {
							int32 best = -1;
							float32 bestD2 = 1e30f;
							for (int32 li = 0; li < Demo3DState::kNumLights; li++) {
								// On projette l'ANCRE du widget — le meme point que celui du
								// dessin, et sur la lumiere EFFECTIVE : viser ou l'on voit.
								const renderer::NkLightDesc eff = Demo3D_LightEffective(st, li);
								float32 lx = 0.f, ly = 0.f;
								if (!lproj(LG::Anchor(eff), lx, ly))
									continue; // derriere la camera
								if (!LG::HitScreen(lx, ly, gin.mouseX, gin.mouseY))
									continue;
								const float32 dx = gin.mouseX - lx, dy = gin.mouseY - ly;
								const float32 d2 = dx * dx + dy * dy;
								if (d2 < bestD2) {
									bestD2 = d2;
									best = li;
								}
							}
							if (best >= 0) {
								// Selection faite ICI : on prive le gizmo de l'evenement pour
								// qu'il ne la defasse pas avec son propre pick 3D (qui, cibles
								// nulles, ne trouverait rien et viderait la selection).
								st->lightGizmo.Select(best);
								st->lightSel = best;
								lin.leftPressed = false;
								lightClaimedClick = true;
								// MODE PAR DEFAUT : TRANSLATE pour TOUS les types, comme pour un objet.
								// Le type ne restreint pas les gestes disponibles ; il determine
								// seulement lesquels ont un effet VISIBLE, ce que le journal dit une
								// fois, a la selection.
								const renderer::NkLightDesc &BL = st->lights[best];
								st->lightGizmo.SetMode(renderer::NkGizmo3D::MODE_TRANSLATE);
								static const char *kLName[4] = {"directionnelle", "ponctuelle", "spot", "surfacique"};
								const int32 ti = (int32)BL.type & 3;
								// Ce que la manipulation CHANGE pour ce type : annonce a la selection,
								// plutot qu'en refusant une touche. L'utilisateur sait a quoi s'attendre
								// et garde la main.
								const char *note = "";
								if (!LG::CanTranslate(BL.type))
									note = " (la deplacer ne change pas l'eclairage : seule sa direction compte)";
								else if (!LG::CanRotate(BL.type))
									note = " (la tourner ne change pas l'eclairage : rayonnement isotrope)";
								logger.Info("[Demo3D][LUMIERE] selection -> {0} ({1}) a {2} px du curseur{3}\n", best,
											kLName[ti], sqrtf(bestD2), note);
							}
						}
						// NK_LIGHT_MOVE="dx,dy,dz" : deplace UNE FOIS la lumiere selectionnee,
						// sans souris. Ce levier ne sert pas a piloter la demo : il sert a
						// PROUVER que la manipulation change bien l'ECLAIRAGE et pas seulement
						// le widget. Sans lui, une capture ne montrerait qu'un marqueur qui
						// bouge — ce qui est precisement l'illusion contre laquelle
						// Demo3D_LightEffective a ete ecrite.
						static bool lmvDone = false;
						if (!lmvDone && st->lightSel >= 0) {
							if (const char *lm = getenv("NK_LIGHT_MOVE")) {
								lmvDone = true;
								float32 mv[3] = {0.f, 0.f, 0.f};
								int32 mk = 0;
								const char *pm = lm;
								while (mk < 3 && *pm) {
									mv[mk++] = (float32)atof(pm);
									while (*pm && *pm != ',')
										pm++;
									if (*pm == ',')
										pm++;
								}
								st->lightGizmo.SetSelectedTransform({mv[0], mv[1], mv[2]}, NkMat4f::Identity(),
																	{0.f, 0.f, 0.f});
								const renderer::NkLightDesc ef = Demo3D_LightEffective(st, st->lightSel);
								logger.Info("[Demo3D][LUMIERE] NK_LIGHT_MOVE ({0}, {1}, {2}) -> lumiere {3} "
											"effective en ({4}, {5}, {6})\n",
											mv[0], mv[1], mv[2], st->lightSel, ef.position.x, ef.position.y,
											ef.position.z);
							}
						}
						const bool lwasDrag = st->lightGizmo.IsDragging();
						st->lightGizmo.Update(ltg, Demo3DState::kNumLights, lin);
						if (!lwasDrag && st->lightGizmo.IsDragging())
							lightClaimedClick = true; // poignee saisie : le clic est a nous
						st->lightSel = st->lightGizmo.ActiveIndex();
						// LE CLIC EST CONSOMME : sans cela, deplacer une lumiere selectionnerait
						// en meme temps l'objet situe derriere elle.
						if (lightClaimedClick)
							gin.leftPressed = false;
						if (st->lightDragPrev && !st->lightGizmo.IsDragging()) {
							// Fin de drag : on RENTRE le resultat dans la base et on remet le
							// gizmo a zero. Sinon la base et l'accumulation divergeraient et le
							// pick (qui projette l'effective) finirait par ne plus tomber la ou
							// l'on voit le widget apres plusieurs manipulations.
							for (int32 li = 0; li < Demo3DState::kNumLights; li++)
								st->lights[li] = Demo3D_LightEffective(st, li);
							st->lightGizmo.ResetSelected();
							// Une lumiere manipulee ne peut plus etre animee : son animation
							// reecrirait la position a la frame suivante et effacerait le geste.
							if (st->lightSel == 3)
								st->spotAnimated = false;
							logger.Info("[Demo3D][LUMIERE] {0} : position ({1}, {2}, {3}), portee {4}\n",
										st->lightSel < 0 ? 0 : st->lightSel,
										st->lights[st->lightSel < 0 ? 0 : st->lightSel].position.x,
										st->lights[st->lightSel < 0 ? 0 : st->lightSel].position.y,
										st->lights[st->lightSel < 0 ? 0 : st->lightSel].position.z,
										st->lights[st->lightSel < 0 ? 0 : st->lightSel].range);
						}
						st->lightDragPrev = st->lightGizmo.IsDragging();
					}
					// ── GIZMO DES EMPTIES (parente) ──────────────────────────────
					// Un empty n'a pas de volume : sa selection VIENT DE LA
					// HIERARCHIE (aucun pick, rayon nul), mais ses poignees se
					// manipulent ici comme celles d'une lumiere. Pendant le drag,
					// la transformation EFFECTIVE (base + decalages) est vue par le
					// detecteur de parente -> les enfants suivent EN DIRECT ; en
					// fin de drag le resultat est replie dans la base.
					{
						st->emptyGizmo.SetCamera(cam.GetPosition(), cam.GetTarget(), 60.f,
												 (float32)ctx.width, (float32)ctx.height);
						renderer::NkGizmoTarget etg[70];
						const float32 kD2R = 0.017453292f;
						for (int32 e = 0; e < 70; ++e) {
							const NkMat4f eR =
								NkMat4f::RotationZ(NkAngle::FromRad(nkvpEmptyRotDeg[e][2] * kD2R)) *
								NkMat4f::RotationY(NkAngle::FromRad(nkvpEmptyRotDeg[e][1] * kD2R)) *
								NkMat4f::RotationX(NkAngle::FromRad(nkvpEmptyRotDeg[e][0] * kD2R));
							etg[e].base = NkMat4f::Translate({nkvpEmptyPos[e][0], nkvpEmptyPos[e][1],
															  nkvpEmptyPos[e][2]}) *
										  eR *
										  NkMat4f::Scale({nkvpEmptyScl[e][0], nkvpEmptyScl[e][1],
														  nkvpEmptyScl[e][2]});
							etg[e].localHalf = {0.f, 0.f, 0.f};
							etg[e].pickRadius = 0.f; // pas de pick : la hierarchie selectionne
						}
						renderer::NkGizmoInput ein = gin;
						// PICK ECRAN des MAILLAGES UTILISATEUR : un clic dans la vue
						// les selectionne comme n'importe quel objet (Rihen). Meme
						// principe que les lumieres : centre projete, rayon approche.
						if (gin.leftPressed && !st->emptyGizmo.IsDragging()) {
							const Demo3D_ScreenProj uproj = Demo3D_ScreenProj::Make(
								cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width,
								(float32)ctx.height);
							const NkVec3f camP2 = cam.GetPosition();
							const NkVec3f fwd2 = (cam.GetTarget() - camP2).Normalized();
							const NkVec3f rgt2 = fwd2.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
							int32 bestU = -1;
							float32 bestD2 = 1e30f;
							for (int32 u2 = 0; u2 < kNkvpMaxUser; ++u2) {
								const uint8 uk2 = nkvpUserKind[u2];
								if (uk2 == 0 || uk2 == 4)
									continue;
								const int32 un2 = kNkvpFirstUser + u2;
								if (HostHiddenEff(un2) || HostLockedEff(un2))
									continue;
								const int32 e2 = un2 - 90;
								const NkVec3f c2{nkvpEmptyPos[e2][0], nkvpEmptyPos[e2][1],
												 nkvpEmptyPos[e2][2]};
								float32 rw = fabsf(nkvpEmptyScl[e2][0]);
								if (fabsf(nkvpEmptyScl[e2][1]) > rw)
									rw = fabsf(nkvpEmptyScl[e2][1]);
								if (fabsf(nkvpEmptyScl[e2][2]) > rw)
									rw = fabsf(nkvpEmptyScl[e2][2]);
								rw = rw * 0.7f + 0.15f;
								if (nkvpBaseSet[un2] && nkvpDimFactor[un2][0] > 0.f)
									rw *= nkvpDimFactor[un2][0];
								float32 sx0 = 0.f, sy0 = 0.f, sx1 = 0.f, sy1 = 0.f;
								if (!uproj(c2, sx0, sy0))
									continue;
								const NkVec3f edge{c2.x + rgt2.x * rw, c2.y + rgt2.y * rw,
												   c2.z + rgt2.z * rw};
								if (!uproj(edge, sx1, sy1))
									continue;
								const float32 rpix = sqrtf((sx1 - sx0) * (sx1 - sx0) +
														   (sy1 - sy0) * (sy1 - sy0));
								const float32 pdx = gin.mouseX - sx0, pdy = gin.mouseY - sy0;
								const float32 pd2 = pdx * pdx + pdy * pdy;
								if (pd2 <= rpix * rpix && pd2 < bestD2) {
									bestD2 = pd2;
									bestU = e2;
								}
							}
							if (bestU >= 0) {
								st->gizmo.ClearSelection();
								st->lightGizmo.ClearSelection();
								st->lightSel = -1;
								if (gin.shiftDown || gin.ctrlDown)
									st->emptyGizmo.ToggleSelection(bestU); // multi successif
								else
									st->emptyGizmo.Select(bestU);
								ein.leftPressed = false;
								gin.leftPressed = false; // le clic est a nous
							}
						}
						const bool ewasDrag = st->emptyGizmo.IsDragging();
						st->emptyGizmo.Update(etg, 70, ein);
						if (!ewasDrag && st->emptyGizmo.IsDragging())
							gin.leftPressed = false; // poignee saisie : le clic est a nous
						if (st->emptyDragPrev && !st->emptyGizmo.IsDragging()) {
							const int32 es = st->emptyGizmo.ActiveIndex();
							if (es >= 0 && es < 70) {
								// REPLI : effective -> base, gizmo remis a zero (meme
								// regle que les lumieres, sinon base et decalages
								// divergent).
								const NkVec3f tr = st->emptyGizmo.TranslateOf(es);
								const NkMat4f oR = st->emptyGizmo.RotationOf(es);
								const NkVec3f os = st->emptyGizmo.ScaleOf(es);
								const NkMat4f bR =
									NkMat4f::RotationZ(NkAngle::FromRad(nkvpEmptyRotDeg[es][2] * kD2R)) *
									NkMat4f::RotationY(NkAngle::FromRad(nkvpEmptyRotDeg[es][1] * kD2R)) *
									NkMat4f::RotationX(NkAngle::FromRad(nkvpEmptyRotDeg[es][0] * kD2R));
								NkVec3f fp, fr, fs;
								HostDecompose(oR * bR, fp, fr, fs);
								nkvpEmptyPos[es][0] += tr.x;
								nkvpEmptyPos[es][1] += tr.y;
								nkvpEmptyPos[es][2] += tr.z;
								nkvpEmptyRotDeg[es][0] = fr.x;
								nkvpEmptyRotDeg[es][1] = fr.y;
								nkvpEmptyRotDeg[es][2] = fr.z;
								nkvpEmptyScl[es][0] *= (1.f + os.x);
								nkvpEmptyScl[es][1] *= (1.f + os.y);
								nkvpEmptyScl[es][2] *= (1.f + os.z);
								st->emptyGizmo.ResetSelected();
							}
						}
						st->emptyDragPrev = st->emptyGizmo.IsDragging();
					}
					st->gizmo.Update(targets, n, gin);
					if (getenv("NK_SEL_AT")) {
						static int32 lastLogged = -2;
						const int32 nowSel = st->gizmo.ActiveIndex();
						if (nowSel != lastLogged) {
							lastLogged = nowSel;
							const char *what = "?";
							if (nowSel < 0)
								what = "rien";
							else if (nowSel < 16)
								what = "sphere";
							else if (nowSel == 16)
								what = "cube central";
							else if (nowSel <= 18)
								what = "colonne";
							else if (nowSel < 83)
								what = "cube instancie";
							else if (nowSel == Demo3DState::kIdxFloor)
								what = "SOL";
							else if (nowSel == Demo3DState::kIdxFoliage)
								what = "PANNEAU FEUILLAGE";
							else if (nowSel == Demo3DState::kIdxGIWall)
								what = "MUR GI";
							logger.Info("[Demo3D] selection -> index {0} ({1})\n", nowSel, what);
						}
					}

					// ── OUTILS DE SÉLECTION PAR ZONE, MODE OBJET ─────────────────────
					// B = rectangle, Ctrl+glisser = lasso, C = cercle — exactement les
					// mêmes touches qu'en mode édition, et la même logique de zone.
					// Ces outils n'existaient QU'EN ÉDITION : tout leur code avait été
					// écrit à l'intérieur du bloc `if (st->editMode && ...)`, donc en mode
					// objet il n'était jamais atteint. Ce n'était ni un choix ni une
					// limite technique — juste l'endroit où ils avaient été développés.
					// Blender les propose dans les deux modes : seule la CIBLE change
					// (sommets/arêtes/faces en édition, objets ici).
					{
						const Demo3D_ScreenProj proj = Demo3D_ScreenProj::Make(
							cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width, (float32)ctx.height);
						// NK_OBJ_ZONE="x0,y0,x1,y1" : applique UNE FOIS un rectangle de
						// sélection en mode objet, sans souris. Sans ce levier, la sélection
						// par zone ne serait vérifiable qu'à la main — donc pas en capture,
						// donc pas de façon reproductible.
						static bool objZoneDone = false;
						if (!objZoneDone) {
							objZoneDone = true;
							if (const char *oz = getenv("NK_OBJ_ZONE")) {
								float32 zv[4] = {0.f, 0.f, 0.f, 0.f};
								int32 zk = 0;
								const char *pz = oz;
								while (zk < 4 && *pz) {
									zv[zk++] = (float32)atof(pz);
									while (*pz && *pz != ',')
										pz++;
									if (*pz == ',')
										pz++;
								}
								const float32 zx0 = NkMin(zv[0], zv[2]), zx1 = NkMax(zv[0], zv[2]);
								const float32 zy0 = NkMin(zv[1], zv[3]), zy1 = NkMax(zv[1], zv[3]);
								Demo3D_SelectObjectsInZone(
									st, 0,
									[&](float32 px, float32 py) {
										return px >= zx0 && px <= zx1 && py >= zy0 && py <= zy1;
									},
									proj);
								int32 nSel = 0;
								for (int32 q = 0; q < (int32)Demo3DState::kNumObj; q++)
									if (st->gizmo.IsSelected(q))
										nSel++;
								logger.Info("[Demo3D][ZONE OBJET] rectangle -> {0} objets selectionnes, "
											"actif={1}\n",
											nSel, st->gizmo.ActiveIndex());
							}
						}
						const bool clickNowObj = gin.leftDown && !st->prevLeftDownObj;
						st->prevLeftDownObj = gin.leftDown;
						const bool altDownObj =
							NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
						// Le gizmo a la priorité : si une poignée est saisie, on ne démarre
						// pas de zone (sinon tout déplacement tracerait un rectangle).
						// Le gizmo des LUMIERES compte autant : tirer une poignee de lumiere
						// ne doit pas tracer un rectangle de selection par-dessus.
						const bool gizmoBusy = st->gizmo.IsDragging() || st->lightGizmo.IsDragging();

						if (st->selTool == 3) { // CERCLE : on peint tant que le bouton est tenu
							st->selX1 = gin.mouseX;
							st->selY1 = gin.mouseY;
							if (st->lastWheel != 0.f) {
								st->selCircleR += st->lastWheel * 6.f;
								st->selCircleR = NkMax(6.f, NkMin(400.f, st->selCircleR));
							}
							if (gin.leftDown && !gizmoBusy) {
								const float32 cx = gin.mouseX, cy = gin.mouseY;
								const float32 r2 = st->selCircleR * st->selCircleR;
								Demo3D_SelectObjectsInZone(
									st, gin.ctrlDown ? 2 : 1,
									[&](float32 px, float32 py) {
										return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r2;
									},
									proj);
							}
						} else if (clickNowObj && !gizmoBusy && !altDownObj &&
								   (st->selTool == 1 || gin.ctrlDown)) {
							st->selDragging = true;
							st->selTool = (st->selTool == 1) ? 1 : 2; // 1=rectangle, 2=lasso
							st->selX0 = st->selX1 = gin.mouseX;
							st->selY0 = st->selY1 = gin.mouseY;
							st->selLasso.Clear();
							st->selLasso.PushBack(NkVec2f{gin.mouseX, gin.mouseY});
							st->selMode = (st->selTool == 1) ? (gin.shiftDown ? 1 : (gin.ctrlDown ? 2 : 0))
															 : (gin.shiftDown ? 2 : 1);
						}
						if (st->selDragging) {
							st->selX1 = gin.mouseX;
							st->selY1 = gin.mouseY;
							if (st->selTool == 2) {
								const NkVec2f &lp = st->selLasso[(uint32)st->selLasso.Size() - 1];
								if (fabsf(lp.x - gin.mouseX) + fabsf(lp.y - gin.mouseY) > 3.f)
									st->selLasso.PushBack(NkVec2f{gin.mouseX, gin.mouseY});
							}
							if (!gin.leftDown) { // relache -> on applique la zone
								// UN CLIC N'EST PAS UNE ZONE : l'outil rectangle arme en
								// continu par la barre transformait chaque clic en
								// rectangle VIDE qui REMPLACAIT la selection -- la
								// « deselection instantanee » constatee par Rihen. En
								// dessous de 4 px, le pick du press garde son effet.
								const bool tinyObj = fabsf(st->selX1 - st->selX0) < 4.f &&
													 fabsf(st->selY1 - st->selY0) < 4.f;
								if (tinyObj) {
									// clic simple : rien a appliquer
								} else if (st->selTool == 1) {
									const float32 x0 = NkMin(st->selX0, st->selX1), x1 = NkMax(st->selX0, st->selX1);
									const float32 y0 = NkMin(st->selY0, st->selY1), y1 = NkMax(st->selY0, st->selY1);
									Demo3D_SelectObjectsInZone(
										st, st->selMode,
										[&](float32 px, float32 py) {
											return px >= x0 && px <= x1 && py >= y0 && py <= y1;
										},
										proj);
									logger.Info("[Demo3D] OBJET : selection RECTANGLE appliquee\n");
								} else if (st->selTool == 2 && st->selLasso.Size() >= 3) {
									Demo3D_SelectObjectsInZone(
										st, st->selMode,
										[&](float32 px, float32 py) {
											return Demo3D_PointInPoly(st->selLasso, px, py);
										},
										proj);
									logger.Info("[Demo3D] OBJET : selection LASSO appliquee ({0} points)\n",
												(uint32)st->selLasso.Size());
								}
								st->selDragging = false;
								st->selTool = 0; // one-shot, comme Blender
								st->selLasso.Clear();
							}
						}
					}

					// ── Sélection « outline silhouette » (option NK_SELECT_OUTLINE) ──
					// Soumet l'objet ACTIF au masque de silhouette : le liseré orange
					// épousera son maillage (post-process edge-detect). Limité aux objets
					// NON instanciés (spheres 0-15, cube 16, colonnes 17-18) ; les cubes
					// instanciés (19+) n'ont pas de transform per-instance côté drawcall
					// simple -> hors périmètre de cette démo.
					if (r3d->IsSelectionOutlineEnabled() && st->gizmo.HasSelection()) {
						// TOUS les objets sélectionnés, pas seulement l'ACTIF. Le code ne
						// soumettait que gizmo.ActiveIndex() : en sélection multiple, un seul
						// objet recevait le liseré orange alors que le gizmo, lui, agissait
						// bien sur tout le groupe — l'affichage mentait sur l'état réel.
						// Blender entoure tous les sélectionnés, l'actif d'une teinte plus
						// claire ; ici le masque de silhouette est monochrome, donc tous
						// ressortent de la même couleur (distinction actif/sélectionné à
						// faire plus tard, elle demande un second canal de masque).
						const int32 activeIdx = st->gizmo.ActiveIndex();
						int32 nOut = 0;
						// Les cubes INSTANCIÉS (19..82) sont inclus : ils n'ont pas de drawcall
						// individuel dans la passe principale (un seul draw instancié), mais le
						// masque de silhouette, lui, accepte un draw par objet — leur transform
						// est connue (objXform, renseignée à la construction du batch). Il n'y
						// avait donc aucune raison de les exclure : c'était une limite de
						// commodité, pas une contrainte technique.
						for (int32 i = 0; i < (int32)Demo3DState::kNumObj && i < renderer::NkGizmo3D::kMax; i++) {
							if (!st->gizmo.IsSelected(i))
								continue;
							NkDrawCall3D sdc;
							sdc.mesh = meshFor(i, (i < 16) ? st->meshSphere : st->meshCube);
							// Matrice EXACTE ayant servi à dessiner l'objet cette frame
							// (objXform est renseigné à la soumission). Sans ça, le liseré
							// se décale de l'objet dès qu'une transformation est en cours.
							// Repli : recalcul depuis la base de repos (objet pas encore
							// soumis cette frame, ex. tout premier pick).
							sdc.transform = (selDrawValid && i == selDrawIdx)
												? selDrawXform
												: ((i < (int32)Demo3DState::kNumObj) ? st->objXform[i]
																					 : userXform(i, Demo3D_ObjBase(i)));
							// L'objet ACTIF reçoit un liseré de teinte plus claire (façon
							// Blender) : sans cette distinction, impossible de savoir sur quel
							// objet porteront les opérations qui ne visent que l'actif.
							r3d->SubmitSelection(sdc, i == activeIdx);
							nOut++;
						}
						(void)nOut;
					}

					// Rendu PLEIN (façon Blender solide) : lignes fines (tiges/liserés) via
					// DrawDebugLine + formes PLEINES (cônes/cubes/rubans) via DrawDebugTriangle,
					// toutes en overlay (au-dessus de la scène, alpha-blend). Le 2e callback active
					// la surcharge Draw(drawLine, drawTri) du gizmo.
					// NK_OUTLINE_ONLY=1 : masque le gizmo pour une capture « liseré silhouette
					// SEUL » (le contour devient l'unique indicateur de sélection à l'écran).
					static int outlineOnly = -1;
					if (outlineOnly == -1) {
						const char *v = getenv("NK_OUTLINE_ONLY");
						outlineOnly = (v && v[0] && v[0] != '0') ? 1 : 0;
					}
					if (!outlineOnly)
						if (!nkvpGizmoHidden)
					st->gizmo.Draw(
							[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
							[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
								r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
							});
				}
			}

			// ── WIDGETS DES LUMIERES (facon Blender) ────────────────────────────────
			// Dessines en OVERLAY (dernier argument true) : un widget masque par la
			// geometrie ne sert a rien — on doit pouvoir attraper une lumiere placee
			// derriere un objet. NK_LIGHT_GIZMOS=0 les masque pour une capture propre ;
			// NK_LIGHT_SEL=<n> selectionne la n-ieme lumiere (teinte claire), ce qui
			// rend la distinction actif/selectionne verifiable en capture.
			{
				static int lgShow = -1;
				if (lgShow == -1) {
					const char *v = getenv("NK_LIGHT_GIZMOS");
					lgShow = (v && v[0] == '0') ? 0 : 1;
					if (const char *ls = getenv("NK_LIGHT_SEL"))
						st->lightSel = atoi(ls);
				}
				// NK_LIGHT_STYLE=<0|1|2> : 0 = symbole filaire facon Blender (defaut),
				// 1 = billboard facon Unreal, 2 = l'ancien solide 3D. Les DEUX designs
				// demandes sont dans le systeme ; ce levier permet de les comparer sur
				// la meme scene, sans recompiler et donc de facon reproductible.
				if (lgShow && st->showLightGizmos) {
					const NkVec3f camP = cam.GetPosition();
					// AXES ECRAN DE LA CAMERA — indispensables aux deux nouveaux designs :
					// un symbole « face camera » calcule depuis les axes du monde resterait
					// plaque dans un plan fixe et disparaitrait de profil. Meme construction
					// que Demo3D_ScreenProj, pour que ce qu'on DESSINE et ce qu'on PROJETTE
					// au pick soient rigoureusement le meme repere.
					{
						const NkVec3f fwd = (cam.GetTarget() - camP).Normalized();
						const NkVec3f rgt = fwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
						renderer::NkLightGizmo::SetCameraAxes(rgt, rgt.Cross(fwd).Normalized());
					}
					for (uint32 li = 0; li < (uint32)st->frameLights.Size(); li++) {
						const renderer::NkLightDesc &L = st->frameLights[li];
						// Distance camera->lumiere : dimensionne le corps du widget pour
						// qu'il reste lisible de loin sans ecraser la scene de pres.
						const float32 dist = (L.position - camP).Len();
						const bool sel = ((int32)li == st->lightSel);
						renderer::NkLightGizmo::Draw(
							L, sel, sel, dist,
							[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
							[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
								r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
							});
					}
					// POIGNEES DE MANIPULATION de la lumiere selectionnee. Sans elles le
					// pick serait sans suite : on saurait quelle lumiere est prise, sans
					// pouvoir la bouger. Dessinees APRES les widgets pour rester au-dessus.
					if (st->lightSel >= 0)
						if (!nkvpGizmoHidden)
					st->lightGizmo.Draw(
							[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
							[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
								r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
							});
				}
			}

			// ── WIDGETS DES EMPTIES (croix d'axes, facon Blender) ───────────────
			// Un empty n'a pas de geometrie mais il EXISTE dans la scene : une
			// croix d'axes a sa position EFFECTIVE (drag du gizmo compris),
			// teinte claire quand il est actif -- et les POIGNEES de son gizmo,
			// qui tournaient sans jamais etre dessinees (constate par Rihen).
			{
				const int32 esel = st->emptyGizmo.ActiveIndex();
				for (int32 e = 0; e < 70; ++e) {
					// Seuls les EMPTIES ont une croix : un maillage utilisateur a
					// son rendu, un slot libre n'existe pas.
					if (90 + e >= kNkvpFirstUser && nkvpUserKind[e - 6] != 4)
						continue;
					if (HostHiddenEff(90 + e))
						continue;
					const NkVec3f etr = st->emptyGizmo.TranslateOf(e);
					const NkVec3f ep{nkvpEmptyPos[e][0] + etr.x, nkvpEmptyPos[e][1] + etr.y,
									 nkvpEmptyPos[e][2] + etr.z};
					const float32 eh = 0.35f;
					const bool esl = (e == esel);
					const NkVec4f ecol = esl ? NkVec4f{1.f, 0.75f, 0.25f, 1.f}
											 : NkVec4f{0.75f, 0.75f, 0.78f, 0.9f};
					r3d->DrawDebugLine({ep.x - eh, ep.y, ep.z}, {ep.x + eh, ep.y, ep.z}, ecol, 0.f,
									   true);
					r3d->DrawDebugLine({ep.x, ep.y - eh, ep.z}, {ep.x, ep.y + eh, ep.z}, ecol, 0.f,
									   true);
					r3d->DrawDebugLine({ep.x, ep.y, ep.z - eh}, {ep.x, ep.y, ep.z + eh}, ecol, 0.f,
									   true);
				}
				// MARQUEUR DE SELECTION des maillages utilisateur : boite
				// filaire orange (le lisere de la demo ne les connait pas).
				for (int32 g2 = 6; g2 < 70; ++g2) {
					const int32 gn2 = 90 + g2;
					if (nkvpUserKind[g2 - 6] == 0 || nkvpUserKind[g2 - 6] == 4 ||
						nkvpUserKind[g2 - 6] == 5)
						continue;
					if (!st->emptyGizmo.IsSelected(g2) || HostHiddenEff(gn2))
						continue;
					// Boite ORIENTEE : les 8 coins passent par la transformation
					// effective (rotation, echelle, dimensions suivies -- Rihen).
					const NkVec3f tr2 = st->emptyGizmo.TranslateOf(g2);
					const NkVec3f os3 = st->emptyGizmo.ScaleOf(g2);
					const float32 kD2Rm = 0.017453292f;
					const NkMat4f mR =
						st->emptyGizmo.RotationOf(g2) *
						(NkMat4f::RotationZ(NkAngle::FromRad(nkvpEmptyRotDeg[g2][2] * kD2Rm)) *
						 NkMat4f::RotationY(NkAngle::FromRad(nkvpEmptyRotDeg[g2][1] * kD2Rm)) *
						 NkMat4f::RotationX(NkAngle::FromRad(nkvpEmptyRotDeg[g2][0] * kD2Rm)));
					const uint8 mk = nkvpUserKind[g2 - 6];
					float32 hb[3] = {0.62f, 0.62f, 0.62f};
					if (mk == 1)
						hb[0] = hb[1] = hb[2] = 0.58f;
					if (mk == 3) {
						hb[0] = hb[2] = 1.05f;
						hb[1] = 0.05f;
					}
					const float32 osv[3] = {os3.x, os3.y, os3.z};
					float32 hx[3];
					for (int32 a3 = 0; a3 < 3; ++a3) {
						hx[a3] = fabsf(nkvpEmptyScl[g2][a3]) * (1.f + osv[a3]) * hb[a3];
						if (nkvpBaseSet[gn2] && nkvpDimFactor[gn2][a3] > 0.f)
							hx[a3] *= nkvpDimFactor[gn2][a3];
						hx[a3] += 0.03f;
					}
					const NkVec3f c3{nkvpEmptyPos[g2][0] + tr2.x, nkvpEmptyPos[g2][1] + tr2.y,
									 nkvpEmptyPos[g2][2] + tr2.z};
					NkVec3f cn[8];
					for (int32 ci2 = 0; ci2 < 8; ++ci2) {
						const float32 lx = (ci2 & 1) ? hx[0] : -hx[0];
						const float32 ly = (ci2 & 2) ? hx[1] : -hx[1];
						const float32 lz = (ci2 & 4) ? hx[2] : -hx[2];
						cn[ci2] = {c3.x + mR.mat[0][0] * lx + mR.mat[1][0] * ly + mR.mat[2][0] * lz,
								   c3.y + mR.mat[0][1] * lx + mR.mat[1][1] * ly + mR.mat[2][1] * lz,
								   c3.z + mR.mat[0][2] * lx + mR.mat[1][2] * ly + mR.mat[2][2] * lz};
					}
					const NkVec4f oc{1.f, 0.45f, 0.05f, 1.f};
					for (int32 a3 = 0; a3 < 8; ++a3)
						for (int32 b3 = 0; b3 < 3; ++b3) {
							const int32 nb = a3 | (1 << b3);
							if (nb != a3)
								r3d->DrawDebugLine(cn[a3], cn[nb], oc, 0.f, true);
						}
				}
				if (esel >= 0 && !nkvpGizmoHidden)
					st->emptyGizmo.Draw(
						[&](NkVec3f a, NkVec3f b, NkVec4f c) { r3d->DrawDebugLine(a, b, c, 0.f, true); },
						[&](NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f col) {
							r3d->DrawDebugTriangle(a, b, c, col, 0.f, true);
						});
			}

			// (Le gizmo d'édition de vertices + pick VERTEX/EDGE/FACE + marqueurs fins
			//  sont désormais gérés plus haut, dans la section « EDIT MODE ».)

			// ── Axes X/Y/Z en LIGNES 3D réelles (DrawDebugLine) : correct partout ───
			// (perspective, ancrés à l'origine, parallèles aux objets verticaux, top/bottom OK).
			// Remplace les axes du SHADER grille (désactivés via g.showAxes=false à l'Init) qui
			// avaient des artefacts de projection sur l'axe Y. Étendus loin -> effet "infini".
			// NK_GRID_CLEAN : on masque ces axes debug 3D épais (la grille du shader dessine
			// alors ses PROPRES axes sol fins AA -> rendu épuré pour juger la grille).
			// ── CURSEUR 3D façon Blender ─────────────────────────────────────────────
			// Placement (Shift + clic DROIT) : rayon sous la souris -> 1) surface du mesh
			// ÉDITÉ si l'on est en Edit Mode (intersection exacte des triangles), 2) sinon
			// le plan du SOL (y=0), 3) sinon un plan face-caméra passant par la position
			// actuelle du curseur (cas d'un rayon qui monte, jamais de « pas de résultat »).
			// Dessin : petit cercle POINTILLÉ rouge/blanc + croix, taille écran-constante,
			// en overlay (toujours visible) — comme dans Blender.
			{
				const NkVec3f ccPos = cam.GetPosition(), ccTgt = cam.GetTarget();
				const NkVec3f cfwd = (ccTgt - ccPos).Normalized();
				const NkVec3f crgt = cfwd.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
				const NkVec3f cup = crgt.Cross(cfwd).Normalized();
				const float32 cthY = tanf(60.f * 0.5f * 3.14159265f / 180.f);
				const float32 cthX = cthY * ((float32)ctx.width / (float32)ctx.height);
				if (st->cursorPlacePending) {
					st->cursorPlacePending = false;
					const float32 nx = st->cursorPX / (float32)ctx.width * 2.f - 1.f;
					const float32 ny = 1.f - st->cursorPY / (float32)ctx.height * 2.f;
					NkVec3f rd = cfwd + crgt * (nx * cthX) + cup * (ny * cthY);
					const float32 rl = rd.Len();
					if (rl > 1e-6f)
						rd = rd * (1.f / rl);
					float32 bestT = 1e30f;
					if (st->editMode) { // surface du maillage édité (Möller–Trumbore)
						for (uint32 t = 0; t + 2 < (uint32)st->editIdx.Size(); t += 3) {
							const NkVec3f v0 = st->editAnchor * st->editLive[st->editIdx[t]].pos;
							const NkVec3f v1 = st->editAnchor * st->editLive[st->editIdx[t + 1]].pos;
							const NkVec3f v2 = st->editAnchor * st->editLive[st->editIdx[t + 2]].pos;
							const NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = rd.Cross(e2);
							const float32 aa = e1.Dot(h);
							if (fabsf(aa) < 1e-7f)
								continue;
							const float32 fi = 1.f / aa;
							const NkVec3f s = ccPos - v0;
							const float32 u = fi * s.Dot(h);
							if (u < 0.f || u > 1.f)
								continue;
							const NkVec3f q = s.Cross(e1);
							const float32 vvv = fi * rd.Dot(q);
							if (vvv < 0.f || u + vvv > 1.f)
								continue;
							const float32 tt = fi * e2.Dot(q);
							if (tt > 1e-4f && tt < bestT)
								bestT = tt;
						}
					}
					if (bestT > 1e29f && fabsf(rd.y) > 1e-5f) { // plan du sol y=0
						const float32 tg = -ccPos.y / rd.y;
						if (tg > 1e-4f)
							bestT = tg;
					}
					if (bestT > 1e29f) { // repli : plan face-caméra passant par le curseur
						const float32 dn = rd.Dot(cfwd);
						if (fabsf(dn) > 1e-5f)
							bestT = (st->cursor3D - ccPos).Dot(cfwd) / dn;
					}
					if (bestT > 1e-4f && bestT < 1e29f) {
						st->cursor3D = ccPos + rd * bestT;
						logger.Info("[Demo3D] Curseur 3D -> ({0}, {1}, {2})\n", st->cursor3D.x, st->cursor3D.y,
									st->cursor3D.z);
					}
				}
				// Dessin (taille ~14 px de rayon quelle que soit la distance).
				const NkVec3f CC = st->cursor3D;
				float32 depth = (CC - ccPos).Dot(cfwd);
				if (depth < 0.05f)
					depth = 0.05f;
				const float32 Rw = 14.f * ((2.f * cthY) / (float32)ctx.height) * depth;
				const NkVec4f colR{0.92f, 0.16f, 0.13f, 1.f}, colW{1.f, 1.f, 1.f, 1.f};
				NkVec3f prevC = CC + crgt * Rw;
				for (int32 kseg = 1; kseg <= 32; kseg++) {
					const float32 a = (float32)kseg * 6.2831853f / 32.f;
					const NkVec3f P = CC + (crgt * cosf(a) + cup * sinf(a)) * Rw;
					r3d->DrawDebugLine(prevC, P, (kseg & 1) ? colR : colW, 0.f, true); // pointillés bicolores
					prevC = P;
				}
				// Croix : 4 branches courtes qui dépassent du cercle (repère de placement).
				r3d->DrawDebugLine(CC - crgt * (Rw * 2.0f), CC - crgt * (Rw * 0.7f), colW, 0.f, true);
				r3d->DrawDebugLine(CC + crgt * (Rw * 0.7f), CC + crgt * (Rw * 2.0f), colW, 0.f, true);
				r3d->DrawDebugLine(CC - cup * (Rw * 2.0f), CC - cup * (Rw * 0.7f), colW, 0.f, true);
				r3d->DrawDebugLine(CC + cup * (Rw * 0.7f), CC + cup * (Rw * 2.0f), colW, 0.f, true);
			}

			// PORTAGE NK3DModeler : ces trois axes debug sont AUSSI sous la
			// bascule « Axes du plan » du shell -- la decocher les eteint.
			if (!gridClean && nkvpAxesOn) {
				const float32 A = 1000.f;
				const float32 h = 0.02f; // légèrement au-dessus du sol/grille -> pas de z-fight (pointillés)
				r3d->DrawDebugLine({-A, h, 0.f}, {A, h, 0.f}, {1.f, 0.f, 0.f, 1.f});	 // X rouge
				r3d->DrawDebugLine({0.f, -A, 0.f}, {0.f, A, 0.f}, {0.f, 1.f, 0.f, 1.f}); // Y vert
				r3d->DrawDebugLine({0.f, h, -A}, {0.f, h, A}, {0.f, 0.f, 1.f, 1.f});	 // Z bleu
			}

			// ── Overlay ──────────────────────────────────────────────────────────
			// PORTAGE NK3DModeler : le HUD passe sous la bascule « Affichage » du
			// shell — il chevauchait la barre d'outils de l'editeur.
			if (auto *overlay = nkvpHudOn ? ctx.renderer->GetOverlay() : nullptr) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawStats(ctx.renderer->GetStats());
				{
					const char *sm[6] = {"RENDERED", "SOLID", "WIREFRAME", "NORMAL", "UV", "AO"};
					const char *cm[3] = {"MATERIAL", "GRIS", "CUSTOM"};
					int32 mcId = 0;
					if (auto *r3dh = ctx.renderer->GetRender3D())
						mcId = r3dh->Matcap();
					// Nom lu dans NkMatcapLibrary : source unique. Une liste recopiee ici
					// se desynchroniserait du contenu reel de l'atlas au premier ajout.
					const char *mcName = renderer::NkMatcapLibrary::Name(mcId);
					// MatCap pertinent seulement en SOLID/WIREFRAME (modes 1 et 2).
					if (st->shadingMode == 1 || st->shadingMode == 2)
						overlay->DrawText(
							{20.f, 35.f},
							"Demo 3D  |  API : %s  |  Affichage(Z): %s  |  MatCap(M): %s (%d/%d)  |  Couleur(B): %s",
							NkGraphicsApiName(ctx.api), sm[st->shadingMode % 6], mcName, mcId + 1,
							(int32)renderer::NkRender3D::kMatcapCount, cm[st->unlitColorMode % 3]);
					else
						overlay->DrawText({20.f, 35.f}, "Demo 3D  |  API : %s  |  Affichage(Z): %s  |  Couleur(B): %s",
										  NkGraphicsApiName(ctx.api), sm[st->shadingMode % 6],
										  cm[st->unlitColorMode % 3]);
				}
				overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f  |  dt: %.2f ms", dt > 1e-4f ? 1.f / dt : 0.f,
								  dt * 1000.f);
				// Phase H : indication visuelle du chargement texture file-based.
				overlay->DrawText({20.f, 75.f}, "[Phase H] Texture file-based : %s",
								  st->phaseHLoadOk ? "test_pattern.png LOAD OK" : "fallback procedural");
				// Aide gizmo : mode + orientation + rappel des touches.
				const char *gmName[4] = {"TRANSLATE", "ROTATE", "SCALE", "COMBINE (T+R+S)"};
				const char *orName[3] = {"GLOBAL", "LOCAL", "NORMAL"};
				const char *seName[3] = {"VERTEX", "EDGE", "FACE"};
				if (st->editMode) {
					char modeStr[8];
					int mi = 0;
					if (st->editSelMask & 1)
						modeStr[mi++] = 'V';
					if (st->editSelMask & 2)
						modeStr[mi++] = 'E';
					if (st->editSelMask & 4)
						modeStr[mi++] = 'F';
					modeStr[mi] = '\0';
					(void)seName;
					// L'orientation courante du gizmo est affichée AUSSI en edit mode (P2) :
					// « NORMAL* » = un repère d'élément (normale de face/arête/sommet) est
					// effectivement posé ; « NORMAL » sans étoile = repli Local.
					const int32 eo = st->editGizmo.Orientation() % 3;
					const bool nf = (eo == 2) && st->editGizmo.HasNormalFrame();
					overlay->DrawText({20.f, 100.f},
									  "EDIT MODE (obj #%d)  |  Modes(1/2/3,Shift=combi): %s  |  Gizmo(G/R/S/C): %s  |  "
									  "Orient(,): %s%s  |  X-ray(Alt+Z): %s",
									  st->editObjIdx, modeStr, gmName[st->editGizmo.Mode() & 3], orName[eo],
									  nf ? "*" : "", st->editXray ? "ON" : "OFF");
					overlay->DrawText({20.f, 118.f},
									  "E=extrude %s(Sh:%s) X=suppr M=souder(Sh:%s) W=subdiv(Sh:x%d) "
									  "Ctrl+R=loopcut(Sh:x%d) K=couteau%s | TAB=sortir",
									  (st->editSelMask & 4)	  ? "FACES"
									  : (st->editSelMask & 2) ? "ARETES"
															  : "SOMMETS",
									  st->extrudeIndividual ? "indiv" : "region",
									  (st->mergeMode == 2)	 ? "last"
									  : (st->mergeMode == 1) ? "first"
															 : "center",
									  st->subdivCuts, st->loopCuts,
									  st->knifeArmed ? (st->knifeHasP0 ? "[2e pt]" : "[1er pt]") : "");
					// Outils de sélection : rappel des raccourcis + outil modal actif.
					const char *stName[4] = {"-", "RECTANGLE", "LASSO", "CERCLE"};
					overlay->DrawText({20.f, 136.f},
									  "Selection: B=rect  Ctrl+glisser=lasso  C=cercle(molette=rayon)  "
									  "Alt+clic=boucle  |  outil: %s%s",
									  stName[st->selTool & 3],
									  (st->selTool == 3) ? "  (clic=peindre, Echap=sortir)" : "");
					// Nouvelles opérations de maillage (lot 2) — sur DEUX lignes : la barre
					// d'aide dépasserait la largeur de l'écran sur une seule.
					overlay->DrawText({20.f, 172.f},
									  "Ctrl+B=bevel ARETE  Ctrl+Shift+B=bevel SOMMET (Alt+B=segments x%d, "
									  "Alt+Shift+B=largeur %s)  |  I=inset %s (Shift=mode, Alt=prof %.2f)",
									  st->bevelSegments, st->bevelOffset <= 0.f ? "AUTO" : "manuelle",
									  st->insetIndividual ? "indiv" : "region", st->insetDepth);
					overlay->DrawText({20.f, 190.f},
									  "V=edge split (Shift=ecart %s)  |  J=spin %d pas / %.0f deg "
									  "(Shift=pas, Alt=angle, centre=curseur 3D)  |  Ctrl+X=dissolve %s  |  "
									  "Shift+Alt+S=TO SPHERE  Ctrl+Alt+S=SHRINK/FATTEN",
									  st->splitGap <= 0.f ? "AUTO" : "large", st->spinSteps, st->spinAngleDeg,
									  (st->editSelMask & 4) ? "FACES" : ((st->editSelMask & 2) ? "ARETES" : "SOMMETS"));
					// Ombrage courant (Shift+S / Shift+F) + point de pivot (.) + curseur 3D.
					const bool anySm = st->editHE.AnyFaceSmooth();
					const bool allSm = st->editHE.AllFacesSmooth();
					// OPERATION MODALE EN COURS : bandeau facon Blender (op + valeurs + sortie).
					if (st->modalOp != 0) {
						overlay->DrawText({20.f, 208.f},
											"[MODAL] %s  |  SOURIS CAPTUREE (ni camera, ni selection, ni gizmo)  |  "
											"%s(souris): %.4f  |  %s(molette): %d%s  |  clic gauche=CONFIRMER  ·  "
											"Echap/clic droit=ANNULER",
											Demo3D_ModalName(st->modalOp),
											(st->modalOp == 4) ? "slide" : "valeur", st->modalVal,
											(st->modalOp == 4) ? "coupes" : "segments", st->modalSeg,
											(st->modalOp == 4) ? "  |  anneau: survol souris (occlusion testee)" : "");
					}
					overlay->DrawText({20.f, 154.f},
									  "Ombrage(Shift+S/Shift+F): %s  |  Pivot(.): %s  |  Curseur 3D: "
									  "Shift+clic droit (Alt+. = origine)",
									  allSm ? "SMOOTH" : (anySm ? "MIXTE" : "FLAT"),
									  st->editGizmo.PivotName());
				} else {
					overlay->DrawText(
						{20.f, 100.f},
						"OBJET  |  Gizmo(G/R/S/C): %s  |  Orient(,): %s  |  Pivot(.): %s  |  TAB=editer l'objet "
						"selectionne",
						gmName[st->gizmo.Mode() & 3], orName[st->gizmo.Orientation() % 3], st->gizmo.PivotName());
					overlay->DrawText({20.f, 118.f}, "clic=sel  Shift+clic=multi  A/Alt+A=tout/rien  Alt+G/R/S=clear  "
													 "|  Ctrl=snap  X/Y/Z=verrou axe  |  Shift+clic droit=curseur 3D");
				}

				// ── Tracé des OUTILS DE SÉLECTION (overlay 2D, façon Blender) ──────
				// Rectangle en POINTILLÉS, lasso en ligne fine, cercle en contour.
				if (st->editMode && st->selTool != 0) {
					if (auto *r2dS = ctx.renderer->GetRender2D()) {
						const NkVec4f col{1.f, 1.f, 1.f, 0.85f};
						if (st->selTool == 1 && st->selDragging) {
							const float32 x0 = NkMin(st->selX0, st->selX1), x1 = NkMax(st->selX0, st->selX1);
							const float32 y0 = NkMin(st->selY0, st->selY1), y1 = NkMax(st->selY0, st->selY1);
							// Pointillés : segments de 6 px espacés de 6 px sur les 4 bords.
							for (float32 x = x0; x < x1; x += 12.f) {
								const float32 xe = NkMin(x + 6.f, x1);
								r2dS->DrawLine({x, y0}, {xe, y0}, col, 1.f);
								r2dS->DrawLine({x, y1}, {xe, y1}, col, 1.f);
							}
							for (float32 y = y0; y < y1; y += 12.f) {
								const float32 ye = NkMin(y + 6.f, y1);
								r2dS->DrawLine({x0, y}, {x0, ye}, col, 1.f);
								r2dS->DrawLine({x1, y}, {x1, ye}, col, 1.f);
							}
						} else if (st->selTool == 2 && st->selLasso.Size() >= 2) {
							for (uint32 i = 1; i < (uint32)st->selLasso.Size(); i++)
								r2dS->DrawLine(st->selLasso[i - 1], st->selLasso[i], col, 1.f);
							r2dS->DrawLine(st->selLasso[(uint32)st->selLasso.Size() - 1], st->selLasso[0], col, 1.f);
						} else if (st->selTool == 3) {
							// Cercle : 48 segments autour du curseur.
							const int32 kSeg = 48;
							float32 pxp = st->selX1 + st->selCircleR, pyp = st->selY1;
							for (int32 i = 1; i <= kSeg; i++) {
								const float32 a = (float32)i * 6.2831853f / (float32)kSeg;
								const float32 nx = st->selX1 + cosf(a) * st->selCircleR;
								const float32 ny = st->selY1 + sinf(a) * st->selCircleR;
								r2dS->DrawLine({pxp, pyp}, {nx, ny}, col, 1.f);
								pxp = nx;
								pyp = ny;
							}
						}
					}
				}

				// ── Debug panel : params shadow live-tunable ───────────────────────
				// Background semi-transparent en haut a droite
				if (auto *r2d = ctx.renderer->GetRender2D()) {
					NkRectF panel = {(float32)ctx.width - 320.f, 10.f, 310.f, 180.f};
					r2d->FillRect(panel, {0.f, 0.f, 0.f, 0.6f});
				}
				const float32 px = (float32)ctx.width - 310.f;
				overlay->DrawText({px, 30.f}, "== Shadow tweak (panel debug) ==");
				if (auto *sh = ctx.renderer->GetShadow()) {
					const auto &cfg = sh->GetConfig();
					overlay->DrawText({px, 50.f}, "F5/F6    bias     : %.4f", cfg.shadowBias);
					overlay->DrawText({px, 70.f}, " VSM atlas : %u px", sh->GetAtlasSize());
					overlay->DrawText({px, 90.f}, "F7       quality  : %d", (int)cfg.quality);
					overlay->DrawText({px, 110.f}, "F8/F9    softness : %.3f", cfg.softness);
					overlay->DrawText({px, 130.f}, " slots: %u (rend %u | cache %u)", sh->GetActiveSlotCount(),
									  sh->GetRenderedSlotsCount(), sh->GetCachedSlotsCount());
				} else {
					overlay->DrawText({px, 50.f}, "(no shadow system)");
				}
				overlay->DrawText({px, 160.f}, "framesInFlight : %u", (uint32)ctx.renderer->GetConfig().framesInFlight);

				overlay->EndOverlay();
			}

			// PORTAGE NK3DModeler : Present = executer le graphe dans le cmd de
			// l'editeur. Il ecrit dans la cible hors ecran (SetFinalColorTarget
			// pose par l'hote) que l'interface affiche ensuite comme une image.
			if (nkvpCmd)
				if (auto *graph = ctx.renderer->GetRenderGraph())
					graph->Execute((NkICommandBuffer *)nkvpCmd);
		}

		void Demo3D_Shutdown(DemoCtx &ctx) {
			auto *st = (Demo3DState *)ctx.userData;
			if (st && st->maskedMat)
				NkMaterial::Destroy(st->maskedMat);
			delete st;
			ctx.userData = nullptr;
			logger.Info("[Demo3D] Shutdown\n");
		}


		// ═══════════════════════ HOTE NK3DModeler ═══════════════════════════
		// Ce bloc n'existait pas dans renderdemo : c'est le pont entre la demo
		// (portee telle quelle ci-dessus) et l'editeur. Il possede le renderer
		// — la MEME config que --demo=2 dans le main du Sandbox —, la cible
		// hors ecran que l'interface affiche, et le DemoCtx que la demo recoit.
		// Plomberie identique a NkViewport3D.cpp (elle tourne depuis des jours) :
		// second NkRenderer sur le device de l'editeur, rendu redirige vers la
		// cible (SetFinalColorTarget) a la resolution de la vue
		// (SetRenderSizeOverride), texture publiee sous l'id 4096 — le meme
		// qu'avant, pour que l'interface n'ait pas a changer d'une ligne.
		namespace {
			struct NkDemo3DHostState {
					DemoCtx ctx;
					NkOffscreenTarget *rt = nullptr;
					uint32 wantW = 1280, wantH = 720;
					bool tried = false;
					bool ok = false;
					const char *err = nullptr;
					float64 lastNs = 0.0;
			};
			NkDemo3DHostState hst;
			constexpr uint32 kHostTexId = 4096u;

			bool HostInit() {
				if (hst.tried)
					return hst.ok;
				hst.tried = true;
				if (!hst.ctx.device || !hst.ctx.device->IsValid()) {
					hst.err = "device partage absent";
					return false;
				}
				// La config EXACTE de --demo=2 (BuildConfig, main.cpp du Sandbox) :
				// ForGame + une seule cascade d'ombre — la scene tient dans ~7
				// unites, et une cascade large evite le scintillement des
				// transitions quand la camera orbite.
				NkRendererConfig cfg =
					NkRendererConfig::ForGame(hst.ctx.device->GetApi(), hst.wantW, hst.wantH);
				cfg.shadow.cascadeCount = 1;
				cfg.Enable(NK_SS_OFFSCREEN);
				hst.ctx.api = hst.ctx.device->GetApi();
				hst.ctx.width = hst.wantW;
				hst.ctx.height = hst.wantH;
				hst.ctx.renderer = NkRenderer::Create(hst.ctx.device, cfg);
				if (!hst.ctx.renderer) {
					hst.err = "creation du renderer refusee";
					return false;
				}
				NkOffscreenDesc od;
				od.width = hst.wantW;
				od.height = hst.wantH;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				od.readback = false;
				od.name = "NK3DModelerDemo3D";
				hst.rt = hst.ctx.renderer->CreateOffscreen(od);
				if (!hst.rt || !hst.rt->IsValid()) {
					hst.err = "cible hors ecran refusee";
					return false;
				}
				// Sans l'override, le graphe rendrait a la taille de la FENETRE
				// dans une cible a la taille de la VUE.
				hst.ctx.renderer->SetRenderSizeOverride(hst.wantW, hst.wantH);
				if (auto *texLib = hst.ctx.renderer->GetTextures())
					hst.ctx.renderer->SetFinalColorTarget(texLib->GetRHIHandle(hst.rt->GetColorResult()));
				nkvpW = (float32)hst.wantW;
				nkvpH = (float32)hst.wantH;
				if (!Demo3D_Init(hst.ctx)) {
					hst.err = "Demo3D_Init a echoue";
					return false;
				}
				// NK_VP_ORTHO=1 : demarre la vue en projection ORTHOGRAPHIQUE.
				// Outil de verification headless (captures d'agent) : l'etat ortho
				// se constate sans avoir a cliquer dans une session en cours.
				if (const char *oe = getenv("NK_VP_ORTHO"))
					if (oe[0] && oe[0] != '0')
						((Demo3DState *)hst.ctx.userData)->orthoView = true;
				hst.ok = true;
				return true;
			}
		} // namespace

		void Demo3DHostSetDevice(void *device) {
			hst.ctx.device = (NkIDevice *)device;
		}

		void Demo3DHostResize(uint32 w, uint32 h) {
			if (w < 16u)
				w = 16u;
			if (h < 16u)
				h = 16u;
			hst.wantW = w;
			hst.wantH = h;
			// La cible n'est refaite que si la taille CHANGE : le redimensionnement
			// d'une fenetre appelle cette fonction a chaque pixel parcouru.
			if (!hst.ok || !hst.rt || (w == hst.ctx.width && h == hst.ctx.height))
				return;
			if (hst.rt->Resize(w, h)) {
				hst.ctx.width = w;
				hst.ctx.height = h;
				nkvpW = (float32)w;
				nkvpH = (float32)h;
				hst.ctx.renderer->SetRenderSizeOverride(w, h);
				if (auto *texLib = hst.ctx.renderer->GetTextures())
					hst.ctx.renderer->SetFinalColorTarget(texLib->GetRHIHandle(hst.rt->GetColorResult()));
			}
		}

		void Demo3DHostSetView(float32 offX, float32 offY, bool hover, bool inputOn) {
			nkvpOffX = offX;
			nkvpOffY = offY;
			nkvpHover = hover;
			nkvpInputOn = inputOn;
		}

		void Demo3DHostFrame(void *cmd) {
			if (!HostInit() || !cmd)
				return;
			// dt calcule ICI : le crochet preUI de l'editeur ne le fournit pas, et
			// la demo (orbite auto, GI anime, modales) en a besoin. Meme garde-fou
			// que le main du Sandbox : un dt aberrant devient 1/60.
			const float64 nowNs = ::nkentseu::NkChrono::Now().nanoseconds;
			float32 dt = hst.lastNs > 0.0 ? (float32)((nowNs - hst.lastNs) / 1.0e9) : (1.f / 60.f);
			hst.lastNs = nowNs;
			if (dt <= 0.f || dt > 0.25f)
				dt = 1.f / 60.f;
			hst.ctx.totalTime += dt;
			hst.ctx.frame++;
			HostParentEnsureInit(); // la parente sert DANS la frame (visibilite)
			nkvpCmd = cmd;
			Demo3D_Frame(hst.ctx, dt);
			nkvpCmd = nullptr;
			// Parente : repercuter les deltas des parents a leurs enfants, et
			// faire respecter le cadenas (INselectionnable, meme depuis la vue).
			HostHierarchyFrame();
		}

		void Demo3DHostRegisterInto(void *guiBackend) {
			if (!hst.ok || !hst.rt || !guiBackend)
				return;
			auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
			auto *texLib = hst.ctx.renderer->GetTextures();
			if (!texLib)
				return;
			b->RegisterTexture(kHostTexId, texLib->GetRHIHandle(hst.rt->GetColorResult()));
		}

		// ── ACCESSEURS DU CABLAGE ───────────────────────────────────────────
		// Les boutons du shell parlent a la demo A TRAVERS ces fonctions, qui
		// refont EXACTEMENT ce que font ses raccourcis (memes lignes, memes
		// champs). Regle : jamais de logique nouvelle ici — si un raccourci de
		// la demo fait trois ecritures, l'accesseur fait les trois memes.
		namespace {
			Demo3DState *HostSt() {
				return hst.ok ? (Demo3DState *)hst.ctx.userData : nullptr;
			}
			renderer::NkRender3D *HostR3D() {
				return hst.ok ? hst.ctx.renderer->GetRender3D() : nullptr;
			}
			// Memoire du lisere (le moteur n'a pas de getter) et de la camera
			// memorisee par le menu de vue.
			bool hostOutlineOn = true;
			bool hostCamStored = false;
			NkVec3f hostCamTarget{0.f, 0.5f, 0.f};
			float32 hostCamDist = 6.5f, hostCamYaw = 0.7f, hostCamPitch = 0.4f;
			bool hostCamOrtho = false;
		} // namespace

		// ── Ombrage (la touche Z de la demo, adressable) ────────────────────
		void Demo3DHostSetShading(int32 mode) {
			auto *st = HostSt();
			auto *r3d = HostR3D();
			if (!st || !r3d)
				return;
			st->shadingMode = ((mode % 6) + 6) % 6;
			const int32 vm[6] = {0, 1, 1, 2, 3, 4};
			r3d->SetWireframe(st->shadingMode == 2);
			r3d->SetViewMode(vm[st->shadingMode]);
		}
		int32 Demo3DHostShading() {
			auto *st = HostSt();
			return st ? st->shadingMode : 0;
		}
		void Demo3DHostSetUnlitColor(int32 mode) {
			if (auto *st = HostSt())
				st->unlitColorMode = ((mode % 3) + 3) % 3;
		}
		int32 Demo3DHostUnlitColor() {
			auto *st = HostSt();
			return st ? st->unlitColorMode : 1;
		}

		// ── Matcaps (la touche M, adressable + noms pour l'interface) ───────
		void Demo3DHostSetMatcap(int32 id) {
			if (auto *r3d = HostR3D())
				r3d->SetMatcap(id);
		}
		int32 Demo3DHostMatcap() {
			auto *r3d = HostR3D();
			return r3d ? r3d->Matcap() : 0;
		}
		// Les PIXELS d'une boule matcap : la bibliotheque les genere, l'hote
		// les tend a l'interface qui les uploade comme n'importe quelle icone
		// -- c'est ce qui donne un APERCU REEL dans le selecteur.
		void Demo3DHostMatcapBall(int32 id, uint8 *rgba, uint32 size) {
			if (rgba && size)
				renderer::NkMatcapLibrary::GenerateBall(id, size, rgba);
		}
		int32 Demo3DHostMatcapCount() {
			return (int32)renderer::NkRender3D::kMatcapCount;
		}
		const char *Demo3DHostMatcapName(int32 id) {
			return renderer::NkMatcapLibrary::Name(id);
		}

		// ── Projection et vues d'axe (le pave numerique, adressable) ────────
		void Demo3DHostSetOrtho(bool on) {
			if (auto *st = HostSt())
				st->orthoView = on;
		}
		bool Demo3DHostIsOrtho() {
			auto *st = HostSt();
			return st && st->orthoView;
		}
		void Demo3DHostAxisView(int32 which, bool opposite) {
			auto *st = HostSt();
			if (!st)
				return;
			auto &c = st->editorCam;
			const NkVec3f t = c.GetTarget();
			const float32 d = c.GetDistance();
			const float32 P = 1.55f; // ~90 degres (meme clamp que la demo)
			if (which == 0)
				c.SetCenter(t, d, opposite ? -1.5708f : 1.5708f, 0.f); // avant / arriere
			else if (which == 1)
				c.SetCenter(t, d, opposite ? 3.1416f : 0.f, 0.f); // droite / gauche
			else
				c.SetCenter(t, d, 0.f, opposite ? -P : P); // dessus / dessous
			st->orthoView = true; // vue axiale -> ortho, facon Blender
		}
		void Demo3DHostResetView() {
			auto *st = HostSt();
			if (!st)
				return;
			// La pose d'OUVERTURE de la demo (Demo3D_Init).
			st->editorCam.SetCenter({0.f, 0.5f, 0.f}, 6.5f, 0.7f, 0.4f);
			st->orthoView = false;
		}
		void Demo3DHostStoreCamera() {
			auto *st = HostSt();
			if (!st)
				return;
			hostCamTarget = st->editorCam.GetTarget();
			hostCamDist = st->editorCam.GetDistance();
			hostCamYaw = st->editorCam.GetYaw();
			hostCamPitch = st->editorCam.GetPitch();
			hostCamOrtho = st->orthoView;
			hostCamStored = true;
		}
		bool Demo3DHostRecallCamera() {
			auto *st = HostSt();
			if (!st || !hostCamStored)
				return false;
			st->editorCam.SetCenter(hostCamTarget, hostCamDist, hostCamYaw, hostCamPitch);
			st->orthoView = hostCamOrtho;
			return true;
		}

		// ── Navigation directe (gizmo de navigation, loupe, main) ───────────
		void Demo3DHostOrbit(float32 dx, float32 dy) {
			auto *st = HostSt();
			if (!st)
				return;
			st->editorCam.Rotate(dx, dy);
			st->orthoView = false; // orbite libre -> perspective (meme regle que la demo)
		}
		void Demo3DHostPan(float32 dx, float32 dy) {
			if (auto *st = HostSt())
				st->editorCam.Pan(-dx, -dy); // « grab » facon Blender, comme la demo
		}
		void Demo3DHostZoomWheel(float32 notches) {
			if (auto *st = HostSt())
				st->wheelAccum += (float64)notches; // le MEME chemin que la molette
		}
		void Demo3DHostToggleFlyCam() {
			if (auto *st = HostSt())
				st->useSimCam = !st->useSimCam;
		}
		bool Demo3DHostIsFlyCam() {
			auto *st = HostSt();
			return st && st->useSimCam;
		}
		void Demo3DHostSetCamSpeed(float32 mult) {
			auto *st = HostSt();
			if (!st)
				return;
			// Le pan est proportionnel (defaut 0.0015), le zoom est un facteur par
			// cran (defaut 0.88 ; plus PETIT = plus rapide). La rotation ne change
			// pas : on ne « tourne pas plus vite » dans une grande scene.
			st->editorCam.SetPanSpeed(0.0015f * mult);
			float32 zs = 0.88f - 0.04f * (mult - 1.f);
			if (zs < 0.55f)
				zs = 0.55f;
			st->editorCam.SetZoomStep(zs);
		}
		void Demo3DHostCameraAxes(float32 *rgt, float32 *upv, float32 *fwd) {
			auto *st = HostSt();
			NkVec3f f{0.f, 0.f, -1.f};
			if (st && st->useSimCam)
				f = st->simCam.GetForward();
			else if (st) {
				const NkVec3f d = st->editorCam.GetTarget() - st->editorCam.GetPosition();
				const float32 l = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
				if (l > 1e-6f)
					f = {d.x / l, d.y / l, d.z / l};
			}
			// right = f ^ up_monde, up = right ^ f — le repere de la scene.
			NkVec3f r{f.z * 1.f - 0.f, 0.f - f.x * 0.f, 0.f}; // f ^ (0,1,0)
			r = {-f.z, 0.f, f.x};
			float32 rl = sqrtf(r.x * r.x + r.y * r.y + r.z * r.z);
			if (rl < 1e-6f) {
				r = {1.f, 0.f, 0.f};
				rl = 1.f;
			}
			r = {r.x / rl, r.y / rl, r.z / rl};
			const NkVec3f u{r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x};
			rgt[0] = r.x;
			rgt[1] = r.y;
			rgt[2] = r.z;
			upv[0] = u.x;
			upv[1] = u.y;
			upv[2] = u.z;
			fwd[0] = f.x;
			fwd[1] = f.y;
			fwd[2] = f.z;
		}

		// ── Gizmo : operation, orientation, aimantation, visibilite ─────────
		void Demo3DHostSetGizmoOp(int32 op) {
			auto *st = HostSt();
			if (!st)
				return;
			op &= 3;
			st->gizmo.SetMode(op);
			st->editGizmo.SetMode(op);
			st->lightGizmo.SetMode(op);
			st->emptyGizmo.SetMode(op);
		}
		int32 Demo3DHostGizmoOp() {
			auto *st = HostSt();
			if (!st)
				return 0;
			return (st->editMode ? st->editGizmo : st->gizmo).Mode();
		}
		void Demo3DHostSetOrientation(int32 o) {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.SetOrientation(o);
			st->editGizmo.SetOrientation(o);
			st->lightGizmo.SetOrientation(o);
		}
		int32 Demo3DHostOrientation() {
			auto *st = HostSt();
			if (!st)
				return 0;
			return (st->editMode ? st->editGizmo : st->gizmo).Orientation();
		}
		void Demo3DHostSetSnap(bool on, float32 t, float32 rotDeg, float32 scl) {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.SetSnapEnabled(on);
			st->editGizmo.SetSnapEnabled(on);
			st->lightGizmo.SetSnapEnabled(on);
			st->gizmo.SetSnapSteps(t, rotDeg, scl);
			st->editGizmo.SetSnapSteps(t, rotDeg, scl);
			st->lightGizmo.SetSnapSteps(t, rotDeg, scl);
		}
		bool Demo3DHostSnapEnabled() {
			auto *st = HostSt();
			return st && st->gizmo.IsSnapEnabled();
		}
		void Demo3DHostSetGizmoHidden(bool hidden) {
			nkvpGizmoHidden = hidden;
		}

		// ── Mode edition : sous-modes V/E/F, outils de zone, curseur ────────
		bool Demo3DHostInEditMode() {
			auto *st = HostSt();
			return st && st->editMode;
		}
		void Demo3DHostSetEditSelMask(int32 mask) {
			auto *st = HostSt();
			if (!st)
				return;
			mask &= 7;
			if (mask == 0)
				mask = 1;
			if (st->editSelMask != mask) {
				st->editSelMask = mask;
				st->editOverlayDirty = true; // meme ecriture que les touches 1/2/3
			}
		}
		int32 Demo3DHostEditSelMask() {
			auto *st = HostSt();
			return st ? st->editSelMask : 1;
		}
		void Demo3DHostSetZoneTool(int32 shape) {
			// shape : -1 = desarme, 0 = rectangle, 1 = cercle, 2 = lasso — le
			// selTool de la demo (1 rect / 2 lasso / 3 cercle) se re-arme apres
			// chaque geste : c'est ce qui transforme le one-shot de la touche B
			// en OUTIL persistant de barre, sans toucher au coeur.
			auto *st = HostSt();
			if (!st || st->selDragging)
				return;
			const int32 want = (shape < 0) ? 0 : (shape == 0 ? 1 : (shape == 1 ? 3 : 2));
			st->selTool = want;
		}
		void Demo3DHostSetCursorTool(bool on) {
			nkvpCursorTool = on;
		}

		// ── Surimpressions : grille, liseres, HUD ───────────────────────────
		void Demo3DHostSetGridFlags(bool grid, bool minor, bool major, bool axes) {
			auto *r3d = HostR3D();
			if (!r3d)
				return;
			nkvpGridOn = grid;
			r3d->SetInfiniteGridEnabled(grid);
			auto &g = r3d->GetInfiniteGridParams();
			g.showMinor = minor;
			g.showMajor = major;
			g.showAxes = axes;
			nkvpAxesOn = axes; // les axes debug de la demo suivent la meme case
		}
		void Demo3DHostGridFlags(bool *grid, bool *minor, bool *major, bool *axes) {
			auto *r3d = HostR3D();
			if (!r3d) {
				*grid = *minor = *major = *axes = true;
				return;
			}
			auto &g = r3d->GetInfiniteGridParams();
			*grid = nkvpGridOn; // la volonte, pas l'etat moteur (l'ortho le coupe)
			*minor = g.showMinor;
			*major = g.showMajor;
			*axes = g.showAxes;
		}
		void Demo3DHostSetOutline(bool on) {
			auto *r3d = HostR3D();
			if (!r3d)
				return;
			if (hostOutlineOn != on) {
				hostOutlineOn = on;
				r3d->SetSelectionOutline(on, {1.f, 0.45f, 0.05f, 1.f}, 3.f);
			}
		}
		bool Demo3DHostOutline() {
			return hostOutlineOn;
		}
		void Demo3DHostSetHud(bool on) {
			nkvpHudOn = on;
		}
		bool Demo3DHostHud() {
			return nkvpHudOn;
		}

		// ── Fond de la vue (le SetBackgroundColor du moteur, garde d'egalite) ─
		void Demo3DHostSetBackground(float32 r, float32 g, float32 b) {
			if (hst.ok)
				hst.ctx.renderer->SetBackgroundColor({r, g, b, 1.f});
		}

		// ── OBJETS DE LA SCENE (hierarchie, panneau Objet) ──────────────────
		// La scene de la demo : kNumObj cibles du gizmo + kNumLights lumieres.
		// Les noms sont derives des PLAGES D'INDICES de sa construction -- la
		// demo n'a pas de champ nom, et inventer un stockage parallele ici se
		// desynchroniserait ; les plages, elles, sont structurelles.
		int32 Demo3DHostObjectCount() {
			return hst.ok ? Demo3DState::kNumObj : 0;
		}
		void Demo3DHostObjectName(int32 i, char *out, uint32 cap) {
			if (!out || !cap)
				return;
			if (i < 16)
				snprintf(out, cap, "Sphere %02d", i + 1);
			else if (i == 16)
				snprintf(out, cap, "Cube central");
			else if (i <= 18)
				snprintf(out, cap, "Colonne %d", i - 16);
			else if (i <= 82)
				snprintf(out, cap, "Instance %02d", i - 18);
			else if (i == 83)
				snprintf(out, cap, "Sol");
			else if (i == 84)
				snprintf(out, cap, "Feuillage");
			else
				snprintf(out, cap, "Mur GI");
		}
		bool Demo3DHostObjectSelected(int32 i) {
			auto *st = HostSt();
			return st && st->gizmo.IsSelected(i);
		}
		int32 Demo3DHostActiveObject() {
			auto *st = HostSt();
			return st ? st->gizmo.ActiveIndex() : -1;
		}
		void Demo3DHostSelectObject(int32 i, bool additive) {
			auto *st = HostSt();
			if (!st)
				return;
			// lightSel seul ne suffit pas : le gizmo des lumieres le RESSUSCITE
			// chaque frame (lightSel = ActiveIndex) -- on vide sa selection.
			if (HostLockedEff(i))
				return; // CADENASSE (lui ou un ancetre) = INselectionnable
			st->lightGizmo.ClearSelection();
			st->emptyGizmo.ClearSelection();
			st->lightSel = -1;
			if (additive)
				st->gizmo.ToggleSelection(i); // Maj+clic, comme dans la vue
			else
				st->gizmo.Select(i);
		}
		// SELECTION DE GROUPE (parent -> enfants) : le gizmo transforme deja
		// une multi-selection autour du pivot commun -- selectionner le parent,
		// c'est selectionner tous ses enfants d'un coup, comme Unity. Le vrai
		// re-parentage libre viendra avec le format projet.
		void Demo3DHostSelectGroup(int32 start, int32 count, bool additive) {
			auto *st = HostSt();
			if (!st)
				return;
			st->lightGizmo.ClearSelection(); // meme regle que Demo3DHostSelectObject
			st->emptyGizmo.ClearSelection();
			st->lightSel = -1;
			if (!additive)
				st->gizmo.ClearSelection();
			for (int32 k = 0; k < count; ++k) {
				const int32 i = start + k;
				if (i >= 0 && i < Demo3DState::kNumObj && !HostLockedEff(i) &&
					!st->gizmo.IsSelected(i))
					st->gizmo.ToggleSelection(i);
			}
		}
		// Le PARENT « Lumieres » se selectionne aussi : toutes les lumieres
		// entrent dans le gizmo des lumieres (multi-selection) et se
		// transforment ensemble. Le parentage LIBRE entre natures differentes
		// (une lumiere enfant d'un maillage...) viendra avec le format projet.
		void Demo3DHostSelectAllLights() {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.ClearSelection();
			st->emptyGizmo.ClearSelection();
			st->lightGizmo.SelectAll();
			st->lightSel = st->lightGizmo.ActiveIndex();
		}
		bool Demo3DHostAllLightsSelected() {
			auto *st = HostSt();
			if (!st)
				return false;
			for (int32 li = 0; li < Demo3DState::kNumLights; ++li)
				if (!st->lightGizmo.IsSelected(li))
					return false;
			return true;
		}
		void Demo3DHostDeselectAll() {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.ClearSelection();
			st->lightGizmo.ClearSelection(); // sinon lightSel renait a la frame suivante
			st->emptyGizmo.ClearSelection();
			st->lightSel = -1;
		}
		void Demo3DHostObjectPosition(int32 i, float32 *out3) {
			auto *st = HostSt();
			if (!st || i < 0 || i >= Demo3DState::kNumObj) {
				out3[0] = out3[1] = out3[2] = 0.f;
				return;
			}
			const NkVec3f p = st->objXform[i] * NkVec3f{0.f, 0.f, 0.f};
			out3[0] = p.x;
			out3[1] = p.y;
			out3[2] = p.z;
		}
		int32 Demo3DHostLightCount() {
			return hst.ok ? Demo3DState::kNumLights : 0;
		}
		void Demo3DHostLightName(int32 li, char *out, uint32 cap) {
			static const char *const kNames[4] = {"Soleil", "Ponctuelle rouge", "Ponctuelle bleue",
												  "Projecteur"};
			if (out && cap)
				snprintf(out, cap, "%s", (li >= 0 && li < 4) ? kNames[li] : "Lumiere");
		}
		int32 Demo3DHostSelectedLight() {
			auto *st = HostSt();
			return st ? st->lightSel : -1;
		}
		void Demo3DHostSelectLight(int32 li) {
			auto *st = HostSt();
			if (!st)
				return;
			if (li >= 0 && HostLockedEff(86 + li))
				return; // lumiere cadenassee (elle ou un ancetre)
			st->gizmo.ClearSelection();
			st->emptyGizmo.ClearSelection();
			// PAS lightSel directement : la demo le REECRIT chaque frame depuis
			// la selection interne du gizmo des lumieres (lightSel =
			// lightGizmo.ActiveIndex()). C'est donc LE GIZMO qu'on pilote --
			// lightSel suit tout seul, et la selection SURVIT.
			if (li >= 0 && li < Demo3DState::kNumLights)
				st->lightGizmo.Select(li);
			else
				st->lightGizmo.ClearSelection();
			st->lightSel = (li >= 0 && li < Demo3DState::kNumLights) ? li : -1;
		}
		void Demo3DHostSetLightPosition(int32 li, const float32 *xyz) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights)
				return;
			// La BASE : l'effective = base + decalages du gizmo, et la demo
			// replie les decalages dans la base en fin de drag.
			st->lights[li].position = {xyz[0], xyz[1], xyz[2]};
		}
		int32 Demo3DHostLightType(int32 li) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights)
				return 1;
			return (int32)st->lights[li].type & 3; // 0 dir, 1 point, 2 spot, 3 area
		}
		void Demo3DHostLightDir(int32 li, float32 *out3) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights) {
				out3[0] = 0.f;
				out3[1] = -1.f;
				out3[2] = 0.f;
				return;
			}
			const NkVec3f d = st->lights[li].direction;
			out3[0] = d.x;
			out3[1] = d.y;
			out3[2] = d.z;
		}
		void Demo3DHostSetLightDir(int32 li, const float32 *xyz) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights)
				return;
			NkVec3f d{xyz[0], xyz[1], xyz[2]};
			if (d.Len() < 1e-5f)
				d = {0.f, -1.f, 0.f};
			st->lights[li].direction = d.Normalized();
		}
		void Demo3DHostLightParams(int32 li, float32 *color3, float32 *intensity) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights) {
				color3[0] = color3[1] = color3[2] = 1.f;
				*intensity = 1.f;
				return;
			}
			color3[0] = st->lights[li].color.x;
			color3[1] = st->lights[li].color.y;
			color3[2] = st->lights[li].color.z;
			*intensity = st->lights[li].intensity;
		}
		void Demo3DHostSetLightParams(int32 li, const float32 *color3, float32 intensity) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights)
				return;
			st->lights[li].color = {color3[0], color3[1], color3[2]};
			st->lights[li].intensity = intensity;
		}
		void Demo3DHostLightPosition(int32 li, float32 *out3) {
			auto *st = HostSt();
			if (!st || li < 0 || li >= Demo3DState::kNumLights) {
				out3[0] = out3[1] = out3[2] = 0.f;
				return;
			}
			const NkLightDesc L = Demo3D_LightEffective(st, li);
			out3[0] = L.position.x;
			out3[1] = L.position.y;
			out3[2] = L.position.z;
		}

		// ── REGLAGES DE VUE (panneau Scene) ─────────────────────────────────
		void Demo3DHostSetViewFar(float32 f) {
			nkvpFarOverride = f;
		}
		float32 Demo3DHostViewFar() {
			return nkvpFarOverride;
		}
		void Demo3DHostSetOrthoScale(float32 f) {
			if (f > 0.05f && f < 5.f)
				nkvpOrthoScale = f;
		}
		float32 Demo3DHostOrthoScale() {
			return nkvpOrthoScale;
		}
		void Demo3DHostSetGridExtent(int32 n) {
			// Jusqu'a 2000 : « l'etendue doit pouvoir etre infinie » -- 2000
			// demi-unites couvrent tout ce qu'on voit, sans faire exploser le
			// nombre de lignes debug par image.
			if (n >= 5 && n <= 2000)
				nkvpGridExtent = n;
		}
		int32 Demo3DHostGridExtent() {
			return nkvpGridExtent;
		}
		// CURSEUR <-> SELECTION, en mode objet comme en edition (le pivot de
		// la selection d'edition prime quand il existe).
		void Demo3DHostCursorToSelection() {
			auto *st = HostSt();
			if (!st)
				return;
			if (st->editMode && st->editGizmo.HasSelection())
				st->cursor3D = st->editGizmo.GetPivot();
			else if (st->gizmo.HasSelection())
				st->cursor3D = st->gizmo.GetPivot();
		}
		void Demo3DHostSelectionToCursor() {
			auto *st = HostSt();
			if (!st || !st->gizmo.HasSelection())
				return;
			// Mode OBJET : la selection SAUTE au curseur (delta commun sur les
			// decalages). L'equivalent en edition passera par le cadre modal.
			const NkVec3f piv = st->gizmo.GetPivot();
			const NkVec3f d = {st->cursor3D.x - piv.x, st->cursor3D.y - piv.y,
							   st->cursor3D.z - piv.z};
			for (int32 i = 0; i < Demo3DState::kNumObj; ++i) {
				if (!st->gizmo.IsSelected(i) || nkvpObjLocked[i])
					continue;
				const NkVec3f t = st->gizmo.TranslateOf(i);
				st->gizmo.SetTranslateOf(i, {t.x + d.x, t.y + d.y, t.z + d.z});
			}
		}
		void Demo3DHostResetCursor() {
			if (auto *st = HostSt())
				st->cursor3D = {0.f, 0.f, 0.f}; // la meme ecriture que Alt+.
		}
		void Demo3DHostClearXform(int32 which) {
			auto *st = HostSt();
			if (!st)
				return;
			// L'« Appliquer » de l'outil : remet la composante de la SELECTION,
			// exactement Alt+G / Alt+R / Alt+S de la demo -- sur le gizmo ACTIF.
			renderer::NkGizmo3D &G = st->editMode ? st->editGizmo : st->gizmo;
			if (which == 0)
				G.ClearSelectedTranslate();
			else if (which == 1)
				G.ClearSelectedRotation();
			else
				G.ClearSelectedScale();
		}

		// ── OEIL / CADENAS / SCENE VIERGE ───────────────────────────────────
		void Demo3DHostSetObjectHidden(int32 i, bool hidden) {
			if (i >= 0 && i < 160)
				nkvpObjHidden[i] = hidden;
		}
		bool Demo3DHostObjectHidden(int32 i) {
			return (i >= 0 && i < 160) && nkvpObjHidden[i];
		}
		void Demo3DHostSetObjectLocked(int32 i, bool locked) {
			if (i >= 0 && i < 160)
				nkvpObjLocked[i] = locked;
		}
		bool Demo3DHostObjectLocked(int32 i) {
			return (i >= 0 && i < 160) && nkvpObjLocked[i];
		}
		void Demo3DHostSetLightHidden(int32 li, bool hidden) {
			if (li >= 0 && li < 8)
				nkvpLightHidden[li] = hidden;
		}
		bool Demo3DHostLightHidden(int32 li) {
			return (li >= 0 && li < 8) && nkvpLightHidden[li];
		}
		void Demo3DHostSetAllHidden(bool hidden) {
			// La SCENE VIERGE d'un nouvel onglet : tout est masque d'un coup.
			for (int32 i = 0; i < 160; ++i)
				nkvpObjHidden[i] = hidden;
			for (int32 i = 0; i < 8; ++i)
				nkvpLightHidden[i] = hidden;
		}

		// ── TRANSFORMATION DE L'OBJET ACTIF (panneau Proprietes) ────────────
		// Lecture : decomposition du MONDE reellement rendu (objXform), la meme
		// que l'ancienne vue (colonne-majeur, R = Rz*Ry*Rx). Ecriture :
		// INCREMENTALE sur les decalages du gizmo -- position exacte, rotation
		// par delta d'angles, echelle par rapport d'axes ; aucune inversion de
		// matrice n'est necessaire.
		static void HostDecompose(const NkMat4f &M, NkVec3f &pos, NkVec3f &rotDeg, NkVec3f &scl) {
			pos = {M.mat[3][0], M.mat[3][1], M.mat[3][2]};
			const NkVec3f c0{M.mat[0][0], M.mat[0][1], M.mat[0][2]};
			const NkVec3f c1{M.mat[1][0], M.mat[1][1], M.mat[1][2]};
			const NkVec3f c2{M.mat[2][0], M.mat[2][1], M.mat[2][2]};
			scl = {c0.Len(), c1.Len(), c2.Len()};
			const float32 sx = scl.x > 1e-8f ? 1.f / scl.x : 0.f;
			const float32 sy = scl.y > 1e-8f ? 1.f / scl.y : 0.f;
			const float32 sz = scl.z > 1e-8f ? 1.f / scl.z : 0.f;
			const float32 r00 = M.mat[0][0] * sx, r01 = M.mat[0][1] * sx, r02 = M.mat[0][2] * sx;
			const float32 r12 = M.mat[1][2] * sy, r22 = M.mat[2][2] * sz;
			float32 syn = -r02;
			if (syn < -1.f)
				syn = -1.f;
			if (syn > 1.f)
				syn = 1.f;
			const float32 kRad2Deg = 57.29577951f;
			rotDeg.y = asinf(syn) * kRad2Deg;
			rotDeg.x = atan2f(r12, r22) * kRad2Deg;
			rotDeg.z = atan2f(r01, r00) * kRad2Deg;
		}
		// ── PARENTE DE SCENE : helpers ──────────────────────────────────────
		static void HostParentEnsureInit() {
			if (nkvpParentInit)
				return;
			nkvpParentInit = true;
			for (int32 i = 0; i < kNkvpMaxNodes; ++i) {
				nkvpParentOf[i] = -1;
				nkvpXmit[i] = 7; // tout se transmet par defaut
			}
			// Les familles historiques deviennent de VRAIS EMPTIES (un groupe
			// sans identite de scene est un empty, regle de Rihen) : 90 Spheres,
			// 91 Cube central, 92 Colonnes, 93 Instances, 94 Decor, 95 Lumieres.
			for (int32 i = 0; i <= 15; ++i)
				nkvpParentOf[i] = 90;
			nkvpParentOf[16] = 91;
			nkvpParentOf[17] = 92;
			nkvpParentOf[18] = 92;
			for (int32 i = 19; i <= 82; ++i)
				nkvpParentOf[i] = 93;
			for (int32 i = 83; i <= 85; ++i)
				nkvpParentOf[i] = 94;
			for (int32 i = 86; i <= 89; ++i)
				nkvpParentOf[i] = 95;
		}
		static bool HostIsDescendant(int32 node, int32 anc) {
			// node est-il DANS le sous-arbre de anc ? (garde anti-cycle)
			int32 cur = node;
			for (int32 guard = 0; guard < kNkvpMaxNodes && cur >= 0; ++guard) {
				if (cur == anc)
					return true;
				cur = nkvpParentOf[cur];
			}
			return false;
		}
		int32 Demo3DHostNodeCount() {
			return kNkvpMaxNodes;
		}
		int32 Demo3DHostNodeParent(int32 node) {
			HostParentEnsureInit();
			return (node >= 0 && node < kNkvpMaxNodes) ? nkvpParentOf[node] : -1;
		}
		bool Demo3DHostSetNodeParent(int32 child, int32 parent) {
			HostParentEnsureInit();
			if (child < 0 || child >= kNkvpMaxNodes || parent < -1 || parent >= kNkvpMaxNodes)
				return false;
			// Refus des CYCLES : on ne parente pas a soi-meme ni a un de ses
			// propres descendants.
			if (parent >= 0 && HostIsDescendant(parent, child))
				return false;
			nkvpParentOf[child] = parent;
			return true;
		}
		int32 Demo3DHostNodeXmitMask(int32 node) {
			HostParentEnsureInit();
			return (node >= 0 && node < kNkvpMaxNodes) ? (int32)nkvpXmit[node] : 7;
		}
		void Demo3DHostSetNodeXmitMask(int32 node, int32 mask) {
			HostParentEnsureInit();
			if (node >= 0 && node < kNkvpMaxNodes)
				nkvpXmit[node] = (uint8)(mask & 7);
		}
		bool Demo3DHostNodeHasChildren(int32 node) {
			HostParentEnsureInit();
			for (int32 i = 0; i < kNkvpMaxNodes; ++i)
				if (nkvpParentOf[i] == node)
					return true;
			return false;
		}
		static NkMat4f HostRotFromEuler(const float32 *rotDeg);
		void Demo3DHostSelectEmptyNode(int32 node) {
			auto *st = HostSt();
			if (!st)
				return;
			if (node < kNkvpFirstEmpty || node >= kNkvpMaxNodes) {
				st->emptyGizmo.ClearSelection();
				return;
			}
			if (HostLockedEff(node))
				return; // empty cadenasse (lui ou un ancetre)
			if (nkvpDeleted[node] ||
				(node >= kNkvpFirstUser && nkvpUserKind[node - kNkvpFirstUser] == 0))
				return; // supprime ou slot libre
			st->gizmo.ClearSelection();
			st->lightGizmo.ClearSelection();
			st->lightSel = -1;
			st->emptyGizmo.Select(node - kNkvpFirstEmpty);
		}
		void Demo3DHostToggleEmptyNode(int32 node) {
			auto *st = HostSt();
			if (!st || node < kNkvpFirstEmpty || node >= kNkvpMaxNodes)
				return;
			if (HostLockedEff(node) || nkvpDeleted[node] ||
				(node >= kNkvpFirstUser && nkvpUserKind[node - kNkvpFirstUser] == 0))
				return;
			st->gizmo.ClearSelection();
			st->lightGizmo.ClearSelection();
			st->lightSel = -1;
			st->emptyGizmo.ToggleSelection(node - kNkvpFirstEmpty);
		}
		bool Demo3DHostEmptyNodeSelected(int32 node) {
			auto *st = HostSt();
			return st && node >= kNkvpFirstEmpty && node < kNkvpMaxNodes &&
				   st->emptyGizmo.IsSelected(node - kNkvpFirstEmpty);
		}
		int32 Demo3DHostSelectedEmptyNode() {
			auto *st = HostSt();
			if (!st)
				return -1;
			const int32 e = st->emptyGizmo.ActiveIndex();
			return e >= 0 ? kNkvpFirstEmpty + e : -1;
		}
		bool Demo3DHostEmptyTransform(int32 node, float32 *pos3, float32 *rotDeg3, float32 *scl3) {
			if (node < kNkvpFirstEmpty || node >= kNkvpMaxNodes)
				return false;
			const int32 e = node - kNkvpFirstEmpty;
			for (int32 a = 0; a < 3; ++a) {
				pos3[a] = nkvpEmptyPos[e][a];
				rotDeg3[a] = nkvpEmptyRotDeg[e][a];
				scl3[a] = nkvpEmptyScl[e][a];
			}
			// EFFECTIF : les decalages d'un drag du gizmo en cours s'ajoutent.
			auto *st = HostSt();
			if (st) {
				const NkVec3f tr = st->emptyGizmo.TranslateOf(e);
				pos3[0] += tr.x;
				pos3[1] += tr.y;
				pos3[2] += tr.z;
				NkVec3f gp, gr, gs;
				HostDecompose(st->emptyGizmo.RotationOf(e) * HostRotFromEuler(nkvpEmptyRotDeg[e]),
							  gp, gr, gs);
				rotDeg3[0] = gr.x;
				rotDeg3[1] = gr.y;
				rotDeg3[2] = gr.z;
				const NkVec3f os = st->emptyGizmo.ScaleOf(e);
				scl3[0] *= (1.f + os.x);
				scl3[1] *= (1.f + os.y);
				scl3[2] *= (1.f + os.z);
			}
			return true;
		}
		void Demo3DHostSetEmptyTransform(int32 node, const float32 *pos3, const float32 *rotDeg3,
										 const float32 *scl3) {
			if (node < kNkvpFirstEmpty || node >= kNkvpMaxNodes)
				return;
			const int32 e = node - kNkvpFirstEmpty;
			// INCREMENTAL sur la BASE, delta calcule contre l'EFFECTIF (le gizmo
			// peut porter des decalages en plein drag) -- meme logique que l'objet.
			float32 cp[3], cr[3], cs[3];
			Demo3DHostEmptyTransform(node, cp, cr, cs);
			for (int32 a = 0; a < 3; ++a)
				nkvpEmptyPos[e][a] += pos3[a] - cp[a];
			const float32 kD2R = 0.017453292f;
			const float32 ddx = (rotDeg3[0] - cr[0]) * kD2R;
			const float32 ddy = (rotDeg3[1] - cr[1]) * kD2R;
			const float32 ddz = (rotDeg3[2] - cr[2]) * kD2R;
			if (fabsf(ddx) + fabsf(ddy) + fabsf(ddz) > 1e-7f) {
				const NkMat4f dR = NkMat4f::RotationZ(NkAngle::FromRad(ddz)) *
								   NkMat4f::RotationY(NkAngle::FromRad(ddy)) *
								   NkMat4f::RotationX(NkAngle::FromRad(ddx));
				NkVec3f bp, br, bs;
				HostDecompose(dR * HostRotFromEuler(nkvpEmptyRotDeg[e]), bp, br, bs);
				nkvpEmptyRotDeg[e][0] = br.x;
				nkvpEmptyRotDeg[e][1] = br.y;
				nkvpEmptyRotDeg[e][2] = br.z;
			}
			for (int32 a = 0; a < 3; ++a)
				if (cs[a] > 1e-6f && fabsf(scl3[a] - cs[a]) > 1e-7f)
					nkvpEmptyScl[e][a] *= scl3[a] / cs[a];
		}
		// ── Materiau par objet (surcharges + cache des valeurs effectives) ──
		bool Demo3DHostMeshMaterial(int32 i, float32 *tint3, float32 *metallic, float32 *roughness) {
			if (i < 0 || i >= kNkvpMaxNodes)
				return false;
			tint3[0] = nkvpMatCache[i][0];
			tint3[1] = nkvpMatCache[i][1];
			tint3[2] = nkvpMatCache[i][2];
			*metallic = nkvpMatCache[i][3];
			*roughness = nkvpMatCache[i][4];
			return true;
		}
		void Demo3DHostSetMeshTint(int32 i, const float32 *rgb3) {
			if (i < 0 || i >= kNkvpMaxNodes)
				return;
			nkvpMatMask[i] |= 1;
			nkvpMatTint[i][0] = rgb3[0];
			nkvpMatTint[i][1] = rgb3[1];
			nkvpMatTint[i][2] = rgb3[2];
		}
		void Demo3DHostSetMeshMetalRough(int32 i, float32 metallic, float32 roughness) {
			if (i < 0 || i >= kNkvpMaxNodes)
				return;
			nkvpMatMask[i] |= 2 | 4;
			nkvpMatMetal[i] = metallic;
			nkvpMatRough[i] = roughness;
		}
		void Demo3DHostResetMeshMat(int32 i) {
			if (i >= 0 && i < kNkvpMaxNodes)
				nkvpMatMask[i] = 0;
		}
		// ── DETECTEUR DE PARENTE (une passe par frame) ──────────────────────
		// Il observe la transformation MONDE de chaque noeud ; tout delta d'un
		// parent (gizmo, panneau, raccourcis G/R/S de la demo, animation) est
		// repercute a ses enfants avec la semantique ORBITE : l'enfant tourne
		// et s'ecarte AUTOUR du parent. Un seul point de passage pour toutes
		// les sources de transformation -- aucune n'a a connaitre la parente.
		static float32 sHierPos[kNkvpMaxNodes][3];
		static NkMat4f sHierRot[kNkvpMaxNodes];
		static float32 sHierScl[kNkvpMaxNodes][3];
		static bool sHierOk = false;
		static NkMat4f HostRotFromEuler(const float32 *rotDeg) {
			const float32 kDeg2Rad = 0.017453292f;
			return NkMat4f::RotationZ(NkAngle::FromRad(rotDeg[2] * kDeg2Rad)) *
				   NkMat4f::RotationY(NkAngle::FromRad(rotDeg[1] * kDeg2Rad)) *
				   NkMat4f::RotationX(NkAngle::FromRad(rotDeg[0] * kDeg2Rad));
		}
		static NkMat4f HostRotTranspose(const NkMat4f &m) {
			NkMat4f t = NkMat4f::Identity();
			for (int32 c = 0; c < 3; ++c)
				for (int32 r = 0; r < 3; ++r)
					t.mat[c][r] = m.mat[r][c];
			return t;
		}
		// Etat MONDE d'un noeud : position, rotation pure (colonnes normees),
		// echelle. Lumieres : position effective (base + decalage du gizmo).
		static void HostNodeWorld(Demo3DState *st, int32 n, float32 *pos, NkMat4f &rot, float32 *scl) {
			rot = NkMat4f::Identity();
			scl[0] = scl[1] = scl[2] = 1.f;
			if (n < Demo3DState::kNumObj) {
				const NkMat4f &M = st->objXform[n];
				pos[0] = M.mat[3][0];
				pos[1] = M.mat[3][1];
				pos[2] = M.mat[3][2];
				for (int32 c = 0; c < 3; ++c) {
					const float32 lx = M.mat[c][0], ly = M.mat[c][1], lz = M.mat[c][2];
					const float32 len = sqrtf(lx * lx + ly * ly + lz * lz);
					scl[c] = len;
					const float32 inv = len > 1e-8f ? 1.f / len : 0.f;
					rot.mat[c][0] = lx * inv;
					rot.mat[c][1] = ly * inv;
					rot.mat[c][2] = lz * inv;
				}
			} else if (n < kNkvpFirstEmpty) {
				const int32 li = n - Demo3DState::kNumObj;
				const NkVec3f base = st->lights[li].position;
				const NkVec3f off = st->lightGizmo.TranslateOf(li);
				pos[0] = base.x + off.x;
				pos[1] = base.y + off.y;
				pos[2] = base.z + off.z;
			} else {
				// EFFECTIF : base + decalages du gizmo des empties -> pendant un
				// drag, les enfants suivent EN DIRECT.
				const int32 e = n - kNkvpFirstEmpty;
				const NkVec3f tr = st->emptyGizmo.TranslateOf(e);
				pos[0] = nkvpEmptyPos[e][0] + tr.x;
				pos[1] = nkvpEmptyPos[e][1] + tr.y;
				pos[2] = nkvpEmptyPos[e][2] + tr.z;
				rot = st->emptyGizmo.RotationOf(e) * HostRotFromEuler(nkvpEmptyRotDeg[e]);
				const NkVec3f os = st->emptyGizmo.ScaleOf(e);
				scl[0] = nkvpEmptyScl[e][0] * (1.f + os.x);
				scl[1] = nkvpEmptyScl[e][1] * (1.f + os.y);
				scl[2] = nkvpEmptyScl[e][2] * (1.f + os.z);
			}
		}
		static bool HostNodeSelected(Demo3DState *st, int32 n) {
			if (n < Demo3DState::kNumObj)
				return st->gizmo.IsSelected(n);
			if (n < kNkvpFirstEmpty)
				return st->lightGizmo.IsSelected(n - Demo3DState::kNumObj);
			return false; // la selection des empties vit dans le shell
		}
		// Fait SUIVRE a l'enfant c le delta de son parent : orbite autour du
		// pivot (position du parent avant le geste), rotation et echelle
		// composees -- memes ecritures incrementales que le panneau.
		static void HostFollowParent(Demo3DState *st, int32 c, const float32 *pPrev,
									 const float32 *pCur, const NkMat4f &dR, bool hasRot,
									 const float32 *ratio, bool hasScl) {
			float32 cpos[3], cscl[3];
			NkMat4f crot;
			HostNodeWorld(st, c, cpos, crot, cscl);
			float32 rel[3] = {(cpos[0] - pPrev[0]) * ratio[0], (cpos[1] - pPrev[1]) * ratio[1],
							  (cpos[2] - pPrev[2]) * ratio[2]};
			float32 tgt[3];
			for (int32 rr2 = 0; rr2 < 3; ++rr2)
				tgt[rr2] = pCur[rr2] + dR.mat[0][rr2] * rel[0] + dR.mat[1][rr2] * rel[1] +
						   dR.mat[2][rr2] * rel[2];
			const float32 dp[3] = {tgt[0] - cpos[0], tgt[1] - cpos[1], tgt[2] - cpos[2]};
			if (c < Demo3DState::kNumObj) {
				renderer::NkGizmo3D &G = st->gizmo;
				const NkVec3f t = G.TranslateOf(c);
				G.SetTranslateOf(c, {t.x + dp[0], t.y + dp[1], t.z + dp[2]});
				if (hasRot)
					G.SetRotationOf(c, dR * G.RotationOf(c));
				if (hasScl) {
					NkVec3f sv = G.ScaleOf(c);
					sv.x = (1.f + sv.x) * ratio[0] - 1.f;
					sv.y = (1.f + sv.y) * ratio[1] - 1.f;
					sv.z = (1.f + sv.z) * ratio[2] - 1.f;
					G.SetScaleOf(c, sv);
				}
			} else if (c < kNkvpFirstEmpty) {
				// Lumiere : la POSITION suit (l'orientation d'un spot viendra
				// avec le format projet).
				const int32 li = c - Demo3DState::kNumObj;
				st->lights[li].position.x += dp[0];
				st->lights[li].position.y += dp[1];
				st->lights[li].position.z += dp[2];
			} else {
				const int32 e = c - kNkvpFirstEmpty;
				nkvpEmptyPos[e][0] += dp[0];
				nkvpEmptyPos[e][1] += dp[1];
				nkvpEmptyPos[e][2] += dp[2];
				if (hasRot) {
					NkMat4f nr = dR * HostRotFromEuler(nkvpEmptyRotDeg[e]);
					NkVec3f p2, r2, s2;
					HostDecompose(nr, p2, r2, s2);
					nkvpEmptyRotDeg[e][0] = r2.x;
					nkvpEmptyRotDeg[e][1] = r2.y;
					nkvpEmptyRotDeg[e][2] = r2.z;
				}
				if (hasScl) {
					nkvpEmptyScl[e][0] *= ratio[0];
					nkvpEmptyScl[e][1] *= ratio[1];
					nkvpEmptyScl[e][2] *= ratio[2];
				}
			}
		}
		static void HostHierRecurse(Demo3DState *st, int32 pnode) {
			float32 cpos[3], cscl[3];
			NkMat4f crot;
			HostNodeWorld(st, pnode, cpos, crot, cscl);
			const float32 *ppos = sHierPos[pnode];
			const float32 dp[3] = {cpos[0] - ppos[0], cpos[1] - ppos[1], cpos[2] - ppos[2]};
			const bool hasPos =
				fabsf(dp[0]) + fabsf(dp[1]) + fabsf(dp[2]) > 1e-5f;
			const NkMat4f dR = crot * HostRotTranspose(sHierRot[pnode]);
			float32 offDiag = 0.f;
			for (int32 c2 = 0; c2 < 3; ++c2)
				for (int32 r2 = 0; r2 < 3; ++r2)
					offDiag += fabsf(dR.mat[c2][r2] - (c2 == r2 ? 1.f : 0.f));
			const bool hasRot = offDiag > 1e-4f;
			float32 ratio[3];
			bool hasScl = false;
			for (int32 a = 0; a < 3; ++a) {
				const float32 prev = sHierScl[pnode][a];
				ratio[a] = prev > 1e-6f ? cscl[a] / prev : 1.f;
				if (fabsf(ratio[a] - 1.f) > 1e-5f)
					hasScl = true;
			}
			// MASQUE DE TRANSMISSION du parent : une composante eteinte est
			// RETENUE -- elle n'atteint jamais les enfants.
			const uint8 xm = nkvpXmit[pnode];
			const bool xPos = hasPos && (xm & 1) != 0;
			const bool xRot = hasRot && (xm & 2) != 0;
			const bool xScl = hasScl && (xm & 4) != 0;
			const NkMat4f dRm = xRot ? dR : NkMat4f::Identity();
			const float32 ratiom[3] = {xScl ? ratio[0] : 1.f, xScl ? ratio[1] : 1.f,
										   xScl ? ratio[2] : 1.f};
			const float32 pCurm[3] = {xPos ? cpos[0] : ppos[0], xPos ? cpos[1] : ppos[1],
										  xPos ? cpos[2] : ppos[2]};
			const bool moved = xPos || xRot || xScl;
			const bool selP = HostNodeSelected(st, pnode);
			for (int32 c = 0; c < kNkvpMaxNodes; ++c) {
				if (nkvpParentOf[c] != pnode)
					continue;
				// Un enfant DEJA emporte par le meme geste (parent et enfant
				// selectionnes ensemble) ne doit pas etre deplace deux fois.
				if (moved && !(selP && HostNodeSelected(st, c)))
					HostFollowParent(st, c, ppos, pCurm, dRm, xRot, ratiom, xScl);
				HostHierRecurse(st, c);
			}
		}
		static void HostHierarchyFrame() {
			auto *st = HostSt();
			if (!st)
				return;
			HostParentEnsureInit();
			// CADENAS INVIOLABLE : quel que soit le chemin (clic vue, zone,
			// panneau), un objet verrouille est desselectionne d'office.
			for (int32 i = 0; i < Demo3DState::kNumObj; ++i)
				if ((HostLockedEff(i) || nkvpDeleted[i]) && st->gizmo.IsSelected(i))
					st->gizmo.ToggleSelection(i);
			for (int32 li2 = 0; li2 < Demo3DState::kNumLights; ++li2)
				if ((HostLockedEff(86 + li2) || nkvpDeleted[86 + li2]) &&
					st->lightGizmo.IsSelected(li2))
					st->lightGizmo.ToggleSelection(li2);
			if (!sHierOk) {
				sHierOk = true;
			} else {
				for (int32 n = 0; n < kNkvpMaxNodes; ++n)
					if (nkvpParentOf[n] < 0)
						HostHierRecurse(st, n);
			}
			// RACCOURCIS par polling : fronts de D/C/V/X/P/Suppr avec les
			// modificateurs, hors saisie de texte et hors drag de gizmo (X y
			// verrouille un axe).
			if (nkvpInputOn) {
				const bool sh2 = NkInput.IsKeyDown(NkKey::NK_LSHIFT) ||
								 NkInput.IsKeyDown(NkKey::NK_RSHIFT);
				const bool ct2 = NkInput.IsKeyDown(NkKey::NK_LCTRL) ||
								 NkInput.IsKeyDown(NkKey::NK_RCTRL);
				uint8 now2 = 0;
				if (NkInput.IsKeyDown(NkKey::NK_D))
					now2 |= 1;
				if (NkInput.IsKeyDown(NkKey::NK_C))
					now2 |= 2;
				if (NkInput.IsKeyDown(NkKey::NK_V))
					now2 |= 4;
				if (NkInput.IsKeyDown(NkKey::NK_X))
					now2 |= 8;
				if (NkInput.IsKeyDown(NkKey::NK_P))
					now2 |= 16;
				if (NkInput.IsKeyDown(NkKey::NK_DELETE))
					now2 |= 32;
				const uint8 fresh = (uint8)(now2 & (uint8)~nkvpShortcutPrev);
				nkvpShortcutPrev = now2;
				const bool anyDrag = st->gizmo.IsDragging() || st->lightGizmo.IsDragging() ||
									 st->emptyGizmo.IsDragging();
				if ((fresh & 1) && sh2)
					nkvpShortcutBits |= 1;
				if ((fresh & 2) && ct2)
					nkvpShortcutBits |= 2;
				if ((fresh & 4) && ct2)
					nkvpShortcutBits |= 4;
				if (((fresh & 8) && !anyDrag && !ct2) || (fresh & 32))
					nkvpShortcutBits |= 8;
				if ((fresh & 16) && ct2)
					nkvpShortcutBits |= 16;
				if ((fresh & 16) && sh2 && !ct2)
					nkvpShortcutBits |= 32;
			}
			// Cliche de fin : l'etat APRES propagation devient la reference de
			// la frame suivante.
			for (int32 n = 0; n < kNkvpMaxNodes; ++n)
				HostNodeWorld(st, n, sHierPos[n], sHierRot[n], sHierScl[n]);
		}
		// ── SUPPRESSION / DUPLICATION / PRESSE-PAPIERS ──────────────────────
		bool Demo3DHostNodeDeleted(int32 node) {
			return node >= 0 && node < kNkvpMaxNodes && nkvpDeleted[node];
		}
		static void HostDeselectNode(Demo3DState *st, int32 n) {
			if (n < Demo3DState::kNumObj) {
				if (st->gizmo.IsSelected(n))
					st->gizmo.ToggleSelection(n);
			} else if (n < kNkvpFirstEmpty) {
				if (st->lightGizmo.IsSelected(n - Demo3DState::kNumObj))
					st->lightGizmo.ToggleSelection(n - Demo3DState::kNumObj);
				st->lightSel = st->lightGizmo.ActiveIndex();
			} else if (st->emptyGizmo.ActiveIndex() == n - kNkvpFirstEmpty) {
				st->emptyGizmo.ClearSelection();
			}
		}
		void Demo3DHostDeleteNode(int32 node, bool withChildren) {
			HostParentEnsureInit();
			if (node < 0 || node >= kNkvpMaxNodes || nkvpDeleted[node])
				return;
			nkvpDeleted[node] = true;
			if (node >= kNkvpFirstUser)
				nkvpUserKind[node - kNkvpFirstUser] = 0; // slot recyclable
			auto *st = HostSt();
			if (st)
				HostDeselectNode(st, node);
			for (int32 c = 0; c < kNkvpMaxNodes; ++c) {
				if (nkvpParentOf[c] != node || nkvpDeleted[c])
					continue;
				if (withChildren)
					Demo3DHostDeleteNode(c, true); // le sous-arbre part avec (Rihen)
				else
					nkvpParentOf[c] = nkvpParentOf[node]; // remonte d'un cran
			}
		}
		static int32 HostAllocUser(uint8 kind) {
			HostParentEnsureInit();
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				if (nkvpUserKind[u] != 0)
					continue;
				const int32 n = kNkvpFirstUser + u;
				nkvpUserKind[u] = kind;
				nkvpDeleted[n] = false;
				nkvpParentOf[n] = -1;
				nkvpXmit[n] = 7;
				nkvpMatMask[n] = 0;
				nkvpObjHidden[n] = false;
				nkvpObjLocked[n] = false;
				nkvpBaseSet[n] = false;
				const int32 e = n - kNkvpFirstEmpty;
				for (int32 a = 0; a < 3; ++a) {
					nkvpEmptyPos[e][a] = 0.f;
					nkvpEmptyRotDeg[e][a] = 0.f;
					nkvpEmptyScl[e][a] = 1.f;
				}
				return n;
			}
			return -1;
		}
		// Nature GEOMETRIQUE d'un noeud (pour duplication/collage).
		static uint8 HostKindOf(int32 node) {
			if (node >= kNkvpFirstUser)
				return nkvpUserKind[node - kNkvpFirstUser];
			if (node >= kNkvpFirstEmpty)
				return 4; // empty
			if (node >= Demo3DState::kNumObj)
				return 5; // lumiere : descripteur natif copie
			if (node <= 15)
				return 1; // spheres
			if (node == 83 || node == 84)
				return 3; // sol et feuillage : plans
			return 2; // cube central, colonnes, instances, mur
		}
		int32 Demo3DHostUserKind(int32 node) {
			return (node >= kNkvpFirstUser && node < kNkvpMaxNodes)
					   ? (int32)nkvpUserKind[node - kNkvpFirstUser]
					   : 0;
		}
		static int32 HostSpawnLike(int32 src, const float32 *offset) {
			auto *st = HostSt();
			if (!st || src < 0 || src >= kNkvpMaxNodes)
				return -1;
			const uint8 kind = HostKindOf(src);
			if (kind == 0)
				return -1;
			const int32 n = HostAllocUser(kind);
			if (n < 0)
				return -1;
			// Transform MONDE de la source (decalage pour la voir naitre a cote).
			float32 wp[3], wsc[3];
			NkMat4f wr;
			HostNodeWorld(st, src, wp, wr, wsc);
			NkVec3f qp, qr, qs;
			HostDecompose(wr, qp, qr, qs);
			const int32 e = n - kNkvpFirstEmpty;
			for (int32 a = 0; a < 3; ++a) {
				nkvpEmptyPos[e][a] = wp[a] + offset[a];
				nkvpEmptyScl[e][a] = wsc[a];
			}
			nkvpEmptyRotDeg[e][0] = qr.x;
			nkvpEmptyRotDeg[e][1] = qr.y;
			nkvpEmptyRotDeg[e][2] = qr.z;
			// Le rendu EFFECTIF de la source devient la surcharge du double.
			if (kind >= 1 && kind <= 3) {
				nkvpMatMask[n] = 1 | 2 | 4;
				nkvpMatTint[n][0] = nkvpMatCache[src][0];
				nkvpMatTint[n][1] = nkvpMatCache[src][1];
				nkvpMatTint[n][2] = nkvpMatCache[src][2];
				nkvpMatMetal[n] = nkvpMatCache[src][3];
				nkvpMatRough[n] = nkvpMatCache[src][4];
			}
			if (kind == 5) {
				// Une lumiere dupliquee RESTE une lumiere (Rihen).
				nkvpUserLight[n - kNkvpFirstUser] =
					src >= kNkvpFirstUser
						? nkvpUserLight[src - kNkvpFirstUser]
						: Demo3D_LightEffective(st, src - Demo3DState::kNumObj);
			}
			// Le double reprend aussi la TAILLE LOCALE surchargee.
			if (nkvpBaseSet[src]) {
				nkvpBaseSet[n] = true;
				for (int32 a = 0; a < 3; ++a) {
					nkvpBaseSize[n][a] = nkvpBaseSize[src][a];
					nkvpDimFactor[n][a] = nkvpDimFactor[src][a];
				}
			}
			const int32 pp = nkvpParentOf[src];
			nkvpParentOf[n] = (pp >= 0 && !nkvpDeleted[pp]) ? pp : -1;
			return n;
		}
		int32 Demo3DHostDuplicateNode(int32 node) {
			const float32 off[3] = {0.45f, 0.f, 0.45f};
			return HostSpawnLike(node, off);
		}
		void Demo3DHostCopyNode(int32 node) {
			auto *st = HostSt();
			if (!st || node < 0 || node >= kNkvpMaxNodes)
				return;
			const uint8 kind = HostKindOf(node);
			if (kind == 0)
				return;
			nkvpClipSet = true;
			nkvpClipKind = kind;
			if (kind == 5)
				nkvpClipLight = node >= kNkvpFirstUser
									? nkvpUserLight[node - kNkvpFirstUser]
									: Demo3D_LightEffective(st, node - Demo3DState::kNumObj);
			float32 wp[3], wsc[3];
			NkMat4f wr;
			HostNodeWorld(st, node, wp, wr, wsc);
			NkVec3f qp, qr, qs;
			HostDecompose(wr, qp, qr, qs);
			for (int32 a = 0; a < 3; ++a) {
				nkvpClipTRS[a] = wp[a];
				nkvpClipTRS[6 + a] = wsc[a];
			}
			nkvpClipTRS[3] = qr.x;
			nkvpClipTRS[4] = qr.y;
			nkvpClipTRS[5] = qr.z;
			for (int32 a = 0; a < 5; ++a)
				nkvpClipMat[a] = nkvpMatCache[node][a];
		}
		int32 Demo3DHostPasteNode() {
			if (!nkvpClipSet)
				return -1;
			const int32 n = HostAllocUser(nkvpClipKind);
			if (n < 0)
				return -1;
			const int32 e = n - kNkvpFirstEmpty;
			for (int32 a = 0; a < 3; ++a) {
				nkvpEmptyPos[e][a] = nkvpClipTRS[a];
				nkvpEmptyRotDeg[e][a] = nkvpClipTRS[3 + a];
				nkvpEmptyScl[e][a] = nkvpClipTRS[6 + a];
			}
			if (nkvpClipKind == 5)
				nkvpUserLight[n - kNkvpFirstUser] = nkvpClipLight;
			if (nkvpClipKind >= 1 && nkvpClipKind <= 3) {
				nkvpMatMask[n] = 1 | 2 | 4;
				nkvpMatTint[n][0] = nkvpClipMat[0];
				nkvpMatTint[n][1] = nkvpClipMat[1];
				nkvpMatTint[n][2] = nkvpClipMat[2];
				nkvpMatMetal[n] = nkvpClipMat[3];
				nkvpMatRough[n] = nkvpClipMat[4];
			}
			return n;
		}
		// Taille LOCALE approchee d'un noeud par nature : les DIMENSIONS du
		// panneau = echelle monde x cette base (le format projet l'affinera
		// avec les vraies boites englobantes).
		void Demo3DHostNodeBaseSize(int32 node, float32 *out3) {
			if (node >= 0 && node < kNkvpMaxNodes && nkvpBaseSet[node]) {
				out3[0] = nkvpBaseSize[node][0];
				out3[1] = nkvpBaseSize[node][1];
				out3[2] = nkvpBaseSize[node][2];
				return;
			}
			const uint8 k = HostKindOf(node);
			float32 b = 1.2f; // cube unite de la demo
			if (k == 1)
				b = 1.1f; // sphere
			out3[0] = out3[1] = out3[2] = b;
			if (k == 3) { // plan : etendu en X/Z, plat en Y
				out3[0] = out3[2] = (node == 83) ? 80.f : 2.f;
				out3[1] = 0.f;
			}
			if (k == 0 || k == 4)
				out3[0] = out3[1] = out3[2] = 1.f;
		}
		void Demo3DHostSetNodeBaseSize(int32 node, const float32 *in3) {
			if (node < 0 || node >= kNkvpMaxNodes)
				return;
			// Facteur de RENDU = taille voulue / taille par defaut de la nature.
			const bool was = nkvpBaseSet[node];
			nkvpBaseSet[node] = false;
			float32 def[3];
			Demo3DHostNodeBaseSize(node, def);
			nkvpBaseSet[node] = was;
			for (int32 a = 0; a < 3; ++a) {
				const float32 v = in3[a] < 0.f ? 0.f : in3[a];
				nkvpBaseSize[node][a] = v;
				nkvpDimFactor[node][a] = def[a] > 1e-6f ? v / def[a] : 1.f;
			}
			nkvpBaseSet[node] = true;
		}
		bool Demo3DHostUserLightParams(int32 node, float32 *color3, float32 *intensity) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes ||
				nkvpUserKind[node - kNkvpFirstUser] != 5)
				return false;
			const renderer::NkLightDesc &L = nkvpUserLight[node - kNkvpFirstUser];
			color3[0] = L.color.x;
			color3[1] = L.color.y;
			color3[2] = L.color.z;
			*intensity = L.intensity;
			return true;
		}
		void Demo3DHostSetUserLightParams(int32 node, const float32 *color3, float32 intensity) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes ||
				nkvpUserKind[node - kNkvpFirstUser] != 5)
				return;
			renderer::NkLightDesc &L = nkvpUserLight[node - kNkvpFirstUser];
			L.color = {color3[0], color3[1], color3[2]};
			L.intensity = intensity < 0.f ? 0.f : intensity;
		}
		int32 Demo3DHostTakeShortcuts() {
			const int32 b = nkvpShortcutBits;
			nkvpShortcutBits = 0; // consommes une fois
			return b;
		}
		// Le MEME geste pour toute la selection : le delta tape dans le panneau
		// sur l'objet ACTIF est propage aux autres selectionnes -- position en
		// delta monde, rotation en delta d'angles, echelle en rapport.
		void Demo3DHostApplyDeltaToSelection(const float32 *dPos, const float32 *dRotDeg,
											 const float32 *sclRatio, int32 except) {
			auto *st = HostSt();
			if (!st)
				return;
			renderer::NkGizmo3D &G = st->gizmo;
			const float32 kDeg2Rad = 0.017453292f;
			for (int32 i = 0; i < Demo3DState::kNumObj; ++i) {
				if (i == except || !G.IsSelected(i) || HostLockedEff(i))
					continue;
				const NkVec3f t = G.TranslateOf(i);
				G.SetTranslateOf(i, {t.x + dPos[0], t.y + dPos[1], t.z + dPos[2]});
				if (fabsf(dRotDeg[0]) + fabsf(dRotDeg[1]) + fabsf(dRotDeg[2]) > 1e-6f) {
					const NkMat4f R = NkMat4f::RotationZ(NkAngle::FromRad(dRotDeg[2] * kDeg2Rad)) *
									  NkMat4f::RotationY(NkAngle::FromRad(dRotDeg[1] * kDeg2Rad)) *
									  NkMat4f::RotationX(NkAngle::FromRad(dRotDeg[0] * kDeg2Rad));
					G.SetRotationOf(i, R * G.RotationOf(i));
				}
				NkVec3f sv = G.ScaleOf(i);
				float32 *sc[3] = {&sv.x, &sv.y, &sv.z};
				for (int32 a = 0; a < 3; ++a)
					if (sclRatio[a] > 1e-6f && fabsf(sclRatio[a] - 1.f) > 1e-6f)
						*sc[a] = (1.f + *sc[a]) * sclRatio[a] - 1.f;
				G.SetScaleOf(i, sv);
			}
		}
		bool Demo3DHostObjectTransform(int32 i, float32 *pos3, float32 *rotDeg3, float32 *scl3) {
			auto *st = HostSt();
			if (!st || i < 0 || i >= Demo3DState::kNumObj)
				return false;
			NkVec3f p, r, sc;
			HostDecompose(st->objXform[i], p, r, sc);
			pos3[0] = p.x;
			pos3[1] = p.y;
			pos3[2] = p.z;
			rotDeg3[0] = r.x;
			rotDeg3[1] = r.y;
			rotDeg3[2] = r.z;
			scl3[0] = sc.x;
			scl3[1] = sc.y;
			scl3[2] = sc.z;
			return true;
		}
		void Demo3DHostSetObjectTransform(int32 i, const float32 *pos3, const float32 *rotDeg3,
										  const float32 *scl3) {
			auto *st = HostSt();
			if (!st || i < 0 || i >= Demo3DState::kNumObj || HostLockedEff(i))
				return;
			NkVec3f cp, cr, cs;
			HostDecompose(st->objXform[i], cp, cr, cs);
			renderer::NkGizmo3D &G = st->gizmo;
			// Position : delta MONDE exact.
			const NkVec3f t = G.TranslateOf(i);
			G.SetTranslateOf(i, {t.x + (pos3[0] - cp.x), t.y + (pos3[1] - cp.y),
								 t.z + (pos3[2] - cp.z)});
			// Rotation : delta d'angles compose DANS L'ORDRE de la decomposition.
			const float32 kDeg2Rad = 0.017453292f;
			const float32 dx = (rotDeg3[0] - cr.x) * kDeg2Rad;
			const float32 dy = (rotDeg3[1] - cr.y) * kDeg2Rad;
			const float32 dz = (rotDeg3[2] - cr.z) * kDeg2Rad;
			if (fabsf(dx) + fabsf(dy) + fabsf(dz) > 1e-6f) {
				const NkMat4f R = NkMat4f::RotationZ(NkAngle::FromRad(dz)) *
								  NkMat4f::RotationY(NkAngle::FromRad(dy)) *
								  NkMat4f::RotationX(NkAngle::FromRad(dx));
				G.SetRotationOf(i, R * G.RotationOf(i));
			}
			// Echelle : rapport par axe sur le decalage (1+s).
			NkVec3f sv = G.ScaleOf(i);
			const float32 want[3] = {scl3[0], scl3[1], scl3[2]};
			const float32 cur[3] = {cs.x, cs.y, cs.z};
			float32 *sc[3] = {&sv.x, &sv.y, &sv.z};
			for (int32 a = 0; a < 3; ++a)
				if (cur[a] > 1e-6f && fabsf(want[a] - cur[a]) > 1e-6f)
					*sc[a] = (1.f + *sc[a]) * (want[a] / cur[a]) - 1.f;
			G.SetScaleOf(i, sv);
		}

		void Demo3DHostGetCameraPose(float32 *t3, float32 *dist, float32 *yaw, float32 *pitch,
									 bool *ortho) {
			auto *st = HostSt();
			if (!st) {
				t3[0] = t3[1] = t3[2] = 0.f;
				*dist = 6.5f;
				*yaw = 0.7f;
				*pitch = 0.4f;
				*ortho = false;
				return;
			}
			const NkVec3f t = st->editorCam.GetTarget();
			t3[0] = t.x;
			t3[1] = t.y;
			t3[2] = t.z;
			*dist = st->editorCam.GetDistance();
			*yaw = st->editorCam.GetYaw();
			*pitch = st->editorCam.GetPitch();
			*ortho = st->orthoView;
		}
		void Demo3DHostSetCameraPose(const float32 *t3, float32 dist, float32 yaw, float32 pitch,
									 bool ortho) {
			auto *st = HostSt();
			if (!st)
				return;
			st->editorCam.SetCenter({t3[0], t3[1], t3[2]}, dist, yaw, pitch);
			st->orthoView = ortho;
		}

		bool Demo3DHostReady() {
			return hst.ok;
		}

		const char *Demo3DHostError() {
			return hst.err;
		}

		void Demo3DHostShutdown() {
			if (hst.ok)
				Demo3D_Shutdown(hst.ctx);
			if (hst.ctx.renderer) {
				if (hst.rt)
					hst.ctx.renderer->DestroyOffscreen(hst.rt);
				NkRenderer::Destroy(hst.ctx.renderer);
			}
			hst.rt = nullptr;
			hst.ctx.renderer = nullptr;
			hst.ok = false;
			hst.tried = false;
		}

	} // namespace demo
} // namespace nkentseu
