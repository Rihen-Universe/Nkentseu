#pragma once
// -----------------------------------------------------------------------------
// @File    NkDemo3DHost.h
// @Brief   Facade OPAQUE de la vue 3D portee de renderdemo --demo=2.
// @License Proprietary - Free to use and modify
//
// MEME REGLE QUE NkViewport3D.h : aucun type NKRenderer ici, car NKRenderer et
// NKCanvas declarent tous deux `renderer::NkBlendMode`/`NkVertex2D` et ne
// peuvent pas se rencontrer dans une unite de compilation. `device` est un
// NkIDevice, `cmd` un NkICommandBuffer, `guiBackend` un NkGuiRHIBackend.
//
// CE QUE C'EST : le pont vers NkDemo3D.cpp, portage INTEGRAL de Demo3D
// (--demo=2 de renderdemo), rendu dans une cible hors ecran publiee sous la
// texture id 4096 — le meme id qu'avant, l'interface n'a pas change.
// La demo garde SES raccourcis et SES gestes (TAB, G/R/S, Z, clic-milieu...) ;
// le cablage des boutons de l'interface vers ses etats viendra ensuite.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace demo {

		// Le device de l'editeur (NkEditorRHIRenderer::GetDevice). A poser une
		// fois avant la premiere frame ; l'initialisation reelle est paresseuse.
		void Demo3DHostSetDevice(void *device);

		// Taille de la VUE (le panneau, pas la fenetre). Ne refait la cible que
		// si la taille change vraiment.
		void Demo3DHostResize(uint32 w, uint32 h);

		// Origine de la vue dans la fenetre (traduction souris), survol, et
		// autorisation d'entree (faux pendant une saisie de texte).
		void Demo3DHostSetView(float32 offX, float32 offY, bool hover, bool inputOn);

		// Rend la frame de la demo dans la cible hors ecran, sur le command
		// buffer de l'editeur (crochet preUI). Calcule son dt lui-meme.
		void Demo3DHostFrame(void *cmd);

		// Publie la cible aupres du backend NKGui sous l'id 4096.
		void Demo3DHostRegisterInto(void *guiBackend);

		// ── CABLAGE DES BOUTONS DE LA VUE ───────────────────────────────────
		// Chaque fonction refait EXACTEMENT ce que fait le raccourci
		// correspondant de la demo (Z, M, pave numerique, G/R/S, virgule,
		// Shift+TAB, F1..F4) — memes ecritures, aucun etat parallele.
		void Demo3DHostSetShading(int32 mode); // 0 rendu, 1 solide, 2 filaire, 3 normales, 4 uv, 5 ao
		int32 Demo3DHostShading();
		void Demo3DHostSetUnlitColor(int32 mode); // 0 materiau, 1 gris, 2 personnalisee
		int32 Demo3DHostUnlitColor();
		void Demo3DHostSetMatcap(int32 id);
		int32 Demo3DHostMatcap();
		int32 Demo3DHostMatcapCount();
		const char *Demo3DHostMatcapName(int32 id);
		void Demo3DHostMatcapBall(int32 id, uint8 *rgba, uint32 size);
		void Demo3DHostSetOrtho(bool on);
		bool Demo3DHostIsOrtho();
		void Demo3DHostAxisView(int32 which, bool opposite); // 0 avant/arriere, 1 droite/gauche, 2 dessus/dessous
		void Demo3DHostResetView();
		void Demo3DHostStoreCamera();
		bool Demo3DHostRecallCamera();
		void Demo3DHostOrbit(float32 dx, float32 dy);
		void Demo3DHostPan(float32 dx, float32 dy);
		void Demo3DHostZoomWheel(float32 notches);
		void Demo3DHostToggleFlyCam();
		bool Demo3DHostIsFlyCam();
		void Demo3DHostSetCamSpeed(float32 mult);
		void Demo3DHostCameraAxes(float32 *rgt, float32 *upv, float32 *fwd);
		void Demo3DHostSetGizmoOp(int32 op); // 0 deplacer, 1 tourner, 2 echelle, 3 combine
		int32 Demo3DHostGizmoOp();
		void Demo3DHostSetOrientation(int32 o); // 0 monde, 1 local, 2 normale
		int32 Demo3DHostOrientation();
		void Demo3DHostSetSnap(bool on, float32 t, float32 rotDeg, float32 scl);
		bool Demo3DHostSnapEnabled();
		void Demo3DHostSetGizmoHidden(bool hidden);
		bool Demo3DHostInEditMode();
		void Demo3DHostSetEditSelMask(int32 mask); // bits 1 sommet, 2 arete, 4 face
		int32 Demo3DHostEditSelMask();
		void Demo3DHostSetZoneTool(int32 shape); // -1 off, 0 rectangle, 1 cercle, 2 lasso
		void Demo3DHostSetCursorTool(bool on);
		void Demo3DHostSetGridFlags(bool grid, bool minor, bool major, bool axes);
		void Demo3DHostGridFlags(bool *grid, bool *minor, bool *major, bool *axes);
		void Demo3DHostSetOutline(bool on);
		bool Demo3DHostOutline();
		void Demo3DHostSetHud(bool on);
		bool Demo3DHostHud();
		void Demo3DHostSetBackground(float32 r, float32 g, float32 b);

		// ── Objets de la scene (hierarchie, panneau Objet) ──────────────────
		int32 Demo3DHostObjectCount();
		void Demo3DHostObjectName(int32 i, char *out, uint32 cap);
		bool Demo3DHostObjectSelected(int32 i);
		int32 Demo3DHostActiveObject();
		void Demo3DHostSelectObject(int32 i, bool additive);
		void Demo3DHostSelectGroup(int32 start, int32 count, bool additive);
		void Demo3DHostSelectAllLights();
		bool Demo3DHostAllLightsSelected();
		void Demo3DHostDeselectAll();
		void Demo3DHostObjectPosition(int32 i, float32 *out3);
		int32 Demo3DHostLightCount();
		void Demo3DHostLightName(int32 li, char *out, uint32 cap);
		int32 Demo3DHostSelectedLight();
		void Demo3DHostSelectLight(int32 li);
		void Demo3DHostLightPosition(int32 li, float32 *out3);
		void Demo3DHostSetLightPosition(int32 li, const float32 *xyz);
		int32 Demo3DHostLightType(int32 li); // 0 dir, 1 point, 2 spot, 3 area
		void Demo3DHostLightDir(int32 li, float32 *out3);
		void Demo3DHostSetLightDir(int32 li, const float32 *xyz);
		void Demo3DHostLightParams(int32 li, float32 *color3, float32 *intensity);
		void Demo3DHostSetLightParams(int32 li, const float32 *color3, float32 intensity);
		void Demo3DHostSetObjectHidden(int32 i, bool hidden);
		bool Demo3DHostObjectHidden(int32 i);
		void Demo3DHostSetObjectLocked(int32 i, bool locked);
		bool Demo3DHostObjectLocked(int32 i);
		// EFFECTIFS : le sien OU celui d'un ancetre. C'est ce que l'interface doit
		// montrer, sinon un refus de selection herite parait inexplicable.
		bool Demo3DHostObjectLockedEff(int32 i);
		bool Demo3DHostObjectHiddenEff(int32 i);
		void Demo3DHostSetLightHidden(int32 li, bool hidden);
		bool Demo3DHostLightHidden(int32 li);
		void Demo3DHostSetAllHidden(bool hidden); // scene VIERGE d'un nouvel onglet
		bool Demo3DHostObjectTransform(int32 i, float32 *pos3, float32 *rotDeg3, float32 *scl3);
		void Demo3DHostApplyDeltaToSelection(const float32 *dPos, const float32 *dRotDeg,
											 const float32 *sclRatio, int32 except);
		void Demo3DHostSetObjectTransform(int32 i, const float32 *pos3, const float32 *rotDeg3,
										  const float32 *scl3);

		// ── PARENTE DE SCENE ────────────────────────────────────────────────
		// TOUT NOEUD peut etre parent ou enfant : 0..85 objets, 86..89
		// lumieres, 90..95 EMPTIES (les anciens groupes, devenus de vrais
		// objets vides). La transformation d'un parent est repercutee a son
		// sous-arbre par l'hote (semantique orbite) ; selectionner un parent
		// ne selectionne PAS ses enfants.
		int32 Demo3DHostNodeCount();		 // 96 (plafond, empties compris)
		int32 Demo3DHostNodeParent(int32 node); // -1 = racine
		bool Demo3DHostSetNodeParent(int32 child, int32 parent); // refuse les cycles
		bool Demo3DHostNodeHasChildren(int32 node);
		// Masque de TRANSMISSION du parent : bit 1 position, bit 2 rotation,
		// bit 4 echelle -- une composante eteinte ne se propage plus.
		int32 Demo3DHostNodeXmitMask(int32 node);
		void Demo3DHostSetNodeXmitMask(int32 node, int32 mask);
		// Selection d'un EMPTY (gizmo dedie dans la vue ; -1 = aucun).
		void Demo3DHostSelectEmptyNode(int32 node);
		int32 Demo3DHostSelectedEmptyNode();
		void Demo3DHostToggleEmptyNode(int32 node); // Maj/Ctrl+clic : multi successif
		bool Demo3DHostEmptyNodeSelected(int32 node);
		// Transform EFFECTIVE d'un empty (node >= 90), drag du gizmo compris.
		bool Demo3DHostEmptyTransform(int32 node, float32 *pos3, float32 *rotDeg3, float32 *scl3);
		void Demo3DHostSetEmptyTransform(int32 node, const float32 *pos3, const float32 *rotDeg3,
										 const float32 *scl3);

		// ── Materiau par objet (panneau Modele) ─────────────────────────────
		// Lecture = valeurs EFFECTIVES vues a la derniere soumission ;
		// ecriture = surcharge persistante (teinte / metallique+rugosite).
		bool Demo3DHostMeshMaterial(int32 i, float32 *tint3, float32 *metallic, float32 *roughness);
		void Demo3DHostSetMeshTint(int32 i, const float32 *rgb3);
		void Demo3DHostSetMeshMetalRough(int32 i, float32 metallic, float32 roughness);
		void Demo3DHostResetMeshMat(int32 i);

		// ── Suppression / duplication / presse-papiers ──────────────────────
		// Les noeuds 96..159 sont les OBJETS UTILISATEUR (crees ici).
		bool Demo3DHostNodeDeleted(int32 node);
		void Demo3DHostDeleteNode(int32 node, bool withChildren);
		int32 Demo3DHostDuplicateNode(int32 node); // -1 si impossible (lumiere v1)
		int32 Demo3DHostArchiveNode(int32 node);   // copie invisible pour asset
		// APPARTENANCE par document : chaque noeud vit dans UNE scene ou UN
		// editeur ; ailleurs il n'est ni rendu ni liste.
		void Demo3DHostSetActiveScene(int32 id);
		int32 Demo3DHostActiveScene();
		int32 Demo3DHostNodeScene(int32 node);
		void Demo3DHostMoveTreeScene(int32 node, int32 id); // isolation
		// VUE CAMERA : la vue montre ce que voit ce noeud camera (-1 = vue 3D).
		// MESH INTERNE d'un model (cree dans un editeur de model) et nature
		// du document courant : ensemble ils font que les mesh se voient dans
		// la hierarchie du model seulement, et qu'un clic en scene selectionne
		// tout le model.
		void Demo3DHostSetNodeIsMesh(int32 node, bool v);
		bool Demo3DHostNodeIsMesh(int32 node);
		// Le CONTENEUR d'un model : seul lui porte ce nom, ses maillages
		// restent des maillages.
		bool Demo3DHostNodeIsModel(int32 node);
		void Demo3DHostSetDocIsModel(bool v);
		bool Demo3DHostDocIsModel();
		int32 Demo3DHostModelRootOf(int32 node);
		// Remet les maillages d'un model A PLAT (tous enfants directs de sa
		// racine) : dans un model, le seul parent est le model.
		void Demo3DHostFlattenModel(int32 root);
		// ORIGINE (pivot) d'un noeud : point autour duquel il tourne et se met a
		// l'echelle, lui et tout ce qu'il porte. La deplacer NE DEPLACE PAS la
		// matiere -- les enfants sont recules d'autant.
		// Compteurs REELS de la geometrie d'un noeud (sommets, aretes, triangles).
		bool Demo3DHostMeshCounts(int32 node, int32 *verts, int32 *edges, int32 *tris);
		bool Demo3DHostNodeOrigin(int32 node, float32 *out3);
		void Demo3DHostSetNodeOrigin(int32 node, const float32 *p3);
		bool Demo3DHostMeshesCenter(int32 node, float32 *out3);
		// Fait du noeud un MODEL et descend sa geometrie propre dans un
		// premier maillage interne. Renvoie ce maillage (-1 si rien a faire).
		int32 Demo3DHostEnsureModelMesh(int32 root);
		void Demo3DHostSetCameraView(int32 node);
		int32 Demo3DHostCameraView();
		// Vue camera facon Blender : ViewCamera(node) regarde cette camera (et la
		// rend ACTIVE ; -1 = retour vue libre, pose restituee). Toggle = pave 0 :
		// bascule vue libre <-> camera active.
		void Demo3DHostViewCamera(int32 node);
		bool Demo3DHostToggleCameraView();
		int32 Demo3DHostActiveCamera();
		void Demo3DHostSetActiveCamera(int32 node);
		// Passe-partout de la vue camera : couleur+opacite PAR camera (defaut
		// noir a 60 %), hors du cadre de la camera dans la vue.
		void Demo3DHostCamPasse(int32 node, float32 *rgba4);
		void Demo3DHostSetCamPasse(int32 node, const float32 *rgba4);
		// Cadre EXACT de l'image camera dans la vue, normalise [0,1] (x,y,w,h).
		// Le rendu en vue camera zoome pour que l'image de la camera occupe
		// precisement ce cadre : voile et capture s'y alignent.
		void Demo3DHostCameraFrame(float32 *xywh);
		// Type de camera : perspective (defaut) ou orthographique. En ortho, la
		// demi-hauteur du cadre = l'echelle Y du noeud (regle consignee).
		bool Demo3DHostCamOrtho(int32 node);
		void Demo3DHostSetCamOrtho(int32 node, bool ortho);
		// Echelle ortho (demi-hauteur du cadre) : lecture/ecriture de l'echelle
		// du noeud, uniforme a l'ecriture.
		float32 Demo3DHostCamOrthoScale(int32 node);
		void Demo3DHostSetCamOrthoScale(int32 node, float32 s);
		// Lens (parite Blender) : unite d'affichage de la focale (deg/mm),
		// capteur (mm) pour la conversion, focale en millimetres derivee, et
		// guides de composition (bits : 1 tiers, 2 centre, 4 diagonales,
		// 8 nombre d'or, 16 zones sures).
		bool Demo3DHostCamLensMM(int32 node);
		void Demo3DHostSetCamLensMM(int32 node, bool mm);
		float32 Demo3DHostCamSensor(int32 node);
		void Demo3DHostSetCamSensor(int32 node, float32 mm);
		float32 Demo3DHostCamFocalMM(int32 node);
		void Demo3DHostSetCamFocalMM(int32 node, float32 mm);
		int32 Demo3DHostCamGuides(int32 node);
		void Demo3DHostSetCamGuides(int32 node, int32 bits);
		void Demo3DHostCopyNode(int32 node);
		int32 Demo3DHostPasteNode();
		int32 Demo3DHostUserKind(int32 node); // 0 aucun, 1 sphere, 2 cube, 3 plan, 4 empty
		// Raccourcis par POLLING : bits 1 dupliquer, 2 copier, 4 coller,
		// 8 supprimer, 16 parenter, 32 deparenter. Consommes a la lecture.
		int32 Demo3DHostTakeShortcuts();
		// Menu AJOUTER : cree un noeud utilisateur (kind 1..9, sub = variante).
		int32 Demo3DHostAddNode(int32 kind, int32 sub);
		// Parametres du mesh cree (segments / anneaux-subdivisions) : le
		// panneau « Ajuster la creation » les edite AVANT validation.
		int32 Demo3DHostUserSub(int32 node); // variante demandee au menu Ajouter
		// Changer le TYPE d'une lumiere en place (sous-type + descripteur natif).
		void Demo3DHostSetUserSub(int32 node, int32 sub);
		// REGLAGES D'OMBRE, globaux au rendu (biais normal, biais de pente,
		// douceur, qualite 0 aucune / 1 PCF3 / 2 PCF5 / 3 PCSS).
		bool Demo3DHostShadowCfg(float32 *normalBias, float32 *slopeBias, float32 *softness,
								 int32 *quality);
		void Demo3DHostSetShadowCfg(float32 normalBias, float32 slopeBias, float32 softness,
									int32 quality);
		// Eclairage d'ambiance : ce que la scene recoit de son environnement,
		// sans aucune source. 0 = noir absolu hors des lumieres.
		float32 Demo3DHostAmbient();
		void Demo3DHostSetAmbient(float32 v);
		// Teinte de l'ambiance (pendant du « World > Color » de Blender).
		void Demo3DHostAmbientColor(float32 *rgb);
		void Demo3DHostSetAmbientColor(const float32 *rgb);
		// Ambiance issue de l'ENVIRONNEMENT (ciel procedural / HDRI) plutot que
		// d'une couleur unie. Faux = aplat parfait, comme le monde de Blender.
		bool Demo3DHostAmbientUseEnv();
		void Demo3DHostSetAmbientUseEnv(bool on);
		// Le ciel VISIBLE en fond de scene — independant de la SOURCE d'ambiance
		// ci-dessus. Deux reglages distincts du moteur : l'un dit si le ciel
		// ECLAIRE, l'autre s'il se VOIT. On veut les deux separement.
		bool Demo3DHostSkyVisible();
		void Demo3DHostSetSkyVisible(bool on);
		// Luminosite du ciel AFFICHE, encore distincte de l'intensite d'ambiance :
		// « combien l'environnement eclaire » n'est pas « a quel point le ciel se
		// voit ». 1 = le ciel tel qu'il a ete genere.
		float32 Demo3DHostSkyIntensity();
		void Demo3DHostSetSkyIntensity(float32 v);
		// ── MODELE DE CIEL ─────────────────────────────────────────────────
		// 0 = degrade a trois couleurs (l'ancien, et le defaut : une scene
		// existante ne doit pas changer d'aspect sans qu'on le demande),
		// 1 = ciel PHYSIQUE (Preetham : diffusion atmospherique + disque
		// solaire, la couleur decoule de la position du soleil et de la
		// turbidite). Les NUAGES sont une couche independante : elle se pose
		// aussi bien sur un degrade stylise que sur un ciel physique.
		// Tous ces reglages ne descendent au moteur qu'a la REGENERATION
		// (Demo3DHostApplySky) : ce sont des convolutions CPU, pas quelque
		// chose qu'on recalcule sous le doigt qui tire un curseur.
		int32 Demo3DHostSkyModel();
		void Demo3DHostSetSkyModel(int32 m);
		void Demo3DHostSkySun(float32 *dir, float32 *turbidity, bool *disc, float32 *intensity);
		// Teinte du soleil du ciel (s'applique a son DISQUE, et a la lumiere
		// qu'il projette quand il eclaire la scene). Blanc = neutre.
		void Demo3DHostSkySunColor(float32 *rgb);
		void Demo3DHostSetSkySunColor(const float32 *rgb);
		// LE SOLEIL DU CIEL ECLAIRE-T-IL LA SCENE ? En mode « Manuel » le ciel a
		// son propre soleil ; cette option lui donne TOUS les effets d'une
		// directionnelle — eclairage et ombres portees — au lieu d'un decor
		// qu'il faudrait doubler d'une lampe gardee alignee a la main.
		// Sans objet quand le ciel SUIT une lumiere : elle eclaire deja.
		bool Demo3DHostSkySunLightsScene();
		void Demo3DHostSetSkySunLightsScene(bool on);
		void Demo3DHostSetSkySun(const float32 *dir, float32 turbidity, bool disc, float32 intensity);
		// VITESSE DE DEFILEMENT des nuages. N'agit que sur le ciel evalue en
		// temps reel dans le shader — une cuisson produit une image fixe, une
		// vitesse n'y aurait aucun sens. Ne marque donc pas « a regenerer ».
		// SOLEIL ALIEN : temperature de surface de l'etoile (Kelvin). 5 778 = la
		// notre ; 3 000 = naine rouge ; 15 000 = etoile bleue. La teinte du
		// MONDE entier en decoule — pas seulement le disque.
		float32 Demo3DHostSkyAlienTemp();
		void Demo3DHostSetSkyAlienTemp(float32 kelvin);
		float32 Demo3DHostSkyCloudSpeed();
		void Demo3DHostSetSkyCloudSpeed(float32 v);
		// ETOILES. Comme la vitesse des nuages, elles n'ont de sens que pour le
		// ciel evalue en temps reel — des etoiles cuites ne scintilleraient pas.
		// Elles s'effacent SEULES quand le ciel s'eclaire : un cycle jour/nuit les
		// fera apparaitre et disparaitre sans qu'on ait a les piloter.
		void Demo3DHostSkyStars(float32 *intensity, float32 *density);
		void Demo3DHostSetSkyStars(float32 intensity, float32 density);
		// MOUVEMENT du ciel etoile : rotation celeste (rad/s) et etoiles filantes
		// (apparitions par minute). Les filantes sont TIREES DU TEMPS et non d'un
		// generateur aleatoire — une meme seconde redonne toujours la meme, donc
		// une capture se rejoue a l'identique et un rendu par images se recolle.
		void Demo3DHostSkyStarMotion(float32 *rotation, float32 *shooting);
		void Demo3DHostSetSkyStarMotion(float32 rotation, float32 shooting);
		// LUNES (0 a 2). Elevation / azimut, comme le soleil — c'est ainsi qu'on
		// situe un astre. Leur PHASE ne figure pas ici : elle se DEDUIT de la
		// position du soleil, cote shader. Un curseur de phase aurait permis
		// d'afficher un croissant sans rapport avec l'eclairage de la scene.
		int32 Demo3DHostSkyMoonCount();
		void Demo3DHostSetSkyMoonCount(int32 n);
		void Demo3DHostSkyMoon(int32 i, float32 *elev, float32 *azim, float32 *size, float32 *bright,
							   float32 *color);
		void Demo3DHostSetSkyMoon(int32 i, float32 elev, float32 azim, float32 size, float32 bright,
								  const float32 *color);
		// PHASE FORCEE, en option. Par defaut elle se deduit du soleil et reste
		// donc coherente avec l'eclairage. La forcer est un choix de MISE EN
		// SCENE — legitime pour un plan de film — et il est declare, pas subi.
		// phase : -1 nouvelle a gauche, 0 pleine, +1 nouvelle a droite.
		void Demo3DHostSkyMoonPhase(int32 i, bool *manual, float32 *phase);
		void Demo3DHostSetSkyMoonPhase(int32 i, bool manual, float32 phase);
		void Demo3DHostSkyClouds(bool *on, float32 *coverage, float32 *density, float32 *scale, float32 *color);
		void Demo3DHostSetSkyClouds(bool on, float32 coverage, float32 density, float32 scale,
									const float32 *color);
		// Vrai si un reglage du ciel attend une regeneration. Sans ce retour,
		// on tire un curseur, rien ne bouge, et rien ne dit que c'est normal :
		// le reglage passe pour « sans effet ».
		bool Demo3DHostSkyNeedsApply();
		// ── LE CIEL SUIT UN SOLEIL DE LA SCENE ─────────────────────────────
		// Une scene peut porter PLUSIEURS directionnelles : on CHOISIT laquelle
		// le ciel suit. La source est designee par son NOEUD, pas par un rang
		// dans une liste — sinon le ciel changerait de soleil des qu'on en
		// supprime un autre. -1 = manuel (elevation/azimut du panneau).
		// Demo3DHostSunNodes remplit `out` avec les noeuds des directionnelles
		// vivantes et renvoie leur nombre ; passer out=nullptr pour compter.
		int32 Demo3DHostSunNodes(int32 *out, int32 maxCount);
		int32 Demo3DHostSkySunSource();
		void Demo3DHostSetSkySunSource(int32 node);
		// REMISE A ZERO, en trois portees separees : on ne veut pas perdre son
		// ciel en cherchant seulement a retrouver l'ambiance d'origine. Les
		// valeurs viennent des memes constantes que l'etat initial.
		void Demo3DHostResetAmbient();
		void Demo3DHostResetSky();
		void Demo3DHostResetClouds();
		// Le CIEL PROCEDURAL : trois couleurs (zenith, horizon, sol) dont le
		// moteur deduit l'irradiance et les reflets. La regeneration est un calcul
		// CPU, d'ou un declenchement a la demande.
		void Demo3DHostEnvSky(float32 *top, float32 *horizon, float32 *ground);
		void Demo3DHostSetEnvSky(const float32 *top, const float32 *horizon,
								 const float32 *ground);
		bool Demo3DHostApplySky();
		// Une IMAGE HDRI equirectangulaire : elle apporte lumiere ET reflets d'un
		// lieu reel.
		const char *Demo3DHostHdrPath();
		bool Demo3DHostLoadHdr(const char *path);
		// Brouillard de scene : loi lineaire (debut/fin) ou exponentielle (densite).
		void Demo3DHostFog(bool *on, float32 *rgb, float32 *density, float32 *start,
						   float32 *end, int32 *mode);
		void Demo3DHostSetFog(bool on, const float32 *rgb, float32 density, float32 start,
							  float32 end, int32 mode);
		// Sol infini (option) : plan de sol recepteur d'ombres, couleur /
		// hauteur / rugosite -- distinct de la grille. Motif : 0 uni,
		// 1 damier, 2 carreaux a joints ; taille du carreau en metres.
		void Demo3DHostFloor(bool *on, float32 *rgb, float32 *y, float32 *rough, int32 *pattern,
							 float32 *tile, float32 *metal);
		void Demo3DHostSetFloor(bool on, const float32 *rgb, float32 y, float32 rough,
								int32 pattern, float32 tile, float32 metal);
		// Mise a jour des ombres : dynamique (elles suivent la scene) ou statique
		// (calculees une fois, puis gardees telles quelles).
		bool Demo3DHostShadowDynamic();
		void Demo3DHostSetShadowDynamic(bool dynamic);
		// Recalcul force des ombres (bouton du mode statique) : invalide le
		// cache, la prochaine frame re-rend tout, puis refige.
		void Demo3DHostShadowRecalc();
		// « Capturer la vue » : sauve la DERNIERE image rendue de la vue 3D
		// (scene seule, sans interface) en PNG a ce chemin.
		bool Demo3DHostCaptureView(const char *path);
		// Vrai quand une ombre figee ne correspond plus a la scene (lumiere ou
		// geometrie modifiee depuis le gel) : le bouton se colore.
		bool Demo3DHostShadowRecalcPending();
		// Temperature de couleur (kelvins ; 0 = desactivee) et exposition (stops).
		bool Demo3DHostLightTempExp(int32 node, float32 *tempK, float32 *exposure);
		void Demo3DHostSetLightTempExp(int32 node, float32 tempK, float32 exposure);
		// Camera (vide sous-type 10) : focale + clips, declaratifs pour l'instant.
		// Cookie (texture de faisceau) d'une lumiere : -1 = couleur pure.
		int32 Demo3DHostLightCookie(int32 node);
		void Demo3DHostSetLightCookie(int32 node, int32 idx);
		// Proprietes NATIVES d'une lumiere par NOEUD (demo ou utilisateur).
		bool Demo3DHostLightEx(int32 node, float32 *range, float32 *inner, float32 *outer,
							   float32 *aw, float32 *ah, bool *shadow, int32 *type);
		void Demo3DHostSetLightEx(int32 node, float32 range, float32 inner, float32 outer,
								  float32 aw, float32 ah, bool shadow);
		bool Demo3DHostCameraParams(int32 node, float32 *fov, float32 *nearC, float32 *farC);
		void Demo3DHostSetCameraParams(int32 node, float32 fov, float32 nearC, float32 farC);
		bool Demo3DHostMeshParams(int32 node, int32 *segs, int32 *rings, float32 *aux);
		void Demo3DHostSetMeshParams(int32 node, int32 segs, int32 rings, float32 aux);
		// Lumiere UTILISATEUR (kind 5) : couleur + intensite.
		bool Demo3DHostUserLightParams(int32 node, float32 *color3, float32 *intensity);
		void Demo3DHostSetUserLightParams(int32 node, const float32 *color3, float32 intensity);
		void Demo3DHostNodeBaseSize(int32 node, float32 *out3); // taille locale par nature
		void Demo3DHostSetNodeBaseSize(int32 node, const float32 *in3); // decouplee de l'echelle

		// ── Reglages de vue (panneau Scene / Outil) ─────────────────────────
		void Demo3DHostSetViewFar(float32 f); // 0 = auto
		float32 Demo3DHostViewFar();
		void Demo3DHostSetOrthoScale(float32 f);
		float32 Demo3DHostOrthoScale();
		void Demo3DHostSetGridExtent(int32 n);
		int32 Demo3DHostGridExtent();
		void Demo3DHostResetCursor();
		void Demo3DHostCursorToSelection();
		void Demo3DHostSelectionToCursor();
		void Demo3DHostClearXform(int32 which); // 0 translation, 1 rotation, 2 echelle
		// Pose de camera : les ONGLETS DE SCENE s'en servent pour donner a
		// chaque scene sa propre vue (memorisee a la bascule d'onglet).
		void Demo3DHostGetCameraPose(float32 *t3, float32 *dist, float32 *yaw, float32 *pitch,
									 bool *ortho);
		void Demo3DHostSetCameraPose(const float32 *t3, float32 dist, float32 yaw, float32 pitch,
									 bool ortho);

		// Vrai des que la demo rend dans sa cible (l'interface peut poser la
		// texture 4096).
		bool Demo3DHostReady();

		// Dernier echec d'initialisation, ou nullptr.
		const char *Demo3DHostError();

		void Demo3DHostShutdown();

	} // namespace demo
} // namespace nkentseu
