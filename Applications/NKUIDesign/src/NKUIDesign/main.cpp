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
#include "NKEditorKit/NkEditorModal.h" // le cadre modal du kit (choix Nouveau projet)
#include "NKEditorKit/NkThemeToGui.h"  // NkThemeUnpack : role de theme -> couleur de dessin
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKPlatform/NkEnv.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"

#include <cstdlib> // atof (levier --lignes=)

#include "Backend.h"
#include "Costume.h" // le costume exact Banani : polices 9-16 px + icônes (remandat 31/08)
#include "NkGuiRoundTrip.h"
#include "NkDocPoolControls.h"
#include "Panels.h"
#include "Probe.h"
#include "DesignAIRecette.h" // --recette-ia : la preuve de recette du pipeline IA
#include "RecetteEdition.h"	 // --recette-edition : le contrat universel d'edition, par site



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
static nkentseu::int32 RecetteAnnulation() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	auto ser = [](DesignState &s) {
		NkString o;
		s.doc.Save(o);
		return o;
	};
	auto identiques = [](const NkString &a, const NkString &b) -> bool {
		const char *x = a.Data() ? a.Data() : "";
		const char *y = b.Data() ? b.Data() : "";
		while (*x && *x == *y) {
			++x;
			++y;
		}
		return *x == *y;
	};
	// L'observateur pousse apres ~6 passages STABLES : on lui donne 10.
	auto stabiliser = [&](DesignState &s) {
		for (int32 i = 0; i < 10; ++i) {
			NkString o;
			s.doc.Save(o);
			s.histoire.Observer(o);
		}
	};
	static DesignState st; // static : l'etat est gros, pas sur la pile
	st.BuildStarterDocument();
	stabiliser(st);
	const NkString initial = ser(st);
	int32 echecs = 0, gestes = 0;
	NkString avant;
	auto avantGeste = [&] { avant = ser(st); };
	auto apresGeste = [&](const char *nom) {
		stabiliser(st);
		++gestes;
		const NkString apres = ser(st);
		st.Annuler();
		const bool okU = identiques(ser(st), avant);
		st.Retablir();
		const bool okR = identiques(ser(st), apres);
		printf("%s  %s (annuler %s avant, retablir %s apres)\n",
			   (okU && okR) ? "OK   " : "ECHEC", nom, okU ? "==" : "!=", okR ? "==" : "!=");
		if (!okU || !okR)
			++echecs;
	};

	// 1. TRACE : poser une forme (le geste de l'outil rectangle).
	avantGeste();
	int32 forme = st.doc.AddChild(0, "", NkAuthor::Humain);
	{
		NkUINode &n = st.doc.nodes[(uint32)forme];
		n.label = NkString("Forme recette");
		n.shape = NkString("rect");
		n.posX = 10.f;
		n.posY = 12.f;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = 120.f;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = 60.f;
		st.doc.MarkHumanEdit(forme);
	}
	apresGeste("trace d'une forme");
	// 2. DEPLACEMENT (le drag entier = un pas : ici sa fin).
	avantGeste();
	st.doc.nodes[(uint32)forme].posX += 25.f;
	st.doc.MarkHumanEdit(forme);
	apresGeste("deplacement");
	// 3. REDIMENSION.
	avantGeste();
	st.doc.nodes[(uint32)forme].width.value += 40.f;
	st.doc.MarkHumanEdit(forme);
	apresGeste("redimension");
	// 4. RENOMMAGE (l'etiquette = la cle `label`).
	avantGeste();
	st.doc.nodes[(uint32)forme].label = NkString("Renommee");
	st.doc.MarkHumanEdit(forme);
	apresGeste("renommage");
	// 5. EDITION DE TEXTE (la cle `texte`).
	avantGeste();
	st.doc.nodes[(uint32)forme].shape = NkString("text");
	st.doc.nodes[(uint32)forme].text = NkString("Bonjour recette");
	st.doc.MarkHumanEdit(forme);
	apresGeste("edition de texte");
	// 6. PROPRIETE D'INSPECTEUR (apparence : fond hexa).
	avantGeste();
	st.doc.nodes[(uint32)forme].fill = NkString("#12ab34");
	st.doc.MarkHumanEdit(forme);
	apresGeste("propriete d'inspecteur (fond)");
	// 7. ROLE (le geste « promouvoir »).
	avantGeste();
	st.doc.nodes[(uint32)forme].role = NkString("bouton");
	st.doc.MarkHumanEdit(forme);
	apresGeste("role");
	// 8. METRIQUE D'ESPACEMENT (section ESPACEMENT de l'Inspecteur).
	avantGeste();
	st.doc.SetMetric("gouttiere_recette", 14.f);
	st.doc.MarkHumanEdit(0);
	apresGeste("metrique d'espacement");
	// 9. VERSION MOBILE (la transposition — un sous-arbre entier en un pas).
	avantGeste();
	{
		NkVector<NkString> constats;
		const int32 m = st.doc.TransposerVersMobile(forme, constats);
		if (m >= 0)
			st.doc.MarkHumanEdit(m);
	}
	apresGeste("version mobile (transposition)");
	// 10. SUPPRESSION d'un sous-arbre (renumerotation comprise — l'instantane
	//     restaure exactement, indices et tout).
	avantGeste();
	st.doc.RemoveSubtree(forme, nullptr);
	apresGeste("suppression d'un sous-arbre");

	// 11. LES ONGLETS (multi-documents, 01/09) : L'ANNULATION NE TRAVERSE PAS.
	//     Geste sur A, bascule vers B, Annuler -> B ne bouge pas ; retour vers
	//     A -> il est reste modifie, et SON annulation defait SON geste.
	{
		st.ouverts.Clear();
		{
			DesignState::NkDocOuvert s0;
			st.ouverts.PushBack(s0);
			st.ongletActif = 0;
		}
		NkUIDocument db;
		NkBuildBlankDocument(db);
		db.title = NkString("Recette B");
		const int32 iB = st.OuvrirOngletInactif(db, "");
		const NkString aAvant = ser(st);
		st.doc.SetMetric("onglet_recette", 3.f);
		st.doc.MarkHumanEdit(0);
		stabiliser(st);
		++gestes;
		const NkString aApres = ser(st);
		st.BasculerVers((uint32)iB);
		const NkString bAvant = ser(st);
		st.Annuler(); // il n'y a RIEN a annuler dans B — et surtout pas le geste de A
		const bool okB = identiques(ser(st), bAvant);
		st.BasculerVers(0);
		const bool okA1 = identiques(ser(st), aApres); // A est revenu MODIFIE
		st.Annuler();
		const bool okA2 = identiques(ser(st), aAvant); // et SON Ctrl+Z defait SON geste
		printf("%s  onglets : l'annulation ne traverse pas (B %s, A garde %s, A annule %s)\n",
			   (okB && okA1 && okA2) ? "OK   " : "ECHEC", okB ? "intact" : "TOUCHE",
			   okA1 ? "==" : "!=", okA2 ? "==" : "!=");
		if (!okB || !okA1 || !okA2)
			++echecs;
	}

	// 12. LE MULTILINGUE (01/09) : declarer une langue + poser une traduction
	//     est UN geste annulable, et l'aller-retour par la serialisation prouve
	//     que les cles additives (`langues`, `texte_<langue>`) SURVIVENT a
	//     Save -> Load -> Save octet pour octet (l'annulation restaure par Load).
	avantGeste();
	{
		st.doc.langues.PushBack(NkString("en"));
		const int32 tx = st.doc.AddChild(0, "", NkAuthor::Humain);
		if (st.doc.IsValidIndex(tx)) {
			NkUINode &n = st.doc.nodes[(uint32)tx];
			n.label = NkString("Texte multilingue");
			n.shape = NkString("text");
			n.text = NkString("Bonjour");
			n.PoserTexte("en", "Hello");
			st.doc.MarkHumanEdit(tx);
		}
	}
	apresGeste("multilingue (langue declaree + traduction)");

	// LA CONTRE-EPREUVE : N gestes, N Annuler -> l'etat initial EXACT.
	for (int32 i = 0; i < gestes; ++i)
		st.Annuler();
	const bool okInitial = identiques(ser(st), initial);
	printf("%s  %d gestes puis %d Annuler -> etat initial octet pour octet\n",
		   okInitial ? "OK   " : "ECHEC", gestes, gestes);
	if (!okInitial)
		++echecs;
	// Et le retour : N Retablir -> le dernier etat.
	printf("RECETTE ANNULATION : %d/%d %s\n", gestes + 1 - echecs, gestes + 1,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
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

static void EcrireReleveUI(NkEditorFrameContext &ec, void *) {
	CalerLargeursDock(ec.Ui());
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
	bool dbl = false; ///< --clic=x:y:frame:d — injecte AUSSI un double-clic
} gClics[4];
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
static void InjecterClics(nkgui::NkGuiContext &ctx) {
	static int32 compteur = 0;
	++compteur;
	for (int32 i = 0; i < 4; ++i) {
		if (gClics[i].frame < 0)
			continue;
		// La souris TIENT la position a partir du clic (le harnais releve-menus
		// pilote pareil : plusieurs trames, pas une) — le popup survit au survol.
		// le survol se resout sur hotIdPrev (la trame d'AVANT) : la position
		// se tient CINQ trames avant le clic, sinon le clic vise un survol
		// pas encore etabli et manque.
		if (compteur >= gClics[i].frame - 5)
			ctx.input.mousePos = {gClics[i].x, gClics[i].y};
		if (compteur == gClics[i].frame) {
			ctx.input.mouseDown[0] = true;
			ctx.input.mouseClicked[0] = true;
			// le DOUBLE-CLIC s'injecte tel quel (la detection temporelle de la
			// fenetre ne verra jamais deux vrais clics) — c'est le levier de
			// preuve du FORAGE sous curseur.
			if (gClics[i].dbl)
				ctx.input.mouseDoubleClicked[0] = true;
		} else if (compteur == gClics[i].frame + 1) {
			// ⚠️ EFFACER le clic : si ce rappel tourne deux fois par trame, un
			//    clic qui persiste au second passage REFERME le menu qu'il vient
			//    d'ouvrir (double bascule) — mesure au releve : « survole,replie ».
			ctx.input.mouseClicked[0] = false;
			ctx.input.mouseDown[0] = false;
			ctx.input.mouseReleased[0] = true;
			ctx.input.mouseDoubleClicked[0] = false;
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
	for (int32 i = 0; i < 4; ++i) {
		if (gTouches[i].frame < 0)
			continue;
		if (compteur == gTouches[i].frame)
			ctx.input.keyInit[(int32)gTouches[i].touche] = true;
		else if (compteur == gTouches[i].frame + 1)
			ctx.input.keyInit[(int32)gTouches[i].touche] = false;
	}
}

static void DrawMenuBar(NkEditorFrameContext &ec, void *) {
	InjecterClics(ec.Ui());
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
		if (BeginMenu(ctx, "Exporter")) {
			MenuItem(ctx, "Document .nkgui", "Ctrl+E", false);
			MenuItem(ctx, "Ressources", nullptr, false);
			MenuItem(ctx, "Code", nullptr, false);
			EndMenu(ctx);
		}
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
		MenuItem(ctx, "Couper", "Ctrl+X", false);
		MenuItem(ctx, "Copier", "Ctrl+C", false);
		MenuItem(ctx, "Coller", "Ctrl+V", false);
		MenuItem(ctx, "Coller à la même place", "Ctrl+Maj+V", false);
		MenuItem(ctx, "Coller le style seul", "Ctrl+Alt+V", false);
		MenuItem(ctx, "Dupliquer", "Ctrl+D", false);
		MenuItem(ctx, "Supprimer", "Suppr", false);
		Separator(ctx);
		MenuItem(ctx, "Tout sélectionner", "Ctrl+A", false);
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
		MenuItem(ctx, "Magnétisme", "Ctrl+;", false, true);
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
		MenuItem(ctx, "Grouper", "Ctrl+G", false);
		MenuItem(ctx, "Dégrouper", "Ctrl+Maj+G", false);
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
		if (MenuItem(ctx, "Ouvrir le chat IA"))
			FocusPanel("IA");
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

static void DrawProjectTabs(NkEditorFrameContext &ec, void *) {
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

	for (uint32 i = 0; i < argCount; ++i) {
		const char *a = argv[i];
		if (!a)
			continue;
		if (NkComponentDecl::StrEq(a, "--probe"))
			return nkuidesign::RunProbe();
		// La preuve de recette du pipeline IA (Q31 [IA], branchement n.1) : sans
		// fenetre ni GPU, comme la sonde -- elle tourne sur la machine
		// d'integration.
		if (NkComponentDecl::StrEq(a, "--recette-ia"))
			return nkuidesign::RunRecetteIA();
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
				for (int32 ci = 0; ci < 4; ++ci)
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
						// 4e champ optionnel « d » : ce clic est un DOUBLE-clic
						// (--clic=x:y:frame:d — levier de preuve du forage).
						while (*q && *q != ':')
							++q;
						gClics[ci].dbl = (*q == ':' && q[1] == 'd');
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
			puts("  --annuler=N             N pas d'annulation au lancement (preuve UI)");
			puts("  --retablir=N            N pas de retablissement apres --annuler");
			puts("  --recette-ia            la preuve de recette du pipeline IA");
			puts("  --roundtrip[=<dossier>] l'aller-retour du format .nkgui");
			puts("  --roundtrip-controles   les temoins du lecteur/ecrivain");
			puts("  --pool-controles        les témoins du pool de chaînes");
			puts("  --valider[=<dossier>]   la validation par role et par type");
			puts("  --dump-ui               publier le relevé de l'interface dessinée");
			puts("  --releve-menus[=<fichier>] relever la barre de menus SANS fenêtre");
			puts("  --small                 fenêtre réduite (1024x640)");
			puts("  --theme=<nom>           thème au lancement (nom de NkThemeLibrary)");
			puts("  --selection=<n>         sélectionner le nœud n au premier affichage");
			puts("  --editer-texte=<n>      ouvrir l'édition en place sur le nœud texte n (mise en scène)");
			puts("  --frappe=texte:frame    injecter des codepoints ASCII à cette trame (preuve de saisie)");
			puts("  --touche=nom:frame      injecter entree|echap|retour à cette trame");
			puts("  --document=<chemin>     charger ce document au lancement (mise en scène)");
			puts("  --lignes=v<f>,h<px>     lignes de magnétisme figées (mise en scène)");
			puts("  --toile-seule           panneaux fermés, rails retirés (mise en scène)");
			puts("  --vue=x<px>,y<px>,z<f>  poser pan/zoom de la vue au lancement (mesure)");
			puts("  --tiroir=<c>:<n>        ouvrir un tiroir de rail (d/g/b, mise en scène)");
			puts("  --zone-sure             afficher la zone sûre des cadres Mobile");
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
	static nkuidesign::HierarchyPanel hierarchie(&gDesign);
	static nkuidesign::InspectorPanel inspecteur(&gDesign);
	// ⚠️ LA PALETTE REVIENT, MAIS PAR LE RAIL — ET CE N EST PAS UN RETOUR EN
	//    ARRIERE. Le §13.1 la place explicitement sur le rail GAUCHE, comme
	//    panneau SECONDAIRE : c est sa place, pas le dock. Elle est enregistree
	//    (le tiroir la retrouve par son titre) mais FERMEE : `DrawPanels` la
	//    saute, seul le tiroir la dessine. C est la difference entre debrancher
	//    un panneau et le ranger.
	static nkuidesign::SimulationPanel simulation(&gDesign);
	static nkuidesign::AmbiancesPanel ambiances(&gDesign);
	static nkuidesign::GreffonsPanel greffons(&gDesign);
	static nkuidesign::BibliothequePanel bibliotheque(&gDesign);
	bibliotheque.SetOpen(false); // vit dans le TIROIR du rail droit (ecran 9)
	static nkuidesign::PalettePanel palette(&gDesign);
	palette.SetOpen(false);
	if (gToileSeule) {
		// La toile seule : les deux panneaux fixes se FERMENT (ils restent
		// enregistres — Affichage les rouvre), les rails ne seront pas poses.
		hierarchie.SetOpen(false);
		inspecteur.SetOpen(false);
		ai.SetOpen(false);
	}
	// ⚠️ L ORDRE D AJOUT DECIDE DE L ORDRE DES ONGLETS dans une meme feuille de
	//    dock : Hierarchie d abord (elle est seule a gauche), puis le centre,
	//    puis l Inspecteur, puis le bas.
	shell->AddPanel(&hierarchie);
	shell->AddPanel(&preview);
	shell->AddPanel(&inspecteur);
	shell->AddPanel(&ai);
	shell->AddPanel(&palette);
	shell->AddPanel(&bibliotheque);
	shell->AddPanel(&simulation);
	shell->AddPanel(&ambiances);
	shell->AddPanel(&greffons);
	// Mise en scene : --panneau=<titre> ouvre un panneau ferme par defaut.
	if (gPanneauInitial[0]) {
		nkentseu::editorkit::NkEditorPanel *tous[3] = {&simulation, &ambiances, &greffons};
		for (int32 pi = 0; pi < 3; ++pi)
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

	// ── LE THEME : UNE SEULE AUTORITE, POUSSEE VERS LE DESSIN ────────────
	// ⚠️ SANS CET APPEL, LA MOITIE DE LA FENETRE NE SUIVRAIT PAS. La
	//    bibliotheque de l editeur porte les roles ; `NkGuiContext::theme` est ce
	//    que chaque primitive LIT. Poser les themes sans les pousser donne une
	//    bascule qui n emporte que ce qui passe par les roles -- et une bascule
	//    a moitie est pire qu une bascule absente, parce qu elle a l air de
	//    marcher. Le point unique est `NkEditorShell::ApplyTheme`.
	gThemes.AddBuiltins(); // Sombre, Clair, GitHub Dark Pro, GitHub Light Pro
	gShell = shell.Get(); // ⚠️ AVANT `AppliquerTheme`, qui s'en sert.
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
		char plein[176];
		snprintf(plein, sizeof(plein), "%s%s", modifie ? "\xE2\x97\x8F " : "", nom);
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
		{"Test", "Aperçu / Test — exécuter l'interface dessinée", "T",
		 [](nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool, bool, void *) {
			 nkuidesign::costume::IcOeilVague(ui.dl, r.x + (r.w - 14.f) * 0.5f,
											  r.y + (r.h - 14.f) * 0.5f, ui.theme.textMuted);
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
		{"Test", "Aperçu / Test — exécuter l'interface dessinée", "T",
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
	shell->SetRail(NkEditorDockSide::NK_RIGHT, kRailDroite, gToileSeule ? 0 : 3);
	shell->SetRail(NkEditorDockSide::NK_BOTTOM, kRailBas, gToileSeule ? 0 : 2);
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
	// Le titre initial vient du DOCUMENT (le callback `titre` prendra le
	// relais a la premiere mesure — meme regle : jamais un nom en dur).
	shell->SetTitleInfo(gDesign.doc.title.Data() ? gDesign.doc.title.Data() : "NkUIDesign");
	shell->RegisterCommand("Document: Enregistrer", &CmdSave, nullptr, "Ctrl+S");
	// L'annulation unifiée (§7) : Ctrl+Z / Ctrl+Y, et Ctrl+Maj+Z en seconde
	// orthographe du rétablir (le standard des trois éditeurs de référence).
	shell->RegisterCommand("Édition: Annuler", &CmdUndo, nullptr, "Ctrl+Z");
	shell->RegisterCommand("Édition: Rétablir", &CmdRedo, nullptr, "Ctrl+Y");
	shell->RegisterCommand("Édition: Rétablir (Maj)", &CmdRedo, nullptr, "Ctrl+Shift+Z");
	shell->RegisterCommand("Document: Recharger", &CmdLoad, nullptr, "Ctrl+R");
	shell->RegisterCommand("Document: Nouveau", &CmdNew, nullptr, "Ctrl+N");
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");
	gShell = shell.Get();
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

	return shell->Run();
}
