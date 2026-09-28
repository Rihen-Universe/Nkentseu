// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// main.cpp — NkUIDesign : designer des interfaces a partir de composants declares.
//
// TROIS MODES :
//   `--probe`  : la sonde headless. Aucune fenetre, aucun GPU. C'est le TEMOIN
//                de chaque capacite ajoutee.
//   `--roundtrip=<dossier>` : l'ALLER-RETOUR du format `.nkgui`. Analyse chaque
//                `.nkgui` du dossier, le reemet, le reanalyse, et compare. C'est
//                le critere d'acceptation du lecteur/ecrivain, et il ne depend
//                d'aucun jugement. Sans dossier, prend le dossier courant.
//   `--roundtrip-controles` : les temoins de bruit, controles positifs et
//                negatifs de ce meme aller-retour. ⚠️ A LANCER AVANT DE CROIRE UN
//                TAUX : un banc qui ne sait dire que « oui » ne mesure rien.
//   `--valider=<dossier>` : la VALIDATION par role et par type (vocabulaire du
//                document 7). Separee de la lecture a dessein : un document
//                fautif doit rester ouvrable, sinon sa faute est incorrigeable.
//   (defaut)   : l'editeur fenetre.
//
// ⚠️ LE MODE SONDE EST TESTE AVANT TOUTE CREATION DE FENETRE, deliberement : la
//    sonde doit pouvoir tourner sur une machine sans GPU disponible, et un
//    `Init()` place avant elle rendrait ce mode inutilisable exactement quand on
//    en a besoin.
//
// =============================================================================
//  LE CHOIX DU BACKEND GRAPHIQUE (directive de Rodolf, 2026-08-18)
// =============================================================================
//  *« Pour toutes nos applications, on doit pouvoir choisir le backend graphique
//  entre ceux disponibles. »* Le mecanisme existait deja dans la coquille
//  (`NkEditorShellConfig::graphicsApi`) ; ce qui manquait, c'est qu'une
//  application l'EXPOSE. Ici :
//
//    --gfx=auto|opengl|vulkan|dx11|dx12|metal|software     (ligne de commande)
//    NK_GFX_API=...                                        (variable d'env)
//    --small                                               (fenetre 1024x640)
//
//  L'ordre est celui qu'on attend : la ligne de commande gagne sur la variable
//  d'environnement, qui gagne sur la detection automatique.
//
//  ⚠️ TROIS REGLES, ET ELLES SONT LA MOITIE DE L'INTERET DE LA DIRECTIVE :
//    1. **le choix est journalise au demarrage** — demande / source / retenu.
//       Sans trace, personne ne sait sur quoi il vient de mesurer ;
//    2. **un backend indisponible se DIT, il ne se remplace pas en silence.** Un
//       repli muet donne « ca repond toujours » — la pire des reponses, parce
//       qu'elle fait passer une API absente pour une API qui marche. Ici, un
//       backend demande et refuse fait ECHOUER le lancement, avec la raison ;
//    3. `metal` est accepte a l'ANALYSE et refuse a la RESOLUTION : l'enumeration
//       de la coquille (`NkEditorGfxApi`) n'a pas d'entree Metal. Le taire
//       reviendrait a lancer silencieusement autre chose sur macOS. **Manque
//       porte au canal** — c'est un fichier de NKEditorKit, pas d'ici.
// =============================================================================
#include <cstdio>

#include "NKEditorKit/NkEditorKit.h"
// ⚠️ L'umbrella ne tire PAS l'implementation NKCanvas, deliberement : le kit
// serait alors lie a NKCanvas chez TOUS ses consommateurs, y compris ceux qui
// rendent en NKRHI. C'est a l'application de choisir son backend et de
// l'inclure. Voir NkEditorShell::Init (2026-09-01).
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKEditorKit/NkAiPanneauImage.h" // NK_AI_IMAGE : le panneau IA rendu par l'application
#include "NKEditorKit/NkEditorScriptEvenements.h" // (Q9) NK_EVENEMENTS
#include "NKEditorKit/NkEditorModal.h" // le cadre modal du kit (choix Nouveau projet)
#include "NKEditorKit/NkThemeToGui.h"  // NkThemeUnpack : role de theme -> couleur de dessin
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKPlatform/NkEnv.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"

#include <cstdlib> // atof (levier --lignes=)

#include "NKMath/NkEarcut.h" // cas 42 : le triangulateur du remplissage concave
#include "Backend.h"
#include "Costume.h" // le costume exact Banani : polices 9-16 px + icônes (remandat 31/08)
#include "NkGuiRoundTrip.h"
#include "NkDocPoolControls.h"
#include "Panels.h"
#include "ExportDialogue.h" // ④ le dialogue d'export : un seul, deux portes
#include "Probe.h"
#include "NkCoquilleDocument.h" // LE MENU « Design » ET LA BARRE D'ETAT VIENNENT D'UN DOCUMENT
#include "SondeCoquille.h"      // --sonde-coquille : leur verdict, sans fenetre ni GPU
#include "SondeLecture.h"       // --sonde-lecture  : ouvrir un .nkgui, et ce que ca coute
#include "SondeEdition.h"       // --sonde-edition  : LE TEMOIN DE R1 -- une ligne, pas deux
#include "SondeImages.h"        // --sonde-images   : un `image:` arrive-t-il dessine, et aux bonnes proportions
#include "SondeEcouteurs.h"     // --sonde-ecouteurs: un evenement part-il du bon widget
#include "SondeToile.h"         // --sonde-toile    : le role Canvas et son cadrage
#include "SondeLangues.h"       // --sonde-langues  : `@t:cle` et la bascule a chaud
#include "NkUIDesignLangues.h"  // LA table de traduction de cette application
#include "SondePont.h"          // --sonde-pont     : les deux vues de la toile s'accordent-elles
#include "NKEditorKit/NkEditorImages.h" // LE chargeur d'images des .nkgui (kit : 2D comme 3D)
#include "DesignAIRecette.h" // --recette-ia : la preuve de recette du pipeline IA
#include "DesignIABoutEnBout.h" // --ia-bout-en-bout : taper, poser, annuler
#include "RecetteEdition.h"	 // --recette-edition : le contrat universel d'edition, par site
#include "RecetteProprietes.h" // --recette-proprietes : les listes de proprietes, par le geste
#include "TemoinRendu.h"	 // --temoin-rendu : le flux de commandes du peintre, diffable
#include "RecetteEcrivain.h" // --recette-ecrivain : NKUIDesign ECRIT un .nkgui, le monteur le remonte
#include "RecettePlacement.h" // --recette-placement : poser un widget, et garder le flux intact



using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NKUIDesign";
	d.appVersion = "0.2.0";
	return d;
})());

// ═════════════════════════════════════════════════════════════════════════════
//  L INTERRUPTEUR DES ANCIENS PANNEAUX -- reversible en un caractere
// ═════════════════════════════════════════════════════════════════════════════
//  0 = les panneaux des PLANCHES (Hierarchie a gauche, Inspecteur a droite).
//  1 = les quatre anciens (Palette, Composition, Proprietes, Preferences).
//
//  ⚠️ DEBRANCHER N EST PAS SUPPRIMER (Rodolf : « meme si tu laisses le code »).
//     Les quatre classes restent ecrites dans `Panels.h`, et elles restent
//     COMPILEES a 1 : ce drapeau ne les met pas au rebut, il decide seulement de
//     leur enregistrement aupres de la coquille.
//
//  ⚠️ ET CE N EST PAS UNE MIGRATION. Hierarchie n est pas Composition renomme,
//     Inspecteur n est pas Proprietes renomme : les anciens SORTENT, les
//     nouveaux sont batis sur les composants du kit. Confondre les deux aurait
//     conserve la structure qu on veut precisement quitter.
#define NKUIDESIGN_ANCIENS_PANNEAUX 0

// ═════════════════════════════════════════════════════════════════════════════
//  LES BARRES D ACTIVITE (les bandes verticales d icones aux deux bords)
// ═════════════════════════════════════════════════════════════════════════════
//  1 = presentes (defaut de la coquille) · 0 = retirees, le dock reprend la place.
//
//  ⚠️ MESURE FAITE AVANT DE RETIRER, parce que deux lectures etaient possibles et
//     qu elles ne se corrigent pas au meme endroit :
//       (A) un reste de chrome facon VS Code, herite de NKCode ;
//       (B) le RAIL DE PASTILLES gauche du plan (« rail — palette · bibliotheque »,
//           bande fine de 28 px, document 3 §13).
//     **C est (A), et trois mesures le disent :**
//       1. `--dump-ui` rend `panneau.hierarchie = 48.0 ...` : la bande fait
//          **48 px**, pas les 28 px que le plan exige d un rail ;
//       2. `NkEditorShell.h:173` la nomme lui-meme — « BARRES D ACTIVITE (bandes
//          verticales d icones, facon VSCode) : presentes par defaut, parce que
//          l IDE en vit. Une application qui n a PAS de vues a basculer doit
//          pouvoir les retirer : sinon elle herite du chrome de NKCode et lui
//          ressemble, alors qu elle ne fait pas le meme metier. » NkUIDesign est
//          exactement ce cas ;
//       3. son contenu (document, loupe, branche, lecture, personnages, grille,
//          histogramme, engrenage) ne correspond a aucune pastille du plan, qui
//          ne prevoit a gauche que « palette · bibliotheque ».
//
//  ⚠️ CONSEQUENCE A NE PAS PERDRE : **le rail de pastilles du plan reste a
//     construire.** Retirer cette bande ne le fabrique pas ; §13 (rails de 28 px,
//     pastilles a quatre etats) n a toujours aucun code. Croire la case cochee
//     parce que le bord est propre serait l erreur symetrique.
//
//  ⚠️ LA BARRE DE DROITE EST DE LA MEME NATURE, ET ELLE PART AUSSI. Meme appel,
//     meme largeur mesuree (l Inspecteur finissait a x = 1408 dans une fenetre de
//     1456, soit 48 px), meme absence de branchement : `SetActivityHandler` n est
//     jamais appele, donc ses trois pictogrammes ne repondent a rien. Garder a
//     droite une bande inerte apres avoir retire celle de gauche laisserait une
//     asymetrie que rien ne justifie -- ni le plan, ni le code.
#define NKUIDESIGN_BARRES_ACTIVITE_GAUCHE 0
#define NKUIDESIGN_BARRES_ACTIVITE_DROITE 0

static nkuidesign::DesignState gDesign;
static NkEditorShell *gShell = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
//  --capture=<fichier.png> : UNE IMAGE VRAIE DE L'APPLICATION, PUIS FERMER
// ═══════════════════════════════════════════════════════════════════════════
//  Le témoin de rendu (--temoin-rendu) dit CE QUI est dessiné ; il ne dit pas
//  comment ça se PROPORTIONNE à l'œil. Pour juger l'alignement et les
//  proportions — le côte à côte avec les planches — il faut des pixels.
//
//  ⚠️ CE MODE OUVRE SA PROPRE FENÊTRE ET NE TOUCHE QU'À ELLE. Ce n'est PAS une
//     capture d'écran de l'OS : c'est un readback du backbuffer de CETTE
//     instance (NkRenderWindow::Capture, le code du noyau — pas une neuvième
//     recopie, cf. CAPTURE_MONTAGE_ET_IA_DANS_LES_APPS.md §2). Une instance
//     déjà ouverte ailleurs n'est ni photographiée, ni fermée, ni même vue.
//
//  ⚠️ POURQUOI ATTENDRE HUIT FRAMES : la coquille restaure l'état de fenêtre de
//     la session précédente, remonte les atlas de police et stabilise le dock
//     dans les premières frames. Capturer la frame 1 photographierait un
//     échafaudage. Huit est un choix (généreux), pas une mesure — s'il se
//     révèle court un jour, le symptôme sera une image sans texte.
//
//  Le tick vit dans le callback de menu applicatif (SetAppMenu) : c'est le seul
//  crochet PAR FRAME que la coquille offre à l'app aujourd'hui. Le jour où
//  l'app veut un vrai menu applicatif, ce squat devra déménager — d'ici là il
//  ne dessine rien (il n'ajoute aucun menu).
static char gCapturePath[512] = {0};
static int32 gCaptureFrame = 0;
/// Vrai si AUCUN clic, glisser ni cran de molette n'est programme. Defini plus
/// bas, a cote des tableaux qu'il interroge -- ici seulement declare, parce que
/// `CaptureTick` s'ecrit avant eux.
static bool AucuneEntreeProgrammee();
/// LA POSITION QUE L'INJECTEUR A POSEE POUR CETTE IMAGE. Ecrite par
/// `InjecterClics`, lue par `CaptureTick` -- qui passe APRES lui et doit donc la
/// REPOSER au lieu de l'ecraser.
static bool gSourisSondePosee = false;
static float32 gSourisSondeX = 0.f, gSourisSondeY = 0.f;
// ⚠️ PLUS UNE CONSTANTE : un geste dure. Mesure du 14/09 -- un glisser de la
//    Bibliotheque vers la toile demande des dizaines d'images, et la capture
//    fermait la fenetre bien avant. Le banc rendait alors « 0 pixel a
//    change » : une reponse verte a une question jamais posee, la meme
//    famille que `--clic` ecrase par le tick de capture. `--capture-frame=<n>`
//    repousse la photo ; le defaut reste 8, donc aucune mise en scene ne bouge.
static int32 kCaptureFramePrete = 8;
/// (R20) la capture est armee (une seule fois) -- cf. le pave de `CaptureTick`
static bool gCaptureArmee = false;
/// --selectionner=<libellé> : le nœud à sélectionner AVANT la photo.
///
/// ⚠️ NÉ D'UNE PREUVE EN CREUX (E6 du doc 17, payée le jour même) : un
///    changement du retrait des champs de saisie a traversé DEUX témoins
///    verts — le flux (pas de glyphes en headless) et la capture (la Toile
///    sélectionnée ne dessine aucun champ). *Une capture d'un état vide ne
///    témoigne que du vide.* Ce drapeau met l'inspecteur dans l'état PLEIN.
static char gSelectionner[128] = {0};
// --scene-fusion : avant la photo, six paires de rectangles superposes, un mode de
// fusion par paire -- le temoin en PIXELS des modes exacts, sur le dorsal reel (②-2).
static bool gSceneFusion = false;
// ⚠️ PAS de gPanneau ici : `--panneau=<titre>` EXISTE DEJA (gPanneauInitial,
//    mise en scene ecran 9). J'en avais ecrit un doublon avant de chercher —
//    la porte « chercher avant d'ecrire » vaut aussi pour ses propres ajouts.

// ═══════════════════════════════════════════════════════════════════════════
//  LE BANC DE L'ASYNCHRONE (g3) — --mesure-async=<ms>[:sync] et --mesure-fps=<ms>
// ═══════════════════════════════════════════════════════════════════════════
//  Ce qu'il faut prouver, et pourquoi il faut DEUX chiffres et non un :
//    - la fenetre VIT pendant la generation -> on compte les IMAGES ;
//    - le modele n'est PAS ralenti pour ca  -> on compte les SECONDES.
//  Un seul des deux se truque en abimant l'autre : on peut rendre la fenetre
//  fluide en decoupant le travail, et on peut rendre le travail rapide en
//  gelant la fenetre. Les deux ensemble ne se truquent pas.
//
//  ⚠️ LE DORSAL DU BANC DORT, il n'appelle aucun modele. C'est voulu : il
//     reproduit EXACTEMENT ce que fait un fil pendant un appel bloquant (rien,
//     longtemps), sans occuper la carte sept minutes et sans dependre de ce
//     qu'un modele repond. Un banc cale sur le vrai modele dependrait de la VRAM
//     libre, du pilote et de la longueur de l'invite.
//
//  ⚠️ `:sync` EST LE NEGATIF, et c'est lui qui donne un sens au chiffre : il
//     emprunte l'ANCIEN chemin bloquant. S'il ne rendait pas ~1 image, le banc
//     ne saurait pas distinguer une fenetre vivante d'une fenetre figee, et le
//     « N images » du chemin asynchrone ne prouverait rien.
static nkentseu::int64 gMesureAsyncMs = -1;
static bool gMesureAsyncSync = false;
/// `--mesure-async=<ms>:prop` : la meme mesure, mais par le chemin de
/// « Proposer (apercu) » -- le bouton ou Rodolf a vu le gel.
static bool gMesureAsyncProp = false;
static nkentseu::int64 gMesureFpsMs = -1;
/// (k2) --mesure-double=<images> : combien de fois la toile est-elle dessinee
/// dans UNE image ? Le seul chiffre acceptable est 1.
static nkentseu::int64 gMesureDoubleImages = -1;
/// (c2) --mesure-texte=<images>[:<largeurForcee>] : combien de textes sortent de
/// leur rognage. Attendu en conditions normales : ZERO. La largeur forcee est la
/// PREUVE DE NON-MUTISME -- a 20 px, tout doit deborder.
static nkentseu::int64 gMesureTexteImages = -1;
static float32 gMesureTexteLargeur = 0.f;
// 🔴 DEUX COMPTEURS QUI NE COMPTENT PAS LA MEME CHOSE -- mesure du 14/09.
//    `mAppMenuFn` (ou vit ce tick) est appele DEUX FOIS par image par la
//    coquille ; `mMenuBarFn` (ou vit la recolte) UNE fois. Compter la reference
//    avec l'un et la generation avec l'autre donnait « 278 images/s au repos
//    contre 140 pendant la generation » -- une chute de moitie entierement
//    IMAGINAIRE : les deux chemins tournaient a la MEME cadence.
//    C'est « un compteur dont le zero n'est pas zero » en version double : deux
//    compteurs de cadences differentes, compares comme s'ils etaient le meme.
//    UN SEUL compteur d'images desormais, incremente UNE fois par image dans
//    `DrawMenuBar`, et les deux modes le lisent.
static int32 gImagesReelles = 0;
static int32 gMesureImages = 0;
static bool gMesureLancee = false;
static nkentseu::NkChrono gMesureHorloge;

/// Le panneau IA, pour que le banc puisse le piloter. Pose au montage.
static nkuidesign::AIPanel *gPanneauIA = nullptr;
/// (Q6) Le fichier de l'etat d'interface (largeur du panneau de droite...).
static char gCheminEtatUi[260] = {0};
/// (R19) La toile, pour que la sonde des portes ouvre ses menus du clic droit.
static nkuidesign::PreviewPanel *gPanneauToile = nullptr;

/// ── NK_AI_IMAGE=<chemin>,<image> : LE PANNEAU IA, RENDU PAR L'APPLICATION ──
/// La preuve exigee le 21/09 : une IMAGE du panneau, rendue par NKUIDesign
/// lui-meme -- la liste d'affichage COMPLETE de cette image (fenetres fusionnees,
/// surcouches posees), rasterisee sans GPU, decoupee au rectangle que le panneau
/// a PUBLIE. Jamais une capture de l'ecran. `<chemin>.png` = le panneau,
/// `<chemin>_fenetre.png` = la fenetre entiere.
static void ImagePanneauIA(nkentseu::nkgui::NkGuiContext &ui, nkentseu::int32 W, nkentseu::int32 H, void *user) {
	using namespace nkentseu;
	static int32 sCible = -2;
	static int32 sApres = 0;
	static int32 sImage = 0;
	static char sChemin[256] = {0};
	if (sCible == -2) {
		sCible = -1;
		if (const char *v = std::getenv("NK_AI_IMAGE")) {
			const char *virg = nullptr;
			for (const char *c = v; *c; ++c)
				if (*c == ',')
					virg = c;
			uint32 n = 0;
			for (const char *c = v; *c && (!virg || c < virg) && n + 1u < sizeof(sChemin); ++c)
				sChemin[n++] = *c;
			sChemin[n] = 0;
			// `apres:<n>` : n images APRES la premiere reponse recoltee -- la photo
			// dit alors ce que la reponse a produit, quel que soit son temps.
			if (virg && std::strncmp(virg + 1, "apres:", 6) == 0) {
				sApres = (int32)std::atoi(virg + 7);
				sCible = 0;
			} else
				sCible = virg ? (int32)std::atoi(virg + 1) : 120;
		}
	}
	++sImage;
	if (sApres > 0 && sCible == 0 && gPanneauIA && gPanneauIA->Recoltes() > 0)
		sCible = sImage + sApres;
	if (sCible <= 0 || sImage != sCible)
		return;
	auto &F = nkuidesign::costume::Fontes();
	const nkgui::NkGuiFont *polices[9] = {ui.font, &F.px9, &F.px10, &F.px11, &F.px12,
										  &F.px13, &F.px15, &F.px16, &F.mono};
	const nkgui::NkGuiDrawList *listes[2] = {&ui.dl, &ui.dlOverlay};
	char c1[300], c2[300];
	snprintf(c1, sizeof(c1), "%s.png", sChemin);
	snprintf(c2, sizeof(c2), "%s_fenetre.png", sChemin);
	const editorkit::NkPaintRect r = gPanneauIA ? gPanneauIA->Panneau().rect : editorkit::NkPaintRect{};
	const uint32 fond = gDesign.theme.Get(editorkit::NkRole::WindowBg);
	const editorkit::NkAiImageResultat r1 =
		editorkit::NkAiEcrireImageListes(listes, 2, W, H, r.x, r.y, r.w, r.h, polices, 9, fond, c1);
	const editorkit::NkAiImageResultat r2 = editorkit::NkAiEcrireImageListes(
		listes, 2, W, H, 0.f, 0.f, (float32)W, (float32)H, polices, 9, fond, c2);
	// ⚠️ LE PANNEAU A-T-IL ETE PEINT A CETTE IMAGE ? Son rectangle est celui de la
	//    DERNIERE peinture : un tiroir referme laisserait un rectangle perime, et
	//    l'image montrerait autre chose sous ce rectangle sans que rien ne le dise.
	printf("[NKUIDesign] AI IMAGE image=%d panneau=(%.0f,%.0f,%.0f,%.0f) peint %u fois : %s | %s\n", (int)sImage,
		   (double)r.x, (double)r.y, (double)r.w, (double)r.h, gPanneauIA ? (unsigned)gPanneauIA->ImagesPeintes() : 0u,
		   r1.ok ? r1.message : "ECHEC", r2.ok ? r2.message : "ECHEC");
	fflush(stdout);
	// NK_AI_QUITTER : la photo prise, la sonde se ferme d'elle-meme (Q5) -- une
	// reponse locale peut prendre une minute, un `--capture-frame` fixe non.
	if (std::getenv("NK_AI_QUITTER") && user)
		static_cast<NkEditorShell *>(user)->RequestClose();
}

/// LE TICK DU BANC. Il vit dans le meme crochet par image que la capture -- la
/// coquille n'en offre qu'un, et l'un exclut l'autre (on ne photographie pas une
/// fenetre qu'on mesure).
static void MesureTick(NkEditorShell *sh) {
	++gMesureImages;
	// --mesure-fps : on ne fait RIEN pendant `ms`, on compte les images. C'est la
	// REFERENCE : sans elle, « N images pendant la generation » serait un nombre
	// sans echelle.
	if (gMesureFpsMs >= 0) {
		if (!gMesureLancee) {
			gMesureLancee = true;
			gImagesReelles = 0;
			gMesureHorloge = nkentseu::NkChrono();
			return;
		}
		const float64 sec = gMesureHorloge.Elapsed().ToSeconds();
		if (sec * 1000.0 >= (float64)gMesureFpsMs) {
			printf("[mesure-fps] repos : %d images en %.3f s -> %.1f images/s\n",
				   gImagesReelles, sec, (float64)gImagesReelles / (sec > 0.0 ? sec : 1.0));
			fflush(stdout);
			sh->RequestClose();
		}
		return;
	}
	// (c2) LE RELEVE DES TEXTES QUI DEBORDENT DE LEUR ROGNAGE.
	if (gMesureTexteImages >= 0) {
		nkuidesign::costume::NkReleveTexte &rel = nkuidesign::costume::Releve();
		if (!gMesureLancee) {
			// ⚠️ ON NE JUGE PAS LES PREMIERES IMAGES : la coquille restaure l'etat
			//    de fenetre et stabilise le dock ; un texte mesure pendant que sa
			//    colonne n'a pas sa largeur finale deborderait pour rien.
			if (gImagesReelles < 10)
				return;
			gMesureLancee = true;
			rel.largeurForcee = gMesureTexteLargeur;
			rel.actif = true;
			rel.Reinitialiser();
			gImagesReelles = 0;
			return;
		}
		if (gImagesReelles >= (int32)gMesureTexteImages) {
			rel.actif = false;
			printf("[mesure-texte] %u texte(s) examine(s) sur %d images ; "
				   "%u COUPE(S) par le rognage",
				   rel.examines, gImagesReelles, rel.coupes);
			if (gMesureTexteLargeur > 0.f)
				printf(" [largeur FORCEE a %.0f px]", gMesureTexteLargeur);
			printf("\n");
			if (rel.coupes > 0)
				printf("[mesure-texte]    premier : \"%s\"\n"
					   "[mesure-texte]    pire    : \"%s\"  (deborde de %.1f px)\n",
					   rel.premier, rel.pire, rel.pireDebord);
			fflush(stdout);
			sh->RequestClose();
		}
		return;
	}
	// (k2) LE COMPTE DE DESSINS DE LA TOILE. On laisse passer `n` images puis on
	// rend le PIRE vu. Attendu : 1. A 2, deux exemplaires vivants du meme panneau
	// se disputent un seul etat de vue -- le defaut du 14/09, rouvert.
	if (gMesureDoubleImages >= 0) {
		if (gImagesReelles >= (int32)gMesureDoubleImages) {
			printf("[mesure-double] la toile est dessinee au plus %u fois dans une image "
				   "(sur %d images ; attendu 1)\n",
				   gDesign.dessinsToileMax, gImagesReelles);
			fflush(stdout);
			sh->RequestClose();
		}
		return;
	}
	if (gMesureAsyncMs < 0 || !gPanneauIA)
		return;
	if (!gMesureLancee) {
		// ⚠️ PAS A LA PREMIERE IMAGE : la coquille remonte les atlas et stabilise
		//    le dock dans les premieres images. Lancer la-dedans compterait des
		//    images de demarrage comme des images d'attente.
		if (gMesureImages < 4)
			return;
		gMesureLancee = true;
		gImagesReelles = 0;
		gMesureHorloge = nkentseu::NkChrono();
		if (gMesureAsyncProp)
			gPanneauIA->BancAsyncProposer(gMesureAsyncMs);
		else
			gPanneauIA->BancAsyncLancer(gMesureAsyncMs, gMesureAsyncSync);
		if (gMesureAsyncSync) {
			// Le chemin bloquant a DEJA rendu la main : tout s'est passe dans
			// cette seule image. C'est exactement ce que le negatif doit montrer.
			printf("[mesure-async] SYNCHRONE : %d image(s) pendant l'attente, "
				   "travail %.3f s (attendu %.3f s)\n",
				   1, gPanneauIA->BancAsyncSecondes(), (float64)gMesureAsyncMs / 1000.0);
			fflush(stdout);
			sh->RequestClose();
		}
		return;
	}
	if (gPanneauIA->BancAsyncEnCours())
		return;
	printf("[mesure-async] ASYNCHRONE : %u image(s) pendant l'attente, "
		   "travail %.3f s (attendu %.3f s) -> %.1f images/s\n",
		   gPanneauIA->BancAsyncImages(), gPanneauIA->BancAsyncSecondes(),
		   (float64)gMesureAsyncMs / 1000.0,
		   (float64)gPanneauIA->BancAsyncImages()
			   / (gPanneauIA->BancAsyncSecondes() > 0.0 ? gPanneauIA->BancAsyncSecondes() : 1.0));
	fflush(stdout);
	sh->RequestClose();
}

