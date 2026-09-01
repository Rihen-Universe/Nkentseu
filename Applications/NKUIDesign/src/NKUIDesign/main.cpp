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
	// ⚠️ L'ANCRE — LE VOLET *CONSERVATION*, SANS LEQUEL CETTE RECETTE ETAIT
	//    VERTE SUR UN ECRIVAIN MORT. Mesure du 01/09 : `NkUIDocument::Save`
	//    neutralisee (elle rend du vide) -> **RECETTE ANNULATION : 13/13
	//    PROUVEE**. Les treize cas ne comparaient que DEUX SORTIES DU MEME
	//    PRODUCTEUR : deux chaines vides sont egales, donc tout passait, et la
	//    recette declarait l'annulation prouvee sur un format qui n'ecrivait
	//    plus rien.
	//    C'est la porte du depot : « stable » ne veut pas dire « juste ». Un
	//    aller-retour porte DEUX exigences — la stabilite ET la conservation.
	//    Ici la conservation tient en une ligne : la serialisation doit DIRE
	//    quelque chose (son en-tete, et au moins un noeud).
	auto ancree = [](const NkString &s) -> bool {
		if (s.Size() == 0 || !s.Data())
			return false;
		return s.Contains("nkuidoc") && s.Contains("noeud");
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
		// LA CONSERVATION D'ABORD : si les deux etats compares ne DISENT rien,
		// leur egalite ne prouve rien. On l'exige AVANT de la lire.
		const bool dit = ancree(avant) && ancree(apres);
		const uint32 nAvant = st.doc.NodeCount();
		st.Annuler();
		const bool okU = dit && identiques(ser(st), avant);
		st.Retablir();
		const bool okR = dit && identiques(ser(st), apres) && st.doc.NodeCount() == nAvant;
		printf("%s  %s (annuler %s avant, retablir %s apres%s)\n",
			   (okU && okR) ? "OK   " : "ECHEC", nom, okU ? "==" : "!=", okR ? "==" : "!=",
			   dit ? "" : ", SERIALISATION MUETTE");
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
	const bool okInitial = ancree(initial) && identiques(ser(st), initial);
	printf("%s  %d gestes puis %d Annuler -> etat initial octet pour octet (et cet etat "
		   "DIT quelque chose)\n",
		   okInitial ? "OK   " : "ECHEC", gestes, gestes);
	if (!okInitial)
		++echecs;
	// Et le retour : N Retablir -> le dernier etat.
	printf("RECETTE ANNULATION : %d/%d %s\n", gestes + 1 - echecs, gestes + 1,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}

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
static nkentseu::int32 RecetteGestes() {
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
	auto stabiliser = [&](DesignState &s) {
		for (int32 i = 0; i < 10; ++i) {
			NkString o;
			s.doc.Save(o);
			s.histoire.Observer(o);
		}
	};
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	static DesignState st; // static : l'etat est gros, pas sur la pile
	// La surface de disposition : la MEME que la toile appelle (espace
	// document). Grouper lit `layout` — sans elle il refuse, et il le dit.
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	auto poser = [&](int32 parent, const char *nom, float32 x, float32 y, float32 w,
					 float32 h) -> int32 {
		const int32 i = st.doc.AddChild(parent, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(nom);
		n.shape = NkString("rect");
		n.posX = x;
		n.posY = y;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = w;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = h;
		st.doc.MarkHumanEdit(i);
		return i;
	};
	auto compteLabel = [&](const char *nom) -> int32 {
		int32 n = 0;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i)
			if (st.doc.nodes[i].label.Data() && NkComponentDecl::StrEq(st.doc.nodes[i].label.Data(), nom))
				++n;
		return n;
	};

	// ── 1. COPIER + COLLER : le sous-arbre revient ENTIER, et une seule fois
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 pere = poser(0, "Pere", 10.f, 10.f, 200.f, 200.f);
		st.doc.nodes[(uint32)pere].layout.kind = NkLayoutKind::Free;
		poser(pere, "Fils", 5.f, 5.f, 40.f, 40.f);
		st.Recompute(surface);
		st.SelectSingle(pere);
		const uint32 nCopies = st.CopierSelection();
		st.CollerPressePapiers();
		st.Recompute(surface);
		char d[128];
		snprintf(d, sizeof(d), "copies=%u, Pere x%d, Fils x%d", nCopies, compteLabel("Pere"),
				 compteLabel("Fils"));
		verdict("copier/coller : le sous-arbre revient ENTIER (pere + fils)",
				nCopies == 1 && compteLabel("Pere") == 2 && compteLabel("Fils") == 2, d);
	}
	// ── 2. LE PIEGE DU DOUBLON : pere ET fils selectionnes -> le fils ne se
	//    colle PAS deux fois (RacinesSelection). Sans ce filtre : Fils x4.
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 pere = poser(0, "Pere", 10.f, 10.f, 200.f, 200.f);
		st.doc.nodes[(uint32)pere].layout.kind = NkLayoutKind::Free;
		const int32 fils = poser(pere, "Fils", 5.f, 5.f, 40.f, 40.f);
		st.Recompute(surface);
		st.sel.Set(pere);
		st.sel.Add(fils); // la selection COUVRANTE, celle qui piege
		st.selected = st.sel.Primary();
		st.CopierSelection();
		st.CollerPressePapiers();
		char d[96];
		snprintf(d, sizeof(d), "Pere x%d, Fils x%d (x4 = le doublon)", compteLabel("Pere"),
				 compteLabel("Fils"));
		verdict("copier pere+fils : le fils n'arrive PAS en double",
				compteLabel("Pere") == 2 && compteLabel("Fils") == 2, d);
	}
	// ── 2bis. LA COPIE NE PERD AUCUN CHAMP, et c'est mesure PAR LE FORMAT.
	//    ⚠️ C'EST LE DEFAUT QUE LES AUTRES CAS NE PEUVENT PAS VOIR. Compter des
	//    noeuds prouve qu'il y en a deux ; ca ne dit RIEN d'un `fontWeight` que
	//    `CopierSousArbre` aurait oublie de recopier — le collage aurait l'air
	//    juste et perdrait une propriete en silence, exactement le « collage qui
	//    perd » que l'avertissement de la methode annonce. On ecrit donc un
	//    noeud RICHE, on le colle, et on compare les deux SERIALISATIONS ligne a
	//    ligne : le juge vient du round-trip, pas du code teste (lecon T5).
	//    Ce cas echouera le jour ou un champ neuf de NkUINode oubliera la copie —
	//    c'est le rappel qu'on veut, et il est automatique.
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "Riche", 12.f, 34.f, 111.f, 22.f);
		{
			NkUINode &n = st.doc.nodes[(uint32)a];
			n.shape = NkString("text");
			n.text = NkString("Bonjour");
			n.fontPx = 19.f;
			n.fontWeight = 700;
			n.radius = 7.f;
			n.borderW = 3.f;
			n.role = NkString("titre");
			n.alignText = 2;
			st.doc.MarkHumanEdit(a);
		}
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CopierSelection();
		st.CollerPressePapiers();
		const int32 b = st.selected;
		// ⚠️ LE JUGE N'EMPLOIE PAS `CopierSousArbre`, ET C'EST TOUTE LA QUESTION.
		//    Isoler les deux noeuds AVEC la copie aurait ete un controle de
		//    SYMETRIE : un champ oublie par la copie serait oublie des DEUX
		//    cotes, les deux serialisations resteraient identiques, et le cas
		//    passerait au vert en couvrant precisement le defaut qu'il cherche.
		//    (Le depot a deja paye cette lecon — Q14, « un controle de symetrie
		//    ne peut pas voir une erreur symetrique ».) On isole donc par
		//    Save/Load/RemoveSubtree : trois mecanismes prouves ailleurs, et
		//    aucun d'eux n'est le code teste.
		bool ok = st.doc.IsValidIndex(b) && b != a;
		NkString sa, sb;
		if (ok) {
			st.doc.nodes[(uint32)b].posX = st.doc.nodes[(uint32)a].posX;
			st.doc.nodes[(uint32)b].posY = st.doc.nodes[(uint32)a].posY;
			NkString tout;
			st.doc.Save(tout);
			NkUIDocument da, db;
			ok = da.Load(tout.Data()) && db.Load(tout.Data());
			if (ok) {
				// `a` et `b` sont les deux seuls enfants de la racine : on garde
				// le premier d'un cote, le second de l'autre.
				ok = da.nodes[0].children.Size() == 2 && db.nodes[0].children.Size() == 2;
				if (ok) {
					ok = da.RemoveSubtree(da.nodes[0].children[1])
						 && db.RemoveSubtree(db.nodes[0].children[0]);
				}
			}
			if (ok) {
				da.Save(sa);
				db.Save(sb);
				ok = identiques(sa, sb);
			}
		}
		char d[96];
		snprintf(d, sizeof(d), "%u vs %u octets, %s", (unsigned)(sa.Data() ? strlen(sa.Data()) : 0),
				 (unsigned)(sb.Data() ? strlen(sb.Data()) : 0),
				 ok ? "serialisations IDENTIQUES" : "UN CHAMP MANQUE");
		verdict("la copie ne perd AUCUN champ (comparaison par le format)", ok, d);
	}
	// ── 3. COLLER DECALE de 10 px (sinon le collage disparait SOUS l'original)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "Boite", 40.f, 60.f, 80.f, 30.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CopierSelection();
		st.CollerPressePapiers();
		const int32 b = st.selected;
		const bool ok = st.doc.IsValidIndex(b) && b != a
						&& st.doc.nodes[(uint32)b].posX == st.doc.nodes[(uint32)a].posX + 10.f
						&& st.doc.nodes[(uint32)b].posY == st.doc.nodes[(uint32)a].posY + 10.f;
		char d[96];
		snprintf(d, sizeof(d), "original (%.0f,%.0f) -> colle (%.0f,%.0f)",
				 st.doc.nodes[(uint32)a].posX, st.doc.nodes[(uint32)a].posY,
				 st.doc.IsValidIndex(b) ? st.doc.nodes[(uint32)b].posX : -1.f,
				 st.doc.IsValidIndex(b) ? st.doc.nodes[(uint32)b].posY : -1.f);
		verdict("coller : decale de 10 px, et le colle devient la selection", ok, d);
	}
	// ── 4. COUPER : le presse-papiers est plein ET l'original est parti
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		poser(0, "Reste", 0.f, 0.f, 10.f, 10.f);
		const int32 a = poser(0, "Coupe", 40.f, 60.f, 80.f, 30.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CouperSelection();
		const bool parti = compteLabel("Coupe") == 0;
		st.CollerPressePapiers();
		char d[96];
		snprintf(d, sizeof(d), "apres couper : x%d ; apres coller : x%d", parti ? 0 : 1,
				 compteLabel("Coupe"));
		verdict("couper puis coller : l'original part, le contenu revient",
				parti && compteLabel("Coupe") == 1, d);
	}
	// ── 5. DUPLIQUER SUR PLACE (Ctrl+D) : meme parent, decale
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		const int32 a = poser(cadre, "Bouton", 20.f, 30.f, 100.f, 40.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.DupliquerSelection();
		const int32 b = st.selected;
		const bool ok = st.doc.IsValidIndex(b) && b != a
						&& st.doc.nodes[(uint32)b].parent == cadre && compteLabel("Bouton") == 2;
		char d[96];
		snprintf(d, sizeof(d), "Bouton x%d, meme parent=%s", compteLabel("Bouton"),
				 (st.doc.IsValidIndex(b) && st.doc.nodes[(uint32)b].parent == cadre) ? "oui" : "non");
		verdict("dupliquer : la copie nait dans le MEME parent", ok, d);
	}
	// ── 6. GROUPER PRESERVE LES POSITIONS A L'ECRAN, AU PIXEL
	//    (c'est LA propriete qui distingue un vrai Grouper d'un re-parentage)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		const int32 a = poser(cadre, "A", 20.f, 30.f, 60.f, 40.f);
		const int32 b = poser(cadre, "B", 150.f, 90.f, 60.f, 40.f);
		st.Recompute(surface);
		const NkPaintRect ra0 = st.layout.At(a), rb0 = st.layout.At(b);
		st.sel.Set(a);
		st.sel.Add(b);
		st.selected = st.sel.Primary();
		const bool fait = st.GrouperSelection();
		st.Recompute(surface);
		// apres groupement les index ne bougent pas (AddChild + Reparent), mais
		// on les RETROUVE par le libelle : ne jamais supposer une numerotation.
		int32 ia = -1, ib = -1, ig = -1;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i) {
			const char *l = st.doc.nodes[i].label.Data();
			if (!l)
				continue;
			if (NkComponentDecl::StrEq(l, "A"))
				ia = (int32)i;
			else if (NkComponentDecl::StrEq(l, "B"))
				ib = (int32)i;
			else if (NkComponentDecl::StrEq(l, "Groupe 1"))
				ig = (int32)i;
		}
		const bool dedans = ia >= 0 && ib >= 0 && ig >= 0 && st.doc.nodes[(uint32)ia].parent == ig
							&& st.doc.nodes[(uint32)ib].parent == ig;
		const NkPaintRect ra1 = (ia >= 0 && st.layout.Has(ia)) ? st.layout.At(ia) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const NkPaintRect rb1 = (ib >= 0 && st.layout.Has(ib)) ? st.layout.At(ib) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const bool memePlace = ra1.x == ra0.x && ra1.y == ra0.y && rb1.x == rb0.x && rb1.y == rb0.y;
		char d[160];
		snprintf(d, sizeof(d), "A (%.0f,%.0f)->(%.0f,%.0f), B (%.0f,%.0f)->(%.0f,%.0f)", ra0.x,
				 ra0.y, ra1.x, ra1.y, rb0.x, rb0.y, rb1.x, rb1.y);
		verdict("grouper : les positions A L'ECRAN ne bougent pas d'un pixel",
				fait && dedans && memePlace, d);
		// ── 7. DEGROUPER remet tout en place, au pixel aussi
		st.SelectSingle(ig);
		const bool fait2 = st.DegrouperSelection();
		st.Recompute(surface);
		int32 ja = -1, jb = -1;
		bool groupeParti = true;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i) {
			const char *l = st.doc.nodes[i].label.Data();
			if (!l)
				continue;
			if (NkComponentDecl::StrEq(l, "A"))
				ja = (int32)i;
			else if (NkComponentDecl::StrEq(l, "B"))
				jb = (int32)i;
			else if (NkComponentDecl::StrEq(l, "Groupe 1"))
				groupeParti = false;
		}
		const NkPaintRect ra2 = (ja >= 0 && st.layout.Has(ja)) ? st.layout.At(ja) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const NkPaintRect rb2 = (jb >= 0 && st.layout.Has(jb)) ? st.layout.At(jb) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		char d2[160];
		snprintf(d2, sizeof(d2), "groupe parti=%s, A (%.0f,%.0f), B (%.0f,%.0f)",
				 groupeParti ? "oui" : "non", ra2.x, ra2.y, rb2.x, rb2.y);
		verdict("degrouper : le groupe part, les positions ecran tiennent",
				fait2 && groupeParti && ra2.x == ra0.x && ra2.y == ra0.y && rb2.x == rb0.x
					&& rb2.y == rb0.y,
				d2);
	}
	// ── 8. SUPPRIMER UNE MULTI-SELECTION : les DEUX partent (le piege est la
	//    renumerotation — supprimer le premier perime l'index du second)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "A", 0.f, 0.f, 10.f, 10.f);
		poser(0, "Garde", 20.f, 0.f, 10.f, 10.f);
		const int32 c = poser(0, "C", 40.f, 0.f, 10.f, 10.f);
		st.Recompute(surface);
		st.sel.Set(a);
		st.sel.Add(c);
		st.selected = st.sel.Primary();
		st.SupprimerSelection();
		char d[96];
		snprintf(d, sizeof(d), "A x%d, C x%d, Garde x%d", compteLabel("A"), compteLabel("C"),
				 compteLabel("Garde"));
		verdict("supprimer une multi-selection : les deux partent, le voisin reste",
				compteLabel("A") == 0 && compteLabel("C") == 0 && compteLabel("Garde") == 1, d);
	}
	// ── 9. TOUT SELECTIONNER au niveau du FORAGE (Lunacy), pas tout le document
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		poser(cadre, "E1", 0.f, 0.f, 10.f, 10.f);
		poser(cadre, "E2", 20.f, 0.f, 10.f, 10.f);
		poser(cadre, "E3", 40.f, 0.f, 10.f, 10.f);
		st.Recompute(surface);
		st.forageToile = -1;
		const uint32 n0 = st.ToutSelectionner(); // premier niveau : le cadre seul
		st.forageToile = cadre;
		const uint32 n1 = st.ToutSelectionner(); // dans le cadre : les trois
		char d[96];
		snprintf(d, sizeof(d), "premier niveau=%u, dans le cadre=%u", n0, n1);
		verdict("tout selectionner : au NIVEAU du forage, pas tout le document",
				n0 == 1 && n1 == 3, d);
	}
	// ── 10 a 13. UN GESTE = UN PAS D'ANNULATION (la mesure qui manquait)
	//    Un seul Annuler doit suffire a chaque geste : c'est ce que
	//    --recette-annulation ne pouvait pas dire.
	{
		struct Cas {
				const char *nom;
				int32 quoi; // 0 coller, 1 couper, 2 dupliquer, 3 grouper
		};
		const Cas cas4[4] = {{"coller", 0}, {"couper", 1}, {"dupliquer", 2}, {"grouper", 3}};
		for (int32 k = 0; k < 4; ++k) {
			st.doc.NewDocument("recette gestes", NkAuthor::Humain);
			const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
			st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
			const int32 a = poser(cadre, "A", 20.f, 30.f, 60.f, 40.f);
			const int32 b = poser(cadre, "B", 150.f, 90.f, 60.f, 40.f);
			st.Recompute(surface);
			st.histoire = NkHistorique();
			stabiliser(st);
			const NkString avant = ser(st);
			switch (cas4[k].quoi) {
				case 0:
					st.SelectSingle(a);
					st.CopierSelection();
					st.CollerPressePapiers();
					break;
				case 1:
					st.SelectSingle(a);
					st.CouperSelection();
					break;
				case 2:
					st.SelectSingle(a);
					st.DupliquerSelection();
					break;
				default:
					st.sel.Set(a);
					st.sel.Add(b);
					st.selected = st.sel.Primary();
					st.GrouperSelection();
					break;
			}
			st.Recompute(surface);
			stabiliser(st);
			const bool aChange = !identiques(ser(st), avant);
			st.Annuler(); // UN SEUL
			const bool revenu = identiques(ser(st), avant);
			char d[96];
			snprintf(d, sizeof(d), "le geste ecrit=%s, UN Annuler suffit=%s",
					 aChange ? "oui" : "non", revenu ? "oui" : "non");
			char nom[96];
			snprintf(nom, sizeof(nom), "%s : UN SEUL pas d'annulation", cas4[k].nom);
			verdict(nom, aChange && revenu, d);
		}
	}
	// ── LE PLUS PROCHE ANCETRE COMMUN (groupement, 01/09) ──────────────────
	//     ⚠️ TROIS CAS DANS UN SEUL BANC, PARCE QU'ILS SE TIENNENT : le bon
	//     niveau, le refus du raccourci « tout a la racine », et la protection
	//     contre le cycle. Separes, le troisieme aurait ete « evident » et donc
	//     jamais ecrit -- or c'est celui qui fige l'application.
	{
		static NkUIDocument d;
		d.NewDocument("ancetre", NkAuthor::Humain);
		//        0
		//        +-- 1 page
		//            +-- 2 carte A      +-- 4 carte B
		//                +-- 3 texte        +-- 5 icone
		const int32 page = d.AddChild(0, "", NkAuthor::Humain);
		const int32 cA = d.AddChild(page, "", NkAuthor::Humain);
		const int32 tA = d.AddChild(cA, "", NkAuthor::Humain);
		const int32 cB = d.AddChild(page, "", NkAuthor::Humain);
		const int32 iB = d.AddChild(cB, "", NkAuthor::Humain);
		// (a) LE BON NIVEAU : deux petits-enfants de la page -> LA PAGE, pas la
		//     racine. C'est tout le retour : grouper deux elements d'une meme
		//     page ne doit PAS les sortir de cette page.
		const int32 deux[2] = {tA, iB};
		const int32 lca1 = NkAncetreCommun(d, deux, 2u);
		// (b) MEME PARENT : le calcul general doit redonner l'ancien resultat,
		//     sinon on aurait « generalise » en cassant le cas qui marchait.
		const int32 memeParent[2] = {cA, cB};
		const int32 lca2 = NkAncetreCommun(d, memeParent, 2u);
		// (c) UN SEUL element : son propre parent.
		const int32 seul[1] = {tA};
		const int32 lca3 = NkAncetreCommun(d, seul, 1u);
		// (d) LE CYCLE : un noeud avec son propre descendant -> on MONTE d'un
		//     cran, jamais « dans lui-meme ».
		const int32 cycle[2] = {cA, tA};
		const int32 lca4 = NkAncetreCommun(d, cycle, 2u);
		char det[176];
		snprintf(det, sizeof(det),
				 "petits-enfants->%d (page=%d) ; meme parent->%d ; seul->%d ; noeud+descendant->%d "
				 "(pas %d)",
				 lca1, page, lca2, lca3, lca4, cA);
		verdict("le groupe nait au PLUS PROCHE ancetre commun (pas a la racine), le cas "
				"« meme parent » est preserve, et un noeud + son descendant remonte d'un cran "
				"au lieu de se mettre dans lui-meme",
				lca1 == page && lca2 == page && lca3 == cA && lca4 == page && lca4 != cA, det);
	}
	printf("\nRECETTE GESTES : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}

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
static nkentseu::int32 RecetteSelection() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	auto ancree = [](const NkString &s) -> bool {
		return s.Size() > 0 && s.Data() && s.Contains("nkuidoc") && s.Contains("noeud");
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	int32 cadre = -1, a = -1, b = -1, c = -1, profond = -1;
	auto scene = [&]() {
		st.doc.NewDocument("recette selection", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &f = st.doc.nodes[(uint32)cadre];
			f.label = NkString("Page");
			f.shape = NkString("frame");
			f.layout.kind = NkLayoutKind::Free;
			f.width.mode = NkSizeMode::Fixed;
			f.width.value = 600.f;
			f.height.mode = NkSizeMode::Fixed;
			f.height.value = 400.f;
		}
		auto poser = [&](int32 parent, const char *nom, float32 x, float32 y, float32 w,
						 float32 h) {
			const int32 i = st.doc.AddChild(parent, "", NkAuthor::Humain);
			NkUINode &n = st.doc.nodes[(uint32)i];
			n.label = NkString(nom);
			n.shape = NkString("rect");
			n.layout.kind = NkLayoutKind::Free;
			n.posX = x;
			n.posY = y;
			n.width.mode = NkSizeMode::Fixed;
			n.width.value = w;
			n.height.mode = NkSizeMode::Fixed;
			n.height.value = h;
			return i;
		};
		a = poser(cadre, "A", 20.f, 20.f, 100.f, 80.f);
		b = poser(cadre, "B", 200.f, 150.f, 60.f, 40.f);
		c = poser(cadre, "C", 400.f, 300.f, 50.f, 50.f);
		profond = poser(a, "Profond", 10.f, 10.f, 30.f, 20.f);
		st.Recompute(surface);
		st.SelectClear();
	};

	// ── 1. LES DEUX TABLES, ET ELLES SONT DIFFERENTES A DESSEIN ─────────────
	{
		const bool okToile = NkGesteToile(false, false) == NkGesteSel::Remplacer
							 && NkGesteToile(true, false) == NkGesteSel::Profond
							 && NkGesteToile(false, true) == NkGesteSel::Basculer;
		const bool okListe = NkGesteListe(false, false) == NkGesteSel::Remplacer
							 && NkGesteListe(true, false) == NkGesteSel::Basculer
							 && NkGesteListe(false, true) == NkGesteSel::Basculer;
		// LE PIEGE 2, TENU EXPLICITEMENT : les deux tables ne doivent PAS etre
		// identiques -- si un jour quelqu'un « harmonise », ce cas le dit.
		const bool differentes = NkGesteToile(true, false) != NkGesteListe(true, false);
		verdict("1. les deux tables : toile (Ctrl=profond, Maj=basculer) et liste "
				"(Ctrl ou Maj=basculer), et elles DIFFERENT a dessein",
				okToile && okListe && differentes,
				differentes ? "Ctrl : toile=profond, liste=basculer" : "TABLES CONFONDUES");
	}
	// ── 2. LA RACINE NE S'INVITE JAMAIS (piege 1) ───────────────────────────
	scene();
	{
		st.sel.Set(0); // l'etat de demarrage : la racine est « selectionnee »
		st.selected = 0;
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Basculer);
		const bool ok = st.sel.Count() == 1 && st.sel.Primary() == a && !st.sel.Contains(0);
		char d[96];
		snprintf(d, sizeof(d), "compte=%u, principal=%d (racine %s)", st.sel.Count(),
				 st.sel.Primary(), st.sel.Contains(0) ? "PRESENTE" : "absente");
		verdict("2. Maj+clic depuis l'etat de demarrage : la RACINE ne rentre pas dans la "
				"selection",
				ok, d);
		// et on ne peut pas l'y forcer
		NkAppliquerGeste(st.doc, st.sel, st.selected, 0, NkGesteSel::Basculer);
		verdict("2b. la racine refuse meme un geste qui la vise explicitement",
				!st.sel.Contains(0) && st.sel.Count() == 1, "");
	}
	// ── 3. BASCULER AJOUTE PUIS RETIRE ──────────────────────────────────────
	scene();
	{
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Remplacer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Basculer);
		const uint32 deux = st.sel.Count();
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Basculer);
		const uint32 un = st.sel.Count();
		NkAppliquerGeste(st.doc, st.sel, st.selected, c, NkGesteSel::Remplacer);
		const uint32 remplace = st.sel.Count();
		char d[96];
		snprintf(d, sizeof(d), "ajout=%u, retrait=%u, remplacement=%u", deux, un, remplace);
		verdict("3. Basculer ajoute puis RETIRE ; Remplacer ecrase (1, pas 3)",
				deux == 2 && un == 1 && remplace == 1 && st.sel.Primary() == c, d);
	}
	// ── 4. LE PRINCIPAL EST LE PREMIER DESIGNE (l'Inspecteur le nomme) ──────
	scene();
	{
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Remplacer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Basculer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, c, NkGesteSel::Basculer);
		char d[96];
		snprintf(d, sizeof(d), "principal=%d (B=%d), compte=%u", st.selected, b,
				 st.sel.Count());
		verdict("4. le PRINCIPAL reste le premier designe, pas le dernier",
				st.selected == b && st.sel.Count() == 3, d);
	}
	// ── 5. L'ENGLOBANT (piege 3 : un seul calcul, trois consommateurs) ──────
	scene();
	{
		st.sel.Set(a);
		st.sel.Add(b);
		st.selected = st.sel.Primary();
		NkPaintRect eng = {0.f, 0.f, 0.f, 0.f};
		const bool ok = NkRectSelection(st.doc, st.layout, st.sel, eng);
		const NkPaintRect ra = st.layout.At(a), rb = st.layout.At(b);
		const float32 x0 = ra.x < rb.x ? ra.x : rb.x;
		const float32 y0 = ra.y < rb.y ? ra.y : rb.y;
		const float32 x1 = (ra.x + ra.w) > (rb.x + rb.w) ? (ra.x + ra.w) : (rb.x + rb.w);
		const float32 y1 = (ra.y + ra.h) > (rb.y + rb.h) ? (ra.y + ra.h) : (rb.y + rb.h);
		char d[128];
		snprintf(d, sizeof(d), "englobant (%.0f,%.0f %.0fx%.0f), attendu (%.0f,%.0f %.0fx%.0f)",
				 eng.x, eng.y, eng.w, eng.h, x0, y0, x1 - x0, y1 - y0);
		verdict("5. l'englobant de la multi-selection couvre EXACTEMENT les deux",
				ok && eng.x == x0 && eng.y == y0 && eng.w == x1 - x0 && eng.h == y1 - y0, d);
	}
	// ── 5b. L'ENGLOBANT IGNORE LA RACINE (elle couvre tout : elle noierait tout)
	scene();
	{
		st.sel.Set(0);
		st.sel.Add(b);
		NkPaintRect eng = {0.f, 0.f, 0.f, 0.f};
		const bool ok = NkRectSelection(st.doc, st.layout, st.sel, eng);
		const NkPaintRect rb = st.layout.At(b);
		char d[112];
		snprintf(d, sizeof(d), "englobant (%.0f,%.0f %.0fx%.0f), B (%.0f,%.0f %.0fx%.0f)",
				 eng.x, eng.y, eng.w, eng.h, rb.x, rb.y, rb.w, rb.h);
		verdict("5b. l'englobant IGNORE la racine : sinon elle couvrirait toute la page et "
				"la boite ne dirait plus rien",
				ok && eng.w == rb.w && eng.h == rb.h, d);
	}
	// ── 6. LES VALEURS MIXTES : commune quand elle l'est, « — » sinon ───────
	scene();
	{
		st.sel.Set(a);
		st.sel.Add(b);
		float32 v = -1.f;
		// largeurs differentes -> MIXTE
		const bool mixte = !NkValeurCommune(st.doc, st.sel,
											[](const NkUINode &n) { return n.width.value; }, v);
		// on les egalise -> COMMUNE
		st.doc.nodes[(uint32)b].width.value = st.doc.nodes[(uint32)a].width.value;
		float32 w = -1.f;
		const bool commune = NkValeurCommune(st.doc, st.sel,
											 [](const NkUINode &n) { return n.width.value; }, w);
		char d[112];
		snprintf(d, sizeof(d), "largeurs differentes -> %s ; egalisees -> %s (%.0f)",
				 mixte ? "MIXTE" : "commune", commune ? "commune" : "mixte", w);
		verdict("6. valeur commune / valeur MIXTE : l'Inspecteur saura quand ecrire un tiret",
				mixte && commune && w == 100.f, d);
	}
	// ── 7. LE RECTANGLE DE SELECTION PREND CE QU'IL TOUCHE (Lunacy) ─────────
	scene();
	{
		// une zone qui EFFLEURE A par son coin bas-droit, sans le contenir
		const NkPaintRect ra = st.layout.At(a);
		const NkPaintRect zone = {ra.x + ra.w - 5.f, ra.y + ra.h - 5.f, 60.f, 60.f};
		st.sel.Clear();
		NkPickInRect(st.doc, st.layout, zone, st.sel);
		const bool prisA = st.sel.Contains(a);
		// et une zone franchement a cote ne prend rien
		NkSelection loin;
		NkPickInRect(st.doc, st.layout, {5000.f, 5000.f, 10.f, 10.f}, loin);
		char d[112];
		snprintf(d, sizeof(d), "effleure A -> %s ; loin de tout -> %u element(s)",
				 prisA ? "PRIS" : "rate", loin.Count());
		verdict("7. le rectangle prend ce qu'il TOUCHE (pas ce qu'il contient), et rien "
				"quand il est loin",
				prisA && loin.Count() == 0, d);
	}
	// ── 8. VOLET CONSERVATION : selectionner n'ECRIT RIEN dans le document ──
	scene();
	{
		NkString avant;
		st.doc.Save(avant);
		st.sel.Set(a);
		st.sel.Add(b);
		st.sel.Toggle(c);
		st.sel.Toggle(c);
		NkAppliquerGeste(st.doc, st.sel, st.selected, profond, NkGesteSel::Profond);
		NkPaintRect eng;
		(void)NkRectSelection(st.doc, st.layout, st.sel, eng);
		float32 v = 0.f;
		(void)NkValeurCommune(st.doc, st.sel, [](const NkUINode &n) { return n.posX; }, v);
		NkString apres;
		st.doc.Save(apres);
		const bool dit = ancree(avant) && ancree(apres);
		char d[112];
		snprintf(d, sizeof(d), "document %s%s",
				 (avant.Compare(apres) == 0) ? "INCHANGE" : "MODIFIE",
				 dit ? "" : ", SERIALISATION MUETTE");
		verdict("8. CONSERVATION : selectionner, basculer, mesurer l'englobant et lire les "
				"valeurs n'ecrit RIEN dans le document",
				dit && avant.Compare(apres) == 0, d);
	}
	printf("\nRECETTE SELECTION : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}

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
static nkentseu::int32 RecettePoints() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	auto ancree = [](const NkString &s) -> bool {
		return s.Size() > 0 && s.Data() && s.Contains("nkuidoc") && s.Contains("noeud");
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	auto poser = [&](const char *forme, const char *texte) -> int32 {
		const int32 i = st.doc.AddChild(0, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(forme);
		n.shape = NkString(forme);
		if (texte)
			n.text = NkString(texte);
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = 100.f;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = 80.f;
		return i;
	};

	// ── 1. LA TABLE DU DOUBLE-CLIC, EN ENTIER (les deux issues freres) ──────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iTexte = poser("text", "bonjour");
		const int32 iRect = poser("rect", nullptr);
		const int32 iLigne = poser("line", nullptr);
		const int32 iEtoile = poser("etoile", nullptr);
		const int32 iRectTexte = poser("rect", "j'ai du texte");
		const int32 iGroupe = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iGroupe].label = NkString("Groupe");
		st.doc.AddChild(iGroupe, "", NkAuthor::Humain);
		auto issue = [&](int32 i) { return NkIssueDeDblClic(st.doc.nodes[(uint32)i]); };
		const int32 iEllipse = poser("ellipse", nullptr);
		const int32 iImage = poser("image", nullptr);
		// ⚠️ `rect` ET `ellipse` SONT PASSES DE `CoinsSeuls` A `ModePoints` LE
		//    01/09 -- ce cas a ete RETOURNE, pas contourne. Retour de Rodolf :
		//    « double-cliquer sur une shape ne permet pas encore de faire
		//    l'edition de la shape. » La mesure lui a donne raison sur les deux
		//    formes qu'on pose le plus, et sa reference (`lunacy_shape_edit_1`)
		//    montre un RECTANGLE en `EDIT SHAPE` a quatre ancres.
		// ⚠️ ET `image` RESTE `CoinsSeuls` DANS LE MEME CAS, deliberement : sans
		//    ce temoin, on ne saurait pas si la branche `CoinsSeuls` existe
		//    encore ou si elle est devenue du code mort qu'aucun banc n'atteint.
		const bool ok = issue(iTexte) == NkIssueDblClic::EditerTexte
						&& issue(iRect) == NkIssueDblClic::ModePoints
						&& issue(iEllipse) == NkIssueDblClic::ModePoints
						&& issue(iImage) == NkIssueDblClic::CoinsSeuls
						&& issue(iLigne) == NkIssueDblClic::ModePoints
						&& issue(iEtoile) == NkIssueDblClic::ModePoints
						// un rect QUI PORTE du texte s'edite : la regle du 4e
						// retour, deplacee dans la table sans etre perdue
						&& issue(iRectTexte) == NkIssueDblClic::EditerTexte
						// et le FORAGE prime sur tout : un groupe se traverse
						&& issue(iGroupe) == NkIssueDblClic::Forer;
		verdict("1. la table du double-clic : texte->editer, rect ET ellipse->points, "
				"image->coins (temoin), ligne et etoile->points, rect AVEC texte->editer, "
				"groupe->forer",
				ok, ok ? "les huit issues" : "UNE ISSUE A CHANGE");
	}
	// ── 2. LA NATURE DES SOMMETS, forme par forme ──────────────────────────
	{
		// ⚠️ ET LE PREDICAT `NkSommetsStockes` EST VERIFIE ICI, pas seulement les
		//    natures : c'est LUI que six sites appellent pour decider s'ils
		//    ecrivent dans `n.sommets`. Une nature juste et un predicat faux
		//    donneraient une table exacte et une edition qui ne marche que sur
		//    trois formes sur cinq -- le defaut d'origine, deplace d'un cran.
		const bool ok = NkNatureDe("line") == NkNatureSommets::Bouts
						&& NkNatureDe("line_up") == NkNatureSommets::Bouts
						&& NkNatureDe("triangle") == NkNatureSommets::Polygone
						&& NkNatureDe("pentagone") == NkNatureSommets::Polygone
						&& NkNatureDe("etoile") == NkNatureSommets::Polygone
						&& NkNatureDe("rect") == NkNatureSommets::CoinsEditables
						&& NkNatureDe("ellipse") == NkNatureSommets::CoinsEditables
						&& NkNatureDe("image") == NkNatureSommets::Coins
						&& NkNatureDe("frame") == NkNatureSommets::Coins
						&& NkNatureDe("text") == NkNatureSommets::Aucun
						&& NkSommetsStockes(NkNatureSommets::Polygone)
						&& NkSommetsStockes(NkNatureSommets::CoinsEditables)
						&& !NkSommetsStockes(NkNatureSommets::Coins)
						&& !NkSommetsStockes(NkNatureSommets::Bouts)
						&& !NkSommetsStockes(NkNatureSommets::Aucun);
		verdict("2. la nature des sommets : bouts / polygone / coins EDITABLES (rect, "
				"ellipse) / coins (image, frame) / aucun -- et le predicat de stockage "
				"les suit",
				ok, "");
	}
	// ── 3. LE COMPTE DES SOMMETS, et il vient de LA table ──────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iL = poser("line", nullptr);
		const int32 iT = poser("triangle", nullptr);
		const int32 iP = poser("pentagone", nullptr);
		const int32 iE = poser("etoile", nullptr);
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iX = poser("text", "x");
		st.Recompute(surface);
		float32 xy[64];
		auto nb = [&](int32 i) {
			return NkSommetsDe(st.doc.nodes[(uint32)i], st.layout.At(i), xy, 32);
		};
		char d[144];
		snprintf(d, sizeof(d), "ligne=%u, triangle=%u, pentagone=%u, etoile=%u, rect=%u, "
							   "ellipse=%u, texte=%u",
				 nb(iL), nb(iT), nb(iP), nb(iE), nb(iR), nb(iEl), nb(iX));
		verdict("3. le compte des sommets : 2 / 3 / 5 / 10 / 4 (coins du rect) / 12 (courbe "
				"de l'ellipse) / 0",
				nb(iL) == 2 && nb(iT) == 3 && nb(iP) == 5 && nb(iE) == 10 && nb(iR) == 4
					&& nb(iEl) == 12 && nb(iX) == 0,
				d);
	}
	// ── 4. LES SOMMETS TOMBENT DANS LA BOITE, ET LA LIGNE SUR SA DIAGONALE ──
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iL = poser("line", nullptr);
		const int32 iLu = poser("line_up", nullptr);
		st.Recompute(surface);
		float32 xy[8], xu[8];
		const uint32 n1 = NkSommetsDe(st.doc.nodes[(uint32)iL], st.layout.At(iL), xy, 4);
		const uint32 n2 = NkSommetsDe(st.doc.nodes[(uint32)iLu], st.layout.At(iLu), xu, 4);
		const NkPaintRect r1 = st.layout.At(iL), r2 = st.layout.At(iLu);
		// `line` descend : (x,y) -> (x+w, y+h) ; `line_up` monte : l'inverse.
		const bool descend = n1 == 2 && xy[0] == r1.x && xy[1] == r1.y
							 && xy[2] == r1.x + r1.w && xy[3] == r1.y + r1.h;
		const bool monte = n2 == 2 && xu[1] == r2.y + r2.h && xu[3] == r2.y;
		char d[112];
		snprintf(d, sizeof(d), "line (%.0f,%.0f)->(%.0f,%.0f) ; line_up y %.0f->%.0f", xy[0],
				 xy[1], xy[2], xy[3], xu[1], xu[3]);
		verdict("4. les bouts d'une ligne SONT la diagonale de sa boite, et `line_up` "
				"monte",
				descend && monte, d);
	}
	// ── 5. MATERIALISER : la table devient une liste, et RIEN NE BOUGE ──────
	//     ⚠️ C'est le cas qui tient la promesse : materialiser ne doit pas
	//     DEPLACER un sommet d'un pixel, sinon l'etoile sauterait au premier
	//     clic dans le mode points, avant meme qu'on tire quoi que ce soit.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		st.Recompute(surface);
		float32 avant[64], apres[64];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), avant, 32);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), apres, 32);
		bool memes = (nA == nB && nA == 10);
		for (uint32 i = 0; memes && i < nA * 2; ++i)
			if (avant[i] != apres[i])
				memes = false;
		// et c'est idempotent
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		const bool idem = st.doc.nodes[(uint32)iE].sommets.Size() == 10;
		char d[112];
		snprintf(d, sizeof(d), "%u -> %u sommets, positions %s, 2e appel : %u", nA, nB,
				 memes ? "IDENTIQUES" : "DEPLACEES",
				 (uint32)st.doc.nodes[(uint32)iE].sommets.Size());
		verdict("5. materialiser une etoile ne DEPLACE aucun sommet, et n'est pas cumulatif",
				memes && idem, d);
	}
	// ── 6. MATERIALISER NE VAUT QUE POUR LES POLYGONES ──────────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iIm = poser("image", nullptr);
		const int32 iL = poser("line", nullptr);
		st.Recompute(surface);
		// ⚠️ CE CAS A CHANGE DE VERDICT LE 01/09, ET IL DIT MAINTENANT LA VRAIE
		//    LIGNE DE PARTAGE. Il affirmait « le rectangle ne recoit pas de
		//    sommets » ; c'etait la consequence de la table d'hier, pas une
		//    verite du modele. La ligne de partage n'est pas rect/polygone,
		//    c'est : la LIGNE s'exprime par sa boite (deux facons de dire la
		//    meme chose = une de trop), l'IMAGE et le CADRE ne s'editent pas
		//    (une cible d'artboard deformee en quadrilatere ne veut rien dire),
		//    tout le reste STOCKE ses sommets.
		// ⚠️ ET MATERIALISER NE DOIT DEPLACER AUCUN COIN : c'est la promesse que
		//    le cas 5 tient pour l'etoile, portee ici au rectangle -- sinon le
		//    rect sauterait au premier clic dans le mode points, avant meme
		//    qu'on tire quoi que ce soit. Le meme piege, sur le chemin frere.
		float32 avant[16], apres[16];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iR], st.layout.At(iR), avant, 8);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iEl]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iIm]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iL]);
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iR], st.layout.At(iR), apres, 8);
		bool memes = (nA == nB && nA == 4);
		for (uint32 i = 0; memes && i < nA * 2; ++i)
			if (avant[i] != apres[i])
				memes = false;
		// ⚠️ L'ELLIPSE EST PASSEE DE 4 A 12 LE 01/09, et ce cas le dit au lieu
		//    de le subir : ses quatre « coins » ne touchaient la courbe NULLE
		//    PART (retour 1 de Rodolf). Le rect garde ses 4 -- ses coins SONT
		//    son contour. Le compte differe donc par forme, et c'est voulu.
		const bool ok = st.doc.nodes[(uint32)iR].sommets.Size() == 4
						&& st.doc.nodes[(uint32)iEl].sommets.Size() == 12
						&& st.doc.nodes[(uint32)iIm].sommets.Empty()
						&& st.doc.nodes[(uint32)iL].sommets.Empty() && memes;
		char d[128];
		snprintf(d, sizeof(d), "rect=%u, ellipse=%u, image=%u, ligne=%u | coins %s",
				 (uint32)st.doc.nodes[(uint32)iR].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iEl].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iIm].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iL].sommets.Size(),
				 memes ? "IDENTIQUES" : "DEPLACES");
		verdict("6. le rect recoit ses 4 COINS et l'ellipse ses 12 points DE COURBE, sans "
				"qu'aucun bouge ; l'image et le cadre n'en recoivent pas (rien a deformer), "
				"la ligne non plus (elle EST sa boite)",
				ok, d);
	}
	// ── 7. ALLER-RETOUR + CONSERVATION : le document d'avant ne bouge pas ───
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkString a1, a2;
		st.doc.Save(a1);
		NkUIDocument r1;
		const bool lu1 = r1.Load(a1.Data());
		if (lu1)
			r1.Save(a2);
		bool sansCle = true;
		for (const char *q = a1.Data() ? a1.Data() : ""; *q; ++q)
			if (q[0] == 's' && q[1] == 'o' && q[2] == 'm' && q[3] == 'm' && q[4] == 'e'
				&& q[5] == 't' && q[6] == '_') {
				sansCle = false;
				break;
			}
		// conservation : la forme est encore la
		const bool garde = lu1 && r1.IsValidIndex(iE)
						   && StrEq(r1.nodes[(uint32)iE].shape.Data(), "etoile")
						   && r1.nodes[(uint32)iE].sommets.Empty();
		char d[128];
		snprintf(d, sizeof(d), "octets %s, cle `sommet_` %s, forme %s",
				 (lu1 && a1.Compare(a2) == 0) ? "IDENTIQUES" : "DIFFERENTS",
				 sansCle ? "absente" : "APPARUE", garde ? "gardee" : "PERDUE");
		verdict("7. une etoile REGULIERE ne gagne aucune cle de sommets et se reenregistre "
				"OCTET POUR OCTET",
				lu1 && a1.Compare(a2) == 0 && sansCle && garde && ancree(a1), d);
	}
	// ── 8. UN SOMMET DEPLACE fait l'aller-retour, valeur par valeur ─────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[3].x = -0.25f;
		st.doc.nodes[(uint32)iE].sommets[3].y = 0.75f;
		NkString b1, b2;
		st.doc.Save(b1);
		NkUIDocument r2;
		const bool lu2 = r2.Load(b1.Data());
		if (lu2)
			r2.Save(b2);
		const NkUINode *rn = (lu2 && r2.IsValidIndex(iE)) ? &r2.nodes[(uint32)iE] : nullptr;
		const bool champs = rn && rn->sommets.Size() == 10 && rn->sommets[3].x == -0.25f
							&& rn->sommets[3].y == 0.75f;
		char d[112];
		snprintf(d, sizeof(d), "%u sommet(s), le 4e (%.2f, %.2f), octets %s",
				 rn ? (uint32)rn->sommets.Size() : 0u, rn ? rn->sommets[3].x : -9.f,
				 rn ? rn->sommets[3].y : -9.f,
				 (lu2 && b1.Compare(b2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
		verdict("8. un sommet deplace fait l'aller-retour et se reenregistre a l'identique",
				champs && lu2 && b1.Compare(b2) == 0, d);
	}
	// ── 9. LA COPIE emporte les sommets ─────────────────────────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[0].x = 0.5f;
		const int32 j = st.doc.CopierSousArbre(st.doc, iE, 0);
		const bool ok = st.doc.IsValidIndex(j) && st.doc.nodes[(uint32)j].sommets.Size() == 10
						&& st.doc.nodes[(uint32)j].sommets[0].x == 0.5f;
		char d[96];
		snprintf(d, sizeof(d), "%u sommet(s) copie(s)",
				 st.doc.IsValidIndex(j) ? (uint32)st.doc.nodes[(uint32)j].sommets.Size() : 0u);
		verdict("9. copier un sous-arbre emporte les sommets deplaces", ok, d);
	}
	// ── 10. CONSERVATION : REGARDER les sommets n'ecrit rien ────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		const int32 iL = poser("line", nullptr);
		st.Recompute(surface);
		NkString avant;
		st.doc.Save(avant);
		float32 xy[64];
		(void)NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), xy, 32);
		(void)NkSommetsDe(st.doc.nodes[(uint32)iL], st.layout.At(iL), xy, 32);
		(void)NkIssueDeDblClic(st.doc.nodes[(uint32)iE]);
		(void)NkNatureDe(st.doc.nodes[(uint32)iL].shape.Data());
		NkString apres;
		st.doc.Save(apres);
		char d[96];
		snprintf(d, sizeof(d), "document %s",
				 (avant.Compare(apres) == 0) ? "INCHANGE" : "MODIFIE");
		verdict("10. CONSERVATION : lire les sommets et interroger la table n'ecrit RIEN",
				ancree(avant) && avant.Compare(apres) == 0, d);
	}
	// ── 11. UN SOMMET DEPLACE SE VOIT DANS LA GEOMETRIE DESSINEE ────────────
	//     ⚠️ CE CAS EXISTE PARCE QU'UNE MUTATION N'A RIEN CASSE. En forcant
	//     `NkSommetsDe` a ignorer la liste du document (donc a toujours rendre
	//     l'etoile REGULIERE), les dix cas precedents restaient verts : le cas 8
	//     lit le MODELE (le sommet est bien enregistre), le cas 5 compare deux
	//     appels qui utilisaient tous deux la table. Personne ne verifiait le
	//     seul lien qui compte : que deplacer un sommet CHANGE CE QUI EST
	//     DESSINE. Sans ce cas, tout le mode points pouvait etre parfaitement
	//     enregistre et parfaitement invisible.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		st.Recompute(surface);
		float32 avant[64], apres[64];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), avant, 32);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[2].x = 0.123f; // une pointe tiree
		st.doc.nodes[(uint32)iE].sommets[2].y = -0.456f;
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), apres, 32);
		const NkPaintRect r = st.layout.At(iE);
		const float32 attX = r.x + r.w * 0.5f + 0.123f * (r.w * 0.5f);
		const float32 attY = r.y + r.h * 0.5f + (-0.456f) * (r.h * 0.5f);
		const bool bouge = nA == nB && nB > 2 && apres[4] == attX && apres[5] == attY
						   && (avant[4] != apres[4] || avant[5] != apres[5]);
		// et les AUTRES sommets n'ont pas bouge : deplacer un point n'en deplace
		// qu'un.
		bool autresFixes = true;
		for (uint32 i = 0; i < nB && autresFixes; ++i) {
			if (i == 2)
				continue;
			if (avant[i * 2] != apres[i * 2] || avant[i * 2 + 1] != apres[i * 2 + 1])
				autresFixes = false;
		}
		char d[144];
		snprintf(d, sizeof(d), "3e sommet (%.1f,%.1f) -> (%.1f,%.1f), attendu (%.1f,%.1f) ; "
							   "les autres %s",
				 avant[4], avant[5], apres[4], apres[5], attX, attY,
				 autresFixes ? "fixes" : "ONT BOUGE");
		verdict("11. deplacer un sommet CHANGE la geometrie dessinee -- et ne deplace que "
				"celui-la",
				bouge && autresFixes, d);
	}
	// ── 12. LA PRIORITE DE POIGNEE EST DITE, PAS SUBIE ─────────────────────
	//     ⚠️ Sur une LIGNE, les deux bouts tombent EXACTEMENT sur deux coins de
	//     la boite : poignee de redimensionnement et poignee de sommet au MEME
	//     pixel. La premiere correction les rendait exclusives par deux gardes
	//     situees a six cents lignes l'une de l'autre -- une priorite SUBIE, que
	//     le premier qui deplace un des deux `if` casse sans s'en apercevoir.
	//     La regle se cite maintenant, et ce cas la tient.
	{
		const bool ok = NkAQuiLaPoignee(7, 7) == NkProprioPoignee::Sommet
						&& NkAQuiLaPoignee(-1, 7) == NkProprioPoignee::Redimension
						&& NkAQuiLaPoignee(7, 9) == NkProprioPoignee::Redimension
						&& NkAQuiLaPoignee(7, -1) == NkProprioPoignee::Aucune
						&& NkAQuiLaPoignee(-1, -1) == NkProprioPoignee::Aucune;
		verdict("12. en mode points le SOMMET gagne ; ailleurs, la poignee de "
				"redimensionnement -- et la regle se cite au lieu de dependre de l'ordre du "
				"code",
				ok, "");
	}
	// ── 13. LES DEUX VOIES D'ENTREE DISENT LA MEME CHOSE ────────────────────
	//     ⚠️ CE CAS A CHANGE DE SUJET LE 01/09, ET IL FAUT LE DIRE PLUTOT QUE DE
	//     LE REECRIRE EN SILENCE. Il tenait un invariant STRUCTUREL : « le mode
	//     points ne s'ouvre jamais sur rect/ellipse, donc le conflit poignee de
	//     coin / poignee de sommet y est impossible ». **Cet invariant n'existe
	//     plus** -- rect et ellipse entrent desormais en mode points, et leurs
	//     quatre coins portent bien DEUX poignees au meme pixel.
	//
	//     Ce qui reste, et qui est le vrai contenu du cas : les DEUX VOIES
	//     D'ENTREE (`NkPeutEntrerEnPoints` et `NkIssueDeDblClic`) doivent dire la
	//     meme chose, forme par forme. Elles sont ecrites a deux endroits
	//     differents du meme fichier ; c'est exactement le genre de couple qui
	//     derive sans qu'on le voie -- le motif du carnet.
	//
	//     ⚠️ ET LE CONFLIT DE POIGNEES, LUI, EST TENU PAR LE CAS 12
	//     (`NkAQuiLaPoignee`), qui n'a pas eu besoin d'une ligne de changement :
	//     il dit « en mode points, le sommet gagne » sans rien savoir de la
	//     forme. C'est le benefice qu'on avait paye en remplacant deux gardes
	//     distantes par une regle citable, et il se percoit aujourd'hui sur un
	//     cas qui n'existait pas quand on l'a ecrite.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iIm = poser("image", nullptr);
		const int32 iL = poser("line", nullptr);
		const int32 iE = poser("etoile", nullptr);
		const int32 iT = poser("text", "x");
		auto peut = [&](int32 i) { return NkPeutEntrerEnPoints(st.doc.nodes[(uint32)i]); };
		// et la table du double-clic dit la MEME chose : les deux voies d'entree
		// ne peuvent pas diverger sans que ce cas le voie.
		auto issue = [&](int32 i) { return NkIssueDeDblClic(st.doc.nodes[(uint32)i]); };
		const bool coherent =
			(peut(iR) == (issue(iR) == NkIssueDblClic::ModePoints))
			&& (peut(iEl) == (issue(iEl) == NkIssueDblClic::ModePoints))
			&& (peut(iIm) == (issue(iIm) == NkIssueDblClic::ModePoints))
			&& (peut(iL) == (issue(iL) == NkIssueDblClic::ModePoints))
			&& (peut(iE) == (issue(iE) == NkIssueDblClic::ModePoints))
			&& (peut(iT) == (issue(iT) == NkIssueDblClic::ModePoints));
		char d[128];
		snprintf(d, sizeof(d), "rect=%d, ellipse=%d, image=%d | ligne=%d, etoile=%d, texte=%d",
				 peut(iR) ? 1 : 0, peut(iEl) ? 1 : 0, peut(iIm) ? 1 : 0, peut(iL) ? 1 : 0,
				 peut(iE) ? 1 : 0, peut(iT) ? 1 : 0);
		verdict("13. les deux voies d'entree du mode points disent la MEME chose forme par "
				"forme -- rect/ellipse/ligne/etoile oui, image et texte non",
				peut(iR) && peut(iEl) && !peut(iIm) && peut(iL) && peut(iE) && !peut(iT)
					&& coherent,
				d);
	}
	// ── 14. UN DOUBLE-CLIC VAUT APPUI ──────────────────────────────────────
	//     ⚠️ CE CAS EXISTE PARCE QUE LE BANC ET LE GESTE REEL NE PASSAIENT PAS
	//     PAR LA MEME PORTE, et c'est ce qui a rendu le defaut invisible.
	//     Mesure du 01/09 : la classe de fenetre porte CS_DBLCLKS, donc Windows
	//     REMPLACE le second WM_LBUTTONDOWN par WM_LBUTTONDBLCLK ; le Win32
	//     n'en tirait aucun appui ; `mouseClicked[0]` restait faux ; et la toile
	//     enfermait sa branche double-clic dans `if (in.mousePressed)`. Le geste
	//     de Rodolf n'y entrait JAMAIS -- « double-cliquer ne me permet pas
	//     d'acceder a sa modification fine ». L'injecteur, lui, posait
	//     `mouseDown` PUIS `SetDoubleClick` : il prenait l'autre porte.
	{
		const bool ok = NkAppuiDeToile(false, true) // LE cas qui manquait
						&& NkAppuiDeToile(true, false) && NkAppuiDeToile(true, true)
						&& !NkAppuiDeToile(false, false);
		verdict("14. un double-clic vaut APPUI -- meme quand l'OS n'envoie pas d'appui avec "
				"(CS_DBLCLKS remplace le second WM_LBUTTONDOWN)",
				ok, ok ? "sans appui + double-clic = appui" : "LE DOUBLE-CLIC NE VAUT PLUS APPUI");
	}
	// ── 15. CHAQUE SUITE DU DOUBLE-CLIC DIT QUELQUE CHOSE ──────────────────
	//     ⚠️ LA REGLE DE LA MAISON, TENUE PAR UN BANC PLUTOT QUE PAR UNE
	//     INTENTION : un controle qui ne fait rien sans rien dire est le defaut
	//     qu'on chasse. Le cas parcourt les suites PAR LEUR NOMBRE, pas par une
	//     liste ecrite a la main -- une septieme suite ajoutee sans sa phrase
	//     fait tomber ce cas au lieu de passer inapercue.
	//     Et il exige aussi que deux suites ne disent pas LA MEME phrase : « ca
	//     dit quelque chose » ne suffit pas si ca ne distingue rien.
	{
		bool toutesParlent = true, toutesDistinctes = true;
		const uint32 n = NkNbSuitesDblClic();
		const char *phrases[8] = {nullptr};
		for (uint32 i = 0; i < n && i < 8; ++i) {
			const char *p = NkRaisonDeDblClic((NkSuiteDblClic)i);
			phrases[i] = p;
			if (!p || !*p)
				toutesParlent = false;
		}
		for (uint32 i = 0; i < n && i < 8; ++i)
			for (uint32 j = i + 1; j < n && j < 8; ++j)
				if (phrases[i] && phrases[j] && NkComponentDecl::StrEq(phrases[i], phrases[j]))
					toutesDistinctes = false;
		char d[96];
		snprintf(d, sizeof(d), "%u suites, toutes %s et %s", n,
				 toutesParlent ? "parlantes" : "PAS TOUTES PARLANTES",
				 toutesDistinctes ? "distinctes" : "PAS TOUTES DISTINCTES");
		verdict("15. CHAQUE suite du double-clic produit une raison DITE, et deux suites ne "
				"disent pas la meme chose",
				toutesParlent && toutesDistinctes, d);
	}
	// ── 16. LE CORPS D'UN GROUPE N'EST PAS UN TROU ─────────────────────────
	//     ⚠️ LE DEFAUT MESURE LE 01/09 SUR LE DOCUMENT DASHBOARD, ET C'EST LA
	//     PLUS GRANDE PARTIE DE LA SURFACE D'UNE CARTE : un double-clic sur le
	//     corps de `Carte_Actifs` (ou de `Panel_Nav`, ou d'un artboard) rendait
	//     l'issue `Forer` -- correcte -- puis `NkPickDansContexte` rendait -2
	//     (« hors du contexte »), et la branche faisait un `SelectSingle` MUET
	//     sans armer le forage. Rien ne se passait, rien ne le disait, et le
	//     double-clic suivant refaisait exactement la meme chose.
	//     Le cas tient les DEUX suites de `Forer` cote a cote : sur un enfant
	//     (descendre) et a cote (entrer quand meme, et le dire).
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iG = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &g = st.doc.nodes[(uint32)iG];
			g.label = NkString("Carte");
			g.shape = NkString("rect");
			g.width.mode = NkSizeMode::Fixed;
			g.width.value = 200.f;
			g.height.mode = NkSizeMode::Fixed;
			g.height.value = 100.f;
		}
		const int32 iT = st.doc.AddChild(iG, "", NkAuthor::Humain);
		{
			NkUINode &t = st.doc.nodes[(uint32)iT];
			t.label = NkString("Valeur");
			t.shape = NkString("text");
			t.text = NkString("42");
			t.width.mode = NkSizeMode::Fixed;
			t.width.value = 40.f;
			t.height.mode = NkSizeMode::Fixed;
			t.height.value = 20.f;
		}
		st.Recompute(surface);
		const NkPaintRect rg = st.layout.At(iG), rt = st.layout.At(iT);
		// (a) SUR l'enfant : on descend.
		const float32 ax = rt.x + rt.w * 0.5f, ay = rt.y + rt.h * 0.5f;
		const int32 candA = NkPickDansContexte(st.doc, st.layout, ax, ay, -1);
		const int32 enfA = st.doc.IsValidIndex(candA)
							   ? NkPickDansContexte(st.doc, st.layout, ax, ay, candA)
							   : -1;
		const NkSuiteDblClic sA =
			NkSuiteDeDblClic(NkIssueDeDblClic(st.doc.nodes[(uint32)(candA > 0 ? candA : 0)]),
							 enfA >= 0);
		// (b) A COTE de l'enfant, DANS le groupe : le cas qui etait muet.
		const float32 bx = rg.x + rg.w - 6.f, by = rg.y + rg.h - 6.f;
		const int32 candB = NkPickDansContexte(st.doc, st.layout, bx, by, -1);
		const int32 enfB = st.doc.IsValidIndex(candB)
							   ? NkPickDansContexte(st.doc, st.layout, bx, by, candB)
							   : -1;
		const NkSuiteDblClic sB =
			NkSuiteDeDblClic(NkIssueDeDblClic(st.doc.nodes[(uint32)(candB > 0 ? candB : 0)]),
							 enfB >= 0);
		const char *raison = NkRaisonDeDblClic(sB);
		const bool ok = candA == iG && enfA == iT && sA == NkSuiteDblClic::ForerVersEnfant
						&& candB == iG && enfB < 0 && sB == NkSuiteDblClic::ForerSansEnfant
						&& raison && *raison;
		char d[160];
		snprintf(d, sizeof(d), "sur l'enfant : cand=%d enfant=%d ; au corps : cand=%d enfant=%d, "
							   "raison %s",
				 candA, enfA, candB, enfB, (raison && *raison) ? "DITE" : "MUETTE");
		verdict("16. le CORPS d'un groupe n'est pas un trou : le double-clic y entre quand meme "
				"et le DIT (les deux suites de Forer, cote a cote)",
				ok, d);
	}
	// ── 17. LE MENU DU CLIC DROIT : QUI N'AGIT PAS DIT POURQUOI ────────────
	//     ⚠️ L'INVENTAIRE LUNACY, RENDU MESURABLE. Un tableau dans un rapport se
	//     perime ; ce cas, non. Il exige les DEUX sens, et le second est celui
	//     qu'on oublie : qui n'agit pas porte une raison NON VIDE, ET qui agit
	//     n'en porte PAS -- sinon la raison serait un ornement qu'on cesse de
	//     lire, et le jour ou elle compte personne ne la verrait.
	{
		NkContexteCtx c;
		c.aTexte = true;
		c.pasRacine = true;
		c.aEnfants = true;
		NkMenuCtx menu;
		NkConstruireMenuCtx(c, menu);
		bool coherent = (menu.n > 0);
		for (uint32 i = 0; i < menu.n; ++i) {
			const NkEntreeCtx &e = menu.items[i];
			if (!e.libelle[0])
				coherent = false;
			if (!e.agit && (!e.raison || !*e.raison))
				coherent = false; // muet
			if (e.agit && e.raison)
				coherent = false; // raison decorative
		}
		char d[96];
		snprintf(d, sizeof(d), "%u entrees, chacune parlante", (uint32)menu.n);
		verdict("17. menu contextuel : qui n'agit pas DIT pourquoi, et qui agit ne dit rien "
				"d'inutile",
				coherent, d);
	}
	// ── 18. LES DEUX SURFACES : MEME JEU D'ENTREES, APPLICABILITE DIFFERENTE ─
	//     ⚠️ LE PIEGE DE GROUPE, TENU PAR UN BANC. Lunacy montre le meme menu
	//     depuis sa toile et depuis son panneau Layers ; deux jeux auraient
	//     derive au premier ajout, et l'utilisateur aurait cherche dans un menu
	//     une commande qu'il venait de voir dans l'autre. Le cas verifie que la
	//     LISTE des actions est identique, ET qu'au moins une applicabilite
	//     DIFFERE -- sans quoi `surfaceListe` serait un parametre declare et non
	//     honore, la famille de defauts que ce depot compte depuis huit fois.
	{
		NkContexteCtx t, l;
		t.aTexte = l.aTexte = true;
		t.pasRacine = l.pasRacine = true;
		t.surfaceListe = false;
		l.surfaceListe = true;
		NkMenuCtx mt, ml;
		NkConstruireMenuCtx(t, mt);
		NkConstruireMenuCtx(l, ml);
		bool memesActions = (mt.n == ml.n);
		bool uneDifference = false;
		for (uint32 i = 0; memesActions && i < mt.n; ++i) {
			if (mt.items[i].action != ml.items[i].action)
				memesActions = false;
			if (mt.items[i].agit != ml.items[i].agit)
				uneDifference = true;
		}
		char d[128];
		snprintf(d, sizeof(d), "%u vs %u entrees, actions %s, applicabilite %s", (uint32)mt.n,
				 (uint32)ml.n, memesActions ? "IDENTIQUES" : "DIVERGENTES",
				 uneDifference ? "differente" : "IDENTIQUE (surfaceListe non honore)");
		verdict("18. toile et hierarchie : MEME jeu d'entrees, applicabilite differente",
				memesActions && uneDifference, d);
	}
	// ── 19. LA RANGEE D'ICONES PARLE DANS LES DEUX ETATS ───────────────────
	//     ⚠️ UNE ICONE MUETTE EST UN BOUTON QU'ON N'OSE PAS PRESSER, et une
	//     icone GRISEE muette est pire : elle ne dit meme pas ce qui manque.
	//     Trois des sept (verrou, visibilite, registre) n'ont AUCUN champ dans
	//     le modele : elles sont PRESENTES et grisees, et leur infobulle le dit.
	//     Le cas parcourt les icones PAR LEUR NOMBRE, jamais par une liste.
	{
		bool toutesParlent = true;
		const uint32 n = (uint32)NkIconeCtx::NB;
		for (uint32 i = 0; i < n; ++i) {
			const char *a = NkInfobulleIconeCtx((NkIconeCtx)i, true);
			const char *b = NkInfobulleIconeCtx((NkIconeCtx)i, false);
			if (!a || !*a || !b || !*b || NkComponentDecl::StrEq(a, b))
				toutesParlent = false; // muette, ou la meme phrase dans les deux etats
		}
		// et l'applicabilite de la rangee suit le MEME contexte que le menu :
		// une poubelle active au-dessus d'un « Supprimer » grise serait deux
		// verites pour un seul fait.
		NkContexteCtx vide; // racine, presse-papiers vide
		bool aucuneNAgit = true;
		for (uint32 i = 0; i < n; ++i)
			if (NkIconeCtxAgit((NkIconeCtx)i, vide))
				aucuneNAgit = false;
		char d[112];
		snprintf(d, sizeof(d), "%u icones, %s ; sur la racine : %s", n,
				 toutesParlent ? "toutes parlantes dans les 2 etats" : "UNE MUETTE",
				 aucuneNAgit ? "aucune n'agit" : "UNE AGIT");
		verdict("19. la rangee d'icones parle dans les DEUX etats, et son applicabilite suit le "
				"meme contexte que le menu",
				toutesParlent && aucuneNAgit, d);
	}
	// ── 27. LE TRACE A LA SOURIS, ET SES DEUX MODIFICATEURS ────────────────
	//     Retour (4) de Rodolf : « je veux le cliquer+glisser pour modifier la
	//     taille des elements comme sur Lunacy, avec le Ctrl ou Shift enfonce
	//     pour gerer la proportionnalite ».
	//
	//     ⚠️ MESURE AVANT D'ECRIRE, ET ELLE A EVITE TROIS REECRITURES : le
	//     cliquer-glisser EXISTAIT, Maj CONTRAIGNAIT deja, et la puce de
	//     dimensions s'affichait deja pendant le geste. Il manquait UNE chose --
	//     tracer depuis le CENTRE. Les quatre cas ci-dessous tiennent donc trois
	//     comportements ANCIENS (pour qu'ils ne se cassent pas en ajoutant le
	//     quatrieme) et un NEUF.
	{
		// (a) le glisser nu : le rectangle va du depart a la souris
		const NkPaintRect a = NkRectDeTrace(100.f, 100.f, 180.f, 150.f, false, false, false);
		const bool nu = a.x == 100.f && a.y == 100.f && a.w == 80.f && a.h == 50.f;
		// (b) VERS LA GAUCHE ET VERS LE HAUT : l'ancre reste le point de depart
		const NkPaintRect b = NkRectDeTrace(100.f, 100.f, 60.f, 70.f, false, false, false);
		const bool arriere = b.x == 60.f && b.y == 70.f && b.w == 40.f && b.h == 30.f;
		// (c) MAJ : carre, ET il ne GLISSE PAS sous la main quand on tire vers
		//     le haut-gauche (la regle ecrite dans l'ancien RectTrace, conservee)
		const NkPaintRect c = NkRectDeTrace(100.f, 100.f, 60.f, 70.f, true, false, false);
		const bool carre = c.w == c.h && c.w == 40.f && c.x == 60.f && c.y == 60.f;
		// (d) ALT : depuis le CENTRE -- la souris donne le DEMI-cote
		//     ⚠️ Prendre `d` comme cote entier ferait grandir la forme DEUX FOIS
		//     plus vite que la main : le defaut classique de cette option quand
		//     on l'ajoute apres coup.
		const NkPaintRect e = NkRectDeTrace(100.f, 100.f, 140.f, 130.f, false, true, false);
		const bool centre = e.x == 60.f && e.y == 70.f && e.w == 80.f && e.h == 60.f;
		// (e) LES DEUX ENSEMBLE : un carre centre sur le point de depart
		const NkPaintRect f = NkRectDeTrace(100.f, 100.f, 140.f, 130.f, true, true, false);
		const bool lesDeux = f.w == f.h && f.w == 80.f && f.x == 60.f && f.y == 60.f;
		// (f) LA LIGNE ne se contraint PAS comme une boite : l'horizontale doit
		//     rester atteignable, c'est de loin la plus demandee
		const NkPaintRect g = NkRectDeTrace(100.f, 100.f, 200.f, 108.f, true, false, true);
		const bool ligneH = g.h == 0.f && g.w == 100.f;
		char d[176];
		snprintf(d, sizeof(d), "nu=%d arriere=%d carre=%d centre=%d les2=%d ligneH=%d",
				 nu ? 1 : 0, arriere ? 1 : 0, carre ? 1 : 0, centre ? 1 : 0, lesDeux ? 1 : 0,
				 ligneH ? 1 : 0);
		verdict("27. le trace : glisser nu, vers l'arriere sans glisser sous la main, Maj = "
				"carre, Alt = depuis le CENTRE (demi-cote), les deux ensemble, et la ligne "
				"garde son horizontale",
				nu && arriere && carre && centre && lesDeux && ligneH, d);
	}
	// ── 26. LES SOMMETS EPOUSENT LA FORME, ET LES BOUGER NE TOUCHE PAS LA BOITE ─
	//     Retour (1) de Rodolf, 01/09 : « en plus colle sur la forme, donc
	//     epouser la forme, de telle sorte que cliquer sur un point et le
	//     deplacer modifie le mesh ».
	//
	//     ⚠️ DEUX MOITIES, ET LA SECONDE EST CELLE QU'IL SOUPCONNAIT. « Epouser
	//     la forme » se verifie en mesurant la DISTANCE des ancres au contour ;
	//     « modifier le mesh » se verifie en montrant que le deplacement ecrit
	//     dans `sommets` et NE TOUCHE PAS posX/posY/largeur/hauteur. Sa capture
	//     laissait croire que la boite avait bouge -- le cas tranche.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("ellipse", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iE);
		float32 anc[64];
		const uint32 nb = NkSommetsDe(st.doc.nodes[(uint32)iE], r, anc, 32);
		// (a) CHAQUE ancre est SUR l'ellipse : ((x-cx)/hx)^2 + ((y-cy)/hy)^2 == 1
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		const float32 hx = r.w * 0.5f, hy = r.h * 0.5f;
		float32 pireEcart = 0.f;
		for (uint32 i = 0; i < nb; ++i) {
			const float32 u = (anc[i * 2] - cx) / hx, v = (anc[i * 2 + 1] - cy) / hy;
			const float32 d = u * u + v * v - 1.f;
			const float32 ad = d < 0.f ? -d : d;
			if (ad > pireEcart)
				pireEcart = ad;
		}
		// (b) DEPLACER un sommet ecrit dans `sommets` et LAISSE LA BOITE INTACTE
		NkUINode &n = st.doc.nodes[(uint32)iE];
		const float32 x0 = n.posX, y0 = n.posY, w0 = n.width.value, h0 = n.height.value;
		NkMaterialiserSommets(n);
		n.sommets[0].x += 0.4f;
		n.sommets[0].y += 0.3f;
		const bool boiteIntacte = n.posX == x0 && n.posY == y0 && n.width.value == w0
								  && n.height.value == h0;
		// (c) et le CONTOUR PEINT a bouge : c'est le mesh, pas la boite
		float32 ct[256];
		const uint32 nbC = NkContourDe(n, r, ct, 128);
		const bool contourBouge = nbC == nb && (ct[0] != anc[0] || ct[1] != anc[1]);
		char d[176];
		snprintf(d, sizeof(d), "%u ancres, pire ecart a la courbe %.4f ; boite %s ; contour %s",
				 nb, pireEcart, boiteIntacte ? "INTACTE" : "A BOUGE",
				 contourBouge ? "a bouge" : "IDENTIQUE");
		verdict("26. les ancres d'une ellipse sont SUR la courbe (pas aux coins de sa boite), "
				"et deplacer un sommet modifie le TRACE sans toucher a la boite",
				nb == 12 && pireEcart < 0.001f && boiteIntacte && contourBouge, d);
	}
	// ── 24. LE MODE POINTS N'EST ARME QUE SUR LE NOEUD SELECTIONNE ─────────
	//     🔴 LE BOGUE BLOQUANT du 01/09 (capture `probleme_vertices_141913`) :
	//     « a un moment ca disparait, ca n'apparait plus, et c'est difficile ou
	//     impossible de le deselectionner. »
	//
	//     Lu A LA SOURCE : le bloc du mode points etait garde par
	//     `mPointsNode >= 0 && screen.Has(mPointsNode)` SEUL. Le mode survivait
	//     donc a un changement de selection, son gestionnaire de clic tournait
	//     sur TOUS les clics du canevas, et un clic a moins de 6 px du contour de
	//     la forme QUITTEE lui ajoutait un sommet en silence.
	//
	//     ⚠️ CE CAS TIENT LES DEUX LECTURES ENSEMBLE, ET C'EST TOUT SON INTERET :
	//     « le mode est-il arme ? » et « a qui la poignee ? » doivent repondre la
	//     MEME chose. Sur sa capture, elles repondaient DIFFEREMMENT -- le pied de
	//     fenetre annoncait « Mode edition de forme » pendant que l'ecran montrait
	//     les carres de redimensionnement de la boite. Deux formulations d'une
	//     meme question qui divergent : le motif du carnet, pris sur le fait.
	{
		const bool armeOk = NkModePointsArme(21, 21)
							&& NkAQuiLaPoignee(21, 21) == NkProprioPoignee::Sommet;
		// on selectionne AUTRE CHOSE : le mode tombe, et la boite reprend ses
		// poignees. C'est le cas que le code d'hier ratait.
		const bool sortSurAutreSelection =
			!NkModePointsArme(21, 7) && NkAQuiLaPoignee(21, 7) == NkProprioPoignee::Redimension;
		// on clique dans le VIDE (selection videe) : le mode tombe aussi
		const bool sortSurVide =
			!NkModePointsArme(21, -1) && NkAQuiLaPoignee(21, -1) == NkProprioPoignee::Aucune;
		const bool pasDeMode = !NkModePointsArme(-1, 21);
		char d[160];
		snprintf(d, sizeof(d), "arme(21,21)=%d ; autre selection=%d ; vide=%d ; sans mode=%d",
				 armeOk ? 1 : 0, sortSurAutreSelection ? 1 : 0, sortSurVide ? 1 : 0,
				 pasDeMode ? 1 : 0);
		verdict("24. le mode points n'est arme QUE sur le noeud selectionne -- changer de "
				"selection en sort, le vide en sort -- et les deux lectures (mode arme / a qui "
				"la poignee) disent la MEME chose",
				armeOk && sortSurAutreSelection && sortSurVide && pasDeMode, d);
	}
	// ── 25. SUR SON DOCUMENT REEL, PAS SUR UN CAS FABRIQUE ─────────────────
	//     ⚠️ DEUX FOIS CETTE SEMAINE UN BANC EST PASSE AU VERT LA OU SA MAIN
	//     ECHOUAIT, parce que le banc ne passait pas par la meme porte. Ce cas
	//     charge donc `nkuidesign_document.nkuidoc` -- LE document de sa capture,
	//     avec ses 40 et quelques noeuds -- et fait marcher la vraie table dessus.
	//
	//     Il tient le retour (3) : « ca doit etre sur les formes dessinees AUTRES
	//     QUE LE TEXTE ». Aucun noeud portant du texte, aucun cadre, aucun groupe
	//     ne doit pouvoir entrer en mode points.
	{
		NkUIDocument vrai;
		// ⚠️ L'ORDRE DES CHEMINS A ETE CORRIGE PAR LA PREMIERE EXECUTION, ET LA
		//    LECON VAUT D'ETRE ECRITE. Ma premiere version cherchait d'abord
		//    `nkuidesign_document.nkuidoc` A COTE DE L'EXECUTABLE : elle a trouve
		//    un STUB de 5 noeuds datant du 18/08, et le cas est passe... a
		//    « 0 faute » -- vert sur un document qui n'a NI texte NI cadre, donc
		//    qui ne pouvait rien prouver. *Un banc qui charge le mauvais fichier
		//    ne mesure pas moins : il mesure autre chose, et il le dit vert.*
		//    Le depot est interroge EN PREMIER, et le seuil ci-dessous refuse
		//    tout document trop petit pour etre le sien.
		const char *chemins[3] = {
			"D:/Projets/2026/Nkentseu/Nkentseu-noge/nkuidesign_document.nkuidoc",
			"../../../../nkuidesign_document.nkuidoc", "nkuidesign_document.nkuidoc"};
		bool charge = false;
		for (uint32 c = 0; c < 3 && !charge; ++c) {
			if (!nkentseu::NkFile::Exists(chemins[c]))
				continue;
			const NkString txt = nkentseu::NkFile::ReadAllText(chemins[c]);
			if (txt.Size() > 0 && vrai.Load(txt.Data())) {
				// ⚠️ LE SEUIL EST LA GARDE QUI MANQUAIT : le document de sa
				//    capture porte deux artboards et une quarantaine de noeuds.
				//    Un stub de cinq noeuds n'est pas « une version reduite »,
				//    c'est un AUTRE document -- et il rendrait ce cas vert sans
				//    qu'aucun texte ni cadre n'ait ete examine.
				charge = (vrai.nodes.Size() >= 20u);
			}
		}
		if (!charge) {
			// ⚠️ UN CAS QUI NE PEUT PAS MESURER LE DIT, il ne passe pas au vert.
			verdict("25. sur le document REEL de sa capture : aucun TEXTE, cadre ou groupe "
					"n'entre en mode points",
					false,
					"DOCUMENT REEL INTROUVABLE (ou trop petit) -- ce cas ne prouve rien, et "
					"il le DIT plutot que de passer au vert sur un stub");
		} else {
			uint32 nTexte = 0, nCadre = 0, nGroupe = 0, nFormes = 0, fautes = 0;
			for (uint32 i = 1; i < (uint32)vrai.nodes.Size(); ++i) {
				const NkUINode &n = vrai.nodes[i];
				const bool peut = NkPeutEntrerEnPoints(n);
				const bool issuePoints = NkIssueDeDblClic(n) == NkIssueDblClic::ModePoints;
				const bool estTexte = NkComponentDecl::StrEq(n.shape.Data(), "text")
									  || !n.text.Empty();
				const bool estCadre = NkComponentDecl::StrEq(n.shape.Data(), "frame");
				if (n.children.Size() > 0) {
					++nGroupe;
					if (peut || issuePoints)
						++fautes;
				} else if (estTexte) {
					++nTexte;
					if (peut || issuePoints)
						++fautes;
				} else if (estCadre) {
					++nCadre;
					if (peut || issuePoints)
						++fautes;
				} else if (peut) {
					++nFormes;
				}
			}
			char d[184];
			snprintf(d, sizeof(d),
					 "%u noeuds : %u textes, %u cadres, %u groupes, %u formes editables ; "
					 "%u faute(s)",
					 (uint32)vrai.nodes.Size(), nTexte, nCadre, nGroupe, nFormes, fautes);
			verdict("25. sur le document REEL de sa capture : aucun TEXTE, cadre ou groupe "
					"n'entre en mode points",
					fautes == 0 && nTexte > 0, d);
		}
	}
	// ── 23. LE MENU DU CLIC DROIT DANS LE VIDE ─────────────────────────────
	//     Retour de Rodolf, 01/09, signale en Q43 : le clic droit dans le vide
	//     ne faisait RIEN et ne disait RIEN.
	//
	//     ⚠️ CE CAS TIENT LE MEME INVARIANT QUE LE 17, SUR UN AUTRE MENU, ET
	//     C'EST DELIBERE. C'est ca, traiter les chemins freres comme un groupe :
	//     on ne partage pas le CONTENU (le vide de la toile est LA VUE, pas un
	//     objet sans proprietes -- pas une seule entree de la reference Lunacy ne
	//     parle d'un calque), on partage la REGLE : qui n'agit pas dit pourquoi,
	//     qui agit ne dit rien d'inutile.
	//
	//     ⚠️ ET IL AJOUTE UNE REGLE QUE L'AUTRE N'A PAS : UNE COCHE SUR UNE
	//     ENTREE QUI N'AGIT PAS EST TOUJOURS FAUSSE. C'est exactement le defaut
	//     que Q42 a trouve sur le menu Affichage -- « une case toujours cochee a
	//     cote d'un aimant qui ne faisait rien ». On ne le refait pas dix fois
	//     dans un menu neuf.
	{
		NkContexteVide c;
		c.pressePapiersPlein = true;
		c.grilleVisible = true;
		c.aimantCalques = false;
		NkMenuVide m;
		NkConstruireMenuVide(c, m);
		bool coherent = (m.n > 0);
		bool aucuneCocheFantome = true;
		uint32 nAgit = 0;
		for (uint32 i = 0; i < m.n; ++i) {
			const NkEntreeVide &e = m.items[i];
			if (!e.libelle[0])
				coherent = false;
			if (!e.agit && (!e.raison || !*e.raison))
				coherent = false; // muette
			if (e.agit && e.raison)
				coherent = false; // raison decorative
			if (!e.agit && e.coche)
				aucuneCocheFantome = false;
			if (e.agit)
				++nAgit;
		}
		// LES COCHES LISENT L'ETAT REEL, elles ne sont pas decoratives : ici
		// la grille est VRAIE et l'aimant est FAUX, et le menu doit le dire.
		bool grilleCochee = false, aimantCoche = true;
		for (uint32 i = 0; i < m.n; ++i) {
			if (m.items[i].action == NkActionVide::GrillePixels)
				grilleCochee = m.items[i].coche;
			if (m.items[i].action == NkActionVide::AimanterCalques)
				aimantCoche = m.items[i].coche;
		}
		char d[160];
		snprintf(d, sizeof(d), "%u entrees, %u agissent ; grille cochee=%d, aimant coche=%d ; %s",
				 (uint32)m.n, nAgit, grilleCochee ? 1 : 0, aimantCoche ? 1 : 0,
				 aucuneCocheFantome ? "aucune coche fantome" : "UNE COCHE SUR UNE ENTREE INERTE");
		verdict("23. menu du VIDE : chaque entree agit ou dit pourquoi, les coches lisent "
				"l'etat REEL, et AUCUNE entree inerte n'est cochee",
				coherent && aucuneCocheFantome && grilleCochee && !aimantCoche && nAgit >= 3u, d);
	}
	// ── 20. AJOUTER UN SOMMET NE DEFORME PAS LA FORME ──────────────────────
	//     ⚠️ C'EST LA PROMESSE ENTIERE DE L'AJOUT, et elle n'est pas evidente :
	//     poser le sommet neuf a la position de la SOURIS l'aurait mis a cote du
	//     trace, et la forme aurait bouge AU MOMENT MEME DE L'AJOUT -- avant
	//     qu'on tire quoi que ce soit. C'est pour ca que `NkInsererSommet` prend
	//     une fraction de segment et non un point libre.
	//     Le cas mesure le CONTOUR PEINT avant et apres : cinq ancres au lieu de
	//     quatre, et un dessin identique. C'est le seul juge qui compte -- lire
	//     le modele aurait dit « il y a bien cinq sommets » sans rien prouver de
	//     ce qu'on voit.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iR);
		float32 avant[64], apres[64];
		const uint32 cA = NkContourDe(st.doc.nodes[(uint32)iR], r, avant, 32);
		const int32 neuf = NkInsererSommet(st.doc.nodes[(uint32)iR], 1u, 0.5f);
		float32 anc[64];
		const uint32 nAnc = NkSommetsDe(st.doc.nodes[(uint32)iR], r, anc, 32);
		const uint32 cB = NkContourDe(st.doc.nodes[(uint32)iR], r, apres, 32);
		// le contour gagne un point (le sommet neuf EST sur le trace), et tous
		// les points d'avant s'y retrouvent inchanges.
		bool surLeTrace = (neuf == 2 && nAnc == 5 && cB == 5);
		if (surLeTrace) {
			// le sommet 2 (indice 1) et le sommet 4 (indice 3) etaient les coins
			// haut-droit et bas-gauche ; le neuf est a mi-cote droit.
			const float32 mx = (avant[1 * 2] + avant[2 * 2]) * 0.5f;
			const float32 my = (avant[1 * 2 + 1] + avant[2 * 2 + 1]) * 0.5f;
			if (anc[2 * 2] != mx || anc[2 * 2 + 1] != my)
				surLeTrace = false;
		}
		// et le sommet neuf est VIF : l'arrondi ne s'herite pas d'un voisin.
		const bool vif = neuf >= 0 && st.doc.nodes[(uint32)iR].sommets[(uint32)neuf].rayon == 0.f;
		char d[128];
		snprintf(d, sizeof(d), "contour %u -> %u pts, ancre neuve #%d a (%.1f,%.1f), rayon %s",
				 cA, cB, neuf + 1, nAnc > 2 ? anc[4] : 0.f, nAnc > 2 ? anc[5] : 0.f,
				 vif ? "0 (vif)" : "HERITE");
		verdict("20. ajouter un sommet le pose SUR le cote, sans deformer la forme, et il "
				"nait VIF",
				surLeTrace && vif, d);
	}
	// ── 21. LE DOUBLE-CLIC SUR UN SOMMET L'ARRONDIT, ET CA SE VOIT ─────────
	//     ⚠️ DEUX MOITIES, ET LA SECONDE EST CELLE QUI COMPTE. Que le modele
	//     porte un rayon se lit ; que le DESSIN change est le seul lien qui
	//     prouve que le champ n'est pas declare-et-inerte. C'est la lecon de la
	//     mutation 3 du mode points (Q42) : « tout pouvait etre parfaitement
	//     enregistre et parfaitement invisible ».
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iR);
		float32 vif[64], rond[64];
		const uint32 cVif = NkContourDe(st.doc.nodes[(uint32)iR], r, vif, 32);
		const float32 r1 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const uint32 cRond = NkContourDe(st.doc.nodes[(uint32)iR], r, rond, 32);
		// le cycle : 0 -> 8 -> 16 -> 32 -> 0, et il BOUCLE (sinon un sommet
		// arrondi par erreur serait irrattrapable au double-clic).
		const float32 r2 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const float32 r3 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const float32 r4 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
		// et les ANCRES ne bougent pas : arrondir un coin ne deplace pas le coin.
		float32 anc[64];
		const uint32 nAnc = NkSommetsDe(st.doc.nodes[(uint32)iR], r, anc, 32);
		char d[128];
		snprintf(d, sizeof(d), "contour %u -> %u pts, cycle %.0f/%.0f/%.0f/%.0f, %u ancres",
				 cVif, cRond, r1, r2, r3, r4, nAnc);
		verdict("21. arrondir un sommet AJOUTE DES POINTS AU CONTOUR PEINT (le champ agit, il "
				"n'est pas seulement enregistre), le cycle boucle a vif, et les 4 ancres "
				"restent 4",
				cVif == 4 && cRond > cVif && cycle && nAnc == 4, d);
	}
	// ── 22. L'ARRONDI SURVIT A L'ALLER-RETOUR, ET LE FICHIER D'AVANT NE BOUGE PAS ─
	//     ⚠️ LE VOLET CONSERVATION, ECRIT D'ENTREE ET NON EN SORTIE. Deux
	//     exigences ENSEMBLE, parce que l'une seule passe au vert sur une perte :
	//     (a) un trace SANS arrondi se reecrit avec DEUX nombres par sommet,
	//     octet pour octet -- c'est la preuve d'entree de l'additivite ;
	//     (b) un trace AVEC arrondi retrouve sa valeur.
	//     ⚠️ La lecon de la mutation 7 (Q42, FILLS) est portee ici sans etre
	//     redecouverte : « stable » ne veut pas dire « juste ». Un ecrivain qui
	//     laisserait tomber le rayon rendrait deux fichiers identiques et
	//     passerait un test qui ne demande que la stabilite.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR]);
		NkString sansR;
		st.doc.Save(sansR);
		const bool ancree0 = ancree(sansR) && !sansR.Contains("sommet_1 = -1 -1 ");
		NkUIDocument d1;
		NkString sansR2;
		const bool lu1 = d1.Load(sansR.Data());
		d1.Save(sansR2);
		const bool stable = lu1 && sansR.Size() == sansR2.Size()
							&& NkComponentDecl::StrEq(sansR.Data(), sansR2.Data());
		// (b) avec arrondi
		// ⚠️ CETTE GARDE A ETE ECRITE PAR UNE MUTATION, ET C'EST SA RAISON D'ETRE.
		//    En remettant `rect` en boite non editable, la materialisation devient
		//    un non-evenement, `sommets` reste VIDE, et la ligne suivante ecrivait
		//    dans `sommets[1]` : LA RECETTE PLANTAIT (code de sortie 9) au lieu de
		//    rendre un verdict. Un banc qui plante ne dit pas QUEL cas est tombe --
		//    il oblige a relancer sous debogueur pour apprendre ce qu'une ligne
		//    aurait dit. Le cas echoue desormais en NOMMANT sa cause.
		//    (La mutation ne cherchait pas ce defaut-la : elle l'a trouve en
		//    passant, sur le banc lui-meme et non sur le code teste.)
		const bool aDesSommets = st.doc.nodes[(uint32)iR].sommets.Size() >= 4;
		if (aDesSommets)
			st.doc.nodes[(uint32)iR].sommets[1].rayon = 12.f;
		NkString avecR;
		st.doc.Save(avecR);
		NkUIDocument d2;
		const bool lu2 = d2.Load(avecR.Data());
		float32 relu = -1.f;
		for (uint32 i = 1; lu2 && i < (uint32)d2.nodes.Size(); ++i)
			if (d2.nodes[i].sommets.Size() == 4)
				relu = d2.nodes[i].sommets[1].rayon;
		char dd[176];
		snprintf(dd, sizeof(dd), "%ssans rayon : %s (%u o), avec rayon 12 -> relu %.0f",
				 aDesSommets ? "" : "AUCUN SOMMET A MATERIALISER (le rect ne stocke plus) | ",
				 stable ? "octet pour octet" : "A BOUGE", (uint32)sansR.Size(), relu);
		verdict("22. CONSERVATION : un trace a coins vifs se reecrit octet pour octet (le "
				"3e nombre n'est pas ecrit), et un rayon pose se retrouve apres "
				"aller-retour",
				aDesSommets && ancree0 && stable && relu == 12.f, dd);
	}
	// ── 28. LIRE UN SOMMET N'ECRIT RIEN ────────────────────────────────────
	//     🔴 CE CAS EXISTE PARCE QUE LE DEFAUT A FAILLI PARTIR. La premiere
	//     version de la section « EDITION DE FORME » de l'Inspecteur appelait
	//     `NkMaterialiserSommets` POUR LIRE : ouvrir un panneau ajoutait la liste
	//     `sommets` au noeud, changeait les octets du fichier et marquait le
	//     document modifie -- alors que l'utilisateur n'avait rien fait.
	//
	// ⚠️ ET LE VOLET CONSERVATION DU CAS 22 NE L'AURAIT PAS VU : il entre en mode
	//    points et en sort, mais il n'execute pas l'Inspecteur. *Une garde ne
	//    couvre que la porte qu'elle regarde.* Celle-ci porte donc sur le
	//    MECANISME de lecture, la ou les deux panneaux passent.
	//
	// ⚠️ ET IL EXIGE AUSSI L'ACCORD DES DEUX LECTURES. Une lecture qui n'ecrit
	//    rien mais rend d'autres nombres que la table materialisee serait pire
	//    que le defaut d'origine : le panneau montrerait le polygone regulier
	//    pendant que l'ecran montre la forme deja deformee.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr);
		const int32 iE = poser("etoile", nullptr);
		NkString avant;
		st.doc.Save(avant);
		// (a) LIRE les sommets des deux formes -- rien ne doit bouger
		uint32 nbR = NkNbSommetsDe(st.doc.nodes[(uint32)iR]);
		uint32 nbE = NkNbSommetsDe(st.doc.nodes[(uint32)iE]);
		float32 lus[20][3];
		bool tousLus = nbR == 4 && nbE == 10;
		for (uint32 i = 0; i < nbE && i < 20; ++i)
			if (!NkLireSommet(st.doc.nodes[(uint32)iE], i, lus[i][0], lus[i][1], lus[i][2]))
				tousLus = false;
		NkString apres;
		st.doc.Save(apres);
		const bool stable = avant.Size() == apres.Size()
							&& NkComponentDecl::StrEq(avant.Data(), apres.Data());
		// (b) LES MEMES NOMBRES qu'apres materialisation : une lecture qui
		//     diverge de l'ecriture serait pire que le defaut d'origine
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		bool memes = (uint32)st.doc.nodes[(uint32)iE].sommets.Size() == nbE;
		for (uint32 i = 0; memes && i < nbE; ++i)
			memes = st.doc.nodes[(uint32)iE].sommets[i].x == lus[i][0]
					&& st.doc.nodes[(uint32)iE].sommets[i].y == lus[i][1]
					&& st.doc.nodes[(uint32)iE].sommets[i].rayon == lus[i][2];
		// (c) 🔴 LE VOLET QU'UNE MUTATION A DESIGNE. Sans lui, ce cas ne
		//     couvrait que le REPLI sur la table reguliere : une mutation qui
		//     faisait ignorer la LISTE STOCKEE a `NkLireSommet` lui survivait
		//     28/28. Or c'est le pire des deux defauts -- le panneau montrerait
		//     le polygone regulier pendant que l'ecran montre la forme deformee.
		//     *Une garde qui ne teste que la branche facile rend un vert qui ment.*
		st.doc.nodes[(uint32)iE].sommets[3].x = -0.75f;
		st.doc.nodes[(uint32)iE].sommets[3].rayon = 12.f;
		float32 dx = 0.f, dy = 0.f, dr = 0.f;
		const bool litLeDeplace = NkLireSommet(st.doc.nodes[(uint32)iE], 3, dx, dy, dr)
								  && dx == -0.75f && dr == 12.f;
		char d[224];
		snprintf(d, sizeof(d),
				 "rect=%u sommets, etoile=%u ; %u octets %s ; memes nombres=%d ; sommet deplace "
				 "relu x=%.2f r=%.0f",
				 nbR, nbE, (uint32)avant.Size(), stable ? "octet pour octet" : "ONT BOUGE",
				 memes ? 1 : 0, (double)dx, (double)dr);
		verdict("28. LIRE un sommet (NkLireSommet / NkNbSommetsDe) n'ecrit RIEN dans le "
				"document, rend les MEMES nombres que la table materialisee, ET rend la LISTE "
				"STOCKEE des qu'elle existe (pas la table reguliere)",
				tousLus && stable && memes && litLeDeplace && avant.Size() > 0, d);
	}

	// ── 29. LA BOITE SE RECADRE SUR LE TRACE, ET LA FORME NE BOUGE PAS ─────
	//     Retour de Rodolf, 01/09 (soir) : « lorsqu'on modifie un objet par ses
	//     vertices, on doit redefinir sa bounding box pour la selection. »
	//
	// ⚠️ MESURE AVANT D'ECRIRE : le glisser d'un sommet n'ecrivait QUE
	//    `sommets[i].x/y`. `posX`, `posY`, `width` et `height` ne bougeaient pas.
	//    Tout ce qui lit la boite -- poignees, puce, Inspecteur, aimant, et
	//    surtout le POINTAGE -- lisait donc une boite perimee : on cliquait sur
	//    la forme sans la selectionner, et on la selectionnait en cliquant dans
	//    le vide.
	//
	// ⚠️ ET LE VRAI DANGER N'EST PAS L'ENGLOBANT, C'EST LE REFERENTIEL. Nos
	//    sommets sont UNITAIRES (-1..1 en fraction de la boite) : recadrer change
	//    leur unite. Recalculer sans renormaliser ferait SAUTER la forme a
	//    l'ecran au moment meme du relachement. Le cas exige donc les DEUX : la
	//    boite epouse le trace, ET chaque sommet retombe au meme point ABSOLU.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr); // 100 x 80, pose a l'origine
		{
			NkUINode &n = st.doc.nodes[(uint32)iR];
			n.posX = 20.f;
			n.posY = 10.f;
			NkMaterialiserSommets(n);
			// on TIRE le coin haut-gauche VERS L'EXTERIEUR : le trace deborde
			n.sommets[0].x = -2.f; // soit 100 px a gauche du bord gauche
			n.sommets[0].y = -1.5f;
		}
		// les points ABSOLUS avant recadrage -- c'est eux qui ne doivent pas bouger
		float32 avantX[16], avantY[16];
		uint32 nbAv = 0;
		{
			const NkUINode &n = st.doc.nodes[(uint32)iR];
			nbAv = (uint32)n.sommets.Size();
			for (uint32 i = 0; i < nbAv && i < 16; ++i) {
				avantX[i] = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
				avantY[i] = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
			}
		}
		const bool aRecadre = NkRecadrerNoeud(st.doc.nodes[(uint32)iR], true);
		const NkUINode &n = st.doc.nodes[(uint32)iR];
		// (a) LA BOITE A CHANGE, et elle vaut l'englobant du trace
		//     depart : x 20..120, y 10..90 ; sommet 0 tire a x=-2 -> 20-50=-30,
		//     y=-1.5 -> 10-20=-10. Englobant attendu : (-30,-10) 150 x 100.
		const bool boite = aRecadre && n.posX == -30.f && n.posY == -10.f
						   && n.width.value == 150.f && n.height.value == 100.f;
		// (b) LA FORME N'A PAS BOUGE D'UN PIXEL : chaque sommet retombe au meme
		//     point absolu. C'est la moitie que la renormalisation paie.
		float32 pireEcart = 0.f;
		for (uint32 i = 0; i < nbAv && i < 16; ++i) {
			const float32 X = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
			const float32 Y = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
			const float32 ex = X > avantX[i] ? X - avantX[i] : avantX[i] - X;
			const float32 ey = Y > avantY[i] ? Y - avantY[i] : avantY[i] - Y;
			if (ex > pireEcart)
				pireEcart = ex;
			if (ey > pireEcart)
				pireEcart = ey;
		}
		const bool immobile = pireEcart < 0.001f;
		// (c) LES MODES PASSENT A `Fixed` : ecrire une valeur dans un axe
		//     `expand` la laisserait sans effet, et la boite resterait fausse en
		//     silence.
		const bool fixes = n.width.mode == NkSizeMode::Fixed
						   && n.height.mode == NkSizeMode::Fixed;
		// (d) IDEMPOTENT : recadrer une forme deja recadree ne fait RIEN.
		const bool idem = !NkRecadrerNoeud(st.doc.nodes[(uint32)iR], true);
		char d[224];
		snprintf(d, sizeof(d), "boite=(%.0f,%.0f) %.0fx%.0f ; pire ecart %.5f px ; fixes=%d ; "
							   "2e appel change=%d",
				 (double)n.posX, (double)n.posY, (double)n.width.value,
				 (double)n.height.value, (double)pireEcart, fixes ? 1 : 0, idem ? 0 : 1);
		verdict("29. la boite se RECADRE sur le trace (englobant exact), la forme ne bouge pas "
				"d'un pixel (renormalisation), les modes passent a Fixed, et l'operation est "
				"IDEMPOTENTE",
				boite && immobile && fixes && idem, d);
	}

	// ── 30. DIX DEPLACEMENTS SUCCESSIFS NE FONT PAS DERIVER LA FORME ───────
	// ⚠️ C'EST LE VRAI RISQUE DE CE MECANISME, ET IL NE SE VOIT PAS SUR UN
	//    ALLER-RETOUR. Chaque recadrage divise puis multiplie par des largeurs
	//    differentes ; l'erreur d'arrondi du flottant s'ajoute a chaque tour. Une
	//    forme qui se decale d'un centieme de pixel par geste a bouge d'un pixel
	//    au centieme geste -- et personne ne saura d'ou ca vient.
	//
	// ⚠️ ET LE CAS MESURE UN POINT QUI N'EST PAS TOUCHE : on tire le sommet 2 dix
	//    fois, et on surveille le sommet 0. Mesurer le sommet TIRE ne dirait
	//    rien -- il est cense bouger.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iE = poser("etoile", nullptr);
		st.doc.nodes[(uint32)iE].posX = 40.f;
		st.doc.nodes[(uint32)iE].posY = 40.f;
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		auto absolu = [&](uint32 i, float32 &X, float32 &Y) {
			const NkUINode &n = st.doc.nodes[(uint32)iE];
			X = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
			Y = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
		};
		float32 x0 = 0.f, y0 = 0.f;
		absolu(0, x0, y0);
		for (uint32 tour = 0; tour < 10; ++tour) {
			NkUINode &n = st.doc.nodes[(uint32)iE];
			// on pousse le sommet 2 d'un cran vers l'exterieur, puis on recadre
			n.sommets[2].x += 0.3f;
			n.sommets[2].y += 0.2f;
			NkRecadrerNoeud(n, true);
		}
		float32 x1 = 0.f, y1 = 0.f;
		absolu(0, x1, y1);
		const float32 dx = x1 > x0 ? x1 - x0 : x0 - x1;
		const float32 dy = y1 > y0 ? y1 - y0 : y0 - y1;
		const bool stable = dx < 0.01f && dy < 0.01f;
		char d[192];
		snprintf(d, sizeof(d),
				 "sommet 0 (jamais touche) : (%.4f,%.4f) -> (%.4f,%.4f), derive %.5f px",
				 (double)x0, (double)y0, (double)x1, (double)y1,
				 (double)(dx > dy ? dx : dy));
		verdict("30. DIX deplacements + recadrages successifs ne font pas deriver un sommet "
				"qu'on n'a pas touche (l'erreur cumulee est le vrai risque)",
				stable, d);
	}

	// ── 31. UN COTE PLAT NE DIVISE PAS, ET UN PARENT CALCULE REFUSE ────────
	// ⚠️ DEUX REFUS, ET ILS SE MESURENT PLUTOT QUE DE SE SUPPOSER.
	//    (a) trois sommets alignes donnent une largeur nulle : sans plancher, la
	//        renormalisation divise par zero et rend des NaN -- qui se propagent
	//        en SILENCE jusqu'au dessin, ou ils n'affichent plus rien du tout ;
	//    (b) sous un agencement calcule, `posX`/`posY` sont IGNORES : y ecrire un
	//        decalage donnerait une valeur sans effet pendant que le trace, lui,
	//        aurait bouge. On rend faux, et l'appelant a une raison a dire.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iT = poser("triangle", nullptr);
		{
			NkUINode &n = st.doc.nodes[(uint32)iT];
			NkMaterialiserSommets(n);
			for (uint32 i = 0; i < (uint32)n.sommets.Size(); ++i)
				n.sommets[i].x = 0.f; // tous alignes : largeur du trace = 0
		}
		const bool aPlat = NkRecadrerNoeud(st.doc.nodes[(uint32)iT], true);
		const NkUINode &t = st.doc.nodes[(uint32)iT];
		bool sain = t.width.value >= 1.f && t.height.value > 0.f;
		for (uint32 i = 0; i < (uint32)t.sommets.Size(); ++i) {
			// un NaN n'est egal a rien, pas meme a lui-meme : c'est comme ca
			// qu'on l'attrape sans <math.h>.
			const float32 v = t.sommets[i].x;
			if (!(v == v) || !(t.sommets[i].y == t.sommets[i].y))
				sain = false;
		}
		// (b) le meme geste, parent NON libre
		const int32 iR2 = poser("rect", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR2]);
		st.doc.nodes[(uint32)iR2].sommets[0].x = -3.f;
		const bool refuse = !NkRecadrerNoeud(st.doc.nodes[(uint32)iR2], false);
		char d[192];
		snprintf(d, sizeof(d), "plat : %.0fx%.0f, aucun NaN=%d, a recadre=%d ; parent calcule "
							   "refuse=%d",
				 (double)t.width.value, (double)t.height.value, sain ? 1 : 0, aPlat ? 1 : 0,
				 refuse ? 1 : 0);
		verdict("31. un cote PLAT garde un plancher et ne produit AUCUN NaN, et un parent qui "
				"place ses enfants lui-meme REFUSE le recadrage",
				sain && refuse, d);
	}

	// ── 32. LA TROISIEME TABLE DE SELECTION, ET LA MULTI-SELECTION DE SOMMETS ─
	//     Retour de Rodolf, 01/09 (soir) : « on doit pouvoir selectionner
	//     plusieurs vertices pour deplacement ou transformation simultanes. »
	//
	// ⚠️ LA DECISION EST PRISE ET TENUE : les sommets suivent la table de la
	//    LISTE, pas celle de la toile -- `Profond` n'a de sens que la ou il y a
	//    une profondeur a traverser, et il n'y a rien sous un sommet. La source
	//    le confirme (`lunacy.docs.icons8.com/editing_shapes/` : « drag over them
	//    or hold down Shift when clicking several points »). Le cas tient l'ECART
	//    avec la table de la toile, sans quoi le premier lecteur le
	//    « corrigerait » en croyant reparer un oubli.
	{
		const bool table = NkGesteSommet(false, false) == NkGesteSel::Remplacer
						   && NkGesteSommet(false, true) == NkGesteSel::Basculer
						   // ⚠️ L'ECART AVEC LA TOILE, TENU EXPRES :
						   && NkGesteSommet(true, false) == NkGesteSel::Remplacer
						   && NkGesteToile(true, false) == NkGesteSel::Profond;
		// l'etat de multi-selection lui-meme
		DesignState::NkModeForme m;
		m.MarquerSeul(3);
		const bool seul = m.Marque(3) && m.NbMarques() == 1 && m.sommet == 3;
		m.BasculerMarque(5);
		m.BasculerMarque(7);
		const bool trois = m.NbMarques() == 3 && m.sommet == 7 && m.Marque(5);
		// ⚠️ RETIRER LE PRINCIPAL LUI FAIT ELIRE UN SUCCESSEUR : sans ca,
		//    l'Inspecteur afficherait les coordonnees d'un point qui n'est plus
		//    selectionne -- la faute exacte que `sel`/`selected` evite.
		m.BasculerMarque(7);
		const bool successeur = m.NbMarques() == 2 && m.sommet >= 0 && m.Marque(m.sommet);
		// ⚠️ ON RETIENT LA VALEUR AVANT `Quitter`, sinon le message imprimerait
		//    « sommet=-1 » a cote d'un verdict qui a mesure autre chose. *Un
		//    chiffre porte sa provenance.*
		const int32 successeurLu = m.sommet;
		// et sortir efface TOUT (le quatrieme champ que `Quitter` doit porter)
		m.Quitter();
		const bool vide = m.NbMarques() == 0 && m.sommet == -1 && m.noeud == -1;
		char d[224];
		snprintf(d, sizeof(d), "table=%d seul=%d trois=%d successeur(sommet=%d)=%d vide=%d",
				 table ? 1 : 0, seul ? 1 : 0, trois ? 1 : 0, successeurLu, successeur ? 1 : 0,
				 vide ? 1 : 0);
		verdict("32. la table des SOMMETS (Maj bascule, Ctrl ne fait rien -- l'ecart avec la "
				"toile est voulu), et le principal se DERIVE de l'ensemble",
				table && seul && trois && successeur && vide, d);
	}

	// ── 32bis. LES SOMMETS MARQUES BOUGENT ENSEMBLE, ET D'UN ECART ─────────
	// ⚠️ LE MECANISME EST DESCENDU DANS `Sommets.h` POUR CE CAS. Ecrit dans la
	//    boucle de rendu, il aurait vecu la ou aucun banc ne va -- la facture que
	//    ce chantier a deja payee quatre fois -- et « les sommets bougent
	//    ensemble » serait reste une opinion.
	//
	// ⚠️ ET LE CAS VISE LA FAUTE PRECISE, PAS « ca bouge ». Poser chaque sommet
	//    marque SOUS la souris les ferait tous se SUPERPOSER au premier pixel du
	//    geste : la forme s'effondrerait en un point. Le sous-cas (b) mesure donc
	//    que les sommets restent DISTINCTS et gardent leur ecart mutuel -- ce
	//    qu'un simple « ils ont bouge » ne verrait pas.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		NkUINode &e = st.doc.nodes[(uint32)iE];
		// marques : 0 (tire), 2 et 5
		const nkentseu::uint64 marq = (1ull << 0) | (1ull << 2) | (1ull << 5);
		const float32 ax0 = e.sommets[0].x, ay0 = e.sommets[0].y;
		const float32 ax2 = e.sommets[2].x, ay2 = e.sommets[2].y;
		const float32 ax3 = e.sommets[3].x, ay3 = e.sommets[3].y; // NON marque
		const float32 ax5 = e.sommets[5].x;
		const uint32 bouges = NkDeplacerSommetsMarques(e, 0, marq, ax0 + 0.4f, ay0 - 0.25f);
		// (a) le sommet TIRE est exactement ou on l'a demande
		const bool tireExact = e.sommets[0].x == ax0 + 0.4f && e.sommets[0].y == ay0 - 0.25f;
		// (b) les autres MARQUES ont pris le MEME ECART -- ils ne se sont pas
		//     poses sur la souris (l'ecart 2<->5 est conserve)
		const bool memeEcart = e.sommets[2].x == ax2 + 0.4f && e.sommets[2].y == ay2 - 0.25f
							   && e.sommets[5].x == ax5 + 0.4f;
		const bool distincts = e.sommets[2].x != e.sommets[0].x
							   || e.sommets[2].y != e.sommets[0].y;
		// (c) le NON marque n'a pas bouge d'un iota
		const bool intact = e.sommets[3].x == ax3 && e.sommets[3].y == ay3;
		char d[192];
		snprintf(d, sizeof(d), "%u sommet(s) deplace(s) sur 10 ; tire exact=%d meme ecart=%d "
							   "distincts=%d ; le non marque intact=%d",
				 bouges, tireExact ? 1 : 0, memeEcart ? 1 : 0, distincts ? 1 : 0,
				 intact ? 1 : 0);
		verdict("32bis. les sommets MARQUES bougent ensemble d'un ECART (ils ne se superposent "
				"pas sous la souris), et ceux qui ne le sont pas ne bougent pas",
				bouges == 3 && tireExact && memeEcart && distincts && intact, d);
	}

	// ── 33. LE DOUBLE-CLIC SUR UN SOMMET APPARTIENT AU MODE ────────────────
	//     Retour (3) de Rodolf : il redemande l'arrondi au double-clic.
	//
	// 🔴 ET LA RE-MESURE LUI DONNE RAISON : le double-clic avait DEUX lecteurs.
	//    La toile arrondit le sommet ; `HandleMouse` relisait le meme evenement
	//    et -- depuis le correctif du forage a deux temps de ce matin --
	//    RE-ENTRAIT dans le mode (`Quitter()` puis `noeud = cand`). L'arrondi
	//    passait, mais `sommet` retombait a -1, la section « EDITION DE FORME »
	//    se vidait, et le message « Sommet 3 arrondi » etait remplace par celui
	//    du forage. *Un geste, deux lecteurs, et lequel gagne ne dependait que de
	//    l'ordre du code* -- la meme classe de defaut que la collision de
	//    poignees, tranchee du meme cote.
	{
		// (a) le mode garde son double-clic sur SON noeud
		const bool aMoi = NkDblClicAuModeForme(7, 7, 7);
		// (b) mais pas sur un AUTRE noeud : double-cliquer ailleurs doit
		//     continuer de forer/selectionner normalement
		const bool pasAilleurs = !NkDblClicAuModeForme(7, 7, 9);
		// (c) ni quand le mode n'est pas arme (selection differente)
		const bool pasSiDesarme = !NkDblClicAuModeForme(7, 9, 7);
		// (d) ni quand il n'y a pas de mode du tout
		const bool pasSansMode = !NkDblClicAuModeForme(-1, 7, 7);
		// (e) ET L'ARRONDI LUI-MEME MARCHE ENCORE -- le cycle 0/8/16/32, sur un
		//     rect a enfants comme ceux de son document
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iC = poser("rect", nullptr);
		st.doc.AddChild(iC, "", NkAuthor::Humain); // un rect QUI PORTE un enfant
		const float32 r1 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r2 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r3 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r4 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
		char d[192];
		snprintf(d, sizeof(d), "a moi=%d ailleurs=%d desarme=%d sans mode=%d ; cycle %.0f %.0f "
							   "%.0f %.0f",
				 aMoi ? 1 : 0, pasAilleurs ? 1 : 0, pasSiDesarme ? 1 : 0, pasSansMode ? 1 : 0,
				 (double)r1, (double)r2, (double)r3, (double)r4);
		verdict("33. le double-clic sur un sommet appartient au MODE (et a lui seul), et "
				"l'arrondi cycle encore 0/8/16/32 sur un rect QUI PORTE DES ENFANTS",
				aMoi && pasAilleurs && pasSiDesarme && pasSansMode && cycle, d);
	}

	printf("\nRECETTE POINTS : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}

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
static nkentseu::int32 RecetteTransfo() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	auto proche = [](float32 a, float32 b, float32 tol) {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= tol;
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};

	// ── 1. L'IDENTITE NE TOUCHE A RIEN ─────────────────────────────────────
	//     ⚠️ CE CAS N'EST PAS DECORATIF : sans le court-circuit `Identite()`,
	//     chaque point de chaque forme repasserait par un aller-retour en
	//     flottant qui le deplacerait d'un ulp, et un document NON TOURNE
	//     cesserait de se dessiner au pixel pres. Le defaut serait invisible a
	//     l'oeil et visible dans toutes les captures comparees.
	{
		NkTransfo t;
		float32 x = 137.5f, y = -42.25f;
		NkTransfoPoint(t, 10.f, 20.f, x, y);
		const bool ok = t.Identite() && x == 137.5f && y == -42.25f;
		char d[96];
		snprintf(d, sizeof(d), "(137.5,-42.25) -> (%.4f,%.4f)", x, y);
		verdict("1. la transformation neutre ne deplace aucun point, AU BIT PRES", ok, d);
	}
	// ── 2. ALLER-RETOUR : l'inverse est vraiment l'inverse ─────────────────
	//     ⚠️ ET LE CAS QUI COMPTE EST « miroir ET rotation ENSEMBLE ». Un inverse
	//     qui garderait l'ordre direct marche parfaitement tant qu'il n'y a QUE
	//     l'un des deux -- il ne tombe que dans le cas mixte, le plus rare, donc
	//     le dernier trouve. C'est pour ca qu'il est teste en premier.
	{
		bool ok = true;
		float32 pireEcart = 0.f;
		const float32 angles[5] = {0.f, 30.f, 90.f, 187.5f, -45.f};
		for (uint32 a = 0; a < 5; ++a)
			for (uint32 m = 0; m < 4; ++m) {
				NkTransfo t;
				t.deg = angles[a];
				t.mh = (m & 1u) != 0u;
				t.mv = (m & 2u) != 0u;
				float32 x = 300.f, y = 120.f;
				NkTransfoPoint(t, 200.f, 150.f, x, y);
				NkTransfoPointInverse(t, 200.f, 150.f, x, y);
				const float32 e1 = x - 300.f, e2 = y - 120.f;
				const float32 e = (e1 < 0.f ? -e1 : e1) + (e2 < 0.f ? -e2 : e2);
				if (e > pireEcart)
					pireEcart = e;
				if (e > 0.01f)
					ok = false;
			}
		char d[96];
		snprintf(d, sizeof(d), "20 combinaisons (5 angles x 4 miroirs), pire ecart %.5f px",
				 pireEcart);
		verdict("2. l'inverse est l'inverse, MIROIR ET ROTATION MELANGES", ok, d);
	}
	// ── 3. LE QUART DE TOUR, SUR DES NOMBRES ECRITS A LA MAIN ──────────────
	//     Un cas dont on connait la reponse sans calculer : le coin haut-gauche
	//     d'un carre tourne de 90 degres horaires part en haut-droite.
	{
		NkTransfo t;
		t.deg = 90.f;
		const NkPaintRect r = {100.f, 100.f, 200.f, 200.f}; // centre (200,200)
		float32 x = 100.f, y = 100.f;
		NkTransfoPoint(t, 200.f, 200.f, x, y);
		const bool ok = proche(x, 300.f, 0.01f) && proche(y, 100.f, 0.01f);
		char d[96];
		snprintf(d, sizeof(d), "(100,100) -> (%.2f,%.2f), attendu (300,100)", x, y);
		verdict("3. 90 degres horaires : le coin haut-gauche passe en haut-droite", ok, d);
		(void)r;
	}
	// ── 4. LE MIROIR HORIZONTAL RETOURNE, ET SEULEMENT EN X ────────────────
	{
		NkTransfo t;
		t.mh = true;
		float32 x = 100.f, y = 130.f;
		NkTransfoPoint(t, 200.f, 200.f, x, y);
		const bool ok = proche(x, 300.f, 0.001f) && proche(y, 130.f, 0.001f);
		char d[96];
		snprintf(d, sizeof(d), "(100,130) -> (%.2f,%.2f), attendu (300,130)", x, y);
		verdict("4. le miroir H retourne en X et LAISSE Y", ok, d);
	}
	// ── 5. L'ORDRE COMPTE, ET ON LE PROUVE ─────────────────────────────────
	//     ⚠️ CE CAS EXISTE POUR QUE PERSONNE NE « SIMPLIFIE » L'ORDRE UN JOUR.
	//     Miroir-puis-rotation et rotation-puis-miroir donnent des resultats
	//     DIFFERENTS (le miroir change le signe de l'angle). Si les deux donnaient
	//     la meme chose, l'ordre serait un detail -- ce cas montre qu'il n'en est
	//     pas un, donc que le commentaire qui le fixe est necessaire.
	{
		NkTransfo t;
		t.deg = 45.f;
		t.mh = true;
		float32 x1 = 300.f, y1 = 200.f;
		NkTransfoPoint(t, 200.f, 200.f, x1, y1); // miroir puis rotation (le notre)
		// l'ordre inverse, ecrit a la main pour la comparaison
		float32 dx = 300.f - 200.f, dy = 200.f - 200.f;
		float32 s = 0.f, c = 1.f;
		NkSinCosDeg(45.f, s, c);
		float32 rx = dx * c - dy * s, ry = dx * s + dy * c;
		rx = -rx; // miroir APRES
		const float32 x2 = 200.f + rx, y2 = 200.f + ry;
		const bool different = !proche(x1, x2, 1.f) || !proche(y1, y2, 1.f);
		char d[112];
		snprintf(d, sizeof(d), "notre ordre (%.1f,%.1f) vs ordre inverse (%.1f,%.1f)", x1, y1, x2,
				 y2);
		verdict("5. miroir-puis-rotation N'EST PAS rotation-puis-miroir : l'ordre fixe dans le "
				"fichier est une decision, pas un detail",
				different, d);
	}
	// ── 6. LA PROPAGATION AUX ENFANTS, ET LES MIROIRS QUI S'ANNULENT ───────
	//     ⚠️ LE PIEGE EST DANS LA COMPOSITION DES MIROIRS. Un OU logique aurait
	//     rendu « retourne » un enfant retourne sous un parent retourne -- alors
	//     qu'il doit revenir a l'endroit. La moitie des cas faux, et INVISIBLE
	//     sur toute forme symetrique : on ne l'aurait vu que sur du texte.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 g = st.doc.AddChild(0, "", NkAuthor::Humain);
		const int32 e = st.doc.AddChild(g, "", NkAuthor::Humain);
		const int32 pf = st.doc.AddChild(e, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)g].rotation = 30.f;
		st.doc.nodes[(uint32)g].miroirH = true;
		st.doc.nodes[(uint32)e].rotation = 15.f;
		st.doc.nodes[(uint32)e].miroirH = true; // s'annule avec celui du parent
		st.doc.nodes[(uint32)pf].rotation = 5.f;
		st.doc.nodes[(uint32)pf].miroirV = true;
		const NkTransfo tg = NkTransfoEffective(st.doc, g);
		const NkTransfo te = NkTransfoEffective(st.doc, e);
		const NkTransfo tp = NkTransfoEffective(st.doc, pf);
		const bool ok = tg.deg == 30.f && tg.mh && !tg.mv && te.deg == 45.f && !te.mh && !te.mv
						&& tp.deg == 50.f && !tp.mh && tp.mv;
		char d[144];
		snprintf(d, sizeof(d), "groupe %.0f/H=%d ; enfant %.0f/H=%d ; petit-fils %.0f/H=%d,V=%d",
				 tg.deg, tg.mh ? 1 : 0, te.deg, te.mh ? 1 : 0, tp.deg, tp.mh ? 1 : 0,
				 tp.mv ? 1 : 0);
		verdict("6. la rotation S'ACCUMULE le long des ancetres et les miroirs S'ANNULENT deux "
				"a deux",
				ok, d);
	}
	// ── 7. LE PICKING D'UN RECTANGLE TOURNE ────────────────────────────────
	//     ⚠️ « UN CLIC DANS UN RECTANGLE TOURNE N'EST PLUS UN TEST DE
	//     RECTANGLE » (arbitrage Q42, point 3). Le cas prend un point qui est
	//     DANS la boite droite et HORS de la forme tournee, et un autre qui fait
	//     l'inverse -- sans ces deux-la, un picking qui ignorerait la rotation
	//     passerait au vert.
	{
		NkTransfo t;
		t.deg = 45.f;
		const NkPaintRect r = {100.f, 180.f, 200.f, 40.f}; // centre (200,200)
		// le centre est dans les deux cas
		const bool centre = NkPointDansRectTransfo(t, r, 200.f, 200.f);
		// un point pres du bord GAUCHE de la boite droite : dans la boite,
		// HORS de la forme tournee de 45 degres
		const bool horsApres = !NkPointDansRectTransfo(t, r, 110.f, 200.f);
		// ⚠️ ET LE POINT INVERSE EST CALCULE, PAS DEVINE -- MA PREMIERE SCENE
		//    ETAIT FAUSSE, PAS LE CODE. J'avais pris (200,260), « en dessous de
		//    la boite » : il est aussi en dehors de la forme tournee, et le cas
		//    echouait en accusant le picking. La barre tournee de 45 degres
		//    s'etend le long de la DIAGONALE : un point a 60 px du centre dans
		//    cette direction vaut (200 + 60/racine(2), 200 + 60/racine(2)).
		//    C'est exactement la lecon des scenes de snap de Q42 -- « dans un
		//    banc de geometrie, une scene mal choisie fabrique le contre-exemple
		//    qu'on cherchait a eviter, et on accuse le calcul ».
		const float32 dd = 60.f * 0.70710678f; // 42,43 px sur chaque axe
		const bool dansApres = NkPointDansRectTransfo(t, r, 200.f + dd, 200.f + dd);
		char d[128];
		snprintf(d, sizeof(d), "centre=%d, (110,200) exclu=%d, (%.0f,%.0f) inclus=%d",
				 centre ? 1 : 0, horsApres ? 1 : 0, 200.f + dd, 200.f + dd, dansApres ? 1 : 0);
		verdict("7. le picking suit la forme TOURNEE, pas sa boite : un point de la boite en "
				"sort, un point hors de la boite y entre",
				centre && horsApres && dansApres, d);
	}
	// ── 8. L'ENGLOBANT D'UN OBJET TOURNE EST PLUS GRAND QUE SA BOITE ───────
	//     Une carte de 200x40 tournee de 45 degres occupe un carre bien plus
	//     large. Un survol qui lirait la boite droite dessinerait un cadre qui
	//     COUPE l'objet.
	{
		NkTransfo t;
		t.deg = 45.f;
		const NkPaintRect r = {100.f, 180.f, 200.f, 40.f};
		const NkPaintRect e = NkEnglobantTransfo(t, r);
		const float32 attendu = (200.f + 40.f) * 0.70710678f; // (w+h)/racine(2)
		const bool ok = proche(e.w, attendu, 0.5f) && proche(e.h, attendu, 0.5f)
						&& proche(e.x + e.w * 0.5f, 200.f, 0.01f);
		char d[128];
		snprintf(d, sizeof(d), "200x40 a 45 deg -> %.1fx%.1f (attendu %.1f), centre conserve",
				 e.w, e.h, attendu);
		verdict("8. l'englobant d'un objet tourne grandit, et son centre ne bouge pas", ok, d);
	}
	// ── 9. LES POIGNEES DE ROTATION SONT DEHORS ────────────────────────────
	//     ⚠️ « LE COIN REDIMENSIONNE, SON EXTERIEUR TOURNE. » Posees SUR les
	//     coins, elles auraient vole le geste de redimensionnement, cent fois
	//     plus frequent. Le cas verifie qu'aucune des quatre ne CHEVAUCHE la
	//     boite -- pas seulement qu'elles sont « a peu pres la ».
	{
		const NkPaintRect r = {100.f, 100.f, 200.f, 150.f};
		bool toutesDehors = true;
		for (uint32 k = 0; k < NkNbPoigneesRotation(); ++k) {
			const NkPaintRect p = NkPoigneeRotation(r, k, 12.f);
			const bool chevauche = p.x < r.x + r.w && p.x + p.w > r.x && p.y < r.y + r.h
								   && p.y + p.h > r.y;
			if (chevauche)
				toutesDehors = false;
		}
		const NkPaintRect p0 = NkPoigneeRotation(r, 0, 12.f);
		const NkPaintRect p2 = NkPoigneeRotation(r, 2, 12.f);
		const bool enDiagonale = p0.x < r.x && p0.y < r.y && p2.x > r.x + r.w
								 && p2.y > r.y + r.h;
		char d[128];
		snprintf(d, sizeof(d), "%u poignees, coin 0 a (%.0f,%.0f), coin 2 a (%.0f,%.0f)",
				 NkNbPoigneesRotation(), p0.x, p0.y, p2.x, p2.y);
		verdict("9. les 4 poignees de rotation sont DEHORS et en diagonale : elles ne volent "
				"pas le geste de redimensionnement",
				toutesDehors && enDiagonale, d);
	}
	// ── 10. L'ANGLE MESURE ET SON AIMANT ───────────────────────────────────
	{
		const bool droite = proche(NkAngleDeg(0.f, 0.f, 100.f, 0.f), 0.f, 0.2f);
		const bool bas = proche(NkAngleDeg(0.f, 0.f, 0.f, 100.f), 90.f, 0.2f);
		const bool diag = proche(NkAngleDeg(0.f, 0.f, 100.f, 100.f), 45.f, 0.2f);
		const bool gauche = proche(NkAngleDeg(0.f, 0.f, -100.f, 0.f), 180.f, 0.2f);
		// l'aimant : 15 degres, et SEULEMENT quand Maj est tenue
		const bool aimant = NkAngleAimante(43.f, true) == 45.f && NkAngleAimante(43.f, false) == 43.f
							&& NkAngleAimante(-8.f, true) == -15.f;
		// et un angle est une DIRECTION, pas un compteur de tours
		const bool norm = NkAngleNormalise(370.f) == 10.f && NkAngleNormalise(-90.f) == 270.f
						  && NkAngleNormalise(0.f) == 0.f;
		char d[128];
		snprintf(d, sizeof(d), "0/90/45/180 = %.1f/%.1f/%.1f/%.1f ; aimant %s ; normalise %s",
				 NkAngleDeg(0.f, 0.f, 100.f, 0.f), NkAngleDeg(0.f, 0.f, 0.f, 100.f),
				 NkAngleDeg(0.f, 0.f, 100.f, 100.f), NkAngleDeg(0.f, 0.f, -100.f, 0.f),
				 aimant ? "ok" : "FAUX", norm ? "ok" : "FAUX");
		verdict("10. l'angle sous la souris, son aimant a 15 degres (Maj SEULEMENT) et sa "
				"normalisation dans [0,360)",
				droite && bas && diag && gauche && aimant && norm, d);
	}
	// ── 11. CE QUE LE PEINTRE NE SAIT PAS FAIRE EST DIT, PAS CACHE ─────────
	//     ⚠️ MESURE A LA SOURCE : `Text()` prend un `NkPaintRect`, droit par
	//     construction -- il n'existe aucun moyen de peindre du texte tourne avec
	//     ce peintre. Le champ s'enregistre quand meme (sinon tourner un groupe
	//     qui contient un texte echouerait A MOITIE, sans qu'on sache pourquoi),
	//     et l'application LE DIT. Un cas le tient, pas une note.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iR = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iR].shape = NkString("rect");
		const int32 iT = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iT].shape = NkString("text");
		st.doc.nodes[(uint32)iT].text = NkString("bonjour");
		const int32 iRT = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iRT].shape = NkString("rect");
		st.doc.nodes[(uint32)iRT].text = NkString("j'ai du texte");
		const char *r = NkRaisonRotationTexte();
		const bool ok = NkPeintureSaitTourner(st.doc.nodes[(uint32)iR])
						&& !NkPeintureSaitTourner(st.doc.nodes[(uint32)iT])
						// ET un rect QUI PORTE du texte non plus : la meme regle que
						// dans la table du double-clic, prise au meme endroit
						&& !NkPeintureSaitTourner(st.doc.nodes[(uint32)iRT]) && r && *r;
		verdict("11. le peintre sait tourner une forme, PAS un texte (ni un rect qui en porte), "
				"et la raison est DITE",
				ok, ok ? "rect oui, texte non, rect-a-texte non, phrase non vide" : "UNE MUETTE");
	}
	// ── 12. CONSERVATION + ALLER-RETOUR ────────────────────────────────────
	//     ⚠️ LE VOLET CONSERVATION, ECRIT D'ENTREE. Deux exigences ENSEMBLE,
	//     parce que l'une seule passe au vert sur une perte : (a) un document
	//     SANS transformation se reecrit octet pour octet (les trois cles ne sont
	//     pas ecrites), (b) les trois valeurs se retrouvent apres aller-retour.
	//     C'est la lecon de la mutation 7 de Q42, portee ici au lieu d'y etre
	//     redecouverte.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iA = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iA].shape = NkString("rect");
		NkString sans, sans2;
		st.doc.Save(sans);
		NkUIDocument d0;
		const bool lu0 = d0.Load(sans.Data());
		d0.Save(sans2);
		const bool propre = !sans.Contains("rotation") && !sans.Contains("miroir_h")
							&& !sans.Contains("miroir_v");
		const bool stable = lu0 && sans.Size() == sans2.Size()
							&& NkComponentDecl::StrEq(sans.Data(), sans2.Data());
		// (b) avec les trois
		st.doc.nodes[(uint32)iA].rotation = 42.5f;
		st.doc.nodes[(uint32)iA].miroirH = true;
		st.doc.nodes[(uint32)iA].miroirV = true;
		NkString avec;
		st.doc.Save(avec);
		NkUIDocument d1;
		const bool lu1 = d1.Load(avec.Data());
		float32 rot = -1.f;
		bool mh = false, mv = false;
		for (uint32 i = 1; lu1 && i < (uint32)d1.nodes.Size(); ++i)
			if (d1.nodes[i].rotation != 0.f) {
				rot = d1.nodes[i].rotation;
				mh = d1.nodes[i].miroirH;
				mv = d1.nodes[i].miroirV;
			}
		char d[176];
		snprintf(d, sizeof(d), "sans : %s%s (%u o) | avec : %.1f deg, H=%d, V=%d",
				 propre ? "aucune cle" : "CLE PARASITE",
				 stable ? ", octet pour octet" : ", A BOUGE", (uint32)sans.Size(), rot,
				 mh ? 1 : 0, mv ? 1 : 0);
		verdict("12. CONSERVATION : un document sans transformation n'ecrit aucune des trois "
				"cles et se reecrit octet pour octet ; les trois valeurs survivent a "
				"l'aller-retour",
				propre && stable && rot == 42.5f && mh && mv, d);
	}
	// ── 13. LA COPIE NE REDRESSE PAS L'OBJET ───────────────────────────────
	//     ⚠️ LE CHEMIN FRERE : `CopierSousArbre` et la copie de TRANSPOSITION
	//     recopient toutes deux un noeud, membre a membre, a deux endroits
	//     eloignes du meme fichier. Oublier les trois champs dans l'une aurait
	//     donne une copie de meme forme et d'orientation differente -- et c'est
	//     le genre de perte qu'on ne voit pas sur une forme symetrique.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iA = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iA].shape = NkString("triangle");
		st.doc.nodes[(uint32)iA].rotation = 33.f;
		st.doc.nodes[(uint32)iA].miroirV = true;
		NkString a1;
		st.doc.Save(a1);
		NkUIDocument src;
		src.Load(a1.Data());
		// la copie passe par le meme chemin que Ctrl+C / Ctrl+V
		const int32 copie = st.doc.CopierSousArbre(src, iA, 0);
		const bool ok = st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].rotation == 33.f
						&& st.doc.nodes[(uint32)copie].miroirV
						&& !st.doc.nodes[(uint32)copie].miroirH;
		char d[112];
		snprintf(d, sizeof(d), "copie : %.0f deg, H=%d, V=%d",
				 st.doc.IsValidIndex(copie) ? st.doc.nodes[(uint32)copie].rotation : -1.f,
				 (st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].miroirH) ? 1 : 0,
				 (st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].miroirV) ? 1 : 0);
		verdict("13. copier un objet tourne et retourne le garde tourne et retourne", ok, d);
	}
	// ── 14. UN GROUPE TOURNE D'UN BLOC : SES ENFANTS SE DEPLACENT ──────────
	//     ⚠️ C'EST LE CAS QUI M'A EMPECHE D'ECRIRE UNE FAUSSE PROPAGATION, ET IL
	//     FAUT LE DIRE. J'allais me servir de l'ANGLE CUMULE (`NkTransfoEffective`)
	//     pour peindre les enfants d'un groupe tourne. C'est faux : chaque ancetre
	//     tourne autour de SON PROPRE CENTRE, pas de celui de l'enfant. Un angle
	//     cumule fait pivoter chaque enfant SUR LUI-MEME -- le groupe se disloque
	//     au lieu de tourner d'un bloc, et chaque element reste obstinement a sa
	//     place.
	//
	//     Le cas prend un enfant DECENTRE dans son groupe : c'est le seul qui
	//     distingue les deux. Un enfant centre sur le centre du groupe donnerait
	//     LE MEME resultat avec la bonne et la mauvaise formule -- et le banc
	//     serait passe au vert sur la faute exacte qu'il doit attraper.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 g = st.doc.AddChild(0, "", NkAuthor::Humain);
		NkUINode &gn = st.doc.nodes[(uint32)g];
		gn.shape = NkString("rect");
		gn.layout.kind = NkLayoutKind::Free;
		gn.width.mode = NkSizeMode::Fixed;
		gn.width.value = 200.f;
		gn.height.mode = NkSizeMode::Fixed;
		gn.height.value = 200.f;
		const int32 e = st.doc.AddChild(g, "", NkAuthor::Humain);
		NkUINode &en = st.doc.nodes[(uint32)e];
		en.shape = NkString("rect");
		en.posX = 150.f; // DECENTRE : c'est ce qui rend le cas discriminant
		en.posY = 80.f;
		en.width.mode = NkSizeMode::Fixed;
		en.width.value = 40.f;
		en.height.mode = NkSizeMode::Fixed;
		en.height.value = 40.f;
		st.Recompute(surface);
		const NkPaintRect rg = st.layout.At(g), re = st.layout.At(e);
		const float32 cgx = rg.x + rg.w * 0.5f, cgy = rg.y + rg.h * 0.5f;
		const float32 cex = re.x + re.w * 0.5f, cey = re.y + re.h * 0.5f;
		// sans rotation, la matrice effective est l'identite
		const bool neutre = NkMatEffective(st.doc, st.layout, e).Identite();
		// on tourne LE GROUPE d'un quart de tour
		st.doc.nodes[(uint32)g].rotation = 90.f;
		const NkMat2D me = NkMatEffective(st.doc, st.layout, e);
		float32 x = cex, y = cey;
		NkMatPoint(me, x, y);
		// le centre de l'enfant doit tourner de 90 degres AUTOUR DU CENTRE DU
		// GROUPE : (dx,dy) -> (-dy,dx)
		const float32 ax = cgx - (cey - cgy), ay = cgy + (cex - cgx);
		const bool bouge = proche(x, ax, 0.5f) && proche(y, ay, 0.5f);
		// et il A VRAIMENT BOUGE (sinon « proche » serait vrai par accident sur
		// un enfant centre) : c'est la garde qui rend le cas discriminant
		const bool aBouge = !proche(x, cex, 1.f) || !proche(y, cey, 1.f);
		char d[160];
		snprintf(d, sizeof(d), "enfant (%.0f,%.0f) -> (%.0f,%.0f), attendu (%.0f,%.0f)%s", cex,
				 cey, x, y, ax, ay, aBouge ? "" : " [IL N'A PAS BOUGE]");
		verdict("14. tourner un GROUPE deplace ses enfants autour du centre DU GROUPE : il "
				"tourne d'un bloc, il ne se disloque pas",
				neutre && bouge && aBouge, d);
	}
	// ── 15. LA MATRICE ET SON INVERSE, ET LE PICKING QUI EN DECOULE ────────
	{
		st.doc.nodes[1].rotation = 37.f; // le groupe du cas precedent
		const NkMat2D m = NkMatEffective(st.doc, st.layout, 2);
		const NkMat2D inv = NkMatInverse(m);
		float32 x = 321.f, y = 654.f;
		NkMatPoint(m, x, y);
		NkMatPoint(inv, x, y);
		const bool rond = proche(x, 321.f, 0.01f) && proche(y, 654.f, 0.01f);
		// et le picking de l'enfant suit : son centre est toujours dedans, ou
		// qu'il soit parti
		const NkPaintRect re = st.layout.At(2);
		float32 cx = re.x + re.w * 0.5f, cy = re.y + re.h * 0.5f;
		NkMatPoint(m, cx, cy);
		const bool dedans = NkPointDansNoeud(st.doc, st.layout, 2, cx, cy);
		// un point a 200 px de la, lui, n'y est pas
		const bool dehors = !NkPointDansNoeud(st.doc, st.layout, 2, cx + 200.f, cy);
		char d[128];
		snprintf(d, sizeof(d), "aller-retour (%.3f,%.3f), centre dedans=%d, +200px dehors=%d", x,
				 y, dedans ? 1 : 0, dehors ? 1 : 0);
		verdict("15. la matrice s'inverse, et le picking d'un enfant de groupe tourne suit son "
				"centre la ou il est parti",
				rond && dedans && dehors, d);
	}
	printf("\nRECETTE TRANSFO : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}

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
static nkentseu::int32 RecetteDocument() {
	using namespace nkuidesign;
	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};

	// ── LE DOCUMENT DE RODOLF D'ABORD, SON JUMEAU VERSIONNE ENSUITE ────────
	// Lance depuis la racine de l'arbre, le premier chemin EST le fichier que
	// son application ouvre. Lance ailleurs, le second garde la recette
	// reproductible -- c'est le meme document a quelques valeurs pres (il en
	// derive).
	static const char *const kCandidats[2] = {
		"nkuidesign_document.nkuidoc",
		"Applications/NKUIDesign/design/mises_en_scene/demo_ecran_01.nkuidoc"};
	const char *chemin = nullptr;
	for (uint32 i = 0; i < 2 && !chemin; ++i)
		if (nkentseu::NkFile::Exists(kCandidats[i]))
			chemin = kCandidats[i];
	if (!chemin) {
		printf("ECHEC  0. le document a mesurer est INTROUVABLE  -- essaye : \"%s\" puis "
			   "\"%s\" (lancer depuis la racine de l'arbre)\n",
			   kCandidats[0], kCandidats[1]);
		printf("\nRECETTE DOCUMENT : 0/1 EN ECHEC\n");
		return 1;
	}
	NkUIDocument doc;
	{
		const NkString texte = nkentseu::NkFile::ReadAllText(chemin);
		const bool lu = !texte.Empty() && doc.Load(texte.Data());
		char d[320];
		snprintf(d, sizeof(d), "fichier=%s  titre=%s  noeuds=%u", chemin, doc.title.Data(),
				 (uint32)doc.nodes.Size());
		verdict("1. LE DOCUMENT REEL SE LIT (et la recette DIT lequel elle a lu)",
				lu && doc.nodes.Size() > 1, d);
		if (!lu) {
			printf("\nRECETTE DOCUMENT : %d/%d EN ECHEC\n", cas - echecs, cas);
			return 1;
		}
	}
	NkLayoutResult lay;
	NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 1600.f, 1000.f}, lay);

	// LE DOUBLE-CLIC, TEL QUE LA TOILE L'EXECUTE — mForage compris.
	// ⚠️ LA BOUCLE EST LA MOITIE DE LA MESURE. Un double-clic sur une forme
	//    posee DANS un artboard ne l'atteint pas du premier coup : il FORE.
	//    Compter les coups est donc la seule facon de repondre a « double-
	//    cliquer sur un rectangle ne permet pas d'avoir ca » autrement que par
	//    oui/non -- la reponse peut etre « si, mais au troisieme coup ».
	// LE POINT OU L'ON PEUT VRAIMENT VISER CE NOEUD -- son CORPS LIBRE.
	// ⚠️ MESURER AU CENTRE ETAIT UNE ERREUR, ET ELLE A RENDU UN CHIFFRE FAUX.
	//    Le centre d'un bouton est occupe par son libelle, le centre d'une carte
	//    par sa valeur, le centre d'un champ par son texte d'aide : viser le
	//    centre, c'est viser l'ENFANT ou le FRERE du dessus, jamais le
	//    rectangle. Et le pointage a raison de rendre celui du dessus -- c'est le
	//    contrat de Lunacy, et c'est la regle qu'on a POSEE au 4e retour de
	//    Rodolf. *La question n'est donc pas « le centre ouvre-t-il le mode »
	//    mais « existe-t-il un endroit ou l'utilisateur peut atteindre cette
	//    forme », et c'est un balayage, pas un point.*
	// Rend faux si AUCUN point du rectangle ne designe ce noeud (entierement
	// recouvert) -- un cas qui doit se DIRE, pas se confondre avec un refus.
	auto corpsLibre = [&](int32 noeud, float32 &ox, float32 &oy) -> bool {
		if (!lay.Has(noeud))
			return false;
		const NkPaintRect r = lay.At(noeud);
		for (uint32 gy = 0; gy < 7; ++gy)
			for (uint32 gx = 0; gx < 7; ++gx) {
				const float32 px = r.x + r.w * ((float32)gx + 0.5f) / 7.f;
				const float32 py = r.y + r.h * ((float32)gy + 0.5f) / 7.f;
				if (NkPickSelectable(doc, lay, px, py) == noeud) {
					ox = px;
					oy = py;
					return true;
				}
			}
		return false;
	};

	// Rend le nombre de double-clics jusqu'a la suite `cible`, ou -1.
	auto coupsJusquA = [&](int32 noeud, NkSuiteDblClic cible, int32 &atteint) -> int32 {
		atteint = -1;
		float32 cx = 0.f, cy = 0.f;
		if (!corpsLibre(noeud, cx, cy))
			return -2; // entierement recouvert : ce n'est pas un refus du mecanisme
		int32 forage = -1;
		for (int32 coup = 1; coup <= 6; ++coup) {
			// ⚠️ LE MEME ORDRE QUE LA TOILE, `forageAvant` COMPRIS. C'est tout
			//    l'objet de cette recette : emprunter la porte du geste, pas une
			//    porte voisine qui lui ressemble.
			const int32 forageAvant = forage;
			int32 cand = NkPickDansContexte(doc, lay, cx, cy, forage);
			if (cand == -2) {
				forage = -1;
				cand = NkPickTopLevel(doc, lay, cx, cy);
			}
			if (cand < 0)
				return -1;
			const NkUINode &cn = doc.nodes[(uint32)cand];
			const NkIssueDblClic issue = NkIssueDeDblClic(cn);
			const int32 enfant = (issue == NkIssueDblClic::Forer)
									 ? NkPickDansContexte(doc, lay, cx, cy, cand)
									 : -1;
			const NkSuiteDblClic suite = NkSuiteDeDblClic(issue, enfant >= 0,
														  NkFormeEditable(cn),
														  forageAvant == cand);
			atteint = (suite == NkSuiteDblClic::ForerVersEnfant) ? enfant : cand;
			if (suite == cible)
				return coup;
			if (suite == NkSuiteDblClic::ForerVersEnfant
				|| suite == NkSuiteDblClic::ForerSansEnfant)
				forage = cand;
			else
				return -1; // une suite terminale qui n'est pas la cible : ca n'ira pas plus loin
		}
		return -1;
	};

	// ── 2. LES RECTANGLES DU DOCUMENT OUVRENT-ILS LEURS SOMMETS ? ─────────
	//     Retour (1) de Rodolf, 01/09 : « les rectangles presents, quand je
	//     double-clique dessus, ne font pas apparaitre ces elements de
	//     modification de vertices ; mais quand j'en cree un nouveau ca
	//     apparait. » L'ecart est ICI, et nulle part dans une forme fabriquee.
	//
	// ⚠️ LE CAS PORTE SUR **TOUS** LES RECTANGLES, PAS SUR LES « SIMPLES ». La
	//    premiere version de ce cas ecartait les rects a enfants (« un rect a
	//    enfants FORE, c'est un autre cas ») : elle rendait 12/14 et **passait a
	//    cote du defaut**, parce que les rectangles que Rodolf VOIT sont
	//    justement ceux qui portent des enfants -- le bouton « Se connecter »,
	//    `Panel_Nav`, les cartes. Les 12 qui ouvraient etaient les BARRES DU
	//    GRAPHIQUE, celles qu'on ne double-clique jamais. *Un banc qui ecarte le
	//    cas difficile mesure la partie facile et rend un vert qui ment.*
	{
		uint32 nbRect = 0, ouvrent = 0, recouverts = 0;
		char premierRate[256];
		char premierRecouvert[224];
		premierRate[0] = 0;
		premierRecouvert[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || !n.text.Empty())
				continue; // un rect QUI PORTE du texte s'edite : c'est le cas 3
			++nbRect;
			int32 atteint = -1;
			const int32 coups = coupsJusquA((int32)i, NkSuiteDblClic::ModePoints, atteint);
			if (coups > 0)
				++ouvrent;
			else if (coups == -2) {
				// ENTIEREMENT RECOUVERT : aucun point de ce rectangle ne le
				// designe. Ce n'est pas un refus du mecanisme, c'est une
				// consequence du contrat « le plus haut gagne » -- et ca se DIT.
				++recouverts;
				if (!premierRecouvert[0])
					snprintf(premierRecouvert, sizeof(premierRecouvert),
							 " ; recouvert : \"%s\" (n%u) n'a aucun pixel a lui", n.label.Data(),
							 i);
			} else if (!premierRate[0])
				snprintf(premierRate, sizeof(premierRate),
						 " ; 1er RATE = \"%s\" (n%u) -> on reste sur \"%s\" (n%d)",
						 n.label.Data(), i,
						 (atteint >= 0) ? doc.nodes[(uint32)atteint].label.Data() : "rien",
						 atteint);
		}
		char d[512];
		snprintf(d, sizeof(d), "%u rect(s), %u ouvrent le mode, %u entierement recouvert(s)%s%s",
				 nbRect, ouvrent, recouverts, premierRecouvert, premierRate);
		verdict("2. CHAQUE RECTANGLE DU DOCUMENT REEL ouvre le mode edition de forme depuis son "
				"CORPS LIBRE (retour 1 de Rodolf)",
				nbRect > 0 && ouvrent + recouverts == nbRect, d);
	}

	// ── 5. LE RECTANGLE QUI PORTE DES ENFANTS -- L'IMPASSE, NOMMEE ────────
	//     C'est le cas que la mesure a trouve, et il merite son propre verdict :
	//     un rect a enfants rendait `Forer` a chaque coup, le forage etait arme,
	//     le double-clic suivant ressortait au premier niveau (le contexte rend
	//     -2) et refaisait exactement la meme chose. Le geste tournait en rond,
	//     indefiniment, sur le rectangle le plus visible de son ecran.
	// ⚠️ ET LE CAS EXIGE **DEUX** CHOSES, PAS UNE : que le mode s'ouvre, et qu'il
	//    ne s'ouvre PAS du premier coup. Le premier double-clic doit rester
	//    « j'entre dans le groupe » -- c'est le geste de Lunacy et il sert (une
	//    fois dedans, les clics simples designent le contenu). Un cas qui
	//    n'exigerait que « ca finit par s'ouvrir » laisserait passer la version
	//    qui ouvre du premier coup et supprime le forage.
	{
		uint32 nbConteneurs = 0, ouvrent = 0, auPremierCoup = 0;
		char premier[224];
		premier[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() == 0)
				continue;
			++nbConteneurs;
			int32 atteint = -1;
			const int32 coups = coupsJusquA((int32)i, NkSuiteDblClic::ModePoints, atteint);
			if (coups == 1)
				++auPremierCoup;
			if (coups > 0) {
				++ouvrent;
				if (!premier[0])
					snprintf(premier, sizeof(premier), " ; ex. \"%s\" (n%u) en %d coup(s)",
							 n.label.Data(), i, coups);
			}
		}
		char d[320];
		snprintf(d, sizeof(d),
				 "%u rect(s) a enfants, %u ouvrent le mode, %u des le 1er coup (doit rester 0)%s",
				 nbConteneurs, ouvrent, auPremierCoup, premier);
		verdict("5. UN RECTANGLE QUI PORTE DES ENFANTS finit par ouvrir SA forme -- et jamais "
				"au premier coup (le 1er double-clic ENTRE, comme dans Lunacy)",
				nbConteneurs > 0 && ouvrent == nbConteneurs && auPremierCoup == 0, d);
	}

	// ── 3. UN TEXTE N'A PAS DE SOMMETS, ET IL N'EN A NULLE PART ───────────
	//     Retour (2) de Rodolf, 01/09 : « ca doit etre sur les formes dessinees
	//     autres que le texte ». Un texte s'edite par sa saisie, jamais par son
	//     contour.
	// ⚠️ DEUX CHOSES SE MESURENT ICI, PAS UNE : que le double-clic ouvre bien la
	//    SAISIE, et que la table des sommets refuse le texte a la source
	//    (`NkNatureDe` -> `Aucun`, `NkSommetsDe` -> 0). La premiere sans la
	//    seconde laisserait un chemin lateral (le raccourci, un futur bouton)
	//    poser des poignees sur un mot.
	{
		uint32 nbTexte = 0, editent = 0, sansSommets = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "text"))
				continue;
			++nbTexte;
			int32 atteint = -1;
			if (coupsJusquA((int32)i, NkSuiteDblClic::EditerTexte, atteint) > 0)
				++editent;
			float32 xy[64];
			const NkPaintRect r = lay.Has((int32)i) ? lay.At((int32)i) : NkPaintRect{};
			if (NkNatureDe(n.shape.Data()) == NkNatureSommets::Aucun
				&& NkSommetsDe(n, r, xy, 32) == 0)
				++sansSommets;
		}
		char d[192];
		snprintf(d, sizeof(d), "%u texte(s) : %u ouvrent la saisie, %u sans aucun sommet",
				 nbTexte, editent, sansSommets);
		verdict("3. UN TEXTE OUVRE SA SAISIE ET N'A AUCUN SOMMET, dans le document reel "
				"(retour 2 de Rodolf)",
				nbTexte > 0 && editent == nbTexte && sansSommets == nbTexte, d);
	}

	// ── 4. CONSERVATION : MESURER N'ECRIT RIEN ───────────────────────────
	//     Volet obligatoire de toute garde de ce chantier : lire un document,
	//     le disposer et simuler des double-clics dessus ne doit pas modifier
	//     un octet. Sans ce volet, la recette pourrait « reussir » en
	//     materialisant les sommets de chaque rect au passage.
	{
		NkString avant, apres;
		doc.Save(avant);
		NkLayoutResult l2;
		NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 900.f, 700.f}, l2);
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i)
			(void)NkIssueDeDblClic(doc.nodes[i]);
		doc.Save(apres);
		const bool stable = avant.Size() == apres.Size()
							&& NkComponentDecl::StrEq(avant.Data(), apres.Data());
		char d[128];
		snprintf(d, sizeof(d), "%u octets %s", (uint32)avant.Size(),
				 stable ? "octet pour octet" : "ONT BOUGE");
		verdict("4. CONSERVATION : lire, disposer et interroger le document n'ecrit RIEN",
				stable && avant.Size() > 0, d);
	}

	// ── 6. LE PANNEAU DROIT CHANGE AVEC LA TOILE ─────────────────────────
	//     Comparaison en deux temps de Rodolf, 01/09 : temps 1
	//     (`lunacy_2temps_selection_181741.png`) le panneau habituel ; temps 2
	//     (`lunacy_2temps_edition_181745.png`) une section `EDIT SHAPE` REMPLACE
	//     la geometrie, et LAYER / FILLS / BORDERS / EFFECTS / PROTOTYPING
	//     restent en dessous.
	//
	// ⚠️ CE CAS EXISTE PARCE QUE CE TROISIEME CHANGEMENT AVAIT ETE RATE, et il
	//    n'a ete vu que quand Rodolf a envoye la PAIRE d'images. La consigne
	//    ecrite disait « les poignees de boite disparaissent » et ne parlait pas
	//    du panneau. *Une question sur du visuel se pose avec une capture* -- et
	//    ce qui a ete rate une fois se tient desormais au banc, pas a l'oeil.
	//
	// ⚠️ ET IL EXIGE LES DEUX SENS. « ÉDITION DE FORME est la » ne prouve rien si
	//    DISPOSITION est restee a cote : ce serait un AJOUT, et l'ecran aurait
	//    deux X et deux Y sans rien qui dise lequel parle du sommet. Le cas
	//    verifie donc aussi ce qui doit AVOIR DISPARU, et ce qui doit RESTER.
	{
		const char *const *forme = nullptr;
		const uint32 nf = NkSectionsInspecteur(true, false, forme);
		const char *const *normal = nullptr;
		const uint32 nn = NkSectionsInspecteur(false, false, normal);
		auto contient = [](const char *const *l, uint32 n, const char *quoi) {
			for (uint32 i = 0; i < n; ++i)
				if (NkComponentDecl::StrEq(l[i], quoi))
					return true;
			return false;
		};
		// (a) la section neuve est PREMIERE -- une section d'edition qu'il
		//     faudrait aller chercher au bas du panneau n'est pas trouvee
		const bool premiere = nf > 0 && NkComponentDecl::StrEq(forme[0], "ÉDITION DE FORME");
		// (b) la GEOMETRIE a disparu (remplacement, pas ajout)
		const bool remplace = !contient(forme, nf, "DISPOSITION")
							  && !contient(forme, nf, "ALIGNEMENT")
							  && !contient(forme, nf, "ESPACEMENT")
							  && !contient(forme, nf, "ANCRAGE");
		// (c) ce qui decrit l'OBJET reste -- chez Lunacy comme chez nous
		const bool restent = contient(forme, nf, "REMPLISSAGES")
							 && contient(forme, nf, "BORDURES")
							 && contient(forme, nf, "APPARENCE")
							 && contient(forme, nf, "EFFETS");
		// (d) TEMOIN : hors du mode, la geometrie est bien la et la section
		//     d'edition ABSENTE. Sans ce temoin, une fonction qui rendrait
		//     toujours la liste du mode forme passerait les trois premiers.
		const bool temoin = contient(normal, nn, "DISPOSITION")
							&& !contient(normal, nn, "ÉDITION DE FORME");
		char d[192];
		snprintf(d, sizeof(d), "mode forme : %u sections (1re = %s) ; normal : %u sections",
				 nf, nf > 0 ? forme[0] : "?", nn);
		verdict("6. LE PANNEAU DROIT CHANGE AVEC LA TOILE : « ÉDITION DE FORME » PREMIERE, la "
				"geometrie REMPLACEE (pas doublee), l'apparence conservee, et le temoin hors "
				"mode",
				premiere && remplace && restent && temoin, d);
	}

	// ── 7. SUR SON DOCUMENT : LA BOITE SUIT LE TRACE, ET LE POINTAGE SUIT ──
	//     Retour de Rodolf, 01/09 (soir) : « on doit redefinir sa bounding box
	//     pour la selection ». Le mot qui compte est « pour la selection » : ce
	//     n'est pas le lisere qui le gene, c'est que le clic rate.
	//
	// ⚠️ CE CAS MESURE LE POINTAGE, PAS LA BOITE. Verifier que `width` a la bonne
	//    valeur serait verifier que j'ai su ecrire une soustraction. Ce qu'il
	//    faut prouver, c'est qu'apres avoir tire un sommet HORS de la boite
	//    d'origine, un clic la ou la forme se VOIT maintenant l'attrape -- et
	//    qu'un clic la ou elle n'est PLUS ne l'attrape plus. Les deux sens, sur
	//    son document, avec le vrai `NkComputeLayout` et le vrai `NkPickNode`.
	{
		uint32 nbEssais = 0, pointageOk = 0;
		char premierRate[224];
		premierRate[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size() && nbEssais < 6; ++i) {
			NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() > 0
				|| !n.text.Empty())
				continue;
			const int32 pa = n.parent;
			if (!doc.IsValidIndex(pa) || doc.nodes[(uint32)pa].layout.kind != NkLayoutKind::Free)
				continue;
			++nbEssais;
			// on TIRE le coin haut-gauche loin en dehors, comme la main le ferait
			NkMaterialiserSommets(n);
			n.sommets[0].x = -3.f;
			n.sommets[0].y = -3.f;
			NkRecadrerNoeud(n, true);
			NkLayoutResult l3;
			NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 1600.f, 1000.f}, l3);
			if (!l3.Has((int32)i))
				continue;
			const NkPaintRect r = l3.At((int32)i);
			// 🔴 LE POINT D'ESSAI VIENT DU TRACE, PAS DE LA BOITE, ET LA MUTATION
			//    A DU ME LE DIRE. Ma premiere version visait `r.x + 3` : un point
			//    a trois pixels du bord de la boite est DANS la boite par
			//    construction, quelle qu'elle soit. Le cas mesurait donc la boite
			//    contre elle-meme -- et sous la mutation « le recadrage ne fait
			//    rien », il restait VERT. *Un banc qui prend ses deux mesures du
			//    meme cote ne mesure rien.*
			//    On vise donc le SOMMET TIRE, la ou la forme SE VOIT maintenant :
			//    sans recadrage il tombe loin hors de la boite perimee, et le
			//    pointage le rate -- ce que Rodolf decrit.
			float32 anc[64];
			const uint32 nbA = NkSommetsDe(n, r, anc, 32);
			if (nbA == 0)
				continue;
			// le sommet 0 est celui qu'on a tire ; on vise un peu EN DEDANS de
			// lui, vers le centre, pour ne pas jouer sur le pixel du bord.
			const float32 cxT = r.x + r.w * 0.5f, cyT = r.y + r.h * 0.5f;
			const float32 vx = anc[0] + (cxT - anc[0]) * 0.06f;
			const float32 vy = anc[1] + (cyT - anc[1]) * 0.06f;
			const bool dedans = NkPickSelectable(doc, l3, vx, vy) == (int32)i;
			// (b) ET LA BOITE N'EST PAS DEVENUE LA TOILE ENTIERE : un clic bien
			//     au-dela du trace ne l'attrape pas (sinon « recadrer » pourrait
			//     se contenter d'agrandir sans borne, et le cas (a) passerait
			//     pour une mauvaise raison).
			const bool dehors =
				NkPickSelectable(doc, l3, anc[0] - 40.f, anc[1] - 40.f) != (int32)i;
			if (dedans && dehors)
				++pointageOk;
			else if (!premierRate[0])
				snprintf(premierRate, sizeof(premierRate),
						 " ; 1er rate = \"%s\" (n%u) dedans=%d dehors=%d", n.label.Data(), i,
						 dedans ? 1 : 0, dehors ? 1 : 0);
		}
		char d[320];
		snprintf(d, sizeof(d), "%u forme(s) deformee(s), %u attrapables la ou elles se voient%s",
				 nbEssais, pointageOk, premierRate);
		verdict("7. SUR SON DOCUMENT : apres avoir tire un sommet HORS de la boite, le POINTAGE "
				"suit la forme (on l'attrape ou elle est, pas ou elle etait)",
				nbEssais > 0 && pointageOk == nbEssais, d);
	}

	// ── 8. LES SIX FORMES QUI REFUSAIENT HIER : L'ARRONDI MARCHE ENCORE ────
	//     Retour (3) de Rodolf : il redemande le double-clic qui arrondit. Il
	//     avait ete livre AVANT le correctif du blocage et celui du forage a deux
	//     temps -- donc avant deux changements qui touchent le meme geste.
	//
	// ⚠️ ON RE-MESURE SUR LES FORMES QUI REFUSAIENT, PAS SUR UNE FORME NEUVE.
	//    C'est la lecon de ce matin : les six rectangles a enfants (« Se
	//    connecter », `Panel_Nav`, les quatre cartes) sont exactement ceux qu'un
	//    banc a noeuds fabriques n'exerce jamais. Si le double-clic sur sommet
	//    devait casser quelque part, c'est la.
	{
		uint32 nbC = 0, arrondissent = 0, contourSuit = 0;
		char premierRate[224];
		premierRate[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() == 0)
				continue;
			++nbC;
			// le mode est arme sur CE noeud : le double-clic lui appartient
			const bool aMoi = NkDblClicAuModeForme((int32)i, (int32)i, (int32)i);
			const float32 r1 = NkArrondirSommet(n, 0);
			const float32 r2 = NkArrondirSommet(n, 0);
			const float32 r3 = NkArrondirSommet(n, 0);
			const float32 r4 = NkArrondirSommet(n, 0);
			const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
			if (aMoi && cycle)
				++arrondissent;
			else if (!premierRate[0])
				snprintf(premierRate, sizeof(premierRate),
						 " ; 1er rate = \"%s\" (n%u) au mode=%d cycle %.0f/%.0f/%.0f/%.0f",
						 n.label.Data(), i, aMoi ? 1 : 0, (double)r1, (double)r2, (double)r3,
						 (double)r4);
			// ⚠️ ET LE CONTOUR PEINT DOIT SUIVRE, pas seulement le modele : un
			//    rayon enregistre que le dessin n'honore pas est « un champ qui
			//    n'agit pas », la famille de defauts qu'on chasse. C'est la
			//    mutation qui avait mordu en Q44.
			(void)NkArrondirSommet(n, 0); // -> 8 px
			const NkPaintRect rr = lay.Has((int32)i) ? lay.At((int32)i) : NkPaintRect{};
			float32 ct[256], an[64];
			const uint32 nbAn = NkSommetsDe(n, rr, an, 32);
			const uint32 nbCt = NkContourDe(n, rr, ct, 128);
			if (nbCt > nbAn)
				++contourSuit;
			(void)NkArrondirSommet(n, 0); // on repart du cycle, sans laisser de trace
			(void)NkArrondirSommet(n, 0);
			(void)NkArrondirSommet(n, 0);
		}
		char d[320];
		snprintf(d, sizeof(d), "%u rect(s) a enfants : %u arrondissent (cycle 0/8/16/32), %u "
							   "dont le CONTOUR PEINT suit%s",
				 nbC, arrondissent, contourSuit, premierRate);
		verdict("8. LES FORMES QUI REFUSAIENT HIER : le double-clic appartient au mode, "
				"l'arrondi cycle encore, et le contour PEINT le montre (retour 3 de Rodolf)",
				nbC > 0 && arrondissent == nbC && contourSuit == nbC, d);
	}

	printf("\nRECETTE DOCUMENT : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}


// ═════════════════════════════════════════════════════════════════════════════
// Le VRAI `NkCalculerSnap`, celui que le glisser appelle. Sans fenetre ni GPU.
//
// ⚠️ C'EST POUR CETTE RECETTE QUE LE CALCUL A ETE SORTI DE `OnUI`. Ecrit dans
//    le panneau, il aurait vecu la ou aucun banc ne va — la faute que Q41 a
//    relevee sur le zoom et le deplacement (« restes trois etapes sans preuve,
//    et j'ai annonce qu'ils marchaient sans l'avoir vu »). Ce qui reste hors de
//    portee ici, et la recette le dit : le DESSIN du guide et la bascule de
//    l'aimant, qui se jugent a la capture et au releve `canvas.snap`.
static nkentseu::int32 RecetteSnap() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	int32 cadre = -1;
	auto poser = [&](const char *nom, float32 x, float32 y, float32 w, float32 h) -> int32 {
		const int32 i = st.doc.AddChild(cadre, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(nom);
		n.shape = NkString("rect");
		n.posX = x;
		n.posY = y;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = w;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = h;
		return i;
	};
	// La scene : une page 0..600 x 0..400, un voisin fixe, un mobile.
	auto scene = [&](int32 &voisin, int32 &mobile) {
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.posX = 0.f;
			c.posY = 0.f;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		voisin = poser("Voisin", 100.f, 100.f, 80.f, 60.f);
		mobile = poser("Mobile", 300.f, 250.f, 40.f, 30.f);
		st.Recompute(surface);
	};
	// Le rectangle du mobile, DEPLACE de (dx,dy) : exactement ce que le geste
	// donne au calcul.
	auto rectDe = [&](int32 n, float32 dx, float32 dy) {
		NkPaintRect r = st.layout.At(n);
		r.x += dx;
		r.y += dy;
		return r;
	};
	const float32 tol6 = 6.f; // 6 px ecran au zoom 1

	int32 voisin = -1, mobile = -1;
	// ── 1. BORD GAUCHE contre BORD GAUCHE, a 3 px : ca colle EXACTEMENT
	scene(voisin, mobile);
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		// on amene le mobile a 3 px a droite du bord gauche du voisin
		const float32 dx = (rv.x + 3.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		const bool ok = s.guideV.actif && s.dx == -3.f && s.guideV.coord == rv.x
						&& s.guideV.voisin == voisin;
		char d[128];
		snprintf(d, sizeof(d), "dx=%.1f (attendu -3), guide x=%.1f (bord voisin %.1f), voisin=%d",
				 s.dx, s.guideV.coord, rv.x, s.guideV.voisin);
		verdict("bord a 3 px du bord voisin : snap EXACT, guide sur le bord", ok, d);
	}
	// ── 2. HORS TOLERANCE : rien ne bouge, et AUCUN guide (le controle negatif
	//    sans lequel le cas 1 ne prouve rien : un aimant qui colle toujours
	//    passerait le cas 1 les yeux fermes)
	// ⚠️ 33 px, ET LE CHIFFRE EST CHOISI. La premiere version disait 20 px et
	//    ECHOUAIT -- non par un defaut du calcul, mais parce qu a 20 px de son
	//    bord gauche le mobile avait son CENTRE exactement sur le centre du
	//    voisin : un vrai alignement, correctement detecte. La lecon vaut d etre
	//    ecrite : dans un banc d aimantation, une scene mal choisie fabrique des
	//    alignements par accident, et on accuse le code. A 33 px, le candidat le
	//    plus proche tombe a 7 px -- JUSTE au-dela des 6 de tolerance, ce qui
	//    fait de ce cas une borne et plus seulement un contre-exemple.
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 33.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		char d[96];
		snprintf(d, sizeof(d), "dx=%.1f, guideV=%s", s.dx, s.guideV.actif ? "oui" : "non");
		verdict("a 33 px (candidat le plus proche a 7) : RIEN ne colle, aucun guide",
				s.dx == 0.f && !s.guideV.actif, d);
	}
	// ── 3. AIMANT ETEINT : la tolerance tombe a zero, plus rien ne colle
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 3.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), 0.f);
		char d[96];
		snprintf(d, sizeof(d), "dx=%.1f, aimante=%s", s.dx, s.Aimante() ? "oui" : "non");
		verdict("aimant eteint (tolerance nulle) : le meme geste ne colle plus",
				s.dx == 0.f && !s.Aimante(), d);
	}
	// ── 4. LA TOLERANCE SE DIVISE PAR LE ZOOM. 6 px ecran au zoom 4 = 1,5 unite
	//    document : un ecart de 3 unites, qui collait au zoom 1, ne colle plus.
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 3.f) - rm.x;
		st.view.zoom = 4.f;
		const float32 tolZoom4 = st.view.ToDocLength(DesignState::kSnapTolEcran);
		const NkSnapResultat s =
			NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tolZoom4);
		st.view.zoom = 1.f;
		char d[112];
		snprintf(d, sizeof(d), "tolerance zoom 4 = %.2f unites, dx=%.1f", tolZoom4, s.dx);
		verdict("la tolerance suit le ZOOM : 3 unites ne collent plus au zoom 4",
				tolZoom4 == 1.5f && s.dx == 0.f, d);
	}
	// ── 5. CENTRE contre CENTRE
	scene(voisin, mobile);
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 cv = rv.y + rv.h * 0.5f;
		const float32 cm = rm.y + rm.h * 0.5f;
		const float32 dy = (cv + 2.f) - cm; // centre du mobile a 2 px sous celui du voisin
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, 0.f, dy), tol6);
		char d[112];
		snprintf(d, sizeof(d), "dy=%.1f (attendu -2), guide y=%.1f (centre voisin %.1f)", s.dy,
				 s.guideH.coord, cv);
		verdict("centre contre centre : le guide se pose sur le CENTRE du voisin",
				s.guideH.actif && s.dy == -2.f && s.guideH.coord == cv, d);
	}
	// ── 6. LA PAGE aimante aussi (bord gauche du cadre)
	scene(voisin, mobile);
	{
		const NkPaintRect rp = st.layout.At(cadre);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rp.x + 4.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		char d[112];
		snprintf(d, sizeof(d), "dx=%.1f (attendu -4), voisin=%d (-1 = la page)", s.dx,
				 s.guideV.voisin);
		verdict("la PAGE aimante comme un voisin, et le guide le dit (voisin = -1)",
				s.guideV.actif && s.dx == -4.f && s.guideV.voisin == -1, d);
	}
	// ── 7. L'ESPACEMENT EGAL, avec son BADGE de distance
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A finit a 100, B commence a 300 : 200 px libres, le mobile fait 40 ->
		// 160 de libre, donc 80 de chaque cote. La position egale est x = 180.
		poser("A", 40.f, 50.f, 60.f, 30.f);
		poser("B", 300.f, 50.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 176.f, 50.f, 40.f, 30.f); // a 4 px de la place egale
		st.Recompute(surface);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		const NkPaintRect rp = st.layout.At(cadre);
		char d[144];
		snprintf(d, sizeof(d), "dx=%.1f (attendu 4), badge=%s, ecart=%.1f (attendu 80)", s.dx,
				 s.guideV.badge ? "oui" : "non", s.guideV.ecart);
		verdict("espacement EGAL : la place du milieu aimante, et l'ecart se DIT",
				s.guideV.actif && s.dx == 4.f && s.guideV.badge && s.guideV.ecart == 80.f
					&& s.guideV.coord == rp.x + 180.f,
				d);
	}
	// ── 8. ON NE S'AIMANTE PAS SUR SOI-MEME NI SUR SON DESCENDANT
	//    (un descendant BOUGE AVEC le noeud : il collerait toujours, et
	//    n'alignerait rien — le snap qui ne sert a rien mais qui bloque tout)
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// ⚠️ (233, 155) N EST PAS UN CHIFFRE AU HASARD. A (200, 200) ce cas
		//    ECHOUAIT : le bord haut du mobile tombait pile sur le CENTRE
		//    vertical de la page (400 / 2). Le calcul avait raison, la scene
		//    avait tort. Ici le mobile est loin des trois reperes de chaque axe
		//    de la page -- donc le SEUL candidat a portee est son propre enfant,
		//    a 2 px. S il collait, ce cas le dirait.
		const int32 m = poser("Mobile", 233.f, 155.f, 40.f, 30.f);
		st.doc.nodes[(uint32)m].layout.kind = NkLayoutKind::Free;
		const int32 enf = st.doc.AddChild(m, "", NkAuthor::Humain);
		{
			NkUINode &e = st.doc.nodes[(uint32)enf];
			e.label = NkString("Enfant");
			e.shape = NkString("rect");
			e.posX = 2.f;
			e.posY = 2.f;
			e.width.mode = NkSizeMode::Fixed;
			e.width.value = 10.f;
			e.height.mode = NkSizeMode::Fixed;
			e.height.value = 10.f;
		}
		st.Recompute(surface);
		// seul le mobile et son enfant existent : aucun voisin, et la page est
		// loin (bords a 0 et 600). Rien ne doit coller.
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		char d[96];
		snprintf(d, sizeof(d), "dx=%.1f dy=%.1f, aimante=%s", s.dx, s.dy,
				 s.Aimante() ? "oui" : "non");
		verdict("ni soi-meme ni son enfant : au milieu de la page, rien ne colle",
				!s.Aimante(), d);
	}
	// ── 9. L'ESPACEMENT REPETE : deux blocs a N, le troisieme s'aimante a N ──
	//     Retour de Rodolf, 01/09 : « pas de snap proportionnel, par exemple
	//     snapper par rapport a l'espace entre deux blocs deja poses ».
	//
	//     ⚠️ CE CAS EST CONSTRUIT POUR QU'AUCUN AUTRE CANDIDAT NE PUISSE LE
	//     REUSSIR A SA PLACE, et c'est la lecon des scenes de snap de Q42
	//     appliquee d'entree. La position visee (300) n'est ni un bord, ni un
	//     centre, ni le milieu entre A et B, ni un repere de la page : si
	//     l'espacement repete n'existait pas, RIEN ne collerait. Sans cette
	//     precaution, le cas serait passe au vert sur un alignement de bord
	//     fabrique par accident, et n'aurait rien prouve du tout.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A : 60..120 ; B : 180..240 -> l'ecart MODELE vaut 60.
		// Le troisieme doit donc se poser a 240 + 60 = 300.
		poser("A", 60.f, 47.f, 60.f, 30.f);
		poser("B", 180.f, 47.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 304.f, 47.f, 60.f, 30.f); // a 4 px de la place
		st.doc.nodes[(uint32)m].posY = 143.f; // hors de la bande de A et B : aucun
											  // alignement vertical ne peut aider
		st.Recompute(surface);
		const NkPaintRect rp = st.layout.At(cadre);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		const bool colle = s.guideV.actif && s.dx == -4.f && s.guideV.coord == rp.x + 300.f;
		const bool dit = s.guideV.badge && s.guideV.ecart == 60.f;
		// LES DEUX DISTANCES SONT ECRITES : celle du modele ET celle de la copie.
		bool deuxMesures = s.nbMesures >= 2u;
		if (deuxMesures)
			for (uint32 i = 0; i < 2u; ++i)
				if (s.mesures[i].valeur != 60.f)
					deuxMesures = false;
		char d[176];
		snprintf(d, sizeof(d), "dx=%.1f (attendu -4), ecart=%.0f (attendu 60), %u mesure(s) "
							   "ecrite(s) [%.0f, %.0f]",
				 s.dx, s.guideV.ecart, s.nbMesures, s.nbMesures > 0 ? s.mesures[0].valeur : -1.f,
				 s.nbMesures > 1 ? s.mesures[1].valeur : -1.f);
		verdict("9. espacement REPETE : deux blocs espaces de 60, le troisieme s'aimante a 60 "
				"-- et les DEUX distances sont ecrites (le modele ET la copie)",
				colle && dit && deuxMesures, d);
	}
	// ── 10. L'ESPACEMENT REPETE MARCHE DANS LES DEUX SENS ────────────────────
	//     ⚠️ LE CHEMIN FRERE, TENU PAR UN BANC. N'ecrire que « apres B » aurait
	//     donne un aimant qui marche quand on construit vers la droite et pas
	//     vers la gauche -- une asymetrie que personne ne devine et que tout le
	//     monde prend pour une panne. Le cas pose le mobile AVANT A.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A : 400..460 ; B : 520..580 -> ecart 60. AVANT A : le bord DROIT du
		// mobile doit se poser a 400 - 60 = 340, donc son bord gauche a 280.
		poser("A", 400.f, 47.f, 60.f, 30.f);
		poser("B", 520.f, 47.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 277.f, 143.f, 60.f, 30.f); // a 3 px
		st.Recompute(surface);
		const NkPaintRect rp = st.layout.At(cadre);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		char d[144];
		snprintf(d, sizeof(d), "dx=%.1f (attendu 3), guide x=%.0f (attendu %.0f), ecart=%.0f",
				 s.dx, s.guideV.coord, rp.x + 340.f, s.guideV.ecart);
		verdict("10. l'espacement repete marche AUSSI vers la gauche : on construit une serie "
				"dans les deux sens",
				s.guideV.actif && s.dx == 3.f && s.guideV.coord == rp.x + 340.f
					&& s.guideV.ecart == 60.f,
				d);
	}
	// ── 11. DEUX BLOCS COLLES NE SONT PAS UN MOTIF ───────────────────────────
	//     ⚠️ UN ECART NUL N'EST PAS UNE REGULARITE A REPRODUIRE. En faire une
	//     collerait le troisieme bloc aux deux autres a la moindre approche, et
	//     l'aimant deviendrait de la glu -- le genre de comportement pour lequel
	//     on finit par eteindre l'aimantation, donc par perdre tout le reste.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A et B se TOUCHENT (60..120 et 120..180) : l'ecart modele est 0.
		poser("A", 60.f, 47.f, 60.f, 30.f);
		poser("B", 120.f, 47.f, 60.f, 30.f);
		// le mobile est a 3 px du bord droit de B : si l'ecart 0 etait un motif,
		// il collerait a 180 en se disant « repete ».
		const int32 m = poser("Mobile", 183.f, 143.f, 60.f, 30.f);
		st.Recompute(surface);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		// il PEUT coller (bord contre bord, c'est legitime) mais JAMAIS avec un
		// badge d'espacement repete a 0 : c'est ca qu'on interdit.
		const bool pasDeMotifNul = !(s.guideV.badge && s.guideV.ecart == 0.f);
		bool aucuneMesureNulle = true;
		for (uint32 i = 0; i < s.nbMesures; ++i)
			if (s.mesures[i].valeur == 0.f)
				aucuneMesureNulle = false;
		char d[144];
		snprintf(d, sizeof(d), "dx=%.1f, badge=%s, ecart=%.1f, %u mesure(s)", s.dx,
				 s.guideV.badge ? "oui" : "non", s.guideV.ecart, s.nbMesures);
		verdict("11. deux blocs COLLES ne fabriquent pas un motif d'espacement 0 (sinon "
				"l'aimant devient de la glu)",
				pasDeMotifNul && aucuneMesureNulle, d);
	}
	printf("\nRECETTE SNAP : %d/%d %s\n", cas - echecs, cas,
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
static void InjecterClics(nkgui::NkGuiContext &ctx) {
	static int32 compteur = 0;
	++compteur;
	for (int32 i = 0; i < (int32)(sizeof(gClics) / sizeof(gClics[0])); ++i) {
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
	for (int32 i = 0; i < 2; ++i) {
		if (gGlissers[i].frame < 0)
			continue;
		const int32 f0 = gGlissers[i].frame;
		const int32 fn = f0 + (gGlissers[i].duree > 0 ? gGlissers[i].duree : 12);
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
	// Les gestes d'édition Lunacy qui n'ont PAS de drapeau `want*` dans NKGui
	// (Ctrl+C/X/V/A en ont un, eux — cf. le commentaire de CmdDupliquer).
	shell->RegisterCommand("Édition: Dupliquer", &CmdDupliquer, nullptr, "Ctrl+D");
	shell->RegisterCommand("Objet: Grouper", &CmdGrouper, nullptr, "Ctrl+G");
	shell->RegisterCommand("Objet: Dégrouper", &CmdDegrouper, nullptr, "Ctrl+Shift+G");
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
