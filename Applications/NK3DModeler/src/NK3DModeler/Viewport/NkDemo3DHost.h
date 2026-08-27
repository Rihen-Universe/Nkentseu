#pragma once
// -----------------------------------------------------------------------------
// @File    NkDemo3DHost.h
// @Brief   Facade OPAQUE de la vue 3D portee de renderdemo --demo=2.
// @License Proprietary - All Rights Reserved (see LICENSE)
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
		/// ── LA CARTE DES IDENTIFIANTS DE TEXTURE D'INTERFACE ────────────────
		/// Ils sont pris dans UN SEUL espace, partage par tout ce que l'interface
		/// affiche. Deux usages sur le meme numero, et l'un ecrase l'autre en
		/// silence -- c'est arrive : l'apercu de materiau avait pris 4600, deja
		/// occupe par les couvertures du launcher, et l'ecran d'accueil montrait
		/// une sphere de materiau a la place de la miniature du projet (Rihen,
		/// 14 aout). D'ou cette carte, a tenir a jour :
		///
		///   4096        vue 3D (cible hors ecran)
		///   4300..4399  matcaps
		///   4400..4463  vignettes des materiaux (une par emplacement)
		///   4500..4598  miniatures des scenes
		///   4599        image d'accueil
		///   4600..      couvertures des projets recents (launcher)
		///   5000        grand apercu de materiau  <- ici
		///
		/// Il vit dans CE fichier, et non dans le module de rendu : le panneau doit
		/// pouvoir poser l'image sans rien connaitre de NKRenderer.
		constexpr uint32 kNkMatPreviewTexId = 5000u;

		void Demo3DHostFrame(void *cmd);
		/// Rend l'apercu du materiau `slot` a `w` x `h`, dans le command buffer de
		/// l'editeur — donc AVANT la passe backbuffer, une passe de rendu ne
		/// pouvant pas en contenir une autre. Une seule image par frame : celle
		/// que le panneau affiche. Sa texture est publiee par
		/// Demo3DHostRegisterInto sous nk3d::matprev::kPreviewTexId.
		void Demo3DHostMatPreviewFrame(void *cmd, int32 slot, int32 w, int32 h);
		/// Demande la CAPTURE de la vignette d'un materiau vers `cheminPng`. La
		/// prise se fait sur deux frames -- rendu puis relecture, une image devant
		/// etre terminee pour etre lue -- et une seule par frame. Rendue en SPHERE
		/// et en carre : c'est une icone de liste, elle doit se comparer aux autres.
		void Demo3DHostMatThumbRequest(int32 slot, const char *cheminPng);
		/// Suspend les demandes de vignette. A poser autour d'une ecriture qui est
		/// elle-meme DECLENCHEE par une vignette, sinon la chaine se relance sans
		/// fin. A remettre a faux dans tous les cas.
		void Demo3DHostMatThumbSuspend(bool on);
		/// La VIGNETTE d'un materiau : un PNG encode en base64, qui vit DANS le
		/// materiau et voyage donc avec lui. Chaine vide tant qu'aucune capture
		/// n'a ete prise (le materiau n'a jamais ete enregistre).
		/// Recupere les PIXELS d'une vignette fraichement rendue, s'il y en a une
		/// a prendre. Vrai une seule fois par prise : c'est ce qui evite de
		/// televerser la meme image a chaque frame. L'image affichee se rafraichit
		/// ainsi des qu'un reglage change, sans attendre l'enregistrement -- lui
		/// seul declenche l'encodage dans le fichier, qui coute bien plus cher.
		bool Demo3DHostMatThumbTakePixels(int32 i, const uint8 **px, int32 *cote);
		/// Un materiau dont la vignette vient d'etre encodee et n'est pas encore
		/// dans son fichier, ou -1. A consommer par l'application, qui seule sait
		/// ecrire : une vignette encodee apres l'enregistrement resterait sinon
		/// absente du disque jusqu'a la sauvegarde suivante.
		int32 Demo3DHostMatThumbTakeDirty();
		const char *Demo3DHostProjMatThumb(int32 i);
		void Demo3DHostProjMatSetThumb(int32 i, const char *b64);

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
		// Point de PIVOT (Blender) : 0 barycentre, 1 boite englobante,
		// 2 curseur 3D, 3 origines individuelles, 4 element actif.
		void Demo3DHostSetPivotMode(int32 m);
		int32 Demo3DHostPivotMode();
		// Cible d'AIMANTATION (Blender « Snap To ») : 0 increment, 1 grille,
		// 2 sommet, 3 arete, 4 face, 5 volume (dans la matiere sous le
		// curseur), 6 centre d'arete, 7 arete perpendiculaire (le trajet du
		// geste devient perpendiculaire a l'arete), 8 centre de face.
		void Demo3DHostSetSnapTarget(int32 t);
		int32 Demo3DHostSnapTarget();
		// BASE D'ACCROCHE (Blender « Snap Base ») : quel point de l'objet
		// deplace cherche la cible. 0 = le plus proche (sommets), 1 = pivot
		// (centre du gizmo -- « Centre » et « Median » fusionnent ici, le
		// pivot suit deja le mode de pivot de l'application), 2 = objet actif.
		int32 Demo3DHostSnapBase();
		void Demo3DHostSetSnapBase(int32 b);
		// ALIGNER LA ROTATION SUR LA CIBLE (Blender) : quand une FACE (ou un
		// centre de face) accroche, l'objet tourne pour poser son +Z sur sa
		// normale -- autour de son centre, sans bouger. Les autres cibles
		// n'ont pas de normale stable : elles n'alignent pas.
		bool Demo3DHostSnapAlignRot();
		void Demo3DHostSetSnapAlignRot(bool on);
		// ECHELLE EXACTE : autorise le CISAILLEMENT. Coupee (defaut, choix
		// d'Unreal), l'echelle est projetee sur les axes de l'objet et reste
		// stockable en trois facteurs. Active, un scale en repere global sur un
		// objet tourne le deforme vraiment (un carre devient un losange) : le
		// noeud memorise alors le repere de son echelle.
		bool Demo3DHostShearScale();
		void Demo3DHostSetShearScale(bool on);
		// EDITION PROPORTIONNELLE (mode Edition) : deplacer un sommet entraine
		// ses voisins, d'autant moins qu'ils sont loin. `falloff` : 0 lisse,
		// 1 sphere, 2 racine, 3 carre inverse, 4 net, 5 lineaire, 6 constant,
		// 7 aleatoire -- les huit lois de Blender.
		void Demo3DHostPropEdit(bool *on, float32 *radius, int32 *falloff);
		void Demo3DHostSetPropEdit(bool on, float32 radius, int32 falloff);
		// BROUILLARD AU SOL : altitude de base, epaisseur (0 = pas de nappe, le
		// brouillard ne depend alors que de la distance), force du souffle, et
		// « derive liee au vent des nuages » -- le sol et le ciel avancent alors
		// du meme pas. La physique du vent la pilotera le jour venu.
		void Demo3DHostFogGround(float32 *base, float32 *thickness, float32 *wind,
								 bool *fromClouds);
		void Demo3DHostSetFogGround(float32 base, float32 thickness, float32 wind,
									bool fromClouds);
		int32 Demo3DHostOrientation();
		void Demo3DHostSetSnap(bool on, float32 t, float32 rotDeg, float32 scl);
		bool Demo3DHostSnapEnabled();
		void Demo3DHostSetGizmoHidden(bool hidden);
		// OPERATIONS D EDITION, EXPOSEES AU SHELL -- PORTAGE DE LA FORME (etape 1)
		// NkViewport3D exposait ces operations, NkDemo3D savait les faire mais ne les
		// offrait a PERSONNE : la facade comptait 421 entrees et ZERO operation
		// d edition. Les 42 actions de NkVpAction etaient donc cablees sur une vue
		// sans device -- elles tournaient a chaque image et ne faisaient rien.
		// LES CORPS VIENNENT DES FONCTIONS VIVANTES DE NkDemo3D, jamais de la vue
		// dormante : trois y sont plus anciennes -- Merge sans le curseur 3D, Inset
		// sans individual, Dissolve sans mode.
		// Rendent true si la commande a REELLEMENT modifie le maillage.
		bool Demo3DHostEditExtrude(bool individual);
		bool Demo3DHostEditDelete();
		bool Demo3DHostEditMerge();
		bool Demo3DHostEditMakeFace();
		bool Demo3DHostEditSubdivide();
		bool Demo3DHostEditLoopCut();
		bool Demo3DHostEditBevel(bool vertexMode);
		bool Demo3DHostEditInset();
		bool Demo3DHostEditDissolve();
		bool Demo3DHostInEditMode();
		void Demo3DHostSetEditSelMask(int32 mask); // bits 1 sommet, 2 arete, 4 face
		int32 Demo3DHostEditSelMask();
		void Demo3DHostSetZoneTool(int32 shape); // -1 off, 0 rectangle, 1 cercle, 2 lasso
		void Demo3DHostSetCursorTool(bool on);
		void Demo3DHostSetGridFlags(bool grid, bool minor, bool major, bool axes);
		void Demo3DHostGridFlags(bool *grid, bool *minor, bool *major, bool *axes);
		// CURSEUR 3D visible dans la vue (onglet Affichage, coche par defaut).
		// C'est un repere de TRAVAIL : on doit pouvoir l'eteindre sans renoncer
		// a l'outil qui le place, et il ne part dans un rendu que si l'aide
		// « Curseur 3D » est explicitement demandee.
		bool Demo3DHostCursorShown();
		void Demo3DHostSetCursorShown(bool on);
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
		// ── QU'Y A-T-IL SOUS CE PIXEL ? (glisser-deposer du navigateur) ─────
		// Le MEME pick que le clic de selection (Demo3D_PickEmptyAt) : le
		// lacher et le clic ne peuvent donc pas designer deux objets
		// differents. Coordonnees en pixels de la FENETRE -- celles de la
		// souris, telles quelles. L'origine de la vue est soustraite DEDANS,
		// comme pour toute autre entree : l'appelant n'a pas a la connaitre.
		//
		// Deux appels, parce que la reponse n'existe qu'a la frame suivante :
		// le pick a besoin de la camera et de la taille de vue, toutes deux
		// locales a la frame. L'interface DEMANDE, la boucle EXECUTE.
		void Demo3DHostPickRequest(float32 xFenetre, float32 yFenetre);
		/// Lit la reponse UNE SEULE FOIS ; false tant qu'elle n'est pas prete.
		/// `*node` porte TROIS issues, et elles sont dans la valeur :
		///   >= 0 : le noeud designe
		///   -1   : RIEN sous le curseur (le vide). C'est un RESULTAT legitime,
		///          pas un echec -- et surtout pas le noeud 0, qui existe.
		///   -2   : hors du viseur (la question n'a pas de sens ici).
		/// `world3` recoit le point du monde vise (surface touchee, sinon plan
		/// du sol y=0) : c'est la ou un modele lache dans le vide doit atterrir.
		bool Demo3DHostPickTake(int32 *node, float32 *world3);
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
		// APRES UN CHARGEMENT DE PROJET : l'etat restaure devient la REFERENCE
		// du detecteur de hierarchie. Celui-ci propage aux enfants le
		// DEPLACEMENT d'un parent depuis le cliche de la frame precedente ;
		// sans recalage, la premiere frame apres un chargement verrait tous
		// les parents « avoir bouge » et trainerait leurs enfants -- qui sont
		// deja a leur place, telle que le fichier l'a ecrite. A appeler une
		// fois, apres avoir pose transformations ET parentes.
		void Demo3DHostHierarchyResync();
		// Selection d'un EMPTY (gizmo dedie dans la vue ; -1 = aucun).
		void Demo3DHostSelectEmptyNode(int32 node);
		int32 Demo3DHostSelectedEmptyNode();
		void Demo3DHostToggleEmptyNode(int32 node); // Maj/Ctrl+clic : multi successif
		bool Demo3DHostEmptyNodeSelected(int32 node);
		// Transform EFFECTIVE d'un empty (node >= 90), drag du gizmo compris.
		bool Demo3DHostEmptyTransform(int32 node, float32 *pos3, float32 *rotDeg3, float32 *scl3);
		void Demo3DHostSetEmptyTransform(int32 node, const float32 *pos3, const float32 *rotDeg3,
										 const float32 *scl3);
		// Poser un MODEL : le conteneur et ses maillages, d'un seul delta. Les
		// transforms etant absolues, deplacer le seul conteneur laisse sa
		// matiere en arriere -- et elle seule est rendue.
		void Demo3DHostSetModelTransform(int32 node, const float32 *pos3,
						     const float32 *rotDeg3, const float32 *scl3);

		// ── Materiau par objet (panneau Modele) ─────────────────────────────
		// Lecture = valeurs EFFECTIVES vues a la derniere soumission ;
		// ecriture = surcharge persistante (teinte / metallique+rugosite).
		bool Demo3DHostMeshMaterial(int32 i, float32 *tint3, float32 *metallic, float32 *roughness);
		void Demo3DHostSetMeshTint(int32 i, const float32 *rgb3);
		void Demo3DHostSetMeshMetalRough(int32 i, float32 metallic, float32 roughness);
		void Demo3DHostResetMeshMat(int32 i);
		// ── MATERIAUX DU PROJET (pastille Materiau) ─────────────────────────
		// Ressources nommees, assignables a plusieurs cibles ; parametres =
		// sous-ensemble NkPBRParams (la sauvegarde .nkasset passera par
		// NkMaterialLibrary). Info renvoie faux sur un slot libre -- l'iteration
		// parcourt 0..63 et saute les trous. Assign(-1) retire l'assignation.
		int32 Demo3DHostProjMatCreate();
		void Demo3DHostProjMatDelete(int32 i);
		bool Demo3DHostProjMatInfo(int32 i, char *name, uint32 cap, float32 *albedo3,
								   float32 *rough, float32 *metal);
		// Physique de surface (vernis 0..1, sa rugosite, diffusion 0..1). A part
		// de SetParams pour ne pas forcer ses appelants historiques. La couleur
		// de diffusion suit l'albedo (posee par le hook de drawcall).
		void Demo3DHostProjMatSurface(int32 i, float32 *cc, float32 *ccRough, float32 *sss);
		void Demo3DHostProjMatSetSurface(int32 i, float32 cc, float32 ccRough, float32 sss);
		void Demo3DHostProjMatSetParams(int32 i, const float32 *albedo3, float32 rough,
										float32 metal);
		void Demo3DHostProjMatSetName(int32 i, const char *name);
		void Demo3DHostProjMatAssign(int32 node, int32 mat);
		int32 Demo3DHostProjMatOf(int32 node);
		// LA LISTE DES MATERIAUX D'UN OBJET (12 aout) : distincte de celle du
		// PROJET. Retirer d'ici n'affecte que cet objet et ne detruit rien ;
		// supprimer un materiau ne se fait que depuis le navigateur de contenu.
		int32 Demo3DHostNodeMatCount(int32 node);
		int32 Demo3DHostNodeMatAt(int32 node, int32 k);
		bool Demo3DHostNodeMatAdd(int32 node, int32 slot);
		bool Demo3DHostNodeMatRemove(int32 node, int32 slot);
		// APERCU du materiau (rendu analytique CPU) : forme 0 plan, 1 sphere,
		// 2 cube, 3 liquide, 4 cheveux. PreviewTake rend vrai si l'apercu
		// etait perime et vient d'etre regenere dans rgba (size x size x 4) --
		// l'appelant l'uploade alors comme image d'interface.
		// ── LES QUATRE CANAUX DE TEXTURE D'UN MATERIAU ─────────────────────
		// 0 COULEUR (albedo) · 1 NORMALE (relief) · 2 ORM (occlusion /
		// rugosite / metallique empaquetees, standard glTF) · 3 EMISSIF.
		// UNE SEULE paire de fonctions pour les quatre, indexee par canal :
		// quatre copies auraient diverge au premier correctif -- c'est ce qui
		// est arrive aux combos de la sortie.
		// Chemin image ; "" = pas de texture (les valeurs numeriques font
		// alors foi), « - » au set pour la retirer. LE CHARGEMENT FAIT FOI :
		// un chemin qui ne charge pas n'est pas memorise et le set rend faux.
		int32 Demo3DHostMatChanCount();
		const char *Demo3DHostMatChanName(int32 c);
		const char *Demo3DHostProjMatMap(int32 i, int32 chan);
		bool Demo3DHostProjMatSetMap(int32 i, int32 chan, const char *path);
		// INTENSITES des canaux qui en ont une : relief (0..2) et emissif
		// (0..20). Sans leur texture elles n'ont pas d'effet -- sauf la teinte
		// emissive, qui vaut aussi sans.
		void Demo3DHostProjMatChanStrength(int32 i, float32 *nrm, float32 *emi);
		void Demo3DHostProjMatSetChanStrength(int32 i, float32 nrm, float32 emi);
		// Echelle du PARALLAX (canal Hauteur) : 0 = coupe, ~0.02-0.08 utile.
		float32 Demo3DHostProjMatParallax(int32 i);
		void Demo3DHostProjMatSetParallax(int32 i, float32 scale);
		// Ombre d'un objet transparent : 0 pleine, 1 proportionnelle, 2 aucune.
		int32 Demo3DHostProjMatShadowMode(int32 i);
		void Demo3DHostProjMatSetShadowMode(int32 i, int32 mode);
		// MELANGE de materiaux (etape 1) : B par emplacement (-1 = off),
		// source 0=Facteur 1..4=vColor RGBA 5=UV.x 6=UV.y, facteur 0..1.
		int32 Demo3DHostProjMatMixWith(int32 i);
		int32 Demo3DHostProjMatMixSource(int32 i);
		float32 Demo3DHostProjMatMixFactor(int32 i);
		void Demo3DHostProjMatSetMix(int32 i, int32 withSlot, int32 source, float32 factor);
		// TYPE de materiau (valeur NkMaterialType : 0 PBR, 3 peau, 4 cheveux,
		// 5 verre, 6 tissu, 7 carrosserie, 8 feuillage, 9 eau, 11 emissif,
		// 20 toon, 21 toon encre, 22 anime, 60 sans eclairage).
		int32 Demo3DHostProjMatType(int32 i);
		void Demo3DHostProjMatSetType(int32 i, int32 type);
		// Reglages PBR restants : opacite, anisotropie, sheen.
		void Demo3DHostProjMatPBRExtra(int32 i, float32 *alpha, float32 *aniso,
									   float32 *sheen);
		void Demo3DHostProjMatSetPBRExtra(int32 i, float32 alpha, float32 aniso,
										  float32 sheen);
		// Famille TOON en paquet de 14 flottants (cf. definition).
		void Demo3DHostProjMatToon(int32 i, float32 *v14);
		void Demo3DHostProjMatSetToon(int32 i, const float32 *v14);
		// MATERIAU PAR DEFAUT (-1 = premier du registre) + echange
		// d'emplacements (toutes references suivies : noeuds, melanges, defaut).
		int32 Demo3DHostProjMatDefault();
		void Demo3DHostProjMatSetDefault(int32 i);
		bool Demo3DHostProjMatSwap(int32 a, int32 b);
		void Demo3DHostProjMatEmissive(int32 i, float32 *rgb);
		void Demo3DHostProjMatSetEmissive(int32 i, const float32 *rgb);
		/// Un materiau EMISSIF eclaire-t-il la scene ? Faux par defaut : une
		/// surface emissive s'AFFICHE lumineuse sans forcement eclairer ses
		/// voisins. Coche, elle injecte une lumiere dans la grille de GI -- et la
		/// seulement : elle ne rejoint pas les lumieres du rendu direct, sinon
		/// l'objet gagnerait un speculaire et une ombre qu'aucune surface emissive
		/// ne produit.
		bool Demo3DHostProjMatEmiLights(int32 i);
		void Demo3DHostProjMatSetEmiLights(int32 i, bool on);
		int32 Demo3DHostProjMatPrevShape(int32 i);
		void Demo3DHostProjMatSetPrevShape(int32 i, int32 shape);
		/// Rend la vignette d'apercu en RGBA, `width` x `height`. Rectangulaire :
		/// la largeur etend le damier, la hauteur seule dimensionne l'objet --
		/// elargir le panneau ne doit pas grossir la sphere (Rihen, 13 aout).
		/// Rend faux si la vignette est deja a jour (les dimensions comptent).
		bool Demo3DHostProjMatPreviewTake(int32 i, uint8 *rgba, uint32 width, uint32 height);
		// Le registre n'est JAMAIS vide face a l'utilisateur : renvoie le
		// premier materiau, en creant le materiau de base au besoin.
		int32 Demo3DHostProjMatEnsureDefault();
		// ── OUVRIR UN PROJET : REMPLACER, PAS EMPILER ───────────────────────
		// Vide TOUT le registre et toutes les assignations. ProjMatDelete ne
		// convient pas ici : il REFUSE volontairement de supprimer le dernier
		// materiau (tout maillage en porte un) -- regle juste en pleine
		// session, mais qui laisserait survivre un materiau de la session
		// precedente au milieu de ceux du fichier qu'on ouvre.
		void Demo3DHostProjMatClear();
		// Nombre d'EMPLACEMENTS du registre : l'iteration va de 0 a Max-1 et
		// saute les trous (ProjMatInfo rend faux). La valeur vit dans
		// NkDemo3D.cpp et nulle part ailleurs -- une seconde copie divergerait
		// au premier agrandissement (CONVENTIONS_FICHIERS.md §3).
		int32 Demo3DHostProjMatMax();

		// ── Suppression / duplication / presse-papiers ──────────────────────
		// Les noeuds 96..159 sont les OBJETS UTILISATEUR (crees ici).
		bool Demo3DHostNodeDeleted(int32 node);
		void Demo3DHostDeleteNode(int32 node, bool withChildren);
		int32 Demo3DHostDuplicateNode(int32 node); // -1 si impossible (lumiere v1)
		// DUPLIQUER EN CHOISISSANT LE PARTAGE (decision de Rodolf, 16 aout).
		// independant=false (le DEFAUT, et ce que rend Demo3DHostDuplicateNode) :
		// le double reference les MEMES maillages -- instantane, sans cout
		// memoire, et c'est ce que veulent le modificateur array, le jeu et le
		// film. independant=true : sa geometrie est recopiee, pour obtenir deux
		// models dont un seul sera retouche.
		// Dans les deux cas, UN DOUBLE DE MODEL EST UN MODEL et emporte sa matiere.
		int32 Demo3DHostDuplicateNodeEx(int32 node, bool independent);
		int32 Demo3DHostArchiveNode(int32 node);   // copie invisible pour asset
		// ARCHIVER UN NOEUD DEJA CREE, pour la RELECTURE d'un projet. Distinct de
		// Demo3DHostDeleteNode, qui libere l'emplacement (nature remise a zero) :
		// une archive doit garder sa nature, sinon le prochain objet cree recycle
		// sa place et l'asset du navigateur perd sa geometrie.
		// ARCHIVER un noeud DEJA CREE. Distinct de Demo3DHostDeleteNode, qui libere
		// l'emplacement (nature remise a zero) : une archive garde sa nature, sinon
		// le prochain objet cree reprend sa place et l'asset perd sa geometrie.
		void Demo3DHostSetNodeArchived(int32 node, bool v); // un seul noeud (relecture)
		// Appartenance d'un maillage a un model : le MEME parcours que le
		// deplacement de document et l'archivage, expose pour l'ecriture du
		// fichier de model.
		bool Demo3DHostNodeInnerMeshOf(int32 node, int32 root);
		void Demo3DHostArchiveTree(int32 node, bool v);		// le model ET ses maillages
		// Pose l'origine d'un model sur le barycentre (X/Z) de sa matiere.
		// Idempotente : sert aussi a reparer un asset ancien a l'usage.
		// Renvoie VRAI si l'origine a REELLEMENT change -- ce retour sert la
		// persistance (decision de Rihen, 17/08) : une correction faite en
		// memoire marque la carte du model pour que la prochaine SAUVEGARDE
		// ecrive le fichier. Jamais d'ecriture disque en douce au depot.
		bool Demo3DHostRecenterModel(int32 root);
		bool Demo3DHostNodeArchived(int32 node);
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
		// POSER le drapeau tel quel : c'est ce qu'exige la RELECTURE d'un
		// projet. EnsureModelMesh ne convient pas la -- il CREERAIT un
		// maillage interne, alors que celui du fichier est deja restaure a
		// cote, et le model se retrouverait avec un maillage en trop.
		void Demo3DHostSetNodeIsModel(int32 node, bool v);
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
		// CADENAS D'ORBITE en vue camera : actif, la rotation orbite la camera
		// autour d'un centre (selection, sinon le point vise) au lieu de
		// tourner sur place -- comme Blender.
		bool Demo3DHostCamOrbitLock();
		void Demo3DHostSetCamOrbitLock(bool on);
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
		// NAISSANCE DEPUIS DES DONNEES (import d'un fichier 3D, 17/08). Le
		// conteneur d'abord, ses maillages ensuite. Positions MONDE -- les
		// transforms de ce systeme ne composent jamais parent x enfant -- et
		// les SOMMETS d'un maillage sont LOCAUX a son noeud (l'appelant les a
		// rebases autour de `pos3`). keepCPU est pose EXPLICITEMENT : sans
		// copie CPU, ni archivage relisible ni copie independante. Chaque
		// nouveau-ne recale son cliche de hierarchie (regle du 17/08).
		// `verts` : des NkVertex3D en layout Default3D. Rend -1 si plus
		// d'emplacement libre ou si la creation du maillage echoue.
		int32 Demo3DHostCreateModelRoot(const float32 *pos3);
		int32 Demo3DHostCreateMeshNode(int32 root, const void *verts, uint32 vcount,
									   const uint32 *indices, uint32 icount,
									   const float32 *pos3, const char *debugName);
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
		// AMBIANCES de nuages : des reglages tout faits (couverture, densite,
		// echelle, couleur, vitesse) qui regenerent aussitot. 0 = defaut
		// (valeurs d'origine, nuages laisses allumes), 1 = pluie, 2 = desert.
		// La requete dit laquelle est EN PLACE (-1 : aucune / retouchee), pour
		// que l'interface allume le bon bouton.
		void Demo3DHostApplyCloudPreset(int32 which);
		int32 Demo3DHostCloudPreset();
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
		// Occlusion ambiante (SSAO v1) : ETEINTE par defaut dans le modeleur —
		// reglable depuis le panneau Rendu. Rayon en METRES (monde), intensite
		// 0..4 (0 = passe active mais muette). La config du renderer fait foi.
		void Demo3DHostSSAO(bool *on, float32 *radius, float32 *intensity);
		void Demo3DHostSetSSAO(bool on, float32 radius, float32 intensity);
		// Exposition (multiplicateur HDR avant tonemap) et bloom (actif, seuil
		// de luminance — peut depasser 1 en HDR —, intensite). Panneau Rendu.
		void Demo3DHostPostFx(float32 *exposure, bool *bloomOn, float32 *bloomThr,
							  float32 *bloomStr);
		void Demo3DHostSetPostFx(float32 exposure, bool bloomOn, float32 bloomThr,
								 float32 bloomStr);
		// EXPOSITION AUTOMATIQUE : le moteur mesure la luminance moyenne de la
		// scene et adapte l'exposition, comme l'oeil qui s'accommode. Dosage
		// 0 = manuel, 1 = entierement automatique ; cible = gris moyen (0.18 en
		// convention photo) ; vitesse en unites par seconde (0 = fige sur la
		// premiere mesure). Repond au blanc plat des fortes luminosites.
		void Demo3DHostAutoExp(float32 *strength, float32 *key, float32 *speed);
		void Demo3DHostSetAutoExp(float32 strength, float32 key, float32 speed);
		// Sol infini (option) : plan de sol recepteur d'ombres, couleur /
		// hauteur / rugosite -- distinct de la grille. Motif : 0 uni,
		// 1 damier, 2 carreaux a joints ; taille du carreau en metres.
		void Demo3DHostFloor(bool *on, float32 *rgb, float32 *y, float32 *rough, int32 *pattern,
							 float32 *tile, float32 *metal);
		void Demo3DHostSetFloor(bool on, const float32 *rgb, float32 y, float32 rough,
								int32 pattern, float32 tile, float32 metal);
		// Vrai tant qu'un geste de gizmo est en cours (objet, edition, lumiere ou
		// empty). Le shell surveille le FRONT DESCENDANT : un geste qui se
		// termine est un commit et allume la pastille « non enregistre ».
		// Enumerer les mutations une a une cote shell en oubliait -- le
		// deplacement d'un cube n'allumait pas la pastille (constate par Rihen).
		bool Demo3DHostAnyGizmoDragging();
		// Invalide la grille d'occlusion ambiante (GI voxel) : les objets
		// utilisateur y sont des occludeurs, elle doit donc etre reconstruite a
		// chaque modification de la scene. Appele par NkMarkDirty -- la meme
		// action qui allume la pastille rafraichit l'ambiance.
		void Demo3DHostGIMarkDirty();
		/// ILLUMINATION GLOBALE (rebond indirect sur la grille voxel). Elle vivait
		/// dans le portage de la demo, pilotable par une touche et par rien
		/// d'autre. L'intensite n'est pas bornee en haut : comme l'emission, sa
		/// valeur utile depend de la scene.
		void Demo3DHostGI(bool *on, float32 *intensity);
		void Demo3DHostSetGI(bool on, float32 intensity);
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

		// ── SORTIE (pastille Output) ────────────────────────────────────────
		// Ce qui SORT de la scene, par opposition a la pastille Rendu qui dit
		// comment elle est eclairee. La resolution est INDEPENDANTE de la
		// taille de la fenetre : un rendu 4K depuis une fenetre 1600x900 fait
		// bien 4K. `source` vaut -1 pour la vue 3D, sinon le noeud camera.
		void Demo3DHostOutMain(int32 *source, int32 *w, int32 *h, int32 *scalePct, int32 *format,
							   bool *transparent);
		void Demo3DHostSetOutMain(int32 source, int32 w, int32 h, int32 scalePct, int32 format,
								  bool transparent);
		// Resolution reellement produite (demandee x pourcentage) : le panneau
		// l'affiche pour que rien ne soit a deviner.
		void Demo3DHostOutEffectiveSize(int32 *w, int32 *h);
		const char *Demo3DHostOutDir();
		void Demo3DHostSetOutDir(const char *d);
		const char *Demo3DHostOutName();
		void Demo3DHostSetOutName(const char *n);
		// INCRUSTATIONS : des cibles secondaires posees SUR la principale
		// (Rihen : « une principale et les autres en miniature, rectangle,
		// carre, cercle etc. »). Info rend faux sur un emplacement libre --
		// l'iteration parcourt 0..Max-1 et saute les trous, comme les
		// materiaux de projet. Position et taille sont des FRACTIONS de la
		// principale : changer la resolution ne deplace donc rien.
		// CHAQUE FORME A SES PROPRES DIMENSIONS (Rihen) : `size2` porte la
		// LARGEUR puis la HAUTEUR, chacune fraction du cote correspondant de la
		// principale -- 0,25 se lit « le quart », dans les deux sens. Un carre
		// ou un cercle n'a qu'un cote a donner : seule la premiere valeur
		// compte, la seconde la recopie pour ne jamais rien dire de
		// contradictoire. Voir NkInsetDimCount / NkInsetDimName (NkOutCompose.h)
		// pour savoir combien de champs afficher et sous quel nom.
		int32 Demo3DHostOutInsetMax();
		bool Demo3DHostOutInset(int32 i, int32 *source, int32 *shape, float32 *xy2, float32 *size2,
								float32 *border, float32 *borderCol3, float32 *opacity);
		void Demo3DHostSetOutInset(int32 i, int32 source, int32 shape, const float32 *xy2,
								   const float32 *size2, float32 border,
								   const float32 *borderCol3, float32 opacity);
		// FICHIER PROPRE : la miniature est AUSSI ecrite seule, en plus d'etre
		// posee sur l'image principale. Elle sort alors TELLE QUELLE --
		// rectangulaire, sans masque de forme ni lisere : la forme appartient a
		// la composition, pas a l'image. Nom : <nom>_<NNN>_<camera>[_<mode>].
		bool Demo3DHostOutInsetOwnFile(int32 i);
		void Demo3DHostSetOutInsetOwnFile(int32 i, bool on);
		// Ce fichier propre garde-t-il la FORME et le lisere ? Non par defaut --
		// la forme appartient a la composition -- mais c'est un choix, pas une
		// regle : une vignette ronde peut etre le livrable voulu.
		bool Demo3DHostOutInsetOwnShaped(int32 i);
		void Demo3DHostSetOutInsetOwnShaped(int32 i, bool on);
		// L'interface depose ici le VRAI nom d'un noeud : l'hote ne connait que
		// des numeros, et les fichiers produits doivent porter « Camera.002 »
		// plutot que « cam2 » (Rihen).
		void Demo3DHostSetNodeLabel(int32 node, const char *name);
		// ── EXCLU DU RENDU (colonne camera de la hierarchie, Rihen) ─────────
		// DISTINCT de l'oeil : l'objet reste VISIBLE dans la vue -- on travaille
		// avec -- mais n'apparait pas dans l'image produite. Pendant du bouton
		// appareil photo de Blender. Il s'HERITE comme le masquage et le
		// cadenas : exclure un parent exclut son sous-arbre, chaque enfant
		// gardant son propre drapeau. `Eff` rend l'etat EFFECTIF (le sien ou
		// celui d'un ancetre) -- c'est ce que l'interface doit montrer, sinon
		// une exclusion heritee paraitrait inexplicable.
		bool Demo3DHostNodeNoRender(int32 node);
		bool Demo3DHostNodeNoRenderEff(int32 node);
		void Demo3DHostSetNodeNoRender(int32 node, bool on);
		// CADENCE DE CAPTURE, en masque de bits : 1 cible fixe entre les images,
		// 2 lecture differee en anneau, 4 encodage pendant le rendu. Les trois
		// actifs par defaut -- ce sont les bons reglages du cas courant, et on
		// les coupe pour diagnostiquer.
		// ── ENREGISTREMENT VIDEO DE LA SESSION ──────────────────────────────
		// On filme ce qui SE PASSE dans la vue, a sa resolution : le modeleur
		// n'a pas de timeline, et une plage d'images produirait la meme image
		// repetee. Le rendu d'animation viendra pour les captures quand la
		// timeline existera ; le tutoriel n'en aura jamais besoin (Rihen).
		// Stop(keep=false) ABANDONNE : le fichier est ferme puis efface, pour
		// qu'une prise ratee ne laisse rien derriere elle.
		bool Demo3DHostRecStart();
		bool Demo3DHostRecStop(bool keep);
		bool Demo3DHostRecActive();
		int32 Demo3DHostRecFrames();
		const char *Demo3DHostRecPath();
		// PAUSE : on cesse de deposer des images ; le fichier ne garde aucune
		// trace du temps suspendu. Reprendre ne coute rien.
		void Demo3DHostRecPause(bool on);
		bool Demo3DHostRecPaused();
		// Images SAUTEES parce que l'encodage avait du retard : une video qui
		// saute vaut mieux qu'une application qui s'etouffe, mais il faut le
		// DIRE -- sinon on croirait a un enregistrement fidele.
		int32 Demo3DHostRecDropped();
		// EN SATURATION : sauter des images (fluide, video trouee) ou laisser la
		// file GONFLER (fidele, le fil principal attend). Aucune des deux n'est
		// bonne dans tous les cas -- filmer une demonstration veut de la
		// fluidite, filmer un resultat veut la fidelite. Ne change qu'entre
		// deux prises.
		bool Demo3DHostRecGrow();
		void Demo3DHostSetRecGrow(bool on);
		// ── ENREGISTREMENT DU TUTORIEL : LA FENETRE ENTIERE ─────────────────
		// Seul l'application sait photographier sa propre fenetre (l'OS le
		// fait) : elle POUSSE donc ses pixels ici, et toute la mecanique --
		// file, fil d'encodage, cadence, formats, pause, abandon -- est celle
		// de la vue. Les deux peuvent tourner EN MEME TEMPS : ce sont deux
		// points de vue d'une meme session, et leurs fichiers portent des noms
		// distincts pour ne pas s'ecraser.
		// `Wants` dit si l'image est attendue par la cadence : une capture
		// d'ecran coute cher, on ne la prend que si elle va servir.
		bool Demo3DHostRecTutoStart(int32 w, int32 h);
		bool Demo3DHostRecTutoStop(bool keep);
		bool Demo3DHostRecTutoActive();
		void Demo3DHostRecTutoPause(bool on);
		bool Demo3DHostRecTutoPaused();
		int32 Demo3DHostRecTutoFrames();
		int32 Demo3DHostRecTutoDropped();
		const char *Demo3DHostRecTutoPath();
		bool Demo3DHostRecTutoWants(float32 dt);
		bool Demo3DHostRecTutoPush(const uint8 *rgba, int32 w, int32 h);
		int32 Demo3DHostOutFastCount();
		const char *Demo3DHostOutFastName(int32 i);
		int32 Demo3DHostOutFastMask();
		void Demo3DHostSetOutFastMask(int32 m);
		int32 Demo3DHostOutInsetAdd(); // -1 si les emplacements sont pris
		void Demo3DHostOutInsetDelete(int32 i);
		int32 Demo3DHostOutInsetShapeCount();
		const char *Demo3DHostOutInsetShapeName(int32 s);
		// Lance le rendu. Il s'etale sur plusieurs images (le GPU doit rendre
		// entre le redimensionnement de la cible et la lecture de ses pixels),
		// donc il rend vrai s'il a demarre, pas s'il a fini : Busy dit qu'il
		// travaille, LastPath/LastOk disent ce qu'il a produit.
		bool Demo3DHostRenderOutput();
		// Meme rendu, sous le nom d'une capture : « Rendre l'image » et
		// « Capturer la vue » font EXACTEMENT le meme travail -- resolution,
		// source, echelle, incrustations, types de rendu -- et ne different que
		// par le nom du fichier. `which` : 0 rendu, 1 vue, 2 tutoriel.
		bool Demo3DHostRenderOutputAs(int32 which);
		bool Demo3DHostOutBusy();
		const char *Demo3DHostOutLastPath();
		bool Demo3DHostOutLastOk();
		// Cameras de la scene, pour les listes de sources (rend le nombre ecrit).
		int32 Demo3DHostSceneCameras(int32 *out, int32 cap);
		// FORMATS reellement ecrivables par NKImage. WebP et SVG en sont
		// absents : ils y sont declares mais annonces « non implemente », et
		// les proposer aurait produit des fichiers vides.
		int32 Demo3DHostOutFormatCount();
		const char *Demo3DHostOutFormatName(int32 f);
		const char *Demo3DHostOutFormatExt(int32 f);
		bool Demo3DHostOutFormatLossy(int32 f); // seul le JPEG lit la qualite
		// Porte-t-il la transparence ? Proposer un fond transparent pour un
		// format qui n'a pas d'alpha produirait un fond NOIR sans le dire.
		bool Demo3DHostOutFormatAlpha(int32 f);
		// Format « Libre » : un ETAT, pas une deduction. Le format nomme se
		// deduit bien de la resolution -- taper 1920x1080 affiche « Full HD » --
		// mais « Libre » ne se deduit de rien, et sur une resolution qui vaut
		// justement un format connu il serait sinon impossible a choisir.
		bool Demo3DHostOutFreeSize();
		void Demo3DHostSetOutFreeSize(bool on);
		int32 Demo3DHostOutQuality();
		void Demo3DHostSetOutQuality(int32 q);
		// MODES DE RENDU A PRODUIRE, en masque de bits (Rihen : « cocher les
		// types de rendu qu'on veut »). Meme ordre que la touche Z de la vue.
		// Chaque mode coche produit SON image, incrustations comprises, et le
		// nom du fichier porte son suffixe. Masque nul = le mode courant seul.
		// AIDES VISUELLES DANS LE RENDU, en masque de bits. Coupees par defaut
		// -- une lumiere n'existe dans une image que par son effet -- mais
		// recouvrables : on veut parfois montrer justement la grille ou le
		// cadre d'une camera, dans une planche pedagogique. Le masquage est une
		// option, pas une regle du moteur (Rihen). « Tutoriel » n'y est pas
		// soumis : il photographie la fenetre telle qu'elle est.
		int32 Demo3DHostOutAidCount();
		const char *Demo3DHostOutAidName(int32 a);
		int32 Demo3DHostOutAids();
		void Demo3DHostSetOutAids(int32 mask);
		int32 Demo3DHostOutModeCount();
		const char *Demo3DHostOutModeName(int32 m);
		int32 Demo3DHostOutModes();
		void Demo3DHostSetOutModes(int32 mask);
		// VIDEO : la configuration se REGLE et se conserve, mais rien ne
		// pretend l'executer -- le rendu viendra plus tard (decision de Rihen).
		// QUALITE PROPRE A LA VIDEO : elle etait partagee avec l'image fixe,
		// alors qu'une image livrable veut 95 et qu'une video de session tient
		// tres bien a 75 en pesant trois fois moins.
		int32 Demo3DHostOutVideoQuality();
		void Demo3DHostSetOutVideoQuality(int32 q);
		void Demo3DHostOutVideo(bool *on, int32 *fps, int32 *first, int32 *last, int32 *cont);
		void Demo3DHostSetOutVideo(bool on, int32 fps, int32 first, int32 last, int32 cont);
		// CONTENEUR ET CODEC SEPARES (comme Blender) : le conteneur dit le
		// FICHIER, le codec dit COMMENT les images y sont ecrites. Tous les
		// couples n'existent pas -- la liste des codecs depend du conteneur,
		// et changer de conteneur ramene le codec a son premier choix.
		int32 Demo3DHostOutVidContCount();
		const char *Demo3DHostOutVidContName(int32 c);
		const char *Demo3DHostOutVidContExt(int32 c); ///< vide = c'est un dossier
		int32 Demo3DHostOutVidCodCount(int32 c);
		const char *Demo3DHostOutVidCodName(int32 c, int32 k);
		int32 Demo3DHostOutVidCod();
		void Demo3DHostSetOutVidCod(int32 k);
		// CURSEUR DANS LA VIDEO DE TUTORIEL : la capture de fenetre de l'OS ne
		// contient pas le pointeur. On le dessine, avec la trace de ses
		// dernieres positions -- sans quoi la video montre des menus qui
		// s'ouvrent tout seuls. Ne concerne QUE la video : une capture fixe
		// n'a pas de trajectoire a raconter.
		bool Demo3DHostOutCursor();
		void Demo3DHostSetOutCursor(bool on);
		// CONSERVER LE DOSSIER QOI (nom_qoi_numero) apres l'encodage : les
		// images sans perte de la prise valent une source de montage. Decoche
		// par defaut -- le dossier s'efface une fois la video construite.
		bool Demo3DHostOutKeepQoi();
		void Demo3DHostSetOutKeepQoi(bool on);
		// TOUTE VIDEO s'encode APRES la prise (l'H.264 tient 2,2 images/s la
		// ou la capture en produit 25 ; filmer directement donnait une video
		// onze fois trop rapide -- et meme le MJPEG sautait une image sur six).
		// La prise ecrit des images QOI sans perte ; la passe finale construit
		// le conteneur. Ces valeurs disent ou elle en est -- sans elles,
		// plusieurs minutes de travail passeraient pour un blocage.
		bool Demo3DHostRecEncoding();
		bool Demo3DHostRecTutoEncoding();
		void Demo3DHostRecEncodeProgress(int32 *done, int32 *total);
		void Demo3DHostRecTutoEncodeProgress(int32 *done, int32 *total);
		// Prochain chemin libre, dans le DOSSIER et au FORMAT de la sortie.
		// Les deux captures l'empruntent : il n'y a qu'une destination
		// configuree dans l'application. `which` choisit le nom de base --
		// 0 la sortie, 1 « Capturer la vue », 2 « Tutoriel » -- car un rendu
		// final, une capture de la scene et une photo de l'interface ne se
		// rangent pas sous le meme nom.
		bool Demo3DHostOutNextPath(char *out, int32 cap, int32 which = 0);
		const char *Demo3DHostCaptureName(int32 which); // 1 vue, 2 tutoriel
		void Demo3DHostSetCaptureName(int32 which, const char *n);
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
		// Loi d'attenuation de la lumiere : 0 = heritee ((1-d/portee)^2, celle
		// des scenes existantes), 1 = physique (1/d^2 fenetree, comme UE et
		// Blender — la portee ne fait que couper). Au choix PAR lumiere.
		int32 Demo3DHostLightAttMode(int32 node);
		void Demo3DHostSetLightAttMode(int32 node, int32 mode);
		// Profondeur d'ombre LINEAIRE des omnis (option LearnOpenGL) : l'atlas
		// recoit dist/portee, biais constant, coutures de faces effacees.
		bool Demo3DHostLightShadowLinear(int32 node);
		void Demo3DHostSetLightShadowLinear(int32 node, bool lin);
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