static void CaptureTick(NkEditorFrameContext &ec, void *user) {
	NkEditorShell *sh = static_cast<NkEditorShell *>(user);
	++gCaptureFrame;
	// ⚠️ LE CURSEUR PHYSIQUE FUIT DANS LA PHOTO — mesure le 02/09 : deux
	//    captures du MEME etat differaient sur un bouton… parce que la souris
	//    de la machine survolait ce bouton pendant l'une des deux. La fenetre
	//    s'ouvre sous le curseur, ou qu'il soit ; le survol depend donc d'ou
	//    la main de l'utilisateur a laisse sa souris. On neutralise DANS NOTRE
	//    CONTEXTE (jamais la vraie souris : elle ne nous appartient pas) : la
	//    position est repoussee hors ecran a chaque frame, avant les panneaux.
	//
	// 🔴 MAIS PAS QUAND UNE ENTREE EST PROGRAMMEE — defaut d'INSTRUMENT mesure
	//    le 14/09. `mAppMenuFn` (ce tick) est appele DEUX FOIS par image par la
	//    coquille : une fois tres tot (NkEditorShell.cpp l.817) et une fois
	//    APRES la barre de menus (l.2358). Or c'est la barre de menus qui
	//    appelle `InjecterClics`. Le second passage ecrasait donc la position
	//    que le clic venait de poser, a chaque image, sans rien dire.
	//    Consequence : `--clic` et `--capture` ne pouvaient PAS servir ensemble,
	//    et une mesure qui les combinait rendait « 0 pixel a change » -- une
	//    reponse VERTE a une question jamais posee. C'est la famille « un
	//    negatif incapable de refuter » : sa construction garantissait le
	//    resultat.
    // 🔴 ET LA PREMIERE VERSION DE CE CORRECTIF ETAIT FAUSSE, MESUREE FAUSSE :
	//    elle se contentait de NE PAS neutraliser quand une entree etait
	//    programmee. Resultat : entre deux ecritures de l'injecteur, la position
	//    du CURSEUR PHYSIQUE revenait -- la fuite que le commentaire ci-dessus
	//    decrit depuis le 02/09, rouverte par sa propre correction. Symptome
	//    mesure : la MEME commande a rendu une fois 4 923 pixels changes hors du
	//    tiroir (l'outil Texte s'etait arme tout seul) et cinq fois zero. Un
	//    banc qui n'est pas repetable ne mesure pas ce qu'il croit ; j'ai failli
	//    rapporter ce 4 923 comme une preuve.
	//    La regle est donc : **on force la position A CHAQUE IMAGE**, soit celle
	//    que la sonde a posee, soit hors ecran. Jamais celle de la machine.
	if (gSourisSondePosee)
		ec.Ui().input.mousePos = {gSourisSondeX, gSourisSondeY};
	else
		ec.Ui().input.mousePos = {-10000.f, -10000.f};
	if (gCaptureFrame == 1 && gSceneFusion) {
		// LA SCENE DES MODES DE FUSION : fond #808080, dessus #606060 (#a0a0a0 pour
		// Lighten, sinon max = le fond) ; attendu au pixel : normal #606060, multiply
		// #303030, screen #b0b0b0, darken #606060, lighten #a0a0a0, plus lighter #e0e0e0.
		using namespace nkuidesign;
		gDesign.doc.NewDocument("Scene fusion", NkAuthor::Humain);
		gDesign.doc.nodes[0].layout.kind = NkLayoutKind::Free;
		static const char *const kModes[6] = {"", "multiply", "screen", "darken", "lighten", "plus-lighter"};
		for (nkentseu::uint32 k = 0; k < 6u; ++k) {
			for (nkentseu::uint32 couche = 0; couche < 2u; ++couche) {
				const nkentseu::int32 id = gDesign.doc.AddChild(0, "", NkAuthor::Humain);
				if (id < 0)
					continue;
				NkUINode &n = gDesign.doc.nodes[(nkentseu::uint32)id];
				n.shape = NkString("rect");
				n.label = NkString(couche == 0u ? "fond" : kModes[k][0] ? kModes[k] : "normal");
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = couche == 0u ? 120.f : 80.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = couche == 0u ? 120.f : 80.f;
				n.posX = 40.f + 140.f * (nkentseu::float32)k + (couche == 0u ? 0.f : 20.f);
				n.posY = 60.f + (couche == 0u ? 0.f : 20.f);
				NkRemplissage f;
				f.couleur = NkString(couche == 0u ? "#808080" : (k == 4u ? "#a0a0a0" : "#606060"));
				if (couche == 1u && kModes[k][0])
					f.fusion = NkString(kModes[k]);
				n.fills.PushBack(f);
			}
		}
		gDesign.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
		gDesign.host.SyncTo(gDesign.doc);
		gDesign.SelectClear();
		puts("[NKUIDesign] --scene-fusion : six paires posees (normal, multiply, screen, darken, lighten, plus-lighter)");
	}
	if (gCaptureFrame == 1 && gSelectionner[0]) {
		// ⚠️ PLUSIEURS LIBELLES, SEPARES PAR VIRGULE — parce que Rodolf dit
		//    « l'objet OU LES OBJETS », et qu'un banc qui ne sait selectionner
		//    qu'un seul noeud ne peut pas photographier la multi-selection.
		//    *Un harnais qui ne sait pas poser la question ne mesure rien.*
		nkentseu::int32 trouve = -1;
		char un[128];
		const char *lecture = gSelectionner;
		bool premier = true;
		while (*lecture) {
			nkentseu::uint32 l = 0;
			while (*lecture && *lecture != ',' && l + 1u < (nkentseu::uint32)sizeof(un))
				un[l++] = *lecture++;
			un[l] = '\0';
			while (*lecture == ',' || *lecture == ' ')
				++lecture;
			nkentseu::int32 ici = -1;
			for (nkentseu::uint32 k = 0; k < (nkentseu::uint32)gDesign.doc.nodes.Size(); ++k)
				if (gDesign.doc.nodes[k].label.Data()
					&& 0 == strcmp(gDesign.doc.nodes[k].label.Data(), un)) {
					ici = (nkentseu::int32)k;
					break;
				}
			if (ici < 0) {
				fputs("[NKUIDesign] --selectionner : libelle introuvable : ", stdout);
				puts(un);
				continue;
			}
			if (premier) {
				gDesign.SelectSingle(ici);
				premier = false;
			} else
				gDesign.SelectToggle(ici);
			trouve = ici;
		}
		if (trouve >= 0) {
			// la selection est posee ci-dessus
		} else {
			// Le manque SE DIT : une photo prise « quand même » sans le dire
			// redeviendrait le témoin d'un état vide qu'on croit plein.
			fputs("[NKUIDesign] --selectionner : libelle introuvable : ", stdout);
			puts(gSelectionner);
		}
	}
	// 🔴 (R20) LA CAPTURE SE DECLENCHE SUR L'IMAGE REELLE, PAS SUR CE TICK.
	//    Mesure du 17/09 : `mAppMenuFn` est appele DEUX FOIS par image (le commentaire
	//    ci-dessus le dit deja), donc `gCaptureFrame` compte des DEMI-IMAGES.
	//    `--capture-frame=105` photographiait l'image reelle ~52. Consequence mesuree :
	//    un glisser programme a l'image 60 n'avait PAS COMMENCE sur la photo, et trois
	//    captures censees montrer trois etats differents sont sorties IDENTIQUES AU BIT
	//    (meme SHA-256) -- « un negatif incapable de refuter », pour la quatrieme fois.
	//    `gImagesReelles` est la seule cadence de reference (une incrementation par image,
	//    dans `DrawMenuBar`). ⚠️ Un banc qui demandait 105 photographie desormais l'image
	//    105 et non ~52 : plus tard, donc plus stable, mais ce n'est PAS le meme instant.
	if (!gCaptureArmee && gImagesReelles >= kCaptureFramePrete) {
		gCaptureArmee = true;
		printf("[NKUIDesign] capture armee a l'image reelle %d (tick d'application %d)\n",
			   gImagesReelles, gCaptureFrame);
		fflush(stdout);
		// Armée ICI, exécutée par le backend APRÈS le Display() de cette même
		// frame — le seul moment que le contrat du readback autorise.
		if (sh->Renderer() && !sh->Renderer()->CaptureNext(gCapturePath)) {
			// Refus d'armement (chemin trop long, backend sans readback) : rien
			// n'arrivera au EndFrame — fermer tout de suite, le verdict de fin
			// (fichier absent) dira l'échec.
			sh->RequestClose();
		}
	} else if (gCaptureArmee && gImagesReelles > kCaptureFramePrete) {
		// La frame SUIVANTE : la capture de la frame précédente est faite (ou
		// pas — le fichier en témoignera). Fermer proprement.
		sh->RequestClose();
	}
}
/// L onglet de projet actif. ⚠️ UN SEUL ETAT, ici : le dessin de la bande et le
///    clic le lisent tous les deux. Deux copies auraient diverge des le premier
///    onglet ferme.
// (l'onglet actif vit desormais dans DesignState::ongletActif — multi-documents)
// L'etat « non enregistre » MESURE (pousse par le canal gDesign.titre) : la
// barre de titre ET l'onglet actif portent la meme pastille « ● ».
static bool gDocumentModifie = false;
// Mise en scene « toile seule » (ecrans gros plan de la maquette) :
// panneaux fermes, rails retires — pose par --toile-seule.
static bool gToileSeule = false;
// ⚠️ --titre-sonde : LA FENETRE SE DENONCE.
//    Le 14/09, une fenetre ouverte par un agent est restee SEPT MINUTES a
//    l'ecran sous le titre du PRODUIT (le temps d'une generation synchrone).
//    Rodolf pouvait la prendre pour son application -- c'est arrive une fois
//    deja dans ce depot, et le titre est le seul endroit qu'on regarde.
//    Ce drapeau n'est JAMAIS pose par un lancement normal : sans lui, pas un
//    caractere ne change. Avec lui, le bandeau de titre porte la phrase, et le
//    rappel du document vient APRES -- un agent ne doit pas avoir a se souvenir
//    de le faire, il doit avoir a se souvenir de NE PAS le faire.
static bool gTitreSonde = false;
static const char *const kTitreSonde = "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT *** ";
// Tiroir de rail a ouvrir au lancement (--tiroir=d:0) : 0 = aucun.
static char gTiroirCote = 0;
static char gPanneauInitial[48] = {0};
static int32 gTiroirIndex = -1;
/// ⚠️ L AUTORITE DES THEMES, ET ELLE EST UNIQUE. `gDesign.theme` (lu par les
///    composants du kit) et `mUI.theme` de la coquille (lu par les primitives)
///    en sont deux CONSOMMATEURS ; ils ne decident rien.
static nkentseu::editorkit::NkThemeLibrary gThemes;
/// Theme demande en ligne de commande, vide si aucun.
static NkString gThemeDemande;

/// Bascule de theme : la bibliotheque decide, puis POUSSE vers les deux
/// consommateurs. Un seul chemin -- c'est ce qui interdit qu'une moitie de la
/// fenetre reste dans l'ancien theme.
static void AppliquerTheme(uint32 i) {
	if (i >= gThemes.Count())
		return;
	gThemes.SetCurrentIndex(i);
	gDesign.theme = gThemes.Current();
	if (gShell) {
		gShell->ApplyTheme(gThemes.Current());
		gShell->SetFooter("Thème : ", gThemes.Current().Name().CStr());
	}
}

// ④ L'EXPORT : le raccourci et le menu ouvrent LE MEME dialogue. Il vise la selection
//    quand il y en a une, la page sinon -- et le dialogue le dit avant de rien ecrire.
static void CmdExporter(void *) {
	if (gDesign.SaisieOuverte())
		return; // jamais au milieu d'un renommage (meme garde que Ctrl+D)
	nkuidesign::NkOuvrirDialogueExport(gDesign, !gDesign.sel.Empty() || gDesign.selected > 0);
}
static void CmdSave(void *) {
	gDesign.SaveDoc();
}
// L'ANNULATION UNIFIEE (§7 : « une action Behavior est annulable comme une
// action Design ») — voir Historique.h : instantanés de sérialisation, un
// geste = un pas, tout type de geste confondu.
static void CmdUndo(void *) {
	gDesign.Annuler();
}
static void CmdRedo(void *) {
	gDesign.Retablir();
}
static void CmdLoad(void *) {
	gDesign.LoadDoc();
}
static void CmdNew(void *) {
	// Multi-documents (01/09) : Ctrl+N OUVRE UN NOUVEL ONGLET — il n'ecrase
	// plus le document courant (5e retour de Rodolf).
	gDesign.NouvelOngletVierge();
}
// ── LES GESTES D'EDITION (Lunacy) BRANCHES SUR LA COQUILLE ──────────────────
// ⚠️ CTRL+D ET CTRL+G PASSENT PAR `RegisterCommand`, PAS PAR LES DRAPEAUX
//    `want*`. La raison est dans NKGui : `wantCopy/Cut/Paste/SelectAll` sont
//    les QUATRE drapeaux que la coquille leve et que les CHAMPS TEXTE
//    consomment — il n'y a pas de `wantDuplicate`. Ctrl+D et Ctrl+G n'ont
//    donc qu'un chemin : la table de commandes, la meme que Ctrl+S et Ctrl+Z.
//
// ⚠️ ET C'EST POUR CA QUE CHACUNE COMMENCE PAR `SaisieOuverte()`.
//    `NkEditorShell` execute ses raccourcis « meme pendant la frappe »
//    (NkEditorShell.cpp, callback de touche) — c'est voulu pour Ctrl+S, c'est
//    un piege pour Ctrl+D : sans cette garde, dupliquer partirait au milieu
//    d'un renommage. Les quatre `want*`, eux, sont deja gardes dans la toile
//    (meme condition que les lettres d'outil).
static void CmdDupliquer(void *) {
	if (gDesign.SaisieOuverte())
		return;
	gDesign.DupliquerSelection();
}
static void CmdGrouper(void *) {
	if (gDesign.SaisieOuverte())
		return;
	gDesign.GrouperSelection();
}
static void CmdDegrouper(void *) {
	if (gDesign.SaisieOuverte())
		return;
	gDesign.DegrouperSelection();
}

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-annulation : LA BATTERIE DE PREUVE DE L'ANNULATION (§7)
// ═════════════════════════════════════════════════════════════════════════════
// CHAQUE TYPE DE GESTE, suivi d'un Annuler -> serialisation IDENTIQUE OCTET
// POUR OCTET a l'etat d'avant, puis d'un Retablir -> identique a l'etat
// d'apres. Contre-epreuve finale : N gestes, N Annuler -> l'etat INITIAL
// exact. Le MEME mecanisme que l'interface (DesignState::Annuler/Retablir,
// NkHistorique::Observer), pas une reimplementation de banc — la lecon T5 :
// un cote de la mesure vient d'ailleurs que du code teste (la serialisation,
// prouvee par le round-trip). Sans fenetre ni GPU.

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-gestes : LES RACCOURCIS D'EDITION (Lunacy) PROUVES PAR LEUR EFFET
// ═════════════════════════════════════════════════════════════════════════════
// Copier/Couper/Coller/Dupliquer/Grouper/Degrouper/Supprimer-multi/Tout
// selectionner. Sans fenetre ni GPU : le VRAI code de DesignState, celui que
// le clavier, le menu Edition et le menu contextuel appellent tous les trois.
//
// ⚠️ CE QUE CETTE RECETTE N'EXERCE PAS, ET ELLE LE DIT : la traduction
//    touche -> geste (les drapeaux `want*` de NkEditorShell, la table de
//    commandes pour Ctrl+D/G). C'est une ligne par geste, et elle se prouve au
//    releve par injection, pas ici. Ce qu'elle exerce, c'est TOUT le reste —
//    et c'est la ou vivent les defauts qui se voient (un enfant colle en
//    double, un groupe qui deplace ce qu'il groupe).
//
// ⚠️ ET ELLE MESURE L'ANNULATION AUTREMENT QUE --recette-annulation : celle-ci
//    prouve qu'un Annuler restaure ; celle-la prouve qu'il en faut UN SEUL —
//    la difference exacte entre « annulable » et « un geste = un pas ». Un
//    Couper qui coute deux Annuler passerait la premiere et echouerait ici.

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-selection : LE CONTRAT DE SELECTION (Lunacy) PROUVE PAR SES ETATS
// ═════════════════════════════════════════════════════════════════════════════
// Le VRAI mecanisme de `SelectionGeste.h`, celui que la toile ET la Hierarchie
// appellent. Sans fenetre ni GPU.
//
// ⚠️ LES PIEGES DE GROUPE SONT ECRITS D'ENTREE, pas apres coup -- c'est la
//    lecon des familles 42/43/44 : ce qu'on redecouvre au troisieme banc coute
//    plus cher que ce qu'on porte au premier. Ici, trois pieges connus :
//      1. la RACINE qui s'invite dans la selection (deja paye le 01/09) ;
//      2. les DEUX TABLES qui derivent (deja paye : Ctrl/Maj inverses) ;
//      3. un englobant calcule plusieurs fois (pas encore paye -- on l'evite).
//
// ⚠️ ET LE VOLET CONSERVATION : selectionner ne doit RIEN ecrire dans le
//    document. Un banc qui ne verifie que « la selection a change » laisserait
//    passer une selection qui salit le fichier -- et le controle 40h l'exige
//    depuis aout. On compare donc la SERIALISATION avant/apres, ancree.

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-snap : L'AIMANTATION (Lunacy) PROUVEE PAR SES NOMBRES
// ═════════════════════════════════════════════════════════════════════════════
//  --recette-points : LE MODE POINTS (§8bis restreint) ET SA TABLE DE DECISION
// ═════════════════════════════════════════════════════════════════════════════
// ⚠️ LE PIEGE DE GROUPE EST TENU D'ENTREE, ET IL EST NOMME : entrer en EDITION
//    DE TEXTE et entrer en MODE POINTS sont deux issues du MEME double-clic.
//    Leur table vit au meme endroit (`NkIssueDeDblClic`), et le premier cas de
//    cette recette la couvre EN ENTIER -- pas seulement la branche neuve.
//    Ecrire un banc qui ne teste que « le polygone entre en mode points »
//    aurait laisse la branche texte se faire manger a la premiere retouche.
//
// ⚠️ ET LE SECOND PIEGE DE GROUPE : les sommets DESSINES et les sommets
//    MANIPULES doivent venir de la meme fonction. Un cas compare donc les
//    sommets rendus par `NkSommetsDe` a ceux que le peintre utiliserait -- ils
//    sont le meme appel, et ce cas existe pour que ca le reste.
//
// ⚠️ VOLET CONSERVATION : entrer en mode points, regarder les sommets et en
//    SORTIR ne doit RIEN ecrire ; et un polygone materialise se relit a
//    l'identique.

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-transfo : LA ROTATION ET LES DEUX MIROIRS
// ═════════════════════════════════════════════════════════════════════════════
// Retour de Rodolf, 01/09 : « dans proprietes il n'y a pas miroir, rotation
// etc., ni autour de l'objet selectionne. »
//
// ⚠️ CETTE RECETTE EXISTE PARCE QUE LA ROTATION A UNE MOITIE QUI NE SE VOIT PAS.
//    Le champ et les poignees se jugent a l'oeil ; le PICKING, la PROPAGATION
//    aux enfants et l'ENGLOBANT ne se jugent qu'au nombre. Or c'est exactement
//    la moitie qui rend une rotation utilisable ou non : un angle qui s'affiche
//    et un clic qui tombe a cote donnent un objet qu'on ne peut plus attraper.
//
// ⚠️ ET L'ARBITRAGE DE Q42 DEMANDAIT LES CINQ POINTS OU RIEN (« un demi-champ de
//    rotation est pire que pas de rotation »). Les trois qui sont des calculs
//    sont ici ; les deux autres (rendu, poignees) sont dans la toile et se
//    voient. Ce que le peintre NE SAIT PAS faire est tenu par un cas, pas par
//    une note : `NkPeintureSaitTourner`.

// ═════════════════════════════════════════════════════════════════════════════
//  --recette-document : LE DOUBLE-CLIC MESURE SUR LE DOCUMENT REEL DE RODOLF
// ═════════════════════════════════════════════════════════════════════════════
// ⚠️ CETTE RECETTE EXISTE PARCE QUE DEUX FOIS CETTE SEMAINE UN BANC EST PASSE AU
//    VERT LA OU SA MAIN ECHOUAIT. La cause etait la meme les deux fois : le banc
//    fabriquait ses noeuds (un rect seul, pose a la racine) et sa main, elle,
//    double-cliquait dans un DOCUMENT -- avec des artboards, des groupes, et des
//    freres empiles. *Un banc qui emprunte une autre porte que le geste ne
//    prouve rien du geste.*
//
//    Ici, le document N'EST PAS FABRIQUE : il est LU sur le disque, par le vrai
//    `NkUIDocument::Load`, dispose par le vrai `NkComputeLayout`, pointe par le
//    vrai `NkPickTopLevel`. Et la recette IMPRIME LE CHEMIN QU'ELLE A LU : un
//    rapport qui ne dit pas sur quel fichier il porte n'est pas un rapport.
//
// ⚠️ ET ELLE NE « SAUTE » PAS SI LE FICHIER MANQUE. Un cas qui s'absente quand
//    sa donnee manque rend un vert qui ne veut rien dire -- exactement le defaut
//    du 18/08 (« la sonde a pu repasser verte sur un magenta plein ecran »).
//    Fichier introuvable = ECHEC, et le message nomme les chemins essayes.


// ═════════════════════════════════════════════════════════════════════════════
// Le VRAI `NkCalculerSnap`, celui que le glisser appelle. Sans fenetre ni GPU.
//
// ⚠️ C'EST POUR CETTE RECETTE QUE LE CALCUL A ETE SORTI DE `OnUI`. Ecrit dans
//    le panneau, il aurait vecu la ou aucun banc ne va — la faute que Q41 a
//    relevee sur le zoom et le deplacement (« restes trois etapes sans preuve,
//    et j'ai annonce qu'ils marchaient sans l'avoir vu »). Ce qui reste hors de
//    portee ici, et la recette le dit : le DESSIN du guide et la bascule de
//    l'aimant, qui se jugent a la capture et au releve `canvas.snap`.
// ⚠️ POSE EN OVERLAY, ET C'EST LE SEUL ENDROIT QUI CONVIENT : il est appele
//    APRES tous les panneaux, donc tous les rectangles de l'image sont deja
//    enregistres. Le poser dans un panneau publierait un registre a moitie
//    rempli — celui des panneaux dessines avant lui — et l'essai viserait une
//    cible qui existe une image sur deux.
// ⚠️ LE FICHIER CHANGE DE NOM EN MEME TEMPS QUE DE FORMAT, et ce n'est pas
//    de la coquetterie. L'ancien `nkuidesign_ui_rects.txt` portait
//    « identifiant = x y w h » ; celui-ci porte nature, niveau, etats, cle et
//    libelle. **Garder le nom aurait laisse un script lire un format qu'il ne
//    comprend plus, sans rien casser de visible** — la panne serait sortie
//    ailleurs, plus tard, comme toutes celles que ce depot a payees cher.
//    Mesure du 2026-08-29 avant de trancher : personne ne lit ce fichier
//    aujourd'hui (`grep` sur tout l'arbre -> le .gitignore, le carnet, et le
//    site d'ecriture ; aucun lecteur). Le renommage est donc gratuit — mais
//    il ne l'aurait pas ete, et il fallait le verifier avant, pas apres.
static const char *const kCheminReleveUI = "nkuidesign_releve_ui.txt";

/// `--dump-ui` a-t-il ete passe ? Lu a la creation de la coquille.
static bool gReleveDemande = false;

// ── LES LARGEURS DE DOCK DE LA MAQUETTE, POSEES UNE FOIS ────────────────────
// Banani ecran 1 : Hierarchie 220 px, Inspecteur 236 px. Le dock ne persiste
// pas ses ratios (stubs Save/LoadLayout) et ses defauts donnent des largeurs
// approchees des la premiere image — exactement ce que le remandat interdit.
// UNE fois, au premier passage ou les rects sont resolus ; l'utilisateur
// garde ensuite la main sur les splitters.
static void CalerLargeursDock(nkgui::NkGuiContext &ctx) {
	static bool fait = false;
	if (fait || gToileSeule)
		return;
	const nkgui::NkGuiId idHier = ctx.GetId("Hiérarchie");
	const nkgui::NkGuiId idInsp = ctx.GetId("Inspecteur");
	bool touche = false;
	for (nkentseu::usize ni = 0; ni < ctx.dockNodes.Size(); ++ni) {
		nkgui::NkGuiDockNode &nd = ctx.dockNodes[ni];
		if (nd.kind != 2)
			continue;
		for (int32 w = 0; w < nd.winCount; ++w) {
			const bool hier = (nd.windows[w] == idHier);
			const bool insp = (nd.windows[w] == idInsp);
			if (!hier && !insp)
				continue;
			const int32 pi = nd.parent;
			if (pi < 0)
				continue;
			nkgui::NkGuiDockNode &pa = ctx.dockNodes[(nkentseu::usize)pi];
			if (pa.kind != 1 || !pa.vertical || pa.rect.w <= 1.f)
				continue;
			const float32 vise = hier ? 220.f : 236.f;
			const bool premier = (pa.child0 == (int32)ni);
			pa.ratio = premier ? (vise / pa.rect.w) : (1.f - vise / pa.rect.w);
			touche = true;
		}
	}
	if (touche)
		fait = true;
}

static void FocusPanel(const char *titre); // defini plus bas (il tient gShell)

// ═══════════════════════════════════════════════════════════════════════════
//  RECETTE : L'IDENTITE D'UN PANNEAU -- renommer ne doit pas perdre la disposition
// ═══════════════════════════════════════════════════════════════════════════
//  Sans fenetre, sans GPU : elle n'exerce que l'enregistrement et la relecture de
//  la disposition. C'est le CRITERE qui decide de l'identifiant stable, et il doit
//  ROUGIR tant que la coquille adresse les panneaux par leur titre affiche.
//
//  Le geste mesure est celui de Rodolf : il renomme un panneau -- ou il le traduit,
//  ce qui revient au meme -- et retrouve sa disposition au lancement suivant.
struct PanneauSonde : public nkentseu::editorkit::NkEditorPanel {
		PanneauSonde(const char *ident, const char *titre)
			: nkentseu::editorkit::NkEditorPanel(ident, titre,
												 nkentseu::editorkit::NkEditorDockSide::NK_LEFT) {}
		void OnUI(nkentseu::editorkit::NkEditorFrameContext &) override {}
};


// ═══════════════════════════════════════════════════════════════════════════
//  LES HUIT RECETTES — sorties de ce fichier le 28/09/2026
// ═══════════════════════════════════════════════════════════════════════════
//  Rodolf : « tout fichier de plus de 1k ligne reste trop volumineux ». Elles
//  pesaient 6 497 lignes ici ; `main.cpp` passe de 10 765 a 4 268.
//
//  ⚠️ INCLUSES **ICI**, ET PAS EN TETE DE FICHIER. Une recette lit l'etat global
//     et les fonctions declarees AU-DESSUS d'elle. Les remonter parmi les
//     `#include` du debut les ferait referencer du code pas encore declare.
//     Cette position est exactement celle qu'occupait la derniere d'entre
//     elles : tout ce dont elles dependent est declare avant, et leur seul
//     appelant -- la boucle d'arguments -- vient apres.
//
//  📌 C'EST UN RANGEMENT, PAS UNE MODULARISATION. Rien n'a ete renomme, aucune
//     signature n'a bouge, le code est identique a l'octet pres -- c'est ce qui
//     permet de le verifier par la construction ET par le verdict des recettes
//     elles-memes. La vraie modularisation, `Panels.h` et ses 21 052 lignes,
//     est le livrable 6 du doc 5 : un chantier annonce, pas un deplacement.
#include "RecetteAnnulation.h"
#include "RecetteGestes.h"
#include "RecetteSelection.h"
#include "RecettePoints.h"
#include "RecetteTransfo.h"
#include "RecetteDocument.h"
#include "RecetteSnap.h"
#include "RecetteIdentite.h"

static void EcrireReleveUI(NkEditorFrameContext &ec, void *) {
	CalerLargeursDock(ec.Ui());
	// LE SELECTEUR DE COULEUR : ici et pas dans le panneau -- c'est le seul
	// endroit ou l'entree de la souris est REELLE (le shell la masque pendant
	// les panneaux des qu'un popup est survole).
	nkuidesign::NkDessinerPickerDemande(ec.Ui(), gDesign);
	// « EXPORTER... » (05/09) : le selecteur de fichier du kit en mode enregistrer,
	// meme endroit, meme raison (l'entree reelle) ; l'export se fait a la confirmation.
	// ④ LE DIALOGUE D'EXPORT d'abord (il ouvre le selecteur a la confirmation), puis le
	//    selecteur lui-meme : deux etapes, un seul chemin.
	nkuidesign::NkDessinerDialogueExport(ec.Ui(), gDesign);
	nkuidesign::NkDessinerPickerExport(ec.Ui(), gDesign);
	// LE MENU DES ROLES (ecrans 5-6-7) : dessine en OVERLAY, par-dessus les
	// panneaux ; choisir ECRIT la cle `role` du noeud (le geste
	// « promouvoir » du §4.3). Le code 0x01 = retirer le role.
	if (gDesign.menuRole.ouvert && gDesign.doc.IsValidIndex(gDesign.selected)) {
		nkuidesign::NkUINode &n = gDesign.doc.nodes[(nkentseu::uint32)gDesign.selected];
		const char *choisi =
			nkuidesign::menurole::Dessiner(ec.Ui(), gDesign.menuRole, n.role.Data());
		if (choisi) {
			n.role = (choisi[0] == '\x01') ? NkString() : NkString(choisi);
			gDesign.doc.MarkHumanEdit(gDesign.selected);
			gDesign.status =
				n.role.Empty()
					? NkString("Rôle retiré.")
					: NkString("Rôle posé — l'arbre, la toile et Behavior le montrent.");
		}
	} else if (gDesign.menuRole.ouvert)
		gDesign.menuRole.ouvert = false; // plus de selection : le menu se ferme
	// (R19) LES DEUX MENUS DU CLIC DROIT DE LA TOILE, ICI ET PLUS DANS LE PANNEAU.
	// Mesure du 14/09 (sonde des portes) : dessines dans `PreviewPanel::OnUI`, ils
	// reservaient la saisie, le shell masquait alors l'entree de TOUS les panneaux -- eux
	// compris. Ni Echap ni clic ne leur parvenait : le menu restait ouvert et le corps
	// masque POUR TOUJOURS (le symptome du gel), pendant que la barre de titre vivait.
	// Le menu contextuel du SHELL, meme widget, dessine apres la restauration, etait vert :
	// c'est la PHASE qui decide, pas le widget. Meme raison que la ligne 7126 ci-dessus.
	if (gPanneauToile && !nkuidesign::NkMenusToileDansLePanneau())
		gPanneauToile->DessinerMenusToile(ec.Ui());
	// LE MENU DES FORMATS (catalogue Formats.h, 31/08) — meme couche, meme
	// patron que le menu des roles ; le choix passe par AppliquerFormat
	// (cible + redimension + constats au rapport), annulable en un pas.
	if (gDesign.menuFormat.ouvert && gDesign.doc.IsValidIndex(gDesign.menuFormat.page)) {
		const nkuidesign::NkUINode &pf =
			gDesign.doc.nodes[(nkentseu::uint32)gDesign.menuFormat.page];
		const nkuidesign::menuformat::Choix ch =
			nkuidesign::menuformat::Dessiner(ec.Ui(), gDesign.menuFormat, pf.target.Data());
		if (ch.fait)
			gDesign.AppliquerFormat(gDesign.menuFormat.page, ch.nom, ch.w, ch.h, ch.note);
	} else if (gDesign.menuFormat.ouvert)
		gDesign.menuFormat.ouvert = false; // la page a disparu : le menu se ferme
	// LE RAPPORT DE TRANSPOSITION (ecran 27) : modal honnete — les cibles
	// REELLES du document, 0 constat tant que la transposition n'existe pas.
	if (gDesign.rapportTransposition) {
		auto &ctx = ec.Ui();
		auto &dl = ctx.dlOverlay;
		auto &F = nkuidesign::costume::Fontes();
		ctx.appModal = true;
		const nkgui::NkRect m = {((float32)ctx.viewW - 440.f) * 0.5f,
								 ((float32)ctx.viewH - 240.f) * 0.5f, 440.f, 240.f};
		dl.AddRectFilled({0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH},
						 {0, 0, 0, 89});
		dl.AddRectFilled({m.x - 1.f, m.y + 4.f, m.w + 2.f, m.h + 6.f}, {0, 0, 0, 80},
						 12.f);
		dl.AddRectFilled(m, ctx.theme.panel, 8.f);
		dl.AddRect(m, ctx.theme.border, 1.f, 8.f);
		nkuidesign::costume::TexteGras(dl, F.px13, m.x + 18.f, m.y + 16.f,
									   "Rapport de transposition", ctx.theme.text, 0.4f);
		// les cibles REELLES : les cadres a cle `cible` du document
		float32 cy = m.y + 48.f;
		int32 nCibles = 0;
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)gDesign.doc.nodes.Size(); ++i) {
			const auto &nd = gDesign.doc.nodes[i];
			if (nd.target.Empty())
				continue;
			char b[128];
			snprintf(b, sizeof(b), "%s — %s", nd.label.Data(), nd.target.Data());
			const float32 wb = nkuidesign::costume::Largeur(F.px11, b) + 20.f;
			dl.AddRectFilled({m.x + 18.f, cy, wb, 24.f}, ctx.theme.button, 12.f);
			nkuidesign::costume::Texte(dl, F.px11, m.x + 28.f,
									   nkuidesign::costume::CentrerY(F.px11, cy, 24.f), b,
									   ctx.theme.text);
			cy += 30.f;
			++nCibles;
		}
		if (nCibles == 0) {
			nkuidesign::costume::Texte(dl, F.px11, m.x + 18.f, cy,
									   "(aucun cadre a cible dans ce document)",
									   ctx.theme.textMuted);
			cy += 24.f;
		}
		// LES ELEMENTS HORS PAGE (6e retour, volet B) : poses a la racine de la
		// toile — PERMIS, mais la transposition ne les couvre pas. Le rapport
		// le DIT au lieu de les ignorer en silence.
		{
			nkentseu::int32 horsPage = 0;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)gDesign.doc.nodes.Size(); ++i) {
				const auto &nd = gDesign.doc.nodes[i];
				if (nd.parent >= 0 && gDesign.doc.IsValidIndex(nd.parent)
					&& gDesign.doc.nodes[(nkentseu::uint32)nd.parent].parent < 0
					&& !nkuidesign::NkComponentDecl::StrEq(nd.shape.Data(), "frame"))
					++horsPage;
			}
			if (horsPage > 0) {
				char hb[96];
				snprintf(hb, sizeof(hb),
						 "%d élément(s) hors page — non couverts par la transposition.",
						 horsPage);
				nkuidesign::costume::Texte(dl, F.px10, m.x + 18.f, cy, hb, ctx.theme.textMuted);
				cy += 20.f;
			}
		}
		// LES TEXTES SANS TRADUCTION dans la langue active (multilingue 01/09) :
		// le repli est visible sur la toile (attenue), et il se COMPTE ici.
		if (!gDesign.langueActive.Empty()) {
			nkentseu::int32 manquants = 0;
			for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)gDesign.doc.nodes.Size(); ++i) {
				const auto &nd = gDesign.doc.nodes[i];
				if (nd.text.Empty())
					continue;
				bool traduit = true;
				(void)nd.TexteEn(gDesign.langueActive.Data(), &traduit);
				if (!traduit)
					++manquants;
			}
			char lb[96];
			snprintf(lb, sizeof(lb), "%d texte(s) sans traduction en « %s ».", manquants,
					 gDesign.langueActive.Data());
			nkuidesign::costume::Texte(dl, F.px10, m.x + 18.f, cy, lb,
									   manquants > 0 ? ctx.theme.text : ctx.theme.textMuted);
			cy += 20.f;
		}
		// LES CONSTATS RÉELS de la dernière transposition (« Générer la
		// version mobile ») : le rapport a cessé d'être vide le jour où la
		// tranche 1 a existé — il liste ce que la re-disposition n'a pas su
		// absorber. Aucune transposition lancée = il le dit.
		if (gDesign.constatsTransposition.Empty()) {
			nkuidesign::costume::Texte(dl, F.px11, m.x + 18.f, m.y + m.h - 74.f,
									   "0 constat — aucune transposition lancée dans cette "
									   "session.",
									   ctx.theme.textMuted);
			nkuidesign::costume::Texte(dl, F.px10, m.x + 18.f, m.y + m.h - 56.f,
									   "Sélectionnez une page, barre d'appareil → « Générer "
									   "la version mobile ».",
									   ctx.theme.textMuted);
		} else {
			char t[96];
			snprintf(t, sizeof(t), "%d constat(s) de la dernière transposition :",
					 (int32)gDesign.constatsTransposition.Size());
			nkuidesign::costume::TexteGras(dl, F.px11, m.x + 18.f, m.y + m.h - 92.f, t,
										   ctx.theme.text, 0.3f);
			const int32 nAff =
				(int32)gDesign.constatsTransposition.Size() < 3
					? (int32)gDesign.constatsTransposition.Size()
					: 3;
			for (int32 ci = 0; ci < nAff; ++ci)
				nkuidesign::costume::Texte(dl, F.px10, m.x + 18.f,
										   m.y + m.h - 74.f + (float32)ci * 16.f,
										   gDesign.constatsTransposition[(uint32)ci].Data(),
										   ctx.theme.textMuted);
			if ((int32)gDesign.constatsTransposition.Size() > nAff) {
				snprintf(t, sizeof(t), "… et %d autre(s).",
						 (int32)gDesign.constatsTransposition.Size() - nAff);
				nkuidesign::costume::Texte(dl, F.px10, m.x + 18.f,
										   m.y + m.h - 74.f + (float32)nAff * 16.f, t,
										   ctx.theme.textMuted);
			}
		}
		const float32 wf = nkuidesign::costume::Largeur(F.px11, "Fermer") + 24.f;
		const nkgui::NkRect rf = {m.x + m.w - wf - 16.f, m.y + m.h - 34.f, wf, 24.f};
		dl.AddRectFilled(rf, ctx.theme.accent, 4.f);
		nkuidesign::costume::TexteGras(dl, F.px11, rf.x + 12.f,
									   nkuidesign::costume::CentrerY(F.px11, rf.y, 24.f),
									   "Fermer", ctx.theme.onAccent, 0.3f);
		if ((ctx.input.mouseClicked[0] && nkgui::NkGuiRectContains(rf, ctx.input.mousePos))
			|| ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
			gDesign.rapportTransposition = false;
	}
	// Le déclencheur de simulation (en-tête de l'Inspecteur) demande le
	// panneau Simulation — seul ce rappel tient la coquille (FocusPanel,
	// qui ouvre + ancre + met devant : la leçon du 28/08).
	if (gDesign.ouvrirSimulation) {
		gDesign.ouvrirSimulation = false;
		FocusPanel("Simulation");
	}
	// ── GARDE DE DÉCOUPE (classe du 31/08 : « tu ne définis pas bien le
	//    clipping ») ────────────────────────────────────────────────────────
	// À cet endroit — après tous les panneaux, avant EndFrame — chaque pile de
	// découpe doit être revenue à ZÉRO : un panneau qui ouvre un PushClip sans
	// le refermer laisse SON rectangle en vigueur pour tout ce qui se dessine
	// après lui, et la panne sort ailleurs (un panneau voisin amputé, un fond
	// qui « manque »). La mesure est publiée (`garde.decoupe`) pour que
	// `--dump-ui` la montre, et un déséquilibre se JOURNALISE avec le compte —
	// jamais réparé en silence (Reset() remet à zéro à l'image suivante, c'est
	// précisément ce qui rendait la classe invisible).
	{
		auto &ui = ec.Ui();
		nkentseu::int32 fuites = ui.dl.clipDepth + ui.dlOverlay.clipDepth;
		for (nkentseu::int32 wi = 0; wi < ui.winCount; ++wi)
			fuites += ui.winDL[wi].clipDepth;
		nkgui::NkGuiNoterMesure(ui, "garde.decoupe", (float32)fuites, 0.f, 0.f, 0.f);
		static bool dejaDit = false;
		if (fuites != 0 && !dejaDit) {
			dejaDit = true; // une fois par session : un log par image serait du bruit
			logger.Warn("[NKUIDesign] GARDE DE DECOUPE : {0} PushClip sans PopClip a la fin de "
						"l'image — un panneau ne referme pas sa decoupe.",
						fuites);
		}
	}
	nkgui::NkGuiIntrospectEcrire(ec.Ui(), kCheminReleveUI);
}

