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
// APERCU DE MATERIAU rendu par le moteur : sa mini-scene vit a part, dans son
// propre fichier -- ce fichier-ci en compte deja pres de dix-sept mille.
#include "NK3DModeler/Viewport/NkMatPreview3D.h"
#include "NKWindow/Core/NkWESystem.h" // NkEvents()
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"	   // NkMouseWheelVerticalEvent, NkMouseButton
#include "NKEvent/NkEventDispatcher.h" // NkInput (IsMouseDown / MouseDeltaX/Y / IsKeyDown)
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h" // ciel procedural / HDRI
#include "NKRenderer/Core/NkCameraController.h" // NkOrbitCameraController3D / NkFlyCameraController3D
#include "NKRenderer/Core/NkGizmo.h"			// NkGizmo3D (gizmo éditeur réutilisable)
#include "NKRenderer/Materials/NkMatcapLibrary.h" // noms des 30 matcaps (source unique)
#include "NKRenderer/Core/NkLightGizmo.h"   // widgets des lumieres (facon Blender)
#include "NKImage/NKImage.h"
#include "NKContainers/String/Encoding/NkBase64.h"					// Phase H : test ecriture PNG procedural
#include "NKContainers/Associative/NkHashMap.h" // dedup arêtes Edit Mode
#include "NKRenderer/Mesh/NkEditMesh.h"			// structure demi-arête n-gon
#include "NKFileSystem/NkFile.h"				// save/load session d'édition (journal de commandes)
#include "NKTime/NkChrono.h"					// mesure du coût des aperçus modaux (NK_MODAL_PERF)
#include "NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h" // NK_GI_TEST : GI à un rebond
#include "NKLogger/NkLog.h"			  // diagnostic par le journal (methode de travail)
#include "NKFileSystem/NkDirectory.h" // dossier de sortie cree a la volee
#include "NkOutCompose.h"			  // formes et composition des incrustations
#include "NKMedia/Video/NkVideoWriter.h"			// enregistrement video de la session
#include "NKMedia/Video/NkImageSequenceWriter.h" // suite d'images (workflow Blender)
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h" // MP4/H.264 (muxe lui-meme)
#include "NKThreading/NkThread.h"				// l'encodage video vit sur son propre fil
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
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
		// ── TOUCHE DE NAVIGATION, LUE PAR POLLING ───────────────────────────
		// Les gestionnaires d'EVENEMENTS de la vue sont deja gardes par
		// nkvpInputOn ; le pilotage de camera, lui, interroge l'etat clavier a
		// chaque image et echappait donc a cette garde : renommer une camera
		// dans la hierarchie et se deplacer dans le texte avec les fleches
		// faisait AUSSI voyager la vue 3D, alors que le champ avait le focus
		// (constate par Rihen). Toute lecture clavier de navigation passe
		// desormais par ici -- un seul point de passage, donc plus d'oubli.
		static bool HostNavKey(NkKey k);
		static bool nkvpHover = false;				   // souris au-dessus de la vue
		static bool HostNavKey(NkKey k) {
			return nkvpInputOn && NkInput.IsKeyDown(k);
		}
		static void *nkvpCmd = nullptr;				   // cmd de l'editeur (frame courante)
		static bool nkvpHudOn = true;				   // HUD texte de la demo (surimpression)
		// OEIL et CADENAS de la hierarchie : visibilite et verrou PAR OBJET.
		// La visibilite gate les soumissions de la demo ; le verrou bloque la
		// selection depuis la hierarchie et l'ecriture de transformation.
		static bool nkvpObjHidden[160] = {};
		static bool nkvpObjLocked[160] = {};
		// DRAPEAUX DU MODEL, distincts de ceux de la scene (regle de Rihen) :
		// cacher dans la scene ne doit rien changer dans l'editeur de model,
		// tandis que cacher DANS le model se voit dans toutes les scenes. Un
		// seul drapeau par noeud ne peut pas dire les deux -- il en faut un
		// par contexte, et c'est le document courant qui choisit lequel on
		// lit et lequel on ecrit.
		static bool nkvpMeshHidden[160] = {};
		static bool nkvpMeshLocked[160] = {};
		static bool nkvpLightHidden[8] = {};
		static float32 nkvpFarOverride = 0.f;  // 0 = auto (dist*20+100) ; sinon la
											   // DISTANCE DE VUE choisie, independante
											   // des cameras de la scene
		static float32 nkvpOrthoScale = 0.55f; // demi-hauteur ortho = dist * ce facteur
		static int32 nkvpGridExtent = 20;	   // demi-etendue de la grille ortho (unites)
		// GRILLE ET SES TRAITS COUPES PAR DEFAUT (Rihen) : le sol infini en
		// damier donne le repere au sol -- le shell TIRE ces valeurs a la
		// premiere image (la demo est la source de verite a l'ouverture),
		// c'est donc ICI que vivent les defauts, pas dans NkModelerState.
		static bool nkvpGridOn = false; // la VOLONTE de grille (la case du shell), meme quand
										// l'ortho coupe la grille infinie du moteur
		static bool nkvpAxesOn = true;	 // axes debug +-1000 (bascule « Axes du plan »)
		static bool nkvpMinorOn = false; // volonte « Lignes internes »
		static bool nkvpMajorOn = false; // volonte « Lignes majeures »
		static bool nkvpCursorTool = false;			   // outil CURSEUR : clic gauche = poser le curseur 3D
		// CURSEUR 3D VISIBLE dans la vue (case de l'onglet Affichage, cochee par
		// defaut) : c'est un repere de travail, on doit pouvoir l'eteindre sans
		// pour autant renoncer a l'outil qui le place.
		static bool nkvpCursorShow = true;
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
		// ── EDITION PROPORTIONNELLE (Blender : « proportional editing ») ────
		// Deplacer un sommet ENTRAINE ses voisins, d'autant moins qu'ils sont
		// loin. Le RAYON dit jusqu'ou porte l'influence ; l'ATTENUATION dit
		// comment elle decroit -- huit lois, celles de Blender, parce que la
		// forme du fondu EST le resultat : « Lisse » arrondit, « Net » pince,
		// « Constant » deplace un bloc entier.
		static bool nkvpPropEditOn = false;
		static float32 nkvpPropEditRadius = 1.f;
		static int32 nkvpPropEditFalloff = 0; // 0 lisse .. 7 aleatoire
		// Distance de chaque sommet au plus proche sommet SELECTIONNE, calculee
		// UNE FOIS au debut du geste : la refaire a chaque image couterait
		// nv x nsel par frame, et la reference bougerait avec les sommets.
		static NkVector<float32> nkvpPropDist;
		// MEME MECANIQUE POUR LES OBJETS (Rihen) : distance de chaque noeud au
		// plus proche noeud SELECTIONNE, figee au debut du geste. C'est ce qui
		// permet de disposer une foret ou d'incurver une rangee de batiments
		// sans toucher chaque objet un a un.
		static float32 nkvpPropDistNode[70] = {};
		static bool nkvpPropNodeArmed = false;
		// Pivot du geste, fige lui aussi : rotation et echelle des voisins
		// tournent autour de LUI, jamais autour de leur propre centre -- c'est
		// ce qui fait qu'une rangee s'incurve au lieu que chaque objet pivote
		// sur place.
		static NkVec3f nkvpPropPivot{0.f, 0.f, 0.f};
		// Poids d'un sommet a la distance d, pour un rayon r.
		static float32 HostPropFalloff(float32 d, float32 r, int32 type) {
			if (r <= 1e-6f)
				return 0.f;
			float32 t = 1.f - d / r; // 1 au centre, 0 au bord
			if (t <= 0.f)
				return 0.f;
			if (t > 1.f)
				t = 1.f;
			switch (type) {
				case 1: // Sphere
					return math::NkSqrt(1.f - (1.f - t) * (1.f - t));
				case 2: // Racine
					return math::NkSqrt(t);
				case 3: // Carre inverse
					return t * t;
				case 4: // Net
					return t * t * t;
				case 5: // Lineaire
					return t;
				case 6: // Constant : tout le rayon bouge d'un bloc
					return 1.f;
				case 7: { // Aleatoire, STABLE par sommet (sinon ca grouille)
					const float32 h = math::NkSin(d * 127.1f) * 43758.545f;
					const float32 rnd = h - (float32)(int64)h;
					return t * (rnd < 0.f ? rnd + 1.f : rnd);
				}
				default: // 0 Lisse (Hermite) : le defaut de Blender
					return t * t * (3.f - 2.f * t);
			}
		}
		// ── LA ROTATION D'UN NOEUD EST UN QUATERNION ────────────────────────
		// C'est la VERITE (choix d'Unreal : FTransform stocke un FQuat), et les
		// angles d'Euler ne sont plus que la langue de l'interface. Un
		// quaternion ne se decompose pas : il n'a donc ni gimbal lock ni saut de
		// 180 degres, les deux defauts constates par Rihen.
		//
		// LES ANGLES SAISIS SONT CONSERVES tant qu'ils decrivent la MEME
		// orientation. Sans ce cache, taper 190 puis relire afficherait -170 --
		// meme orientation, autre ecriture -- c'est le comportement d'Unreal, et
		// il surprend a l'usage. Des qu'on tourne a la souris, les angles sont
		// relus du quaternion en choisissant l'ecriture la plus proche de la
		// precedente (HostDecomposeNear).
		static NkQuatf nkvpEmptyQuat[70];
		static bool nkvpEmptyQuatInit = false;
		static bool nkvpRotCacheOk[70] = {}; // les angles affiches font-ils foi ?
		static void HostQuatEnsure() {
			if (nkvpEmptyQuatInit)
				return;
			nkvpEmptyQuatInit = true;
			for (int32 i = 0; i < 70; ++i)
				nkvpEmptyQuat[i] = NkQuatf::Identity();
		}
		// ── ECHELLE EXACTE (cisaillement autorise) ──────────────────────────
		// Par DEFAUT l'echelle vit dans les axes de l'objet -- choix d'Unreal
		// (FTransform refuse le cisaillement), retenu pour NK3DModeler, cf.
		// PRINCIPES_CONCEPTION.private.md. Avec l'option, un noeud memorise EN
		// PLUS le repere MONDE dans lequel son echelle a ete appliquee : sa
		// transform devient T * (B S Bt) * R, ce qui rend le VRAI cisaillement
		// -- un carre devient un losange, comme l'exige un scale global sur un
		// objet tourne. Sans base memorisee (le cas courant), on retombe
		// exactement sur T * R * S : aucun cout, aucun changement.
		static bool nkvpShearOpt = false;		   // l'option, pour toute la scene
		static NkVec3f nkvpEmptySclAx[70][3] = {}; // repere monde de l'echelle
		static bool nkvpEmptyShear[70] = {};	   // ce noeud en porte-t-il un ?
		static constexpr int32 kNkvpMaxUser = 64;
		static uint8 nkvpUserKind[kNkvpMaxUser] = {};
		// Sous-type du noeud utilisateur (style d'empty, variante de courbe/
		// surface/metaball, primitive demandee) -- porte par le menu Ajouter.
		static uint8 nkvpUserSub[kNkvpMaxUser] = {};
		// Mesh PARAMETRIQUE du slot (regenere quand ses parametres changent)
		// et ses parametres (segments / anneaux-subdivisions).
		static NkMeshHandle nkvpUserMesh[kNkvpMaxUser];
		static int32 nkvpUserSeg[kNkvpMaxUser];
		static int32 nkvpUserRing[kNkvpMaxUser];
		static float32 nkvpUserAux[kNkvpMaxUser]; // ex. rayon interne du tore
		static float32 nkvpUserCam[kNkvpMaxUser][3]; // camera : fov, clip debut/fin
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
		// Mise a jour des ombres, reglee au panneau Rendu : dynamique par defaut,
		// parce qu'un modeleur passe son temps a deplacer objets et lumieres.
		static bool nkvpShadowDynamic = true;
		// Brouillard de scene, regle au panneau Rendu. Eteint par defaut : il
		// change radicalement l'image, il ne doit pas s'inviter tout seul.
		// SOL INFINI (option, Rihen) : un VRAI plan de sol -- il recoit lumiere
		// et ombres. Ce n'est pas la grille : les deux coexistent, le plan
		// legerement SOUS elle (coplanaires, ils z-fightaient jadis).
		// ACTIF PAR DEFAUT, motif damier (Rihen) : la grille du viewport est
		// coupee par defaut, c'est ce sol qui donne le repere.
		static bool nkvpFloorOn = true;
		static float32 nkvpFloorColor[3] = {0.45f, 0.45f, 0.47f};
		static float32 nkvpFloorY = 0.f;
		static float32 nkvpFloorRough = 0.9f;
		static float32 nkvpFloorMetal = 0.f;
		// Motif : 0 uni, 1 damier, 2 carreaux a joints (references Unreal de
		// Rihen). Taille du carreau en metres. DAMIER par defaut (Rihen).
		static int32 nkvpFloorPattern = 1;
		static float32 nkvpFloorTile = 1.f;
		static bool nkvpFogOn = false;
		static float32 nkvpFogColor[3] = {0.5f, 0.6f, 0.7f};
		static float32 nkvpFogDensity = 0.02f;
		static float32 nkvpFogStart = 10.f;
		static float32 nkvpFogEnd = 60.f;
		static int32 nkvpFogMode = 0; // 0 lineaire, 1 exponentiel
		// ── BROUILLARD AU SOL, SOUFFLE COMME UNE FUMEE (Rihen) ──────────────
		// Epaisseur a zero : le brouillard ne depend que de la distance, comme
		// avant. Le SOUFFLE module sa densite par un bruit qui derive ; lie au
		// vent des NUAGES, le sol et le ciel respirent ensemble. Quand la
		// physique du vent existera, c'est elle qui donnera direction et force.
		static float32 nkvpFogHeightBase = 0.f;
		static float32 nkvpFogThickness = 0.f;
		static float32 nkvpFogWind = 0.f;
		static bool nkvpFogWindFromClouds = true;
		static renderer::NkLightDesc nkvpClipLight;
		static int32 nkvpShortcutBits = 0;
		static uint8 nkvpShortcutPrev = 0;
		static void HostHierarchyFrame(); // detecteur (defini pres des accesseurs)
		static void HostParentEnsureInit();
		static int32 HostAllocUser(uint8 kind);
		// Panneau « Occlusion ambiante » : lus par la reconstruction du GI et la
		// bascule du panneau, definies AVANT les facades — la grille voxel obeit
		// au meme bouton que la SSAO (une seule notion d'occlusion d'ambiance).
		void Demo3DHostSSAO(bool *on, float32 *radius, float32 *intensity);
		void Demo3DHostGIMarkDirty();
		static void HostDecompose(const NkMat4f &M, NkVec3f &pos, NkVec3f &rotDeg, NkVec3f &scl);
		// ── TRANSFORM LOCALE D'UN NOEUD : POINT DE PASSAGE UNIQUE ───────────
		// La meme formule etait recomposee A LA MAIN dans cinq endroits (rendu,
		// fil de fer, base du gizmo, pick, contour) : c'est exactement le genre
		// de duplication qui finit par diverger. Tout passe desormais par ici.
		//   withGizmo : inclut les decalages du gizmo (etat EFFECTIF pendant un
		//   geste) ; faux pour la BASE que le gizmo consomme lui-meme.
		static NkMat4f HostEmptyXform(int32 e, bool withGizmo);
		// Vitesse de derive des nuages : definie plus bas, lue par le brouillard
		// au sol (qui suit le meme vent) bien avant.
		float32 Demo3DHostSkyCloudSpeed();
		// Variante CONTINUE (evite le saut de 180 degres des angles d'Euler) :
		// definie avec l'autre, appelee bien avant.
		static void HostDecomposeNear(const NkMat4f &M, const float32 *prevDeg, NkVec3f &pos,
									  NkVec3f &rotDeg, NkVec3f &scl);
		// Rotation d'un triplet d'angles (convention Z*Y*X du projet) : definie
		// avec les autres helpers, appelee par le commit du gizmo bien avant.
		static NkMat4f HostRotFromEuler(const float32 *rotDeg);
		// ── ROTATION D'UN NOEUD : le QUATERNION fait foi ────────────────────
		// Definies avec les autres helpers, appelees partout bien avant.
		static NkQuatf HostNodeQuat(int32 e);
		static void HostSetNodeQuat(int32 e, const NkQuatf &q);
		static void HostSetNodeEuler(int32 e, const float32 *deg);
		static void HostNodeEuler(int32 e, float32 *outDeg);
		// Transform MONDE d'un noeud. Enveloppe SANS Demo3DState : la structure
		// n'est declaree que plus bas, et la vue camera en a besoin ici.
		static void HostNodeWorldById(int32 n, float32 *pos, NkMat4f &rot, float32 *scl);
		// Racine du model qui contient un noeud : le pick de la vue s'en sert bien
		// avant la definition (rangee plus bas avec les autres APIs).
		int32 Demo3DHostModelRootOf(int32 node);
		// Naissance d'un noeud calque sur un autre (duplication, archive, premier
		// maillage d'un model) ; defini plus bas, appele plus haut.
		static int32 HostSpawnLike(int32 src, const float32 *offset);
		// APPARTENANCE : chaque noeud appartient a UN document (scene ou
		// editeur d'asset). Un noeud d'un autre document n'est ni rendu ni
		// liste ni selectionnable ici (regle de Rihen).
		static uint8 nkvpSceneOf[kNkvpMaxNodes] = {};
		static uint8 nkvpCurScene = 0;
		// MESH INTERNE d'un model : cree DANS un editeur de model. Il se rend
		// dans les deux vues, mais ne figure que dans la hierarchie du model,
		// et dans la scene un clic dessus selectionne TOUT le model (Rihen).
		static bool nkvpIsMesh[kNkvpMaxNodes] = {};
		// MODEL : le conteneur. Il garde sa nature (on ne le transforme PAS en
		// empty -- ce changement repartait dans la scene), mais il ne rend plus
		// sa geometrie en propre : ce sont ses maillages qui la rendent, dans
		// les deux vues. Seul lui a droit au nom de model (Rihen).
		static bool nkvpIsModel[kNkvpMaxNodes] = {};
		static bool nkvpDocIsModel = false;
		// (L'etancheite du model aux reglages de la scene ne passe PAS par la
		// connaissance de sa racine, mais par des drapeaux separes par contexte --
		// voir nkvpMeshHidden / nkvpMeshLocked plus bas.)
		// VUE CAMERA : noeud regarde (-1 = vue 3D libre).
		static int32 nkvpCamViewNode = -1;
		// CADENAS D'ORBITE en vue camera (bouton de la colonne de vue) : actif,
		// la rotation ORBITE la camera autour d'un centre au lieu de tourner
		// sur place -- comme Blender (regle de Rihen).
		static bool nkvpCamOrbitLock = false;
		// CAMERA ACTIVE (facon Blender) : celle que le pave 0 regarde et que la
		// capture « Camera » utilisera. Devient active des qu'on la regarde.
		static int32 nkvpActiveCamNode = -1;
		// ── SORTIE (pastille Output) ────────────────────────────────────────
		// Ce qui SORT de la scene, par opposition a la pastille Rendu qui dit
		// COMMENT elle est eclairee. Une cible PRINCIPALE, et jusqu'a huit
		// INCRUSTATIONS posees par-dessus (Rihen : « une principale et les
		// autres en miniature, rectangle, carre, cercle etc., positionnees
		// au-dessus de la principale »).
		//
		// La resolution de sortie est INDEPENDANTE de la taille de la fenetre :
		// sinon le champ ne serait qu'une decoration, et un rendu 4K depuis une
		// fenetre 1600x900 resterait en 1600x900.
		struct NkVpOutInset {
				bool used = false;
				int32 source = -1;	 // noeud camera ; -1 = la vue 3D
				int32 shape = 0;	 // voir kNkvpInsetShapes
				float32 x = 0.70f;	 // coin haut-gauche, fraction de la principale
				float32 y = 0.04f;
				// DEUX DIMENSIONS, chacune fraction du COTE correspondant de la
				// principale (Rihen) : 0,25 se lit « le quart », en largeur
				// comme en hauteur. Le carre et le cercle n'utilisent que
				// `size` et se ferment sur un cadre carre en pixels.
				float32 size = 0.26f;  // largeur (ou cote / diametre)
				float32 sizeH = 0.26f; // hauteur, pour les formes a deux cotes
				float32 border = 2.f;  // lisere, en pixels de SORTIE
				float32 borderCol[3] = {1.f, 1.f, 1.f};
				float32 opacity = 1.f;
				// FICHIER PROPRE (Rihen) : en plus d'etre posee sur l'image
				// principale, la miniature est ecrite seule. Utile quand la
				// vignette est aussi un livrable -- une vue de dessus, un plan
				// de detail -- et pas seulement un ornement.
				bool ownFile = false;
				// Et si ce fichier propre garde la FORME et le lisere. Par
				// defaut non : une vue de dessus livree en rond, avec un
				// contour blanc, ne servirait a rien -- la forme appartient a
				// la composition. Mais c'est un choix (Rihen), pas une regle :
				// une vignette ronde peut etre exactement le livrable voulu.
				bool ownShaped = false;
		};
		// Les formes sont definies UNE FOIS dans NkOutCompose.h, avec leurs
		// noms : le panneau ne peut donc pas afficher « Cercle » et le rendu
		// produire autre chose.
		static const int32 kNkvpInsetShapes = nk3d::kInsetShapeCount;
		static const int32 kNkvpMaxInsets = 8;
		static NkVpOutInset nkvpOutInset[kNkvpMaxInsets];
		// La source principale est une VUE PRECISE : -1 la vue 3D, sinon un
		// noeud camera. Une entree « camera active » a existe ici ; elle a ete
		// retiree (Rihen) -- la camera active est deja celle qu'on voit, et une
		// source qui se deplace toute seule rendait imprevisible ce qu'on
		// s'appretait a produire.
		static int32 nkvpOutSource = -1;
		static int32 nkvpOutW = 1920;
		static int32 nkvpOutH = 1080;
		static int32 nkvpOutScale = 100;   // pourcentage applique aux deux cotes
		static char nkvpOutDir[260] = "captures";
		static char nkvpOutName[64] = "rendu";
		static int32 nkvpOutFormat = 0;	   // indice dans kNkvpOutFmt
		static bool nkvpOutTransparent = false;
		// FORMATS : ceux que NkImage sait REELLEMENT ecrire. WebP et SVG sont
		// declares dans NKImage mais annonces « non implemente » -- les
		// proposer aurait produit des fichiers vides. L'extension suffit :
		// NkImage::Save choisit l'encodeur d'apres elle.
		struct NkVpOutFmt {
				const char *name;
				const char *ext;
				bool lossy; // seul le JPEG consomme le reglage de qualite
				bool alpha; // porte-t-il la transparence ?
		};
		// `alpha` : proposer un fond transparent pour un format qui ne sait pas
		// le porter produirait un fond NOIR sans le dire. JPEG et HDR n'ont pas
		// de canal alpha ; BMP en a un, mais si peu d'outils le lisent qu'il
		// vaut mieux ne pas le promettre.
		static const NkVpOutFmt kNkvpOutFmt[] = {
			{"PNG", "png", false, true},		  {"JPEG", "jpg", true, false},
			{"BMP", "bmp", false, false},		  {"TGA", "tga", false, true},
			{"QOI", "qoi", false, true},		  {"HDR (32 bits)", "hdr", false, false},
			{"EXR (32 bits)", "exr", false, true}};
		static const int32 kNkvpOutFmtCount =
			(int32)(sizeof(kNkvpOutFmt) / sizeof(kNkvpOutFmt[0]));
		static int32 nkvpOutQuality = 90; // JPEG
		// ── FORMAT « LIBRE » : UN ETAT, PAS UNE DEDUCTION ───────────────────
		// Le format nomme se DEDUIT de la resolution -- c'est elle la verite,
		// et taper 1920x1080 a la main doit afficher « Full HD ». Mais « Libre »
		// ne se deduit de rien : sur une resolution qui vaut justement un format
		// connu, le choisir etait annule a l'image suivante et il devenait le
		// seul format impossible a selectionner (constate par Rihen). Il lui
		// faut donc sa propre memoire.
		static bool nkvpOutFreeSize = false;
		// MODES DE RENDU A PRODUIRE, en masque de bits (Rihen : « cocher les
		// types de rendu qu'on veut »). Meme ordre que la touche Z de la vue :
		// 0 Rendu, 1 Solide, 2 Fil de fer, 3 Normales, 4 UV, 5 Occlusion.
		// Zero = le mode courant de la vue, et lui seul : cocher n'est pas
		// obligatoire pour rendre ce qu'on a sous les yeux.
		static int32 nkvpOutModes = 0;
		static const int32 kNkvpOutModeCount = 6;
		static const char *const kNkvpOutModeNames[kNkvpOutModeCount] = {
			"Rendu", "Solide", "Fil de fer", "Normales", "UV", "Occlusion (AO)"};
		// Suffixes de fichier : sans accent ni espace, un nom de fichier n'a
		// pas a etre une phrase.
		static const char *const kNkvpOutModeTags[kNkvpOutModeCount] = {
			"rendu", "solide", "filaire", "normales", "uv", "ao"};
		// ── CAPTURES : LEUR PROPRE NOM (Rihen) ─────────────────────────────
		// « Capturer la vue » et « Tutoriel » ecrivent dans le DOSSIER de la
		// sortie -- il n'y a qu'une destination configuree -- mais chacune
		// garde SON nom de base : melanger un rendu final, une capture d'ecran
		// de la scene et une photo de l'interface sous un meme nom rendrait le
		// dossier illisible. Le format et la qualite, eux, sont ceux de la
		// sortie : c'est un reglage d'image, pas de destination.
		// ── NOMS DE SCENE, POUSSES PAR L'INTERFACE ──────────────────────────
		// Les noms des noeuds vivent dans l'interface (elle les edite dans la
		// hierarchie) ; l'hote, lui, ne connait que des numeros. Pour que les
		// fichiers produits portent le VRAI nom de la camera -- « Camera.002 »
		// et non « cam2 » (Rihen y tient) -- l'interface les depose ici. Vide =
		// jamais renseigne : on retombe alors sur le rang, qui ne ment pas.
		static char nkvpNodeLabel[kNkvpMaxUser][32] = {};
		static char nkvpCapViewName[64] = "vue";
		static char nkvpCapTutoName[64] = "tutoriel";
		// ── VIDEO : LA CONFIGURATION EXISTE, LE RENDU VIENDRA ───────────────
		// Rihen l'a demandee en sachant que le developpement est pour plus
		// tard. Elle se REGLE donc des maintenant et se conserve, mais rien ne
		// pretend l'executer : aucun bouton qui ferait semblant.
		static bool nkvpOutVideoOn = false;
		static int32 nkvpOutFps = 25;
		static int32 nkvpOutFrameStart = 1;
		static int32 nkvpOutFrameEnd = 250;
		// ── CONTENEUR ET CODEC SEPARES (Rihen, comme Blender) ───────────────
		// Une seule liste melangeait les deux (« AVI (MJPEG) », « MOV (MJPEG) »)
		// et cachait que le meme codec sert dans deux conteneurs -- et que l'AVI
		// sait aussi ecrire du NON COMPRESSE. Les deux notions sont
		// independantes : le conteneur dit le FICHIER, le codec dit COMMENT les
		// images y sont ecrites. Tous les couples ne sont pas possibles : la
		// table ci-dessous dit lesquels NKMedia sait reellement produire, et le
		// choix de codec se restreint donc au conteneur retenu.
		struct NkVpVidCodec {
				const char *name;
				int32 id; ///< 0 PNG, 1 JPEG, 2 BMP, 3 TGA, 4 QOI (suite d'images)
						  ///< 10 non compresse, 11 MJPEG, 12 MPEG-1, 13 H.264
		};
		struct NkVpVidCont {
				const char *name;
				const char *ext; ///< vide = c'est un DOSSIER, pas un fichier
				NkVpVidCodec cod[5];
				int32 codCount;
		};
		static const NkVpVidCont kNkvpVidCont[] = {
			{"Suite d'images",
			 "",
			 {{"PNG (sans perte)", 0},
			  {"JPEG", 1},
			  {"BMP", 2},
			  {"TGA", 3},
			  {"QOI (sans perte)", 4}},
			 5},
			{"AVI", "avi", {{"MJPEG", 11}, {"Non compresse", 10}}, 2},
			{"QuickTime (MOV)", "mov", {{"MJPEG", 11}}, 1},
			{"MPEG-1 elementaire", "m1v", {{"MPEG-1", 12}}, 1},
			{"MPEG-4 (MP4)", "mp4", {{"H.264", 13}}, 1},
		};
		static const int32 kNkvpVidContCount =
			(int32)(sizeof(kNkvpVidCont) / sizeof(kNkvpVidCont[0]));
		static int32 nkvpOutVideoCont = 0; // indice dans kNkvpVidCont
		static int32 nkvpOutVideoCod = 0;  // indice DANS le conteneur choisi
		// CURSEUR DANS LA VIDEO DE TUTORIEL : la capture de fenetre de l'OS ne
		// contient PAS le pointeur -- une video sans lui montre des menus qui
		// s'ouvrent tout seuls. On le dessine, avec la trace de ses dernieres
		// positions. Actif par defaut : c'est ce qu'on attend d'un tutoriel.
		static bool nkvpOutCursor = true;
		// CONSERVER LES IMAGES QOI apres l'encodage (Rihen) : decoche par
		// defaut -- le dossier intermediaire s'efface une fois la video
		// construite. Coche, il reste : les images sans perte de la prise
		// valent une source de montage, et les reencoder ne coute rien.
		static bool nkvpOutKeepQoi = false;
		// QUALITE PROPRE A LA VIDEO (Rihen) : elle etait partagee avec l'image
		// fixe, alors que les deux n'ont pas les memes exigences -- une image
		// livrable veut 95, une video de session tient tres bien a 75 et pese
		// trois fois moins. Un seul curseur pour les deux obligeait a choisir
		// entre un fichier trop lourd et une capture degradee.
		static int32 nkvpOutVideoQuality = 80;
		// ── CADENCE DE CAPTURE (Rihen : « en option de configuration ») ─────
		// Une image fixe coute TROIS images d'attente : poser la cible, laisser
		// le GPU rendre, lire les pixels. Invisible pour une photo, ruineux pour
		// une video -- et c'est la LECTURE qui coute, elle synchronise le CPU
		// sur le GPU. Trois leviers, reglables parce qu'ils ont chacun leur
		// contrepartie :
		//   1 CIBLE FIXE  : ne pas reposer la taille entre deux images. Une
		//     passe au lieu de trois. A couper si la sortie change de format en
		//     cours de sequence.
		//   2 LECTURE DIFFEREE : lire l'image N-2 pendant que N se rend, en
		//     anneau. Le GPU ne s'arrete plus. Coute deux images de latence et
		//     la memoire du tampon.
		//   4 ENCODAGE PARALLELE : l'encodeur (CPU) consomme une image pendant
		//     que la suivante se calcule. Coute un fil et un tampon de plus.
		// Les trois sont actifs par defaut : ce sont les bons reglages dans le
		// cas courant, et on les coupe pour diagnostiquer.
		// ── ENREGISTREMENT VIDEO DE LA SESSION ──────────────────────────────
		// Ce que l'on filme, c'est ce qui se PASSE dans la vue -- pas une
		// animation parcourue image par image : le modeleur n'a pas encore de
		// timeline, et une plage d'images produirait 250 fois la meme. Le rendu
		// d'animation viendra pour les CAPTURES quand la timeline existera ; le
		// tutoriel, lui, n'en aura jamais besoin (Rihen).
		//
		// La vue est lue TELLE QU'ELLE EST AFFICHEE, a sa resolution : aucune
		// cible a redimensionner, aucune double passe -- une seule lecture par
		// image. C'est ce qui rend l'enregistrement tenable en continu.
		// L'ENCODAGE VIT SUR SON PROPRE FIL. Encoder un JPEG 1920x1080 coute
		// plusieurs millisecondes ; fait sur le fil principal, a chaque image,
		// il ralentissait tout le modeleur -- « c'est comme s'il n'y avait pas
		// d'enregistrement dans un thread separe » (Rihen, et il avait raison).
		// Le fil principal ne fait plus que LIRE les pixels -- ce que seul lui
		// peut faire, le contexte GPU lui appartient -- et depose l'image dans
		// une file ; le fil d'encodage la vide a son rythme.
		// La file est BORNEE : si l'encodage prend du retard, on prefere PERDRE
		// des images plutot que gonfler la memoire sans fin. Une video qui saute
		// vaut mieux qu'une application qui s'etouffe.
		// EN SATURATION, C'EST L'UTILISATEUR QUI TRANCHE (Rihen) : sauter des
		// images -- l'application reste fluide, la video a des trous -- ou
		// laisser la file GONFLER -- rien n'est perdu, la memoire monte et le
		// fil principal finit par attendre. Aucune des deux n'est bonne dans
		// tous les cas : filmer une demonstration veut de la fluidite, filmer
		// un resultat veut la fidelite.
		static bool nkvpRecGrow = false; // faux = sauter (defaut), vrai = gonfler
		// IMAGES A IGNORER APRES UN REDIMENSIONNEMENT DE LA VUE. La cible prend
		// sa nouvelle taille immediatement, mais le rendu -- trois images en vol
		// -- couvre encore l'ancienne : les lignes du bas gardent la couleur
		// d'effacement, et la video montrait une bande vide qui apparaissait et
		// disparaissait (« clignotement entre l'interface et la vue », Rihen).
		// Comparer les tailles ne suffisait pas : elles sont deja d'accord, c'est
		// le CONTENU qui est en retard. On laisse donc passer quelques images.
		static int32 nkvpRecSettle = 0;
		static const int32 kNkvpRecQueue = 8;
		static const int32 kNkvpRecQueueMax = 240; // ~10 s a 25 i/s : garde-fou memoire
		struct NkVpRecSlot {
				NkImage img;
				bool full = false;
		};
		struct NkVpRec {
				bool on = false;
				bool paused = false;
				int32 frames = 0;
				int32 dropped = 0;
				uint32 w = 0, h = 0;
				// Trois ecrivains possibles, un seul actif : suite d'images,
				// conteneur simple (AVI/MOV/MPEG-1) ou MP4/H.264. Les garder
				// cote a cote plutot que derriere une interface commune evite
				// une couche d'abstraction pour trois cas connus d'avance.
				int32 kind = 0; // 0 = suite d'images, 1 = conteneur, 2 = MP4/H264
				bool seq = false;
				media::NkVideoWriter vw;
				media::NkImageSequenceWriter sw;
				media::NkH264Encoder h264;
				char path[300] = {};
				float32 acc = 0.f; // secondes accumulees depuis la derniere image
				// ── TOUTE VIDEO EST DIFFEREE (decision de Rihen, 5 aout) ────
				// L'encodeur H.264 tient 2,2 images/s en 1936x1048 quand la
				// capture en produit 25 : filmer directement saturait la file,
				// sautait 9 images sur 10, et la video sortait onze fois trop
				// rapide -- et meme le MJPEG, a 21 i/s, en sautait une sur six.
				// La prise ecrit donc des images QOI -- SANS PERTE et plus
				// rapides a encoder que le JPEG -- et le fichier final se
				// construit A L'ARRET, hors temps reel. Aucune image sautee,
				// UNE seule generation de compression, tous conteneurs. Le brut
				// n'aurait rien ajoute : meme fidelite, mais 8 Mo par image la
				// ou le QOI en ecrit un ou deux -- c'est le disque qui devient
				// le goulot. Seule la suite d'images reste en direct : les
				// images Y SONT le livrable, au format choisi par l'utilisateur.
				bool deferred = false;
				char tmpDir[300] = {};	// images intermediaires, effacees ensuite
				const char *tmpExt = "qoi";
				int32 fps = 25;			// retenus au demarrage : la phase finale
				int32 qp = 26;			// tourne apres, sans relire les reglages
				int32 vq = 80;			// qualite MJPEG de la passe finale
				int32 finalCod = 11;	// 10 brut, 11 MJPEG, 12 MPEG-1, 13 H.264
				bool encoding = false;	// la passe finale est en cours
				bool keepTmp = false;	// fige nkvpOutKeepQoi au demarrage
				int32 encDone = 0, encTotal = 0;
				threading::NkThread encTh;
				// File et fil d'encodage.
				// La file est dimensionnee au MAXIMUM ; `cap` dit combien de
				// places sont reellement ouvertes -- 8 en mode « sauter », tout
				// en mode « gonfler ». Un seul tampon, donc pas de
				// reallocation pendant l'enregistrement.
				NkVpRecSlot slot[kNkvpRecQueueMax];
				int32 cap = kNkvpRecQueue;
				int32 head = 0, tail = 0;
				threading::NkMutex mtx;
				threading::NkThread th;
				bool stopThread = false;
		};
		// DEUX SOURCES, UNE SEULE MECANIQUE. La vue est lue par l'hote ; la
		// FENETRE ENTIERE (tutoriel) est photographiee par l'application, qui
		// pousse ses pixels ici. File, fil d'encodage, cadence, formats,
		// pause et abandon sont ecrits une fois et servent les deux -- les
		// dupliquer dans main.cpp aurait fait diverger les deux comportements
		// au premier correctif.
		static NkVpRec nkvpRecView;
		static NkVpRec nkvpRecTuto;
		static int32 nkvpOutFastMask = 1 | 2 | 4;
		static const int32 kNkvpOutFastCount = 3;
		static const char *const kNkvpOutFastNames[kNkvpOutFastCount] = {
			"Cible fixe entre les images", "Lecture differee (anneau)",
			"Encodage pendant le rendu"};
		// Machine du rendu hors bande. Le redimensionnement de la cible et la
		// lecture des pixels ne peuvent pas avoir lieu dans la meme image : le
		// GPU doit avoir rendu ENTRE les deux. Les phases sont donc etalees sur
		// plusieurs images -- 1 pose la taille et la camera, 2 laisse rendre,
		// 3 lit et sauve, puis tout est restaure.
		static int32 nkvpOutPhase = 0;
		static uint32 nkvpOutSaveW = 0, nkvpOutSaveH = 0;
		static int32 nkvpOutSaveCam = -1;
		static int32 nkvpOutStep = -1;	  // -1 = principale, sinon indice d'incrustation
		static char nkvpOutLastPath[300] = {};
		static bool nkvpOutLastOk = false;
		// Modes restant a produire pendant CE rendu, et le mode de la vue a
		// restituer quand tout sera fini.
		static int32 nkvpOutModeQueue[kNkvpOutModeCount] = {};
		static int32 nkvpOutModeCount = 0;
		static int32 nkvpOutModeIdx = 0;
		static int32 nkvpOutSaveShading = 0;
		// Habillage a restituer apres la sortie : il est coupe le temps du
		// rendu (une lumiere n'existe dans l'image que par son effet).
		// ── AIDES VISUELLES DANS LE RENDU (Rihen) ───────────────────────────
		// Coupees par defaut -- une lumiere n'existe dans une image que par son
		// effet -- mais RECOUVRABLES : on veut parfois montrer justement la
		// grille, un symbole de lumiere ou le cadre d'une camera, dans une
		// planche pedagogique ou une capture d'explication. Le masquage est
		// donc une OPTION, pas une regle du moteur.
		// CHAQUE AIDE EST DISTINCTE (Rihen). Les quatre traits de la grille ont
		// leur propre case, comme dans l'onglet Affichage : on veut pouvoir
		// garder les AXES du plan sans la grille, ou l'inverse. Les regrouper
		// obligeait a tout prendre ou tout laisser.
		// Bits : 1 grille, 2 lignes fines, 4 lignes majeures, 8 axes du plan,
		// 16 symboles de lumiere, 32 reperes (vides), 64 cameras,
		// 128 poignees de gizmo, 256 HUD, 512 curseur 3D.
		static int32 nkvpOutAids = 0;
		// Bit 1024 : le SOL INFINI. Ce n'est pas une aide comme les autres --
		// c'est du DECOR, et il se rend comme de la geometrie -- mais il n'a pas
		// d'existence dans la hierarchie, donc la colonne camera ne peut pas
		// l'exclure. Il lui faut sa propre case. Coupe d'office avec le fond
		// transparent (demander un detourage et garder un damier n'a pas de
		// sens), mais RECUPERABLE : une ombre portee au sol donne du poids a un
		// objet detoure.
		// Bit 2048 : SOL EN RECEPTEUR D'OMBRE. Le sol ne se peint plus mais
		// garde l'ombre qu'il recoit : un objet detoure conserve son ombre
		// portee, donc son poids, au lieu de flotter. C'est le complement
		// naturel du fond transparent.
		static const int32 kNkvpOutAidCount = 12;
		static const char *const kNkvpOutAidNames[kNkvpOutAidCount] = {
			"Grille",		   "Lignes fines",		"Lignes majeures",	  "Axes du plan",
			"Symboles de lumiere", "Reperes (vides)", "Cameras",			  "Poignees de gizmo",
			"Informations (HUD)",  "Curseur 3D",	  "Sol infini",
			"Sol : ombre seule"};
		static bool nkvpOutSaveGizmoHidden = false;
		static bool nkvpOutSaveGrid = false;
		static bool nkvpOutSaveMinor = false, nkvpOutSaveMajor = false, nkvpOutSaveAxes = false;
		static bool nkvpOutSaveHud = false;
		static bool nkvpOutSaveLightGiz = true;
		// Couleur de fond courante (le moteur ne la relit pas) et etat du ciel,
		// pour les rendre apres un rendu a fond transparent.
		static float32 nkvpBgColor[3] = {0.05f, 0.05f, 0.07f};
		static bool nkvpOutSaveSky = true;
		static bool nkvpOutSaveFloor = true;
		// Definies plus bas : elles ont besoin de l'etat de la demo, declare
		// apres ce bloc.
		static bool HostShowLightGizmos();
		static void HostSetShowLightGizmos(bool on);
		// Quel nom de base porte le fichier produit : 0 le rendu, 1 la capture
		// de la vue, 2 le tutoriel. Le TRAVAIL est le meme, seul le nom change.
		static int32 nkvpOutNaming = 0;
		// Declarations avancees : le rappel clavier (pave 0) precede les
		// definitions, placees pres des autres fonctions hote.
		void Demo3DHostViewCamera(int32 node);
		bool Demo3DHostToggleCameraView();
		static int32 HostSceneCameras(int32 *out, int32 cap);
		void Demo3DHostCameraFrame(float32 *xywh);
		void Demo3DHostSetCameraView(int32 node); // la sortie pose la source avant de rendre
		int32 Demo3DHostShading();				 // la sortie produit un fichier par mode coche
		void Demo3DHostSetShading(int32 mode);
		bool Demo3DHostSkyVisible();			 // coupe le temps d'un rendu a fond transparent
		void Demo3DHostSetSkyVisible(bool on);
		// UNE SEULE VERITE optique pour la camera : le WIDGET (pyramide), le
		// rendu en vue camera, le voile et la capture derivent tous du couple
		// focale + format. Format Full HD en v1 ; la pastille Output le pilotera.
		static constexpr float32 kCamFrameMargin = 0.86f;
		// LE RAPPORT DE LA CAMERA EST CELUI DE LA SORTIE (c'etait annonce ici
		// meme : « Full HD en v1 ; la pastille Output le pilotera »). Le cadre
		// dessine dans la vue, le voile et le rendu derivent tous de cette
		// fonction : regler la sortie en 1:1 ou en vertical se voit donc
		// immediatement dans la vue camera, sans quoi on cadrerait sur un
		// rectangle qui n'est pas celui qu'on produit.
		static float32 HostCamAspect();
		// TYPE de camera (Rihen) : perspective (defaut) ou ORTHOGRAPHIQUE.
		// En ortho, la demi-hauteur du cadre = l'ECHELLE Y du noeud (decision
		// consignee : l'echelle regle le cadrage ortho, comme Blender).
		static bool nkvpUserCamOrtho[kNkvpMaxUser] = {};
		void Demo3DHostGetCameraPose(float32 *t3, float32 *dist, float32 *yaw, float32 *pitch,
									 bool *ortho);
		void Demo3DHostSetCameraPose(const float32 *t3, float32 dist, float32 yaw, float32 pitch,
									 bool ortho);
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
			// Dans l'editeur de model : SEUL le masquage pose dans le model.
			// En scene : le sien OU celui du model (le model se propage, pas
			// l'inverse).
			if (nkvpDocIsModel)
				return nkvpMeshHidden[n];
			return nkvpObjHidden[n] || nkvpMeshHidden[n];
		}
		// APPARTENANCE : elle vaut pour LE NOEUD, jamais par heritage. Un
		// enfant reste chez lui quand son parent part ailleurs (isolation)
		// ou disparait -- sinon il devenait invisible ET inselectionnable
		// dans sa propre scene (constate par Rihen).
		static bool HostNodeForeign(int32 n) {
			return n >= 0 && n < kNkvpMaxNodes && nkvpSceneOf[n] != nkvpCurScene;
		}
		// La chaine d'ancetres s'ARRETE des qu'un ancetre est etranger au
		// document ou supprime : au-dela, son etat ne nous concerne plus.
		static bool HostChainBreaks(int32 p) {
			return p >= 0 && p < kNkvpMaxNodes &&
				   (nkvpDeleted[p] || HostNodeForeign(p));
		}
		// VERROU PROPRE au document : celui de la scene ne verrouille pas dans
		// le model, et celui du model ne verrouille pas dans la scene -- il n'y
		// a pas d'importance a ce niveau (Rihen).
		//
		// C'est CETTE separation qui rend le model etanche aux reglages de scene.
		// Une precedente version faisait plutot ignorer a la racine du model son
		// PROPRE drapeau : elle rendait du meme coup impossible de masquer ou de
		// verrouiller le model DANS son editeur. La separation par contexte suffit,
		// et la remontee s'arrete de toute facon a l'ancetre reste dans la scene
		// (etranger au document).
		static bool HostLockedOwn(int32 n) {
			if (n < 0 || n >= 160)
				return false;
			return nkvpDocIsModel ? nkvpMeshLocked[n] : nkvpObjLocked[n];
		}
		// ── EXCLU DU RENDU (colonne camera de la hierarchie, Rihen) ─────────
		// Distinct de l'OEIL : un objet peut rester VISIBLE dans la vue -- on
		// travaille avec -- tout en etant absent de l'image produite. C'est le
		// pendant de l'icone appareil photo de Blender. Comme le masquage et le
		// cadenas, il s'HERITE : exclure un parent exclut son sous-arbre, et
		// chaque enfant garde son propre drapeau, restitue au retour.
		static bool nkvpNoRender[kNkvpMaxNodes] = {};
		static bool HostNoRenderEff(int32 n) {
			for (int32 g = 0; g < kNkvpMaxNodes && n >= 0; ++g) {
				if (n < kNkvpMaxNodes && nkvpNoRender[n])
					return true;
				const int32 pa = nkvpParentOf[n];
				if (HostChainBreaks(pa))
					break;
				n = pa;
			}
			return false;
		}
		static bool HostHiddenEff(int32 n) {
			// PENDANT UNE SORTIE, un noeud exclu du rendu compte comme cache.
			// C'est ici -- le point de passage unique de la visibilite -- que
			// l'exclusion doit se brancher : tout ce qui interroge la visibilite
			// (meshes, widgets, liseres) le saute alors sans qu'on ait a le
			// traiter en dix endroits, et sans risquer d'en oublier un.
			if (nkvpOutPhase != 0 && HostNoRenderEff(n))
				return true;
			if (HostNodeForeign(n))
				return true; // lui-meme vit dans un autre document
			for (int32 g = 0; g < kNkvpMaxNodes && n >= 0; ++g) {
				if (HostNodeHiddenOwn(n))
					return true;
				const int32 pa = nkvpParentOf[n];
				if (HostChainBreaks(pa))
					break;
				n = pa;
			}
			return false;
		}
		static bool HostLockedEff(int32 n) {
			for (int32 g = 0; g < kNkvpMaxNodes && n >= 0; ++g) {
				if (HostLockedOwn(n))
					return true;
				const int32 pa = nkvpParentOf[n];
				if (HostChainBreaks(pa))
					break;
				n = pa;
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
		// ── MATERIAUX DU PROJET (pastille Materiau) ─────────────────────────
		// Un materiau est une RESSOURCE NOMMEE, independante des objets : on le
		// cree une fois, on l'assigne a plusieurs cibles, on le retouche et
		// toutes suivent. Les champs sont ceux de NkPBRParams : la SAUVEGARDE
		// passera par NkMaterialAsset + NkMaterialLibrary (.nkasset, deja
		// prevus par NKRenderer) -- pas de format maison. L'edition NODALE
		// evaluera son graphe vers ces memes parametres, l'edition directe
		// ci-dessous restant toujours possible ; les surcharges par objet du
		// panneau Modele sont des retouches PAR-DESSUS le materiau assigne.
		// ── LES QUATRE CANAUX D'UN MATERIAU ────────────────────────────────
		// Le moteur les porte depuis toujours (SetAlbedoMap / SetNormalMap /
		// SetORMMap / SetEmissiveMap) ; seule la COULEUR etait reglable ici.
		// Un seul indice de canal traverse desormais tout le chemin -- etat,
		// API hote, panneau -- plutot que quatre copies de la meme fonction :
		// c'est ce qui a fait diverger les combos par le passe.
		//   0 COULEUR (albedo)   1 NORMALE (relief)
		//   2 ORM (occlusion/rugosite/metallique empaquetees, standard glTF)
		//   3 EMISSIF (ce que la surface emet)
		//   4 HAUTEUR (parallax : BLANC = haut ; l'echelle « Parallax » dose)
		static constexpr int32 kNkvpMatChanCount = 5;
		static const char *const kNkvpMatChanNames[kNkvpMatChanCount] = {
			"Couleur", "Normale", "ORM", "Emissif", "Hauteur"};
		struct NkVpProjMat {
				bool used;
				// apercu : 0 plan, 1 sphere, 2 cube, 3 liquide, 4 cheveux, 5 tissu,
			// 6 tete. Valeurs SERIALISEES (« apercu ») : on ajoute a la fin, on ne
			// renumerote pas.
			int8 prevShape;
				char name[32];
				// Chemins des quatre canaux ("" = pas de texture, les valeurs
				// numeriques font alors foi). MAX_PATH chacun.
				char maps[kNkvpMatChanCount][260];
				float32 albedo[3];
				float32 rough, metal;
				// PHYSIQUE DE SURFACE (2026-08-09) : vernis et diffusion — le shader
				// les calculait depuis longtemps, le panneau ne les proposait pas
				// (passation §5 : « gain le moins cher »). La couleur de diffusion
				// suit l'albedo : c'est la matiere elle-meme qui transmet sa teinte.
				float32 clearcoat;	// 0..1
				float32 ccRough;	// rugosite du vernis, 0..1
				float32 subsurface; // 0..1
				// INTENSITES des canaux qui en ont une : le relief se dose (0 =
				// normale ignoree, 1 = pleine), l'emissif aussi. Sans texture,
				// elles n'ont pas d'effet -- c'est voulu, pas un oubli.
				float32 nrmStrength;
				float32 emiStrength;
				float32 emissive[3]; // teinte emise, meme sans texture
				// ECHELLE DU PARALLAX (0 = coupe) : ne sert qu'avec le canal
				// Hauteur — comme les intensites, sans texture elle n'a pas d'effet.
				float32 parallax;
				// OMBRE d'un objet transparent : 0 pleine, 1 proportionnelle
				// (tramage suivant l'opacite), 2 aucune. Defaut : proportionnelle.
				int32 shadowMode;
				// ── MELANGE (etape 1, exigence Blender/UE de Rihen) ─────────
				// mixWith = emplacement+1 du materiau B (0 = pas de melange) ;
				// mixSource : 0=Facteur constant, 1..4=couleur de sommets RGBA,
				// 5=UV.x, 6=UV.y ; mixFactor ne sert qu'en source Facteur.
				int8 mixWith;
				int8 mixSource;
				float32 mixFactor;
				// ── TYPE DE MATERIAU (11 aout — « tout ce qui est public ») ──
				// Valeur brute de NkMaterialType (0 PBR, 3 peau, 4 cheveux,
				// 5 verre, 6 tissu, 7 carrosserie, 8 feuillage, 9 eau,
				// 11 emissif, 20 toon, 21 toon encre, 22 anime, 60 sans
				// eclairage). Le melange (mixWith) prime : il impose LAYERED_V1.
				uint8 matType;
				// Reglages PBR restes sans curseur jusqu'ici.
				float32 alpha;	// opacite (albedo.a, file transparente)
				float32 aniso;	// anisotropie du lobe
				float32 sheenV; // voile textile
				// Famille TOON (NkToonParams cote moteur).
				float32 toonThresh, toonSmooth;
				float32 toonShadow[3];
				float32 outlineW;
				float32 outlineCol[3];
				float32 rimI;
				float32 rimCol[3];
				float32 specHard;
		};
		static constexpr int32 kNkvpMaxProjMats = 64;
		static NkVpProjMat nkvpProjMats[kNkvpMaxProjMats] = {};
		/// Emplacement RESERVE au materiau magenta « aucun materiau ». Declare ici,
		/// avec le registre : la creation, la lecture et le rendu doivent tous le
		/// connaitre, et il est plus simple de le poser a la source que de le
		/// declarer trois fois.
		static const int32 kNkvpMissingMat = kNkvpMaxProjMats - 1;
		// Cote MOTEUR d'un materiau a texture : la teinte/rugosite/metallique
		// passent par le draw call, mais une TEXTURE exige une vraie instance
		// de materiau (meme mecanique que le sol carrele).
		static NkTexHandle nkvpProjMatChanTex[kNkvpMaxProjMats][kNkvpMatChanCount] = {};
		static NkMaterial *nkvpProjMatEng[kNkvpMaxProjMats] = {};
		/// LES MEMES MATERIAUX, DANS LE RENDERER DE L'APERCU. Une instance
		/// appartient au systeme de materiaux qui l'a creee : chaque renderer a sa
		/// collection (64 emplacements, son UBO). Passer une instance de la vue 3D
		/// au renderer d'apercu n'a donc aucun sens -- il lui faut les siennes,
		/// construites par le MEME code depuis le MEME etat.
		static NkMaterial *nkvpProjMatPrev[kNkvpMaxProjMats] = {};
		/// LA VIGNETTE D'UN MATERIAU, en PNG encode base64. Elle vit DANS le
		/// materiau (Rihen, 14 aout) : un .nkmat se deplace alors sans perdre son
		/// image, la ou un PNG voisin se serait separe de lui au premier
		/// deplacement de fichier.
		///
		/// GARDEE HORS de NkVpProjMat, volontairement : cette structure est
		/// comparee OCTET A OCTET pour savoir si l'apercu doit etre reconstruit
		/// (memcmp). Une chaine dedans casserait la comparaison -- et une vignette
		/// n'est pas un reglage : elle est le RESULTAT des reglages.
		static NkString nkvpProjMatThumb[kNkvpMaxProjMats];
		/// OU ECRIVENT LES FACADES, et AVEC QUEL renderer. Par defaut la vue 3D ; le
		/// temps de reconstruire un apercu, on bascule sur l'autre jeu. C'est ce qui
		/// evite de recopier les quarante lignes d'application des reglages -- une
		/// copie aurait diverge au premier reglage ajoute.
		static NkMaterial **nkvpMatCible = nkvpProjMatEng;
		static renderer::NkRenderer *nkvpMatRdCible = nullptr;
		static inline NkMaterial *&NkvpMatEng(int32 i) { return nkvpMatCible[i]; }
		/// Le renderer sur lequel les facades travaillent. Declare ici, DEFINI plus
		/// bas : `hst` (l'etat de l'hote) n'existe pas encore a cette hauteur.
		static renderer::NkRenderer *NkvpMatRd();
		static int32 nkvpMatSerial = 0; // numerote « Materiau.NNN », jamais reutilise
		// Assignation par NOEUD, stockee en indice+1 : le zero de l'init
		// statique veut naturellement dire « aucun materiau ». C'est le
		// materiau ACTIF, celui que le rendu applique.
		static int32 nkvpNodeMatP1[kNkvpMaxNodes] = {};
		// ── LA LISTE DES MATERIAUX D'UN OBJET (12 aout) ──────────────────
		// Modele fixe avec Rihen : DEUX listes distinctes. Celle du PROJET
		// (nkvpProjMats, un .nkmat par entree sur le disque) et celle de
		// chaque OBJET — les materiaux qui lui sont associes, parmi lesquels
		// un seul est actif. Retirer depuis la pastille sort le materiau de
		// CETTE liste sans rien detruire ; le « + » l'y remet, sans avoir a
		// en recreer un. Supprimer, lui, ne se fait que depuis le navigateur
		// de projet, et delie alors TOUS les objets porteurs.
		//
		// Stockage en indice+1 pour la meme raison que ci-dessus. Le nombre
		// d'emplacements n'est PAS un chiffre choisi a la main : un objet ne
		// peut pas porter plus de materiaux qu'il n'en existe dans le projet
		// (« pourquoi juste 8 ? un modele pourrait avoir un materiau par mesh
		// ou par espace de vertices », Rihen, 12 aout — et il a raison : un
		// glTF importe en aligne couramment vingt ou trente). La borne suit
		// donc celle du projet, et le jour ou l'une monte, l'autre suit.
		// Cout : 160 noeuds x 64 x 4 o = 40 Ko de statique, negligeable.
		//
		// Un materiau PAR FACE (ou par groupe de sommets) viendra plus tard
		// par-dessus : chaque face portera l'INDICE de son emplacement dans
		// cette liste — c'est le modele de Blender et de glTF. La liste
		// elle-meme reste bornee par le projet.
		static constexpr int32 kNkvpMaxMatsPerNode = kNkvpMaxProjMats;
		static int32 nkvpNodeMatsP1[kNkvpMaxNodes][kNkvpMaxMatsPerNode] = {};

		// Combien de materiaux cet objet porte-t-il ?
		static int32 HostNodeMatCount(int32 node) {
			if (node < 0 || node >= kNkvpMaxNodes)
				return 0;
			int32 n = 0;
			for (int32 k = 0; k < kNkvpMaxMatsPerNode; ++k)
				if (nkvpNodeMatsP1[node][k] > 0)
					++n;
			return n;
		}

		// Le k-ieme materiau de l'objet, ou -1. L'ordre des emplacements est
		// STABLE : un retrait laisse un trou plutot que de tout decaler, sinon
		// la selection de l'interface designerait un autre materiau apres coup.
		static int32 HostNodeMatAt(int32 node, int32 k) {
			if (node < 0 || node >= kNkvpMaxNodes || k < 0)
				return -1;
			int32 seen = 0;
			for (int32 i = 0; i < kNkvpMaxMatsPerNode; ++i) {
				if (nkvpNodeMatsP1[node][i] <= 0)
					continue;
				if (seen == k)
					return nkvpNodeMatsP1[node][i] - 1;
				++seen;
			}
			return -1;
		}

		// Associe un materiau a l'objet s'il n'y est pas deja. Renvoie false si
		// la liste est pleine.
		static bool HostNodeMatAdd(int32 node, int32 slot) {
			if (node < 0 || node >= kNkvpMaxNodes || slot < 0 || slot >= kNkvpMaxProjMats)
				return false;
			for (int32 k = 0; k < kNkvpMaxMatsPerNode; ++k)
				if (nkvpNodeMatsP1[node][k] == slot + 1)
					return true; // deja la : rien a faire, et surtout pas de doublon
			for (int32 k = 0; k < kNkvpMaxMatsPerNode; ++k)
				if (nkvpNodeMatsP1[node][k] <= 0) {
					nkvpNodeMatsP1[node][k] = slot + 1;
					// UN OBJET QUI N'AVAIT RIEN PREND CELUI-CI POUR ACTIF (Rihen,
					// 13 aout). Sans cela, l'objet restait MAGENTA apres qu'on lui
					// eut ajoute un materiau : l'actif pointait encore sur le
					// magenta « aucun materiau », qui n'est justement pas un choix
					// de l'utilisateur mais le constat d'une absence -- absence qui
					// vient de cesser. Un actif deja choisi, lui, n'est pas touche :
					// ajouter a une liste n'est pas assigner.
					const int32 actuel = nkvpNodeMatP1[node] - 1;
					if (actuel < 0 || actuel == kNkvpMissingMat)
						nkvpNodeMatP1[node] = slot + 1;
					return true;
				}
			return false;
		}

		/// Le magenta « aucun materiau » : defini avec le registre, plus bas.
		/// Declare ICI car le RETRAIT l'assigne — c'est lui qui rend l'absence
		/// visible, exactement comme la creation assigne le materiau par defaut.
		static int32 HostEnsureMissingMat();

		// Retire un materiau de CET objet seulement.
		//
		// LE DERNIER SE RETIRE AUSSI, depuis le 13 aout (Rihen) : « permettre de
		// supprimer tous les materiaux, mais un objet sans materiau sera en
		// magenta ». La regle precedente -- toujours au moins un materiau --
		// interdisait un geste legitime pour eviter un cas d'affichage ; le cas
		// est desormais traite la ou il se voit, au rendu, par une couleur qui
		// signale l'absence au lieu de la masquer.
		static bool HostNodeMatRemove(int32 node, int32 slot) {
			if (node < 0 || node >= kNkvpMaxNodes || slot < 0)
				return false;
			for (int32 k = 0; k < kNkvpMaxMatsPerNode; ++k) {
				if (nkvpNodeMatsP1[node][k] != slot + 1)
					continue;
				nkvpNodeMatsP1[node][k] = 0;
				// Si c'etait l'actif, l'objet bascule sur le premier restant. S'il
				// n'en reste AUCUN, il recoit le materiau magenta « aucun materiau ».
				//
				// MEME PRINCIPE QUE LE MATERIAU PAR DEFAUT (Rihen, 13 aout) : celui-ci
				// est assigne A LA CREATION du maillage, et tout le pipeline le lit
				// ensuite sans rien savoir de particulier. Le magenta est assigne ICI,
				// au retrait du dernier, et se lit exactement pareil. Le rattraper au
				// moment du rendu, comme je l'avais fait, obligeait a rejouer a la
				// main ce que le chemin normal fait tout seul -- et ne marchait pas.
				if (nkvpNodeMatP1[node] == slot + 1) {
					const int32 first = HostNodeMatAt(node, 0);
					nkvpNodeMatP1[node] =
						(first >= 0) ? first + 1 : HostEnsureMissingMat() + 1;
				}
				return true;
			}
			return false;
		}
		static int32 HostEnsureDefaultMat(); // defini avec le registre, plus bas
		// ── LES REGLAGES D'UN MATERIAU DE PROJET, POSES SUR UN DRAW CALL ────
		// Extrait de HostMatHook, et c'est le coeur du sujet : les facades de
		// materiau n'ecrivent QUE dans l'etat -- ce sont ces surcharges-ci qui
		// portent la matiere jusqu'au rendu. Rihen l'a dit d'un mot : « elle ne
		// touche pas l'instance dans la previsualisation mais elle touche dans la
		// vue 3D ».
		//
		// La vue 3D arrive ici par un NOEUD, l'apercu par un EMPLACEMENT, mais ce
		// qui est applique doit etre le MEME : un apercu qui montrerait autre
		// chose que la scene n'aurait aucun interet.
		template <typename TDC>
		static void HostMatSlotToDC(int32 pm, TDC &dc) {
			if (pm < 0 || pm >= kNkvpMaxProjMats || !nkvpProjMats[pm].used)
				return;
			dc.tint.x = nkvpProjMats[pm].albedo[0];
			dc.tint.y = nkvpProjMats[pm].albedo[1];
			dc.tint.z = nkvpProjMats[pm].albedo[2];
			// L'OPACITE part d'ici : c'est dc.alpha qui route le draw vers la file
			// TRANSPARENTE du moteur -- sans cette ligne, le curseur du panneau
			// etait muet (constate par Rihen, 11 aout).
			dc.alpha = nkvpProjMats[pm].alpha;
			dc.metallic = nkvpProjMats[pm].metal;
			dc.roughness = nkvpProjMats[pm].rough;
			// Physique de surface : la couleur de diffusion suit l'albedo (la
			// matiere transmet sa propre teinte, pas du blanc).
			dc.clearcoat = nkvpProjMats[pm].clearcoat;
			dc.clearcoatRough = nkvpProjMats[pm].ccRough;
			dc.subsurface = nkvpProjMats[pm].subsurface;
			dc.subsurfaceColor = {nkvpProjMats[pm].albedo[0], nkvpProjMats[pm].albedo[1],
								  nkvpProjMats[pm].albedo[2]};
		}

		template <typename TDC>
		static void HostMatHook(int32 i, TDC &dc) {
			if (i < 0 || i >= kNkvpMaxNodes)
				return;
			// Le MATERIAU DU PROJET s'applique d'abord (c'est la base), les
			// surcharges par objet du panneau Modele le retouchent ensuite --
			// l'ordre est le contrat : assigner un materiau ne detruit pas une
			// retouche locale, et retirer la retouche rend le materiau.
			{
				const int32 pm = nkvpNodeMatP1[i] - 1;
				// ── AUCUN MATERIAU : MAGENTA ────────────────────────────────
				// Depuis qu'on peut retirer TOUS les materiaux d'un objet (Rihen,
				// 13 aout), l'absence doit se VOIR. Le magenta est la convention
				// d'Unreal, de Source et d'Unity, et pour une raison precise : le
				// noir ressemble a un objet correctement rendu mais non eclaire,
				// donc on cherche le probleme du cote des lumieres. Aucune matiere
				// reelle n'est magenta pur -- la couleur ne suggere pas un defaut,
				// elle l'annonce.
				//
				// AUCUN CAS PARTICULIER ICI. Un objet sans materiau s'est vu
				// assigner le magenta au moment du retrait ; il arrive donc avec un
				// materiau valide, lu par le chemin ordinaire ci-dessous.
				HostMatSlotToDC(pm, dc); // MEME application que l'apercu
			}
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
				// Primitives du menu AJOUTER (vraies formes, regle de Rihen).
				NkMeshHandle meshCylinder;
				NkMeshHandle meshCone;
				NkMeshHandle meshIco;
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
				// Ctrl+Alt+0 : aligner la camera ACTIVE sur la vue actuelle --
				// traite dans la frame (le rappel clavier n'a pas la camera).
				bool camAlignPending = false;
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
		
		// ── Aretes n-gon des MAILLAGES UTILISATEUR (menu Ajouter / import) ─────────
		// En mode filaire n-gon, RIEN n'est rasterise : sans leurs aretes dans le
		// lot, les objets de l'utilisateur etaient INVISIBLES en fil de fer alors
		// que la demo, elle, y figurait (constate par Rihen : seul le contour de
		// selection subsistait). Caches par emplacement, remplis a la reconstruction.
		static NkVector<NkVec3f> sUserWireEdges[kNkvpMaxUser];
		static uint32 sUserOff[kNkvpMaxUser] = {};
		static uint32 sUserCnt[kNkvpMaxUser] = {};
		static NkMat4f sUserXf[kNkvpMaxUser];

		// Transform monde d'un maillage utilisateur — MEME recette que son rendu
		// (position + offsets gizmo, rotation composee, echelle, dimensions).
		static NkMat4f Demo3D_UserWireXform(Demo3DState *st, int32 u) {
			const int32 e = (kNkvpFirstUser + u) - 90; // meme indexation que le rendu
			NkMat4f W = HostEmptyXform(e, true);	   // point de passage unique
			const int32 un = kNkvpFirstUser + u;
			if (nkvpBaseSet[un])
				W = W * NkMat4f::Scale({nkvpDimFactor[un][0], nkvpDimFactor[un][1],
										nkvpDimFactor[un][2]});
			return W;
		}

		// Remplit la tranche d'un maillage UTILISATEUR dans le batch monde.
		static void Demo3D_UserWireFill(Demo3DState *st, int32 u, const NkMat4f &W) {
			const NkVec4f col = Demo3D_WireColor();
			float32 *dst = st->wireVerts.Data() + (uint64)sUserOff[u] * 7;
			for (uint32 k = 0; k < sUserCnt[u]; k++) {
				const NkVec3f w = W * sUserWireEdges[u][k];
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
			// LA VISIBILITE fait partie de l'empreinte : ouvrir/fermer un oeil (ou
			// charger une scene utilisateur ou toute la demo est masquee) doit
			// reconstruire le lot -- sinon le fil de fer d'objets INVISIBLES
			// restait a l'ecran (Rihen : « 96 elements pourtant j'ai juste mon
			// cube »).
			for (int32 i = 0; i < Demo3DState::kNumObj; i++)
				if (HostHiddenEff(i))
					stamp += (i + 1) * 1009;
			// Les MAILLAGES UTILISATEUR font partie de l'empreinte : creation,
			// suppression, changement de primitive/mesh ou de visibilite.
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				const uint8 uk = nkvpUserKind[u];
				if (uk < 1 || uk > 3)
					continue;
				const int32 un = kNkvpFirstUser + u;
				stamp += (u + 3) * (HostHiddenEff(un) ? 8191 : 127);
				stamp += (int32)nkvpUserSub[u] * 31 + (nkvpUserMesh[u].IsValid() ? 5 : 0);
			}
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
					if (HostHiddenEff(i))
						continue; // invisible (oeil ferme / scene sans la demo) : pas de fantome
					const NkVector<NkVec3f> *src = Demo3D_WireSrc(st, ms, i);
					st->wireCnt[i] = (uint32)src->Size();
					off += st->wireCnt[i];
				}
				// ── MAILLAGES UTILISATEUR : leurs aretes au meme lot ────────────
				for (int32 u = 0; u < kNkvpMaxUser; ++u) {
					sUserCnt[u] = 0;
					const uint8 uk = nkvpUserKind[u];
					if (uk < 1 || uk > 3)
						continue;
					const int32 un = kNkvpFirstUser + u;
					if (HostHiddenEff(un))
						continue;
					NkMeshHandle mh = st->meshPlane;
					if (uk == 1)
						mh = nkvpUserSub[u] == 1 ? st->meshIco : st->meshSphere;
					else if (uk == 2)
						mh = nkvpUserSub[u] == 1
								 ? st->meshCylinder
								 : (nkvpUserSub[u] == 2 ? st->meshCone : st->meshCube);
					if (nkvpUserMesh[u].IsValid())
						mh = nkvpUserMesh[u];
					sUserWireEdges[u].Clear();
					Demo3D_NgonEdgesOf(ms, mh, sUserWireEdges[u]);
					sUserOff[u] = off;
					sUserCnt[u] = (uint32)sUserWireEdges[u].Size();
					off += sUserCnt[u];
				}
				st->wireTotalV = off;
				st->wireVerts.Resize(off * 7);
				for (int32 i = 0; i < Demo3DState::kNumObj; i++) {
					if (!st->wireCnt[i])
						continue;
					Demo3D_WireFillSlice(st, i, Demo3D_WireSrc(st, ms, i));
					st->wireXform[i] = st->objXform[i];
				}
				for (int32 u = 0; u < kNkvpMaxUser; ++u) {
					if (!sUserCnt[u])
						continue;
					const NkMat4f W = Demo3D_UserWireXform(st, u);
					Demo3D_UserWireFill(st, u, W);
					sUserXf[u] = W;
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
			// Un maillage UTILISATEUR deplace/tourne/mis a l'echelle : sa tranche
			// suit, comme celles de la demo.
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				if (!sUserCnt[u])
					continue;
				const NkMat4f W = Demo3D_UserWireXform(st, u);
				if (sUserXf[u] == W)
					continue;
				Demo3D_UserWireFill(st, u, W);
				sUserXf[u] = W;
				r3d->UpdateNgonWireLines(st->wireVerts.Data() + (uint64)sUserOff[u] * 7, sUserOff[u],
										 sUserCnt[u]);
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

		// ── RAYON -> MAILLAGE REEL ──────────────────────────────────────────────────
		// La selection/deselection au clic se decide sur les TRIANGLES du maillage
		// (regle de Rihen), pas sur un volume approche : le disque « rayon = plus
		// grande echelle » d'avant volait les clics du vide voisin des qu'un objet
		// grandissait -- cliquer a cote SELECTIONNAIT au lieu de deselectionner.
		// Moller-Trumbore en espace LOCAL : le rayon monde est ramene par l'inverse
		// du transform ; direction NON normalisee des deux cotes, donc le parametre
		// t reste celui du rayon MONDE et se compare entre objets.
		static bool Demo3D_RayMeshT(renderer::NkMeshSystem *ms, NkMeshHandle mesh,
									const NkMat4f &world, NkVec3f ro, NkVec3f rd, float32 &bestT) {
			if (!ms || !ms->HasCPUData(mesh))
				return false;
			const uint8 *vb = (const uint8 *)ms->GetVertices(mesh);
			const uint32 *ib = ms->GetIndices(mesh);
			const uint32 ic = ms->GetIndexCount(mesh);
			const uint32 stride = ms->GetVertexStride(mesh);
			if (!vb || !ib || ic < 3 || stride < sizeof(NkVec3f))
				return false;
			const NkMat4f inv = world.Inverse();
			const NkVec3f lo = inv * ro;
			const NkVec3f l1 = inv * NkVec3f{ro.x + rd.x, ro.y + rd.y, ro.z + rd.z};
			const NkVec3f ld{l1.x - lo.x, l1.y - lo.y, l1.z - lo.z};
			bool hit = false;
			for (uint32 k = 0; k + 2 < ic; k += 3) {
				const NkVec3f &A = *(const NkVec3f *)(vb + ib[k] * stride);
				const NkVec3f &B = *(const NkVec3f *)(vb + ib[k + 1] * stride);
				const NkVec3f &C = *(const NkVec3f *)(vb + ib[k + 2] * stride);
				const NkVec3f e1{B.x - A.x, B.y - A.y, B.z - A.z};
				const NkVec3f e2{C.x - A.x, C.y - A.y, C.z - A.z};
				const NkVec3f p{ld.y * e2.z - ld.z * e2.y, ld.z * e2.x - ld.x * e2.z,
								ld.x * e2.y - ld.y * e2.x};
				const float32 det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
				// DOUBLE FACE voulu (pas de test de signe) : un plan doit se
				// cliquer des deux cotes.
				if (det > -1e-9f && det < 1e-9f)
					continue;
				const float32 invDet = 1.f / det;
				const NkVec3f tv{lo.x - A.x, lo.y - A.y, lo.z - A.z};
				const float32 u = (tv.x * p.x + tv.y * p.y + tv.z * p.z) * invDet;
				if (u < 0.f || u > 1.f)
					continue;
				const NkVec3f q{tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z,
								tv.x * e1.y - tv.y * e1.x};
				const float32 v = (ld.x * q.x + ld.y * q.y + ld.z * q.z) * invDet;
				if (v < 0.f || u + v > 1.f)
					continue;
				const float32 t = (e2.x * q.x + e2.y * q.y + e2.z * q.z) * invDet;
				if (t > 1e-4f && t < bestT) {
					bestT = t;
					hit = true;
				}
			}
			return hit;
		}

		// Test PRECIS des objets de DEMO pour le gizmo : resout le MEME mesh que le
		// rendu (mesh edite prioritaire, sinon la primitive de l'index) et rejoue le
		// rayon-triangle ci-dessus. Le monde RECU est celui du MARQUEUR du gizmo
		// (base * Translate(centre AABB), cf. fitTarget) : on defait ce recentrage
		// pour retomber sur le transform du mesh.
		// Retour : 1 touche, 0 rate (autorite), -1 pas de donnees CPU (repli boite).
		struct Demo3DRayCtx {
				Demo3DState *st = nullptr;
				renderer::NkMeshSystem *ms = nullptr;
		};
		// ── LES REPERES QUE SEUL L'HOTE CONNAIT (Gimbal, Curseur, Parent) ─────
		// GIMBAL : les axes des angles d'EULER du noeud actif -- X est l'axe
		// monde, Z a subi les rotations X puis Y (chaque anneau correspond alors
		// a un angle du panneau, c'est ce qui rend le gimbal lisible).
		// CURSOR : notre curseur 3D n'a pas d'orientation -- aucun repere n'est
		// pose, l'entree retombe sur Local et ne ment pas.
		// PARENT : les axes MONDE du parent.
		// RAFRAICHIS CHAQUE IMAGE : poses seulement au changement d'orientation,
		// ils devenaient perimes des qu'on changeait d'objet actif.
		static void HostPushExtFrames(Demo3DState *st, int32 o) {
			if (!st)
				return;
			const int32 an = st->emptyGizmo.ActiveIndex();
			const int32 node = an >= 0 ? an + 90 : -1;
			auto poseAll = [&](int32 which, const NkVec3f &ax, const NkVec3f &az) {
				st->gizmo.SetExtFrame(which, ax, az);
				st->editGizmo.SetExtFrame(which, ax, az);
				st->lightGizmo.SetExtFrame(which, ax, az);
				st->emptyGizmo.SetExtFrame(which, ax, az);
			};
			auto clearAll = [&](int32 which) {
				st->gizmo.ClearExtFrame(which);
				st->editGizmo.ClearExtFrame(which);
				st->lightGizmo.ClearExtFrame(which);
				st->emptyGizmo.ClearExtFrame(which);
			};
			if (o == 3) {
				if (node >= 90 && node < kNkvpMaxNodes) {
					const float32 kD2Rg = 0.017453292f;
					const float32 *e = nkvpEmptyRotDeg[an];
					const NkMat4f Rx = NkMat4f::RotationX(NkAngle::FromRad(e[0] * kD2Rg));
					const NkMat4f Ry = NkMat4f::RotationY(NkAngle::FromRad(e[1] * kD2Rg));
					// Axes par TRANSFORMATION, jamais par lecture de lignes.
					poseAll(3, NkVec3f{1.f, 0.f, 0.f},
							(Rx * Ry) * NkVec3f{0.f, 0.f, 1.f});
				} else {
					clearAll(3);
				}
			}
			if (o == 6) {
				if (node >= 90 && node < kNkvpMaxNodes && nkvpParentOf[node] >= 0) {
					float32 pp[3], psc[3];
					NkMat4f pr;
					HostNodeWorldById(nkvpParentOf[node], pp, pr, psc);
					poseAll(6, pr * NkVec3f{1.f, 0.f, 0.f}, pr * NkVec3f{0.f, 0.f, 1.f});
				} else {
					clearAll(6); // sans parent : repli sur Local
				}
			}
		}

		// ── BASE D'ACCROCHE (Blender : « Snap Base ») ──────────────────────────
		// QUEL point de l'objet deplace cherche la cible :
		//   0 = LE PLUS PROCHE (defaut) : les sommets echantillonnes de la
		//       selection -- c'est le contact naturel, surface contre surface ;
		//   1 = PIVOT : le centre du gizmo. « Centre » et « Median » de Blender
		//       fusionnent ici : le pivot du gizmo SUIT deja le mode de pivot
		//       de l'application (median / actif / origines), les distinguer
		//       encore ferait deux entrees pour un meme point ;
		//   2 = OBJET ACTIF : l'origine du dernier objet selectionne.
		static int32 nkvpSnapBase = 0;

		// ── AIMANTATION GEOMETRIQUE : la reponse de l'hote au gizmo ────────────
		// Cible la plus proche (sommet / arete / face / centres) parmi les
		// MAILLAGES UTILISATEUR visibles et NON selectionnes -- l'objet en
		// deplacement s'aimanterait a ses propres sommets (distance ~0) et
		// resterait colle sur place, Blender l'exclut pareillement. Donnees CPU
		// des meshes, transform monde du noeud (parents compris).
		static int32 Demo3D_SnapQuery(void *user, int32 target, const NkVec3f &nearP,
									  const NkVec3f &origP, const NkVec3f &rayO,
									  const NkVec3f &rayD, float32 maxDist, NkVec3f &out,
									  NkVec3f *outN) {
			const Demo3DRayCtx *cx = (const Demo3DRayCtx *)user;
			if (!cx || !cx->st || !cx->ms || maxDist <= 0.f)
				return 0;
			auto *st = cx->st;
			auto *ms = cx->ms;
			// ── LE POINT D'ACCROCHE EST CELUI DE L'OBJET DEPLACE LE PLUS
			// PROCHE (Blender : base « Closest »), pas son centre. Accrocher le
			// CENTRE exigeait de faire chevaucher les deux objets de moitie
			// avant que la cible entre dans la portee -- l'aimantation ne se
			// declenchait jamais (constate par Rihen ; trace : cand=1 found=0
			// avec une portee de 28 cm).
			//
			// Les sommets des objets deplaces sont pris a leur pose de BASE
			// (nkvpEmptyPos n'est commite qu'en fin de glissement) puis
			// ramenes a la position libre par le meme delta que le pivot.
			NkVec3f srcPts[96];
			int32 nSrc = 0;
			// PIVOT COMME BASE : le point d'accroche est le centre du gizmo,
			// rien a echantillonner. C'est le comportement d'avant la base
			// « le plus proche » -- utile pour poser un objet PAR son origine
			// (un empty, une lumiere, un axe de perçage) plutot que par sa
			// surface.
			if (nkvpSnapBase == 1)
				srcPts[nSrc++] = nearP;
			else {
				NkVec3f pivBase{0.f, 0.f, 0.f};
				int32 nSel = 0;
				for (int32 u = 0; u < kNkvpMaxUser; ++u) {
					const int32 un = kNkvpFirstUser + u;
					if (nkvpDeleted[un] || !st->emptyGizmo.IsSelected(un - 90))
						continue;
					float32 wp[3], wsc[3];
					NkMat4f wr;
					HostNodeWorldById(un, wp, wr, wsc);
					pivBase = {pivBase.x + wp[0], pivBase.y + wp[1], pivBase.z + wp[2]};
					++nSel;
				}
				// OBJET ACTIF COMME BASE : l'origine du dernier selectionne,
				// deplacee du meme delta que le pivot. Sans actif valide, on
				// retombe sur le pivot -- jamais sur un point invente.
				if (nkvpSnapBase == 2) {
					const int32 an = st->emptyGizmo.ActiveIndex();
					const int32 anNode = an >= 0 ? an + 90 : -1;
					if (nSel > 0 && anNode >= kNkvpFirstUser && !nkvpDeleted[anNode]) {
						const float32 inv = 1.f / (float32)nSel;
						pivBase = {pivBase.x * inv, pivBase.y * inv, pivBase.z * inv};
						float32 ap[3], asc[3];
						NkMat4f ar;
						HostNodeWorldById(anNode, ap, ar, asc);
						srcPts[nSrc++] = {ap[0] + (nearP.x - pivBase.x),
										  ap[1] + (nearP.y - pivBase.y),
										  ap[2] + (nearP.z - pivBase.z)};
					} else {
						srcPts[nSrc++] = nearP;
					}
				} else if (nSel > 0) {
					const float32 inv = 1.f / (float32)nSel;
					pivBase = {pivBase.x * inv, pivBase.y * inv, pivBase.z * inv};
					const NkVec3f delta{nearP.x - pivBase.x, nearP.y - pivBase.y,
										nearP.z - pivBase.z};
					for (int32 u = 0; u < kNkvpMaxUser && nSrc < 96; ++u) {
						const uint8 uk = nkvpUserKind[u];
						if (uk < 1 || uk > 3)
							continue;
						const int32 un = kNkvpFirstUser + u;
						if (nkvpDeleted[un] || !st->emptyGizmo.IsSelected(un - 90))
							continue;
						NkMeshHandle mh = nkvpUserMesh[u];
						if (!mh.IsValid()) {
							const uint8 usv = nkvpUserSub[u];
							if (uk == 1)
								mh = usv == 1 ? st->meshIco : st->meshSphere;
							else if (uk == 2)
								mh = usv == 1 ? st->meshCylinder
											  : (usv == 2 ? st->meshCone : st->meshCube);
							else
								mh = st->meshPlane;
						}
						if (!mh.IsValid() || !ms->HasCPUData(mh))
							continue;
						const auto *sv = (const renderer::NkVertex3D *)ms->GetVertices(mh);
						const uint32 svc = ms->GetVertexCount(mh);
						if (!sv || !svc)
							continue;
						float32 wp[3], wsc[3];
						NkMat4f wr;
						HostNodeWorldById(un, wp, wr, wsc);
						const NkMat4f W = NkMat4f::Translate({wp[0], wp[1], wp[2]}) * wr *
										  NkMat4f::Scale({wsc[0], wsc[1], wsc[2]});
						// ECHANTILLONNAGE : au plus 96 points, pas regulier --
						// un maillage dense ne doit pas couter une seconde par
						// image, et ses sommets voisins sont redondants.
						const uint32 step = svc > 96u ? (svc / 96u) : 1u;
						for (uint32 i = 0; i < svc && nSrc < 96; i += step) {
							const NkVec3f p = W * sv[i].pos;
							srcPts[nSrc++] = {p.x + delta.x, p.y + delta.y, p.z + delta.z};
						}
					}
				}
			}
			// Repli : sans geometrie source (empty, camera), le point d'accroche
			// EST le pivot -- l'ancien comportement, qui reste juste pour eux.
			if (nSrc == 0)
				srcPts[nSrc++] = nearP;
			// L'ARETE PERPENDICULAIRE se mesure depuis le DEBUT du geste :
			// chaque point source est ramene a sa position d'ORIGINE par le
			// delta inverse du pivot. Le pied de perpendiculaire abaisse depuis
			// la position courante changerait a chaque image -- la cible
			// glisserait sous le curseur au lieu de tenir.
			NkVec3f srcOrig[96];
			{
				const NkVec3f gd{nearP.x - origP.x, nearP.y - origP.y, nearP.z - origP.z};
				for (int32 s = 0; s < nSrc; ++s)
					srcOrig[s] = {srcPts[s].x - gd.x, srcPts[s].y - gd.y, srcPts[s].z - gd.z};
			}
			float32 best = maxDist * maxDist;
			int32 found = 0;
			int32 cand = 0; // maillages REELLEMENT examines
			NkVec3f bestP{0.f, 0.f, 0.f};
			// Normale de la cible gagnante (faces et centres de face) : sert a
			// l'alignement de rotation. Les autres cibles n'en ont pas de
			// stable -- un sommet appartient a plusieurs faces.
			NkVec3f bestN{0.f, 0.f, 0.f};
			bool haveN = false;
			// VOLUME : etat propre -- la cible est SOUS LE CURSEUR, la portee
			// ecran du pivot ne s'y applique pas. On retient la traversee la
			// plus proche de la camera, tous maillages confondus.
			float32 volBestT = 1e30f;
			NkVec3f volP{0.f, 0.f, 0.f};
			int32 volFound = 0;
			// `p` est une cible de la scene : on cherche le point SOURCE le plus
			// proche d'elle, et on retient la POSITION QUE LE PIVOT doit prendre
			// pour que les deux coincident.
			auto consider = [&](const NkVec3f &p) {
				for (int32 s = 0; s < nSrc; ++s) {
					const float32 dx = p.x - srcPts[s].x, dy = p.y - srcPts[s].y,
								  dz = p.z - srcPts[s].z;
					const float32 d2 = dx * dx + dy * dy + dz * dz;
					if (d2 < best) {
						best = d2;
						bestP = {nearP.x + dx, nearP.y + dy, nearP.z + dz};
						found = 1;
					}
				}
			};
			// Point le plus proche d'un SEGMENT : evalue depuis chaque source.
			auto segClosest = [&](const NkVec3f &a, const NkVec3f &b) {
				const NkVec3f ab{b.x - a.x, b.y - a.y, b.z - a.z};
				const float32 l2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
				for (int32 s = 0; s < nSrc; ++s) {
					float32 t = 0.f;
					if (l2 > 1e-12f)
						t = ((srcPts[s].x - a.x) * ab.x + (srcPts[s].y - a.y) * ab.y +
							 (srcPts[s].z - a.z) * ab.z) /
							l2;
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					const NkVec3f q{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
					const float32 dx = q.x - srcPts[s].x, dy = q.y - srcPts[s].y,
								  dz = q.z - srcPts[s].z;
					const float32 d2 = dx * dx + dy * dy + dz * dz;
					if (d2 < best) {
						best = d2;
						bestP = {nearP.x + dx, nearP.y + dy, nearP.z + dz};
						found = 1;
					}
				}
			};
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				const uint8 uk = nkvpUserKind[u];
				if (uk < 1 || uk > 3)
					continue;
				const int32 un = kNkvpFirstUser + u;
				if (nkvpDeleted[un] || HostHiddenEff(un))
					continue;
				if (st->emptyGizmo.IsSelected(un - 90))
					continue; // pas soi-meme
				// LA MEME resolution de mesh que la soumission : le mesh
				// parametrique s'il existe, sinon la PRIMITIVE partagee -- un
				// cube n'a pas de mesh regenere, et ne considerer que
				// nkvpUserMesh le rendait invisible a l'aimantation (constate
				// par Rihen : « le sommet ne fonctionne pas »).
				NkMeshHandle mh = nkvpUserMesh[u];
				if (!mh.IsValid()) {
					const uint8 usv = nkvpUserSub[u];
					if (uk == 1)
						mh = usv == 1 ? st->meshIco : st->meshSphere;
					else if (uk == 2)
						mh = usv == 1 ? st->meshCylinder
									  : (usv == 2 ? st->meshCone : st->meshCube);
					else
						mh = st->meshPlane;
				}
				if (!mh.IsValid() || !ms->HasCPUData(mh))
					continue;
				++cand;
				const auto *vv = (const renderer::NkVertex3D *)ms->GetVertices(mh);
				const uint32 vc = ms->GetVertexCount(mh);
				if (!vv || !vc)
					continue;
				float32 wp[3], wsc[3];
				NkMat4f wr;
				HostNodeWorldById(un, wp, wr, wsc);
				const NkMat4f W = NkMat4f::Translate({wp[0], wp[1], wp[2]}) * wr *
								  NkMat4f::Scale({wsc[0], wsc[1], wsc[2]});
				if (target == 2) {
					// SOMMET : le plus proche.
					for (uint32 i = 0; i < vc; ++i)
						consider(W * vv[i].pos);
					continue;
				}
				const uint32 *ii = ms->GetIndices(mh);
				const uint32 ic = ms->GetIndexCount(mh);
				if (!ii || ic < 3)
					continue;
				if (target == 5) {
					// VOLUME : toutes les intersections du rayon de VISEE avec
					// ce maillage, triees ; le point d'accroche est le MILIEU
					// de la premiere traversee (entree/sortie) -- l'objet se
					// pose DANS la matiere visee, comme Blender. Un maillage
					// ouvert (une seule intersection) accroche sur elle.
					float32 ts[32];
					int32 nT = 0;
					for (uint32 t3 = 0; t3 + 2 < ic && nT < 32; t3 += 3) {
						const NkVec3f A = W * vv[ii[t3]].pos;
						const NkVec3f B = W * vv[ii[t3 + 1]].pos;
						const NkVec3f C = W * vv[ii[t3 + 2]].pos;
						// Moller-Trumbore, sans elagage de face arriere : une
						// SORTIE de volume est par nature une face arriere.
						const NkVec3f e0{B.x - A.x, B.y - A.y, B.z - A.z};
						const NkVec3f e1{C.x - A.x, C.y - A.y, C.z - A.z};
						const NkVec3f pv{rayD.y * e1.z - rayD.z * e1.y,
										 rayD.z * e1.x - rayD.x * e1.z,
										 rayD.x * e1.y - rayD.y * e1.x};
						const float32 det = e0.x * pv.x + e0.y * pv.y + e0.z * pv.z;
						if (det > -1e-9f && det < 1e-9f)
							continue;
						const float32 inv = 1.f / det;
						const NkVec3f tv{rayO.x - A.x, rayO.y - A.y, rayO.z - A.z};
						const float32 u = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
						if (u < 0.f || u > 1.f)
							continue;
						const NkVec3f qv{tv.y * e0.z - tv.z * e0.y,
										 tv.z * e0.x - tv.x * e0.z,
										 tv.x * e0.y - tv.y * e0.x};
						const float32 v = (rayD.x * qv.x + rayD.y * qv.y + rayD.z * qv.z) * inv;
						if (v < 0.f || u + v > 1.f)
							continue;
						const float32 t = (e1.x * qv.x + e1.y * qv.y + e1.z * qv.z) * inv;
						if (t > 1e-4f)
							ts[nT++] = t;
					}
					// Tri par insertion : 32 valeurs au plus, la simplicite gagne.
					for (int32 a = 1; a < nT; ++a) {
						const float32 v = ts[a];
						int32 b = a - 1;
						for (; b >= 0 && ts[b] > v; --b)
							ts[b + 1] = ts[b];
						ts[b + 1] = v;
					}
					if (nT >= 1) {
						const float32 tMid = (nT >= 2) ? (ts[0] + ts[1]) * 0.5f : ts[0];
						if (tMid < volBestT) {
							volBestT = tMid;
							volP = {rayO.x + rayD.x * tMid, rayO.y + rayD.y * tMid,
									rayO.z + rayD.z * tMid};
							volFound = 1;
						}
					}
					continue;
				}
				// Pied de la perpendiculaire abaissee depuis l'ORIGINE de chaque
				// source sur la droite (a,b), borne au segment : le trajet du
				// geste devient perpendiculaire a l'arete (cible 7).
				auto perpEdge = [&](const NkVec3f &a, const NkVec3f &b) {
					const NkVec3f ab{b.x - a.x, b.y - a.y, b.z - a.z};
					const float32 l2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
					if (l2 < 1e-12f)
						return;
					for (int32 s = 0; s < nSrc; ++s) {
						// L'ACCEPTATION mesure la distance A L'ARETE depuis la
						// position COURANTE -- le pied de perpendiculaire, lui,
						// est souvent loin LE LONG de l'arete, et le confronter
						// a la portee de 60 px rendait la cible muette une fois
						// sur deux (« ca marche mais pas toujours », Rihen).
						float32 tc = ((srcPts[s].x - a.x) * ab.x +
									  (srcPts[s].y - a.y) * ab.y +
									  (srcPts[s].z - a.z) * ab.z) /
									 l2;
						tc = tc < 0.f ? 0.f : (tc > 1.f ? 1.f : tc);
						const float32 cx = a.x + ab.x * tc - srcPts[s].x,
									  cy = a.y + ab.y * tc - srcPts[s].y,
									  cz = a.z + ab.z * tc - srcPts[s].z;
						const float32 dEdge2 = cx * cx + cy * cy + cz * cz;
						if (dEdge2 >= best)
							continue;
						// LE POINT D'ACCROCHE : le pied abaisse depuis
						// l'ORIGINE du geste -- le trajet devient
						// perpendiculaire a l'arete.
						const NkVec3f &sO = srcOrig[s];
						float32 t = ((sO.x - a.x) * ab.x + (sO.y - a.y) * ab.y +
									 (sO.z - a.z) * ab.z) /
									l2;
						t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
						const NkVec3f q{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
						best = dEdge2;
						bestP = {nearP.x + (q.x - srcPts[s].x),
								 nearP.y + (q.y - srcPts[s].y),
								 nearP.z + (q.z - srcPts[s].z)};
						found = 1;
					}
				};
				for (uint32 t3 = 0; t3 + 2 < ic; t3 += 3) {
					const NkVec3f A = W * vv[ii[t3]].pos;
					const NkVec3f B = W * vv[ii[t3 + 1]].pos;
					const NkVec3f C = W * vv[ii[t3 + 2]].pos;
					if (target == 7) {
						perpEdge(A, B);
						perpEdge(B, C);
						perpEdge(C, A);
					} else if (target == 3) {
						// ARETE : le point le plus proche sur chaque segment.
						segClosest(A, B);
						segClosest(B, C);
						segClosest(C, A);
					} else if (target == 6) {
						// CENTRE D'ARETE.
						consider({(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f, (A.z + B.z) * 0.5f});
						consider({(B.x + C.x) * 0.5f, (B.y + C.y) * 0.5f, (B.z + C.z) * 0.5f});
						consider({(C.x + A.x) * 0.5f, (C.y + A.y) * 0.5f, (C.z + A.z) * 0.5f});
					} else if (target == 8) {
						// CENTRE DE FACE -- et sa NORMALE, si ce candidat gagne :
						// poser une rangee d'objets alignes aux faces en depend.
						const float32 prevBest = best;
						consider({(A.x + B.x + C.x) / 3.f, (A.y + B.y + C.y) / 3.f,
								  (A.z + B.z + C.z) / 3.f});
						if (best < prevBest) {
							const NkVec3f e0{B.x - A.x, B.y - A.y, B.z - A.z};
							const NkVec3f e1{C.x - A.x, C.y - A.y, C.z - A.z};
							NkVec3f N{e0.y * e1.z - e0.z * e1.y, e0.z * e1.x - e0.x * e1.z,
									  e0.x * e1.y - e0.y * e1.x};
							// Orientee vers la SOURCE : l'objet se pose du cote
							// d'ou il vient, pas a l'interieur de la cible.
							const float32 sd = (srcPts[0].x - A.x) * N.x +
											   (srcPts[0].y - A.y) * N.y +
											   (srcPts[0].z - A.z) * N.z;
							if (sd < 0.f)
								N = {-N.x, -N.y, -N.z};
							bestN = N;
							haveN = true;
						}
					} else if (target == 4) {
						// FACE : pour CHAQUE point source, sa projection sur le
						// plan du triangle si elle y tombe (test barycentrique),
						// sinon les aretes font foi.
						const NkVec3f e0{B.x - A.x, B.y - A.y, B.z - A.z};
						const NkVec3f e1{C.x - A.x, C.y - A.y, C.z - A.z};
						const NkVec3f N{e0.y * e1.z - e0.z * e1.y,
										e0.z * e1.x - e0.x * e1.z,
										e0.x * e1.y - e0.y * e1.x};
						const float32 n2 = N.x * N.x + N.y * N.y + N.z * N.z;
						if (n2 < 1e-12f)
							continue;
						const float32 d00 = e0.x * e0.x + e0.y * e0.y + e0.z * e0.z;
						const float32 d01 = e0.x * e1.x + e0.y * e1.y + e0.z * e1.z;
						const float32 d11 = e1.x * e1.x + e1.y * e1.y + e1.z * e1.z;
						const float32 den = d00 * d11 - d01 * d01;
						if (den < 1e-12f)
							continue;
						bool anyInside = false;
						for (int32 s = 0; s < nSrc; ++s) {
							const NkVec3f &S0 = srcPts[s];
							const float32 dpl = ((S0.x - A.x) * N.x + (S0.y - A.y) * N.y +
												 (S0.z - A.z) * N.z) /
												n2;
							const NkVec3f P{S0.x - N.x * dpl, S0.y - N.y * dpl,
											S0.z - N.z * dpl};
							const NkVec3f v2{P.x - A.x, P.y - A.y, P.z - A.z};
							const float32 d20 = v2.x * e0.x + v2.y * e0.y + v2.z * e0.z;
							const float32 d21 = v2.x * e1.x + v2.y * e1.y + v2.z * e1.z;
							const float32 bv = (d11 * d20 - d01 * d21) / den;
							const float32 bw = (d00 * d21 - d01 * d20) / den;
							if (bv >= 0.f && bw >= 0.f && bv + bw <= 1.f) {
								anyInside = true;
								const float32 dx = P.x - S0.x, dy = P.y - S0.y,
											  dz = P.z - S0.z;
								const float32 d2 = dx * dx + dy * dy + dz * dz;
								if (d2 < best) {
									best = d2;
									bestP = {nearP.x + dx, nearP.y + dy, nearP.z + dz};
									found = 1;
									// La NORMALE du triangle gagnant, orientee du
									// cote de la source : l'alignement de rotation
									// posera +Z dessus.
									bestN = (dpl >= 0.f) ? N : NkVec3f{-N.x, -N.y, -N.z};
									haveN = true;
								}
							}
						}
						if (!anyInside) {
							// Le bord de la face fait foi : si l'un de ces trois
							// segments gagne, la normale reste celle de CE
							// triangle -- sans quoi bestN garderait celle d'une
							// face precedente, fausse pour l'alignement.
							const float32 prevBest = best;
							segClosest(A, B);
							segClosest(B, C);
							segClosest(C, A);
							if (best < prevBest) {
								const float32 sd = (srcPts[0].x - A.x) * N.x +
												   (srcPts[0].y - A.y) * N.y +
												   (srcPts[0].z - A.z) * N.z;
								bestN = (sd >= 0.f) ? N : NkVec3f{-N.x, -N.y, -N.z};
								haveN = true;
							}
						}
					}
				}
			}
			(void)cand; // compte des candidats : servait au diagnostic d'accroche
			// VOLUME : sa reponse est le point DANS la matiere visee, hors du
			// jeu portee/sources des autres cibles.
			if (target == 5) {
				if (volFound)
					out = volP;
				return volFound;
			}
			if (found) {
				out = bestP;
				if (outN && haveN)
					*outN = bestN;
			}
			return found;
		}
		static int32 Demo3D_GizmoRayTest(void *user, int32 idx, const NkMat4f &world, NkVec3f ro,
										 NkVec3f rd, float32 &tInOut) {
			const Demo3DRayCtx *cx = (const Demo3DRayCtx *)user;
			if (!cx || !cx->st || !cx->ms || idx < 0 || idx >= Demo3DState::kNumObj)
				return -1;
			// UN OBJET INVISIBLE NE SE CLIQUE PAS. Les cibles du gizmo couvrent
			// toute la demo meme quand elle est masquee (scene utilisateur) : le
			// sol 80x80 invisible a y=0 attrapait alors tout clic sous l'horizon
			// -- selection fantome, vecue comme « refuse de se deselectionner »
			// (constate par Rihen). Reponse 0 = teste et rate, qui fait autorite.
			if (HostHiddenEff(idx))
				return 0;
			Demo3DState *st = cx->st;
			NkMeshHandle mh = st->meshSphere; // 0..15 : grille PBR de spheres
			if (idx == Demo3DState::kIdxFloor)
				mh = st->meshPlane;
			else if (idx >= 16) // cube central, colonnes, grille instanciee, feuillage, mur GI
				mh = st->meshCube;
			if (st->objMesh[idx].IsValid())
				mh = st->objMesh[idx];
			if (!cx->ms->HasCPUData(mh))
				return -1;
			const auto *vv = (const renderer::NkVertex3D *)cx->ms->GetVertices(mh);
			const uint32 vc = cx->ms->GetVertexCount(mh);
			if (!vv || vc == 0)
				return -1;
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
			const NkVec3f c = (mn + mx) * 0.5f;
			const NkMat4f W = world * NkMat4f::Translate({-c.x, -c.y, -c.z});
			return Demo3D_RayMeshT(cx->ms, mh, W, ro, rd, tInOut) ? 1 : 0;
		}

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
			st->meshCylinder = meshSys->GetCylinder();
			st->meshCone = meshSys->GetCone();
			st->meshIco = meshSys->GetIcosphere();
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
				// ── PAVE 0 : VUE CAMERA facon Blender (hors mode test GI, qui le
				// reserve). Simple : bascule vue libre <-> camera ACTIVE, la pose
				// libre est memorisee/restituee cote hote. Ctrl+0 : la camera
				// SELECTIONNEE devient l'active, et on la regarde.
				else if (!st->giTest && k == NkKey::NK_NUMPAD_0) {
					// REGLES DE RIHEN :
					//   0 camera    -> pave 0 et Ctrl+0 ne font RIEN ;
					//   1 camera    -> pave 0 y va (elle est la principale par
					//                  defaut), Ctrl+0 sans effet ;
					//   plusieurs   -> Ctrl+0 CHANGE la principale (la camera
					//                  selectionnee si c'en est une, sinon la
					//                  suivante dans l'ordre de la scene), et
					//                  pave 0 regarde la principale.
					int32 cams0[64];
					const int32 nC0 = HostSceneCameras(cams0, 64);
					const bool alt0 =
						NkInput.IsKeyDown(NkKey::NK_LALT) || NkInput.IsKeyDown(NkKey::NK_RALT);
					if (ctrl && alt0) {
						// CTRL+ALT+0 : la camera ACTIVE saute sur la VUE ACTUELLE
						// (« Align Active Camera to View » de Blender, Rihen).
						if (nC0 == 0)
							logger.Info("[Demo3D] Ctrl+Alt+pave 0 : aucune camera en scene\n");
						else
							st->camAlignPending = true; // execute dans la frame, ou vit la camera
					} else if (ctrl) {
						if (nC0 <= 1) {
							logger.Info("[Demo3D] Ctrl+pave 0 : {0} camera en scene, rien a changer\n",
										nC0);
						} else {
							int32 next = -1;
							// L'index du gizmo des vides est DECALE de 6 par
							// rapport aux emplacements utilisateur (les 6
							// premiers vides appartiennent a la demo) : ea - 6,
							// comme partout ailleurs. Sans ce decalage, la
							// camera SELECTIONNEE n'etait jamais reconnue.
							const int32 es0 = st->emptyGizmo.ActiveIndex();
							const int32 us0 = es0 - (kNkvpFirstUser - kNkvpFirstEmpty);
							if (us0 >= 0 && us0 < kNkvpMaxUser && nkvpUserKind[us0] == 4 &&
								nkvpUserSub[us0] == 10)
								next = kNkvpFirstUser + us0;
							if (next < 0) {
								int32 cur = -1;
								for (int32 i0 = 0; i0 < nC0; ++i0)
									if (cams0[i0] == nkvpActiveCamNode)
										cur = i0;
								next = cams0[(cur + 1) % nC0];
							}
							nkvpActiveCamNode = next;
							if (nkvpCamViewNode >= 0)
								Demo3DHostViewCamera(next); // deja en vue camera : bascule directe
							logger.Info("[Demo3D] Camera principale -> noeud {0} (Ctrl+pave 0)\n",
										next);
						}
					} else {
						if (nC0 == 0) {
							logger.Info("[Demo3D] Pave 0 : aucune camera en scene\n");
						} else {
							const bool onCv = Demo3DHostToggleCameraView();
							logger.Info("[Demo3D] Vue camera : {0} (pave 0)\n", onCv ? "ON" : "OFF");
						}
					}
				}
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
				// !ctrl : Ctrl+Maj+S est ENREGISTRER TOUT. La touche arrive par DEUX
				// voies -- le shell (main.cpp) et ce rappel -- et LES DEUX doivent
				// ceder. C'est le meme piege que Ctrl+S qui armait l'echelle, et que
				// Ctrl+V qui collait deux fois : chercher la seconde voie.
				const bool ctrlG =
					NkInput.IsKeyDown(NkKey::NK_LCTRL) || NkInput.IsKeyDown(NkKey::NK_RCTRL);
				if (shiftG && !alt && !ctrlG && (k == NkKey::NK_S || k == NkKey::NK_F)) {
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
				if (k == NkKey::NK_S && !NkInput.IsKeyDown(NkKey::NK_LCTRL) &&
					!NkInput.IsKeyDown(NkKey::NK_RCTRL)) { // Ctrl+S = ENREGISTRER, pas le gizmo
					// (meme garde que Ctrl+C juste dessous -- la touche arrive par
					// DEUX voies, le shell et ce rappel : les deux doivent ceder)
					if (alt)
						G.ClearSelectedScale();
					else
						G.SetMode(GZ::MODE_SCALE);
				}
				if (k == NkKey::NK_C && !NkInput.IsKeyDown(NkKey::NK_LCTRL) &&
					!NkInput.IsKeyDown(NkKey::NK_RCTRL)) // Ctrl+C = COPIER, pas le gizmo
					G.SetMode(GZ::MODE_COMBINE);
				if (k == NkKey::NK_A) {
					if (alt) {
						G.ClearSelection();
					} else {
						// TOUT = tout ce qui se VOIT. Ce gestionnaire herite de la
						// demo selectionnait ses ~90 objets meme CACHES dans une
						// scene utilisateur — Maj+A les faisait tous reapparaitre
						// en fond (constate par Rihen, 10 aout). Si toute la demo
						// est masquee, il n'a rien a selectionner.
						bool anyVisible = false;
						for (int32 o = 0; o < Demo3DState::kNumObj && !anyVisible; ++o)
							anyVisible = !HostHiddenEff(o);
						if (anyVisible)
							G.SelectAll();
					}
				}
				if (k == NkKey::NK_COMMA) {
					G.CycleOrientation();
					const char *o[3] = {"GLOBAL", "LOCAL", "NORMAL"};
					logger.Info("[Demo3D] Orientation = {0}\n", o[G.Orientation()]);
				}
				if (k == NkKey::NK_G || k == NkKey::NK_R ||
					((k == NkKey::NK_S || k == NkKey::NK_C) &&
					 !NkInput.IsKeyDown(NkKey::NK_LCTRL) &&
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
				// LE SOL EST A ZERO, EXACTEMENT. Il valait 0.01 pour ne pas se
				// battre en profondeur avec un sol solide separe -- or ce sol a ete
				// RETIRE depuis, et le decalage lui a survecu. Consequences qu'il
				// causait : un objet pose a y=0 s'enfoncait d'un centimetre sous la
				// grille, et une ombre calculee au sol tombait sous la surface censee
				// la recevoir. Le zero de la scene (curseur 3D, aimantation, axes 3D)
				// vaut 0 : le sol doit valoir 0 aussi, sans quoi rien n'est coherent.
				// Aucun z-fight a craindre : le plan et ses lignes sont LE MEME quad.
				g.planeY = 0.0f;
				g.lineColor = {0.42f, 0.45f, 0.52f,
							   1.0f}; // gris moyen : bien visible sur fond sombre MAIS sous le seuil du bloom
				g.cellColor = {0.09f, 0.10f, 0.12f, 0.18f}; // intérieur = PLAN INFINI (.w=opacité ; 0=transparent)
				g.axisXColor = {1.0f, 0.0f, 0.0f, 1.0f};	// X rouge PLEIN
				g.axisZColor = {0.0f, 0.0f, 1.0f, 1.0f};	// Z bleu PLEIN
				// Axes du SHADER grille DESACTIVES : les trois axes sont dessines en
				// lignes 3D reelles (DrawDebugLine, cf. Frame), a y = 0 exactement.
				//
				// Deux raisons de ne PAS les confier au shader :
				//   1. l'axe Y, calcule en projection ecran dans le fragment,
				//      quittait l'origine et cessait d'etre parallele aux verticales
				//      en perspective ;
				//   2. surtout, ils seraient alors DEPENDANTS de la grille --
				//      decocher « Grille » effacerait les axes, alors que « Axes du
				//      plan » est un reglage distinct qui doit rester independant.
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
			// L'OCCLUSION AMBIANTE EST UNE SEULE NOTION POUR L'UTILISATEUR
			// (Rihen, 10 aout : « tache noire au sol autour d'un objet alors
			// que l'occlusion ambiante n'est pas activee ») : la grille voxel
			// assombrissait l'ambiance autour des objets injectes MEME panneau
			// eteint — un reglage affiche eteint qui agit quand meme, c'est le
			// defaut de conception deja paye. Panneau eteint => grille VIDE :
			// voxAO vaut 1 partout, et le rebond indirect s'eteint avec (il
			// exige les memes occludeurs).
			{
				bool aoOn = false;
				float32 aoR = 0.5f, aoI = 1.f;
				Demo3DHostSSAO(&aoOn, &aoR, &aoI);
				if (!aoOn) {
					vao->Build(); // grille vide televersee : plus aucune tache
					st->giBuildMs = vao->GetLastBuildMs();
					st->giInjectMs = 0.f;
					return;
				}
			}
			// LES OCCLUDEURS DE LA DEMO NE VALENT QUE SI LEURS OBJETS SE VOIENT.
			// Cette boite 16x16 est le sol de la DEMO, et son plafond depasse a
			// y=0.05 : enregistree dans une scene UTILISATEUR, elle plongeait
			// tout le sol dans un grand carre sombre en mode rendu (constate par
			// Rihen des la premiere reconstruction de la grille -- avant, la
			// grille ne se construisait jamais, l'erreur dormait). Meme test de
			// visibilite que le rendu : une seule verite.
			if (!HostHiddenEff(Demo3DState::kIdxFloor)) {
				NkVoxelOccluder floorOcc;
				floorOcc.minWorld = {-8.f, -0.6f, -8.f};
				floorOcc.maxWorld = {8.f, 0.05f, 8.f};
				floorOcc.opacity = 1.f;
				floorOcc.albedo = {0.5f, 0.5f, 0.5f};
				vao->RegisterOccluder(floorOcc);
			}
			// ── LES OBJETS UTILISATEUR OCCLUDENT AUSSI ──────────────────────
			// Seuls le sol de la demo et le mur GI etaient voxelises : partout
			// ailleurs voxAO valait 1, et l'ambiant entrait a pleine puissance --
			// d'ou l'INTERIEUR d'un cube ferme reste gris clair au lieu de
			// sombre (constate par Rihen, capture du 7 aout ; la shadow map,
			// elle, etait correcte : elle n'eteint que la lumiere DIRECTE).
			// AABB monde du cube unite transforme : pour l'ambiance, l'enveloppe
			// suffit -- l'erreur d'une sphere approchee par sa boite est
			// invisible dans une occlusion en cones.
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				if (!nkvpUserMesh[u].IsValid())
					continue;
				const int32 un = kNkvpFirstUser + u;
				if (nkvpDeleted[un] || HostHiddenEff(un))
					continue;
				NkVoxelOccluder oc;
				Demo3D_XformAABB(Demo3D_UserWireXform(st, u), oc.minWorld, oc.maxWorld);
				oc.opacity = 1.f;
				oc.albedo = {0.5f, 0.5f, 0.5f};
				vao->RegisterOccluder(oc);
			}
			// L'occluder épouse la transform EFFECTIVE du mur (clavier + gizmo) : la
			// lumière rebondit donc toujours exactement sur le mur qu'on voit bouger.
			// Meme regle que le sol demo : seulement si le mur se VOIT.
			if (!HostHiddenEff(Demo3DState::kIdxGIWall)) {
				NkVoxelOccluder wall;
				Demo3D_XformAABB(st->giWallXform, wall.minWorld, wall.maxWorld);
				wall.opacity = 1.f;
				wall.albedo = {0.9f, 0.05f, 0.05f};
				vao->RegisterOccluder(wall);
			}
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
			// pour NOTRE renderer : reset du pool d'UBO objets, upload des
			// materiaux, ET la reconstruction du graphe en attente — sans elle,
			// activer l'occlusion ambiante depuis le panneau armait un drapeau
			// que personne ne consommait jamais : le bouton semblait mort
			// (constate par Rihen, 9 aout). (Meme contrainte et meme modele que
			// NkViewport3D.cpp.)
			auto *r3d = ctx.renderer->GetRender3D();
			if (!r3d)
				return;
			ctx.renderer->FlushGraphRebuilds();
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
					if (HostNavKey(NkKey::NK_W) || HostNavKey(NkKey::NK_UP))
						fwd += spd;
					if (HostNavKey(NkKey::NK_S) || HostNavKey(NkKey::NK_DOWN))
						fwd -= spd;
					if (HostNavKey(NkKey::NK_D) || HostNavKey(NkKey::NK_RIGHT))
						rgt += spd;
					if (HostNavKey(NkKey::NK_A) || HostNavKey(NkKey::NK_LEFT))
						rgt -= spd;
					if (HostNavKey(NkKey::NK_E))
						up += spd;
					if (HostNavKey(NkKey::NK_Q))
						up -= spd;
					st->simCam.Move(fwd, rgt, up);
					if (wheel != 0.f)
						st->simCam.Move(wheel * 0.6f, 0.f, 0.f); // molette = avancer
					st->simCam.Apply(cam);
				} else {
					// STRUCTURE : pilotage camera OU navigation libre en tete,
					// puis les blocs COMMUNS (application de la vue camera,
					// projection ortho + grille) -- ils doivent servir les DEUX
					// sous-modes, les scinder a deja gele la vue camera une fois.
					if (nkvpCamViewNode >= kNkvpFirstUser &&
						nkvpCamViewNode - kNkvpFirstUser < kNkvpMaxUser) {
					// ── EN VUE CAMERA, la navigation PILOTE LA CAMERA (le « lock
					// camera to view » de Blender, demande par Rihen) : clic
					// milieu = tourner la camera, Shift+milieu = travelling
					// lateral/vertical, molette = avancer/reculer. Le NOEUD bouge
					// reellement -- panneau, gizmo et capture voient la meme
					// camera.
					const int32 eC = nkvpCamViewNode - 90;
					// Pose MONDE AVANT tout geste : l'ancrage de l'orbite du
					// cadenas (centre et distance se mesurent sur elle).
					float32 cwp0[3], cwsc0[3];
					NkMat4f cwr0;
					HostNodeWorldById(nkvpCamViewNode, cwp0, cwr0, cwsc0);
					const NkVec3f fwd0 = (cwr0 * NkVec3f{0.f, 0.f, -1.f}).Normalized();
					if (NkInput.IsMouseDown(NkMouseButton::NK_MB_MIDDLE) && !shift &&
						(mdx != 0.f || mdy != 0.f)) {
						// PILOTAGE DE LA CAMERA : lacet et tangage se pensent en
						// ANGLES (le tangage se borne a +-89 pour ne pas basculer),
						// puis on repose le quaternion -- lui reste la verite.
						float32 camDeg[3];
						HostNodeEuler(eC, camDeg);
						camDeg[1] -= mdx * 0.25f; // lacet
						camDeg[0] -= mdy * 0.25f; // tangage, borne
						if (camDeg[0] > 89.f)
							camDeg[0] = 89.f;
						if (camDeg[0] < -89.f)
							camDeg[0] = -89.f;
						HostSetNodeEuler(eC, camDeg);
						// ── CADENAS ACTIF : la rotation ORBITE la camera autour
						// d'un CENTRE (Rihen, comme Blender) au lieu de tourner
						// sur place. Le centre : la selection si elle existe,
						// sinon le point vise 6 m devant l'objectif. La camera
						// garde sa distance et se replace pour regarder le
						// centre -- l'orientation vient des eulers deja mis a
						// jour ci-dessus, la position suit.
						if (nkvpCamOrbitLock) {
							// LE CENTRE EST DEVANT L'OBJECTIF : le point vise a
							// distance fixe le long du REGARD COURANT, jamais un
							// pivot exterieur -- le pivot de selection faisait
							// SAUTER la rotation vers le centre du monde des
							// qu'un objet y etait selectionne (constate par
							// Rihen). La camera orbite donc toujours autour de
							// ce qu'elle regarde.
							const float32 dFoc = 6.f;
							const NkVec3f C{cwp0[0] + fwd0.x * dFoc,
											cwp0[1] + fwd0.y * dFoc,
											cwp0[2] + fwd0.z * dFoc};
							const NkVec3f offP{cwp0[0] - C.x, cwp0[1] - C.y,
											   cwp0[2] - C.z};
							const float32 distC = math::NkSqrt(
								offP.x * offP.x + offP.y * offP.y + offP.z * offP.z);
							float32 cwp1[3], cwsc1[3];
							NkMat4f cwr1;
							HostNodeWorldById(nkvpCamViewNode, cwp1, cwr1, cwsc1);
							const NkVec3f fwd1 =
								(cwr1 * NkVec3f{0.f, 0.f, -1.f}).Normalized();
							const float32 newP[3] = {C.x - fwd1.x * distC,
													 C.y - fwd1.y * distC,
													 C.z - fwd1.z * distC};
							for (int32 a = 0; a < 3; ++a)
								nkvpEmptyPos[eC][a] += newP[a] - cwp0[a];
						}
					}
					float32 cwpN[3], cwscN[3];
					NkMat4f cwrN;
					HostNodeWorldById(nkvpCamViewNode, cwpN, cwrN, cwscN);
					// Axes obtenus en TRANSFORMANT les vecteurs unitaires --
					// independant de la convention de stockage de NkMat4f (meme
					// lecon que le pick OBB et l'override de vue ci-dessous).
					const NkVec3f rgtN = (cwrN * NkVec3f{1.f, 0.f, 0.f}).Normalized();
					const NkVec3f upN = (cwrN * NkVec3f{0.f, 1.f, 0.f}).Normalized();
					const NkVec3f fwdN = (cwrN * NkVec3f{0.f, 0.f, -1.f}).Normalized();
					// ── CADENAS FERME : LA ROTATION SEULE (regle de Rihen). Ni
					// travelling, ni molette, ni fleches, ni focale depuis la
					// vue -- la camera ne fait qu'orbiter autour de son centre.
					if (!nkvpCamOrbitLock) {
						if (NkInput.IsMouseDown(NkMouseButton::NK_MB_MIDDLE) && shift) {
							// « grab » : on tire la scene, la camera part a l'oppose.
							const float32 psc = 0.012f;
							nkvpEmptyPos[eC][0] += (-rgtN.x * mdx + upN.x * mdy) * psc;
							nkvpEmptyPos[eC][1] += (-rgtN.y * mdx + upN.y * mdy) * psc;
							nkvpEmptyPos[eC][2] += (-rgtN.z * mdx + upN.z * mdy) * psc;
						}
						if (wheel != 0.f) {
							const float32 zsc = 0.6f;
							nkvpEmptyPos[eC][0] += fwdN.x * wheel * zsc;
							nkvpEmptyPos[eC][1] += fwdN.y * wheel * zsc;
							nkvpEmptyPos[eC][2] += fwdN.z * wheel * zsc;
						}
						// TOUCHES DIRECTIONNELLES (Rihen) : haut/bas = avancer/
						// reculer le long du regard, gauche/droite = pas lateral.
						const float32 kspd = (shift ? 9.f : 3.f) * dt;
						float32 mvF = 0.f, mvR = 0.f;
						if (HostNavKey(NkKey::NK_UP))
							mvF += kspd;
						if (HostNavKey(NkKey::NK_DOWN))
							mvF -= kspd;
						if (HostNavKey(NkKey::NK_RIGHT))
							mvR += kspd;
						if (HostNavKey(NkKey::NK_LEFT))
							mvR -= kspd;
						if (mvF != 0.f || mvR != 0.f) {
							nkvpEmptyPos[eC][0] += fwdN.x * mvF + rgtN.x * mvR;
							nkvpEmptyPos[eC][1] += fwdN.y * mvF + rgtN.y * mvR;
							nkvpEmptyPos[eC][2] += fwdN.z * mvF + rgtN.z * mvR;
						}
					}
					st->camAlignPending = false; // deja en vue camera : rien a aligner
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
					// TOUCHES DIRECTIONNELLES en vue libre aussi (Rihen) :
					// haut/bas = avancer/reculer, gauche/droite = pas lateral.
					// (Les FLECHES, pas WASD : ces lettres restent au gizmo.)
					// TRANSLATION, jamais du zoom : le zoom BUTAIT sur sa
					// distance minimale (« ca bloque a un niveau », Rihen). On
					// deplace la CIBLE de l'orbite a plat sur le plan -- la
					// position suit, la distance et l'angle ne changent pas.
					{
						const float32 kspd = (shift ? 3.f : 1.f) * dt * 60.f;
						float32 mvF2 = 0.f, mvR2 = 0.f;
						if (HostNavKey(NkKey::NK_UP))
							mvF2 += 1.f;
						if (HostNavKey(NkKey::NK_DOWN))
							mvF2 -= 1.f;
						if (HostNavKey(NkKey::NK_RIGHT))
							mvR2 += 1.f;
						if (HostNavKey(NkKey::NK_LEFT))
							mvR2 -= 1.f;
						if (mvF2 != 0.f || mvR2 != 0.f) {
							// L'AVANT vient du LACET + TANGAGE de la camera
							// d'orbite -- pas de `cam`, qui a ce stade porte
							// encore ses valeurs par defaut de debut de frame.
							// VOL COMPLET (Rihen) : regarder vers le bas et
							// avancer PLONGE -- l'avant suit le regard en 3D,
							// comme en vue camera. Le pas lateral, lui, reste
							// horizontal (il ne depend que du lacet).
							// Convention : yaw=pi/2 = vue FRONT (regard -Z),
							// offset camera = (cos y cos p, sin p, sin y cos p).
							const float32 yawV = st->editorCam.GetYaw();
							const float32 pitV = st->editorCam.GetPitch();
							const float32 cpV = math::NkCos(pitV);
							const NkVec3f fwdV{-math::NkCos(yawV) * cpV, -math::NkSin(pitV),
											   -math::NkSin(yawV) * cpV};
							const NkVec3f rgtV{math::NkSin(yawV), 0.f, -math::NkCos(yawV)};
							const float32 stp = 0.09f * kspd;
							NkVec3f tC = st->editorCam.GetTarget();
							tC.x += (fwdV.x * mvF2 + rgtV.x * mvR2) * stp;
							tC.y += fwdV.y * mvF2 * stp;
							tC.z += (fwdV.z * mvF2 + rgtV.z * mvR2) * stp;
							st->editorCam.SetCenter(tC, st->editorCam.GetDistance(), yawV,
													pitV);
						}
					}
					st->editorCam.Apply(cam);
					// ── CTRL+ALT+0 : la camera ACTIVE saute SUR LA VUE ACTUELLE
					// (« Align Active Camera to View » de Blender, Rihen), puis
					// on entre dans sa vue. Traite ICI : c'est la que `cam`
					// porte la pose reellement affichee.
					if (st->camAlignPending) {
						st->camAlignPending = false;
						int32 camsA[64];
						const int32 nA = HostSceneCameras(camsA, 64);
						if (nA > 0) {
							bool okA = false;
							for (int32 iA = 0; iA < nA; ++iA)
								if (camsA[iA] == nkvpActiveCamNode)
									okA = true;
							const int32 nodeA = okA ? nkvpActiveCamNode : camsA[0];
							const int32 eA = nodeA - kNkvpFirstEmpty;
							const NkVec3f eyeV = cam.GetPosition();
							const NkVec3f fV = (cam.GetTarget() - eyeV).Normalized();
							nkvpEmptyPos[eA][0] = eyeV.x;
							nkvpEmptyPos[eA][1] = eyeV.y;
							nkvpEmptyPos[eA][2] = eyeV.z;
							// Regard -Z local, R = Rz*Ry*Rx :
							// f = (-cos(x)sin(y), sin(x), -cos(x)cos(y))
							// -> tangage = asin(f.y), lacet = atan2(-f.x, -f.z).
							const float32 kR2D = 57.29578f;
							const float32 fy = fV.y < -1.f ? -1.f : (fV.y > 1.f ? 1.f : fV.y);
							// ALIGNER SUR LA VUE : angles deduits du regard, puis
							// reposes comme quaternion.
							const float32 algDeg[3] = {math::NkAsin(fy) * kR2D,
													   math::NkAtan2(-fV.x, -fV.z) * kR2D,
													   0.f};
							HostSetNodeEuler(eA, algDeg);
							Demo3DHostViewCamera(nodeA);
							logger.Info(
								"[Demo3D] Camera alignee sur la vue (Ctrl+Alt+pave 0) : noeud {0}\n",
								nodeA);
						}
					}
					} // fin pilotage / navigation libre -- la suite sert les DEUX
					// ── VUE CAMERA (Rihen) ─────────────────────────────────────
					// La vue montre EXACTEMENT ce que voit la camera choisie :
					// sa position monde, son regard (-Z local, comme le filaire)
					// et sa focale. Toujours en perspective -- une camera reelle
					// n'est pas orthographique.
					if (nkvpCamViewNode >= 0 && nkvpCamViewNode < kNkvpMaxNodes &&
						!nkvpDeleted[nkvpCamViewNode]) {
						float32 cwp[3], cwsc[3];
						NkMat4f cwr;
						HostNodeWorldById(nkvpCamViewNode, cwp, cwr, cwsc);
						const NkVec3f eyeC{cwp[0], cwp[1], cwp[2]};
						// Le REGARD (-Z local) s'obtient en TRANSFORMANT l'axe,
						// pas en lisant une ligne de la matrice : la lecture
						// directe depend de la convention de stockage (meme
						// lecon que le pick OBB) -- transposee, la vue regardait
						// AILLEURS que la camera (Rihen : « ca montre autre
						// chose »).
						const NkVec3f fwdC = (cwr * NkVec3f{0.f, 0.f, -1.f}).Normalized();
						cam.SetPosition(eyeC);
						cam.SetTarget({eyeC.x + fwdC.x, eyeC.y + fwdC.y,
									   eyeC.z + fwdC.z});
						const int32 cuV = nkvpCamViewNode - kNkvpFirstUser;
						float32 frV[4];
						Demo3DHostCameraFrame(frV);
						// PENDANT UN RENDU DE SORTIE, l'image de la camera
						// occupe TOUTE la cible : pas de cadre en retrait, donc
						// pas d'elargissement du champ. Le reglage est ici, et
						// non dans le cadre lui-meme, qui sert aussi a peindre
						// le passe-partout de la vue.
						const float32 fhnV =
							(nkvpOutPhase != 0) ? 1.f : (frV[3] > 0.05f ? frV[3] : 1.f);
						// LES CLIPS de la camera s'appliquent AUSSI, en direct :
						// sans cette ligne, Clip debut/fin du panneau restaient
						// purement declaratifs (constate par Rihen -- aucun effet
						// visible en les modifiant).
						if (cuV >= 0 && cuV < kNkvpMaxUser) {
							float32 cnV = nkvpUserCam[cuV][1];
							float32 cfV = nkvpUserCam[cuV][2];
							if (cnV < 0.001f)
								cnV = 0.001f;
							if (cfV <= cnV + 0.01f)
								cfV = cnV + 0.01f;
							cam.SetNearFar(cnV, cfV);
						}
						if (cuV >= 0 && cuV < kNkvpMaxUser && nkvpUserCamOrtho[cuV]) {
							// CAMERA ORTHOGRAPHIQUE : demi-hauteur du cadre =
							// echelle Y du noeud, elargie du rapport du cadre en
							// retrait. Le bloc ortho + grille de la vue fait le
							// reste : on regle la DISTANCE de la cible pour que
							// « dist x nkvpOrthoScale » donne pile cette
							// demi-hauteur.
							const float32 orthoH =
								(cwsc[1] > 0.01f ? cwsc[1] : 1.f) / fhnV;
							const float32 dT =
								orthoH / (nkvpOrthoScale > 0.01f ? nkvpOrthoScale : 0.55f);
							cam.SetTarget({eyeC.x + fwdC.x * dT, eyeC.y + fwdC.y * dT,
										   eyeC.z + fwdC.z * dT});
							st->orthoView = true;
						} else {
							// PERSPECTIVE : zoom arriere pour que l'image exacte
							// de la camera occupe le cadre en retrait.
							st->orthoView = false;
							float32 fovV = 60.f;
							if (cuV >= 0 && cuV < kNkvpMaxUser && nkvpUserCam[cuV][0] > 1.f)
								fovV = nkvpUserCam[cuV][0];
							const float32 tV = math::NkTan(fovV * 0.5f * 0.017453292f) / fhnV;
							cam.SetFOV(2.f * math::NkAtan2(tV, 1.f) * 57.29578f);
						}
					}
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
			// Le soleil du CIEL peut lui aussi eclairer la scene (mode Manuel) :
			// sa fabrication vit avec les autres reglages du ciel, plus bas dans
			// ce fichier. Declaree ici, appelee juste apres les lumieres reelles.
			bool HostSkySunAsLight(renderer::NkLightDesc &out);
			// Le ciel VISIBLE est evalue dans le shader : ses parametres partent a
			// chaque image, ce qui rend tout reglage immediat -- et animable.
			void HostPushSkyToRenderer();
			HostPushSkyToRenderer();
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
				// LES QUATRE LUMIERES DE LA DEMO restent en loi HERITEE : leurs
				// intensites (3/12/2.5/8) ont ete reglees a l'oeil pour elle —
				// en loi physique (le defaut depuis le 10 aout) ce serait la
				// quasi-obscurite. Les lumieres UTILISATEUR, elles, naissent en
				// physique a leur puissance de reference.
				NkLightDesc &sun = st->lights[0];
				sun = NkLightDesc{};
				sun.attenuationMode = 0;
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
				redLight.attenuationMode = 0; // demo : reglee a l'oeil en loi heritee
				redLight.type = NkLightType::NK_POINT;
				redLight.position = {3.f, 2.5f, 0.f};
				redLight.color = {1.f, 0.2f, 0.1f};
				redLight.intensity = 12.f;
				redLight.range = 10.f;
				// TOUTES LES LUMIERES NAISSENT NORMALES (Rihen) : pas de texture de
				// faisceau heritee de la demo. Les cookies reviendront quand on
				// pourra charger une image et melanger -- pour toutes les lumieres,
				// pas seulement le spot.
				redLight.cookieIdx = -1;
				redLight.castShadow = true;
				redLight.shadowStatic = true;

				// [2] Fill bleue
				NkLightDesc &blue = st->lights[2];
				blue = NkLightDesc{};
				blue.attenuationMode = 0; // demo : reglee a l'oeil en loi heritee
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
				spot.attenuationMode = 0; // demo : reglee a l'oeil en loi heritee
				spot.type = NkLightType::NK_SPOT;
				spot.position = {3.f, 4.f, 0.f};
				spot.direction = (NkVec3f{0.f, 0.f, 0.f} - spot.position).Normalized();
				spot.color = {1.f, 0.95f, 0.85f};
				spot.intensity = 8.f;
				spot.range = 10.f;
				spot.innerAngle = 18.f;
				spot.outerAngle = 28.f;
				spot.cookieIdx = -1; // normale elle aussi (voir plus haut)
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
				if (!HostHiddenEff(86 + li)) { // oeil ferme (le sien ou celui d'un ancetre)
					renderer::NkLightDesc LD = Demo3D_LightEffective(st, li);
					LD.shadowStatic = !nkvpShadowDynamic; // cf. panneau Rendu
					sctx.lights.PushBack(LD);
				}
			// LUMIERES UTILISATEUR (dupliquees/collees) : descripteur NATIF
			// conserve, position pilotee par les tableaux + le gizmo.
			for (int32 u = 0; u < kNkvpMaxUser; ++u) {
				if (nkvpUserKind[u] != 5)
					continue;
				const int32 un = kNkvpFirstUser + u;
				if (HostHiddenEff(un))
					continue;
				renderer::NkLightDesc L2 = nkvpUserLight[u];
				// STATIQUE OU DYNAMIQUE : c'est le reglage du panneau Rendu qui
				// tranche, plus l'heritage. shadowStatic autorise NkVSM a garder
				// ses depth maps telles quelles ; les lumieres creees ici heritent
				// du descripteur d'une lumiere de demo, qui le portait -- leurs
				// ombres etaient donc calculees a la premiere image puis JAMAIS
				// refaites (HUD : "rend 0 | cache 6"), figees et sans rapport avec
				// la scene. En dynamique elles suivent ; en statique, c'est un choix
				// explicite de l'utilisateur, pas un accident.
				L2.shadowStatic = !nkvpShadowDynamic;
				const int32 e = un - 90;
				const NkVec3f tr = st->emptyGizmo.TranslateOf(e);
				L2.position = {nkvpEmptyPos[e][0] + tr.x, nkvpEmptyPos[e][1] + tr.y,
							   nkvpEmptyPos[e][2] + tr.z};
				// ROTATION du noeud -> direction du faisceau (-Y local) ;
				// l'ECHELLE d'une SURFACIQUE regle ses dimensions (Rihen).
				// Rotation depuis le QUATERNION (les angles ne sont qu'un affichage).
				const NkMat4f lRm = st->emptyGizmo.RotationOf(e) * HostNodeQuat(e).ToMat4();
				if (((int32)L2.type & 3) != 1)
					L2.direction = {-lRm.mat[1][0], -lRm.mat[1][1], -lRm.mat[1][2]};
				if (((int32)L2.type & 3) == 3) {
					const NkVec3f osl = st->emptyGizmo.ScaleOf(e);
					// l'echelle MULTIPLIE les dimensions du panneau (les deux agissent)
					L2.areaWidth *= fabsf(nkvpEmptyScl[e][0]) * (1.f + osl.x);
					L2.areaHeight *= fabsf(nkvpEmptyScl[e][1]) * (1.f + osl.y);
				}
				// TEMPERATURE ET EXPOSITION : appliquees a la couleur au moment de
				// la soumission. Le moteur porte les deux reglages ; les convertir
				// ici laisse les shaders et les structures GPU inchanges -- rien
				// de ce qui existait ne bouge.
				L2.color = renderer::NkLightEffectiveColor(L2);
				L2.temperatureK = 0.f; // deja appliquees : ne pas compter deux fois
				L2.exposure = 0.f;
				sctx.lights.PushBack(L2);
			}




			// LE SOLEIL DU CIEL, s'il a ete charge d'eclairer la scene. Ajoute
			// APRES les lumieres reelles : c'est un complement, pas un
			// remplacement, et il disparait des que le ciel suit une vraie
			// lumiere (sinon on eclairerait deux fois).
			{
				renderer::NkLightDesc skySun;
				if (HostSkySunAsLight(skySun)) {
					skySun.shadowStatic = !nkvpShadowDynamic;
					sctx.lights.PushBack(skySun);
				}
			}

			sctx.ambientIntensity = 0.15f;
			// BROUILLARD : le contexte le portait deja, plus personne ne le
			// lisait. Il est desormais rempli depuis le panneau Rendu et
			// consomme par le shader PBR.
			sctx.fogEnabled = nkvpFogOn;
			sctx.fogColor = {nkvpFogColor[0], nkvpFogColor[1], nkvpFogColor[2]};
			// La densite ne sert qu'a la loi exponentielle ; a zero, le shader
			// retombe sur la loi lineaire debut/fin.
			sctx.fogDensity = nkvpFogMode == 1 ? nkvpFogDensity : 0.f;
			sctx.fogStart = nkvpFogStart;
			sctx.fogEnd = nkvpFogEnd;
			// NAPPE AU SOL : hauteur, epaisseur, souffle. La VITESSE de derive
			// suit celle des nuages quand on l'a demande -- c'est ce qui fait
			// que la fumee au sol et le ciel avancent du meme pas.
			sctx.fogHeightBase = nkvpFogHeightBase;
			sctx.fogThickness = nkvpFogThickness;
			sctx.fogWind = nkvpFogWind;
			// (La vitesse des nuages est declaree plus bas dans ce fichier :
			// on passe par son accesseur, qui existe deja.)
			sctx.fogWindSpeed = nkvpFogWindFromClouds
									? (Demo3DHostSkyCloudSpeed() * 2.f + 0.02f)
									: 0.15f;

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
				if (st->giDirty)
					st->giWallXform = wallXform;
			}

			// ── LA GRILLE D'OCCLUSION SE RECONSTRUIT HORS DU MODE TEST ──────────
			// La consommation de giDirty vivait SOUS st->giTest : en scene
			// utilisateur normale, la grille voxel n'etait JAMAIS construite --
			// voxAO valait 1 partout, l'ambiant entrait a pleine puissance, et
			// l'interieur d'un cube ferme restait gris clair (constate par Rihen ;
			// c'est ce qui faisait croire a un bug de shadow map). Desormais toute
			// modification de la scene -- pastille, chargement, geste -- pose
			// giDirty (cf. Demo3DHostGIMarkDirty) et la reconstruction se fait
			// ici, mode test ou pas.
			if (st->giDirty) {
				Demo3D_RebuildGI(st, ctx.renderer, sctx.lights);
				st->giDirty = false;
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

			// ── SOL INFINI (option, Rihen) ───────────────────────────────────
			// Le plan SUIT la camera, arrondi au metre : infini en pratique, et
			// le snap garde les ombres stables. RECEPTEUR seulement
			// (castShadow=false) : il ne gonfle pas la cascade d'ombres --
			// exactement le piege repare au 4.7.
			if (nkvpFloorOn) {
				NkDrawCall3D dcF;
				dcF.mesh = st->meshPlane;
				const float32 kExtF = 1500.f;
				// ── CARRELAGE (option, references Unreal de Rihen) : DAMIER
				// (cases alternees) ou CARREAUX A JOINTS (dalles claires,
				// joints sombres + sous-lignes fines). Textures et materiaux
				// generes UNE fois ; la bibliotheque de materiaux n'ayant pas
				// de tiling UV, le motif vit dans les UV REPETES d'un mesh
				// dedie, reconstruit quand carreau ou motif change.
				static NkTexHandle sFloorTex[2] = {};
				static NkMaterial *sFloorMat[2] = {nullptr, nullptr};
				static NkMeshHandle sFloorMesh{};
				static float32 sFloorMeshTile = -1.f;
				static int32 sFloorMeshPat = -1;
				const int32 patF =
					nkvpFloorPattern < 0 ? 0 : (nkvpFloorPattern > 2 ? 2 : nkvpFloorPattern);
				const float32 tileF =
					nkvpFloorTile < 0.1f ? 0.1f : (nkvpFloorTile > 50.f ? 50.f : nkvpFloorTile);
				// PERIODE monde du motif = pas de SNAP du plan : le sol suit la
				// camera par bonds d'une periode, le motif ne rampe donc jamais.
				const float32 perF = patF == 1 ? tileF * 2.f : (patF == 2 ? tileF : 1.f);
				if (patF > 0 && ctx.renderer) {
					auto *texL = ctx.renderer->GetTextures();
					auto *matS = ctx.renderer->GetMaterials();
					auto *msF = ctx.renderer->GetMeshSystem();
					const int32 pi = patF - 1;
					if (!sFloorTex[pi].IsValid() && texL) {
						const uint32 TS = 256;
						static NkVector<uint8> pxF;
						pxF.Resize(TS * TS * 4);
						// SOUS-GRILLE (regle de Rihen) : chaque grand carreau porte
						// DIX petits rectangles, traces par des lignes en ALTERNANCE
						// -- une moyennement foncee, une plus claire, et ainsi de
						// suite (5 + 5). Retourne 0 = pas de ligne, 1 = claire,
						// 2 = moyenne ; `span` est la taille du grand carreau.
						auto sub10 = [TS](uint32 t, uint32 span) -> uint8 {
							const uint32 l = t % span;
							for (uint32 i = 1; i < 10; ++i)
								if (l == (i * span) / 10u)
									return (i & 1u) ? 1u : 2u;
							return 0u;
						};
						for (uint32 ty = 0; ty < TS; ++ty) {
							for (uint32 tx = 0; tx < TS; ++tx) {
								uint8 v = 208;
								if (pi == 0) {
									// DAMIER : 2x2 cases par texture (une periode),
									// et la MEME sous-grille de 10 dans chaque case
									// (lignes en assombrissement de la case). La
									// FRONTIERE entre case claire et case sombre
									// porte son propre trait (regle de Rihen) : sans
									// lui, deux cases se touchaient sans couture.
									const bool aC = (((tx * 2u) / TS) ^ ((ty * 2u) / TS)) & 1u;
									v = aC ? 205 : 152;
									if (tx % (TS / 2u) < 1u || ty % (TS / 2u) < 1u)
										v = 110;
									else {
										const uint8 lx = sub10(tx, TS / 2u);
										const uint8 ly = sub10(ty, TS / 2u);
										const uint8 lm = lx > ly ? lx : ly;
										if (lm == 2u)
											v = (uint8)(v - 45u);
										else if (lm == 1u)
											v = (uint8)(v - 22u);
									}
								} else {
									// CARREAUX : joints sombres au bord (FINS -- les
									// traits epais mangeaient les dalles, Rihen),
									// puis la sous-grille de 10 alternee.
									if (tx < 2u || ty < 2u || tx >= TS - 2u || ty >= TS - 2u)
										v = 110;
									else {
										const uint8 lx = sub10(tx, TS);
										const uint8 ly = sub10(ty, TS);
										const uint8 lm = lx > ly ? lx : ly;
										if (lm == 2u)
											v = 150;
										else if (lm == 1u)
											v = 192;
									}
								}
								const uint32 o4 = (ty * TS + tx) * 4u;
								pxF[o4 + 0] = v;
								pxF[o4 + 1] = v;
								pxF[o4 + 2] = v;
								pxF[o4 + 3] = 255;
							}
						}
						NkTextureCreateDesc tdF;
						tdF.pixels = pxF.Data();
						tdF.width = TS;
						tdF.height = TS;
						tdF.srgb = true;
						tdF.debugName = pi == 0 ? "Demo3D_SolDamier" : "Demo3D_SolCarreaux";
						sFloorTex[pi] = texL->Create(tdF);
					}
					if (!sFloorMat[pi] && matS && sFloorTex[pi].IsValid()) {
						sFloorMat[pi] = NkMaterial::Create(matS, NkMaterialType::NK_PBR_METALLIC);
						if (sFloorMat[pi])
							sFloorMat[pi]->SetAlbedoMap(sFloorTex[pi]);
					}
					if (msF && (!sFloorMesh.IsValid() || sFloorMeshTile != tileF ||
								sFloorMeshPat != patF)) {
						if (sFloorMesh.IsValid())
							msF->Release(sFloorMesh);
						const float32 K = kExtF / perF; // repetitions jusqu'au bord
						NkVertex3D vF[4] = {};
						const float32 sgn[4][2] = {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}};
						for (int32 i4 = 0; i4 < 4; ++i4) {
							vF[i4].pos = {sgn[i4][0], 0.f, sgn[i4][1]};
							vF[i4].normal = {0.f, 1.f, 0.f};
							vF[i4].tangent = {1.f, 0.f, 0.f};
							vF[i4].uv = {sgn[i4][0] * K, sgn[i4][1] * K};
							vF[i4].uv2 = vF[i4].uv;
							vF[i4].color = 0xFFFFFFFFu;
						}
						// Les DEUX sens d'enroulement : le sol se voit d'en haut
						// comme d'en bas, et on ne depend pas du culling du
						// pipeline.
						const uint32 iF[12] = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
						renderer::NkMeshDesc dF = renderer::NkMeshDesc::Simple(
							renderer::NkVertexLayout::Default3D(), vF, 4, iF, 12);
						sFloorMesh = msF->Create(dF);
						sFloorMeshTile = tileF;
						sFloorMeshPat = patF;
					}
					if (sFloorMesh.IsValid())
						dcF.mesh = sFloorMesh;
					if (sFloorMat[pi]) {
						// RECEPTEUR D'OMBRE le temps d'une sortie : le sol cesse
						// de se peindre et ne rend que l'ombre qu'il recoit, ce
						// qui donne son poids a un objet detoure. Pose ici, sur
						// le materiau reellement soumis, et remis juste apres --
						// le sol reste un sol dans la vue.
						sFloorMat[pi]->SetShadowCatcher(nkvpOutPhase != 0 &&
														(nkvpOutAids & 2048) != 0);
						dcF.material = sFloorMat[pi]->GetInstHandle();
					}
				}
				// SNAP a la periode (plancher, pas troncature : traverser zero
				// ne fait pas sauter le motif).
				//
				// LE SOL EST EXACTEMENT A nkvpFloorY -- plus aucun retrait. Les
				// 2 mm « anti z-fight » dataient d'avant le biais de profondeur de
				// la grille : elle tire deja sa profondeur vers la camera (-1.5),
				// elle gagne donc le z-fight SANS qu'on enterre le sol. Et ces
				// 2 mm se VOYAIENT : l'ombre d'un objet pose a y=0 se calculait
				// 2 mm sous son pied, premiere moitie du « cube qui flotte »
				// constate par Rihen (l'autre moitie etant le biais normal, cf.
				// NkVirtualShadowMaps.h). Un plan MAILLE cree a y=0 n'avait pas ce
				// defaut -- c'est ce contraste qui a designe le coupable.
				const NkVec3f cpF = cam.GetPosition();
				const float32 qxF = cpF.x / perF;
				const float32 qzF = cpF.z / perF;
				const float32 fxS = perF * (float32)((int64)(qxF >= 0.f ? qxF : qxF - 1.f));
				const float32 fzS = perF * (float32)((int64)(qzF >= 0.f ? qzF : qzF - 1.f));
				dcF.transform = NkMat4f::Translate({fxS, nkvpFloorY, fzS}) *
								NkMat4f::Scale({kExtF, 1.f, kExtF});
				dcF.aabb = {{fxS - kExtF, nkvpFloorY - 0.01f, fzS - kExtF},
							{fxS + kExtF, nkvpFloorY, fzS + kExtF}};
				dcF.castShadow = false;
				dcF.tint = {nkvpFloorColor[0], nkvpFloorColor[1], nkvpFloorColor[2]};
				dcF.metallic = nkvpFloorMetal;
				dcF.roughness = nkvpFloorRough;
				r3d->Submit(dcF);
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
				if (uk < 1 || uk > 3)
					continue; // seuls les MAILLAGES se rendent (une lumiere kind 5
							  // tombait dans le cas « plan » -- constate par Rihen)
				const int32 un = kNkvpFirstUser + u;
				if (nkvpIsModel[un])
					continue; // conteneur : sa geometrie vit dans ses maillages
				if (HostHiddenEff(un))
					continue;
				const int32 e = un - 90;
				const NkVec3f utr = st->emptyGizmo.TranslateOf(e);
				const NkVec3f uos = st->emptyGizmo.ScaleOf(e);
				const NkMat4f uR = st->emptyGizmo.RotationOf(e) * HostNodeQuat(e).ToMat4();
				NkDrawCall3D dc;
				// La VARIANTE demandee au menu choisit la vraie primitive
				// (tore : generateur moteur a venir, sphere en attendant).
				const uint8 usv = nkvpUserSub[u];
				dc.mesh = st->meshPlane;
				if (uk == 1)
					dc.mesh = usv == 1 ? st->meshIco : st->meshSphere;
				else if (uk == 2)
					dc.mesh = usv == 1 ? st->meshCylinder
										   : (usv == 2 ? st->meshCone : st->meshCube);
				if (nkvpUserMesh[u].IsValid())
					dc.mesh = nkvpUserMesh[u]; // mesh parametrique regenere
				dc.transform = HostEmptyXform(e, true); // point de passage unique
				// AABB MONDE REELLE (boite locale transformee), plus le cube
				// « echelle max » : un sol aplati 25x0,5x25 annoncait 50 m de
				// HAUT, l'auto-fit des ombres directionnelles etendait la
				// couverture de cette hauteur fantome (rayon 116 m, texel 23 cm,
				// prouve par la trace VSMAutoFit) et l'ombre du soleil se
				// reduisait a une tache (defaut 4.7). Boite locale = celle du
				// mesh CPU si disponible (mesh edite/parametrique), sinon le
				// cube unitaire des primitives (+-1).
				{
					NkVec3f lmn{-1.f, -1.f, -1.f}, lmx{1.f, 1.f, 1.f};
					if (auto *msA = ctx.renderer->GetMeshSystem()) {
						if (msA->HasCPUData(dc.mesh)) {
							const auto *vvA = (const renderer::NkVertex3D *)msA->GetVertices(dc.mesh);
							const uint32 vcA = msA->GetVertexCount(dc.mesh);
							if (vvA && vcA > 0) {
								lmn = {1e30f, 1e30f, 1e30f};
								lmx = {-1e30f, -1e30f, -1e30f};
								for (uint32 vi = 0; vi < vcA; vi++) {
									const NkVec3f p = vvA[vi].pos;
									lmn.x = NkMin(lmn.x, p.x);
									lmn.y = NkMin(lmn.y, p.y);
									lmn.z = NkMin(lmn.z, p.z);
									lmx.x = NkMax(lmx.x, p.x);
									lmx.y = NkMax(lmx.y, p.y);
									lmx.z = NkMax(lmx.z, p.z);
								}
							}
						}
					}
					// Transform COMPLETE : HostMatHook ajoute les dimensions a
					// dc.transform APRES ce bloc -- on les compose ici aussi,
					// sinon la boite ignorerait le panneau Dimensions.
					NkMat4f fullX = dc.transform;
					if (nkvpBaseSet[un])
						fullX = fullX * NkMat4f::Scale({nkvpDimFactor[un][0], nkvpDimFactor[un][1],
														nkvpDimFactor[un][2]});
					NkVec3f wmn{1e30f, 1e30f, 1e30f}, wmx{-1e30f, -1e30f, -1e30f};
					for (int32 c8 = 0; c8 < 8; c8++) {
						const NkVec3f l{(c8 & 1) ? lmx.x : lmn.x, (c8 & 2) ? lmx.y : lmn.y,
										(c8 & 4) ? lmx.z : lmn.z};
						const NkVec3f p = fullX * l;
						wmn.x = NkMin(wmn.x, p.x);
						wmn.y = NkMin(wmn.y, p.y);
						wmn.z = NkMin(wmn.z, p.z);
						wmx.x = NkMax(wmx.x, p.x);
						wmx.y = NkMax(wmx.y, p.y);
						wmx.z = NkMax(wmx.z, p.z);
					}
					dc.aabb = {{wmn.x - 0.05f, wmn.y - 0.05f, wmn.z - 0.05f},
							   {wmx.x + 0.05f, wmx.y + 0.05f, wmx.z + 0.05f}};
				}
				// UN MAILLAGE NE VIT PAS SANS MATERIAU (regle de Rihen) : ceux
				// crees avant la regle, ou par un chemin qui l'ignorerait, sont
				// RATTRAPES ici, au point de passage de la soumission.
				if (nkvpNodeMatP1[un] == 0)
					nkvpNodeMatP1[un] = HostEnsureDefaultMat() + 1;
				// Le PLAN INFINI ne projette pas d'ombre : rien ne vit dessous, et
				// un caster de +-1500 m gonflerait l'auto-fit VSM jusqu'a rendre
				// le texel inutilisable (meme regle que le sol systeme). A
				// revoir quand la sculpture lui donnera du relief.
				dc.castShadow = !(uk == 3 && usv == 3);
				dc.tint = effTint({0.7f, 0.7f, 0.72f});
				dc.metallic = 0.f;
				dc.roughness = 0.85f; // mat par defaut, sans brillance marquee
				HostMatHook(un, dc);
				// TEXTURES du materiau projet : des qu'un canal QUELCONQUE
				// existe, le maillage passe sur la vraie instance moteur -- la
				// couleur du materiau reste en TEINTE par-dessus. Tester le
				// seul albedo laissait une normal map ou un emissif sans effet
				// tant qu'aucune texture de couleur n'etait posee.
				{
					const int32 pmU = nkvpNodeMatP1[un] - 1;
					if (pmU >= 0 && pmU < kNkvpMaxProjMats && nkvpProjMats[pmU].used &&
						nkvpProjMatEng[pmU]) {
						bool anyMap = false;
						for (int32 c = 0; c < kNkvpMatChanCount && !anyMap; ++c)
							anyMap = nkvpProjMats[pmU].maps[c][0] != 0;
						// L'instance moteur est AUSSI requise des que le TYPE n'est
						// plus Standard ou qu'un MELANGE est actif : sans elle, un
						// Toon sans texture restait rendu en PBR generique — « j'ai
						// change en toon mais le materiau n'a pas suivi » (Rihen).
						// TOUJOURS liee desormais : l'anisotropie et le sheen vivent
						// dans l'UBO d'instance — sans liaison, leurs curseurs seraient
						// muets sur un PBR sans texture. (void)anyMap : la variable
						// documente encore le cas historique.
						(void)anyMap;
						dc.material = nkvpProjMatEng[pmU]->GetInstHandle();
					}
				}
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
					// ── EDITION PROPORTIONNELLE : distances calculees ICI, une
					// fois pour tout le geste. Les refaire a chaque image
					// couterait nv x nsel par frame, et surtout la reference
					// bougerait avec les sommets deja deplaces -- l'influence
					// deriverait en cours de route.
					if (nkvpPropEditOn) {
						nkvpPropDist.Resize((uint32)nv);
						for (int32 i = 0; i < nv; ++i) {
							if (st->vertSel[i]) {
								nkvpPropDist[(uint32)i] = 0.f;
								continue;
							}
							float32 best = 1e30f;
							const NkVec3f pi = st->editRest[i].pos;
							for (int32 j = 0; j < nv; ++j) {
								if (!st->vertSel[j])
									continue;
								const NkVec3f pj = st->editRest[j].pos;
								const float32 dx = pi.x - pj.x, dy = pi.y - pj.y,
											  dz = pi.z - pj.z;
								const float32 d2 = dx * dx + dy * dy + dz * dz;
								if (d2 < best)
									best = d2;
							}
							nkvpPropDist[(uint32)i] = math::NkSqrt(best);
						}
					}
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
						if (!st->vertSel[i]) {
							// ── LES VOISINS SUIVENT, D'AUTANT MOINS QU'ILS SONT
							// LOIN. Le sommet parcourt une FRACTION du chemin que
							// ferait un sommet selectionne : c'est ce qui donne le
							// renflement doux au lieu d'une bosse a arete vive.
							if (!nkvpPropEditOn || (uint32)i >= nkvpPropDist.Size())
								continue;
							const float32 w = HostPropFalloff(nkvpPropDist[(uint32)i],
															  nkvpPropEditRadius,
															  nkvpPropEditFalloff);
							if (w <= 0.001f)
								continue;
							const NkVec3f full =
								st->editAnchorInv * (Gcommon * worldV(i));
							const NkVec3f rest = st->editRest[i].pos;
							st->editLive[i].pos = {rest.x + (full.x - rest.x) * w,
												   rest.y + (full.y - rest.y) * w,
												   rest.z + (full.z - rest.z) * w};
							continue;
						}
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
						// REGLES par TYPE (Rihen) : ponctuelle = deplacement seul ;
						// soleil/spot = + rotation ; surfacique = + echelle. Le mode
						// demande est rabattu si interdit.
						if (st->lightSel >= 0 && st->lightSel < Demo3DState::kNumLights) {
							const int32 lt2 = (int32)st->lights[st->lightSel].type & 3;
							int32 md = st->gizmo.Mode();
							if (md == 1 && lt2 == 1)
								md = 0;
							if (md == 2 && lt2 != 3)
								md = 0;
							if (md == 3 && lt2 == 1)
								md = 0;
							st->lightGizmo.SetMode(md);
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
							// BASE (sans les decalages du gizmo : il les porte lui-meme)
							etg[e].base = HostEmptyXform(e, false);
							etg[e].localHalf = {0.f, 0.f, 0.f};
							etg[e].pickRadius = 0.f; // pas de pick : la hierarchie selectionne
						}
						renderer::NkGizmoInput ein = gin;
						// Memes regles pour une LUMIERE utilisateur selectionnee ;
						// les autres noeuds gardent le mode demande.
						{
							const int32 ea = st->emptyGizmo.ActiveIndex();
							if (ea >= 6 && nkvpUserKind[ea - 6] == 5) {
								const int32 lt2 = (int32)nkvpUserLight[ea - 6].type & 3;
								int32 md = st->gizmo.Mode();
								if (md == 1 && lt2 == 1)
									md = 0;
								if (md == 2 && lt2 != 3)
									md = 0;
								if (md == 3 && lt2 == 1)
									md = 0;
								st->emptyGizmo.SetMode(md);
							} else {
								st->emptyGizmo.SetMode(st->gizmo.Mode());
							}
						}
						// ── EDITION PROPORTIONNELLE SUR LES OBJETS ──────────────
					// Les distances sont figees au DEBUT du geste : les refaire
					// en cours de route ferait deriver l'influence, puisque les
					// objets deja entraines deviendraient eux-memes references.
					if (nkvpPropEditOn && !st->emptyGizmo.IsDragging())
						nkvpPropNodeArmed = false;
					if (nkvpPropEditOn && st->emptyGizmo.IsDragging() &&
						!nkvpPropNodeArmed) {
						nkvpPropNodeArmed = true;
						// LE PIVOT AUSSI EST FIGE : le gizmo le recalcule a chaque
						// image depuis les centres DEJA transformes, donc il suit le
						// geste. Rotation et echelle des voisins doivent tourner
						// autour du pivot du DEBUT, sinon le centre fuit et les
						// voisins partent en spirale.
						nkvpPropPivot = st->emptyGizmo.GetPivot();
						for (int32 a = 0; a < 70; ++a) {
							if (st->emptyGizmo.IsSelected(a)) {
								nkvpPropDistNode[a] = 0.f;
								continue;
							}
							float32 best = 1e30f;
							for (int32 b = 0; b < 70; ++b) {
								if (!st->emptyGizmo.IsSelected(b))
									continue;
								const float32 dx = nkvpEmptyPos[a][0] - nkvpEmptyPos[b][0];
								const float32 dy = nkvpEmptyPos[a][1] - nkvpEmptyPos[b][1];
								const float32 dz = nkvpEmptyPos[a][2] - nkvpEmptyPos[b][2];
								const float32 d2 = dx * dx + dy * dy + dz * dz;
								if (d2 < best)
									best = d2;
							}
							nkvpPropDistNode[a] = math::NkSqrt(best);
						}
					}
					const bool ewasDrag = st->emptyGizmo.IsDragging();
						// Reperes Gimbal / Parent rafraichis avant le geste : ils
						// dependent du noeud ACTIF, qui change sans que
						// l'orientation, elle, ne change.
						HostPushExtFrames(st, st->emptyGizmo.Orientation());
						st->emptyGizmo.Update(etg, 70, ein);
						if (!ewasDrag && st->emptyGizmo.IsDragging())
							gin.leftPressed = false; // poignee saisie : le clic est a nous
						if (st->emptyDragPrev && !st->emptyGizmo.IsDragging()) {
							// REPLI : effective -> base, gizmo remis a zero (meme
							// regle que les lumieres, sinon base et decalages
							// divergent).
							// TOUTE LA SELECTION, pas seulement l'ACTIF : le gizmo
							// calcule bien une transformation par objet
							// selectionne, mais on n'en repliait qu'une -- les
							// autres perdaient la leur au ResetSelected() et ne
							// bougeaient pas (constate par Rihen).
							bool anyCommit = false;
							// LES VOISINS D'ABORD : leur deplacement se lit sur la
							// translation de l'ACTIF, qui est encore intacte tant
							// qu'on n'a rien commite.
							if (nkvpPropEditOn && nkvpPropNodeArmed) {
								const int32 sA0 = st->emptyGizmo.ActiveIndex();
								const int32 sA = sA0 >= 0 ? sA0 : 0;
								// LES TROIS COMPOSANTES, comme l'apercu. La POSITION
								// passe par la matrice attenuee (elle contient la
								// rotation et l'echelle autour du pivot fige : un
								// voisin s'ecarte quand on agrandit, decrit un arc
								// quand on tourne). L'orientation et l'echelle PROPRES
								// du voisin suivent la meme fraction du geste.
								const NkQuatf qg =
									NkQuatf(st->emptyGizmo.RotationOf(sA)).Normalized();
								const NkVec3f og = st->emptyGizmo.ScaleOf(sA);
								for (int32 es = 0; es < 70; ++es) {
									if (st->emptyGizmo.IsSelected(es))
										continue;
									const float32 w =
										HostPropFalloff(nkvpPropDistNode[es],
														nkvpPropEditRadius,
														nkvpPropEditFalloff);
									if (w <= 0.001f)
										continue;
									const NkMat4f W = st->emptyGizmo.ApplyAboutWeighted(
										sA, nkvpPropPivot, w);
									const NkVec3f P = W * NkVec3f{nkvpEmptyPos[es][0],
																  nkvpEmptyPos[es][1],
																  nkvpEmptyPos[es][2]};
									const NkQuatf qw = NkQuatf::Identity().SLerp(qg, w);
									// ── ON N'ECRIT JAMAIS UN NaN DANS L'ETAT ────────
									// Un seul terme degenere contaminait la scene
									// ENTIERE et plus rien ne la rattrapait : position
									// et rotation restaient « nan » a l'ecran (constate
									// par Rihen). Le voisin est laisse INTACT et le
									// journal dit QUEL terme a degenere -- ca se
									// diagnostique, ca ne se devine pas.
									const bool okP = math::NkIsFinite(P.x) &&
													 math::NkIsFinite(P.y) &&
													 math::NkIsFinite(P.z);
									const bool okQ =
										math::NkIsFinite(qw.x) && math::NkIsFinite(qw.y) &&
										math::NkIsFinite(qw.z) && math::NkIsFinite(qw.w);
									if (!okP || !okQ) {
										logger.Error("[PropEdit] terme degenere noeud={0} w={1} "
													 "posOk={2} quatOk={3} qg=({4};{5};{6};{7})\n",
													 es, w, okP ? 1 : 0, okQ ? 1 : 0, qg.x, qg.y,
													 qg.z, qg.w);
										logger.Error("[PropEdit]   piv=({0};{1};{2}) "
													 "pos=({3};{4};{5}) qwOk={6}\n",
													 nkvpPropPivot.x, nkvpPropPivot.y,
													 nkvpPropPivot.z, nkvpEmptyPos[es][0],
													 nkvpEmptyPos[es][1], nkvpEmptyPos[es][2],
													 okQ ? 1 : 0);
										continue; // ce voisin ne bouge pas, la scene survit
									}
									nkvpEmptyPos[es][0] = P.x;
									nkvpEmptyPos[es][1] = P.y;
									nkvpEmptyPos[es][2] = P.z;
									// Rotation attenuee : SLerp depuis l'identite --
									// l'angle diminue, l'axe reste celui du geste.
									HostSetNodeQuat(es, qw * HostNodeQuat(es));
									nkvpEmptyScl[es][0] *= (1.f + og.x * w);
									nkvpEmptyScl[es][1] *= (1.f + og.y * w);
									nkvpEmptyScl[es][2] *= (1.f + og.z * w);
									anyCommit = true;
								}
								nkvpPropNodeArmed = false; // le geste est fini
							}
							for (int32 es = 0; es < 70; ++es) {
								if (!st->emptyGizmo.IsSelected(es))
									continue;
								// ON COMMIT LA MATRICE REELLEMENT COMPOSEE, pas les
								// morceaux recomposes a notre facon : l'echelle du
								// geste vit dans le REPERE du geste, et la
								// reappliquer aux axes locaux du noeud rendait
								// GLOBAL et LOCAL identiques au relachement -- on
								// voyait la difference pendant le glissement, elle
								// disparaissait a la fin (constate par Rihen). La
								// decomposition rend exactement ce qui etait
								// affiche, pour les sept reperes d'un coup.
								// ── ECHELLE EXACTE : on MEMORISE le repere du geste
								// quand l'option est active et que ce repere n'est
								// pas celui de l'objet. La transform devient alors
								// T * (B S Bt) * R et le cisaillement est conserve
								// -- sinon la decomposition ci-dessous le perdrait,
								// puisque trois facteurs ne peuvent pas le porter.
								if (nkvpShearOpt && st->emptyGizmo.Orientation() !=
													   renderer::NkGizmo3D::ORIENT_LOCAL) {
									NkVec3f gb[3];
									st->emptyGizmo.ScaleBasisOf(es, gb);
									for (int32 a = 0; a < 3; ++a)
										nkvpEmptySclAx[es][a] = gb[a];
									nkvpEmptyShear[es] = true;
								}
								// PAS DE « sinon » qui efface : un geste en repere
								// LOCAL n'a aucune raison de DETRUIRE le
								// cisaillement deja porte par l'objet -- il le
								// perdait d'un coup (constate par Rihen). La base
								// memorisee reste, le nouveau facteur s'y compose.
								// Seule la coupure de l'option redresse les objets,
								// et c'est un geste explicite.
								// ── COMMIT PAR MORCEAUX, PAS PAR DECOMPOSITION ──────
								// Decomposer la matrice composee la lisait en T*R*S,
								// alors que HostEmptyXform la RECONSTRUIT en
								// T*(B S Bt)*R : deux formules differentes, donc un
								// SAUT au relachement (constate par Rihen -- « ca
								// scale un peu tout en gardant le cisaillement »).
								// Chaque morceau se commit dans SON espace, ou il est
								// exact : la translation en monde, la rotation comme
								// produit de rotations PURES (decomposition continue,
								// sans perte), et l'echelle comme facteurs dans la
								// base du geste -- celle qu'on vient de memoriser.
								const NkVec3f tr = st->emptyGizmo.TranslateOf(es);
								nkvpEmptyPos[es][0] += tr.x;
								nkvpEmptyPos[es][1] += tr.y;
								nkvpEmptyPos[es][2] += tr.z;
								// ROTATION : composition de QUATERNIONS, sans passer
								// par les angles. C'est ce qui supprime le gimbal
								// lock : rien n'est decompose, donc rien ne peut
								// degenerer ni sauter de 180 degres.
								HostSetNodeQuat(es, NkQuatf(st->emptyGizmo.RotationOf(es)) *
														HostNodeQuat(es));
								const NkVec3f os = st->emptyGizmo.ScaleOf(es);
								nkvpEmptyScl[es][0] *= (1.f + os.x);
								nkvpEmptyScl[es][1] *= (1.f + os.y);
								nkvpEmptyScl[es][2] *= (1.f + os.z);
								anyCommit = true;
							}
							if (anyCommit)
								st->emptyGizmo.ResetSelected(); // une fois, apres tous
						}
						st->emptyDragPrev = st->emptyGizmo.IsDragging();
					}
					// PICK PRECIS AU TRIANGLE pour les objets de demo aussi (regle de
					// Rihen : la selection se decide sur le mesh reel, partout).
					static Demo3DRayCtx sRayCtx;
					sRayCtx.st = st;
					sRayCtx.ms = ctx.renderer->GetMeshSystem();
					st->gizmo.SetRayPickTest(&Demo3D_GizmoRayTest, &sRayCtx);
					// L'AIMANTATION GEOMETRIQUE interroge le meme contexte ; les
					// deux gizmos de deplacement la recoivent (l'oubli du gizmo
					// des vides a deja coute une fois).
					st->gizmo.SetSnapQuery(&Demo3D_SnapQuery, &sRayCtx);
					st->emptyGizmo.SetSnapQuery(&Demo3D_SnapQuery, &sRayCtx);
					st->gizmo.Update(targets, n, gin);
						// PICK ECRAN des MAILLAGES UTILISATEUR -- APRES le gizmo objets :
						// une FLECHE saisie garde son clic, un objet derriere elle
						// n'est plus vole (constate par Rihen). Un clic dans la vue
						// les selectionne comme n'importe quel objet (Rihen). Meme
						// principe que les lumieres : centre projete, rayon approche.
						if (gin.leftPressed && !st->emptyGizmo.IsDragging() &&
							!st->gizmo.IsDragging()) { // une POIGNEE saisie a priorite
							const Demo3D_ScreenProj uproj = Demo3D_ScreenProj::Make(
								cam.GetPosition(), cam.GetTarget(), 60.f, (float32)ctx.width,
								(float32)ctx.height);
							const NkVec3f camP2 = cam.GetPosition();
							const NkVec3f fwd2 = (cam.GetTarget() - camP2).Normalized();
							const NkVec3f rgt2 = fwd2.Cross(NkVec3f{0.f, 1.f, 0.f}).Normalized();
							int32 bestU = -1; // widgets : distance ECRAN
								// Maillages : candidat par RAYON-TRIANGLE sur le mesh
								// reel, metrique = t du rayon (le plus PROCHE gagne).
								int32 bestMeshU = -1;
								float32 bestMeshT = 1e30f;
								// Rayon MONDE du pixel clique -- meme convention que
								// uproj (inverse exact de sa projection).
								const float32 rnx = 2.f * gin.mouseX / uproj.vw - 1.f;
								const float32 rny = 1.f - 2.f * gin.mouseY / uproj.vh;
								const NkVec3f rdW{uproj.fwd.x + uproj.rgt.x * rnx * uproj.thX +
													  uproj.upv.x * rny * uproj.thY,
												  uproj.fwd.y + uproj.rgt.y * rnx * uproj.thX +
													  uproj.upv.y * rny * uproj.thY,
												  uproj.fwd.z + uproj.rgt.z * rnx * uproj.thX +
													  uproj.upv.z * rny * uproj.thY};
								auto *msPick = ctx.renderer->GetMeshSystem();
								const float32 kD2Rp = 0.017453292f;
							float32 bestD2 = 1e30f;
							for (int32 u2 = 0; u2 < kNkvpMaxUser; ++u2) {
								const uint8 uk2 = nkvpUserKind[u2];
								if (uk2 == 0)
									continue; // slot libre -- tout le reste se clique
								const int32 un2 = kNkvpFirstUser + u2;
								if (nkvpIsModel[un2])
									continue; // un model se clique PAR SA MATIERE : sa
											  // propre origine ne dessine rien, et y
											  // laisser une cible faisait une zone
											  // fantome loin de ses maillages (Rihen)
								if (HostHiddenEff(un2) || HostLockedEff(un2))
									continue;
								const int32 e2 = un2 - 90;
									if (uk2 >= 1 && uk2 <= 3) {
										// MAILLAGE : le clic se decide sur le MESH REEL
										// (rayon-triangle), transform monde identique a
										// celui du rendu. Le disque approche d'avant
										// (rayon = plus grande echelle) volait le vide
										// voisin des objets agrandis.
										NkMeshHandle pm = st->meshPlane;
										const uint8 usv2 = nkvpUserSub[u2];
										if (uk2 == 1)
											pm = usv2 == 1 ? st->meshIco : st->meshSphere;
										else if (uk2 == 2)
											pm = usv2 == 1 ? st->meshCylinder
														   : (usv2 == 2 ? st->meshCone
																		: st->meshCube);
										if (nkvpUserMesh[u2].IsValid())
											pm = nkvpUserMesh[u2];
										NkMat4f W = HostEmptyXform(e2, true); // point de passage unique
										if (nkvpBaseSet[un2])
											W = W * NkMat4f::Scale({nkvpDimFactor[un2][0],
																	nkvpDimFactor[un2][1],
																	nkvpDimFactor[un2][2]});
										float32 tHit = bestMeshT;
										if (Demo3D_RayMeshT(msPick, pm, W, camP2, rdW, tHit)) {
											bestMeshT = tHit;
											bestMeshU = e2;
										}
										continue;
									}
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
								float32 sx0 = 0.f, sy0 = 0.f;
								if (!uproj(c2, sx0, sy0))
									continue;
								// Maillage : rayon MONDE projete ; widgets (lumiere,
								// camera, vides, marqueurs) : taille ecran constante.
								float32 rpix = 16.f;
								if (uk2 >= 1 && uk2 <= 3) {
									float32 sx1 = 0.f, sy1 = 0.f;
									const NkVec3f edge{c2.x + rgt2.x * rw, c2.y + rgt2.y * rw,
													   c2.z + rgt2.z * rw};
									if (!uproj(edge, sx1, sy1))
										continue;
									rpix = sqrtf((sx1 - sx0) * (sx1 - sx0) +
												 (sy1 - sy0) * (sy1 - sy0));
								}
								const float32 pdx = gin.mouseX - sx0, pdy = gin.mouseY - sy0;
								const float32 pd2 = pdx * pdx + pdy * pdy;
								if (pd2 <= rpix * rpix && pd2 < bestD2) {
									bestD2 = pd2;
									bestU = e2;
								}
							}
							// PRIORITE aux widgets (petite cible ecran, intention
							// precise) ; sinon le maillage reellement TOUCHE par le
							// rayon -- et rien touche = deselection plus bas.
							if (bestU < 0)
								bestU = bestMeshU;
							// DANS UNE SCENE, cliquer un MESH INTERNE selectionne TOUT
							// le model auquel il appartient (facon Blender) ; dans
							// l'editeur de model, chaque mesh se selectionne
							// individuellement (regle de Rihen).
							if (bestU >= 0 && !nkvpDocIsModel) {
								const int32 nB = bestU + kNkvpFirstEmpty;
								const int32 rB = Demo3DHostModelRootOf(nB);
								if (rB != nB && rB >= kNkvpFirstEmpty)
									bestU = rB - kNkvpFirstEmpty;
							}
							const bool clickedActive =
								(bestU >= 0 && bestU == st->emptyGizmo.ActiveIndex());
							if (clickedActive)
								bestU = -1; // deja actif : le clic est aux POIGNEES
							if (bestU >= 0) {
								st->gizmo.ClearSelection();
								st->lightGizmo.ClearSelection();
								st->lightSel = -1;
								if (gin.shiftDown || gin.ctrlDown)
									st->emptyGizmo.ToggleSelection(bestU); // multi successif
								else
									st->emptyGizmo.Select(bestU);
								gin.leftPressed = false; // le clic est a nous
							} else if (!clickedActive && !gin.shiftDown && !gin.ctrlDown) {
								// CLIC DANS LE VIDE : les maillages UTILISATEUR se
								// deselectionnent AUSSI. Ce chemin ne gerait que la
								// selection -- ne rien faire ici laissait un plan ou un
								// model selectionne POUR TOUJOURS quand on cliquait
								// dans le vide (constate par Rihen ; l'intermittence
								// venait des clics qui tombaient dans le disque de pick
								// d'un AUTRE objet, qui changeaient donc la selection).
								// Les objets de demo, eux, se deselectionnaient deja
								// via le gizmo.
								st->emptyGizmo.ClearSelection();
							}
						}
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
					// MAILLAGES UTILISATEUR selectionnes : le MEME lisere
					// silhouette que les objets natifs (Rihen), transformation
					// EFFECTIVE identique au rendu (base + gizmo + dimensions).
					if (r3d->IsSelectionOutlineEnabled()) {
						const int32 esel2 = st->emptyGizmo.ActiveIndex();
						const float32 kD2Ro = 0.017453292f;
						for (int32 u = 0; u < kNkvpMaxUser; ++u) {
							if (nkvpUserKind[u] < 1 || nkvpUserKind[u] > 3)
								continue;
							const int32 e = 6 + u;
							const int32 un = kNkvpFirstUser + u;
							if (nkvpIsModel[un])
								continue; // il ne rend rien : ses maillages le cernent
							if (HostHiddenEff(un))
								continue;
							// LA ZONE D'UN MODEL, C'EST SA MATIERE (Rihen) : un
							// maillage se lisere aussi quand c'est SON MODEL qui est
							// selectionne -- sinon le lisere restait a l'origine du
							// model, souvent loin des maillages qu'il porte.
							bool selOut = st->emptyGizmo.IsSelected(e);
							if (!selOut && nkvpIsMesh[un]) {
								const int32 mr = Demo3DHostModelRootOf(un);
								selOut = mr >= kNkvpFirstEmpty &&
										 st->emptyGizmo.IsSelected(mr - kNkvpFirstEmpty);
							}
							if (!selOut)
								continue;
							NkDrawCall3D sdc2;
							{
								const uint8 usv2 = nkvpUserSub[u];
								sdc2.mesh = st->meshPlane;
								if (nkvpUserKind[u] == 1)
									sdc2.mesh = usv2 == 1 ? st->meshIco : st->meshSphere;
								else if (nkvpUserKind[u] == 2)
									sdc2.mesh = usv2 == 1
													? st->meshCylinder
													: (usv2 == 2 ? st->meshCone : st->meshCube);
								if (nkvpUserMesh[u].IsValid())
									sdc2.mesh = nkvpUserMesh[u];
							}
							sdc2.transform = HostEmptyXform(e, true); // point de passage unique
							if (nkvpBaseSet[un])
								sdc2.transform = sdc2.transform *
												 NkMat4f::Scale({nkvpDimFactor[un][0],
																 nkvpDimFactor[un][1],
																 nkvpDimFactor[un][2]});
							r3d->SubmitSelection(sdc2, e == esel2);
						}
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
			//
			// PAS PENDANT UN RENDU DE SORTIE. Les CAMERAS sont des empties : leur
			// pyramide de visee se dessine ici, et non avec les widgets de
			// lumiere. Couper `showLightGizmos` retirait donc les lumieres de
			// l'image mais laissait les cameras (constate par Rihen). Une
			// camera n'existe dans un rendu que par ce qu'elle CADRE, jamais par
			// sa representation -- comme dans Blender.
			// Pendant un rendu, deux aides distinctes gouvernent ce bloc : le
			// bit 4 pour les REPERES vides, le bit 8 pour les CAMERAS. Le test
			// fin se fait par noeud plus bas ; ici on ne saute tout que si
			// AUCUN des deux n'est demande.
			if (nkvpOutPhase != 0 && !(nkvpOutAids & (32 | 64))) {
				// rien : ni croix, ni pyramide, ni poignees
			} else {
				const int32 esel = st->emptyGizmo.ActiveIndex();
				for (int32 e = 0; e < 70; ++e) {
					// Croix pour les EMPTIES et les MARQUEURS types (texte, courbe,
					// surface, metaball) ; un maillage a son rendu, une lumiere son
					// widget, un slot libre n'existe pas.
					if (90 + e >= kNkvpFirstUser &&
						(nkvpUserKind[e - 6] == 0 ||
						 (nkvpUserKind[e - 6] >= 1 && nkvpUserKind[e - 6] <= 3) ||
						 nkvpUserKind[e - 6] == 5) &&
						nkvpUserKind[e - 6] != 10)
						continue;
					if (HostHiddenEff(90 + e))
						continue;
					// PENDANT UN RENDU : une CAMERA (sous-type 10) obeit a son
					// aide, un repere vide a la sienne. Les regrouper obligeait
					// a montrer les croix d'axes pour voir un cadrage (Rihen).
					if (nkvpOutPhase != 0) {
						const int32 uk9 = e - 6;
						const bool isCam9 = uk9 >= 0 && uk9 < kNkvpMaxUser &&
											nkvpUserKind[uk9] == 4 && nkvpUserSub[uk9] == 10;
						if (isCam9 ? !(nkvpOutAids & 64) : !(nkvpOutAids & 32))
							continue;
					}
					const NkVec3f etr = st->emptyGizmo.TranslateOf(e);
					const NkVec3f ep{nkvpEmptyPos[e][0] + etr.x, nkvpEmptyPos[e][1] + etr.y,
									 nkvpEmptyPos[e][2] + etr.z};
					const float32 eh = 0.35f;
					const bool esl = (e == esel);
					const NkVec4f ecol = esl ? NkVec4f{1.f, 0.75f, 0.25f, 1.f}
											 : NkVec4f{0.75f, 0.75f, 0.78f, 0.9f};
					// Chaque nature a SA forme (regle de Rihen) : cercle FERME
					// d'aretes sans face, camera en pyramide de visee, croix sinon.
					const uint8 dk2 = (90 + e >= kNkvpFirstUser) ? nkvpUserKind[e - 6] : (uint8)4;
					const uint8 ds2 = (90 + e >= kNkvpFirstUser) ? nkvpUserSub[e - 6] : (uint8)0;
					const NkMat4f cRm =
						st->emptyGizmo.RotationOf(e) * HostNodeQuat(e).ToMat4();
					const NkVec3f cOs = st->emptyGizmo.ScaleOf(e);
					const float32 sx2 = fabsf(nkvpEmptyScl[e][0]) * (1.f + cOs.x);
					const float32 sy2 = fabsf(nkvpEmptyScl[e][1]) * (1.f + cOs.y);
					const float32 sz2 = fabsf(nkvpEmptyScl[e][2]) * (1.f + cOs.z);
					auto cxf = [&](float32 lx, float32 ly, float32 lz) {
						lx *= sx2;
						ly *= sy2;
						lz *= sz2;
						return NkVec3f{
							ep.x + cRm.mat[0][0] * lx + cRm.mat[1][0] * ly + cRm.mat[2][0] * lz,
							ep.y + cRm.mat[0][1] * lx + cRm.mat[1][1] * ly + cRm.mat[2][1] * lz,
							ep.z + cRm.mat[0][2] * lx + cRm.mat[1][2] * ly + cRm.mat[2][2] * lz};
					};
					if (dk2 == 10 || (dk2 == 7 && (ds2 == 1 || ds2 == 3))) {
						// CERCLE ferme : 32 aretes, aucune face.
						NkVec3f prevPt{};
						for (int32 seg = 0; seg <= 32; ++seg) {
							const float32 an = (float32)seg * (6.2831853f / 32.f);
							const NkVec3f pt = cxf(cosf(an), 0.f, sinf(an));
							if (seg > 0)
								r3d->DrawDebugLine(prevPt, pt, ecol, 0.f, true);
							prevPt = pt;
						}
					} else if (dk2 == 4 && ds2 == 10) {
						// CAMERA : pyramide de visee DERIVEE DE L'OPTIQUE (focale
						// verticale x format de sortie), plus une forme figee.
						// Le rectangle du widget est ainsi EXACTEMENT le cadre de
						// l'image de la camera : en vue camera, il coincide avec
						// le voile et la capture -- une seule verite (Rihen : le
						// voile ne touchait pas les « vrais bords »).
						const int32 uCam = e - 6;
						float32 fovW = 50.f;
						if (uCam >= 0 && uCam < kNkvpMaxUser && nkvpUserCam[uCam][0] > 1.f)
							fovW = nkvpUserCam[uCam][0];
						const float32 dW = 0.7f;
						const bool orthoW =
							uCam >= 0 && uCam < kNkvpMaxUser && nkvpUserCamOrtho[uCam];
						// PERSPECTIVE : pyramide (l'ouverture suit la focale).
						// ORTHO : TUBE rectangulaire (rayons paralleles) --
						// demi-hauteur locale 1, l'ECHELLE Y du noeud est donc la
						// demi-hauteur monde du cadre, comme au rendu.
						const float32 hyW =
							orthoW ? 1.f : dW * math::NkTan(fovW * 0.5f * 0.017453292f);
						const float32 hxW = hyW * HostCamAspect();
						const NkVec3f apex = cxf(0.f, 0.f, 0.f);
						NkVec3f cw[4];
						cw[0] = cxf(-hxW, -hyW, -dW);
						cw[1] = cxf(hxW, -hyW, -dW);
						cw[2] = cxf(hxW, hyW, -dW);
						cw[3] = cxf(-hxW, hyW, -dW);
						if (orthoW) {
							NkVec3f cb[4];
							cb[0] = cxf(-hxW, -hyW, 0.f);
							cb[1] = cxf(hxW, -hyW, 0.f);
							cb[2] = cxf(hxW, hyW, 0.f);
							cb[3] = cxf(-hxW, hyW, 0.f);
							for (int32 k3 = 0; k3 < 4; ++k3) {
								r3d->DrawDebugLine(cb[k3], cw[k3], ecol, 0.f, true);
								r3d->DrawDebugLine(cw[k3], cw[(k3 + 1) & 3], ecol, 0.f, true);
								r3d->DrawDebugLine(cb[k3], cb[(k3 + 1) & 3], ecol, 0.f, true);
							}
						} else {
							for (int32 k3 = 0; k3 < 4; ++k3) {
								r3d->DrawDebugLine(apex, cw[k3], ecol, 0.f, true);
								r3d->DrawDebugLine(cw[k3], cw[(k3 + 1) & 3], ecol, 0.f, true);
							}
						}
						// Triangle « haut », proportionnel au cadre.
						r3d->DrawDebugLine(cxf(-hxW * 0.35f, hyW * 1.15f, -dW),
										   cxf(hxW * 0.35f, hyW * 1.15f, -dW), ecol, 0.f, true);
						r3d->DrawDebugLine(cxf(hxW * 0.35f, hyW * 1.15f, -dW),
										   cxf(0.f, hyW * 1.75f, -dW), ecol, 0.f, true);
						r3d->DrawDebugLine(cxf(0.f, hyW * 1.75f, -dW),
										   cxf(-hxW * 0.35f, hyW * 1.15f, -dW), ecol, 0.f, true);
					} else {
						r3d->DrawDebugLine({ep.x - eh, ep.y, ep.z}, {ep.x + eh, ep.y, ep.z}, ecol,
										   0.f, true);
						r3d->DrawDebugLine({ep.x, ep.y - eh, ep.z}, {ep.x, ep.y + eh, ep.z}, ecol,
										   0.f, true);
						r3d->DrawDebugLine({ep.x, ep.y, ep.z - eh}, {ep.x, ep.y, ep.z + eh}, ecol,
										   0.f, true);
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
			//
			// PAS DANS UN RENDU, sauf demande explicite (Rihen) : c'est un repere
			// de travail, pas un element de la scene. Il est aussi masquable
			// dans la vue par la case « Curseur 3D » de l'onglet Affichage,
			// cochee par defaut.
			// Filtre, comme les autres aides : le rendu ne peut que RETIRER ce
			// que la vue montre, jamais ajouter ce qu'elle cache.
			if (nkvpCursorShow && (nkvpOutPhase == 0 || (nkvpOutAids & 512) != 0)) {
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
				// LES TROIS AXES SONT A ZERO -- et dessines via un DECALAGE LE LONG
				// DU RAYON DE VUE, la seule technique qui soit a la fois exacte a
				// l'ecran et robuste en profondeur.
				//
				// Historique des tentatives, pour ne pas y revenir :
				//   - y = 0.02 : les axes ne se croisaient plus a l'origine, et les
				//     ombres passaient SOUS eux (repere faux) ;
				//   - axes du shader de grille : dependants de la case « Grille » ;
				//   - biais de profondeur (-1.5 puis -64 unites) : INSUFFISANT --
				//     le sol est un quad de +-250 m, les axes des lignes de
				//     +-1000 m, et deux primitives d'etendues si differentes
				//     interpolent la meme profondeur avec des erreurs bien
				//     au-dessus du biais. Assez de biais pour gagner ferait saigner
				//     les lignes a travers les objets (constate : pointilles, puis
				//     disparition selon l'angle -- captures de Rihen).
				//
				// ICI : chaque extremite est tiree de 0.1 % VERS LA CAMERA. Tout
				// point du segment [P, camera] se projette au MEME pixel que P --
				// la position ecran des axes est donc EXACTE (ils se croisent a
				// l'origine, l'aimantation et le curseur 3D les voient a y = 0),
				// seule leur profondeur gagne ~2 cm a 20 m. Le decalage suit la
				// camera : il est recalcule chaque frame, quel que soit l'angle.
				const NkVec3f cp = cam.GetPosition();
				auto lift = [&](const NkVec3f &p2) {
					return p2 + (cp - p2) * 0.001f;
				};
				// UNE SEULE PASSE PAR AXE (decision de Rihen : l'epaississement par
				// lignes paralleles divergeait en eventail vers l'horizon).
				r3d->DrawDebugLine(lift({-A, 0.f, 0.f}), lift({A, 0.f, 0.f}),
								   {1.f, 0.f, 0.f, 1.f}); // X rouge
				r3d->DrawDebugLine(lift({0.f, -A, 0.f}), lift({0.f, A, 0.f}),
								   {0.f, 1.f, 0.f, 1.f}); // Y vert
				r3d->DrawDebugLine(lift({0.f, 0.f, -A}), lift({0.f, 0.f, A}),
								   {0.f, 0.f, 1.f, 1.f}); // Z bleu
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
					// COMBIEN D'OBJETS la passe d'ombre voit-elle vraiment ? Un atlas
					// plein de slots mais nourri par ZERO caster explique une ombre
					// qui ne correspond a rien. C'est le chiffre qui tranche.
					if (auto *r3 = ctx.renderer->GetRender3D())
						overlay->DrawText({px, 150.f}, " casters : %u", r3->GetShadowCasterCount());
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
				// SSAO ETEINTE PAR DEFAUT dans le modeleur (Rihen, 9 aout : le
				// depot sombre au pied des objets « ne donne rien de bon pour une
				// application ») — meme choix que l'autre vue (ForEditor : latence
				// et lisibilite) et que le viewport de Blender. Elle reste
				// DISPONIBLE : panneau Rendu > « Occlusion ambiante », qui passe
				// par SetPostConfig (le graphe se reconstruit a l'aplomb de la
				// frame suivante quand la passe apparait/disparait).
				cfg.postProcess.ssao = false;
				// BLOOM ETEINT PAR DEFAUT lui aussi (Rihen, 10 aout) : un
				// modeleur montre la matiere, pas un halo — il s'active au
				// panneau « Exposition et bloom » quand on compose une image.
				cfg.postProcess.bloom = false;
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
				// readback=true : le bouton « Capturer la vue » lit cette cible
				// telle quelle (NkOffscreenTarget::Capture exige le tampon de
				// relecture, cree a l'init). Cout : un staging de w*h*4 octets.
				od.readback = true;
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

		bool Demo3DHostCaptureView(const char *path) {
			// « Capturer la vue » : la vue 3D rend deja dans une cible hors
			// ecran lisible -- on fige la DERNIERE image rendue (scene seule,
			// sans interface) et NkOffscreenTarget::Capture la sauve en PNG.
			if (!hst.ok || !hst.rt || !hst.rt->IsValid() || !path || !*path)
				return false;
			bool ok = false;
			if (nkvpCamViewNode >= 0) {
				// EN VUE CAMERA : la capture RECADRE sur le cadre exact de la
				// camera (meme verite que le rendu et le voile) -- on obtient
				// ses VRAIS bords, sans le pourtour de la vue (Rihen).
				const uint32 w = hst.rt->GetWidth(), h = hst.rt->GetHeight();
				NkImage full;
				if (w > 8 && h > 8 && full.Create(w, h, math::NkColor(0, 0, 0, 255), 4) &&
					hst.rt->ReadbackPixels(full.Pixels(), w * 4u)) {
					float32 fr[4];
					Demo3DHostCameraFrame(fr);
					const uint32 cx = (uint32)(fr[0] * (float32)w);
					const uint32 cy = (uint32)(fr[1] * (float32)h);
					uint32 cw = (uint32)(fr[2] * (float32)w);
					uint32 ch = (uint32)(fr[3] * (float32)h);
					if (cx + cw > w)
						cw = w - cx;
					if (cy + ch > h)
						ch = h - cy;
					NkImage crop;
					if (cw > 8 && ch > 8 && crop.Create(cw, ch, math::NkColor(0, 0, 0, 255), 4)) {
						const uint32 *src = (const uint32 *)full.Pixels();
						uint32 *dst = (uint32 *)crop.Pixels();
						for (uint32 yy = 0; yy < ch; ++yy) {
							const uint32 *s = src + (uint64)(cy + yy) * w + cx;
							uint32 *d = dst + (uint64)yy * cw;
							for (uint32 xx = 0; xx < cw; ++xx)
								d[xx] = s[xx];
						}
						ok = crop.Save(path);
					}
				}
			} else {
				ok = hst.rt->Capture(path);
			}
			logger.Info("[NkDemo3D] Capture de la vue -> {0} : {1} (camView={2}, active={3})\n",
						path, ok ? "ecrite" : "ECHEC", nkvpCamViewNode, nkvpActiveCamNode);
			return ok;
		}

		// ════════════════════════════════════════════════════════════════════
		//  SORTIE : cible principale + incrustations
		// ════════════════════════════════════════════════════════════════════

		// Rapport de la camera = rapport de la SORTIE. Le pourcentage d'echelle
		// n'entre pas dans le calcul : il change la taille, jamais la forme.
		static float32 HostCamAspect() {
			const float32 w = (float32)(nkvpOutW > 0 ? nkvpOutW : 1920);
			const float32 h = (float32)(nkvpOutH > 0 ? nkvpOutH : 1080);
			const float32 a = w / h;
			// Garde-fou : un rapport degenere ferait un cadre plat ou infini,
			// et le champ de la vue camera divise par lui.
			return (a > 0.05f && a < 20.f) ? a : (1920.f / 1080.f);
		}

		// Definie plus bas, pres de la machine de rendu : les reglages en ont
		// besoin des la lecture (« camera active » doit se resoudre partout de
		// la meme facon).
		static int32 HostOutResolveSource(int32 source);

		// Resolution EFFECTIVE : la resolution demandee, mise a l'echelle du
		// pourcentage. Un seul calcul, pour que le panneau affiche exactement
		// ce que le rendu produira -- deux formules donneraient deux verites.
		static void HostOutSize(int32 *w, int32 *h) {
			const float32 k = (float32)nkvpOutScale * 0.01f;
			int32 ww = (int32)((float32)nkvpOutW * k + 0.5f);
			int32 hh = (int32)((float32)nkvpOutH * k + 0.5f);
			if (ww < 16)
				ww = 16;
			if (hh < 16)
				hh = 16;
			if (ww > 8192)
				ww = 8192;
			if (hh > 8192)
				hh = 8192;
			*w = ww;
			*h = hh;
		}

		void Demo3DHostOutMain(int32 *source, int32 *w, int32 *h, int32 *scalePct, int32 *format,
							   bool *transparent) {
			if (source)
				*source = nkvpOutSource;
			if (w)
				*w = nkvpOutW;
			if (h)
				*h = nkvpOutH;
			if (scalePct)
				*scalePct = nkvpOutScale;
			if (format)
				*format = nkvpOutFormat;
			if (transparent)
				*transparent = nkvpOutTransparent;
		}
		void Demo3DHostSetOutMain(int32 source, int32 w, int32 h, int32 scalePct, int32 format,
								  bool transparent) {
			// ── UNE VUE NE PEUT PAS ETRE PRINCIPALE ET MINIATURE (Rihen) ────
			// Si la source qu'on promeut alimente deja une incrustation, les
			// deux ECHANGENT : l'incrustation herite de l'ancienne principale.
			// Sans cet echange, la meme camera occupait le fond et la vignette
			// posee dessus -- une image qui se repete elle-meme en plus petit.
			// La comparaison porte sur les sources RESOLUES : « camera active »
			// et cette meme camera nommee sont la meme vue.
			if (source != nkvpOutSource) {
				const int32 nw = HostOutResolveSource(source);
				// L'ANCIENNE SOURCE, RESOLUE. Rendre « camera active » telle
				// quelle a l'incrustation ne reglait rien quand cette camera
				// active EST celle qu'on promeut : les deux pointaient encore
				// au meme endroit et la miniature affichait toujours la meme
				// vue (constate par Rihen). Si l'ancienne se resout sur la
				// nouvelle, l'incrustation retombe sur la vue 3D -- la seule
				// vue qui existe toujours.
				int32 old = HostOutResolveSource(nkvpOutSource);
				if (old == nw)
					old = -1;
				for (int32 i = 0; i < kNkvpMaxInsets; ++i) {
					if (!nkvpOutInset[i].used)
						continue;
					if (HostOutResolveSource(nkvpOutInset[i].source) != nw)
						continue;
					nkvpOutInset[i].source = old;
					logger.Info("[Output] Echange : l'incrustation {0} prend l'ancienne source "
								"principale ({1})\n",
								i + 1, old);
					break;
				}
			}
			nkvpOutSource = source;
			nkvpOutW = w < 16 ? 16 : (w > 8192 ? 8192 : w);
			nkvpOutH = h < 16 ? 16 : (h > 8192 ? 8192 : h);
			nkvpOutScale = scalePct < 1 ? 1 : (scalePct > 400 ? 400 : scalePct);
			nkvpOutFormat = format;
			nkvpOutTransparent = transparent;
		}
		void Demo3DHostOutEffectiveSize(int32 *w, int32 *h) {
			HostOutSize(w, h);
		}
		const char *Demo3DHostOutDir() {
			return nkvpOutDir;
		}
		void Demo3DHostSetOutDir(const char *d) {
			if (!d)
				return;
			uint32 i = 0;
			for (; d[i] && i + 1 < sizeof(nkvpOutDir); ++i)
				nkvpOutDir[i] = d[i];
			nkvpOutDir[i] = 0;
		}
		const char *Demo3DHostOutName() {
			return nkvpOutName;
		}
		void Demo3DHostSetOutName(const char *n) {
			if (!n)
				return;
			uint32 i = 0;
			for (; n[i] && i + 1 < sizeof(nkvpOutName); ++i)
				nkvpOutName[i] = n[i];
			nkvpOutName[i] = 0;
		}
		int32 Demo3DHostOutInsetMax() {
			return kNkvpMaxInsets;
		}
		// `size2` recoit LARGEUR puis HAUTEUR (fractions du cote correspondant).
		// Pour un carre ou un cercle, seule la premiere a un sens : la seconde
		// la recopie, pour qu'un appelant qui l'ignore ne lise jamais une valeur
		// contradictoire.
		bool Demo3DHostOutInset(int32 i, int32 *source, int32 *shape, float32 *xy2, float32 *size2,
								float32 *border, float32 *borderCol3, float32 *opacity) {
			if (i < 0 || i >= kNkvpMaxInsets || !nkvpOutInset[i].used)
				return false;
			const NkVpOutInset &o = nkvpOutInset[i];
			if (source)
				*source = o.source;
			if (shape)
				*shape = o.shape;
			if (xy2) {
				xy2[0] = o.x;
				xy2[1] = o.y;
			}
			if (size2) {
				size2[0] = o.size;
				size2[1] = (nk3d::NkInsetDimCount(o.shape) == 1) ? o.size : o.sizeH;
			}
			if (border)
				*border = o.border;
			if (borderCol3)
				for (int32 a = 0; a < 3; ++a)
					borderCol3[a] = o.borderCol[a];
			if (opacity)
				*opacity = o.opacity;
			return true;
		}
		bool Demo3DHostNodeNoRender(int32 node) {
			return node >= 0 && node < kNkvpMaxNodes && nkvpNoRender[node];
		}
		bool Demo3DHostNodeNoRenderEff(int32 node) {
			return node >= 0 && node < kNkvpMaxNodes && HostNoRenderEff(node);
		}
		void Demo3DHostSetNodeNoRender(int32 node, bool on) {
			if (node >= 0 && node < kNkvpMaxNodes)
				nkvpNoRender[node] = on;
		}
		void Demo3DHostSetNodeLabel(int32 node, const char *name) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser || !name)
				return;
			uint32 i = 0;
			for (; name[i] && i + 1 < sizeof(nkvpNodeLabel[0]); ++i)
				nkvpNodeLabel[u][i] = name[i];
			nkvpNodeLabel[u][i] = 0;
		}
		bool Demo3DHostOutInsetOwnFile(int32 i) {
			return i >= 0 && i < kNkvpMaxInsets && nkvpOutInset[i].used &&
				   nkvpOutInset[i].ownFile;
		}
		void Demo3DHostSetOutInsetOwnFile(int32 i, bool on) {
			if (i >= 0 && i < kNkvpMaxInsets && nkvpOutInset[i].used)
				nkvpOutInset[i].ownFile = on;
		}
		bool Demo3DHostOutInsetOwnShaped(int32 i) {
			return i >= 0 && i < kNkvpMaxInsets && nkvpOutInset[i].used &&
				   nkvpOutInset[i].ownShaped;
		}
		void Demo3DHostSetOutInsetOwnShaped(int32 i, bool on) {
			if (i >= 0 && i < kNkvpMaxInsets && nkvpOutInset[i].used)
				nkvpOutInset[i].ownShaped = on;
		}
		void Demo3DHostSetOutInset(int32 i, int32 source, int32 shape, const float32 *xy2,
								   const float32 *size2, float32 border, const float32 *borderCol3,
								   float32 opacity) {
			if (i < 0 || i >= kNkvpMaxInsets || !nkvpOutInset[i].used)
				return;
			NkVpOutInset &o = nkvpOutInset[i];
			// MEME REGLE DANS L'AUTRE SENS : donner a une miniature la source
			// de la principale les fait echanger, plutot que de laisser la meme
			// vue occuper le fond et la vignette.
			if (source != o.source) {
				const int32 nw = HostOutResolveSource(source);
				if (HostOutResolveSource(nkvpOutSource) == nw) {
					nkvpOutSource = o.source;
					logger.Info("[Output] Echange : la principale prend la source de "
								"l'incrustation {0} ({1})\n",
								i + 1, o.source);
				} else {
					// Ni deux miniatures sur la meme vue : la precedente
					// recupere celle qu'on libere.
					for (int32 j = 0; j < kNkvpMaxInsets; ++j) {
						if (j == i || !nkvpOutInset[j].used)
							continue;
						if (HostOutResolveSource(nkvpOutInset[j].source) != nw)
							continue;
						nkvpOutInset[j].source = o.source;
						break;
					}
				}
			}
			o.source = source;
			o.shape = shape < 0 ? 0 : (shape >= kNkvpInsetShapes ? kNkvpInsetShapes - 1 : shape);
			if (xy2) {
				o.x = xy2[0];
				o.y = xy2[1];
			}
			if (size2) {
				const float32 sw = size2[0], sh = size2[1];
				o.size = sw < 0.02f ? 0.02f : (sw > 1.f ? 1.f : sw);
				o.sizeH = sh < 0.02f ? 0.02f : (sh > 1.f ? 1.f : sh);
			}
			o.border = border < 0.f ? 0.f : (border > 64.f ? 64.f : border);
			if (borderCol3)
				for (int32 a = 0; a < 3; ++a)
					o.borderCol[a] = borderCol3[a];
			o.opacity = opacity < 0.f ? 0.f : (opacity > 1.f ? 1.f : opacity);
		}
		int32 Demo3DHostOutInsetAdd() {
			for (int32 i = 0; i < kNkvpMaxInsets; ++i)
				if (!nkvpOutInset[i].used) {
					nkvpOutInset[i] = NkVpOutInset{};
					nkvpOutInset[i].used = true;
					// Une NOUVELLE incrustation ne se pose pas sur la
					// precedente : chacune descend d'une hauteur, sinon deux
					// ajouts de suite paraissent n'en avoir fait qu'un.
					nkvpOutInset[i].y = 0.04f + 0.30f * (float32)(i % 3);
					// SA SOURCE NE PEUT PAS ETRE CELLE DE LA PRINCIPALE : une
					// vignette qui repete le fond n'apprend rien, et la regle
					// « une vue n'est pas a la fois principale et miniature »
					// vaut aussi a la naissance -- pas seulement lors d'un
					// echange (constate par Rihen : principale et miniature
					// toutes deux sur la vue 3D). On prend la premiere source
					// libre : une camera non encore employee, sinon la vue 3D.
					const int32 mainNode = nkvpOutSource;
					int32 cams[16];
					const int32 nc = HostSceneCameras(cams, 16);
					int32 pick = (mainNode == -1) ? -1 : -1; // la vue 3D par defaut
					for (int32 c = 0; c < nc; ++c) {
						if (cams[c] == mainNode)
							continue;
						bool taken = false;
						for (int32 j = 0; j < kNkvpMaxInsets && !taken; ++j)
							if (j != i && nkvpOutInset[j].used &&
								nkvpOutInset[j].source == cams[c])
								taken = true;
						if (!taken) {
							pick = cams[c];
							break;
						}
					}
					nkvpOutInset[i].source = (pick == mainNode) ? -1 : pick;
					return i;
				}
			return -1;
		}
		void Demo3DHostOutInsetDelete(int32 i) {
			if (i >= 0 && i < kNkvpMaxInsets)
				nkvpOutInset[i].used = false;
		}
		const char *Demo3DHostOutInsetShapeName(int32 s) {
			return nk3d::NkInsetShapeName(s);
		}
		int32 Demo3DHostOutInsetShapeCount() {
			return kNkvpInsetShapes;
		}
		// ── LE RENDU DE SORTIE, ETALE SUR PLUSIEURS IMAGES ──────────────────
		// Redimensionner la cible et lire ses pixels ne peuvent pas avoir lieu
		// dans la meme image : il faut que le GPU ait rendu ENTRE les deux. La
		// sortie se deroule donc en etapes -- la principale, puis chaque
		// incrustation -- et chaque etape prend trois images : poser, laisser
		// rendre, lire. Neuf cibles font vingt-sept images, moins d'une demi-
		// seconde, et rien ne bloque l'interface pendant ce temps.
		static NkImage nkvpOutCanvas;
		static int32 nkvpOutWait = 0;
		// FOND TRANSPARENT : chaque cible est rendue DEUX fois, sur fond noir
		// puis sur fond blanc, et l'alpha se deduit de l'ecart (voir
		// NkAlphaFromTwoBackgrounds). `nkvpOutDark` garde la premiere en
		// attendant la seconde.
		static NkImage nkvpOutDark;
		static int32 nkvpOutPass = 0; // 0 = fond noir, 1 = fond blanc

		// ── UNE CIBLE DEDIEE AU RENDU DE SORTIE ─────────────────────────────
		// La sortie ecrivait dans la cible de la VUE en la redimensionnant :
		// l'image a l'ecran se deformait le temps du rendu puis reprenait sa
		// forme (constate par Rihen), et toute erreur de taille se payait
		// directement sur ce que l'utilisateur regarde. Une cible propre, creee
		// a la demande et detruite ensuite, supprime les deux : la vue garde sa
		// taille et continue d'afficher sa derniere image pendant que la sortie
		// travaille a cote.
		static NkOffscreenTarget *nkvpOutRT = nullptr;
		static uint32 nkvpOutRTW = 0, nkvpOutRTH = 0;

		static bool HostOutBindTarget(uint32 w, uint32 h) {
			if (!hst.ok || !hst.ctx.renderer)
				return false;
			if (!nkvpOutRT || nkvpOutRTW != w || nkvpOutRTH != h) {
				if (nkvpOutRT) {
					hst.ctx.renderer->DestroyOffscreen(nkvpOutRT);
					nkvpOutRT = nullptr;
				}
				NkOffscreenDesc od;
				od.width = w;
				od.height = h;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				od.readback = true; // la sortie relit ses pixels
				od.name = "NK3DModelerOutput";
				nkvpOutRT = hst.ctx.renderer->CreateOffscreen(od);
				if (!nkvpOutRT || !nkvpOutRT->IsValid()) {
					if (nkvpOutRT) {
						hst.ctx.renderer->DestroyOffscreen(nkvpOutRT);
						nkvpOutRT = nullptr;
					}
					nkvpOutRTW = nkvpOutRTH = 0;
					return false;
				}
				nkvpOutRTW = w;
				nkvpOutRTH = h;
			}
			// Le graphe rend DANS cette cible, a cette taille. La cible de la
			// vue n'est pas touchee : ni sa taille, ni son contenu.
			hst.ctx.width = w;
			hst.ctx.height = h;
			nkvpW = (float32)w;
			nkvpH = (float32)h;
			hst.ctx.renderer->SetRenderSizeOverride(w, h);
			if (auto *texLib = hst.ctx.renderer->GetTextures())
				hst.ctx.renderer->SetFinalColorTarget(
					texLib->GetRHIHandle(nkvpOutRT->GetColorResult()));
			return true;
		}

		// Rend la main a la vue : sa cible, sa taille, son contenu intact.
		static void HostOutUnbindTarget(uint32 w, uint32 h) {
			if (!hst.ok || !hst.ctx.renderer || !hst.rt)
				return;
			hst.ctx.width = w;
			hst.ctx.height = h;
			nkvpW = (float32)w;
			nkvpH = (float32)h;
			hst.ctx.renderer->SetRenderSizeOverride(w, h);
			if (auto *texLib = hst.ctx.renderer->GetTextures())
				hst.ctx.renderer->SetFinalColorTarget(
					texLib->GetRHIHandle(hst.rt->GetColorResult()));
			if (nkvpOutRT) {
				hst.ctx.renderer->DestroyOffscreen(nkvpOutRT);
				nkvpOutRT = nullptr;
				nkvpOutRTW = nkvpOutRTH = 0;
			}
		}

		// Formes et composition vivent dans NkOutCompose.h : c'est du calcul
		// PUR, donc verifiable hors de l'application (planche des six formes
		// generee par un test isole). Ici on ne fait que les brancher.
		static void HostInsetPixels(const NkVpOutInset &o, int32 mainW, int32 mainH, int32 *w,
									int32 *h) {
			nk3d::NkInsetPixels(o.shape, o.size, o.sizeH, mainW, mainH, w, h);
		}

		static void HostOutCompose(const NkVpOutInset &o, const NkImage &src, int32 dstX,
								   int32 dstY) {
			if (!nkvpOutCanvas.Pixels() || !src.Pixels())
				return;
			nk3d::NkInsetStyle st;
			st.shape = o.shape;
			st.border = o.border;
			st.opacity = o.opacity;
			for (int32 c = 0; c < 3; ++c)
				st.borderCol[c] = o.borderCol[c];
			nk3d::NkInsetCompose((uint8 *)nkvpOutCanvas.Pixels(), (int32)nkvpOutCanvas.Width(),
								 (int32)nkvpOutCanvas.Height(), (const uint8 *)src.Pixels(),
								 (int32)src.Width(), (int32)src.Height(), dstX, dstY, st);
		}

		// Prochain chemin libre de la sortie. Meme convention numerotee que les
		// captures, mais dans le dossier, sous le nom et au FORMAT choisis au
		// panneau. `tag` suffixe le mode de rendu quand on en produit
		// plusieurs (« rendu_001_filaire.png ») : sans lui, les images se
		// remplaceraient l'une l'autre sans qu'on sache laquelle est laquelle.
		// ── UN SEUL NUMERO POUR TOUTE LA PRISE ──────────────────────────────
		// Chaque motif cherchait AUTREFOIS son propre premier numero libre : la
		// principale sortait en « rendu_041 » et ses miniatures en
		// « rendu_001_Camera_002 », si bien qu'on ne pouvait plus les rapprocher
		// (constate par Rihen -- les fichiers etaient pourtant bien ecrits).
		// Le numero est desormais celui de la PRISE : calcule une fois au
		// depart, il coiffe la principale, chaque type de rendu et chaque
		// miniature. C'est ce qui fait d'eux une serie.
		static int32 nkvpOutTake = 0;

		// `asDir` : la prise produit un DOSSIER (suite d'images) et non un
		// fichier. Sans cette distinction, on cherchait un nom de FICHIER libre
		// alors que la prise cree un dossier : le numero retombait toujours sur
		// le meme et chaque enregistrement reecrivait dans « rendu_060 »
		// (constate par Rihen). Un fichier et un dossier de meme nom ne se
		// voient pas l'un l'autre -- il faut chercher ce qu'on va reellement
		// creer.
		// UN NUMERO = UNE PRISE, QUELLE QUE SOIT SA NATURE. On cherche le
		// premier rang ou RIEN n'existe : ni image, ni video, ni dossier de
		// suite d'images. Chercher une seule forme donnait deux defauts (tous
		// deux constates par Rihen) : la video reprenait le rang d'une image
		// libre et ECRASAIT le fichier .avi qui portait deja ce rang ; et les
		// dossiers repartaient a 001 en parallele des images, si bien que
		// « rendu_001/ » cotoyait « rendu_060.png » sans aucun rapport entre
		// eux. Une prise porte desormais son rang sur toutes ses sorties.
		static void HostOutBeginTake(bool /*asDir*/ = false, const char *nameOverride = nullptr) {
			const char *dir = nkvpOutDir[0] ? nkvpOutDir : "captures";
			NkDirectory::CreateRecursive(dir);
			// Le nom peut etre impose : un enregistrement porte celui de SA
			// source (vue ou tutoriel), et son rang doit etre cherche sous ce
			// nom-la, pas sous celui de la sortie.
			const char *nm =
				nameOverride ? nameOverride : (nkvpOutName[0] ? nkvpOutName : "rendu");
			// Toutes les extensions qu'une prise peut produire : celles des
			// images ET celles des videos. En oublier une, c'est risquer
			// d'ecraser ce qu'elle avait produit.
			// Le compte se DEDUIT du tableau : fige a 3, il avait laisse « mp4 »
			// hors de la recherche le jour ou le format est arrive -- et deux
			// prises ont porte le rang 006 (tutoriel_006.mp4 et .avi). Une liste
			// et son compte doivent tenir dans la meme expression.
			static const char *const kVidExt[] = {"avi", "mov", "m1v", "mp4"};
			static const int32 kVidExtCount = (int32)(sizeof(kVidExt) / sizeof(kVidExt[0]));
			char probe[300];
			for (int32 i = 1; i < 1000; ++i) {
				bool taken = false;
				// le dossier de suite d'images
				snprintf(probe, sizeof(probe), "%s/%s_%03d", dir, nm, (int)i);
				taken = NkDirectory::Exists(probe);
				// ... et le dossier QOI conserve d'une prise video : si
				// l'utilisateur a garde les images mais efface la video, le
				// rang doit rester occupe -- sinon la prise suivante ecrirait
				// SES images par-dessus les siennes.
				if (!taken) {
					snprintf(probe, sizeof(probe), "%s/%s_qoi_%03d", dir, nm, (int)i);
					taken = NkDirectory::Exists(probe);
				}
				// toutes les images
				for (int32 f = 0; f < kNkvpOutFmtCount && !taken; ++f) {
					snprintf(probe, sizeof(probe), "%s/%s_%03d.%s", dir, nm, (int)i,
							 kNkvpOutFmt[f].ext);
					taken = NkFile::Exists(probe);
				}
				// toutes les videos
				for (int32 v = 0; v < kVidExtCount && !taken; ++v) {
					snprintf(probe, sizeof(probe), "%s/%s_%03d.%s", dir, nm, (int)i, kVidExt[v]);
					taken = NkFile::Exists(probe);
				}
				if (!taken) {
					nkvpOutTake = i;
					return;
				}
			}
			nkvpOutTake = 999;
		}

		static bool HostOutNextPath(char *out, int32 cap, const char *tag) {
			const char *dir = nkvpOutDir[0] ? nkvpOutDir : "captures";
			NkDirectory::CreateRecursive(dir);
			const char *nm = nkvpOutName[0] ? nkvpOutName : "rendu";
			const int32 f = (nkvpOutFormat >= 0 && nkvpOutFormat < kNkvpOutFmtCount)
								? nkvpOutFormat
								: 0;
			const char *ext = kNkvpOutFmt[f].ext;
			const int32 n = nkvpOutTake > 0 ? nkvpOutTake : 1;
			if (tag && tag[0])
				snprintf(out, (size_t)cap, "%s/%s_%03d_%s.%s", dir, nm, (int)n, tag, ext);
			else
				snprintf(out, (size_t)cap, "%s/%s_%03d.%s", dir, nm, (int)n, ext);
			return true;
		}
		// LES CAPTURES PARTAGENT LA DESTINATION DE LA SORTIE (Rihen : « le nom
		// mis la n'est pas celui qui sort, c'est toujours vue »). Il n'y a
		// qu'un dossier configure dans l'application : deux conventions
		// concurrentes ne pouvaient que surprendre. Chacune garde en revanche
		// SON nom de base -- un rendu final, une capture de la scene et une
		// photo de l'interface ne se rangent pas sous le meme nom.
		// `which` : 0 = la sortie, 1 = capture de la vue, 2 = tutoriel.
		// `newTake` : ouvrir une NOUVELLE prise (numero suivant) plutot que de
		// reprendre celle en cours. Le rendu garde son numero pour toute la
		// serie ; « Tutoriel », lui, est un acte isole -- sans cela il ecraserait
		// le dernier fichier produit.
		static bool HostOutNamedPath(char *out, int32 cap, const char *tag, int32 which,
									 bool newTake = false) {
			const char *save = nkvpOutName;
			char keep[64];
			if (which != 0) {
				// On emprunte la mecanique de nommage de la sortie en lui
				// substituant le temps d'un appel le nom de la capture : un
				// seul endroit compose les chemins, donc un seul comportement.
				uint32 i = 0;
				for (; save[i] && i + 1 < sizeof(keep); ++i)
					keep[i] = save[i];
				keep[i] = 0;
				const char *src = (which == 2) ? nkvpCapTutoName : nkvpCapViewName;
				uint32 j = 0;
				for (; src[j] && j + 1 < sizeof(nkvpOutName); ++j)
					nkvpOutName[j] = src[j];
				nkvpOutName[j] = 0;
			}
			if (newTake)
				HostOutBeginTake(); // apres la substitution : le numero suit CE nom
			const bool ok = HostOutNextPath(out, cap, tag);
			if (which != 0) {
				uint32 i = 0;
				for (; keep[i] && i + 1 < sizeof(nkvpOutName); ++i)
					nkvpOutName[i] = keep[i];
				nkvpOutName[i] = 0;
			}
			return ok;
		}
		bool Demo3DHostOutNextPath(char *out, int32 cap, int32 which) {
			// Usage externe (« Tutoriel ») : acte isole, donc nouvelle prise.
			return HostOutNamedPath(out, cap, nullptr, which, true);
		}
		const char *Demo3DHostCaptureName(int32 which) {
			return (which == 2) ? nkvpCapTutoName : nkvpCapViewName;
		}
		void Demo3DHostSetCaptureName(int32 which, const char *n) {
			if (!n)
				return;
			char *dst = (which == 2) ? nkvpCapTutoName : nkvpCapViewName;
			uint32 i = 0;
			for (; n[i] && i + 1 < 64; ++i)
				dst[i] = n[i];
			dst[i] = 0;
		}

		// Les sources sont deja des vues precises depuis le retrait de « camera
		// active » : cette fonction ne fait plus que garder le point de passage
		// unique par lequel toutes les comparaisons de source passent.
		static int32 HostOutResolveSource(int32 source) {
			return source;
		}

		static bool HostShowLightGizmos() {
			auto *st = hst.ok ? (Demo3DState *)hst.ctx.userData : nullptr;
			return st ? st->showLightGizmos : true;
		}
		static void HostSetShowLightGizmos(bool on) {
			if (auto *st = hst.ok ? (Demo3DState *)hst.ctx.userData : nullptr)
				st->showLightGizmos = on;
		}

		// NOM DE SOURCE UTILISABLE DANS UN FICHIER. Un nom de scene peut porter
		// des espaces, des accents, des points -- rien de tout cela n'a sa place
		// dans un nom de fichier, et le point casserait meme l'extension. On ne
		// garde que lettres, chiffres et tirets ; le reste devient un tiret bas.
		static void HostOutSafeName(int32 node, char *out, uint32 cap) {
			if (!out || cap == 0)
				return;
			out[0] = 0;
			if (node < 0) {
				snprintf(out, cap, "vue3d");
				return;
			}
			// LE VRAI NOM D'ABORD, celui que l'interface a depose. A defaut --
			// noeud jamais vu par un panneau -- on retombe sur le RANG de la
			// camera, qui est stable et ne pretend pas dire un nom qu'on n'a
			// pas. (Passer un identifiant de noeud a la fonction qui nomme les
			// OBJETS donnerait un nom pris au hasard : c'est ce qui avait fait
			// s'annoncer une camera « mur gi ».)
			char raw[48] = {};
			{
				const int32 u = node - kNkvpFirstUser;
				if (u >= 0 && u < kNkvpMaxUser && nkvpNodeLabel[u][0]) {
					snprintf(raw, sizeof(raw), "%s", nkvpNodeLabel[u]);
				} else {
					int32 cams[16];
					const int32 nc = HostSceneCameras(cams, 16);
					int32 rank = -1;
					for (int32 c = 0; c < nc; ++c)
						if (cams[c] == node) {
							rank = c + 1;
							break;
						}
					if (rank > 0)
						snprintf(raw, sizeof(raw), "cam%d", (int)rank);
					else
						snprintf(raw, sizeof(raw), "noeud%d", (int)node);
				}
			}
			uint32 j = 0;
			for (uint32 k = 0; raw[k] && j + 1 < cap; ++k) {
				const char c = raw[k];
				const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
								(c >= '0' && c <= '9') || c == '-';
				out[j++] = ok ? c : '_';
			}
			out[j] = 0;
			if (!out[0])
				snprintf(out, cap, "camera");
		}

		// Pose la source d'une etape : -1 = la vue 3D telle qu'elle est,
		// sinon la camera demandee. Le cadre camera etant force en plein cadre
		// pendant la sortie, son image occupe toute la cible.
		static void HostOutSetSource(int32 source) {
			const int32 node = HostOutResolveSource(source);
			Demo3DHostSetCameraView(node < 0 ? -1 : node);
		}

		// Indice de l'incrustation suivante a rendre, apres `from` (-1 pour
		// commencer). Rend -1 quand il n'en reste plus.
		static int32 HostOutNextInset(int32 from) {
			for (int32 i = from + 1; i < kNkvpMaxInsets; ++i)
				if (nkvpOutInset[i].used)
					return i;
			return -1;
		}

		// Un tour de machine, appele une fois par image APRES le rendu.
		static void HostOutTick() {
			if (nkvpOutPhase == 0)
				return;
			int32 mw = 0, mh = 0;
			HostOutSize(&mw, &mh);
			if (nkvpOutPhase == 1) {
				// POSER : mode de rendu, taille de la cible et source de
				// l'etape courante.
				if (nkvpOutModeIdx < nkvpOutModeCount) {
					const int32 m = nkvpOutModeQueue[nkvpOutModeIdx];
					if (m >= 0 && m < kNkvpOutModeCount && Demo3DHostShading() != m)
						Demo3DHostSetShading(m);
				}
				int32 tw = mw, th = mh;
				if (nkvpOutStep >= 0)
					HostInsetPixels(nkvpOutInset[nkvpOutStep], mw, mh, &tw, &th);
				const int32 src =
					(nkvpOutStep < 0) ? nkvpOutSource : nkvpOutInset[nkvpOutStep].source;
				HostOutSetSource(src);
				// FOND TRANSPARENT : noir a la premiere passe, blanc a la
				// seconde. C'est l'ecart entre les deux qui donnera l'alpha.
				if (nkvpOutTransparent && hst.ok) {
					// PREMIERE PASSE : fond noir ET ALPHA ZERO. L'alpha du fond
					// n'est pas un detail ici -- c'est lui qui permet de savoir
					// si la chaine de rendu transmet la couverture. En effacant
					// a alpha 1, ma propre double passe empechait la detection
					// de voir l'alpha meme quand les shaders le propageaient
					// correctement : elle rendait toujours deux fois.
					// SECONDE PASSE : fond blanc OPAQUE, comme l'exige la
					// reconstruction par difference.
					if (nkvpOutPass == 0)
						hst.ctx.renderer->SetBackgroundColor({0.f, 0.f, 0.f, 0.f});
					else
						hst.ctx.renderer->SetBackgroundColor({1.f, 1.f, 1.f, 1.f});
				}
				if (!HostOutBindTarget((uint32)tw, (uint32)th)) {
					logger.Error("[Output] Cible de sortie refusee ({0}x{1}) -- rendu "
								 "abandonne\n",
								 tw, th);
					nkvpOutPhase = 4; // restauration
					return;
				}
				nkvpOutWait = 2; // deux images pleines avant de lire
				nkvpOutPhase = 2;
				return;
			}
			if (nkvpOutPhase == 2) {
				if (--nkvpOutWait > 0)
					return;
				nkvpOutPhase = 3;
				return;
			}
			if (nkvpOutPhase == 3) {
				// LIRE : la cible DEDIEE porte l'image de cette etape.
				if (!nkvpOutRT) {
					nkvpOutPhase = 4;
					return;
				}
				const uint32 w = nkvpOutRT->GetWidth(), h = nkvpOutRT->GetHeight();
				NkImage img;
				if (w >= 8 && h >= 8 && img.Create(w, h, math::NkColor(0, 0, 0, 255), 4) &&
					nkvpOutRT->ReadbackPixels(img.Pixels(), w * 4u)) {
					// ── DEUX FONDS POUR UN ALPHA ────────────────────────────
					// Premiere passe : on met de cote l'image sur fond noir et
					// on refait LA MEME etape sur fond blanc. Seconde passe :
					// l'ecart entre les deux donne l'alpha exact, bords
					// antialiases compris.
					if (nkvpOutTransparent) {
						if (nkvpOutPass == 0) {
							// ── LE MOTEUR SAIT-IL DEJA PROPAGER L'ALPHA ? ───
							// On ne le SUPPOSE pas, on le CONSTATE : si l'image
							// rendue sur fond noir porte deja des pixels
							// transparents, la chaine de post-traitement a
							// transmis la couverture et la seconde passe n'a
							// plus lieu d'etre. Le jour ou les cinq backends
							// seront corriges, la sortie cessera d'elle-meme de
							// rendre deux fois -- sans qu'on ait a s'en
							// souvenir. Et sur un backend encore fautif, le
							// repli reste en place.
							const uint8 *px = (const uint8 *)img.Pixels();
							const int64 n = (int64)img.Width() * (int64)img.Height();
							bool anyAlpha = false;
							for (int64 q = 0; q < n && !anyAlpha; q += 97)
								if (px[q * 4 + 3] < 250)
									anyAlpha = true;
							if (anyAlpha) {
								static bool sSaid = false;
								if (!sSaid) {
									logger.Info("[Output] Alpha transmis par le rendu : une "
												"seule passe suffit\n");
									sSaid = true;
								}
								nkvpOutPass = 0; // rien a reconstruire
							} else {
								nkvpOutDark = static_cast<NkImage &&>(img);
								nkvpOutPass = 1;
								nkvpOutPhase = 1; // meme cible, autre fond
								return;
							}
						} else {
							// Seconde passe : l'alpha se reconstruit par l'ecart
							// entre les deux fonds (chaine de post-traitement
							// qui ne transmet pas la couverture).
							if (nkvpOutDark.Pixels() && nkvpOutDark.Width() == img.Width() &&
								nkvpOutDark.Height() == img.Height())
								nk3d::NkAlphaFromTwoBackgrounds(
									(const uint8 *)nkvpOutDark.Pixels(), (uint8 *)img.Pixels(),
									img.Width(), img.Height());
							nkvpOutDark.Unload();
							nkvpOutPass = 0;
						}
					}
					if (nkvpOutStep < 0) {
						// La principale DEVIENT le canevas. NkImage est
						// move-only (pas de double liberation possible) : on
						// transfere, on ne copie pas.
						nkvpOutCanvas = static_cast<NkImage &&>(img);
					} else {
						const NkVpOutInset &o = nkvpOutInset[nkvpOutStep];
						// SON PROPRE FICHIER, AVANT la composition : elle sort
						// alors TELLE QUELLE -- rectangulaire, sans masque de
						// forme ni lisere. Une vue de dessus livree en rond,
						// avec un contour blanc, ne servirait a rien ; la forme
						// appartient a la composition, pas a l'image.
						// Nom : <nom>_<NNN>_<camera>[_<mode>] -- le nom
						// regroupe la serie, le numero la trie, les qualificatifs
						// viennent en queue pour ne pas casser ce tri (Rihen).
						if (o.ownFile) {
							// FORME GARDEE, si demande : on compose l'image sur
							// un fond TRANSPARENT du meme cadre, ce qui donne la
							// vignette telle qu'elle apparaitra -- masque et
							// lisere compris -- sans le reste de la scene.
							NkImage shaped;
							const NkImage *toSave = &img;
							if (o.ownShaped && shaped.Create((uint32)img.Width(),
															 (uint32)img.Height(),
															 math::NkColor(0, 0, 0, 0), 4)) {
								nk3d::NkInsetStyle sty;
								sty.shape = o.shape;
								sty.border = o.border;
								sty.opacity = o.opacity;
								for (int32 c = 0; c < 3; ++c)
									sty.borderCol[c] = o.borderCol[c];
								nk3d::NkInsetCompose((uint8 *)shaped.Pixels(), shaped.Width(),
													 shaped.Height(),
													 (const uint8 *)img.Pixels(), img.Width(),
													 img.Height(), 0, 0, sty);
								toSave = &shaped;
							}
							char camTag[64] = {};
							HostOutSafeName(o.source, camTag, sizeof(camTag));
							char full[96];
							if (nkvpOutModeCount > 1 && nkvpOutModeIdx < nkvpOutModeCount) {
								const int32 m = nkvpOutModeQueue[nkvpOutModeIdx];
								snprintf(full, sizeof(full), "%s_%s", camTag,
										 (m >= 0 && m < kNkvpOutModeCount) ? kNkvpOutModeTags[m]
																		   : "");
							} else {
								snprintf(full, sizeof(full), "%s", camTag);
							}
							char pth[300];
							if (HostOutNamedPath(pth, (int32)sizeof(pth), full, nkvpOutNaming)) {
								const int32 f2 =
									(nkvpOutFormat >= 0 && nkvpOutFormat < kNkvpOutFmtCount)
										? nkvpOutFormat
										: 0;
								const bool okI = kNkvpOutFmt[f2].lossy
													 ? toSave->Save(pth, nkvpOutQuality)
													 : toSave->Save(pth);
								logger.Info("[Output] Incrustation {0} -> {1} : {2}\n",
											nkvpOutStep + 1, pth, okI ? "ecrite" : "ECHEC");
							}
						}
						const int32 dx = (int32)(o.x * (float32)mw + 0.5f);
						const int32 dy = (int32)(o.y * (float32)mh + 0.5f);
						HostOutCompose(o, img, dx, dy);
					}
				} else {
					logger.Error("[Output] Lecture des pixels impossible a l'etape {0}\n",
								 nkvpOutStep);
				}
				const int32 next = HostOutNextInset(nkvpOutStep);
				if (next >= 0) {
					nkvpOutStep = next;
					nkvpOutPhase = 1;
					return;
				}
				// ECRIRE CE MODE. Le suffixe n'apparait que si plusieurs modes
				// sont demandes : une image seule n'a pas besoin qu'on lui
				// rappelle de quoi elle est faite.
				nkvpOutLastOk = false;
				const char *tag = nullptr;
				if (nkvpOutModeCount > 1 && nkvpOutModeIdx < nkvpOutModeCount) {
					const int32 m = nkvpOutModeQueue[nkvpOutModeIdx];
					if (m >= 0 && m < kNkvpOutModeCount)
						tag = kNkvpOutModeTags[m];
				}
				if (nkvpOutCanvas.Pixels() &&
					HostOutNamedPath(nkvpOutLastPath, (int32)sizeof(nkvpOutLastPath), tag,
									 nkvpOutNaming)) {
					// La QUALITE ne concerne que le JPEG ; Save l'ignore pour
					// les autres, mais autant ne la passer que la ou elle a un
					// sens.
					const int32 f = (nkvpOutFormat >= 0 && nkvpOutFormat < kNkvpOutFmtCount)
										? nkvpOutFormat
										: 0;
					nkvpOutLastOk = kNkvpOutFmt[f].lossy
										? nkvpOutCanvas.Save(nkvpOutLastPath, nkvpOutQuality)
										: nkvpOutCanvas.Save(nkvpOutLastPath);
				}
				logger.Info("[Output] Rendu {0}x{1} -> {2} : {3}\n", mw, mh, nkvpOutLastPath,
							nkvpOutLastOk ? "ecrit" : "ECHEC");
				// MODE SUIVANT s'il en reste : chaque mode coche produit SON
				// image, incrustations comprises.
				if (++nkvpOutModeIdx < nkvpOutModeCount) {
					nkvpOutCanvas.Unload(); // vide l'image -- voir la note plus bas
					nkvpOutStep = -1;
					nkvpOutPhase = 1;
					return;
				}
				nkvpOutPhase = 4;
				return;
			}
			// PHASE 4 : restaurer la vue telle qu'elle etait. La taille demandee
			// entre-temps par l'editeur a ete memorisee par Demo3DHostResize.
			// TRACEE PAS A PAS : c'est ici que l'application se fermait, et une
			// fermeture nette ne laisse rien d'autre a lire.
			logger.Info("[Output] Restauration : source={0} taille={1}x{2} mode={3}\n",
						nkvpOutSaveCam, nkvpOutSaveW, nkvpOutSaveH, nkvpOutSaveShading);
			if (Demo3DHostShading() != nkvpOutSaveShading)
				Demo3DHostSetShading(nkvpOutSaveShading);
			// L'habillage revient exactement comme il etait.
			nkvpGizmoHidden = nkvpOutSaveGizmoHidden;
			nkvpGridOn = nkvpOutSaveGrid;
			nkvpMinorOn = nkvpOutSaveMinor;
			nkvpMajorOn = nkvpOutSaveMajor;
			nkvpAxesOn = nkvpOutSaveAxes;
			nkvpHudOn = nkvpOutSaveHud;
			HostSetShowLightGizmos(nkvpOutSaveLightGiz);
			// Ciel et couleur de fond reviennent tels quels.
			if (Demo3DHostSkyVisible() != nkvpOutSaveSky)
				Demo3DHostSetSkyVisible(nkvpOutSaveSky);
			nkvpFloorOn = nkvpOutSaveFloor;
			if (hst.ok)
				hst.ctx.renderer->SetBackgroundColor(
					{nkvpBgColor[0], nkvpBgColor[1], nkvpBgColor[2], 1.f});
			HostOutSetSource(nkvpOutSaveCam);
			logger.Info("[Output]   source restituee\n");
			// UNLOAD, PAS FREE. NkImage::Free() ne vide pas l'image : elle fait
			// nkFree(this) et libere L'OBJET LUI-MEME. Appelee sur ce canevas
			// statique, elle rendait a l'allocateur une adresse qui ne lui
			// appartenait pas -- l'application se fermait net juste apres avoir
			// ecrit le fichier (constate deux fois par Rihen). Unload libere les
			// pixels et remet l'objet a vide, ce que son en-tete annonce
			// explicitement : « sur pile comme heap ».
			nkvpOutDark.Unload(); // la passe « fond noir » n'a plus de jumelle
			nkvpOutPass = 0;
			nkvpOutCanvas.Unload(); // AVANT de rendre la cible : plus rien n'en depend
			logger.Info("[Output]   canevas vide\n");
			nkvpOutPhase = 0; // rendu la garde, le resize normal reprend la main
			HostOutUnbindTarget(nkvpOutSaveW, nkvpOutSaveH);
			logger.Info("[Output]   vue restituee -- sortie terminee\n");
			nkvpOutStep = -1;
		}

		// `which` : 0 = « Rendre l'image », 1 = « Capturer la vue ». Les deux
		// font EXACTEMENT la meme chose (Rihen) -- meme resolution, meme
		// source, meme echelle, memes incrustations, memes types de rendu -- et
		// ne different que par le nom du fichier produit. Un bouton qui figeait
		// l'ecran tel quel a cote d'un bouton qui respecte les reglages, c'etait
		// deux verites pour un seul acte.
		bool Demo3DHostRenderOutputAs(int32 which) {
			if (nkvpOutPhase != 0 || !hst.ok || !hst.rt || !hst.rt->IsValid())
				return false;
			nkvpOutNaming = which;
			nkvpOutSaveW = hst.ctx.width;
			nkvpOutSaveH = hst.ctx.height;
			nkvpOutSaveCam = nkvpCamViewNode;
			nkvpOutSaveShading = Demo3DHostShading();
			// ── L'HABILLAGE NE PART PAS DANS L'IMAGE (Rihen, parite Blender) ─
			// Une lumiere ou une camera n'existe dans le rendu que par son
			// EFFET : sa representation -- symbole, pyramide -- est une aide de
			// travail. Idem pour la grille, les poignees de gizmo et le HUD.
			// On coupe pendant la sortie et on restaure apres, en passant par
			// les memes drapeaux que l'interface : aucun etat parallele.
			nkvpOutSaveGizmoHidden = nkvpGizmoHidden;
			nkvpOutSaveGrid = nkvpGridOn;
			nkvpOutSaveMinor = nkvpMinorOn;
			nkvpOutSaveMajor = nkvpMajorOn;
			nkvpOutSaveAxes = nkvpAxesOn;
			nkvpOutSaveHud = nkvpHudOn;
			nkvpOutSaveLightGiz = HostShowLightGizmos();
			// LES AIDES SONT UN FILTRE, L'AFFICHAGE EST LE CONTENU. Decochee,
			// l'aide COUPE ; cochee, elle laisse passer ce que la vue montre --
			// c'est l'onglet Affichage qui dit alors quelles lignes de grille
			// sont tracees, pas cette case.
			// LA GRILLE, C'EST QUATRE DRAPEAUX : la grille elle-meme, les
			// lignes fines, les majeures et LES AXES DU PLAN. N'en couper qu'un
			// laissait les axes partir dans l'image alors que l'aide etait
			// decochee -- « c'est toujours Affichage qui domine » (Rihen).
			if (!(nkvpOutAids & 1))
				nkvpGridOn = false;
			if (!(nkvpOutAids & 2))
				nkvpMinorOn = false;
			if (!(nkvpOutAids & 4))
				nkvpMajorOn = false;
			if (!(nkvpOutAids & 8))
				nkvpAxesOn = false;
			if (!(nkvpOutAids & 16))
				HostSetShowLightGizmos(false);
			if (!(nkvpOutAids & 128))
				nkvpGizmoHidden = true;
			if (!(nkvpOutAids & 256))
				nkvpHudOn = false;
			// ── FOND TRANSPARENT ────────────────────────────────────────────
			// Deux choses a couper, pas une : le CIEL, qui peindrait un decor
			// opaque derriere la scene, et l'ALPHA de la couleur d'effacement.
			// N'en faire qu'une laisserait soit un ciel, soit un aplat noir --
			// dans les deux cas une image qu'on ne peut pas superposer.
			// ── LE SOL INFINI SUIT LE FOND TRANSPARENT ─────────────────────
			// Coupe d'office quand on demande un detourage : garder un damier
			// sous un objet qu'on veut decouper n'aurait pas de sens, et il
			// occupe presque tout le cadre (constate par Rihen). La case
			// « Sol infini » le rappelle quand on le veut -- une ombre portee
			// au sol donne du poids a un objet detoure.
			nkvpOutSaveFloor = nkvpFloorOn;
			// « Sol : ombre seule » IMPLIQUE que le sol soit rendu -- sinon il
			// n'y a plus rien pour recevoir l'ombre. Cocher l'un sans l'autre
			// n'aurait aucun effet visible, ce qui ressemblerait a une panne.
			if (nkvpOutAids & 2048)
				nkvpFloorOn = true;
			else if (nkvpOutTransparent && !(nkvpOutAids & 1024))
				nkvpFloorOn = false;
			else if (nkvpOutAids & 1024)
				nkvpFloorOn = nkvpOutSaveFloor;
			nkvpOutSaveSky = Demo3DHostSkyVisible();
			if (nkvpOutTransparent) {
				Demo3DHostSetSkyVisible(false);
				logger.Info("[Output] Fond transparent : ciel coupe, effacement -> (0,0,0,0) "
							"[renderer={0}]\n",
							hst.ok ? 1 : 0);
				if (hst.ok)
					hst.ctx.renderer->SetBackgroundColor({0.f, 0.f, 0.f, 0.f});
			}
			// FILE DES MODES A PRODUIRE. Aucun coche = le mode courant, et lui
			// seul : cocher ne doit pas etre un prealable pour rendre ce qu'on
			// a sous les yeux.
			nkvpOutModeCount = 0;
			for (int32 m = 0; m < kNkvpOutModeCount; ++m)
				if (nkvpOutModes & (1 << m))
					nkvpOutModeQueue[nkvpOutModeCount++] = m;
			if (nkvpOutModeCount == 0)
				nkvpOutModeQueue[nkvpOutModeCount++] = nkvpOutSaveShading;
			nkvpOutModeIdx = 0;
			nkvpOutStep = -1; // la principale d'abord
			HostOutBeginTake(); // UN numero pour toute la serie
			nkvpOutPhase = 1;
			int32 w = 0, h = 0;
			HostOutSize(&w, &h);
			logger.Info("[Output] Rendu demande : {0}x{1}, source={2}, {3} mode(s), format {4}, "
						"transparent={5}, ciel={6}\n",
						w, h, nkvpOutSource, nkvpOutModeCount,
						kNkvpOutFmt[(nkvpOutFormat >= 0 && nkvpOutFormat < kNkvpOutFmtCount)
										? nkvpOutFormat
										: 0]
							.name,
						nkvpOutTransparent ? 1 : 0, Demo3DHostSkyVisible() ? 1 : 0);
			return true;
		}
		bool Demo3DHostRenderOutput() {
			return Demo3DHostRenderOutputAs(0);
		}

		// ── ENREGISTREMENT VIDEO : DEMARRER / ARRETER / ABANDONNER ──────────
		// ARRETER ferme proprement le fichier ; ABANDONNER le ferme aussi mais
		// l'EFFACE (Rihen : « pouvoir arreter un enregistrement qui ne serait
		// pas transfere »). Sans l'abandon, une prise ratee laisserait un
		// fichier a supprimer a la main, et on hesiterait a enregistrer.
		static void HostRecEncodeLoopOn(NkVpRec &R); // definies plus bas, pres du tick
		static void HostRecEncodeDeferred(NkVpRec &R);

		// Le couple conteneur/codec est lu de PARTOUT : mieux vaut le borner en
		// un seul endroit qu'esperer que chaque appelant l'ait fait. Un indice
		// hors bornes viendrait d'un reglage relu apres un changement de liste.
		static int32 HostVidContIdx() {
			return (nkvpOutVideoCont < 0 || nkvpOutVideoCont >= kNkvpVidContCount)
					   ? 0
					   : nkvpOutVideoCont;
		}
		static int32 HostVidCodIdx() {
			const NkVpVidCont &C = kNkvpVidCont[HostVidContIdx()];
			return (nkvpOutVideoCod < 0 || nkvpOutVideoCod >= C.codCount) ? 0
																		 : nkvpOutVideoCod;
		}

		// ── LES TROIS GESTES COMMUNS AUX DEUX ENREGISTREMENTS ───────────────
		// Attendre une place, deposer une image, ouvrir/fermer un fichier. La
		// vue et le tutoriel les partagent : ecrits deux fois, ils auraient
		// diverge au premier correctif.

		// Attend qu'une place se libere dans la file. Faux = l'image doit etre
		// SAUTEE (file pleine et mode « sauter »).
		static bool HostRecWaitSlot(NkVpRec &R) {
			bool full = false;
			{
				threading::NkScopedLock<threading::NkMutex> lk(R.mtx);
				full = R.slot[R.head].full;
			}
			if (!full)
				return true;
			if (!nkvpRecGrow) {
				++R.dropped;
				return false;
			}
			// GONFLER : on attend que l'encodage rende une place. Le fil
			// principal ralentit -- c'est le prix de la fidelite, et c'est ce
			// que l'utilisateur a demande. La file etant deja au maximum,
			// l'attente est bornee par le temps d'encoder une image.
			for (int32 guard = 0; guard < 2000 && full; ++guard) {
				NkChrono::Sleep((int64)1);
				threading::NkScopedLock<threading::NkMutex> lk(R.mtx);
				full = R.slot[R.head].full;
			}
			if (full) {
				++R.dropped; // l'encodage ne suit vraiment plus
				return false;
			}
			return true;
		}

		static void HostRecEnqueue(NkVpRec &R, NkImage &img) {
			threading::NkScopedLock<threading::NkMutex> lk(R.mtx);
			R.slot[R.head].img = static_cast<NkImage &&>(img);
			R.slot[R.head].full = true;
			R.head = (R.head + 1) % R.cap;
		}

		// `which` : 1 = la vue, 2 = le tutoriel -- il choisit le nom de base du
		// fichier, rien d'autre. Le reste est identique.
		static bool HostRecStartOn(NkVpRec &R, uint32 w, uint32 h, int32 which);
		static bool HostRecStopOn(NkVpRec &R, bool keep);

		bool Demo3DHostRecStart() {
			if (!hst.ok || !hst.rt || !hst.rt->IsValid())
				return false;
			return HostRecStartOn(nkvpRecView, hst.rt->GetWidth(), hst.rt->GetHeight(), 1);
		}

		static bool HostRecStartOn(NkVpRec &R, uint32 wIn, uint32 hIn, int32 which) {
			if (R.on)
				return false;
			R.w = wIn;
			R.h = hIn;
			if (R.w < 16 || R.h < 16)
				return false;
			const char *dir = nkvpOutDir[0] ? nkvpOutDir : "captures";
			NkDirectory::CreateRecursive(dir);
			// LE NOM DE BASE DIT LA SOURCE : la vue et le tutoriel peuvent
			// tourner EN MEME TEMPS -- ce sont deux points de vue de la meme
			// session -- et deux fichiers de meme nom se seraient ecrases.
			const char *recNm = (which == 2)
									? (nkvpCapTutoName[0] ? nkvpCapTutoName : "tutoriel")
									: (nkvpCapViewName[0] ? nkvpCapViewName : "vue");
			const int32 fps = nkvpOutFps > 0 ? nkvpOutFps : 25;
			const NkVpVidCont &C = kNkvpVidCont[HostVidContIdx()];
			const int32 codId = C.cod[HostVidCodIdx()].id;
			// 0..4 = suite d'images (le codec dit le format), 10..12 = conteneur
			// simple, 13 = H.264. TOUT conteneur est DIFFERE (cf. NkVpRec).
			R.kind = (codId < 10) ? 0 : ((codId == 13) ? 2 : 1);
			R.seq = (R.kind == 0);
			R.deferred = (R.kind != 0);
			R.frames = 0;
			R.acc = 0.f;
			R.fps = fps;
			R.encoding = false;
			R.encDone = R.encTotal = 0;
			R.tmpDir[0] = 0;
			// Reglages FIGES au demarrage : la passe finale tourne apres
			// l'arret, elle ne doit pas relire des combos qui ont pu changer.
			const int32 vq = nkvpOutVideoQuality;
			R.vq = vq;
			R.finalCod = codId;
			if (R.kind == 0) {
				// Suite d'images : un DOSSIER par prise, sinon les images de
				// deux enregistrements se melangeraient dans le meme rang. Le
				// FORMAT est celui du codec choisi -- il etait fige en PNG,
				// donc le choix de codec ne servait a rien ici.
				static const media::NkImageSeqFormat kSeqFmt[5] = {
					media::NkImageSeqFormat::PNG, media::NkImageSeqFormat::JPEG,
					media::NkImageSeqFormat::BMP, media::NkImageSeqFormat::TGA,
					media::NkImageSeqFormat::QOI};
				HostOutBeginTake(true, recNm); // c'est un DOSSIER qu'on va creer
				snprintf(R.path, sizeof(R.path), "%s/%s_%03d", dir,
						 recNm, (int)nkvpOutTake);
				NkDirectory::CreateRecursive(R.path);
				R.on = R.sw.Open(R.path, "img", (int32)R.w, (int32)R.h,
								 kSeqFmt[codId < 0 || codId > 4 ? 0 : codId], 5, vq);
			} else {
				// VIDEO EN DEUX TEMPS, quel que soit le conteneur. On n'ouvre
				// AUCUN encodeur ici : la prise ecrit des images QOI -- sans
				// perte, alpha compris, et plus rapides a encoder que le JPEG
				// (choix de Rihen : « peut-etre mieux que mon idee » de brut ;
				// meme fidelite que le brut pour 4 a 8 fois moins de disque).
				// Le fichier final se construit a l'arret, hors temps reel.
				if (R.kind == 2) {
					// La qualite H.264 se dit en QP (0..51, PETIT = meilleur) :
					// notre echelle 1..100 est bornee loin des extremes, ou
					// l'encodeur donne soit un fichier enorme, soit une bouillie.
					R.qp = 51 - (int32)((float32)vq * 0.36f + 0.5f);
					if (R.qp < 12)
						R.qp = 12;
					if (R.qp > 48)
						R.qp = 48;
				}
				HostOutBeginTake(false, recNm);
				snprintf(R.path, sizeof(R.path), "%s/%s_%03d.%s", dir,
						 recNm, (int)nkvpOutTake, C.ext);
				// Le dossier intermediaire dit son CONTENU dans son nom
				// (nom_qoi_numero, demande de Rihen) : conserve, on sait ce
				// qu'il est ; efface, personne n'a eu a le comprendre.
				snprintf(R.tmpDir, sizeof(R.tmpDir), "%s/%s_qoi_%03d", dir,
						 recNm, (int)nkvpOutTake);
				NkDirectory::CreateRecursive(R.tmpDir);
				R.tmpExt = "qoi";
				R.keepTmp = nkvpOutKeepQoi;
				R.on = R.sw.Open(R.tmpDir, "img", (int32)R.w, (int32)R.h,
								 media::NkImageSeqFormat::QOI, 5, 100);
			}
			if (R.on) {
				// Le fil d'encodage ne demarre qu'une fois le writer ouvert :
				// il ecrit dedans des sa premiere image.
				R.cap = nkvpRecGrow ? kNkvpRecQueueMax : kNkvpRecQueue;
				R.head = R.tail = 0;
				R.dropped = 0;
				R.paused = false;
				R.stopThread = false;
				for (int32 i = 0; i < R.cap; ++i)
					R.slot[i].full = false;
				// Deux lambdas plutot qu'un parametre : NkThread ne transporte
				// qu'un void*, et deux enregistrements suffisent -- une table
				// d'indirection pour deux cas connus serait du zele.
				if (which == 2)
					R.th = threading::NkThread([](void *) { HostRecEncodeLoopOn(nkvpRecTuto); });
				else
					R.th = threading::NkThread([](void *) { HostRecEncodeLoopOn(nkvpRecView); });
			}
			logger.Info("[Video] Enregistrement {0} ({1}) : {2}x{3} @ {4} i/s -> {5}\n",
						R.on ? "demarre" : "REFUSE", which == 2 ? "tutoriel" : "vue", R.w, R.h,
						fps, R.path);
			return R.on;
		}

		// PAUSE : on cesse de DEPOSER des images ; le fil d'encodage continue de
		// vider ce qui reste. Reprendre ne coute donc rien, et le fichier ne
		// contient aucune trace du temps suspendu -- c'est bien ce qu'on veut
		// d'une pause (Rihen).
		void Demo3DHostRecPause(bool on) {
			if (nkvpRecView.on)
				nkvpRecView.paused = on;
		}
		bool Demo3DHostRecPaused() {
			return nkvpRecView.on && nkvpRecView.paused;
		}
		int32 Demo3DHostRecDropped() {
			return nkvpRecView.dropped;
		}
		bool Demo3DHostRecGrow() {
			return nkvpRecGrow;
		}
		void Demo3DHostSetRecGrow(bool on) {
			// Ne change qu'entre deux prises : la file est dimensionnee au
			// demarrage, la modifier en cours melangerait les rangs.
			if (!nkvpRecView.on)
				nkvpRecGrow = on;
		}

		bool Demo3DHostRecStop(bool keep) {
			return HostRecStopOn(nkvpRecView, keep);
		}

		static bool HostRecStopOn(NkVpRec &R, bool keep) {
			if (!R.on)
				return false;
			R.on = false;
			R.paused = false;
			// ON ATTEND LA FIN DE L'ENCODAGE avant de fermer : les images encore
			// en file appartiennent a la prise, et fermer le writer sous le fil
			// qui ecrit dedans corromprait le fichier.
			{
				threading::NkScopedLock<threading::NkMutex> lk(R.mtx);
				R.stopThread = true;
			}
			if (R.th.Joinable())
				R.th.Join();
			// Pendant la prise, seule la SUITE d'images (directe ou
			// intermediaire QOI) a ecrit : c'est elle qu'on ferme.
			R.sw.Close();
			if (!keep) {
				// ABANDON : on efface ce qui vient d'etre ecrit. Le fichier a
				// bien ete ferme d'abord -- supprimer un fichier encore ouvert
				// echouerait silencieusement sous Windows.
				if (R.seq)
					NkDirectory::Delete(R.path, true);
				else
					NkDirectory::Delete(R.tmpDir, true); // la video n'existe pas encore
			}
			logger.Info("[Video] Enregistrement {0} : {1} image(s) -> {2}\n",
						keep ? "termine" : "ABANDONNE", R.frames, R.path);
			// VIDEO : la prise est finie, l'ENCODAGE commence. Sur son propre
			// fil pour que l'application reste utilisable -- une passe de
			// plusieurs minutes qui fige tout serait pire que le defaut
			// qu'elle corrige.
			if (keep && R.deferred && R.frames > 0) {
				R.encTotal = R.frames;
				R.encDone = 0;
				R.encoding = true;
				if (R.encTh.Joinable())
					R.encTh.Join();
				if (&R == &nkvpRecTuto)
					R.encTh =
						threading::NkThread([](void *) { HostRecEncodeDeferred(nkvpRecTuto); });
				else
					R.encTh =
						threading::NkThread([](void *) { HostRecEncodeDeferred(nkvpRecView); });
			}
			return true;
		}

		bool Demo3DHostRecActive() {
			return nkvpRecView.on;
		}
		int32 Demo3DHostRecFrames() {
			return nkvpRecView.frames;
		}
		const char *Demo3DHostRecPath() {
			return nkvpRecView.path;
		}

		// LA BOUCLE D'ENCODAGE, sur son fil. Elle ne touche QUE la file et les
		// writers ; le fil principal ne touche que la file. Un seul verrou
		// suffit donc, tenu le temps de prendre ou deposer une image -- jamais
		// pendant l'encodage lui-meme, qui est le long.
		static void HostRecEncodeLoopOn(NkVpRec &R) {
			for (;;) {
				NkImage img;
				bool got = false;
				{
					threading::NkScopedLock<threading::NkMutex> lk(R.mtx);
					if (R.stopThread && R.head == R.tail)
						break;
					if (R.slot[R.tail].full) {
						img = static_cast<NkImage &&>(R.slot[R.tail].img);
						R.slot[R.tail].full = false;
						R.tail = (R.tail + 1) % R.cap;
						got = true;
					}
				}
				if (!got) {
					if (R.stopThread)
						break;
					NkChrono::Sleep((int64)1); // rien a faire : on rend la main
					continue;
				}
				const uint8 *px = (const uint8 *)img.Pixels();
				// PENDANT LA PRISE, on n'ecrit QUE des images : celles du
				// livrable (suite d'images) ou les intermediaires QOI de la
				// video, encodee plus tard (voir HostRecEncodeDeferred).
				const bool ok = R.sw.WriteFrame(px, media::NkVideoInputFormat::RGBA32);
				if (ok)
					++R.frames; // lu par l'interface a titre indicatif
			}
		}

		// ── LA PASSE FINALE DE TOUTE VIDEO, sur son fil ─────────────────────
		// Elle relit les images QOI de la prise une a une et les encode vers le
		// conteneur retenu AU DEMARRAGE (les combos ont pu changer depuis).
		// Hors temps reel : l'encodeur peut prendre le temps qu'il veut, la
		// video durera exactement ce que la session a dure. Le dossier
		// temporaire n'est efface QU'APRES un encodage complet -- une panne a
		// mi-chemin doit laisser les images, sinon la prise serait perdue
		// deux fois.
		static void HostRecEncodeDeferred(NkVpRec &R) {
			bool ok = false;
			if (R.kind == 2) {
				// H.264 : l'encodeur muxe lui-meme le .mp4. GOP d'une seconde.
				ok = R.h264.Open(R.path, (int32)R.w, (int32)R.h, R.fps, 1, R.qp, R.fps);
			} else {
				media::NkVideoConfig cfg;
				cfg.width = (int32)R.w;
				cfg.height = (int32)R.h;
				cfg.fpsNum = R.fps;
				cfg.fpsDen = 1;
				cfg.quality = R.vq;
				// 10 = brut, 11 = MJPEG, 12 = MPEG-1. Le conteneur se lit sur
				// l'extension du chemin, figee au demarrage comme le reste.
				usize n = 0;
				while (R.path[n])
					++n;
				const char e = (n >= 3) ? R.path[n - 3] : 'a';
				if (R.finalCod == 12) {
					cfg.codec = media::NkVideoCodec::MPEG1;
					cfg.container = media::NkVideoContainer::ELEMENTARY;
				} else {
					cfg.codec = (R.finalCod == 10) ? media::NkVideoCodec::RAW_BGR
												   : media::NkVideoCodec::MJPEG;
					cfg.container = (e == 'm') ? media::NkVideoContainer::MOV
											   : media::NkVideoContainer::AVI;
				}
				ok = R.vw.Open(R.path, cfg);
			}
			int32 written = 0;
			char src[420];
			for (int32 i = 1; ok && i <= R.encTotal; ++i) {
				snprintf(src, sizeof(src), "%s/img_%05d.%s", R.tmpDir, (int)i, R.tmpExt);
				NkImage im;
				// QUATRE CANAUX IMPOSES : l'encodeur lit du RGBA. Le QOI en
				// porte deja quatre, mais l'imposer ici protege la passe si le
				// format intermediaire change un jour.
				if (!im.Load(src, 4) || !im.IsValid())
					break; // la suite manque : on garde ce qui est deja encode
				if ((uint32)im.Width() != R.w || (uint32)im.Height() != R.h)
					break;
				const bool wrote =
					(R.kind == 2) ? R.h264.WriteFrame((const uint8 *)im.Pixels(),
													  media::NkVideoInputFormat::RGBA32)
								  : R.vw.WriteFrame((const uint8 *)im.Pixels(),
													media::NkVideoInputFormat::RGBA32);
				if (!wrote)
					break;
				++written;
				R.encDone = written;
			}
			if (ok) {
				if (R.kind == 2)
					R.h264.Close();
				else
					R.vw.Close();
			}
			// Le dossier QOI ne s'efface QUE si l'encodage est complet ET que
			// l'utilisateur n'a pas demande a le garder (case du panneau,
			// figee au demarrage de la prise).
			if (ok && written == R.encTotal && R.encTotal > 0 && !R.keepTmp)
				NkDirectory::Delete(R.tmpDir, true);
			logger.Info("[Video] Encodage final : {0}/{1} image(s) -> {2}{3}\n", written,
						R.encTotal, R.path,
						(written == R.encTotal)
							? (R.keepTmp ? " (images QOI conservees)" : "")
							: " (images conservees dans le dossier)");
			R.encoding = false;
		}

		// Un tour d'enregistrement : appele APRES le rendu de l'image.
		static void HostRecTick(float32 dt) {
			NkVpRec &R = nkvpRecView;
			if (!R.on || R.paused || !hst.rt)
				return;
			// CADENCE TENUE : on n'ecrit une image que lorsque le temps voulu
			// s'est ecoule. Sans cela, une machine rapide produirait une video
			// acceleree et une machine lente une video au ralenti -- le fichier
			// doit durer ce que la session a dure.
			const int32 fps = nkvpOutFps > 0 ? nkvpOutFps : 25;
			R.acc += dt;
			const float32 step = 1.f / (float32)fps;
			if (R.acc < step)
				return;
			R.acc -= step;
			if (R.acc > step * 3.f)
				R.acc = step * 3.f; // pas de rattrapage sans fin apres un a-coup
			const uint32 w = hst.rt->GetWidth(), h = hst.rt->GetHeight();
			if (w != R.w || h != R.h)
				return; // la vue a change de taille : on saute plutot que de mentir
			// Le CONTENU peut etre en retard sur la taille juste apres un
			// redimensionnement : on laisse les images en vol se vider.
			if (nkvpRecSettle > 0) {
				--nkvpRecSettle;
				return;
			}
			// FILE PLEINE : selon le choix de l'utilisateur, on saute cette
			// image ou on attend qu'une place se libere. On regarde AVANT de
			// lire les pixels -- lire pour jeter ensuite serait payer le prix
			// fort pour rien.
			if (!HostRecWaitSlot(R))
				return;
			NkImage img;
			if (!img.Create(w, h, math::NkColor(0, 0, 0, 255), 4) ||
				!hst.rt->ReadbackPixels(img.Pixels(), w * 4u))
				return;
			HostRecEnqueue(R, img);
		}

		// ── ENREGISTREMENT DU TUTORIEL : LA FENETRE ENTIERE ─────────────────
		// L'application photographie sa propre fenetre -- seul l'OS sait le
		// faire -- et depose l'image ici. Toute la mecanique (file, fil
		// d'encodage, cadence, formats, pause, abandon) est celle de la vue :
		// une seule ecriture, donc un seul comportement a corriger.
		bool Demo3DHostRecTutoStart(int32 w, int32 h) {
			return HostRecStartOn(nkvpRecTuto, (uint32)w, (uint32)h, 2);
		}
		bool Demo3DHostRecTutoStop(bool keep) {
			return HostRecStopOn(nkvpRecTuto, keep);
		}
		bool Demo3DHostRecTutoActive() {
			return nkvpRecTuto.on;
		}
		void Demo3DHostRecTutoPause(bool on) {
			if (nkvpRecTuto.on)
				nkvpRecTuto.paused = on;
		}
		bool Demo3DHostRecTutoPaused() {
			return nkvpRecTuto.on && nkvpRecTuto.paused;
		}
		int32 Demo3DHostRecTutoFrames() {
			return nkvpRecTuto.frames;
		}
		int32 Demo3DHostRecTutoDropped() {
			return nkvpRecTuto.dropped;
		}
		const char *Demo3DHostRecTutoPath() {
			return nkvpRecTuto.path;
		}
		// Vrai si cette image etait ATTENDUE (cadence) : l'appelant ne
		// photographie la fenetre que dans ce cas -- une capture d'ecran coute
		// cher, la demander pour la jeter ensuite serait absurde.
		bool Demo3DHostRecTutoWants(float32 dt) {
			NkVpRec &R = nkvpRecTuto;
			if (!R.on || R.paused)
				return false;
			const int32 fps = nkvpOutFps > 0 ? nkvpOutFps : 25;
			const float32 step = 1.f / (float32)fps;
			R.acc += dt;
			if (R.acc < step)
				return false;
			R.acc -= step;
			if (R.acc > step * 3.f)
				R.acc = step * 3.f;
			return true;
		}
		bool Demo3DHostRecTutoPush(const uint8 *rgba, int32 w, int32 h) {
			NkVpRec &R = nkvpRecTuto;
			if (!R.on || R.paused || !rgba)
				return false;
			if ((uint32)w != R.w || (uint32)h != R.h)
				return false; // la fenetre a change de taille : on saute
			if (!HostRecWaitSlot(R))
				return false;
			NkImage img;
			if (!img.Create((uint32)w, (uint32)h, math::NkColor(0, 0, 0, 255), 4))
				return false;
			const int64 n = (int64)w * (int64)h * 4;
			uint8 *dst = img.Pixels();
			for (int64 i = 0; i < n; ++i)
				dst[i] = rgba[i];
			HostRecEnqueue(R, img);
			return true;
		}

		bool Demo3DHostOutBusy() {
			return nkvpOutPhase != 0;
		}
		const char *Demo3DHostOutLastPath() {
			return nkvpOutLastPath;
		}
		bool Demo3DHostOutLastOk() {
			return nkvpOutLastOk;
		}
		int32 Demo3DHostSceneCameras(int32 *out, int32 cap) {
			return HostSceneCameras(out, cap);
		}
		int32 Demo3DHostOutFormatCount() {
			return kNkvpOutFmtCount;
		}
		const char *Demo3DHostOutFormatName(int32 f) {
			return (f >= 0 && f < kNkvpOutFmtCount) ? kNkvpOutFmt[f].name : "";
		}
		const char *Demo3DHostOutFormatExt(int32 f) {
			return (f >= 0 && f < kNkvpOutFmtCount) ? kNkvpOutFmt[f].ext : "";
		}
		bool Demo3DHostOutFormatLossy(int32 f) {
			return (f >= 0 && f < kNkvpOutFmtCount) ? kNkvpOutFmt[f].lossy : false;
		}
		bool Demo3DHostOutFormatAlpha(int32 f) {
			return (f >= 0 && f < kNkvpOutFmtCount) ? kNkvpOutFmt[f].alpha : false;
		}
		bool Demo3DHostOutFreeSize() {
			return nkvpOutFreeSize;
		}
		void Demo3DHostSetOutFreeSize(bool on) {
			nkvpOutFreeSize = on;
		}
		int32 Demo3DHostOutQuality() {
			return nkvpOutQuality;
		}
		void Demo3DHostSetOutQuality(int32 q) {
			nkvpOutQuality = q < 1 ? 1 : (q > 100 ? 100 : q);
		}
		int32 Demo3DHostOutAidCount() {
			return kNkvpOutAidCount;
		}
		const char *Demo3DHostOutAidName(int32 a) {
			return (a >= 0 && a < kNkvpOutAidCount) ? kNkvpOutAidNames[a] : "";
		}
		int32 Demo3DHostOutAids() {
			return nkvpOutAids;
		}
		void Demo3DHostSetOutAids(int32 mask) {
			nkvpOutAids = mask & ((1 << kNkvpOutAidCount) - 1);
		}
		int32 Demo3DHostOutModeCount() {
			return kNkvpOutModeCount;
		}
		const char *Demo3DHostOutModeName(int32 m) {
			return (m >= 0 && m < kNkvpOutModeCount) ? kNkvpOutModeNames[m] : "";
		}
		int32 Demo3DHostOutModes() {
			return nkvpOutModes;
		}
		void Demo3DHostSetOutModes(int32 mask) {
			nkvpOutModes = mask & ((1 << kNkvpOutModeCount) - 1);
		}
		int32 Demo3DHostOutVideoQuality() {
			return nkvpOutVideoQuality;
		}
		void Demo3DHostSetOutVideoQuality(int32 q) {
			nkvpOutVideoQuality = q < 1 ? 1 : (q > 100 ? 100 : q);
		}
		void Demo3DHostOutVideo(bool *on, int32 *fps, int32 *first, int32 *last, int32 *cont) {
			if (on)
				*on = nkvpOutVideoOn;
			if (fps)
				*fps = nkvpOutFps;
			if (first)
				*first = nkvpOutFrameStart;
			if (last)
				*last = nkvpOutFrameEnd;
			if (cont)
				*cont = HostVidContIdx();
		}
		// ── CONTENEUR ET CODEC, deux listes (comme Blender) ─────────────────
		// Le codec est indexe DANS le conteneur : changer de conteneur change
		// la liste des codecs possibles, et l'indice retombe a 0 s'il n'a plus
		// de sens -- proposer « H.264 » sous « AVI » serait promettre un
		// fichier que NKMedia ne sait pas ecrire.
		int32 Demo3DHostOutVidContCount() {
			return kNkvpVidContCount;
		}
		const char *Demo3DHostOutVidContName(int32 c) {
			return (c >= 0 && c < kNkvpVidContCount) ? kNkvpVidCont[c].name : "";
		}
		const char *Demo3DHostOutVidContExt(int32 c) {
			return (c >= 0 && c < kNkvpVidContCount) ? kNkvpVidCont[c].ext : "";
		}
		int32 Demo3DHostOutVidCodCount(int32 c) {
			return (c >= 0 && c < kNkvpVidContCount) ? kNkvpVidCont[c].codCount : 0;
		}
		const char *Demo3DHostOutVidCodName(int32 c, int32 k) {
			if (c < 0 || c >= kNkvpVidContCount)
				return "";
			return (k >= 0 && k < kNkvpVidCont[c].codCount) ? kNkvpVidCont[c].cod[k].name : "";
		}
		int32 Demo3DHostOutVidCod() {
			return HostVidCodIdx();
		}
		bool Demo3DHostOutCursor() {
			return nkvpOutCursor;
		}
		void Demo3DHostSetOutCursor(bool on) {
			nkvpOutCursor = on;
		}
		bool Demo3DHostOutKeepQoi() {
			return nkvpOutKeepQoi;
		}
		void Demo3DHostSetOutKeepQoi(bool on) {
			nkvpOutKeepQoi = on;
		}
		void Demo3DHostSetOutVidCod(int32 k) {
			const NkVpVidCont &C = kNkvpVidCont[HostVidContIdx()];
			nkvpOutVideoCod = (k < 0 || k >= C.codCount) ? 0 : k;
		}
		// ── ENCODAGE DIFFERE DU MP4 : ce qu'il faut pouvoir AFFICHER ────────
		// Une passe de plusieurs minutes qui ne dirait rien passerait pour un
		// blocage. La barre du pied de page lit ces trois valeurs.
		bool Demo3DHostRecEncoding() {
			return nkvpRecView.encoding;
		}
		bool Demo3DHostRecTutoEncoding() {
			return nkvpRecTuto.encoding;
		}
		void Demo3DHostRecEncodeProgress(int32 *done, int32 *total) {
			if (done)
				*done = nkvpRecView.encDone;
			if (total)
				*total = nkvpRecView.encTotal;
		}
		void Demo3DHostRecTutoEncodeProgress(int32 *done, int32 *total) {
			if (done)
				*done = nkvpRecTuto.encDone;
			if (total)
				*total = nkvpRecTuto.encTotal;
		}
		int32 Demo3DHostOutFastCount() {
			return kNkvpOutFastCount;
		}
		const char *Demo3DHostOutFastName(int32 i) {
			return (i >= 0 && i < kNkvpOutFastCount) ? kNkvpOutFastNames[i] : "";
		}
		int32 Demo3DHostOutFastMask() {
			return nkvpOutFastMask;
		}
		void Demo3DHostSetOutFastMask(int32 m) {
			nkvpOutFastMask = m & ((1 << kNkvpOutFastCount) - 1);
		}
		void Demo3DHostSetOutVideo(bool on, int32 fps, int32 first, int32 last, int32 cont) {
			nkvpOutVideoOn = on;
			nkvpOutFps = fps < 1 ? 1 : (fps > 240 ? 240 : fps);
			nkvpOutFrameStart = first < 0 ? 0 : first;
			nkvpOutFrameEnd = last < nkvpOutFrameStart ? nkvpOutFrameStart : last;
			const int32 c0 = nkvpOutVideoCont;
			nkvpOutVideoCont = (cont < 0 || cont >= kNkvpVidContCount) ? 0 : cont;
			// CHANGER DE CONTENEUR REMET LE CODEC A ZERO : garder l'indice 1
			// en passant d'AVI (2 codecs) a MOV (1 seul) aurait designe un
			// codec inexistant.
			if (nkvpOutVideoCont != c0)
				nkvpOutVideoCod = 0;
		}

		void Demo3DHostResize(uint32 w, uint32 h) {
			// PENDANT UN RENDU DE SORTIE, la taille de la fenetre n'a pas voix
			// au chapitre : l'editeur appelle cette fonction a chaque image et
			// ramenerait la cible a la taille de la vue, donc au format de
			// l'ecran plutot qu'a celui demande. La demande est memorisee et
			// s'appliquera a la restauration.
			if (nkvpOutPhase != 0) {
				hst.wantW = w;
				hst.wantH = h;
				return;
			}
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
				// L'enregistrement laisse passer les images qui suivent : leur
				// contenu couvre encore l'ancienne taille (voir nkvpRecSettle).
				nkvpRecSettle = 5;
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
			// ── SORTIE : un tour de machine AVANT le rendu ──────────────────
			// Le redimensionnement de la cible DETRUIT et recree sa texture.
			// Fait apres Demo3D_Frame, il arrachait donc la texture que les
			// commandes de CETTE image venaient de referencer, et la
			// soumission trouvait un objet mort -- l'application se fermait
			// net au retour a la taille de la vue (constate par Rihen).
			// Avant le rendu, l'image entiere s'enregistre et se soumet sur
			// une cible stable. La lecture, elle, ne change pas de sens : elle
			// prend la DERNIERE image rendue, qui est celle de l'etape
			// precedente.
			HostOutTick();
			nkvpCmd = cmd;
			Demo3D_Frame(hst.ctx, dt);
			nkvpCmd = nullptr;
			// Parente : repercuter les deltas des parents a leurs enfants, et
			// faire respecter le cadenas (INselectionnable, meme depuis la vue).
			HostHierarchyFrame();
			// ENREGISTREMENT : apres le rendu, la cible porte l'image de cette
			// frame -- c'est le seul moment ou elle est complete.
			HostRecTick(dt);
		}

		/// Reconstruit l'instance moteur d'un materiau depuis son etat. Definie
		/// bien plus bas, avec les facades qu'elle appelle ; declaree ICI parce que
		/// l'apercu, lui, s'en sert des cette hauteur.
		static void HostMatRebuildEngine(int32 i);

		// Le renderer sur lequel les facades de materiau travaillent : celui de
		// l'apercu pendant qu'on reconstruit ses instances, celui de la vue sinon.
		static renderer::NkRenderer *NkvpMatRd() {
			return nkvpMatRdCible ? nkvpMatRdCible : hst.ctx.renderer;
		}

		// ── LA BASCULE, EN PORTEE ───────────────────────────────────────────
		// Elle remet TOUJOURS la cible d'origine en sortant, y compris sur un
		// retour anticipe : une cible restee sur l'apercu ferait ecrire les
		// reglages de l'utilisateur dans le mauvais renderer -- un defaut qui ne
		// se verrait qu'a la prochaine modification, donc tres loin de sa cause.
		struct NkvpMatCibleScope {
				NkMaterial **oldT;
				renderer::NkRenderer *oldR;
				NkvpMatCibleScope(NkMaterial **t, renderer::NkRenderer *r)
					: oldT(nkvpMatCible), oldR(nkvpMatRdCible) {
					nkvpMatCible = t;
					nkvpMatRdCible = r;
				}
				~NkvpMatCibleScope() {
					nkvpMatCible = oldT;
					nkvpMatRdCible = oldR;
				}
		};

		void Demo3DHostRegisterInto(void *guiBackend) {
			if (!hst.ok || !hst.rt || !guiBackend)
				return;
			auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
			auto *texLib = hst.ctx.renderer->GetTextures();
			if (!texLib)
				return;
			b->RegisterTexture(kHostTexId, texLib->GetRHIHandle(hst.rt->GetColorResult()));
			// La texture de l'APERCU DE MATERIAU suit le meme chemin : rendue hors
			// ecran par son propre renderer, publiee comme une image d'interface.
			nk3d::matprev::RegisterInto(guiBackend);
		}

		// ── APERCU DE MATERIAU : une image par frame ────────────────────────
		// Appelee depuis la frame de l'hote, avant que la passe backbuffer ne
		// s'ouvre. Le slot demande est celui que le panneau affiche ; les autres
		// materiaux ne sont pas rendus (leurs cartes recevront une capture figee
		// a l'enregistrement).
		// ── VIGNETTE D'UN MATERIAU : CAPTURE DU VRAI RENDU ──────────────────
		// « Les cartes recoivent le resultat correct, mais sous forme de capture a
		// la sauvegarde » (Rihen, 13 aout). C'est la bonne economie : le rendu GPU
		// est fidele -- il execute le shader du verre, du toon, de l'emissif --
		// mais rendre soixante-quatre scenes par frame pour des vignettes de
		// quarante pixels serait absurde. On en fige une, au moment ou le materiau
		// est ecrit sur le disque.
		//
		// EN DEUX FRAMES, et c'est oblige : la lecture des pixels doit porter sur
		// une image TERMINEE. On rend a la frame N, on relit a la frame N+1. Une
		// file, parce qu'un enregistrement de projet ecrit tous les materiaux d'un
		// coup et qu'on n'en rend qu'un par frame.
		namespace {
			constexpr int32 kThumbMax = 64;
			constexpr int32 kThumbPx = 128; // carre : c'est une icone de liste
			struct NkMatThumbFile {
					int32 slot[kThumbMax] = {};
					char chemin[kThumbMax][320] = {};
					int32 nb = 0;
					int32 rendu = -1; // slot rendu a la frame precedente, -1 = aucun
					char renduChemin[320] = {};
			};
			NkMatThumbFile gThumbs;
		} // namespace

		const char *Demo3DHostProjMatThumb(int32 i) {
			return (i >= 0 && i < kNkvpMaxProjMats) ? nkvpProjMatThumb[i].CStr() : "";
		}
		void Demo3DHostProjMatSetThumb(int32 i, const char *b64) {
			if (i >= 0 && i < kNkvpMaxProjMats)
				nkvpProjMatThumb[i] = b64 ? b64 : "";
		}

		void Demo3DHostMatThumbRequest(int32 slot, const char *cheminPng) {
			if (slot < 0 || slot >= kNkvpMaxProjMats)
				return;
			(void)cheminPng; // la vignette ne va plus dans un fichier voisin
			if (gThumbs.nb >= kThumbMax)
				return; // file pleine : la vignette attendra le prochain
			for (int32 i = 0; i < gThumbs.nb; ++i)
				if (gThumbs.slot[i] == slot)
					return; // deja demande
			gThumbs.slot[gThumbs.nb] = slot;
			++gThumbs.nb;
		}

		void Demo3DHostMatPreviewFrame(void *cmd, int32 slot, int32 w, int32 h) {
			if (!cmd)
				return;
			// ── LA VIGNETTE PASSE AVANT ─────────────────────────────────────
			// Elle emprunte la meme cible : une capture demandee prend donc la
			// frame, et le grand apercu reprend a la suivante. Un scintillement
			// d'une frame vaut mieux qu'une seconde cible hors ecran gardee toute
			// la session pour un usage aussi rare.
			//
			// D'ABORD RELIRE ce qui a ete rendu a la frame precedente : l'image
			// est terminee, c'est le seul moment ou elle est lisible.
			if (gThumbs.rendu >= 0) {
				const int32 sc = gThumbs.rendu;
				gThumbs.rendu = -1;
				static uint8 px[kThumbPx * kThumbPx * 4];
				if (sc >= 0 && sc < kNkvpMaxProjMats && nk3d::matprev::Readback(px)) {
					NkImage im;
					if (im.Create((uint32)kThumbPx, (uint32)kThumbPx,
								  math::NkColor(0, 0, 0, 255), 4)) {
						memcpy(im.Pixels(), px, sizeof(px));
						// PNG D'ABORD, base64 ENSUITE. Les pixels bruts en base64
						// pesteraient 87 Ko par materiau ; compresses, il en reste
						// quelques-uns -- pour la meme image.
						uint8 *pngBuf = nullptr;
						usize pngSz = 0;
						if (im.EncodePNG(pngBuf, pngSz) && pngBuf && pngSz > 0) {
							nkvpProjMatThumb[sc] = encoding::base64::NkEncode(pngBuf, pngSz);
							NkLog::Instance().Info(
								"[apercu] vignette du materiau {0} : {1} octets PNG", sc,
								(uint32)pngSz);
							memory::NkFree(pngBuf);
						} else
							NkLog::Instance().Info("[apercu] vignette {0} : encodage refuse",
												   sc);
					}
				}
			}
			// PUIS rendre la suivante de la file, en SPHERE et en carre : c'est
			// une icone de liste, elle doit se comparer aux autres.
			if (gThumbs.nb > 0 && nk3d::matprev::Init(hst.ctx.device, (uint32)kThumbPx,
													  (uint32)kThumbPx)) {
				const int32 sc = gThumbs.slot[0];
				snprintf(gThumbs.renduChemin, sizeof(gThumbs.renduChemin), "%s",
						 gThumbs.chemin[0]);
				for (int32 i = 1; i < gThumbs.nb; ++i) {
					gThumbs.slot[i - 1] = gThumbs.slot[i];
					snprintf(gThumbs.chemin[i - 1], sizeof(gThumbs.chemin[0]), "%s",
							 gThumbs.chemin[i]);
				}
				--gThumbs.nb;
				if (sc >= 0 && sc < kNkvpMaxProjMats && nkvpProjMats[sc].used &&
					nkvpProjMatPrev[sc]) {
					NkDrawCall3D mt;
					HostMatSlotToDC(sc, mt);
					nk3d::matprev::RenderOne((NkICommandBuffer *)cmd,
											 nkvpProjMatPrev[sc]->GetInstHandle(), 1,
											 (uint32)kThumbPx, (uint32)kThumbPx,
											 (float32)hst.ctx.totalTime, mt);
					gThumbs.rendu = sc;
					return; // cette frame est a elle
				}
			}
			if (slot < 0 || slot >= kNkvpMaxProjMats)
				return;
			if (!nkvpProjMats[slot].used)
				return;
			if (!nk3d::matprev::Init(hst.ctx.device, (uint32)(w > 0 ? w : 260),
									 (uint32)(h > 0 ? h : 150)))
				return;
			// ── L'INSTANCE DU RENDERER D'APERCU ─────────────────────────────
			// Reconstruite par `HostMatRebuildEngine`, la MEME fonction que pour
			// la vue 3D : on bascule seulement sa destination. Recopier son
			// application des reglages aurait fait deux verites sur ce qu'est un
			// materiau, et la copie aurait diverge au premier reglage ajoute.
			//
			// Reconstruite QUAND L'ETAT CHANGE, pas a chaque frame : sans temoin on
			// recreerait une instance soixante fois par seconde, et la collection
			// du renderer d'apercu (64 emplacements) serait pleine en une seconde.
			//
			// LE TEMOIN EST L'ETAT ENTIER, compare octet a octet. Une signature
			// faite d'une somme de quelques champs -- ce que j'avais ecrit -- ne
			// voyait pas les autres : changer l'emissif, une texture, un reglage
			// toon ou l'anisotropie ne rafraichissait rien, et l'apercu mentait
			// (Rihen : « changer les proprietes du materiau doit s'appliquer »).
			// Comparer la structure entiere ne peut rien manquer, et le jour ou un
			// reglage s'ajoute, il est couvert sans qu'on ait a y penser.
			static NkVpProjMat sVuPrev[kNkvpMaxProjMats] = {};
			static bool sVuInit[kNkvpMaxProjMats] = {};
			const NkVpProjMat &pm = nkvpProjMats[slot];
			if (!nkvpProjMatPrev[slot] || !sVuInit[slot] ||
				memcmp(&sVuPrev[slot], &pm, sizeof(NkVpProjMat)) != 0) {
				memcpy(&sVuPrev[slot], &pm, sizeof(NkVpProjMat));
				sVuInit[slot] = true;
				NkvpMatCibleScope bascule(nkvpProjMatPrev, nk3d::matprev::Renderer());
				HostMatRebuildEngine(slot);
			}
			NkMaterial *me = nkvpProjMatPrev[slot];
			if (!me) {
				static bool sDit = false;
				if (!sDit) {
					sDit = true;
					NkLog::Instance().Info(
						"[apercu] pas d'instance de materiau pour l'emplacement {0}", slot);
				}
				return;
			}
		// LES SURCHARGES DE MATIERE, remplies par la MEME fonction que la vue 3D.
			// Un draw call vierge sert de porteur : c'est le type que `HostMatSlotToDC`
			// sait remplir, et s'en servir garantit qu'aucun reglage n'est oublie en
			// route -- ni aujourd'hui, ni quand un reglage s'ajoutera.
			NkDrawCall3D matiere;
			HostMatSlotToDC(slot, matiere);
			nk3d::matprev::RenderOne((NkICommandBuffer *)cmd, me->GetInstHandle(),
									 (int32)nkvpProjMats[slot].prevShape, (uint32)(w > 0 ? w : 260),
									 (uint32)(h > 0 ? h : 150), (float32)hst.ctx.totalTime,
									 matiere);
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
			else if (st && nkvpCamViewNode >= 0 && nkvpCamViewNode < kNkvpMaxNodes &&
					 !nkvpDeleted[nkvpCamViewNode]) {
				// EN VUE CAMERA, la boule suit LA CAMERA : le regard vient du
				// noeud (transformation d'axes, meme lecon que l'override de
				// vue), pas de la camera d'editeur -- sinon la boule restait
				// figee pendant qu'on tournait la camera (constate par Rihen).
				float32 cwpA[3], cwscA[3];
				NkMat4f cwrA;
				HostNodeWorldById(nkvpCamViewNode, cwpA, cwrA, cwscA);
				f = (cwrA * NkVec3f{0.f, 0.f, -1.f}).Normalized();
			} else if (st) {
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
			HostPushExtFrames(st, o); // repere pose des le changement
			st->gizmo.SetOrientation(o);
			st->editGizmo.SetOrientation(o);
			st->lightGizmo.SetOrientation(o);
			// Le gizmo des VIDES (cameras, maillages utilisateur, empties)
			// manquait a la liste : Local/Normal n'y arrivaient jamais -- les
			// axes d'une camera restaient GLOBAUX quelle que soit sa rotation
			// (constate par Rihen).
			st->emptyGizmo.SetOrientation(o);
		}
		int32 Demo3DHostOrientation() {
			auto *st = HostSt();
			if (!st)
				return 0;
			return (st->editMode ? st->editGizmo : st->gizmo).Orientation();
		}
		// ── POINT DE PIVOT (Blender : « Transform Pivot Point ») ────────────
		// 0 barycentre, 1 boite englobante, 2 curseur 3D, 3 origines
		// individuelles, 4 element actif -- pousse aux QUATRE gizmos, comme
		// l'orientation (l'oubli du gizmo des vides a deja coute une fois).
		void Demo3DHostSetPivotMode(int32 m) {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.SetPivotMode(m);
			st->editGizmo.SetPivotMode(m);
			st->lightGizmo.SetPivotMode(m);
			st->emptyGizmo.SetPivotMode(m);
		}
		int32 Demo3DHostPivotMode() {
			auto *st = HostSt();
			if (!st)
				return 0;
			return (st->editMode ? st->editGizmo : st->gizmo).PivotMode();
		}
		// ── CIBLE D'AIMANTATION (Blender : « Snap To », liste de Rihen) ─────
		// 0 increment, 1 grille, 2 sommet, 3 arete, 4 face, 5 volume (a venir),
		// 6 centre d'arete, 7 arete perpendiculaire (a venir), 8 centre de face.
		void Demo3DHostSetSnapTarget(int32 t) {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.SetSnapTarget(t);
			st->editGizmo.SetSnapTarget(t);
			st->lightGizmo.SetSnapTarget(t);
			st->emptyGizmo.SetSnapTarget(t);
			// Trace de diagnostic : si elle manque au journal apres un choix
			// dans le panneau, le poussage UI a echoue -- meme methode que le
			// combo Source de l'ambiance.
			logger.Info("[NkDemo3D] Aimantation : cible -> {0}\n", t);
		}
		// ── ECHELLE EXACTE (cisaillement autorise) ──────────────────────────
		// ── EDITION PROPORTIONNELLE ─────────────────────────────────────────
		void Demo3DHostPropEdit(bool *on, float32 *radius, int32 *falloff) {
			if (on)
				*on = nkvpPropEditOn;
			if (radius)
				*radius = nkvpPropEditRadius;
			if (falloff)
				*falloff = nkvpPropEditFalloff;
		}
		void Demo3DHostSetPropEdit(bool on, float32 radius, int32 falloff) {
			nkvpPropEditOn = on;
			// Rayon NUL = aucune influence : on garde un plancher, sinon la
			// bascule paraitrait sans effet.
			nkvpPropEditRadius = radius < 0.01f ? 0.01f : radius;
			nkvpPropEditFalloff = falloff < 0 ? 0 : (falloff > 7 ? 7 : falloff);
		}
		bool Demo3DHostShearScale() {
			return nkvpShearOpt;
		}
		void Demo3DHostSetShearScale(bool on) {
			auto *st = HostSt();
			nkvpShearOpt = on;
			if (!st)
				return;
			st->gizmo.SetAllowShear(on);
			st->editGizmo.SetAllowShear(on);
			st->lightGizmo.SetAllowShear(on);
			st->emptyGizmo.SetAllowShear(on);
			// COUPER l'option REDRESSE les objets : leur cisaillement n'est plus
			// representable, et le garder afficherait un etat que plus rien ne
			// pourrait modifier ni sauvegarder.
			if (!on)
				for (int32 e = 0; e < 70; ++e)
					nkvpEmptyShear[e] = false;
			logger.Info("[NkDemo3D] Echelle exacte (cisaillement) -> {0}\n",
						on ? "oui" : "non");
		}
		int32 Demo3DHostSnapBase() {
			return nkvpSnapBase;
		}
		void Demo3DHostSetSnapBase(int32 b) {
			nkvpSnapBase = (b < 0 || b > 2) ? 0 : b;
		}
		bool Demo3DHostSnapAlignRot() {
			auto *st = HostSt();
			if (!st)
				return false;
			return st->gizmo.SnapAlignRot();
		}
		void Demo3DHostSetSnapAlignRot(bool on) {
			auto *st = HostSt();
			if (!st)
				return;
			// Les QUATRE gizmos, comme la cible : celui des vides a deja ete
			// oublie deux fois dans des fan-out.
			st->gizmo.SetSnapAlignRot(on);
			st->editGizmo.SetSnapAlignRot(on);
			st->lightGizmo.SetSnapAlignRot(on);
			st->emptyGizmo.SetSnapAlignRot(on);
		}
		int32 Demo3DHostSnapTarget() {
			auto *st = HostSt();
			if (!st)
				return 0;
			return (st->editMode ? st->editGizmo : st->gizmo).SnapTarget();
		}
		void Demo3DHostSetSnap(bool on, float32 t, float32 rotDeg, float32 scl) {
			auto *st = HostSt();
			if (!st)
				return;
			st->gizmo.SetSnapEnabled(on);
			st->editGizmo.SetSnapEnabled(on);
			st->lightGizmo.SetSnapEnabled(on);
			// Le gizmo des VIDES (maillages utilisateur, cameras, empties)
			// MANQUAIT : l'aimantation n'arrivait jamais aux cubes de la scene
			// (constate par Rihen -- « l'aimant ne fonctionne pas »).
			st->emptyGizmo.SetSnapEnabled(on);
			st->gizmo.SetSnapSteps(t, rotDeg, scl);
			st->editGizmo.SetSnapSteps(t, rotDeg, scl);
			st->lightGizmo.SetSnapSteps(t, rotDeg, scl);
			st->emptyGizmo.SetSnapSteps(t, rotDeg, scl);
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
			// JAMAIS g.showAxes : l'axe Y du shader est errone (la « seconde
			// ligne verte » constatee par Rihen). Les vrais axes sont les
			// lignes debug, pilotees par nkvpAxesOn.
			g.showAxes = false;
			nkvpAxesOn = axes; // les axes debug de la demo suivent la meme case
			nkvpMinorOn = minor;
			nkvpMajorOn = major;
		}
		bool Demo3DHostCursorShown() {
			return nkvpCursorShow;
		}
		void Demo3DHostSetCursorShown(bool on) {
			nkvpCursorShow = on;
		}
		void Demo3DHostGridFlags(bool *grid, bool *minor, bool *major, bool *axes) {
			auto *r3d = HostR3D();
			if (!r3d) {
				*grid = *minor = *major = *axes = true;
				return;
			}
			// Les VOLONTES, jamais l'etat moteur : la demo coupe les axes shader
			// a l'init (axes en lignes debug) et la premiere synchro du shell
			// DECOCHAIT « Axes du plan » et « Lignes internes » (Rihen).
			*grid = nkvpGridOn;
			*minor = nkvpMinorOn;
			*major = nkvpMajorOn;
			*axes = nkvpAxesOn;
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
			// MEMORISEE : le rendu a fond transparent la remplace le temps d'une
			// sortie et doit pouvoir la remettre. Le moteur n'expose pas de
			// lecture de sa couleur d'effacement -- on garde donc la derniere
			// posee, qui est par construction celle qui est active.
			nkvpBgColor[0] = r;
			nkvpBgColor[1] = g;
			nkvpBgColor[2] = b;
			// PENDANT UNE SORTIE TRANSPARENTE, L'INTERFACE N'A PAS LA MAIN.
			// Le panneau de rendu repose cette couleur A CHAQUE IMAGE (« le
			// moteur a une garde d'egalite », dit son commentaire) : elle
			// ecrasait donc l'effacement transparent avant meme que le rendu
			// n'ait lieu, et l'image sortait avec le fond gris habituel,
			// opaque. Exactement le meme piege que Demo3DHostResize, qui se
			// protege deja de la taille de la vue pour la meme raison. La
			// valeur demandee est memorisee ci-dessus : elle sera reappliquee
			// a la restauration.
			if (nkvpOutPhase != 0 && nkvpOutTransparent)
				return;
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
				if (!st->gizmo.IsSelected(i) || HostLockedOwn(i))
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
			// On ecrit le drapeau DU DOCUMENT COURANT : masquer depuis la scene ne
			// doit rien changer dans l'editeur de model, alors que masquer depuis
			// le model se voit dans toutes les scenes (regle de Rihen).
			if (i >= 0 && i < 160) {
				if (nkvpDocIsModel)
					nkvpMeshHidden[i] = hidden;
				else
					nkvpObjHidden[i] = hidden;
			}
		}
		bool Demo3DHostObjectHidden(int32 i) {
			return (i >= 0 && i < 160) &&
				   (nkvpDocIsModel ? nkvpMeshHidden[i] : nkvpObjHidden[i]);
		}
		// ETAT EFFECTIF (le sien OU celui d'un ancetre). L'interface DOIT montrer
		// celui-la : un enfant dont le parent est cadenasse refuse la selection,
		// et afficher son cadenas OUVERT rendait ce refus incomprehensible
		// (constate par Rihen : « je ne peux selectionner ni le parent ni
		// l'enfant » -- le parent avait ete verrouille par megarde).
		bool Demo3DHostObjectLockedEff(int32 i) {
			return HostLockedEff(i);
		}
		bool Demo3DHostObjectHiddenEff(int32 i) {
			return HostHiddenEff(i);
		}
		void Demo3DHostSetObjectLocked(int32 i, bool locked) {
			// Le verrou reste DANS SON CONTEXTE, dans les deux sens : verrouiller
			// en scene n'entrave pas l'edition du model, et verrouiller dans le
			// model n'entrave pas la scene -- ca n'y a pas d'importance (Rihen).
			if (i >= 0 && i < 160) {
				if (nkvpDocIsModel)
					nkvpMeshLocked[i] = locked;
				else
					nkvpObjLocked[i] = locked;
			}
		}
		bool Demo3DHostObjectLocked(int32 i) {
			return (i >= 0 && i < 160) && HostLockedOwn(i);
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
		// ── DECOMPOSITION CONTINUE ──────────────────────────────────────────
		// Une meme orientation s'ecrit de DEUX facons en angles d'Euler :
		//   (X, Y, Z)  et  (X+180, 180-Y, Z+180).
		// HostDecompose en choisit toujours une (celle ou |Y| <= 90, asin ne
		// sachant pas rendre autre chose). Quand la rotation franchit ce
		// domaine, l'ecriture bascule sur l'autre solution : les angles SAUTENT
		// de 180 degres et un axe part en sens inverse -- la « cassure »
		// constatee par Rihen. Ici on calcule les deux ecritures, on garde
		// celle qui est la PLUS PROCHE des angles precedents, puis on la
		// DEROULE (multiples de 360) pour rester dans la continuite de ce que
		// l'utilisateur lisait. Le gimbal lock vrai (Y a +-90 exactement) reste
		// une degenerescence des angles d'Euler ; seule une representation par
		// quaternion l'evite completement.
		static void HostDecomposeNear(const NkMat4f &M, const float32 *prevDeg, NkVec3f &pos,
									  NkVec3f &rotDeg, NkVec3f &scl) {
			HostDecompose(M, pos, rotDeg, scl);
			if (!prevDeg)
				return;
			// Seconde ecriture, equivalente a la premiere.
			NkVec3f alt{rotDeg.x + 180.f, 180.f - rotDeg.y, rotDeg.z + 180.f};
			auto wrapNear = [](float32 a, float32 ref) {
				// Ramene `a` dans la fenetre de +-180 degres autour de `ref` :
				// 179 -> -179 devient 181, et la lecture ne saute pas.
				while (a - ref > 180.f)
					a -= 360.f;
				while (ref - a > 180.f)
					a += 360.f;
				return a;
			};
			const NkVec3f c1{wrapNear(rotDeg.x, prevDeg[0]), wrapNear(rotDeg.y, prevDeg[1]),
							 wrapNear(rotDeg.z, prevDeg[2])};
			const NkVec3f c2{wrapNear(alt.x, prevDeg[0]), wrapNear(alt.y, prevDeg[1]),
							 wrapNear(alt.z, prevDeg[2])};
			const float32 d1 = math::NkAbs(c1.x - prevDeg[0]) + math::NkAbs(c1.y - prevDeg[1]) +
							   math::NkAbs(c1.z - prevDeg[2]);
			const float32 d2 = math::NkAbs(c2.x - prevDeg[0]) + math::NkAbs(c2.y - prevDeg[1]) +
							   math::NkAbs(c2.z - prevDeg[2]);
			rotDeg = (d2 < d1) ? c2 : c1;
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
			// SCENE DE DEPART VIERGE (Rihen) : le menu Ajouter cree tout,
			// la scene demo n'est plus imposee -- restent le SOL et le SOLEIL.
			for (int32 i = 0; i < kNkvpMaxNodes; ++i)
				nkvpDeleted[i] = true;
			// Le TRIO de depart (cube + ponctuelle + camera) est cree a la
			// premiere frame, quand le moteur est pret.
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
			// DANS UN MODEL, LE SEUL PARENT EST LE MODEL (regle de Rihen). Un
			// maillage est une donnee geometrique, pas un noeud de hierarchie :
			// toute tentative de le rendre parent est rabattue sur la racine du
			// model, de sorte que ses maillages restent tous FRERES. Le controle
			// est ici, au point de passage unique, plutot que dans chaque geste
			// (depot, Ctrl+P, menu Ajouter) ou l'on finirait par en oublier un.
			if (nkvpDocIsModel && parent >= 0 && nkvpIsMesh[parent])
				parent = Demo3DHostModelRootOf(parent);
			// Refus des CYCLES : on ne parente pas a soi-meme ni a un de ses
			// propres descendants.
			if (parent >= 0 && HostIsDescendant(parent, child))
				return false;
			nkvpParentOf[child] = parent;
			return true;
		}
		int32 Demo3DHostEnsureModelMesh(int32 root) {
			// A l'ouverture d'un editeur : le noeud devient le MODEL, et sa
			// geometrie propre descend dans un premier MESH interne -- le cube
			// avec ses sommets et ses faces (Rihen). Les maillages ajoutes
			// ensuite sont FRERES de celui-la, jamais ses enfants, et lui ne
			// devient jamais un model : seul le conteneur porte ce nom.
			if (root < kNkvpFirstUser || root >= kNkvpMaxNodes)
				return -1;
			for (int32 c = 0; c < kNkvpMaxNodes; ++c)
				if (!nkvpDeleted[c] && nkvpIsMesh[c] && nkvpParentOf[c] == root) {
					// Il a deja sa matiere -- mais il faut TOUJOURS le dire model :
					// a la deuxieme ouverture, sortir d'ici sans le marquer le
					// laissait rendre sa geometrie en double de son maillage.
					nkvpIsModel[root] = true;
					return c;
				}
			const uint8 rk = nkvpUserKind[root - kNkvpFirstUser];
			if (rk < 1 || rk > 3)
				return -1; // pas une geometrie : rien a deleguer
			const float32 zero[3] = {0.f, 0.f, 0.f};
			const int32 m = HostSpawnLike(root, zero);
			if (m < 0)
				return -1;
			nkvpIsMesh[m] = true;
			nkvpIsModel[m] = false;
			nkvpParentOf[m] = root;
			// DANS le model : transform locale neutre. Il se pose exactement
			// sur lui, et suit sa position, sa rotation et son echelle.
			const int32 e = m - kNkvpFirstEmpty;
			for (int32 a = 0; a < 3; ++a) {
				nkvpEmptyPos[e][a] = 0.f;
				nkvpEmptyRotDeg[e][a] = 0.f;
				nkvpEmptyScl[e][a] = 1.f;
			}
			// Le noeud devient le MODEL. Sa NATURE ne change pas (il reste un
			// cube) : on ne le convertit surtout pas en empty, ce changement
			// repartirait dans la scene. Il cesse simplement de rendre sa
			// geometrie, que son maillage rend desormais pour lui.
			nkvpIsModel[root] = true;
			return m;
		}
		bool Demo3DHostMeshCounts(int32 node, int32 *verts, int32 *edges, int32 *tris) {
			// LES VRAIS COMPTEURS du maillage, lus dans le systeme de maillages.
			// Le panneau doit montrer la geometrie REELLE de l'objet : des nombres
			// d'illustration y seraient pires que rien.
			*verts = *edges = *tris = 0;
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return false;
			const int32 u = node - kNkvpFirstUser;
			if (!nkvpUserMesh[u].IsValid())
				return false;
			auto *ms = hst.ctx.renderer ? hst.ctx.renderer->GetMeshSystem() : nullptr;
			if (!ms)
				return false;
			const int32 nv = (int32)ms->GetVertexCount(nkvpUserMesh[u]);
			const int32 ni = (int32)ms->GetIndexCount(nkvpUserMesh[u]);
			*verts = nv;
			*tris = ni / 3;
			// Aretes : sans table d'adjacence, la relation d'Euler donne le compte
			// exact d'un maillage ferme (V - E + F = 2 -> E = V + F - 2).
			*edges = (nv > 0 && *tris > 0) ? (nv + *tris - 2) : 0;
			return nv > 0;
		}
		bool Demo3DHostNodeOrigin(int32 node, float32 *out3) {
			// L'ORIGINE d'un noeud est son point de pivot : c'est autour d'elle
			// qu'il tourne et se met a l'echelle, lui et tout ce qu'il porte. Elle
			// merite d'etre lue et posee explicitement (Rihen).
			if (node < kNkvpFirstEmpty || node >= kNkvpMaxNodes)
				return false;
			const int32 e = node - kNkvpFirstEmpty;
			out3[0] = nkvpEmptyPos[e][0];
			out3[1] = nkvpEmptyPos[e][1];
			out3[2] = nkvpEmptyPos[e][2];
			return true;
		}
		void Demo3DHostSetNodeOrigin(int32 node, const float32 *p3) {
			// DEPLACER L'ORIGINE SANS DEPLACER LA MATIERE : le noeud bouge, et ses
			// enfants sont recules d'autant -- a l'ecran, rien ne bouge, mais le
			// point de rotation et de mise a l'echelle a change. C'est le geste
			// « definir l'origine » des modeleurs.
			if (node < kNkvpFirstEmpty || node >= kNkvpMaxNodes)
				return;
			const int32 e = node - kNkvpFirstEmpty;
			const float32 d[3] = {p3[0] - nkvpEmptyPos[e][0], p3[1] - nkvpEmptyPos[e][1],
								  p3[2] - nkvpEmptyPos[e][2]};
			for (int32 a = 0; a < 3; ++a)
				nkvpEmptyPos[e][a] = p3[a];
			for (int32 c = 0; c < kNkvpMaxNodes; ++c) {
				if (nkvpDeleted[c] || nkvpParentOf[c] != node || c < kNkvpFirstEmpty)
					continue;
				const int32 ce = c - kNkvpFirstEmpty;
				for (int32 a = 0; a < 3; ++a)
					nkvpEmptyPos[ce][a] -= d[a];
			}
		}
		bool Demo3DHostMeshesCenter(int32 node, float32 *out3) {
			// Centre de la MATIERE d'un noeud : moyenne des origines de ses
			// maillages. Sert a poser l'origine « au centre de la geometrie ».
			float32 acc[3] = {0.f, 0.f, 0.f};
			int32 n = 0;
			for (int32 c = kNkvpFirstEmpty; c < kNkvpMaxNodes; ++c) {
				if (nkvpDeleted[c] || !nkvpIsMesh[c] || nkvpParentOf[c] != node)
					continue;
				const int32 ce = c - kNkvpFirstEmpty;
				for (int32 a = 0; a < 3; ++a)
					acc[a] += nkvpEmptyPos[ce][a];
				++n;
			}
			if (n == 0)
				return false;
			const int32 e = node - kNkvpFirstEmpty;
			for (int32 a = 0; a < 3; ++a)
				out3[a] = nkvpEmptyPos[e][a] + acc[a] / (float32)n;
			return true;
		}
		void Demo3DHostFlattenModel(int32 root) {
			// Remet A PLAT les maillages d'un model : tous enfants DIRECTS de la
			// racine. Sert a l'ouverture d'un editeur, pour que la regle vaille
			// aussi sur ce qui a ete assemble avant elle.
			if (root < 0 || root >= kNkvpMaxNodes)
				return;
			for (int32 c = 0; c < kNkvpMaxNodes; ++c) {
				if (c == root || nkvpDeleted[c] || !nkvpIsMesh[c])
					continue;
				int32 cur = nkvpParentOf[c];
				for (int32 g = 0; g < kNkvpMaxNodes && cur >= 0; ++g) {
					if (cur == root) {
						nkvpParentOf[c] = root;
						break;
					}
					if (!nkvpIsMesh[cur])
						break; // appartient a un AUTRE model
					cur = nkvpParentOf[cur];
				}
			}
			// PAS DE RATTRAPAGE ICI. J'avais ajoute une boucle marquant comme
			// matiere toute geometrie portee par le model : elle happait aussi ses
			// MODELS ENFANTS, restes dans la scene mais toujours parentes a lui.
			// Ils devenaient des maillages -- donc des maillages AVEC enfants,
			// disparaissant de la hierarchie de scene (Rihen). Un noeud ne devient
			// de la matiere qu'a sa NAISSANCE dans un editeur de model.
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
				// CE QUE LIT LE PANNEAU : les angles saisis tant qu'ils font foi,
				// sinon une lecture CONTINUE du quaternion (l'ecriture la plus
				// proche de la precedente). Pendant un glissement, on y compose
				// la rotation en cours -- toujours sans decomposer le stockage.
				if (st->emptyGizmo.IsDragging() && st->emptyGizmo.IsSelected(e)) {
					NkVec3f gp, gr, gs;
					HostDecomposeNear((NkQuatf(st->emptyGizmo.RotationOf(e)) *
									   HostNodeQuat(e))
										  .ToMat4(),
									  nkvpEmptyRotDeg[e], gp, gr, gs);
					rotDeg3[0] = gr.x;
					rotDeg3[1] = gr.y;
					rotDeg3[2] = gr.z;
				} else {
					HostNodeEuler(e, rotDeg3);
				}
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
			// ── LES ANGLES TAPES SONT LES ANGLES VOULUS ─────────────────────
			// Ils posent le quaternion ET restent affiches tels quels : taper
			// 190 ne doit pas se relire -170 (meme orientation, autre ecriture
			// -- le comportement d'Unreal, qui surprend a l'usage). L'ancienne
			// voie composait un DELTA puis redecomposait le produit : taper 45
			// sur X changeait alors TOUS les angles (constate par Rihen).
			if (fabsf(rotDeg3[0] - cr[0]) + fabsf(rotDeg3[1] - cr[1]) +
					fabsf(rotDeg3[2] - cr[2]) >
				1e-7f)
				HostSetNodeEuler(e, rotDeg3);
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
		// ── MATERIAUX DU PROJET (pastille Materiau) ─────────────────────────
		int32 Demo3DHostProjMatCreate() {
			// Le DERNIER emplacement est reserve au magenta « aucun materiau » : la
			// creation s'arrete avant lui, sinon un materiau de l'utilisateur
			// l'ecraserait des que le registre se remplit.
			for (int32 i = 0; i < kNkvpMissingMat; ++i) {
				if (nkvpProjMats[i].used)
					continue;
				NkVpProjMat &m = nkvpProjMats[i];
				m.used = true;
				m.prevShape = 1; // la sphere, l'apercu canonique
				snprintf(m.name, sizeof(m.name), "Materiau.%03d", nkvpMatSerial++);
				// GRIS NEUTRE MAT : le meme point de depart que les objets nus,
				// pour que « creer puis assigner » ne change rien tant qu'on n'a
				// pas touche un curseur -- aucun effet subi.
				m.albedo[0] = m.albedo[1] = m.albedo[2] = 0.7f;
				m.rough = 0.85f;
				m.metal = 0.f;
				// Physique de surface neutre : ni vernis ni diffusion sur un
				// materiau neuf (l'emplacement peut etre REUTILISE apres une
				// suppression — l'init statique du tableau ne suffit pas).
				m.clearcoat = 0.f;
				m.ccRough = 0.f;
				m.subsurface = 0.f;
				// Intensites NEUTRES : relief a pleine echelle (une normal map
				// posee doit se voir telle qu'elle est), emissif a 1 mais avec
				// une teinte NOIRE -- un materiau neuf n'emet rien.
				m.nrmStrength = 1.f;
				m.emiStrength = 1.f;
				m.emissive[0] = m.emissive[1] = m.emissive[2] = 0.f;
				m.parallax = 0.f; // le relief parallax est un choix, pas un defaut
				// PROPORTIONNELLE par defaut : une vitre a 0.3 laisse passer
				// 70 % de la lumiere, ce que l'oeil attend.
				m.shadowMode = 1;
				m.mixWith = 0;	  // pas de melange a la naissance
				m.mixSource = 0;
				m.mixFactor = 0.5f;
				m.matType = 0; // Standard (PBR)
				m.alpha = 1.f;
				m.aniso = 0.f;
				m.sheenV = 0.f;
				// Defauts toon = ceux de NkToonParams (une seule verite).
				m.toonThresh = 0.3f;
				m.toonSmooth = 0.05f;
				m.toonShadow[0] = 0.2f;
				m.toonShadow[1] = 0.1f;
				m.toonShadow[2] = 0.3f;
				m.outlineW = 2.f;
				m.outlineCol[0] = m.outlineCol[1] = m.outlineCol[2] = 0.f;
				m.rimI = 0.5f;
				m.rimCol[0] = m.rimCol[1] = m.rimCol[2] = 1.f;
				m.specHard = 32.f;
				return i;
			}
			return -1;
		}
		// Le materiau PAR DEFAUT du projet, en indice+1. Defini ICI et non plus
		// bas avec le registre : la suppression, juste en dessous, doit le
		// connaitre pour le proteger.
		static int32 nkvpDefaultMatP1 = 0;

		void Demo3DHostProjMatDelete(int32 i) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			// LE MATERIAU PAR DEFAUT NE SE SUPPRIME PAS (Rihen, 12 aout : « on
			// doit avoir un materiau par defaut insupprimable par
			// l'utilisateur »). Il est le filet du projet : tout objet
			// nouvellement cree le porte, et delier un materiau y ramene. Le
			// perdre laisserait des objets sans matiere. Rien n'empeche en
			// revanche de le RETIRER d'un objet et d'y mettre le sien.
			if (nkvpDefaultMatP1 - 1 == i)
				return;
			// LE DERNIER MATERIAU NE SE SUPPRIME PAS (regle de Rihen) : tout
			// maillage porte toujours un materiau, il faut donc qu'il en reste.
			int32 nUsed = 0, fallback = -1;
			for (int32 k = 0; k < kNkvpMaxProjMats; ++k)
				if (nkvpProjMats[k].used) {
					++nUsed;
					if (k != i && fallback < 0)
						fallback = k;
				}
			if (nUsed <= 1 || fallback < 0)
				return;
			nkvpProjMats[i].used = false;
			// Les porteurs CONVERGENT vers un materiau restant, jamais vers
			// rien (regle de Rihen) : supprimer l'un des deux fait converger
			// tous ses maillages vers l'autre.
			for (int32 n = 0; n < kNkvpMaxNodes; ++n)
				if (nkvpNodeMatP1[n] - 1 == i)
					nkvpNodeMatP1[n] = fallback + 1;
		}
		// ── MATERIAU PAR DEFAUT (11 aout, demande de Rihen) ─────────────────
		// L'emplacement+1 choisi comme defaut (0 = aucun choix explicite : le
		// premier du registre fait foi, comme avant).
		int32 Demo3DHostProjMatDefault() {
			const int32 d = nkvpDefaultMatP1 - 1;
			return (d >= 0 && d < kNkvpMaxProjMats && nkvpProjMats[d].used) ? d : -1;
		}
		void Demo3DHostProjMatSetDefault(int32 i) {
			nkvpDefaultMatP1 =
				(i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used) ? i + 1 : 0;
		}
		// ── LE MATERIAU « AUCUN » : MAGENTA, INTERNE, INACCESSIBLE ──────────────
		// Idee de Rihen (13 aout) : plutot que de peindre en magenta au dernier
		// moment, un objet sans materiau est ASSIGNE a un vrai materiau magenta que
		// l'utilisateur ne voit pas. C'est la bonne facon de faire, et pour une
		// raison qui a coute deux correctifs : forcer `dc.tint` juste avant l'envoi
		// ne suffit pas -- des qu'un materiau porte une instance moteur, c'est ELLE
		// qui decide de la couleur, de l'opacite et du type. Un objet sans materiau
		// n'en liait aucune, et la teinte magenta se faisait ecraser par le reste
		// du pipeline (l'objet restait gris et translucide).
		//
		// Il occupe le DERNIER emplacement du registre, et `Demo3DHostProjMatInfo`
		// le refuse : il n'apparait donc dans aucune liste, aucune pastille, aucun
		// fichier. Il n'est pas « le defaut » -- celui-la est un vrai materiau que
		// l'utilisateur peut regler ; celui-ci signale une ABSENCE.

		static int32 HostEnsureMissingMat() {
			NkVpProjMat &m = nkvpProjMats[kNkvpMissingMat];
			if (!m.used) {
				m = NkVpProjMat{};
				m.used = true;
				snprintf(m.name, sizeof(m.name), "%s", "(aucun materiau)");
				m.albedo[0] = 1.f;
				m.albedo[1] = 0.f;
				m.albedo[2] = 1.f;
				m.alpha = 1.f;	 // OPAQUE : une absence ne se devine pas au travers
				m.rough = 1.f;	 // mat : aucun reflet ne doit adoucir le signal
				m.metal = 0.f;
			}
			return kNkvpMissingMat;
		}

		// Il existe TOUJOURS au moins un materiau des qu'un maillage existe :
		// renvoie le DEFAUT choisi s'il vit encore, sinon le premier du
		// registre, en le creant au besoin.
		static int32 HostEnsureDefaultMat() {
			const int32 d = nkvpDefaultMatP1 - 1;
			if (d >= 0 && d < kNkvpMaxProjMats && nkvpProjMats[d].used)
				return d;
			// AUCUN DEFAUT DESIGNE : on en designe un plutot que de renvoyer un
			// materiau quelconque. Sans cela, « le defaut » aurait change au
			// gre des suppressions, et la regle « il est insupprimable »
			// n'aurait protege personne en particulier.
			for (int32 k = 0; k < kNkvpMaxProjMats; ++k)
				if (nkvpProjMats[k].used) {
					nkvpDefaultMatP1 = k + 1;
					return k;
				}
			const int32 ni = Demo3DHostProjMatCreate();
			if (ni >= 0)
				nkvpDefaultMatP1 = ni + 1;
			return ni;
		}
		// ── ECHANGE DE DEUX EMPLACEMENTS (fleches d'organisation de Rihen) ──
		// L'ordre AFFICHE est l'ordre des emplacements : echanger reellement
		// garantit que fichiers, cartes et melanges suivent — un simple ordre
		// d'affichage aurait diverge du reste. TOUTES les references suivent :
		// porteurs (noeuds), melanges (mixWith), defaut, textures, moteur.
		bool Demo3DHostProjMatSwap(int32 a, int32 b) {
			if (a < 0 || b < 0 || a >= kNkvpMaxProjMats || b >= kNkvpMaxProjMats ||
				a == b || !nkvpProjMats[a].used || !nkvpProjMats[b].used)
				return false;
			NkVpProjMat tm = nkvpProjMats[a];
			nkvpProjMats[a] = nkvpProjMats[b];
			nkvpProjMats[b] = tm;
			for (int32 c = 0; c < kNkvpMatChanCount; ++c) {
				NkTexHandle tt = nkvpProjMatChanTex[a][c];
				nkvpProjMatChanTex[a][c] = nkvpProjMatChanTex[b][c];
				nkvpProjMatChanTex[b][c] = tt;
			}
			NkMaterial *te = nkvpProjMatEng[a];
			nkvpProjMatEng[a] = nkvpProjMatEng[b];
			nkvpProjMatEng[b] = te;
			for (int32 n = 0; n < kNkvpMaxNodes; ++n) {
				if (nkvpNodeMatP1[n] == a + 1)
					nkvpNodeMatP1[n] = b + 1;
				else if (nkvpNodeMatP1[n] == b + 1)
					nkvpNodeMatP1[n] = a + 1;
			}
			for (int32 k = 0; k < kNkvpMaxProjMats; ++k) {
				if (!nkvpProjMats[k].used)
					continue;
				if (nkvpProjMats[k].mixWith == (int8)(a + 1))
					nkvpProjMats[k].mixWith = (int8)(b + 1);
				else if (nkvpProjMats[k].mixWith == (int8)(b + 1))
					nkvpProjMats[k].mixWith = (int8)(a + 1);
			}
			if (nkvpDefaultMatP1 == a + 1)
				nkvpDefaultMatP1 = b + 1;
			else if (nkvpDefaultMatP1 == b + 1)
				nkvpDefaultMatP1 = a + 1;
			return true;
		}
		int32 Demo3DHostProjMatEnsureDefault() {
			return HostEnsureDefaultMat();
		}
		bool Demo3DHostProjMatInfo(int32 i, char *name, uint32 cap, float32 *albedo3,
								   float32 *rough, float32 *metal) {
			// LE MAGENTA « AUCUN MATERIAU » N'EXISTE POUR PERSONNE. Il porte une
			// absence, pas une matiere : il ne doit apparaitre ni dans la liste du
			// projet, ni dans celle de l'objet, ni dans une pastille, ni dans un
			// fichier (Rihen, 13 aout : « il ne peut pas etre visible dans la liste
			// des materiaux de l'objet actif, pas du tout »). Tout passe par cette
			// fonction pour lire un materiau — la refuser ici le rend invisible
			// partout d'un seul geste, sans avoir a filtrer dans chaque appelant.
			if (i == kNkvpMissingMat)
				return false;
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return false;
			const NkVpProjMat &m = nkvpProjMats[i];
			if (name && cap)
				snprintf(name, cap, "%s", m.name);
			if (albedo3) {
				albedo3[0] = m.albedo[0];
				albedo3[1] = m.albedo[1];
				albedo3[2] = m.albedo[2];
			}
			if (rough)
				*rough = m.rough;
			if (metal)
				*metal = m.metal;
			return true;
		}
		void Demo3DHostProjMatSetParams(int32 i, const float32 *albedo3, float32 rough,
										float32 metal) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			if (albedo3) {
				m.albedo[0] = albedo3[0];
				m.albedo[1] = albedo3[1];
				m.albedo[2] = albedo3[2];
			}
			m.rough = rough < 0.f ? 0.f : (rough > 1.f ? 1.f : rough);
			m.metal = metal < 0.f ? 0.f : (metal > 1.f ? 1.f : metal);
		}
		// ── PHYSIQUE DE SURFACE : vernis + diffusion (2026-08-09) ────────────
		// A part de SetParams : les appelants historiques (collage de groupe,
		// reinitialisation) ne connaissent que albedo/rugosite/metallique, et
		// changer leur signature les aurait tous forces a se prononcer.
		void Demo3DHostProjMatSurface(int32 i, float32 *cc, float32 *ccRough, float32 *sss) {
			const bool ok = (i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used);
			if (cc)
				*cc = ok ? nkvpProjMats[i].clearcoat : 0.f;
			if (ccRough)
				*ccRough = ok ? nkvpProjMats[i].ccRough : 0.f;
			if (sss)
				*sss = ok ? nkvpProjMats[i].subsurface : 0.f;
		}
		void Demo3DHostProjMatSetSurface(int32 i, float32 cc, float32 ccRough, float32 sss) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			// DOUBLE EMPLOI ASSUME de `clearcoat`, selon le type du materiau :
			//   - familles PBR : le VERNIS, borne a [0,1] ;
			//   - type VERRE   : l'INDICE DE REFRACTION, dans [1,4] — et son
			//     SIGNE porte la coche d'activation (negatif = « desactive,
			//     mais je garde la valeur »), ce qui evite de propager un
			//     booleen de plus jusqu'a l'UBO, au .nkmat et aux facades.
			// Meme principe qu'Unreal, qui reaffecte ses entrees selon le
			// modele d'ombrage. AUCUN plafond physique sur l'indice : le
			// silicium vaut ~3.9, le germanium 4 a 5 dans l'infrarouge, et un
			// metamateriau descend sous 1. Le facteur de Fresnel, lui, reste
			// borne [0,1] par conservation d'energie — la formule s'en charge.
			// Garde-fou numerique seulement (division a n = -1).
			m.clearcoat = cc < -64.f ? -64.f : (cc > 64.f ? 64.f : cc);
			m.ccRough = ccRough < 0.f ? 0.f : (ccRough > 1.f ? 1.f : ccRough);
			m.subsurface = sss < 0.f ? 0.f : (sss > 1.f ? 1.f : sss);
		}
		void Demo3DHostProjMatSetName(int32 i, const char *name) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used || !name || !name[0])
				return;
			snprintf(nkvpProjMats[i].name, sizeof(nkvpProjMats[i].name), "%s", name);
		}
		void Demo3DHostProjMatAssign(int32 node, int32 mat) {
			if (node < 0 || node >= kNkvpMaxNodes)
				return;
			const bool ok = (mat >= 0 && mat < kNkvpMaxProjMats && nkvpProjMats[mat].used);
			nkvpNodeMatP1[node] = ok ? mat + 1 : 0;
			// ASSIGNER, C'EST AUSSI ASSOCIER : le materiau entre dans la liste
			// de l'objet s'il n'y etait pas. Sans cela la liste resterait vide
			// pour tous les objets crees avant ce modele, et la pastille
			// n'aurait rien a montrer.
			if (ok)
				(void)HostNodeMatAdd(node, mat);
		}

		// ── LA LISTE DE L'OBJET, EXPOSEE A L'INTERFACE ───────────────────
		int32 Demo3DHostNodeMatCount(int32 node) {
			return HostNodeMatCount(node);
		}
		int32 Demo3DHostNodeMatAt(int32 node, int32 k) {
			return HostNodeMatAt(node, k);
		}
		bool Demo3DHostNodeMatAdd(int32 node, int32 slot) {
			return HostNodeMatAdd(node, slot);
		}
		bool Demo3DHostNodeMatRemove(int32 node, int32 slot) {
			return HostNodeMatRemove(node, slot);
		}
		int32 Demo3DHostProjMatOf(int32 node) {
			if (node < 0 || node >= kNkvpMaxNodes)
				return -1;
			const int32 m = nkvpNodeMatP1[node] - 1;
			// Un objet peint en magenta n'a PAS de materiau : c'est ce que
			// l'interface doit lire, sinon la pastille afficherait le materiau
			// interne comme s'il etait le sien.
			return (m == kNkvpMissingMat) ? -1 : m;
		}
		int32 Demo3DHostMatChanCount() {
			return kNkvpMatChanCount;
		}
		const char *Demo3DHostMatChanName(int32 c) {
			return (c >= 0 && c < kNkvpMatChanCount) ? kNkvpMatChanNames[c] : "";
		}
		const char *Demo3DHostProjMatMap(int32 i, int32 chan) {
			return (i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used && chan >= 0 &&
					chan < kNkvpMatChanCount)
					   ? nkvpProjMats[i].maps[chan]
					   : "";
		}
		// UNE SEULE fonction pour les quatre canaux : quatre copies auraient
		// diverge au premier correctif -- c'est exactement ce qui est arrive
		// aux combos de la sortie.
		bool Demo3DHostProjMatSetMap(int32 i, int32 chan, const char *path) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used || chan < 0 ||
				chan >= kNkvpMatChanCount)
				return false;
			auto *matS = NkvpMatRd() ? NkvpMatRd()->GetMaterials() : nullptr;
			// « - » ou vide RETIRE la texture : retour aux valeurs numeriques.
			// Le moteur reprend sa texture PAR DEFAUT (blanc pour la couleur,
			// normale plate...) : poser un handle invalide laisserait l'ancienne
			// en place, et retirer n'aurait aucun effet visible.
			if (!path || !path[0] || (path[0] == '-' && !path[1])) {
				nkvpProjMats[i].maps[chan][0] = 0;
				nkvpProjMatChanTex[i][chan] = NkTexHandle{};
				if (NkvpMatEng(i)) {
					switch (chan) {
						case 0: NkvpMatEng(i)->SetAlbedoMap(NkTexHandle{}); break;
						case 1: NkvpMatEng(i)->SetNormalMap(NkTexHandle{}, 0.f); break;
						case 2: NkvpMatEng(i)->SetORMMap(NkTexHandle{}); break;
						case 3: NkvpMatEng(i)->SetEmissiveMap(NkTexHandle{}); break;
						default:
							// HAUTEUR : retour au blanc 1x1 (surface plate) — un
							// handle invalide laisserait l'ancienne carte au GPU.
							if (auto *tl = NkvpMatRd() ? NkvpMatRd()->GetTextures() : nullptr)
								NkvpMatEng(i)->SetTexture("height", tl->GetWhite1x1());
							break;
					}
				}
				return true;
			}
			auto *texL = NkvpMatRd() ? NkvpMatRd()->GetTextures() : nullptr;
			if (!texL || !matS)
				return false;
			// Le chargement fait foi : un chemin qui ne charge pas n'est PAS
			// memorise -- le champ garde l'ancienne verite au lieu d'afficher
			// un chemin mort.
			//
			// L'ESPACE COULEUR DEPEND DU CANAL (LearnOpenGL, Gamma Correction ;
			// verifie le 9 aout) : une image d'ALBEDO ou d'EMISSIF est peinte a
			// l'ecran, donc encodee sRGB -> le GPU doit la lineariser a la
			// lecture (format _SRGB). Une carte de NORMALES ou d'ORM porte des
			// PARAMETRES, pas des couleurs : deja lineaire -- la decoder comme
			// du sRGB TORD les normales et decale rugosite/metallicite dans les
			// tons moyens. Le defaut (srgb=true pour tout) etait faux pour les
			// canaux 1 et 2.
			NkLoadOptions texOpts;
			texOpts.srgb = (chan == 0 || chan == 3);
			NkTexHandle t = texL->Load(NkString(path), texOpts);
			if (!t.IsValid()) {
				logger.Warn("[NkDemo3D] Materiau '{0}' : texture introuvable {1}\n",
							nkvpProjMats[i].name, path);
				return false;
			}
			nkvpProjMatChanTex[i][chan] = t;
			if (!NkvpMatEng(i))
				NkvpMatEng(i) = NkMaterial::Create(matS, NkMaterialType::NK_PBR_METALLIC);
			if (NkvpMatEng(i)) {
				switch (chan) {
					case 0: NkvpMatEng(i)->SetAlbedoMap(t); break;
					case 1:
						NkvpMatEng(i)->SetNormalMap(t, nkvpProjMats[i].nrmStrength);
						break;
					case 2: NkvpMatEng(i)->SetORMMap(t); break;
					case 3: NkvpMatEng(i)->SetEmissiveMap(t); break;
					default:
						NkvpMatEng(i)->SetTexture("height", t);
						NkvpMatEng(i)->SetParallaxScale(nkvpProjMats[i].parallax);
						break;
				}
			}
			snprintf(nkvpProjMats[i].maps[chan], sizeof(nkvpProjMats[i].maps[chan]), "%s",
					 path);
			logger.Info("[NkDemo3D] Materiau '{0}' : canal {1} -> {2}\n",
						nkvpProjMats[i].name, kNkvpMatChanNames[chan], path);
			return true;
		}
		// INTENSITES : relief (0..2) et emissif (0..20, les emetteurs montent
		// haut). Elles n'ont d'effet qu'avec leur texture -- sauf la teinte
		// emissive, qui vaut aussi sans.
		void Demo3DHostProjMatChanStrength(int32 i, float32 *nrm, float32 *emi) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			if (nrm)
				*nrm = ok ? nkvpProjMats[i].nrmStrength : 1.f;
			if (emi)
				*emi = ok ? nkvpProjMats[i].emiStrength : 1.f;
		}
		void Demo3DHostProjMatSetChanStrength(int32 i, float32 nrm, float32 emi) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			m.nrmStrength = nrm < 0.f ? 0.f : (nrm > 2.f ? 2.f : nrm);
			m.emiStrength = emi < 0.f ? 0.f : (emi > 20.f ? 20.f : emi);
			if (!NkvpMatEng(i))
				return;
			// L'intensite de relief vit DANS SetNormalMap : la reposer exige de
			// redonner la texture -- sinon le curseur n'aurait aucun effet.
			if (m.maps[1][0] && nkvpProjMatChanTex[i][1].IsValid())
				NkvpMatEng(i)->SetNormalMap(nkvpProjMatChanTex[i][1], m.nrmStrength);
			NkvpMatEng(i)->SetEmissive({m.emissive[0], m.emissive[1], m.emissive[2]},
										   m.emiStrength);
		}
		// ── ECHELLE DU PARALLAX (canal Hauteur — etape 3, 10 aout) ──────────
		// A part des intensites : c'est un reglage de PROFONDEUR (en fraction
		// d'UV, ~0.02-0.08 utile), pas un dosage de texture. Borne a 0.2 : au
		// dela, l'etirement rasant detruit toute lecture de la surface.
		float32 Demo3DHostProjMatParallax(int32 i) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			return ok ? nkvpProjMats[i].parallax : 0.f;
		}
		void Demo3DHostProjMatSetParallax(int32 i, float32 scale) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			m.parallax = scale < 0.f ? 0.f : (scale > 0.2f ? 0.2f : scale);
			if (NkvpMatEng(i))
				NkvpMatEng(i)->SetParallaxScale(m.parallax);
		}
		int32 Demo3DHostProjMatShadowMode(int32 i) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return 1;
			return nkvpProjMats[i].shadowMode;
		}
		void Demo3DHostProjMatSetShadowMode(int32 i, int32 mode) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			m.shadowMode = (mode < 0 || mode > 2) ? 1 : mode;
			if (NkvpMatEng(i))
				NkvpMatEng(i)->SetTransShadowMode((uint32)m.shadowMode);
		}
		void Demo3DHostProjMatEmissive(int32 i, float32 *rgb) {
			if (!rgb)
				return;
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			for (int32 k = 0; k < 3; ++k)
				rgb[k] = ok ? nkvpProjMats[i].emissive[k] : 0.f;
		}
		void Demo3DHostProjMatSetEmissive(int32 i, const float32 *rgb) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used || !rgb)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			for (int32 k = 0; k < 3; ++k)
				m.emissive[k] = rgb[k] < 0.f ? 0.f : rgb[k];
			if (NkvpMatEng(i))
				NkvpMatEng(i)->SetEmissive({m.emissive[0], m.emissive[1], m.emissive[2]},
											   m.emiStrength);
		}
		// ── MELANGE DE MATERIAUX (etape 1 — Rihen : « mixer, operations, comme
		// Blender et Unreal ») ──────────────────────────────────────────────
		// Le moteur porte le melange par le gabarit LAYERED_V1 : couche 0 = CE
		// materiau, couche 1 = le materiau B, masque = facteur constant, couleur
		// de sommets ou UV. Changer de mode change de GABARIT : l'instance
		// moteur est RECREEE puis TOUT est reapplique depuis NkVpProjMat (l'
		// ancienne instance reste au registre moteur — meme politique que la
		// suppression). LIMITE V1 assumee : les couches melangent les VALEURS
		// (couleur/metal/rugosite), pas les textures des deux materiaux — le
		// nodal NKGraphe portera ce chantier-la.
		static void HostMatRebuildEngine(int32 i) {
			auto *matS = NkvpMatRd() ? NkvpMatRd()->GetMaterials() : nullptr;
			if (!matS || i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			const int32 bIdx = (int32)m.mixWith - 1;
			const bool mix = bIdx >= 0 && bIdx < kNkvpMaxProjMats && bIdx != i &&
							 nkvpProjMats[bIdx].used;
			// Le TYPE choisi fait le gabarit ; le melange prime (LAYERED_V1).
			NkvpMatEng(i) = NkMaterial::Create(
				matS, mix ? NkMaterialType::NK_LAYERED_V1 : (NkMaterialType)m.matType);
			if (!NkvpMatEng(i))
				return;
			if (mix) {
				const NkVpProjMat &b = nkvpProjMats[bIdx];
				NkPBRLayer l0;
				l0.albedo = {m.albedo[0], m.albedo[1], m.albedo[2], 1.f};
				l0.metallic = m.metal;
				l0.roughness = m.rough;
				NkPBRLayer l1;
				l1.albedo = {b.albedo[0], b.albedo[1], b.albedo[2], 1.f};
				l1.metallic = b.metal;
				l1.roughness = b.rough;
				// Source du masque de la couche B ; la couche 0 est la base.
				static const NkLayerMaskSource kSrc[7] = {
					NK_LAYER_MASK_CONSTANT,	 NK_LAYER_MASK_VCOLOR_R, NK_LAYER_MASK_VCOLOR_G,
					NK_LAYER_MASK_VCOLOR_B,	 NK_LAYER_MASK_VCOLOR_A, NK_LAYER_MASK_UV_X,
					NK_LAYER_MASK_UV_Y};
				const int32 s = m.mixSource < 0 ? 0 : (m.mixSource > 6 ? 6 : (int32)m.mixSource);
				NkvpMatEng(i)
					->SetLayerV1(0, l0)
					->SetLayerV1(1, l1)
					->SetLayerV1Mask(1, kSrc[s], m.mixFactor)
					->SetLayerV1Count(2);
				return;
			}
			// Retour au PBR : reappliquer TOUT depuis l'etat, par les memes
			// facades que la relecture d'un .nkmat (une seule verite d'application).
			Demo3DHostProjMatSetParams(i, m.albedo, m.rough, m.metal);
			Demo3DHostProjMatSetSurface(i, m.clearcoat, m.ccRough, m.subsurface);
			Demo3DHostProjMatSetChanStrength(i, m.nrmStrength, m.emiStrength);
			Demo3DHostProjMatSetEmissive(i, m.emissive);
			Demo3DHostProjMatSetParallax(i, m.parallax);
			Demo3DHostProjMatSetShadowMode(i, m.shadowMode);
			for (int32 c = 0; c < kNkvpMatChanCount; ++c)
				if (m.maps[c][0]) {
					// Copie locale : SetMap re-ecrit le champ qu'on lui passe
					// (snprintf sur soi-meme = recouvrement indefini).
					char path[260];
					snprintf(path, sizeof(path), "%s", m.maps[c]);
					Demo3DHostProjMatSetMap(i, c, path);
				}
			// Les reglages sans facade historique : opacite, anisotropie, sheen,
			// et la famille toon quand le type l'est.
			NkvpMatEng(i)
				->SetAlbedo({m.albedo[0], m.albedo[1], m.albedo[2]}, m.alpha)
				->SetAnisotropy(m.aniso)
				->SetSheen(m.sheenV);
			if (m.matType == 20 || m.matType == 21 || m.matType == 22) {
				NkvpMatEng(i)
					->SetToonThreshold(m.toonThresh)
					->SetToonSmooth(m.toonSmooth)
					->SetToonShadow({m.toonShadow[0], m.toonShadow[1], m.toonShadow[2]})
					->SetOutline(m.outlineW,
								 {m.outlineCol[0], m.outlineCol[1], m.outlineCol[2]})
					->SetRim(m.rimI, {m.rimCol[0], m.rimCol[1], m.rimCol[2]})
					->SetSpecHardness(m.specHard);
			}
		}
		int32 Demo3DHostProjMatMixWith(int32 i) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			return ok ? (int32)nkvpProjMats[i].mixWith - 1 : -1;
		}
		int32 Demo3DHostProjMatMixSource(int32 i) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			return ok ? (int32)nkvpProjMats[i].mixSource : 0;
		}
		float32 Demo3DHostProjMatMixFactor(int32 i) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			return ok ? nkvpProjMats[i].mixFactor : 0.5f;
		}
		void Demo3DHostProjMatSetMix(int32 i, int32 withSlot, int32 source, float32 factor) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			const bool valid = withSlot >= 0 && withSlot < kNkvpMaxProjMats &&
							   withSlot != i && nkvpProjMats[withSlot].used;
			m.mixWith = (int8)(valid ? withSlot + 1 : 0);
			m.mixSource = (int8)(source < 0 ? 0 : (source > 6 ? 6 : source));
			m.mixFactor = factor < 0.f ? 0.f : (factor > 1.f ? 1.f : factor);
			HostMatRebuildEngine(i);
		}
		// ── TYPE DE MATERIAU + reglages publics restants (11 aout) ──────────
		int32 Demo3DHostProjMatType(int32 i) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			return ok ? (int32)nkvpProjMats[i].matType : 0;
		}
		void Demo3DHostProjMatSetType(int32 i, int32 type) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			if ((int32)nkvpProjMats[i].matType == type)
				return;
			nkvpProjMats[i].matType = (uint8)(type < 0 ? 0 : (type > 255 ? 0 : type));
			// UN TYPE EST UN PREREGLAGE (decision d'architecture du 12 aout) :
			// choisir « Verre » doit donner une VITRE tout de suite, sans aller
			// chercher un curseur. On ne pose ce defaut que si l'opacite n'a
			// jamais ete touchee (exactement 1) — un reglage voulu se garde.
			// L'opacite reste ensuite pleinement reglable : a 1 on obtient un
			// verre teinte/laque, opaque MAIS qui reflete toujours.
			if (type == 5 /* NK_GLASS */ && nkvpProjMats[i].alpha >= 0.999f)
				nkvpProjMats[i].alpha = 0.12f;
			HostMatRebuildEngine(i); // changer de type = changer de gabarit
		}
		void Demo3DHostProjMatPBRExtra(int32 i, float32 *alpha, float32 *aniso,
									   float32 *sheen) {
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			if (alpha)
				*alpha = ok ? nkvpProjMats[i].alpha : 1.f;
			if (aniso)
				*aniso = ok ? nkvpProjMats[i].aniso : 0.f;
			if (sheen)
				*sheen = ok ? nkvpProjMats[i].sheenV : 0.f;
		}
		void Demo3DHostProjMatSetPBRExtra(int32 i, float32 alpha, float32 aniso,
										  float32 sheen) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			m.alpha = alpha < 0.f ? 0.f : (alpha > 1.f ? 1.f : alpha);
			m.aniso = aniso < -1.f ? -1.f : (aniso > 1.f ? 1.f : aniso);
			m.sheenV = sheen < 0.f ? 0.f : (sheen > 1.f ? 1.f : sheen);
			if (NkvpMatEng(i))
				NkvpMatEng(i)
					->SetAlbedo({m.albedo[0], m.albedo[1], m.albedo[2]}, m.alpha)
					->SetAnisotropy(m.aniso)
					->SetSheen(m.sheenV);
		}
		// Famille TOON, en PAQUET de 14 flottants : seuil, adoucissement,
		// ombre RVB, largeur contour, contour RVB, intensite lisere, lisere
		// RVB, durete speculaire — un couple get/set par champ aurait fait
		// quatorze facades qui divergent.
		void Demo3DHostProjMatToon(int32 i, float32 *v14) {
			if (!v14)
				return;
			const bool ok = i >= 0 && i < kNkvpMaxProjMats && nkvpProjMats[i].used;
			const NkVpProjMat &m = nkvpProjMats[ok ? i : 0];
			v14[0] = ok ? m.toonThresh : 0.3f;
			v14[1] = ok ? m.toonSmooth : 0.05f;
			for (int32 k = 0; k < 3; ++k)
				v14[2 + k] = ok ? m.toonShadow[k] : 0.2f;
			v14[5] = ok ? m.outlineW : 2.f;
			for (int32 k = 0; k < 3; ++k)
				v14[6 + k] = ok ? m.outlineCol[k] : 0.f;
			v14[9] = ok ? m.rimI : 0.5f;
			for (int32 k = 0; k < 3; ++k)
				v14[10 + k] = ok ? m.rimCol[k] : 1.f;
			v14[13] = ok ? m.specHard : 32.f;
		}
		void Demo3DHostProjMatSetToon(int32 i, const float32 *v14) {
			if (i < 0 || i >= kNkvpMaxProjMats || !nkvpProjMats[i].used || !v14)
				return;
			NkVpProjMat &m = nkvpProjMats[i];
			m.toonThresh = v14[0];
			m.toonSmooth = v14[1];
			for (int32 k = 0; k < 3; ++k)
				m.toonShadow[k] = v14[2 + k];
			m.outlineW = v14[5];
			for (int32 k = 0; k < 3; ++k)
				m.outlineCol[k] = v14[6 + k];
			m.rimI = v14[9];
			for (int32 k = 0; k < 3; ++k)
				m.rimCol[k] = v14[10 + k];
			m.specHard = v14[13];
			if (NkvpMatEng(i))
				NkvpMatEng(i)
					->SetToonThreshold(m.toonThresh)
					->SetToonSmooth(m.toonSmooth)
					->SetToonShadow({m.toonShadow[0], m.toonShadow[1], m.toonShadow[2]})
					->SetOutline(m.outlineW,
								 {m.outlineCol[0], m.outlineCol[1], m.outlineCol[2]})
					->SetRim(m.rimI, {m.rimCol[0], m.rimCol[1], m.rimCol[2]})
					->SetSpecHardness(m.specHard);
		}
		int32 Demo3DHostProjMatPrevShape(int32 i) {
			return (i >= 0 && i < kNkvpMaxProjMats) ? nkvpProjMats[i].prevShape : 1;
		}
		void Demo3DHostProjMatSetPrevShape(int32 i, int32 shape) {
			if (i >= 0 && i < kNkvpMaxProjMats)
				// 0..6 depuis le 13 aout (ajout du tissu et de la tete). Les valeurs
				// 0 a 4 n'ont PAS bouge : elles sont serialisees sous « apercu » dans
				// les .nkmat et les scenes, et les renumeroter aurait change la forme
				// des materiaux deja enregistres.
				nkvpProjMats[i].prevShape = (int8)(shape < 0 ? 0 : (shape > 6 ? 6 : shape));
		}
		int32 Demo3DHostProjMatMax() {
			return kNkvpMaxProjMats;
		}
		// OUVRIR UN PROJET REMPLACE LA SCENE : sans ce vidage, les materiaux de
		// la session precedente survivraient au milieu de ceux du fichier, et
		// les assignations pointeraient sur des emplacements qui ne veulent
		// plus rien dire. On passe par ProjMatSetMap(« - ») plutot que de
		// remettre `used` a faux : c'est LUI qui detache aussi les textures
		// cote moteur -- un emplacement recycle plus tard aurait sinon herite
		// des cartes de l'ancien materiau.
		void Demo3DHostProjMatClear() {
			for (int32 i = 0; i < kNkvpMaxProjMats; ++i) {
				if (!nkvpProjMats[i].used)
					continue;
				for (int32 c = 0; c < kNkvpMatChanCount; ++c)
					(void)Demo3DHostProjMatSetMap(i, c, "-");
				nkvpProjMats[i].used = false;
			}
			for (int32 n = 0; n < kNkvpMaxNodes; ++n)
				nkvpNodeMatP1[n] = 0;
		}
		// ── APERCU D'UN MATERIAU : rendu ANALYTIQUE CPU ─────────────────────
		// Assez fidele pour JUGER une matiere (diffus + reflet selon rugosite/
		// metallique), pas le vrai pipeline -- un apercu GPU offscreen pourra
		// le remplacer sans changer l'interface. Sept formes, comme Blender :
		// plan, sphere, cube, liquide, cheveux, tissu, tete.
		//
		// L'IMAGE EST RECTANGULAIRE, ET C'EST TOUT LE POINT. Elle etait carree ;
		// elargir le panneau grossissait donc l'objet avec elle. Blender fait
		// l'inverse, et Rihen le veut ainsi (13 aout) : « c'est juste la largeur
		// qui s'agrandit sans modifier la taille des elements, cube, sphere et
		// autre restent intacts ». D'ou la regle tenue partout ici : TOUTE
		// dimension d'objet se mesure sur la HAUTEUR (`h`), jamais sur la largeur.
		// La largeur ne sert qu'au damier, qui s'etend d'autant.
		// `forme` est un PARAMETRE et non plus `m.prevShape` : les vignettes des
		// cartes du navigateur montrent TOUJOURS une sphere (Rihen, 13 aout), alors
		// que le grand apercu suit la forme choisie. Une liste ou chaque entree a
		// une silhouette differente ne se compare plus d'un coup d'oeil -- c'est
		// pourtant tout ce qu'on demande a une vignette.
		static void HostMatPreviewRender(const NkVpProjMat &m, uint8 *px, int32 w, int32 h,
										 int32 forme) {
			const float32 Lx = -0.44f, Ly = 0.74f, Lz = 0.51f; // cle, deja ~normalisee
			const float32 gloss = 1.f - (m.rough < 0.f ? 0.f : (m.rough > 1.f ? 1.f : m.rough));
			const float32 met = m.metal < 0.f ? 0.f : (m.metal > 1.f ? 1.f : m.metal);
			const float32 specE = 2.f + 220.f * gloss * gloss * gloss;
			const int8 shp = (int8)forme;
			auto bgAt = [&](int32 x, int32 y) -> float32 {
				// damier sombre, comme la piece d'apercu de Blender
				return (((x >> 4) ^ (y >> 4)) & 1) ? 0.20f : 0.155f;
			};
			auto shade = [&](float32 nx, float32 ny, float32 nz, float32 *out) {
				float32 nl = nx * Lx + ny * Ly + nz * Lz;
				if (nl < 0.f)
					nl = 0.f;
				// H = normalize(L + V), V = (0,0,1)
				const float32 hx = Lx, hy = Ly, hz = Lz + 1.f;
				const float32 hl = 1.f / math::NkSqrt(hx * hx + hy * hy + hz * hz);
				float32 nh = nx * hx * hl + ny * hy * hl + nz * hz * hl;
				if (nh < 0.f)
					nh = 0.f;
				float32 sp = 1.f;
				for (float32 e = specE; e >= 1.f; e -= 1.f)
					sp *= nh; // pow entiere, suffisante ici
				sp *= 0.10f + 0.9f * gloss;
				float32 fr = 1.f - (nz < 0.f ? 0.f : nz); // rim de fresnel
				fr = fr * fr * fr * (0.10f + 0.35f * gloss);
				const float32 dif = (0.20f + 0.80f * nl) * (1.f - 0.85f * met);
				for (int32 c = 0; c < 3; ++c) {
					const float32 sc = met > 0.f ? (1.f - met) + met * m.albedo[c] : 1.f;
					out[c] = m.albedo[c] * dif + sc * (sp * (0.5f + 1.1f * met) + fr);
				}
			};
			// Centre en largeur, mais TAILLE prise sur la hauteur : c'est ce qui
			// laisse l'objet intact quand l'image s'elargit.
			const float32 cx = 0.5f * (float32)w, cy = 0.48f * (float32)h;
			const float32 R = 0.36f * (float32)h;
			// BASE ORTHONORMEE de l'apercu cube : vD pointe vers la camera (on
			// voit le dessus et deux faces), Rv/Uv encadrent l'ecran.
			const float32 vD[3] = {0.4851f, 0.5821f, 0.6524f};
			float32 Rv[3] = {vD[2], 0.f, -vD[0]};
			{
				const float32 rl = 1.f / math::NkSqrt(Rv[0] * Rv[0] + Rv[2] * Rv[2]);
				Rv[0] *= rl;
				Rv[2] *= rl;
			}
			const float32 Uv[3] = {vD[1] * Rv[2] - vD[2] * Rv[1],
								   vD[2] * Rv[0] - vD[0] * Rv[2],
								   vD[0] * Rv[1] - vD[1] * Rv[0]};
			for (int32 y = 0; y < h; ++y) {
				for (int32 x = 0; x < w; ++x) {
					float32 col[3];
					const float32 bg = bgAt(x, y);
					col[0] = col[1] = col[2] = bg;
					const float32 dx = ((float32)x - cx) / R;
					const float32 dy = (cy - (float32)y) / R; // Y ecran inverse
					if (shp == 0) {
						// PLAN : carte CARREE et CENTREE, mesuree sur la hauteur.
						// Elle occupait toute la largeur ; l'image devenue
						// rectangulaire, elle se serait etiree indefiniment et
						// aurait recouvert le damier au lieu de poser dessus --
						// alors que c'est le damier qui doit s'etendre.
						const float32 demi = 0.375f * (float32)h;
						const float32 ddx = (float32)x - cx, ddy = (float32)y - 0.5f * (float32)h;
						if (ddx > -demi && ddx < demi && ddy > -demi && ddy < demi) {
							const float32 t = ddy / (float32)h;
							const float32 ny2 = 0.30f + 0.25f * t;
							const float32 il = 1.f / math::NkSqrt(ny2 * ny2 + 0.92f * 0.92f);
							shade(0.f, ny2 * il, 0.92f * il, col);
						}
					} else if (shp == 2) {
						// CUBE : intersection orthographique EXACTE rayon/boite.
						// (L'approximation par demi-plans deformait la
						// silhouette -- « on dirait pas un cube », Rihen.)
						const float32 u = dx * 1.55f, w = dy * 1.55f;
						float32 O[3], dir[3];
						for (int32 a3 = 0; a3 < 3; ++a3) {
							O[a3] = u * Rv[a3] + w * Uv[a3] + 4.f * vD[a3];
							dir[a3] = -vD[a3];
						}
						float32 tIn = -1e9f, tOut = 1e9f;
						int32 axIn = 0;
						for (int32 a3 = 0; a3 < 3; ++a3) {
							const float32 inv = 1.f / dir[a3];
							float32 t1 = (-1.f - O[a3]) * inv;
							float32 t2 = (1.f - O[a3]) * inv;
							if (t1 > t2) {
								const float32 tw = t1;
								t1 = t2;
								t2 = tw;
							}
							if (t1 > tIn) {
								tIn = t1;
								axIn = a3;
							}
							if (t2 < tOut)
								tOut = t2;
						}
						if (tIn <= tOut) {
							float32 N3[3] = {0.f, 0.f, 0.f};
							N3[axIn] = dir[axIn] > 0.f ? -1.f : 1.f;
							// normale exprimee dans la base ECRAN : la cle et le
							// fresnel du shade() restent coherents
							shade(N3[0] * Rv[0] + N3[1] * Rv[1] + N3[2] * Rv[2],
								  N3[0] * Uv[0] + N3[1] * Uv[1] + N3[2] * Uv[2],
								  N3[0] * vD[0] + N3[1] * vD[1] + N3[2] * vD[2], col);
						}
					} else if (shp == 4) {
						// CHEVEUX : meches verticales ondulees, reflet en bande.
						// La touffe est CENTREE et large d'une fraction de la
						// hauteur -- elle courait d'un bord a l'autre, ce qui la
						// faisait s'etirer avec le panneau.
						const int32 demiT = (int32)(0.33f * (float32)h);
						const int32 x0 = (int32)cx - demiT, x1 = (int32)cx + demiT;
						if (x >= x0 && x < x1 && y >= h / 10 && y < h - h / 10) {
							const float32 fy = (float32)y / (float32)h;
							const float32 wob = math::NkSin(fy * 9.f + (float32)x * 0.53f) * 1.7f;
							const int32 strand = (int32)((float32)x + wob);
							// pseudo-alea PAR MECHE, stable d'une image a l'autre.
							// Nomme `hsh` et non `h` : `h` est desormais la HAUTEUR
							// de l'image, et la masquer ici serait un piege silencieux.
							const float32 hsh = math::NkSin((float32)strand * 12.9898f) * 43758.545f;
							const float32 rnd = hsh - (float32)(int64)hsh;
							const float32 tone = 0.35f + 0.65f * (rnd < 0.f ? rnd + 1.f : rnd);
							// bande de reflet : nette si lisse, diffuse si rugueux
							const float32 bandY = 0.34f + 0.10f * math::NkSin((float32)strand * 0.31f);
							const float32 sig = 0.02f + 0.16f * (1.f - gloss);
							const float32 dband = (fy - bandY) / sig;
							float32 hl = 1.f - dband * dband;
							if (hl < 0.f)
								hl = 0.f;
							for (int32 c = 0; c < 3; ++c) {
								const float32 sc = met > 0.f ? (1.f - met) + met * m.albedo[c] : 1.f;
								col[c] = m.albedo[c] * tone * (1.f - 0.5f * met) +
										 sc * hl * (0.25f + 0.75f * gloss);
							}
						}
					} else if (shp == 5) {
						// TISSU : une etoffe suspendue, plis verticaux et ourlet
						// ondule. La surface est une hauteur z = f(u,v) dont on
						// derive la normale ANALYTIQUEMENT -- pas de maillage a
						// porter dans un apercu qui tient en quelques lignes.
						const float32 u = dx * 1.30f, v = dy * 1.30f;
						// Silhouette : legerement evasee vers le bas (le tissu
						// tombe), et l'ourlet ondule au lieu d'etre coupe net.
						const float32 demiL = 0.86f - 0.10f * v;
						const float32 basV = -0.86f + 0.10f * math::NkSin(u * 7.3f);
						if (u > -demiL && u < demiL && v < 0.92f && v > basV) {
							const float32 pli = math::NkSin(u * 6.2f + v * 1.1f);
							const float32 att = 1.f - 0.30f * v; // plis plus creuses en bas
							const float32 dfu = 0.34f * 6.2f * math::NkCos(u * 6.2f + v * 1.1f) * att;
							const float32 dfv = 0.34f * (1.1f * math::NkCos(u * 6.2f + v * 1.1f) * att -
														 0.30f * pli);
							const float32 il = 1.f / math::NkSqrt(dfu * dfu + dfv * dfv + 1.f);
							shade(-dfu * il, -dfv * il, il, col);
						}
					} else if (shp == 6) {
						// TETE (« Suzanne ») : union de quatre ellipsoides -- crane,
						// museau, deux oreilles -- en projection orthographique. Pour
						// chaque pixel on garde le point le PLUS PROCHE de la camera,
						// ce qui donne l'union sans avoir a intersecter les surfaces.
						// Ce n'est pas le maillage de Blender et ne pretend pas
						// l'etre : l'apercu sert a juger une MATIERE, il lui faut des
						// courbures variees et une silhouette reconnaissable.
						const float32 u = dx * 1.28f, v = dy * 1.28f;
						struct Ell {
								float32 cx, cy, cz, rx, ry, rz;
						};
						static const Ell kParts[4] = {
							{0.00f, 0.10f, 0.00f, 0.74f, 0.70f, 0.70f},	 // crane
							{0.00f, -0.44f, 0.34f, 0.48f, 0.34f, 0.46f}, // museau
							{-0.80f, 0.16f, -0.10f, 0.24f, 0.32f, 0.20f}, // oreille G
							{0.80f, 0.16f, -0.10f, 0.24f, 0.32f, 0.20f}}; // oreille D
						float32 meilleurZ = -1e9f;
						float32 N3[3] = {0.f, 0.f, 0.f};
						for (int32 e = 0; e < 4; ++e) {
							const Ell &q = kParts[e];
							const float32 a = (u - q.cx) / q.rx, b = (v - q.cy) / q.ry;
							const float32 d2e = a * a + b * b;
							if (d2e > 1.f)
								continue;
							const float32 w = math::NkSqrt(1.f - d2e);
							const float32 z = q.cz + q.rz * w;
							if (z <= meilleurZ)
								continue;
							meilleurZ = z;
							// Normale d'un ellipsoide : le gradient, donc chaque
							// composante divisee par le CARRE de son rayon.
							float32 nx = a / q.rx, ny = b / q.ry, nz = w / q.rz;
							const float32 il = 1.f / math::NkSqrt(nx * nx + ny * ny + nz * nz);
							N3[0] = nx * il;
							N3[1] = ny * il;
							N3[2] = nz * il;
						}
						if (meilleurZ > -1e8f)
							shade(N3[0], N3[1], N3[2], col);
					} else {
						// SPHERE (1) et LIQUIDE (3) : meme geometrie, matiere differente.
						const float32 d2 = dx * dx + dy * dy;
						if (d2 <= 1.f) {
							const float32 nz = math::NkSqrt(1.f - d2);
							shade(dx, dy, nz, col);
							if (shp == 3) {
								// LIQUIDE : le fond TRAVERSE (transmission), le bord
								// s'allume (fresnel) et le reflet reste net.
								float32 fr = 1.f - nz;
								fr = fr * fr;
								const float32 keep = 0.30f + 0.55f * fr;
								for (int32 c = 0; c < 3; ++c)
									col[c] = col[c] * keep + bg * (1.f - keep) +
											 fr * 0.22f;
							}
						} else if (d2 <= 1.10f) {
							// bord adouci
							const float32 a = (1.10f - d2) / 0.10f;
							float32 sc[3];
							const float32 il = 1.f / math::NkSqrt(d2);
							shade(dx * il, dy * il, 0.f, sc);
							for (int32 c = 0; c < 3; ++c)
								col[c] = sc[c] * a + bg * (1.f - a);
						}
					}
					const int32 o = (y * w + x) * 4;
					for (int32 c = 0; c < 3; ++c) {
						float32 v = col[c];
						v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
						px[o + c] = (uint8)(v * 255.f + 0.5f);
					}
					px[o + 3] = 255;
				}
			}
		}
		// Regeneration A LA DEMANDE : l'hote detecte lui-meme qu'un apercu est
		// perime (parametres ou forme changes) ; l'appelant uploade quand vrai.
		bool Demo3DHostProjMatPreviewTake(int32 i, uint8 *rgba, uint32 width, uint32 height) {
			if (i < 0 || i >= kNkvpMaxProjMats || !rgba || !width || !height)
				return false;
			const NkVpProjMat &m = nkvpProjMats[i];
			if (!m.used)
				return false;
			static float32 sSig[kNkvpMaxProjMats] = {};
			// LES DIMENSIONS FONT PARTIE DE LA SIGNATURE. Sans elles, elargir le
			// panneau ne redemandait aucun rendu : l'ancienne image, calculee pour
			// une autre largeur, restait affichee telle quelle.
			// La FORME n'entre plus dans la signature : la vignette est toujours une
			// sphere, changer la forme d'apercu ne doit donc rien reconstruire ici.
			const float32 sig = m.albedo[0] * 1.7f + m.albedo[1] * 2.3f +
								m.albedo[2] * 3.1f + m.rough * 5.3f + m.metal * 7.9f +
								(float32)width * 0.017f + (float32)height * 0.031f + 1.f;
			if (sSig[i] == sig)
				return false;
			sSig[i] = sig;
			// 1 = sphere, toujours : c'est l'icone d'un materiau dans une liste.
			HostMatPreviewRender(m, rgba, (int32)width, (int32)height, 1);
			return true;
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
		static NkMat4f HostRotFromEuler(const float32 *rotDeg); // defini juste apres
		// ── TRANSFORM LOCALE D'UN NOEUD : LE POINT DE PASSAGE UNIQUE ────────
		// Voir la declaration en tete de fichier. Deux formes :
		//   sans base memorisee : T * R * S  -- l'echelle vit dans les axes de
		//     l'objet, il ne cisaille jamais (defaut, choix d'Unreal) ;
		//   avec base memorisee : T * (B S Bt) * R -- l'echelle vit dans le
		//     repere MONDE du geste, ce qui produit le vrai cisaillement.
		static NkMat4f HostEmptyXform(int32 e, bool withGizmo) {
			if (e < 0 || e >= 70)
				return NkMat4f::Identity();
			auto *st = HostSt();
			// ── PENDANT UN GESTE, LE GIZMO FAIT FOI ─────────────────────────
			// Sa matrice composee porte l'echelle DANS LE REPERE du geste ;
			// recomposer ici en multipliant simplement les facteurs ignorait ce
			// repere, si bien que le rendu ne suivait pas et que le resultat
			// n'apparaissait qu'au relachement (constate par Rihen). L'apercu et
			// le resultat viennent desormais de la meme source.
			if (withGizmo && st && st->emptyGizmo.IsDragging() &&
				st->emptyGizmo.IsSelected(e))
				return st->emptyGizmo.ComposedOf(e);
			// ── UN VOISIN SUIT PARTIELLEMENT (edition proportionnelle) ──────
			// LES TROIS transformations sont propagees, pas seulement la
			// translation : ne propager que celle-ci etait une limitation que
			// je m'etais donnee sans raison, et Rihen l'a constate -- le
			// deplacement suivait, la rotation et l'echelle non. Le voisin subit
			// le meme geste attenue, AUTOUR DU PIVOT FIGE : une rangee
			// s'incurve, un groupe s'evase.
			float32 propW = 0.f;
			if (withGizmo && st && nkvpPropEditOn && nkvpPropNodeArmed &&
				st->emptyGizmo.IsDragging() && e >= 0 && e < 70 &&
				!st->emptyGizmo.IsSelected(e))
				propW = HostPropFalloff(nkvpPropDistNode[e], nkvpPropEditRadius,
										nkvpPropEditFalloff);
			NkVec3f gTr{0.f, 0.f, 0.f}, gOs{0.f, 0.f, 0.f};
			NkMat4f gRot = NkMat4f::Identity();
			if (withGizmo && st) {
				gTr = st->emptyGizmo.TranslateOf(e);
				gOs = st->emptyGizmo.ScaleOf(e);
				gRot = st->emptyGizmo.RotationOf(e);
			}
			// LE VOISIN PARCOURT UNE FRACTION DU CHEMIN DES SELECTIONNES.
			// Sa transform propre reste intacte ; c'est la matrice MONDE du
			// geste attenue qui vient se poser devant -- exactement le patron
			// de Apply(i, base), et la MEME source que le commit, pour qu'il
			// n'y ait aucun saut au relachement.
			if (propW > 0.001f && st) {
				const int32 sA0 = st->emptyGizmo.ActiveIndex();
				const NkMat4f W = st->emptyGizmo.ApplyAboutWeighted(sA0 >= 0 ? sA0 : 0,
																   nkvpPropPivot, propW);
				return W * HostEmptyXform(e, false);
			}
			// LA ROTATION VIENT DU QUATERNION, jamais des angles : eux ne sont
			// qu'un affichage, et les relire ici reintroduirait le gimbal lock.
			const NkMat4f R = gRot * HostNodeQuat(e).ToMat4();
			const NkVec3f P{nkvpEmptyPos[e][0] + gTr.x, nkvpEmptyPos[e][1] + gTr.y,
							nkvpEmptyPos[e][2] + gTr.z};
			NkMat4f S = NkMat4f::Scale({nkvpEmptyScl[e][0] * (1.f + gOs.x),
										nkvpEmptyScl[e][1] * (1.f + gOs.y),
										nkvpEmptyScl[e][2] * (1.f + gOs.z)});
			if (nkvpEmptyShear[e]) {
				// B (base -> monde) et sa transposee (monde -> base ; la base
				// est orthonormee, donc transposee = inverse). Convention du
				// moteur : M * {1,0,0} rend mat[0].
				const NkVec3f *B = nkvpEmptySclAx[e];
				NkMat4f Bm = NkMat4f::Identity(), Bt = NkMat4f::Identity();
				for (int32 a = 0; a < 3; ++a) {
					Bm.mat[a][0] = B[a].x;
					Bm.mat[a][1] = B[a].y;
					Bm.mat[a][2] = B[a].z;
					Bt.mat[0][a] = B[a].x;
					Bt.mat[1][a] = B[a].y;
					Bt.mat[2][a] = B[a].z;
				}
				return NkMat4f::Translate(P) * (Bm * S * Bt) * R;
			}
			return NkMat4f::Translate(P) * R * S;
		}
		static NkMat4f HostRotFromEuler(const float32 *rotDeg) {
			const float32 kDeg2Rad = 0.017453292f;
			return NkMat4f::RotationZ(NkAngle::FromRad(rotDeg[2] * kDeg2Rad)) *
				   NkMat4f::RotationY(NkAngle::FromRad(rotDeg[1] * kDeg2Rad)) *
				   NkMat4f::RotationX(NkAngle::FromRad(rotDeg[0] * kDeg2Rad));
		}
		// ── LES TROIS GESTES SUR LA ROTATION D'UN NOEUD ─────────────────────
		// Le QUATERNION fait foi ; les angles ne sont qu'une lecture.
		//   HostNodeQuat  : la rotation, sous sa forme de verite ;
		//   HostSetNodeEuler : poser des angles SAISIS (le cache les garde tels
		//     quels -- taper 190 ne doit pas se relire -170) ;
		//   HostNodeEuler : les angles a AFFICHER (le cache s'il fait foi,
		//     sinon une lecture continue du quaternion).
		static NkQuatf HostNodeQuat(int32 e) {
			HostQuatEnsure();
			return (e >= 0 && e < 70) ? nkvpEmptyQuat[e] : NkQuatf::Identity();
		}
		static void HostSetNodeQuat(int32 e, const NkQuatf &q) {
			HostQuatEnsure();
			if (e < 0 || e >= 70)
				return;
			nkvpEmptyQuat[e] = q.Normalized();
			// La rotation vient d'ailleurs que du panneau : les angles saisis ne
			// decrivent plus forcement cette orientation, on les relira.
			nkvpRotCacheOk[e] = false;
		}
		static void HostSetNodeEuler(int32 e, const float32 *deg) {
			HostQuatEnsure();
			if (e < 0 || e >= 70)
				return;
			const float32 kD2R = 0.017453292f;
			// MEME ORDRE que la convention du projet (Z*Y*X).
			nkvpEmptyQuat[e] = (NkQuatf::RotateZ(NkAngle::FromRad(deg[2] * kD2R)) *
								NkQuatf::RotateY(NkAngle::FromRad(deg[1] * kD2R)) *
								NkQuatf::RotateX(NkAngle::FromRad(deg[0] * kD2R)))
								   .Normalized();
			for (int32 a = 0; a < 3; ++a)
				nkvpEmptyRotDeg[e][a] = deg[a];
			nkvpRotCacheOk[e] = true; // ce sont SES valeurs, on les garde telles quelles
		}
		static void HostNodeEuler(int32 e, float32 *outDeg) {
			HostQuatEnsure();
			if (e < 0 || e >= 70) {
				outDeg[0] = outDeg[1] = outDeg[2] = 0.f;
				return;
			}
			if (!nkvpRotCacheOk[e]) {
				// Lecture CONTINUE : on garde l'ecriture la plus proche de la
				// precedente, sinon les angles sauteraient de 180 degres en
				// pleine rotation.
				NkVec3f p, rDeg, s;
				HostDecomposeNear(nkvpEmptyQuat[e].ToMat4(), nkvpEmptyRotDeg[e], p, rDeg, s);
				nkvpEmptyRotDeg[e][0] = rDeg.x;
				nkvpEmptyRotDeg[e][1] = rDeg.y;
				nkvpEmptyRotDeg[e][2] = rDeg.z;
				nkvpRotCacheOk[e] = true;
			}
			for (int32 a = 0; a < 3; ++a)
				outDeg[a] = nkvpEmptyRotDeg[e][a];
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
		static void HostNodeWorld(Demo3DState *st, int32 n, float32 *pos, NkMat4f &rot,
								  float32 *scl);
		static void HostNodeWorldById(int32 n, float32 *pos, NkMat4f &rot, float32 *scl) {
			pos[0] = pos[1] = pos[2] = 0.f;
			rot = NkMat4f::Identity();
			scl[0] = scl[1] = scl[2] = 1.f;
			auto *st = HostSt();
			if (st)
				HostNodeWorld(st, n, pos, rot, scl);
		}
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
				// premiere frame : pousser les volontes d'affichage dans le
				// moteur (lignes internes/majeures visibles d'entree).
				Demo3DHostSetGridFlags(nkvpGridOn, nkvpMinorOn, nkvpMajorOn, nkvpAxesOn);
				// LE TRIO DE DEPART (Rihen) : un cube, une lumiere ponctuelle et
				// une camera -- comme n'importe quel ajout utilisateur.
				{
					const int32 nCube = HostAllocUser(2);
					if (nCube >= 0)
						nkvpEmptyPos[nCube - kNkvpFirstEmpty][1] = 0.5f;
					const int32 nLit = HostAllocUser(5);
					if (nLit >= 0) {
						renderer::NkLightDesc L0 = Demo3D_LightEffective(st, 1);
						L0.color = {1.f, 1.f, 1.f};
						L0.position = {3.f, 4.f, 2.5f};
						L0.cookieIdx = -1; // couleur pure par defaut
						// Loi PHYSIQUE des la naissance (decision du 10 aout) :
						// la ponctuelle du trio part a sa reference, 1000 W.
						L0.attenuationMode = 1;
						L0.intensity = 1000.f;
						nkvpUserLight[nLit - kNkvpFirstUser] = L0;
						nkvpUserSub[nLit - kNkvpFirstUser] = 1;
						const int32 e0 = nLit - kNkvpFirstEmpty;
						nkvpEmptyPos[e0][0] = 3.f;
						nkvpEmptyPos[e0][1] = 4.f;
						nkvpEmptyPos[e0][2] = 2.5f;
					}
					const int32 nCam = HostAllocUser(4);
					if (nCam >= 0) {
						nkvpUserSub[nCam - kNkvpFirstUser] = 10;
						const int32 e0 = nCam - kNkvpFirstEmpty;
						nkvpEmptyPos[e0][0] = 5.f;
						nkvpEmptyPos[e0][1] = 3.5f;
						nkvpEmptyPos[e0][2] = 5.f;
						nkvpEmptyRotDeg[e0][0] = -24.f;
						nkvpEmptyRotDeg[e0][1] = 45.f;
					}
				}
			} else {
				for (int32 n = 0; n < kNkvpMaxNodes; ++n)
					if (nkvpParentOf[n] < 0)
						HostHierRecurse(st, n);
			}
			// Un SOLEIL trop loin rendait sa poignee ultra-rapide (le pas d'un
			// drag est proportionnel a la PROFONDEUR) : les directionnelles
			// restent a portee.
			for (int32 li3 = 0; li3 < Demo3DState::kNumLights; ++li3)
				if (((int32)st->lights[li3].type & 3) == 0) {
					const NkVec3f p3 = st->lights[li3].position;
					const float32 L3 = p3.Len();
					if (L3 > 12.f)
						st->lights[li3].position = p3 * (12.f / L3);
				}
			for (int32 u3 = 0; u3 < kNkvpMaxUser; ++u3)
				if (nkvpUserKind[u3] == 5 && ((int32)nkvpUserLight[u3].type & 3) == 0) {
					const int32 e3 = 6 + u3;
					const float32 L3 = sqrtf(nkvpEmptyPos[e3][0] * nkvpEmptyPos[e3][0] +
											 nkvpEmptyPos[e3][1] * nkvpEmptyPos[e3][1] +
											 nkvpEmptyPos[e3][2] * nkvpEmptyPos[e3][2]);
					if (L3 > 12.f) {
						const float32 k3 = 12.f / L3;
						nkvpEmptyPos[e3][0] *= k3;
						nkvpEmptyPos[e3][1] *= k3;
						nkvpEmptyPos[e3][2] *= k3;
					}
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
				if ((fresh & 8) && ct2)
					nkvpShortcutBits |= 64; // Ctrl+X : couper (navigateur)
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
		// CHARGEMENT D'UN PROJET : on reprend le cliche de reference SANS rien
		// propager. Le detecteur ci-dessus deduit le mouvement d'un parent de
		// l'ecart avec le cliche precedent ; apres un chargement, cet ecart
		// n'est pas un geste -- c'est toute la scene qui a ete remplacee. Sans
		// ce recalage, la frame suivante traine chaque enfant du deplacement
		// apparent de son parent, alors que le fichier l'avait deja pose.
		void Demo3DHostHierarchyResync() {
			auto *st = HostSt();
			if (!st)
				return;
			HostParentEnsureInit();
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
				nkvpSceneOf[n] = nkvpCurScene; // nait dans le document ACTIF
				nkvpIsMesh[n] = false;
				nkvpIsModel[n] = false;
				nkvpParentOf[n] = -1;
				nkvpXmit[n] = 7;
				nkvpMatMask[n] = 0;
				nkvpObjHidden[n] = false;
				nkvpObjLocked[n] = false;
				nkvpMeshHidden[n] = false;
				nkvpMeshLocked[n] = false;
				nkvpBaseSet[n] = false;
				nkvpUserSub[u] = 0;
				nkvpUserMesh[u] = NkMeshHandle{};
				nkvpUserSeg[u] = 32;
				nkvpUserRing[u] = 16;
				nkvpUserAux[u] = 0.15f;
				nkvpUserCam[u][0] = 50.f;
				nkvpUserCam[u][1] = 0.1f;
				nkvpUserCam[u][2] = 100.f;
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
				nkvpUserSub[n - kNkvpFirstUser] =
					(uint8)((int32)nkvpUserLight[n - kNkvpFirstUser].type & 3);
			}
			// Le double reprend aussi la TAILLE LOCALE surchargee.
			if (nkvpBaseSet[src]) {
				nkvpBaseSet[n] = true;
				for (int32 a = 0; a < 3; ++a) {
					nkvpBaseSize[n][a] = nkvpBaseSize[src][a];
					nkvpDimFactor[n][a] = nkvpDimFactor[src][a];
				}
			}
			nkvpIsMesh[n] = nkvpIsMesh[src]; // un double de mesh reste un mesh
			// Un double de MODEL naitrait vide (ses maillages ne sont pas
			// copies ici) : il redevient donc un objet ordinaire.
			nkvpIsModel[n] = false;
			if (src >= kNkvpFirstUser) {
				const int32 su = src - kNkvpFirstUser;
				const int32 nu = n - kNkvpFirstUser;
				nkvpUserSub[nu] = nkvpUserSub[su];
				nkvpUserMesh[nu] = nkvpUserMesh[su]; // meme geometrie parametree
				nkvpUserSeg[nu] = nkvpUserSeg[su];
				nkvpUserRing[nu] = nkvpUserRing[su];
				nkvpUserAux[nu] = nkvpUserAux[su];
			}
			const int32 pp = nkvpParentOf[src];
			nkvpParentOf[n] = (pp >= 0 && !nkvpDeleted[pp]) ? pp : -1;
			return n;
		}
		int32 Demo3DHostDuplicateNode(int32 node) {
			const float32 off[3] = {0.45f, 0.f, 0.45f};
			return HostSpawnLike(node, off);
		}
		int32 Demo3DHostArchiveNode(int32 node) {
			// ARCHIVE d'asset : copie INVISIBLE qui survit a la suppression de
			// l'original (le navigateur clone depuis elle). deleted=true la
			// sort du rendu et de la hierarchie ; kind!=0 empeche le recyclage
			// du slot par HostAllocUser.
			const float32 off[3] = {0.f, 0.f, 0.f};
			const int32 n = HostSpawnLike(node, off);
			if (n >= 0) {
				nkvpDeleted[n] = true;
				nkvpParentOf[n] = -1;
			}
			return n;
		}
		// MEME REGLE D'APPARTENANCE que Demo3DHostMoveTreeScene, ecrite UNE fois :
		// un model emporte SES MESH INTERNES et eux seuls ; ses enfants de scene
		// sont des models a part entiere et restent ou ils sont. Deux parcours
		// separes auraient fini par ne plus emporter les memes noeuds -- et un
		// mesh oublie en chemin reapparait dans une scene ou personne ne l'a mis.
		static bool HostIsInnerMeshOf(int32 c, int32 root) {
			if (c == root || c < 0 || c >= kNkvpMaxNodes || !nkvpIsMesh[c])
				return false;
			int32 cur = nkvpParentOf[c];
			for (int32 g = 0; g < kNkvpMaxNodes && cur >= 0; ++g) {
				if (cur == root)
					return true;
				if (!nkvpIsMesh[cur])
					return false; // on a quitte la matiere de CE model
				cur = nkvpParentOf[cur];
			}
			return false;
		}
		bool Demo3DHostNodeInnerMeshOf(int32 node, int32 root) {
			// EXPOSE le parcours d'appartenance : l'ecriture d'un fichier de model
			// doit emporter EXACTEMENT les memes noeuds que le deplacement de
			// document et que l'archivage. Le refaire cote projet aurait fait un
			// troisieme parcours, donc une troisieme occasion de diverger.
			return HostIsInnerMeshOf(node, root);
		}
		void Demo3DHostSetNodeArchived(int32 node, bool v) {
			// UN SEUL noeud. Sert a la RELECTURE d'un projet : chaque noeud porte
			// son propre drapeau dans le fichier, il est repose tel quel.
			// On ne passe PAS par Demo3DHostDeleteNode (il remet la nature a zero,
			// donc l'emplacement serait recycle) et on NE TOUCHE PAS a la parente :
			// c'est elle qui tient le model et ses maillages ensemble.
			if (node < 0 || node >= kNkvpMaxNodes)
				return;
			nkvpDeleted[node] = v;
		}
		void Demo3DHostArchiveTree(int32 node, bool v) {
			// LE MODEL ET SA MATIERE ENSEMBLE. L'editeur de Model travaille sur
			// l'asset lui-meme : il le sort de l'archive en entrant et l'y remet en
			// sortant. Ne traiter que la racine laissait ses MAILLAGES vivants dans
			// un document mort -- ils repartaient alors dans la premiere scene a
			// l'enregistrement, visibles en vue 3D et absents de la hierarchie
			// (constate par Rihen, captures du 8 aout).
			if (node < 0 || node >= kNkvpMaxNodes)
				return;
			nkvpDeleted[node] = v;
			for (int32 c = 0; c < kNkvpMaxNodes; ++c)
				if (HostIsInnerMeshOf(c, node))
					nkvpDeleted[c] = v;
		}
		bool Demo3DHostNodeArchived(int32 node) {
			// Une ARCHIVE est retiree de la vue mais garde sa nature ; un noeud
			// vraiment supprime a perdu la sienne. C'est cette difference, et elle
			// seule, qui distingue les deux.
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return false;
			return nkvpDeleted[node] && nkvpUserKind[node - kNkvpFirstUser] != 0;
		}
		void Demo3DHostSetActiveScene(int32 id) {
			nkvpCurScene = (uint8)(id & 0xFF);
		}
		int32 Demo3DHostActiveScene() { return (int32)nkvpCurScene; }
		int32 Demo3DHostNodeScene(int32 node) {
			return (node >= 0 && node < kNkvpMaxNodes) ? (int32)nkvpSceneOf[node]
														: 0;
		}
		void Demo3DHostSetNodeIsMesh(int32 node, bool v) {
			if (node >= 0 && node < kNkvpMaxNodes)
				nkvpIsMesh[node] = v;
		}
		bool Demo3DHostNodeIsMesh(int32 node) {
			return node >= 0 && node < kNkvpMaxNodes && nkvpIsMesh[node];
		}
		bool Demo3DHostNodeIsModel(int32 node) {
			return node >= 0 && node < kNkvpMaxNodes && nkvpIsModel[node];
		}
		// POSE le drapeau tel quel, pour la RELECTURE d'un projet.
		// EnsureModelMesh ne convient pas la : il fabriquerait un maillage
		// interne, alors que celui du fichier est restaure a cote -- le model
		// rouvert se serait retrouve avec un maillage de trop a chaque
		// ouverture.
		void Demo3DHostSetNodeIsModel(int32 node, bool v) {
			if (node >= 0 && node < kNkvpMaxNodes)
				nkvpIsModel[node] = v;
		}
		void Demo3DHostSetDocIsModel(bool v) { nkvpDocIsModel = v; }
		bool Demo3DHostDocIsModel() { return nkvpDocIsModel; }
		// La RACINE du model qui contient ce noeud : on remonte tant que
		// l'ancetre est lui-meme un mesh interne.
		int32 Demo3DHostModelRootOf(int32 node) {
			if (node < 0 || node >= kNkvpMaxNodes)
				return node;
			int32 cur = node;
			for (int32 g = 0; g < kNkvpMaxNodes; ++g) {
				if (!nkvpIsMesh[cur])
					return cur;
				const int32 pa = nkvpParentOf[cur];
				if (pa < 0)
					return cur;
				cur = pa;
			}
			return cur;
		}
		void Demo3DHostSetCameraView(int32 node) {
			nkvpCamViewNode = node;
		}
		// CE QUE LA *VUE* REGARDE -- pas ce que le moteur rend a l'instant.
		// Pendant une sortie, le moteur bascule reellement de camera pour
		// produire chaque etape ; l'interface, elle, lit cette fonction pour
		// peindre le passe-partout, le cadre et le badge. Elle suivait donc ces
		// bascules : regarder par une camera pendant qu'on rend depuis la vue
		// 3D faisait « sauter » l'habillage, comme si la vue camera devenait la
		// vue libre le temps du rendu (constate par Rihen). On rend la valeur
		// figee au demarrage : la vue ne change pas de point de vue, seul le
		// moteur travaille ailleurs.
		int32 Demo3DHostCameraView() {
			return (nkvpOutPhase != 0) ? nkvpOutSaveCam : nkvpCamViewNode;
		}
		bool Demo3DHostCamOrbitLock() { return nkvpCamOrbitLock; }
		void Demo3DHostSetCamOrbitLock(bool on) { nkvpCamOrbitLock = on; }

		// ── VUE CAMERA (pave 0 + selecteur de la vue) ───────────────────────
		// La pose LIBRE est memorisee ICI, cote hote : clavier et selecteur
		// passent par le meme chemin et restent d'accord -- deux memoires (une
		// UI, une hote) auraient fini par restituer des poses differentes.
		namespace {
			float32 sFreePose[6] = {};
			bool sFreePoseOrtho = false;
			bool sFreePoseSaved = false;
		} // namespace
		void Demo3DHostViewCamera(int32 node) {
			if (node < 0) {
				// RETOUR a la vue libre : la pose d'avant est restituee.
				if (nkvpCamViewNode >= 0 && sFreePoseSaved)
					Demo3DHostSetCameraPose(sFreePose, sFreePose[3], sFreePose[4], sFreePose[5],
											sFreePoseOrtho);
				nkvpCamViewNode = -1;
				return;
			}
			if (node >= kNkvpMaxNodes || nkvpDeleted[node])
				return;
			if (nkvpCamViewNode < 0) {
				// On quitte la vue LIBRE : memoriser sa pose UNE fois -- passer
				// d'une camera a l'autre ne doit pas l'ecraser.
				Demo3DHostGetCameraPose(sFreePose, &sFreePose[3], &sFreePose[4], &sFreePose[5],
										&sFreePoseOrtho);
				sFreePoseSaved = true;
			}
			nkvpCamViewNode = node;
			nkvpActiveCamNode = node; // la camera regardee devient l'ACTIVE (facon Blender)
		}
		// Cameras VISIBLES de la scene, dans l'ordre des noeuds.
		static int32 HostSceneCameras(int32 *out, int32 cap) {
			int32 n = 0;
			for (int32 u = 0; u < kNkvpMaxUser && n < cap; ++u) {
				const int32 node = kNkvpFirstUser + u;
				if (nkvpUserKind[u] == 4 && nkvpUserSub[u] == 10 && !nkvpDeleted[node] &&
					!HostHiddenEff(node))
					out[n++] = node;
			}
			return n;
		}
		bool Demo3DHostToggleCameraView() {
			if (nkvpCamViewNode >= 0) {
				Demo3DHostViewCamera(-1);
				return false;
			}
			int32 cams[64];
			const int32 n = HostSceneCameras(cams, 64);
			if (n == 0)
				return false; // pas de camera : le pave 0 ne fait rien
			// L'active est-elle encore une camera valide de la scene ? Sinon la
			// PREMIERE devient la principale par defaut -- sans ca, le premier
			// pave 0 d'une session restait muet (constate par Rihen).
			bool activeOk = false;
			for (int32 i = 0; i < n; ++i)
				if (cams[i] == nkvpActiveCamNode)
					activeOk = true;
			if (!activeOk)
				nkvpActiveCamNode = cams[0];
			Demo3DHostViewCamera(nkvpActiveCamNode);
			return nkvpCamViewNode >= 0;
		}
		int32 Demo3DHostActiveCamera() { return nkvpActiveCamNode; }
		void Demo3DHostSetActiveCamera(int32 node) { nkvpActiveCamNode = node; }

		// ── CADRE CAMERA dans la vue ────────────────────────────────────────
		// UNE SEULE VERITE, a trois clients : le RENDU en vue camera zoome en
		// arriere pour que l'image EXACTE de la camera occupe ce cadre (en
		// retrait, facon Blender) ; le VOILE l'entoure ; la CAPTURE recadre
		// dessus. Sans ce zoom arriere, le voile ne pouvait pas epouser les
		// vrais bords de la camera (constate par Rihen).
		void Demo3DHostCameraFrame(float32 *xywh) {
			// CE CADRE DIT TOUJOURS LA VERITE DE LA VUE. Le forcer en plein
			// cadre pendant un rendu paraissait economique -- un seul point de
			// passage a neutraliser -- mais l'INTERFACE le consomme aussi pour
			// peindre le passe-partout et le lisere : ils disparaissaient donc
			// le temps de la capture et revenaient apres (constate par Rihen).
			// Seul le calcul du CHAMP, cote rendu, sait qu'une sortie occupe
			// toute sa cible ; c'est la que la neutralisation a sa place.
			// LA TAILLE DE LA VUE, PAS CELLE DE LA CIBLE COURANTE. Pendant un
			// rendu de sortie, la cible passe a la resolution demandee (2560 de
			// large, par exemple) : calcule dessus, le cadre couvrait toute la
			// vue et le passe-partout disparaissait le temps de la capture,
			// comme si la vue 3D etait devenue la camera (constate deux fois par
			// Rihen). `wantW/wantH` est ce que l'editeur demande pour la VUE --
			// il reste juste quoi qu'il arrive a la cible.
			const uint32 vwU = hst.wantW > 0 ? hst.wantW : hst.ctx.width;
			const uint32 vhU = hst.wantH > 0 ? hst.wantH : hst.ctx.height;
			const float32 vw = (float32)(vwU > 0 ? vwU : 1);
			const float32 vh = (float32)(vhU > 0 ? vhU : 1);
			const float32 aspect = HostCamAspect();
			float32 fw = vw, fh = vw / aspect;
			if (fh > vh) {
				fh = vh;
				fw = vh * aspect;
			}
			fw *= kCamFrameMargin;
			fh *= kCamFrameMargin;
			xywh[0] = (vw - fw) * 0.5f / vw;
			xywh[1] = (vh - fh) * 0.5f / vh;
			xywh[2] = fw / vw;
			xywh[3] = fh / vh;
		}

		// ── PASSE-PARTOUT de la vue camera (Rihen) : ce qui deborde du cadre
		// de la camera est voile d'une couleur PAR CAMERA, noir a 60 % par
		// defaut, reglable au panneau de la camera.
		namespace {
			float32 sCamPasse[kNkvpMaxUser][4];
			bool sCamPasseSet[kNkvpMaxUser] = {};
		} // namespace
		void Demo3DHostCamPasse(int32 node, float32 *rgba4) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser || !sCamPasseSet[u]) {
				rgba4[0] = rgba4[1] = rgba4[2] = 0.f;
				rgba4[3] = 0.6f;
				return;
			}
			for (int32 i = 0; i < 4; ++i)
				rgba4[i] = sCamPasse[u][i];
		}
		void Demo3DHostSetCamPasse(int32 node, const float32 *rgba4) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser)
				return;
			sCamPasseSet[u] = true;
			for (int32 i = 0; i < 4; ++i)
				sCamPasse[u][i] = rgba4[i];
		}
		bool Demo3DHostCamOrtho(int32 node) {
			const int32 u = node - kNkvpFirstUser;
			return u >= 0 && u < kNkvpMaxUser && nkvpUserCamOrtho[u];
		}
		void Demo3DHostSetCamOrtho(int32 node, bool ortho) {
			const int32 u = node - kNkvpFirstUser;
			if (u >= 0 && u < kNkvpMaxUser)
				nkvpUserCamOrtho[u] = ortho;
		}
		// ECHELLE ORTHO = l'echelle du noeud (regle consignee). Le panneau
		// l'edite comme un champ dedie, facon Blender : lecture sur Y, ecriture
		// UNIFORME (les trois axes) pour garder un cadre non deforme.
		// ── LENS (parite Blender, Rihen) : unite d'affichage de la focale
		// (degres ou millimetres), taille de CAPTEUR pour la conversion, et
		// GUIDES de composition (bits : 1 tiers, 2 centre, 4 diagonales,
		// 8 nombre d'or, 16 zones sures) -- PAR camera. La focale ANGULAIRE
		// (nkvpUserCam[0], degres) reste l'unique verite ; les millimetres ne
		// sont qu'une presentation, convertie par le capteur.
		namespace {
			bool sCamLensMM[kNkvpMaxUser] = {};
			float32 sCamSensor[kNkvpMaxUser] = {};
			uint8 sCamGuides[kNkvpMaxUser] = {};
		} // namespace
		bool Demo3DHostCamLensMM(int32 node) {
			const int32 u = node - kNkvpFirstUser;
			return u >= 0 && u < kNkvpMaxUser && sCamLensMM[u];
		}
		void Demo3DHostSetCamLensMM(int32 node, bool mm) {
			const int32 u = node - kNkvpFirstUser;
			if (u >= 0 && u < kNkvpMaxUser)
				sCamLensMM[u] = mm;
		}
		float32 Demo3DHostCamSensor(int32 node) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser || sCamSensor[u] < 1.f)
				return 36.f; // plein format, le defaut de Blender
			return sCamSensor[u];
		}
		void Demo3DHostSetCamSensor(int32 node, float32 mm) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser)
				return;
			sCamSensor[u] = mm < 1.f ? 1.f : (mm > 200.f ? 200.f : mm);
		}
		float32 Demo3DHostCamFocalMM(int32 node) {
			// focale mm = capteur / (2 tan(fov/2))
			const int32 u = node - kNkvpFirstUser;
			float32 fov = 50.f;
			if (u >= 0 && u < kNkvpMaxUser && nkvpUserCam[u][0] > 1.f)
				fov = nkvpUserCam[u][0];
			const float32 t = math::NkTan(fov * 0.5f * 0.017453292f);
			return Demo3DHostCamSensor(node) / (2.f * (t > 0.001f ? t : 0.001f));
		}
		void Demo3DHostSetCamFocalMM(int32 node, float32 mm) {
			const int32 u = node - kNkvpFirstUser;
			if (u < 0 || u >= kNkvpMaxUser)
				return;
			if (mm < 1.f)
				mm = 1.f;
			float32 fov =
				2.f * math::NkAtan2(Demo3DHostCamSensor(node) / (2.f * mm), 1.f) * 57.29578f;
			if (fov < 5.f)
				fov = 5.f;
			if (fov > 150.f)
				fov = 150.f;
			nkvpUserCam[u][0] = fov;
		}
		int32 Demo3DHostCamGuides(int32 node) {
			const int32 u = node - kNkvpFirstUser;
			return (u >= 0 && u < kNkvpMaxUser) ? (int32)sCamGuides[u] : 0;
		}
		void Demo3DHostSetCamGuides(int32 node, int32 bits) {
			const int32 u = node - kNkvpFirstUser;
			if (u >= 0 && u < kNkvpMaxUser)
				sCamGuides[u] = (uint8)(bits & 0xFF);
		}

		float32 Demo3DHostCamOrthoScale(int32 node) {
			const int32 e = node - kNkvpFirstEmpty;
			if (e < 0 || e >= 70)
				return 1.f;
			const float32 s = nkvpEmptyScl[e][1];
			return s < 0.f ? -s : s;
		}
		void Demo3DHostSetCamOrthoScale(int32 node, float32 s) {
			const int32 e = node - kNkvpFirstEmpty;
			if (e < 0 || e >= 70)
				return;
			if (s < 0.05f)
				s = 0.05f;
			nkvpEmptyScl[e][0] = s;
			nkvpEmptyScl[e][1] = s;
			nkvpEmptyScl[e][2] = s;
		}
		void Demo3DHostMoveTreeScene(int32 node, int32 id) {
			// ISOLATION : le noeud change de document, avec SES MESH INTERNES et
			// EUX SEULS. Ses enfants de SCENE (qui sont des models a part entiere)
			// restent dans la scene : un editeur de model ne contient que le model
			// et sa matiere (regle de Rihen -- emporter les enfants de scene etait
			// justement ce qu'on avait refuse).
			if (node < 0 || node >= kNkvpMaxNodes)
				return;
			nkvpSceneOf[node] = (uint8)(id & 0xFF);
			// Le parcours vit dans HostIsInnerMeshOf : l'archivage d'un asset suit
			// EXACTEMENT le meme, et deux copies auraient divergé.
			for (int32 c = 0; c < kNkvpMaxNodes; ++c)
				if (!nkvpDeleted[c] && HostIsInnerMeshOf(c, node))
					nkvpSceneOf[c] = (uint8)(id & 0xFF);
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
			// LA VERITE DU MESH : nos primitives vont de -0,5 a +0,5, donc UN
			// METRE de cote (cube), de diametre (sphere) ou d'envergure (plan)
			// a l'echelle 1 -- comme Unreal (cube de 100 cm). L'ancien defaut
			// annoncait 2 m : le panneau Dimensions MENTAIT d'un facteur deux
			// sur un objet fraichement cree (constate par Rihen).
			float32 b = 1.f;
			out3[0] = out3[1] = out3[2] = b;
			if (k == 3) { // plan : etendu en X/Z, plat en Y
				out3[0] = out3[2] = (node == 83) ? 80.f : 1.f;
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
		// CREATION d'un noeud utilisateur (menu Ajouter) : 1 sphere, 2 cube,
		// 3 plan, 4 empty, 5 lumiere (sub = type), 6 texte, 7 courbe,
		// 8 surface, 9 metaball. Les natures 6..9 sont des MARQUEURS types en
		// attendant leur backend de geometrie (transformables, parentables).
		// Regenere le mesh PARAMETRIQUE d'un slot utilisateur d'apres ses
		// (segments, anneaux) -- chaque nature mappe ses parametres.
		static void HostRegenUserMesh(int32 u) {
			auto *ms = hst.ctx.renderer ? hst.ctx.renderer->GetMeshSystem() : nullptr;
			if (!ms || u < 0 || u >= kNkvpMaxUser)
				return;
			const uint8 uk = nkvpUserKind[u];
			const uint8 sub = nkvpUserSub[u];
			int32 sg = nkvpUserSeg[u];
			int32 rg = nkvpUserRing[u];
			if (sg < 3)
				sg = 3;
			if (sg > 128)
				sg = 128;
			if (rg < 2)
				rg = 2;
			if (rg > 64)
				rg = 64;
			if (uk == 1) {
				if (sub == 1) {
					int32 sd = nkvpUserSeg[u];
					if (sd < 1)
						sd = 1;
					if (sd > 5)
						sd = 5;
					nkvpUserMesh[u] = ms->CreateIcosphereMesh((uint32)sd);
				} else if (sub == 2) {
					nkvpUserMesh[u] = ms->CreateTorusMesh((uint32)sg, (uint32)rg,
														  nkvpUserAux[u]);
				} else if (sub == 3) {
					nkvpUserMesh[u] = ms->CreateCapsuleMesh((uint32)sg, (uint32)rg);
				} else {
					nkvpUserMesh[u] = ms->CreateSphereMesh((uint32)rg, (uint32)sg);
				}
			} else if (uk == 2 && sub == 1) {
				nkvpUserMesh[u] = ms->CreateCylinderMesh((uint32)sg);
			} else if (uk == 2 && sub == 2) {
				nkvpUserMesh[u] = ms->CreateConeMesh((uint32)sg);
			} else if (uk == 3) {
				int32 dv = nkvpUserSeg[u];
				if (dv < 1)
					dv = 1;
				if (dv > 64)
					dv = 64;
				nkvpUserMesh[u] = ms->CreatePlaneMesh((uint32)dv, (uint32)dv);
			}
		}
		// Descripteur d'une lumiere par NOEUD (demo 86..89 ou utilisateur).
		static renderer::NkLightDesc *HostLightDescOf(int32 node) {
			auto *st = HostSt();
			if (!st)
				return nullptr;
			if (node >= 86 && node < 90)
				return &st->lights[node - 86];
			if (node >= kNkvpFirstUser && node < kNkvpMaxNodes &&
				nkvpUserKind[node - kNkvpFirstUser] == 5)
				return &nkvpUserLight[node - kNkvpFirstUser];
			return nullptr;
		}
		int32 Demo3DHostLightCookie(int32 node) {
			const renderer::NkLightDesc *L = HostLightDescOf(node);
			return L ? L->cookieIdx : -1;
		}
		void Demo3DHostSetLightCookie(int32 node, int32 idx) {
			renderer::NkLightDesc *L = HostLightDescOf(node);
			if (L)
				L->cookieIdx = idx < -1 ? -1 : (idx > 7 ? 7 : idx);
		}
		bool Demo3DHostLightEx(int32 node, float32 *range, float32 *inner, float32 *outer,
							   float32 *aw, float32 *ah, bool *shadow, int32 *type) {
			const renderer::NkLightDesc *L = HostLightDescOf(node);
			if (!L)
				return false;
			*range = L->range;
			*inner = L->innerAngle;
			*outer = L->outerAngle;
			*aw = L->areaWidth;
			*ah = L->areaHeight;
			*shadow = L->castShadow;
			*type = (int32)L->type & 3;
			return true;
		}
		void Demo3DHostSetLightEx(int32 node, float32 range, float32 inner, float32 outer,
								  float32 aw, float32 ah, bool shadow) {
			renderer::NkLightDesc *L = HostLightDescOf(node);
			if (!L)
				return;
			L->range = range < 0.1f ? 0.1f : range;
			L->innerAngle = inner < 1.f ? 1.f : (inner > 89.f ? 89.f : inner);
			L->outerAngle = outer < L->innerAngle ? L->innerAngle : (outer > 90.f ? 90.f : outer);
			L->areaWidth = aw < 0.01f ? 0.01f : aw;
			L->areaHeight = ah < 0.01f ? 0.01f : ah;
			L->castShadow = shadow;
		}
		// ── Loi d'attenuation par lumiere (2026-08-09, decision de Rihen) ────
		// A part de SetLightEx : ses appelants n'ont pas a se prononcer sur la
		// loi a chaque reglage de portee.
		int32 Demo3DHostLightAttMode(int32 node) {
			const renderer::NkLightDesc *L = HostLightDescOf(node);
			return L ? L->attenuationMode : 0;
		}
		void Demo3DHostSetLightAttMode(int32 node, int32 mode) {
			renderer::NkLightDesc *L = HostLightDescOf(node);
			if (L)
				L->attenuationMode = (mode != 0) ? 1 : 0;
		}
		// ── Profondeur d'ombre LINEAIRE (omni, option — 2026-08-10) ──────────
		// A part de SetLightEx pour la meme raison que la loi : c'est un choix
		// qu'on fait une fois, pas un reglage qui accompagne chaque portee.
		bool Demo3DHostLightShadowLinear(int32 node) {
			const renderer::NkLightDesc *L = HostLightDescOf(node);
			return L ? L->pointShadowLinear : false;
		}
		void Demo3DHostSetLightShadowLinear(int32 node, bool lin) {
			renderer::NkLightDesc *L = HostLightDescOf(node);
			if (L)
				L->pointShadowLinear = lin;
		}
		bool Demo3DHostCameraParams(int32 node, float32 *fov, float32 *nearC, float32 *farC) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes ||
				nkvpUserKind[node - kNkvpFirstUser] != 4 ||
				nkvpUserSub[node - kNkvpFirstUser] != 10)
				return false;
			const int32 u = node - kNkvpFirstUser;
			*fov = nkvpUserCam[u][0];
			*nearC = nkvpUserCam[u][1];
			*farC = nkvpUserCam[u][2];
			return true;
		}
		void Demo3DHostSetCameraParams(int32 node, float32 fov, float32 nearC, float32 farC) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return;
			const int32 u = node - kNkvpFirstUser;
			nkvpUserCam[u][0] = fov < 5.f ? 5.f : (fov > 150.f ? 150.f : fov);
			nkvpUserCam[u][1] = nearC < 0.01f ? 0.01f : nearC;
			nkvpUserCam[u][2] = farC < nkvpUserCam[u][1] + 0.1f ? nkvpUserCam[u][1] + 0.1f : farC;
		}
		int32 Demo3DHostUserSub(int32 node) {
			return (node >= kNkvpFirstUser && node < kNkvpMaxNodes)
					   ? (int32)nkvpUserSub[node - kNkvpFirstUser]
					   : 0;
		}
		bool Demo3DHostShadowCfg(float32 *normalBias, float32 *slopeBias, float32 *softness,
								 int32 *quality) {
			// REGLAGES D'OMBRE, lus a chaud dans les shadow maps. Ils sont GLOBAUX
			// au rendu -- une ombre douce l'est pour toute la scene -- d'ou leur
			// place dans les proprietes de RENDU et non sur chaque lumiere, qui ne
			// garde que son interrupteur d'ombre.
			auto *sh = hst.ctx.renderer ? hst.ctx.renderer->GetShadow() : nullptr;
			if (!sh)
				return false;
			const auto &c = sh->GetConfig();
			*normalBias = c.normalBias;
			*slopeBias = c.shadowBias;
			*softness = c.softness;
			*quality = (int32)c.quality;
			return true;
		}
		// ── ECLAIRAGE D'AMBIANCE ────────────────────────────────────────────
		// C'est la lumiere que la scene recoit de son ENVIRONNEMENT, sans aucune
		// source. Elle etait a 0.3 alors qu'aucun environnement n'est charge : la
		// cubemap par defaut etant BLANCHE, tout objet recevait 30 % de blanc sur
		// toutes ses faces -- d'ou un cube gris clair et parfaitement plat meme
		// sans la moindre lumiere (constate par Rihen, qui attendait du noir).
		float32 Demo3DHostAmbient() {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			return r3 ? r3->GetIBLStrength() : 0.f;
		}
		void Demo3DHostSetAmbient(float32 v) {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			if (r3)
				r3->SetIBLStrength(v < 0.f ? 0.f : (v > 2.f ? 2.f : v));
		}
		void Demo3DHostAmbientColor(float32 *rgb) {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			const renderer::NkVec3f c = r3 ? r3->GetIBLColor() : renderer::NkVec3f{1.f, 1.f, 1.f};
			rgb[0] = c.x;
			rgb[1] = c.y;
			rgb[2] = c.z;
		}
		void Demo3DHostSetAmbientColor(const float32 *rgb) {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			if (r3)
				r3->SetIBLColor({rgb[0], rgb[1], rgb[2]});
		}
		// ── L'ENVIRONNEMENT, CONCRETEMENT ───────────────────────────────────
		// C'est ce que la scene « voit » tout autour d'elle, et donc ce qui
		// l'eclaire quand aucune lampe ne le fait. Il prend deux formes :
		//   - un CIEL PROCEDURAL : trois couleurs (zenith, horizon, sol) dont le
		//     moteur deduit une cubemap ;
		//   - une IMAGE HDRI : une photo 360 en virgule flottante, qui apporte a
		//     la fois la lumiere et les reflets d'un lieu reel.
		// Dans les deux cas le moteur en tire une cubemap d'irradiance (la lumiere
		// diffuse recue selon la normale) et une cubemap de reflets. La troisieme
		// option, « couleur unie », n'utilise aucune cubemap : l'ambiance est un
		// aplat, comme le monde par defaut de Blender.
		// ── VALEURS D'ORIGINE ───────────────────────────────────────────────
		// UNE SEULE definition des defauts, qui sert a la fois a l'etat initial
		// et aux boutons « Reinitialiser ». Les ecrire deux fois, c'est se
		// garantir qu'elles divergeront le jour ou l'une des deux bougera -- et
		// le bouton remettrait alors des valeurs qui n'ont jamais ete celles du
		// depart. C'est le meme principe que le point de passage unique.
		static constexpr float32 kSkyTopDef[3] = {0.35f, 0.55f, 0.9f};
		static constexpr float32 kSkyHorDef[3] = {0.8f, 0.85f, 0.9f};
		static constexpr float32 kSkyGndDef[3] = {0.25f, 0.22f, 0.2f};
		static constexpr float32 kSkySunDirDef[3] = {0.35f, -0.65f, 0.35f};
		static constexpr float32 kSkyTurbidityDef = 2.5f;
		// 0.5 par defaut (Rihen, 10 aout), et la valeur est en WATTS — la MEME
		// echelle que le Soleil de scene (5 = plein soleil en loi physique) :
		// une seule unite pour tous les soleils, plus de facteur cache.
		static constexpr float32 kSkySunIntensityDef = 0.5f;
		static constexpr bool kSkySunDiscDef = true;
		static constexpr int32 kSkyModelDef = 0;
		static constexpr float32 kCloudCovDef = 0.5f;
		static constexpr float32 kCloudDenDef = 1.f;
		static constexpr float32 kCloudSclDef = 2.f;
		static constexpr float32 kCloudColDef[3] = {1.f, 1.f, 1.f};
		static constexpr float32 kCloudSpdDef = 0.02f;
		// AMBIANCES de nuages : une TABLE UNIQUE sert a la fois a poser un
		// preset et a reconnaitre lequel est en place -- deux copies des memes
		// valeurs finiraient forcement par diverger et le bouton s'allumerait
		// sur la mauvaise ambiance.
		struct NkCloudPreset {
			float32 cov, den, scl, col[3], spd;
		};
		static constexpr NkCloudPreset kCloudPresets[3] = {
			// DEFAUT : les valeurs d'origine, nuages laisses allumes -- on
			// repose les curseurs, on n'eteint pas la couche.
			{kCloudCovDef, kCloudDenDef, kCloudSclDef,
			 {kCloudColDef[0], kCloudColDef[1], kCloudColDef[2]}, kCloudSpdDef},
			// PLUIE : ciel presque couvert, couche epaisse (le soleil ne perce
			// qu'en lueur), gris-bleu sombre, et ca defile -- un ciel de pluie
			// n'est jamais fige.
			{0.88f, 1.75f, 2.6f, {0.33f, 0.35f, 0.40f}, 0.28f},
			// DESERT : quelques voiles etires et clairsemes, teintes de la
			// poussiere en suspension, presque immobiles dans l'air chaud.
			{0.22f, 0.65f, 5.0f, {0.95f, 0.87f, 0.74f}, 0.05f},
		};
		static constexpr float32 kAmbientDef = 0.05f;
		static constexpr float32 kAmbientColDef[3] = {1.f, 1.f, 1.f};
		static constexpr float32 kSkyBrightnessDef = 1.f;
		static constexpr float32 kSkySunColDef[3] = {1.f, 1.f, 1.f};
		// PUISSANCE DE REFERENCE D'UN SOLEIL dans ce moteur. C'est la meme valeur
		// que kDefIntensity[0] employe quand on bascule une lumiere sur « Soleil » :
		// la reprendre ici garantit qu'un soleil du ciel et un soleil de la scene
		// eclairent pareil a puissance egale, au lieu de deux echelles a comparer
		// a l'oeil.
		static constexpr float32 kSunLightRefIntensity = 3.f;

		// EN ATTENTE DE REGENERATION. Les parametres du ciel ne descendent au
		// moteur qu'a la demande (convolutions CPU). Sans ce drapeau, on tire un
		// curseur, rien ne bouge, et rien ne dit que c'est normal : le reglage
		// passe pour « sans effet ». L'interface s'en sert pour le signaler.
		// Declare ICI, avant le premier setter qui le pose.
		static bool nkvpSkyDirty = false;

		static float32 nkvpSkyTop[3] = {kSkyTopDef[0], kSkyTopDef[1], kSkyTopDef[2]};
		static float32 nkvpSkyHorizon[3] = {kSkyHorDef[0], kSkyHorDef[1], kSkyHorDef[2]};
		static float32 nkvpSkyGround[3] = {kSkyGndDef[0], kSkyGndDef[1], kSkyGndDef[2]};
		static char nkvpHdrPath[256] = {0};
		void Demo3DHostEnvSky(float32 *top, float32 *horizon, float32 *ground) {
			for (int32 i = 0; i < 3; ++i) {
				top[i] = nkvpSkyTop[i];
				horizon[i] = nkvpSkyHorizon[i];
				ground[i] = nkvpSkyGround[i];
			}
		}
		void Demo3DHostSetEnvSky(const float32 *top, const float32 *horizon,
								 const float32 *ground) {
			nkvpSkyDirty = true;
			for (int32 i = 0; i < 3; ++i) {
				nkvpSkyTop[i] = top[i];
				nkvpSkyHorizon[i] = horizon[i];
				nkvpSkyGround[i] = ground[i];
			}
		}
		// La regeneration est un CALCUL CPU (convolutions d'irradiance) : elle se
		// declenche a la demande, jamais a chaque image ni pendant qu'on tire un
		// curseur -- sinon l'interface se figerait sous la main.
		// ── MODELE DE CIEL ET SES PARAMETRES ────────────────────────────────
		// Le degrade a trois couleurs etait le seul ciel possible. Le moteur
		// propose desormais aussi un ciel PHYSIQUE (diffusion atmospherique,
		// disque solaire) et une couche de NUAGES qui se pose sur l'un ou
		// l'autre. Tout est ici a l'etat, et ne descend au moteur qu'a la
		// regeneration -- ce sont des convolutions CPU, pas un reglage a tirer
		// sous le doigt.
		static int32 nkvpSkyModel = kSkyModelDef; // 0 = degrade, 1 = physique
		static float32 nkvpSkySunDir[3] = {kSkySunDirDef[0], kSkySunDirDef[1], kSkySunDirDef[2]};
		static float32 nkvpSkyTurbidity = kSkyTurbidityDef;
		static bool nkvpSkySunDisc = kSkySunDiscDef;
		static float32 nkvpSkySunIntensity = kSkySunIntensityDef;
		static float32 nkvpSkySunColor[3] = {kSkySunColDef[0], kSkySunColDef[1], kSkySunColDef[2]};
		// LE SOLEIL DU CIEL ECLAIRE-T-IL LA SCENE ? En mode « Manuel », le ciel
		// possede son propre soleil : sans cette option il ne serait qu'un decor,
		// et il faudrait creer a cote une directionnelle qu'on devrait garder
		// alignee a la main. Avec elle, ce soleil a TOUS les effets d'une
		// directionnelle -- eclairage et ombres portees.
		//
		// Sans objet quand le ciel SUIT un soleil de la scene : cette lumiere-la
		// existe deja et eclaire deja. En ajouter une seconde doublerait
		// l'eclairement sans que rien ne l'explique.
		static bool nkvpSkySunLights = false;
		static bool nkvpSkyClouds = false;
		static float32 nkvpSkyCloudCoverage = kCloudCovDef;
		static float32 nkvpSkyCloudDensity = kCloudDenDef;
		static float32 nkvpSkyCloudScale = kCloudSclDef;
		static float32 nkvpSkyCloudColor[3] = {kCloudColDef[0], kCloudColDef[1], kCloudColDef[2]};
		// Vitesse de defilement des nuages. Sans objet pour la cuisson (une
		// cubemap est une image fixe) : elle n'a de sens que depuis que le ciel
		// visible est evalue a chaque image dans le shader.
		static float32 nkvpSkyCloudSpeed = kCloudSpdDef;
		// Etoiles : meme nature que la vitesse des nuages — elles n'existent que
		// pour le ciel evalue en temps reel, et ne demandent aucune regeneration.
		static float32 nkvpSkyStarIntensity = 0.f;
		static float32 nkvpSkyStarDensity = 200.f;
		// Rotation celeste (rad/s) et etoiles filantes (apparitions par minute).
		// Les deux n'existent que pour le ciel evalue en temps reel.
		static float32 nkvpSkyStarRotation = 0.f;
		static float32 nkvpSkyShootingRate = 0.f;
		// LUNES : un tableau, pas un cas particulier. Elevation / azimut comme le
		// soleil — c'est ainsi qu'on situe un astre dans le ciel. Leur PHASE ne
		// figure pas ici : elle se deduit du soleil, cote shader.
		static int32 nkvpSkyMoonCount = 0;
		static float32 nkvpSkyMoonElev[2] = {35.f, 20.f};
		static float32 nkvpSkyMoonAzim[2] = {120.f, -60.f};
		static float32 nkvpSkyMoonSize[2] = {0.030f, 0.018f};
		static float32 nkvpSkyMoonBright[2] = {1.f, 0.7f};
		static float32 nkvpSkyMoonColor[2][3] = {{1.f, 0.97f, 0.92f}, {0.92f, 0.94f, 1.f}};
		// PHASE FORCEE, en option. Par defaut elle se deduit du soleil ; la forcer
		// est un choix de MISE EN SCENE, utile pour un plan de film ou l'on veut un
		// croissant precis. C'est assume et declare, pas un etat affiche par accident.
		static bool nkvpSkyMoonManualPhase[2] = {false, false};
		static float32 nkvpSkyMoonPhase[2] = {0.25f, 0.25f};
		// Le ciel visible vient-il de l'HDRI charge ? (source d'ambiance = 2)
		static bool nkvpSkyFromHdr = false;
		// Temperature de l'etoile du modele SOLEIL ALIEN. 5 778 K = la notre.
		static float32 nkvpSkyAlienTempK = 5778.f;

		int32 Demo3DHostSkyModel() {
			return nkvpSkyModel;
		}
		void Demo3DHostSetSkyModel(int32 m) {
			// 0 degrade, 1 Preetham, 2 atmosphere, 3 Hosek, 4 Prague, 5 alien.
			const int32 v = (m < 0) ? 0 : (m > 5 ? 5 : m);
			// NE MARQUER « a regenerer » QUE SI QUELQUE CHOSE CHANGE. L'interface
			// repousse la valeur des qu'elle differe de la derniere poussee, ce
			// qui arrive legitimement apres une remise a zero : sans ce test,
			// l'etoile se rallumait sur un ciel qu'on venait justement de
			// regenerer.
			if (v == nkvpSkyModel)
				return;
			nkvpSkyModel = v;
			nkvpSkyDirty = true;
		}
		void Demo3DHostSkySunColor(float32 *rgb) {
			if (!rgb)
				return;
			rgb[0] = nkvpSkySunColor[0];
			rgb[1] = nkvpSkySunColor[1];
			rgb[2] = nkvpSkySunColor[2];
		}
		void Demo3DHostSetSkySunColor(const float32 *rgb) {
			if (!rgb)
				return;
			if (rgb[0] == nkvpSkySunColor[0] && rgb[1] == nkvpSkySunColor[1] &&
				rgb[2] == nkvpSkySunColor[2])
				return;
			nkvpSkySunColor[0] = rgb[0];
			nkvpSkySunColor[1] = rgb[1];
			nkvpSkySunColor[2] = rgb[2];
			nkvpSkyDirty = true;
		}
		bool Demo3DHostSkySunLightsScene() {
			return nkvpSkySunLights;
		}
		void Demo3DHostSetSkySunLightsScene(bool on) {
			// PAS de marquage « a regenerer » : cette option ne change pas le ciel
			// genere, seulement la lumiere ajoutee a la scene. Son effet est
			// immediat, a la frame suivante.
			nkvpSkySunLights = on;
		}
		void Demo3DHostSkySun(float32 *dir, float32 *turbidity, bool *disc, float32 *intensity) {
			if (dir) {
				dir[0] = nkvpSkySunDir[0];
				dir[1] = nkvpSkySunDir[1];
				dir[2] = nkvpSkySunDir[2];
			}
			if (turbidity)
				*turbidity = nkvpSkyTurbidity;
			if (disc)
				*disc = nkvpSkySunDisc;
			if (intensity)
				*intensity = nkvpSkySunIntensity;
		}
		void Demo3DHostSetSkySun(const float32 *dir, float32 turbidity, bool disc, float32 intensity) {
			nkvpSkyDirty = true;
			if (dir) {
				nkvpSkySunDir[0] = dir[0];
				nkvpSkySunDir[1] = dir[1];
				nkvpSkySunDir[2] = dir[2];
			}
			nkvpSkyTurbidity = turbidity < 1.f ? 1.f : (turbidity > 10.f ? 10.f : turbidity);
			nkvpSkySunDisc = disc;
			nkvpSkySunIntensity = intensity < 0.f ? 0.f : (intensity > 10.f ? 10.f : intensity);
		}
		int32 Demo3DHostSkyMoonCount() {
			return nkvpSkyMoonCount;
		}
		void Demo3DHostSetSkyMoonCount(int32 n) {
			nkvpSkyMoonCount = n < 0 ? 0 : (n > 2 ? 2 : n);
		}
		void Demo3DHostSkyMoon(int32 i, float32 *elev, float32 *azim, float32 *size, float32 *bright,
							   float32 *color) {
			if (i < 0 || i > 1)
				return;
			if (elev)
				*elev = nkvpSkyMoonElev[i];
			if (azim)
				*azim = nkvpSkyMoonAzim[i];
			if (size)
				*size = nkvpSkyMoonSize[i];
			if (bright)
				*bright = nkvpSkyMoonBright[i];
			if (color) {
				color[0] = nkvpSkyMoonColor[i][0];
				color[1] = nkvpSkyMoonColor[i][1];
				color[2] = nkvpSkyMoonColor[i][2];
			}
		}
		void Demo3DHostSetSkyMoon(int32 i, float32 elev, float32 azim, float32 size, float32 bright,
								  const float32 *color) {
			if (i < 0 || i > 1)
				return;
			// Aucun marquage « a regenerer » : une lune n'existe que dans le ciel
			// evalue en temps reel, et sa phase suit le soleil a chaque image.
			nkvpSkyMoonElev[i] = elev;
			nkvpSkyMoonAzim[i] = azim;
			nkvpSkyMoonSize[i] = size < 0.002f ? 0.002f : (size > 0.35f ? 0.35f : size);
			nkvpSkyMoonBright[i] = bright < 0.f ? 0.f : (bright > 8.f ? 8.f : bright);
			if (color) {
				nkvpSkyMoonColor[i][0] = color[0];
				nkvpSkyMoonColor[i][1] = color[1];
				nkvpSkyMoonColor[i][2] = color[2];
			}
		}
		void Demo3DHostSkyMoonPhase(int32 i, bool *manual, float32 *phase) {
			if (i < 0 || i > 1)
				return;
			if (manual)
				*manual = nkvpSkyMoonManualPhase[i];
			if (phase)
				*phase = nkvpSkyMoonPhase[i];
		}
		void Demo3DHostSetSkyMoonPhase(int32 i, bool manual, float32 phase) {
			if (i < 0 || i > 1)
				return;
			nkvpSkyMoonManualPhase[i] = manual;
			nkvpSkyMoonPhase[i] = phase < -1.f ? -1.f : (phase > 1.f ? 1.f : phase);
		}
		void Demo3DHostSkyStars(float32 *intensity, float32 *density) {
			if (intensity)
				*intensity = nkvpSkyStarIntensity;
			if (density)
				*density = nkvpSkyStarDensity;
		}
		void Demo3DHostSkyStarMotion(float32 *rotation, float32 *shooting) {
			if (rotation)
				*rotation = nkvpSkyStarRotation;
			if (shooting)
				*shooting = nkvpSkyShootingRate;
		}
		void Demo3DHostSetSkyStarMotion(float32 rotation, float32 shooting) {
			// Aucune regeneration : ces deux-la ne vivent que dans le ciel evalue
			// en temps reel. Une etoile filante cuite dans une cubemap serait une
			// trainee figee au meme endroit pour toujours.
			nkvpSkyStarRotation = rotation < -1.f ? -1.f : (rotation > 1.f ? 1.f : rotation);
			nkvpSkyShootingRate = shooting < 0.f ? 0.f : (shooting > 120.f ? 120.f : shooting);
		}
		void Demo3DHostSetSkyStars(float32 intensity, float32 density) {
			// PAS de marquage « a regenerer » : les etoiles n'existent que dans le
			// ciel evalue en temps reel. Une cuisson produit une image fixe, et
			// des etoiles cuites ne scintilleraient pas.
			nkvpSkyStarIntensity = intensity < 0.f ? 0.f : (intensity > 4.f ? 4.f : intensity);
			nkvpSkyStarDensity = density < 20.f ? 20.f : (density > 2000.f ? 2000.f : density);
		}
		float32 Demo3DHostSkyAlienTemp() {
			return nkvpSkyAlienTempK;
		}
		void Demo3DHostSetSkyAlienTemp(float32 kelvin) {
			const float32 v = kelvin < 1000.f ? 1000.f : (kelvin > 30000.f ? 30000.f : kelvin);
			if (v == nkvpSkyAlienTempK)
				return;
			nkvpSkyAlienTempK = v;
			nkvpSkyDirty = true; // l'ECLAIRAGE devra etre regenere ; le visible suit seul
		}
		float32 Demo3DHostSkyCloudSpeed() {
			return nkvpSkyCloudSpeed;
		}
		void Demo3DHostSetSkyCloudSpeed(float32 v) {
			// PAS de marquage « a regenerer » : la vitesse n'agit que sur le ciel
			// evalue en temps reel. La cuisson, elle, produit une image fixe --
			// une vitesse n'y a aucun sens.
			nkvpSkyCloudSpeed = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
		}
		void Demo3DHostSkyClouds(bool *on, float32 *coverage, float32 *density, float32 *scale, float32 *color) {
			if (on)
				*on = nkvpSkyClouds;
			if (coverage)
				*coverage = nkvpSkyCloudCoverage;
			if (density)
				*density = nkvpSkyCloudDensity;
			if (scale)
				*scale = nkvpSkyCloudScale;
			if (color) {
				color[0] = nkvpSkyCloudColor[0];
				color[1] = nkvpSkyCloudColor[1];
				color[2] = nkvpSkyCloudColor[2];
			}
		}
		void Demo3DHostSetSkyClouds(bool on, float32 coverage, float32 density, float32 scale,
									const float32 *color) {
			nkvpSkyDirty = true;
			nkvpSkyClouds = on;
			nkvpSkyCloudCoverage = coverage < 0.f ? 0.f : (coverage > 1.f ? 1.f : coverage);
			nkvpSkyCloudDensity = density < 0.f ? 0.f : (density > 2.f ? 2.f : density);
			nkvpSkyCloudScale = scale < 0.1f ? 0.1f : (scale > 20.f ? 20.f : scale);
			if (color) {
				nkvpSkyCloudColor[0] = color[0];
				nkvpSkyCloudColor[1] = color[1];
				nkvpSkyCloudColor[2] = color[2];
			}
		}

		// ── LE CIEL SUIT UN SOLEIL DE LA SCENE ──────────────────────────────
		// Une scene peut porter PLUSIEURS directionnelles : il faut donc CHOISIR
		// laquelle le ciel suit, pas la deviner. On identifie la source par son
		// NOEUD et non par un rang dans une liste : la liste change des qu'on
		// ajoute ou supprime une lumiere, le noeud non -- sans quoi le ciel se
		// mettrait a suivre une autre lampe apres une suppression.
		// -1 = MANUEL : l'elevation et l'azimut du panneau font foi.
		static int32 nkvpSkySunNode = -1;

		// Direction EFFECTIVE (gizmo compris) de la directionnelle d'un noeud.
		// Faux si ce noeud n'est pas une directionnelle vivante.
		static bool HostSunDirOf(int32 node, float32 *dir) {
			auto *st = HostSt();
			if (!st || !dir)
				return false;
			// Lumieres de demo : 86..89.
			if (node >= 86 && node < 86 + Demo3DState::kNumLights) {
				const renderer::NkLightDesc L = Demo3D_LightEffective(st, node - 86);
				if (L.type != renderer::NkLightType::NK_DIRECTIONAL)
					return false;
				dir[0] = L.direction.x;
				dir[1] = L.direction.y;
				dir[2] = L.direction.z;
				return true;
			}
			// Lumieres utilisateur : 96..159, sous-type 0 = soleil.
			if (node >= kNkvpFirstUser && node < kNkvpMaxNodes) {
				const int32 u = node - kNkvpFirstUser;
				if (nkvpUserKind[u] != 5)
					return false;
				if (((int32)nkvpUserLight[u].type & 3) != 0)
					return false;
				// MEME calcul qu'a la soumission, AU QUATERNION PRES : cette
				// fonction recomposait la rotation depuis les ANGLES affiches —
				// or « le quaternion fait foi », les angles ne sont qu'un
				// affichage. Tourner le soleil au gizmo mettait a jour le
				// quaternion, pas ce calcul : le ciel ne voyait jamais la
				// nouvelle direction et le disque restait cloue (constate par
				// Rihen, 10 aout, Source pourtant bien reglee).
				const int32 e = node - kNkvpFirstEmpty;
				const NkMat4f lRm = st->emptyGizmo.RotationOf(e) * HostNodeQuat(e).ToMat4();
				dir[0] = -lRm.mat[1][0];
				dir[1] = -lRm.mat[1][1];
				dir[2] = -lRm.mat[1][2];
				return true;
			}
			return false;
		}

		int32 Demo3DHostSunNodes(int32 *out, int32 maxCount) {
			int32 n = 0;
			float32 tmp[3];
			for (int32 node = 86; node < kNkvpMaxNodes && n < maxCount; ++node) {
				// On saute la plage des empties (90..95) : aucune lumiere n'y vit.
				if (node >= kNkvpFirstEmpty && node < kNkvpFirstUser)
					continue;
				if (HostSunDirOf(node, tmp)) {
					if (out)
						out[n] = node;
					++n;
				}
			}
			return n;
		}
		int32 Demo3DHostSkySunSource() {
			return nkvpSkySunNode;
		}
		void Demo3DHostSetSkySunSource(int32 node) {
			float32 d[3];
			if (node >= 0 && !HostSunDirOf(node, d))
				node = -1; // noeud disparu ou plus directionnel : on revient au manuel
			if (node == nkvpSkySunNode)
				return; // rien de neuf : ne pas rallumer l'etoile pour rien
			nkvpSkySunNode = node;
			nkvpSkyDirty = true;
		}

		// ── LE CIEL VISIBLE, POUSSE A CHAQUE IMAGE ──────────────────────────
		// Depuis que le ciel est evalue DANS LE SHADER, ses parametres ne sont
		// plus une consigne de cuisson mais un etat de rendu : on les envoie a
		// chaque frame, et tout reglage devient immediat. C'est aussi ce qui les
		// rend animables -- il suffira qu'une piste d'animation ecrive dans ces
		// memes variables.
		//
		// La CUISSON (Demo3DHostApplySky) reste, mais elle ne sert plus qu'a
		// l'ECLAIRAGE : irradiance et reflets, les seuls calculs qui la
		// justifient.
		void HostPushSkyToRenderer() {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			if (!r3)
				return;
			// Le ciel suit-il un soleil de la scene ? Sa direction fait alors foi,
			// et elle est relue ICI, a chaque image : c'est ce qui fait que le
			// ciel visible suit la lumiere SANS attendre une regeneration.
			if (nkvpSkySunNode >= 0) {
				float32 d[3];
				if (HostSunDirOf(nkvpSkySunNode, d)) {
					nkvpSkySunDir[0] = d[0];
					nkvpSkySunDir[1] = d[1];
					nkvpSkySunDir[2] = d[2];
				}
			}
			renderer::NkSkyParams sp;
			sp.model = (renderer::NkSkyModel)nkvpSkyModel;
			sp.skyTop = {nkvpSkyTop[0], nkvpSkyTop[1], nkvpSkyTop[2]};
			sp.horizon = {nkvpSkyHorizon[0], nkvpSkyHorizon[1], nkvpSkyHorizon[2]};
			sp.ground = {nkvpSkyGround[0], nkvpSkyGround[1], nkvpSkyGround[2]};
			sp.sunDirection = {nkvpSkySunDir[0], nkvpSkySunDir[1], nkvpSkySunDir[2]};
			sp.turbidity = nkvpSkyTurbidity;
			sp.sunDisc = nkvpSkySunDisc;
			sp.sunIntensity = nkvpSkySunIntensity;
			sp.sunColor = {nkvpSkySunColor[0], nkvpSkySunColor[1], nkvpSkySunColor[2]};
			sp.clouds = nkvpSkyClouds;
			sp.cloudCoverage = nkvpSkyCloudCoverage;
			sp.cloudDensity = nkvpSkyCloudDensity;
			sp.cloudScale = nkvpSkyCloudScale;
			sp.cloudColor = {nkvpSkyCloudColor[0], nkvpSkyCloudColor[1], nkvpSkyCloudColor[2]};
			sp.cloudSpeed = nkvpSkyCloudSpeed;
			sp.starIntensity = nkvpSkyStarIntensity;
			sp.starDensity = nkvpSkyStarDensity;
			sp.starRotation = nkvpSkyStarRotation;
			sp.shootingRate = nkvpSkyShootingRate;
			sp.alienTempK = nkvpSkyAlienTempK;
			// LUNES : elevation/azimut -> direction VERS la lune. Meme conversion
			// que pour le soleil, mais SANS le signe oppose : le soleil est donne
			// par sa direction de PROPAGATION, la lune par l'endroit ou elle SE
			// TROUVE. C'est la seule difference, et elle est volontaire.
			sp.moonCount = nkvpSkyMoonCount;
			for (int32 m = 0; m < renderer::NkSkyParams::kMaxMoons; ++m) {
				const float32 er = nkvpSkyMoonElev[m] * 0.0174532925f;
				const float32 ar = nkvpSkyMoonAzim[m] * 0.0174532925f;
				sp.moons[m].direction = {cosf(er) * sinf(ar), sinf(er), cosf(er) * cosf(ar)};
				sp.moons[m].angularSize = nkvpSkyMoonSize[m];
				sp.moons[m].brightness = nkvpSkyMoonBright[m];
				sp.moons[m].color = {nkvpSkyMoonColor[m][0], nkvpSkyMoonColor[m][1],
									 nkvpSkyMoonColor[m][2]};
				sp.moons[m].manualPhase = nkvpSkyMoonManualPhase[m];
				sp.moons[m].phase = nkvpSkyMoonPhase[m];
			}
			r3->SetSkyParams(sp);
			// HDRI : l'image vient d'un fichier, elle ne se calcule pas. Le shader
			// lit alors la cubemap telle quelle.
			r3->SetSkyFromCubemap(nkvpHdrPath[0] != 0 && nkvpSkyFromHdr);

			// ── PRAGUE EN QUASI TEMPS REEL ──────────────────────────────────
			// Quand un reglage du ciel est en attente et que le modele est
			// Prague, la cubemap VISIBLE est recuite toute seule (~13 ms
			// mesures), CADENCEE a 4 Hz pour ne pas manger la frame : le ciel
			// mesure suit le soleil qu'on tire au gizmo. L'etoile du bouton
			// reste allumee : l'ECLAIRAGE, lui, attend toujours la regeneration
			// complete — c'est lui qui coute.
			if ((nkvpSkyModel == 4 || nkvpSkyModel == 5) && nkvpSkyDirty) {
				static float64 sLastPragueMs = 0.0;
				// Cle des parametres effectivement cuits : le drapeau « en
				// attente » reste allume jusqu'a la regeneration COMPLETE, il ne
				// dit donc pas si quelque chose a change depuis la derniere
				// recuisson visuelle. Sans cette cle, on recuirait une table
				// identique toutes les 250 ms.
				static float32 sLastKey[7] = {1e30f, 0, 0, 0, 0, 0, 0};
				const float32 key[7] = {sp.sunDirection.x, sp.sunDirection.y, sp.sunDirection.z,
										sp.turbidity,	  sp.sunIntensity,	 sp.ground.x,
										sp.alienTempK};
				bool changed = false;
				for (int32 k = 0; k < 7 && !changed; ++k)
					changed = key[k] != sLastKey[k];
				const float64 nowMs = ::nkentseu::NkChrono::Now().nanoseconds / 1.0e6;
				if (changed && nowMs - sLastPragueMs > 250.0) {
					sLastPragueMs = nowMs;
					if (auto *env = hst.ctx.renderer->GetEnvironment()) {
						if (env->RefreshBakedSkyVisual(sp)) {
							for (int32 k = 0; k < 7; ++k)
								sLastKey[k] = key[k];
						}
					}
				}
			}
		}

		bool Demo3DHostApplySky() {
			auto *env = hst.ctx.renderer ? hst.ctx.renderer->GetEnvironment() : nullptr;
			if (!env)
				return false;
			// LE CIEL SUIT-IL UN SOLEIL DE LA SCENE ? Si oui, sa direction fait
			// foi et remplace l'elevation/azimut saisis : c'est tout l'interet du
			// lien. On la recopie AUSSI dans l'etat, pour que le panneau affiche
			// la valeur effective plutot qu'une consigne perimee -- un etat qui se
			// propage doit s'afficher sous sa forme effective.
			if (nkvpSkySunNode >= 0) {
				float32 d[3];
				if (HostSunDirOf(nkvpSkySunNode, d)) {
					nkvpSkySunDir[0] = d[0];
					nkvpSkySunDir[1] = d[1];
					nkvpSkySunDir[2] = d[2];
				} else {
					nkvpSkySunNode = -1; // la source a disparu : retour au manuel
				}
			}
			renderer::NkSkyParams sp;
			sp.model = (renderer::NkSkyModel)nkvpSkyModel;
			sp.skyTop = {nkvpSkyTop[0], nkvpSkyTop[1], nkvpSkyTop[2]};
			sp.horizon = {nkvpSkyHorizon[0], nkvpSkyHorizon[1], nkvpSkyHorizon[2]};
			sp.ground = {nkvpSkyGround[0], nkvpSkyGround[1], nkvpSkyGround[2]};
			sp.sunDirection = {nkvpSkySunDir[0], nkvpSkySunDir[1], nkvpSkySunDir[2]};
			sp.turbidity = nkvpSkyTurbidity;
			sp.sunDisc = nkvpSkySunDisc;
			sp.sunIntensity = nkvpSkySunIntensity;
			sp.sunColor = {nkvpSkySunColor[0], nkvpSkySunColor[1], nkvpSkySunColor[2]};
			sp.clouds = nkvpSkyClouds;
			sp.cloudCoverage = nkvpSkyCloudCoverage;
			sp.cloudDensity = nkvpSkyCloudDensity;
			sp.cloudScale = nkvpSkyCloudScale;
			sp.cloudColor = {nkvpSkyCloudColor[0], nkvpSkyCloudColor[1], nkvpSkyCloudColor[2]};
			env->LoadProceduralEx(sp);
			// On revient a un ciel PROCEDURAL : le shader doit le calculer, plus
			// lire l'image HDRI.
			nkvpSkyFromHdr = false;
			nkvpSkyDirty = false;
			// Les cubemaps viennent d'etre RECREEES : sans ce rafraichissement, les
			// jeux de descripteurs pointent encore sur les anciennes et la
			// regeneration reste invisible.
			if (auto *r3 = hst.ctx.renderer->GetRender3D())
				r3->RefreshEnvironmentBindings();
			return true;
		}
		// LE SOLEIL DU CIEL, VU COMME UNE LUMIERE DE SCENE.
		// Renvoie faux si le ciel ne doit rien eclairer : option decochee, ou
		// bien le ciel SUIT deja une lumiere de la scene -- celle-la eclaire
		// deja, en ajouter une seconde doublerait l'eclairement sans raison.
		//
		// Une directionnelle n'a pas de POSITION utile (le shader ne lit que sa
		// direction) : on n'en fabrique donc pas. C'est la meme regle que
		// NkLightGizmo::CanTranslate applique aux lumieres de la scene.
		bool HostSkySunAsLight(renderer::NkLightDesc &out) {
			if (!nkvpSkySunLights || nkvpSkySunNode >= 0)
				return false;
			out = renderer::NkLightDesc{};
			out.type = renderer::NkLightType::NK_DIRECTIONAL;
			out.direction = {nkvpSkySunDir[0], nkvpSkySunDir[1], nkvpSkySunDir[2]};
			out.color = {nkvpSkySunColor[0], nkvpSkySunColor[1], nkvpSkySunColor[2]};
			// EN WATTS, la MEME echelle que le Soleil de scene en loi physique
			// (5 = plein soleil) — plus le facteur x3 herite : une seule unite
			// pour tous les soleils (Rihen, 10 aout).
			out.intensity = nkvpSkySunIntensity;
			out.attenuationMode = 1;
			out.castShadow = true;
			return true;
		}

		bool Demo3DHostSkyNeedsApply() {
			// SI LE CIEL SUIT UN SOLEIL, bouger ce soleil rend le ciel perime.
			// Le constater ICI plutot que dans la boucle de frame evite de faire
			// porter au rendu un test qui n'interesse que le panneau -- et ce
			// panneau appelle cette fonction a chaque image de toute facon.
			// On ne REGENERE pas tout seul : les convolutions sont un calcul CPU,
			// les declencher a chaque degre de rotation figerait l'interface.
			if (nkvpSkySunNode >= 0) {
				float32 d[3];
				if (HostSunDirOf(nkvpSkySunNode, d)) {
					const float32 e = 1e-4f;
					if (fabsf(d[0] - nkvpSkySunDir[0]) > e || fabsf(d[1] - nkvpSkySunDir[1]) > e ||
						fabsf(d[2] - nkvpSkySunDir[2]) > e)
						nkvpSkyDirty = true;
				}
			}
			return nkvpSkyDirty;
		}

		// -- REMISE A ZERO ---------------------------------------------------
		// Trois portees SEPAREES, parce qu'on ne veut pas perdre son ciel en
		// voulant seulement retrouver l'ambiance d'origine. Chacune repart des
		// constantes kXxxDef declarees plus haut : c'est la MEME source que
		// l'etat initial, donc le bouton ne peut pas remettre une valeur qui
		// n'a jamais ete celle du depart.
		void Demo3DHostResetAmbient() {
			Demo3DHostSetAmbient(kAmbientDef);
			Demo3DHostSetAmbientColor(kAmbientColDef);
			// La luminosite du ciel est posee ICI directement : son accesseur est
			// defini plus bas dans ce fichier, et l'en-tete n'est pas inclus par
			// cette unite de compilation.
			if (auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr)
				r3->SetSkyIntensity(kSkyBrightnessDef);
		}
		void Demo3DHostResetSky() {
			for (int32 i = 0; i < 3; ++i) {
				nkvpSkyTop[i] = kSkyTopDef[i];
				nkvpSkyHorizon[i] = kSkyHorDef[i];
				nkvpSkyGround[i] = kSkyGndDef[i];
				nkvpSkySunDir[i] = kSkySunDirDef[i];
			}
			nkvpSkyModel = kSkyModelDef;
			nkvpSkyTurbidity = kSkyTurbidityDef;
			nkvpSkySunDisc = kSkySunDiscDef;
			nkvpSkySunIntensity = kSkySunIntensityDef;
			// La remise a zero est un geste EXPLICITE : on regenere aussitot,
			// sinon l'utilisateur verrait les champs revenir a leur valeur sans
			// que l'image suive, et douterait de ce qui a ete fait.
			Demo3DHostApplySky();
		}
		void Demo3DHostApplyCloudPreset(int32 which) {
			// Une AMBIANCE est un point de depart, pas un verrou : elle pose les
			// curseurs et l'utilisateur reste libre de les retoucher ensuite.
			if (which < 0 || which > 2)
				return;
			const NkCloudPreset &pr = kCloudPresets[which];
			nkvpSkyClouds = true;
			nkvpSkyCloudCoverage = pr.cov;
			nkvpSkyCloudDensity = pr.den;
			nkvpSkyCloudScale = pr.scl;
			for (int32 i = 0; i < 3; ++i)
				nkvpSkyCloudColor[i] = pr.col[i];
			nkvpSkyCloudSpeed = pr.spd;
			Demo3DHostApplySky();
		}
		int32 Demo3DHostCloudPreset() {
			// L'ambiance EN PLACE se reconnait aux valeurs, pas a un drapeau
			// memorise : des que l'utilisateur retouche un curseur, plus rien ne
			// correspond et aucun bouton ne s'allume -- c'est le bon message.
			if (!nkvpSkyClouds)
				return -1;
			for (int32 w = 0; w < 3; ++w) {
				const NkCloudPreset &pr = kCloudPresets[w];
				if (nkvpSkyCloudCoverage == pr.cov && nkvpSkyCloudDensity == pr.den &&
					nkvpSkyCloudScale == pr.scl && nkvpSkyCloudColor[0] == pr.col[0] &&
					nkvpSkyCloudColor[1] == pr.col[1] && nkvpSkyCloudColor[2] == pr.col[2] &&
					nkvpSkyCloudSpeed == pr.spd)
					return w;
			}
			return -1;
		}
		const char *Demo3DHostHdrPath() {
			return nkvpHdrPath;
		}
		bool Demo3DHostLoadHdr(const char *path) {
			auto *env = hst.ctx.renderer ? hst.ctx.renderer->GetEnvironment() : nullptr;
			if (!env || !path || !path[0])
				return false;
			const bool ok = env->LoadFromHDR(NkString(path));
			if (ok) {
				snprintf(nkvpHdrPath, sizeof(nkvpHdrPath), "%s", path);
				// Le ciel VISIBLE vient desormais de cette image : elle ne se
				// calcule pas, le shader devra lire la cubemap telle quelle.
				nkvpSkyFromHdr = true;
				if (auto *r3 = hst.ctx.renderer->GetRender3D())
					r3->RefreshEnvironmentBindings(); // cubemaps recreees
			}
			return ok;
		}
		bool Demo3DHostAmbientUseEnv() {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			return r3 ? r3->GetIBLUseEnv() : false;
		}
		void Demo3DHostSetAmbientUseEnv(bool on) {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			// Trace de diagnostic du combo Source : si elle manque au journal
			// apres un changement, le poussage UI a echoue.
			logger.Info("[NkDemo3D] Ambiance : source environnement -> {0}\n",
						on ? "oui" : "non");
			if (r3)
				r3->SetIBLUseEnv(on);
		}
		// ── LE CIEL EST-IL VISIBLE EN FOND DE SCENE ? ───────────────────────
		// A NE PAS CONFONDRE avec la SOURCE d'ambiance juste au-dessus : ce
		// sont deux mecanismes separes du moteur, et ils doivent le rester.
		//   SetIBLUseEnv     : le ciel ECLAIRE les objets (ambiance, reflets)
		//   SetSkyboxEnabled : le ciel est VISIBLE derriere eux
		// On veut pouvoir etre eclaire par un HDRI sans l'afficher en fond
		// (c'est le cas courant en rendu produit), et reciproquement afficher
		// un ciel sans qu'il pilote l'ambiance. Les lier serait une facilite
		// qu'on paierait plus tard.
		//
		// Le moteur savait deja tout faire — NkRender3D::SetSkyboxEnabled et le
		// shader Skybox existent et sont compiles au demarrage — mais AUCUNE
		// ligne de l'application ne l'appelait : le ciel ne pouvait donc,
		// par construction, jamais apparaitre (constate par Rihen).
		static bool nkvpSkyVisible = false;
		bool Demo3DHostSkyVisible() {
			return nkvpSkyVisible;
		}
		void Demo3DHostSetSkyVisible(bool on) {
			nkvpSkyVisible = on;
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			if (r3)
				r3->SetSkyboxEnabled(on);
		}
		// LUMINOSITE du ciel affiche — encore un reglage SEPARE de l'intensite
		// d'ambiance. Le shader Skybox multipliait son echantillon par
		// l'intensite d'ambiance (0.05) : le ciel sortait quasi noir et
		// paraissait absent alors qu'il etait bien genere.
		float32 Demo3DHostSkyIntensity() {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			return r3 ? r3->GetSkyIntensity() : 1.f;
		}
		void Demo3DHostSetSkyIntensity(float32 v) {
			auto *r3 = hst.ctx.renderer ? hst.ctx.renderer->GetRender3D() : nullptr;
			if (r3)
				r3->SetSkyIntensity(v < 0.f ? 0.f : (v > 10.f ? 10.f : v));
		}
		// ── BROUILLARD ──────────────────────────────────────────────────────
		// Etat gardé ici : c'est le contexte de scene, reconstruit a chaque
		// image, qui le porte jusqu'au moteur (cf. la soumission).
		void Demo3DHostFog(bool *on, float32 *rgb, float32 *density, float32 *start,
						   float32 *end, int32 *mode) {
			*on = nkvpFogOn;
			rgb[0] = nkvpFogColor[0];
			rgb[1] = nkvpFogColor[1];
			rgb[2] = nkvpFogColor[2];
			*density = nkvpFogDensity;
			*start = nkvpFogStart;
			*end = nkvpFogEnd;
			*mode = nkvpFogMode;
		}
		void Demo3DHostSetFog(bool on, const float32 *rgb, float32 density, float32 start,
							  float32 end, int32 mode) {
			nkvpFogOn = on;
			for (int32 i = 0; i < 3; ++i)
				nkvpFogColor[i] = rgb[i] < 0.f ? 0.f : (rgb[i] > 1.f ? 1.f : rgb[i]);
			nkvpFogDensity = density < 0.f ? 0.f : (density > 1.f ? 1.f : density);
			nkvpFogStart = start < 0.f ? 0.f : start;
			nkvpFogEnd = end < start ? start + 0.001f : end;
			nkvpFogMode = mode & 1;
		}
		// ── OCCLUSION AMBIANTE (SSAO) ───────────────────────────────────────
		// AUCUN etat local : la config du renderer fait deja foi (actif, rayon,
		// intensite y vivent), et un second exemplaire ici aurait diverge au
		// premier correctif — la lecon des tableaux par onglet. SetPostConfig
		// reconstruit le graphe a l'aplomb de la frame suivante quand la passe
		// apparait ou disparait.
		void Demo3DHostSSAO(bool *on, float32 *radius, float32 *intensity) {
			if (!hst.ctx.renderer) {
				if (on)
					*on = false;
				if (radius)
					*radius = 0.5f;
				if (intensity)
					*intensity = 1.f;
				return;
			}
			const auto &pp = hst.ctx.renderer->GetConfig().postProcess;
			if (on)
				*on = pp.ssao;
			if (radius)
				*radius = pp.ssaoRadius;
			if (intensity)
				*intensity = pp.ssaoIntensity;
		}
		void Demo3DHostSetSSAO(bool on, float32 radius, float32 intensity) {
			if (!hst.ctx.renderer)
				return;
			renderer::NkPostConfig pp = hst.ctx.renderer->GetConfig().postProcess;
			pp.ssao = on;
			// Rayon en METRES (v1). Plancher : sous 5 cm le disque ecran tombe
			// sous le texel a demi-resolution et il ne reste que du bruit.
			pp.ssaoRadius = radius < 0.05f ? 0.05f : (radius > 10.f ? 10.f : radius);
			pp.ssaoIntensity = intensity < 0.f ? 0.f : (intensity > 4.f ? 4.f : intensity);
			hst.ctx.renderer->SetPostConfig(pp);
			// La grille voxel OBEIT au meme bouton (une seule notion d'occlusion
			// d'ambiance pour l'utilisateur) : la rebatir a la bascule.
			Demo3DHostGIMarkDirty();
		}
		// ── EXPOSITION & BLOOM (2026-08-09) ─────────────────────────────────
		// Les reglages existaient dans NkPostConfig depuis le debut, aucun
		// panneau ne les proposait — un spot surpuissant faisait un halo geant
		// sans qu'on puisse ni baisser l'exposition ni relever le seuil.
		// Meme regle que la SSAO : la config du renderer fait foi.
		void Demo3DHostPostFx(float32 *exposure, bool *bloomOn, float32 *bloomThr,
							  float32 *bloomStr) {
			const bool ok = hst.ctx.renderer != nullptr;
			const renderer::NkPostConfig pp =
				ok ? hst.ctx.renderer->GetConfig().postProcess : renderer::NkPostConfig{};
			if (exposure)
				*exposure = pp.exposure;
			if (bloomOn)
				*bloomOn = pp.bloom;
			if (bloomThr)
				*bloomThr = pp.bloomThreshold;
			if (bloomStr)
				*bloomStr = pp.bloomStrength;
		}
		void Demo3DHostSetPostFx(float32 exposure, bool bloomOn, float32 bloomThr,
								 float32 bloomStr) {
			if (!hst.ctx.renderer)
				return;
			renderer::NkPostConfig pp = hst.ctx.renderer->GetConfig().postProcess;
			pp.exposure = exposure < 0.01f ? 0.01f : (exposure > 16.f ? 16.f : exposure);
			pp.bloom = bloomOn;
			// Seuil : en HDR il peut (et devrait souvent) depasser 1.0 — seuls
			// les pixels REELLEMENT brillants irradient (LearnOpenGL, Bloom).
			pp.bloomThreshold = bloomThr < 0.f ? 0.f : (bloomThr > 16.f ? 16.f : bloomThr);
			pp.bloomStrength = bloomStr < 0.f ? 0.f : (bloomStr > 8.f ? 8.f : bloomStr);
			hst.ctx.renderer->SetPostConfig(pp);
		}
		// ── NAPPE AU SOL (height fog) et son SOUFFLE ────────────────────────
		void Demo3DHostFogGround(float32 *base, float32 *thickness, float32 *wind,
								 bool *fromClouds) {
			if (base)
				*base = nkvpFogHeightBase;
			if (thickness)
				*thickness = nkvpFogThickness;
			if (wind)
				*wind = nkvpFogWind;
			if (fromClouds)
				*fromClouds = nkvpFogWindFromClouds;
		}
		void Demo3DHostSetFogGround(float32 base, float32 thickness, float32 wind,
									bool fromClouds) {
			nkvpFogHeightBase = base;
			// Epaisseur NEGATIVE n'a pas de sens ; a zero, le brouillard
			// redevient purement fonction de la distance.
			nkvpFogThickness = thickness < 0.f ? 0.f : thickness;
			nkvpFogWind = wind < 0.f ? 0.f : (wind > 1.f ? 1.f : wind);
			nkvpFogWindFromClouds = fromClouds;
		}
		void Demo3DHostFloor(bool *on, float32 *rgb, float32 *y, float32 *rough, int32 *pattern,
							 float32 *tile, float32 *metal) {
			*on = nkvpFloorOn;
			rgb[0] = nkvpFloorColor[0];
			rgb[1] = nkvpFloorColor[1];
			rgb[2] = nkvpFloorColor[2];
			*y = nkvpFloorY;
			*rough = nkvpFloorRough;
			*pattern = nkvpFloorPattern;
			*tile = nkvpFloorTile;
			*metal = nkvpFloorMetal;
		}
		void Demo3DHostSetFloor(bool on, const float32 *rgb, float32 y, float32 rough,
								int32 pattern, float32 tile, float32 metal) {
			nkvpFloorOn = on;
			for (int32 i = 0; i < 3; ++i)
				nkvpFloorColor[i] = rgb[i] < 0.f ? 0.f : (rgb[i] > 1.f ? 1.f : rgb[i]);
			nkvpFloorY = y < -1000.f ? -1000.f : (y > 1000.f ? 1000.f : y);
			nkvpFloorRough = rough < 0.05f ? 0.05f : (rough > 1.f ? 1.f : rough);
			nkvpFloorPattern = pattern < 0 ? 0 : (pattern > 2 ? 2 : pattern);
			nkvpFloorTile = tile < 0.1f ? 0.1f : (tile > 50.f ? 50.f : tile);
			nkvpFloorMetal = metal < 0.f ? 0.f : (metal > 1.f ? 1.f : metal);
		}
		// ── UN GESTE DE GIZMO EST-IL EN COURS ? ─────────────────────────────
		// Lu chaque frame par le shell : le FRONT DESCENDANT (ca glissait, ca ne
		// glisse plus) signifie « un geste vient d'etre commis » et allume la
		// pastille « non enregistre ». C'est le seul point qui VOIT les quatre
		// gizmos -- objet, edition, lumieres, empties -- la ou enumerer les
		// mutations une a une dans le shell en aurait toujours oublie une (le
		// deplacement d'un cube n'allumait PAS la pastille, constate par Rihen).
		bool Demo3DHostAnyGizmoDragging() {
			auto *st = hst.ok ? (Demo3DState *)hst.ctx.userData : nullptr;
			if (!st)
				return false;
			return st->gizmo.IsDragging() || st->editGizmo.IsDragging() ||
				   st->lightGizmo.IsDragging() || st->emptyGizmo.IsDragging();
		}

		// L'AMBIANCE DOIT SUIVRE LA SCENE : les objets utilisateur sont des
		// occludeurs du GI voxel (cf. Demo3D_RebuildGI), donc toute modification
		// -- geste de gizmo termine, ajout, suppression, chargement -- doit
		// invalider la grille. Appele par NkMarkDirty cote shell : la meme action
		// qui allume la pastille « non enregistre » rafraichit l'ambiance, une
		// seule notion de « la scene a change ».
		void Demo3DHostGIMarkDirty() {
			auto *st = hst.ok ? (Demo3DState *)hst.ctx.userData : nullptr;
			if (st)
				st->giDirty = true;
		}

		bool Demo3DHostShadowDynamic() {
			return nkvpShadowDynamic;
		}
		void Demo3DHostSetShadowDynamic(bool dynamic) {
			// Trace de diagnostic (Rihen : « repasser a dynamic, l'ombre ne
			// suivait plus ») : si cette ligne manque au journal apres un
			// changement du combo, c'est le poussage UI qui a echoue, pas le
			// moteur.
			logger.Info("[NkDemo3D] Ombres : mise a jour -> {0}\n",
						dynamic ? "dynamique" : "statique");
			nkvpShadowDynamic = dynamic;
		}
		bool Demo3DHostShadowRecalcPending() {
			// Une ombre figee ne correspond plus a la scene : le bouton
			// « Recalculer l'ombre » se colore (demande de Rihen).
			auto *sh = hst.ctx.renderer ? hst.ctx.renderer->GetShadow() : nullptr;
			return sh && sh->HasPendingRecalc();
		}
		void Demo3DHostShadowRecalc() {
			// Le bouton « Recalculer » du mode statique : une passe complete de
			// re-rendu des ombres, puis le cache refige.
			auto *sh = hst.ctx.renderer ? hst.ctx.renderer->GetShadow() : nullptr;
			if (!sh)
				return;
			sh->InvalidateShadowCache();
			logger.Info("[NkDemo3D] Ombres : recalcul force demande\n");
		}
		void Demo3DHostSetShadowCfg(float32 normalBias, float32 slopeBias, float32 softness,
									int32 quality) {
			auto *sh = hst.ctx.renderer ? hst.ctx.renderer->GetShadow() : nullptr;
			if (!sh)
				return;
			auto &c = sh->GetConfig();
			// Le biais normal se mesure desormais en TEXELS du tile echantillonne
			// (cf. NkShadowTexelWorld) : l'ancien plafond de 1.0 datait de l'echelle
			// en metres et bridait meme la valeur par defaut (1.5). Huit texels est
			// deja enorme -- au-dela, l'ombre se detache visiblement de son objet.
			c.normalBias = normalBias < 0.f ? 0.f : (normalBias > 8.f ? 8.f : normalBias);
			c.shadowBias = slopeBias < 0.f ? 0.f : (slopeBias > 0.05f ? 0.05f : slopeBias);
			c.softness = softness < 0.f ? 0.f : (softness > 0.05f ? 0.05f : softness);
			// 0..4 : l'enum moteur compte CINQ crans (NONE, PCF3, PCF5, POISSON,
			// PCSS). L'ancien plafond a 3 rendait le vrai PCSS inatteignable --
			// le combo « Penombre (PCSS) » reglait en silence POISSON.
			if (quality >= 0 && quality <= 4) {
				if ((int32)c.quality != quality)
					logger.Info("[NkDemo3D] Ombres : qualite -> {0}\n", quality);
				c.quality = (renderer::NkVSMShadowQuality)quality;
			}
		}
		bool Demo3DHostLightTempExp(int32 node, float32 *tempK, float32 *exposure) {
			// Temperature de couleur (kelvins, 0 = desactivee) et exposition (stops).
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes ||
				nkvpUserKind[node - kNkvpFirstUser] != 5)
				return false;
			const renderer::NkLightDesc &L = nkvpUserLight[node - kNkvpFirstUser];
			*tempK = L.temperatureK;
			*exposure = L.exposure;
			return true;
		}
		void Demo3DHostSetLightTempExp(int32 node, float32 tempK, float32 exposure) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes ||
				nkvpUserKind[node - kNkvpFirstUser] != 5)
				return;
			renderer::NkLightDesc &L = nkvpUserLight[node - kNkvpFirstUser];
			L.temperatureK = tempK < 0.f ? 0.f : (tempK > 12000.f ? 12000.f : tempK);
			L.exposure = exposure < -10.f ? -10.f : (exposure > 10.f ? 10.f : exposure);
		}
		void Demo3DHostSetUserSub(int32 node, int32 sub) {
			// CHANGER LE TYPE D'UNE LUMIERE en place (les quatre boutons du
			// panneau, facon Blender) : le sous-type ET le descripteur natif
			// doivent bouger ensemble, sinon le rendu garde l'ancien type.
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return;
			const int32 u = node - kNkvpFirstUser;
			nkvpUserSub[u] = (uint8)(sub & 0xFF);
			if (nkvpUserKind[u] == 5) {
				const int32 t = sub & 3;
				nkvpUserLight[u].type = (decltype(nkvpUserLight[u].type))t;
				// L'INTENSITE N'A PAS LE MEME SENS D'UN TYPE A L'AUTRE. Une
				// ponctuelle rayonne dans toutes les directions et s'attenue avec
				// la distance ; une directionnelle arrive partout avec la meme
				// force. Garder la valeur de l'ancien type en changeant de type
				// donnait un soleil a la puissance d'une ampoule : tout saturait
				// en blanc, au point qu'on ne distinguait plus les faces du cube
				// (constate par Rihen). Chaque type repart donc de SA valeur de
				// reference, comme dans Blender.
				//
				// EN LOI PHYSIQUE (le defaut depuis le 10 aout), la reference
				// est en WATTS : 1000 pour les sources locales, 5 pour le
				// Soleil (decision de Rihen — une directionnelle n'est pas en
				// watts, sa valeur est une irradiance). Loi heritee : les
				// anciennes references, reglees a l'oeil.
				const bool phys = nkvpUserLight[u].attenuationMode == 1;
				static const float32 kDefIntensity[4] = {3.f, 8.f, 8.f, 6.f};
				static const float32 kDefWatts[4] = {5.f, 1000.f, 1000.f, 1000.f};
				nkvpUserLight[u].intensity = phys ? kDefWatts[t] : kDefIntensity[t];
			}
		}
		bool Demo3DHostMeshParams(int32 node, int32 *segs, int32 *rings, float32 *aux) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return false;
			const int32 u = node - kNkvpFirstUser;
			const uint8 uk = nkvpUserKind[u];
			const uint8 sub = nkvpUserSub[u];
			const bool ok =
				uk == 1 || (uk == 2 && (sub == 1 || sub == 2)) || uk == 3 || uk == 10;
			if (!ok)
				return false;
			*segs = nkvpUserSeg[u];
			*rings = nkvpUserRing[u];
			*aux = nkvpUserAux[u];
			return true;
		}
		void Demo3DHostSetMeshParams(int32 node, int32 segs, int32 rings, float32 aux) {
			if (node < kNkvpFirstUser || node >= kNkvpMaxNodes)
				return;
			const int32 u = node - kNkvpFirstUser;
			nkvpUserSeg[u] = segs;
			nkvpUserRing[u] = rings;
			nkvpUserAux[u] = aux;
			HostRegenUserMesh(u);
		}
		int32 Demo3DHostAddNode(int32 kind, int32 sub) {
			auto *st = HostSt();
			if (!st || kind < 1 || kind > 10)
				return -1;
			const int32 n = HostAllocUser((uint8)kind);
			if (n < 0)
				return -1;
			nkvpUserSub[n - kNkvpFirstUser] = (uint8)(sub & 0xFF);
			const int32 e = n - kNkvpFirstEmpty;
			// nait AU CURSEUR 3D : il n'y en a qu'un et il peut etre place
			// n'importe ou (regle de Rihen).
			nkvpEmptyPos[e][0] = st->cursor3D.x;
			nkvpEmptyPos[e][1] = st->cursor3D.y;
			nkvpEmptyPos[e][2] = st->cursor3D.z;
			// Parametres par defaut de la nature, puis mesh PARAMETRIQUE reel.
			{
				const int32 u = n - kNkvpFirstUser;
				// PLAN INFINI (sub 3) : il nait DEJA SUBDIVISE au maximum -- sa
				// raison d'etre est d'etre sculpte plus tard, et un plan de 3 km
				// a 2 triangles n'aurait aucun vertex a deplacer.
				const bool infPlane = kind == 3 && (sub & 0xFF) == 3;
				nkvpUserSeg[u] = kind == 3 ? (infPlane ? 64 : 1)
										   : (kind == 1 && (sub & 0xFF) == 1 ? 3 : 32);
				nkvpUserRing[u] = 16;
				nkvpUserAux[u] = 0.15f;
				if (kind >= 1 && kind <= 3) {
					HostRegenUserMesh(u);
					// TOUT MAILLAGE NAIT AVEC UN MATERIAU (regle de Rihen,
					// comme Blender) : le premier du registre -- cree au
					// besoin -- devient son materiau PRINCIPAL. Les creations
					// suivantes de materiaux ne rebranchent rien ; seule
					// l'assignation (en mode edition) en decidera autrement.
					nkvpNodeMatP1[n] = HostEnsureDefaultMat() + 1;
				}
				if (infPlane) {
					// La MEME emprise que le sol systeme (+-1500 m) : « infini »
					// a l'echelle de la scene, mais un vrai noeud -- l'echelle
					// reste la sienne, modifiable comme sur tout objet. Y reste
					// a 1 : la future sculpture donnera le relief, une echelle
					// verticale de 1500 transformerait le moindre coup de
					// brosse en falaise.
					nkvpEmptyScl[e][0] = 1500.f;
					nkvpEmptyScl[e][1] = 1.f;
					nkvpEmptyScl[e][2] = 1500.f;
				}
			}
			if (kind == 5) {
				// Lumiere NATIVE : partir de la lumiere existante la plus proche
				// du type demande (0 dir, 1 point, 2 spot, 3 surfacique).
				const int32 tpl = sub == 0 ? 0 : (sub == 2 ? 3 : 1);
				renderer::NkLightDesc L = Demo3D_LightEffective(st, tpl);
				L.type = (decltype(L.type))(sub & 3);
				L.direction = {0.f, -1.f, 0.f};
				L.cookieIdx = -1; // COULEUR PURE par defaut (pas de texture heritee)
				// UNE LUMIERE NEUVE NAIT EN LOI PHYSIQUE (decision de Rihen,
				// 10 aout), a la puissance de reference : 1000 W, sauf le
				// Soleil a 5 (sa valeur est une irradiance, pas des watts) —
				// PAS l'intensite du gabarit, reglee pour la loi heritee.
				L.attenuationMode = 1;
				static const float32 kNewWatts[4] = {5.f, 1000.f, 1000.f, 1000.f};
				L.intensity = kNewWatts[sub & 3];
				// BLANCHE a la naissance (Rihen, 10 aout) : le gabarit copiait la
				// teinte de la lumiere de demo la plus proche (rouge, bleue...).
				L.color = {1.f, 1.f, 1.f};
				nkvpUserLight[n - kNkvpFirstUser] = L;
			}
			return n;
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