// ── AMENER UN PANNEAU AU PREMIER PLAN, AU CLAVIER ───────────────────────────
// ⚠️ CE N'EST PAS UN ACCESSOIRE D'ESSAI, MAIS IL EN DEBLOQUE UN. Les onglets de
//    panneaux sont dessines par la COQUILLE : leurs rectangles ne passent pas par
//    mon registre, donc un essai a la souris ne peut pas atteindre un panneau
//    cache derriere un autre — et « le panneau n'etait pas dessine » ressemblerait
//    a « le bouton ne marche pas ».
//    Un raccourci clavier n'a, lui, aucune coordonnee : il est **insensible a la
//    mise en page** par construction. C'est la meme sortie que pour le clic —
//    brancher l'essai sur une source qui ne varie pas avec ce qu'on mesure.
//    Et c'est utile a l'utilisateur, pas seulement a l'essai.
// ⚠️ J'AVAIS REECRIT CE QUI EXISTAIT. Ma premiere version appelait
//    `nkgui::DockFocusWindow` — qui ne fait qu'une partie du travail : elle
//    donne le focus a une fenetre DEJA ancree et ouverte. La coquille expose
//    **`NkEditorShell::FocusPanel`** (public, `NkEditorShell.h:113`), qui
//    OUVRE le panneau s'il etait ferme, l'ANCRE a son cote par defaut s'il ne
//    l'etait pas, puis le met devant. C'est exactement le geste voulu, et il
//    etait deja ecrit.
//    La regle « chercher l'existant avant d'ecrire » m'a coute une heure ici :
//    j'ai diagnostique un raccourci qui ne partait pas, alors que ma fonction
//    n'aurait de toute facon pas ouvert un panneau ferme.
/// L'indice de la pastille « Chat IA » sur le rail droit, RESOLU PAR LE NOM au
/// moment ou le rail est pose -- voir `SetRail` plus bas.
///
/// ⚠️ PAR LE NOM, PAS PAR L'INDICE ECRIT EN DUR. `kRailDroite[1]` est vrai
///    aujourd'hui et faux le jour ou quelqu'un insere une pastille avant : le
///    tiroir s'ouvrirait sur la Bibliotheque sans que rien ne le dise. Ce depot
///    a deja paye un indice qui se decale (`--demo=2`).
/// ⚠️ -1 tant qu'il n'est pas resolu, et on REFUSE d'ouvrir : un tiroir ouvert
///    au hasard serait pire qu'un refus, l'utilisateur croirait avoir vu le chat.
static nkentseu::int32 gTiroirIA = -1;
/// `--tiroir=ia` : ouvrir par la MEME porte que le menu, pour l'eprouver.
static bool gTiroirParLeNom = false;

static void OuvrirTiroirIA() {
	if (!gShell) {
		logger.Warn("[NKUIDesign] tiroir IA demande sans coquille");
		return;
	}
	if (gTiroirIA < 0) {
		logger.Warn("[NKUIDesign] aucune pastille « Chat IA » sur le rail droit : "
					"rien n'est ouvert plutot qu'un tiroir au hasard");
		return;
	}
	gShell->OuvrirTiroir(nkentseu::editorkit::NkEditorDockSide::NK_RIGHT, gTiroirIA);
	logger.Info("[NKUIDesign] tiroir IA ouvert (pastille {0} du rail droit)", gTiroirIA);
}

static void FocusPanel(const char *titre) {
	if (!gShell) {
		logger.Warn("[NKUIDesign] vue '{0}' demandée sans coquille", titre);
		return;
	}
	// ⚠️ ON JOURNALISE LE RESULTAT, PAS L'APPEL. « la commande est partie » et
	//    « le panneau est passe devant » sont deux faits differents, et c'est
	//    exactement la confusion qui m'a fait cliquer a travers un panneau cache.
	const bool ok = gShell->FocusPanel(titre);
	logger.Info("[NKUIDesign] vue '{0}' : FocusPanel -> {1}", titre, ok ? "vrai" : "FAUX");
}
static void CmdVueHierarchie(void *) {
	FocusPanel("Hiérarchie");
}
static void CmdVueInspecteur(void *) {
	FocusPanel("Inspecteur");
}
#if NKUIDESIGN_ANCIENS_PANNEAUX
static void CmdVuePalette(void *) {
	FocusPanel("Palette");
}
static void CmdVueComposition(void *) {
	FocusPanel("Composition");
}
static void CmdVueProprietes(void *) {
	FocusPanel("Propriétés");
}
static void CmdVuePreferences(void *) {
	FocusPanel("Préférences");
}
#endif

static void CmdQuit(void *user) {
	if (user)
		static_cast<NkEditorShell *>(user)->RequestClose();
}

// =============================================================================
//  LA TABLE D'ACTIONS DU DOCUMENT `.nkgui` — UNE SEULE, POUR DEUX USAGES
// =============================================================================
//  L'identifiant d'un `MenuItem` de `Resources/Interface/NKUIDesign/menu_design.nkgui`
//  EST le nom ci-dessous. C'est la convention de cet hote, pas une extension du
//  format : la grammaire n'a pas de `on Pressed -> ...`, et `NkBandeDocument`
//  derive l'appui puis cherche le nom ici.
//
//  ⚠️ ELLE EST A PORTEE FICHIER POUR QUE LA SONDE JUGE CE QUE L'APPLICATION
//     BRANCHE. Declaree dans le bloc de cablage, elle aurait oblige la sonde a en
//     tenir une COPIE — et le jour ou l'une gagne une entree, la sonde declare
//     « tout est servi » sur un menu qui a un trou, ou l'inverse. *Deux compteurs
//     sans code commun ne peuvent pas se contredire, donc ne peuvent rien
//     prouver.*
//
//  ⚠️ AUCUNE ENVELOPPE AUTOUR DES `Cmd*`. `NkEditorCommandFn` et
//     `nkgui::NkActionFn` sont la meme signature (`void (*)(void *)`) : pointer
//     droit dessus fait un nom de moins a tenir d'accord. Et ce sont les MEMES
//     fonctions que `RegisterCommand` sert — un menu du document et la palette de
//     commandes ne peuvent donc pas divergerr sur ce que fait une action.
static const nkgui::NkActionNommee gActionsDocument[] = {
	{"design.enregistrer", &CmdSave, nullptr},
	{"design.recharger", &CmdLoad, nullptr},
	{"design.nouveau", &CmdNew, nullptr},
	{"design.annuler", &CmdUndo, nullptr},
	{"design.retablir", &CmdRedo, nullptr},
	{"design.grouper", &CmdGrouper, nullptr},
	{"design.degrouper", &CmdDegrouper, nullptr},
	{"design.dupliquer", &CmdDupliquer, nullptr},
	{"design.exporter", &CmdExporter, nullptr},
	{"design.vue.hierarchie", &CmdVueHierarchie, nullptr},
	{"design.vue.inspecteur", &CmdVueInspecteur, nullptr},
	// 🔴 QUATRE NOMS N'Y SONT PAS, ET LA CONSTRUCTION ME L'A APPRIS.
	//    `CmdVuePalette`, `CmdVueComposition`, `CmdVueProprietes` et
	//    `CmdVuePreferences` vivent sous `#if NKUIDESIGN_ANCIENS_PANNEAUX`, qui est
	//    ETEINT : elles n'existent pas dans cette construction. Ma premiere version
	//    les listait ici, et le compilateur a refuse — c'est le seul negatif qui
	//    n'a rien coute.
	//
	//    Leurs entrees de menu restent dans le document, GRISEES avec leur raison :
	//    « une entree qui s'affiche et ne fait rien est un mensonge ; une entree
	//    grisee est une promesse datee ». La sonde les compte a part (`grisees`) et
	//    ne les reproche pas — une entree sans action n'a pas besoin d'action.
};
static const nkentseu::uint32 gNbActionsDocument =
	(nkentseu::uint32)(sizeof(gActionsDocument) / sizeof(gActionsDocument[0]));

// =============================================================================
//  L EN-TETE A DEUX BANDES -- document 3 §4/§5, planche 091913
// =============================================================================
//
//  Bande 1 (28 px) : huit menus colles au logo, le nom du design au centre de
//  la FENETRE ENTIERE, les boutons de fenetre a droite.
//  Bande 2 (28 px) : les onglets de projets.
//  Bloc logo : 56 x 56, carre, a cheval sur les deux bandes ; les bandes
//  commencent a x = 56.
//
//  ⚠️ OU ATTERRIT LE CHOIX DU BACKEND GRAPHIQUE. Il vivait dans le panneau de
//     droite, qui vient d etre debranche. La regle du depot est que TOUTE
//     application doit laisser choisir son backend DEPUIS L INTERFACE, la
//     configuration n etant que le defaut lu au lancement. Il est donc pose ICI,
//     dans `Fichier > Backend graphique >`, et il y a ete pose **avant** le
//     debranchement -- une capacite ne se retire pas avant que son remplacant
//     existe.
// ── INJECTION DE CLICS (mise en scene : --clic=x:y:frame, jusqu'a 4) ─────────
// Le meme principe que le harnais releve-menus : un clic SYNTHETIQUE pose dans
// l'input du contexte, jamais la souris reelle (regle du creneau). Sert a
// ouvrir un menu pour une capture (ecran 26).
static struct {
	float32 x = 0.f, y = 0.f;
	int32 frame = -1;
	bool dbl = false;	///< --clic=x:y:frame:d — injecte AUSSI un double-clic
	bool droit = false; ///< --clic=x:y:frame:r — clic DROIT (menu contextuel)
	/// --clic=x:y:frame:o — LE DOUBLE-CLIC TEL QUE L'OS L'ENVOIE : le drapeau de
	/// double-clic SEUL, SANS appui.
	/// ⚠️ CET INSTRUMENT EXISTE PARCE QUE LE BANC PRENAIT UNE AUTRE PORTE QUE LA
	///    SOURIS DE RODOLF, et c'est ce qui a cache le defaut du 01/09. Avec
	///    `CS_DBLCLKS`, Windows REMPLACE le second WM_LBUTTONDOWN par
	///    WM_LBUTTONDBLCLK : sur le second clic, `mouseDown` reste FAUX. Le `:d`
	///    ci-dessus, lui, posait l'appui ET le double-clic — donc il prouvait un
	///    chemin que le geste reel n'empruntait jamais. `:o` reproduit le vrai.
	bool dblOS = false;
	/// --clic=x:y:frame:c (CTRL) ou :s (MAJ). ⚠️ SANS EUX, LE CONTRAT DE
	/// SELECTION DE LUNACY N'EST PAS MESURABLE : ses trois clics ne different
	/// QUE par le modificateur (nu = groupe de 1er niveau, Ctrl = profond, Maj
	/// = multi). Un injecteur qui ne sait poser qu'un clic nu ne peut prouver
	/// qu'un tiers du contrat -- et c'est le tiers qui marchait deja.
	bool ctrl = false;
	bool maj = false;
} gClics[10]; // 10 depuis le 01/09 : une scene de synthese demande plus de
			  // quatre gestes (replier des sections, ajouter, multi-selectionner).
// ── FRAPPE ET TOUCHES INJECTEES (mise en scene, 01/09) ──────────────────────
// Le meme principe que gClics : on ecrit dans ctx.input, jamais le clavier
// reel. Necessaire pour PROUVER les saisies en place (renommage d'arbre,
// edition de texte) au releve — un clic sait ouvrir la saisie, seule la
// frappe sait la remplir. --frappe=texte:frame (ASCII, ':' interdit dans le
// texte) ; --touche=entree|echap|retour:frame.
static struct {
	int32 frame = -1;
	char texte[64] = {};
} gFrappes[2];
static struct {
	int32 frame = -1;
	nkgui::NkGuiKey touche = nkgui::NkGuiKey::Enter;
} gTouches[4];
// ── GLISSER INJECTE (mesure, 01/09) : --glisser=x1:y1:x2:y2:frame[:duree] ────
// Un VRAI drag : presse a (x1,y1), la souris interpole vers (x2,y2) sur
// `duree` trames (12 par defaut — un drag d'une trame raterait les seuils
// anti-tremblement), relache a l'arrivee. Necessaire pour PROUVER la poignee
// de sections, le pouce d'ascenseur et le deplacement d'une page par son
// etiquette — un clic ne tient pas la souris. Meme regle que gClics : on
// ecrit dans ctx.input, JAMAIS la souris reelle.
static struct {
	float32 x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
	int32 frame = -1, duree = 12;
	// ⚠️ NE PAS RELACHER (suffixe `:t`) -- L'INSTRUMENT QUI MANQUAIT POUR
	//    MESURER UN ETAT *PENDANT* UN GESTE. Le releve s'ecrit a chaque image,
	//    mais certains etats ne vivent QUE pendant le geste et meurent au
	//    relacher : les guides d'aimantation en sont. Mesure du 01/09 : un
	//    glisser qui amenait une page pile sur le bord de sa voisine laissait
	//    `canvas.snap` a « aucun guide » -- non parce que l'aimant avait rate,
	//    mais parce qu'il avait FINI. Sans ce drapeau, la seule facon de voir
	//    le guide au releve aurait ete de le faire SURVIVRE a son geste,
	//    c'est-a-dire de casser le comportement pour pouvoir le mesurer.
	bool tenir = false;
} gGlissers[2];
// ── MOLETTE INJECTEE (mesure, 01/09) : --molette=x:y:delta:frame ─────────────
// Le defilement a la molette, pose a une position donnee (le survol decide
// qui defile). delta > 0 = vers le haut, comme l'OS.
static struct {
	float32 x = 0.f, y = 0.f, delta = 0.f;
	int32 frame = -1;
} gMolettes[2];
/// ⚠️ ELLE LIT LES TROIS TABLEAUX, ET LES TROIS BORNES SE LISENT DU TABLEAU.
///    Une borne ecrite en chiffre ne suit pas ce qu'elle borne -- ce fichier
///    l'a deja paye deux fois (le remplisseur de `gClics` reste a 4 quand le
///    tableau est passe a 10, et la borne de `gTouches` dans l'autre sens).
static bool AucuneEntreeProgrammee() {
	for (int32 i = 0; i < (int32)(sizeof(gClics) / sizeof(gClics[0])); ++i)
		if (gClics[i].frame >= 0)
			return false;
	for (int32 i = 0; i < (int32)(sizeof(gGlissers) / sizeof(gGlissers[0])); ++i)
		if (gGlissers[i].frame >= 0)
			return false;
	for (int32 i = 0; i < (int32)(sizeof(gMolettes) / sizeof(gMolettes[0])); ++i)
		if (gMolettes[i].frame >= 0)
			return false;
	return true;
}

static void InjecterClics(nkgui::NkGuiContext &ctx) {
	static int32 compteur = 0;
	++compteur;
	// ⚠️ REMIS A FAUX A CHAQUE IMAGE : une position de sonde qui SURVIVRAIT a son
	//    clic figerait la souris la pour toujours, et la capture montrerait un
	//    survol que personne n'a demande.
	gSourisSondePosee = false;
	for (int32 i = 0; i < (int32)(sizeof(gClics) / sizeof(gClics[0])); ++i) {
		if (gClics[i].frame < 0)
			continue;
		// La souris TIENT la position a partir du clic (le harnais releve-menus
		// pilote pareil : plusieurs trames, pas une) — le popup survit au survol.
		// le survol se resout sur hotIdPrev (la trame d'AVANT) : la position
		// se tient CINQ trames avant le clic, sinon le clic vise un survol
		// pas encore etabli et manque.
		if (compteur >= gClics[i].frame - 5) {
			ctx.input.mousePos = {gClics[i].x, gClics[i].y};
			gSourisSondePosee = true;
			gSourisSondeX = gClics[i].x;
			gSourisSondeY = gClics[i].y;
		}
		if (compteur == gClics[i].frame) {
			const int32 b = gClics[i].droit ? 1 : 0; // :r = clic DROIT
			// Les modificateurs se posent AVEC le clic et se retirent avec lui :
			// laisses colles, ils changeraient le sens de tous les clics suivants.
			if (gClics[i].ctrl)
				ctx.input.ctrlDown = true;
			if (gClics[i].maj)
				ctx.input.shiftDown = true;
			// ⚠️ `:o` NE POSE PAS D'APPUI, et c'est tout son objet : sur le
			//    second clic d'un vrai double-clic Windows, `mouseDown` reste
			//    faux (CS_DBLCLKS remplace WM_LBUTTONDOWN par WM_LBUTTONDBLCLK).
			if (!gClics[i].dblOS) {
				ctx.input.mouseDown[b] = true;
				ctx.input.mouseClicked[b] = true;
			}
			// le DOUBLE-CLIC s'injecte tel quel (la detection temporelle de la
			// fenetre ne verra jamais deux vrais clics) — c'est le levier de
			// preuve du FORAGE sous curseur.
			if (gClics[i].dbl || gClics[i].dblOS)
				ctx.input.mouseDoubleClicked[0] = true;
		} else if (compteur == gClics[i].frame + 1) {
			// ⚠️ EFFACER le clic : si ce rappel tourne deux fois par trame, un
			//    clic qui persiste au second passage REFERME le menu qu'il vient
			//    d'ouvrir (double bascule) — mesure au releve : « survole,replie ».
			const int32 b = gClics[i].droit ? 1 : 0;
			ctx.input.mouseClicked[b] = false;
			ctx.input.mouseDown[b] = false;
			ctx.input.mouseReleased[b] = true;
			ctx.input.mouseDoubleClicked[0] = false;
			if (gClics[i].ctrl)
				ctx.input.ctrlDown = false;
			if (gClics[i].maj)
				ctx.input.shiftDown = false;
		}
	}
	// ── LA FRAPPE (--frappe=) : les codepoints poses UNE trame ──────────────
	for (int32 i = 0; i < 2; ++i) {
		if (gFrappes[i].frame < 0)
			continue;
		if (compteur == gFrappes[i].frame) {
			for (const char *q = gFrappes[i].texte; *q; ++q)
				ctx.input.PushChar((nkentseu::uint32)(unsigned char)*q);
		} else if (compteur == gFrappes[i].frame + 1)
			ctx.input.charCount = 0;
	}
	// ── LES TOUCHES (--touche=) : keyInit une trame (le one-shot que
	//    KeyPressed lit), efface a la suivante ────────────────────────────────
	for (int32 i = 0; i < 4; ++i) { // gTouches[4] -- la borne suit LE TABLEAU
		if (gTouches[i].frame < 0)
			continue;
		if (compteur == gTouches[i].frame)
			ctx.input.keyInit[(int32)gTouches[i].touche] = true;
		else if (compteur == gTouches[i].frame + 1)
			ctx.input.keyInit[(int32)gTouches[i].touche] = false;
	}
	// ── LE GLISSER (--glisser=) : presse, interpole, relache ────────────────
	// 🔴 (R20) LE GLISSER NE POSAIT PAS LA SOURIS DE SONDE. `CaptureTick` repousse la souris
	//    hors ecran a son second passage de l'image, SAUF si `gSourisSondePosee` : c'est le
	//    correctif du 14/09 pour `--clic`, et `--glisser` n'en profitait pas. Sous `--capture`,
	//    le geste etait donc teleporte hors ecran a chaque image -- la famille « un negatif
	//    incapable de refuter », une troisieme fois. On pose la position a la fin de la boucle,
	//    tant que le geste est vif.
	struct PoseGlisser {
			~PoseGlisser() {
				if (vif) {
					gSourisSondePosee = true;
					gSourisSondeX = x;
					gSourisSondeY = y;
				}
			}
			bool vif = false;
			float32 x = 0.f, y = 0.f;
	} poseGlisser;
	for (int32 i = 0; i < 2; ++i) {
		if (gGlissers[i].frame < 0)
			continue;
		const int32 f0 = gGlissers[i].frame;
		const int32 fn = f0 + (gGlissers[i].duree > 0 ? gGlissers[i].duree : 12);
		if (compteur >= f0 - 5 && (compteur <= fn + 1 || gGlissers[i].tenir))
			poseGlisser.vif = true;
		if (compteur >= f0 - 5 && compteur < f0)
			ctx.input.mousePos = {gGlissers[i].x1, gGlissers[i].y1}; // survol etabli
		else if (compteur == f0) {
			ctx.input.mousePos = {gGlissers[i].x1, gGlissers[i].y1};
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = true;
		} else if (compteur > f0 && compteur <= fn) {
			const float32 t = (float32)(compteur - f0) / (float32)(fn - f0);
			ctx.input.mousePos = {gGlissers[i].x1 + (gGlissers[i].x2 - gGlissers[i].x1) * t,
								  gGlissers[i].y1 + (gGlissers[i].y2 - gGlissers[i].y1) * t};
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = false;
		} else if (compteur > fn && gGlissers[i].tenir) {
			// `:t` : on TIENT -- la souris reste a l'arrivee, bouton enfonce. Le
			// geste ne se termine jamais, donc son etat vif reste lisible au
			// releve aussi longtemps que l'application tourne.
			ctx.input.mousePos = {gGlissers[i].x2, gGlissers[i].y2};
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = false;
		} else if (compteur == fn + 1) {
			ctx.input.mousePos = {gGlissers[i].x2, gGlissers[i].y2};
			ctx.input.mouseDown[0] = false;
			ctx.input.mouseReleased[0] = true;
		}
	}
	poseGlisser.x = ctx.input.mousePos.x; // (R20) lue par le destructeur, apres la boucle
	poseGlisser.y = ctx.input.mousePos.y;
	// ── LA MOLETTE (--molette=) : un cran a la position donnee ──────────────
	for (int32 i = 0; i < 2; ++i) {
		if (gMolettes[i].frame < 0)
			continue;
		if (compteur >= gMolettes[i].frame - 5 && compteur <= gMolettes[i].frame)
			ctx.input.mousePos = {gMolettes[i].x, gMolettes[i].y};
		if (compteur == gMolettes[i].frame)
			ctx.input.wheel += gMolettes[i].delta;
	}
}

// ═════════════════════════════════════════════════════════════════════════════
//  (R16) LA SONDE DES PORTES DU CORPS -- --sonde-portes=<source>[,<source>...]
// ═════════════════════════════════════════════════════════════════════════════
//  Rodolf voit un corps qui ne recoit plus l'entree pendant que la barre de titre
//  la recoit. La coquille masque le corps quand l'une des portes P A O C R est
//  fermee (NkEditorShell::PortesDuCorps). Cette sonde OUVRE puis FERME chaque
//  source par l'API ou par le geste interne de la source (Echap pose dans NOTRE
//  contexte), puis verifie que toutes les portes sont rouvertes.
//  🔴 AUCUNE ENTREE SUR LA MACHINE : ni souris ni clavier de l'OS. La position est
//     forcee dans notre contexte a CHAQUE passage (hors ecran, ou au centre du popup
//     ouvert), et les boutons physiques sont neutralises : la fenetre SONDE ne
//     recoit rien de la main de l'utilisateur.
//  ⚠️ LA SOURIS RESTE AU CENTRE DU POPUP APRES SA FERMETURE, et c'est expres : un
//     rectangle de popup reste pose (popupDepth non rendu) ne se voit QUE si la
//     souris est dedans. Hors ecran, la porte O serait muette par construction.
//  ⚠️ `mAppMenuFn` EST APPELE DEUX FOIS PAR IMAGE (lecon du 14/09) : les gestes ne
//     partent qu'au passage ou `gImagesReelles` a change ; la position, elle, est
//     forcee aux deux.
static char gSondePortes[256] = {};
/// (R20) --sauver-document=<image>:<chemin> : a l'image donnee, ECRIRE le document, le RELIRE
/// dans un document neuf, et comparer les composants poses dans les deux.
static nkentseu::int32 gSauverImage = -1;
/// (identite) --sauver-disposition=<image>:<chemin> : ecrire la DISPOSITION REELLE de
/// l'application a cette image, puis fermer. Sert a verifier, sur les quinze vrais
/// panneaux, que le fichier porte des IDENTIFIANTS et non des libelles affiches.
static nkentseu::int32 gDispoImage = -1;
static char gDispoChemin[512] = {};
static char gSauverChemin[512] = {};
static void SauverEtRelire() {
	using namespace nkuidesign;
	NkString texte;
	gDesign.doc.Save(texte);
	const bool ecrit = nkentseu::NkFile::WriteAllText(gSauverChemin, texte.Data());
	NkUIDocument relu;
	const bool lu = ecrit && relu.Load(texte.Data());
	auto compter = [](const NkUIDocument &d, char *buf, size_t taille) {
		nkentseu::int32 n = 0;
		buf[0] = 0;
		size_t k = 0;
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)d.nodes.Size(); ++i) {
			const char *c = d.nodes[i].component.Data();
			if (c && *c) {
				++n;
				k += (size_t)snprintf(buf + k, k < taille ? taille - k : 0, "%s%s", n > 1 ? "," : "", c);
			}
		}
		return n;
	};
	char a[2048], b[2048];
	const nkentseu::int32 na = compter(gDesign.doc, a, sizeof(a));
	const nkentseu::int32 nb = lu ? compter(relu, b, sizeof(b)) : -1;
	if (!lu)
		b[0] = 0;
	printf("[depot] SAUVE %s -> %s ; relu %s\n"
		   "[depot]   en memoire : %u noeuds, %d composant(s) : %s\n"
		   "[depot]   relu       : %u noeuds, %d composant(s) : %s\n",
		   gSauverChemin, ecrit ? "ecrit" : "ECHEC", lu ? "OUI" : "NON", (unsigned)gDesign.doc.nodes.Size(), na, a,
		   lu ? (unsigned)relu.nodes.Size() : 0u, nb, b);
	fflush(stdout);
}
/// (R19) combien de fois la commande SANS EFFET de la sonde a ete executee
static nkentseu::int32 gSondeRienExecutee = 0;
static nkentseu::editorkit::NkModal &LauncherModalSonde();

static const char *NkNomsPortesSonde(nkentseu::int32 p, char *buf, nkentseu::int32 taille) {
	static const char *const kNoms[6] = {"P", "A", "O", "C", "R", "S"};
	nkentseu::int32 n = 0;
	buf[0] = 0;
	for (nkentseu::int32 i = 0; i < 6; ++i)
		if (p & (1 << i))
			n += snprintf(buf + n, (size_t)(taille - n > 0 ? taille - n : 0), "%s%s", n ? "+" : "", kNoms[i]);
	if (!n)
		snprintf(buf, (size_t)taille, "aucune");
	return buf;
}

/// La source est-elle OUVERTE selon SON PROPRE etat (pas selon la porte) ?
/// -1 = la source n'a pas d'etat lisible hors de sa porte (Preferences, menu Fichier).
static nkentseu::int32 NkSourceOuverte(const char *src, NkEditorShell *sh, nkgui::NkGuiContext &ctx) {
	if (!strcmp(src, "menu-ctx"))
		return sh->IsContextMenuOpen() ? 1 : 0;
	if (!strcmp(src, "export") || !strcmp(src, "export-croix") || !strcmp(src, "export-etat"))
		return gDesign.choixExport.dialogue.open ? 1 : 0;
	if (!strcmp(src, "selecteur-fichier"))
		return gDesign.choixExport.picker.pickerOpen ? 1 : 0;
	if (!strcmp(src, "couleur"))
		return gDesign.picker.ouvert ? 1 : 0;
	if (!strcmp(src, "role"))
		return gDesign.menuRole.ouvert ? 1 : 0;
	if (!strcmp(src, "format"))
		return gDesign.menuFormat.ouvert ? 1 : 0;
	if (!strcmp(src, "rapport"))
		return gDesign.rapportTransposition ? 1 : 0;
	if (!strncmp(src, "palette", 7))
		return sh->PaletteOuverte() ? 1 : 0;
	if (!strcmp(src, "menu-noeud"))
		return (gPanneauToile && gPanneauToile->MenuContextuelOuvert()) ? 1 : 0;
	if (!strcmp(src, "menu-vide"))
		return (gPanneauToile && gPanneauToile->MenuVideOuvert()) ? 1 : 0;
	if (!strncmp(src, "nouveau-projet", 14))
		return LauncherModalSonde().open ? 1 : 0;
	(void)ctx;
	return -1;
}

/// (GEL) LES SOURCES OUVERTES, DANS LE VOCABULAIRE DE L'APPLICATION. Le kit connait ses six
/// portes ; lui seul ne sait pas dire « le menu des roles est ouvert ». Appelee UNIQUEMENT
/// quand une ligne de gel part au journal (jamais par image) -- cf. NkDetecterGel.
static const char *SourcesOuvertesNKUIDesign(void *) {
	static char buf[512];
	int32 n = 0;
	buf[0] = 0;
	const auto ajouter = [&](const char *nom) {
		n += snprintf(buf + n, (size_t)(n < (int32)sizeof(buf) ? sizeof(buf) - (size_t)n : 0), "%s%s",
					  n ? ", " : "", nom);
	};
	if (gShell && gShell->IsContextMenuOpen())
		ajouter("menu contextuel du shell");
	if (gShell && gShell->PaletteOuverte())
		ajouter("palette de commandes");
	if (gDesign.choixExport.dialogue.open)
		ajouter("dialogue Exporter");
	if (gDesign.choixExport.picker.pickerOpen)
		ajouter("selecteur de fichier");
	if (gDesign.picker.ouvert)
		ajouter("selecteur de couleur");
	if (gDesign.menuRole.ouvert)
		ajouter("menu des roles");
	if (gDesign.menuFormat.ouvert)
		ajouter("menu des formats");
	if (gDesign.rapportTransposition)
		ajouter("rapport de transposition");
	if (gPanneauToile && gPanneauToile->MenuContextuelOuvert())
		ajouter("menu du clic droit (noeud)");
	if (gPanneauToile && gPanneauToile->MenuVideOuvert())
		ajouter("menu du clic droit (vide)");
	if (LauncherModalSonde().open)
		ajouter("dialogue Nouveau projet");
	if (!n)
		snprintf(buf, sizeof(buf), "AUCUNE source de l'application n'est ouverte");
	return buf;
}

/// (GEL) LA SONDE DU DETECTEUR : `--sonde-gel=porte|inconnu|sain`. Elle ne touche PAS la
/// machine -- clics et position sont poses dans NOTRE contexte, comme les autres sondes.
///   porte   : `appModal` est pose A CHAQUE IMAGE des l'image 30 et n'est jamais retire --
///             une porte connue qui ne se rouvre pas. La ligne doit nommer A(appModal).
///   inconnu : AUCUNE porte, mais des gestes qui ne changent rien (souris hors des
///             panneaux) -- la ligne « AUCUNE PORTE CONNUE » doit partir.
///   sain    : la meme course, SANS aucun geste. Aucune ligne ne doit partir.
static char gSondeGel[32] = {};
static void GelTick(NkEditorFrameContext &ec, void *user) {
	NkEditorShell *sh = static_cast<NkEditorShell *>(user);
	auto &ctx = ec.Ui();
	const int32 n = gImagesReelles;
	const bool porte = !strcmp(gSondeGel, "porte");
	const bool gestes = !porte ? !strcmp(gSondeGel, "inconnu") : true;
	// La souris est TENUE hors des panneaux : sans cela, celle de la machine ferait varier
	// le survol, donc la cle d'etat, et la course ne serait pas repetable.
	ctx.input.mousePos = {-10000.f, -10000.f};
	if (porte && n >= 30)
		ctx.appModal = true; // la porte qui ne se rouvre jamais
	if (gestes && n >= 40 && n <= 220 && (n % 10) == 0) {
		ctx.input.mouseClicked[0] = true;
		ctx.input.mouseDown[0] = true;
	}
	if (n >= 300) {
		puts("[sonde-gel] fin de course");
		fflush(stdout);
		sh->RequestClose();
	}
}

static const char *src_ou_vide(nkentseu::int32 etape, nkentseu::int32 n, const char s[][32]) {
	return (etape >= 0 && etape < n) ? s[etape] : "";
}

static void PortesTick(NkEditorFrameContext &ec, void *user) {
	using namespace nkentseu;
	NkEditorShell *sh = static_cast<NkEditorShell *>(user);
	nkgui::NkGuiContext &ctx = ec.Ui();
	static nkgui::NkVec2 souris = {-10000.f, -10000.f};
	ctx.input.mousePos = souris; // aux DEUX passages : jamais la souris de la machine
	static int32 dernier = -1;
	if (gImagesReelles == dernier)
		return;
	dernier = gImagesReelles;
	for (int32 b = 0; b < 3; ++b) { // les boutons physiques ne comptent pas
		ctx.input.mouseDown[b] = false;
		ctx.input.mouseClicked[b] = false;
		ctx.input.mouseDoubleClicked[b] = false;
	}

	static char sources[16][32] = {};
	static int32 nSources = -1;
	if (nSources < 0) {
		nSources = 0;
		const char *p = gSondePortes;
		while (*p && nSources < 16) {
			int32 k = 0;
			while (*p && *p != ',' && k < 31)
				sources[nSources][k++] = *p++;
			sources[nSources][k] = 0;
			if (k)
				++nSources;
			if (*p == ',')
				++p;
		}
		printf("[sonde-portes] %d source(s) ; mutation NK_PORTES_MUTATION=%s\n", nSources,
			   getenv("NK_PORTES_MUTATION") ? getenv("NK_PORTES_MUTATION") : "(aucune)");
		fflush(stdout);
	}
	if (gImagesReelles < 40)
		return; // la coquille restaure la disposition et cale le dock

	static int32 etape = 0, t = 0;
	static int32 verts = 0, rouges = 0, nonJugees = 0;
	static int32 portesOuvert = 0, profondeurOuvert = 0;
	static bool departPropre = false, ouvertureConfirmee = false;
	// (R17) LE NEGATIF QUI COMPTE. Un correctif qui ne masque plus rien passerait « les
	// portes se rouvrent » ET « la mutation rougit ». Il est donc exige qu'une source
	// MODALE ouverte masque le corps ENTIER (P|A|C|R) a chaque image de 2 a 9 apres son
	// ouverture. (R18) +2 et non +3 : la valeur lue au passage t est celle de l'image
	// N+t-1 ; t=2 lit N+1, la PREMIERE image qu'une source ecrite dans l'overlay (roles,
	// formats, rapport) doit masquer. L'image N elle-meme (t=1) atteint le corps -- avant
	// comme apres R17, mesure en R18 -- et n'est donc pas exigee. Les popups ordinaires (menu Fichier, couleur) n'y sont pas soumis : ils
	// ne masquent que sous la souris (O).
	static int32 ouvertSansMasque = 0;
	const bool sourceModale = strcmp(src_ou_vide(etape, nSources, sources), "menu-fichier") != 0
							  && strcmp(src_ou_vide(etape, nSources, sources), "couleur") != 0;
	char noms[64];

	if (etape >= nSources) {
		if (t == 0) {
			printf("[sonde-portes] BILAN : %d VERT(S), %d ROUGE(S), %d NON JUGEE(S)\n", verts, rouges, nonJugees);
			fflush(stdout);
			sh->JournalPortesBilan("fin de sonde");
		}
		if (++t > 5)
			sh->RequestClose();
		return;
	}
	const char *src = sources[etape];

	if (t == 0) {
		souris = {-10000.f, -10000.f};
		ctx.input.mousePos = souris;
		const int32 p0 = sh->PortesDuCorps();
		departPropre = (p0 & NkEditorShell::kPortesCorpsEntier) == 0 && ctx.popupDepth == 0;
		ouvertSansMasque = 0;
		printf("[sonde-portes] --- %s : image %d ; DEPART portes %s, popupDepth %d%s\n", src, gImagesReelles,
			   NkNomsPortesSonde(p0, noms, (int32)sizeof(noms)), ctx.popupDepth,
			   departPropre ? "" : "  <<< DEPART NON PROPRE : cette etape ne sera pas jugee");
		bool ouvert = true;
		if (!strcmp(src, "preferences"))
			sh->OpenPreferences(0);
		else if (!strcmp(src, "menu-ctx")) {
			static const char *const kItems[2] = {"Sonde A", "Sonde B"};
			sh->OpenContextMenu({(float32)ctx.viewW * 0.5f, (float32)ctx.viewH * 0.4f}, kItems, nullptr, 2);
		} else if (!strcmp(src, "export") || !strcmp(src, "export-croix") || !strcmp(src, "export-etat"))
			nkuidesign::NkOuvrirDialogueExport(gDesign, false);
		else if (!strcmp(src, "selecteur-fichier"))
			nkuidesign::NkOuvrirSelecteurExport(gDesign, "sonde_portes");
		else if (!strcmp(src, "menu-fichier"))
			ctx.OpenPopupLevel(ctx.GetId("Fichier"), 0);
		else if (!strcmp(src, "couleur")) {
			nkuidesign::DesignState::DemandePicker &d = gDesign.picker;
			d = nkuidesign::DesignState::DemandePicker{};
			d.ouvert = true;
			d.id = ctx.GetId("##nkuidesign.sonde.portes.couleur");
			d.ancre = {(float32)ctx.viewW * 0.5f, (float32)ctx.viewH * 0.3f, 20.f, 20.f};
			snprintf(d.hex, sizeof(d.hex), "#808080");
			d.noeud = -1; // le decor : la selection ne le ferme pas
		} else if (!strcmp(src, "role") || !strcmp(src, "format")) {
			if (!gDesign.doc.IsValidIndex(gDesign.selected) && gDesign.doc.IsValidIndex(1))
				gDesign.SelectSingle(1);
			const nkgui::NkRect ancre = {(float32)ctx.viewW * 0.72f, 160.f, 220.f, 20.f};
			if (!strcmp(src, "role")) {
				gDesign.menuRole.ouvert = true;
				gDesign.menuRole.ancre = ancre;
				gDesign.menuRole.vientDOuvrir = true;
			} else {
				gDesign.menuFormat.ouvert = true;
				gDesign.menuFormat.ancre = ancre;
				gDesign.menuFormat.vientDOuvrir = true;
				gDesign.menuFormat.page = gDesign.selected;
			}
		} else if (!strncmp(src, "palette", 7))
			sh->OpenCommandPalette();
		else if (!strcmp(src, "menu-noeud") || !strcmp(src, "menu-vide")) {
			const nkgui::NkVec2 pos = {(float32)ctx.viewW * 0.45f, (float32)ctx.viewH * 0.45f};
			if (!gPanneauToile) {
				ouvert = false;
				printf("[sonde-portes]     la toile n'est pas montee\n");
			} else if (!strcmp(src, "menu-noeud")) {
				if (!gDesign.doc.IsValidIndex(gDesign.selected) && gDesign.doc.IsValidIndex(1))
					gDesign.SelectSingle(1);
				gPanneauToile->SondeOuvrirMenuNoeud(gDesign.selected, pos);
			} else
				gPanneauToile->SondeOuvrirMenuVide(pos);
		} else if (!strncmp(src, "nouveau-projet", 14))
			LauncherModalSonde().open = true;
		else if (!strcmp(src, "rapport"))
			gDesign.rapportTransposition = true;
		else {
			ouvert = false;
			printf("[sonde-portes]     source INCONNUE : %s\n", src);
		}
		if (!ouvert)
			t = 30; // saute au passage suivant
	}
	if (t == 4 && ctx.popupDepth > 0 && ctx.popupRects[0].w > 0.f)
		souris = {ctx.popupRects[0].x + ctx.popupRects[0].w * 0.5f, ctx.popupRects[0].y + ctx.popupRects[0].h * 0.5f};
	if (t >= 2 && t <= 9 && sourceModale && NkSourceOuverte(src, sh, ctx) != 0) {
		const int32 pm = sh->PortesDuCorps();
		const int32 corpsEntier = NkEditorShell::kPortePreferences | NkEditorShell::kPorteAppModal
								  | NkEditorShell::kPorteMenuCtx | NkEditorShell::kPorteSaisie;
		if ((pm & corpsEntier) == 0) {
			if (ouvertSansMasque == 0)
				printf("[sonde-portes]     !!! image +%d : source MODALE ouverte et le corps N'EST PAS masque (portes %s)\n",
					   t, NkNomsPortesSonde(pm, noms, (int32)sizeof(noms)));
			++ouvertSansMasque;
		}
	}
	if (t == 8) {
		portesOuvert = sh->PortesDuCorps();
		profondeurOuvert = ctx.popupDepth;
		const int32 so = NkSourceOuverte(src, sh, ctx);
		ouvertureConfirmee = (portesOuvert & NkEditorShell::kPortesCorpsEntier) != 0 || profondeurOuvert > 0;
		printf("[sonde-portes]     OUVERT : source %s ; portes %s ; popupDepth %d ; souris (%.0f, %.0f)%s\n",
			   so < 0 ? "(sans etat propre)" : so ? "ouverte" : "FERMEE", NkNomsPortesSonde(portesOuvert, noms, (int32)sizeof(noms)),
			   profondeurOuvert, (double)souris.x, (double)souris.y,
			   ouvertureConfirmee ? "" : "  <<< AUCUNE PORTE NE S'EST FERMEE : ouverture non confirmee");
	}
	if (t == 10) {
		if (!strcmp(src, "preferences")) {
			// Preferences ne connait pas Echap : sa fermeture est le clic HORS de sa fenetre.
			souris = {8.f, (float32)ctx.viewH * 0.5f};
			ctx.input.mousePos = souris;
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = true;
			printf("[sonde-portes]     FERMETURE : clic interne hors de la fenetre (8, %.0f)\n", (double)souris.y);
		} else if ((!strcmp(src, "export-croix") || !strcmp(src, "nouveau-projet-croix")) && ctx.popupDepth > 0) {
			// ⚠️ POURQUOI LA CROIX, ET PAS ECHAP : Echap fait aussi `--popupDepth` dans
			//    NkGuiContext::EndFrame, et un clic hors de la boite fait `popupDepth = 0`
			//    au meme endroit. Ces deux fermetures sont donc RATTRAPEES par NKGui : une
			//    mutation qui oublie de rendre le niveau n'y rougirait jamais (mesure : la
			//    course « mutation » avec Echap est sortie VERTE). La croix est DANS la
			//    boite : NKGui ne ferme rien, seule la modale rend le niveau.
			//    Geometrie de NkModalFrameDraw : cs = titleH - 10, croix a (w - cs - 6, 5).
			const nkgui::NkRect b = ctx.popupRects[0];
			const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
			const float32 cs = lh + 12.f - 10.f;
			souris = {b.x + b.w - cs - 6.f + cs * 0.5f, b.y + 5.f + cs * 0.5f};
			ctx.input.mousePos = souris;
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = true;
			printf("[sonde-portes]     FERMETURE : clic interne sur la CROIX (%.0f, %.0f) -- dans la boite\n",
				   (double)souris.x, (double)souris.y);
		} else if (!strcmp(src, "palette-echap")) {
			const bool prise = sh->PaletteTouche(NkKey::NK_ESCAPE);
			printf("[sonde-portes]     FERMETURE : PaletteTouche(Echap) -- le chemin du rappel OS, sans frappe (%s)\n",
				   prise ? "prise" : "palette deja fermee");
		} else if (!strcmp(src, "palette-commande")) {
			const int32 avant = gSondeRienExecutee;
			sh->PaletteTouche(NkKey::NK_UP); // depuis 0 : la DERNIERE commande, « Sonde : rien »
			sh->PaletteTouche(NkKey::NK_ENTER);
			printf("[sonde-portes]     FERMETURE : Haut puis Entree -- commande sans effet executee %d fois\n",
				   gSondeRienExecutee - avant);
		} else if (!strcmp(src, "export-etat")) {
			// LA FERMETURE PAR L'ETAT, telle que ExportDialogue.h l'ecrit a la confirmation
			// (`c.dialogue.open = false`, l. 283 et 293) : NkModalFrameDraw n'est plus appele,
			// personne ne rend le niveau de popup. La souris reste dans l'ancienne boite.
			gDesign.choixExport.dialogue.open = false;
			printf("[sonde-portes]     FERMETURE : dialogue.open = false (le chemin de la confirmation)\n");
		} else if (!strcmp(src, "selecteur-fichier")) {
			gDesign.choixExport.picker.pickerOpen = false; // pas d'Echap dans ce selecteur : son etat
			printf("[sonde-portes]     FERMETURE : pickerOpen = false (API d'etat)\n");
		} else {
			ctx.input.keyInit[(int32)nkgui::NkGuiKey::Escape] = true;
			printf("[sonde-portes]     FERMETURE : Echap pose dans notre contexte\n");
		}
	}
	if (t == 22) {
		const int32 p = sh->PortesDuCorps();
		const int32 so = NkSourceOuverte(src, sh, ctx);
		const int32 fermees = p & NkEditorShell::kPortesCorpsEntier;
		const char *verdict;
		if (!departPropre) {
			verdict = "NON JUGEE";
			++nonJugees;
		} else if (ouvertSansMasque > 0) {
			verdict = "ROUGE : une MODALE ouverte n'a pas masque le corps (images N+1..N+8)";
			++rouges;
		} else if (!ouvertureConfirmee) {
			verdict = "NON JUGEE";
			++nonJugees;
		} else if (so == 1 && (fermees & NkEditorShell::kPortesCorpsEntier) != 0) {
			// (R19) Une source restee OUVERTE qui MASQUE le corps : elle a pu etre dessinee
			// sous le masque qu'elle cause, et son geste de fermeture ne lui parvient plus.
			// Ce n'est pas « la sonde a manque son geste » -- la mutation `ctxnon` le tranche.
			verdict = "ROUGE : la SOURCE est restee ouverte ET masque le corps";
			++rouges;
		} else if (so == 1) {
			verdict = "NON JUGEE (la SOURCE ne s'est pas fermee : c'est la sonde qui a manque son geste)";
			++nonJugees;
		} else if (fermees != 0 || ctx.popupDepth != 0) {
			verdict = "ROUGE : une porte reste fermee apres la fermeture de sa source";
			++rouges;
		} else {
			verdict = "VERT : toutes les portes rouvertes";
			++verts;
		}
		printf("[sonde-portes]     APRES 12 images : source %s ; portes %s ; popupDepth %d ; souris (%.0f, %.0f) -> %s\n",
			   so < 0 ? "(sans etat propre)" : so ? "OUVERTE" : "fermee", NkNomsPortesSonde(p, noms, (int32)sizeof(noms)),
			   ctx.popupDepth, (double)souris.x, (double)souris.y, verdict);
		fflush(stdout);
		sh->JournalPortesBilan(src);
	}
	if (t >= 30) {
		++etape;
		t = 0;
		return;
	}
	++t;
}

static void DrawMenuBar(NkEditorFrameContext &ec, void *) {
	InjecterClics(ec.Ui());
	// ⚠️ LA RECOLTE DE LA GENERATION EST ICI, ET PAS DANS LE PANNEAU IA.
	//    `DrawMenuBar` est le seul rappel que la coquille appelle a CHAQUE image
	//    sans condition ; le panneau IA, lui, n'est dessine que quand son tiroir
	//    est ouvert. Mesure du 14/09 avec la recolte dans le panneau :
	//    « 0 image(s) pendant l'attente » -- la reponse n'arrivait jamais.
	//    *Une tache de fond ne se recolte pas dans le dessin de ce qui l'affiche.*
	gDesign.RecolterIA();
	++gImagesReelles; // UNE fois par image : la seule cadence de reference
	// (Q9) NK_EVENEMENTS : la souris, le clavier et le depot rejoues par les MEMES
	// rappels que Windows (ceux de la coquille).
	{
		static editorkit::NkEditorScriptEvenements sScript;
		sScript.Tick();
	}
	// (Q7) LES PASTILLES LIEES A LA SELECTION se retirent sans selection.
	if (gShell)
		gShell->SetRailSelection(gDesign.doc.IsValidIndex(gDesign.selected) && gDesign.selected != 0);
	// (Q8) LA TRACE DE LA SELECTION : chaque changement, lu dans l'etat -- la sonde
	// « on ne peut plus rien selectionner » rougit si elle ne bouge pas.
	{
		static int32 sSelAvant = -2;
		if (gDesign.selected != sSelAvant) {
			sSelAvant = gDesign.selected;
			printf("[NKUIDesign] SELECTION image=%d noeud=%d « %s »%c", (int)gImagesReelles, (int)gDesign.selected,
				   gDesign.doc.IsValidIndex(gDesign.selected) ? gDesign.doc.nodes[(uint32)gDesign.selected].label.Data()
															  : "",
				   (char)10);
			fflush(stdout);
		}
	}
	if (gSauverImage >= 0 && gImagesReelles == gSauverImage)
		SauverEtRelire(); // (R20)
	if (gDispoImage >= 0 && gImagesReelles == gDispoImage) {
		gShell->SaveUiState(gDispoChemin);
		printf("[disposition] ecrite : %s\n", gDispoChemin);
		fflush(stdout);
		gShell->RequestClose();
	}
	gDesign.RangerCompteDessins(); // (k2) idem : une fois par image, avant les panneaux
	auto &ctx = ec.Ui();
	using namespace nkentseu::nkgui;

	// ⚠️ NEUF ENTREES, PAS HUIT — et ce n'est pas une preference : c'est une
	//    decision de Rodolf du 2026-08-20 (document 3 §5bis), prise apres
	//    validation de la planche du menu deroulant. Le mot « Fenetre »
	//    designait DEUX choses : les fenetres de l'EDITEUR, et la fenetre de
	//    l'application qu'on dessine. « Cible » recueille la seconde.
	//    ⚠️ LES PLANCHES A HUIT ENTREES SONT PERIMEES SUR CE POINT, et la
	//       specification le dit elle-meme (§22.5 a §22.7 du document Banani et
	//       `plan_fenetre_principale.svg`). Quand une planche et §5bis se
	//       contredisent, §5bis gagne : il porte les decisions datees et signees.
	//
	// ⚠️ UNE ENTREE QUI NE PEUT RIEN PRODUIRE SE GRISE, ELLE NE DISPARAIT PAS
	//    (§5bis.1). Une barre dont le contenu varie avec ce qui est branche
	//    apprend a l'utilisateur une carte qui se deforme sous ses pieds ; une
	//    entree grisee dit « ca existe, pas encore ici ».
	// ⚠️ UN ETAT PORTE UNE COCHE, jamais un libelle qui s'inverse (meme §).
	//    C'est ce qui a fait grossir `nkgui::MenuItem` d'un parametre `checked` :
	//    le manque etait dans le socle, il a ete comble dans le socle.
	// ⚠️ LES ACCENTS SONT LA, ET LA CAUSE DE LEUR ABSENCE A ETE MESUREE.
	//    Trois hypotheses etaient ouvertes : sources sans accents, atlas sans
	//    glyphes, encodage perdu en route. Mesure : `NkGuiDrawList::AddText`
	//    decode l'UTF-8 (`NkFontDecodeUTF8`) et cherche le glyphe PAR POINT DE
	//    CODE ; le pipeline sait donc les rendre. Et `grep` d'une chaine
	//    d'interface accentuee dans NKGui + NKEditorKit rend **zero**. La cause
	//    est la SOURCE, pas la police -- ce fichier est en UTF-8.
	//    ⚠️ La tolerance « francais sans accents » du CLAUDE.md parent ne couvre
	//       QUE les fichiers de `echanges/` et les messages de commit. Elle avait
	//       deborde sur l'interface : c'est exactement une tolerance qui s'etend
	//       au-dela de son domaine. L'interface d'un produit francophone porte
	//       ses accents.

	if (BeginMenu(ctx, "Fichier")) {
		if (MenuItem(ctx, "Nouveau projet…", "Ctrl+N"))
			CmdNew(nullptr);
		MenuItem(ctx, "Ouvrir…", "Ctrl+O", false);
		if (BeginMenu(ctx, "Ouvrir récent")) {
			MenuItem(ctx, "(aucun projet récent)", nullptr, false);
			EndMenu(ctx);
		}
		MenuItem(ctx, "Fermer le projet", "Ctrl+W", false);
		Separator(ctx);
		if (MenuItem(ctx, "Enregistrer", "Ctrl+S"))
			CmdSave(nullptr);
		MenuItem(ctx, "Enregistrer sous…", "Ctrl+Maj+S", false);
		MenuItem(ctx, "Enregistrer tout", "Ctrl+Alt+S", false);
		if (MenuItem(ctx, "Revenir à la version enregistrée"))
			CmdLoad(nullptr);
		Separator(ctx);
		if (BeginMenu(ctx, "Importer")) {
			MenuItem(ctx, "Composant…", nullptr, false);
			MenuItem(ctx, "Document…", nullptr, false);
			MenuItem(ctx, "Ressources…", nullptr, false);
			EndMenu(ctx);
		}
		// ④ UNE SEULE ENTREE, UN SEUL DIALOGUE (2026-09-05). Le sous-menu portait NEUF
		//    combinaisons (page x1/x2/x3, selection x1/x2, SVG, SVG embarque...) : chacune
		//    etait un chemin, et le format ne se demandait nulle part. Rodolf : « les deux
		//    raccourcis, clic droit et Ctrl+E, doivent demander le format, peut-etre
		//    directement dans le dialogue approprie. » Le menu, le raccourci et le clic droit
		//    ouvrent DESORMAIS le meme dialogue -- et Ctrl+E n'est declare qu'une fois (la
		//    table de commandes de la coquille), la lecon de Ctrl+D du matin.
		if (MenuItem(ctx, "Exporter…", "Ctrl+E"))
			nkuidesign::NkOuvrirDialogueExport(gDesign, !gDesign.sel.Empty() || gDesign.selected > 0);
		MenuItem(ctx, "Document .nkgui", nullptr, false);
		MenuItem(ctx, "Valider le document", "Ctrl+Maj+V", false);
		Separator(ctx);
		// ⚠️ LE CHOIX DU BACKEND, ET IL NE DEPEND PLUS D AUCUN PANNEAU.
		//    C'est la seule chose qui devait etre finie AVANT de debrancher le
		//    panneau de droite : le selecteur y vivait, et le retirer sans
		//    remplacant aurait fait perdre la capacite exigee par la regle du
		//    depot (« toute application doit laisser choisir son backend, et un
		//    reglage se change DEPUIS L INTERFACE »).
		//    ⚠️ Il passe par `DesignState::SetGfxConfig` -- la MEME fonction que
		//       le panneau appelait. Deux ecritures auraient diverge, et un choix
		//       fait ici ne se serait pas vu la-bas.
		if (BeginMenu(ctx, "Backend graphique")) {
			uint32 nApis = 0;
			const char *const *apis = nkuidesign::NkGfxApiNames(nApis);
			for (uint32 i = 0; i < nApis; ++i) {
				// ⚠️ LA COCHE MARQUE CE QUE LE FICHIER PORTE, pas ce qui tourne.
				//    Sur un lancement `--gfx=vulkan` avec un fichier qui dit
				//    `dx11`, cocher `vulkan` ferait croire que le fichier a
				//    change. Le pied de fenetre, lui, annonce le redemarrage.
				const bool courant = !gDesign.cfgChoice.Empty()
									 && NkComponentDecl::StrEq(gDesign.cfgChoice.Data(), apis[i]);
				if (MenuItem(ctx, apis[i], nullptr, true, courant)) {
					const bool ok = gDesign.SetGfxConfig(apis[i]);
					// ⚠️ LE RESULTAT SE DIT, PAS L APPEL, et le REDEMARRAGE est
					//    annonce (regle du 18/08 : « ce qui implique un
					//    redemarrage le DIT »). Sans cette phrase, l'utilisateur
					//    regle, ne voit rien changer, et croit que rien n'a ete
					//    ecrit.
					if (gShell)
						gShell->SetFooter(ok ? "gfx écrit dans nkuidesign.cfg, actif au "
											   "PROCHAIN lancement — "
											 : "ÉCHEC d'écriture : rien n'a "
											   "été modifié — ",
										  apis[i]);
				}
			}
			EndMenu(ctx);
		}
		MenuItem(ctx, "Préférences…", "Ctrl+,", false);
		Separator(ctx);
		if (MenuItem(ctx, "Quitter", "Ctrl+Q"))
			CmdQuit(gShell);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Édition")) {
		// CÂBLÉS depuis l'annulation unifiée (§7) — grisés quand la pile est
		// vide de leur côté, comme partout.
		if (MenuItem(ctx, "Annuler", "Ctrl+Z", gDesign.histoire.PeutAnnuler()))
			CmdUndo(nullptr);
		if (MenuItem(ctx, "Rétablir", "Ctrl+Y", gDesign.histoire.PeutRetablir()))
			CmdRedo(nullptr);
		Separator(ctx);
		// ── CÂBLÉS (01/09) — le geste vit dans DesignState, le menu et le
		//    clavier l'appellent tous deux. ⚠️ GRISÉS SUR L'ÉTAT RÉEL, pas
		//    « toujours actifs » : un « Coller » cliquable avec un
		//    presse-papiers vide est un paramètre déclaré qui n'est pas honoré.
		const bool aSel = !gDesign.sel.Empty() && !gDesign.sel.Contains(0);
		if (MenuItem(ctx, "Couper", "Ctrl+X", aSel))
			gDesign.CouperSelection();
		if (MenuItem(ctx, "Copier", "Ctrl+C", aSel))
			gDesign.CopierSelection();
		if (MenuItem(ctx, "Coller", "Ctrl+V", gDesign.pressePapiersPlein))
			gDesign.CollerPressePapiers();
		MenuItem(ctx, "Coller à la même place", "Ctrl+Maj+V", false);
		MenuItem(ctx, "Coller le style seul", "Ctrl+Alt+V", false);
		if (MenuItem(ctx, "Dupliquer", "Ctrl+D", aSel))
			gDesign.DupliquerSelection();
		if (MenuItem(ctx, "Supprimer", "Suppr", aSel))
			gDesign.SupprimerSelection();
		Separator(ctx);
		if (MenuItem(ctx, "Tout sélectionner", "Ctrl+A"))
			gDesign.ToutSelectionner();
		MenuItem(ctx, "Sélectionner tous les éléments du même rôle", nullptr, false);
		if (MenuItem(ctx, "Désélectionner", "Échap"))
			gDesign.SelectClear();
		Separator(ctx);
		MenuItem(ctx, "Rechercher…", "Ctrl+F", false);
		MenuItem(ctx, "Remplacer une propriété…", "Ctrl+H", false);
		MenuItem(ctx, "Renommer", "F2", false);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Affichage")) {
		MenuItem(ctx, "Zoom avant", "Ctrl++", false);
		MenuItem(ctx, "Zoom arrière", "Ctrl+-", false);
		MenuItem(ctx, "Zoom 100 %", "Ctrl+0", false);
		MenuItem(ctx, "Ajuster à la sélection", "Maj+2", false);
		MenuItem(ctx, "Ajuster à la page", "Maj+1", false);
		Separator(ctx);
		MenuItem(ctx, "Grille", "Ctrl+'", false, true);
		// CÂBLÉ (01/09) : la coche LIT l'état réel. ⚠️ Elle était posée à `true`
		// en dur — une case toujours cochée à côté d'un aimant qui ne faisait
		// rien : exactement le « paramètre déclaré qui n'est pas honoré » que ce
		// dépôt a mesuré huit fois cette semaine.
		if (MenuItem(ctx, "Magnétisme", "Ctrl+;", true, gDesign.aimantActif))
			gDesign.aimantActif = !gDesign.aimantActif;
		MenuItem(ctx, "Règles", nullptr, false, false);
		MenuItem(ctx, "Repères intelligents", nullptr, false, true);
		Separator(ctx);
		MenuItem(ctx, "Marges et remplissage", nullptr, false, true);
		MenuItem(ctx, "Régions de fenêtre", nullptr, false, false);
		MenuItem(ctx, "Éléments désactivés par héritage", nullptr, false, false);
		Separator(ctx);
		if (BeginMenu(ctx, "Mode")) {
			MenuItem(ctx, "Design", "Ctrl+1", false, true);
			MenuItem(ctx, "Behavior", "Ctrl+2", false);
			MenuItem(ctx, "Animation", "Ctrl+3", false);
			MenuItem(ctx, "Split", "Ctrl+4", false);
			EndMenu(ctx);
		}
		// ⚠️ LA LISTE VIENT DE LA BIBLIOTHEQUE, PAS D UNE TABLE ECRITE ICI.
		//    Une liste en dur afficherait aujourd'hui les bons noms sans lire
		//    quoi que ce soit -- et n'afficherait pas le theme que l'utilisateur
		//    deposera demain dans son dossier personnel.
		if (BeginMenu(ctx, "Thème")) {
			for (uint32 i = 0; i < gThemes.Count(); ++i) {
				const bool courant = (i == gThemes.CurrentIndex());
				if (MenuItem(ctx, gThemes.At(i).Name().CStr(), nullptr, true, courant))
					AppliquerTheme(i);
			}
			Separator(ctx);
			MenuItem(ctx, "Système", nullptr, false);
			EndMenu(ctx);
		}
		// LES PANNEAUX REELLEMENT ENREGISTRES -- la coquille les liste elle-meme.
		// ⚠️ Une liste ecrite a la main ici mentirait des le premier panneau
		//    debranche : c'est exactement ce qui vient d'arriver aux quatre
		//    anciens.
		if (BeginMenu(ctx, "Panneaux")) {
			if (gShell)
				gShell->DrawPanelsMenuItems();
			EndMenu(ctx);
		}
		MenuItem(ctx, "Plein écran", "F11", false);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Objet")) {
		MenuItem(ctx, "Attribuer un rôle…", nullptr, false);
		MenuItem(ctx, "Retirer le rôle", nullptr, false);
		Separator(ctx);
		// CÂBLÉS (01/09) : grouper demande au moins un élément, dégrouper
		// demande un conteneur qui a des enfants — le grisé DIT la condition.
		if (MenuItem(ctx, "Grouper", "Ctrl+G", !gDesign.sel.Empty() && !gDesign.sel.Contains(0)))
			gDesign.GrouperSelection();
		if (MenuItem(ctx, "Dégrouper", "Ctrl+Maj+G",
					 gDesign.doc.IsValidIndex(gDesign.selected) && gDesign.selected != 0
						 && !gDesign.doc.nodes[(uint32)gDesign.selected].children.Empty()))
			gDesign.DegrouperSelection();
		MenuItem(ctx, "Convertir en composant", "Ctrl+K", false);
		MenuItem(ctx, "Détacher l'instance", nullptr, false);
		MenuItem(ctx, "Promouvoir en composant partagé", nullptr, false);
		Separator(ctx);
		if (BeginMenu(ctx, "Aligner")) {
			MenuItem(ctx, "(à brancher)", nullptr, false);
			EndMenu(ctx);
		}
		if (BeginMenu(ctx, "Répartir")) {
			MenuItem(ctx, "(à brancher)", nullptr, false);
			EndMenu(ctx);
		}
		if (BeginMenu(ctx, "Ordre")) {
			MenuItem(ctx, "Premier plan", nullptr, false);
			MenuItem(ctx, "Avancer", nullptr, false);
			MenuItem(ctx, "Reculer", nullptr, false);
			MenuItem(ctx, "Arrière-plan", nullptr, false);
			EndMenu(ctx);
		}
		Separator(ctx);
		MenuItem(ctx, "Verrouiller", "Ctrl+L", false, false);
		// ⚠️ TROIS MOTS, PAS UN (§5bis.5 et §11.1) : masquer pour TRAVAILLER n'est
		//    pas rendre invisible a l'utilisateur final. « Masquer » tout court
		//    confondrait les deux au moment ou l'on choisit.
		MenuItem(ctx, "Masquer dans l'éditeur", "Ctrl+Maj+H", false, false);
		if (BeginMenu(ctx, "Disponibilité")) {
			MenuItem(ctx, "Actif", nullptr, false, true);
			MenuItem(ctx, "Désactivé", nullptr, false);
			MenuItem(ctx, "Lecture seule", nullptr, false);
			MenuItem(ctx, "Occupé", nullptr, false);
			EndMenu(ctx);
		}
		EndMenu(ctx);
	}

	// ⚠️ « CIBLE » SE PLACE ENTRE « OBJET » ET « COMPORTEMENT » (§5bis.6), et sa
	//    place n'est pas decorative : c'est le neuvieme menu, adopte le 20/08
	//    pour lever la collision du mot « Fenetre ». Tout ce qui releve de
	//    l'application VISEE est ici ; « Fenetre » reste a l'editeur.
	if (BeginMenu(ctx, "Cible")) {
		if (BeginMenu(ctx, "Classe")) {
			MenuItem(ctx, "Bureau", nullptr, false, true);
			MenuItem(ctx, "Mobile", nullptr, false);
			MenuItem(ctx, "Web", nullptr, false);
			EndMenu(ctx);
		}
		MenuItem(ctx, "Appareil…", nullptr, false);
		if (BeginMenu(ctx, "Orientation")) {
			MenuItem(ctx, "Portrait", nullptr, false);
			MenuItem(ctx, "Paysage", nullptr, false, true);
			EndMenu(ctx);
		}
		Separator(ctx);
		// L'ecran 26 le montre COCHE et il PILOTE vraiment l'affichage
		// (ecrans 11/12) : la coche suit l'etat, cliquer bascule.
		if (MenuItem(ctx, "Afficher la zone sûre", nullptr, true, gDesign.zoneSure))
			gDesign.zoneSure = !gDesign.zoneSure;
		if (BeginMenu(ctx, "Décoration")) {
			MenuItem(ctx, "Native", nullptr, false, true);
			MenuItem(ctx, "Client", nullptr, false);
			EndMenu(ctx);
		}
		MenuItem(ctx, "Curseur…", nullptr, false);
		Separator(ctx);
		// ── LA LANGUE DU DOCUMENT (multilingue, 01/09) ─────────────────────
		// Le selecteur vit ICI : ni Banani ni Lunacy n'en montrent un (leurs
		// captures n'ont pas d'i18n) — la langue est une CIBLE de l'interface
		// concue, comme l'appareil. Bascule A CHAUD : la coche suit, l'apercu
		// et l'edition en place suivent a l'image meme. « Ajouter » declare la
		// langue au DOCUMENT (cle additive `langues`, annulable).
		if (BeginMenu(ctx, "Langue du document")) {
			const bool principale = gDesign.langueActive.Empty();
			if (MenuItem(ctx, "Principale (texte)", nullptr, true, principale))
				gDesign.langueActive = NkString();
			for (nkentseu::uint32 li = 0; li < (nkentseu::uint32)gDesign.doc.langues.Size();
				 ++li) {
				const char *code = gDesign.doc.langues[li].Data();
				const bool active =
					!principale && NkComponentDecl::StrEq(gDesign.langueActive.Data(), code);
				if (MenuItem(ctx, code, nullptr, true, active))
					gDesign.langueActive = nkentseu::NkString(code);
			}
			Separator(ctx);
			static const char *const kLangues[3] = {"en", "es", "de"};
			for (int32 la = 0; la < 3; ++la) {
				bool deja = false;
				for (nkentseu::uint32 li = 0;
					 li < (nkentseu::uint32)gDesign.doc.langues.Size(); ++li)
					if (NkComponentDecl::StrEq(gDesign.doc.langues[li].Data(), kLangues[la]))
						deja = true;
				if (deja)
					continue;
				char lib[32];
				snprintf(lib, sizeof(lib), "Ajouter « %s »", kLangues[la]);
				if (MenuItem(ctx, lib)) {
					gDesign.doc.langues.PushBack(nkentseu::NkString(kLangues[la]));
					gDesign.doc.MarkHumanEdit(0);
					gDesign.langueActive = nkentseu::NkString(kLangues[la]);
					gDesign.status = NkString("Langue ajoutée au document — les textes non "
											  "traduits s'affichent atténués (voir le Rapport).");
				}
			}
			EndMenu(ctx);
		}
		Separator(ctx);
		MenuItem(ctx, "Points de rupture…", nullptr, false);
		MenuItem(ctx, "Aperçu multi-cibles", nullptr, false);
		// L'écran 27 : l'entrée OUVRE le rapport (mécanisme absent, et le
		// rapport le dit : 0 constat).
		if (MenuItem(ctx, "Rapport de transposition…"))
			gDesign.rapportTransposition = true;
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Comportement")) {
		MenuItem(ctx, "Ouvrir le graphe", nullptr, false);
		MenuItem(ctx, "Vue Code", "Ctrl+²", false);
		Separator(ctx);
		MenuItem(ctx, "Ajouter un événement…", nullptr, false);
		MenuItem(ctx, "Lier à un callback…", nullptr, false);
		MenuItem(ctx, "Délier", nullptr, false);
		MenuItem(ctx, "Gestionnaire de callbacks…", nullptr, false);
		Separator(ctx);
		MenuItem(ctx, "Simuler", "F5", false);
		MenuItem(ctx, "Geler la simulation", "F6", false);
		MenuItem(ctx, "Recharger la simulation", "Maj+F5", false);
		MenuItem(ctx, "Système simulé…", nullptr, false);
		MenuItem(ctx, "Rapport de couverture…", nullptr, false);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "IA")) {
		if (BeginMenu(ctx, "Générer")) {
			MenuItem(ctx, "un composant…", nullptr, false);
			MenuItem(ctx, "un comportement…", nullptr, false);
			MenuItem(ctx, "une animation…", nullptr, false);
			EndMenu(ctx);
		}
		MenuItem(ctx, "Proposer un rôle pour la sélection", nullptr, false);
		Separator(ctx);
		// ⚠️ COCHEE ET VISIBLE (§5bis.8) : l'exposer dit a l'utilisateur que
		//    l'outil REUTILISE avant de dupliquer. C'est une garantie qu'un
		//    comportement silencieux ne peut pas donner.
		MenuItem(ctx, "Chercher dans la bibliothèque avant de générer", nullptr, false, true);
		Separator(ctx);
		// 🔴 CETTE PORTE MENAIT AILLEURS QUE LA PASTILLE. `FocusPanel("IA")`
		//    ANCRAIT le panneau (en bas, avant le 20/09) ; la pastille du rail
		//    droit le DEPLIE en tiroir. Deux portes, deux resultats -- et c'est
		//    par celle-ci que Rodolf est passe, d'ou « je n'ai pas de pastille a
		//    droite » : il n'a jamais vu celle qui marche.
		//    Elles menent desormais au MEME endroit.
		if (MenuItem(ctx, "Ouvrir le chat IA"))
			OuvrirTiroirIA();
		MenuItem(ctx, "Réglages du modèle…", nullptr, false);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Fenêtre")) {
		MenuItem(ctx, "Nouvelle fenêtre", nullptr, false);
		MenuItem(ctx, "Détacher l'onglet dans une fenêtre", nullptr, false);
		Separator(ctx);
		if (BeginMenu(ctx, "Disposition")) {
			MenuItem(ctx, "Par défaut", nullptr, false);
			MenuItem(ctx, "Design", nullptr, false);
			MenuItem(ctx, "Comportement", nullptr, false);
			MenuItem(ctx, "Enregistrer la disposition…", nullptr, false);
			MenuItem(ctx, "Réinitialiser", nullptr, false);
			EndMenu(ctx);
		}
		Separator(ctx);
		MenuItem(ctx, "Onglet suivant", "Ctrl+Tab", false);
		MenuItem(ctx, "Onglet précédent", "Ctrl+Maj+Tab", false);
		EndMenu(ctx);
	}

	if (BeginMenu(ctx, "Aide")) {
		MenuItem(ctx, "Documentation", "F1", false);
		MenuItem(ctx, "Raccourcis clavier…", nullptr, false);
		MenuItem(ctx, "Glossaire des composants", nullptr, false);
		Separator(ctx);
		MenuItem(ctx, "Gestionnaire de greffons…", nullptr, false);
		Separator(ctx);
		MenuItem(ctx, "Console…", nullptr, false);
		MenuItem(ctx, "Informations système — copier", nullptr, false);
		Separator(ctx);
		MenuItem(ctx, "Rechercher les mises à jour", nullptr, false);
		MenuItem(ctx, "À propos de NkUIDesign", nullptr, false);
		EndMenu(ctx);
	}
}

// LA BANDE 2 : les onglets de projets (document 3 §6).
//
// ⚠️ L ONGLET ACTIF SE DIT DE **DEUX** FACONS, ET C EST §6 QUI L EXIGE :
//    « fond legerement different (--bg-canvas vs --bg-subtle pour les
//    inactifs), petit lisere accent EN BAS de l onglet actif ». Deux signaux
//    plutot qu un : un fond seul se perd sur un ecran mal calibre, un lisere
//    seul disparait sous une barre de defilement. Rodolf le demande d ailleurs
//    par le second — « une barre bleue de hauteur fine ».
//
// ⚠️ EN BAS, PAS EN HAUT. Un lisere pose en haut se lit comme la separation
//    d avec la barre de menu, pas comme l etat de l onglet.
//
// ⚠️ DES JETONS, PAS UNE COULEUR. `theme.tabActive` / `theme.tab` / `theme.accent`
//    existent deja dans `NkGuiTheme` -- ecrire un bleu litteral ici, c est une
//    couleur de plus parmi les 426 en dur que le depot a mesurees chez les
//    consommateurs de NKGui, et un onglet qui resterait bleu en theme clair.
//
// ⚠️ CE QUI N EST PAS ENCORE LA, et §6 le decrit : miniature du projet, point
//    de non-enregistrement, croix de fermeture au survol, `+` ouvrant le
//    Launcher en modal, glisser pour reordonner/detacher, molette = defilement
//    horizontal avec chevron de depassement. Les libelles restent INERTES : ce
//    qui se juge ici est la geometrie et l etat, pas le comportement.
// {A} L ETAT DU CHOIX « NOUVEAU PROJET » (§3 du plan, l.84). Le kit porte
//    deja tout le cadre (`NkModalFrameDraw` : voile, titre deplacable, croix,
//    Echap, clic dehors) -- la porte a ete appliquee AVANT d ecrire, et il n y
//    avait qu a le consommer. Ce n est PAS le Launcher complet du §3 (sidebar
//    + grille de projets) : c est son raccourci a trois branches, celui que le
//    §3 decrit pour le bouton « + Nouveau projet ».
static nkentseu::editorkit::NkModal gLauncherModal;
/// (R19) la sonde des portes est definie plus haut que ce dialogue
static nkentseu::editorkit::NkModal &LauncherModalSonde() {
	return gLauncherModal;
}

static void DrawProjectTabs(NkEditorFrameContext &ec, void *) {
	// ── LA PIPETTE PREND SON CLIC ICI, ET C'EST MESURE ────────────────────
	// Ce crochet de barre d'outils est dessine AVANT le dock (`DrawToolbar`
	// precede `DrawPanels` dans la coquille) : c'est le seul endroit de
	// l'application ou l'on voit le clic avant que la toile ne le recoive.
	// Sans ca, prelever une couleur deselectionnerait au passage.
	nkuidesign::NkPipettePrendLeClic(ec.Ui(), gDesign);
	auto &ctx = ec.Ui();
	using namespace nkentseu::nkgui;
	// ⚠️ LES ONGLETS SONT LES DOCUMENTS OUVERTS (5e retour de Rodolf, 01/09) :
	//    plus de libelles ecrits en dur. Cliquer BASCULE le document actif
	//    (DesignState::BasculerVers — Hierarchie, toile, Inspecteur, titre
	//    suivent, ils lisent deja `doc`) ; la croix FERME (un document modifie
	//    refuse et le dit) ; le « + » ouvre « Nouveau projet », dont la branche
	//    Vierge cree un NOUVEL onglet.
	// ⚠️ DES RECTANGLES EXPLICITES, pas le flux : la bande fait 28 px et les
	//    onglets doivent la remplir exactement. `SetNextItemRect` est le moyen
	//    prevu par NKGui pour poser un widget (mesure NKGuiDrawTest, 9/9).
	// ⚠️ J AI FAILLI REECRIRE CE QUE LE SOCLE PORTE — ET LA MESURE M A ARRETE.
	//    Ma premiere version dessinait a la main le fond de chaque onglet et son
	//    lisere. Capture : **aucun lisere**, parce que `Button` repeint son propre
	//    fond PAR-DESSUS. En allant lire pourquoi, j ai trouve que
	//    `nkgui::TabBarEx` fait deja exactement ce que le document 3 §6 demande :
	//        bg  = selected ? theme.panel : theme.button        (fond distinct)
	//        AddRectFilled({r.x, r.y + r.h - 3, r.w, 3}, theme.accent)  (lisere)
	//    Fond different pour l actif, contour pour les autres, lisere d accent de
	//    3 px EN BAS. Ecrire ma version, c etait `Splitter` une seconde fois :
	//    reecrire chez soi ce que la bibliotheque porte, faute d avoir cherche.
	//    ⚠️ Et le lisere prend le JETON `theme.accent` : il suit le theme, il ne
	//       reste pas bleu quand on passe en clair.
	const NkRect z = ctx.layout.region;
	// ⚠️ COSTUME BANANI (remandat 31/08) : les onglets V2 du TopHeader portent
	//    une ICONE de document, la pastille « ● » du non-enregistre, une croix
	//    de fermeture et le « + » — le widget TabBar du socle n'en dessine
	//    aucun. Le dessin est donc posé ICI, à la main, aux mesures du JSX
	//    (onglet 27 px aligné en bas de la bande de 28, padding 12, écarts 4,
	//    libellé 11 px, liseré actif 2 px, filet vertical `border` entre
	//    onglets). Ce n'est pas TabBar réécrit « faute d'avoir cherché » : le
	//    socle est LU, il ne porte pas ce costume ; un opt-in de kit viendra si
	//    NKCode veut le même.
	auto &dl = ctx.DL();
	auto &F = nkuidesign::costume::Fontes();
	namespace cos = nkuidesign::costume;
	dl.AddRectFilled(z, ctx.theme.panel);
	const float32 hT = 27.f;
	const float32 yT = z.y + z.h - hT;
	float32 x = z.x;
	const uint32 nOnglets = (uint32)gDesign.ouverts.Size();
	for (uint32 i = 0; i < nOnglets; ++i) {
		const bool actif = (i == gDesign.ongletActif);
		// libellé : nom du document ; « ● » quand il est modifié (l'actif est
		// MESURÉ en direct — même canal que la barre de titre — l'inactif
		// porte la mesure prise à son rangement).
		const bool modif = actif ? gDocumentModifie : gDesign.ouverts[i].modifie;
		// L'onglet ACTIF lit le titre VIVANT (un renommage de document se voit
		// sans attendre une bascule) ; l'inactif lit son ardoise.
		const char *nomOng = actif ? gDesign.doc.title.Data() : gDesign.ouverts[i].nom.Data();
		char libelle[64];
		snprintf(libelle, sizeof(libelle), "%s%s", (nomOng && *nomOng) ? nomOng : "(sans nom)",
				 modif ? " \xE2\x97\x8F" : "");
		const float32 wTxt = cos::Largeur(F.px11, libelle);
		const float32 wX = cos::Largeur(F.px11, "\xC3\x97"); // « × »
		// 12 (pad) + 10 (icône) + 4 + texte + 4 + croix + 12 (pad)
		const float32 wT = 12.f + 10.f + 4.f + wTxt + 4.f + wX + 12.f;
		const NkRect r = {x, yT, wT, hT};
		if (actif) {
			dl.AddRectFilled(r, ctx.theme.bgPrimary); // l'actif rejoint le fond document (V2)
			dl.AddRectFilled({r.x, r.y + r.h - 2.f, r.w, 2.f}, ctx.theme.accent);
		}
		cos::IcDocOnglet(dl, r.x + 12.f, r.y + (hT - 10.f) * 0.5f,
						 actif ? ctx.theme.accent : ctx.theme.textMuted);
		const float32 yTxt = cos::CentrerY(F.px11, r.y, hT);
		if (actif)
			cos::TexteGras(dl, F.px11, r.x + 26.f, yTxt, libelle, ctx.theme.text, 0.3f);
		else
			cos::Texte(dl, F.px11, r.x + 26.f, yTxt, libelle, ctx.theme.textMuted);
		// la croix de fermeture (10 px, muted) — inerte, et elle le DIT au clic.
		const NkRect rx = {r.x + 26.f + wTxt + 4.f, r.y, wX + 6.f, hT};
		cos::Texte(dl, F.px11, rx.x, yTxt, "\xC3\x97", ctx.theme.textMuted);
		dl.AddLine({r.x + r.w, r.y, }, {r.x + r.w, r.y + r.h}, ctx.theme.border, 1.f);
		// clics : croix d'abord (elle est DANS l'onglet), l'onglet ensuite.
		if (ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
			const NkVec2 m = ctx.input.mousePos;
			const bool dansX = m.x >= rx.x && m.x < rx.x + rx.w && m.y >= rx.y && m.y < rx.y + rx.h;
			const bool dansT = m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h;
			if (dansX) {
				// FermerOnglet refuse un document modifie et le dit au pied.
				// La liste peut se raccourcir : on sort de la boucle.
				gDesign.FermerOnglet(i);
				break;
			} else if (dansT)
				gDesign.BasculerVers(i);
		}
		x += wT;
	}
	// Le « + » (15 px, muted) — il ouvre le choix « Nouveau projet ».
	{
		const NkRect rp = {x, yT, 12.f + cos::Largeur(F.px15, "+") + 12.f, hT};
		cos::Texte(dl, F.px15, rp.x + 12.f, cos::CentrerY(F.px15, rp.y, hT), "+",
				   ctx.theme.textMuted);
		if (ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
			const NkVec2 m = ctx.input.mousePos;
			if (m.x >= rp.x && m.x < rp.x + rp.w && m.y >= rp.y && m.y < rp.y + rp.h)
				gLauncherModal.open = true;
		}
		// 🔴 LA BANDE DIT OU ELLE S'ARRETE, ET C'EST UNE CAPTURE DE RODOLF QUI L'A
		//    EXIGE (27/09). Ces onglets se dessinent a coups de rectangles EXPLICITES,
		//    avec leur propre `x` local : ils ne deplacent JAMAIS `ctx.layout.cursor`.
		//    Tant qu'ils etaient seuls dans la bande, cela ne se voyait pas. Depuis que
		//    la barre d'outils monte AUSSI une racine `.nkgui` apres eux, le document
		//    repartait du DEBUT de la bande — la capture montrait « Annuler d Retablir
		//    e Landing_Page x Grouper », des fragments d'onglet entre les boutons.
		//
		//    *Un dessin en coordonnees explicites qui ne repose pas le curseur laisse
		//    le suivant repartir de zero* — et le suivant n'existait pas le jour ou ce
		//    code a ete ecrit.
		ctx.layout.cursor.x = rp.x + rp.w + 8.f;
		ctx.layout.cursor.y = z.y;
		ctx.layout.lineStartX = ctx.layout.cursor.x;
	}

	// ── LE CHOIX A TROIS BRANCHES (§3) ──────────────────────────────────────
	if (gLauncherModal.open) {
		using namespace nkentseu::editorkit;
		const float32 cw = 380.f, chh = 210.f;
		NkModalFrame fr = NkModalFrameDraw(ctx, gLauncherModal, "Nouveau projet", cw, chh);
		if (!fr.visible || fr.closeAsked) {
			gLauncherModal.open = false;
			gLauncherModal.posInit = false; // se recentre a la prochaine ouverture
			return;
		}
		// ⚠️ LES WIDGETS DU CONTENU PASSENT EN COUCHE OVERLAY. Le cadre est
		//    peint dans `dlOverlay` ; un Button ordinaire ecrirait dans `dl`,
		//    soumise AVANT -- il serait DERRIERE la boite, cliquable mais
		//    invisible. C est le meme piege que le contenu de modale de
		//    NK3DModeler, resolu ici par la couche prevue (`PushOverlay`).
		PushOverlay(ctx);
		ctx.BeginLayout({fr.content.x, fr.content.y, fr.content.w, fr.content.h});

		// -- Vierge : la seule branche qui EXISTE, et elle agit ----------
		if (Button(ctx, "Vierge — un canvas vide, une « Page 1 »")) {
			CmdNew(nullptr);
			gLauncherModal.open = false;
			gLauncherModal.posInit = false;
			if (gShell)
				gShell->SetFooter("Nouveau projet vierge : ", "ouvert dans un nouvel onglet.");
		}
		nkgui::TextWrapped(ctx, "S'ouvre dans un NOUVEL onglet — le document courant "
								"reste ouvert dans le sien.");
		ctx.Spacing(8.f);

		// -- Gabarit / Via IA : grisees, et elles DISENT pourquoi --------
		// ⚠️ GRISEES, PAS MUETTES, PAS ABSENTES -- la regle des menus : un
		//    bouton actif qui ne fait rien se lit comme un bouton casse ; un
		//    bouton absent fait croire que la branche n existe pas.
		ctx.BeginDisabled();
		(void)Button(ctx, "Gabarit — Formulaire, Dashboard, HUD…");
		nkgui::TextWrapped(ctx, "La galerie de gabarits n'est pas encore branchée.");
		ctx.Spacing(8.f);
		(void)Button(ctx, "Via IA — décrire l'écran, valider l'aperçu");
		nkgui::TextWrapped(ctx, "La génération n'est pas encore branchée (doc 1 §6.1).");
		ctx.EndDisabled();

		PopOverlay(ctx);
	}
}

// =============================================================================
//  `--releve-menus` — LE RELEVE DE LA BARRE DE MENUS, SANS FENETRE ET SANS GPU
// =============================================================================
// ⚠️ CE QU'IL CORRIGE, ET LE COUT DEJA PAYE. Le sous-menu « Fichier > Backend
//    graphique » n'a JAMAIS ete photographie ouvert : deux tours de suite, un
//    agent a livre le cablage sans pouvoir montrer le resultat, faute d'un
//    levier pour derouler un menu. Le meme manque a laisse partir trois etapes
//    livrees sans que personne ne voie la fenetre, et un menu contextuel de
//    NK3DModeler invisible un tour entier. Ce n'est pas une panne de
//    l'application : c'est l'instrument qui manquait.
//
// ⚠️ UNE AFFIRMATION A CORRIGER, ET ELLE VENAIT DE MOI : « --dump-ui fonctionne
//    deja sans GPU » est FAUX. `--dump-ui` ne fait que lever un drapeau ; le
//    releve est ecrit par `DumpUiRects`, pose en OVERLAY de la coquille, donc
//    apres la fenetre, le contexte graphique et la boucle de rendu. Le seul
//    chemin reellement sans fenetre etait `--probe`, et `Probe.h` ne construit
//    aucun `NkGuiContext` : il n'a jamais vu un widget. La propriete etait donc
//    a CREER, pas a garder.
//
// ⚠️ ET ELLE EST CREABLE PARCE QUE NKGUI EST ENTIEREMENT CALCULABLE SANS CARTE :
//    `NkGuiContext::Init` se reduit a `viewW = w; viewH = h;`, les widgets
//    produisent une liste de dessin et rien d'autre, et `LoadEmbedded` construit
//    son atlas EN MEMOIRE VIVE — l'envoi a la carte est le travail du backend,
//    et on ne le fait pas ici. La police est donc la VRAIE, donc les largeurs
//    mesurees sont les vraies : le releve n'est pas une maquette.
//
// ⚠️ AUCUNE API D'INJECTION D'ENTREE N'A ETE AJOUTEE A NKGUI POUR CA, et c'est
//    la frontiere du chantier. Ce harnais ecrit dans `ctx.input` — le meme champ
//    public que la coquille remplit depuis les evenements de la fenetre. LIRE
//    est dans le socle ; AGIR reste chez l'appelant, tant qu'un chantier
//    « agir » n'aura pas ete ouvert pour de bon.
//
// ⚠️ LA SOURIS EST PILOTEE PAR LE RELEVE LUI-MEME, pas par des coordonnees
//    ecrites a la main. Un harnais qui clique en (42, 17) casse a la premiere
//    entree de menu ajoutee, en silence, et personne ne sait pourquoi. Ici,
//    chaque trame lit le rectangle publie a la trame precedente et vise son
//    centre : la geometrie peut bouger, le harnais suit.
static int32 ReleveMenus(const char *chemin) {
	// ── 1. La configuration, lue COMME AU LANCEMENT ──────────────────────
	// ⚠️ LE MEME CHEMIN ET LE MEME CLASSIFICATEUR. La preuve de recette porte
	//    sur « la coche est sur ce que le FICHIER porte » : si le harnais lisait
	//    le fichier autrement que l'application, il pourrait prouver une coche
	//    que personne ne verra jamais a l'ecran.
	const NkString cfgText = nkentseu::NkFile::Exists(nkuidesign::NkGfxConfigPath())
								 ? nkentseu::NkFile::ReadAllText(nkuidesign::NkGfxConfigPath())
								 : NkString("");
	char cfgGfx[32] = {0};
	nkuidesign::NkGfxConfigClassify(nkentseu::NkFile::Exists(nkuidesign::NkGfxConfigPath()),
									cfgText.Data(), cfgGfx, sizeof(cfgGfx));
	gDesign.Init();
	gDesign.cfgChoice = NkString(cfgGfx);
	logger.Info("[NKUIDesign/relevé] nkuidesign.cfg porte gfx='{0}' (vide = clé absente)",
				cfgGfx[0] ? cfgGfx : "(aucune)");

	// ── 2. Le contexte et la police, en memoire vive ─────────────────────
	static nkgui::NkGuiContext ctx;
	if (!ctx.Init(1456, 939)) {
		logger.Error("[NKUIDesign/relevé] NkGuiContext::Init a refusé.");
		return 3;
	}
	static nkgui::NkGuiFont police;
	// ⚠️ SI LA POLICE MANQUE, ON LE DIT ET ON CONTINUE. Sans elle, NKGui replie
	//    sur des largeurs forfaitaires (40 px par titre) : la structure, les
	//    libelles et les etats restent JUSTES, seule la geometrie devient
	//    approximative. Un releve muet vaudrait moins qu'un releve annonce
	//    comme approximatif.
	if (!police.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 14.f))
		logger.Warn("[NKUIDesign/relevé] police embarquée indisponible : les largeurs "
					"seront forfaitaires, les libellés et les états restent exacts.");
	else
		ctx.font = &police;

	nkgui::NkGuiIntrospectActiver(ctx, true);
	nkentseu::editorkit::NkEditorFrameContext ec;
	ec.ui = &ctx;
	ec.dt = 1.f / 60.f;

	// La barre de menus telle que la coquille la pose : bande haute de 28 px,
	// origine a x=56 (le logo carre de 56 est a cheval sur les deux bandes).
	const nkgui::NkRect barre = {56.f, 0.f, 1456.f - 56.f - 126.f, 28.f};

	// ── 3. Les trames, et la souris pilotee par le releve ────────────────
	// ⚠️ POURQUOI PLUSIEURS TRAMES POUR UN SEUL GESTE. Trois mecanismes de NKGui
	//    imposent chacun un tour de retard, et ils s'additionnent :
	//      - le survol se resout sur `hotIdPrev`, donc la trame SUIVANTE ;
	//      - un popup de menu se MESURE une trame et s'applique a la suivante
	//        (`menuMeasure*` -> `MenuSizeSet`) ;
	//      - l'occultation lue est celle ecrite a la trame precedente.
	//    Un harnais a une seule trame ne verrait donc jamais un menu ouvert. Ce
	//    n'est pas un defaut : c'est le prix de la stabilite du z-ordre.
	const char *kTitre = "Fichier";
	const char *kSousMenu = "Backend graphique";
	nkgui::NkVec2 souris = {-100.f, -100.f};
	bool bouton = false;
	int32 trameOuvertureBarre = -1;
	int32 trameOuvertureSousMenu = -1;

	static constexpr int32 kTrames = 24;
	for (int32 t = 0; t < kTrames; ++t) {
		// L'entree se pose AVANT BeginFrame : c'est lui qui calcule les
		// transitions (clic/relache) a partir de l'etat brut.
		ctx.input.mousePos = souris;
		ctx.input.mouseDown[0] = bouton;
		ctx.BeginFrame(ec.dt);
		if (nkgui::BeginMenuBar(ctx, barre)) {
			DrawMenuBar(ec, nullptr);
			nkgui::EndMenuBar(ctx);
		}
		ctx.EndFrame();

		// ── Le pilotage, decide sur le releve QUI VIENT D'ETRE ECRIT ──────
		const nkgui::NkGuiNote *sousMenu =
			nkgui::NkGuiIntrospectTrouver(ctx, kSousMenu, nkgui::NkGuiNature::Menu);
		const bool sousMenuOuvert = sousMenu && (sousMenu->etats & nkgui::NK_GUI_ETAT_OUVERT);
		if (sousMenuOuvert && trameOuvertureSousMenu < 0)
			trameOuvertureSousMenu = t;
		if (sousMenuOuvert && t >= trameOuvertureSousMenu + 2)
			break; // deux tours de plus : les six entrees ont leur place definitive

		const nkgui::NkGuiNote *titre =
			nkgui::NkGuiIntrospectTrouver(ctx, kTitre, nkgui::NkGuiNature::Menu);
		const bool barreOuverte = titre && (titre->etats & nkgui::NK_GUI_ETAT_OUVERT);
		if (barreOuverte && trameOuvertureBarre < 0)
			trameOuvertureBarre = t;

		bouton = false;
		if (!barreOuverte) {
			// Viser le titre « Fichier » et PRESSER. `BeginMenu` ouvre au PRESS
			// (pas au relachement) : un clic complet ouvrirait puis refermerait.
			if (titre) {
				souris = {titre->rect.x + titre->rect.w * 0.5f, titre->rect.y + titre->rect.h * 0.5f};
				bouton = true;
			}
		} else if (sousMenu) {
			// Le menu est deroule : survoler la ligne du sous-menu. Un sous-menu
			// s'ouvre au SURVOL, sans clic — et le survol demande un tour.
			souris = {sousMenu->rect.x + sousMenu->rect.w * 0.5f,
					  sousMenu->rect.y + sousMenu->rect.h * 0.5f};
		}
	}

	// ── 4. Le verdict, PUIS le fichier ───────────────────────────────────
	const char *sortie = (chemin && *chemin) ? chemin : "nkuidesign_releve_menus.txt";
	// `toujours` : un banc ecrit son releve une fois, meme identique au
	// precedent. La garde « n'ecrire que si ca change » sert la boucle de
	// l'editeur, pas un tir unique.
	if (!nkgui::NkGuiIntrospectEcrire(ctx, sortie, /*toujours=*/true)) {
		logger.Error("[NKUIDesign/relevé] écriture impossible : {0}", sortie);
		return 3;
	}

	// ⚠️ LE HARNAIS SE JUGE LUI-MEME, ET SON CODE DE SORTIE LE DIT. Un releve
	//    ecrit n'est pas une preuve : le fichier existerait aussi si le
	//    sous-menu etait reste ferme. Le critere est nomme ici, en toutes
	//    lettres, et un banc peut s'y fier sans lire le fichier.
	const nkgui::NkGuiNote *sm = nkgui::NkGuiIntrospectTrouver(ctx, kSousMenu, nkgui::NkGuiNature::Menu);
	uint32 nApis = 0;
	const char *const *apis = nkuidesign::NkGfxApiNames(nApis);
	int32 trouvees = 0, cochees = 0;
	bool cocheJuste = true;
	for (uint32 i = 0; i < nApis; ++i) {
		const nkgui::NkGuiNote *e =
			nkgui::NkGuiIntrospectTrouver(ctx, apis[i], nkgui::NkGuiNature::EntreeMenu);
		if (!e)
			continue;
		++trouvees;
		const bool coche = (e->etats & nkgui::NK_GUI_ETAT_COCHE) != 0;
		const bool attendu =
			!gDesign.cfgChoice.Empty() && NkComponentDecl::StrEq(gDesign.cfgChoice.Data(), apis[i]);
		if (coche)
			++cochees;
		if (coche != attendu)
			cocheJuste = false;
	}

	int32 total = 0;
	nkgui::NkGuiIntrospectNotes(ctx, total);
	logger.Info("[NKUIDesign/relevé] {0} contrôle(s) relevé(s), écrit dans '{1}'.", total, sortie);
	logger.Info("[NKUIDesign/relevé] sous-menu '{0}' : {1} — {2}/{3} entrées, {4} cochée(s).", kSousMenu,
				(sm && (sm->etats & nkgui::NK_GUI_ETAT_OUVERT)) ? "OUVERT" : "FERME", trouvees, nApis,
				cochees);

	const bool ok = sm && (sm->etats & nkgui::NK_GUI_ETAT_OUVERT) && trouvees == (int32)nApis && cocheJuste;
	if (!ok) {
		// ⚠️ LA COCHE ATTENDUE PEUT ETRE ZERO, ET C'EST CORRECT. Quand
		//    `nkuidesign.cfg` ne porte pas de cle `gfx`, AUCUNE entree ne doit
		//    etre cochee : le menu marque ce que le FICHIER porte, pas ce qui
		//    tourne. Le harnais compare a cette regle, il n'exige pas une coche.
		logger.Error("[NKUIDesign/relevé] RECETTE NON PROUVÉE. Attendu : sous-menu ouvert, "
					 "{0} entrées, coche exactement sur '{1}'.",
					 nApis, gDesign.cfgChoice.Empty() ? "(aucune : clé gfx absente)" : gDesign.cfgChoice.Data());
		return 1;
	}
	logger.Info("[NKUIDesign/relevé] RECETTE PROUVÉE.");
	return 0;
}

int nkmain(const NkEntryState &state) {
	// ⚠️ `NkEntryState` porte `args` (un `NkVector<NkString>`), PAS `argc/argv` :
	//    le conteneur est le meme sur les huit plateformes, la ou `argv` n'existe
	//    ni sur UWP ni sur Android.
	// ⚠️ ET LE NOM `gState` ETAIT DEJA PRIS par `nkentseu::gState` (`NkEntry.h`) —
	//    d'ou `gDesign`. Le compilateur l'a dit tout de suite ; c'est le genre de
	//    collision qu'un `using namespace` large rend possible.
	uint32 width = 1440, height = 900;

	// Les arguments en tableau de pointeurs : `NkGfxResolve` est une fonction PURE
	// et ne connait pas les conteneurs de l'entree. C'est ce qui permet a `--probe`
	// d'appeler EXACTEMENT la meme resolution que le lancement reel.
	// ⚠️ La troncature au-dela de 32 arguments se DIT. Un tableau fixe qui laisse
	//    tomber les arguments en trop en silence, c'est la famille « ca repond
	//    toujours » : on croirait avoir passe --gfx et il aurait ete ignore.
	static const uint32 kMaxArgs = 32;
	const char *argv[kMaxArgs];
	const uint32 rawCount = (uint32)state.args.Size();
	uint32 argCount = 0;
	for (uint32 i = 0; i < rawCount && argCount < kMaxArgs; ++i)
		argv[argCount++] = state.args[i].Data();

	// LA TABLE DE TRADUCTION, AVANT TOUT MONTAGE (27/09).
	// ⚠️ AVANT LA BOUCLE D'ARGUMENTS, ET PAS APRES : les sondes montent des
	//    documents et sortent sans jamais atteindre la creation de la fenetre.
	//    Posee plus bas, la table leur aurait manque, et leurs libelles seraient
	//    retombes sur leur cle -- un releve qui accuse le document pour un simple
	//    ordre d'appel.
	nkuidesign::NkUIDesignPoserLangues();

	for (uint32 i = 0; i < argCount; ++i) {
		const char *a = argv[i];
		if (!a)
			continue;
		if (NkComponentDecl::StrEq(a, "--probe"))
			return nkuidesign::RunProbe();
		// LA COQUILLE VENUE D'UN DOCUMENT, jugee sans fenetre ni GPU. Elle recoit
		// LA table que l'application branche (`gActionsDocument`) : une copie ici
		// aurait donne deux tables incapables de se contredire.
		if (NkComponentDecl::StrEq(a, "--sonde-coquille"))
			return nkuidesign::SondeCoquille(gActionsDocument, gNbActionsDocument);
		// L'OUVERTURE d'un `.nkgui` dans le modele (chantier A, doc 5 §2.2). Sa
		// soeur juge le MONTAGE -- ce que le document AFFICHE ; celle-ci juge la
		// LECTURE -- ce que le document DEVIENT quand on veut l'editer.
		if (NkComponentDecl::StrEq(a, "--sonde-lecture"))
			return nkuidesign::SondeLecture();
		// LE TEMOIN DE R1 (doc 5 §2.3), sans souris et sans fenetre : une
		// propriete posee doit deplacer UNE ligne du fichier, pas une de plus.
		if (NkComponentDecl::StrEq(a, "--sonde-edition"))
			return nkuidesign::SondeEdition();
		// LE CROCHET D'IMAGE, juge sans GPU : la draw-list est du CPU, donc une
		// image emise s'y compte et son rectangle s'y lit.
		if (NkComponentDecl::StrEq(a, "--sonde-images"))
			return nkuidesign::SondeImages();
		// L'ACHEMINEMENT DES EVENEMENTS vers le C++ (27/09). Aucune injection sur
		// la machine : l'entree est ecrite en memoire, dans son propre contexte.
		if (NkComponentDecl::StrEq(a, "--sonde-ecouteurs"))
			return nkuidesign::SondeEcouteurs();
		// LE ROLE `Canvas` : son cadrage, ses gestes, sa grille. Sans GPU.
		if (NkComponentDecl::StrEq(a, "--sonde-toile"))
			return nkuidesign::SondeToile();
		// LE MULTILINGUE descendu de NKCode vers NKGui (27/09), NKUIDesign premier
		// utilisateur. Sans fenetre : la traduction est une table, pas un pixel.
		if (NkComponentDecl::StrEq(a, "--sonde-langues"))
			return nkuidesign::SondeLangues();
		// LE PONT entre la vue de la toile et celle du role `Canvas` : mesure
		// PREALABLE a l'integration, pour ne pas ouvrir `Panels.h` sur une
		// hypothese.
		if (NkComponentDecl::StrEq(a, "--sonde-pont"))
			return nkuidesign::SondePont();
		// DIAGNOSTIC : l'etat des langues TEL QUE `nkmain` l'a laisse, sans rien
		// reposer. Les sondes reposent leur propre table et masqueraient donc un
		// echec de la pose faite plus haut -- c'est exactement ce qui s'est
		// produit : sept sondes vertes, et l'application affichant ses cles.
		if (NkComponentDecl::StrEq(a, "--diag-langues")) {
			const nkgui::NkGuiLanguesRapport &r = nkgui::NkGuiLanguesReleve();
			printf("tables posees = %d, entrees = %d\n", r.tables, r.entrees);
			printf("design.enregistrer -> \"%s\"\n",
				   nkgui::NkGuiTexteLangue("design.enregistrer"));
			printf("design.ui.menuDesign -> \"%s\"\n",
				   nkgui::NkGuiTexteLangue("design.ui.menuDesign"));
			// 🔴 ET LE CHEMIN REEL : on monte le document comme la fenetre le fait,
			//    et on imprime le libelle que le MONTEUR obtient. Interroger la
			//    table ne prouve que la table ; c'est le montage qui dessine.
			{
				nkuidesign::NkCoquilleDocument coq;
				if (coq.ChargerDepuisDossier("Resources/Interface/NKUIDesign")) {
					const nkentseu::NkArchiveNode *corps = coq.bande.doc.FindNode(
						nkentseu::NkStringView(nkentseu::NkGuiArchive::KeyBody()));
					uint32 vus = 0;
					for (uint32 i = 0; corps && i < (uint32)corps->array.Size() && vus < 3u; ++i) {
						if (!corps->array[i].IsObject() || !corps->array[i].object)
							continue;
						const nkentseu::NkArchive &sec = *corps->array[i].object;
						const nkentseu::NkArchiveNode *cw = sec.FindNode(
							nkentseu::NkStringView(nkentseu::NkGuiArchive::KeyBody()));
						for (uint32 k = 0; cw && k < (uint32)cw->array.Size() && vus < 3u; ++k) {
							if (!cw->array[k].IsObject() || !cw->array[k].object)
								continue;
							const nkentseu::NkArchive &b = *cw->array[k].object;
							const nkentseu::NkString bid(nkentseu::NkGuiArchive::IdOf(b));
							printf("  montage : \"%s\" -> libelle \"%s\"\n", bid.CStr(),
								   nkgui::NkGLibelle(b, bid).CStr());
							++vus;
						}
					}
				} else {
					printf("  montage : document NON LU\n");
				}
			}
			return 0;
		}
		// La preuve de recette du pipeline IA (Q31 [IA], branchement n.1) : sans
		// fenetre ni GPU, comme la sonde -- elle tourne sur la machine
		// d'integration.
		if (NkComponentDecl::StrEq(a, "--recette-ia"))
			return nkuidesign::RunRecetteIA();
		// LA CHAINE COMPLETE, sans fenetre : taper -> envoyer -> poser -> annuler.
		// Elle emprunte le MEME chemin que le bouton du panneau.
		{
			const NkString arg2(a);
			if (arg2.StartsWith("--ia-bout-en-bout")) {
				const char *d = a + 17;
				if (*d == '=')
					++d;
				else
					d = "Un ecran de connexion : un titre, deux champs avec libelles, un bouton.";
				return nkuidesign::RunIABoutEnBout(d);
			}
		}
		// Levier de MISE EN SCENE (captures, bancs) — pas un reglage :
		// --selection=N selectionne le noeud N au premier affichage. Meme
		// patron que --theme= : l'option force, l'interface decide ensuite.
		{
			const NkString arg(a);
			if (arg.StartsWith("--selection=")) {
				int32 v = 0;
				for (const char *q = a + 12; *q >= '0' && *q <= '9'; ++q)
					v = v * 10 + (*q - '0');
				gDesign.selectionInitiale = v;
				continue;
			}
			// Mise en scene (correction 5, 31/08) : ouvrir l'edition en place
			// sur le noeud N au premier affichage — l'etat que le double-clic
			// pose, atteignable sans souris (le harnais --clic ne sait pas
			// produire un double-clic).
			if (arg.StartsWith("--editer-texte=")) {
				int32 v = 0;
				for (const char *q = a + 15; *q >= '0' && *q <= '9'; ++q)
					v = v * 10 + (*q - '0');
				gDesign.editTexteInitial = v;
				continue;
			}
			// Mise en scene (01/09) : ouvrir le MODE EDITION DE FORME sur le
			// noeud N au premier affichage — l'etat que le double-clic pose sur
			// une forme a sommets. Meme raison que --editer-texte= : le harnais
			// --clic ne sait pas produire un double-clic sur une coordonnee de
			// toile qu'on ne connait pas d'avance, et une capture qui vise a
			// cote ne prouve rien -- elle rend une image de plus a interpreter.
			if (arg.StartsWith("--mode-forme=")) {
				int32 v = 0;
				for (const char *q = a + 13; *q >= '0' && *q <= '9'; ++q)
					v = v * 10 + (*q - '0');
				gDesign.modeFormeInitial = v;
				continue;
			}
			// Mise en scene : quels SOMMETS sont marques au premier affichage
			// (--sommets=0,2,3). Meme famille que --mode-forme=, meme raison :
			// le Maj+clic vise une ancre dont on ignore la coordonnee d'ecran.
			if (NkComponentDecl::StrEq(a, "--courber")) {
				gDesign.courberInitial = true;
				continue;
			}
			if (arg.StartsWith("--sommets=")) {
				nkentseu::uint64 m = 0;
				int32 v = -1;
				for (const char *q = a + 10;; ++q) {
					if (*q >= '0' && *q <= '9')
						v = (v < 0 ? 0 : v) * 10 + (*q - '0');
					else {
						if (v >= 0 && v < 64)
							m |= (1ull << (nkentseu::uint32)v);
						v = -1;
						if (!*q)
							break;
					}
				}
				gDesign.sommetsInitiaux = m;
				continue;
			}
			// MISE EN SCENE (remandat Banani : un document par ecran) : charger
			// un document donne au lancement. ⚠️ Ctrl+S ecrira LA ou on a
			// charge — le fichier de travail par defaut ne bouge pas.
			if (arg.StartsWith("--document=")) {
				static NkString cheminDoc; // survit a l'analyse (le chargement vient apres)
				cheminDoc = arg.SubStr(11);
				if (!cheminDoc.Empty())
					nkuidesign::kDocumentPath = cheminDoc.Data();
				continue;
			}
			// TOILE SEULE (mise en scene des ecrans « gros plan » : la
			// maquette ne montre que la toile) : panneaux fermes, rails
			// retires — l'en-tete de la coquille reste, la paire se cadre sur
			// la toile et le DIT.
			// --annuler=N / --retablir=N : N pas d'annulation/retablissement au
			// lancement (apres les gestes injectes --clic) — le levier de preuve
			// UI de l'annulation ; la batterie complete est --recette-annulation.
			// --langue=xx : la langue active d'apercu posee au lancement (mise
			// en scene du multilingue — la bascule reelle passe par le menu
			// Cible > Langue du document).
			if (arg.StartsWith("--langue=")) {
				gDesign.langueActive = arg.SubStr(9);
				continue;
			}
			// --frappe=texte:frame — les codepoints ASCII poses dans l'input a
			// cette trame (preuve des saisies en place ; ':' separe, donc
			// interdit dans le texte).
			if (arg.StartsWith("--frappe=")) {
				for (int32 fi = 0; fi < 2; ++fi) {
					if (gFrappes[fi].frame >= 0)
						continue;
					const char *q = a + 9;
					nkentseu::usize k = 0;
					while (*q && *q != ':' && k + 1 < sizeof(gFrappes[fi].texte))
						gFrappes[fi].texte[k++] = *q++;
					gFrappes[fi].texte[k] = 0;
					gFrappes[fi].frame = (*q == ':') ? (int32)atof(q + 1) : 90;
					break;
				}
				continue;
			}
			// --touche=entree|echap|retour:frame — un one-shot clavier injecte.
			if (arg.StartsWith("--touche=")) {
				for (int32 ti = 0; ti < 4; ++ti) {
					if (gTouches[ti].frame >= 0)
						continue;
					const char *q = a + 9;
					if (NkString(q).StartsWith("entree"))
						gTouches[ti].touche = nkgui::NkGuiKey::Enter;
					else if (NkString(q).StartsWith("echap"))
						gTouches[ti].touche = nkgui::NkGuiKey::Escape;
					else if (NkString(q).StartsWith("retour"))
						gTouches[ti].touche = nkgui::NkGuiKey::Backspace;
					while (*q && *q != ':')
						++q;
					gTouches[ti].frame = (*q == ':') ? (int32)atof(q + 1) : 100;
					break;
				}
				continue;
			}
			if (arg.StartsWith("--annuler=")) {
				gDesign.annulerInitial = (int32)atof(a + 10);
				continue;
			}
			if (arg.StartsWith("--retablir=")) {
				gDesign.retablirInitial = (int32)atof(a + 11);
				continue;
			}
			if (arg.StartsWith("--clic=")) {
				// ⚠️ LA BORNE SE LIT DU TABLEAU, PAS D'UN LITTERAL — et elle a
				//    deja menti : le tableau est passe a 10 le 01/09, ce
				//    remplisseur etait reste a 4, et les clics 5 a 10 etaient
				//    SILENCIEUSEMENT ignores. Le meme defaut que la borne de
				//    `gTouches`, dans l'autre sens : la ou l'un depassait, celui-ci
				//    tronquait. C'est le second cas en deux jours : une borne
				//    ecrite en chiffre ne suit pas ce qu'elle borne.
				for (int32 ci = 0; ci < (int32)(sizeof(gClics) / sizeof(gClics[0])); ++ci)
					if (gClics[ci].frame < 0) {
						const char *q = a + 7;
						gClics[ci].x = (float32)atof(q);
						while (*q && *q != ':')
							++q;
						if (*q == ':')
							gClics[ci].y = (float32)atof(++q);
						while (*q && *q != ':')
							++q;
						gClics[ci].frame = (*q == ':') ? (int32)atof(++q) : 30;
						// 4e champ optionnel : « d » = DOUBLE-clic (preuve du
						// forage) ; « r » = clic DROIT (menu contextuel) ;
						// « o » = double-clic TEL QUE L'OS L'ENVOIE (sans appui).
						while (*q && *q != ':')
							++q;
						gClics[ci].dbl = (*q == ':' && q[1] == 'd');
						gClics[ci].droit = (*q == ':' && q[1] == 'r');
						gClics[ci].ctrl = (*q == ':' && q[1] == 'c');
						gClics[ci].maj = (*q == ':' && q[1] == 's');
						gClics[ci].dblOS = (*q == ':' && q[1] == 'o');
						break;
					}
				continue;
			}
			// --glisser=x1:y1:x2:y2:frame[:duree] — un drag injecte (poignee,
			// pouce d'ascenseur, deplacement d'une page par son etiquette).
			if (arg.StartsWith("--glisser=")) {
				for (int32 gi = 0; gi < 2; ++gi)
					if (gGlissers[gi].frame < 0) {
						const char *q = a + 10;
						float32 v[4] = {0.f, 0.f, 0.f, 0.f};
						int32 nv = 0;
						v[nv++] = (float32)atof(q);
						while (nv < 4) {
							while (*q && *q != ':')
								++q;
							if (*q != ':')
								break;
							v[nv++] = (float32)atof(++q);
						}
						gGlissers[gi].x1 = v[0];
						gGlissers[gi].y1 = v[1];
						gGlissers[gi].x2 = v[2];
						gGlissers[gi].y2 = v[3];
						while (*q && *q != ':')
							++q;
						gGlissers[gi].frame = (*q == ':') ? (int32)atof(++q) : 30;
						while (*q && *q != ':')
							++q;
						if (*q == ':')
							gGlissers[gi].duree = (int32)atof(++q);
						// suffixe `:t` -- la souris reste ENFONCEE a l'arrivee
						while (*q && *q != ':')
							++q;
						if (*q == ':' && (q[1] == 't' || q[1] == 'T'))
							gGlissers[gi].tenir = true;
						break;
					}
				continue;
			}
			// --molette=x:y:delta:frame — un cran de molette a cette position.
			if (arg.StartsWith("--molette=")) {
				for (int32 mi = 0; mi < 2; ++mi)
					if (gMolettes[mi].frame < 0) {
						const char *q = a + 10;
						gMolettes[mi].x = (float32)atof(q);
						while (*q && *q != ':')
							++q;
						if (*q == ':')
							gMolettes[mi].y = (float32)atof(++q);
						while (*q && *q != ':')
							++q;
						if (*q == ':')
							gMolettes[mi].delta = (float32)atof(++q);
						while (*q && *q != ':')
							++q;
						gMolettes[mi].frame = (*q == ':') ? (int32)atof(++q) : 30;
						break;
					}
				continue;
			}
			if (NkComponentDecl::StrEq(a, "--proposer")) {
				gDesign.proposerInitial = true;
				continue;
			}
			if (arg.StartsWith("--mode=")) {
				gDesign.modeInitial = (int32)atof(a + 7);
				continue;
			}
			if (NkComponentDecl::StrEq(a, "--zone-sure")) {
				gDesign.zoneSure = true;
				continue;
			}
			// --aimant=0|1 : poser le magnétisme au lancement. ⚠️ CE LEVIER
			// EXISTE POUR LE CONTRÔLE NÉGATIF, et c'est sa seule raison : « le
			// même glisser, aimant éteint, ne colle plus » ne se mesure pas si
			// l'état ne s'atteint qu'en cliquant un bouton dont il faut d'abord
			// deviner les coordonnées. Un cas vert n'apprend rien sans son cas
			// rouge tiré du MÊME banc.
			if (arg.StartsWith("--aimant=")) {
				gDesign.aimantActif = (atof(a + 9) != 0.0);
				continue;
			}
			if (arg.StartsWith("--capture-frame=")) {
				const int32 v = (int32)atof(a + 16);
				if (v > 0)
					kCaptureFramePrete = v;
				continue;
			}
			// (R16) --sonde-portes=preferences,menu-ctx,export,... : voir PortesTick.
			if (arg.StartsWith("--sauver-disposition=")) {
				gDispoImage = (int32)atof(a + 21);
				const char *q = a + 21;
				while (*q && *q != ':')
					++q;
				snprintf(gDispoChemin, sizeof(gDispoChemin), "%s", *q == ':' ? q + 1 : "disposition.cfg");
				continue;
			}
			if (arg.StartsWith("--sauver-document=")) {
				gSauverImage = (int32)atof(a + 18);
				const char *q = a + 18;
				while (*q && *q != ':')
					++q;
				snprintf(gSauverChemin, sizeof(gSauverChemin), "%s", *q == ':' ? q + 1 : "depot_relu.nkuidoc");
				continue;
			}
			if (arg.StartsWith("--sonde-gel=")) {
				snprintf(gSondeGel, sizeof(gSondeGel), "%s", a + 12); // « --sonde-gel= » fait 12 caracteres
				continue;
			}
			if (arg.StartsWith("--sonde-portes=")) {
				snprintf(gSondePortes, sizeof(gSondePortes), "%s", a + 15);
				continue;
			}
			if (arg.StartsWith("--titre-sonde")) {
				gTitreSonde = true;
				continue;
			}
			if (arg.StartsWith("--mesure-async=")) {
				const char *q = a + 15;
				gMesureAsyncMs = (nkentseu::int64)atof(q);
				while (*q && *q != ':')
					++q;
				gMesureAsyncSync = (*q == ':' && q[1] == 's');
				// `:prop` emprunte le bouton « Proposer (apercu) » lui-meme.
				gMesureAsyncProp = (*q == ':' && q[1] == 'p');
				continue;
			}
			if (arg.StartsWith("--mesure-fps=")) {
				gMesureFpsMs = (nkentseu::int64)atof(a + 13);
				continue;
			}
			if (arg.StartsWith("--mesure-double=")) {
				gMesureDoubleImages = (nkentseu::int64)atof(a + 16);
				continue;
			}
			if (arg.StartsWith("--mesure-texte=")) {
				const char *q = a + 15;
				gMesureTexteImages = (nkentseu::int64)atof(q);
				while (*q && *q != ':')
					++q;
				if (*q == ':')
					gMesureTexteLargeur = (float32)atof(q + 1);
				continue;
			}
			if (arg.StartsWith("--toile-seule")) {
				gToileSeule = true;
				continue;
			}
			// L'ONGLET D'INSPECTEUR au lancement (mise en scene, ecrans 4-6) :
			// --inspecteur-onglet=2 ouvre Behavior.
			// Ouvrir un TIROIR de rail au lancement (mise en scene, ecran 9) :
			// --tiroir=droit:0 (cote:index).
			if (arg.StartsWith("--panneau=")) {
				snprintf(gPanneauInitial, sizeof(gPanneauInitial), "%s", a + 10);
				continue;
			}
			if (NkComponentDecl::StrEq(a, "--rapport-transposition")) {
				gDesign.rapportTransposition = true;
				continue;
			}
			// ⚠️ `--tiroir=ia` EMPRUNTE LA PORTE DU MENU, PAS UN INDICE. C'est
			//    le crochet du meme chemin que le clic : il appelle
			//    `OuvrirTiroirIA()`, donc il eprouve la RESOLUTION PAR LE NOM.
			//    `--tiroir=d:1` reste, mais il court-circuite cette resolution --
			//    *un banc qui passe a cote du chemin repare ne prouve pas la
			//    reparation.*
			if (NkComponentDecl::StrEq(a, "--tiroir=ia")) {
				gTiroirParLeNom = true;
				continue;
			}
			if (arg.StartsWith("--tiroir=")) {
				gTiroirCote = a[9];
				gTiroirIndex = (int32)atof(a + 11);
				continue;
			}
			if (NkComponentDecl::StrEq(a, "--filtre-hierarchie")) {
				gDesign.filtreHierarchieInitial = true;
				continue;
			}
			if (NkComponentDecl::StrEq(a, "--menu-role")) {
				gDesign.menuRoleInitial = true;
				continue;
			}
			if (arg.StartsWith("--inspecteur-onglet=")) {
				gDesign.ongletInitial = (int32)atof(a + 20);
				continue;
			}
			// LA VUE POSEE : --vue=x<px>,y<px>[,z<zoom>] — pan (et zoom) au
			// lancement, pour MESURER l'effet d'un deplacement de vue par
			// paires de captures (protocole du bogue « effet bizarre au
			// deplacement », 31/08). Meme famille que --selection=.
			if (arg.StartsWith("--vue=")) {
				for (const char *q = a + 6; *q;) {
					if (*q == 'x')
						gDesign.vueX = (float32)atof(q + 1);
					else if (*q == 'y')
						gDesign.vueY = (float32)atof(q + 1);
					else if (*q == 'z')
						gDesign.vueZ = (float32)atof(q + 1);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
				}
				gDesign.vuePosee = true;
				continue;
			}
			// Lignes de magnetisme FIGEES : --lignes=v0.44,h460 (v = fraction
			// de la toile, h = pixels depuis son haut ; chacune optionnelle).
			// Meme famille que --selection= : un levier de capture, pas un
			// reglage.
			if (arg.StartsWith("--lignes=")) {
				for (const char *q = a + 9; *q;) {
					if (*q == 'v')
						gDesign.ligneV = (float32)atof(q + 1);
					else if (*q == 'h')
						gDesign.ligneH = (float32)atof(q + 1);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
				}
				continue;
			}
		}
		// ⚠️ AVANT TOUTE FENETRE, pour la meme raison que la sonde : l'aller-retour
		//    ne touche ni au GPU ni a l'ecran, et il doit pouvoir tourner sur la
		//    machine d'integration qui n'en a pas. C'est aussi ce qui le rend
		//    utilisable comme controle de non-regression a chaque changement du
		//    format.
		// ⚠️ LES CONTROLES SE TESTENT AVANT L'ALLER-RETOUR, et l'ordre n'est pas
		//    esthetique : `--roundtrip-controles` commence par `--roundtrip`, donc
		//    le tester apres le ferait avaler par la comparaison prefixee.
		if (NkComponentDecl::StrEq(a, "--roundtrip-controles"))
			return nkuidesign::guifmt::NkGRunControls();
		// La batterie de preuve de l'annulation (§7) — sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-annulation"))
			return RecetteAnnulation();
		// Le contrat universel d'edition, prouve PAR SITE (Hierarchie,
		// etiquette d'artboard, texte de toile) — sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-edition"))
			return nkuidesign::RecetteEdition();
		// Les LISTES DE PROPRIETES (remplissages, bordures, effets) exercees par
		// le GESTE : une vraie souris qui vise la poubelle, sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-proprietes"))
			return nkuidesign::NkRecetteProprietes();
		// `--recette-ecrivain` : NKUIDesign ECRIT un `.nkgui`, le monteur le remonte,
		// et les deux releves de rectangles se comparent PAR IDENTIFIANT -- sans
		// fenetre ni GPU (les deux rendus passent par le rasteriseur logiciel).
		if (NkComponentDecl::StrEq(a, "--recette-ecrivain"))
			return nkuidesign::ecrivain::RecetteEcrivain();
		// `--recette-placement` : le PLACEMENT PAR WIDGET (`pos` est l'interrupteur),
		// les `Window` imbriques, la section `geometry` et le voile d'une modale.
		// Sans fenetre ni GPU, comme la recette ecrivain.
		if (NkComponentDecl::StrEq(a, "--recette-placement"))
			return nkuidesign::placement::RecettePlacement();
		// Le TEMOIN DE RENDU : le flux de commandes du peintre, ecrit tel quel.
		// Il se DIFFE -- une refonte d apparence se juge sur ce qui bouge.
		if (NkComponentDecl::StrEq(a, "--temoin-rendu"))
			return nkuidesign::NkTemoinRendu(nullptr);
		{
			const NkString argT(a);
			if (argT.StartsWith("--temoin-rendu="))
				return nkuidesign::NkTemoinRendu(argT.SubStr(15).Data());
			// --flux=<doc>[,<sortie>] : le flux de la toile d'UN document charge --
			// l'avant / apres entre deux binaires, sans fenetre ni GPU (11/09).
			if (argT.StartsWith("--flux="))
				return nkuidesign::NkFluxDocument(argT.SubStr(7).Data());
			// --capture=<fichier.png> : PAS un retour immédiat, contrairement à
			// toutes les recettes — ce mode a BESOIN de la fenêtre et du GPU.
			// On note le chemin ; la boucle normale démarre, CaptureTick arme le
			// readback à la frame 8, la coquille se ferme, et le verdict se lit
			// sur le FICHIER après Run() — l'effet, pas l'intention.
			if (NkComponentDecl::StrEq(a, "--scene-fusion")) {
				gSceneFusion = true; // avec --capture : le temoin en pixels des modes de fusion
				continue;
			}
			if (argT.StartsWith("--capture=")) {
				const NkString chemin = argT.SubStr(10);
				uint32 i = 0;
				for (; chemin.Data()[i] && i + 1 < sizeof(gCapturePath); ++i)
					gCapturePath[i] = chemin.Data()[i];
				gCapturePath[i] = '\0';
				continue;
			}
			// --selectionner=<libelle> : met l'inspecteur dans l'etat PLEIN
			// avant la photo (E6 : une capture d'un etat vide temoigne du vide).
			if (argT.StartsWith("--selectionner=")) {
				const NkString lib = argT.SubStr(15);
				uint32 i = 0;
				for (; lib.Data()[i] && i + 1 < sizeof(gSelectionner); ++i)
					gSelectionner[i] = lib.Data()[i];
				gSelectionner[i] = '\0';
				continue;
			}
		}
		// Les gestes d'edition Lunacy (copier/coller/dupliquer/grouper/...)
		// prouves par leur EFFET, et « un geste = un pas » — sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-gestes"))
			return RecetteGestes();
		// L'aimantation Lunacy (bords, centres, page, espacements egaux) prouvee
		// par ses nombres -- sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-snap"))
			return RecetteSnap();
		// Le contrat de selection Lunacy (tables, racine, englobant, mixtes,
		// rectangle, conservation) -- sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-selection"))
			return RecetteSelection();
		// Le mode points (table du double-clic, sommets, aller-retour) -- sans
		// fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-points"))
			return RecettePoints();
		// La rotation et les deux miroirs : propagation, picking, englobant,
		// poignees, conservation -- sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-transfo"))
			return RecetteTransfo();
		// Le double-clic mesure sur le document REEL lu sur le disque, pas sur des
		// noeuds fabriques par le banc. Sans fenetre ni GPU.
		if (NkComponentDecl::StrEq(a, "--recette-document"))
			return RecetteDocument();
		// Meme raison que ci-dessus : le pool de chaines du document ne touche ni
		// au GPU ni a l ecran. Il porte les noms de metrique que le kit declare
		// en const char* et que personne ne possedait a la relecture.
		if (NkComponentDecl::StrEq(a, "--pool-controles"))
			return ::nkuidesign::poolctl::RunPoolControls();
		if (NkComponentDecl::StrEq(a, "--roundtrip"))
			return nkuidesign::guifmt::NkGRunRoundTrip(".");
		// ⚠️ LIRE ET JUGER SONT DEUX GESTES, ET DEUX MODES. `--valider` verifie
		//    les roles et les types contre le vocabulaire du document 7 ; il ne
		//    touche pas au modele, donc un document fautif reste lisible,
		//    modifiable et enregistrable. Un outil qui refuserait d'ouvrir ce
		//    qu'il signale serait celui qui empeche de le reparer.
		if (NkComponentDecl::StrEq(a, "--recette-identite"))
			return RecetteIdentite();
		if (NkComponentDecl::StrEq(a, "--valider"))
			return nkuidesign::guifmt::NkGRunValidate(".");
		{
			const NkString arg(a);
			if (arg.StartsWith("--roundtrip=")) {
				return nkuidesign::guifmt::NkGRunRoundTrip(arg.SubStr(12).Data());
			}
			if (arg.StartsWith("--valider=")) {
				return nkuidesign::guifmt::NkGRunValidate(arg.SubStr(10).Data());
			}
		}
		// Fenetre reduite : sert aux essais quand la carte est occupee ailleurs.
		// 1024x640 est le PLANCHER de la coquille (`NkEditorShell::Init` impose
		// minWidth 1024 / minHeight 640) — demander moins ne donnerait pas moins,
		// ca donnerait la meme fenetre avec un chiffre faux dans le journal.
		// ⚠️ `--dump-ui` : l'interface PUBLIE les rectangles qu'elle a dessines.
		//    C'est l'equivalent, pour les panneaux, de ce que la famille 34 fait
		//    pour les composants — lire ce qui a ete EMIS, jamais des pixels. Un
		//    essai a la souris vise alors un rectangle publie, et cesse de dependre
		//    de la hauteur du texte au-dessus. Sans ce drapeau, le registre
		//    n'ecrit rien.
		// ⚠️ LE `continue` N'EST PAS DECORATIF, ET SON ABSENCE A COUTE QUATRE
		//    JOURS DE SILENCE. Ces deux drapeaux posaient leur variable puis
		//    TOMBAIENT dans le refus ci-dessous : le programme repondait
		//    « drapeau inconnu : --dump-ui » **en imprimant `--dump-ui` dans la
		//    liste des drapeaux reconnus, trois lignes plus bas**.
		//
		//    C'est la meme famille que la parade `grep` de Q4, qui contenait
		//    elle-meme le motif qu'elle faisait compter : **un garde-fou qui
		//    refuse une entree valide et se contredit dans la meme sortie.**
		//    Le cout reel n'est pas la gene : `--dump-ui` est le SEUL moyen
		//    d'observer ce que l'interface dessine, donc la seule voie d'essai
		//    automatisable de l'UI est restee fermee sans que rien ne le dise.
		//
		//    Mesure du 2026-08-27 : `--small` et `--dump-ui` rendaient tous deux
		//    le code de sortie **2**.
		if (NkComponentDecl::StrEq(a, "--dump-ui")) {
			// ⚠️ LE DRAPEAU NE PEUT PLUS ALLUMER L'INSTRUMENT ICI, et il faut le
			//    dire : le releve vit dans le `NkGuiContext`, qui n'existe pas
			//    encore a l'analyse des arguments. On memorise l'intention, et
			//    l'activation se fait a la creation de la coquille. Une variable
			//    de plus, mais aucune ambiguite : l'ancien `UiRects::Enabled()`
			//    etait un booleen global precisement parce qu'il n'avait pas de
			//    contexte ou vivre — c'etait le symptome du mauvais etage.
			gReleveDemande = true;
			continue;
		}
		// ⚠️ `--releve-menus` REND UN VERDICT, IL NE SE CONTENTE PAS D'ECRIRE.
		//    Il sort AVANT toute creation de fenetre — meme raison que `--probe`
		//    plus haut : un `Init()` place avant lui rendrait ce mode inutilisable
		//    exactement sur la machine ou l'on en a besoin (un agent, une session
		//    sans ecran, une carte deja prise par autre chose).
		if (NkComponentDecl::StrEq(a, "--releve-menus"))
			return ReleveMenus(nullptr);
		{
			const NkString arg(a);
			if (arg.StartsWith("--releve-menus="))
				return ReleveMenus(arg.SubStr(15).Data());
		}
		if (NkComponentDecl::StrEq(a, "--small")) {
			width = 1024;
			height = 640;
			continue;
		}
		// ⚠️ `--theme=` EXISTE POUR QUE LA BASCULE SOIT PROUVABLE. Un thème ne
		//    se change qu'à la souris, dans un menu — donc sa preuve dépend
		//    d'un clic, c'est-à-dire de l'instrument le plus fragile de ce
		//    chantier. Avec ce drapeau, deux lancements donnent deux captures
		//    comparables, et « toute la fenêtre a-t-elle suivi ? » devient une
		//    question qu'on tranche sur des images, pas sur une intuition.
		//    Même patron que `--gfx=` : l'option force, l'interface décide.
		{
			const NkString arg(a);
			if (arg.StartsWith("--theme=")) {
				gThemeDemande = arg.SubStr(8);
				continue;
			}
		}
		// ⚠️ UN DRAPEAU INCONNU EST REFUSE, IL NE TOMBE PAS DANS LE CHEMIN PAR
		//    DEFAUT. Mesure du 2026-08-23, et elle m'a coute dix minutes : j'ai
		//    tape `--validate=` au lieu de `--valider=`. Le programme n'a pas
		//    bouclé -- **il attendait**, parce qu'un argument non reconnu laissait
		//    passer jusqu'a l'ouverture de la fenetre de l'editeur. J'ai cherche
		//    une boucle infinie dans du code que je venais d'ecrire.
		//
		//    Une faute de frappe doit couter une ligne de message, pas une
		//    seance de diagnostic. Tout ce qui commence par `--` et que personne
		//    n'a reconnu plus haut est donc une erreur nommee, avec la liste de
		//    ce qui existe.
		if (a[0] == '-' && a[1] == '-') {
			fputs("drapeau inconnu : ", stdout);
			puts(a);
			puts("drapeaux reconnus :");
			puts("  --probe                 la sonde headless");
			puts("  --recette-annulation    la batterie de preuve de l'annulation (§7)");
			puts("  --recette-edition       le contrat universel d'edition, par site");
			puts("  --recette-proprietes    les listes de proprietes exercees par le GESTE");
			puts("  --temoin-rendu[=<f>]    le flux de commandes du peintre (diffable)");
			puts("  --capture=<f.png>       ouvre l'app, photographie SA fenetre (frame 8), ferme");
		puts("  --titre-sonde           le bandeau de titre dit que CETTE FENETRE N'EST PAS");
		puts("                          LE PRODUIT -- a poser sur TOUTE fenetre ouverte par");
		puts("                          un agent de mesure");
			puts("  --selectionner=<nom>    selectionne ce noeud avant la photo (inspecteur PLEIN)");
			puts("  --recette-gestes        les gestes d'édition Lunacy (copier/grouper/...)");
			puts("  --recette-snap          l'aimantation (bords, centres, espacements égaux)");
			puts("  --recette-selection     le contrat de sélection (Ctrl/Maj, englobant, mixtes)");
			puts("  --recette-points        le mode points (table du double-clic, sommets)");
			puts("  --recette-transfo       rotation et miroirs (picking, propagation, poignées)");
			puts("  --recette-document      le double-clic mesuré sur le document RÉEL du disque");
			puts("  --annuler=N             N pas d'annulation au lancement (preuve UI)");
			puts("  --retablir=N            N pas de retablissement apres --annuler");
			puts("  --recette-ia            la preuve de recette du pipeline IA");
			puts("  --roundtrip[=<dossier>] l'aller-retour du format .nkgui");
			puts("  --roundtrip-controles   les temoins du lecteur/ecrivain");
			puts("  --pool-controles        les témoins du pool de chaînes");
			puts("  --valider[=<dossier>]   la validation par role et par type");
			puts("  --recette-placement     le placement par widget, geometry, Window imbriques");
			puts("  --dump-ui               publier le relevé de l'interface dessinée");
			puts("  --releve-menus[=<fichier>] relever la barre de menus SANS fenêtre");
			puts("  --small                 fenêtre réduite (1024x640)");
			puts("  --theme=<nom>           thème au lancement (nom de NkThemeLibrary)");
			puts("  --selection=<n>         sélectionner le nœud n au premier affichage");
			puts("  --editer-texte=<n>      ouvrir l'édition en place sur le nœud texte n (mise en scène)");
			puts("  --mode-forme=<n>        ouvrir l'édition de forme sur le nœud n (mise en scène)");
			puts("  --sommets=<a,b,c>       marquer ces sommets en édition de forme (mise en scène)");
			puts("  --clic=x:y:frame[:d|r|c|s|o]  injecter un clic (d double, r droit, c Ctrl, "
				 "s Maj, o double-clic OS SANS appui)");
			puts("  --frappe=texte:frame    injecter des codepoints ASCII à cette trame (preuve de saisie)");
			puts("  --touche=nom:frame      injecter entree|echap|retour à cette trame");
			puts("  --glisser=x1:y1:x2:y2:frame[:duree[:t]]  injecter un drag (`t` = ne pas relâcher)");
			puts("  --molette=x:y:delta:frame  injecter un cran de molette à cette position");
			puts("  --document=<chemin>     charger ce document au lancement (mise en scène)");
			puts("  --lignes=v<f>,h<px>     lignes de magnétisme figées (mise en scène)");
			puts("  --toile-seule           panneaux fermés, rails retirés (mise en scène)");
			puts("  --vue=x<px>,y<px>,z<f>  poser pan/zoom de la vue au lancement (mesure)");
			puts("  --tiroir=<c>:<n>        ouvrir un tiroir de rail (d/g/b, mise en scène)");
			puts("  --zone-sure             afficher la zone sûre des cadres Mobile");
			puts("  --aimant=0|1            poser le magnétisme au lancement (contrôle négatif)");
			puts("  --inspecteur-onglet=<n> ouvrir cet onglet d'Inspecteur (mise en scène)");
			return 2;
		}
	}

	// ── Le choix, journalise AVANT toute tentative ──────────────────────
	// ⚠️ `nkentseu::env::GetEnvVar`, PAS `std::getenv` (Rodolf, 18/08 : « ce n'est
	//    pas une exception, il faut corriger ca »). L'equivalent maison est
	//    header-only et multiplateforme.
	//    📌 ET IL FAUT DIRE CE QUE LA SUBSTITUTION NE FAIT PAS : `GetEnvVar`
	//       ENVELOPPE `std::getenv` (`NKPlatform/NkEnv.h:672`). L'occurrence
	//       quitte NKUIDesign, elle ne quitte pas le depot. Le comptage du 18/08
	//       (62 occurrences) portait sur NKEditorKit, NKGui et NKUIDesign :
	//       **NKPlatform n'etait pas dans le perimetre**, donc ce 63e n'y figure
	//       pas. Porte au canal ; ce n'est pas mon fichier.
	// ⚠️ LE FICHIER EST LU ICI, ET SA VALEUR EST **PASSEE** A LA RESOLUTION —
	//    elle n'y entre jamais par un acces au disque cache au milieu du calcul.
	//    C'est ce qui laisse `NkGfxResolve` PURE, donc appelable a l'identique par
	//    `--probe`. La regle du 18/08 dit que la sonde doit lire la meme
	//    configuration que l'application : ici elles appellent la meme fonction,
	//    et la sonde peut lui donner n'importe quel contenu de fichier sans
	//    toucher au disque. Une resolution qui lirait elle-meme serait
	//    intestable, et c'est exactement comme ca qu'on obtient deux verites.
	const NkString cfgText = nkentseu::NkFile::Exists(nkuidesign::NkGfxConfigPath())
								 ? nkentseu::NkFile::ReadAllText(nkuidesign::NkGfxConfigPath())
								 : NkString("");
	char cfgGfx[32] = {0};
	const nkuidesign::NkGfxConfigState cfgState = nkuidesign::NkGfxConfigClassify(
		nkentseu::NkFile::Exists(nkuidesign::NkGfxConfigPath()), cfgText.Data(), cfgGfx,
		sizeof(cfgGfx));
	// ⚠️ UN FICHIER PRESENT ET INEXPLOITABLE SE DIT. Sans cette ligne, il se
	//    comportait exactement comme un fichier absent — un repli MUET, et
	//    l'utilisateur cherchait pourquoi son reglage ne prenait pas.
	if (const char *m = nkuidesign::NkGfxConfigStateMessage(cfgState); m && *m)
		logger.Warn("{0}", m);

	const nkuidesign::NkGfxChoice gfx = nkuidesign::NkGfxResolve(
		cfgGfx[0] ? cfgGfx : nullptr, nkentseu::env::GetEnvVar("NK_GFX_API"), argv, argCount);

	if (rawCount > kMaxArgs)
		logger.Warnf("[NKUIDesign] %u arguments reçus, seuls les %u premiers ont été lus.", rawCount,
					 kMaxArgs);

	// ⚠️ `Info` (accolades INDEXEES `{0}`), PAS `Infof` (famille printf `%s`). La
	//    premiere version melangeait les deux familles et le journal imprimait
	//    « {} » a la place du backend — cf. l'en-tete de `Backend.h`.
	logger.Info("{0}", nkuidesign::NkGfxJournalLine(gfx).Data());

	if (!gfx.supported) {
		// Regle 3 : un backend indisponible se DIT, il ne se remplace pas. La raison
		// est deja dans la ligne ci-dessus ; celle-ci ne porte que la conduite a tenir.
		logger.Error("[NKUIDesign] lancement refuse, et rien n'a ete lance a la place. "
					 "Relancez avec --gfx=auto pour laisser la coquille choisir.");
		return -2;
	}

	gDesign.Init();

	// ⚠️ RECOPIE DU RESULTAT, JAMAIS UN SECOND CALCUL. Le panneau Preferences
	//    affiche ce que CETTE resolution a decide ; le laisser rappeler
	//    `NkGfxResolve` creerait une seconde verite qui divergerait au premier
	//    argument oublie.
	gDesign.gfxEffective = NkString(gfx.effective);
	gDesign.gfxSource = NkString(nkuidesign::NkGfxSourceName(gfx.source));
	// ⚠️ CE QUE LE FICHIER NOMME, ET C EST L AUTRE FAIT. Le menu marque l entree
	//    que `nkuidesign.cfg` porte, PAS celle qui tourne : sur un lancement
	//    `--gfx=vulkan` avec un fichier qui dit `dx11`, marquer `vulkan`
	//    laisserait croire que le fichier a change. `cfgGfx` est vide quand la
	//    cle est absente -- aucune entree n est alors marquee, et c est exact.
	gDesign.cfgChoice = NkString(cfgGfx);
	{
		uint32 nApis = 0;
		const char *const *apis = nkuidesign::NkGfxApiNames(nApis);
		for (uint32 i = 0; i < nApis; ++i)
			if (NkComponentDecl::StrEq(apis[i], gfx.requested))
				gDesign.prefsChoice = (int32)i;
	}

	// ⚠️ L'ETAT DES ROLES EST JOURNALISE AVANT L'OUVERTURE DE LA FENETRE. Le
	//    18/08, la seule facon d'apprendre que 23 roles declares ne resolvaient
	//    pas etait d'ouvrir la fenetre et de voir du magenta -- une couleur qui
	//    dit qu'il y a un probleme sans dire lequel. Cette ligne le dit avec des
	//    noms, sur une machine sans ecran, et avant meme la coquille.
	logger.Info("[NKUIDesign] rôles de thème — {0}", gDesign.roleAudit.Data());

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	NkEditorShellConfig cfg;
	cfg.title = "NkUIDesign — composer des interfaces à partir de composants déclarés";
	cfg.width = width;
	cfg.height = height;
	cfg.graphicsApi = gfx.api;
	// ── BACKEND DE RENDU, INJECTE ────────────────────────────────────────
	// Le kit n'en cree plus par defaut depuis le 2026-09-01 : un defaut dans
	// son .cpp etait une dependance de LIEN pour tout le monde. `static` parce
	// que le shell NE POSSEDE PAS ce pointeur -- l'objet doit lui survivre.
	static NkEditorCanvasRenderer canvasRenderer;
	cfg.renderer = &canvasRenderer;
	if (!shell || !shell->Init(cfg)) {
		logger.Error("[NKUIDesign] la coquille a refuse le backend '{0}' (retenu : {1}). Rien n'a "
					 "ete remplace : c'est un refus, pas un repli.",
					 gfx.requested, gfx.effective);
		return -1;
	}
	// ⚠️ Les guillemets francais ressortent en « ? » dans le fichier de journal :
	//    la ligne d'au-dessus les evite deja, celle-ci fait pareil.
	// ⚠️ ET LA TAILLE EST CELLE **DEMANDEE**, pas celle obtenue : la coquille
	//    restaure l'etat de fenetre de la session precedente. Un essai a demande
	//    1024x640 et a mesure une fenetre de 1936x1048 -- ecrire « fenetre WxH »
	//    sans le mot « demandee » ferait lire un chiffre faux comme une mesure.
	logger.Info("[NKUIDesign] coquille initialisée — backend demandé '{0}', retenu '{1}', "
				"fenêtre demandée {2}x{3} (l'état restauré peut la changer).",
				gfx.requested, gfx.effective, width, height);

	// ── LES PANNEAUX DES PLANCHES, ET LES ANCIENS QUI PARTENT ────────────
	// ⚠️ DEBRANCHER N EST PAS SUPPRIMER (Rodolf : « meme si tu laisses le
	//    code »). Les quatre classes restent ecrites dans `Panels.h`, compilees
	//    et compilables ; seul leur ENREGISTREMENT disparait. Remettre
	//    `NKUIDESIGN_ANCIENS_PANNEAUX` a 1 les rebranche a l identique, sans
	//    toucher a une seule autre ligne : c est l interrupteur, et il est
	//    reversible en un caractere.
	//
	// ⚠️ CE NE SONT PAS LES MEMES PANNEAUX QUI REVIENNENT SOUS UN AUTRE NOM.
	//    Palette/Composition a gauche et Proprietes/Preferences a droite
	//    SORTENT ; Hierarchie et Inspecteur (planches §22.5 et 091913) sont des
	//    panneaux NEUFS, batis sur les composants du kit. Faire evoluer les
	//    premiers vers les seconds aurait conserve leur structure -- c est
	//    exactement ce qu il ne faut pas.
	static nkuidesign::PreviewPanel preview(&gDesign);
	static nkuidesign::AIPanel ai(&gDesign);
	gPanneauIA = &ai; // le banc --mesure-async le pilote ; rien d'autre ne le lit
	gPanneauToile = &preview; // (R19) la sonde des portes ouvre ses menus
	static nkuidesign::HierarchyPanel hierarchie(&gDesign);
	static nkuidesign::InspectorPanel inspecteur(&gDesign);
	// LE RAIL « VARIABLES » (§15.14) : a gauche, onglet a cote de la Hierarchie --
	// c'est la place de Lunacy (`Variables` au rail de gauche). Il se ferme avec
	// les panneaux fixes en mode toile seule.
	static nkuidesign::VariablesPanel variables(&gDesign);
	static nkuidesign::StylesPanel stylesRail(&gDesign); // §15.15 : le rail « Styles », meme feuille
	// ⚠️ LA PALETTE REVIENT, MAIS PAR LE RAIL — ET CE N EST PAS UN RETOUR EN
	//    ARRIERE. Le §13.1 la place explicitement sur le rail GAUCHE, comme
	//    panneau SECONDAIRE : c est sa place, pas le dock. Elle est enregistree
	//    (le tiroir la retrouve par son titre) mais FERMEE : `DrawPanels` la
	//    saute, seul le tiroir la dessine. C est la difference entre debrancher
	//    un panneau et le ranger.
	static nkuidesign::SimulationPanel simulation(&gDesign);
	static nkuidesign::ConsolePanel console(&gDesign); // le rail bas « Console » l'ouvre par son titre
	static nkuidesign::AmbiancesPanel ambiances(&gDesign);
	static nkuidesign::GreffonsPanel greffons(&gDesign);
	static nkuidesign::BibliothequePanel bibliotheque(&gDesign);
	bibliotheque.SetOpen(false); // vit dans le TIROIR du rail droit (ecran 9)
	static nkuidesign::PalettePanel palette(&gDesign);
	palette.SetOpen(false);
	// (Q7) ILS VIVENT DANS LE PANNEAU DE DROITE : fermes au dock, le tiroir les
	// dessine (un panneau ouvert ET vise par une pastille serait dessine deux fois).
	inspecteur.SetOpen(false);
	variables.SetOpen(false);
	stylesRail.SetOpen(false);
	ambiances.SetOpen(false);
	greffons.SetOpen(false);
	if (gToileSeule) {
		// La toile seule : les deux panneaux fixes se FERMENT (ils restent
		// enregistres — Affichage les rouvre), les rails ne seront pas poses.
		hierarchie.SetOpen(false);
		inspecteur.SetOpen(false);
		variables.SetOpen(false);
		stylesRail.SetOpen(false);
		ai.SetOpen(false);
	}
	// ⚠️ L ORDRE D AJOUT DECIDE DE L ORDRE DES ONGLETS dans une meme feuille de
	//    dock : Hierarchie d abord (elle est seule a gauche), puis le centre,
	//    puis l Inspecteur, puis le bas.
	shell->AddPanel(&hierarchie);
	shell->AddPanel(&variables); // second onglet de la feuille gauche, apres la Hierarchie
	shell->AddPanel(&stylesRail); // troisieme : Styles (Lunacy : Styles puis Variables ; l'ordre d'ajout decide)
	shell->AddPanel(&preview);
	shell->AddPanel(&inspecteur);
	shell->AddPanel(&ai);
	shell->AddPanel(&palette);
	shell->AddPanel(&bibliotheque);
	shell->AddPanel(&simulation);
	shell->AddPanel(&console);
	shell->AddPanel(&ambiances);
	shell->AddPanel(&greffons);
	// Mise en scene : --panneau=<titre> ouvre un panneau ferme par defaut.
	if (gPanneauInitial[0]) {
		// `&ai` ajoute le 02/09 : la capture du panneau IA (tranche 3) en avait
		// besoin — un panneau ferme ne temoigne que du vide.
		nkentseu::editorkit::NkEditorPanel *tous[4] = {&simulation, &ambiances, &greffons, &ai};
		for (int32 pi = 0; pi < 4; ++pi)
			if (NkComponentDecl::StrEq(tous[pi]->Title(), gPanneauInitial))
				tous[pi]->SetOpen(true);
	}

#if NKUIDESIGN_ANCIENS_PANNEAUX
	static nkuidesign::CompositionPanel composition(&gDesign);
	static nkuidesign::PropertiesPanel properties(&gDesign);
	static nkuidesign::PreferencesPanel prefs(&gDesign);
	shell->AddPanel(&composition);
	shell->AddPanel(&properties);
	shell->AddPanel(&prefs);
#endif
	// ⚠️ L'INSTRUMENT S'ALLUME ICI, PAS DANS L'OVERLAY. Active depuis
	//    l'overlay, il aurait rate la PREMIERE image entiere : l'overlay passe
	//    apres les panneaux, donc le releve n'aurait commence a se remplir qu'a
	//    l'image suivante. Une image perdue n'est rien pour un editeur qui
	//    tourne — mais tout pour une capture prise au demarrage, et c'est
	//    exactement l'usage qu'on veut servir.
	// ⚠️ `NK_GUI_INTROSPECT=1` marche AUSSI, et sans ce drapeau : NKGui le lit
	//    a `Init`. Les deux voies mènent au meme booleen ; `--dump-ui` ne fait
	//    que l'allumer une seconde fois, ce qui est sans effet.
	if (gReleveDemande)
		nkgui::NkGuiIntrospectActiver(shell->Ui(), true);
	shell->SetOverlay(&EcrireReleveUI, nullptr);
	// (Q8) UN FICHIER LACHE SUR LE PANNEAU IA s'y joint (s'il est une image).
	shell->SetDropFilesHandler(
		+[](void *, const NkVector<NkString> &chemins, nkentseu::int32 x, nkentseu::int32 y) {
			if (!gPanneauIA)
				return;
			const editorkit::NkPaintRect r = gPanneauIA->Panneau().rect;
			if (!((float32)x >= r.x && (float32)x < r.x + r.w && (float32)y >= r.y && (float32)y < r.y + r.h))
				return;
			NkVector<const char *> c;
			for (usize i = 0; i < chemins.Size(); ++i)
				c.PushBack(chemins[i].CStr());
			NkString pq;
			const uint32 n = gPanneauIA->Panneau().DeposerFichiers(c.Data(), (uint32)c.Size(), pq);
			printf("[NKUIDesign] AI DEPOT %u fichier(s) -> %u image(s) jointe(s) %s%c", (unsigned)c.Size(), (unsigned)n,
				   pq.CStr(), (char)10);
		},
		nullptr);
	// NK_AI_IMAGE : apres l'image complete, avant sa soumission (21/09).
	if (std::getenv("NK_AI_IMAGE"))
		shell->SetApresImage(&ImagePanneauIA, shell.Get());

	// ── LE THEME : UNE SEULE AUTORITE, POUSSEE VERS LE DESSIN ────────────
	// ⚠️ SANS CET APPEL, LA MOITIE DE LA FENETRE NE SUIVRAIT PAS. La
	//    bibliotheque de l editeur porte les roles ; `NkGuiContext::theme` est ce
	//    que chaque primitive LIT. Poser les themes sans les pousser donne une
	//    bascule qui n emporte que ce qui passe par les roles -- et une bascule
	//    a moitie est pire qu une bascule absente, parce qu elle a l air de
	//    marcher. Le point unique est `NkEditorShell::ApplyTheme`.
	gThemes.AddBuiltins(); // Sombre, Clair, GitHub Dark Pro, GitHub Light Pro
	gShell = shell.Get(); // ⚠️ AVANT `AppliquerTheme`, qui s'en sert.
	// LA CHAINE DE L'IMAGE (05/09) : le cache du document televerse par le shell, le
	// peintre demande au cache -- pose ici, une fois, explicite.
	gDesign.images.televerser = [](void *u, const uint8 *px, int32 w, int32 h) -> uint32 {
		return static_cast<NkEditorShell *>(u)->UploadRGBA(px, w, h);
	};
	gDesign.images.televerserUser = gShell;
	nkuidesign::renderdetail::NkPoserFournisseurImages(&nkuidesign::NkObtenirImageDuDocument, &gDesign);
	if (!gThemeDemande.Empty()) {
		// ⚠️ UN NOM INCONNU SE DIT, IL NE SE REMPLACE PAS EN SILENCE. Même
		//    famille que le backend refusé : un repli muet ferait mesurer sur un
		//    thème qu'on n'a pas demandé, et on chercherait la différence
		//    ailleurs. Ici on garde le défaut, en le NOMMANT.
		const int32 i = gThemes.Find(gThemeDemande.Data());
		if (i < 0)
			logger.Error("[NKUIDesign] thème '{0}' INCONNU : le défaut est gardé. "
						 "Voir Affichage > Thème pour la liste.",
						 gThemeDemande.Data());
		else
			gThemes.SetCurrentIndex((uint32)i);
	}
	gDesign.theme = gThemes.Current();
	shell->ApplyTheme(gThemes.Current());
	logger.Info("[NKUIDesign] thème appliqué : '{0}' ({1} disponibles).",
				gThemes.Current().Name().CStr(), gThemes.Count());

	// ── L EN-TETE AUX COTES DE LA MAQUETTE ───────────────────────────────
	// 28 + 28, bloc logo carre de 56 a cheval sur les deux. Ces nombres sont
	// des PIXELS : la mesure sur la capture doit les rendre tels quels.
	shell->SetHeaderLayout(28.f, 28.f, 56.f);
	// ⚠️ DEBRANCHER, PAS DETRUIRE : la coquille garde son code de barre
	//    d activite intact, on lui dit seulement de ne pas la poser. Le dock
	//    reprend la largeur liberee -- c est ecrit dans `NkEditorShell.h`.
	shell->SetActivityBars(NKUIDESIGN_BARRES_ACTIVITE_GAUCHE != 0,
						   NKUIDESIGN_BARRES_ACTIVITE_DROITE != 0);
	// ⚠️ UN SEUL INDICATEUR DE ZOOM, ET C EST CELUI DE LA TOILE. Celui du pied
	//    mesure la police de code (une notion de NKCode) : NkUIDesign n a pas
	//    d editeur de code, et affichait donc « Zoom 107 % » a cote du « 100 % »
	//    du cluster -- deux grandeurs differentes sous le meme mot, au meme
	//    instant. Le plan n en prevoit qu un, dans le cluster, qui appartient au
	//    canvas.
	shell->SetFooterZoomIndicator(false);
	// LA GRANDE BARRE EXTERNE DES PANNEAUX SE DEBRANCHE (retour de Rodolf,
	// 01/09 : « la scrollbar la plus grande doit etre supprimee, elle n'est
	// plus importante ») : la Hierarchie a ses ascenseurs PAR SECTION,
	// l'Inspecteur se replie par sections et garde la MOLETTE — la reference
	// Banani ne montre aucune barre externe. Interrupteur additif du kit,
	// motif SetStatusBarVisible.
	shell->SetDockScrollbarVisible(false);
	// Convention V2 (Banani, decision coordinateur 31/08) : l'onglet ACTIF
	// rejoint le fond de la zone document. OPT-IN du socle -- NKCode et les
	// autres consommateurs gardent l'historique tant qu'ils n'optent pas.
	shell->Ui().theme.tabActiveIsWindowBg = true;

	// ── LE COSTUME EXACT (remandat Rodolf 31/08 : « THEME, DESIGN, POLICE,
	//    TOUT ») — chaque ligne consomme un crochet OPT-IN du kit. ──────────
	// 1. Les polices de la maquette : corps 9..16, Inter embarquee. La police
	//    d'INTERFACE passe a 12 px (le --text-base des jetons), celle de la
	//    BARRE DE TITRE a 11 px (menus + nom de fichier du TopHeader).
	nkuidesign::costume::Fontes().Charger(*shell, shell->DpiScale());
	if (!nkuidesign::costume::Fontes().ok)
		logger.Error("[NKUIDesign] polices du costume : au moins un corps n'a pas chargé — "
					 "les zones concernées retomberont sur la police d'interface.");
	// `CorpsMaquette` : LE réglage unique de taille (Costume.h) — la coquille
	// suit le même +2 que les sept corps du costume (test de Rodolf, 31/08).
	shell->ForceUiFontSize(nkuidesign::costume::CorpsMaquette(12.f));
	shell->SetTitleBarFont(&nkuidesign::costume::Fontes().px11);
	// 2. Le bloc logo 56x56 de la maquette (degrade + 4 carreaux + diagonale).
	//    Le « O » Rihen reste le defaut du kit pour toutes les autres
	//    applications — ici la planche prime, decision a l'oeil pour Rodolf.
	shell->SetHeaderLogoFn(
		[](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, void *) {
			nkuidesign::costume::LogoBanani(ui.dl, r);
		},
		nullptr);
	// 3. Controles de fenetre compacts, fermer sur fond rouge permanent.
	//    20 px, pas les 13 de la maquette : Rodolf (31/08, 2e passe) —
	//    « les boutons reduire/agrandir/fermer sont trop petits » ;
	//    proportionnes a la bande de titre de 28.
	shell->SetWindowControlsCompact(true, 20.f);
	// 4. Les panneaux lateraux dessinent leur propre en-tete de 34 px : la
	//    barre d'onglets du dock disparait quand ils sont seuls.
	shell->SetSideTabsVisible(false);
	// 5. Les etats colores prennent les valeurs EXACTES des jetons Banani
	//    (success #3fb950, warning #d29922, error #f85149) — la pastille « ● »
	//    et le badge d'erreurs les lisent.
	shell->Ui().theme.success = {63, 185, 80, 255};
	shell->Ui().theme.warning = {210, 153, 34, 255};
	shell->Ui().theme.danger = {248, 81, 73, 255};
	// 6. « ● Pret » a droite du rail bas.
	shell->SetRailFooterStatus("Prêt", {63, 185, 80, 255});
	// LE CHARGEUR D'IMAGES DES DOCUMENTS (27/09) — ici parce que la coquille a
	// desormais son renderer. Sans cet appel, un `image:` d'un `.nkgui` serait
	// compte `sansChargeur` et hachure : visible, jamais muet.
	nkgui::NkEditorInstallerChargeurImage(shell.Get());
	// ⚠️ UN SEUL BANDEAU BAS (§4/§13 ; Rodolf, 30/08 : « pourquoi il y a deux
	//    footers ? ») : la barre d'etat VSCode se debranche, le RAIL de
	//    pastilles est le survivant — l'aide contextuelle et les messages
	//    d'etat vivent dedans, a droite des pastilles. `SetFooter` (gfx...)
	//    y est route par la coquille : aucun message ne se perd.
	shell->SetStatusBarVisible(false);
	gDesign.pied = [](void *u, const char *t) {
		static_cast<NkEditorShell *>(u)->SetRailFooterText(t);
	};
	gDesign.piedUser = shell.Get();
	// La pastille « ● » du nom de fichier (Banani TopHeader : non-enregistre).
	// L'etat arrive MESURE (Panels) ; ici on ne repeint qu'au changement.
	gDesign.titre = [](void *u, bool modifie) {
		// ⚠️ LE NOM VIENT DU DOCUMENT ACTIF, plus d'un libelle en dur (le
		//    « Dashboard_Admin.nkgui » ecrit ici mentait des qu'un autre
		//    onglet devenait actif — 5e retour). Le fichier reel prime ;
		//    un document jamais enregistre montre son titre.
		static bool dernier = false;
		static nkentseu::NkString dernierNom;
		char nom[160];
		const char *base = gDesign.cheminActif.Data();
		if (base && *base) {
			const char *slash = base;
			for (const char *q = base; *q; ++q)
				if (*q == '/' || *q == '\\')
					slash = q + 1;
			snprintf(nom, sizeof(nom), "%s", slash);
		} else
			snprintf(nom, sizeof(nom), "%s", gDesign.doc.title.Data());
		const bool memeNom = dernierNom.Data() && NkComponentDecl::StrEq(dernierNom.Data(), nom);
		if (memeNom && modifie == dernier)
			return;
		dernier = modifie;
		dernierNom = nkentseu::NkString(nom);
		gDocumentModifie = modifie; // l'onglet actif porte la meme pastille
		char plein[256];
		snprintf(plein, sizeof(plein), "%s%s%s", gTitreSonde ? kTitreSonde : "",
				 modifie ? "\xE2\x97\x8F " : "", nom);
		static_cast<NkEditorShell *>(u)->SetTitleInfo(plein);
	};
	gDesign.titreUser = shell.Get();

	// ── LES TROIS RAILS DE PASTILLES (document 3 §13.1) ──────────────────
	// ⚠️ CE SONT DES PANNEAUX SECONDAIRES, et le rail existe pour qu ils
	//    cessent de saturer l ecran en permanence tout en restant a un clic.
	//    Les pastilles portent une LETTRE faute d atlas d icones -- les icones
	//    se dessineront dans NkUIDesign lui-meme (regle du 18/08), et c est
	//    justement une des choses que cette application doit rendre possible.
	// ⚠️ LE TITRE EST UNE CLE : il doit s ecrire a l identique ici et dans le
	//    constructeur du panneau. Le meme piege a deja coute un `Ctrl+J` muet
	//    aujourd hui (« Hierarchie » contre « Hiérarchie »).
	// ⚠️ CE QUI N EXISTE PAS ENCORE LE DIT PLUTOT QUE DE MANQUER : quatre des
	//    six panneaux ne sont pas ecrits (Bibliothèque, Callbacks, Console,
	//    Aperçu/Test). Leur pastille est POSEE quand meme -- le tiroir affiche
	//    alors « aucun panneau enregistre sous ce titre », en rouge. Une
	//    pastille absente ferait croire que le plan a change ; une pastille qui
	//    s ouvre sur un message dit exactement ou on en est.
	// ⚠️ COSTUME BANANI (31/08) : l'ecran 1 ne montre AUCUN rail gauche — il est
	//    retire (la Palette reste accessible par le menu Affichage). Le rail
	//    DROIT porte les trois pastilles de la maquette (bibliotheque en
	//    carreaux, etoile IA violette, oeil-vague d'apercu) et le rail BAS ses
	//    deux PILULES (Console, Apercu) — chaque icone est dessinee par
	//    l'application via le crochet `icone` du kit, aux traces exacts du JSX.
	//    Les CLES de panneau ne changent pas (« Bibliothèque », « IA »,
	//    « Test », « Console ») : seul le costume bouge.
	using nkuidesign::costume::Fontes;
	static NkEditorShell::NkEditorRailItem kRailDroite[] = {
		{"Bibliothèque", "Bibliothèque de composants — acquérir", "B",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool, bool, void *) {
			 nkuidesign::costume::IcCarreaux(ui.dl, r.x + (r.w - 14.f) * 0.5f,
											 r.y + (r.h - 14.f) * 0.5f, ui.theme.textMuted);
		 }},
		{"IA", "Chat IA", "IA",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool, void *) {
			 // L'etoile IA est VIOLETTE au repos, sur un voile accent a 13 % —
			 // c'est l'etat que la maquette fige (SideRail, etoile active).
			 const nkgui::NkColor violet = nkentseu::editorkit::NkThemeUnpack(
				 gDesign.theme.Get(nkentseu::editorkit::NkRole::AccentAI));
			 if (!ouvert) {
				 nkgui::NkColor voile = ui.theme.accent;
				 voile.a = 34;
				 ui.dl.AddRectFilled(r, voile, 4.f);
			 }
			 nkuidesign::costume::IcEtoile(ui.dl, r.x + (r.w - 14.f) * 0.5f,
										   r.y + (r.h - 14.f) * 0.5f, violet);
		 }},
		// 🔴 LA CLE ETAIT « Test », ET AUCUN PANNEAU NE S'APPELLE AINSI. Mesure du
		//    14/09 : `--tiroir=d:2` ouvrait un tiroir portant « Aucun panneau
		//    enregistre sous ce titre. » en rouge (466 px de #f85149 contre 72 de
		//    fond partout ailleurs). Le panneau existe : c'est `PreviewPanel`,
		//    `NkEditorPanel("Aperçu", NK_CENTER)` (Panels.h). Le titre EST la cle --
		//    l'avertissement etait deja ecrit vingt lignes plus haut, et il a quand
		//    meme ete paye deux fois (« Hierarchie » contre « Hiérarchie », puis ici).
		// (Q7, 21/09) « Aperçu » QUITTE LE RAIL DROIT : c'est la toile CENTRALE, et
		//    son tiroir ne pouvait qu'ecrire « deja ancre ». Le rail bas garde sa
		//    pilule. A sa place, les panneaux qui etaient ANCRES a droite ou a
		//    gauche : Rodolf veut UN panneau de droite dont le contenu change selon
		//    la pastille, comme NK3DModeler.
		{"Inspecteur", "Inspecteur — propriétés de l'élément sélectionné", "I",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool survol, void *) {
			 nkuidesign::costume::IcInspecteur(ui.dl, r.x + (r.w - 14.f) * 0.5f, r.y + (r.h - 14.f) * 0.5f,
									 (ouvert || survol) ? ui.theme.text : ui.theme.textMuted);
		 }},
		{"Styles", "Styles — remplissages et typographies", "S",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool survol, void *) {
			 nkuidesign::costume::IcStyles(ui.dl, r.x + (r.w - 14.f) * 0.5f, r.y + (r.h - 14.f) * 0.5f,
									 (ouvert || survol) ? ui.theme.text : ui.theme.textMuted);
		 }},
		{"Variables", "Variables — couleurs et nombres nommés", "V",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool survol, void *) {
			 nkuidesign::costume::IcVariables(ui.dl, r.x + (r.w - 14.f) * 0.5f, r.y + (r.h - 14.f) * 0.5f,
									 (ouvert || survol) ? ui.theme.text : ui.theme.textMuted);
		 }},
		{"Ambiances", "Ambiances — les jeux de variables", "A",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool survol, void *) {
			 nkuidesign::costume::IcAmbiances(ui.dl, r.x + (r.w - 14.f) * 0.5f, r.y + (r.h - 14.f) * 0.5f,
									 (ouvert || survol) ? ui.theme.text : ui.theme.textMuted);
		 }},
		{"Greffons", "Greffons — extensions", "G",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool survol, void *) {
			 nkuidesign::costume::IcGreffons(ui.dl, r.x + (r.w - 14.f) * 0.5f, r.y + (r.h - 14.f) * 0.5f,
									 (ouvert || survol) ? ui.theme.text : ui.theme.textMuted);
		 }},
	};
	static NkEditorShell::NkEditorRailItem kRailBas[] = {
		{"Console", "Console / Validation", "C",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool ouvert, bool, void *) {
			 // La pilule Console de la maquette : fond `button` (#21262d), icone
			 // `>_`, libelle 11 px. Le badge rouge n'apparait qu'avec de VRAIES
			 // erreurs — la maquette en fige deux, nous n'inventons pas l'etat.
			 if (!ouvert)
				 ui.dl.AddRectFilled(r, ui.theme.button, 4.f);
			 nkuidesign::costume::IcConsole(ui.dl, r.x + 8.f, r.y + (r.h - 12.f) * 0.5f,
											ui.theme.textMuted);
			 nkuidesign::costume::Texte(ui.dl, Fontes().px11, r.x + 8.f + 12.f + 6.f,
										nkuidesign::costume::CentrerY(Fontes().px11, r.y, r.h),
										"Console", ui.theme.textMuted);
		 }},
		// Meme cle, meme correctif que sur le rail droit : « Aperçu », pas « Test ».
		{"Aperçu", "Aperçu / Test — exécuter l'interface dessinée", "T",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool, bool, void *) {
			 nkuidesign::costume::IcOeil(ui.dl, r.x + 8.f, r.y + (r.h - 12.f) * 0.5f,
										 ui.theme.textMuted);
			 nkuidesign::costume::Texte(ui.dl, Fontes().px11, r.x + 8.f + 12.f + 6.f,
										nkuidesign::costume::CentrerY(Fontes().px11, r.y, r.h),
										"Aperçu", ui.theme.textMuted);
		 }},
	};
	// Largeur des pilules : 8 (marge) + 12 (icone) + 6 (ecart) + texte + 8.
	kRailBas[0].largeur = 34.f + nkuidesign::costume::Largeur(Fontes().px11, "Console");
	kRailBas[1].largeur = 34.f + nkuidesign::costume::Largeur(Fontes().px11, "Aperçu");
	shell->SetRail(NkEditorDockSide::NK_LEFT, nullptr, 0);
	// On resout ICI, ou `kRailDroite` existe : le nom fait foi, jamais l'indice.
	// ⚠️ ET SEULEMENT SI LE RAIL EST REELLEMENT POSE. Avec `--toile-seule`,
	//    `SetRail` installe ZERO pastille : resoudre quand meme laissait
	//    `OuvrirTiroir` ne rien faire (son garde `index < mRailCount` echoue)
	//    pendant que le journal annoncait « tiroir IA ouvert ». *Un journal qui
	//    annonce le resultat au lieu de le constater est un instrument qui ment*,
	//    et je l'ai vu mentir avant de le corriger.
	if (!gToileSeule)
	for (int32 i = 0; i < (int32)(sizeof(kRailDroite) / sizeof(kRailDroite[0])); ++i)
		if (kRailDroite[i].panel && NkComponentDecl::StrEq(kRailDroite[i].panel, "IA")) {
			gTiroirIA = i;
			break;
		}
	// (Q6/Q7) LE PANNEAU IA PORTE SON EN-TETE ; L'INSPECTEUR NE VAUT QUE POUR UNE
	//    SELECTION ; LE TIROIR DROIT EST ANCRE (il prend sa place, comme le panneau
	//    de droite du modeleur, au lieu de passer sur la toile).
	for (int32 i = 0; i < (int32)(sizeof(kRailDroite) / sizeof(kRailDroite[0])); ++i) {
		if (NkComponentDecl::StrEq(kRailDroite[i].panel, "IA"))
			kRailDroite[i].titrePropre = true;
		if (NkComponentDecl::StrEq(kRailDroite[i].panel, "Inspecteur"))
			kRailDroite[i].lieALaSelection = true;
	}
	shell->SetRail(NkEditorDockSide::NK_RIGHT, kRailDroite,
				   gToileSeule ? 0 : (int32)(sizeof(kRailDroite) / sizeof(kRailDroite[0])));
	shell->SetRailAncre(NkEditorDockSide::NK_RIGHT, true);
	shell->SetRail(NkEditorDockSide::NK_BOTTOM, kRailBas, gToileSeule ? 0 : 2);
	// Le crochet du chemin REPARE : il passe par la resolution par le nom.
	if (gTiroirParLeNom)
		OuvrirTiroirIA();
	// (Q7) PAR DEFAUT le panneau de droite est OUVERT sur l'Inspecteur -- c'etait
	// le panneau ancre a droite ; sa place ne change pas, seul son hote change.
	if (!gToileSeule && !gTiroirParLeNom && gTiroirCote != 'd')
		for (int32 i = 0; i < (int32)(sizeof(kRailDroite) / sizeof(kRailDroite[0])); ++i)
			if (NkComponentDecl::StrEq(kRailDroite[i].panel, "Inspecteur"))
				shell->OuvrirTiroir(NkEditorDockSide::NK_RIGHT, i);
	// (Q6) LA LARGEUR DU PANNEAU DE DROITE EST MEMORISEE par la persistance
	// existante de la coquille (`LoadUiState` / `SaveUiState`, ligne `tiroir=`).
	// `NK_UI_ETAT` designe un autre fichier : une sonde ne touche pas l'etat de
	// Rodolf.
	{
		const char *etat = std::getenv("NK_UI_ETAT");
		// ⚠️ UNE FENETRE DE SONDE N'ECRIT PAS DANS LE FICHIER DE RODOLF.
		//    (25/09) LA GARDE TESTAIT `gTitreSonde` -- c'est-a-dire l'option
		//    `--titre-sonde` -- ET PAS `NK_SONDE`. Deux noms pour la meme idee, et
		//    ils ont diverge : une course lancee avec `NK_SONDE=1` seul passait a
		//    travers et REECRIVAIT `logs/nkuidesign_ui.cfg`. Mesure par
		//    `Tools/sonde_etat_utilisateur.py` : 284 octets -> 228, empreinte
		//    changee. On demande donc au KIT, qui connait les deux signaux
		//    (`NK_SONDE`, `NK_TOAST_PROBE`), au lieu de recopier un troisieme test.
		//
		// ⚠️ ET ON REDIRIGE PLUTOT QUE DE NE RIEN ECRIRE : une sonde qui n'ecrit
		//    rien perd la largeur de son tiroir d'une course a l'autre, donc on ne
		//    peut plus eprouver la PERSISTANCE. `NK_ETAT_SONDE` (ou son defaut,
		//    `logs/sonde-etat/`) lui donne son propre fichier.
		{
			const NkString redir =
				nkentseu::editorkit::NkSondeChemin(nullptr, "nkuidesign_ui.cfg");
			if (etat && *etat)
				snprintf(gCheminEtatUi, sizeof(gCheminEtatUi), "%s", etat);
			else if (!redir.Empty())
				snprintf(gCheminEtatUi, sizeof(gCheminEtatUi), "%s", redir.CStr());
			else if (!gTitreSonde)
				snprintf(gCheminEtatUi, sizeof(gCheminEtatUi), "%s", "logs/nkuidesign_ui.cfg");
		}
		// ⚠️ SANS LA GEOMETRIE DE LA FENETRE : elle grossirait de +16/+39 px a
		//    chaque lancement (`SetSize(GetSize())` n'est pas l'identite).
		shell->SetUiStateGeometrie(false);
		shell->EcrireEtatDocks("avant LoadUiState"); // (25/09) NK_DOCKS : l'etat AVANT le fichier
		if (gCheminEtatUi[0])
			shell->LoadUiState(gCheminEtatUi);
		printf("[NKUIDesign] ETAT UI relu de %s : panneau de droite %.0f px\n", gCheminEtatUi,
			   (double)shell->RailLargeur(NkEditorDockSide::NK_RIGHT));
		// (25/09) ET L'ETAT APRES : la difference dit exactement ce que le fichier a
		// ouvert -- c'est-a-dire ce qui est arrive par la seconde porte.
		shell->EcrireEtatDocks("apres LoadUiState");
	}
	if (gTiroirCote == 'd')
		shell->OuvrirTiroir(NkEditorDockSide::NK_RIGHT, gTiroirIndex);
	else if (gTiroirCote == 'g')
		shell->OuvrirTiroir(NkEditorDockSide::NK_LEFT, gTiroirIndex);
	else if (gTiroirCote == 'b')
		shell->OuvrirTiroir(NkEditorDockSide::NK_BOTTOM, gTiroirIndex);
	if (gToileSeule)
		shell->SetRailFooterStatus("", {0, 0, 0, 0}); // pas de bandeau bas du tout
	shell->SetMenuBar(&DrawMenuBar, nullptr);
	shell->SetToolbar(&DrawProjectTabs, nullptr);
	// ═════════════════════════════════════════════════════════════════════════
	//  LE MENU « Design » ET LA BARRE D'ETAT VIENNENT D'UN DOCUMENT `.nkgui`
	// ═════════════════════════════════════════════════════════════════════════
	//  Rodolf, 27/09 : « NKUIDesign doit lui aussi utiliser .nkgui […] on copie,
	//  on modifie apres, doucement doucement. »
	//
	//  ⚠️ STRICTEMENT ADDITIF, ET C'EST MESURE. `SetMenuBar` (447 lignes qui
	//     portent AUSSI la recolte de la generation IA) et `SetToolbar` (les
	//     onglets de projet, enumeres a l'execution) restent ce qu'ils sont, deux
	//     lignes au-dessus. Les deux crochets pris ici etaient LIBRES :
	//     `SetStatusBarFn` n'etait jamais pose, et `SetAppMenu` ne l'est que sous
	//     un drapeau de sonde. Rien de ce qui marche n'est retire.
	//
	//  ⚠️ ET LA TABLE POINTE SUR LES `Cmd*` QUE `RegisterCommand` SERT DEJA.
	//     `NkEditorCommandFn` et `NkActionFn` sont la meme signature
	//     (`void (*)(void *)`), donc aucune enveloppe : un nom de moins a tenir
	//     d'accord. Le kit, lui, ne sait lancer une commande que par un INDICE
	//     (`ExecuteCommand(int32)`) — *un indice n'est pas un nom*, il se decale
	//     des qu'une commande est inseree.
	{
		static nkuidesign::NkCoquilleDocument s_coquilleDoc;
		const bool luDoc = s_coquilleDoc.ChargerDepuisDossier("Resources/Interface/NKUIDesign");
		// LA MEME table que celle que la sonde juge — voir `gActionsDocument`.
		s_coquilleDoc.PoserTables(gActionsDocument, gNbActionsDocument, nullptr, 0u);
		// ⚠️ ON NE BRANCHE PAS UNE BANDE QU'ON N'A PAS LUE. Poser le crochet sur un
		//    document absent aurait donne une bande vide, indiscernable d'une bande
		//    qui n'affiche rien — et le refus se serait tu. `RefusTotal` le NOMME
		//    dans le journal, et la sonde `--sonde-coquille` en fait un verdict.
		if (luDoc) {
			shell->SetStatusBarFn(&nkuidesign::NkCoquilleDocument::MonterBarreEtat,
								  &s_coquilleDoc);
			// ── LA BARRE D'OUTILS FAIT LES DEUX ─────────────────────────────
			// ⚠️ `SetToolbar` N'ACCEPTE QU'UN SEUL RAPPEL, et `DrawProjectTabs`
			//    l'occupait : il enumere les projets ouverts A L'EXECUTION, et le
			//    format ne sait pas exprimer une liste engendree. Le remplacer aurait
			//    SUPPRIME les onglets — on ne migre pas vers moins.
			//
			//    Ce rappel appelle donc les deux, dans cet ordre : les onglets
			//    d'abord (le code qui marche), puis la racine `barre_outils`. Rien
			//    n'est retire, et le document gagne sa place dans la bande.
			shell->SetToolbar(
				[](NkEditorFrameContext &ec, void *u) {
					DrawProjectTabs(ec, nullptr);
					nkuidesign::NkCoquilleDocument::MonterBarreOutils(ec, u);
				},
				&s_coquilleDoc);
			// Le panneau dont le CONTENU vient du document. Il s'ajoute aux panneaux
			// existants de `Panels.h` — aucun n'est touche.
			{
				static nkuidesign::PanneauDocument s_panneauDoc(s_coquilleDoc);
				shell->AddPanel(&s_panneauDoc);
			}
			// La sonde garde la priorite sur `SetAppMenu` : elle MESURE, un menu non.
			if (!gCapturePath[0] && !gSondeGel[0] && !gSondePortes[0] && gMesureAsyncMs < 0
				&& gMesureFpsMs < 0 && gMesureDoubleImages < 0 && gMesureTexteImages < 0)
				shell->SetAppMenu(&nkuidesign::NkCoquilleDocument::MonterMenuApp,
								  &s_coquilleDoc);
		} else {
			// ⚠️ `printf` ET NON LE JOURNAL, parce que c'est ce que ce fichier fait
			//    partout (220 sites) et que le verdict d'une sonde se lit sur la
			//    sortie standard. Un message envoye au journal quand tout le reste
			//    passe par la console se cherche dans le mauvais fichier — ce depot
			//    a deja paye « le verdict n'existait pas la ou on regarde ».
			printf("[COQUILLE] %u document(s) .nkgui non lu(s) dans "
				   "Resources/Interface/NKUIDesign - bandes non branchees\n",
				   s_coquilleDoc.RefusTotal());
		}
	}
	// Le titre initial vient du DOCUMENT (le callback `titre` prendra le
	// relais a la premiere mesure — meme regle : jamais un nom en dur).
	{
		// ⚠️ LE TITRE INITIAL AUSSI, et pas seulement le rappel : le rappel ne
		//    s'execute qu'a un CHANGEMENT (il sort tot si le nom et l'etat
		//    « modifie » n'ont pas bouge). Une fenetre de sonde qui n'edite rien
		//    ne le declencherait jamais -- elle serait restee sous le titre du
		//    produit, exactement le defaut qu'on ferme.
		char titre0[256];
		snprintf(titre0, sizeof(titre0), "%s%s", gTitreSonde ? kTitreSonde : "",
				 gDesign.doc.title.Data() ? gDesign.doc.title.Data() : "NkUIDesign");
		shell->SetTitleInfo(titre0);
	}
	shell->RegisterCommand("Document: Enregistrer", &CmdSave, nullptr, "Ctrl+S");
	// ④ CTRL+E : DECLARE UNE SEULE FOIS, ici. La toile ne le lit pas -- deux declarations
	//    feraient deux ouvertures, exactement le defaut ① du matin (Ctrl+D).
	shell->RegisterCommand("Fichier: Exporter…", &CmdExporter, nullptr, "Ctrl+E");
	// L'annulation unifiée (§7) : Ctrl+Z / Ctrl+Y, et Ctrl+Maj+Z en seconde
	// orthographe du rétablir (le standard des trois éditeurs de référence).
	shell->RegisterCommand("Édition: Annuler", &CmdUndo, nullptr, "Ctrl+Z");
	shell->RegisterCommand("Édition: Rétablir", &CmdRedo, nullptr, "Ctrl+Y");
	shell->RegisterCommand("Édition: Rétablir (Maj)", &CmdRedo, nullptr, "Ctrl+Shift+Z");
	shell->RegisterCommand("Document: Recharger", &CmdLoad, nullptr, "Ctrl+R");
	shell->RegisterCommand("Document: Nouveau", &CmdNew, nullptr, "Ctrl+N");
	// Les gestes d'édition Lunacy qui n'ont PAS de drapeau `want*` dans NKGui
	// (Ctrl+C/X/V/A en ont un, eux — cf. le commentaire de CmdDupliquer).
	// 🔴 SANS RACCOURCI ICI, ET C'EST LA SECONDE MOITIE DU DEFAUT ① (2026-09-05).
	//    Ctrl+D, Ctrl+G et Ctrl+Maj+G etaient declares DEUX FOIS : ici (la coquille les
	//    rejoue a chaque evenement clavier) ET dans la table de la toile (`MenuContexte.h`,
	//    lue au FRONT par `KeyPressed`). Une pression donnait donc au moins deux copies,
	//    et une touche tenue en donnait une par evenement de repetition.
	//    ⚠️ LES COMMANDES RESTENT (la palette Ctrl+P les liste et les execute) : seul le
	//       RACCOURCI part. Un geste de toile a UNE porte -- celle qui connait le mode
	//       d'edition, le popup ouvert et le renommage en cours, c'est-a-dire la toile.
	//    Ctrl+S / Ctrl+Z / Ctrl+Y / Ctrl+N restent ici : la toile ne les lit pas.
	// ⚠️ ET CE N'EST PAS LA REPETITION DE L'OS -- hypothese ECRITE PUIS INFIRMEE le
	//    05/09 : j'ai d'abord accuse la coquille de rejouer le raccourci a chaque
	//    evenement clavier. Mesure : le dorsal Win32 TRIE deja (`NkWin32EventSystem.cpp`,
	//    `isPress && isRep` -> `NkKeyRepeatEvent`, sinon `NkKeyPressEvent`) et la coquille
	//    n'ecoute pas la repetition. Une touche tenue n'envoie donc qu'UN `NkKeyPressEvent`.
	//    Le compte etait exactement DEUX copies par pression, et il n'en reste qu'une.
	shell->RegisterCommand("Édition: Dupliquer (Ctrl+D sur la toile)", &CmdDupliquer, nullptr, nullptr);
	shell->RegisterCommand("Objet: Grouper (Ctrl+G sur la toile)", &CmdGrouper, nullptr, nullptr);
	shell->RegisterCommand("Objet: Dégrouper (Ctrl+Maj+G sur la toile)", &CmdDegrouper, nullptr, nullptr);
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");
	gShell = shell.Get();
	// LE DETECTEUR DE GEL (NKEditorKit) NOMME LES SOURCES DE L'APPLICATION : sans ce crochet,
	// sa ligne dirait la porte du kit sans dire QUI l'a posee. Toujours pose, rien a armer.
	shell->SetSourcesOuvertes(&SourcesOuvertesNKUIDesign);
	// Le mode --capture branche son tick par frame — cf. le bloc CaptureTick en
	// tête de fichier. Hors capture, aucun callback : rien ne change.
	if (gCapturePath[0])
		shell->SetAppMenu(&CaptureTick, shell.Get());
	else if (gSondeGel[0])
		shell->SetAppMenu(&GelTick, shell.Get()); // la sonde du detecteur de gel
	else if (gSondePortes[0])
		shell->SetAppMenu(&PortesTick, shell.Get()); // (R16) la sonde des portes du corps
	else if (gMesureAsyncMs >= 0 || gMesureFpsMs >= 0 || gMesureDoubleImages >= 0
			 || gMesureTexteImages >= 0)
		shell->SetAppMenu(
			[](NkEditorFrameContext &, void *u) { MesureTick(static_cast<NkEditorShell *>(u)); },
			shell.Get());
	// ⚠️ DES LETTRES, PAS DES CHIFFRES, ET C'EST UNE CONTRAINTE MESUREE :
	//    `NkEditorShell::TryRunShortcut` n'accepte qu'un nom de touche de la
	//    forme exacte « NK_X » (quatre caracteres). Un `Ctrl+1` s'affiche a cote
	//    de la commande et **ne se declenche jamais** — un raccourci cosmetique,
	//    c'est-a-dire un parametre qui n'est pas honore. Il m'a fait croire
	//    pendant une heure que le panneau ne passait pas devant.
	// ⚠️ LES RACCOURCIS SUIVENT LES PANNEAUX. Un `Ctrl+M` qui cherche un panneau
	//    « Preferences » debranche journaliserait `FocusPanel -> FAUX` a chaque
	//    appui : un raccourci annonce qui ne fait rien, c est-a-dire exactement
	//    le « parametre qui n est pas honore » que ce chantier a deja paye.
	shell->RegisterCommand("Vue: Hiérarchie", &CmdVueHierarchie, nullptr, "Ctrl+J");
	shell->RegisterCommand("Vue: Inspecteur", &CmdVueInspecteur, nullptr, "Ctrl+L");
#if NKUIDESIGN_ANCIENS_PANNEAUX
	shell->RegisterCommand("Vue: Palette", &CmdVuePalette, nullptr, "Ctrl+B");
	shell->RegisterCommand("Vue: Composition", &CmdVueComposition, nullptr, "Ctrl+K");
	shell->RegisterCommand("Vue: Propriétés", &CmdVueProprietes, nullptr, "Ctrl+P");
	shell->RegisterCommand("Vue: Préférences", &CmdVuePreferences, nullptr, "Ctrl+M");
#endif

	// (R19) LA COMMANDE SANS EFFET : enregistree EN DERNIER et seulement sous la sonde des portes.
	// La palette se ferme par l'execution d'une commande ; celle-ci ne fait que se compter.
	if (gSondePortes[0] && !shell->RegisterCommand("Sonde : rien", +[](void *) { ++gSondeRienExecutee; }, nullptr, nullptr))
		printf("[sonde-portes] la commande sans effet n'a PAS pu etre enregistree\n");
	const int codeShell = shell->Run();
	// (Q6) LA LARGEUR SURVIT AU RELANCEMENT : ecrite par la persistance existante.
	if (gCheminEtatUi[0] && !gToileSeule) {
		shell->SaveUiState(gCheminEtatUi);
		printf("[NKUIDesign] ETAT UI ecrit dans %s : panneau de droite %.0f px\n", gCheminEtatUi,
			   (double)shell->RailLargeur(NkEditorDockSide::NK_RIGHT));
	}

	// ── Verdict du mode --capture : le FICHIER, pas un drapeau ────────────────
	// Le tick a pu armer, le backend a pu accepter — seul le fichier écrit
	// prouve que des pixels ont été lus. Un readback qui échoue (backend sans
	// Capture, disque plein) laisse un chemin sans fichier : code 3, nommé.
	if (gCapturePath[0]) {
		FILE *f = fopen(gCapturePath, "rb");
		long taille = 0;
		if (f) {
			fseek(f, 0, SEEK_END);
			taille = ftell(f);
			fclose(f);
		}
		if (!f || taille <= 8) {
			fputs("[NKUIDesign] capture EN ECHEC : fichier absent ou vide -- ", stdout);
			puts(gCapturePath);
			return 3;
		}
		fputs("[NKUIDesign] capture ecrite : ", stdout);
		puts(gCapturePath);
	}
	return codeShell;
}
